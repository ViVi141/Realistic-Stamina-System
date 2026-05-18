//! RSS AI Group Locomotion Policy — 群组机动总体策略
//!
//! ## 总体思想（与 docs/RSS_AI体力集成全盘设计方案.md 一致）
//!
//! 1. **原生 AI 决策意图，RSS 只施加生理约束**（不替代行为树）。
//! 2. **个体生理**：每人仍用 Pandolf 管线消耗/恢复，保留个体 `ERSS_AIStaminaState`。
//! 3. **编队行军**（有群组、非 THREATENED、功能开启）：
//!    - **快的等慢的**：步速 = 全员负重/体力估算的第 25 百分位 → `groupPaceMul`。
//!    - **最弱决定全队**：限速/意图/战斗衰减用 **群组最差成员状态**，避免有人 Sprint 掉队。
//!    - **全员统一 OverrideMaxSpeed**（= groupPaceMul），不依赖引擎 KeepInFormation 单独跟队长。
//!    - **几何中心可控**：维护编队几何中心；限制成员距中心散布、中心与队长分离；
//!      离群成员与超前队长额外减速，使中心始终落在可预测范围内。
//! 4. **接敌 (THREATENED)**：立即退回 **个体状态 + 个体速度**，保命优先、不限速。
//!
//! GroupSync 负责路点/休息；本模块负责 **行军时怎么一起走**。

class RSS_GroupLocomotionPolicySnapshot
{
    bool marchingMode;
    ERSS_AIStaminaState groupLocomotionState;
    ERSS_AIStaminaState groupLocomotionStatePrev;
    ERSS_GroupStaminaState groupTier;
    float groupPaceMul;
    float medianStamina01;
    //! 存活成员位置平均（编队几何中心）
    vector groupCenter;
    int geometryMemberCount;
    //! 任一成员到几何中心的最大水平距离
    float maxSpreadM;
    //! 几何中心与队长的水平距离
    float centerLeaderSepM;
    //! 散布/中心偏移过大时全组额外减速 (<1)
    float cohesionGlobalMul;
}

class SCR_RSS_AIGroupLocomotionPolicy
{
    protected static ref map<int, ref RSS_GroupLocomotionPolicySnapshot> s_mPolicyByGid;
    protected static ref map<int, float> s_mLastRefreshTimeS;

    //------------------------------------------------------------------------------------------------
    static void ClearAllForNewWorldSession()
    {
        if (s_mPolicyByGid)
            s_mPolicyByGid.Clear();
        if (s_mLastRefreshTimeS)
            s_mLastRefreshTimeS.Clear();
    }

    //------------------------------------------------------------------------------------------------
    static void InvalidateGroup(int groupId)
    {
        if (!s_mPolicyByGid)
            return;
        s_mPolicyByGid.Remove(groupId);
        if (s_mLastRefreshTimeS)
            s_mLastRefreshTimeS.Remove(groupId);
    }

    //------------------------------------------------------------------------------------------------
    //! 路点完成等时机强制重算（供 GroupSync 调用）
    static void ForceRefreshGroup(SCR_AIGroup scrGrp)
    {
        RefreshPolicy(scrGrp, true);
    }

    //------------------------------------------------------------------------------------------------
    static float GetGroupPaceMultiplier(SCR_AIGroup scrGrp)
    {
        RSS_GroupLocomotionPolicySnapshot snap = GetOrRefreshPolicy(scrGrp, false);
        if (!snap)
            return 1.0;
        return snap.groupPaceMul;
    }

