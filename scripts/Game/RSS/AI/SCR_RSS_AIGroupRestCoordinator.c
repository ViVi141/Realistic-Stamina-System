//! 低体力时以 AI 群组为单位休整：任一员低于阈值时，由队长对群组重心做 Cover 查询（可选），
//! 再向 SCR_AIGroup 插入 AIWaypoint_Defend 动态路点；全员体力≥RSS_AI_REST_RECOVERY_RESUME_STAMINA_MIN 时 CompleteWaypoint。
//! 体力恢复锁定见 SCR_RSS_AIRestRecoveryRegistry。

class SCR_RSS_AIGroupRestCoordinator
{
    protected static ref map<int, float> s_mGroupRestNextAllowedWorldTimeS;
    //! 本模组插入的 RSS 低体力防守路点（按群组 ID），全员体力达标后 CompleteWaypoint。
    protected static ref map<int, AIWaypoint> s_mGroupRssDefendWaypointByGid;
    protected static ref TraceParam s_pCoverTrace;
    protected static ref TraceParam s_pGroundTrace;

    //------------------------------------------------------------------------------------------------
    protected static void EnsureCooldownMap()
    {
        if (!s_mGroupRestNextAllowedWorldTimeS)
            s_mGroupRestNextAllowedWorldTimeS = new map<int, float>();
    }

    //------------------------------------------------------------------------------------------------
    protected static void EnsureRssDefendWaypointMap()
    {
        if (!s_mGroupRssDefendWaypointByGid)
            s_mGroupRssDefendWaypointByGid = new map<int, AIWaypoint>();
    }

    //------------------------------------------------------------------------------------------------
    //! 无 RSS 体力组件时视为 1.0，避免无模组实体卡死 CompleteWaypoint（若需严格判定可改为 0）。
    protected static float GetEntityStaminaPercent01(IEntity ent)
    {
        if (!ent)
            return 1.0;
        SCR_CharacterStaminaComponent stm = SCR_CharacterStaminaComponent.Cast(ent.FindComponent(SCR_CharacterStaminaComponent));
        if (!stm)
            return 1.0;
        return Math.Clamp(stm.GetTargetStamina(), 0.0, 1.0);
    }

    //------------------------------------------------------------------------------------------------
    protected static bool GroupAllAgentsStaminaAtOrAbove(SCR_AIGroup scrGrp, float min01)
    {
        if (!scrGrp)
            return false;
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        if (ac < 1)
            return false;
        for (int i = 0; i < ac; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                return false;
            if (!SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(ce))
                continue;
            float stm = GetEntityStaminaPercent01(ce);
            if (stm < min01)
                return false;
        }
        return true;
    }

    //------------------------------------------------------------------------------------------------
    protected static bool IsWaypointStillInGroup(SCR_AIGroup scrGrp, AIWaypoint wp)
    {
        if (!scrGrp || !wp)
            return false;
        array<AIWaypoint> list = {};
        int n = scrGrp.GetWaypoints(list);
        for (int i = 0; i < n; i++)
        {
            if (list.Get(i) == wp)
                return true;
        }
        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! 若该群组存在 RSS 插入的防守路点，且全员体力不低于恢复线，则 CompleteWaypoint，便于队列继续后续路点。
    static void TryCompleteGroupRestDefendWaypointIfReady(IEntity owner)
    {
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;
        if (!SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(owner))
            return;

        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return;
        AIGroup grpAny = agent.GetParentGroup();
        if (!grpAny)
            return;
        SCR_AIGroup scrGrp = SCR_AIGroup.Cast(grpAny);
        if (!scrGrp)
            return;

        int gid = scrGrp.GetGroupID();
        EnsureRssDefendWaypointMap();
        if (!s_mGroupRssDefendWaypointByGid.Contains(gid))
            return;

        AIWaypoint wp = s_mGroupRssDefendWaypointByGid.Get(gid);
        if (!wp)
        {
            s_mGroupRssDefendWaypointByGid.Remove(gid);
            return;
        }

        if (!IsWaypointStillInGroup(scrGrp, wp))
        {
            s_mGroupRssDefendWaypointByGid.Remove(gid);
            return;
        }

        float minSt = StaminaConstants.RSS_AI_REST_RECOVERY_RESUME_STAMINA_MIN;
        if (!GroupAllAgentsStaminaAtOrAbove(scrGrp, minSt))
            return;

        scrGrp.CompleteWaypoint(wp);
        s_mGroupRssDefendWaypointByGid.Remove(gid);
    }

    //------------------------------------------------------------------------------------------------
    //! 队长实体：GetLeaderEntity / GetLeaderAgent；无或缺 pathfinding 时回退触发者与群内首个可用步兵。
    protected static IEntity ResolveLeaderEntityForCoverQuery(SCR_AIGroup scrGrp, IEntity triggeringMember)
    {
        if (!scrGrp)
            return null;

        IEntity le = scrGrp.GetLeaderEntity();
        if (!le)
        {
            AIAgent lag = scrGrp.GetLeaderAgent();
            if (lag)
                le = lag.GetControlledEntity();
        }
        if (!le)
            le = triggeringMember;

        if (le && !SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(le))
            le = null;

        if (le)
        {
            AIPathfindingComponent pf = AIPathfindingComponent.Cast(le.FindComponent(AIPathfindingComponent));
            if (pf)
                return le;
        }

        if (triggeringMember && SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(triggeringMember))
        {
            AIPathfindingComponent pfT = AIPathfindingComponent.Cast(triggeringMember.FindComponent(AIPathfindingComponent));
            if (pfT)
                return triggeringMember;
        }

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
            if (!SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(ce))
                continue;
            AIPathfindingComponent pf = AIPathfindingComponent.Cast(ce.FindComponent(AIPathfindingComponent));
            if (pf)
                return ce;
        }

        return le;
    }

