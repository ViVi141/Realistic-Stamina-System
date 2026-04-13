//! 表现层桥接工具（从 PlayerBase.c 拆分）
class SCR_RSS_PresentationBridge
{
    static float ClampShake01(float value)
    {
        if (value < 0.0)
            return 0.0;
        if (value > 1.0)
            return 1.0;
        return value;
    }

    static bool IsRagdollActive(CharacterAnimationComponent animComponent)
    {
        if (animComponent && animComponent.IsRagdollActive())
            return true;

        return false;
    }
}