    //------------------------------------------------------------------------------------------------
    //! 是否由编队策略接管 OverrideMaxSpeed（SpeedCap 只改移动档位）
    static bool IsFormationSpeedAuthority(IEntity owner, bool isThreatened)
    {
        if (!owner)
            return false;
        if (isThreatened)
            return false;
        if (!Replication.IsServer())
            return false;
        if (!StaminaConfigBridge.IsAIAdaptivePaceEnabled())
            return false;

        SCR_AIGroup scrGrp = SCR_RSS_AIGroupSync.ResolveOwnerGroup(owner);
        if (!scrGrp)
            return false;

        RSS_GroupLocomotionPolicySnapshot snap = GetOrRefreshPolicy(scrGrp, false);
        if (!snap)
            return false;
        if (!snap.marchingMode)
            return false;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 输出行军用的体力状态（最差成员）及上一帧群组状态（意图过滤滞回用）
    //! @return true = 处于编队行军模式，调用方应使用输出的 locomotion 状态
    static bool ResolveLocomotionForMember(
        IEntity owner,
        bool isThreatened,
        ERSS_AIStaminaState individualState,
        out ERSS_AIStaminaState locomotionState,
        out ERSS_AIStaminaState prevLocomotionState)
    {
        locomotionState = individualState;
        prevLocomotionState = individualState;

        if (!owner)
            return false;
        if (!Replication.IsServer())
            return false;
        if (!StaminaConfigBridge.IsAIStaminaIntegrationEnabled())
            return false;
        if (isThreatened)
            return false;

        SCR_AIGroup scrGrp = SCR_RSS_AIGroupSync.ResolveOwnerGroup(owner);
        if (!scrGrp)
            return false;

        RSS_GroupLocomotionPolicySnapshot snap = GetOrRefreshPolicy(scrGrp, false);
        if (!snap || !snap.marchingMode)
            return false;

        locomotionState = snap.groupLocomotionState;
        prevLocomotionState = snap.groupLocomotionStatePrev;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------
    //! 每个体力 tick 向快照目标平滑收敛（避免个体速度与编队速度交替写入导致顿挫）
    //! @return true 表示 ioSpeedMul 为应使用的 OverrideMaxSpeed
    static bool TickSmoothedFormationSpeed(IEntity owner, bool isThreatened, float deltaSec, out float ioSpeedMul)
    {
        ioSpeedMul = 1.0;
        if (!owner)
            return false;
        if (!Replication.IsServer())
            return false;
        if (isThreatened)
            return false;
        if (!StaminaConfigBridge.IsAIAdaptivePaceEnabled())
            return false;

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            owner.FindComponent(SCR_CharacterControllerComponent));
        if (!ctrl)
            return false;

        SCR_AIGroup scrGrp = SCR_RSS_AIGroupSync.ResolveOwnerGroup(owner);
        if (!scrGrp)
        {
            ctrl.RSS_ResetFormationSpeedSmooth();
            return false;
        }

        RSS_GroupLocomotionPolicySnapshot snap = GetOrRefreshPolicy(scrGrp, false);
        if (!snap || !snap.marchingMode)
        {
            ctrl.RSS_ResetFormationSpeedSmooth();
            return false;
        }

        float target = ctrl.RSS_GetFormationSpeedTarget();
        float smoothed = ctrl.RSS_GetFormationSpeedSmoothed();
        if (smoothed < 0.0)
            smoothed = target;

        float tau = StaminaConstants.RSS_AI_GROUP_COHESION_SPEED_SMOOTH_TAU_SEC;
        float alpha = 1.0;
        if (tau > 0.001)
            alpha = Math.Clamp(deltaSec / tau, 0.0, 1.0);

        smoothed = smoothed + (target - smoothed) * alpha;
        ctrl.RSS_SetFormationSpeedSmoothed(smoothed);
        ioSpeedMul = Math.Clamp(smoothed, 0.01, 1.0);
        return true;
    }

    // ==========================================================================
    //  策略刷新
    // ==========================================================================

    protected static void EnsureMaps()
    {
        if (!s_mPolicyByGid)
            s_mPolicyByGid = new map<int, ref RSS_GroupLocomotionPolicySnapshot>();
        if (!s_mLastRefreshTimeS)
            s_mLastRefreshTimeS = new map<int, float>();
    }

    protected static RSS_GroupLocomotionPolicySnapshot GetOrRefreshPolicy(SCR_AIGroup scrGrp, bool force)
    {
        if (!scrGrp)
            return null;
        RefreshPolicy(scrGrp, force);
        int gid = scrGrp.GetGroupID();
        EnsureMaps();
        if (s_mPolicyByGid.Contains(gid))
            return s_mPolicyByGid.Get(gid);
        return null;
    }

    protected static void RefreshPolicy(SCR_AIGroup scrGrp, bool force)
    {
        if (!scrGrp)
            return;
        if (!StaminaConfigBridge.IsAIStaminaIntegrationEnabled())
            return;

        int gid = scrGrp.GetGroupID();
        float nowS = GetGame().GetWorld().GetWorldTime() / 1000.0;

        EnsureMaps();
        if (!force)
        {
            float lastS = 0.0;
            if (s_mLastRefreshTimeS.Contains(gid))
                lastS = s_mLastRefreshTimeS.Get(gid);
            if (lastS > 0.0 && (nowS - lastS) < StaminaConstants.RSS_AI_GROUP_SYNC_PACE_REFRESH_SEC)
                return;
        }

        RSS_GroupLocomotionPolicySnapshot snap = new RSS_GroupLocomotionPolicySnapshot();
        if (s_mPolicyByGid.Contains(gid))
        {
            RSS_GroupLocomotionPolicySnapshot old = s_mPolicyByGid.Get(gid);
            if (old)
                snap.groupLocomotionStatePrev = old.groupLocomotionState;
            else
                snap.groupLocomotionStatePrev = ERSS_AIStaminaState.FRESH;
        }
        else
        {
            snap.groupLocomotionStatePrev = ERSS_AIStaminaState.FRESH;
        }

        snap.marchingMode = IsGroupMarching(scrGrp);
        snap.medianStamina01 = ComputeMedianStamina(scrGrp);
        snap.groupTier = ClassifyGroupTier(snap.medianStamina01);
        snap.groupLocomotionState = CollectWorstMemberState(scrGrp);
        if (StaminaConfigBridge.IsAIAdaptivePaceEnabled())
            snap.groupPaceMul = ComputeGroupPaceMultiplier(scrGrp);
        else
            snap.groupPaceMul = 1.0;

        if (!snap.marchingMode)
            snap.groupPaceMul = 1.0;

        ComputeGroupGeometry(scrGrp, snap);
        snap.cohesionGlobalMul = 1.0;
        if (snap.marchingMode && snap.geometryMemberCount >= 2)
            snap.cohesionGlobalMul = ComputeCohesionGlobalMul(snap);

        if (snap.marchingMode && StaminaConfigBridge.IsAIAdaptivePaceEnabled())
            ApplyFormationTargetsToMembers(scrGrp, snap);

        s_mPolicyByGid.Set(gid, snap);
        s_mLastRefreshTimeS.Set(gid, nowS);
    }

    //------------------------------------------------------------------------------------------------
    //! 几何中心 + 最大散布 + 中心-队长距离
    protected static void ComputeGroupGeometry(SCR_AIGroup scrGrp, RSS_GroupLocomotionPolicySnapshot snap)
    {
        snap.groupCenter = vector.Zero;
        snap.geometryMemberCount = 0;
        snap.maxSpreadM = 0.0;
        snap.centerLeaderSepM = 0.0;

        if (!scrGrp || !snap)
            return;

        vector sum = vector.Zero;
        int count = 0;

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
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce))
                continue;
            sum = sum + ce.GetOrigin();
            count = count + 1;
        }

        snap.geometryMemberCount = count;
        if (count < 1)
            return;

        float inv = 1.0 / count;
        snap.groupCenter = Vector(sum[0] * inv, sum[1] * inv, sum[2] * inv);

        float maxSpread = 0.0;
        IEntity leader = scrGrp.GetLeaderEntity();
        for (int j = 0; j < ac; j++)
        {
            AIAgent ag2 = agents.Get(j);
            if (!ag2)
                continue;
            IEntity ce2 = ag2.GetControlledEntity();
            if (!ce2)
                continue;
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce2))
                continue;
            float d = HorizontalDistance(ce2.GetOrigin(), snap.groupCenter);
            if (d > maxSpread)
                maxSpread = d;
        }
        snap.maxSpreadM = maxSpread;

        if (leader)
            snap.centerLeaderSepM = HorizontalDistance(leader.GetOrigin(), snap.groupCenter);
    }

    //------------------------------------------------------------------------------------------------
    //! 策略刷新时一次性写入各成员目标倍率（位置与几何中心同一快照，避免实时抖动）
    protected static void ApplyFormationTargetsToMembers(SCR_AIGroup scrGrp, RSS_GroupLocomotionPolicySnapshot snap)
    {
        if (!scrGrp || !snap)
            return;

        float baseMul = snap.groupPaceMul * snap.cohesionGlobalMul;
        IEntity leader = scrGrp.GetLeaderEntity();

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
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce))
                continue;

            SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
                ce.FindComponent(SCR_CharacterControllerComponent));
            if (!ctrl)
                continue;

            float dist = HorizontalDistance(ce.GetOrigin(), snap.groupCenter);
            float spreadMul = ComputeMemberSpreadCohesionMulFromDistance(ctrl, dist);
            float target = baseMul * spreadMul;

            if (leader && ce == leader)
                target = target * ComputeLeaderCenterPullbackMulFromSnap(snap);

            target = Math.Clamp(target, 0.01, 1.0);
            ctrl.RSS_SetFormationSpeedTarget(target);
        }
    }

    //------------------------------------------------------------------------------------------------
    //! 散布过大：全组略减速，等待几何中心重新收拢
    protected static float ComputeCohesionGlobalMul(RSS_GroupLocomotionPolicySnapshot snap)
    {
        float mul = 1.0;
        float targetSpread = StaminaConstants.RSS_AI_GROUP_COHESION_TARGET_SPREAD_M;
        if (snap.maxSpreadM > targetSpread)
        {
            float spreadMul = targetSpread / snap.maxSpreadM;
            if (spreadMul < mul)
                mul = spreadMul;
        }
        float maxCL = StaminaConstants.RSS_AI_GROUP_COHESION_MAX_CENTER_LEADER_M;
        if (snap.centerLeaderSepM > maxCL)
        {
            float centerMul = maxCL / snap.centerLeaderSepM;
            if (centerMul < mul)
                mul = centerMul;
        }
        return Math.Clamp(mul, StaminaConstants.RSS_AI_GROUP_COHESION_GLOBAL_MIN_MUL, 1.0);
    }

    //------------------------------------------------------------------------------------------------
    //! 离几何中心过远的成员额外减速（带滞回，仅在策略刷新时按快照距离计算）
    protected static float ComputeMemberSpreadCohesionMulFromDistance(
        SCR_CharacterControllerComponent ctrl, float distM)
    {
        if (!ctrl)
            return 1.0;

        bool inOuter = ctrl.RSS_GetCohesionOuterZone();
        float enter = StaminaConstants.RSS_AI_GROUP_COHESION_OUTER_ENTER_M;
        float exit = StaminaConstants.RSS_AI_GROUP_COHESION_OUTER_EXIT_M;
        if (!inOuter)
        {
            if (distM > enter)
                inOuter = true;
        }
        else
        {
            if (distM < exit)
                inOuter = false;
        }
        ctrl.RSS_SetCohesionOuterZone(inOuter);

        if (!inOuter)
            return 1.0;

        float soft = StaminaConstants.RSS_AI_GROUP_COHESION_SOFT_RADIUS_M;
        float hard = StaminaConstants.RSS_AI_GROUP_COHESION_HARD_RADIUS_M;
        if (distM >= hard)
            return StaminaConstants.RSS_AI_GROUP_COHESION_MEMBER_MIN_MUL;

        float t = 0.0;
        if (hard > soft)
            t = Math.Clamp((distM - soft) / (hard - soft), 0.0, 1.0);
        float minMul = StaminaConstants.RSS_AI_GROUP_COHESION_MEMBER_MIN_MUL;
        return 1.0 - t * (1.0 - minMul);
    }

    //------------------------------------------------------------------------------------------------
    protected static float ComputeLeaderCenterPullbackMulFromSnap(RSS_GroupLocomotionPolicySnapshot snap)
    {
        if (!snap)
            return 1.0;
        if (snap.geometryMemberCount < 2)
            return 1.0;

        float maxSep = StaminaConstants.RSS_AI_GROUP_COHESION_MAX_CENTER_LEADER_M;
        if (snap.centerLeaderSepM <= maxSep)
            return 1.0;

        return Math.Clamp(maxSep / snap.centerLeaderSepM,
            StaminaConstants.RSS_AI_GROUP_COHESION_LEADER_MIN_MUL, 1.0);
    }

    //------------------------------------------------------------------------------------------------
    protected static float HorizontalDistance(vector a, vector b)
    {
        float dx = a[0] - b[0];
        float dz = a[2] - b[2];
        return Math.Sqrt(dx * dx + dz * dz);
    }

    protected static bool IsGroupMarching(SCR_AIGroup scrGrp)
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
                continue;
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce))
                continue;
            EAIThreatState threat = SCR_RSS_AIGroupSync.GetThreatState(ce);
            if (threat == EAIThreatState.THREATENED)
                return false;
        }
        return true;
    }

    protected static ERSS_AIStaminaState CollectWorstMemberState(SCR_AIGroup scrGrp)
    {
        ERSS_AIStaminaState worst = ERSS_AIStaminaState.FRESH;
        if (!scrGrp)
            return worst;

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
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce))
                continue;

            SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
                ce.FindComponent(SCR_CharacterControllerComponent));
            if (!ctrl)
                continue;

            ERSS_AIStaminaState memberState = ctrl.RSS_GetAIStaminaState();
            worst = SCR_RSS_AIStaminaState.PickWorse(worst, memberState);
        }
        return worst;
    }

    protected static float ComputeMedianStamina(SCR_AIGroup scrGrp)
    {
        array<float> vals = {};
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
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce))
                continue;
            SCR_CharacterStaminaComponent stm = SCR_CharacterStaminaComponent.Cast(
                ce.FindComponent(SCR_CharacterStaminaComponent));
            if (!stm)
                continue;
            vals.Insert(Math.Clamp(stm.GetTargetStamina(), 0.0, 1.0));
        }
        int n = vals.Count();
        if (n < 1)
            return 1.0;
        SortFloats(vals);
        int mid = n / 2;
        return vals.Get(mid);
    }

    protected static ERSS_GroupStaminaState ClassifyGroupTier(float median01)
    {
        if (median01 >= StaminaConstants.RSS_AI_GROUP_SYNC_FIT_THRESHOLD)
            return ERSS_GroupStaminaState.GROUP_FIT;
        if (median01 >= StaminaConstants.RSS_AI_GROUP_SYNC_TIRING_THRESHOLD)
            return ERSS_GroupStaminaState.GROUP_TIRING;
        if (median01 >= StaminaConstants.RSS_AI_GROUP_SYNC_SPENT_THRESHOLD)
            return ERSS_GroupStaminaState.GROUP_FATIGUED;
        return ERSS_GroupStaminaState.GROUP_SPENT;
    }

    protected static float ComputeGroupPaceMultiplier(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return 1.0;

        array<float> speeds = {};
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
            if (!SCR_RSS_AIGroupSync.IsAliveAndActionablePublic(ce))
                continue;

            float weight = 0.0;
            SCR_CharacterInventoryStorageComponent inv = SCR_CharacterInventoryStorageComponent.Cast(
                ce.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (inv)
                weight = inv.GetTotalWeight();

            float stamina = 1.0;
            SCR_CharacterStaminaComponent stm = SCR_CharacterStaminaComponent.Cast(
                ce.FindComponent(SCR_CharacterStaminaComponent));
            if (stm)
                stamina = Math.Clamp(stm.GetTargetStamina(), 0.0, 1.0);

            float effectiveRatio = (90.0 / (90.0 + weight)) * Math.Pow(stamina, 0.6);
            speeds.Insert(RealisticStaminaSpeedSystem.GAME_MAX_SPEED * effectiveRatio);
        }

        int nSpeeds = speeds.Count();
        if (nSpeeds < 1)
            return 1.0;

        SortFloats(speeds);
        int idx = Math.Round(nSpeeds * 0.25);
        if (idx < 0)
            idx = 0;
        if (idx >= nSpeeds)
            idx = nSpeeds - 1;

        return Math.Clamp(speeds.Get(idx) / RealisticStaminaSpeedSystem.GAME_MAX_SPEED,
            StaminaConstants.RSS_AI_GROUP_SYNC_PACE_MIN, 1.0);
    }

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
}
