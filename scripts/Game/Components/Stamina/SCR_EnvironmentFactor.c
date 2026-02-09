// 环境因子模块
// 负责处理环境因素对体力系统的影响（热应激、降雨湿重等）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class EnvironmentFactor
{
    // ==================== 状态变量 ====================
    protected float m_fCachedHeatStressMultiplier = 1.0; // 缓存的热应激倍数
    protected float m_fCachedRainWeight = 0.0; // 缓存的降雨湿重（kg）
    protected float m_fLastEnvironmentCheckTime = 0.0; // 上次环境检测时间
    protected float m_fRainStopTime = -1.0; // 停止降雨的时间（用于湿重衰减）
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
    protected float m_fLastIndoorCheckTime = 0.0; // 上次室内检测时间
    protected const float INDOOR_CHECK_INTERVAL = 1.0; // 室内检测间隔（秒）
    protected bool m_bCachedIndoorState = false; // 缓存的室内状态
    protected ref array<IEntity> m_pCachedBuildings; // 缓存的建筑物列表（用于回调）
    protected float m_fSurfaceWetnessPenalty = 0.0; // 地表湿度惩罚

    // 调试开关：启用后输出室内检测的详细日志
    protected bool m_bIndoorDebug = false; // 默认关闭

    // 是否使用引擎天气API（true=使用引擎数据，false=使用虚拟昼夜模型）
    protected bool m_bUseEngineWeather = true; // 默认启用真实天气

    // ====== 温度计算相关参数（P1 实现） ======
    protected float m_fTempUpdateInterval = 5.0; // 温度步进间隔（秒），默认5s（实时每5秒更新）
    protected float m_fLastTemperatureUpdateTime = 0.0; // 上次温度更新时间（秒）
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

    // 物理模型可调系数（可从 SCR_RSS_Settings 读取）
    protected float m_fCloudBlockingCoeff = 0.7; // 云层遮挡短波的系数（经验）
    protected float m_fLECoef = 200.0; // 潜热系数（W/m2 每单位地表湿度）

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
        m_fSurfaceWetnessPenalty = 0.0;
        
        // 获取天气管理器引用
        if (world)
        {
            ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(world);
            if (chimeraWorld)
                m_pCachedWeatherManager = chimeraWorld.GetTimeAndWeatherManager();
        }

        // 初始化实时检测缓存（若引擎可用）
        if (m_pCachedWeatherManager)
        {
            m_fLastKnownTOD = m_pCachedWeatherManager.GetTimeOfTheDay();
            int y, mo, d;
            m_pCachedWeatherManager.GetDate(y, mo, d);
            m_iLastKnownYear = y;
            m_iLastKnownMonth = mo;
            m_iLastKnownDay = d;
            m_fLastKnownRainIntensity = m_pCachedWeatherManager.GetRainIntensity();
            m_fLastKnownWindSpeed = m_pCachedWeatherManager.GetWindSpeed();
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
        if (m_pCachedWeatherManager && Replication.IsServer() && StaminaConstants.IsDebugEnabled())
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

            PrintFormat("[RealisticSystem][WeatherDebug] OverrideTemp=%1 | TempMin=%2 | TempMax=%3 | Wetness=%4 | Rain=%5 | Wind=%6 | TimeOfDay=%7 | Server=%8 | Extras=%9",
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
            float estLat = 0.0;
            float estLon = 0.0;
            float estConf = EstimateLatLongFromSunriseSunset(estLat, estLon);
            if (estConf > 0.0)
            {
                PrintFormat("[RealisticSystem][LocationEstimate] Estimated Lat=%1 Lon=%2 Conf=%3 (initial)",
                    Math.Round(estLat * 10.0) / 10.0,
                    Math.Round(estLon * 10.0) / 10.0,
                    Math.Round(estConf * 100.0) / 100.0);

                // 若初始置信较低，按需使用天文网格搜索（更慢但更鲁棒）进一步细化
                if (estConf < 0.9)
                {
                    float refinedLat = 0.0;
                    float refinedLon = 0.0;
                    float refinedConf = EstimateLatLongFromAstronomicalSearch(refinedLat, refinedLon);
                    if (refinedConf > estConf)
                    {
                        PrintFormat("[RealisticSystem][LocationEstimate] Refined Lat=%1 Lon=%2 Conf=%3 (improved)",
                            Math.Round(refinedLat * 10.0) / 10.0,
                            Math.Round(refinedLon * 10.0) / 10.0,
                            Math.Round(refinedConf * 100.0) / 100.0);
                    }
                }
            }
        }
        
        // 初始化建筑物列表
        m_pCachedBuildings = new array<IEntity>();
    }

    // 设置室内检测调试开关（用于运行时打开/关闭详细日志）
    void SetIndoorDebug(bool enabled)
    {
        m_bIndoorDebug = enabled;
    }

    bool GetIndoorDebug()
    {
        return m_bIndoorDebug;
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
        if (StaminaConstants.ShouldLog(tmpLogTime))
        {
            m_fNextForceUpdateLogTime = tmpLogTime;
            PrintFormat("[RealisticSystem] ForceUpdate: Pending recompute flagged");
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
        
        // 更新缓存的角色实体引用（用于室内检测）
        if (owner)
            m_pCachedOwner = owner;
        
        // 检查是否需要更新（每 m_fLastEnvironmentCheckTime 秒更新一次），但对管理员的即时修改要实时响应
        bool forceUpdate = false;
        if (m_pCachedWeatherManager)
        {
            // 快速采样当前引擎状态
            float currTOD = m_pCachedWeatherManager.GetTimeOfTheDay();
            int y, mo, d;
            m_pCachedWeatherManager.GetDate(y, mo, d);
            float currRain = m_pCachedWeatherManager.GetRainIntensity();
            float currWind = m_pCachedWeatherManager.GetWindSpeed();
            bool currOverrideTemp = m_pCachedWeatherManager.GetOverrideTemperature();
            float sr = 0.0;
            float ss = 0.0;
            bool hasSR = m_pCachedWeatherManager.GetSunriseHour(sr);
            bool hasSS = m_pCachedWeatherManager.GetSunsetHour(ss);

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
            m_fLastKnownTOD = m_pCachedWeatherManager.GetTimeOfTheDay();
            int y, mo, d;
            m_pCachedWeatherManager.GetDate(y, mo, d);
            m_iLastKnownYear = y;
            m_iLastKnownMonth = mo;
            m_iLastKnownDay = d;
            m_fLastKnownRainIntensity = m_pCachedWeatherManager.GetRainIntensity();
            m_fLastKnownWindSpeed = m_pCachedWeatherManager.GetWindSpeed();
            m_bLastKnownOverrideTemperature = m_pCachedWeatherManager.GetOverrideTemperature();
            float sr = 0.0;
            float ss = 0.0;
            if (m_pCachedWeatherManager.GetSunriseHour(sr))
                m_fLastKnownSunriseHour = sr;
            if (m_pCachedWeatherManager.GetSunsetHour(ss))
                m_fLastKnownSunsetHour = ss;
        }

        // 若之前被标记为 pending，则在安全上下文中执行重算并清理标记
        if (m_bPendingForceUpdate)
        {
            if (m_bUseEngineTemperature && m_bUseEngineWeather && m_pCachedWeatherManager)
            {
                float baseTemp = CalculateTemperatureFromAPI();
                m_fCachedSurfaceTemperature = baseTemp;
                m_fCachedTemperature = m_fCachedSurfaceTemperature;
            }
            else
            {
                float baseTemp = CalculateEquilibriumTemperatureFromPhysics();
                m_fCachedSurfaceTemperature = baseTemp;
                m_fCachedTemperature = m_fCachedSurfaceTemperature;
            }

            // 清理标记
            m_bPendingForceUpdate = false;

            // 日志（使用临时变量以避免 inout 成员写入问题）
            float tmpLogTime2 = m_fNextForceUpdateLogTime;
            if (StaminaConstants.ShouldLog(tmpLogTime2))
            {
                m_fNextForceUpdateLogTime = tmpLogTime2;
                PrintFormat("[RealisticSystem] ForceUpdate: Applied pending recompute: %1°C", Math.Round(m_fCachedSurfaceTemperature * 10.0) / 10.0);
            }
        }

        // 调试信息：环境因子更新（统一节流）
        static float nextEnvLogTime = 0.0;
        if (StaminaConstants.ShouldLog(nextEnvLogTime))
        {
            PrintFormat("[RealisticSystem] 环境因子 / Environment Factors: 虚拟气温=%1°C | 热应激=%2x | 降雨湿重=%3kg | 总湿重=%4kg | 风速=%5m/s | Simulated Temp=%1°C | Heat Stress=%2x | Rain Weight=%3kg | Total Wet Weight=%4kg | Wind Speed=%5m/s",
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
    
    // ==================== 私有方法 ====================
    
    // 计算虚拟气温（基于时间的昼夜温度曲线）
    // 使用余弦函数模拟昼夜温度变化，峰值出现在 14:00
    // @return 虚拟气温（°C）
    protected float CalculateSimulatedTemperature()
    {
        if (!m_pCachedWeatherManager)
            return 15.0; // 默认气温
        
        float currentHour = m_pCachedWeatherManager.GetTimeOfTheDay();
        
        // 昼夜温度模型参数
        const float baseTemp = 15.0;      // 基础气温（清晨最低温）
        const float amplitude = 12.0;     // 昼夜温差幅度
        
        // 使用余弦函数模拟曲线，使峰值出现在 14:00
        // 公式：T(t) = baseTemp + amplitude * cos((t - 14) * π / 12)
        // 14:00 -> 15 + 12 = 27°C (舒适偏热)
        // 02:00 -> 15 - 12 = 3°C (寒冷)
        float simulatedTemp = baseTemp + amplitude * Math.Cos((currentHour - 14.0) * Math.PI / 12.0);
        
        return simulatedTemp;
    }

    // ------------------- 太阳与辐照工具函数（P1） -------------------
    
    // 计算某个日期的年积日（1..365/366）
    protected int DayOfYear(int year, int month, int day)
    {
        int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        // 闰年处理
        bool isLeap = ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
        if (isLeap)
            mdays[1] = 29;

        int n = 0;
        int i = 0;
        while (i < month - 1)
        {
            n += mdays[i];
            i = i + 1;
        }
        n = n + day;
        return n;
    }

    // 太阳偏角（弧度）
    protected float SolarDeclination(int n)
    {
        // δ ≈ 23.44° × sin(2π (284 + n) / 365)
        return 23.44 * Math.DEG2RAD * Math.Sin(2.0 * Math.PI * (284.0 + n) / 365.0);
    }

    // 计算太阳天顶角余弦（cos θ），输入纬度（deg）、年日、时刻（小时）
    protected float SolarCosZenith(float latDeg, int n, float localHour)
    {
        float latRad = latDeg * Math.DEG2RAD;
        float decl = SolarDeclination(n);
        float hourAngleDeg = 15.0 * (localHour - 12.0);
        float hourAngleRad = hourAngleDeg * Math.DEG2RAD;
        float cosTheta = Math.Sin(latRad) * Math.Sin(decl) + Math.Cos(latRad) * Math.Cos(decl) * Math.Cos(hourAngleRad);
        return cosTheta;
    }

    // 估算经纬度（基于引擎提供的日出/日落时间，返回置信度 0.0-1.0）
    // 需要：GetSunriseHour(), GetSunsetHour(), GetDate(), m_fTimeZoneOffsetHours
    protected float EstimateLatLongFromSunriseSunset(out float outLatDeg, out float outLonDeg)
    {
        outLatDeg = 0.0;
        outLonDeg = 0.0;

        if (!m_pCachedWeatherManager)
            return 0.0; // 无天气管理器，无法估算

        float sr = 0.0;
        float ss = 0.0;
        bool hasSR = m_pCachedWeatherManager.GetSunriseHour(sr);
        bool hasSS = m_pCachedWeatherManager.GetSunsetHour(ss);

        if (!hasSR || !hasSS)
            return 0.0; // 缺少关键数据

        // 处理跨日问题
        if (ss < sr)
            ss += 24.0;

        float L = ss - sr; // 昼长，小时
        if (L <= 0.0 || L >= 24.0)
            return 0.0;

        int year, month, day;
        m_pCachedWeatherManager.GetDate(year, month, day);
        int n = DayOfYear(year, month, day);

        // 计算日角 ω0（度）: ω0_deg = 7.5 * L
        float omega0_deg = 7.5 * L; // 15 * (L/2)
        float omega0_rad = omega0_deg * Math.DEG2RAD;

        // 太阳偏角（弧度）
        float decl = SolarDeclination(n); // 返回弧度

        // 检查数值稳定性（tan(decl) 不能接近0）
        float tanDecl = Math.Tan(decl);
        if (Math.AbsFloat(tanDecl) < 1e-6)
            return 0.0; // 不可靠（春/秋分附近）

        // tan(phi) = -cos(omega0) / tan(decl)
        float tanPhi = -Math.Cos(omega0_rad) / tanDecl;

        // 使用稳定的反正切代替（部分平台缺少 Math.Atan）：φ = asin(tanPhi / sqrt(1 + tanPhi^2))
        float denom = Math.Sqrt(1.0 + tanPhi * tanPhi);
        float latRad = Math.Asin(tanPhi / denom);
        float latDeg = latRad * Math.RAD2DEG;

        // 计算经度：基于局部太阳中天
        float t_noon_local = (sr + ss) * 0.5;
        // 归一化
        while (t_noon_local >= 24.0) t_noon_local -= 24.0;

        float tz = m_fTimeZoneOffsetHours; // 假定已填
        float solarNoonUTC = t_noon_local - tz; // 小时

        float lonDeg = 15.0 * (12.0 - solarNoonUTC);
        // 规范化到 -180..180
        while (lonDeg > 180.0) lonDeg -= 360.0;
        while (lonDeg < -180.0) lonDeg += 360.0;

        // 置信度估算：基于昼长、太阳偏角、云量（云会影响日出/日落观测）
        float cloud = InferCloudFactor();
        float conf = 1.0;
        // 减少因云/雨导致的置信度
        conf -= Math.Clamp(cloud * 0.5, 0.0, 0.5);
        // 当太阳偏角接近0(春秋分)置信度下降
        conf -= Math.Clamp(1.0 - Math.AbsFloat(tanDecl) * 1000.0, 0.0, 0.3);
        // 极昼/极夜邻近区域置信度降低
        if (L < 2.0 || L > 22.0) conf -= 0.3;
        conf = Math.Clamp(conf, 0.0, 1.0);

        // 写回成员变量
        m_fLatitude = latDeg;
        m_fLongitude = lonDeg;

        // 日志（使用节流变量，避免传字面量给 ShouldLog）
        float tmpLocationLog = m_fNextLocationEstimateLogTime;
        if (StaminaConstants.ShouldLog(tmpLocationLog))
        {
            m_fNextLocationEstimateLogTime = tmpLocationLog;
            PrintFormat("[RealisticSystem] EstimateLatLong: lat=%1 lon=%2 conf=%3 L=%4 sr=%5 ss=%6 n=%7",
                Math.Round(latDeg * 10.0) / 10.0,
                Math.Round(lonDeg * 10.0) / 10.0,
                Math.Round(conf * 100.0) / 100.0,
                Math.Round(L * 10.0) / 10.0,
                Math.Round(sr * 100.0) / 100.0,
                Math.Round(ss * 100.0) / 100.0,
                n);
        }

        outLatDeg = latDeg;
        outLonDeg = lonDeg;
        return conf;
    }

    // 使用引擎天文函数进行网格搜索以细化经纬度估算（返回置信度 0-1）
    // 利用：GetSunriseHourForDate, GetSunsetHourForDate, GetMoonPhaseForDate
    protected float EstimateLatLongFromAstronomicalSearch(out float outLatDeg, out float outLonDeg)
    {
        outLatDeg = 0.0;
        outLonDeg = 0.0;

        if (!m_pCachedWeatherManager)
            return 0.0;

        // 读取观测日出/日落/月相/时间信息
        float obsSR = 0.0;
        float obsSS = 0.0;
        bool hasSR = m_pCachedWeatherManager.GetSunriseHour(obsSR);
        bool hasSS = m_pCachedWeatherManager.GetSunsetHour(obsSS);
        float obsMoonPhase = m_pCachedWeatherManager.GetMoonPhase(m_pCachedWeatherManager.GetTimeOfTheDay());
        float tod = m_pCachedWeatherManager.GetTimeOfTheDay();
        int year, month, day;
        m_pCachedWeatherManager.GetDate(year, month, day);
        float tz = m_fTimeZoneOffsetHours;
        float dst = m_pCachedWeatherManager.GetDSTOffset();

        if (!hasSR || !hasSS)
            return 0.0;

        if (obsSS < obsSR)
            obsSS += 24.0;

        float obsL = obsSS - obsSR;
        float obsNoon = (obsSR + obsSS) * 0.5;
        while (obsNoon >= 24.0) obsNoon -= 24.0;

        // 搜索参数：粗/细两级网格
        float bestErr = 1e9;
        float bestLat = 0.0;
        float bestLon = 0.0;

        // 权重（可调整）
        float wL = 1.0; // 白昼长度权重
        float wNoon = 0.5; // 中天时差权重
        float wMoon = 0.3; // 月相差异权重

        // 1) 粗网格（步长 5°）
        for (float lat = -85.0; lat <= 85.0; lat += 5.0)
        {
            for (float lon = -180.0; lon <= 180.0; lon += 5.0)
            {
                float sr_c = 0.0;
                float ss_c = 0.0;
                bool okSR = m_pCachedWeatherManager.GetSunriseHourForDate(year, month, day, lat, lon, tz, dst, sr_c);
                bool okSS = m_pCachedWeatherManager.GetSunsetHourForDate(year, month, day, lat, lon, tz, dst, ss_c);

                float penalty = 0.0;
                if (!okSR || !okSS)
                {
                    // 极地季节性无日出/日落，给予大惩罚
                    penalty += 10.0;
                    // 尽量继续但以较差适配度记录
                }

                if (ss_c < sr_c) ss_c += 24.0;
                float Lc = ss_c - sr_c;
                float noon_c = (sr_c + ss_c) * 0.5;
                while (noon_c >= 24.0) noon_c -= 24.0;

                float moon_c = m_pCachedWeatherManager.GetMoonPhaseForDate(year, month, day, tod, tz, dst);

                float err = wL * Math.AbsFloat(obsL - Lc) + wNoon * Math.AbsFloat(obsNoon - noon_c) + wMoon * Math.AbsFloat(obsMoonPhase - moon_c) + penalty;

                if (err < bestErr)
                {
                    bestErr = err;
                    bestLat = lat;
                    bestLon = lon;
                }
            }
        }

        // 2) 细化搜索：在最佳点周围逐级细化
        float searchRadius = 5.0;
        float step = 1.0;
        for (int iter = 0; iter < 3; iter++)
        {
            float localBestErr = bestErr;
            float localBestLat = bestLat;
            float localBestLon = bestLon;

            for (float lat = bestLat - searchRadius; lat <= bestLat + searchRadius; lat += step)
            {
                if (lat < -89.9 || lat > 89.9) continue;
                for (float lon = bestLon - searchRadius; lon <= bestLon + searchRadius; lon += step)
                {
                    float sr_c = 0.0;
                    float ss_c = 0.0;
                    bool okSR = m_pCachedWeatherManager.GetSunriseHourForDate(year, month, day, lat, lon, tz, dst, sr_c);
                    bool okSS = m_pCachedWeatherManager.GetSunsetHourForDate(year, month, day, lat, lon, tz, dst, ss_c);

                    float penalty = 0.0;
                    if (!okSR || !okSS) penalty += 10.0;
                    if (ss_c < sr_c) ss_c += 24.0;
                    float Lc = ss_c - sr_c;
                    float noon_c = (sr_c + ss_c) * 0.5;
                    while (noon_c >= 24.0) noon_c -= 24.0;

                    float moon_c = m_pCachedWeatherManager.GetMoonPhaseForDate(year, month, day, tod, tz, dst);

                    float err = wL * Math.AbsFloat(obsL - Lc) + wNoon * Math.AbsFloat(obsNoon - noon_c) + wMoon * Math.AbsFloat(obsMoonPhase - moon_c) + penalty;

                    if (err < localBestErr)
                    {
                        localBestErr = err;
                        localBestLat = lat;
                        localBestLon = lon;
                    }
                }
            }

            bestErr = localBestErr;
            bestLat = localBestLat;
            bestLon = localBestLon;
            searchRadius = Math.Max(0.5, searchRadius * 0.5);
            step = Math.Max(0.1, step * 0.5);
        }

        // 计算置信度：基于误差大小与云因子
        float maxAcceptableErr = 12.0; // 经验标度（小时），误差越大置信越低
        float errScore = Math.Clamp(bestErr / maxAcceptableErr, 0.0, 1.0);
        float conf = 1.0 - errScore;
        // 云/雨影响置信度
        float cloud = InferCloudFactor();
        conf -= Math.Clamp(cloud * 0.5, 0.0, 0.5);
        conf = Math.Clamp(conf, 0.0, 1.0);

        // 写回与日志
        m_fLatitude = bestLat;
        m_fLongitude = bestLon;

        // 日志（使用节流变量，避免传字面量给 ShouldLog）
        float tmpLocationLog2 = m_fNextLocationEstimateLogTime;
        if (StaminaConstants.ShouldLog(tmpLocationLog2))
        {
            m_fNextLocationEstimateLogTime = tmpLocationLog2;
            PrintFormat("[RealisticSystem] EstimateLatLongAstronomy: lat=%1 lon=%2 conf=%3 bestErr=%4",
                Math.Round(bestLat * 10.0) / 10.0,
                Math.Round(bestLon * 10.0) / 10.0,
                Math.Round(conf * 100.0) / 100.0,
                Math.Round(bestErr * 100.0) / 100.0);
        }

        outLatDeg = bestLat;
        outLonDeg = bestLon;
        return conf;
    }

    // 空气质量近似（Kasten & Young，返回 m）
    protected float AirMass(float cosTheta)
    {
        if (cosTheta <= 0.0)
            return 9999.0; // 夜间或太阳低于地平线
        float thetaDeg = Math.Acos(cosTheta) * Math.RAD2DEG;
        float m = 1.0 / (cosTheta + 0.50572 * Math.Pow(96.07995 - thetaDeg, -1.6364));
        return m;
    }

    // 根据空气质量计算简单的清空透过率（经验）
    protected float ClearSkyTransmittance(float m)
    {
        // 简化模型：tau = exp(-k * m)
        // Enfusion 脚本没有 Math.Exp，使用 Math.Pow(M_E, x) 替代
        float tau = Math.Pow(M_E, -m_fAerosolOpticalDepth * m);
        return Math.Clamp(tau, 0.0, 1.0);
    }

    // 简单云因子推断，从雨强、湿度与天气状态推测云量因子 [0,1]
    protected float InferCloudFactor()
    {
        float cloud = 0.0;
        // 使用降雨强度优先
        cloud = Math.Max(cloud, m_fCachedRainIntensity);
        // 使用地表湿度作为次要指标
        cloud = Math.Max(cloud, m_fCachedSurfaceWetness * 0.8);

        // 基于天气状态名称的增强判断（如果可获得）
        if (m_pCachedWeatherManager)
        {
            BaseWeatherStateTransitionManager transitionManager = m_pCachedWeatherManager.GetTransitionManager();
            if (transitionManager)
            {
                WeatherState currentWeatherState = transitionManager.GetCurrentState();
                if (currentWeatherState)
                {
                    string s = currentWeatherState.GetStateName();
                    s.ToLower();
                    if (s.Contains("storm") || s.Contains("heavy"))
                        cloud = Math.Max(cloud, 0.95);
                    else if (s.Contains("rain") || s.Contains("shower"))
                        cloud = Math.Max(cloud, 0.6);
                    else if (s.Contains("cloud") || s.Contains("overcast"))
                        cloud = Math.Max(cloud, 0.6);
                    else if (s.Contains("partly") || s.Contains("few"))
                        cloud = Math.Max(cloud, 0.25);
                }
            }
        }

        return Math.Clamp(cloud, 0.0, 1.0);
    }

    // 单步温度更新（基于一层混合层的能量平衡）
    // @param dt 秒
    protected void StepTemperature(float dt)
    {
        if (!m_pCachedWeatherManager)
            return;

        // 1) 获取时间/位置/天气因子
        float tod = m_pCachedWeatherManager.GetTimeOfTheDay();
        int year, month, day;
        m_pCachedWeatherManager.GetDate(year, month, day);
        int n = DayOfYear(year, month, day);
        float lat = m_pCachedWeatherManager.GetCurrentLatitude();

        // 使用引擎的日出/日落/月相接口作为权威来源（地图经度可能不可用）
        float localHour = tod;
        if (!m_bUseEngineTimezone)
            localHour -= m_fTimeZoneOffsetHours;
        // 归一化到 [0,24)
        while (localHour < 0.0) localHour += 24.0;
        while (localHour >= 24.0) localHour -= 24.0;

        // 使用引擎的日出/日落 API（不依赖显式经度/经纬），在不可用时回退到基于天顶角的计算
        bool hasSunrise = false;
        bool hasSunset = false;
        float sunRiseHour = 0.0;
        float sunSetHour = 0.0;
        if (m_pCachedWeatherManager)
        {
            hasSunrise = m_pCachedWeatherManager.GetSunriseHour(sunRiseHour);
            hasSunset = m_pCachedWeatherManager.GetSunsetHour(sunSetHour);
        }

        float cosTheta = 0.0;
        if (hasSunrise && hasSunset)
        {
            if (localHour < sunRiseHour || localHour > sunSetHour)
            {
                // 引擎判定为夜间
                cosTheta = 0.0;
            }
            else
            {
                cosTheta = SolarCosZenith(lat, n, localHour);
                if (cosTheta <= 0.0)
                    cosTheta = 0.0;
            }
        }
        else
        {
            // 回退到原有基于天顶角的计算
            cosTheta = SolarCosZenith(lat, n, localHour);
            if (cosTheta <= 0.0)
                cosTheta = 0.0;
        }

        // 获取月相（若引擎可用），供日志或后续扩展使用（使用不依赖经度/经纬的接口）
        float moonPhase01 = 0.0;
        if (m_pCachedWeatherManager)
            moonPhase01 = m_pCachedWeatherManager.GetMoonPhase(tod);

        // 调试信息（仅在 Verbose 时输出）
        if (StaminaConstants.ShouldVerboseLog(m_fLastTemperatureUpdateTime))
        {
            if (hasSunrise && hasSunset)
            {
                PrintFormat("[RealisticSystem][TempStep] Using engine sunrise/sunset: SR=%1, SS=%2, Moon=%3", sunRiseHour, sunSetHour, Math.Round(moonPhase01 * 100.0) / 100.0);
            }
        }

        float I0 = m_fSolarConstant * (1.0 + 0.033 * Math.Cos(2.0 * Math.PI * n / 365.0)) * cosTheta;

        // 3) 计算透过率
        float m = AirMass(cosTheta);
        float tau = ClearSkyTransmittance(m);

        // 4) 云修正
        float cloudFactor = InferCloudFactor();
        float cloudBlocking = m_fCloudBlockingCoeff * cloudFactor; // 云层削弱短波的系数（经验，可配置）

        // 5) 短波到达地表
        float SW_down = I0 * tau * (1.0 - cloudBlocking);

        // 6) 长波下行（简化）
        float T_atm = m_fCachedSurfaceTemperature + 2.0; // 大气温用近地温+偏移近似
        float eps_atm = 0.78 + 0.14 * cloudFactor; // 大气发射率简单模型
        float LW_down = eps_atm * STEFAN_BOLTZMANN * Math.Pow((T_atm + 273.15), 4);

        // 7) 地表发射（近似以空气温代替地表温度）
        float LW_up = m_fSurfaceEmissivity * STEFAN_BOLTZMANN * Math.Pow((m_fCachedSurfaceTemperature + 273.15), 4);

        // 8) 净辐射
        float netRadiation = (1.0 - m_fAlbedo) * SW_down + LW_down - LW_up;

        // 9) 简化感热/潜热项（经验小值，湿润情况增大潜热损失）
        float rho = 1.225; // 空气密度 kg/m3
        float Cp = 1004.0; // 比热 J/(kg·K)
        float Hmix = m_fTemperatureMixingHeight; // m

        float LE = 0.0;
        // 若湿度或降雨增加，加入潜热损失（可配置系数）
        LE = m_fLECoef * m_fCachedSurfaceWetness; // 经验值（可由设置调整）

        // 风速影响混合层高度：风越大，混合层越高，温度变化越小
        float wind_factor = 1.0 + (m_fCachedWindSpeed / 10.0);
        float mixing_height_eff = Math.Max(10.0, Hmix * wind_factor);

        float Q_net = netRadiation - LE; // 忽略感热单独项（混合层近似）

        // 10) 温度变化
        float dT = 0.0;
        if (mixing_height_eff > 0.0)
            dT = (Q_net * dt) / (rho * Cp * mixing_height_eff);

        float newT = m_fCachedSurfaceTemperature + dT;

        // 限制合理范围
        if (newT > 60.0)
            newT = 60.0;
        if (newT < -80.0)
            newT = -80.0;

        // 更新缓存与时间戳
        m_fCachedSurfaceTemperature = newT;

        // 非详细调试输出：只要启用了 Debug（不需 Verbose）就会看到此日志
        if (StaminaConstants.ShouldLog(m_fNextTempStepLogTime))
        {
            PrintFormat("[RealisticSystem][TempStep] dt=%1s | SW=%2W/m2 | NewT=%3°C | Cloud=%4 | MixingH=%5m",
                dt,
                Math.Round(SW_down),
                Math.Round(newT * 10.0) / 10.0,
                Math.Round(cloudFactor * 100.0) / 100.0,
                Math.Round(mixing_height_eff));
        }

        // 详细调试输出（需要 Verbose 打开）
        if (StaminaConstants.ShouldVerboseLog(m_fLastTemperatureUpdateTime))
        {
            PrintFormat("[RealisticSystem][TempStepVerbose] dt=%1s | SW=%2W/m2 | LW_down=%3W/m2 | Net=%4W/m2 | LE=%5 | NewT=%6°C | Cloud=%7",
                dt,
                Math.Round(SW_down),
                Math.Round(LW_down),
                Math.Round(Q_net),
                Math.Round(LE),
                Math.Round(newT * 10.0) / 10.0,
                Math.Round(cloudFactor * 100.0) / 100.0);
        }

        // 非详细调试输出：只要启用了 Debug（不需 Verbose）就会看到此日志
        if (StaminaConstants.ShouldLog(m_fNextTempStepLogTime))
        {
            PrintFormat("[RealisticSystem][TempStep] dt=%1s | SW=%2W/m2 | NewT=%3°C | Cloud=%4 | MixingH=%5m",
                dt,
                Math.Round(SW_down),
                Math.Round(newT * 10.0) / 10.0,
                Math.Round(cloudFactor * 100.0) / 100.0,
                Math.Round(mixing_height_eff));
        }

        // 详细调试输出（需要 Verbose 打开）
        if (StaminaConstants.ShouldVerboseLog(m_fLastTemperatureUpdateTime))
        {
            PrintFormat("[RealisticSystem][TempStepVerbose] dt=%1s | SW=%2W/m2 | LW_down=%3W/m2 | Net=%4W/m2 | LE=%5 | NewT=%6°C | Cloud=%7",
                dt,
                Math.Round(SW_down),
                Math.Round(LW_down),
                Math.Round(Q_net),
                Math.Round(LE),
                Math.Round(newT * 10.0) / 10.0,
                Math.Round(cloudFactor * 100.0) / 100.0);
        }
    }

    // ------------------- 物理平衡求解（用于初始/回退） -------------------
    // 计算给定地表温度下的净辐射（短波+长波 - 地表发射 - 潜热）
    // @param T_surface 地表温度（°C）
    // @param lat 纬度（deg）
    // @param n 年日
    // @param tod 小时
    // @param cloudFactor 云量因子（0..1）
    protected float NetRadiationAtSurface(float T_surface, float lat, int n, float tod, float cloudFactor)
    {
        // 计算太阳几何与顶端辐照：优先使用引擎的日出/日落判定（若可用），否则回退到基于天顶角的计算
        float cosTheta = 0.0;
        if (m_pCachedWeatherManager)
        {
            // 使用不依赖显式经度/经纬的引擎日出/日落接口
            bool hasSR = false;
            bool hasSS = false;
            float sr = 0.0;
            float ss = 0.0;
            hasSR = m_pCachedWeatherManager.GetSunriseHour(sr);
            hasSS = m_pCachedWeatherManager.GetSunsetHour(ss);

            float localHour = tod;
            if (!m_bUseEngineTimezone)
                localHour -= m_fTimeZoneOffsetHours;
            while (localHour < 0.0) localHour += 24.0;
            while (localHour >= 24.0) localHour -= 24.0;

            if (hasSR && hasSS)
            {
                if (localHour < sr || localHour > ss)
                {
                    cosTheta = 0.0; // 夜间
                }
                else
                {
                    cosTheta = SolarCosZenith(lat, n, localHour);
                    if (cosTheta <= 0.0) cosTheta = 0.0;
                }
            }
            else
            {
                cosTheta = SolarCosZenith(lat, n, tod);
                if (cosTheta <= 0.0) cosTheta = 0.0;
            }
        }
        else
        {
            cosTheta = SolarCosZenith(lat, n, tod);
            if (cosTheta <= 0.0)
                cosTheta = 0.0;
        }

        float I0 = m_fSolarConstant * (1.0 + 0.033 * Math.Cos(2.0 * Math.PI * n / 365.0)) * cosTheta;

        // 透过率
        float m = AirMass(cosTheta);
        float tau = ClearSkyTransmittance(m);

        // 云修正
        float cloudBlocking = m_fCloudBlockingCoeff * cloudFactor; // 使用配置的云遮挡系数

        // 短波到达地表
        float SW_down = I0 * tau * (1.0 - cloudBlocking);

        // 长波下行（近似用地表温+2为大气温）
        float T_atm = T_surface + 2.0;
        float eps_atm = 0.78 + 0.14 * cloudFactor;
        float LW_down = eps_atm * STEFAN_BOLTZMANN * Math.Pow((T_atm + 273.15), 4);

        // 地表发射
        float LW_up = m_fSurfaceEmissivity * STEFAN_BOLTZMANN * Math.Pow((T_surface + 273.15), 4);

        // 潜热项（使用当前地表湿度近似，可配置系数）
        float LE = m_fLECoef * m_fCachedSurfaceWetness;
        float net = (1.0 - m_fAlbedo) * SW_down + LW_down - LW_up - LE;

        return net; // W/m2
    }

    // 用二分法求解稳态平衡温度（使净辐射≈0），回退至模拟模型若求解失败
    protected float CalculateEquilibriumTemperatureFromPhysics()
    {
        if (!m_pCachedWeatherManager)
            return CalculateSimulatedTemperature();

        float tod = m_pCachedWeatherManager.GetTimeOfTheDay();
        int year, month, day;
        m_pCachedWeatherManager.GetDate(year, month, day);
        int n = DayOfYear(year, month, day);
        float lat = m_pCachedWeatherManager.GetCurrentLatitude();

        float cloud = InferCloudFactor();

        // 二分法上下界
        float low = -80.0;
        float high = 60.0;
        float mid = 20.0;

        float f_low = NetRadiationAtSurface(low, lat, n, tod, cloud);
        float f_high = NetRadiationAtSurface(high, lat, n, tod, cloud);

        // 若两端符号相同，退用模拟昼夜模型
        if (f_low * f_high > 0.0)
            return CalculateSimulatedTemperature();

        for (int i = 0; i < 40; i++)
        {
            mid = (low + high) * 0.5;
            float f_mid = NetRadiationAtSurface(mid, lat, n, tod, cloud);
            if (Math.AbsFloat(f_mid) < 1.0)
                break; // 误差在 1 W/m2 内可接受
            if (f_mid * f_low <= 0.0)
                high = mid;
            else
                low = mid;
        }

        return Math.Clamp(mid, -80.0, 60.0);
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
    
    // 检测角色是否在室内（基于建筑物边界框 + 向上射线确认）
    // 新方法：先查询周围建筑物，若角色在某建筑物的边界框内则进行向上射线检测（确认有屋顶/覆盖），
    // 只有当边界框判定为"在内"且射线检测也确认有覆盖时，才返回 true（防止开放屋顶/天窗等假阳性）。
    // @param owner 角色实体
    // @return true表示在室内，false表示在室外
    protected bool IsUnderCover(IEntity owner)
    {
        if (!owner)
            return false;
        
        World world = owner.GetWorld();
        if (!world)
            return false;
        
        vector ownerPos = owner.GetOrigin();

        
        // 查询角色周围 50 米范围内的建筑物实体
        vector searchMins = ownerPos + Vector(-50, -50, -50);
        vector searchMaxs = ownerPos + Vector(50, 50, 50);
        
        // 清空建筑物列表
        if (m_pCachedBuildings)
            m_pCachedBuildings.Clear();
        else
            m_pCachedBuildings = new array<IEntity>();
        
        // 使用回调函数收集建筑物
        world.QueryEntitiesByAABB(searchMins, searchMaxs, QueryBuildingCallback);
        
        int buildingCount = m_pCachedBuildings.Count();
        if (m_bIndoorDebug)
            PrintFormat("[RealisticSystem][IndoorDetect] IsUnderCover: ownerPos=(%1,%2,%3) buildingCount=%4", ownerPos[0], ownerPos[1], ownerPos[2], buildingCount);
        if (buildingCount == 0)
            return false;
        
        // 检查角色是否在任何一个建筑物的边界框内，并且通过向上射线确认上方有覆盖
        int checkedBuildings = 0;
        foreach (IEntity building : m_pCachedBuildings)
        {
            if (!building)
                continue;
            
            checkedBuildings++;
            
            // 使用建筑物本地边界 + 世界变换进行 OBB 检测（考虑旋转）
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
                PrintFormat("[RealisticSystem][IndoorDetect] Building #%1 localPos=(%2,%3,%4) mins=(%5,%6,%7) maxs=(%8,%9,%10)",
                    checkedBuildings,
                    Math.Round(localPos[0] * 100.0) / 100.0,
                    Math.Round(localPos[1] * 100.0) / 100.0,
                    Math.Round(localPos[2] * 100.0) / 100.0,
                    buildingMins[0], buildingMins[1], buildingMins[2],
                    buildingMaxs[0], buildingMaxs[1], buildingMaxs[2]);
            
            if (isInside)
            {
                // 已由边界框判定为在建筑物内，进一步使用向上射线检测确认是否有屋顶覆盖
                bool hasRoof = RaycastHasRoof(owner, building);
                if (m_bIndoorDebug)
                {
                    string hasRoofStr;
                    if (hasRoof)
                        hasRoofStr = "true";
                    else
                        hasRoofStr = "false";
                    PrintFormat("[RealisticSystem][IndoorDetect] Building #%1 isInside=true hasRoof=%2", checkedBuildings, hasRoofStr);
                }
                // 如果射线检测也确认被覆盖，则进一步进行水平封闭检测以减少门廊/开放屋顶假阳性
                if (hasRoof)
                {
                    bool enclosed = IsHorizontallyEnclosed(owner);
                    if (m_bIndoorDebug)
                    {
                        string enclosedStr;
                        if (enclosed)
                            enclosedStr = "true";
                        else
                            enclosedStr = "false";
                        PrintFormat("[RealisticSystem][IndoorDetect] Building #%1 roof=true enclosed=%2", checkedBuildings, enclosedStr);
                    }
                    if (enclosed)
                        return true;
                    // 否则继续检查其它建筑物
                }
                // 否则继续检查其它建筑物
            }
        }
        
        if (m_bIndoorDebug)
            PrintFormat("[RealisticSystem][IndoorDetect] No indoor building found after checking %1 buildings", checkedBuildings);
        // 未找到既在建筑物内又有屋顶覆盖的情况
        return false;
    }

    // 向上射线检测上方是否存在覆盖（屋顶/天花板）
    // 多点采样（中心 + 前后左右）以减少窗户或较小开口导致的误判
    // 仅检测当前建筑物，避免邻近物体或地形导致假阳性
    // @param owner 要检测的实体（用于获取位置/世界）
    // @param building 当前候选建筑物
    // @return true表示所有样本点上方在检测高度内都命中遮挡物（有屋顶），false表示至少有一处无覆盖
    protected bool RaycastHasRoof(IEntity owner, IEntity building)
    {
        if (!owner || !building)
            return false;
        World world = owner.GetWorld();
        if (!world)
            return false;

        vector basePos = owner.GetOrigin();
        // 从头部高度开始检测（单位：米），可根据需要调整
        const float HEAD_HEIGHT = 1.6;
        const float CHECK_HEIGHT = StaminaConstants.ENV_INDOOR_CHECK_HEIGHT; // 通用配置（如 10 米）
        const float SAMPLE_OFFSET = 0.4; // 采样点水平偏移（米）

        array<vector> samples = { vector.Zero, vector.Forward * SAMPLE_OFFSET, -vector.Forward * SAMPLE_OFFSET, vector.Right * SAMPLE_OFFSET, -vector.Right * SAMPLE_OFFSET };

        if (m_bIndoorDebug)
            PrintFormat("[RealisticSystem][IndoorDetect] RaycastHasRoof: ownerPos=(%1,%2,%3) HEAD_HEIGHT=%4 CHECK_HEIGHT=%5 samples=%6",
                basePos[0], basePos[1], basePos[2], HEAD_HEIGHT, CHECK_HEIGHT, samples.Count());

        int idx = 0;
        foreach (vector off : samples)
        {
            idx++;
            vector start = basePos + vector.Up * HEAD_HEIGHT + off;
            vector end = start + vector.Up * CHECK_HEIGHT;

            TraceParam param = new TraceParam();
            param.Start = start;
            param.End = end;
            param.Flags = TraceFlags.ENTS;
            param.Include = building;
            param.Exclude = owner;
            param.LayerMask = EPhysicsLayerDefs.Projectile;

            world.TraceMove(param, null);

            bool hit = (param.TraceEnt != null);

            if (m_bIndoorDebug)
            {
                string surface;
                if (param.SurfaceProps)
                    surface = param.SurfaceProps.ToString();
                else
                    surface = "null";
                PrintFormat("[RealisticSystem][IndoorDetect] Sample %1 start=(%2,%3,%4) end=(%5,%6,%7) -> TraceEnt=%8 Collider=%9",
                    idx, start[0], start[1], start[2], end[0], end[1], end[2], param.TraceEnt, param.ColliderName);
                PrintFormat("[RealisticSystem][IndoorDetect]   Surface=%1", surface);
            }

            // 如果任一采样点没有命中任何遮挡物，则不能确认为室内
            if (!hit)
            {
                if (m_bIndoorDebug)
                    PrintFormat("[RealisticSystem][IndoorDetect] Sample %1 missed -> not indoor", idx);
                return false;
            }
        }

        if (m_bIndoorDebug)
            PrintFormat("[RealisticSystem][IndoorDetect] All samples hit -> indoor");

        // 所有采样点都命中遮挡物 → 确认为室内
        return true;
    }

    // 将世界坐标点转换到实体本地坐标（仅适用于正交旋转矩阵）
    protected vector WorldToLocal(vector worldMat[4], vector worldPos)
    {
        vector delta = worldPos - worldMat[3];

        vector localPos;
        localPos[0] = vector.Dot(delta, worldMat[0]);
        localPos[1] = vector.Dot(delta, worldMat[1]);
        localPos[2] = vector.Dot(delta, worldMat[2]);

        return localPos;
    }

    // 水平径向封闭检测：在头部高度向周围发多条短射线以验证是否被墙体包围
    // 若被足够比例的射线在短距离内命中，则认为水平封闭（有墙）
    // @param owner 要检测的实体
    // @return true 表示在水平方向上被包围（更可信的室内），false 表示至少有多个方向开放
    protected bool IsHorizontallyEnclosed(IEntity owner)
    {
        if (!owner)
            return false;
        World world = owner.GetWorld();
        if (!world)
            return false;

        vector basePos = owner.GetOrigin();
        const float HEAD_HEIGHT = 1.6;
        const int SAMPLES = 8; // 8向采样
        const float DIST = 1.2; // 1.2 米检测距离（可根据需要调整）
        const float HIT_RATIO = 0.75; // 至少 75% 的射线命中视为封闭

        int hits = 0;

        for (int i = 0; i < SAMPLES; i++)
        {
            float angle = (360.0 / SAMPLES) * i;
            const float DEG2RAD = 3.14159265 / 180.0;
            float rad = angle * DEG2RAD;
            vector dir = Vector(Math.Cos(rad), Math.Sin(rad), 0);

            vector start = basePos + vector.Up * HEAD_HEIGHT;
            vector end = start + dir * DIST;

            TraceParam param = new TraceParam();
            param.Start = start;
            param.End = end;
            param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
            param.Exclude = owner;
            param.LayerMask = EPhysicsLayerPresets.Projectile;

            world.TraceMove(param, null);

            bool hit = (param.TraceEnt != null) || (param.SurfaceProps != null) || (param.ColliderName != string.Empty);
            if (hit) hits++;

            if (m_bIndoorDebug)
            {
                string hitStr;
                if (hit)
                    hitStr = "true";
                else
                    hitStr = "false";
                PrintFormat("[RealisticSystem][IndoorDetect] Horizontal sample %1 angle=%2 hit=%3", i + 1, Math.Round(angle), hitStr);
            }
        }

        float ratio = (hits / (float)SAMPLES);
        if (m_bIndoorDebug)
            PrintFormat("[RealisticSystem][IndoorDetect] Horizontal enclosure hits=%1/%2 ratio=%3", hits, SAMPLES, Math.Round(ratio * 100.0) / 100.0);

        return (ratio >= HIT_RATIO);
    }

    // 建筑物查询回调（过滤建筑物实体）
    // @param e 实体
    // @return true表示继续查询，false表示停止
    protected bool QueryBuildingCallback(IEntity e)
    {
        if (!e)
            return true;
        
        // 只接受建筑物类型的实体
        Building building = Building.Cast(e);
        if (!building)
            return true;
        
        // 排除角色自身
        if (ChimeraCharacter.Cast(e))
            return true;
        
        // 添加到建筑物列表
        m_pCachedBuildings.Insert(e);
        
        return true;
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
    
    // 检查是否在室内（用于调试）
    // @return true表示在室内（有遮挡），false表示在室外（无遮挡）
    bool IsIndoor()
    {
        return IsUnderCover(m_pCachedOwner);
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
    }

    // 外部调用：在配置发生改变时调用以立即应用新设置
    void OnConfigUpdated()
    {
        ApplySettings();
    }
    
    // 检查是否正在下雨（用于调试）
// 判断是否正在下雨（基于降雨强度）
    // @return true表示正在下雨，false表示未下雨
    bool IsRaining()
    {
        return m_fCachedRainIntensity >= StaminaConstants.ENV_RAIN_INTENSITY_THRESHOLD;
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
        
        // 5. 获取当前气温（根据配置选择：使用引擎温度或完全由模组计算）
        if (m_bUseEngineTemperature && m_bUseEngineWeather && m_pCachedWeatherManager)
        {
            // 使用引擎提供的 min/max 值作为物理模型的边界与初始值
            float baseTemp = CalculateTemperatureFromAPI();
            // 首次采样时初始化缓存表面温度并设置时间戳，避免一次性大步进
            if (m_fLastTemperatureUpdateTime <= 0.0 && m_fCachedSurfaceTemperature == 20.0)
            {
                m_fCachedSurfaceTemperature = baseTemp;
                m_fLastTemperatureUpdateTime = currentTime; // 初始化时间戳
            }

            // 以固定间隔步进温度（每 m_fTempUpdateInterval 秒一次）
            float tempDelta = currentTime - m_fLastTemperatureUpdateTime;
            if (tempDelta >= m_fTempUpdateInterval)
            {
                StepTemperature(tempDelta);
                m_fLastTemperatureUpdateTime = currentTime;
                // 安排下次非详细日志时间，确保步进日志按间隔显示
                m_fNextTempStepLogTime = currentTime + m_fTempUpdateInterval;
            }

            // 将缓存的表面温度作为当前气温输出
            m_fCachedTemperature = m_fCachedSurfaceTemperature;
        }
        else
        {
            // 不使用引擎温度：完全由模组内部计算
            // 首先使用物理平衡估算作为初始温度（若求解失败会退回到模拟昼夜曲线）
            float baseTemp = CalculateEquilibriumTemperatureFromPhysics();

            if (m_fLastTemperatureUpdateTime <= 0.0 && m_fCachedSurfaceTemperature == 20.0)
            {
                m_fCachedSurfaceTemperature = baseTemp;
                m_fLastTemperatureUpdateTime = currentTime; // 初始化时间戳
            }

            // 每 m_fTempUpdateInterval 秒进行一步进
            float tempDelta = currentTime - m_fLastTemperatureUpdateTime;
            if (tempDelta >= m_fTempUpdateInterval)
            {
                StepTemperature(tempDelta);
                m_fLastTemperatureUpdateTime = currentTime;
                // 安排下次非详细日志时间，确保步进日志按间隔显示
                m_fNextTempStepLogTime = currentTime + m_fTempUpdateInterval;
            }

            m_fCachedTemperature = m_fCachedSurfaceTemperature;
        }
        
        // 6. 获取地表湿度
        m_fCachedSurfaceWetness = CalculateSurfaceWetnessFromAPI();
        
        // 7. 计算降雨湿重（基于降雨强度）
        CalculateRainWetWeight(deltaTime);
        
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
        if (StaminaConstants.ShouldVerboseLog(nextAdvancedEnvLogTime))
        {
            PrintFormat("[RealisticSystem] 高级环境因子 / Advanced Environment Factors:");
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
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        // 尝试获取降雨强度（API方法）
        float rainIntensity = 0.0;
        
        // API调用：获取降雨强度
        rainIntensity = m_pCachedWeatherManager.GetRainIntensity();
        
        // 如果API返回有效值，直接使用
        if (rainIntensity > StaminaConstants.ENV_RAIN_INTENSITY_THRESHOLD)
            return rainIntensity;
        
        // 回退方案：基于天气状态名称判断
        return CalculateRainIntensityFromStateName();
    }
    
    // 基于状态名称判断降雨强度（回退方案）
    // @return 降雨强度（0.0-1.0）
    protected float CalculateRainIntensityFromStateName()
    {
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        // 获取当前天气状态
        BaseWeatherStateTransitionManager transitionManager = m_pCachedWeatherManager.GetTransitionManager();
        if (!transitionManager)
            return 0.0;
        
        WeatherState currentWeatherState = transitionManager.GetCurrentState();
        if (!currentWeatherState)
            return 0.0;
        
        string stateName = currentWeatherState.GetStateName();
        stateName.ToLower();
        
        // 基于状态名称判断降雨强度
        if (stateName.Contains("storm") || stateName.Contains("heavy"))
            return 0.9;
        else if (stateName.Contains("rain") || stateName.Contains("shower"))
            return 0.5;
        else if (stateName.Contains("drizzle") || stateName.Contains("light"))
            return 0.2;
        else if (stateName.Contains("cloudy") || stateName.Contains("overcast"))
            return 0.05;
        else
            return 0.0;
    }
    
    // 从API获取风速
    // @return 风速（m/s）
    protected float CalculateWindSpeedFromAPI()
    {
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        // API调用：获取风速
        float windSpeed = m_pCachedWeatherManager.GetWindSpeed();
        
        return windSpeed;
    }
    
    // 从API获取风向
    // @return 风向（度，0-360）
    protected float CalculateWindDirectionFromAPI()
    {
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        // API调用：获取风向
        float windDirection = m_pCachedWeatherManager.GetWindDirection();
        
        return windDirection;
    }
    
    // 计算风阻系数（基于玩家移动方向）
    // @param playerVelocity 玩家速度向量
    // @return 风阻系数（0.0-1.0）
    protected float CalculateWindDrag(vector playerVelocity)
    {
        if (m_fCachedWindSpeed < StaminaConstants.ENV_WIND_SPEED_THRESHOLD)
            return 0.0;
        
        // 获取玩家移动向量（水平）
        vector playerVel = playerVelocity;
        playerVel[1] = 0; // 忽略垂直分量
        
        float speed = playerVel.Length();
        if (speed < 0.1)
            return 0.0; // 玩家静止，无风阻
        
        // 转换风向为向量
        float windDirRad = m_fCachedWindDirection * Math.DEG2RAD;
        vector windDirVec = Vector(Math.Sin(windDirRad), 0, Math.Cos(windDirRad));
        
        // 计算玩家速度与风向的投影
        // Dot > 0 为顺风, Dot < 0 为逆风
        float windProjection = vector.Dot(playerVel.Normalized(), windDirVec);
        
        // 只计算逆风阻力
        if (windProjection >= 0)
            return 0.0; // 顺风，无阻力
        
        // 计算风阻系数：逆风越强，阻力越大
        float windResistance = Math.AbsFloat(windProjection) * m_fCachedWindSpeed * StaminaConstants.ENV_WIND_RESISTANCE_COEFF;
        
        return Math.Clamp(windResistance, 0.0, 1.0);
    }
    
    // 从API获取泥泞度系数
    // @return 泥泞度系数（0.0-1.0）
    protected float CalculateMudFactorFromAPI()
    {
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        // API调用：获取积水程度
        float puddles = m_pCachedWeatherManager.GetCurrentWaterAccumulationPuddles();
        
        return puddles;
    }
    
    // 从API获取当前气温（使用TimeAndWeather的Min/Max进行昼夜插值计算）
    // @return 当前气温（°C）
    protected float CalculateTemperatureFromAPI()
    {
        if (!m_pCachedWeatherManager)
            return 20.0; // 默认20°C

        // 优先使用 TimeAndWeather 提供的日间最小/最大气温作为当日极值
        float tempMin = m_pCachedWeatherManager.GetTemperatureAirMinOverride();
        float tempMax = m_pCachedWeatherManager.GetTemperatureAirMaxOverride();

        // 若 min/max 几乎相等（例如被固定为同一值），认为这是常数或覆盖导致，回退到模拟昼夜模型
        if (Math.AbsFloat(tempMax - tempMin) < 0.05)
        {
            PrintFormat("[RealisticSystem] Warning: Temperature min/max nearly equal (%1/%2). Attempting physical equilibrium estimate.", tempMin, tempMax);
            // 若引擎给出的 min/max 是常数或被覆盖，尝试用物理平衡模型求初始温度，回退到模拟昼夜模型仅在无天气管理器时
            if (m_pCachedWeatherManager)
                return CalculateEquilibriumTemperatureFromPhysics();
            else
                return CalculateSimulatedTemperature();
        }

        // 使用余弦昼夜曲线（峰值出现在 14:00）基于 min/max 进行插值
        float tod = m_pCachedWeatherManager.GetTimeOfTheDay();
        float mean = (tempMin + tempMax) * 0.5;
        float amplitude = (tempMax - tempMin) * 0.5;
        float computedTemp = mean + amplitude * Math.Cos((tod - 14.0) * Math.PI / 12.0);

        // 限制在 min/max 范围内并返回
        float low;
        if (tempMin < tempMax)
            low = tempMin;
        else
            low = tempMax;

        float high;
        if (tempMax > tempMin)
            high = tempMax;
        else
            high = tempMin;

        computedTemp = Math.Clamp(computedTemp, low, high);

        return computedTemp;
    }
    
    // 从API获取地表湿度
    // @return 地表湿度（0.0-1.0）
    protected float CalculateSurfaceWetnessFromAPI()
    {
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        // API调用：获取地表湿度
        float wetness = m_pCachedWeatherManager.GetCurrentWetness();
        
        return wetness;
    }
    
    // 计算降雨湿重（基于降雨强度）
    // @param deltaTime 时间增量（秒）
    protected void CalculateRainWetWeight(float deltaTime)
    {
        if (deltaTime <= 0)
            return;
        
        // 检查是否在室内（室内不受降雨影响）
        bool isIndoor = IsUnderCover(m_pCachedOwner);
        
        // 检查是否在室外且正在下雨（超过阈值）
        bool isOutdoorAndRaining = !isIndoor && m_fCachedRainIntensity >= StaminaConstants.ENV_RAIN_INTENSITY_THRESHOLD;
        
        if (isOutdoorAndRaining)
        {
            // 在室外且正在下雨：增加湿重
            // 计算湿重增加速率（非线性增长）
            float accumulationRate = StaminaConstants.ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE * 
                                     Math.Pow(m_fCachedRainIntensity, StaminaConstants.ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT);
            
            // 增加湿重
            m_fCachedRainWeight = Math.Clamp(
                m_fCachedRainWeight + accumulationRate * deltaTime,
                0.0,
                StaminaConstants.ENV_MAX_TOTAL_WET_WEIGHT
            );
        }
        else
        {
            // 其他所有情况：减少湿重或为0
            if (m_fCachedRainWeight > 0.0)
            {
                // 计算衰减速率
                float decayRate = 1.0 / StaminaConstants.ENV_RAIN_WEIGHT_DURATION;
                float decayAmount = m_fCachedRainWeight * decayRate * deltaTime;
                
                // 减少湿重
                m_fCachedRainWeight = Math.Max(m_fCachedRainWeight - decayAmount, 0.0);
                
                // 如果湿重完全消失，重置状态
                if (m_fCachedRainWeight <= 0.0)
                {
                    m_fCachedRainWeight = 0.0;
                    m_fRainStopTime = -1.0;
                    m_fLastRainIntensity = 0.0;
                }
            }
        }
    }
    
    // 计算暴雨呼吸阻力
    protected void CalculateRainBreathingPenalty()
    {
        // 只有暴雨才会触发呼吸阻力
        if (m_fCachedRainIntensity < StaminaConstants.ENV_RAIN_INTENSITY_HEAVY_THRESHOLD)
        {
            m_fRainBreathingPenalty = 0.0;
            return;
        }
        
        // 计算呼吸阻力（基于降雨强度）
        m_fRainBreathingPenalty = StaminaConstants.ENV_RAIN_INTENSITY_BREATHING_PENALTY * 
                                   (m_fCachedRainIntensity - StaminaConstants.ENV_RAIN_INTENSITY_HEAVY_THRESHOLD);
    }
    
    // 计算泥泞地形系数
    protected void CalculateMudTerrainFactor()
    {
        // 只有在非铺装路面才计算泥泞惩罚
        if (m_fCachedTerrainFactor <= 1.0)
        {
            m_fMudTerrainFactor = 0.0;
            return;
        }
        
        // 计算泥泞惩罚（基于积水程度）
        m_fMudTerrainFactor = m_fCachedMudFactor * StaminaConstants.ENV_MUD_PENALTY_MAX;
    }
    
    // 计算泥泞Sprint惩罚
    protected void CalculateMudSprintPenalty()
    {
        // 只有在泥泞路面才计算Sprint惩罚
        if (m_fCachedMudFactor < StaminaConstants.ENV_MUD_SLIPPERY_THRESHOLD)
        {
            m_fMudSprintPenalty = 0.0;
            return;
        }
        
        // 计算Sprint惩罚（基于泥泞度）
        m_fMudSprintPenalty = StaminaConstants.ENV_MUD_SPRINT_PENALTY * m_fCachedMudFactor;
    }
    
    // 计算滑倒风险
    protected void CalculateSlipRisk()
    {
        // 只有在泥泞路面才计算滑倒风险
        if (m_fCachedMudFactor < StaminaConstants.ENV_MUD_SLIPPERY_THRESHOLD)
        {
            m_fSlipRisk = 0.0;
            return;
        }
        
        // 计算滑倒风险（基于泥泞度）
        m_fSlipRisk = StaminaConstants.ENV_MUD_SLIP_RISK_BASE * m_fCachedMudFactor;
    }
    
    // 计算热应激惩罚
    protected void CalculateHeatStressPenalty()
    {
        // 只有超过热应激阈值才计算
        if (m_fCachedTemperature <= StaminaConstants.ENV_TEMPERATURE_HEAT_THRESHOLD)
        {
            m_fHeatStressPenalty = 0.0;
            return;
        }
        
        // 计算热应激惩罚（每高1度，恢复率降低2%）
        float heatPenaltyCoeff = StaminaConstants.GetEnvTemperatureHeatPenaltyCoeff();
        m_fHeatStressPenalty = (m_fCachedTemperature - StaminaConstants.ENV_TEMPERATURE_HEAT_THRESHOLD) * 
                               heatPenaltyCoeff;
    }
    
    // 计算冷应激惩罚
    protected void CalculateColdStressPenalty()
    {
        // 只有低于冷应激阈值才计算
        if (m_fCachedTemperature >= StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD)
        {
            m_fColdStressPenalty = 0.0;
            m_fColdStaticPenalty = 0.0;
            return;
        }
        
        // 计算冷应激惩罚（每低1度，恢复率降低5%）
        float coldRecoveryPenaltyCoeff = StaminaConstants.GetEnvTemperatureColdRecoveryPenaltyCoeff();
        m_fColdStressPenalty = (StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD - m_fCachedTemperature) * 
                               coldRecoveryPenaltyCoeff;
        
        // 计算冷应激静态惩罚（每低1度，静态消耗增加3%）
        m_fColdStaticPenalty = (StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD - m_fCachedTemperature) * 
                               StaminaConstants.ENV_TEMPERATURE_COLD_STATIC_PENALTY;
    }
    
    // 计算地表湿度惩罚
    // @param owner 角色实体
    // @param stance 当前姿态（0=站立，1=蹲姿，2=趴姿）
    protected void CalculateSurfaceWetnessPenalty(IEntity owner, int stance = 0)
    {
        if (!owner)
            return;
        
        // 只有趴下姿态才计算地表湿度惩罚
        if (stance != 2) // 不是趴姿
        {
            m_fSurfaceWetnessPenalty = 0.0;
            return;
        }
        
        // 只有在地表湿度超过阈值时才计算
        if (m_fCachedSurfaceWetness < StaminaConstants.ENV_SURFACE_WETNESS_THRESHOLD)
        {
            m_fSurfaceWetnessPenalty = 0.0;
            return;
        }
        
        // 计算地表湿度惩罚（趴下时的恢复惩罚）
        float surfaceWetnessPenaltyMax = StaminaConstants.GetEnvSurfaceWetnessPenaltyMax();
        m_fSurfaceWetnessPenalty = surfaceWetnessPenaltyMax * m_fCachedSurfaceWetness;
    }
}
