//! 室内检测模块（从 SCR_EnvironmentFactor.c 拆分）
//! 负责建筑物内/有顶空间判定，包括屋顶射线与水平封闭检测
class SCR_RSS_IndoorDetection
{
    // ==================== 状态变量 ====================
    protected float m_fLastIndoorCheckTime = 0.0;
    protected bool m_bCachedIndoorState = false;
    protected bool m_bCachedRoofedVolumeForSlopeState = false;
    protected ref array<IEntity> m_pCachedBuildings;
    protected ref TraceParam m_pTraceParamRoof;
    protected ref TraceParam m_pTraceParamEnclosed;
    protected bool m_bIndoorDebug = false;

    protected const float INDOOR_CHECK_INTERVAL = 2.0; // perf: 1→2，室内/室外切换频率低

    // ==================== 调试开关 ====================

    void SetIndoorDebug(bool enabled)
    {
        m_bIndoorDebug = enabled;
    }

    bool GetIndoorDebug()
    {
        return m_bIndoorDebug;
    }

    // ==================== 核心检测方法 ====================

    // 检测角色是否在室内（完整室内判定：OBB内 + 有屋顶 + 水平封闭）
    bool IsIndoor(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (!owner)
            return false;
        if (owner)
        {
            float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            if (currentTime - m_fLastIndoorCheckTime < INDOOR_CHECK_INTERVAL)
                return m_bCachedIndoorState;
        }
        return IsUnderCover(owner);
    }

    // 建筑物内有顶且角色在 OBB 内，但不要求水平封闭
    bool IsRoofedBuildingVolumeForEntity(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (!owner)
            return false;
        return EvaluateRoofedBuildingInterior(owner, SCR_RSS_Constants.ENV_SLOPE_SUPPRESS_ROOF_CHECK_HEIGHT, false);
    }

    // 地形坡度是否应按「非室外」处理为零
    bool ShouldSuppressTerrainSlopeForEntity(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (!owner)
            return false;
        if (IsIndoorForEntity(owner))
            return true;
        return IsRoofedBuildingVolumeForEntity(owner);
    }

    // 检查指定实体是否在室内（用于坡度/速度计算）
    bool IsIndoorForEntity(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (!owner)
            return false;
        return IsUnderCover(owner);
    }

