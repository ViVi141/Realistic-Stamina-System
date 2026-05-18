//! RSS AI Group Sync — 群组协同
//!
//! 路点预扫描：在路点配属时12m半径搜索遮蔽位置，缓存映射。

enum ERSS_GroupStaminaState
{
    GROUP_FIT,       // 中位数 ≥ 70%
    GROUP_TIRING,    // 中位数 40%~70%
    GROUP_FATIGUED,  // 中位数 20%~40%
    GROUP_SPENT      // 中位数 < 20%
}

//!
//! 路点预扫描：在路点配属时12m半径搜索遮蔽位置，缓存映射。
//! 自适应群组步速：每段路点完成时计算最慢成员步速，设队长限速。
//! 休息路点插入：路点完成时判定体力中位数，按需插入 WP_Wait/WP_Defend。
//!
//! 事件驱动（非 per-frame tick）：
//!   - Event_OnWaypointAdded   → 预扫描 + 缓存
//!   - Event_OnWaypointCompleted → 步速计算 + 休息判定 + 插入
//!   - Event_OnWaypointRemoved → 清理缓存

class SCR_RSS_AIGroupSync
{
    // ---- 路点 → 最佳休息位置缓存 ----
    protected static ref map<AIWaypoint, vector> s_mWaypointRestSpots;

    // ---- 群组休息冷却 ----
    protected static ref map<int, float> s_mGroupRestNextAllowedTimeS;

    // ---- 本模组插入的 RSS 休息路点（按群组ID） ----
    protected static ref map<int, AIWaypoint> s_mGroupRssRestWaypointByGid;

    // ---- 耗时记录：上一个路点完成的时间戳（按群组ID） ----
    protected static ref map<int, float> s_mGroupLastWaypointCompletedTimeS;

    //! 已注册路点事件的群组（ScriptInvoker 回调只传 AIWaypoint，需反查所属群组）
    protected static ref map<int, SCR_AIGroup> s_mRegisteredGroupsByGid;

    //! 路点 → 所属群组（在 OnWaypointAdded 时填充，OnWaypointRemoved 时清除）
    protected static ref map<AIWaypoint, SCR_AIGroup> s_mWaypointOwnerGroup;

    // ---- 射线追踪缓存 ----
    protected static ref TraceParam s_pCoverTrace;
    protected static ref TraceParam s_pGroundTrace;


    // ==========================================================================
    //  初始化
    // ==========================================================================

    protected static void EnsureRestSpotMap()
    {
        if (!s_mWaypointRestSpots)
            s_mWaypointRestSpots = new map<AIWaypoint, vector>();
    }

    protected static void EnsureCooldownMap()
    {
        if (!s_mGroupRestNextAllowedTimeS)
            s_mGroupRestNextAllowedTimeS = new map<int, float>();
    }

    protected static void EnsureRssWaypointMap()
    {
        if (!s_mGroupRssRestWaypointByGid)
            s_mGroupRssRestWaypointByGid = new map<int, AIWaypoint>();
    }

    protected static void EnsureTimestampMap()
    {
        if (!s_mGroupLastWaypointCompletedTimeS)
            s_mGroupLastWaypointCompletedTimeS = new map<int, float>();
    }

    protected static void EnsureRegisteredGroupsMap()
    {
        if (!s_mRegisteredGroupsByGid)
            s_mRegisteredGroupsByGid = new map<int, SCR_AIGroup>();
    }

    protected static void EnsureWaypointOwnerMap()
    {
        if (!s_mWaypointOwnerGroup)
            s_mWaypointOwnerGroup = new map<AIWaypoint, SCR_AIGroup>();
    }

