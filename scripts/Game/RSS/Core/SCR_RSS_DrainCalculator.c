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

    //! 疲劳积分功率（W）：与 EPOC 相同，用限速内意图速度。
    //! 硬钳关时 v_meas 可远超 v_limit；若按跑飞速度积 If，会在解除武装巡航时虚高疲劳、压低 CP。
    static float GetMetabolicFatiguePowerWatts(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase)
    {
        float vFat = GetEpocSampleVelocityMs(measuredSpeedMs, appliedSpeedLimitMs);
        return SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            vFat, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
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
    //! 无 CP 模型时的无状态近似：须过再武装带（非 disarm 线），避免 25–60% 误判仍可超速
    static bool IsWPrimePoolAvailableForOverspeed(float wPrimePool01)
    {
        float threshold = SCR_RSS_ConfigBridge.GetWPrimeSprintEnableThreshold();
        float rearmAt = threshold + SCR_RSS_Constants.V6_WPRIME_OVERSPEED_REARM;
        return wPrimePool01 > rearmAt;
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

        // Walk 不套有氧巡航硬顶；平路/上坡 Run 在 W′ 耗尽时不得超过 2.4；下坡不套平路帽
        if (movementPhase != 1 && gradePercent >= 0.0)
        {
            if (capMs > SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS)
                capMs = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
        }
        capMs = ApplyRunGaitFloorToCruiseCapMs(capMs, movementPhase);
        if (capMs > 0.05)
            return capMs;
        return -1.0;
    }

    //! Run 意图：CP 巡航不得压进 Walk 动画带（否则限速+物理钳 → 滑步/假 Walk）
    static float ApplyRunGaitFloorToCruiseCapMs(float capMs, int movementPhase)
    {
        if (movementPhase == 1)
            return capMs;
        if (capMs <= 0.05)
            return capMs;
        float floorMs = SCR_RSS_Constants.V6_RUN_GAIT_FLOOR_MS;
        if (capMs < floorMs)
            return floorMs;
        return capMs;
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
    //! Walk/Run：一律按 CP∩有氧巡航顶压速（W′ 不得用来维持引擎 Run 顶）；Sprint+武装才用 availableP。
    //! @param speedForPowerEvalMs 用于判断是否超功率的速度；应优先用意图限速，避免 v_meas 噪声追着压速
    static float GetMetabolicSpeedCapMs(
        float currentSpeedMs,
        int movementPhase,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool isExhausted,
        float worldTimeSec,
        SCR_RSS_CriticalPowerModel cpModel,
        float speedForPowerEvalMs = -1.0)
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

        float evalSpeed = currentSpeedMs;
        if (speedForPowerEvalMs >= 0.0)
            evalSpeed = speedForPowerEvalMs;

        float powerW = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            evalSpeed, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);

        bool isSprintPhase = false;
        if (movementPhase == 3)
            isSprintPhase = true;

        // 解除武装后一律按 Run/CP 巡航压速，忽略引擎仍停在 Sprint 相位（按住 Shift 门禁时常见）
        bool overspeedArmed = true;
        if (cpModel)
            overspeedArmed = IsWPrimePoolAvailableForOverspeed(cpModel);
        if (!overspeedArmed)
            isSprintPhase = false;

        float availableP = cp;
        if (cpModel && isSprintPhase)
            availableP = cpModel.GetAvailablePowerWatts(true, 0.017, worldTimeSec);

        if (powerW <= availableP + 1.0)
        {
            // 即使功率未超 availableP，非冲刺仍须钳在有氧巡航顶以下（防引擎 Run 顶 ~3.8）
            if (!isSprintPhase && movementPhase != 1 && gradePercent >= 0.0)
            {
                float cruiseOnly = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
                cruiseOnly = ApplyRunGaitFloorToCruiseCapMs(cruiseOnly, 2);
                if (evalSpeed > cruiseOnly + 0.05)
                    return cruiseOnly;
            }
            return -1.0;
        }

        float targetP = availableP;
        if (powerW > cp && !isSprintPhase)
            targetP = cp;

        int invertPhase = movementPhase;
        if (!isSprintPhase)
        {
            invertPhase = 2;
            if (movementPhase == 1)
                invertPhase = 1;
        }

        float capMs = SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
            targetP, totalWeightKg, gradePercent, terrainFactor, invertPhase);
        // Walk 不套有氧巡航硬顶；平路/上坡 Run 不得超过 2.4；下坡只按 CP 反解
        if (!isSprintPhase && invertPhase != 1)
        {
            if (gradePercent >= 0.0)
            {
                if (capMs > SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS)
                    capMs = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
            }
            capMs = ApplyRunGaitFloorToCruiseCapMs(capMs, invertPhase);
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
        SCR_RSS_CriticalPowerModel cpModel,
        float appliedSpeedLimitMs = -1.0)
    {
        if (engineBaseMs <= 0.05)
            engineBaseMs = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

        // 功率判定用意图/本帧限速，禁止用 v_meas 追着压（否则 Sprint/限速自激 Bang-Bang）
        float speedForEval = appliedSpeedMultiplier * engineBaseMs;
        if (appliedSpeedLimitMs > 0.05)
            speedForEval = appliedSpeedLimitMs;

        float capMs = GetMetabolicSpeedCapMs(
            currentSpeedMs,
            movementPhase,
            totalWeightKg,
            gradePercent,
            terrainFactor,
            isExhausted,
            worldTimeSec,
            cpModel,
            speedForEval);
        if (capMs < 0.0)
            return appliedSpeedMultiplier;

        float appliedMs = appliedSpeedMultiplier * engineBaseMs;
        if (appliedMs <= capMs + 0.01)
            return appliedSpeedMultiplier;

        float nextFrac = Math.Clamp(capMs / engineBaseMs, 0.01, 3.0);
        float deadband = SCR_RSS_Constants.V6_SPEED_LIMIT_DEADBAND_FRAC;
        if (Math.AbsFloat(nextFrac - appliedSpeedMultiplier) <= deadband)
            return appliedSpeedMultiplier;
        return nextFrac;
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
