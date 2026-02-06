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
        
        // 注意：降雨湿重已在 UpdateAdvancedEnvironmentFactors 中的 CalculateRainWetWeight 方法中计算
        // 不需要在这里再次调用 CalculateRainWeight
        
        // 更新总湿重（游泳湿重 + 降雨湿重，限制在最大值）
        // 使用 SwimmingStateManager 的方法计算总湿重
        m_fCurrentTotalWetWeight = SwimmingStateManager.CalculateTotalWetWeight(swimmingWetWeight, m_fCachedRainWeight);
        
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
    
    // 计算热应激倍数（基于虚拟气温，考虑室内豁免）
    // 热应激模型：基于虚拟气温阈值，而非固定时间段
    // 只有当虚拟气温超过 26°C 时，才开始计算热应激
    // 如果角色在室内（头顶有遮挡），热应激减少 50%
    // @param owner 角色实体（用于室内检测，可为null）
    // @return 热应激倍数（1.0-1.3）
    protected float CalculateHeatStressMultiplier(IEntity owner = null)
    {
        if (!m_pCachedWeatherManager)
            return 1.0;
        
        // 计算虚拟气温
        float simulatedTemp = CalculateSimulatedTemperature();
        
        // 热应激触发阈值：26°C
        // 只有当虚拟气温超过 26°C 时，才开始计算热应激
        const float heatStressThreshold = 26.0;
        float multiplier = 1.0;
        
        if (simulatedTemp < heatStressThreshold)
        {
            // 虚拟气温未达阈值，无热应激
            multiplier = 1.0;
        }
        else
        {
            // 虚拟气温超过阈值，计算热应激倍数
            // 倍数 = 1.0 + (虚拟气温 - 阈值) * 0.02
            // 例如：30°C -> 1.0 + (30 - 26) * 0.02 = 1.08x
            float tempExcess = simulatedTemp - heatStressThreshold;
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
        
        // 5. 获取当前气温（虚拟气温）
        m_fCachedTemperature = CalculateSimulatedTemperature();
        
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
