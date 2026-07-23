//! v6 消耗测速：v_drain / v_acct 一律用 v_meas（不再 min 到 v_limit）。
//! 限速只影响 SetSpeedLimit；代谢由实测速度 + 坡度/负重模型承担，数值靠孪生标定。

class SCR_RSS_DrainCalculator
{
    //! @param measuredSpeedMs GetVelocity 水平模长（m/s）
    //! @param appliedSpeedLimitMs 保留参数（调试/超速判定仍用）；记账不再钳到此值
    //! @return 用于代谢模型的速度（m/s）= v_meas
    static float GetDrainVelocityMs(float measuredSpeedMs, float appliedSpeedLimitMs)
    {
        if (measuredSpeedMs < 0.0)
            return 0.0;
        return measuredSpeedMs;
    }

    //! 代谢记账速度：与 GetDrainVelocityMs 相同，始终 v_meas
    static float GetMetabolicAccountingVelocityMs(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float wPrimePool01 = 1.0,
        bool isSprinting = false)
    {
        return GetDrainVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
    }

    //! 代谢记账功率（W）：按实测速度；有氧侧仍可 min(P, CP)
    static float GetMetabolicAccountingPowerWatts(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase,
        float wPrimePool01 = 1.0,
        bool isSprinting = false)
    {
        float vAcct = GetMetabolicAccountingVelocityMs(
            measuredSpeedMs, appliedSpeedLimitMs, wPrimePool01, isSprinting);
        return SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            vAcct, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
    }

    //! 疲劳积分功率（W）：与记账同用 v_meas
    static float GetMetabolicFatiguePowerWatts(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase)
    {
        float vDrain = GetDrainVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
        return SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            vDrain, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
    }

    //! EPOC 峰值采样速度：限速内意图速度（硬钳关时 v_meas 可远超 v_limit，不能按跑飞速度记氧债）
    static float GetEpocSampleVelocityMs(float measuredSpeedMs, float appliedSpeedLimitMs)
    {
        float v = measuredSpeedMs;
        if (v < 0.0)
            v = 0.0;
        if (appliedSpeedLimitMs > 0.05)
        {
            if (v > appliedSpeedLimitMs)
                v = appliedSpeedLimitMs;
        }
        return v;
    }

    //! EPOC 峰值采样功率（W）：按意图速度，可选再钳到 CP×(1+超额上限)
    static float GetEpocSamplePowerWatts(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase,
        float criticalPowerWatts = -1.0)
    {
        float vEpoc = GetEpocSampleVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
        float powerW = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            vEpoc, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
        if (criticalPowerWatts > 1.0)
        {
            float storeCap = criticalPowerWatts
                * (1.0 + SCR_RSS_Constants.EPOC_MAX_POWER_EXCESS_RATIO);
            if (powerW > storeCap)
                powerW = storeCap;
        }
        return powerW;
    }

    static bool IsMetabolicOverspeedAccounting(float measuredSpeedMs, float appliedSpeedLimitMs)
    {
        if (appliedSpeedLimitMs <= 0.05)
            return false;
        return measuredSpeedMs > appliedSpeedLimitMs + SCR_RSS_Constants.V6_OVERSPEED_ACCOUNTING_EPS_MPS;
    }

    //! W′ 池是否仍可支撑「超限速按 v_meas 记账」
    //! 无 CP 模型时的无状态近似；有模型时请用带 cpModel 的重载（施密特闩锁）
    static bool IsWPrimePoolAvailableForOverspeed(float wPrimePool01)
    {
        float threshold = SCR_RSS_ConfigBridge.GetWPrimeSprintEnableThreshold();
        float enableAt = threshold + SCR_RSS_Constants.V6_WPRIME_OVERSPEED_HYSTERESIS;
        return wPrimePool01 > enableAt;
    }

    //! 施密特：关闭带 disarm，再武装带 rearm，避免阈值附近均速被抬高
    static bool IsWPrimePoolAvailableForOverspeed(SCR_RSS_CriticalPowerModel cpModel)
    {
        if (!cpModel)
            return false;
        return cpModel.RefreshAndGetOverspeedArmed();
    }

    //! W′ 耗尽且仍超速：返回应强制应用的绝对速度上限（m/s）；否则 -1
    static float GetWPrimeExhaustedOverspeedCapMs(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float wPrimePool01,
        int movementPhase,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        SCR_RSS_CriticalPowerModel cpModel)
    {
        if (!SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            return -1.0;
        if (!IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs))
            return -1.0;
        if (IsWPrimePoolAvailableForOverspeed(cpModel))
            return -1.0;
        if (!cpModel)
            return -1.0;

        float capMs = GetMetabolicSpeedCapMs(
            measuredSpeedMs,
            movementPhase,
            totalWeightKg,
            gradePercent,
            terrainFactor,
            false,
            0.0,
            cpModel);
        if (capMs <= 0.05)
        {
            float cp = cpModel.GetEffectiveCriticalPowerWatts();
            if (cp <= 1.0)
                return -1.0;
            capMs = SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
                cp, totalWeightKg, gradePercent, terrainFactor, movementPhase);
        }

