//! RSS 角色速度桥接
//! 原生灌木/铁丝网通过 SCR_ChimeraCharacter.SetSpeedLimit 写入 m_mSpeedReferences，
//! 再经 SCR_CharacterSlowdownEasingSystem 合并后调用 OverrideMaxSpeed。
//! RSS 必须只写入独立 source 参与 min 合并；禁止再单独 OverrideMaxSpeed，
//! 否则会盖掉 Foliage/铁丝网等已合并的限速。
//!
//! 禁止用 SetDynamicSpeed(0.5) 假按 Walk：会把相位锁死，Run 进不去。

class RSS_StaminaSpeedLimitToken : Managed
{
}

class SCR_RSS_SpeedBridge
{
    protected static ref RSS_StaminaSpeedLimitToken s_StaminaSpeedSource;
    protected static const float HORIZ_SOFT_DECEL_MS2 = 9.0;

    protected static RSS_StaminaSpeedLimitToken GetStaminaSpeedSource()
    {
        if (!s_StaminaSpeedSource)
            s_StaminaSpeedSource = new RSS_StaminaSpeedLimitToken();
        return s_StaminaSpeedSource;
    }

    //! 是否写入体力限速（见 V6_APPLY_STAMINA_SPEED_LIMIT）。
    static bool IsStaminaSpeedPressEnabled()
    {
        return SCR_RSS_Constants.V6_APPLY_STAMINA_SPEED_LIMIT;
    }

    //! 是否对物理水平速度硬/软钳（见 V6_APPLY_HORIZONTAL_SPEED_CLAMP）。
    static bool IsHorizontalSpeedClampEnabled()
    {
        return SCR_RSS_Constants.V6_APPLY_HORIZONTAL_SPEED_CLAMP;
    }

    //! 是否用 CP/有氧巡航再压 Run 速度（见 V6_APPLY_CP_METABOLIC_SPEED_CAP）。
    static bool IsCpMetabolicSpeedCapEnabled()
    {
        return SCR_RSS_Constants.V6_APPLY_CP_METABOLIC_SPEED_CAP;
    }

    //! 是否使用 March 行军档绝对速度（见 V6_USE_MARCH_GAIT_SPEEDS）。
    static bool IsMarchGaitSpeedEnabled()
    {
        return SCR_RSS_Constants.V6_USE_MARCH_GAIT_SPEEDS;
    }

