//! RSS 角色速度桥接
//! 原生灌木/铁丝网通过 SCR_ChimeraCharacter.SetSpeedLimit 写入 m_mSpeedReferences，
//! 再经 SCR_CharacterSlowdownEasingSystem 合并后调用 OverrideMaxSpeed。
//! RSS 若直接 OverrideMaxSpeed 会覆盖 Foliage 减速；此处改为 RSS 专用 source 参与合并。

class RSS_StaminaSpeedLimitToken : Managed
{
}

class SCR_RSS_SpeedBridge
{
    protected static ref RSS_StaminaSpeedLimitToken s_StaminaSpeedSource;

    protected static RSS_StaminaSpeedLimitToken GetStaminaSpeedSource()
    {
        if (!s_StaminaSpeedSource)
            s_StaminaSpeedSource = new RSS_StaminaSpeedLimitToken();
        return s_StaminaSpeedSource;
    }

    //! 将 RSS 体力速度倍率写入角色限速图（与灌木/铁丝网等取全局最小值）。
    //! limit=1.0 时引擎从 m_mSpeedReferences 移除本 source（见 docs/灌木丛移动减速机制.md）。
    //! 降速时同步 OverrideMaxSpeed：原生 SetSpeedLimit 走缓降，否则 v_meas 会长期高于 v_limit。
    static void ApplyStaminaSpeedLimit(IEntity owner, float limit)
    {
        if (!owner)
            return;

        limit = Math.Clamp(limit, 0.01, 3.0);

        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner);
        if (character)
        {
            character.SetSpeedLimit(GetStaminaSpeedSource(), limit);
            if (limit < 0.999)
            {
                SCR_CharacterControllerComponent ctrlImmediate = SCR_CharacterControllerComponent.Cast(
                    owner.FindComponent(SCR_CharacterControllerComponent));
                if (ctrlImmediate)
                    ctrlImmediate.OverrideMaxSpeed(Math.Clamp(limit, 0.01, 1.0));
            }
            return;
        }

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            owner.FindComponent(SCR_CharacterControllerComponent));
        if (ctrl)
            ctrl.OverrideMaxSpeed(Math.Clamp(limit, 0.01, 1.0));
    }

    static void ApplyStaminaSpeedLimit(SCR_CharacterControllerComponent ctrl, float limit)
    {
        if (!ctrl)
            return;
        ApplyStaminaSpeedLimit(ctrl.GetOwner(), limit);
    }

    //! W′ 耗尽等硬 metabolic 钳制：SetSpeedLimit + OverrideMaxSpeed（仅强钳制路径，避免覆盖灌木减速）
    static void ApplyHardStaminaSpeedClamp(IEntity owner, float limit)
    {
        ApplyStaminaSpeedLimit(owner, limit);

        if (!owner)
            return;

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            owner.FindComponent(SCR_CharacterControllerComponent));
        if (ctrl)
            ctrl.OverrideMaxSpeed(Math.Clamp(limit, 0.01, 1.0));
    }
}