        // Walk 不套有氧巡航硬顶；Run 在 W′ 耗尽时不得超过 2.4
        if (movementPhase != 1 && capMs > SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS)
            capMs = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
        if (capMs > 0.05)
            return capMs;
        return -1.0;
    }

    //! 回退：按移动相位返回 v5 行军档理论上限（m/s）
    static float GetTheoreticalMaxSpeedMs(int movementPhase, float encumbranceSpeedPenalty)
    {
        float walk = SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs();
        float run = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
        float sprint = SCR_RSS_ConfigBridge.GetMarchSprintSpeedMs();

        float encMult = 1.0 - encumbranceSpeedPenalty;
        if (encMult < 0.5)
            encMult = 0.5;

        walk = walk * encMult;
        run = run * encMult;
        sprint = sprint * encMult;

        if (movementPhase == 3)
            return sprint;
        if (movementPhase == 2)
            return run;
        if (movementPhase == 1)
            return walk;
        return walk;
    }

    //! @deprecated v6 使用 GetEffectiveCriticalPowerWatts；保留作过渡
    static float GetMetabolicOverspeedFactor(float powerWatts)
    {
        float sustainable = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
        if (sustainable <= 1.0)
            sustainable = SCR_RSS_ConfigBridge.GetSustainableWatts();
        if (sustainable <= 1.0)
            sustainable = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        if (powerWatts <= sustainable)
            return 1.0;

        float ratio = sustainable / powerWatts;
        if (ratio < SCR_RSS_Constants.V5_MIN_METABOLIC_SPEED_FACTOR)
            ratio = SCR_RSS_Constants.V5_MIN_METABOLIC_SPEED_FACTOR;
        return ratio;
    }

    //! v6：代谢功率超可用功率时压速。
    //! Walk/Run：W′ 仍可用时不硬压到 CP（步态速度优先，超额由 W′/有氧承担）；
    //! W′ 耗尽后才反解到 CP。Sprint 仍用 availableP（含 W′ 预算）。
    static float GetMetabolicSpeedCapMs(
        float currentSpeedMs,
        int movementPhase,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool isExhausted,
        float worldTimeSec,
        SCR_RSS_CriticalPowerModel cpModel)
    {
        if (!SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            return -1.0;
        if (isExhausted)
            return -1.0;

        float cp = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        if (cpModel)
            cp = cpModel.GetEffectiveCriticalPowerWatts();
        else
            cp = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();

        float powerW = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            currentSpeedMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);

        bool isSprintPhase = false;
        if (movementPhase == 3)
            isSprintPhase = true;

        if (!isSprintPhase && cpModel)
        {
            if (IsWPrimePoolAvailableForOverspeed(cpModel))
                return -1.0;
        }

        float availableP = cp;
        if (cpModel)
            availableP = cpModel.GetAvailablePowerWatts(isSprintPhase, 0.017, worldTimeSec);

        if (powerW <= availableP + 1.0)
            return -1.0;

        float targetP = availableP;
        if (powerW > cp && !isSprintPhase)
            targetP = cp;

        float capMs = SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
            targetP, totalWeightKg, gradePercent, terrainFactor, movementPhase);
        // Walk 不套有氧巡航硬顶；Run 在 W′ 不可用时不得超过 2.4
        if (!isSprintPhase && movementPhase != 1)
        {
            if (capMs > SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS)
                capMs = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
        }
        return capMs;
    }

    //! 代谢限速 → 速度倍率（相对 engine base）
    static float GetMetabolicCorrectedSpeedMultiplier(
        float appliedSpeedMultiplier,
        float currentSpeedMs,
        int movementPhase,
        float encumbranceSpeedPenalty,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool isExhausted,
        float engineBaseMs,
        float worldTimeSec,
        SCR_RSS_CriticalPowerModel cpModel)
    {
        if (engineBaseMs <= 0.05)
            engineBaseMs = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

        float capMs = GetMetabolicSpeedCapMs(
            currentSpeedMs, movementPhase, totalWeightKg, gradePercent, terrainFactor, isExhausted, worldTimeSec, cpModel);
        if (capMs < 0.0)
            return appliedSpeedMultiplier;

        float appliedMs = appliedSpeedMultiplier * engineBaseMs;
        if (appliedMs <= capMs + 0.01)
            return appliedSpeedMultiplier;

        return Math.Clamp(capMs / engineBaseMs, 0.01, 3.0);
    }

    //! 已废弃双速差惩罚：记账与实测同用 v_meas 后差额恒为 0；保留入口以免调用方改签名。
    static float GetClientOverspeedExcessDrainPerSecond(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float wPrimePool01,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase,
        float effectiveCriticalPowerWatts = -1.0)
    {
        return 0.0;
    }
}
