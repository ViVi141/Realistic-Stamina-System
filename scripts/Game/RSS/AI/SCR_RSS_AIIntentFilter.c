//! RSS AI Intent Filter — 行为意图过滤
//!
//! 在体力状态极端时（EXHAUSTED / COLLAPSED）：
//!   过滤攻击和追击行为，替换为 Wait / FindFirePosition
//!
//! 通过 FindActionOfType + SetPriority 提权实现（非直接替换行为树）

class SCR_RSS_AIIntentFilter
{
    //------------------------------------------------------------------------------------------------
    //! 主入口
    static void Apply(IEntity owner, ERSS_AIStaminaState state, bool isThreatened)
    {
        if (!StaminaConfigBridge.IsAIIntentFilterEnabled())
            return;
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_AIUtilityComponent utility = ResolveUtility(owner);
        if (!utility)
            return;

        switch (state)
        {
        case ERSS_AIStaminaState.EXHAUSTED:
            ApplyExhaustedFilter(utility, isThreatened);
            break;

        case ERSS_AIStaminaState.COLLAPSED:
            ApplyCollapsedFilter(utility, isThreatened);
            break;

        case ERSS_AIStaminaState.RECOVERING:
            ApplyRecoveringFilter(utility);
            break;

        default:
            // FATIGUED 及以上：不干预战斗决策
            break;
        }
    }

    //------------------------------------------------------------------------------------------------
    //! EXHAUSTED + 非被压制 → 禁止主动攻击和追击
    protected static void ApplyExhaustedFilter(SCR_AIUtilityComponent utility, bool isThreatened)
    {
        if (isThreatened)
        {
            // 被压制时仍可战斗，但只原地射击（降级，不拦截）
            return;
        }

        // 禁止 Attack
        BlockActionType(utility, SCR_AIAttackBehavior);
        // 禁止追击/调查
        BlockActionType(utility, SCR_AIMoveAndInvestigateBehavior);
        // 提权 Wait
        PromoteWait(utility);
    }

    //------------------------------------------------------------------------------------------------
    //! COLLAPSED → 禁止所有移动行为（保留自卫射击）
    protected static void ApplyCollapsedFilter(SCR_AIUtilityComponent utility, bool isThreatened)
    {
        // 禁止移动
        BlockActionType(utility, SCR_AIMoveIndividuallyBehavior);
        BlockActionType(utility, SCR_AIMoveAndInvestigateBehavior);

        if (!isThreatened)
        {
            // 非被压制时也禁止攻击
            BlockActionType(utility, SCR_AIAttackBehavior);
        }
        // 被压制时保留 Attack（最后手段）

        PromoteWait(utility);
    }

    //------------------------------------------------------------------------------------------------
    //! RECOVERING → 禁止重新主动出击，直到体力恢复
    protected static void ApplyRecoveringFilter(SCR_AIUtilityComponent utility)
    {
        BlockActionType(utility, SCR_AIMoveAndInvestigateBehavior);
        PromoteWait(utility);
    }

    //------------------------------------------------------------------------------------------------
    //! 从 AI 实体解析 SCR_AIUtilityComponent
    protected static SCR_AIUtilityComponent ResolveUtility(IEntity owner)
    {
        if (!owner)
            return null;

        SCR_AIUtilityComponent aiUtil = SCR_AIUtilityComponent.Cast(
            owner.FindComponent(SCR_AIUtilityComponent));
        if (aiUtil)
            return aiUtil;

        AIControlComponent aiControl = AIControlComponent.Cast(
            owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return null;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return null;
        return SCR_AIUtilityComponent.Cast(
            agent.FindComponent(SCR_AIUtilityComponent));
    }

    //------------------------------------------------------------------------------------------------
    //! 禁止指定类型的行为：获取已有实例并将其优先级设到极低。
    protected static void BlockActionType(SCR_AIUtilityComponent utility, typename actionType)
    {
        if (!utility)
            return;

        AIActionBase action = utility.FindActionOfType(actionType);
        SCR_AIActionBase scrAct = SCR_AIActionBase.Cast(action);
        if (scrAct)
            scrAct.SetPriority(0);
    }

    //------------------------------------------------------------------------------------------------
    //! 提权 Wait 行为：让它在下帧决策中被选中。
    protected static void PromoteWait(SCR_AIUtilityComponent utility)
    {
        if (!utility)
            return;

        AIActionBase waitAction = utility.FindActionOfType(SCR_AIWaitBehavior);
        SCR_AIActionBase scrWait = SCR_AIActionBase.Cast(waitAction);
        if (scrWait)
            scrWait.SetPriority(StaminaConstants.RSS_AI_INTENT_WAIT_PROMOTED_PRIORITY);
    }
}
