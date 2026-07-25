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
        float cpWatts = -1.0;
        if (cpModel)
            cpWatts = cpModel.GetEffectiveCriticalPowerWatts();
        capMs = ResolveRunCruiseCapMs(
            capMs, movementPhase, gradePercent, totalWeightKg, terrainFactor, cpWatts);
        if (capMs > 0.05)
            return capMs;
        return -1.0;
    }

    //! 是否启用「低于 Run 地板 → 降 Walk 带」（硬钳开时强制关闭，防滑步）
    static bool IsRunGaitDemoteToWalkEnabled()
    {
        if (!SCR_RSS_Constants.V6_RUN_GAIT_DEMOTE_TO_WALK)
            return false;
        if (SCR_RSS_Constants.V6_APPLY_HORIZONTAL_SPEED_CLAMP)
            return false;
        return true;
    }

    //! Run 巡航帽三带：
    //!   ≥ Run 地板 → 保留；
    //!   Walk 顶～Run 地板 → 保留代谢反解（软 Run，禁止悬崖降到 1.4）；
    //!   < Walk 顶 → 降 Walk（硬钳开时改抬到 Run 地板）。
    //! @param rawCapMs CP/巡航反解（可已含平路 2.4 帽，亦可未含）
    static float ResolveRunCruiseCapMs(
        float rawCapMs,
        int movementPhase,
        float gradePercent,
        float totalWeightKg,
        float terrainFactor,
        float criticalPowerWatts)
    {
        if (movementPhase == 1)
            return rawCapMs;
        if (rawCapMs <= 0.05)
            return rawCapMs;

        float capMs = rawCapMs;
        if (gradePercent >= 0.0)
        {
            if (capMs > SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS)
                capMs = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
        }

        float floorMs = SCR_RSS_Constants.V6_RUN_GAIT_FLOOR_MS;
        if (capMs >= floorMs)
            return capMs;

        if (!IsRunGaitDemoteToWalkEnabled())
            return floorMs;

        // 灰区：代谢还能撑过 Walk 顶，只是略低于 Run 地板 → 保留反解，勿悬崖降档
        float walkTopMs = SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs();
        if (capMs >= walkTopMs)
            return capMs;

        float walkCapMs = capMs;
        if (criticalPowerWatts > 1.0)
        {
            walkCapMs = SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
                criticalPowerWatts, totalWeightKg, gradePercent, terrainFactor, 1);
        }

        if (walkCapMs > walkTopMs)
            walkCapMs = walkTopMs;

        float walkMinMs = SCR_RSS_Constants.V6_WALK_START_MIN_MS;
        if (walkCapMs < walkMinMs)
            walkCapMs = walkMinMs;

        return walkCapMs;
    }

    //! @deprecated 请用 ResolveRunCruiseCapMs；保留：无体重/CP 上下文时的步态对齐
    static float ApplyRunGaitFloorToCruiseCapMs(float capMs, int movementPhase)
    {
        return ResolveRunCruiseCapMs(capMs, movementPhase, 0.0, 0.0, 1.0, -1.0);
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
    //! W′ 解除武装：Run 套 CP∩有氧巡航顶；W′ 武装的纯 Run 不二次压顶（由 W′ 买单，减滑步）。
    //! Sprint+武装用 availableP。
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

        bool overspeedArmed = true;
        if (cpModel)
            overspeedArmed = IsWPrimePoolAvailableForOverspeed(cpModel);
        // W′ 武装纯 Run：勿再压回 2.0~2.4（与 UpdateCoordinator 对齐）
        if (overspeedArmed && movementPhase == 2)
            return -1.0;
        // 解除武装后一律按 Run/CP 巡航压速，忽略引擎仍停在 Sprint 相位（按住 Shift 门禁时常见）
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
                float cruiseOnly = ResolveRunCruiseCapMs(
                    SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS,
                    2,
                    gradePercent,
                    totalWeightKg,
                    terrainFactor,
                    cp);
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
        // Run：平路上限 + 低于地板则降 Walk；Walk 反解本身不再套 2.4 平路帽
        if (!isSprintPhase && invertPhase != 1)
        {
            capMs = ResolveRunCruiseCapMs(
                capMs, invertPhase, gradePercent, totalWeightKg, terrainFactor, cp);
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

    //! 物理超速 STA 附加罚（%/s），按 P(v_meas)−P(v_limit)。
    //! @param wPrimeOverspeedArmed 须与 TickPower 同用施密特武装态（勿用 pool>rearm 近似，
    //!   否则 25–60% 滞回带会 W′ 与 STA 税双计）。
    static float GetClientOverspeedExcessDrainPerSecond(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float wPrimePool01,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase,
        float effectiveCriticalPowerWatts = -1.0,
        bool wPrimeOverspeedArmed = false)
    {
        if (!IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs))
            return 0.0;

        float pMeas = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            measuredSpeedMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);

        // 与 TickPower 一致：施密特武装且 P>CP → 只烧 W′，免 STA 税
        bool armed = wPrimeOverspeedArmed;
        if (!armed)
            armed = IsWPrimePoolAvailableForOverspeed(wPrimePool01);
        if (armed)
        {
            if (effectiveCriticalPowerWatts > 1.0 && pMeas > effectiveCriticalPowerWatts + 1.0)
                return 0.0;
        }

        float pLimit = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            appliedSpeedLimitMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
        float unpaidW = pMeas - pLimit;
        if (unpaidW <= 1.0)
            return 0.0;

        float perSec = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(unpaidW, -1.0);
        float loadKg = Math.Max(totalWeightKg - SCR_RSS_MetabolismMath.CHARACTER_WEIGHT, 0.0);
        perSec = perSec * SCR_RSS_MetabolismModel.GetLoadedGaitStaminaDrainMultiplier(
            loadKg, movementPhase);
        perSec = perSec * SCR_RSS_Constants.V6_OVERSPEED_STA_TAX_MULT;
        return perSec;
    }
}