    //! SCR_AIGroup 的 ScriptInvoker 仅 Invoke(AIWaypoint)，与 AIGroup.OnWaypointAdded 签名一致。
    protected static SCR_AIGroup FindGroupOwningWaypoint(AIWaypoint wp)
    {
        if (!wp)
            return null;

        EnsureWaypointOwnerMap();
        if (s_mWaypointOwnerGroup.Contains(wp))
            return s_mWaypointOwnerGroup.Get(wp);

        EnsureRegisteredGroupsMap();
        array<AIWaypoint> waypoints = {};
        foreach (int gid, SCR_AIGroup grp : s_mRegisteredGroupsByGid)
        {
            if (!grp)
                continue;
            waypoints.Clear();
            grp.GetWaypoints(waypoints);
            foreach (AIWaypoint listed : waypoints)
            {
                if (listed == wp)
                {
                    s_mWaypointOwnerGroup.Set(wp, grp);
                    return grp;
                }
            }
        }
        return null;
    }


    // ==========================================================================
    //  事件注册 / 注销
    // ==========================================================================

    //! 在群组初始化后调用一次（服务器端）。注册三个路点事件监听。
    static void RegisterForGroup(SCR_AIGroup group)
    {
        if (!group)
            return;
        if (!Replication.IsServer())
            return;

        EnsureRegisteredGroupsMap();
        s_mRegisteredGroupsByGid.Set(group.GetGroupID(), group);

        group.GetOnWaypointAdded().Insert(OnWaypointAdded);
        group.GetOnWaypointCompleted().Insert(OnWaypointCompleted);
        group.GetOnWaypointRemoved().Insert(OnWaypointRemoved);
    }

    //! 群组删除/世界重载时注销
    static void UnregisterForGroup(SCR_AIGroup group)
    {
        if (!group)
            return;

        group.GetOnWaypointAdded().Remove(OnWaypointAdded);
        group.GetOnWaypointCompleted().Remove(OnWaypointCompleted);
        group.GetOnWaypointRemoved().Remove(OnWaypointRemoved);

        EnsureRegisteredGroupsMap();
        int gid = group.GetGroupID();
        s_mRegisteredGroupsByGid.Remove(gid);
        SCR_RSS_AIGroupLocomotionPolicy.InvalidateGroup(gid);
    }


    // ==========================================================================
    //  事件处理
    // ==========================================================================

    //------------------------------------------------------------------------------------------------
    //! 路点被添加到群组 → 预扫描附近的遮蔽位置（签名须与 GetOnWaypointAdded().Invoke 一致）
    protected static void OnWaypointAdded(AIWaypoint wp)
    {
        if (!wp)
            return;
        if (!StaminaConfigBridge.IsAIStaminaIntegrationEnabled())
            return;
        if (!Replication.IsServer())
            return;

        SCR_AIGroup scrGrp = FindGroupOwningWaypoint(wp);
        if (scrGrp)
        {
            EnsureWaypointOwnerMap();
            s_mWaypointOwnerGroup.Set(wp, scrGrp);
        }

        vector origin = wp.GetOrigin();
        if (origin == vector.Zero)
            return;

        vector restSpot = ScanNearWaypoint(origin);
        if (restSpot == vector.Zero)
            restSpot = origin;

        EnsureRestSpotMap();
        s_mWaypointRestSpots.Set(wp, restSpot);
    }

    //------------------------------------------------------------------------------------------------
    //! 路点被清除 → 清理缓存（签名须与 GetOnWaypointRemoved().Invoke 一致）
    protected static void OnWaypointRemoved(AIWaypoint wp)
    {
        if (!wp)
            return;
        EnsureRestSpotMap();
        s_mWaypointRestSpots.Remove(wp);
        EnsureWaypointOwnerMap();
        s_mWaypointOwnerGroup.Remove(wp);
    }

