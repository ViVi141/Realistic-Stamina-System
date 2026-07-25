// 环境因子模块
// 负责处理环境因素对体力系统的影响（热应激、降雨湿重等）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

enum ERSS_EnvSignal
{
    RAIN_INTENSITY,
    WIND_SPEED,
    TIME_OF_DAY,
    WETNESS,
    COUNT
}

class SCR_RSS_EnvironmentFactor
{
    protected float m_fCachedHeatStressMultiplier = 1.0; // 缓存的热应激倍数
    protected float m_fCachedRainWeight = 0.0; // 缓存的降雨湿重（kg）
    
    // 枚举 ERSS_EnvSignal 提供编译期名称检查，避免信号名拼写错误
    protected static ref GameSignalsManager s_pGlobalSignals;
    protected static int s_iSignalRainIntensity = -1; // ERSS_EnvSignal.RAIN_INTENSITY
    protected static int s_iSignalWindSpeed     = -1; // ERSS_EnvSignal.WIND_SPEED
    protected static int s_iSignalTOD           = -1; // ERSS_EnvSignal.TIME_OF_DAY
    protected static int s_iSignalWetness       = -1; // ERSS_EnvSignal.WETNESS
    
    protected float m_fLastEnvironmentCheckTime = 0.0; // 上次环境检测时间
    protected float m_fRainStopTime = -1.0; // 停止降雨的时间（秒，绝对值，用于线性湿重衰减）
    protected float m_fRainPeakWeight = 0.0; // 停雨瞬间的湿重峰值（kg），供线性衰减基准
    protected TimeAndWeatherManagerEntity m_pCachedWeatherManager; // 缓存的天气管理器引用
    protected float m_fLastRainIntensity = 0.0; // 上次检测到的降雨强度（用于衰减计算）
    protected IEntity m_pCachedOwner; // 缓存的角色实体引用（用于室内检测）

    // 实时变更检测缓存（用于即时响应管理员修改）
    protected float m_fLastKnownTOD = -1.0; // 上次记录的 GetTimeOfTheDay 值
    protected int m_iLastKnownYear = -1;
    protected int m_iLastKnownMonth = -1;
    protected int m_iLastKnownDay = -1;
    protected float m_fLastKnownRainIntensity = -1.0;
    protected float m_fLastKnownWindSpeed = -1.0;
    protected bool m_bLastKnownOverrideTemperature = false;
    protected float m_fLastKnownSunriseHour = -1.0;
    protected float m_fLastKnownSunsetHour = -1.0;

    protected float m_fCachedRainIntensity = 0.0; // 缓存的降雨强度（0.0-1.0）
    protected float m_fCachedWindSpeed = 0.0; // 缓存的风速（m/s）
    protected float m_fCachedWindDirection = 0.0; // 缓存的风向（度）
    protected float m_fCachedWindDrag = 0.0; // 缓存的风阻系数
    protected float m_fCachedMudFactor = 0.0; // 缓存的泥泞度系数（0.0-1.0）
    protected float m_fCachedTemperature = 20.0; // 缓存的当前气温（°C）
    protected float m_fCachedSurfaceWetness = 0.0; // 缓存的地表湿度（0.0-1.0）
    protected float m_fCurrentTotalWetWeight = 0.0; // 当前总湿重（游泳+降雨，kg）
    protected float m_fLastUpdateTime = 0.0; // 上次更新时间（秒）
    protected float m_fRainBreathingPenalty = 0.0; // 暴雨呼吸阻力惩罚
    protected float m_fCachedTerrainFactor = 1.0; // 缓存的地形系数
    protected float m_fMudTerrainFactor = 0.0; // 泥泞地形系数惩罚
    protected float m_fMudSprintPenalty = 0.0; // 泥泞Sprint惩罚
    protected float m_fSlipRisk = 0.0; // 滑倒风险
    protected float m_fHeatStressPenalty = 0.0; // 热应激惩罚
    protected float m_fColdStressPenalty = 0.0; // 冷应激惩罚
    protected float m_fColdStaticPenalty = 0.0; // 冷应激静态惩罚
    protected ref SCR_RSS_IndoorDetection m_pIndoorDetector; // 室内检测模块
    protected float m_fSurfaceWetnessPenalty = 0.0; // 地表湿度惩罚

    // 是否使用引擎天气API（true=使用引擎数据，false=使用虚拟昼夜模型）
    protected bool m_bUseEngineWeather = true; // 默认启用真实天气

    protected float m_fTempUpdateInterval = 5.0; // 温度步进间隔（秒），默认5s（实时每5秒更新）
    protected float m_fLastTemperatureUpdateTime = 0.0; // 上次温度更新时间（秒）
    
    protected float m_fCachedCloudFactor = 0.0;
    protected float m_fLastCloudFactorUpdateTime = -999.0;
    protected const float CLOUD_FACTOR_CACHE_DURATION = 30.0; // 30秒缓存，云量变化以分钟计
    
    // 位置变化触发温度重算（v3.20.0 性能优化）
    // 玩家移动超过阈值时提前触发，静止时回退到时间间隔，减少不必要的重算
    protected vector m_vLastTempCalcPosition = vector.Zero; // 上次温度计算时的位置
    protected bool m_bTempPositionInitialized = false;      // 位置是否已初始化
    protected const float TEMP_RECALC_DISTANCE_SQ = 10000.0; // 100m² → 100m 触发（存储平方值避免 sqrt）
    protected float m_fNextTempStepLogTime = 0.0; // 下次非详细温度步进日志时间（用于 ShouldLog）
    protected bool m_bPendingForceUpdate = false; // 标记是否有管理员触发的即时温度重算请求
    protected float m_fNextForceUpdateLogTime = 0.0; // ForceUpdate 日志节流时间（避免传字面量给 ShouldLog）
    protected float m_fNextLocationEstimateLogTime = 0.0; // 位置估算日志节流（避免直接传入字面量）
    // 是否使用引擎提供的温度（独立于 m_bUseEngineWeather）
    protected bool m_bUseEngineTemperature = false; // 默认不使用引擎温度，完全通过模组内计算
    protected float m_fTemperatureMixingHeight = 1000.0; // 混合层高度（m），用于热容量计算
    protected float m_fAlbedo = 0.2; // 地表反照率（默认：草地/混合地表）
    protected float m_fAerosolOpticalDepth = 0.14; // 简化的大气光学厚度用于透过率估计
    protected float m_fSurfaceEmissivity = 0.98; // 地表发射率
    protected float m_fCachedSurfaceTemperature = 20.0; // 缓存的近地面温度（°C）
    
