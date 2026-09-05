//! RSS AI Speed Cap — 五级移动限速
//!
//! 根据体力状态决定 AI 的最高移动类型和速度上限（经 SetSpeedLimit 与灌木减速合并）。
//! 限速用绝对行军 m/s → 相位顶分数（与廉价限速 / 玩家 UpdateSpeed 同形）。
//!
//! 调用方：AIManager.Tick
//! 不侵入原生行为树——通过 SetMovementTypeWanted + SetSpeedLimit（经 SCR_RSS_SpeedBridge）间接控制。

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

        // 战斗层限速也按绝对 m/s→相位分数，避免 0.65×冲刺顶仍快过玩家 Run
        float runMs = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
        float walkMs = SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs();
        float phaseTop = ctrl.GetRssSpeedLimitEngineBaseMs();
        if (phaseTop < 0.1)
            phaseTop = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

        switch (state)
        {
        case ERSS_AIStaminaState.FRESH:
            // 全速由廉价限速管线负责，此处不覆盖
            return;

        case ERSS_AIStaminaState.WINDED:
            maxMovement = EMovementType.RUN;
            AISetMovementTypeWanted(owner, maxMovement);
            speedMul = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(runMs, phaseTop, true);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, speedMul);
            return;

        case ERSS_AIStaminaState.FATIGUED:
            maxMovement = EMovementType.RUN;
            speedMul = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                runMs * SCR_RSS_AIConstants.RSS_AI_SPEED_FATIGUED_LIMIT, phaseTop, true);
            break;

        case ERSS_AIStaminaState.EXHAUSTED:
            maxMovement = EMovementType.WALK;
            speedMul = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                walkMs * SCR_RSS_AIConstants.RSS_AI_SPEED_EXHAUSTED_LIMIT, phaseTop, true);
            break;

        case ERSS_AIStaminaState.COLLAPSED:
            maxMovement = EMovementType.IDLE;
            speedMul = 0.01;
            break;

        case ERSS_AIStaminaState.RECOVERING:
            maxMovement = EMovementType.WALK;
            speedMul = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                walkMs * GetRecoveringSpeedMultiplier(staminaPercent), phaseTop, true);
            break;

        default:
            return;
        }

        AISetMovementTypeWanted(owner, maxMovement);
        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, speedMul);
    }

    //------------------------------------------------------------------------------------------------
    //! 恢复期间相对 Walk 行军速的乘数 [RECOVERING_MIN, 1]
    protected static float GetRecoveringSpeedMultiplier(float staminaPercent)
    {
        float t = Math.Clamp(staminaPercent / 0.30, 0.0, 1.0);
        float minMul = SCR_RSS_AIConstants.RSS_AI_SPEED_RECOVERING_MIN;
        return minMul + (1.0 - minMul) * t;
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