    //------------------------------------------------------------------------------------------------
    //! 自 centerPos 水平 16 向寻最近障碍，返回障碍物「内侧」近似蔽护点；无命中则返回 centerPos。
    protected static vector FindApproxCoverWorldPos(IEntity excludeRoot, AIGroup group, vector centerPos)
    {
        World world;
        if (excludeRoot)
            world = excludeRoot.GetWorld();
        if (!world)
            return centerPos;

        if (!s_pCoverTrace)
            s_pCoverTrace = new TraceParam();

        array<IEntity> excludeArr = {};
        if (group)
        {
            array<AIAgent> agents = {};
            int ac = group.GetAgents(agents);
            for (int ai = 0; ai < ac; ai++)
            {
                AIAgent ag = agents.Get(ai);
                if (!ag)
                    continue;
                IEntity ce = ag.GetControlledEntity();
                if (ce)
                    excludeArr.Insert(ce);
            }
        }
        if (excludeRoot)
            excludeArr.Insert(excludeRoot);

        float bestDistSq = 999999.0 * 999999.0;
        vector bestPos = centerPos;
        bool anyHit = false;

        const int SAMPLES = 16;
        const float HEAD_Z = 1.55;
        const float RAY_LEN = StaminaConstants.RSS_AI_GROUP_REST_COVER_TRACE_RADIUS_M;
        const float BACK_OFF = StaminaConstants.RSS_AI_GROUP_REST_COVER_OFFSET_FROM_WALL_M;
        const float DEG2RAD = 3.14159265 / 180.0;

        for (int i = 0; i < SAMPLES; i++)
        {
            float angleDeg = (360.0 / SAMPLES) * i;
            float rad = angleDeg * DEG2RAD;
            vector dir = Vector(Math.Cos(rad), 0.0, Math.Sin(rad));

            vector start = centerPos;
            start[1] = start[1] + HEAD_Z;
            vector end = start + dir * RAY_LEN;

            s_pCoverTrace.Start = start;
            s_pCoverTrace.End = end;
            s_pCoverTrace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
            s_pCoverTrace.LayerMask = EPhysicsLayerPresets.Projectile;
            s_pCoverTrace.Exclude = null;
            s_pCoverTrace.ExcludeArray = excludeArr;

            world.TraceMove(s_pCoverTrace, null);

            bool hit = false;
            if (s_pCoverTrace.TraceEnt)
            {
                if (!ChimeraCharacter.Cast(s_pCoverTrace.TraceEnt))
                    hit = true;
            }
            else
            {
                if (s_pCoverTrace.SurfaceProps)
                    hit = true;
                else
                {
                    if (s_pCoverTrace.ColliderName != string.Empty)
                        hit = true;
                }
            }

            if (!hit)
                continue;

            anyHit = true;

            vector seg = s_pCoverTrace.End - s_pCoverTrace.Start;
            float segLen = seg.Length();
            vector hitPos = s_pCoverTrace.Start;
            if (segLen > 0.001)
            {
                vector dirSeg = seg;
                dirSeg = dirSeg / segLen;
                hitPos = s_pCoverTrace.Start + dirSeg * s_pCoverTrace.TraceDist;
            }

            vector inward = -dir;
            vector candidate = hitPos + inward * BACK_OFF;
            candidate = SnapToGround(world, excludeRoot, candidate);

            float dx = candidate[0] - centerPos[0];
            float dz = candidate[2] - centerPos[2];
            float dSq = dx * dx + dz * dz;
            if (dSq < bestDistSq)
            {
                bestDistSq = dSq;
                bestPos = candidate;
            }
        }

        if (!anyHit)
            bestPos = SnapToGround(world, excludeRoot, centerPos);

        return bestPos;
    }

    //------------------------------------------------------------------------------------------------
    protected static vector SnapToGround(World world, IEntity excludeRoot, vector pos)
    {
        if (!world)
            return pos;

        if (!s_pGroundTrace)
            s_pGroundTrace = new TraceParam();

        vector downStart = pos;
        downStart[1] = downStart[1] + 4.0;
        vector downEnd = pos;
        downEnd[1] = downEnd[1] - 60.0;

        s_pGroundTrace.Start = downStart;
        s_pGroundTrace.End = downEnd;
        s_pGroundTrace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        s_pGroundTrace.LayerMask = EPhysicsLayerPresets.Projectile;
        s_pGroundTrace.Exclude = excludeRoot;
        s_pGroundTrace.ExcludeArray = null;

        world.TraceMove(s_pGroundTrace, null);

        vector seg = s_pGroundTrace.End - s_pGroundTrace.Start;
        float segLen = seg.Length();
        if (segLen < 0.001)
            return pos;

        vector dirSeg = seg / segLen;
        vector ground = s_pGroundTrace.Start + dirSeg * s_pGroundTrace.TraceDist;
        return ground;
    }