    //! 将 RSS 体力速度倍率写入角色限速图（与灌木/铁丝网等取全局最小值）。
    //! limit=1.0 时引擎从 m_mSpeedReferences 移除本 source。
    static void ApplyStaminaSpeedLimit(IEntity owner, float limit)
    {
        if (!owner)
            return;

        limit = Math.Clamp(limit, 0.01, 3.0);

        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner);
        if (character)
        {
            character.SetSpeedLimit(GetStaminaSpeedSource(), limit);
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

    static void ApplyHardStaminaSpeedClamp(IEntity owner, float limit)
    {
        ApplyStaminaSpeedLimit(owner, limit);
    }

    //! 绝对速度 → 相对当前相位顶速的 SetSpeedLimit 倍率
    static float FractionForAbsoluteSpeed(float desiredAbsMs, float phaseTopMs)
    {
        if (phaseTopMs < 0.1)
            return 1.0;
        float frac = desiredAbsMs / phaseTopMs;
        if (frac > 1.0)
            frac = 1.0;
        if (frac < 0.01)
            frac = 0.01;
        return frac;
    }

    //! 清除残留 DynamicSpeed=0.5（非玩家按 Walk）；不主动切入 Walk。
    static void ClearStuckWalkDynamicSpeed(CharacterControllerComponent ctrl, bool walkKeyHeld)
    {
        if (!ctrl || walkKeyHeld)
            return;

        float dyn = ctrl.GetDynamicSpeed();
        if (dyn > 0.52)
            return;

        ctrl.SetDynamicSpeed(1.0);
        ctrl.SetShouldApplyDynamicSpeedOverride(false);
    }

    //! 当前相位下「不滑步」的物理上限：不能超过该相位动画顶速。
    static float GetPhaseSafePhysicsCapMs(
        float appliedAbsMs,
        float phaseTopMs,
        bool isSprinting,
        int movementPhase)
    {
        float cap = appliedAbsMs;
        if (cap < 0.1)
            return cap;

        bool sprintPhase = false;
        if (isSprinting || movementPhase == 3)
            sprintPhase = true;

        if (!sprintPhase && phaseTopMs > 0.1 && cap > phaseTopMs)
            cap = phaseTopMs;
        return cap;
    }

    static void ClampOwnerHorizontalSpeed(IEntity owner, float maxHorizMs)
    {
        ClampOwnerHorizontalSpeed(owner, maxHorizMs, false);
    }

    //! @param forceIgnoreGlobalFlag true：无视 V6_APPLY_HORIZONTAL_SPEED_CLAMP（仅用于 CP 巡航超速纠偏）
    static void ClampOwnerHorizontalSpeed(IEntity owner, float maxHorizMs, bool forceIgnoreGlobalFlag)
    {
        if (!forceIgnoreGlobalFlag)
        {
            if (!IsHorizontalSpeedClampEnabled())
                return;
        }
        if (!owner)
            return;
        if (maxHorizMs < 0.1)
            return;

        Physics physics = owner.GetPhysics();
        if (!physics)
            return;

        vector velocity = physics.GetVelocity();
        float horizSq = velocity[0] * velocity[0] + velocity[2] * velocity[2];
        float slackMs = 0.04;
        float slackSq = (maxHorizMs + slackMs) * (maxHorizMs + slackMs);
        if (horizSq <= slackSq)
            return;
        if (horizSq <= 0.0001)
            return;

        float speed = Math.Sqrt(horizSq);
        float scale = maxHorizMs / speed;
        velocity[0] = velocity[0] * scale;
        velocity[2] = velocity[2] * scale;
        physics.SetVelocity(velocity);
    }

    //! 软钳；超额大时硬钳（控制器每帧回灌时软钳不够）
    static void SoftClampOwnerHorizontalSpeed(IEntity owner, float maxHorizMs, float dtSec)
    {
        SoftClampOwnerHorizontalSpeed(owner, maxHorizMs, dtSec, false);
    }

    static void SoftClampOwnerHorizontalSpeed(IEntity owner, float maxHorizMs, float dtSec, bool forceIgnoreGlobalFlag)
    {
        if (!forceIgnoreGlobalFlag)
        {
            if (!IsHorizontalSpeedClampEnabled())
                return;
        }
        if (!owner)
            return;
        if (maxHorizMs < 0.1)
            return;
        if (dtSec < 0.01)
            dtSec = 0.01;
        if (dtSec > 0.5)
            dtSec = 0.5;

        Physics physics = owner.GetPhysics();
        if (!physics)
            return;

        vector velocity = physics.GetVelocity();
        float horizSq = velocity[0] * velocity[0] + velocity[2] * velocity[2];
        if (horizSq <= 0.0001)
            return;

        float speed = Math.Sqrt(horizSq);
        float slackMs = 0.06;
        if (speed <= maxHorizMs + slackMs)
            return;

        if (speed > maxHorizMs + 0.35)
        {
            ClampOwnerHorizontalSpeed(owner, maxHorizMs, forceIgnoreGlobalFlag);
            return;
        }

        float newSpeed = speed - HORIZ_SOFT_DECEL_MS2 * dtSec;
        if (newSpeed < maxHorizMs)
            newSpeed = maxHorizMs;
        float scale = newSpeed / speed;
        velocity[0] = velocity[0] * scale;
        velocity[2] = velocity[2] * scale;
        physics.SetVelocity(velocity);
    }

    //! CP 巡航 / W′ 解除武装后：SetSpeedLimit 压不住物理速度时纠偏（仅超速时）
    static void EnforceCpCruisePhysicsCap(IEntity owner, float appliedLimitMs, float measuredSpeedMs, float dtSec)
    {
        if (!SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
            return;
        if (appliedLimitMs < 0.1)
            return;
        float eps = SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_EPS_MPS;
        if (measuredSpeedMs <= appliedLimitMs + eps)
            return;
        SoftClampOwnerHorizontalSpeed(owner, appliedLimitMs, dtSec, true);
    }
}
