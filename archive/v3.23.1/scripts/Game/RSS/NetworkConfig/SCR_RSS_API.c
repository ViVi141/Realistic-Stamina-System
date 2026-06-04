// Realistic Stamina System (RSS) - 外部模组 API
// 供其他模组获取玩家体力状态与环境信息
// 用法：SCR_RSS_API.GetPlayerInfo(entity) / SCR_RSS_API.GetEnvironmentInfo(entity)

// ==================== 玩家信息结构体 ====================
class RSS_PlayerInfo
{
    float staminaPercent;       // 体力百分比 (0.0 ~ 1.0)
    float speedMultiplier;      // 当前速度倍率 (0.15 ~ 1.0)
    float currentSpeed;        // 当前水平速度 (m/s)
    int movementPhase;         // 移动阶段: 0=idle, 1=walk, 2=run, 3=sprint
    bool isSprinting;          // 是否在冲刺
    bool isExhausted;          // 是否精疲力尽
    bool isSwimming;           // 是否在游泳
    float currentWeight;      // 当前负重 (kg)
    bool isValid;              // 数据是否有效（实体有 RSS 组件且已初始化）
}

// ==================== 环境信息结构体 ====================
class RSS_EnvironmentInfo
{
    float temperature;         // 气温 (°C)
    float rainIntensity;      // 降雨强度 (0.0 ~ 1.0)
    float windSpeed;          // 风速 (m/s)
    float windDirection;      // 风向 (度)
    float surfaceWetness;     // 地表湿度 (0.0 ~ 1.0)
    float totalWetWeight;     // 总湿重 (kg，游泳+降雨)
    bool isIndoor;            // 是否室内
    float heatStressMultiplier;   // 热应激倍数
    float heatStressPenalty;      // 热应激惩罚
    float coldStressPenalty;     // 冷应激惩罚
    bool isValid;             // 数据是否有效
}

// ==================== RSS API 静态类 ====================
class SCR_RSS_API
{
    protected static ref RSS_PlayerInfo s_pPlayerInfoCache;
    protected static ref RSS_EnvironmentInfo s_pEnvInfoCache;

    // 获取 RSS 控制的角色控制器组件（供高级用法）
    // @param entity 角色实体（IEntity，通常为 ChimeraCharacter）
    // @return SCR_CharacterControllerComponent 或 null
    static SCR_CharacterControllerComponent GetRssController(IEntity entity)
    {
        if (!entity)
            return null;
        return SCR_CharacterControllerComponent.Cast(entity.FindComponent(SCR_CharacterControllerComponent));
    }

    // 获取玩家当前体力与运动状态
    // @param entity 角色实体
    // @return RSS_PlayerInfo，若无效则 isValid=false
    static RSS_PlayerInfo GetPlayerInfo(IEntity entity)
    {
        if (!s_pPlayerInfoCache)
            s_pPlayerInfoCache = new RSS_PlayerInfo();

        s_pPlayerInfoCache.staminaPercent = 0.0;
        s_pPlayerInfoCache.speedMultiplier = 1.0;
        s_pPlayerInfoCache.currentSpeed = 0.0;
        s_pPlayerInfoCache.movementPhase = 0;
        s_pPlayerInfoCache.isSprinting = false;
        s_pPlayerInfoCache.isExhausted = false;
        s_pPlayerInfoCache.isSwimming = false;
        s_pPlayerInfoCache.currentWeight = 0.0;
        s_pPlayerInfoCache.isValid = false;

        SCR_CharacterControllerComponent ctrl = GetRssController(entity);
        if (!ctrl)
            return s_pPlayerInfoCache;

        if (!ctrl.HasRssData())
            return s_pPlayerInfoCache;

        s_pPlayerInfoCache.staminaPercent = ctrl.GetRssStaminaPercent();
        s_pPlayerInfoCache.speedMultiplier = ctrl.GetRssSpeedMultiplier();
        s_pPlayerInfoCache.currentSpeed = ctrl.GetRssCurrentSpeed();
        s_pPlayerInfoCache.movementPhase = ctrl.GetRssMovementPhase();
        s_pPlayerInfoCache.isSprinting = ctrl.GetRssIsSprinting();
        s_pPlayerInfoCache.isExhausted = ctrl.GetRssIsExhausted();
        s_pPlayerInfoCache.isSwimming = ctrl.GetRssIsSwimming();
        s_pPlayerInfoCache.currentWeight = ctrl.GetRssCurrentWeight();
        s_pPlayerInfoCache.isValid = true;

        return s_pPlayerInfoCache;
    }

    // 获取角色所在位置的环境信息
    // @param entity 角色实体（用于室内检测与位置相关环境）
    // @return RSS_EnvironmentInfo，若无效则 isValid=false
    static RSS_EnvironmentInfo GetEnvironmentInfo(IEntity entity)
    {
        if (!s_pEnvInfoCache)
            s_pEnvInfoCache = new RSS_EnvironmentInfo();

        s_pEnvInfoCache.temperature = 20.0;
        s_pEnvInfoCache.rainIntensity = 0.0;
        s_pEnvInfoCache.windSpeed = 0.0;
        s_pEnvInfoCache.windDirection = 0.0;
        s_pEnvInfoCache.surfaceWetness = 0.0;
        s_pEnvInfoCache.totalWetWeight = 0.0;
        s_pEnvInfoCache.isIndoor = false;
        s_pEnvInfoCache.heatStressMultiplier = 1.0;
        s_pEnvInfoCache.heatStressPenalty = 0.0;
        s_pEnvInfoCache.coldStressPenalty = 0.0;
        s_pEnvInfoCache.isValid = false;

        SCR_CharacterControllerComponent ctrl = GetRssController(entity);
        if (!ctrl)
            return s_pEnvInfoCache;

        EnvironmentFactor env = ctrl.GetRssEnvironmentFactor();
        if (!env)
            return s_pEnvInfoCache;

        s_pEnvInfoCache.temperature = env.GetTemperature();
        s_pEnvInfoCache.rainIntensity = env.GetRainIntensity();
        s_pEnvInfoCache.windSpeed = env.GetWindSpeed();
        s_pEnvInfoCache.windDirection = env.GetWindDirection();
        s_pEnvInfoCache.surfaceWetness = env.GetSurfaceWetness();
        s_pEnvInfoCache.totalWetWeight = env.GetTotalWetWeight();
        s_pEnvInfoCache.isIndoor = env.IsIndoorForEntity(entity);
        s_pEnvInfoCache.heatStressMultiplier = env.GetHeatStressMultiplier();
        s_pEnvInfoCache.heatStressPenalty = env.GetHeatStressPenalty();
        s_pEnvInfoCache.coldStressPenalty = env.GetColdStressPenalty();
        s_pEnvInfoCache.isValid = true;

        return s_pEnvInfoCache;
    }

    // 检查实体是否由 RSS 管理（有 RSS 扩展的控制器且已初始化）
    // @param entity 角色实体
    // @return 是否有效
    static bool IsRssManaged(IEntity entity)
    {
        SCR_CharacterControllerComponent ctrl = GetRssController(entity);
        if (!ctrl)
            return false;
        return ctrl.HasRssData();
    }
}
