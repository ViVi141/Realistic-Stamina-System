// PlayerBase split: 泥泞滑倒接口
modded class SCR_CharacterControllerComponent
{
    void RSS_TriggerMudSlipRagdoll()
    {
        if (m_pAnimComponent && m_pAnimComponent.IsRagdollActive())
            return;
        Ragdoll();
        RefreshRagdoll(StaminaMudSlipConstants.ENV_MUD_SLIP_RAGDOLL_WARMUP_SEC);
        if (GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(RSS_MudSlip_FinishRagdoll, StaminaMudSlipConstants.ENV_MUD_SLIP_RAGDOLL_BLEND_DELAY_MS, false);
    }

    protected void RSS_MudSlip_FinishRagdoll()
    {
        RefreshRagdoll(0.0);
    }

    void RSS_SetMudSlipCameraShake01(float value)
    {
        m_fRssMudSlipCameraShake01 = SCR_RSS_PresentationBridge.ClampShake01(value);
    }

    float RSS_GetMudSlipCameraShake01()
    {
        return m_fRssMudSlipCameraShake01;
    }

    bool RSS_IsRagdollActiveForCamera()
    {
        return SCR_RSS_PresentationBridge.IsRagdollActive(m_pAnimComponent);
    }

    bool RSS_IsAiMudSlipBlockedBySafety(IEntity owner)
    {
        // [v3.23.0] MudSlipPolicy 已移除。非玩家 AI 在 SAFE 状态下阻止泥泞滑倒掷骰。
        if (IsPlayerControlled())
            return false;
        return true;
    }

    bool RSS_ShouldAiAllowMudSlipRagdoll(IEntity owner)
    {
        // [v3.23.0] 与 RSS_IsAiMudSlipBlockedBySafety 一致：非玩家 AI 不掷骰
        return !RSS_IsAiMudSlipBlockedBySafety(owner);
    }
}
