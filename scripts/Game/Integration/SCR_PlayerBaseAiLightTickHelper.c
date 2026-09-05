//! AI 廉价限速：所有服端 AI 共用（无论是否算体力消耗）。
//! 跳过地形射线 / 环境因子 / UpdateCoordinator.UpdateSpeed —— 这是「Speed」选项卡顿主因。

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

        if (outExhausted)
        {
            float limpSpeedMultiplier = SCR_RSS_MetabolismMath.GetDynamicLimpMultiplier(
                outEncumbrancePenalty);
            float compensatedLimpMultiplier = Math.Clamp(
                limpSpeedMultiplier * animSpeedCompensation, 0.01, 1.0);
            if (SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, compensatedLimpMultiplier);
            else
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 1.0);
            outAppliedFrac = compensatedLimpMultiplier;
            outPhase = 1;
            return true;
        }

        int phaseNow = ctrl.GetCurrentMovementPhase();
        int effectivePhase = phaseNow;
        if (effectivePhase < 1)
            effectivePhase = 2;
        if (ctrl.IsSprinting())
            effectivePhase = 3;
        outPhase = effectivePhase;

        float speedMult = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(
            outStaminaPercent, effectivePhase, outEncumbrancePenalty);
        speedMult = Math.Clamp(speedMult * animSpeedCompensation, 0.01, 1.0);

        float customSprint = SCR_RSS_ConfigBridge.GetCustomSprintSpeedMultiplier();
        if (customSprint != 1.0 && effectivePhase == 3)
            speedMult = Math.Clamp(speedMult * customSprint, 0.01, 3.0);

        if (SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, speedMult);
        else
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 1.0);

        outAppliedFrac = speedMult;
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
