// 地形检测模块
// 负责处理地形密度检测和地形系数计算（性能优化：动态检测频率 + 距离LOD）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class TerrainDetector
{
    // ==================== 状态变量 ====================
    protected float m_fCachedTerrainDensity = -1.0; // 缓存的地面密度值
    protected string m_sCachedGroundMaterialLabel = ""; // 射线材质通用显示名（如 metal）
    protected float m_fCachedTerrainFactor = 1.0; // 缓存的地形系数
    protected float m_fLastTerrainCheckTime = 0.0; // 上次地形检测时间
    protected float m_fLastMovementTime = 0.0; // 上次移动时间（用于优化静止时的检测）
    protected const float TERRAIN_CHECK_INTERVAL = 0.5; // 地形检测间隔（秒，玩家/近距AI移动时）
    protected const float TERRAIN_CHECK_INTERVAL_IDLE = 2.0; // 地形检测间隔（秒，静止时，优化性能）
    protected const float IDLE_THRESHOLD_TIME = 1.0; // 静止判定阈值（秒，超过此时间视为静止）
    protected ref TraceParam m_pTraceParamGround; // 复用的 TraceParam（GetTerrainDensity）

    // ==================== 距离LOD状态变量 ====================
    // AI 实体按与最近玩家的距离分档降低检测频率，减少射线追踪开销
    // 注意：m_bIsAiEntity 可在运行时通过 SetIsAiEntity() 更新，
    //       以应对玩家接管 AI 角色（OnControlledByPlayer）等动态控制权变更场景。
    protected bool m_bIsAiEntity = false; // 是否为AI实体（可运行时更新，避免接管后误用LOD）
    protected float m_fLastDistanceLodCheckTime = 0.0; // 上次距离LOD计算时间
    protected float m_fCachedDistToNearestPlayer = 0.0; // 缓存的最近玩家距离（秒级更新）
    protected const float DISTANCE_LOD_CACHE_INTERVAL = 2.0; // 距离缓存刷新间隔（秒）
    protected const float LOD_NEAR_DIST = 500.0; // 近距阈值（m）：<500m 使用正常间隔
    protected const float LOD_MID_DIST = 1000.0; // 中距阈值（m）：500-1000m 使用 2 倍间隔
    // 超过 LOD_MID_DIST 的 AI 使用 5 倍间隔（远距档）
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    // @param isAi 是否为AI实体（用于距离LOD分档）
    void Initialize(bool isAi = false)
    {
        m_fCachedTerrainDensity = -1.0;
        m_sCachedGroundMaterialLabel = "";
        m_fCachedTerrainFactor = 1.0;
        m_fLastTerrainCheckTime = 0.0;
        m_fLastMovementTime = 0.0;
        m_bIsAiEntity = isAi;
        m_fLastDistanceLodCheckTime = 0.0;
        m_fCachedDistToNearestPlayer = 0.0;
    }
    
    // 运行时更新实体类型标志（玩家接管AI角色时调用，防止接管后继续以AI LOD频率检测）
    // @param isAi true=AI实体（启用距离LOD），false=玩家控制（全频率）
    void SetIsAiEntity(bool isAi)
    {
        if (m_bIsAiEntity == isAi)
            return;
        m_bIsAiEntity = isAi;
        // 切换为玩家控制时重置距离缓存，确保下次立即以玩家频率检测
        if (!isAi)
        {
            m_fCachedDistToNearestPlayer = 0.0;
            m_fLastDistanceLodCheckTime = 0.0;
        }
    }

    // 更新移动时间（用于判断静止状态）
    // @param currentTime 当前世界时间
    // @param isMoving 是否正在移动
    void UpdateMovementTime(float currentTime, bool isMoving)
    {
        if (isMoving)
            m_fLastMovementTime = currentTime;
    }
    
    // 检查并更新地形系数（如果需要）
    // @param owner 角色实体
    // @param currentTime 当前世界时间
    // @param currentSpeed 当前速度（用于判断是否静止）
    // @return 地形系数（1.0表示铺装路面，越大越难走）
    float GetTerrainFactor(IEntity owner, float currentTime, float currentSpeed)
    {
        bool isCurrentlyMoving = (currentSpeed > 0.05);

        // 更新移动时间
        UpdateMovementTime(currentTime, isCurrentlyMoving);

        // ── 确定本次检测间隔 ──────────────────────────────────────────────
        float terrainCheckInterval = TERRAIN_CHECK_INTERVAL; // 基础 0.5s

        // AI 实体：按与最近玩家的距离分档（距离LOD）
        if (m_bIsAiEntity)
        {
            // 每 DISTANCE_LOD_CACHE_INTERVAL 秒刷新一次距离缓存，避免每帧调用 GetPlayerManager
            if (currentTime - m_fLastDistanceLodCheckTime >= DISTANCE_LOD_CACHE_INTERVAL)
            {
                m_fCachedDistToNearestPlayer = GetDistanceToNearestPlayer(owner);
                m_fLastDistanceLodCheckTime = currentTime;
            }

            if (m_fCachedDistToNearestPlayer > LOD_MID_DIST)
                terrainCheckInterval = TERRAIN_CHECK_INTERVAL * 5.0; // 远距：2.5s
            else if (m_fCachedDistToNearestPlayer > LOD_NEAR_DIST)
                terrainCheckInterval = TERRAIN_CHECK_INTERVAL * 2.0; // 中距：1.0s
            // 近距（<500m）：保持默认 0.5s
        }

        // 静止时进一步降频
        if (!isCurrentlyMoving)
        {
            float idleDuration = currentTime - m_fLastMovementTime;
            if (idleDuration >= IDLE_THRESHOLD_TIME)
            {
                // 长时间静止（≥1秒）：与 AI 距离档叠加，取较大值
                float idleInterval = TERRAIN_CHECK_INTERVAL_IDLE; // 2.0s
                if (idleInterval > terrainCheckInterval)
                    terrainCheckInterval = idleInterval;
            }
        }

        // ── 按间隔执行检测 ────────────────────────────────────────────────
        if (currentTime - m_fLastTerrainCheckTime > terrainCheckInterval)
        {
            float density = GetTerrainDensity(owner);
            m_fCachedTerrainDensity = density;

            if (density >= 0.0)
                m_fCachedTerrainFactor = RealisticStaminaSpeedSystem.GetTerrainFactorFromDensity(density);

            m_fLastTerrainCheckTime = currentTime;
        }

        return m_fCachedTerrainFactor;
    }

    // 获取与最近玩家的距离（仅 AI 距离LOD使用，每 2 秒调用一次）
    // @param owner AI实体
    // @return 最近玩家距离（m），无玩家时返回 float.MAX
    protected float GetDistanceToNearestPlayer(IEntity owner)
    {
        if (!owner)
            return float.MAX;

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return float.MAX;

        array<int> playerIds = new array<int>();
        pm.GetPlayers(playerIds);
        if (playerIds.IsEmpty())
            return float.MAX;

        vector ownerPos = owner.GetOrigin();
        float minDistSq = float.MAX;

        for (int i = 0, cnt = playerIds.Count(); i < cnt; i++)
        {
            PlayerController pc = pm.GetPlayerController(playerIds[i]);
            if (!pc)
                continue;
            IEntity playerEnt = pc.GetControlledEntity();
            if (!playerEnt)
                continue;
            float dSq = vector.DistanceSq(ownerPos, playerEnt.GetOrigin());
            if (dSq < minDistSq)
                minDistSq = dSq;
        }

        if (minDistSq >= float.MAX)
            return float.MAX;

        return Math.Sqrt(minDistSq);
    }
    
    // 获取地形密度（使用射线追踪）
    // @param owner 角色实体
    // @return 地面密度值（-1 表示获取失败）
    float GetTerrainDensity(IEntity owner)
    {
        if (!owner)
        {
            m_sCachedGroundMaterialLabel = "";
            return -1.0;
        }
        
        if (!m_pTraceParamGround)
            m_pTraceParamGround = new TraceParam();

        // 执行射线追踪检测地面
        m_pTraceParamGround.Start = owner.GetOrigin() + (vector.Up * 0.1); // 角色位置上方 0.1 米
        m_pTraceParamGround.End = m_pTraceParamGround.Start - (vector.Up * 0.5); // 向下追踪 0.5 米
        m_pTraceParamGround.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        m_pTraceParamGround.Exclude = owner;
        // 优化：使用更合适的物理层以确保稳定获取地面材质
        // 建议：EPhysicsLayerPresets.Character 或 EPhysicsLayerPresets.Static 可能更稳定
        // 如果 Projectile 层在某些情况下无法命中地面，可以尝试其他层
        m_pTraceParamGround.LayerMask = EPhysicsLayerPresets.Projectile; // 当前使用 Projectile（与官方示例一致）
        
        // 执行追踪
        owner.GetWorld().TraceMove(m_pTraceParamGround, FilterTerrainCallback);
        
        // 获取表面材质
        GameMaterial material = m_pTraceParamGround.SurfaceProps;
        if (!material)
        {
            m_sCachedGroundMaterialLabel = "";
            return -1.0;
        }

        m_sCachedGroundMaterialLabel = MaterialTerrainTable.GetGenericMaterialLabel(material);

        // 获取弹道信息（包含密度）
        BallisticInfo ballisticInfo = material.GetBallisticInfo();
        if (!ballisticInfo)
            return -1.0;

        // 优先使用内置 CSV 表（MaterialTerrainTable），与原版材质 basename 对齐；否则用弹道密度
        float density = ballisticInfo.GetDensity();
        density = MaterialTerrainTable.ResolveDensity(material, density);
        return density;
    }
    
    // 地形检测过滤回调（排除角色实体）
    // @param e 实体
    // @return true表示接受，false表示排除
    bool FilterTerrainCallback(IEntity e)
    {
        if (ChimeraCharacter.Cast(e))
            return false;
        return true;
    }
    
    // 获取缓存的地形密度（用于调试显示）
    // @return 地形密度值（-1表示未检测）
    float GetCachedTerrainDensity()
    {
        return m_fCachedTerrainDensity;
    }

    // 上次射线解析的材质通用名（与密度缓存同步更新频率）
    string GetCachedGroundMaterialLabel()
    {
        return m_sCachedGroundMaterialLabel;
    }
    
    // 获取缓存的地形系数
    // @return 地形系数（1.0-3.0）
    float GetCachedTerrainFactor()
    {
        return m_fCachedTerrainFactor;
    }
    
    // 手动更新地形检测（用于调试）
    // @param owner 角色实体
    // @param currentTime 当前世界时间
    void ForceUpdate(IEntity owner, float currentTime)
    {
        float density = GetTerrainDensity(owner);
        m_fCachedTerrainDensity = density;
        
        if (density >= 0.0)
            m_fCachedTerrainFactor = RealisticStaminaSpeedSystem.GetTerrainFactorFromDensity(density);
        
        m_fLastTerrainCheckTime = currentTime;
    }
}
