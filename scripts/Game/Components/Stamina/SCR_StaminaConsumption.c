// 体力消耗计算模块
// 负责计算体力消耗率（Pandolf模型、姿态修正、Sprint消耗等）
// 模块化拆分：从 PlayerBase.c 提取的体力消耗计算逻辑

class StaminaConsumptionCalculator
{
    // ==================== 公共方法 ====================
    
    // 计算体力消耗率
    // @param currentSpeed 当前速度 (m/s)
    // @param currentWeight 当前重量 (kg)
    // @param gradePercent 坡度百分比
    // @param terrainFactor 地形系数
    // @param postureMultiplier 姿态修正倍数
    // @param totalEfficiencyFactor 综合效率因子
    // @param fatigueFactor 疲劳因子
    // @param sprintMultiplier Sprint消耗倍数
    // @param encumbranceStaminaDrainMultiplier 负重体力消耗倍数
    // @param fatigueSystem 疲劳积累系统模块引用
    // @param out baseDrainRateByVelocity 基础消耗率（输出，用于恢复计算）
    // @param environmentFactor 环境因子模块引用（v2.14.0新增）
    // @param owner 角色实体引用（用于手持物品检测，v2.15.0新增）
    // @return 体力消耗率（每0.2秒）
    static float CalculateStaminaConsumption(
        float currentSpeed,
        float currentWeight,
        float gradePercent,
        float terrainFactor,
        float postureMultiplier,
        float totalEfficiencyFactor,
        float fatigueFactor,
        float sprintMultiplier,
        float encumbranceStaminaDrainMultiplier,
        FatigueSystem fatigueSystem,
        out float baseDrainRateByVelocity,
        EnvironmentFactor environmentFactor = null,
        IEntity owner = null,
        bool isSprinting = false,
        int currentMovementPhase = -1)
    {
        // ==================== v2.14.0 环境因子修正 ====================
        
        // 获取高级环境因子（如果环境因子模块存在）
        float windDrag = 0.0;
        float mudTerrainFactor = 0.0;
        float totalWetWeight = 0.0;
        float coldStaticPenalty = 0.0;
        
        if (environmentFactor)
        {
            windDrag = environmentFactor.GetWindDrag();
            mudTerrainFactor = environmentFactor.GetMudTerrainFactor();
            totalWetWeight = environmentFactor.GetTotalWetWeight();
            coldStaticPenalty = environmentFactor.GetColdStaticPenalty();

            // 检查是否在室内，如果是则忽略坡度影响
            // 使用 IsIndoorForEntity(owner) 确保服务器/多实体场景下正确检测（m_pCachedOwner 可能未更新）
            if (owner && environmentFactor.IsIndoorForEntity(owner))
            {
                gradePercent = 0.0; // 室内时坡度为0
            }
            else if (!owner && environmentFactor.IsIndoor())
            {
                gradePercent = 0.0; // 无 owner 时回退到 IsIndoor()
            }
        }
        
        // ==================== 手持重物额外消耗 ====================
        const float itemBonus = 1.0; // 默认无额外消耗
        
        if (owner)
        {
            // 通过 InventoryStorageManagerComponent 检测玩家右手或双手插槽
            // TODO: 实现手持物品检测逻辑
            // 预留：根据物品重量或 Tag 判断的逻辑结构
            // 示例逻辑：
            /*
            InventoryStorageManagerComponent inventoryManager = InventoryStorageManagerComponent.Cast(owner.FindComponent(InventoryStorageManagerComponent));
            if (inventoryManager)
            {
                // 获取右手或双手插槽的物品
                IEntity rightHandItem = inventoryManager.GetItemInSlot("RightHand");
                IEntity twoHandedItem = inventoryManager.GetItemInSlot("TwoHanded");
                
                if (rightHandItem || twoHandedItem)
                {
                    // 检测到手持实体，增加消耗乘数
                    itemBonus = 1.2; // 暂时固定为1.2倍，后续可根据物品重量或Tag调整
                    
                    // TODO: 根据物品重量或Tag调整消耗乘数
                    // 例如：
                    // float itemWeight = rightHandItem ? rightHandItem.GetWeight() : (twoHandedItem ? twoHandedItem.GetWeight() : 0.0);
                    // if (itemWeight > 5.0) // 大于5kg的物品
                    //     itemBonus = 1.5;
                    // else if (itemWeight > 2.0) // 大于2kg的物品
                    //     itemBonus = 1.2;
                }
            }
            */
        }
        
        // 应用泥泞地形系数（修正地形因子）
        terrainFactor = terrainFactor + mudTerrainFactor;
        
        // 应用降雨湿重（修正当前重量）
        currentWeight = currentWeight + totalWetWeight;

        // ==================== 修复：调用公共静态方法计算基础消耗率 ====================
        // 如果调用方已经计算好了基线（例如 PlayerBase），我们应使用该值以确保一致性；否则自己计算并确保体重包含身体质量
        bool usedFallback = false;
        if (baseDrainRateByVelocity <= 0.0)
        {
            usedFallback = true;
            float weight_for_base = currentWeight + StaminaConstants.CHARACTER_WEIGHT; // 装备+湿重 + 人体
            baseDrainRateByVelocity = StaminaUpdateCoordinator.CalculateLandBaseDrainRate(
                currentSpeed,
                weight_for_base,
                gradePercent,
                terrainFactor,
                windDrag,
                coldStaticPenalty,
                isSprinting,
                currentMovementPhase);
        }

        // 在应用姿态之前，使用环境因子对机械能耗做温度/风速补偿
        // [修复 v2.16.0] AdjustEnergyForTemperature 现已正确将额外 Watts 转换为体力/tick 单位
        float beforeTempAdj = baseDrainRateByVelocity;
        if (environmentFactor && baseDrainRateByVelocity > 0.0)
        {
            baseDrainRateByVelocity = environmentFactor.AdjustEnergyForTemperature(baseDrainRateByVelocity);
        }

        // 保存原始基础消耗率（用于恢复计算，在应用姿态修正之前）
        float originalBaseDrainRate = baseDrainRateByVelocity;
        // debug: log base drain before posture
        if (StaminaConstants.IsDebugEnabled())
        {
            int fbFlag = 0;
            if (usedFallback) fbFlag = 1;
            PrintFormat("[ConsCalc] fb=%1 baseDrain=%2 tempAdj=%3 weight=%4 speed=%5 grade=%6",
                fbFlag,
                Math.Round(baseDrainRateByVelocity * 1000.0) / 1000.0,
                Math.Round((baseDrainRateByVelocity - beforeTempAdj) * 1000000.0) / 1000000.0,
                Math.Round(currentWeight * 10.0) / 10.0,
                Math.Round(currentSpeed * 1000.0) / 1000.0,
                Math.Round(gradePercent * 100.0) / 100.0);
        }
        
        // 应用姿态修正（只在消耗时应用）
        if (baseDrainRateByVelocity > 0.0)
        {
            baseDrainRateByVelocity = baseDrainRateByVelocity * postureMultiplier;
        }
        
        // 应用多维度修正因子（健康状态、累积疲劳、代谢适应）
        float baseDrainRate = 0.0;
        if (baseDrainRateByVelocity < 0.0)
        {
            // 恢复时，直接使用恢复率（负数）
            baseDrainRate = baseDrainRateByVelocity;
        }
        else
        {
            // 消耗时，应用效率因子和疲劳因子
            baseDrainRate = baseDrainRateByVelocity * totalEfficiencyFactor * fatigueFactor;
        }
        
        // 综合体力消耗率
        float totalDrainRate = 0.0;
        if (baseDrainRate < 0.0)
        {
            // 恢复时，直接使用恢复率（负数）
            totalDrainRate = baseDrainRate;
        }
        else
        {
            // 消耗时：已统一为 Pandolf 公式，不再对 Sprint 单独乘倍数（Pandolf 速度项自然体现 Sprint 更高消耗）
            totalDrainRate = baseDrainRate;

            // 应用负重体力消耗倍数（修复：此前传入但未使用）
            totalDrainRate = totalDrainRate * encumbranceStaminaDrainMultiplier;

            // 应用手持重物额外消耗
            totalDrainRate = totalDrainRate * itemBonus;
        }


        
        // 输出基础消耗率（用于恢复计算，使用原始值，不包含姿态修正）
        baseDrainRateByVelocity = originalBaseDrainRate;
        // debug: verify out parameter value (total drain before posture/efficiency)
        if (StaminaConstants.IsDebugEnabled())
        {
            PrintFormat("[ConsCalc] outBase=%1 total=%2",
                Math.Round(baseDrainRateByVelocity * 1000.0) / 1000.0,
                Math.Round(totalDrainRate * 1000.0) / 1000.0);
        }
        
        return totalDrainRate;
    }
    
