//! RSS AI Speed Cap — 五级移动限速
//!
//! 根据体力状态决定 AI 的最高移动类型和速度上限（经 SetSpeedLimit 与灌木减速合并）。
//! 同时包含连续速度衰减曲线（与玩家同源 STAMINA_EXPONENT = 0.6）。
//!
//! 调用方：PlayerBase.c 每帧 Tick
//! 不侵入原生行为树节点——通过 SetMovementTypeWanted + SetSpeedLimit（经 SCR_RSS_SpeedBridge）间接控制。

class SCR_RSS_AISpeedCap
{
    //------------------------------------------------------------------------------------------------
    //! 主入口：对 AI 实体施加基于体力状态的移动限制。
    //!
    //! \param ctrl            角色控制器
    //! \param owner           实体（IEntity）
    //! \param state           当前体力状态
    //! \param staminaPercent  体力百分比 [0~1]
    //! \param isThreatened    当前是否被压制 (THREATENED)
    static void Apply(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        ERSS_AIStaminaState state,
        float staminaPercent,
        bool isThreatened)
    {
        if (!ctrl || !owner)
            return;

        // 玩家不使用此模块
        if (ctrl.IsPlayerControlled())
            return;

        // 仅服务器端
        if (!Replication.IsServer())
            return;

        // 载具中 / 游泳 → 不限速
        ChimeraCharacter ch = ChimeraCharacter.Cast(owner);
        if (ch)
        {
            CompartmentAccessComponent compAccess = ch.GetCompartmentAccessComponent();
            if (compAccess && compAccess.GetCompartment())
                return;
        }
        if (SCR_RSS_SwimmingStateManager.IsSwimming(ctrl))
            return;

        // 被压制时不限速（保命优先）
        if (isThreatened)
            return;

        float speedMul;
        EMovementType maxMovement;

        switch (state)
        {
        case ERSS_AIStaminaState.FRESH:
            // 全速，不干预
            return;

        case ERSS_AIStaminaState.WINDED:
            // 禁止 Sprint，其余正常
            maxMovement = EMovementType.RUN;
            speedMul = 1.0;
            AISetMovementTypeWanted(owner, maxMovement);
            return;  // 不高 OverrideMaxSpeed

        case ERSS_AIStaminaState.FATIGUED:
            // RUN + 限速到 65%
            maxMovement = EMovementType.RUN;
            speedMul = SCR_RSS_Constants.RSS_AI_SPEED_FATIGUED_LIMIT;
            break;

        case ERSS_AIStaminaState.EXHAUSTED:
            // WALK + 限速到 40%
            maxMovement = EMovementType.WALK;
            speedMul = SCR_RSS_Constants.RSS_AI_SPEED_EXHAUSTED_LIMIT;
            break;

        case ERSS_AIStaminaState.COLLAPSED:
            // 无移动
            maxMovement = EMovementType.IDLE;
            speedMul = 0.01; // 几乎不可移动
            break;

        case ERSS_AIStaminaState.RECOVERING:
            // WALK + 连续恢复曲线
            maxMovement = EMovementType.WALK;
            speedMul = GetRecoveringSpeedMultiplier(staminaPercent);
            break;

        default:
            return;
        }

        AISetMovementTypeWanted(owner, maxMovement);
        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, speedMul);
    }

    //------------------------------------------------------------------------------------------------
    //! 连续速度衰减曲线：v6 与玩家同源（相位目标 + 低 STA 跛行，无平台期）
    static float GetContinuousSpeedMultiplier(float staminaPercent01)
    {
        return SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(staminaPercent01, 2, 0.0);
    }

    //------------------------------------------------------------------------------------------------
    //! 恢复期间的速度乘数：体力的连续函数，确保从 COLLAPSED 渐出。
    protected static float GetRecoveringSpeedMultiplier(float staminaPercent)
    {
        float contMul = GetContinuousSpeedMultiplier(staminaPercent);
        // 恢复期间至少保证能 WALK
        return Math.Max(contMul, SCR_RSS_Constants.RSS_AI_SPEED_RECOVERING_MIN);
    }

    //------------------------------------------------------------------------------------------------
    //! 设置 AI 的想要的移动类型（经 AI 设置组件裁剪）
    protected static void AISetMovementTypeWanted(IEntity owner, EMovementType speed)
    {
        if (!owner)
            return;

        AICharacterMovementComponent aiMove = AICharacterMovementComponent.Cast(
            owner.FindComponent(AICharacterMovementComponent));
        if (!aiMove)
            return;

        EMovementType resolved = speed;
        SCR_AICharacterSettingsComponent settingsComp = SCR_AICharacterSettingsComponent.Cast(
            owner.FindComponent(SCR_AICharacterSettingsComponent));
        if (settingsComp)
        {
            SCR_AICharacterMovementSpeedSettingBase setting = SCR_AICharacterMovementSpeedSettingBase.Cast(
                settingsComp.GetCurrentSetting(SCR_AICharacterMovementSpeedSettingBase));
            if (setting)
                resolved = setting.GetSpeed(resolved);
        }

        aiMove.SetMovementTypeWanted(resolved);
    }

    //------------------------------------------------------------------------------------------------
    //! 是否启用了 AI 移动限速模块（从 JSON 配置读取）
    static bool IsEnabled()
    {
        return SCR_RSS_ConfigBridge.IsAIStaminaIntegrationEnabled();
    }
}
