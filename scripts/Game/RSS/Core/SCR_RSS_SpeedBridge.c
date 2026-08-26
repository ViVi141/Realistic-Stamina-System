//! RSS 角色速度桥接
//! 原生灌木/铁丝网通过 SCR_ChimeraCharacter.SetSpeedLimit 写入 m_mSpeedReferences，
//! 再经 SCR_CharacterSlowdownEasingSystem 合并后调用 OverrideMaxSpeed。
//! RSS 必须只写入独立 source 参与 min 合并；禁止再单独 OverrideMaxSpeed，
//! 否则会盖掉 Foliage/铁丝网等已合并的限速。
//!
//! 默认只走 SetSpeedLimit。CP 反解掉出 Run 带时：ResolveRunCruiseCapMs 跳过越步态帽；
//! 若 V6_CP_OUT_OF_BAND_WALK_OVERRIDE，另用 CapsLock 同款 SetDynamicSpeed(0.5) 切 Walk 档
//! （动画与位移同档）。硬钳开时 Resolve 改回抬地板。

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

    //! 是否在掉出 Run 带时用引擎 Walk 动态速度覆盖（见 V6_CP_OUT_OF_BAND_WALK_OVERRIDE）。
    static bool IsCpOutOfBandWalkOverrideEnabled()
    {
        return SCR_RSS_Constants.V6_CP_OUT_OF_BAND_WALK_OVERRIDE;
    }

    //! 开始 CapsLock 同款 Walk 覆盖。已在 Walk 档则不抢所有权。
    //! @param savedSpeed 成功时写入覆盖前的 GetDynamicSpeed
    //! @return true 已由 RSS 持有覆盖
    static bool TryBeginWalkDynamicSpeedOverride(CharacterControllerComponent ctrl, out float savedSpeed)
    {
        savedSpeed = 1.0;
        if (!ctrl)
            return false;

        float walkSpd = SCR_RSS_Constants.ENGINE_WALK_DYNAMIC_SPEED;
        float current = ctrl.GetDynamicSpeed();
        if (Math.AbsFloat(current - walkSpd) <= 0.02)
            return false;

        savedSpeed = current;
        ctrl.SetDynamicSpeed(walkSpd);
        ctrl.SetShouldApplyDynamicSpeedOverride(true);
        return true;
    }

    //! 覆盖期间每 tick 再钉一次，防止走路键抬起把滚轮还原。
    static void HoldWalkDynamicSpeedOverride(CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.SetDynamicSpeed(SCR_RSS_Constants.ENGINE_WALK_DYNAMIC_SPEED);
        ctrl.SetShouldApplyDynamicSpeedOverride(true);
    }

    //! 还原覆盖前的动态速度并关掉 override。
    static void EndWalkDynamicSpeedOverride(CharacterControllerComponent ctrl, float savedSpeed)
    {
        if (!ctrl)
            return;

        float restore = savedSpeed;
        if (restore < 0.0)
            restore = 0.0;
        if (restore > 1.0)
            restore = 1.0;
        ctrl.SetDynamicSpeed(restore);
        ctrl.SetShouldApplyDynamicSpeedOverride(false);
    }

    //! 是否试跑 MovementComponent 绝对顶速（见 V6_TRY_MOVEMENT_MAX_SPEED）。
    static bool IsMovementMaxSpeedTrialEnabled()
    {
        return SCR_RSS_Constants.V6_TRY_MOVEMENT_MAX_SPEED;
    }

    static CharacterMovementComponent ResolveCharacterMovement(IEntity owner)
    {
        if (!owner)
            return null;

        CharacterEntity charEnt = CharacterEntity.Cast(owner);
        if (charEnt)
        {
            CharacterMovementComponent fromEnt = charEnt.GetMovementComponent();
            if (fromEnt)
                return fromEnt;
        }

        return CharacterMovementComponent.Cast(owner.FindComponent(CharacterMovementComponent));
    }

    //! 将绝对 m/s 写入 MovementMaxSpeed；首次调用应先 CaptureNativeMovementMaxSpeed。
    static void ApplyAbsoluteMovementMaxSpeed(IEntity owner, float absMs)
    {
        if (!IsMovementMaxSpeedTrialEnabled())
            return;
        if (!owner)
            return;
        if (absMs < 0.1)
            absMs = 0.1;
        if (absMs > SCR_RSS_MetabolismMath.GAME_MAX_SPEED)
            absMs = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

        CharacterMovementComponent move = ResolveCharacterMovement(owner);
        if (!move)
            return;

        move.SetMovementMaxSpeed(absMs);
    }

    static float CaptureNativeMovementMaxSpeed(IEntity owner)
    {
        CharacterMovementComponent move = ResolveCharacterMovement(owner);
        if (!move)
            return -1.0;
        return move.GetMovementMaxSpeed();
    }

    static void RestoreNativeMovementMaxSpeed(IEntity owner, float nativeMs)
    {
        if (!owner)
            return;
        if (nativeMs < 0.0)
            return;

        CharacterMovementComponent move = ResolveCharacterMovement(owner);
        if (!move)
            return;

        move.SetMovementMaxSpeed(nativeMs);
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

    //! 绝对速度 → 相对当前相位顶速的 SetSpeedLimit 倍率。
    //! @param keepSource true：禁止返回 1.0（Chimera SetSpeedLimit(1.0) 会移除限速源，
    //!   Run→Walk 时若仍停在 Run 顶速会瞬间窜到 3m/s+）。
    static float FractionForAbsoluteSpeed(float desiredAbsMs, float phaseTopMs, bool keepSource = false)
    {
        if (phaseTopMs < 0.1)
        {
            if (keepSource)
                return 0.999;
            return 1.0;
        }
        float frac = desiredAbsMs / phaseTopMs;
        if (frac > 1.0)
            frac = 1.0;
        if (frac < 0.01)
            frac = 0.01;
        if (keepSource)
        {
            if (frac >= 0.999)
                frac = 0.999;
        }
        return frac;
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

    //! CP 巡航 / W′ 解除武装后超速纠偏。
    //! @param gradePercent 当前坡度%
    //! @param movementPhase 1=Walk：永不放行下坡滑行（原版 Walk 顶 ≈1.45，不应到 3m/s+）；
    //!   Run/Sprint 缓下坡可小幅滑行，峭壁整段跳过。
    static void EnforceCpCruisePhysicsCap(
        IEntity owner,
        float appliedLimitMs,
        float measuredSpeedMs,
        float dtSec,
        float gradePercent,
        int movementPhase)
    {
        if (!SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
            return;

        bool isWalkPhase = false;
        if (movementPhase == 1)
            isWalkPhase = true;

        // Walk：始终可钳（引擎步行顶是硬顶）。非 Walk：极陡整段跳过。
        if (!isWalkPhase)
        {
            float gradeAbsMax = SCR_RSS_Constants.V6_CP_CRUISE_PHYS_CLAMP_GRADE_ABS_MAX;
            if (Math.AbsFloat(gradePercent) > gradeAbsMax)
                return;
        }

        if (appliedLimitMs < 0.1)
            return;

        float triggerExcess = SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_EPS_MPS;
        if (!isWalkPhase)
        {
            float downhillSkip = SCR_RSS_Constants.V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_SKIP_GRADE;
            if (gradePercent < downhillSkip)
            {
                // Run 缓下坡：容忍小幅重力滑行；超额仍钳
                triggerExcess = SCR_RSS_Constants.V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS;
            }
        }

        if (measuredSpeedMs <= appliedLimitMs + triggerExcess)
            return;

        // Walk / 明显超额 Run：硬钳（软钳会被动画回灌压回去，日志 1.8↔2.42）
        float hardExcess = 0.25;
        if (isWalkPhase || measuredSpeedMs > appliedLimitMs + hardExcess)
        {
            ClampOwnerHorizontalSpeed(owner, appliedLimitMs, true);
            return;
        }

        SoftClampOwnerHorizontalSpeed(owner, appliedLimitMs, dtSec, true);
    }

    //! 代谢/CP 反解用坡度：钳到 ±V6_METABOLIC_GRADE_ABS_MAX_PCT
    static float ClampGradePercentForMetabolicSpeed(float gradePercent)
    {
        float lim = SCR_RSS_Constants.V6_METABOLIC_GRADE_ABS_MAX_PCT;
        if (lim < 1.0)
            lim = 1.0;
        return Math.Clamp(gradePercent, -lim, lim);
    }
}