    // 计算姿态修正倍数
    // @param currentSpeed 当前速度 (m/s)
    // @param controller 角色控制器组件（用于获取姿态）
    // @return 姿态修正倍数
    // 修复：使用配置参数而非硬编码常量，使管理员可以调整姿态消耗倍数
    static float CalculatePostureMultiplier(float currentSpeed, SCR_CharacterControllerComponent controller)
    {
        float postureMultiplier = StaminaConstants.POSTURE_STAND_MULTIPLIER; // 默认站立姿态（1.0）
        if (currentSpeed > 0.05) // 只在移动时应用姿态修正
        {
            ECharacterStance currentStance = controller.GetStance();
            if (currentStance == ECharacterStance.CROUCH)
            {
                // 修复：使用配置参数而非硬编码常量
                postureMultiplier = StaminaConstants.GetPostureCrouchMultiplier();
            }
            else if (currentStance == ECharacterStance.PRONE)
            {
                // 修复：使用配置参数而非硬编码常量
                postureMultiplier = StaminaConstants.GetPostureProneMultiplier();
            }
        }
        return postureMultiplier;
    }
    
    // 计算代谢适应效率因子
    // @param speedRatio 速度比率 (0.0-1.0)
    // @return 代谢适应效率因子
    static float CalculateMetabolicEfficiencyFactor(float speedRatio)
    {
        float metabolicEfficiencyFactor = 1.0;
        if (speedRatio < RealisticStaminaSpeedSystem.AEROBIC_THRESHOLD)
        {
            metabolicEfficiencyFactor = RealisticStaminaSpeedSystem.AEROBIC_EFFICIENCY_FACTOR; // 0.9
        }
        else if (speedRatio < RealisticStaminaSpeedSystem.ANAEROBIC_THRESHOLD)
        {
            float t = (speedRatio - RealisticStaminaSpeedSystem.AEROBIC_THRESHOLD) / 
                      (RealisticStaminaSpeedSystem.ANAEROBIC_THRESHOLD - RealisticStaminaSpeedSystem.AEROBIC_THRESHOLD);
            metabolicEfficiencyFactor = RealisticStaminaSpeedSystem.AEROBIC_EFFICIENCY_FACTOR + 
                                       t * (RealisticStaminaSpeedSystem.ANAEROBIC_EFFICIENCY_FACTOR - RealisticStaminaSpeedSystem.AEROBIC_EFFICIENCY_FACTOR);
        }
        else
        {
            metabolicEfficiencyFactor = RealisticStaminaSpeedSystem.ANAEROBIC_EFFICIENCY_FACTOR; // 1.2
        }
        return metabolicEfficiencyFactor;
    }
    
    // 计算健康状态效率因子
    // 固定值（22岁训练有素男性，FITNESS_LEVEL=1.0）：预计算结果直接返回，防止不平等游玩
    // = clamp(1.0 - 0.35 × 1.0, 0.7, 1.0) = clamp(0.65, 0.7, 1.0) = 0.70
    // @return 0.70（固定常量）
    static float CalculateFitnessEfficiencyFactor()
    {
        return StaminaConstants.FIXED_FITNESS_EFFICIENCY_FACTOR;
    }
}
