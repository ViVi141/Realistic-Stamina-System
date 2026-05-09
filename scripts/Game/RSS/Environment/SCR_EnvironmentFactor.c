// 环境因子模块
// 负责处理环境因素对体力系统的影响（热应激、降雨湿重等）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class EnvironmentFactor
{
    // ==================== 状态变量 ====================
    protected float m_fCachedHeatStressMultiplier = 1.0; // 缓存的热应激倍数
    protected float m_fCachedRainWeight = 0.0; // 缓存的降雨湿重（kg）
    
    // ====== 引擎 GlobalSignalsManager 信号索引（perf: 替代 C++ 桥接调用）======
    // 改为静态：所有 EnvironmentFactor 实例共享同一组信号索引，避免每实体 AddOrFindSignal
    protected static ref GameSignalsManager s_pGlobalSignals;
    protected static int s_iSignalRainIntensity = -1;
    protected static int s_iSignalWindSpeed     = -1;
    protected static int s_iSignalTOD           = -1; // "TimeOfDay"
    protected static int s_iSignalWetness       = -1;
    
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

    // ==================== 高级环境因子状态变量（v2.15.0）====================
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
    // ==================== 室内检测相关变量 ====================
    protected ref SCR_EnvironmentIndoorDetection m_pIndoorDetector; // 室内检测模块
    protected float m_fSurfaceWetnessPenalty = 0.0; // 地表湿度惩罚

    // 是否使用引擎天气API（true=使用引擎数据，false=使用虚拟昼夜模型）
    protected bool m_bUseEngineWeather = true; // 默认启用真实天气

    // ====== 温度计算相关参数（P1 实现） ======
    protected float m_fTempUpdateInterval = 5.0; // 温度步进间隔（秒），默认5s（实时每5秒更新）
    protected float m_fLastTemperatureUpdateTime = 0.0; // 上次温度更新时间（秒）
    
    // ====== 云因子缓存（perf: 避免每5s做字符串匹配的 InferCloudFactor）======
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
    // ==========================================
    
    // ==================== 公共方法 ====================
    
    // 初始化环境因子模块
    // @param world 世界对象（用于获取天气管理器）
    // @param owner 角色实体（用于室内检测，可为null）
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
            if (SCR_DebugBatchManager.ShouldLog(tmpLogInit))
            {
                m_fNextLocationEstimateLogTime = tmpLogInit;
                PrintFormat("[RSS] init weather mgr lat=%1 lon=%2 tz=%3", dbgLat, dbgLon, dbgTZ);
            }
        }
        m_fSurfaceWetnessPenalty = 0.0;
        
        // 获取天气管理器引用
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

        // 调试：打印天气管理器的重要状态，便于排查温度是否被覆盖为常数（例如10°C）
        if (m_pCachedWeatherManager && Replication.IsServer() && StaminaConfigBridge.IsDebugEnabled())
        {
            bool overrideTemp = m_pCachedWeatherManager.GetOverrideTemperature();
            float tempMin = m_pCachedWeatherManager.GetTemperatureAirMinOverride();
            float tempMax = m_pCachedWeatherManager.GetTemperatureAirMaxOverride();
            float wetness = m_pCachedWeatherManager.GetCurrentWetness();
            float rain = m_pCachedWeatherManager.GetRainIntensity();
            float wind = m_pCachedWeatherManager.GetWindSpeed();
            float tod = m_pCachedWeatherManager.GetTimeOfTheDay();

            string isServer = "false";
            if (Replication.IsServer())
                isServer = "true";

            string useEngineTempStr = "false";
            if (m_bUseEngineTemperature)
                useEngineTempStr = "true";

            string useEngineTzStr = "false";
            if (m_bUseEngineTimezone)
                useEngineTzStr = "true";

            // 合并少数字段为单个 Extras 字符串，使用 PrintFormat 并限制到 9 个占位符
            string extras = useEngineTempStr + " | " + useEngineTzStr + " | Lon=" + (Math.Round(m_fLongitude * 10.0) / 10.0) + " | TZOff=" + (Math.Round(m_fTimeZoneOffsetHours * 10.0) / 10.0);

            PrintFormat("[RSS][WeatherDebug] OverrideTemp=%1 | TempMin=%2 | TempMax=%3 | Wetness=%4 | Rain=%5 | Wind=%6 | TimeOfDay=%7 | Server=%8 | Extras=%9",
                overrideTemp,
                Math.Round(tempMin * 10.0) / 10.0,
                Math.Round(tempMax * 10.0) / 10.0,
                Math.Round(wetness * 100.0) / 100.0,
                Math.Round(rain * 100.0) / 100.0,
                Math.Round(wind * 10.0) / 10.0,
                Math.Round(tod * 10.0) / 10.0,
                isServer,
                extras);

            // 尝试基于日出/日落估算经纬度，以补齐气温模型所需参数（若未显式配置）
            // 如果引擎已经提供了有效的经纬度，则无需估算
            bool skipEstimate = false;
            if (m_pCachedWeatherManager)
            {
                float engLat = m_pCachedWeatherManager.GetCurrentLatitude();
                float engLon = m_pCachedWeatherManager.GetCurrentLongitude();
                if (engLat != 0.0 || engLon != 0.0)
                {
                    skipEstimate = true;
                    // 日志提示我们使用了引擎提供的坐标
                    float tmpLocLog1 = m_fNextLocationEstimateLogTime;
                    if (SCR_DebugBatchManager.ShouldLog(tmpLocLog1))
                    {
                        m_fNextLocationEstimateLogTime = tmpLocLog1;
                        PrintFormat("[RSS][LocationEstimate] using engine coords lat=%1 lon=%2", engLat, engLon);
                    }
                }
            }

            if (!skipEstimate)
            {
                float estLat = 0.0;
                float estLon = 0.0;
                float estConf = EstimateLatLongFromSunriseSunset(estLat, estLon);
                if (estConf > 0.0)
                {
                    float tmpLocLog2 = m_fNextLocationEstimateLogTime;
                    if (SCR_DebugBatchManager.ShouldLog(tmpLocLog2))
                    {
                        m_fNextLocationEstimateLogTime = tmpLocLog2;
                        PrintFormat("[RSS][LocationEstimate] Estimated Lat=%1 Lon=%2 Conf=%3 (initial)",
                            Math.Round(estLat * 10.0) / 10.0,
                            Math.Round(estLon * 10.0) / 10.0,
                            Math.Round(estConf * 100.0) / 100.0);
                    }

                    // 若初始置信较低，按需使用天文网格搜索（更慢但更鲁棒）进一步细化
                    if (estConf < 0.9)
                    {
                        float refinedLat = 0.0;
                        float refinedLon = 0.0;
                        float refinedConf = EstimateLatLongFromAstronomicalSearch(refinedLat, refinedLon);
                        if (refinedConf > estConf)
                        {
                            float tmpLocLog3 = m_fNextLocationEstimateLogTime;
                            if (SCR_DebugBatchManager.ShouldLog(tmpLocLog3))
                            {
                                m_fNextLocationEstimateLogTime = tmpLocLog3;
                                PrintFormat("[RSS][LocationEstimate] Refined Lat=%1 Lon=%2 Conf=%3 (improved)",
                                    Math.Round(refinedLat * 10.0) / 10.0,
                                    Math.Round(refinedLon * 10.0) / 10.0,
                                    Math.Round(refinedConf * 100.0) / 100.0);
                            }
                        }
                    }
                }
            }
        
        // 初始化室内检测模块
        m_pIndoorDetector = new SCR_EnvironmentIndoorDetection();
        
        // perf: 随机偏移环境检测相位，避免多实体（尤其是AI）在同一帧集中触发5s更新
        m_fLastEnvironmentCheckTime = m_fLastEnvironmentCheckTime + Math.RandomFloat(0.0, StaminaConstants.ENV_CHECK_INTERVAL);
        }
    }

    // 设置室内检测调试开关 — 委托给 SCR_EnvironmentIndoorDetection
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

    // 设置是否使用引擎实时天气数据（用于在运行时切换）
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
        if (SCR_DebugBatchManager.ShouldLog(tmpLogTime))
        {
            m_fNextForceUpdateLogTime = tmpLogTime;
            PrintFormat("[RSS] ForceUpdate: Pending recompute flagged");
        }
    }


    
    // 更新环境因子（协调方法）
    // @param currentTime 当前时间（秒）
    // @param owner 角色实体（用于室内检测和姿态判断）
    // @param playerVelocity 玩家速度向量（用于风阻计算）
    // @param terrainFactor 地形系数（用于泥泞计算）
    // @param swimmingWetWeight 游泳湿重（kg，用于总湿重计算）
    // @return 是否需要更新（true表示已更新，false表示跳过）
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

        // 调试：每次获取时检查是否为零-coordinate
        if (m_pCachedWeatherManager)
        {
            float dbgLat = m_pCachedWeatherManager.GetCurrentLatitude();
            float dbgLon = m_pCachedWeatherManager.GetCurrentLongitude();
            if (dbgLat == 0.0 && dbgLon == 0.0)
            {
                float tmpLogU1 = m_fNextLocationEstimateLogTime;
                if (SCR_DebugBatchManager.ShouldLog(tmpLogU1))
                {
                    m_fNextLocationEstimateLogTime = tmpLogU1;
                    Print("[RSS] weather mgr returned 0/0 coordinates, delaying");
                }
            }
            else
            {
                // 如果之前已经打印了0/0，可以输出一次有效坐标
                static bool once = false;
                if (!once)
                {
                    float tmpLogU2 = m_fNextLocationEstimateLogTime;
                    if (SCR_DebugBatchManager.ShouldLog(tmpLogU2))
                    {
                        m_fNextLocationEstimateLogTime = tmpLogU2;
                        PrintFormat("[RSS] weather mgr now has coords lat=%1 lon=%2", dbgLat, dbgLon);
                    }
                    once = true;
                }
            }
        }
        
        // 更新缓存的角色实体引用（用于室内检测）
        if (owner)
            m_pCachedOwner = owner;

        // 按间隔更新室内状态缓存 — 委托给 m_pIndoorDetector
        if (m_pIndoorDetector)
            m_pIndoorDetector.UpdateIndoorCache(m_pCachedOwner, currentTime);
        
        // 检查是否需要更新（每 m_fLastEnvironmentCheckTime 秒更新一次），但对管理员的即时修改要实时响应
        bool forceUpdate = false;
        if (m_pCachedWeatherManager)
        {
            // 快速采样当前引擎状态（perf: 优先使用信号内存读取）
            float currTOD = ReadSignalTOD();
            int y, mo, d;
            m_pCachedWeatherManager.GetDate(y, mo, d);
            float currRain = ReadSignalRainIntensity();
            float currWind = ReadSignalWindSpeed();
            bool currOverrideTemp = m_pCachedWeatherManager.GetOverrideTemperature();
            float sr = 0.0;
            float ss = 0.0;
            bool hasSR = m_pCachedWeatherManager.GetSunriseHour(sr);
            bool hasSS = m_pCachedWeatherManager.GetSunsetHour(ss);

            if (Replication.IsServer() && SCR_DebugBatchManager.IsDebugBatchActive())
            {
                string dateStr = y.ToString() + "/" + mo.ToString() + "/" + d.ToString();
                string line = string.Format("[RSS] 引擎天气: TOD=%1 雨=%2 风=%3 ovTemp=%4 日出=%5 日落=%6 日期=%7",
                    currTOD, currRain, currWind, currOverrideTemp, sr, ss, dateStr);
                SCR_DebugBatchManager.AddDebugBatchLineOnce("EngineTOD", line);
            }

            // 若与缓存值出现显著差异，则触发强制更新（实时响应管理员操作）
            if (m_fLastKnownTOD < 0.0 || Math.AbsFloat(currTOD - m_fLastKnownTOD) > 0.1) // >6min 变化视为人工修改
                forceUpdate = true;
            if (m_iLastKnownYear != y || m_iLastKnownMonth != mo || m_iLastKnownDay != d)
                forceUpdate = true;
            if (Math.AbsFloat(currRain - m_fLastKnownRainIntensity) > 0.05)
                forceUpdate = true;
            if (Math.AbsFloat(currWind - m_fLastKnownWindSpeed) > 0.5)
                forceUpdate = true;
            if (currOverrideTemp != m_bLastKnownOverrideTemperature)
                forceUpdate = true;
            if (hasSR && Math.AbsFloat(sr - m_fLastKnownSunriseHour) > 0.01)
                forceUpdate = true;
            if (hasSS && Math.AbsFloat(ss - m_fLastKnownSunsetHour) > 0.01)
                forceUpdate = true;
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

        if (!forceUpdate && (currentTime - m_fLastEnvironmentCheckTime < StaminaConstants.ENV_CHECK_INTERVAL))
            return false;

        // 标记为已检查时间
        m_fLastEnvironmentCheckTime = currentTime;
        
        // 获取角色姿态（用于地表湿度惩罚计算）
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
        // 使用 SwimmingStateManager 的方法计算总湿重
        m_fCurrentTotalWetWeight = SwimmingStateManager.CalculateTotalWetWeight(swimmingWetWeight, m_fCachedRainWeight);

        // 若检测到管理员即时修改（forceUpdate），通过封装方法标记为待处理（避免在此处直接写入）
        if (forceUpdate)
        {
            MarkPendingForceUpdate();
        }

        // 同步更新实时检测缓存（记录当前引擎状态，供下一次比较）
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

        // 若之前被标记为 pending，则在安全上下文中执行重算并清理标记（使用通用气温模型）
        if (m_bPendingForceUpdate)
        {
            if (m_pCachedWeatherManager)
            {
                float lat = m_fLatitude; // perf: 缓存纬度
                int year, month, day;
                m_pCachedWeatherManager.GetDate(year, month, day);
                int n = DayOfYear(year, month, day);
                float tod = ReadSignalTOD();
                float cloud = GetCloudFactorCached();
                float rain = ReadSignalRainIntensity();
                float altM = GetCurrentAltitudeMeters(owner);
                float T = CalculateUniversalTemperature(lat, n, tod, altM, cloud, rain, m_fFogDensity);
                m_fCachedSurfaceTemperature = T;
                m_fCachedTemperature = m_fCachedSurfaceTemperature;
            }
            m_bPendingForceUpdate = false;

            // 日志（使用临时变量以避免 inout 成员写入问题）
            float tmpLogTime2 = m_fNextForceUpdateLogTime;
            if (SCR_DebugBatchManager.ShouldLog(tmpLogTime2))
            {
                m_fNextForceUpdateLogTime = tmpLogTime2;
                PrintFormat("[RSS] ForceUpdate: Applied pending recompute: %1°C", Math.Round(m_fCachedSurfaceTemperature * 10.0) / 10.0);
            }
        }

        // 调试信息：环境因子更新（统一节流）
        static float nextEnvLogTime = 0.0;
        if (SCR_DebugBatchManager.ShouldLog(nextEnvLogTime))
        {
            PrintFormat("[RSS] 环境因子 / Environment Factors: 虚拟气温=%1°C | 热应激=%2x | 降雨湿重=%3kg | 总湿重=%4kg | 风速=%5m/s | Simulated Temp=%1°C | Heat Stress=%2x | Rain Weight=%3kg | Total Wet Weight=%4kg | Wind Speed=%5m/s",
                Math.Round(m_fCachedTemperature * 10.0) / 10.0,
                Math.Round(m_fCachedHeatStressMultiplier * 100.0) / 100.0,
                Math.Round(m_fCachedRainWeight * 10.0) / 10.0,
                Math.Round(m_fCurrentTotalWetWeight * 10.0) / 10.0,
                Math.Round(m_fCachedWindSpeed * 10.0) / 10.0);
        }
        
        return true;
    }
    
    // 获取热应激倍数（基于时间段）
    // @return 热应激倍数（1.0-1.3）
    float GetHeatStressMultiplier()
    {
        return m_fCachedHeatStressMultiplier;
    }
    
    // 获取降雨湿重（kg）
    // @return 降雨湿重（0.0-8.0 kg）
    float GetRainWeight()
    {
        return m_fCachedRainWeight;
    }
    
    // ==================== 高级环境因子获取方法（v2.14.0）====================
    
    // 获取降雨强度（0.0-1.0）
    // @return 降雨强度（0.0=无雨，1.0=暴雨）
    float GetRainIntensity()
    {
        return m_fCachedRainIntensity;
    }
    
    // 获取风速（m/s）
    // @return 风速（m/s）
    float GetWindSpeed()
    {
        return m_fCachedWindSpeed;
    }
    
    // 获取风向（度）
    // @return 风向（0-360度，0=北，90=东，180=南，270=西）
    float GetWindDirection()
    {
        return m_fCachedWindDirection;
    }
    
    // 获取风阻系数
    // @return 风阻系数（负数=顺风，正数=逆风）
    float GetWindDrag()
    {
        return m_fCachedWindDrag;
    }
    
    // 获取泥泞度系数（0.0-1.0）
    // @return 泥泞度系数（0.0=干燥，1.0=完全泥泞）
    float GetMudFactor()
    {
        return m_fCachedMudFactor;
    }
    
    // 获取当前气温（°C）
    // @return 当前气温（°C）
    float GetTemperature()
    {
        return m_fCachedTemperature;
    }
    
    // 获取地表湿度（0.0-1.0）
    // @return 地表湿度（0.0=干燥，1.0=完全湿润）
    float GetSurfaceWetness()
    {
        return m_fCachedSurfaceWetness;
    }
    
    // 获取总湿重（游泳+降雨，kg）
    // @return 总湿重（0.0-10.0 kg）
    float GetTotalWetWeight()
    {
        return m_fCurrentTotalWetWeight;
    }
    
    // 获取暴雨呼吸阻力惩罚
    // @return 呼吸阻力惩罚（0.0-1.0）
    float GetRainBreathingPenalty()
    {
        return m_fRainBreathingPenalty;
    }
    
    // 获取泥泞地形系数惩罚
    // @return 泥泞地形系数（0.0-0.4）
    float GetMudTerrainFactor()
    {
        return m_fMudTerrainFactor;
    }
    
    // 获取泥泞Sprint惩罚
    // @return Sprint惩罚（0.0-0.1）
    float GetMudSprintPenalty()
    {
        return m_fMudSprintPenalty;
    }
    
    // 获取滑倒风险
    // @return 滑倒风险（0.0-1.0）
    float GetSlipRisk()
    {
        return m_fSlipRisk;
    }

    // 最近一次 UpdateEnvironmentFactors 传入的地形系数（用于滑倒 ACOF 等）
    float GetCachedTerrainFactor()
    {
        return m_fCachedTerrainFactor;
    }
    
    // 获取热应激惩罚
    // @return 热应激惩罚（0.0-1.0）
    float GetHeatStressPenalty()
    {
        return m_fHeatStressPenalty;
    }
    
    // 获取冷应激惩罚
    // @return 冷应激惩罚（0.0-1.0）
    float GetColdStressPenalty()
    {
        return m_fColdStressPenalty;
    }
    
    // 获取冷应激静态惩罚
    // @return 冷应激静态惩罚（0.0-1.0）
    float GetColdStaticPenalty()
    {
        return m_fColdStaticPenalty;
    }
    
    // 获取地表湿度惩罚
    // @return 地表湿度惩罚（0.0-0.15）
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
        float mult = 1.0 + m_fCachedMudFactor * StaminaConstants.ENV_MUD_PENALTY_MAX;

        if (m_fCachedWindDrag > 0.0)
            mult = mult * (1.0 + m_fCachedWindDrag);

        if (m_fHeatStressPenalty > 0.05)
            mult = mult * (1.0 + m_fHeatStressPenalty * 0.5);

        return mult;
    }

    // ==================== 私有方法 ====================
    
    // ── 信号读取辅助（perf: 静态 GlobalSignalsManager 内存读取，回退到 C++ 桥接）──
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

    // 正弦波叠加气温模型 — 委托给 SCR_EnvironmentAstronomyMath
    protected float CalculateUniversalTemperature(float latitude, int dayOfYear, float hourOfDay, float altitudeMeters, float overcast, float rainIntensity, float fogDensity)
    {
        return SCR_EnvironmentAstronomyMath.CalculateUniversalTemperature(latitude, dayOfYear, hourOfDay, altitudeMeters, overcast, rainIntensity, fogDensity);
    }

    // ------------------- 太阳与辐照工具函数（P1） -------------------
    
    // 计算某个日期的年积日（1..365/366）
    protected int DayOfYear(int year, int month, int day)
    {
        return SCR_EnvironmentAstronomyMath.DayOfYear(year, month, day);
    }

    // 太阳偏角（弧度）
    protected float SolarDeclination(int n)
    {
        return SCR_EnvironmentAstronomyMath.SolarDeclination(n);
    }

    // 计算太阳天顶角余弦（cos θ），输入纬度（deg）、年日、时刻（小时）
    protected float SolarCosZenith(float latDeg, int n, float localHour)
    {
        return SCR_EnvironmentAstronomyMath.SolarCosZenith(latDeg, n, localHour);
    }

    // 估算经纬度 — 委托给 SCR_EnvironmentAstronomyMath（副作用：写回 m_fLatitude/m_fLongitude + 日志）
    protected float EstimateLatLongFromSunriseSunset(out float outLatDeg, out float outLonDeg)
    {
        float conf = SCR_EnvironmentAstronomyMath.EstimateLatLongFromSunriseSunset(
            m_pCachedWeatherManager, m_fTimeZoneOffsetHours,
            outLatDeg, outLonDeg,
            m_fCachedRainIntensity, m_fCachedSurfaceWetness);

        if (conf > 0.0)
        {
            m_fLatitude = outLatDeg;
            m_fLongitude = outLonDeg;
            float tmpLog = m_fNextLocationEstimateLogTime;
            if (SCR_DebugBatchManager.ShouldLog(tmpLog))
            {
                m_fNextLocationEstimateLogTime = tmpLog;
                PrintFormat("[RSS] EstimateLatLong: lat=%1 lon=%2 conf=%3",
                    Math.Round(outLatDeg * 10.0) / 10.0,
                    Math.Round(outLonDeg * 10.0) / 10.0,
                    Math.Round(conf * 100.0) / 100.0);
            }
        }
        return conf;
    }

    // 天文网格搜索 — 委托给 SCR_EnvironmentAstronomyMath（副作用：写回 m_fLatitude/m_fLongitude + 日志）
    protected float EstimateLatLongFromAstronomicalSearch(out float outLatDeg, out float outLonDeg)
    {
        float conf = SCR_EnvironmentAstronomyMath.EstimateLatLongFromAstronomicalSearch(
            m_pCachedWeatherManager, m_fTimeZoneOffsetHours,
            outLatDeg, outLonDeg,
            m_fCachedRainIntensity, m_fCachedSurfaceWetness);

        if (conf > 0.0)
        {
            m_fLatitude = outLatDeg;
            m_fLongitude = outLonDeg;
            float tmpLog = m_fNextLocationEstimateLogTime;
            if (SCR_DebugBatchManager.ShouldLog(tmpLog))
            {
                m_fNextLocationEstimateLogTime = tmpLog;
                PrintFormat("[RSS] EstimateLatLongAstronomy: lat=%1 lon=%2 conf=%3",
                    Math.Round(outLatDeg * 10.0) / 10.0,
                    Math.Round(outLonDeg * 10.0) / 10.0,
                    Math.Round(conf * 100.0) / 100.0);
            }
        }
        return conf;
    }

    // 空气质量近似（Kasten & Young，返回 m）
    protected float AirMass(float cosTheta)
    {
        return SCR_EnvironmentAstronomyMath.AirMass(cosTheta);
    }

    // 根据空气质量计算简单的清空透过率（经验）
    protected float ClearSkyTransmittance(float m)
    {
        return SCR_EnvironmentAstronomyMath.ClearSkyTransmittance(m, m_fAerosolOpticalDepth);
    }

    // 简单云因子推断 — 委托给 SCR_EnvironmentAstronomyMath
    protected float InferCloudFactor()
    {
        return SCR_EnvironmentAstronomyMath.InferCloudFactor(m_fCachedRainIntensity, m_fCachedSurfaceWetness, m_pCachedWeatherManager);
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


    // 估算季节均温（°C），用于长波下行中的环境参考温度，避免 T_atm = T_surface+2 导致的热失控
    // 赤道约27°C，随纬度降低；北半球简易季节调制；北大西洋/中高纬海洋偏凉
    protected float EstimateSeasonalAvgTemp(float lat, int dayOfYear)
    {
        return SCR_EnvironmentAstronomyMath.EstimateSeasonalAvgTemp(lat, dayOfYear);
    }


    // 计算热应激倍数（基于当前导出的温度，考虑室内豁免）
    // 热应激模型：基于虚拟气温阈值，而非固定时间段
    // 只有当虚拟气温超过 26°C 时，才开始计算热应激
    // 如果角色在室内（头顶有遮挡），热应激减少 50%
    // @param owner 角色实体（用于室内检测，可为null）
    // @return 热应激倍数（1.0-1.3）
    protected float CalculateHeatStressMultiplier(IEntity owner = null)
    {
        if (!m_pCachedWeatherManager)
            return 1.0;
        
        // 使用当前导出的气温（统一来自 GetTemperature()，可能是引擎/物理模型或模拟模型）
        float currentTemp = GetTemperature();
        
        // 热应激触发阈值：26°C
        // 只有当当前气温超过 26°C 时，才开始计算热应激
        const float heatStressThreshold = 26.0;
        float multiplier = 1.0;
        
        if (currentTemp < heatStressThreshold)
        {
            // 当前气温未达阈值，无热应激
            multiplier = 1.0;
        }
        else
        {
            // 当前气温超过阈值，计算热应激倍数
            // 倍数 = 1.0 + (当前气温 - 阈值) * 0.02
            // 例如：30°C -> 1.0 + (30 - 26) * 0.02 = 1.08x
            float tempExcess = currentTemp - heatStressThreshold;
            multiplier = 1.0 + tempExcess * 0.02;
        }
        
        // 室内豁免：如果角色在室内（头顶有遮挡），热应激减少 50%
        if (owner && IsUnderCover(owner))
        {
            // 室内热应激 = 室外热应激 × (1 - 室内减少比例)
            multiplier = multiplier * (1.0 - StaminaConstants.ENV_HEAT_STRESS_INDOOR_REDUCTION);
        }
        
        return Math.Clamp(multiplier, 1.0, StaminaConstants.ENV_HEAT_STRESS_MAX_MULTIPLIER);
    }
    
    // EvaluateRoofedBuildingInterior / RaycastHasRoof / WorldToLocal / IsHorizontallyEnclosed / QueryBuildingCallback
    // — 全部委托给 SCR_EnvironmentIndoorDetection
    protected bool EvaluateRoofedBuildingInterior(IEntity owner, float roofCheckHeightM, bool requireHorizontalEnclosure)
    {
        if (m_pIndoorDetector)
            return m_pIndoorDetector.EvaluateRoofedBuildingInterior(owner, roofCheckHeightM, requireHorizontalEnclosure);
        return false;
    }


    // 室内检测 — 全部委托给 SCR_EnvironmentIndoorDetection
    protected bool IsUnderCover(IEntity owner)
    {
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsUnderCover(owner);
        return false;
    }

    // 计算降雨湿重（基于天气状态）
    // 优先尝试使用 GetRainIntensity() API，如果没有则回退到字符串匹配
    // 停止降雨后，湿重使用二次方衰减（更自然的蒸发过程）
    // @param currentTime 当前世界时间
    // @return 降雨湿重（0.0-8.0 kg）
    
    // 手动更新环境因子（用于调试或强制更新）
    // @param currentTime 当前世界时间
    // @param owner 角色实体（用于室内检测，可为null）
    // @param swimmingWetWeight 游泳湿重（kg，默认0.0）
    void ForceUpdate(float currentTime, IEntity owner = null, float swimmingWetWeight = 0.0)
    {
        m_fLastEnvironmentCheckTime = 0.0; // 重置时间，强制更新
        UpdateEnvironmentFactors(currentTime, owner, vector.Zero, 1.0, swimmingWetWeight);
    }
    
    // 设置角色实体引用（用于室内检测）
    // @param owner 角色实体
    void SetOwner(IEntity owner)
    {
        m_pCachedOwner = owner;
    }
    
    // 获取天气管理器引用
    // @return 天气管理器引用（可为null）
    TimeAndWeatherManagerEntity GetWeatherManager()
    {
        return m_pCachedWeatherManager;
    }
    
    // 设置天气管理器引用（用于手动设置）
    // @param weatherManager 天气管理器引用
    void SetWeatherManager(TimeAndWeatherManagerEntity weatherManager)
    {
        m_pCachedWeatherManager = weatherManager;
    }
    
    // ==================== 调试信息获取方法 ====================
    
    // 获取当前时间（小时）
    // @return 当前时间（小时，0.0-24.0），如果无法获取则返回-1.0
    float GetCurrentHour()
    {
        if (!m_pCachedWeatherManager)
            return -1.0;
        
        return m_pCachedWeatherManager.GetTimeOfTheDay();
    }
    
    // 检查是否在室内 — 委托给 SCR_EnvironmentIndoorDetection
    bool IsIndoor()
    {
        if (!StaminaConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsIndoor(m_pCachedOwner);
        return false;
    }

    // 室内检测公共接口 — 全部委托给 SCR_EnvironmentIndoorDetection
    bool IsRoofedBuildingVolumeForEntity(IEntity owner)
    {
        if (!StaminaConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.IsRoofedBuildingVolumeForEntity(owner);
        return false;
    }

    bool ShouldSuppressTerrainSlopeForEntity(IEntity owner)
    {
        if (!StaminaConfigBridge.IsIndoorDetectionEnabled())
            return false;
        if (m_pIndoorDetector)
            return m_pIndoorDetector.ShouldSuppressTerrainSlopeForEntity(owner);
        return false;
    }

    bool IsIndoorForEntity(IEntity owner)
    {
        if (!StaminaConfigBridge.IsIndoorDetectionEnabled())
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
    
    // 检查是否正在下雨（与引擎下雨粒子特效对齐）
    // 引擎在 rain<0.15 时通常不显示雨滴，0.1 多为阴天/潮湿无可见雨
    // @return true表示正在下雨（有可见雨效），false表示未下雨
    bool IsRaining()
    {
        return m_fCachedRainIntensity >= StaminaConstants.ENV_RAIN_VISUAL_EFFECT_THRESHOLD;
    }
    
    // ==================== 高级环境因子计算方法（v2.14.0）====================
    
    // 更新高级环境因子（协调方法）
    // @param currentTime 当前时间（秒）
    // @param owner 角色实体
    // @param playerVelocity 玩家速度向量（用于风阻计算）
    // @param stance 当前姿态（0=站立，1=蹲姿，2=趴姿）
    void UpdateAdvancedEnvironmentFactors(float currentTime, IEntity owner, vector playerVelocity = vector.Zero, int stance = 0)
    {
        if (!m_pCachedWeatherManager)
            return;
        
        // 计算时间增量（在更新时间戳之前）
        float deltaTime = currentTime - m_fLastUpdateTime;
        
        // 1. 获取降雨强度（优先使用API，失败则回退到字符串匹配）
        m_fCachedRainIntensity = CalculateRainIntensityFromAPI();
        
        // 2. 获取风速和风向
        m_fCachedWindSpeed = CalculateWindSpeedFromAPI();
        m_fCachedWindDirection = CalculateWindDirectionFromAPI();
        
        // 3. 计算风阻系数（基于玩家移动方向）
        m_fCachedWindDrag = CalculateWindDrag(playerVelocity);
        
        // 4. 获取泥泞度系数
        m_fCachedMudFactor = CalculateMudFactorFromAPI();
        
        // 5. 获取当前气温：通用经验模型（纬度+季节+海拔+昼夜+天气），不依赖物理求解，兼容各模组地图
        // perf: 仅首帧无条件初始化，之后严格按时间/位置触发，避免每5s双重计算
        if (m_pCachedWeatherManager)
        {
            float lat = m_fLatitude; // perf: 缓存纬度，地图常数无需每次查询
            int year, month, day;
            m_pCachedWeatherManager.GetDate(year, month, day);
            int n = DayOfYear(year, month, day);
            
            // 首帧无条件初始化（仅一次）
            if (m_fLastTemperatureUpdateTime <= 0.0 && m_fCachedSurfaceTemperature == 20.0)
            {
                float tod = ReadSignalTOD();
                float cloud = GetCloudFactorCached();
                float rain = ReadSignalRainIntensity();
                float altM = GetCurrentAltitudeMeters(owner);
                float T = CalculateUniversalTemperature(lat, n, tod, altM, cloud, rain, m_fFogDensity);
                m_fCachedSurfaceTemperature = T;
                m_fLastTemperatureUpdateTime = currentTime;
                if (owner)
                {
                    m_vLastTempCalcPosition = owner.GetOrigin();
                    m_bTempPositionInitialized = true;
                }
            }

            // 触发条件：时间间隔 OR 位置移动超过阈值（100m）
            // 位置触发可在玩家快速移动时及时更新海拔/云层，减少静止时的无效重算
            bool timeTrigger = (currentTime - m_fLastTemperatureUpdateTime >= m_fTempUpdateInterval);
            bool posTrigger = false;
            if (owner && m_bTempPositionInitialized)
            {
                vector curPos = owner.GetOrigin();
                float dSq = vector.DistanceSq(curPos, m_vLastTempCalcPosition);
                posTrigger = (dSq >= TEMP_RECALC_DISTANCE_SQ);
            }

            if (timeTrigger || posTrigger)
            {
                float tod = ReadSignalTOD();
                float cloud = GetCloudFactorCached();
                float rain = ReadSignalRainIntensity();
                float altM = GetCurrentAltitudeMeters(owner);
                float T = CalculateUniversalTemperature(lat, n, tod, altM, cloud, rain, m_fFogDensity);
                m_fCachedSurfaceTemperature = T;
                m_fLastTemperatureUpdateTime = currentTime;
                m_fNextTempStepLogTime = currentTime + m_fTempUpdateInterval;
                if (owner)
                    m_vLastTempCalcPosition = owner.GetOrigin();
            }

            m_fCachedTemperature = m_fCachedSurfaceTemperature;
        }
        
        // 6. 获取地表湿度
        m_fCachedSurfaceWetness = CalculateSurfaceWetnessFromAPI();
        
        // 7. 计算降雨湿重（基于降雨强度；需要 currentTime 计算线性衰减）
        CalculateRainWetWeight(currentTime);
        
        // 8. 计算暴雨呼吸阻力
        CalculateRainBreathingPenalty();
        
        // 9. 计算泥泞地形系数
        CalculateMudTerrainFactor();
        
        // 10. 计算泥泞Sprint惩罚
        CalculateMudSprintPenalty();
        
        // 11. 计算滑倒风险
        CalculateSlipRisk();
        
        // 12. 计算热应激惩罚
        CalculateHeatStressPenalty();
        
        // 13. 计算冷应激惩罚
        CalculateColdStressPenalty();
        
        // 14. 计算地表湿度惩罚（传入姿态）
        CalculateSurfaceWetnessPenalty(owner, stance);
        
        // ==================== 调试信息：高级环境因子（v2.14.0）====================
        static float nextAdvancedEnvLogTime = 0.0;
        if (SCR_DebugBatchManager.ShouldVerboseLog(nextAdvancedEnvLogTime))
        {
            PrintFormat("[RSS] 高级环境因子 / Advanced Environment Factors:");
            PrintFormat("  降雨强度 / Rain Intensity: %1 (%2%%)", 
                Math.Round(m_fCachedRainIntensity * 100.0) / 100.0,
                Math.Round(m_fCachedRainIntensity * 100.0).ToString());
            PrintFormat("  风速 / Wind Speed: %1 m/s", Math.Round(m_fCachedWindSpeed * 10.0) / 10.0);
            PrintFormat("  风向 / Wind Direction: %1°", Math.Round(m_fCachedWindDirection));
            PrintFormat("  风阻系数 / Wind Drag: %1", Math.Round(m_fCachedWindDrag * 100.0) / 100.0);
            PrintFormat("  泥泞度 / Mud Factor: %1 (%2%%)", 
                Math.Round(m_fCachedMudFactor * 100.0) / 100.0,
                Math.Round(m_fCachedMudFactor * 100.0).ToString());
            // 温度信息：显示当前值、来源（engine/simulated）以及 min/max
            string tempSource = "simulated";
            if (m_bUseEngineWeather && m_pCachedWeatherManager)
                tempSource = "engine";

            float tempMinDbg = 0.0;
            float tempMaxDbg = 0.0;
            if (m_pCachedWeatherManager)
            {
                tempMinDbg = m_pCachedWeatherManager.GetTemperatureAirMinOverride();
                tempMaxDbg = m_pCachedWeatherManager.GetTemperatureAirMaxOverride();
            }

            PrintFormat("  Temperature: Current=%1°C (source=%2) | Min=%3 | Max=%4", 
                Math.Round(m_fCachedTemperature * 10.0) / 10.0,
                tempSource,
                Math.Round(tempMinDbg * 10.0) / 10.0,
                Math.Round(tempMaxDbg * 10.0) / 10.0);

            PrintFormat("  地表湿度 / Surface Wetness: %1 (%2%%)", 
                Math.Round(m_fCachedSurfaceWetness * 100.0) / 100.0,
                Math.Round(m_fCachedSurfaceWetness * 100.0).ToString());
            PrintFormat("  降雨湿重 / Rain Weight: %1 kg", Math.Round(m_fCachedRainWeight * 10.0) / 10.0);
            PrintFormat("  暴雨呼吸阻力 / Rain Breathing Penalty: %1", Math.Round(m_fRainBreathingPenalty * 10000.0) / 10000.0);
            PrintFormat("  泥泞地形系数 / Mud Terrain Factor: %1", Math.Round(m_fMudTerrainFactor * 100.0) / 100.0);
            PrintFormat("  泥泞Sprint惩罚 / Mud Sprint Penalty: %1", Math.Round(m_fMudSprintPenalty * 100.0) / 100.0);
            PrintFormat("  滑倒风险 / Slip Risk: %1", Math.Round(m_fSlipRisk * 10000.0) / 10000.0);
            PrintFormat("  热应激惩罚 / Heat Stress Penalty: %1", Math.Round(m_fHeatStressPenalty * 100.0) / 100.0);
            PrintFormat("  冷应激惩罚 / Cold Stress Penalty: %1", Math.Round(m_fColdStressPenalty * 100.0) / 100.0);
            PrintFormat("  冷应激静态惩罚 / Cold Static Penalty: %1", Math.Round(m_fColdStaticPenalty * 100.0) / 100.0);
            PrintFormat("  地表湿度惩罚 / Surface Wetness Penalty: %1", Math.Round(m_fSurfaceWetnessPenalty * 100.0) / 100.0);
        }
        
        // 更新时间戳（在所有计算完成后）
        m_fLastUpdateTime = currentTime;
    }
    
    // 从API获取降雨强度（带字符串回退）
    // @return 降雨强度（0.0-1.0）
    protected float CalculateRainIntensityFromAPI()
    {
        float rainIntensity = ReadSignalRainIntensity();
        if (rainIntensity > StaminaConstants.ENV_RAIN_INTENSITY_THRESHOLD)
            return rainIntensity;
        return SCR_EnvironmentWeatherApi.CalculateRainIntensityFromStateName(m_pCachedWeatherManager);
    }
    
    // 从API获取风速（perf: 信号内存读取）
    // @return 风速（m/s）
    protected float CalculateWindSpeedFromAPI()
    {
        return ReadSignalWindSpeed();
    }
    
    // 从API获取风向
    // @return 风向（度，0-360）
    protected float CalculateWindDirectionFromAPI()
    {
        return SCR_EnvironmentWeatherApi.CalculateWindDirectionFromAPI(m_pCachedWeatherManager);
    }
    
    // 计算风阻系数（基于玩家移动方向）
    // @param playerVelocity 玩家速度向量
    // @return 风阻系数（0.0-1.0）
    protected float CalculateWindDrag(vector playerVelocity)
    {
        return SCR_EnvironmentWeatherApi.CalculateWindDrag(m_fCachedWindSpeed, m_fCachedWindDirection, playerVelocity);
    }
    
    // 从API获取泥泞度系数
    // @return 泥泞度系数（0.0-1.0）
    protected float CalculateMudFactorFromAPI()
    {
        return SCR_EnvironmentWeatherApi.CalculateMudFactorFromAPI(m_pCachedWeatherManager);
    }
    

    // 从API获取地表湿度
    // @return 地表湿度（0.0-1.0）
    protected float CalculateSurfaceWetnessFromAPI()
    {
        return ReadSignalWetness();
    }
    
    // 计算降雨湿重（基于降雨强度）
    // 降雨中：按强度非线性累积；停雨后：60秒线性衰减至0
    // @param currentTime 当前世界时间（秒，绝对值）
    protected void CalculateRainWetWeight(float currentTime)
    {
        float deltaTime = currentTime - m_fLastUpdateTime;
        if (deltaTime <= 0)
            return;
        
        // 检查是否在室内（室内不受降雨影响）
        bool isIndoor = IsUnderCover(m_pCachedOwner);
        
        // 检查是否在室外且正在下雨（需超过引擎下雨特效阈值，否则 0.1 等值无可见雨）
        bool isOutdoorAndRaining = !isIndoor && m_fCachedRainIntensity >= StaminaConstants.ENV_RAIN_VISUAL_EFFECT_THRESHOLD;
        
        if (isOutdoorAndRaining)
        {
            // 重新下雨时重置衰减锚点，确保下次停雨的线性起点正确
            m_fRainStopTime = -1.0;
            m_fRainPeakWeight = 0.0;

            // 在室外且正在下雨：增加湿重
            // 计算湿重增加速率（非线性增长）
            float accumulationRate = StaminaConstants.ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE * 
                                     Math.Pow(m_fCachedRainIntensity, StaminaConstants.ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT);
            
            // 增加湿重（上限使用 GetEnvRainWeightMax()，降雨单独上限低于组合池上限）
            m_fCachedRainWeight = Math.Clamp(
                m_fCachedRainWeight + accumulationRate * deltaTime,
                0.0,
                StaminaConfigBridge.GetEnvRainWeightMax()
            );
        }
        else
        {
            // 停雨或室内：线性衰减（60秒内从峰值减至0）
            if (m_fCachedRainWeight > 0.0)
            {
                // 刚停雨的第一帧：锚定当前湿重为衰减峰值
                if (m_fRainStopTime < 0.0)
                {
                    m_fRainStopTime = currentTime;
                    m_fRainPeakWeight = m_fCachedRainWeight;
                }

                float elapsed = currentTime - m_fRainStopTime;
                float duration = StaminaConstants.ENV_RAIN_WEIGHT_DURATION;
                if (elapsed >= duration)
                {
                    m_fCachedRainWeight = 0.0;
                    m_fRainStopTime = -1.0;
                    m_fRainPeakWeight = 0.0;
                    m_fLastRainIntensity = 0.0;
                }
                else
                {
                    // 线性衰减: W(t) = W_peak × (1 - t / D)
                    m_fCachedRainWeight = m_fRainPeakWeight * (1.0 - elapsed / duration);
                }
            }
        }
    }
    
    // 计算暴雨呼吸阻力
    protected void CalculateRainBreathingPenalty()
    {
        m_fRainBreathingPenalty = SCR_EnvironmentPenaltyMath.CalculateRainBreathingPenalty(m_fCachedRainIntensity);
    }
    
    // 计算泥泞地形系数
    protected void CalculateMudTerrainFactor()
    {
        m_fMudTerrainFactor = SCR_EnvironmentPenaltyMath.CalculateMudTerrainFactor(m_fCachedTerrainFactor, m_fCachedMudFactor);
    }
    
    // 计算泥泞Sprint惩罚
    protected void CalculateMudSprintPenalty()
    {
        m_fMudSprintPenalty = SCR_EnvironmentPenaltyMath.CalculateMudSprintPenalty(m_fCachedMudFactor);
    }
    
    // 计算滑倒风险
    protected void CalculateSlipRisk()
    {
        m_fSlipRisk = SCR_EnvironmentPenaltyMath.CalculateSlipRisk(m_fCachedMudFactor);
    }
    
    // 计算热应激惩罚
    protected void CalculateHeatStressPenalty()
    {
        m_fHeatStressPenalty = SCR_EnvironmentPenaltyMath.CalculateHeatStressPenalty(m_fCachedTemperature);
    }

    // 根据当前环境温度和风速对机械能耗进行补偿
    // @param basePower 运动机械功率 (J/s)
    // @return 包含热调节的总功率 (J/s)
    float AdjustEnergyForTemperature(float basePower)
    {
        return SCR_EnvironmentPenaltyMath.AdjustEnergyForTemperature(basePower, m_fCachedTemperature, m_fCachedWindSpeed);
    }
    
    // 计算冷应激惩罚
    protected void CalculateColdStressPenalty()
    {
        SCR_EnvironmentPenaltyMath.CalculateColdStressPenalty(m_fCachedTemperature, m_fColdStressPenalty, m_fColdStaticPenalty);
    }
    
    // 计算地表湿度惩罚
    // @param owner 角色实体
    // @param stance 当前姿态（0=站立，1=蹲姿，2=趴姿）
    protected void CalculateSurfaceWetnessPenalty(IEntity owner, int stance = 0)
    {
        if (!owner)
            return;

        m_fSurfaceWetnessPenalty = SCR_EnvironmentPenaltyMath.CalculateSurfaceWetnessPenalty(m_fCachedSurfaceWetness, stance);
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