    // 上次应用的配置版本（用于检测管理员实时修改，触发 ApplySettings）
    protected static string s_lastAppliedConfigVersion = "";

    // 物理模型可调系数（可从 SCR_RSS_Settings 读取）
    protected float m_fCloudBlockingCoeff = 0.7; // 云层遮挡短波的系数（经验）
    protected float m_fLECoef = 200.0; // 潜热系数（W/m2 每单位地表湿度）

    // 通用气温模型参数（正弦波叠加，兼容各模组地图）
    protected float m_fAltitudeMeters = 0.0; // 海拔（m），缺省 0
    protected float m_fFogDensity = 0.0; // 雾/湿度 (0~1)，用于压缩昼夜温差，缺省 0

    // 时区/经度用于太阳时校正
    protected bool m_bUseEngineTimezone = true; // 是否优先使用引擎时区信息
    protected float m_fLongitude = 0.0; // 经度覆盖（度）
    protected float m_fLatitude = 0.0; // 纬度覆盖（度）
    protected float m_fTimeZoneOffsetHours = 0.0; // 时区偏移覆盖（小时）

    protected float m_fSolarConstant = 1361.0; // 太阳常数（W/m^2）
    protected const float STEFAN_BOLTZMANN = 5.670374419e-8; // 斯特藩-玻尔兹曼常数
    protected const float M_E = 2.718281828459045; // 自然常数 e（用于替代 Math.Exp）
    
    
    // 初始化环境因子模块
    void Initialize(World world = null, IEntity owner = null)
    {
        m_fCachedHeatStressMultiplier = 1.0;
        m_fCachedRainWeight = 0.0;
        m_fLastEnvironmentCheckTime = 0.0;
        m_fRainStopTime = -1.0;
        m_fRainPeakWeight = 0.0;
        m_fLastRainIntensity = 0.0;
        m_pCachedWeatherManager = null;
        m_pCachedOwner = owner;
        
        // ==================== 高级环境因子初始化（v2.15.0）====================
        m_fCachedRainIntensity = 0.0;
        m_fCachedWindSpeed = 0.0;
        m_fCachedWindDirection = 0.0;
        m_fCachedWindDrag = 0.0;
        m_fCachedMudFactor = 0.0;
        m_fCachedTemperature = 20.0;
        m_fCachedSurfaceWetness = 0.0;
        m_fCurrentTotalWetWeight = 0.0;
        m_fLastUpdateTime = 0.0;
        m_fRainBreathingPenalty = 0.0;
        m_fCachedTerrainFactor = 1.0;
        m_fMudTerrainFactor = 0.0;
        m_fMudSprintPenalty = 0.0;
        m_fSlipRisk = 0.0;
        m_fHeatStressPenalty = 0.0;
        m_fColdStressPenalty = 0.0;
        m_fColdStaticPenalty = 0.0;

        // 调试：立即打印天气管理器坐标（可能为0）
        if (m_pCachedWeatherManager)
        {
            float dbgLat = m_pCachedWeatherManager.GetCurrentLatitude();
            float dbgLon = m_pCachedWeatherManager.GetCurrentLongitude();
            float dbgTZ  = m_pCachedWeatherManager.GetTimeZoneOffset() + m_pCachedWeatherManager.GetDSTOffset();
            float tmpLogInit = m_fNextLocationEstimateLogTime; // reuse same throttle
            if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLogInit))
            {
                m_fNextLocationEstimateLogTime = tmpLogInit;
                PrintFormat("[RSS] init weather mgr lat=%1 lon=%2 tz=%3", dbgLat, dbgLon, dbgTZ);
            }
        }
        m_fSurfaceWetnessPenalty = 0.0;
        
        if (world)
        {
            ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(world);
            if (chimeraWorld)
                m_pCachedWeatherManager = chimeraWorld.GetTimeAndWeatherManager();
        }

        // perf: 全局信号索引（静态，所有实例共享，仅首次注册）
        if (!s_pGlobalSignals)
        {
            s_pGlobalSignals = GetGame().GetSignalsManager();
            if (s_pGlobalSignals)
            {
                s_iSignalRainIntensity = s_pGlobalSignals.AddOrFindSignal("RainIntensity");
                s_iSignalWindSpeed     = s_pGlobalSignals.AddOrFindSignal("WindSpeed");
                s_iSignalTOD           = s_pGlobalSignals.AddOrFindSignal("TimeOfDay");
                s_iSignalWetness       = s_pGlobalSignals.AddOrFindSignal("Wetness");
            }
        }

        // perf: 缓存纬度（地图常数，初始化后永不变化）
        if (m_pCachedWeatherManager)
        {
            float engLat = m_pCachedWeatherManager.GetCurrentLatitude();
            if (engLat != 0.0)
                m_fLatitude = engLat;
        }

        // 初始化实时检测缓存（若引擎可用）
        if (m_pCachedWeatherManager)
        {
            m_fLastKnownTOD = ReadSignalTOD();
            int y, mo, d;
            m_pCachedWeatherManager.GetDate(y, mo, d);
            m_iLastKnownYear = y;
            m_iLastKnownMonth = mo;
            m_iLastKnownDay = d;
            m_fLastKnownRainIntensity = ReadSignalRainIntensity();
            m_fLastKnownWindSpeed = ReadSignalWindSpeed();
            m_bLastKnownOverrideTemperature = m_pCachedWeatherManager.GetOverrideTemperature();
            float sr = 0.0;
            float ss = 0.0;
            if (m_pCachedWeatherManager.GetSunriseHour(sr))
                m_fLastKnownSunriseHour = sr;
            if (m_pCachedWeatherManager.GetSunsetHour(ss))
                m_fLastKnownSunsetHour = ss;
        }
        // 初始化 pending 标志
        m_bPendingForceUpdate = false;
        m_fNextForceUpdateLogTime = 0.0;

        // 读取配置并应用到模型参数
        ApplySettings();

        // 调试：打印天气管理器状态
        SCR_RSS_EnvironmentDebug.LogInitWeatherDebug(
            m_pCachedWeatherManager,
            m_bUseEngineTemperature,
            m_bUseEngineTimezone,
            m_fLongitude,
            m_fTimeZoneOffsetHours);

        float latBoot = m_fLatitude;
        float lonBoot = m_fLongitude;
        float nextLocLog = m_fNextLocationEstimateLogTime;
        SCR_RSS_EnvLocationBootstrap.TryEstimateOnInit(
            m_pCachedWeatherManager,
            m_fTimeZoneOffsetHours,
            m_fCachedRainIntensity,
            m_fCachedSurfaceWetness,
            latBoot,
            lonBoot,
            nextLocLog);
        m_fLatitude = latBoot;
        m_fLongitude = lonBoot;
        m_fNextLocationEstimateLogTime = nextLocLog;

        // 初始化室内检测模块（始终创建，不依赖 debug 开关）
        m_pIndoorDetector = new SCR_RSS_IndoorDetection();

        // perf: 随机偏移环境检测相位，避免多实体在同一帧集中触发
        m_fLastEnvironmentCheckTime = m_fLastEnvironmentCheckTime
            + Math.RandomFloat(0.0, SCR_RSS_EnvConstants.ENV_CHECK_INTERVAL);
    }

    void SetIndoorDebug(bool enabled)
    {
        if (m_pIndoorDetector)
            m_pIndoorDetector.SetIndoorDebug(enabled);
    }

    bool GetIndoorDebug()
    {
        if (m_pIndoorDetector)
            return m_pIndoorDetector.GetIndoorDebug();
        return false;
    }

    void SetUseEngineWeather(bool enabled)
    {
        m_bUseEngineWeather = enabled;
    }

    bool GetUseEngineWeather()
    {
        return m_bUseEngineWeather;
    }

    // 控制是否使用引擎温度（仅温度，非整体天气数据）
    void SetUseEngineTemperature(bool enabled)
    {
        m_bUseEngineTemperature = enabled;
    }

    bool GetUseEngineTemperature()
    {
        return m_bUseEngineTemperature;
    }

    // 封装的 setter：标记需要立即重算温度（用于响应管理员实时更改）
    protected void MarkPendingForceUpdate()
    {
        // 标记待处理（在安全位置执行重算），并使用节流变量记录日志
        m_bPendingForceUpdate = true;
        // ShouldLog 参数为 inout，不能直接传递类成员（写保护）。使用临时变量并回写。
        float tmpLogTime = m_fNextForceUpdateLogTime;
        if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLogTime))
        {
            m_fNextForceUpdateLogTime = tmpLogTime;
            PrintFormat("[RSS] ForceUpdate: Pending recompute flagged");
        }
    }


    
    // 更新环境因子（协调方法）
    bool UpdateEnvironmentFactors(float currentTime, IEntity owner = null, vector playerVelocity = vector.Zero, float terrainFactor = 1.0, float swimmingWetWeight = 0.0)
    {
        // 防御性编程：如果天气管理器丢失，尝试重新获取
        if (!m_pCachedWeatherManager)
        {
            World world = GetGame().GetWorld();
            if (world)
            {
                ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(world);
                if (chimeraWorld)
                    m_pCachedWeatherManager = chimeraWorld.GetTimeAndWeatherManager();
            }
        }

        // 调试：零坐标节流
        if (m_pCachedWeatherManager)
        {
            float dbgLat = m_pCachedWeatherManager.GetCurrentLatitude();
            float dbgLon = m_pCachedWeatherManager.GetCurrentLongitude();
            float tmpCoordLog = m_fNextLocationEstimateLogTime;
            SCR_RSS_WeatherChangeDetector.LogZeroCoordinateThrottle(tmpCoordLog, dbgLat, dbgLon);
            m_fNextLocationEstimateLogTime = tmpCoordLog;
        }
        
        // 更新缓存的角色实体引用（用于室内检测）
        if (owner)
            m_pCachedOwner = owner;

        // 按间隔更新室内状态缓存 — 委托给 m_pIndoorDetector
        if (m_pIndoorDetector)
            m_pIndoorDetector.UpdateIndoorCache(m_pCachedOwner, currentTime);
        
        bool forceUpdate = false;
        if (m_pCachedWeatherManager)
        {
            float currTOD = ReadSignalTOD();
            float currRain = ReadSignalRainIntensity();
            float currWind = ReadSignalWindSpeed();
            RSS_WeatherSnapshot snap = SCR_RSS_WeatherChangeDetector.Sample(
                m_pCachedWeatherManager, currTOD, currRain, currWind);
            SCR_RSS_WeatherChangeDetector.LogEngineWeatherBatch(snap);
            forceUpdate = SCR_RSS_WeatherChangeDetector.NeedsForceUpdate(
                snap,
                m_fLastKnownTOD,
                m_iLastKnownYear,
                m_iLastKnownMonth,
                m_iLastKnownDay,
                m_fLastKnownRainIntensity,
                m_fLastKnownWindSpeed,
                m_bLastKnownOverrideTemperature,
                m_fLastKnownSunriseHour,
                m_fLastKnownSunsetHour);
        }

        // CRITICAL FIX: Check if RSS config has been reloaded (admin changed settings).
        // ApplySettings() reads temperature/physics coefficients from SCR_RSS_Settings;
        // without this periodic check, admin changes only take effect on entity re-initialization.
        SCR_RSS_Settings rssSettings = SCR_RSS_ConfigManager.GetSettings();
        if (rssSettings)
        {
            string currentCfgVer = rssSettings.m_sConfigVersion;
            if (currentCfgVer != s_lastAppliedConfigVersion)
            {
                ApplySettings();
                s_lastAppliedConfigVersion = currentCfgVer;
            }
        }

        if (!forceUpdate && (currentTime - m_fLastEnvironmentCheckTime < SCR_RSS_EnvConstants.ENV_CHECK_INTERVAL))
            return false;

        // 标记为已检查时间
        m_fLastEnvironmentCheckTime = currentTime;
        
        int stance = 0; // 默认：站立
        if (owner)
        {
            SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
            if (controller)
            {
                ECharacterStance currentStance = controller.GetStance();
                switch (currentStance)
                {
                    case ECharacterStance.STAND:
                        stance = 0;
                        break;
                    case ECharacterStance.CROUCH:
                        stance = 1;
                        break;
                    case ECharacterStance.PRONE:
                        stance = 2;
                        break;
                    default:
                        stance = 0;
                        break;
                }
            }
        }
        
        // 保存地形系数（用于泥泞计算）
        m_fCachedTerrainFactor = terrainFactor;
        
        // ==================== 更新高级环境因子（v2.14.0）====================
        UpdateAdvancedEnvironmentFactors(currentTime, owner, playerVelocity, stance);
        
        // 更新热应激倍数（考虑室内豁免）
        float oldHeatStress = m_fCachedHeatStressMultiplier;
        m_fCachedHeatStressMultiplier = CalculateHeatStressMultiplier(m_pCachedOwner);
        
        // 注意：降雨湿重已在 UpdateAdvancedEnvironmentFactors 中的 CalculateRainWetWeight 方法中计算
        // 不需要在这里再次调用 CalculateRainWeight
        
        // 更新总湿重（游泳湿重 + 降雨湿重，限制在最大值）
        // 使用 SCR_RSS_SwimmingStateManager 的方法计算总湿重
        m_fCurrentTotalWetWeight = SCR_RSS_SwimmingStateManager.CalculateTotalWetWeight(swimmingWetWeight, m_fCachedRainWeight);

        // 若检测到管理员即时修改（forceUpdate），通过封装方法标记为待处理（避免在此处直接写入）
        if (forceUpdate)
        {
            MarkPendingForceUpdate();
        }

        if (m_pCachedWeatherManager)
        {
            RSS_WeatherSnapshot syncSnap = SCR_RSS_WeatherChangeDetector.Sample(
                m_pCachedWeatherManager,
                ReadSignalTOD(),
                ReadSignalRainIntensity(),
                ReadSignalWindSpeed());
            float lastTOD = m_fLastKnownTOD;
            int lastY = m_iLastKnownYear;
            int lastMo = m_iLastKnownMonth;
            int lastD = m_iLastKnownDay;
            float lastRain = m_fLastKnownRainIntensity;
            float lastWind = m_fLastKnownWindSpeed;
            bool lastOverride = m_bLastKnownOverrideTemperature;
            float lastSunrise = m_fLastKnownSunriseHour;
            float lastSunset = m_fLastKnownSunsetHour;
            SCR_RSS_EnvPendingUpdate.SyncWeatherCacheFromSnapshot(
                syncSnap,
                lastTOD, lastY, lastMo, lastD,
                lastRain, lastWind, lastOverride,
                lastSunrise, lastSunset);
            m_fLastKnownTOD = lastTOD;
            m_iLastKnownYear = lastY;
            m_iLastKnownMonth = lastMo;
            m_iLastKnownDay = lastD;
            m_fLastKnownRainIntensity = lastRain;
            m_fLastKnownWindSpeed = lastWind;
            m_bLastKnownOverrideTemperature = lastOverride;
            m_fLastKnownSunriseHour = lastSunrise;
            m_fLastKnownSunsetHour = lastSunset;
        }

        bool pendingForce = m_bPendingForceUpdate;
        float surfaceTemp = m_fCachedSurfaceTemperature;
        float cachedTemp = m_fCachedTemperature;
        float nextForceLog = m_fNextForceUpdateLogTime;
        SCR_RSS_EnvPendingUpdate.ApplyPendingForceTemperature(
            m_pCachedWeatherManager,
            m_fLatitude,
            m_fFogDensity,
            ReadSignalTOD(),
            GetCloudFactorCached(),
            ReadSignalRainIntensity(),
            GetCurrentAltitudeMeters(owner),
            pendingForce,
            surfaceTemp,
            cachedTemp,
            nextForceLog);
        m_bPendingForceUpdate = pendingForce;
        m_fCachedSurfaceTemperature = surfaceTemp;
        m_fCachedTemperature = cachedTemp;
        m_fNextForceUpdateLogTime = nextForceLog;

        static float nextEnvLogTime = 0.0;
        SCR_RSS_EnvPendingUpdate.LogEnvironmentFactorsThrottle(
            nextEnvLogTime,
            m_fCachedTemperature,
            m_fCachedHeatStressMultiplier,
            m_fCachedRainWeight,
            m_fCurrentTotalWetWeight,
            m_fCachedWindSpeed);
        
        return true;
    }
    
    float GetHeatStressMultiplier()
    {
        return m_fCachedHeatStressMultiplier;
    }
    
    float GetRainWeight()
    {
        return m_fCachedRainWeight;
    }
    
    
    float GetRainIntensity()
    {
        return m_fCachedRainIntensity;
    }
    
    float GetWindSpeed()
    {
        return m_fCachedWindSpeed;
    }
    
    float GetWindDirection()
    {
        return m_fCachedWindDirection;
    }
    
    float GetWindDrag()
    {
        return m_fCachedWindDrag;
    }
    
    float GetMudFactor()
    {
        return m_fCachedMudFactor;
    }
    
    float GetTemperature()
    {
        return m_fCachedTemperature;
    }
    
    float GetSurfaceWetness()
    {
        return m_fCachedSurfaceWetness;
    }
    
    float GetTotalWetWeight()
    {
        return m_fCurrentTotalWetWeight;
    }
    
    float GetRainBreathingPenalty()
    {
        return m_fRainBreathingPenalty;
    }
    
    float GetMudTerrainFactor()
    {
        return m_fMudTerrainFactor;
    }
    
    float GetMudSprintPenalty()
    {
        return m_fMudSprintPenalty;
    }
    
    float GetSlipRisk()
    {
        return m_fSlipRisk;
    }

    // 最近一次 UpdateEnvironmentFactors 传入的地形系数（用于滑倒 ACOF 等）
    float GetCachedTerrainFactor()
    {
        return m_fCachedTerrainFactor;
    }
    
    float GetHeatStressPenalty()
    {
        return m_fHeatStressPenalty;
    }
    
    float GetColdStressPenalty()
    {
        return m_fColdStressPenalty;
    }
    
    float GetColdStaticPenalty()
    {
        return m_fColdStaticPenalty;
    }
    
    float GetSurfaceWetnessPenalty()
    {
        return m_fSurfaceWetnessPenalty;
    }

    // ── 轻量级快速消耗倍数（v3.20.0 性能优化）────────────────────────────
    // 仅读取已缓存的环境值，不触发任何重新计算，供体力消耗快路径使用。
    // 近似完整路径：(terrainFactor + mudPenaltyMax·mudFactor) × (1 + windDrag) × heatMultiplier
    // 快路径系数与 ENV_MUD_PENALTY_MAX / ENV_WIND_RESISTANCE_COEFF 对齐
    float GetQuickEnvironmentMultiplier()
    {
        float mult = 1.0;

        if (SCR_RSS_ConfigBridge.IsMudPenaltyEnabled())
            mult = 1.0 + m_fCachedMudFactor * SCR_RSS_EnvConstants.ENV_MUD_PENALTY_MAX;

        if (SCR_RSS_ConfigBridge.IsWindResistanceEnabled() && m_fCachedWindDrag > 0.0)
            mult = mult * (1.0 + m_fCachedWindDrag);

        if (SCR_RSS_ConfigBridge.IsHeatStressEnabled() && m_fHeatStressPenalty > 0.05)
            mult = mult * (1.0 + m_fHeatStressPenalty * 0.5);

        return mult;
    }

    
    // ── 信号读取辅助（perf: 静态 GlobalSignalsManager 内存读取，回退到 C++ 桥接）──
    // 使用 ERSS_EnvSignal 枚举命名索引，编译期可检查信号名一致性
    protected float ReadSignalRainIntensity()
    {
        if (s_pGlobalSignals && s_iSignalRainIntensity >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalRainIntensity);
        if (m_pCachedWeatherManager)
            return m_pCachedWeatherManager.GetRainIntensity();
        return 0.0;
    }
    
    protected float ReadSignalWindSpeed()
    {
        if (s_pGlobalSignals && s_iSignalWindSpeed >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalWindSpeed);
        if (m_pCachedWeatherManager)
            return m_pCachedWeatherManager.GetWindSpeed();
        return 0.0;
    }
    
    protected float ReadSignalTOD()
    {
        if (s_pGlobalSignals && s_iSignalTOD >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalTOD);
        if (m_pCachedWeatherManager)
            return m_pCachedWeatherManager.GetTimeOfTheDay();
        return 12.0;
    }
    
    protected float ReadSignalWetness()
    {
        if (s_pGlobalSignals && s_iSignalWetness >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalWetness);
        if (m_pCachedWeatherManager)
            return m_pCachedWeatherManager.GetCurrentWetness();
        return 0.0;
    }

    // 查询当前海拔（米）：有 owner 时用 SCR_TerrainHelper.GetTerrainY，否则用配置 m_fAltitudeMeters
    protected float GetCurrentAltitudeMeters(IEntity owner)
    {
        if (owner)
        {
            World world = owner.GetWorld();
            if (world)
            {
                vector pos = owner.GetOrigin();
                return SCR_TerrainHelper.GetTerrainY(pos, world, false, null);
            }
        }
        return m_fAltitudeMeters;
    }

    // 正弦波叠加气温模型 — 直接委托 SCR_RSS_AstronomyMath（见 UpdateAdvancedEnvironmentFactors）

    protected float EstimateLatLongFromSunriseSunset(out float outLatDeg, out float outLonDeg)
    {
        float lat = m_fLatitude;
        float lon = m_fLongitude;
        float nextLog = m_fNextLocationEstimateLogTime;
        float conf = SCR_RSS_EnvLocationBootstrap.ApplySunriseSunsetEstimate(
            m_pCachedWeatherManager,
            m_fTimeZoneOffsetHours,
            m_fCachedRainIntensity,
            m_fCachedSurfaceWetness,
            lat, lon, nextLog,
            outLatDeg, outLonDeg);
        m_fLatitude = lat;
        m_fLongitude = lon;
        m_fNextLocationEstimateLogTime = nextLog;
        return conf;
    }

    protected float EstimateLatLongFromAstronomicalSearch(out float outLatDeg, out float outLonDeg)
    {
        float lat = m_fLatitude;
        float lon = m_fLongitude;
        float nextLog = m_fNextLocationEstimateLogTime;
        float conf = SCR_RSS_EnvLocationBootstrap.ApplyAstronomicalSearchEstimate(
            m_pCachedWeatherManager,
            m_fTimeZoneOffsetHours,
            m_fCachedRainIntensity,
            m_fCachedSurfaceWetness,
            lat, lon, nextLog,
            outLatDeg, outLonDeg);
        m_fLatitude = lat;
        m_fLongitude = lon;
        m_fNextLocationEstimateLogTime = nextLog;
        return conf;
    }

    // 简单云因子推断 — 委托给 SCR_RSS_AstronomyMath
    protected float InferCloudFactor()
    {
        return SCR_RSS_AstronomyMath.InferCloudFactor(m_fCachedRainIntensity, m_fCachedSurfaceWetness, m_pCachedWeatherManager);
    }

    // 云因子缓存包装（perf: 30s TTL，避免每5s做字符串匹配 + C++桥接）
    protected float GetCloudFactorCached()
    {
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
        if (currentTime - m_fLastCloudFactorUpdateTime > CLOUD_FACTOR_CACHE_DURATION)
        {
            m_fCachedCloudFactor = InferCloudFactor();
            m_fLastCloudFactorUpdateTime = currentTime;
        }
        return m_fCachedCloudFactor;
    }

    // 热应激模型：基于虚拟气温阈值，而非固定时间段
    // 只有当虚拟气温超过 26°C 时，才开始计算热应激
    // 如果角色在室内（头顶有遮挡），热应激减少 50%
    protected float CalculateHeatStressMultiplier(IEntity owner = null)
    {
        if (!m_pCachedWeatherManager)
            return 1.0;

        bool indoor = false;
        if (owner && IsUnderCover(owner))
            indoor = true;

        return SCR_RSS_PenaltyMath.CalculateHeatStressMultiplier(GetTemperature(), indoor);
    }
    
    protected bool EvaluateRoofedBuildingInterior(IEntity owner, float roofCheckHeightM, bool requireHorizontalEnclosure)
    {
        if (m_pIndoorDetector)
            return m_pIndoorDetector.EvaluateRoofedBuildingInterior(owner, roofCheckHeightM, requireHorizontalEnclosure);
        return false;
    }


    protected bool IsUnderCover(IEntity owner)
    {
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsUnderCover(owner);
        return false;
    }

    // 优先尝试使用 GetRainIntensity() API，如果没有则回退到字符串匹配
    // 停止降雨后，湿重使用二次方衰减（更自然的蒸发过程）
    
    void ForceUpdate(float currentTime, IEntity owner = null, float swimmingWetWeight = 0.0)
    {
        m_fLastEnvironmentCheckTime = 0.0; // 重置时间，强制更新
        UpdateEnvironmentFactors(currentTime, owner, vector.Zero, 1.0, swimmingWetWeight);
    }
    
    void SetOwner(IEntity owner)
    {
        m_pCachedOwner = owner;
    }
    
    TimeAndWeatherManagerEntity GetWeatherManager()
    {
        return m_pCachedWeatherManager;
    }
    
    void SetWeatherManager(TimeAndWeatherManagerEntity weatherManager)
    {
        m_pCachedWeatherManager = weatherManager;
    }
    
    
    float GetCurrentHour()
    {
        if (!m_pCachedWeatherManager)
            return -1.0;
        
        return m_pCachedWeatherManager.GetTimeOfTheDay();
    }
    
    bool IsIndoor()
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsIndoor(m_pCachedOwner);
        return false;
    }

    bool IsRoofedBuildingVolumeForEntity(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsRoofedBuildingVolumeForEntity(owner);
        return false;
    }

    bool ShouldSuppressTerrainSlopeForEntity(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.ShouldSuppressTerrainSlopeForEntity(owner);
        return false;
    }

    bool IsIndoorForEntity(IEntity owner)
    {
        if (!SCR_RSS_ConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsIndoorForEntity(owner);
        return false;
    }

    // 应用来自 `SCR_RSS_Settings` 的配置到模型（可在初始化时调用）
    protected void ApplySettings()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;

        m_fTempUpdateInterval = settings.m_fTempUpdateInterval;
        m_fTemperatureMixingHeight = settings.m_fTemperatureMixingHeight;
        m_fAlbedo = settings.m_fAlbedo;
        m_fAerosolOpticalDepth = settings.m_fAerosolOpticalDepth;
        m_fSurfaceEmissivity = settings.m_fSurfaceEmissivity;
        m_fCloudBlockingCoeff = settings.m_fCloudBlockingCoeff;
        m_fLECoef = settings.m_fLECoef;
        m_bUseEngineTemperature = settings.m_bUseEngineTemperature;
        m_bUseEngineTimezone = settings.m_bUseEngineTimezone;
        m_fLongitude = settings.m_fLongitude;
        m_fTimeZoneOffsetHours = settings.m_fTimeZoneOffsetHours;
        m_fAltitudeMeters = settings.m_fAltitudeMeters;
        m_fFogDensity = settings.m_fFogDensity;

        // 若引擎可用且启用了引擎时区模式，直接从地图配置读取经度（World Editor 中 Geographic Coords 提供）
        if (m_bUseEngineTimezone && m_pCachedWeatherManager)
            m_fLongitude = m_pCachedWeatherManager.GetCurrentLongitude();
    }

    // 外部调用：在配置发生改变时调用以立即应用新设置
    void OnConfigUpdated()
    {
        ApplySettings();
    }
    
    // 引擎在 rain<0.15 时通常不显示雨滴，0.1 多为阴天/潮湿无可见雨
    bool IsRaining()
    {
        return m_fCachedRainIntensity >= SCR_RSS_EnvConstants.ENV_RAIN_VISUAL_EFFECT_THRESHOLD;
    }
    
    
    void UpdateAdvancedEnvironmentFactors(float currentTime, IEntity owner, vector playerVelocity = vector.Zero, int stance = 0)
    {
        if (!m_pCachedWeatherManager)
            return;
        
        float deltaTime = currentTime - m_fLastUpdateTime;
        
        // 1. 获取降雨强度（优先使用API，失败则回退到字符串匹配）
        m_fCachedRainIntensity = CalculateRainIntensityFromAPI();
        
        // 2. 获取风速和风向
        m_fCachedWindSpeed = CalculateWindSpeedFromAPI();
        m_fCachedWindDirection = CalculateWindDirectionFromAPI();
        
        // 3. 计算风阻系数（基于玩家移动方向）
        m_fCachedWindDrag = CalculateWindDrag(playerVelocity);
        if (!SCR_RSS_ConfigBridge.IsWindResistanceEnabled())
            m_fCachedWindDrag = 0.0;
        
        // 4. 获取泥泞度系数
        m_fCachedMudFactor = CalculateMudFactorFromAPI();
        if (!SCR_RSS_ConfigBridge.IsMudPenaltyEnabled())
            m_fCachedMudFactor = 0.0;
        
        // 5. 气温：通用经验模型（纬度+季节+海拔+昼夜+天气）
        if (m_pCachedWeatherManager)
        {
            float lat = m_fLatitude;
            int year, month, day;
            m_pCachedWeatherManager.GetDate(year, month, day);
            int n = SCR_RSS_AstronomyMath.DayOfYear(year, month, day);
            float tod = ReadSignalTOD();
            float cloud = GetCloudFactorCached();
            float rain = ReadSignalRainIntensity();
            float altM = GetCurrentAltitudeMeters(owner);

            float surfaceTemp = m_fCachedSurfaceTemperature;
            float lastTempTime = m_fLastTemperatureUpdateTime;
            float nextTempLog = m_fNextTempStepLogTime;
            vector lastTempPos = m_vLastTempCalcPosition;
            bool tempPosInit = m_bTempPositionInitialized;

            SCR_RSS_TemperatureSampler.MaybeUpdateUniversalTemperature(
                currentTime,
                m_fTempUpdateInterval,
                lat,
                n,
                tod,
                altM,
                cloud,
                rain,
                m_fFogDensity,
                owner,
                TEMP_RECALC_DISTANCE_SQ,
                surfaceTemp,
                lastTempTime,
                nextTempLog,
                lastTempPos,
                tempPosInit);

            m_fCachedSurfaceTemperature = surfaceTemp;
            m_fLastTemperatureUpdateTime = lastTempTime;
            m_fNextTempStepLogTime = nextTempLog;
            m_vLastTempCalcPosition = lastTempPos;
            m_bTempPositionInitialized = tempPosInit;
            m_fCachedTemperature = m_fCachedSurfaceTemperature;
        }
        
        // 6. 获取地表湿度
        m_fCachedSurfaceWetness = CalculateSurfaceWetnessFromAPI();
        
        // 7-14. 湿重与各惩罚项
        CalculateRainWetWeight(currentTime);
        CalculateRainBreathingPenalty();
        CalculateMudTerrainFactor();
        CalculateMudSprintPenalty();
        CalculateSlipRisk();
        CalculateHeatStressPenalty();
        CalculateColdStressPenalty();
        CalculateSurfaceWetnessPenalty(owner, stance);
        
        // ==================== 调试信息：高级环境因子（v2.14.0）====================
        static float nextAdvancedEnvLogTime = 0.0;
        if (SCR_RSS_DebugBatchManager.ShouldVerboseLog(nextAdvancedEnvLogTime))
        {
            RSS_EnvironmentDebugSnapshot snap = new RSS_EnvironmentDebugSnapshot();
            snap.m_fRainIntensity = m_fCachedRainIntensity;
            snap.m_fWindSpeed = m_fCachedWindSpeed;
            snap.m_fWindDirection = m_fCachedWindDirection;
            snap.m_fWindDrag = m_fCachedWindDrag;
            snap.m_fMudFactor = m_fCachedMudFactor;
            snap.m_fTemperature = m_fCachedTemperature;
            snap.m_fSurfaceWetness = m_fCachedSurfaceWetness;
            snap.m_fRainWeight = m_fCachedRainWeight;
            snap.m_fRainBreathingPenalty = m_fRainBreathingPenalty;
            snap.m_fMudTerrainFactor = m_fMudTerrainFactor;
            snap.m_fMudSprintPenalty = m_fMudSprintPenalty;
            snap.m_fSlipRisk = m_fSlipRisk;
            snap.m_fHeatStressPenalty = m_fHeatStressPenalty;
            snap.m_fColdStressPenalty = m_fColdStressPenalty;
            snap.m_fColdStaticPenalty = m_fColdStaticPenalty;
            snap.m_fSurfaceWetnessPenalty = m_fSurfaceWetnessPenalty;
            SCR_RSS_EnvironmentDebug.LogAdvancedFactors(snap, m_bUseEngineWeather, m_pCachedWeatherManager);
        }
        
        // 更新时间戳（在所有计算完成后）
        m_fLastUpdateTime = currentTime;
    }
    
    protected float CalculateRainIntensityFromAPI()
    {
        float rainIntensity = ReadSignalRainIntensity();
        if (rainIntensity > SCR_RSS_EnvConstants.ENV_RAIN_INTENSITY_THRESHOLD)
            return rainIntensity;
        return SCR_RSS_WeatherApi.CalculateRainIntensityFromStateName(m_pCachedWeatherManager);
    }
    
    protected float CalculateWindSpeedFromAPI()
    {
        return ReadSignalWindSpeed();
    }
    
    protected float CalculateWindDirectionFromAPI()
    {
        return SCR_RSS_WeatherApi.CalculateWindDirectionFromAPI(m_pCachedWeatherManager);
    }
    
    protected float CalculateWindDrag(vector playerVelocity)
    {
        return SCR_RSS_WeatherApi.CalculateWindDrag(m_fCachedWindSpeed, m_fCachedWindDirection, playerVelocity);
    }
    
    protected float CalculateMudFactorFromAPI()
    {
        return SCR_RSS_WeatherApi.CalculateMudFactorFromAPI(m_pCachedWeatherManager);
    }
    

    protected float CalculateSurfaceWetnessFromAPI()
    {
        return ReadSignalWetness();
    }
    
    // 降雨中：按强度非线性累积；停雨后：60秒线性衰减至0
    protected void CalculateRainWetWeight(float currentTime)
    {
        float rainWeight = m_fCachedRainWeight;
        float rainStopTime = m_fRainStopTime;
        float rainPeakWeight = m_fRainPeakWeight;
        float lastRainIntensity = m_fLastRainIntensity;
        bool isIndoor = IsUnderCover(m_pCachedOwner);

        SCR_RSS_RainWetWeight.Update(
            currentTime,
            m_fLastUpdateTime,
            m_fCachedRainIntensity,
            isIndoor,
            rainWeight,
            rainStopTime,
            rainPeakWeight,
            lastRainIntensity);

        m_fCachedRainWeight = rainWeight;
        m_fRainStopTime = rainStopTime;
        m_fRainPeakWeight = rainPeakWeight;
        m_fLastRainIntensity = lastRainIntensity;
    }
    
    protected void CalculateRainBreathingPenalty()
    {
        if (!SCR_RSS_ConfigBridge.IsRainWeightEnabled())
        {
            m_fRainBreathingPenalty = 0.0;
            return;
        }
        m_fRainBreathingPenalty = SCR_RSS_PenaltyMath.CalculateRainBreathingPenalty(m_fCachedRainIntensity);
    }
    
    protected void CalculateMudTerrainFactor()
    {
        m_fMudTerrainFactor = SCR_RSS_PenaltyMath.CalculateMudTerrainFactor(m_fCachedTerrainFactor, m_fCachedMudFactor);
    }
    
    protected void CalculateMudSprintPenalty()
    {
        m_fMudSprintPenalty = SCR_RSS_PenaltyMath.CalculateMudSprintPenalty(m_fCachedMudFactor);
    }
    
    protected void CalculateSlipRisk()
    {
        m_fSlipRisk = SCR_RSS_PenaltyMath.CalculateSlipRisk(m_fCachedMudFactor);
    }
    
    protected void CalculateHeatStressPenalty()
    {
        if (!SCR_RSS_ConfigBridge.IsHeatStressEnabled())
        {
            m_fHeatStressPenalty = 0.0;
            return;
        }
        m_fHeatStressPenalty = SCR_RSS_PenaltyMath.CalculateHeatStressPenalty(m_fCachedTemperature);
    }

    // 根据当前环境温度和风速对机械能耗进行补偿
    float AdjustEnergyForTemperature(float basePower)
    {
        return SCR_RSS_PenaltyMath.AdjustEnergyForTemperature(basePower, m_fCachedTemperature, m_fCachedWindSpeed);
    }
    
    protected void CalculateColdStressPenalty()
    {
        SCR_RSS_PenaltyMath.CalculateColdStressPenalty(m_fCachedTemperature, m_fColdStressPenalty, m_fColdStaticPenalty);
    }
    
    protected void CalculateSurfaceWetnessPenalty(IEntity owner, int stance = 0)
    {
        if (!owner)
            return;

        m_fSurfaceWetnessPenalty = SCR_RSS_PenaltyMath.CalculateSurfaceWetnessPenalty(m_fCachedSurfaceWetness, stance);
    }

    //! 清除本实例中所有指向上一世界引擎对象的引用。
    //! Workbench 重载世界时，SCR_CharacterControllerComponent.OnInit 的 guard 跳过重初始化，
    //! 但本实例内部的 m_pCachedWeatherManager 仍指向已销毁的 TimeAndWeatherManagerEntity。
    //! 后续 UpdateEnvironmentFactors 中 ReadSignal* 的回退分支访问该悬空指针导致 Access violation。
    //! 调用此方法后，所有 ReadSignal* 回退分支安全返回默认值，且 UpdateEnvironmentFactors
    //! 顶部的「!m_pCachedWeatherManager → 重新获取」逻辑将被触发，重新绑定新世界的天气管理器。
    void ClearStaleReferences()
    {
        // 核心修复：清空悬空的天气管理器引用
        m_pCachedWeatherManager = null;

        // 清空陈旧的实体引用（下一帧 UpdateEnvironmentFactors 会通过 owner 参数重新设置）
        m_pCachedOwner = null;

        // 重置时间缓存，迫使所有环境因子在下次更新时从新世界重新采样
        m_fLastEnvironmentCheckTime = 0.0;
        m_fLastUpdateTime = 0.0;
        m_fLastTemperatureUpdateTime = 0.0;
        m_fLastCloudFactorUpdateTime = -999.0;
        m_bTempPositionInitialized = false;

        // 重置实时变更检测缓存，避免与旧世界的缓存值进行差异比较
        m_fLastKnownTOD = -1.0;
        m_iLastKnownYear = -1;
        m_iLastKnownMonth = -1;
        m_iLastKnownDay = -1;
        m_fLastKnownRainIntensity = -1.0;
        m_fLastKnownWindSpeed = -1.0;
        m_bLastKnownOverrideTemperature = false;
        m_fLastKnownSunriseHour = -1.0;
        m_fLastKnownSunsetHour = -1.0;
    }

    //! 新 GameMode / 世界开始时调用（由 SCR_BaseGameMode::OnGameStart 触发）。
    //! Workbench「重载脚本 + 重载世界」后，静态 s_pGlobalSignals 可能仍指向已销毁的管理器，
    //! 继续 GetSignalValue 会 Access violation。清空后由下一次 Initialize 重新绑定当前 GetGame。
    static void ResetGlobalSignalsCache()
    {
        s_pGlobalSignals = null;
        s_iSignalRainIntensity = -1;
        s_iSignalWindSpeed = -1;
        s_iSignalTOD = -1;
        s_iSignalWetness = -1;
    }
}
