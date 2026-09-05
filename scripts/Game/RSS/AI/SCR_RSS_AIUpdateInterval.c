//! AI 体力刷新间隔 / Workbench 预览过滤 / 群组 debug（从 PlayerBase.c 拆分）

class SCR_RSS_AIUpdateInterval
{
    protected static ref array<int> s_aReusablePlayerIds;
    protected static ref array<vector> s_aCachedPlayerOrigins;
    protected static float s_fPlayerPosCacheTimeSec = -1.0;

    //! 刷新全服玩家原点缓存（所有 AI 共享，避免每 tick 每 AI 分配 + GetPlayers）
    protected static void RefreshPlayerOriginsCacheIfNeeded()
    {
        float nowSec = 0.0;
        if (GetGame())
        {
            World world = GetGame().GetWorld();
            if (world)
                nowSec = world.GetWorldTime() / 1000.0;
        }

        float ttl = SCR_RSS_AIConstants.RSS_PERF_AI_PLAYER_POS_CACHE_TTL_SEC;
        if (s_fPlayerPosCacheTimeSec >= 0.0)
        {
            if ((nowSec - s_fPlayerPosCacheTimeSec) < ttl)
                return;
        }

        if (!s_aReusablePlayerIds)
            s_aReusablePlayerIds = new array<int>();
        else
            s_aReusablePlayerIds.Clear();

        if (!s_aCachedPlayerOrigins)
            s_aCachedPlayerOrigins = new array<vector>();
        else
            s_aCachedPlayerOrigins.Clear();

        PlayerManager pm = null;
        if (GetGame())
            pm = GetGame().GetPlayerManager();
        if (!pm)
        {
            s_fPlayerPosCacheTimeSec = nowSec;
            return;
        }

        pm.GetPlayers(s_aReusablePlayerIds);
        int n = s_aReusablePlayerIds.Count();
        for (int i = 0; i < n; i++)
        {
            IEntity pe = pm.GetPlayerControlledEntity(s_aReusablePlayerIds.Get(i));
            if (!pe)
                continue;
            s_aCachedPlayerOrigins.Insert(pe.GetOrigin());
        }

        s_fPlayerPosCacheTimeSec = nowSec;
    }

    static float GetNearestPlayerDistanceM(IEntity ownerEntity)
    {
        if (!ownerEntity)
            return -1.0;

        RefreshPlayerOriginsCacheIfNeeded();
        if (!s_aCachedPlayerOrigins)
            return -1.0;
        if (s_aCachedPlayerOrigins.IsEmpty())
            return -1.0;

        vector ownerPos = ownerEntity.GetOrigin();
        float nearSq = 99999.0 * 99999.0;
        bool any = false;
        int n = s_aCachedPlayerOrigins.Count();
        for (int i = 0; i < n; i++)
        {
            float dSq = vector.DistanceSq(ownerPos, s_aCachedPlayerOrigins.Get(i));
            if (!any)
            {
                nearSq = dSq;
                any = true;
            }
            else if (dSq < nearSq)
            {
                nearSq = dSq;
            }
        }

        if (!any)
            return -1.0;
        return Math.Sqrt(nearSq);
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

        bool lightMode = SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled();

        if (distM < 0.0 || distM <= SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_M)
        {
            if (lightMode)
                return SCR_RSS_AIConstants.RSS_PERF_AI_LIGHT_NEAR_INTERVAL_MS;
            return SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_INTERVAL_MS;
        }
        if (distM <= SCR_RSS_AIConstants.RSS_PERF_AI_LOD_FAR_M)
        {
            if (lightMode)
                return SCR_RSS_AIConstants.RSS_PERF_AI_LIGHT_MID_INTERVAL_MS;
            return SCR_RSS_AIConstants.RSS_PERF_AI_LOD_MID_INTERVAL_MS;
        }
        if (lightMode)
            return SCR_RSS_AIConstants.RSS_PERF_AI_LIGHT_FAR_INTERVAL_MS;
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