    // 按间隔更新室内状态缓存
    bool UpdateIndoorCache(IEntity owner, float currentTime)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled() || !owner)
            return false;

        if (currentTime - m_fLastIndoorCheckTime >= INDOOR_CHECK_INTERVAL)
        {
            m_fLastIndoorCheckTime = currentTime;
            m_bCachedIndoorState = IsUnderCover(owner);
            m_bCachedRoofedVolumeForSlopeState = EvaluateRoofedBuildingInterior(
                owner, SCR_RSS_Constants.ENV_SLOPE_SUPPRESS_ROOF_CHECK_HEIGHT, false);
            return true;
        }
        return false;
    }

    float GetLastIndoorCheckTime()
    {
        return m_fLastIndoorCheckTime;
    }

    bool GetCachedIndoorState()
    {
        return m_bCachedIndoorState;
    }

    bool GetCachedRoofedVolumeForSlopeState()
    {
        return m_bCachedRoofedVolumeForSlopeState;
    }

    // ==================== 底层检测实现 ====================

    // 检测角色是否在室内（基于建筑物边界框 + 向上射线确认）
    bool IsUnderCover(IEntity owner)
    {
        return EvaluateRoofedBuildingInterior(owner, SCR_RSS_Constants.ENV_INDOOR_CHECK_HEIGHT, true);
    }

    // 在建筑物 OBB 内且向上能命中该建筑物的遮挡时返回 true
    bool EvaluateRoofedBuildingInterior(IEntity owner, float roofCheckHeightM, bool requireHorizontalEnclosure)
    {
        if (!owner)
            return false;

        World world = owner.GetWorld();
        if (!world)
            return false;

        vector ownerPos = owner.GetOrigin();

        vector searchMins = ownerPos + Vector(-50, -50, -50);
        vector searchMaxs = ownerPos + Vector(50, 50, 50);

        if (m_pCachedBuildings)
            m_pCachedBuildings.Clear();
        else
            m_pCachedBuildings = new array<IEntity>();

        world.QueryEntitiesByAABB(searchMins, searchMaxs, QueryBuildingCallback);

        int buildingCount = m_pCachedBuildings.Count();
        if (m_bIndoorDebug)
        {
            string reqEnclStr;
            if (requireHorizontalEnclosure)
                reqEnclStr = "true";
            else
                reqEnclStr = "false";
            PrintFormat("[RSS][IndoorDetect] EvaluateRoofedBuildingInterior: ownerPos=(%1,%2,%3) buildingCount=%4 requireEnclosed=%5",
                ownerPos[0], ownerPos[1], ownerPos[2], buildingCount, reqEnclStr);
        }
        if (buildingCount == 0)
            return false;

        int checkedBuildings = 0;
        foreach (IEntity building : m_pCachedBuildings)
        {
            if (!building)
                continue;

            checkedBuildings++;

            vector buildingMins, buildingMaxs;
            building.GetBounds(buildingMins, buildingMaxs);

            vector buildingMat[4];
            building.GetWorldTransform(buildingMat);

            vector localPos = WorldToLocal(buildingMat, ownerPos);

            bool xInside = (localPos[0] >= buildingMins[0] && localPos[0] <= buildingMaxs[0]);
            bool yInside = (localPos[1] >= buildingMins[1] && localPos[1] <= buildingMaxs[1]);
            bool zInside = (localPos[2] >= buildingMins[2] && localPos[2] <= buildingMaxs[2]);

            bool isInside = xInside && yInside && zInside;

            if (m_bIndoorDebug)
                PrintFormat("[RSS][IndoorDetect] Building #%1 localPos=(%2,%3,%4) mins=(%5,%6,%7) maxs=(%8,%9,%10)",
                    checkedBuildings,
                    Math.Round(localPos[0] * 100.0) / 100.0,
                    Math.Round(localPos[1] * 100.0) / 100.0,
                    Math.Round(localPos[2] * 100.0) / 100.0,
                    buildingMins[0], buildingMins[1], buildingMins[2],
                    buildingMaxs[0], buildingMaxs[1], buildingMaxs[2]);

            if (!isInside)
                continue;

            bool hasRoof = RaycastHasRoof(owner, building, roofCheckHeightM);
            if (m_bIndoorDebug)
            {
                string hasRoofStr;
                if (hasRoof)
                    hasRoofStr = "true";
                else
                    hasRoofStr = "false";
                PrintFormat("[RSS][IndoorDetect] Building #%1 isInside=true hasRoof=%2", checkedBuildings, hasRoofStr);
            }

            if (!hasRoof)
                continue;

            if (!requireHorizontalEnclosure)
                return true;

            bool enclosed = IsHorizontallyEnclosed(owner);
            if (m_bIndoorDebug)
            {
                string enclosedStr;
                if (enclosed)
                    enclosedStr = "true";
                else
                    enclosedStr = "false";
                PrintFormat("[RSS][IndoorDetect] Building #%1 roof=true enclosed=%2", checkedBuildings, enclosedStr);
            }
            if (enclosed)
                return true;
        }

        if (m_bIndoorDebug)
            PrintFormat("[RSS][IndoorDetect] No matching building after checking %1 buildings", checkedBuildings);
        return false;
    }

    // 向上射线检测屋顶
    protected bool RaycastHasRoof(IEntity owner, IEntity building, float roofCheckHeightM)
    {
        if (!owner || !building)
            return false;
        World world = owner.GetWorld();
        if (!world)
            return false;

        vector basePos = owner.GetOrigin();
        const float HEAD_HEIGHT = 1.6;
        const float SAMPLE_OFFSET = 0.4;

        array<vector> samples = { vector.Zero, vector.Forward * SAMPLE_OFFSET, -vector.Forward * SAMPLE_OFFSET, vector.Right * SAMPLE_OFFSET, -vector.Right * SAMPLE_OFFSET };

        if (m_bIndoorDebug)
            PrintFormat("[RSS][IndoorDetect] RaycastHasRoof: ownerPos=(%1,%2,%3) HEAD_HEIGHT=%4 CHECK_HEIGHT=%5 samples=%6",
                basePos[0], basePos[1], basePos[2], HEAD_HEIGHT, roofCheckHeightM, samples.Count());

        if (!m_pTraceParamRoof)
            m_pTraceParamRoof = new TraceParam();

        int idx = 0;
        foreach (vector off : samples)
        {
            idx++;
            vector start = basePos + vector.Up * HEAD_HEIGHT + off;
            vector end = start + vector.Up * roofCheckHeightM;

            m_pTraceParamRoof.Start = start;
            m_pTraceParamRoof.End = end;
            m_pTraceParamRoof.Flags = TraceFlags.ENTS;
            m_pTraceParamRoof.Include = building;
            m_pTraceParamRoof.Exclude = owner;
            m_pTraceParamRoof.LayerMask = EPhysicsLayerDefs.Projectile;

            world.TraceMove(m_pTraceParamRoof, null);

            bool hit = (m_pTraceParamRoof.TraceEnt != null);

            if (m_bIndoorDebug)
            {
                string surface;
                if (m_pTraceParamRoof.SurfaceProps)
                    surface = m_pTraceParamRoof.SurfaceProps.ToString();
                else
                    surface = "null";
                PrintFormat("[RSS][IndoorDetect] Sample %1 start=(%2,%3,%4) end=(%5,%6,%7) -> TraceEnt=%8 Collider=%9",
                    idx, start[0], start[1], start[2], end[0], end[1], end[2], m_pTraceParamRoof.TraceEnt, m_pTraceParamRoof.ColliderName);
                PrintFormat("[RSS][IndoorDetect]   Surface=%1", surface);
            }

            if (!hit)
            {
                if (m_bIndoorDebug)
                    PrintFormat("[RSS][IndoorDetect] Sample %1 missed -> not indoor", idx);
                return false;
            }
        }

        if (m_bIndoorDebug)
            PrintFormat("[RSS][IndoorDetect] All samples hit -> indoor");

        return true;
    }

    // 世界坐标 → 实体本地坐标
    protected vector WorldToLocal(vector worldMat[4], vector worldPos)
    {
        vector delta = worldPos - worldMat[3];

        vector localPos;
        localPos[0] = vector.Dot(delta, worldMat[0]);
        localPos[1] = vector.Dot(delta, worldMat[1]);
        localPos[2] = vector.Dot(delta, worldMat[2]);

        return localPos;
    }

    // 水平径向封闭检测
    protected bool IsHorizontallyEnclosed(IEntity owner)
    {
        if (!owner)
            return false;
        World world = owner.GetWorld();
        if (!world)
            return false;

        vector basePos = owner.GetOrigin();
        const float HEAD_HEIGHT = 1.6;
        const int SAMPLES = 8;
        const float DIST = 1.2;
        const float HIT_RATIO = 0.75;

        int hits = 0;

        if (!m_pTraceParamEnclosed)
            m_pTraceParamEnclosed = new TraceParam();

        for (int i = 0; i < SAMPLES; i++)
        {
            float angle = (360.0 / SAMPLES) * i;
            const float DEG2RAD = 3.14159265 / 180.0;
            float rad = angle * DEG2RAD;
            vector dir = Vector(Math.Cos(rad), Math.Sin(rad), 0);

            vector start = basePos + vector.Up * HEAD_HEIGHT;
            vector end = start + dir * DIST;

            m_pTraceParamEnclosed.Start = start;
            m_pTraceParamEnclosed.End = end;
            m_pTraceParamEnclosed.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
            m_pTraceParamEnclosed.Exclude = owner;
            m_pTraceParamEnclosed.LayerMask = EPhysicsLayerPresets.Projectile;

            world.TraceMove(m_pTraceParamEnclosed, null);

            bool hit = (m_pTraceParamEnclosed.TraceEnt != null) || (m_pTraceParamEnclosed.SurfaceProps != null) || (m_pTraceParamEnclosed.ColliderName != string.Empty);
            if (hit)
            {
                hits++;
            }

            if (m_bIndoorDebug)
            {
                string hitStr;
                if (hit)
                    hitStr = "true";
                else
                    hitStr = "false";
                PrintFormat("[RSS][IndoorDetect] Horizontal sample %1 angle=%2 hit=%3", i + 1, Math.Round(angle), hitStr);
            }
        }

        float ratio = (hits / (float)SAMPLES);
        if (m_bIndoorDebug)
            PrintFormat("[RSS][IndoorDetect] Horizontal enclosure hits=%1/%2 ratio=%3", hits, SAMPLES, Math.Round(ratio * 100.0) / 100.0);

        return (ratio >= HIT_RATIO);
    }

    // 建筑物查询回调
    protected bool QueryBuildingCallback(IEntity e)
    {
        if (!e)
            return true;

        Building building = Building.Cast(e);
        if (!building)
            return true;

        if (ChimeraCharacter.Cast(e))
            return true;

        m_pCachedBuildings.Insert(e);

        return true;
    }
}
