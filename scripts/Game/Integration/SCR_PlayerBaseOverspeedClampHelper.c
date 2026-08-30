//! Phase B 超速 / W′ 耗尽巡航物理钳。
//! 从 PlayerBase_UpdateLoop.c 拆出；行为不变。

class SCR_PlayerBaseOverspeedClampHelper
{
    //! W′ tick 之后：见底缓降 + 代谢超速硬/软钳。
    //! @param appliedSpeedLimitMs 权威绝对限速（inout）
    //! @param lastRssSpeedMultiplierApplied 权威限速倍率（inout）
    static void ApplyPostTickOverspeedClamp(
        SCR_CharacterControllerComponent ctrl,
        RSS_StaminaTickLocals loc,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        SCR_RSS_SprintBlockSpeedTransition sprintBlock,
        inout float appliedSpeedLimitMs,
        inout float lastRssSpeedMultiplierApplied,
        float lastRssEngineBaseForLimit)
    {
        if (!ctrl || !loc)
            return;
        if (loc.isExhausted || loc.useSwimmingModel)
            return;
        if (loc.currentSpeed < SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            return;

        SCR_RSS_CriticalPowerModel cpPostTick = null;
        if (anaerobicBurst)
            cpPostTick = anaerobicBurst.GetCpModel();

        float pool01AfterTick = 1.0;
        if (cpPostTick)
            pool01AfterTick = cpPostTick.GetPool01();

        bool overspeeding = SCR_RSS_DrainCalculator.IsMetabolicOverspeedAccounting(
            loc.currentSpeed, appliedSpeedLimitMs);
        bool wPrimeAllowsOverspeed = false;
        if (cpPostTick)
            wPrimeAllowsOverspeed = SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(
                cpPostTick);
        else
            wPrimeAllowsOverspeed = SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(
                pool01AfterTick);

        bool cruiseLatchedNow = false;
        if (cpPostTick)
            cruiseLatchedNow = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(cpPostTick);

        // W′ 见底闩巡航的同一帧：开绝对速度缓降，避免硬钳把限速 SNAP 到巡航顶
        if (sprintBlock && cruiseLatchedNow)
        {
            float disarmTargetAbs = appliedSpeedLimitMs;
            if (SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            {
                float wPrimeCapNow = SCR_RSS_DrainCalculator.GetWPrimeExhaustedOverspeedCapMs(
                    loc.currentSpeed,
                    appliedSpeedLimitMs,
                    pool01AfterTick,
                    loc.phaseNow,
                    loc.totalWeightWithWetAndBody,
                    loc.gradePercent,
                    loc.terrainFactor,
                    cpPostTick);
                if (wPrimeCapNow > 0.05)
                    disarmTargetAbs = wPrimeCapNow;
            }
            sprintBlock.EnsureDisarmTransition(
                loc.currentTime,
                appliedSpeedLimitMs,
                disarmTargetAbs);
        }

        // 绝对速度缓降中不要用代谢硬顶覆盖倍率，但要用当前已应用限速做相位安全软钳（防滑步）
        bool inAbsSpeedTransition = false;
        if (sprintBlock)
            inAbsSpeedTransition = sprintBlock.IsInTransition();

        if (inAbsSpeedTransition && ctrl.IsPlayerControlled() && appliedSpeedLimitMs > 0.05
            && SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
        {
            float engineBase = ctrl.GetRssSpeedLimitEngineBaseMs();
            if (engineBase <= 0.05)
                engineBase = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
            float safeCap = SCR_RSS_SpeedBridge.GetPhaseSafePhysicsCapMs(
                appliedSpeedLimitMs,
                engineBase,
                loc.isSprintingNow,
                loc.phaseNow);
            appliedSpeedLimitMs = safeCap;
            if (SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
            {
                if (!wPrimeAllowsOverspeed || loc.phaseNow == 1)
                {
                    SCR_RSS_SpeedBridge.EnforceCpCruisePhysicsCap(
                        loc.owner,
                        safeCap,
                        loc.currentSpeed,
                        loc.timeDeltaSec,
                        loc.gradePercent,
                        loc.phaseNow);
                }
            }
        }

        // 武装且在缓降中：勿硬顶覆盖。
        // W′ 解除武装的绝对速度缓降窗内：禁止把 SetSpeedLimit 瞬间钉到巡航顶
        // （旧逻辑会 3.3→1.8 SNAP，再与 phys 互殴）；只靠上方对「已缓降 safeCap」的软钳。
        bool allowOverspeedHardClamp = false;
        if (overspeeding)
        {
            if (!inAbsSpeedTransition)
                allowOverspeedHardClamp = true;
            else if (!wPrimeAllowsOverspeed && loc.phaseNow == 1)
            {
                // Walk 缓降窗仍允许硬路径压到步行顶（原版 Walk 不得 3m/s+）
                allowOverspeedHardClamp = true;
            }
        }

        if (allowOverspeedHardClamp
            && SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
        {
            // 必须用已落盘的绝对限速钳制；禁止再用可能切换的 Sprint/Run 分母重算
            float hardAbs = appliedSpeedLimitMs;
            float engineBase = lastRssEngineBaseForLimit;
            if (engineBase <= 0.05)
                engineBase = ctrl.GetRssSpeedLimitEngineBaseMs();
            if (engineBase <= 0.05)
                engineBase = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

            float hardMult = lastRssSpeedMultiplierApplied;
            if (!wPrimeAllowsOverspeed
                && SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            {
                float wPrimeCapMs = SCR_RSS_DrainCalculator.GetWPrimeExhaustedOverspeedCapMs(
                    loc.currentSpeed,
                    appliedSpeedLimitMs,
                    pool01AfterTick,
                    loc.phaseNow,
                    loc.totalWeightWithWetAndBody,
                    loc.gradePercent,
                    loc.terrainFactor,
                    cpPostTick);
                if (wPrimeCapMs > 0.05)
                {
                    if (hardAbs < 0.05 || wPrimeCapMs < hardAbs)
                        hardAbs = wPrimeCapMs;
                    float metabMult = Math.Clamp(wPrimeCapMs / engineBase, 0.01, 3.0);
                    if (metabMult < hardMult)
                        hardMult = metabMult;
                }
            }

            if (hardAbs < 0.05)
                hardAbs = hardMult * engineBase;

            bool keepSrc = !wPrimeAllowsOverspeed;
            if (loc.phaseNow == 1)
                keepSrc = true;
            float hardFrac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                hardAbs, engineBase, keepSrc);
            if (ctrl.IsPlayerControlled())
            {
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, hardFrac);
                float safeCap = SCR_RSS_SpeedBridge.GetPhaseSafePhysicsCapMs(
                    hardAbs, engineBase, loc.isSprintingNow, loc.phaseNow);
                appliedSpeedLimitMs = safeCap;
                if (SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
                {
                    if (!wPrimeAllowsOverspeed || loc.phaseNow == 1)
                    {
                        SCR_RSS_SpeedBridge.EnforceCpCruisePhysicsCap(
                            loc.owner,
                            safeCap,
                            loc.currentSpeed,
                            loc.timeDeltaSec,
                            loc.gradePercent,
                            loc.phaseNow);
                    }
                }
            }
            else
            {
                SCR_RSS_SpeedBridge.ApplyHardStaminaSpeedClamp(loc.owner, hardFrac);
                appliedSpeedLimitMs = hardAbs;
                if (SCR_RSS_SpeedBridge.IsHorizontalSpeedClampEnabled())
                    SCR_RSS_SpeedBridge.ClampOwnerHorizontalSpeed(loc.owner, hardAbs);
            }
            lastRssSpeedMultiplierApplied = hardFrac;
            loc.finalSpeedMultiplier = hardFrac;
        }
        else if (inAbsSpeedTransition && overspeeding && !wPrimeAllowsOverspeed
            && ctrl.IsPlayerControlled() && SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled()
            && SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
        {
            // Run 解除武装缓降中：只软跟当前已缓降的 applied 限速，不 SNAP
            if (appliedSpeedLimitMs > 0.05)
            {
                SCR_RSS_SpeedBridge.EnforceCpCruisePhysicsCap(
                    loc.owner,
                    appliedSpeedLimitMs,
                    loc.currentSpeed,
                    loc.timeDeltaSec,
                    loc.gradePercent,
                    loc.phaseNow);
            }
        }
    }
}