    //------------------------------------------------------------------------------------------------
    //! 路点完成 → 步速计算 + 休息判定 + 插入休息路点（签名须与 GetOnWaypointCompleted().Invoke 一致）
    protected static void OnWaypointCompleted(AIWaypoint wp)
    {
        if (!wp)
            return;
        if (!Replication.IsServer())
            return;
        if (!StaminaConfigBridge.IsAIStaminaIntegrationEnabled())
            return;

        SCR_AIGroup scrGrp = FindGroupOwningWaypoint(wp);
        if (!scrGrp)
            return;

        int gid = scrGrp.GetGroupID();

        // 记录完成时间
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;
        EnsureTimestampMap();
        float prevTime = 0.0;
        if (s_mGroupLastWaypointCompletedTimeS.Contains(gid))
            prevTime = s_mGroupLastWaypointCompletedTimeS.Get(gid);
        s_mGroupLastWaypointCompletedTimeS.Set(gid, nowS);

        // ---------- 1. 群组状态检查 ----------
        if (!IsGroupReadyForRest(scrGrp))
            return;

        // ---------- 2. 体力判定 ----------
        float groupMedianStamina = GetGroupMedianStamina(scrGrp);
        ERSS_GroupStaminaState groupState = ClassifyGroupStamina(groupMedianStamina);

        // ---------- 3. 自适应群组步速（路点完成时强制刷新缓存） ----------
        if (StaminaConfigBridge.IsAIAdaptivePaceEnabled())
        {
            float paceMul = ApplyAdaptiveGroupPace(scrGrp, wp, prevTime, nowS);
            // 调试日志
            if (SCR_DebugBatchManager.IsDebugBatchActive())
            {
                string dbgPace = string.Format("[RSS] GroupSync: 自适应步速 id=%1 paceMul=%2 中位数体力=%3%",
                    gid.ToString(),
                    Math.Round(paceMul * 100.0) / 100.0,
                    Math.Round(groupMedianStamina * 100.0).ToString());
                SCR_DebugBatchManager.AddDebugBatchLine(dbgPace);
            }
        }

        // ---------- 4. 休息判定 ----------
        AIWaypoint nextWp = ResolveNextWaypointAfter(scrGrp, wp);

        switch (groupState)
        {
        case ERSS_GroupStaminaState.GROUP_FIT:
            return;

        case ERSS_GroupStaminaState.GROUP_TIRING:
            if (nextWp)
            {
                float distM = vector.Distance(wp.GetOrigin(), nextWp.GetOrigin());
                if (distM > StaminaConstants.RSS_AI_GROUP_SYNC_TIRING_INSERT_DIST_M)
                    InsertRestWaypoint(scrGrp, nextWp, distM, 30.0);
            }
            return;

        case ERSS_GroupStaminaState.GROUP_FATIGUED:
            if (nextWp)
                InsertRestWaypoint(scrGrp, nextWp, 0.0, 60.0);
            else
                InsertRestWaypointAtCurrentPos(scrGrp, 60.0);
            return;

        case ERSS_GroupStaminaState.GROUP_SPENT:
            InsertRestWaypointAtCurrentPos(scrGrp, 120.0); // 120s 超时，避免无限卡死
            return;
        }
    }


    //------------------------------------------------------------------------------------------------
    //! 路点完成后取「队列中紧随 completedWp 的下一个」；避免 GetCurrentWaypoint 仍指向已完成路点。
    protected static AIWaypoint ResolveNextWaypointAfter(SCR_AIGroup scrGrp, AIWaypoint completedWp)
    {
        if (!scrGrp || !completedWp)
            return null;

        array<AIWaypoint> wps = {};
        int n = scrGrp.GetWaypoints(wps);
        for (int i = 0; i < n; i++)
        {
            if (wps.Get(i) == completedWp)
            {
                if (i + 1 < n)
                    return wps.Get(i + 1);
                return null;
            }
        }

        AIWaypoint current = scrGrp.GetCurrentWaypoint();
        if (current && current != completedWp)
            return current;
        return null;
    }

    // ==========================================================================
    //  预扫描算法
    // ==========================================================================

