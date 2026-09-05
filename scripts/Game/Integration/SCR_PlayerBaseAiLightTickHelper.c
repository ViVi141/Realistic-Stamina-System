//! AI 廉价限速：所有服端 AI 共用（无论是否算体力消耗）。
//! 跳过地形射线 / 环境因子 / UpdateCoordinator.UpdateSpeed —— 这是「Speed」选项卡顿主因。
//!
//! 限速语义与玩家 UpdateSpeed 对齐：绝对目标 m/s ÷ 当前相位引擎顶 → SetSpeedLimit。
//! 禁止把 CalculateV6PhaseSpeedMultiplier（相对 GAME_MAX_SPEED）直接当相位分数——
//! 冲刺时会得到 ≈1.0，Chimera 会移除限速源，AI 变成原版满速冲刺。

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
        outAppliedFrac = 1.0;
        outEncumbrancePenalty = 0.0;
        outStaminaPercent = 1.0;
        outPhase = 2;
        outExhausted = false;

        if (!ctrl || !owner)
            return false;
        if (ctrl.IsPlayerControlled())
            return false;

        if (encumbranceCache)
        {
            encumbranceCache.CheckAndUpdate();
            outEncumbrancePenalty = encumbranceCache.GetSpeedPenaltyFraction();
            outEncumbrancePenalty = outEncumbrancePenalty
                * SCR_RSS_ConfigBridge.GetCustomEncumbranceSpeedPenaltyMultiplier();
            float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
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

        float encMult = 1.0 - outEncumbrancePenalty;
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

        // 不算消耗：没有体力经济，禁止无限原版冲刺 → 指令顶压到 Run
        if (SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
        {
            if (targetMs > runMs)
                targetMs = runMs;
            if (effectivePhase == 3)
                outPhase = 2;
        }
        else
        {
            // 有消耗管线：W′/冲刺门与玩家一致，不允许空池硬冲
            if (effectivePhase == 3 && !ctrl.GetRssSprintAllowed())
            {
                targetMs = runMs;
                outPhase = 2;
            }
        }

        if (outExhausted)
        {
            targetMs = SCR_RSS_MetabolismMath.GetDynamicLimpSpeedMs(outEncumbrancePenalty);
            outPhase = 1;
        }

        if (animSpeedCompensation > 0.01)
            targetMs = targetMs * animSpeedCompensation;

        float phaseTopMs = ResolveAiPhaseTopMs(ctrl, effectivePhase, outExhausted);
        // keepSource：禁止 1.0 移除限速源（否则 AI 冲刺回到引擎 5.5）
        float frac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(targetMs, phaseTopMs, true);
        frac = SCR_RSS_DrainCalculator.ClampSpeedLimitFractionToGaitBand(frac, outExhausted);

        float customSprint = SCR_RSS_ConfigBridge.GetCustomSprintSpeedMultiplier();
        if (customSprint != 1.0 && effectivePhase == 3 && !outExhausted)
        {
            if (!SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
            {
                if (ctrl.GetRssSprintAllowed())
                {
                    float boosted = targetMs * customSprint;
                    frac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(boosted, phaseTopMs, true);
                    frac = SCR_RSS_DrainCalculator.ClampSpeedLimitFractionToGaitBand(frac, false);
                }
            }
        }

        if (SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, frac);
        else
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 0.999);

        outAppliedFrac = frac;
        return true;
    }

    protected static float ResolveAiPhaseTopMs(
        SCR_CharacterControllerComponent ctrl,
        int effectivePhase,
        bool exhausted)
    {
        if (!ctrl)
            return SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

        // 耗尽跛行时限速源挂在当前引擎相位上，用实时相位顶作分母
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
