//! RSS AI Intent Filter — 行为意图过滤
//!
//! 力竭时禁用 Attack / 追击 / 个别移动行为，并抬高 Wait。
//! Attack 使用 CustomEvaluate()，SetPriority(0) 无效；改用 SetStateAllActionsOfType(COMPLETED)。

class SCR_RSS_AIIntentFilter
{
    //------------------------------------------------------------------------------------------------
    static void Apply(IEntity owner, ERSS_AIStaminaState state, ERSS_AIStaminaState prevState, bool isThreatened)
    {
        if (!SCR_RSS_ConfigBridge.IsAIIntentFilterEnabled())
            return;
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_AIUtilityComponent utility = ResolveUtility(owner);
        if (!utility)
            return;

        bool wasRestrictive = IsRestrictiveState(prevState);
        bool isRestrictive = IsRestrictiveState(state);

        if (wasRestrictive && !isRestrictive)
            RestoreCombatActions(utility);

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
            break;
        }
    }

    //------------------------------------------------------------------------------------------------
    protected static bool IsRestrictiveState(ERSS_AIStaminaState state)
    {
        if (state == ERSS_AIStaminaState.EXHAUSTED)
            return true;
        if (state == ERSS_AIStaminaState.COLLAPSED)
            return true;
        if (state == ERSS_AIStaminaState.RECOVERING)
            return true;
        return false;
    }

    //------------------------------------------------------------------------------------------------
    protected static void ApplyExhaustedFilter(SCR_AIUtilityComponent utility, bool isThreatened)
    {
        if (isThreatened)
            return;

        BlockActionType(utility, SCR_AIAttackBehavior);
        BlockActionType(utility, SCR_AIMoveAndInvestigateBehavior);
        PromoteWait(utility);
    }

    //------------------------------------------------------------------------------------------------
    protected static void ApplyCollapsedFilter(SCR_AIUtilityComponent utility, bool isThreatened)
    {
        BlockActionType(utility, SCR_AIMoveIndividuallyBehavior);
        BlockActionType(utility, SCR_AIMoveAndInvestigateBehavior);

        if (!isThreatened)
            BlockActionType(utility, SCR_AIAttackBehavior);

        PromoteWait(utility);
    }

    //------------------------------------------------------------------------------------------------
    protected static void ApplyRecoveringFilter(SCR_AIUtilityComponent utility)
    {
        BlockActionType(utility, SCR_AIMoveAndInvestigateBehavior);
        PromoteWait(utility);
    }

    //------------------------------------------------------------------------------------------------
    protected static void RestoreCombatActions(SCR_AIUtilityComponent utility)
    {
        if (!utility)
            return;

        RestoreActionType(utility, SCR_AIAttackBehavior);
        RestoreActionType(utility, SCR_AIMoveAndInvestigateBehavior);
        RestoreActionType(utility, SCR_AIMoveIndividuallyBehavior);

        AIActionBase waitAction = utility.FindActionOfType(SCR_AIWaitBehavior);
        SCR_AIActionBase scrWait = SCR_AIActionBase.Cast(waitAction);
        if (scrWait)
        {
            scrWait.SetPriority(SCR_AIActionBase.PRIORITY_BEHAVIOR_WAIT);
            scrWait.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
        }
    }

    //------------------------------------------------------------------------------------------------
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
    //! 禁用行为：COMPLETED 使 CustomEvaluate / 选型跳过该类型（含 SCR_AIAttackBehavior）。
    protected static void BlockActionType(SCR_AIUtilityComponent utility, typename actionType)
    {
        if (!utility)
            return;

        utility.SetStateAllActionsOfType(actionType, EAIActionState.COMPLETED, true);

        AIActionBase action = utility.FindActionOfType(actionType);
        SCR_AIActionBase scrAct = SCR_AIActionBase.Cast(action);
        if (scrAct)
            scrAct.SetPriority(0);
    }

    //------------------------------------------------------------------------------------------------
    protected static void RestoreActionType(SCR_AIUtilityComponent utility, typename actionType)
    {
        if (!utility)
            return;

        utility.SetStateAllActionsOfType(actionType, EAIActionState.EVALUATED, true);

        AIActionBase action = utility.FindActionOfType(actionType);
        SCR_AIActionBase scrAct = SCR_AIActionBase.Cast(action);
        if (scrAct)
            scrAct.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
    }

    //------------------------------------------------------------------------------------------------
    protected static void PromoteWait(SCR_AIUtilityComponent utility)
    {
        if (!utility)
            return;

        AIActionBase waitAction = utility.FindActionOfType(SCR_AIWaitBehavior);
        SCR_AIActionBase scrWait = SCR_AIActionBase.Cast(waitAction);
        if (scrWait)
            scrWait.SetPriority(SCR_RSS_AIConstants.RSS_AI_INTENT_WAIT_PROMOTED_PRIORITY);
    }
}
