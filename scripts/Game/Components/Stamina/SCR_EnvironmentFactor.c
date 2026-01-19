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
    
    // ==================== 高级环境因子状态变量（v2.14.0）====================
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
    protected float m_fSurfaceWetnessPenalty = 0.0; // 地表湿度惩罚
    
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
        
        // ==================== 高级环境因子初始化（v2.14.0）====================
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
    }
    
    // 更新环境因子（协调方法）
    // @param currentTime 当前时间（秒）
    // @param owner 角色实体（用于室内检测和姿态判断）
    // @param playerVelocity 玩家速度向量（用于风阻计算）
    // @param terrainFactor 地形系数（用于泥泞计算）
    // @return 是否需要更新（true表示已更新，false表示跳过）
    bool UpdateEnvironmentFactors(float currentTime, IEntity owner = null, vector playerVelocity = vector.Zero, float terrainFactor = 1.0)
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
        
        // 更新降雨湿重
        float oldRainWeight = m_fCachedRainWeight;
        m_fCachedRainWeight = CalculateRainWeight(currentTime);
        
        // 更新总湿重（游泳湿重 + 降雨湿重，限制在最大值）
        // 注意：游泳湿重由外部传入，这里暂时只考虑降雨湿重
        m_fCurrentTotalWetWeight = Math.Clamp(m_fCachedRainWeight, 0.0, StaminaConstants.ENV_MAX_TOTAL_WET_WEIGHT);
        
        // 调试信息：环境因子更新（每5秒输出一次或值变化时）
        static int envDebugCounter = 0;
        static float lastLoggedHeatStress = 1.0;
        static float lastLoggedRainWeight = 0.0;
        static float lastLoggedWindSpeed = 0.0;
        static float lastLoggedTemperature = 20.0;
        envDebugCounter++;
        bool shouldLog = (envDebugCounter >= 125) || // 每25秒（5秒 * 5）
                        (Math.AbsFloat(m_fCachedHeatStressMultiplier - lastLoggedHeatStress) > 0.05) ||
                        (Math.AbsFloat(m_fCachedRainWeight - lastLoggedRainWeight) > 0.5) ||
                        (Math.AbsFloat(m_fCachedWindSpeed - lastLoggedWindSpeed) > 2.0) ||
                        (Math.AbsFloat(m_fCachedTemperature - lastLoggedTemperature) > 2.0);
        
        if (shouldLog)
        {
            envDebugCounter = 0;
            lastLoggedHeatStress = m_fCachedHeatStressMultiplier;
            lastLoggedRainWeight = m_fCachedRainWeight;
            lastLoggedWindSpeed = m_fCachedWindSpeed;
            lastLoggedTemperature = m_fCachedTemperature;
            
            PrintFormat("[RealisticSystem] 环境因子 / Environment Factors: 热应激=%1x | 降雨湿重=%2kg | 风速=%3m/s | 气温=%4°C | Heat Stress=%1x | Rain Weight=%2kg | Wind Speed=%3m/s | Temperature=%4°C",
                Math.Round(m_fCachedHeatStressMultiplier * 100.0) / 100.0,
                Math.Round(m_fCachedRainWeight * 10.0) / 10.0,
                Math.Round(m_fCachedWindSpeed * 10.0) / 10.0,
                Math.Round(m_fCachedTemperature * 10.0) / 10.0);
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
    // 改进方法：多位置检测，提高准确率
    // @param owner 角色实体
    // @return true表示在室内（有遮挡），false表示在室外（无遮挡）
    protected bool IsUnderCover(IEntity owner)
    {
        if (!owner)
            return false;
        
        World world = owner.GetWorld();
        if (!world)
            return false;
        
        vector ownerPos = owner.GetOrigin();
        float expectedDistance = StaminaConstants.ENV_INDOOR_CHECK_HEIGHT;
        
        // 多位置检测：检测角色头顶和周围4个方向，提高准确率
        // 如果多数位置被遮挡，则判定为室内
        // EnforceScript 不支持数组和代码块作用域，重用变量逐个检测
        int hitCount = 0;
        
        // 重用变量（避免重复声明）
        vector checkPos;
        vector traceStart;
        vector traceEnd;
        vector offset;
        TraceParam traceParam;
        float hitDistance;
        
        // 位置1：角色正上方
        checkPos = ownerPos + vector.Zero;
        traceStart = checkPos;
        traceStart[1] = traceStart[1] + 1.0;
        traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + expectedDistance;
        traceParam = new TraceParam();
        traceParam.Start = traceStart;
        traceParam.End = traceEnd;
        traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam.Exclude = owner;
        traceParam.LayerMask = EPhysicsLayerPresets.Character;
        world.TraceMove(traceParam, FilterIndoorCallback);
        hitDistance = vector.Distance(traceParam.Start, traceParam.End);
        if (hitDistance < expectedDistance * 0.95)
        {
            hitCount++;
        }
        else
        {
            traceParam.LayerMask = EPhysicsLayerPresets.Projectile;
            world.TraceMove(traceParam, FilterIndoorCallback);
            hitDistance = vector.Distance(traceParam.Start, traceParam.End);
            if (hitDistance < expectedDistance * 0.95)
                hitCount++;
        }
        
        // 位置2：右侧
        offset[0] = 0.5;
        offset[1] = 0.0;
        offset[2] = 0.0;
        checkPos = ownerPos + offset;
        traceStart = checkPos;
        traceStart[1] = traceStart[1] + 1.0;
        traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + expectedDistance;
        traceParam = new TraceParam();
        traceParam.Start = traceStart;
        traceParam.End = traceEnd;
        traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam.Exclude = owner;
        traceParam.LayerMask = EPhysicsLayerPresets.Character;
        world.TraceMove(traceParam, FilterIndoorCallback);
        hitDistance = vector.Distance(traceParam.Start, traceParam.End);
        if (hitDistance < expectedDistance * 0.95)
        {
            hitCount++;
        }
        else
        {
            traceParam.LayerMask = EPhysicsLayerPresets.Projectile;
            world.TraceMove(traceParam, FilterIndoorCallback);
            hitDistance = vector.Distance(traceParam.Start, traceParam.End);
            if (hitDistance < expectedDistance * 0.95)
                hitCount++;
        }
        
        // 位置3：左侧
        offset[0] = -0.5;
        offset[1] = 0.0;
        offset[2] = 0.0;
        checkPos = ownerPos + offset;
        traceStart = checkPos;
        traceStart[1] = traceStart[1] + 1.0;
        traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + expectedDistance;
        traceParam = new TraceParam();
        traceParam.Start = traceStart;
        traceParam.End = traceEnd;
        traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam.Exclude = owner;
        traceParam.LayerMask = EPhysicsLayerPresets.Character;
        world.TraceMove(traceParam, FilterIndoorCallback);
        hitDistance = vector.Distance(traceParam.Start, traceParam.End);
        if (hitDistance < expectedDistance * 0.95)
        {
            hitCount++;
        }
        else
        {
            traceParam.LayerMask = EPhysicsLayerPresets.Projectile;
            world.TraceMove(traceParam, FilterIndoorCallback);
            hitDistance = vector.Distance(traceParam.Start, traceParam.End);
            if (hitDistance < expectedDistance * 0.95)
                hitCount++;
        }
        
        // 位置4：前方
        offset[0] = 0.0;
        offset[1] = 0.0;
        offset[2] = 0.5;
        checkPos = ownerPos + offset;
        traceStart = checkPos;
        traceStart[1] = traceStart[1] + 1.0;
        traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + expectedDistance;
        traceParam = new TraceParam();
        traceParam.Start = traceStart;
        traceParam.End = traceEnd;
        traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam.Exclude = owner;
        traceParam.LayerMask = EPhysicsLayerPresets.Character;
        world.TraceMove(traceParam, FilterIndoorCallback);
        hitDistance = vector.Distance(traceParam.Start, traceParam.End);
        if (hitDistance < expectedDistance * 0.95)
        {
            hitCount++;
        }
        else
        {
            traceParam.LayerMask = EPhysicsLayerPresets.Projectile;
            world.TraceMove(traceParam, FilterIndoorCallback);
            hitDistance = vector.Distance(traceParam.Start, traceParam.End);
            if (hitDistance < expectedDistance * 0.95)
                hitCount++;
        }
        
        // 位置5：后方
        offset[0] = 0.0;
        offset[1] = 0.0;
        offset[2] = -0.5;
        checkPos = ownerPos + offset;
        traceStart = checkPos;
        traceStart[1] = traceStart[1] + 1.0;
        traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + expectedDistance;
        traceParam = new TraceParam();
        traceParam.Start = traceStart;
        traceParam.End = traceEnd;
        traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam.Exclude = owner;
        traceParam.LayerMask = EPhysicsLayerPresets.Character;
        world.TraceMove(traceParam, FilterIndoorCallback);
        hitDistance = vector.Distance(traceParam.Start, traceParam.End);
        if (hitDistance < expectedDistance * 0.95)
        {
            hitCount++;
        }
        else
        {
            traceParam.LayerMask = EPhysicsLayerPresets.Projectile;
            world.TraceMove(traceParam, FilterIndoorCallback);
            hitDistance = vector.Distance(traceParam.Start, traceParam.End);
            if (hitDistance < expectedDistance * 0.95)
                hitCount++;
        }
        
        // 如果超过60%的检测点被遮挡，判定为室内
        float hitRatio = (float)hitCount / 5.0;
        bool isIndoor = (hitRatio >= 0.6);
        
        // 调试信息：室内检测结果（每5秒输出一次）
        static int indoorDebugCounter = 0;
        static bool lastIndoorState = false;
        indoorDebugCounter++;
        if (indoorDebugCounter >= 25 || isIndoor != lastIndoorState) // 每5秒或状态改变时输出
        {
            indoorDebugCounter = 0;
            lastIndoorState = isIndoor;
            string indoorStatusStr = "";
            if (isIndoor)
                indoorStatusStr = "室内 / Indoor";
            else
                indoorStatusStr = "室外 / Outdoor";
            PrintFormat("[RealisticSystem] 室内检测 / Indoor Detection: %1 | 命中点: %2/5 (%3%%) | Hit Points: %2/5 (%3%%)", 
                indoorStatusStr,
                hitCount.ToString(),
                Math.Round(hitRatio * 100.0 * 10.0) / 10.0);
        }
        
        return isIndoor;
    }
    
    // 室内检测过滤回调（排除角色实体和某些不需要检测的物体）
    // @param e 实体
    // @return true表示接受检测，false表示排除
    bool FilterIndoorCallback(IEntity e)
    {
        if (!e)
            return true;
        
        // 排除角色实体
        if (ChimeraCharacter.Cast(e))
            return false;
        
        // 排除车辆（如果角色在车内，应该通过其他方式检测）
        // 这里可以添加更多过滤条件
        
        return true;
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
        bool isHeavyRain = false; // 在函数开始时声明，避免作用域问题
        
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
            isHeavyRain = weatherStateName.Contains("heavy") || weatherStateName.Contains("Heavy") || 
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
            
            // 调试信息：开始下雨
            string rainIntensityStr = "";
            if (isHeavyRain)
                rainIntensityStr = "暴雨 / Heavy Rain";
            else
                rainIntensityStr = "小雨/中雨 / Light/Moderate Rain";
            PrintFormat("[RealisticSystem] 开始下雨 / Rain Started: %1 | 强度: %2%% | Intensity: %2%%", 
                rainIntensityStr,
                Math.Round(rainIntensity * 100.0).ToString());
        }
        else if (!isRaining && m_bWasRaining)
        {
            // 停止下雨：记录停止时间和最后强度
            m_fRainStopTime = currentTime;
            
            // 调试信息：停止下雨
            Print("[RealisticSystem] 停止下雨 / Rain Stopped: 湿重开始衰减 | Wet Weight Starts Decaying");
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
    
    // 调试室内检测（输出详细信息）
    // @param owner 角色实体
    // @return 检测结果（true=室内，false=室外）
    bool DebugIndoorDetection(IEntity owner)
    {
        if (!owner)
        {
            Print("[RealisticSystem] 室内检测调试: 角色实体为空");
            return false;
        }
        
        World world = owner.GetWorld();
        if (!world)
        {
            Print("[RealisticSystem] 室内检测调试: 世界对象为空");
            return false;
        }
        
        vector ownerPos = owner.GetOrigin();
        PrintFormat("[RealisticSystem] 室内检测调试: 角色位置=(%1, %2, %3)", 
            ownerPos[0], ownerPos[1], ownerPos[2]);
        
        vector traceStart = ownerPos;
        traceStart[1] = traceStart[1] + 0.5;
        vector traceEnd = traceStart;
        traceEnd[1] = traceEnd[1] + StaminaConstants.ENV_INDOOR_CHECK_HEIGHT;
        
        PrintFormat("[RealisticSystem] 室内检测调试: 射线起点=(%1, %2, %3), 终点=(%4, %5, %6)", 
            traceStart[0], traceStart[1], traceStart[2],
            traceEnd[0], traceEnd[1], traceEnd[2]);
        
        float expectedDistance = StaminaConstants.ENV_INDOOR_CHECK_HEIGHT;
        PrintFormat("[RealisticSystem] 室内检测调试: 预期距离=%1m", expectedDistance);
        
        // 测试所有物理层
        bool found = false;
        
        // 1. Character 层
        TraceParam traceParam1 = new TraceParam();
        traceParam1.Start = traceStart;
        traceParam1.End = traceEnd;
        traceParam1.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam1.Exclude = owner;
        traceParam1.LayerMask = EPhysicsLayerPresets.Character;
        world.TraceMove(traceParam1, FilterIndoorCallback);
        float hitDistance1 = vector.Distance(traceParam1.Start, traceParam1.End);
        bool hit1 = (hitDistance1 < expectedDistance * 0.98);
        PrintFormat("[RealisticSystem] 室内检测调试: 物理层=Character, 命中距离=%1m, 是否命中=%2", hitDistance1, hit1);
        if (hit1) found = true;
        
        // 2. Projectile 层
        TraceParam traceParam2 = new TraceParam();
        traceParam2.Start = traceStart;
        traceParam2.End = traceEnd;
        traceParam2.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        traceParam2.Exclude = owner;
        traceParam2.LayerMask = EPhysicsLayerPresets.Projectile;
        world.TraceMove(traceParam2, FilterIndoorCallback);
        float hitDistance2 = vector.Distance(traceParam2.Start, traceParam2.End);
        bool hit2 = (hitDistance2 < expectedDistance * 0.98);
        PrintFormat("[RealisticSystem] 室内检测调试: 物理层=Projectile, 命中距离=%1m, 是否命中=%2", hitDistance2, hit2);
        if (hit2) found = true;
        
        string resultStr = "室外";
        if (found)
            resultStr = "室内";
        PrintFormat("[RealisticSystem] 室内检测调试: 最终结果=%1", resultStr);
        return found;
    }
    
    // 检查是否正在下雨（用于调试）
    // @return true表示正在下雨，false表示未下雨
    bool IsRaining()
    {
        return m_bWasRaining;
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
        
        // 更新时间戳
        m_fLastUpdateTime = currentTime;
        
        // 1. 获取降雨强度（优先使用API，失败则回退到字符串匹配）
        m_fCachedRainIntensity = CalculateRainIntensityFromAPI();
        
        // 2. 获取风速和风向
        m_fCachedWindSpeed = CalculateWindSpeedFromAPI();
        m_fCachedWindDirection = CalculateWindDirectionFromAPI();
        
        // 3. 计算风阻系数（基于玩家移动方向）
        m_fCachedWindDrag = CalculateWindDrag(playerVelocity);
        
        // 4. 获取泥泞度系数
        m_fCachedMudFactor = CalculateMudFactorFromAPI();
        
        // 5. 获取当前气温
        m_fCachedTemperature = CalculateTemperatureFromAPI();
        
        // 6. 获取地表湿度
        m_fCachedSurfaceWetness = CalculateSurfaceWetnessFromAPI();
        
        // 7. 计算降雨湿重（基于降雨强度）
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
        // 每5秒输出一次或值变化时
        static int advancedEnvDebugCounter = 0;
        static float lastLoggedRainIntensity = 0.0;
        static float lastLoggedWindSpeed = 0.0;
        static float lastLoggedWindDirection = 0.0;
        static float lastLoggedWindDrag = 0.0;
        static float lastLoggedMudFactor = 0.0;
        static float lastLoggedTemperature = 0.0;
        static float lastLoggedSurfaceWetness = 0.0;
        static float lastLoggedRainWeight = 0.0;
        static float lastLoggedRainBreathingPenalty = 0.0;
        static float lastLoggedMudTerrainFactor = 0.0;
        static float lastLoggedMudSprintPenalty = 0.0;
        static float lastLoggedSlipRisk = 0.0;
        static float lastLoggedHeatStressPenalty = 0.0;
        static float lastLoggedColdStressPenalty = 0.0;
        static float lastLoggedColdStaticPenalty = 0.0;
        static float lastLoggedSurfaceWetnessPenalty = 0.0;
        
        advancedEnvDebugCounter++;
        bool shouldLog = (advancedEnvDebugCounter >= 25) || // 每5秒（0.2秒 * 25）
                        (Math.AbsFloat(m_fCachedRainIntensity - lastLoggedRainIntensity) > 0.1) ||
                        (Math.AbsFloat(m_fCachedWindSpeed - lastLoggedWindSpeed) > 1.0) ||
                        (Math.AbsFloat(m_fCachedWindDirection - lastLoggedWindDirection) > 10.0) ||
                        (Math.AbsFloat(m_fCachedWindDrag - lastLoggedWindDrag) > 0.1) ||
                        (Math.AbsFloat(m_fCachedMudFactor - lastLoggedMudFactor) > 0.1) ||
                        (Math.AbsFloat(m_fCachedTemperature - lastLoggedTemperature) > 2.0) ||
                        (Math.AbsFloat(m_fCachedSurfaceWetness - lastLoggedSurfaceWetness) > 0.1) ||
                        (Math.AbsFloat(m_fCachedRainWeight - lastLoggedRainWeight) > 0.5) ||
                        (Math.AbsFloat(m_fRainBreathingPenalty - lastLoggedRainBreathingPenalty) > 0.1) ||
                        (Math.AbsFloat(m_fMudTerrainFactor - lastLoggedMudTerrainFactor) > 0.1) ||
                        (Math.AbsFloat(m_fMudSprintPenalty - lastLoggedMudSprintPenalty) > 0.05) ||
                        (Math.AbsFloat(m_fSlipRisk - lastLoggedSlipRisk) > 0.1) ||
                        (Math.AbsFloat(m_fHeatStressPenalty - lastLoggedHeatStressPenalty) > 0.1) ||
                        (Math.AbsFloat(m_fColdStressPenalty - lastLoggedColdStressPenalty) > 0.1) ||
                        (Math.AbsFloat(m_fColdStaticPenalty - lastLoggedColdStaticPenalty) > 0.1) ||
                        (Math.AbsFloat(m_fSurfaceWetnessPenalty - lastLoggedSurfaceWetnessPenalty) > 0.05);
        
        if (shouldLog)
        {
            advancedEnvDebugCounter = 0;
            
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
            PrintFormat("  气温 / Temperature: %1°C", Math.Round(m_fCachedTemperature * 10.0) / 10.0);
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
            
            lastLoggedRainIntensity = m_fCachedRainIntensity;
            lastLoggedWindSpeed = m_fCachedWindSpeed;
            lastLoggedWindDirection = m_fCachedWindDirection;
            lastLoggedWindDrag = m_fCachedWindDrag;
            lastLoggedMudFactor = m_fCachedMudFactor;
            lastLoggedTemperature = m_fCachedTemperature;
            lastLoggedSurfaceWetness = m_fCachedSurfaceWetness;
            lastLoggedRainWeight = m_fCachedRainWeight;
            lastLoggedRainBreathingPenalty = m_fRainBreathingPenalty;
            lastLoggedMudTerrainFactor = m_fMudTerrainFactor;
            lastLoggedMudSprintPenalty = m_fMudSprintPenalty;
            lastLoggedSlipRisk = m_fSlipRisk;
            lastLoggedHeatStressPenalty = m_fHeatStressPenalty;
            lastLoggedColdStressPenalty = m_fColdStressPenalty;
            lastLoggedColdStaticPenalty = m_fColdStaticPenalty;
            lastLoggedSurfaceWetnessPenalty = m_fSurfaceWetnessPenalty;
        }
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
    
    // 从API获取当前气温
    // @return 当前气温（°C）
    protected float CalculateTemperatureFromAPI()
    {
        if (!m_pCachedWeatherManager)
            return 20.0; // 默认20°C
        
        // API调用：获取气温
        float temperature = m_pCachedWeatherManager.GetTemperatureAirMinOverride();
        
        return temperature;
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
    // @param currentTime 当前时间（秒）
    protected void CalculateRainWetWeight(float currentTime)
    {
        if (m_fCachedRainIntensity < StaminaConstants.ENV_RAIN_INTENSITY_THRESHOLD)
            return;
        
        // 计算湿重增加速率（非线性增长）
        float accumulationRate = StaminaConstants.ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE * 
                                 Math.Pow(m_fCachedRainIntensity, StaminaConstants.ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT);
        
        // 计算时间增量（秒）
        float deltaTime = currentTime - m_fLastUpdateTime;
        if (deltaTime <= 0)
            return;
        
        // 增加湿重
        m_fCachedRainWeight = Math.Clamp(
            m_fCachedRainWeight + accumulationRate * deltaTime,
            0.0,
            StaminaConstants.ENV_MAX_TOTAL_WET_WEIGHT
        );
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
        m_fHeatStressPenalty = (m_fCachedTemperature - StaminaConstants.ENV_TEMPERATURE_HEAT_THRESHOLD) * 
                               StaminaConstants.ENV_TEMPERATURE_HEAT_PENALTY_COEFF;
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
        m_fColdStressPenalty = (StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD - m_fCachedTemperature) * 
                               StaminaConstants.ENV_TEMPERATURE_COLD_RECOVERY_PENALTY;
        
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
        m_fSurfaceWetnessPenalty = StaminaConstants.ENV_SURFACE_WETNESS_PRONE_PENALTY * m_fCachedSurfaceWetness;
    }
}