    //------------------------------------------------------------------------------------------------
    //! 以路点为中心，12方向水平射线搜索墙壁/岩石/树，取最佳遮蔽候选。
    //! 12m 半径，0.8m 后退，> 2m 落差跳过。
    protected static vector ScanNearWaypoint(vector centerPos)
    {
        World world = GetGame().GetWorld();
        if (!world)
            return vector.Zero;

        if (!s_pCoverTrace)
            s_pCoverTrace = new TraceParam();
        if (!s_pGroundTrace)
            s_pGroundTrace = new TraceParam();

        const int SAMPLES = 12;
        const float HEAD_Z = 1.55;
        const float RAY_LEN = 12.0;
        const float BACK_OFF = 0.8;
        const float DEG2RAD = 3.14159265 / 180.0;
        const float MAX_HORIZ_DIST = 10.0;
        const float MAX_DROP = 2.0;

        float bestScore = -1.0;
        vector bestPos = vector.Zero;

        for (int i = 0; i < SAMPLES; i++)
        {
            float angleDeg = (360.0 / SAMPLES) * i;
            float rad = angleDeg * DEG2RAD;
            vector dir = Vector(Math.Cos(rad), 0.0, Math.Sin(rad));
            // EnforceScript 无 normalized 构造，显式归一
            float dirLen = Math.Sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
            if (dirLen > 0.001)
            {
                dir[0] = dir[0] / dirLen;
                dir[1] = dir[1] / dirLen;
                dir[2] = dir[2] / dirLen;
            }

            vector start = centerPos;
            start[1] = start[1] + HEAD_Z;
            vector end = start + dir * RAY_LEN;

            s_pCoverTrace.Start = start;
            s_pCoverTrace.End = end;
            s_pCoverTrace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
            s_pCoverTrace.LayerMask = EPhysicsLayerPresets.Projectile;
            s_pCoverTrace.Exclude = null;
            s_pCoverTrace.ExcludeArray = null;

            world.TraceMove(s_pCoverTrace, null);

            bool hit = false;
            if (s_pCoverTrace.TraceEnt)
            {
                if (!ChimeraCharacter.Cast(s_pCoverTrace.TraceEnt))
                    hit = true;
            }
            else if (s_pCoverTrace.SurfaceProps)
            {
                hit = true;
            }
            else if (s_pCoverTrace.ColliderName != string.Empty)
            {
                hit = true;
            }

            if (!hit)
                continue;

            // 击中的世界位置
            vector seg = s_pCoverTrace.End - s_pCoverTrace.Start;
            float segLen = Math.Sqrt(seg[0]*seg[0] + seg[1]*seg[1] + seg[2]*seg[2]);
            vector hitPos = s_pCoverTrace.Start;
            if (segLen > 0.001)
            {
                vector dirSeg = seg;
                dirSeg[0] = dirSeg[0] / segLen;
                dirSeg[1] = dirSeg[1] / segLen;
                dirSeg[2] = dirSeg[2] / segLen;
                hitPos = s_pCoverTrace.Start + dirSeg * s_pCoverTrace.TraceDist;
            }

            // 法线方向后退
            vector inward = dir;
            inward[0] = -inward[0];
            inward[1] = -inward[1];
            inward[2] = -inward[2];
            vector candidate = hitPos + inward * BACK_OFF;

            // 贴地
            candidate = SnapToGround(world, candidate);
            float drop = centerPos[1] - candidate[1];
            if (drop > MAX_DROP || drop < -MAX_DROP)
                continue;

            // 水平距离
            float dx = candidate[0] - centerPos[0];
            float dy = candidate[1] - centerPos[1];
            float dz = candidate[2] - centerPos[2];
            float hDist = Math.Sqrt(dx*dx + dz*dz);
            if (hDist > MAX_HORIZ_DIST)
                continue;

            // 评分：障碍物距离（越近越好，3~6m 最佳）+ 候选点距离（越近越好）
            float hitDist = Math.Sqrt(
                (hitPos[0]-centerPos[0])*(hitPos[0]-centerPos[0]) +
                (hitPos[2]-centerPos[2])*(hitPos[2]-centerPos[2]));
            float diffDist = hitDist - 4.5;
            if (diffDist < 0.0)
                diffDist = -diffDist;
            float obstacleScore = 1.0 - Math.Clamp(diffDist / 8.0, 0.0, 1.0);
            float distScore = 1.0 - Math.Clamp(hDist / MAX_HORIZ_DIST, 0.0, 1.0);
            float score = obstacleScore * 0.7 + distScore * 0.3;

            if (score > bestScore)
            {
                bestScore = score;
                bestPos = candidate;
            }
        }

        if (bestScore < 0.0)
            return vector.Zero;
        return bestPos;
    }

