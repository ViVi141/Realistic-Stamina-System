//! RSS 与 AI 的策略与表现层：泥泞安全上下文、AI 预警限速、AI 调试行缓冲。
//!
//! 与官方行为树的分工：
//! - 行为树节点（如 SCR_AICharacterSetMovementSpeed）通过 AICharacterMovementComponent.SetMovementTypeWanted
//!   表达「期望移动类型」（走/跑等），由 AI 设置组件可做上限裁剪。
//! - 本模组仍通过 SCR_CharacterControllerComponent（RSS）按体力与地形计算 OverrideMaxSpeed，
//!   二者叠加：引擎在期望类型与最大速度之间取实际结果。
//! - 徒步 AI：ApplyOnFootMovementPolicy 在服务器上按体力 / 泥泞（安全上下文）将 RUN·SPRINT 降级为 WALK，
//!   与 SCR_AICharacterSetMovementSpeed 一致经 SCR_AICharacterMovementSpeedSettingBase 裁剪后再 SetMovementTypeWanted。

class SCR_RSS_AIStaminaBridge
{
    protected static ref array<string> s_aAIDebugLines;

    //------------------------------------------------------------------------------------------------
    //! 玩家使用 RSS 主循环间隔；服务器 AI 使用较低频率以节省性能。
    static int GetSpeedUpdateIntervalMs(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS;
        if (ctrl.IsPlayerControlled())
            return StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS;
        return StaminaConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;
    }

    //------------------------------------------------------------------------------------------------
    //! 官方脚本里 SCR_AIUtilityComponent 挂在 AIAgent 上；角色实体上可能没有，需从 AIControl 解析。
    static SCR_AIUtilityComponent ResolveAIUtilityForCharacter(IEntity characterOwner)
    {
        if (!characterOwner)
            return null;
        SCR_AIUtilityComponent aiUtil = SCR_AIUtilityComponent.Cast(characterOwner.FindComponent(SCR_AIUtilityComponent));
        if (aiUtil)
            return aiUtil;
        AIControlComponent aiControl = AIControlComponent.Cast(characterOwner.FindComponent(AIControlComponent));
        if (!aiControl)
            return null;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return null;
        return SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
    }

