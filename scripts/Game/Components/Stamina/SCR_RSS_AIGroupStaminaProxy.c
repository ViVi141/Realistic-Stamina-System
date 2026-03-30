//! 服端性能：非交战 AI 群组在距玩家较远时仅由队长全量计算体力，队员同步体力与速度倍率。

class SCR_RSS_AIGroupStaminaProxy
{
    protected static float s_fPlayerPosCacheTimeS = -1000.0;
    protected static ref array<vector> s_aCachedPlayerPositions;

    //------------------------------------------------------------------------------------------------
    protected static void EnsurePlayerPositionsCache(float nowSec)
    {
        if (s_fPlayerPosCacheTimeS >= 0.0 && (nowSec - s_fPlayerPosCacheTimeS) < 1.0)
            return;
        s_fPlayerPosCacheTimeS = nowSec;
        if (!s_aCachedPlayerPositions)
            s_aCachedPlayerPositions = new array<vector>();
        s_aCachedPlayerPositions.Clear();

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return;
        array<int> ids = new array<int>();
        pm.GetPlayers(ids);
        int n = ids.Count();
        for (int i = 0; i < n; i++)
        {
            IEntity pe = pm.GetPlayerControlledEntity(ids.Get(i));
            if (pe)
                s_aCachedPlayerPositions.Insert(pe.GetOrigin());
        }
    }

    //------------------------------------------------------------------------------------------------
    static float GetMinDistanceToAnyPlayerMeters(vector refPos)
    {
        float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        EnsurePlayerPositionsCache(nowSec);
        if (!s_aCachedPlayerPositions || s_aCachedPlayerPositions.Count() == 0)
            return 0.0;

        float minD = 1.0e12;
        int c = s_aCachedPlayerPositions.Count();
        for (int i = 0; i < c; i++)
        {
            float d = vector.Distance(refPos, s_aCachedPlayerPositions.Get(i));
            if (d < minD)
                minD = d;
        }
        return minD;
    }

    //------------------------------------------------------------------------------------------------
    protected static bool GroupHasAnyBattlefieldDanger(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return false;
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
            if (SCR_RSS_AIStaminaBridge.IsBattlefieldDangerContext(ce))
                return true;
        }
        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! 与群组休整中队长解析思路一致：优先 GetLeaderEntity，否则首个可用步兵。
    static IEntity ResolveLeaderEntity(SCR_AIGroup scrGrp, IEntity memberEntity)
    {
        if (!scrGrp)
            return null;
        IEntity le = scrGrp.GetLeaderEntity();
        if (le && SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(le))
            return le;
        AIAgent lag = scrGrp.GetLeaderAgent();
        if (lag)
        {
            IEntity ce = lag.GetControlledEntity();
            if (ce && SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(ce))
                return ce;
        }
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        for (int ai = 0; ai < ac; ai++)
        {
            AIAgent ag = agents.Get(ai);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            if (!SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(ce))
                continue;
            return ce;
        }
        if (memberEntity && SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(memberEntity))
            return memberEntity;
        return null;
    }

    //------------------------------------------------------------------------------------------------
    //! 群组满足：服端 AI、有群、全员非交战危险、队长参考点距玩家 > 阈值。
    static bool IsSquadProxyModeActive(IEntity owner, out SCR_AIGroup outGrp, out IEntity outLeader)
    {
        outGrp = null;
        outLeader = null;
        if (!StaminaConstants.RSS_PERF_AI_GROUP_PROXY_ENABLED)
            return false;
        if (!owner)
            return false;
        if (!Replication.IsServer())
            return false;

        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return false;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return false;
        AIGroup grpAny = agent.GetParentGroup();
        if (!grpAny)
            return false;
        SCR_AIGroup scrGrp = SCR_AIGroup.Cast(grpAny);
        if (!scrGrp)
            return false;

        if (GroupHasAnyBattlefieldDanger(scrGrp))
            return false;

        IEntity leader = ResolveLeaderEntity(scrGrp, owner);
        if (!leader)
            return false;

        vector refPos = leader.GetOrigin();
        float distMin = GetMinDistanceToAnyPlayerMeters(refPos);
        if (distMin <= StaminaConstants.RSS_PERF_AI_GROUP_PROXY_DISTANCE_M)
            return false;

        outGrp = scrGrp;
        outLeader = leader;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 队长在代理模式下使用较长 tick（与队员同步周期一致）。
    static bool ShouldLeaderUseProxyInterval(SCR_CharacterControllerComponent ctrl, IEntity owner)
    {
        if (!ctrl || !owner)
            return false;
        if (!Replication.IsServer() || ctrl.IsPlayerControlled())
            return false;

        SCR_AIGroup g;
        IEntity leader;
        if (!IsSquadProxyModeActive(owner, g, leader))
            return false;
        if (owner != leader)
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 跟随者：从队长同步体力与 OverrideMaxSpeed；返回 true 表示已处理并应跳过后续 RSS 主路径。
    static bool ProcessFollowerProxySync(SCR_CharacterControllerComponent ctrl, IEntity owner)
    {
        if (!ctrl || !owner)
            return false;
        if (!Replication.IsServer() || ctrl.IsPlayerControlled())
            return false;
        if (!StaminaConstants.RSS_PERF_AI_GROUP_PROXY_ENABLED)
            return false;

        SCR_AIGroup g;
        IEntity leader;
        if (!IsSquadProxyModeActive(owner, g, leader))
            return false;
        if (owner == leader)
            return false;

        SCR_CharacterControllerComponent leadCtrl = SCR_CharacterControllerComponent.Cast(leader.FindComponent(SCR_CharacterControllerComponent));
        if (!leadCtrl)
            return false;

        SCR_CharacterStaminaComponent stm = SCR_CharacterStaminaComponent.Cast(owner.FindComponent(SCR_CharacterStaminaComponent));
        SCR_CharacterStaminaComponent stmL = SCR_CharacterStaminaComponent.Cast(leader.FindComponent(SCR_CharacterStaminaComponent));
        if (stm && stmL)
            stm.SetTargetStamina(stmL.GetTargetStamina());

        ctrl.OverrideMaxSpeed(leadCtrl.RSS_GetLastAppliedSpeedMultiplier());
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 单兵距离 LOD：未命中群组代理时使用。
    static int GetDistanceLodIntervalMsForAi(SCR_CharacterControllerComponent ctrl, IEntity owner, int baseAiMs)
    {
        if (!StaminaConstants.RSS_PERF_AI_DISTANCE_LOD_ENABLED)
            return baseAiMs;
        if (!ctrl || !owner)
            return baseAiMs;
        if (!Replication.IsServer() || ctrl.IsPlayerControlled())
            return baseAiMs;

        vector p = owner.GetOrigin();
        float d = GetMinDistanceToAnyPlayerMeters(p);
        if (d <= StaminaConstants.RSS_PERF_AI_LOD_NEAR_M)
            return StaminaConstants.RSS_PERF_AI_LOD_NEAR_INTERVAL_MS;
        if (d <= StaminaConstants.RSS_PERF_AI_LOD_FAR_M)
            return StaminaConstants.RSS_PERF_AI_LOD_MID_INTERVAL_MS;
        return StaminaConstants.RSS_PERF_AI_LOD_FAR_INTERVAL_MS;
    }
}
