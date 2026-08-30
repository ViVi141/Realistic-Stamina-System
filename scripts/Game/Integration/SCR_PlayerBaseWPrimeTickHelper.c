//! Phase B 服务端 W′ TickPower / EPOC / 复制写回。
//! 从 PlayerBase_UpdateLoop.c 拆出；与 SCR_RSS_WPrimeServerTick（PrepareControls 补 tick）分工不同。
//! 禁止在本 static 内调用 Replication.BumpMe（须由实体实例上下文触发）。

class SCR_PlayerBaseWPrimeTickHelper
{
    //! @param replPool / replCooldownUntil 服务端复制槽（inout）
    //! @return true：已写复制槽，调用方须在实例上 Replication.BumpMe()
    static bool TickPhaseBAnaerobic(
        SCR_CharacterControllerComponent ctrl,
        RSS_StaminaTickLocals loc,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        SCR_RSS_StaminaState staminaState,
        SCR_RSS_EnvironmentFactor environmentFactor,
        SCR_RSS_FatigueSystem fatigueSystem,
        SCR_RSS_EpocState epocState,
        float appliedSpeedLimitMs,
        bool cpWalkOverrideActive,
        inout float replPool,
        inout float replCooldownUntil)
    {
        if (!anaerobicBurst || !loc)
            return false;

        bool tickAnaerobic = Replication.IsServer();
        if (!tickAnaerobic)
            return false;

        SCR_RSS_CriticalPowerModel cpModel = anaerobicBurst.GetCpModel();
        float pool01BeforeTick = 1.0;
        if (cpModel)
            pool01BeforeTick = cpModel.GetPool01();

        float powerW = SCR_RSS_DrainCalculator.GetMetabolicAccountingPowerWatts(
            loc.currentSpeed,
            appliedSpeedLimitMs,
            loc.totalWeightWithWetAndBody,
            loc.gradePercent,
            loc.terrainFactor,
            loc.effectivePhase,
            pool01BeforeTick,
            loc.isSprintActive);

        if (cpModel)
        {
            float loadKg = Math.Max(loc.totalWeightWithWetAndBody - SCR_RSS_MetabolismMath.CHARACTER_WEIGHT, 0.0);
            float envCpMult = 1.0;
            if (environmentFactor && SCR_RSS_ConfigBridge.IsHeatStressEnabled())
            {
                float heatPen = environmentFactor.GetHeatStressPenalty();
                envCpMult = 1.0 - heatPen * 0.35;
            }
            float fatigueNorm = 0.0;
            if (fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
                fatigueNorm = fatigueSystem.GetFatigueIntegralNorm();
            cpModel.SetRuntimeContext(loadKg, loc.gradePercent, envCpMult, fatigueNorm);
            if (fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
            {
                float cpMult = fatigueSystem.GetCpFatigueMultiplier();
                cpModel.SetFatigueCpMultiplier(cpMult);
            }
            else
            {
                cpModel.SetFatigueCpMultiplier(1.0);
            }

            // 非冲刺：
            // - W′ 解除武装且未超速：功率钳到 CP（巡航不烧空池）
            // - W′ 仍武装：P>CP 必须进 TickPower（武装时 v_limit 是步态盖，不是 CP 巡航速）
            // - 超速且 P≤CP（下坡滑行常见）：钉在 CP，禁止 W′ 回充白嫖
            // 步态覆盖带内：引擎 Walk 无法慢于相位顶，相对徒步地板 1.0 的假超速走巡航钳。
            if (!loc.isSprintActive)
            {
                bool physOverspeed = SCR_RSS_DrainCalculator.IsPhysOverspeedForAnaerobicTick(
                    loc.currentSpeed,
                    appliedSpeedLimitMs,
                    cpWalkOverrideActive);
                float cpClamp = cpModel.GetEffectiveCriticalPowerWatts();
                if (cpClamp > 1.0)
                {
                    if (!physOverspeed)
                    {
                        bool cruiseLatched = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(cpModel);
                        if (cruiseLatched)
                        {
                            if (powerW > cpClamp)
                                powerW = cpClamp;
                        }
                    }
                    else
                    {
                        if (powerW <= cpClamp)
                            powerW = cpClamp;
                    }
                }
            }
        }

        anaerobicBurst.TickPower(powerW, loc.isSprintActive, loc.currentTime, loc.timeDeltaSec, loc.currentSpeed);
        if (epocState)
        {
            // EPOC：限速内意图功率；无 W′ 超速记账时再钳到 CP（与有氧 P_bill 对齐，避免下坡跑飞停步暴罚）
            float cpForEpoc = -1.0;
            if (cpModel)
                cpForEpoc = cpModel.GetEffectiveCriticalPowerWatts();
            epocState.SetEffectiveCpWatts(cpForEpoc);
            float powerForEpoc = SCR_RSS_DrainCalculator.GetEpocSamplePowerWatts(
                loc.currentSpeed,
                appliedSpeedLimitMs,
                loc.totalWeightWithWetAndBody,
                loc.gradePercent,
                loc.terrainFactor,
                loc.phaseNow,
                cpForEpoc);
            bool cruiseLatchedForEpoc = false;
            if (cpModel)
                cruiseLatchedForEpoc = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(cpModel);
            bool billAboveCp = false;
            if (loc.isSprintActive)
                billAboveCp = true;
            else if (!cruiseLatchedForEpoc)
                billAboveCp = true;
            if (!billAboveCp)
            {
                if (cpForEpoc > 1.0)
                {
                    if (powerForEpoc > cpForEpoc)
                        powerForEpoc = cpForEpoc;
                }
            }
            epocState.UpdateExercisePowerSample(
                powerForEpoc, loc.currentSpeed, loc.timeDeltaSec);
        }
        if (staminaState)
        {
            staminaState.SetWPrimePoolFromCpModel(anaerobicBurst.GetCpModel());
            staminaState.SetAerobic(loc.staminaPercent);
        }
        SCR_RSS_NetworkSyncManager.ReadAnaerobicForReplication(
            anaerobicBurst, replPool, replCooldownUntil);
        return true;
    }
}
