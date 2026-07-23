//! AI 体力刷新间隔 / Workbench 预览过滤 / 群组 debug（从 PlayerBase.c 拆分）

class SCR_RSS_AIUpdateInterval
{
    static float GetNearestPlayerDistanceM(IEntity ownerEntity)
    {
        if (!ownerEntity)
            return -1.0;

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return -1.0;

        array<int> playerIds = {};
        pm.GetPlayers(playerIds);
        float nearM = 99999.0;
        for (int pi = 0; pi < playerIds.Count(); pi++)
        {
            IEntity pe = pm.GetPlayerControlledEntity(playerIds.Get(pi));
            if (pe)
            {
                float d = vector.Distance(ownerEntity.GetOrigin(), pe.GetOrigin());
                if (d < nearM)
                    nearM = d;
            }
        }
        if (nearM < 99999.0)
            return nearM;
        return -1.0;
    }

    static int GetSpeedUpdateIntervalMs(bool isPlayerControlled, IEntity ownerEntity)
    {
        if (isPlayerControlled)
            return SCR_RSS_AIConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS;

        if (!Replication.IsServer())
            return SCR_RSS_AIConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        if (!SCR_RSS_AIConstants.RSS_PERF_AI_DISTANCE_LOD_ENABLED)
            return SCR_RSS_AIConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        if (!ownerEntity)
            return SCR_RSS_AIConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        float distM = GetNearestPlayerDistanceM(ownerEntity);

        if (distM < 0.0 || distM <= SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_M)
            return SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_INTERVAL_MS;
        if (distM <= SCR_RSS_AIConstants.RSS_PERF_AI_LOD_FAR_M)
            return SCR_RSS_AIConstants.RSS_PERF_AI_LOD_MID_INTERVAL_MS;
        return SCR_RSS_AIConstants.RSS_PERF_AI_LOD_FAR_INTERVAL_MS;
    }

    static bool IsWorkbenchPreviewEntity(bool isPlayerControlled, IEntity owner)
    {
#ifdef WORKBENCH
        if (!owner)
            return true;
        if (isPlayerControlled)
            return false;
        AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiCtrl)
            return true;
        AIAgent agent = aiCtrl.GetAIAgent();
        if (!agent)
            return true;
        AIGroup parentGroup = agent.GetParentGroup();
        if (!parentGroup)
            return true;
        return false;
#else
        return false;
#endif
    }

    static float CalcAiGroupSpreadM(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return -1.0;

        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        if (ac < 2)
            return -1.0;

        float maxDist = 0.0;
        for (int i = 0; i < ac - 1; i++)
        {
            AIAgent agI = agents.Get(i);
            if (!agI)
                continue;
            IEntity ceI = agI.GetControlledEntity();
            if (!ceI)
                continue;
            for (int j = i + 1; j < ac; j++)
            {
                AIAgent agJ = agents.Get(j);
                if (!agJ)
                    continue;
                IEntity ceJ = agJ.GetControlledEntity();
                if (!ceJ)
                    continue;
                float d = vector.Distance(ceI.GetOrigin(), ceJ.GetOrigin());
                if (d > maxDist)
                    maxDist = d;
            }
        }
        return maxDist;
    }

    static int GetAliveMemberCount(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return 0;

        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        int alive = 0;
        for (int i = 0; i < ac; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            if (SCR_CharacterDamageManagerComponent.Cast(ce.FindComponent(SCR_CharacterDamageManagerComponent)))
                alive++;
        }
        return alive;
    }
}
