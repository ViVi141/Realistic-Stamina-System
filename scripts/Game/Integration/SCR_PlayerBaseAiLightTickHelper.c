//! AI 廉价限速：所有服端 AI 共用（无论是否算体力消耗）。
//! 跳过地形射线 / 环境因子 / UpdateCoordinator.UpdateSpeed —— 这是「Speed」选项卡顿主因。
//!
//! 限速语义与玩家 UpdateSpeed 对齐：绝对目标 m/s ÷ 当前相位引擎顶 → SetSpeedLimit。
//! 禁止把 CalculateV6PhaseSpeedMultiplier（相对 GAME_MAX_SPEED）直接当相位分数——
//! 冲刺时会得到 ≈1.0，Chimera 会移除限速源，AI 变成原版满速冲刺。
//!
//! 6.2.32：与玩家脚程对齐（仍不跑 UpdateSpeed）
//!   - 始终 Tobler 坡度缩放（GetRawSlopeAngle / 传入 grade）
//!   - 开消耗时：Sprint→GetV6SprintSpeedMs；Walk/巡航闩→InvertCruiseCapMs + 地形
//! 6.2.33：负重意图项与玩家同形；默认禁止 BT 冲刺顶（需 Fatigue Behaviors）
//! 6.2.35：应用层改走 Agent MovementSpeed Setting（BT 持久裁剪）+ 再写 SetSpeedLimit

class SCR_PlayerBaseAiLightTickHelper
{
    //! 对 AI 应用负重+相位限速（不读配置开关）。
    //! @return true 表示已处理（调用方为 AI）
    static bool ApplyCheapAiSpeed(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        SCR_RSS_EncumbranceCache encumbranceCache,
        float animSpeedCompensation,
        out float outAppliedFrac,
        out float outEncumbrancePenalty,
        out float outStaminaPercent,
        out int outPhase,
        out bool outExhausted)
    {
        float unusedTarget = 0.0;
        return ApplyCheapAiSpeedEx(
            ctrl,
            owner,
            encumbranceCache,
            animSpeedCompensation,
            0.0,
            1.0,
            false,
            outAppliedFrac,
            outEncumbrancePenalty,
            outStaminaPercent,
            outPhase,
            outExhausted,
            unusedTarget);
    }

