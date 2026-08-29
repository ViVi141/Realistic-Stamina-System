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

    //! 步态覆盖已切引擎 Walk，且测速仍在 Walk 相位顶内。
    //! 此时 v_limit 常被徒步地板托在 1.0，但动画顶约 1.45，不得当超速烧 W′/STA。
    static bool IsWalkOverrideInBandCruise(bool cpWalkOverrideActive, float measuredSpeedMs)
    {
        if (!cpWalkOverrideActive)
            return false;
        float ceiling = SCR_RSS_Constants.ENGINE_WALK_TOP_MS
            + SCR_RSS_Constants.V6_WALK_OVERRIDE_IN_BAND_SLACK_MS;
        if (measuredSpeedMs <= ceiling)
            return true;
        return false;
    }

    //! TickPower / STA 税用：步态覆盖带内巡航视为未超速。
    static bool IsPhysOverspeedForAnaerobicTick(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        bool cpWalkOverrideActive)
    {
        if (IsWalkOverrideInBandCruise(cpWalkOverrideActive, measuredSpeedMs))
            return false;
        return IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs);
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

    //! 有氧巡航闩：W′ 见底后才锁 2.4；解除武装但池未空时返回 false（仍可跑、仍烧 W′）。
    static bool IsAerobicCruiseLatched(SCR_RSS_CriticalPowerModel cpModel)
    {
        if (!cpModel)
            return false;
        return cpModel.RefreshAndGetAerobicCruiseLatched();
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
        if (!IsAerobicCruiseLatched(cpModel))
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
            capMs = InvertCruiseCapMs(
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

    //! 巡航限速反解：坡度/地形对速度伺服比对消耗更温和，并托在负重徒步地板上。
    //! 消耗仍用实测坡度与 η；此处只决定 SetSpeedLimit。
    static float InvertCruiseCapMs(
        float criticalPowerWatts,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase)
    {
        if (criticalPowerWatts <= 1.0)
            return 0.0;

        float gradeForInvert = gradePercent;
        float gradeLim = SCR_RSS_Constants.V6_CP_INVERT_GRADE_ABS_MAX_PCT;
        if (gradeForInvert > gradeLim)
            gradeForInvert = gradeLim;
        if (gradeForInvert < -gradeLim)
            gradeForInvert = -gradeLim;

        float terrainForInvert = terrainFactor;
        float terrainMax = SCR_RSS_Constants.V6_CP_INVERT_TERRAIN_MAX;
        if (terrainForInvert > terrainMax)
            terrainForInvert = terrainMax;
        if (terrainForInvert < 0.5)
            terrainForInvert = 0.5;

        float capMs = SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
            criticalPowerWatts, totalWeightKg, gradeForInvert, terrainForInvert, movementPhase);

        float hikeFloor = SCR_RSS_Constants.V6_CP_HIKE_FLOOR_MS;
        if (capMs < hikeFloor)
            capMs = hikeFloor;
        return capMs;
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

    //! Run 巡航帽：
    //!   ≥ Run 地板 / 近地板软带 → 返回帽（仍在 Run 步态内，可写 SetSpeedLimit）；
    //!   更深灰区：硬钳开则抬到 Run 地板；否则返回 -1（不越步态压速）。
    //!   游戏侧 W′ 空时由 V6_CP_OUT_OF_BAND_WALK_OVERRIDE 切引擎 Walk 档。
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

        float walkTopMs = SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs();
        float softRunFloor = floorMs - SCR_RSS_Constants.V6_RUN_SOFT_BAND_BELOW_FLOOR_MS;
        if (softRunFloor < walkTopMs)
            softRunFloor = walkTopMs;
        if (capMs >= softRunFloor)
            return capMs;

        // 掉出 Run 步态带：禁止把 Run 相位 SetSpeedLimit 拧到 Walk/爬行。
        return -1.0;
    }

    //! 步态覆盖触发：Resolve 返回 -1，或软带内帽仍低于 Run 地板。
    //! 0 = 本 tick 未算（W′ 武装跳过巡航），不算掉带。
    static bool IsRunCruiseCapOutOfBand(float capMs)
    {
        if (capMs < -0.01)
            return true;
        if (capMs <= 0.05)
            return false;
        if (capMs < SCR_RSS_Constants.V6_RUN_GAIT_FLOOR_MS)
            return true;
        return false;
    }

    //! 限速倍率不得低于当前相位顶速的 k 倍（防滑步）。
    //! 条空跛行也托下限：把 1 m/s 写进 Run 相位 = 滑步。切 Walk 后相位顶约 1.45，0.5× 与跛行同量级。
    //! @param isExhausted 保留签名；下限对跛行同样生效。
    static float ClampSpeedLimitFractionToGaitBand(float frac, bool isExhausted)
    {
        float minFrac = SCR_RSS_Constants.V6_GAIT_SPEED_LIMIT_MIN_FRAC;
        if (minFrac < 0.2)
            minFrac = 0.2;
        if (minFrac > 0.9)
            minFrac = 0.9;
        if (frac < minFrac)
            frac = minFrac;
        if (isExhausted)
            return frac;
        return frac;
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

    //! v6：代谢功率超可用功率时压速。
    //! W′ 见底闩巡航：Run 套 CP∩有氧巡航顶；武装或剩余 W′ 的纯 Run 不二次压顶（由 W′ 买单）。
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
        bool cruiseLatched = false;
        if (cpModel)
            cruiseLatched = IsAerobicCruiseLatched(cpModel);
        // 剩余 W′：不套 2.4 巡航（让玩家跑完池）。见底闩上后才压。
        if (movementPhase != 1 && !cruiseLatched)
            return -1.0;
        // W′ 武装纯 Run：勿再压回 2.0~2.4（与 UpdateCoordinator 对齐）
        if (overspeedArmed && movementPhase == 2)
            return -1.0;
        // 巡航闩上后一律按 Run/CP 压速，忽略引擎仍停在 Sprint 相位（按住 Shift 门禁时常见）
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
                if (cruiseOnly > 0.05 && evalSpeed > cruiseOnly + 0.05)
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

        // 与 UpdateCoordinator / 孪生 InvertCruiseCapMs 同路：坡度钳 + η 钳 + Walk 徒步地板。
        // 禁止走裸 InvertSpeedForPowerWatts：否则 20% 坡 + η=1.44 会把 Walk 拧到 ~0.5 m/s（滑步）。
        float capMs = InvertCruiseCapMs(
            targetP, totalWeightKg, gradePercent, terrainFactor, invertPhase);
        // Run：平路上限 + 带内保留；掉出 Run 带则跳过越步态限速
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
            return ClampSpeedLimitFractionToGaitBand(appliedSpeedMultiplier, isExhausted);

        float appliedMs = appliedSpeedMultiplier * engineBaseMs;
        float nextFrac = appliedSpeedMultiplier;
        if (appliedMs > capMs + 0.01)
            nextFrac = Math.Clamp(capMs / engineBaseMs, 0.01, 3.0);
        // 已低于帽时也要托步态下限：否则 coordinator 写出 0.37、帽=0.52 会 early-return 跳过 0.5×
        nextFrac = ClampSpeedLimitFractionToGaitBand(nextFrac, isExhausted);
        float deadband = SCR_RSS_Constants.V6_SPEED_LIMIT_DEADBAND_FRAC;
        if (Math.AbsFloat(nextFrac - appliedSpeedMultiplier) <= deadband)
            return appliedSpeedMultiplier;
        return nextFrac;
    }

    //! STA 透支附加罚（%/s）。
    //! - 代谢限速开：武装只烧 W′；解除武装后 Run/Sprint 按 P−CP 步态税（即使 phys 略超 v_limit）。
    //!   Walk 超限速仍走 P(v)−P(limit)×12。步态覆盖且测速在 Walk 顶内：免此税（假超速）。
    //! - 代谢限速关：不压速，解除武装后按 P(v)−CP 罚 STA；武装时只烧 W′。
    //! @param wPrimeOverspeedArmed 须与 TickPower 同用施密特武装态（勿用 pool>rearm 近似，
    //!   否则 25–60% 滞回带会 W′ 与 STA 税双计）。
    //! @param cpWalkOverrideActive 步态覆盖且带内巡航时免相对徒步地板的假超速税。
    static float GetClientOverspeedExcessDrainPerSecond(
        float measuredSpeedMs,
        float appliedSpeedLimitMs,
        float wPrimePool01,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase,
        float effectiveCriticalPowerWatts = -1.0,
        bool wPrimeOverspeedArmed = false,
        bool cpWalkOverrideActive = false)
    {
        if (IsWalkOverrideInBandCruise(cpWalkOverrideActive, measuredSpeedMs))
            return 0.0;

        float pMeas = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            measuredSpeedMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);

        bool armed = wPrimeOverspeedArmed;
        if (!armed)
            armed = IsWPrimePoolAvailableForOverspeed(wPrimePool01);

        bool drainOnlyMode = !SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled();
        float unpaidW = 0.0;
        bool useGaitExcessTax = false;

        if (drainOnlyMode)
        {
            // 不伺服速度：武装透支由 W′ 承担；解除武装后超额相对 CP 扣 STA
            if (armed)
                return 0.0;
            if (effectiveCriticalPowerWatts <= 1.0)
                return 0.0;
            unpaidW = pMeas - effectiveCriticalPowerWatts;
            useGaitExcessTax = true;
        }
        else
        {
            // 与 TickPower 一致：施密特武装且 P>CP → 只烧 W′，免 STA 税
            if (armed)
            {
                if (!IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs))
                    return 0.0;
                if (effectiveCriticalPowerWatts > 1.0 && pMeas > effectiveCriticalPowerWatts + 1.0)
                    return 0.0;
                float pLimitArmed = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                    appliedSpeedLimitMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
                unpaidW = pMeas - pLimitArmed;
            }
            else if (IsMetabolicOverspeedAccounting(measuredSpeedMs, appliedSpeedLimitMs)
                && movementPhase < 2)
            {
                // Walk：相对 v_limit 的小超额，走 12×。Run/Sprint 即使 phys 略超限速
                // 仍按 P−CP 步态税，避免「贴帽满税、滑出反而更便宜」。
                float pLimit = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                    appliedSpeedLimitMs, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
                unpaidW = pMeas - pLimit;
            }
            else
            {
                // 帽内、未越步态压速、或 Run 物理略超限速：STA 承担 P−CP
                if (effectiveCriticalPowerWatts <= 1.0)
                    return 0.0;
                unpaidW = pMeas - effectiveCriticalPowerWatts;
                useGaitExcessTax = true;
            }
        }

        if (unpaidW <= 1.0)
            return 0.0;

        float perSec = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(unpaidW, -1.0);
        float loadKg = Math.Max(totalWeightKg - SCR_RSS_MetabolismMath.CHARACTER_WEIGHT, 0.0);
        perSec = perSec * SCR_RSS_MetabolismModel.GetLoadedGaitStaminaDrainMultiplier(
            loadKg, movementPhase);
        float taxMult = SCR_RSS_Constants.V6_OVERSPEED_STA_TAX_MULT;
        if (useGaitExcessTax && movementPhase >= 2)
            taxMult = SCR_RSS_Constants.V6_GAIT_EXCESS_STA_TAX_MULT;
        else if (drainOnlyMode && !useGaitExcessTax)
            taxMult = SCR_RSS_Constants.V6_CP_EXCESS_STA_TAX_MULT;
        else if (drainOnlyMode && movementPhase < 2)
            taxMult = SCR_RSS_Constants.V6_CP_EXCESS_STA_TAX_MULT;
        perSec = perSec * taxMult;
        if (useGaitExcessTax && movementPhase >= 2)
        {
            float taxMax = SCR_RSS_Constants.V6_GAIT_EXCESS_STA_TAX_MAX_PER_SEC;
            if (perSec > taxMax)
                perSec = taxMax;
        }
        return perSec;
    }
}
