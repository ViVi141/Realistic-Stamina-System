//! 群组休整：由队长对群组重心做 Cover 查询（GetBestCover），仅取世界坐标供动态防守路点使用；
//! 不修改 SCR_AICombatMoveState（全队靠 SCR_AIGroup.AddWaypointsDynamic 靠拢）。

class SCR_RSS_AICoverSeeker
{
    //------------------------------------------------------------------------------------------------
    protected static ChimeraCoverManagerComponent ResolveCoverManager()
    {
        AIWorld aiWorld = GetGame().GetAIWorld();
        if (!aiWorld)
            return null;
        return ChimeraCoverManagerComponent.Cast(aiWorld.FindComponent(CoverManagerComponent));
    }

    //------------------------------------------------------------------------------------------------
    //! 威胁方向：优先当前/危害目标，否则用角色前向远处点，避免无威胁时查询退化。
    protected static vector ComputeSectorDirAndThreat(IEntity owner, out vector threatPosOut)
    {
        vector myPos = owner.GetOrigin();
        threatPosOut = vector.Zero;
        bool hasThreat = false;

        SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(owner.FindComponent(SCR_AICombatComponent));
        if (combat)
        {
            BaseTarget tgt = combat.GetCurrentTarget();
            if (!tgt)
                tgt = combat.GetEndangeringEnemy();
            if (tgt)
            {
                IEntity te = tgt.GetTargetEntity();
                if (te)
                {
                    threatPosOut = te.GetOrigin();
                    hasThreat = true;
                }
                else
                {
                    vector ls = tgt.GetLastSeenPosition();
                    vector deltaLs = ls - myPos;
                    if (deltaLs.LengthSq() > 0.25)
                    {
                        threatPosOut = ls;
                        hasThreat = true;
                    }
                    else
                    {
                        vector ld = tgt.GetLastDetectedPosition();
                        vector deltaLd = ld - myPos;
                        if (deltaLd.LengthSq() > 0.25)
                        {
                            threatPosOut = ld;
                            hasThreat = true;
                        }
                    }
                }
            }
        }

        if (!hasThreat)
        {
            vector worldMat[4];
            owner.GetWorldTransform(worldMat);
            vector fwd = worldMat[2];
            fwd[1] = 0.0;
            float fl = fwd.Length();
            if (fl > 0.01)
            {
                fwd = fwd / fl;
            }
            else
            {
                fwd = Vector(0.0, 0.0, 1.0);
            }
            threatPosOut = myPos + fwd * 80.0;
        }

        vector dir = threatPosOut - myPos;
        dir[1] = 0.0;
        float len = dir.Length();
        if (len > 0.01)
        {
            return dir / len;
        }
        return Vector(1.0, 0.0, 0.0);
    }

    //------------------------------------------------------------------------------------------------
    protected static CoverQueryProperties BuildRestCoverQuery(IEntity threatReferenceEntity, vector sectorPos)
    {
        vector threatPos;
        vector sectorDir = ComputeSectorDirAndThreat(threatReferenceEntity, threatPos);

        CoverQueryProperties props = new CoverQueryProperties();
        props.m_iMaxCoversToCheck = StaminaConstants.RSS_AI_COVER_QUERY_MAX_COVERS;
        props.m_vNearestPolyHalfExtend = Vector(1.0, 2.0, 1.0);
        props.m_vSectorPos = sectorPos;
        props.m_vSectorDir = sectorDir;
        props.m_vThreatPos = threatPos;
        props.m_fQuerySectorAngleCosMin = -1.0;
        props.m_fSectorDistMin = 0.0;
        props.m_fSectorDistMax = StaminaConstants.RSS_AI_COVER_QUERY_SECTOR_DIST_MAX_M;
        props.m_fCoverHeightMin = 0.3;
        props.m_fCoverHeightMax = 10.0;
        props.m_fCoverToThreatAngleCosMin = -1.0;
        props.m_fScoreWeightDirection = 1.0;
        props.m_fScoreWeightDistance = 1.0;
        props.m_fNmAreaCostScale = StaminaConstants.RSS_AI_COVER_NAVMESH_AREA_COST_SCALE;
        props.m_bCheckVisibility = false;
        props.m_fVisibilityTraceFraction = 0.0;
        props.m_bSelectHighestScore = true;
        return props;
    }

    //------------------------------------------------------------------------------------------------
    //! 队长（或回退实体）用 pathfinding + 群组重心扇区做 GetBestCover，仅输出世界坐标，不锁掩护、不发起战斗移动。
    static bool TryGetBestCoverWorldPositionForGroup(IEntity leaderEntity, vector groupSectorCenterPos, out vector outCoverPos)
    {
        outCoverPos = vector.Zero;
        if (!leaderEntity)
            return false;
        if (!Replication.IsServer())
            return false;

        ChimeraCoverManagerComponent coverMgr = ResolveCoverManager();
        if (!coverMgr)
            return false;

        AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(leaderEntity.FindComponent(AIPathfindingComponent));
        if (!pathfinding)
            return false;

        CoverQueryProperties query = BuildRestCoverQuery(leaderEntity, groupSectorCenterPos);

        vector outCoverTallestPos;
        int outTileX;
        int outTileY;
        int outCoverId;

        bool ok = coverMgr.GetBestCover(
            StaminaConstants.RSS_AI_COVER_NAVMESH_WORLD_NAME,
            pathfinding,
            query,
            outCoverPos,
            outCoverTallestPos,
            outTileX,
            outTileY,
            outCoverId);
        return ok;
    }

    //------------------------------------------------------------------------------------------------
    //! 对已分配掩护周期性校验（官方建议：CombatMoveState.VerifyCurrentCover）。
    static void TickVerifyCombatCover(IEntity owner)
    {
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
        if (!ctrl)
            return;
        if (ctrl.IsPlayerControlled())
            return;

        SCR_AIUtilityComponent util = SCR_RSS_AIStaminaBridge.ResolveAIUtilityForCharacter(owner);
        if (!util)
            return;

        SCR_AICombatMoveState state = util.m_CombatMoveState;
        if (!state)
            return;
        if (!state.IsAssignedCoverValid())
            return;

        state.VerifyCurrentCover(owner.GetOrigin());
    }
}