    //------------------------------------------------------------------------------------------------
    //! 泥泞「危险上下文」中的威胁部分：仅 THREATENED（压制）视为须解除安全巡逻。
    //! VIGILANT/ALERTED 无敌人时仍常出现，若按「非 SAFE 即危险」会误开 Ragdoll 掷骰并取消预警压速。
    protected static bool ThreatStateIndicatesMudSlipCombatDanger(IEntity owner)
    {
        if (!owner)
            return false;
        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return false;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return false;
        SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(agent.FindComponent(SCR_AIInfoComponent));
        if (!info)
            return false;
        EAIThreatState threat = info.GetThreatState();
        if (threat == EAIThreatState.THREATENED)
            return true;
        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! 当前动作优先级 ≥ 阈值 → 泥泞上视为高压力（与 Utility 在 Agent 上的事实一致）。
    protected static bool ActionPriorityIndicatesDanger(IEntity owner)
    {
        SCR_AIUtilityComponent aiUtil = ResolveAIUtilityForCharacter(owner);
        if (!aiUtil)
            return false;
        AIActionBase act = aiUtil.GetExecutedAction();
        if (!act)
            act = aiUtil.GetCurrentAction();
        SCR_AIActionBase scrAct = SCR_AIActionBase.Cast(act);
        if (!scrAct)
            return false;
        float pr = scrAct.EvaluatePriorityLevel();
        if (pr >= StaminaConstants.ENV_MUD_SLIP_AI_UNSAFE_PRIORITY_MIN)
            return true;
        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! 休整/群组路点分支：仅统计存活且未失能单位（DEAD、INCAPACITATED 视为无效，与昏迷一致）。
    static bool IsAliveAndActionableForGroupRest(IEntity ent)
    {
        if (!ent)
            return false;
        SCR_CharacterControllerComponent c = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
        if (!c)
            return false;
        ECharacterLifeState ls = c.GetLifeState();
        if (ls == ECharacterLifeState.DEAD)
            return false;
        if (ls == ECharacterLifeState.INCAPACITATED)
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 战场危险上下文：与泥泞滑倒分支 ShouldIgnoreMudSlipSpeedCap 一致（THREATENED 或高优先级动作）。
    static bool IsBattlefieldDangerContext(IEntity owner)
    {
        if (!owner)
            return false;
        if (ThreatStateIndicatesMudSlipCombatDanger(owner))
            return true;
        if (ActionPriorityIndicatesDanger(owner))
            return true;
        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! 自主休整（寻蔽/路点）：战场危险时不进入休整路线，与滑倒「危险上下文」一致。
    static bool ShouldBlockAutonomousRest(IEntity owner)
    {
        if (!owner)
            return true;
        return IsBattlefieldDangerContext(owner);
    }

    //------------------------------------------------------------------------------------------------
    //! 已在休整恢复中且当前为战场危险：终止休整（清锁定、取消战斗移动请求、释放掩护）。由 PlayerBase 体力 tick 定时调用。
    static void TickAbortRestRecoveryIfBattlefieldDanger(IEntity owner)
    {
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;
        if (!IsAliveAndActionableForGroupRest(owner))
        {
            if (SCR_RSS_AIRestRecoveryRegistry.IsInRestRecovery(owner))
                SCR_RSS_AIRestRecoveryRegistry.ClearRestRecovery(owner);
            return;
        }
        if (!SCR_RSS_AIRestRecoveryRegistry.IsInRestRecovery(owner))
            return;
        if (!IsBattlefieldDangerContext(owner))
            return;

        SCR_RSS_AIRestRecoveryRegistry.ClearRestRecovery(owner);

        SCR_AIUtilityComponent util = ResolveAIUtilityForCharacter(owner);
        if (!util)
            return;
        SCR_AICombatMoveState state = util.m_CombatMoveState;
        if (!state)
            return;
        state.CancelRequest();
        state.ReleaseCover();
    }

    //------------------------------------------------------------------------------------------------
    //! 泥泞「巡逻徒步」：仅在 THREATENED（压制）时不强制 WALK；SAFE/VIGILANT/ALERTED 仍可在滑倒预警下走路。
    //! 与 ShouldIgnoreMudSlipSpeedCap 中威胁判定一致（仅 THREATENED 解除安全巡逻）。
    protected static bool ThreatStateAllowsMudPatrolWalk(IEntity owner)
    {
        if (!owner)
            return true;
        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return true;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return true;
        SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(agent.FindComponent(SCR_AIInfoComponent));
        if (!info)
            return true;
        EAIThreatState threat = info.GetThreatState();
        if (threat == EAIThreatState.THREATENED)
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! AI「危险上下文」：THREATENED，或高优先级动作 → 不限速，且允许泥泞 Ragdoll（与 Allow 一致）。
    protected static bool ShouldIgnoreMudSlipSpeedCap(IEntity owner)
    {
        if (!owner)
            return false;

        if (ThreatStateIndicatesMudSlipCombatDanger(owner))
            return true;

        if (ActionPriorityIndicatesDanger(owner))
            return true;

        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! AI 安全（巡逻等）：泥泞 Poisson 与 Ragdoll 在 Runner 内可不执行；与限速无关，限速仅为预计区预警。
    static bool IsMudSlipBlockedBySafety(SCR_CharacterControllerComponent ctrl, IEntity owner)
    {
        if (!ctrl)
            return true;
        if (ctrl.IsPlayerControlled())
            return false;
        if (!owner)
            return true;
        if (ShouldIgnoreMudSlipSpeedCap(owner))
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 是否允许执行泥泞滑倒掷骰；玩家恒真，AI 仅危险上下文为真。
    static bool ShouldAllowMudSlipRagdoll(SCR_CharacterControllerComponent ctrl, IEntity owner)
    {
        if (IsMudSlipBlockedBySafety(ctrl, owner))
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 与 SCR_AICharacterSetMovementSpeed 相同：经 AI 移动速度设置裁剪后再写入。
    protected static void AISetMovementTypeWantedRespectingSettings(IEntity owner, AICharacterMovementComponent aiMove, EMovementType speed)
    {
        if (!owner || !aiMove)
            return;
        EMovementType resolved = speed;
        SCR_AICharacterSettingsComponent settingsComp = SCR_AICharacterSettingsComponent.Cast(owner.FindComponent(SCR_AICharacterSettingsComponent));
        if (settingsComp)
        {
            SCR_AICharacterMovementSpeedSettingBase setting = SCR_AICharacterMovementSpeedSettingBase.Cast(settingsComp.GetCurrentSetting(SCR_AICharacterMovementSpeedSettingBase));
            if (setting)
                resolved = setting.GetSpeed(resolved);
        }
        aiMove.SetMovementTypeWanted(resolved);
    }

    //------------------------------------------------------------------------------------------------
    //! 服务器 AI、徒步、非游泳：低体力或（安全 AI）泥泞应激时，将快跑意图降为行走。
    static void ApplyOnFootMovementPolicy(SCR_CharacterControllerComponent ctrl, IEntity owner, float staminaPercent01)
    {
        if (!ctrl || !owner)
            return;
        if (ctrl.IsPlayerControlled())
            return;
        if (!Replication.IsServer())
            return;

        if (!IsAliveAndActionableForGroupRest(owner))
        {
            if (SCR_RSS_AIRestRecoveryRegistry.IsInRestRecovery(owner))
                SCR_RSS_AIRestRecoveryRegistry.ClearRestRecovery(owner);
            return;
        }

        if (SwimmingStateManager.IsSwimming(ctrl))
            return;

        ChimeraCharacter ch = ChimeraCharacter.Cast(owner);
        if (!ch)
            return;
        CompartmentAccessComponent compAccess = ch.GetCompartmentAccessComponent();
        if (compAccess && compAccess.GetCompartment())
            return;

        AICharacterMovementComponent aiMove = AICharacterMovementComponent.Cast(owner.FindComponent(AICharacterMovementComponent));
        if (!aiMove)
            return;

        if (SCR_RSS_AIRestRecoveryRegistry.IsInRestRecovery(owner))
        {
            if (staminaPercent01 >= StaminaConstants.RSS_AI_REST_RECOVERY_RESUME_STAMINA_MIN)
            {
                SCR_RSS_AIRestRecoveryRegistry.ClearRestRecovery(owner);
            }
            else
            {
                AISetMovementTypeWantedRespectingSettings(owner, aiMove, EMovementType.WALK);
                return;
            }
        }

        if (staminaPercent01 < StaminaConstants.RSS_AI_ONFOOT_STAMINA_WALK_THRESHOLD)
        {
            AISetMovementTypeWantedRespectingSettings(owner, aiMove, EMovementType.WALK);
            return;
        }

        float mudStress = ctrl.RSS_GetMudSlipCameraShake01();
        if (mudStress >= StaminaConstants.ENV_MUD_SLIP_AI_WARN_STRESS_MIN)
        {
            if (!ActionPriorityIndicatesDanger(owner) && ThreatStateAllowsMudPatrolWalk(owner))
                AISetMovementTypeWantedRespectingSettings(owner, aiMove, EMovementType.WALK);
        }
    }

    //------------------------------------------------------------------------------------------------
    //! 仅服务器 AI：安全且已进入预计区（应力阈值）时压速作预警；危险上下文不限速。
    //! 满足条件时始终 OverrideMaxSpeed 到 cap（不再依赖「上次倍率是否高于 cap」），避免未压速。
    static void MaybeApplyMudSlipSpeedCap(SCR_CharacterControllerComponent ctrl, IEntity owner)
    {
        if (!owner)
            return;
        if (!ctrl)
            return;
        if (ctrl.IsPlayerControlled())
            return;
        if (!Replication.IsServer())
            return;
        float mudStress = ctrl.RSS_GetMudSlipCameraShake01();
        if (mudStress < StaminaConstants.ENV_MUD_SLIP_AI_WARN_STRESS_MIN)
            return;
        if (ShouldIgnoreMudSlipSpeedCap(owner))
            return;
        float capMul = StaminaConstants.ENV_MUD_SLIP_MIN_SPEED_MS / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
        ctrl.OverrideMaxSpeed(capMul);
    }

    //------------------------------------------------------------------------------------------------
    static void AppendAIDebugLine(string line)
    {
        if (!s_aAIDebugLines)
            s_aAIDebugLines = new array<string>();
        s_aAIDebugLines.Insert(line);
        if (s_aAIDebugLines.Count() > 15)
            s_aAIDebugLines.RemoveOrdered(0);
    }

    //------------------------------------------------------------------------------------------------
    static void FlushAIDebugLinesToBatch()
    {
        if (!s_aAIDebugLines)
            return;
        for (int i = 0; i < s_aAIDebugLines.Count(); i++)
            StaminaConstants.AddDebugBatchLine(s_aAIDebugLines.Get(i));
        s_aAIDebugLines.Clear();
    }
}