    //------------------------------------------------------------------------------------------------
    //! 在服务器上：任一群员体力低于阈值时，由队长定隐蔽点并插入全队防守路点（受冷却与战场危险限制）。
    static void TryScheduleGroupRestFromStamina(IEntity owner, float staminaPercent01)
    {
        if (!StaminaConstants.RSS_AI_GROUP_REST_ENABLED)
            return;
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
        if (!ctrl)
            return;
        if (ctrl.IsPlayerControlled())
            return;

        if (staminaPercent01 >= StaminaConstants.RSS_AI_GROUP_REST_STAMINA_THRESHOLD)
            return;

        if (!SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(owner))
            return;

        if (SCR_RSS_AIStaminaBridge.ShouldBlockAutonomousRest(owner))
            return;

        ChimeraCharacter ch = ChimeraCharacter.Cast(owner);
        if (!ch)
            return;
        CompartmentAccessComponent compAccess = ch.GetCompartmentAccessComponent();
        if (compAccess)
        {
            if (compAccess.GetCompartment())
                return;
        }
        if (SwimmingStateManager.IsSwimming(ctrl))
            return;

        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return;

        AIGroup grpAny = agent.GetParentGroup();
        if (!grpAny)
            return;

        SCR_AIGroup scrGrp = SCR_AIGroup.Cast(grpAny);
        if (!scrGrp)
            return;

        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;
        int gid = scrGrp.GetGroupID();

        EnsureCooldownMap();
        float nextOk = 0.0;
        if (s_mGroupRestNextAllowedWorldTimeS.Contains(gid))
            nextOk = s_mGroupRestNextAllowedWorldTimeS.Get(gid);

        if (nowS < nextOk)
            return;

        ResourceName prefab = StaminaConstants.RSS_AI_GROUP_REST_DEFEND_WAYPOINT_PREFAB;
        if (prefab == ResourceName.Empty)
            return;

        vector center = scrGrp.GetCenterOfMass();
        IEntity leaderForCover = ResolveLeaderEntityForCoverQuery(scrGrp, owner);

        vector restPos = center;
        if (StaminaConstants.RSS_AI_GROUP_REST_USE_ENGINE_COVER)
        {
            vector coverFromEngine;
            if (leaderForCover && SCR_RSS_AICoverSeeker.TryGetBestCoverWorldPositionForGroup(leaderForCover, center, coverFromEngine))
            {
                restPos = coverFromEngine;
            }
            else
            {
                IEntity approxRoot = leaderForCover;
                if (!approxRoot)
                    approxRoot = owner;
                restPos = FindApproxCoverWorldPos(approxRoot, scrGrp, center);
            }
        }
        else
        {
            IEntity approxRoot = leaderForCover;
            if (!approxRoot)
                approxRoot = owner;
            restPos = FindApproxCoverWorldPos(approxRoot, scrGrp, center);
        }

        ref array<ref SCR_WaypointPrefabLocation> prefabs = new array<ref SCR_WaypointPrefabLocation>();
        SCR_WaypointPrefabLocation loc = new SCR_WaypointPrefabLocation();
        loc.m_WPPrefabName = prefab;
        loc.m_WPInstanceName = "RSS_LowStaminaRest";
        loc.m_WPWorldLocation = restPos;
        loc.m_WPRadiusOverride = 12.0;
        loc.m_WPTimeOverride = StaminaConstants.RSS_AI_GROUP_REST_DEFEND_HOLD_SEC;
        prefabs.Insert(loc);

        array<AIWaypoint> wpsBefore = {};
        int nWaypointsBefore = scrGrp.GetWaypoints(wpsBefore);

        array<IEntity> spawnedWpEntities = {};
        scrGrp.AddWaypointsDynamic(spawnedWpEntities, prefabs);

        array<AIWaypoint> wpsAfter = {};
        int nWaypointsAfter = scrGrp.GetWaypoints(wpsAfter);
        if (nWaypointsAfter > nWaypointsBefore)
        {
            EnsureRssDefendWaypointMap();
            AIWaypoint rssWp = wpsAfter.Get(nWaypointsAfter - 1);
            s_mGroupRssDefendWaypointByGid.Set(gid, rssWp);
        }

        SCR_RSS_AIRestRecoveryRegistry.MarkRestRecoveryForGroup(scrGrp);

        float cooldown = StaminaConstants.RSS_AI_GROUP_REST_COOLDOWN_SEC;
        s_mGroupRestNextAllowedWorldTimeS.Set(gid, nowS + cooldown);
    }
}
