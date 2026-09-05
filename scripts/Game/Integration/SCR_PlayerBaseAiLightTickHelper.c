//! AI 轻量 tick：DisableAIStaminaCalc 时只保留负重/相位限速，跳过地形/环境/代谢整链。
//! 高密度 AI 场景下 Phase A 全量路径是专服 CPU 主因之一。

class SCR_PlayerBaseAiLightTickHelper
{
    //! 对 AI 应用轻量限速并返回 true（调用方应 ScheduleNext 后结束 Phase A）。
    //! @param ctrl 角色控制器（modded PlayerBase）
    //! @param owner 角色实体
    //! @param encumbranceCache 负重缓存（可为 null）
    //! @param animSpeedCompensation 动画速度补偿
    //! @param outAppliedFrac 写出本次限速分数
    //! @return true 表示已处理并应 early-out
    static bool TryApplyLightSpeedLimit(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        SCR_RSS_EncumbranceCache encumbranceCache,
        float animSpeedCompensation,
        out float outAppliedFrac)
    {
        outAppliedFrac = 1.0;
        if (!ctrl || !owner)
            return false;
        if (ctrl.IsPlayerControlled())
            return false;
        if (!SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
            return false;

        float encumbranceSpeedPenalty = 0.0;
        if (encumbranceCache)
        {
            encumbranceCache.CheckAndUpdate();
            encumbranceSpeedPenalty = encumbranceCache.GetSpeedPenaltyFraction();
            encumbranceSpeedPenalty = encumbranceSpeedPenalty
                * SCR_RSS_ConfigBridge.GetCustomEncumbranceSpeedPenaltyMultiplier();
            float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
            encumbranceSpeedPenalty = Math.Clamp(encumbranceSpeedPenalty, 0.0, maxPenalty);
        }

        float staminaPercent = ctrl.GetRssAerobicPercent();
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);

        if (SCR_RSS_MetabolismMath.IsExhausted(staminaPercent))
        {
            float limpSpeedMultiplier = SCR_RSS_MetabolismMath.GetDynamicLimpMultiplier(
                encumbranceSpeedPenalty);
            float compensatedLimpMultiplier = Math.Clamp(
                limpSpeedMultiplier * animSpeedCompensation, 0.01, 1.0);
            if (SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, compensatedLimpMultiplier);
            else
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 1.0);
            outAppliedFrac = compensatedLimpMultiplier;
            return true;
        }

        int phaseNow = ctrl.GetCurrentMovementPhase();
        int effectivePhase = phaseNow;
        if (effectivePhase < 1)
            effectivePhase = 2;
        if (ctrl.IsSprinting())
            effectivePhase = 3;

        float speedMult = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(
            staminaPercent, effectivePhase, encumbranceSpeedPenalty);
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
}
