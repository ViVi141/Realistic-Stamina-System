//! RSS 角色速度桥接
//! 原生灌木/铁丝网通过 SCR_ChimeraCharacter.SetSpeedLimit 写入 m_mSpeedReferences，
//! 再经 SCR_CharacterSlowdownEasingSystem 合并后调用 OverrideMaxSpeed。
//! RSS 必须只写入独立 source 参与 min 合并；禁止再单独 OverrideMaxSpeed，
//! 否则会盖掉 Foliage/铁丝网等已合并的限速。
//!
//! 默认只走 SetSpeedLimit。W′ 空且仍在 Run 带时另试 SetMovement 模拟量（过场/手柄同层）。
//! CP 反解掉出 Run 带时：ResolveRunCruiseCapMs 跳过越步态帽；
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

    //! 是否试跑 SetMovement 命令模拟量（见 V6_TRY_MOVEMENT_ANALOG_SCALE）。
    static bool IsMovementAnalogScaleEnabled()
    {
        return SCR_RSS_Constants.V6_TRY_MOVEMENT_ANALOG_SCALE;
    }

    //! 是否试跑缩放 CharacterForward/Right（见 V6_TRY_ACTION_VALUE_SCALE）。
    static bool IsActionValueScaleEnabled()
    {
        return SCR_RSS_Constants.V6_TRY_ACTION_VALUE_SCALE;
    }

    //! W′ 空时缩放目标：巡航帽与已应用限速取较低。帽未写出时先用平路 2.4，避免缓降窗仍满推。
    //! 掉带（帽 <0）不算有效帽，由 Walk 覆盖接管。
    static float ResolveActionScaleDesiredAbsMs(float appliedLimitMs, float lastRunCruiseCapMs)
    {
        float desired = appliedLimitMs;
        if (lastRunCruiseCapMs > 0.05)
        {
            if (lastRunCruiseCapMs < desired)
                desired = lastRunCruiseCapMs;
            return desired;
        }

        float fallback = SCR_RSS_Constants.V6_AEROBIC_CRUISE_MAX_MS;
        if (fallback < desired)
            desired = fallback;
        return desired;
    }

    //! 缩放分母：优先实机满推 Run 测速。引擎顶 ~3.8，负重满 W 常只有 ~3.55，用 3.8 会缩到 2.26 而不是 2.4。
    static float ResolveActionScaleRunTopMs(float engineRunTopMs, float observedFullRunMs, float walkTopMs)
    {
        float runTop = engineRunTopMs;
        if (runTop < walkTopMs + 0.2)
            runTop = walkTopMs + 0.2;
        if (observedFullRunMs <= walkTopMs + 0.4)
            return runTop;

        float lo = walkTopMs + 0.4;
        float hi = runTop + 0.15;
        float obs = observedFullRunMs;
        if (obs < lo)
            obs = lo;
        if (obs > hi)
            obs = hi;
        return obs;
    }

    //! 把 WASD 向量缩到 desiredAbs/runTop。不抬高手柄半推。
    //! W′ 空路径不因按着 Shift / 仍停在 Sprint 相位而跳过（门禁另清冲刺）。
    //! 走路键、Walk 相位、蹲/趴让路，避免和 CapsLock / 姿态档叠乘。
    //! @return 写入的轴幅度；未写则 -1
    static float TryScaleMoveActionValues(
        ActionManager am,
        CharacterControllerComponent ctrl,
        float desiredAbsMs,
        float walkTopMs,
        float runTopMs)
    {
        if (!IsActionValueScaleEnabled())
            return -1.0;
        if (!am)
            return -1.0;
        if (!ctrl)
            return -1.0;
        if (desiredAbsMs <= walkTopMs + 0.05)
            return -1.0;
        if (desiredAbsMs >= runTopMs - 0.05)
            return -1.0;
        if (ctrl.GetCurrentMovementPhase() == 1)
            return -1.0;
        if (ctrl.GetStance() != ECharacterStance.STAND)
            return -1.0;
        if (am.GetActionValue("CharacterWalk") > 0.5)
            return -1.0;

        float fwd = am.GetActionValue("CharacterForward");
        float right = am.GetActionValue("CharacterRight");
        float magSq = fwd * fwd + right * right;
        if (magSq < 0.01)
            return -1.0;

        float mag = Math.Sqrt(magSq);
        float target = desiredAbsMs / runTopMs;
        if (target < 0.15)
            target = 0.15;
        if (target > 0.98)
            return -1.0;
        if (mag <= target + 0.02)
            return -1.0;

        float scale = target / mag;
        am.SetActionValue("CharacterForward", fwd * scale);
        am.SetActionValue("CharacterRight", right * scale);
        return target;
    }

    //! 绝对 m/s → Walk(1)–Run(2) 模拟量。过场在 (0.5, 2) 之间是连续档。
    static float AnalogForAbsoluteMs(float desiredAbsMs, float walkTopMs, float runTopMs)
    {
        float walkA = SCR_RSS_Constants.ENGINE_MOVE_ANALOG_WALK;
        float runA = SCR_RSS_Constants.ENGINE_MOVE_ANALOG_RUN;
        float floorA = SCR_RSS_Constants.ENGINE_MOVE_ANALOG_FLOOR;
        if (runTopMs <= walkTopMs + 0.05)
            return walkA;

        float t = (desiredAbsMs - walkTopMs) / (runTopMs - walkTopMs);
        if (t < 0.0)
            t = 0.0;
        if (t > 1.0)
            t = 1.0;

        float analog = walkA + t * (runA - walkA);
        if (analog < floorA)
            analog = floorA;
        if (analog > runA)
            analog = runA;
        return analog;
    }

    //! W′ 空、仍在 Run：把命令模拟量压到 desiredAbsMs。不抬高（不把走路变成慢跑）。
    //! @return 写入的模拟量；未写则 -1
    static float TryApplyRunCruiseAnalog(
        CharacterControllerComponent ctrl,
        float desiredAbsMs,
        float walkTopMs,
        float runTopMs)
    {
        if (!IsMovementAnalogScaleEnabled())
            return -1.0;
        if (!ctrl)
            return -1.0;
        if (desiredAbsMs <= walkTopMs + 0.05)
            return -1.0;
        if (desiredAbsMs >= runTopMs - 0.05)
            return -1.0;
        if (ctrl.IsSprinting())
            return -1.0;
        if (ctrl.GetCurrentMovementPhase() != 2)
            return -1.0;

        float currentAnalog = ctrl.GetMovementSpeed();
        if (currentAnalog > SCR_RSS_Constants.ENGINE_MOVE_ANALOG_RUN + 0.05)
            return -1.0;
        if (currentAnalog < SCR_RSS_Constants.ENGINE_MOVE_ANALOG_WALK - 0.05)
            return -1.0;

        float targetAnalog = AnalogForAbsoluteMs(desiredAbsMs, walkTopMs, runTopMs);
        if (currentAnalog <= targetAnalog + 0.02)
            return -1.0;

        vector dir = ctrl.GetMovementInput();
        float dirSq = dir[0] * dir[0] + dir[2] * dir[2];
        if (dirSq < 0.01)
            return -1.0;

        ctrl.SetMovement(targetAnalog, dir);
        CharacterInputContext ctx = ctrl.GetInputContext();
        if (ctx)
            ctx.SetMovement(targetAnalog, dir);
        return targetAnalog;
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