    //------------------------------------------------------------------------------------------------
    protected static vector SnapToGround(World world, vector pos)
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

        world.TraceMove(s_pGroundTrace, null);

        vector seg = s_pGroundTrace.End - s_pGroundTrace.Start;
        float segLen = Math.Sqrt(seg[0]*seg[0] + seg[1]*seg[1] + seg[2]*seg[2]);
        if (segLen < 0.001)
            return pos;

        vector dirSeg = seg;
        dirSeg[0] = dirSeg[0] / segLen;
        dirSeg[1] = dirSeg[1] / segLen;
        dirSeg[2] = dirSeg[2] / segLen;

        return s_pGroundTrace.Start + dirSeg * s_pGroundTrace.TraceDist;
    }


    //------------------------------------------------------------------------------------------------
    //! 路点完成时刷新群组机动策略（步速/最差状态），并写入队长 OverrideMaxSpeed
    protected static float ApplyAdaptiveGroupPace(
        SCR_AIGroup scrGrp, AIWaypoint completedWp,
        float prevTimeS, float nowS)
    {
        if (!scrGrp)
            return 1.0;

        SCR_RSS_AIGroupLocomotionPolicy.ForceRefreshGroup(scrGrp);
        float paceMul = SCR_RSS_AIGroupLocomotionPolicy.GetGroupPaceMultiplier(scrGrp);

        IEntity leader = scrGrp.GetLeaderEntity();
        if (leader)
        {
            SCR_CharacterControllerComponent leaderCtrl = SCR_CharacterControllerComponent.Cast(
                leader.FindComponent(SCR_CharacterControllerComponent));
            if (leaderCtrl)
                SCR_RSS_AISpeedCap.SetLeaderPace(leaderCtrl, paceMul);
        }
        return paceMul;
    }


    // ==========================================================================
    //  休息路点插入
    // ==========================================================================

    //------------------------------------------------------------------------------------------------
    //! 在 nextWp 之前插入休息路点，优先用缓存遮蔽位置。
    protected static void InsertRestWaypoint(SCR_AIGroup scrGrp, AIWaypoint nextWp,
        float distToNextM, float holdSec)
    {
        if (!scrGrp)
            return;
        int gid = scrGrp.GetGroupID();
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;

        EnsureCooldownMap();
        float nextOk = 0.0;
        if (s_mGroupRestNextAllowedTimeS.Contains(gid))
            nextOk = s_mGroupRestNextAllowedTimeS.Get(gid);
        if (nowS < nextOk)
            return;

        // 使用缓存位置，否则用路点本身位置
        vector restPos = nextWp.GetOrigin();
        EnsureRestSpotMap();
        if (s_mWaypointRestSpots.Contains(nextWp))
        {
            vector cached = s_mWaypointRestSpots.Get(nextWp);
            if (cached != vector.Zero)
                restPos = cached;
        }

        ResourceName prefab = GetRestWaypointPrefab(holdSec);
        if (prefab == ResourceName.Empty)
            return;

        ref array<ref SCR_WaypointPrefabLocation> prefabs = new array<ref SCR_WaypointPrefabLocation>();
        SCR_WaypointPrefabLocation loc = new SCR_WaypointPrefabLocation();
        loc.m_WPPrefabName = prefab;
        loc.m_WPInstanceName = "RSS_AIRest";
        loc.m_WPWorldLocation = restPos;
        loc.m_WPRadiusOverride = 8.0;
        if (holdSec > 0.0)
            loc.m_WPTimeOverride = holdSec;
        prefabs.Insert(loc);

        array<IEntity> spawnedWpEntities = {};
        scrGrp.AddWaypointsDynamic(spawnedWpEntities, prefabs);

        array<AIWaypoint> wpsAfter = {};
        int nAfter = scrGrp.GetWaypoints(wpsAfter);
        if (nAfter > 0)
        {
            EnsureRssWaypointMap();
            s_mGroupRssRestWaypointByGid.Set(gid, wpsAfter.Get(nAfter - 1));
        }

        float cooldown = StaminaConstants.RSS_AI_GROUP_SYNC_COOLDOWN_SEC;
        s_mGroupRestNextAllowedTimeS.Set(gid, nowS + cooldown);

        // 调试日志
        if (SCR_DebugBatchManager.IsDebugBatchActive())
        {
            string typeStr;
            if (prefab == StaminaConstants.RSS_AI_GROUP_SYNC_DEFEND_WAYPOINT_PREFAB)
                typeStr = "Defend";
            else
                typeStr = "Wait";
            string dbgRest = string.Format("[RSS] GroupSync: 插入休息路点 id=%1 位置=(%2,%3,%4) 类型=%5 超时=%6s",
                gid.ToString(),
                Math.Round(restPos[0] * 10.0) / 10.0,
                Math.Round(restPos[1] * 10.0) / 10.0,
                Math.Round(restPos[2] * 10.0) / 10.0,
                typeStr,
                Math.Round(holdSec * 10.0) / 10.0);
            SCR_DebugBatchManager.AddDebugBatchLine(dbgRest);
        }
    }

    //------------------------------------------------------------------------------------------------
    //! 在当前群组位置附近插入休息路点（当没有下一个路点时）。
    protected static void InsertRestWaypointAtCurrentPos(SCR_AIGroup scrGrp, float holdSec)
    {
        if (!scrGrp)
            return;
        int gid = scrGrp.GetGroupID();
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;

        EnsureCooldownMap();
        float nextOk = 0.0;
        if (s_mGroupRestNextAllowedTimeS.Contains(gid))
            nextOk = s_mGroupRestNextAllowedTimeS.Get(gid);
        if (nowS < nextOk)
            return;

        vector centerPos = scrGrp.GetCenterOfMass();
        vector restPos = ScanNearWaypoint(centerPos);
        if (restPos == vector.Zero)
            restPos = centerPos;

        ResourceName prefab = GetRestWaypointPrefab(holdSec);
        if (prefab == ResourceName.Empty)
            return;

        ref array<ref SCR_WaypointPrefabLocation> prefabs = new array<ref SCR_WaypointPrefabLocation>();
        SCR_WaypointPrefabLocation loc = new SCR_WaypointPrefabLocation();
        loc.m_WPPrefabName = prefab;
        loc.m_WPInstanceName = "RSS_AIRest";
        loc.m_WPWorldLocation = restPos;
        loc.m_WPRadiusOverride = 8.0;
        if (holdSec > 0.0)
            loc.m_WPTimeOverride = holdSec;
        prefabs.Insert(loc);

        array<IEntity> spawnedWpEntities = {};
        scrGrp.AddWaypointsDynamic(spawnedWpEntities, prefabs);

        float cooldown = StaminaConstants.RSS_AI_GROUP_SYNC_COOLDOWN_SEC;
        s_mGroupRestNextAllowedTimeS.Set(gid, nowS + cooldown);
    }

    //------------------------------------------------------------------------------------------------
    //! 根据休息时长选路点预置体
    protected static ResourceName GetRestWaypointPrefab(float holdSec)
    {
        if (holdSec < 0.0)
            return StaminaConstants.RSS_AI_GROUP_SYNC_DEFEND_WAYPOINT_PREFAB;
        return StaminaConstants.RSS_AI_GROUP_SYNC_WAIT_WAYPOINT_PREFAB;
    }


    // ==========================================================================
    //  群组辅助方法
    // ==========================================================================

    protected static void SortFloats(array<float> arr)
    {
        int n = arr.Count();
        for (int i = 0; i < n - 1; i++)
        {
            for (int j = 0; j < n - i - 1; j++)
            {
                if (arr.Get(j) > arr.Get(j + 1))
                {
                    float tmp = arr.Get(j);
                    arr.Set(j, arr.Get(j + 1));
                    arr.Set(j + 1, tmp);
                }
            }
        }
    }

    //------------------------------------------------------------------------------------------------
    static float GetGroupMedianStamina(SCR_AIGroup scrGrp)
    {
        array<float> values = {};
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
            if (!IsAliveAndActionable(ce))
                continue;

            float s = 1.0;
            SCR_CharacterStaminaComponent stm = SCR_CharacterStaminaComponent.Cast(
                ce.FindComponent(SCR_CharacterStaminaComponent));
            if (stm)
                s = Math.Clamp(stm.GetTargetStamina(), 0.0, 1.0);
            values.Insert(s);
        }

        int n = values.Count();
        if (n < 1)
            return 1.0;

        SortFloats(values);
        int mid = n / 2;
        if (n % 2 == 0 && mid > 0)
            return (values.Get(mid - 1) + values.Get(mid)) * 0.5;
        return values.Get(mid);
    }

    //------------------------------------------------------------------------------------------------
    static ERSS_GroupStaminaState ClassifyGroupStamina(float medianStamina)
    {
        if (medianStamina >= StaminaConstants.RSS_AI_GROUP_SYNC_FIT_THRESHOLD)
            return ERSS_GroupStaminaState.GROUP_FIT;
        if (medianStamina >= StaminaConstants.RSS_AI_GROUP_SYNC_TIRING_THRESHOLD)
            return ERSS_GroupStaminaState.GROUP_TIRING;
        if (medianStamina >= StaminaConstants.RSS_AI_GROUP_SYNC_SPENT_THRESHOLD)
            return ERSS_GroupStaminaState.GROUP_FATIGUED;
        return ERSS_GroupStaminaState.GROUP_SPENT;
    }

    //------------------------------------------------------------------------------------------------
    static bool IsGroupReadyForRest(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return false;
        // 有成员存活
        array<AIAgent> agents = {};
        if (scrGrp.GetAgents(agents) < 1)
            return false;
        // 不被压制
        for (int i = 0; i < agents.Count(); i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            EAIThreatState threat = GetThreatState(ce);
            if (threat == EAIThreatState.THREATENED)
                return false;
        }
        return true;
    }

    //------------------------------------------------------------------------------------------------
    static EAIThreatState GetThreatState(IEntity owner)
    {
        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return EAIThreatState.SAFE;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return EAIThreatState.SAFE;
        SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(agent.FindComponent(SCR_AIInfoComponent));
        if (!info)
            return EAIThreatState.SAFE;
        if (!info.GetThreatSystem())
            return EAIThreatState.SAFE;
        return info.GetThreatSystem().GetState();
    }

    //------------------------------------------------------------------------------------------------
    static bool IsAliveAndActionable(IEntity ent)
    {
        if (!ent)
            return false;
        SCR_CharacterControllerComponent c = SCR_CharacterControllerComponent.Cast(
            ent.FindComponent(SCR_CharacterControllerComponent));
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
    //! 该实体所在的群组是否有 RSS 插入的休息路点
    static bool IsGroupRestWaypointActive(IEntity owner)
    {
        if (!owner)
            return false;
        SCR_AIGroup scrGrp = ResolveOwnerGroup(owner);
        if (!scrGrp)
            return false;

        int gid = scrGrp.GetGroupID();
        EnsureRssWaypointMap();
        if (!s_mGroupRssRestWaypointByGid.Contains(gid))
            return false;

        AIWaypoint wp = s_mGroupRssRestWaypointByGid.Get(gid);
        if (!wp)
        {
            s_mGroupRssRestWaypointByGid.Remove(gid);
            return false;
        }

        // 检查该路点是否仍在群组路点队列中
        array<AIWaypoint> list = {};
        int n = scrGrp.GetWaypoints(list);
        for (int i = 0; i < n; i++)
        {
            if (list.Get(i) == wp)
                return true;
        }
        // 已被完成或移除，清理缓存
        s_mGroupRssRestWaypointByGid.Remove(gid);
        return false;
    }

    //------------------------------------------------------------------------------------------------
    //! 休息路点仍在队列中且体力中位数已恢复达标 → 提前 Complete
    static void TryCompleteRestWaypointIfStaminaRecovered(IEntity owner, float staminaPercent01)
    {
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_AIGroup scrGrp = ResolveOwnerGroup(owner);
        if (!scrGrp)
            return;

        int gid = scrGrp.GetGroupID();
        EnsureRssWaypointMap();
        if (!s_mGroupRssRestWaypointByGid.Contains(gid))
            return;

        AIWaypoint wp = s_mGroupRssRestWaypointByGid.Get(gid);
        if (!wp)
        {
            s_mGroupRssRestWaypointByGid.Remove(gid);
            return;
        }

        // 检查路点是否仍在队列
        array<AIWaypoint> list = {};
        int n = scrGrp.GetWaypoints(list);
        bool found = false;
        for (int i = 0; i < n; i++)
        {
            if (list.Get(i) == wp)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            s_mGroupRssRestWaypointByGid.Remove(gid);
            return;
        }

        // 检查体力中位数是否达标
        float groupMed = GetGroupMedianStamina(scrGrp);
        if (groupMed >= StaminaConstants.RSS_AI_GROUP_SYNC_FIT_THRESHOLD)
        {
            scrGrp.CompleteWaypoint(wp);
            s_mGroupRssRestWaypointByGid.Remove(gid);

            if (SCR_DebugBatchManager.IsDebugBatchActive())
            {
                string dbg = string.Format("[RSS] GroupSync: 恢复达标 提前完成休息路点 id=%1 中位数体力=%2%",
                    gid.ToString(),
                    Math.Round(groupMed * 100.0).ToString());
                SCR_DebugBatchManager.AddDebugBatchLine(dbg);
            }
        }
    }

    //------------------------------------------------------------------------------------------------
    static SCR_AIGroup ResolveOwnerGroup(IEntity owner)
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

    static bool IsAliveAndActionablePublic(IEntity ent)
    {
        return IsAliveAndActionable(ent);
    }

    //------------------------------------------------------------------------------------------------
    //! 世界重载时清空所有缓存
    static void ClearAllForNewWorldSession()
    {
        if (s_mWaypointRestSpots)
            s_mWaypointRestSpots.Clear();
        if (s_mGroupRestNextAllowedTimeS)
            s_mGroupRestNextAllowedTimeS.Clear();
        if (s_mGroupRssRestWaypointByGid)
            s_mGroupRssRestWaypointByGid.Clear();
        if (s_mGroupLastWaypointCompletedTimeS)
            s_mGroupLastWaypointCompletedTimeS.Clear();
        if (s_mRegisteredGroupsByGid)
            s_mRegisteredGroupsByGid.Clear();
        if (s_mWaypointOwnerGroup)
            s_mWaypointOwnerGroup.Clear();
        SCR_RSS_AIGroupLocomotionPolicy.ClearAllForNewWorldSession();
    }
}
