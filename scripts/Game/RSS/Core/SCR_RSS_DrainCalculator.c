//! v6 消耗测速与代谢限速：v_drain = min(v_meas, v_limit_applied)；超限速时记账用 v_meas

class SCR_RSS_DrainCalculator
{
    //! @param measuredSpeedMs GetVelocity 水平模长（m/s）
    //! @param appliedSpeedLimitMs 当前 RSS 已应用的绝对速度上限（m/s）；<=0 时回退相位理论档
    //! @return 用于代谢模型的速度（m/s）
    static float GetDrainVelocityMs(float measuredSpeedMs, float appliedSpeedLimitMs)
    {
        if (measuredSpeedMs < 0.0)
            measuredSpeedMs = 0.0;

        float capMs = appliedSpeedLimitMs;
        if (capMs <= 0.05)
            return measuredSpeedMs;

        if (measuredSpeedMs > capMs)
            return capMs;
        return measuredSpeedMs;
    }

    //! 代谢记账速度：未超 v_limit 同 GetDrainVelocityMs；超限时按 v_meas（W′ 放电）
    static float GetMetabolicAccountingVelocityMs(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float wPrimePool01 = 1.0)
    {
        if (measuredSpeedMs < 0.0)
            measuredSpeedMs = 0.0;

        if (appliedSpeedLimitMs > 0.05 && measuredSpeedMs > appliedSpeedLimitMs + SCR_RSS_Constants.V6_OVERSPEED_ACCOUNTING_EPS_MPS)
        {
            if (!IsWPrimePoolAvailableForOverspeed(wPrimePool01))
                return GetDrainVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
            return measuredSpeedMs;
        }

        return GetDrainVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
    }

    //! 代谢记账功率（W）：超限速时用实测速度（W′ / 调试）；有氧仍 min(P,CP)
    static float GetMetabolicAccountingPowerWatts(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase,
        float wPrimePool01 = 1.0)
    {
        float vAcct = GetMetabolicAccountingVelocityMs(measuredSpeedMs, appliedSpeedLimitMs, wPrimePool01);
        return SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            vAcct, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
    }

    //! 疲劳积分功率（W）：始终 v_drain；超限速超额由 W′ 承担，避免 kW 级功率灌入 I(t)
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

    static bool IsMetabolicOverspeedAccounting(float measuredSpeedMs, float appliedSpeedLimitMs)
    {
        if (appliedSpeedLimitMs <= 0.05)
            return false;
        return measuredSpeedMs > appliedSpeedLimitMs + SCR_RSS_Constants.V6_OVERSPEED_ACCOUNTING_EPS_MPS;
    }

    //! W′ 池是否仍可支撑「超限速按 v_meas 记账」
    static bool IsWPrimePoolAvailableForOverspeed(float wPrimePool01)
    {
        float threshold = SCR_RSS_ConfigBridge.GetWPrimeSprintEnableThreshold();
        return wPrimePool01 > threshold;
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
        if (!IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs))
            return -1.0;
        if (IsWPrimePoolAvailableForOverspeed(wPrimePool01))
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
        if (capMs > 0.05)
            return capMs;

        float cp = cpModel.GetEffectiveCriticalPowerWatts();
        if (cp <= 1.0)
            return -1.0;

        return SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
            cp, totalWeightKg, gradePercent, terrainFactor, movementPhase);
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

    //! v6：代谢功率超 CP 时，将绝对速度上限压至 invert(P=CP)
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
        if (isExhausted)
            return -1.0;

        float cp = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        if (cpModel)
            cp = cpModel.GetEffectiveCriticalPowerWatts();
        else
            cp = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();

        float powerW = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            currentSpeedMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);

        float availableP = cp;
        if (cpModel)
            availableP = cpModel.GetAvailablePowerWatts(movementPhase == 3, 0.017, worldTimeSec);

        if (powerW <= availableP + 1.0)
            return -1.0;

        float targetP = availableP;
        if (powerW > cp && movementPhase != 3)
            targetP = cp;

        return SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
            targetP, totalWeightKg, gradePercent, terrainFactor, movementPhase);
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

    //! W′ 耗尽且 client 仍超 v_limit：超出 v_drain 功率对应的有氧消耗（%/s，可突破疲劳 cap）
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
        if (!IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs))
            return 0.0;
        if (IsWPrimePoolAvailableForOverspeed(wPrimePool01))
            return 0.0;

        float pMeas = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            measuredSpeedMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
        float vDrain = GetDrainVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
        float pDrain = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            vDrain, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
        float excessW = pMeas - pDrain;
        if (excessW <= 1.0)
            return 0.0;

        return SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(
            excessW, -1.0);
    }
}