    //! 带坡度/地形/代谢帽的廉价限速（AI 消耗管线用）。
    //! @param gradePercent 已估 grade%（管线缓存）；0 且 useGradeHint=false 时内部自取坡度
    //! @param terrainFactor 稀采样地形系数
    //! @param useMetabolicCaps true=开消耗时套 CP/Sprint 反解
    //! @param useGradeHint true=用传入 gradePercent 做 Tobler/CP（与消耗同源）
    static bool ApplyCheapAiSpeedEx(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        SCR_RSS_EncumbranceCache encumbranceCache,
        float animSpeedCompensation,
        float gradePercent,
        float terrainFactor,
        bool useMetabolicCaps,
        out float outAppliedFrac,
        out float outEncumbrancePenalty,
        out float outStaminaPercent,
        out int outPhase,
        out bool outExhausted,
        out float outTargetMs)
    {
        outAppliedFrac = 1.0;
        outEncumbrancePenalty = 0.0;
        outStaminaPercent = 1.0;
        outPhase = 2;
        outExhausted = false;
        outTargetMs = 0.0;

        if (!ctrl || !owner)
            return false;
        if (ctrl.IsPlayerControlled())
            return false;

        float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
        if (encumbranceCache)
        {
            encumbranceCache.CheckAndUpdate();
            outEncumbrancePenalty = encumbranceCache.GetSpeedPenaltyFraction();
            outEncumbrancePenalty = outEncumbrancePenalty
                * SCR_RSS_ConfigBridge.GetCustomEncumbranceSpeedPenaltyMultiplier();
            outEncumbrancePenalty = Math.Clamp(outEncumbrancePenalty, 0.0, maxPenalty);
        }

        outStaminaPercent = ctrl.GetRssAerobicPercent();
        outStaminaPercent = Math.Clamp(outStaminaPercent, 0.0, 1.0);
        outExhausted = SCR_RSS_MetabolismMath.IsExhausted(outStaminaPercent);

        int phaseNow = ctrl.GetCurrentMovementPhase();
        int effectivePhase = phaseNow;
        if (effectivePhase < 1)
            effectivePhase = 2;
        if (ctrl.IsSprinting())
            effectivePhase = 3;
        outPhase = effectivePhase;

        // 与玩家 UpdateSpeed 同形：base × (1 + 意图速比)〔冲刺再缩放〕
        bool wantSprint = false;
        if (effectivePhase == 3)
            wantSprint = true;
        float speedRatio = SCR_RSS_SpeedCalculator.GetEncumbranceIntentSpeedRatio(
            effectivePhase, wantSprint);
        float intentEnc = outEncumbrancePenalty * (1.0 + speedRatio);
        if (wantSprint)
            intentEnc = SCR_RSS_SpeedCalculator.ScaleSprintEncumbrancePenalty(intentEnc);
        intentEnc = Math.Clamp(intentEnc, 0.0, maxPenalty);

        float encMult = 1.0 - intentEnc;
        if (encMult < 0.5)
            encMult = 0.5;

        float walkMs = SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs() * encMult;
        float runMs = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs() * encMult;
        float sprintMs = SCR_RSS_SpeedCalculator.GetEnsuredMarchSprintSpeedMs() * encMult;

        float targetMs = runMs;
        if (effectivePhase == 1)
            targetMs = walkMs;
        else if (effectivePhase == 3)
            targetMs = sprintMs;

        // 冲刺顶策略：
        // - 关消耗：无体力经济 → 永远压到 Run
        // - 开消耗但未开 Fatigue Behaviors：BT 常默认冲刺，玩家慢跑同行会「快很多」→ 压到 Run
        // - 开消耗且 Fatigue Behaviors：才允许 W′ 门控冲刺（与战斗层一致）
        bool drainDisabled = SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled();
        bool allowAiSprintTarget = false;
        if (!drainDisabled)
        {
            if (SCR_RSS_ConfigBridge.IsAIStaminaCombatEffectsEnabled())
            {
                if (ctrl.GetRssSprintAllowed())
                    allowAiSprintTarget = true;
            }
        }

        if (drainDisabled)
            useMetabolicCaps = false;

        if (!allowAiSprintTarget)
        {
            if (targetMs > runMs)
                targetMs = runMs;
            if (effectivePhase == 3)
            {
                outPhase = 2;
                effectivePhase = 2;
            }
        }

        if (outExhausted)
        {
            targetMs = SCR_RSS_MetabolismMath.GetDynamicLimpSpeedMs(outEncumbrancePenalty);
            outPhase = 1;
            effectivePhase = 1;
            useMetabolicCaps = false;
        }

        // 坡度：与玩家 Tobler 同锚（始终，含默认仅限速）
        float slopeAngleDeg = 0.0;
        float gradeForCaps = gradePercent;
        if (useMetabolicCaps)
        {
            // 管线传入的 grade 与消耗同源
            slopeAngleDeg = Math.Atan2(gradeForCaps, 100.0) * Math.RAD2DEG;
        }
        else
        {
            vector velHint = vector.Zero;
            slopeAngleDeg = SCR_RSS_SpeedCalculator.GetRawSlopeAngle(ctrl, velHint);
            gradeForCaps = SCR_RSS_SpeedCalculator.GradePercentFromSlopeDegrees(slopeAngleDeg);
        }
        targetMs = SCR_RSS_SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
            targetMs, slopeAngleDeg);

        float tf = terrainFactor;
        if (tf < 0.5)
            tf = 0.5;
        if (tf > 3.0)
            tf = 3.0;

