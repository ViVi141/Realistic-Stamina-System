// 地形检测模块
// 负责处理地形密度检测和地形系数计算（性能优化：动态检测频率）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class TerrainDetector
{
    // ==================== 状态变量 ====================
    protected float m_fCachedTerrainDensity = -1.0; // 缓存的地面密度值
    protected float m_fCachedTerrainFactor = 1.0; // 缓存的地形系数
    protected float m_fLastTerrainCheckTime = 0.0; // 上次地形检测时间
    protected float m_fLastMovementTime = 0.0; // 上次移动时间（用于优化静止时的检测）
    protected const float TERRAIN_CHECK_INTERVAL = 0.5; // 地形检测间隔（秒，移动时）
    protected const float TERRAIN_CHECK_INTERVAL_IDLE = 2.0; // 地形检测间隔（秒，静止时，优化性能）
    protected const float IDLE_THRESHOLD_TIME = 1.0; // 静止判定阈值（秒，超过此时间视为静止）
    protected ref TraceParam m_pTraceParamGround; // 复用的 TraceParam（GetTerrainDensity）
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    void Initialize()
    {
        m_fCachedTerrainDensity = -1.0;
        m_fCachedTerrainFactor = 1.0;
        m_fLastTerrainCheckTime = 0.0;
        m_fLastMovementTime = 0.0;
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
        
        // 检查是否需要更新地形系数
        // 移动时：每0.5秒检测一次
        // 静止时：每2秒检测一次（或停止检测）
        bool shouldCheckTerrain = false;
        float terrainCheckInterval = TERRAIN_CHECK_INTERVAL;
        
        if (isCurrentlyMoving)
        {
            // 移动中：正常频率检测
            shouldCheckTerrain = (currentTime - m_fLastTerrainCheckTime > terrainCheckInterval);
        }
        else
        {
            // 静止时：检查静止时间
            float idleDuration = currentTime - m_fLastMovementTime;
            if (idleDuration < IDLE_THRESHOLD_TIME)
            {
                // 刚停止移动（<1秒）：继续检测（因为可能还在变化地形）
                shouldCheckTerrain = (currentTime - m_fLastTerrainCheckTime > terrainCheckInterval);
            }
            else
            {
                // 长时间静止（≥1秒）：降低检测频率或停止检测
                terrainCheckInterval = TERRAIN_CHECK_INTERVAL_IDLE; // 2秒
                shouldCheckTerrain = (currentTime - m_fLastTerrainCheckTime > terrainCheckInterval);
            }
        }
        
        if (shouldCheckTerrain)
        {
            // 获取地面密度
            float density = GetTerrainDensity(owner);
            m_fCachedTerrainDensity = density;
            
            // 根据密度计算地形系数（插值映射）
            if (density >= 0.0)
                m_fCachedTerrainFactor = RealisticStaminaSpeedSystem.GetTerrainFactorFromDensity(density);
            
            m_fLastTerrainCheckTime = currentTime;
        }
        
        // 返回缓存的地形系数
        return m_fCachedTerrainFactor;
    }
    
    // 获取地形密度（使用射线追踪）
    // @param owner 角色实体
    // @return 地面密度值（-1 表示获取失败）
    float GetTerrainDensity(IEntity owner)
    {
        if (!owner)
            return -1.0;
        
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
            return -1.0;
        
        // 获取弹道信息（包含密度）
        BallisticInfo ballisticInfo = material.GetBallisticInfo();
        if (!ballisticInfo)
            return -1.0;
        
        // 返回密度值
        float density = ballisticInfo.GetDensity();
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
