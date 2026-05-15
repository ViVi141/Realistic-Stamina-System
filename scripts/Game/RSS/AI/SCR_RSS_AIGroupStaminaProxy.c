//! RSS AI Group Stamina Proxy — 远距非交战群组仅队长全量计算体力，队员同步。

class SCR_RSS_AIGroupStaminaProxy
{
    protected static ref map<int, float> s_mGroupBattleCacheTimeS;
    protected static ref map<int, bool> s_mGroupBattleCacheResult;
    protected static ref map<int, float> s_mGroupLeaderLastFullCalcTimeS;

    //------------------------------------------------------------------------------------------------
    static void ClearAllForNewWorldSession()
    {
        if (s_mGroupBattleCacheTimeS)
            s_mGroupBattleCacheTimeS.Clear();
        if (s_mGroupBattleCacheResult)
            s_mGroupBattleCacheResult.Clear();
        if (s_mGroupLeaderLastFullCalcTimeS)
            s_mGroupLeaderLastFullCalcTimeS.Clear();
    }

    //------------------------------------------------------------------------------------------------
    static SCR_AIGroup ResolveGroup(IEntity owner)
    {
        if (!owner)
            return null;
        AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiCtrl)
            return null;
        AIAgent agent = aiCtrl.GetAIAgent();
        if (!agent)
            return null;
        AIGroup grpAny = agent.GetParentGroup();
        if (!grpAny)
            return null;
        return SCR_AIGroup.Cast(grpAny);
    }

    //------------------------------------------------------------------------------------------------
    static bool IsProxyModeActive(IEntity owner, float distToNearestPlayerM, SCR_AIGroup scrGrp)
    {
        if (!StaminaConstants.RSS_PERF_AI_GROUP_PROXY_ENABLED)
            return false;
        if (!Replication.IsServer())
            return false;
        if (!owner || !scrGrp)
            return false;
        if (distToNearestPlayerM < 0.0)
            return false;
        if (distToNearestPlayerM <= StaminaConstants.RSS_PERF_AI_GROUP_PROXY_DISTANCE_M)
            return false;
        if (IsGroupInBattleContext(scrGrp))
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    static bool IsFollower(IEntity owner, SCR_AIGroup scrGrp)
    {
        if (!owner || !scrGrp)
            return false;
        IEntity leader = scrGrp.GetLeaderEntity();
        if (!leader)
            return false;
        return owner != leader;
    }

    //------------------------------------------------------------------------------------------------
    static bool ShouldLeaderRunFullCalc(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return true;

        int gid = scrGrp.GetGroupID();
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;

        EnsureLeaderTimeMap();
        float lastS = 0.0;
        if (s_mGroupLeaderLastFullCalcTimeS.Contains(gid))
            lastS = s_mGroupLeaderLastFullCalcTimeS.Get(gid);

        float intervalSec = StaminaConstants.RSS_PERF_AI_GROUP_PROXY_INTERVAL_MS / 1000.0;
        if (lastS <= 0.0)
            return true;
        if (nowS - lastS >= intervalSec)
            return true;
        return false;
    }

    //------------------------------------------------------------------------------------------------
    static void NotifyLeaderFullTickCompleted(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return;
        int gid = scrGrp.GetGroupID();
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;
        EnsureLeaderTimeMap();
        s_mGroupLeaderLastFullCalcTimeS.Set(gid, nowS);
    }

    //------------------------------------------------------------------------------------------------
    //! 队员：同步队长体力与速度倍率，跳过本 tick 全量 Pandolf。
    static void ApplyFollowerSync(IEntity follower, SCR_AIGroup scrGrp)
    {
        if (!follower || !scrGrp)
            return;

        IEntity leader = scrGrp.GetLeaderEntity();
        if (!leader)
            return;

        SCR_CharacterStaminaComponent leaderStm = SCR_CharacterStaminaComponent.Cast(
            leader.FindComponent(SCR_CharacterStaminaComponent));
        SCR_CharacterStaminaComponent followerStm = SCR_CharacterStaminaComponent.Cast(
            follower.FindComponent(SCR_CharacterStaminaComponent));
        if (leaderStm && followerStm)
        {
            float leaderStamina = Math.Clamp(leaderStm.GetTargetStamina(), 0.0, 1.0);
            followerStm.SetTargetStamina(leaderStamina);
        }

        SCR_CharacterControllerComponent leaderCtrl = SCR_CharacterControllerComponent.Cast(
            leader.FindComponent(SCR_CharacterControllerComponent));
        SCR_CharacterControllerComponent followerCtrl = SCR_CharacterControllerComponent.Cast(
            follower.FindComponent(SCR_CharacterControllerComponent));
        if (leaderCtrl && followerCtrl)
        {
            float leaderMul = leaderCtrl.RSS_GetLastAppliedSpeedMultiplier();
            if (leaderMul <= 0.0)
                leaderMul = 1.0;
            followerCtrl.OverrideMaxSpeed(Math.Clamp(leaderMul, 0.01, 1.0));
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
    protected static bool IsGroupInBattleContext(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return false;

        int gid = scrGrp.GetGroupID();
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;

        EnsureBattleCacheMaps();
        if (s_mGroupBattleCacheTimeS.Contains(gid))
        {
            float cachedAt = s_mGroupBattleCacheTimeS.Get(gid);
            if (nowS - cachedAt < StaminaConstants.RSS_PERF_AI_GROUP_BATTLE_CACHE_SEC)
            {
                if (s_mGroupBattleCacheResult.Contains(gid))
                    return s_mGroupBattleCacheResult.Get(gid);
            }
        }

        bool inBattle = false;
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        for (int i = 0; i < ac; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            if (IsEntityInBattleContext(ce))
            {
                inBattle = true;
                break;
            }
        }

        s_mGroupBattleCacheTimeS.Set(gid, nowS);
        s_mGroupBattleCacheResult.Set(gid, inBattle);
        return inBattle;
    }

    //------------------------------------------------------------------------------------------------
    protected static bool IsEntityInBattleContext(IEntity entity)
    {
        if (!entity)
            return false;

        if (SCR_RSS_AIGroupSync.GetThreatState(entity) == EAIThreatState.THREATENED)
            return true;

        SCR_AIUtilityComponent utility = ResolveUtility(entity);
        if (!utility)
            return false;

        AIActionBase action = utility.GetCurrentAction();
        SCR_AIActionBase scrAct = SCR_AIActionBase.Cast(action);
        if (!scrAct)
            return false;

        float priorityLevel = scrAct.EvaluatePriorityLevel();
        if (priorityLevel >= StaminaConstants.ENV_MUD_SLIP_AI_UNSAFE_PRIORITY_MIN)
            return true;

        return false;
    }

    //------------------------------------------------------------------------------------------------
    protected static void EnsureBattleCacheMaps()
    {
        if (!s_mGroupBattleCacheTimeS)
            s_mGroupBattleCacheTimeS = new map<int, float>();
        if (!s_mGroupBattleCacheResult)
            s_mGroupBattleCacheResult = new map<int, bool>();
    }

    //------------------------------------------------------------------------------------------------
    protected static void EnsureLeaderTimeMap()
    {
        if (!s_mGroupLeaderLastFullCalcTimeS)
            s_mGroupLeaderLastFullCalcTimeS = new map<int, float>();
    }
}