        bool cruiseLatched = false;
        if (useMetabolicCaps && !outExhausted)
        {
            float gearKg = 0.0;
            if (encumbranceCache && encumbranceCache.IsCacheValid())
                gearKg = encumbranceCache.GetCurrentWeight();
            float totalWeightKg = gearKg + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;

            float worldTimeSec = 0.0;
            SCR_RSS_RuntimeGuard.TryGetWorldTimeSec(worldTimeSec);

            SCR_RSS_AnaerobicBurst ana = ctrl.RSS_GetWPrimeBurst();
            SCR_RSS_CriticalPowerModel cpModel = null;
            if (ana)
                cpModel = ana.GetCpModel();

            float gradeClamped = SCR_RSS_SpeedBridge.ClampGradePercentForMetabolicSpeed(gradeForCaps);

            if (effectivePhase == 3 && cpModel)
            {
                float sprintCap = SCR_RSS_SpeedCalculator.GetV6SprintSpeedMs(
                    outEncumbrancePenalty,
                    totalWeightKg,
                    gradeClamped,
                    tf,
                    cpModel,
                    worldTimeSec,
                    0.2);
                if (sprintCap > 0.05 && sprintCap < targetMs)
                    targetMs = sprintCap;
            }

            if (cpModel)
                cruiseLatched = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(cpModel);

            bool applyCruise = false;
            if (SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled() && cpModel)
            {
                if (effectivePhase == 1)
                    applyCruise = true;
                else if (cruiseLatched)
                    applyCruise = true;
            }

            if (applyCruise)
            {
                float cpEffW = cpModel.GetEffectiveCriticalPowerWatts();
                int invertPhase = effectivePhase;
                if (invertPhase < 1)
                    invertPhase = 2;
                if (invertPhase == 3)
                    invertPhase = 2;
                float rawCap = SCR_RSS_DrainCalculator.InvertCruiseCapMs(
                    cpEffW, totalWeightKg, gradeClamped, tf, invertPhase);
                float resolved = SCR_RSS_DrainCalculator.ResolveRunCruiseCapMs(
                    rawCap,
                    invertPhase,
                    gradeClamped,
                    totalWeightKg,
                    tf,
                    cpEffW);
                if (resolved < -0.01)
                {
                    float walkCap = SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs() * encMult;
                    walkCap = SCR_RSS_SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
                        walkCap, slopeAngleDeg);
                    targetMs = walkCap;
                    outPhase = 1;
                    effectivePhase = 1;
                }
                else if (resolved > 0.05 && resolved < targetMs)
                {
                    targetMs = resolved;
                    if (effectivePhase == 3)
                    {
                        outPhase = 2;
                        effectivePhase = 2;
                    }
                }
            }
        }

        if (animSpeedCompensation > 0.01)
            targetMs = targetMs * animSpeedCompensation;

        float customSprint = SCR_RSS_ConfigBridge.GetCustomSprintSpeedMultiplier();
        if (customSprint != 1.0 && effectivePhase == 3 && !outExhausted)
        {
            if (!drainDisabled)
            {
                if (ctrl.GetRssSprintAllowed())
                    targetMs = targetMs * customSprint;
            }
        }

        // —— 应用层：SCENARIO Setting 锁步态（压过航点 BT），再 SetSpeedLimit ——
        // 巡航闩 / 跛行 → 固定钉 WALK；禁止冲刺 → Range 最高 RUN；允许冲刺 → SPRINT
        EMovementType maxGait = EMovementType.RUN;
        if (outExhausted)
            maxGait = EMovementType.WALK;
        else if (cruiseLatched)
            maxGait = EMovementType.WALK;
        else if (effectivePhase == 1)
            maxGait = EMovementType.WALK;
        else if (allowAiSprintTarget && effectivePhase == 3)
            maxGait = EMovementType.SPRINT;
        else
            maxGait = EMovementType.RUN;

        if (maxGait == EMovementType.WALK)
            outPhase = 1;
        else if (maxGait == EMovementType.SPRINT)
            outPhase = 3;
        else
            outPhase = 2;

        bool applyInstant = false;
        if (useMetabolicCaps)
            applyInstant = true;
        if (cruiseLatched)
            applyInstant = true;
        if (outExhausted)
            applyInstant = true;

        float frac = 0.999;
        SCR_RSS_AIMovementApply.ApplyTargetMs(
            ctrl, owner, targetMs, maxGait, applyInstant, frac);

        outAppliedFrac = frac;
        outTargetMs = targetMs;
        return true;
    }

    //! DisableAIStaminaCalc：廉价限速后结束整 tick（不算消耗）。
    static bool TryApplyLightSpeedLimit(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        SCR_RSS_EncumbranceCache encumbranceCache,
        float animSpeedCompensation,
        out float outAppliedFrac)
    {
        outAppliedFrac = 1.0;
        if (!SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
            return false;

        float enc = 0.0;
        float sta = 1.0;
        int phase = 2;
        bool exhausted = false;
        return ApplyCheapAiSpeed(
            ctrl, owner, encumbranceCache, animSpeedCompensation,
            outAppliedFrac, enc, sta, phase, exhausted);
    }
}
