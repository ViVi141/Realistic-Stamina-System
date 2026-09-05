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
//!   限速只走 SetSpeedLimit + SetMovementTypeWanted（AI 专用），不用玩家水平硬钳

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
                ForceAiMovementTypeWanted(owner, EMovementType.RUN);
            }
        }

        if (outExhausted)
        {
            targetMs = SCR_RSS_MetabolismMath.GetDynamicLimpSpeedMs(outEncumbrancePenalty);
            outPhase = 1;
            effectivePhase = 1;
            useMetabolicCaps = false;
            ForceAiMovementTypeWanted(owner, EMovementType.WALK);
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
            else if (SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled() && cpModel)
            {
                bool applyCruise = false;
                if (effectivePhase == 1)
                    applyCruise = true;
                else if (SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(cpModel))
                    applyCruise = true;

                if (applyCruise)
                {
                    float cpEffW = cpModel.GetEffectiveCriticalPowerWatts();
                    int invertPhase = effectivePhase;
                    if (invertPhase < 1)
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
                    if (resolved > 0.05 && resolved < targetMs)
                        targetMs = resolved;
                }
            }
        }

        if (animSpeedCompensation > 0.01)
            targetMs = targetMs * animSpeedCompensation;

        float phaseTopMs = ResolveAiPhaseTopMs(ctrl, effectivePhase, outExhausted);
        float frac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(targetMs, phaseTopMs, true);
        frac = SCR_RSS_DrainCalculator.ClampSpeedLimitFractionToGaitBand(frac, outExhausted);

        float customSprint = SCR_RSS_ConfigBridge.GetCustomSprintSpeedMultiplier();
        if (customSprint != 1.0 && effectivePhase == 3 && !outExhausted)
        {
            if (!drainDisabled)
            {
                if (ctrl.GetRssSprintAllowed())
                {
                    float boosted = targetMs * customSprint;
                    frac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(boosted, phaseTopMs, true);
                    frac = SCR_RSS_DrainCalculator.ClampSpeedLimitFractionToGaitBand(frac, false);
                    targetMs = boosted;
                }
            }
        }

        if (SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, frac);
        else
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 0.999);

        outAppliedFrac = frac;
        outTargetMs = targetMs;
        return true;
    }

    protected static void ForceAiMovementTypeWanted(IEntity owner, EMovementType speed)
    {
        if (!owner)
            return;

        AICharacterMovementComponent aiMove = AICharacterMovementComponent.Cast(
            owner.FindComponent(AICharacterMovementComponent));
        if (!aiMove)
            return;

        EMovementType resolved = speed;
        SCR_AICharacterSettingsComponent settingsComp = SCR_AICharacterSettingsComponent.Cast(
            owner.FindComponent(SCR_AICharacterSettingsComponent));
        if (settingsComp)
        {
            SCR_AICharacterMovementSpeedSettingBase setting = SCR_AICharacterMovementSpeedSettingBase.Cast(
                settingsComp.GetCurrentSetting(SCR_AICharacterMovementSpeedSettingBase));
            if (setting)
                resolved = setting.GetSpeed(resolved);
        }

        aiMove.SetMovementTypeWanted(resolved);
    }

    protected static float ResolveAiPhaseTopMs(
        SCR_CharacterControllerComponent ctrl,
        int effectivePhase,
        bool exhausted)
    {
        if (!ctrl)
            return SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

        if (exhausted)
        {
            float limpTop = ctrl.GetRssSpeedLimitEngineBaseMs();
            if (limpTop > 0.1)
                return limpTop;
            return SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
        }

        if (effectivePhase == 3)
        {
            float sprintTop = ctrl.GetOriginalEngineMaxSpeed_Sprint();
            if (sprintTop > 0.1)
                return sprintTop;
            return SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
        }
        if (effectivePhase == 1)
        {
            float walkTop = ctrl.GetOriginalEngineMaxSpeed_Walk();
            if (walkTop > 0.1)
                return walkTop;
            return SCR_RSS_Constants.ENGINE_WALK_TOP_MS;
        }

        float runTop = ctrl.GetOriginalEngineMaxSpeed_Run();
        if (runTop > 0.1)
            return runTop;
        return SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
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
