// 环境因子模块
// 负责处理环境因素对体力系统的影响（热应激、降雨湿重等）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class EnvironmentFactor
{
    // ==================== 状态变量 ====================
    protected float m_fCachedHeatStressMultiplier = 1.0; // 缓存的热应激倍数
    protected float m_fCachedRainWeight = 0.0; // 缓存的降雨湿重（kg）
    protected float m_fLastEnvironmentCheckTime = 0.0; // 上次环境检测时间
    protected bool m_bWasRaining = false; // 上一帧是否在下雨
    protected float m_fRainStopTime = -1.0; // 停止降雨的时间（用于湿重衰减）
    protected TimeAndWeatherManagerEntity m_pCachedWeatherManager; // 缓存的天气管理器引用
    protected float m_fLastRainIntensity = 0.0; // 上次检测到的降雨强度（用于衰减计算）
    protected IEntity m_pCachedOwner; // 缓存的角色实体引用（用于室内检测）
    
    // ==================== 公共方法 ====================
    
    // 初始化环境因子模块
    // @param world 世界对象（用于获取天气管理器）
    // @param owner 角色实体（用于室内检测，可为null）
    void Initialize(World world = null, IEntity owner = null)
    {
        m_fCachedHeatStressMultiplier = 1.0;
        m_fCachedRainWeight = 0.0;
        m_fLastEnvironmentCheckTime = 0.0;
        m_bWasRaining = false;
        m_fRainStopTime = -1.0;
        m_fLastRainIntensity = 0.0;
        m_pCachedWeatherManager = null;
        m_pCachedOwner = owner;
        
        // 获取天气管理器引用
        if (world)
        {
            ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(world);
            if (chimeraWorld)
                m_pCachedWeatherManager = chimeraWorld.GetTimeAndWeatherManager();
        }
    }
    
    // 更新环境因子（每5秒调用一次，性能优化）
    // @param currentTime 当前世界时间
    // @param owner 角色实体（用于室内检测，可为null）
    // @return 是否需要更新（true表示已更新，false表示跳过）
    bool UpdateEnvironmentFactors(float currentTime, IEntity owner = null)
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
        
        // 检查是否需要更新（每5秒更新一次）
        if (currentTime - m_fLastEnvironmentCheckTime < StaminaConstants.ENV_CHECK_INTERVAL)
            return false;
        
        m_fLastEnvironmentCheckTime = currentTime;
        
        // 更新热应激倍数（考虑室内豁免）
        m_fCachedHeatStressMultiplier = CalculateHeatStressMultiplier(m_pCachedOwner);
        
        // 更新降雨湿重
        m_fCachedRainWeight = CalculateRainWeight(currentTime);
        
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
    
    // ==================== 私有方法 ====================
    
    // 计算热应激倍数（基于时间段，考虑室内豁免）
    // 热应激模型：10:00-14:00 逐渐增加，14:00-18:00 逐渐减少
    // 峰值（14:00）时消耗增加30%
    // 如果角色在室内（头顶有遮挡），热应激减少50%
    // @param owner 角色实体（用于室内检测，可为null）
    // @return 热应激倍数（1.0-1.3）
    protected float CalculateHeatStressMultiplier(IEntity owner = null)
    {
        if (!m_pCachedWeatherManager)
            return 1.0;
        
        float currentHour = m_pCachedWeatherManager.GetTimeOfTheDay();
        
        // 热应激时间段：10:00-18:00
        if (currentHour < StaminaConstants.ENV_HEAT_STRESS_START_HOUR || currentHour >= StaminaConstants.ENV_HEAT_STRESS_END_HOUR)
        {
            // 不在热应激时间段，无影响
            return StaminaConstants.ENV_HEAT_STRESS_BASE_MULTIPLIER;
        }
        
        float multiplier = 1.0;
        
        if (currentHour < StaminaConstants.ENV_HEAT_STRESS_PEAK_HOUR)
        {
            // 10:00-14:00：逐渐增加
            float t = (currentHour - StaminaConstants.ENV_HEAT_STRESS_START_HOUR) / 
                      (StaminaConstants.ENV_HEAT_STRESS_PEAK_HOUR - StaminaConstants.ENV_HEAT_STRESS_START_HOUR);
            t = Math.Clamp(t, 0.0, 1.0);
            multiplier = StaminaConstants.ENV_HEAT_STRESS_BASE_MULTIPLIER + 
                        (StaminaConstants.ENV_HEAT_STRESS_MAX_MULTIPLIER - StaminaConstants.ENV_HEAT_STRESS_BASE_MULTIPLIER) * t;
        }
        else
        {
            // 14:00-18:00：逐渐减少
            float t = (currentHour - StaminaConstants.ENV_HEAT_STRESS_PEAK_HOUR) / 
                      (StaminaConstants.ENV_HEAT_STRESS_END_HOUR - StaminaConstants.ENV_HEAT_STRESS_PEAK_HOUR);
            t = Math.Clamp(t, 0.0, 1.0);
            multiplier = StaminaConstants.ENV_HEAT_STRESS_MAX_MULTIPLIER - 
                        (StaminaConstants.ENV_HEAT_STRESS_MAX_MULTIPLIER - StaminaConstants.ENV_HEAT_STRESS_BASE_MULTIPLIER) * t;
        }
        
        // 室内豁免：如果角色在室内（头顶有遮挡），热应激减少
        if (owner && IsUnderCover(owner))
        {
            // 室内热应激 = 基础倍数 + (室外热应激 - 基础倍数) × (1 - 室内减少比例)
            float outdoorStress = multiplier - StaminaConstants.ENV_HEAT_STRESS_BASE_MULTIPLIER;
            multiplier = StaminaConstants.ENV_HEAT_STRESS_BASE_MULTIPLIER + 
                        outdoorStress * (1.0 - StaminaConstants.ENV_HEAT_STRESS_INDOOR_REDUCTION);
        }
        
        return Math.Clamp(multiplier, 1.0, StaminaConstants.ENV_HEAT_STRESS_MAX_MULTIPLIER);
    }
    
    // 检测角色是否在室内（头顶是否有遮挡）
    // @param owner 角色实体
    // @return true表示在室内（有遮挡），false表示在室外（无遮挡）
    protected bool IsUnderCover(IEntity owner)
    {
        if (!owner)
            return false;
        
        World world = owner.GetWorld();
        if (!world)
            return false;
        
        // 从角色位置向上检测，判断是否有遮挡
        vector ownerPos = owner.GetOrigin();
        vector traceStart = ownerPos;
        traceStart[1] = traceStart[1] + 0.1; // 角色位置上方 0.1 米
        vector traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + StaminaConstants.ENV_INDOOR_CHECK_HEIGHT; // 向上检测 10 米
        
        TraceParam traceParam = new TraceParam();
        traceParam.Start = traceStart;
        traceParam.End = traceEnd;
        traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam.Exclude = owner;
        traceParam.LayerMask = EPhysicsLayerPresets.Projectile;
        
        // 执行追踪
        world.TraceMove(traceParam, null);
        
        // 如果命中物体（End位置被修改），说明头顶有遮挡（在室内）
        // TraceMove 会修改 traceParam.End 为命中点，如果未命中则 End 保持不变
        float hitDistance = vector.Distance(traceParam.Start, traceParam.End);
        float expectedDistance = StaminaConstants.ENV_INDOOR_CHECK_HEIGHT;
        
        // 如果实际距离明显小于预期距离（95%阈值），说明命中物体
        return (hitDistance < expectedDistance * 0.95);
    }
    
    // 计算降雨湿重（基于天气状态）
    // 优先尝试使用 GetRainIntensity() API，如果没有则回退到字符串匹配
    // 停止降雨后，湿重使用二次方衰减（更自然的蒸发过程）
    // @param currentTime 当前世界时间
    // @return 降雨湿重（0.0-8.0 kg）
    protected float CalculateRainWeight(float currentTime)
    {
        if (!m_pCachedWeatherManager)
            return 0.0;
        
        float rainIntensity = 0.0;
        bool isRaining = false;
        
        // 优先尝试使用 GetRainIntensity() API（如果存在）
        // 注意：如果 API 不存在，编译器会报错，需要注释掉或使用条件编译
        // 暂时保留字符串匹配作为主要方法，GetRainIntensity 作为备选
        
        // 获取当前天气状态
        BaseWeatherStateTransitionManager transitionManager = m_pCachedWeatherManager.GetTransitionManager();
        if (!transitionManager)
            return 0.0;
        
        WeatherState currentWeatherState = transitionManager.GetCurrentState();
        if (!currentWeatherState)
            return 0.0;
        
        string weatherStateName = currentWeatherState.GetStateName();
        
        // 方法1：尝试使用字符串匹配（当前主要方法）
        if (weatherStateName.Contains("Rain") || weatherStateName.Contains("rain") || 
            weatherStateName.Contains("RAIN") || weatherStateName.Contains("Rain"))
        {
            isRaining = true;
            
            // 根据天气状态名称判断强度
            bool isHeavyRain = weatherStateName.Contains("heavy") || weatherStateName.Contains("Heavy") || 
                               weatherStateName.Contains("storm") || weatherStateName.Contains("Storm") ||
                               weatherStateName.Contains("HEAVY") || weatherStateName.Contains("STORM");
            
            if (isHeavyRain)
            {
                rainIntensity = 1.0; // 暴雨：100%强度
            }
            else
            {
                rainIntensity = 0.5; // 小雨/中雨：50%强度
            }
        }
        
        // 方法2：如果未来 API 支持，可以尝试直接获取降雨强度
        // rainIntensity = m_pCachedWeatherManager.GetRainIntensity(); // 假设此方法存在
        // isRaining = (rainIntensity > 0.01);
        
        // 检测降雨状态变化
        if (isRaining && !m_bWasRaining)
        {
            // 开始下雨：重置停止时间，保存当前强度
            m_fRainStopTime = -1.0;
            m_fLastRainIntensity = rainIntensity;
        }
        else if (!isRaining && m_bWasRaining)
        {
            // 停止下雨：记录停止时间和最后强度
            m_fRainStopTime = currentTime;
        }
        
        m_bWasRaining = isRaining;
        
        // 计算湿重
        float rainWeight = 0.0;
        
        if (isRaining)
        {
            // 正在下雨：根据降雨强度动态计算湿重
            rainWeight = StaminaConstants.ENV_RAIN_WEIGHT_MIN + 
                        (StaminaConstants.ENV_RAIN_WEIGHT_MAX - StaminaConstants.ENV_RAIN_WEIGHT_MIN) * rainIntensity;
            m_fLastRainIntensity = rainIntensity; // 更新最后强度
        }
        else if (m_fRainStopTime > 0.0)
        {
            // 停止下雨后：湿重逐渐衰减（使用二次方衰减，模拟由湿变干的过程）
            float elapsedTime = currentTime - m_fRainStopTime;
            
            if (elapsedTime < StaminaConstants.ENV_RAIN_WEIGHT_DURATION)
            {
                // 使用二次方衰减：t²，让水分蒸发感更自然
                float t = Math.Clamp(elapsedTime / StaminaConstants.ENV_RAIN_WEIGHT_DURATION, 0.0, 1.0);
                float decayRatio = 1.0 - (t * t); // 二次方衰减
                
                // 使用停止前的强度计算起始湿重
                float startWeight = StaminaConstants.ENV_RAIN_WEIGHT_MIN + 
                                  (StaminaConstants.ENV_RAIN_WEIGHT_MAX - StaminaConstants.ENV_RAIN_WEIGHT_MIN) * m_fLastRainIntensity;
                rainWeight = startWeight * decayRatio;
            }
            else
            {
                // 湿重已完全消失
                rainWeight = 0.0;
                m_fRainStopTime = -1.0;
                m_fLastRainIntensity = 0.0;
            }
        }
        
        return Math.Clamp(rainWeight, 0.0, StaminaConstants.ENV_RAIN_WEIGHT_MAX);
    }
    
    // 手动更新环境因子（用于调试或强制更新）
    // @param currentTime 当前世界时间
    // @param owner 角色实体（用于室内检测，可为null）
    void ForceUpdate(float currentTime, IEntity owner = null)
    {
        m_fLastEnvironmentCheckTime = 0.0; // 重置时间，强制更新
        UpdateEnvironmentFactors(currentTime, owner);
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
    
    // 获取降雨强度（用于调试）
    // @return 降雨强度（0.0-1.0），0.0表示无雨，1.0表示暴雨
    float GetRainIntensity()
    {
        return m_fLastRainIntensity;
    }
    
    // 检查是否正在下雨（用于调试）
    // @return true表示正在下雨，false表示未下雨
    bool IsRaining()
    {
        return m_bWasRaining;
    }
}
