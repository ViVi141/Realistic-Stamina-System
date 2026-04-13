//! AI 泥泞策略桥（从 PlayerBase.c 拆分）
class SCR_RSS_AIMudSlipPolicy
{
    static bool IsBlockedBySafety(SCR_CharacterControllerComponent controller, IEntity owner)
    {
        return SCR_RSS_AIStaminaBridge.IsMudSlipBlockedBySafety(controller, owner);
    }

    static bool ShouldAllowRagdoll(SCR_CharacterControllerComponent controller, IEntity owner)
    {
        return SCR_RSS_AIStaminaBridge.ShouldAllowMudSlipRagdoll(controller, owner);
    }
}
