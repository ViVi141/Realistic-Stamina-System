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
        EnvironmentFactor environmentFactor = null)
    {
        // ==================== v2.14.0 环境因子修正 ====================
        
        // 获取高级环境因子（如果环境因子模块存在）
        float windDrag = 0.0;
        float mudTerrainFactor = 0.0;
        float mudSprintPenalty = 0.0;
        float totalWetWeight = 0.0;
        float coldStaticPenalty = 0.0;
        
        if (environmentFactor)
        {
            windDrag = environmentFactor.GetWindDrag();
            mudTerrainFactor = environmentFactor.GetMudTerrainFactor();
            mudSprintPenalty = environmentFactor.GetMudSprintPenalty();
            totalWetWeight = environmentFactor.GetTotalWetWeight();
            coldStaticPenalty = environmentFactor.GetColdStaticPenalty();
        }
        
        // 应用泥泞地形系数（修正地形因子）
        terrainFactor = terrainFactor + mudTerrainFactor;
        
        // 应用泥泞Sprint惩罚（修正Sprint倍数，只在Sprint时应用）
        if (currentSpeed >= StaminaConstants.SPRINT_VELOCITY_THRESHOLD)
        {
            sprintMultiplier = sprintMultiplier + mudSprintPenalty;
        }
        
        // 应用降雨湿重（修正当前重量）
        currentWeight = currentWeight + totalWetWeight;
        
        // ==================== 原有计算逻辑 ====================
        
        // 如果速度为0，计算静态站立消耗
        if (currentSpeed < 0.1)
        {
            float bodyWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 90kg
            float loadWeight = Math.Max(currentWeight - bodyWeight, 0.0); // 负重（去除身体重量）
            
            float staticDrainRate = RealisticStaminaSpeedSystem.CalculateStaticStandingCost(bodyWeight, loadWeight);
            
            // 应用冷应激静态惩罚（v2.14.0）
            staticDrainRate = staticDrainRate * (1.0 + coldStaticPenalty);
            
            baseDrainRateByVelocity = staticDrainRate * 0.2; // 转换为每0.2秒的消耗率
        }
        // 如果速度 > 2.2 m/s，使用 Givoni-Goldman 跑步模型
        else if (currentSpeed > 2.2)
        {
            float runningDrainRate = RealisticStaminaSpeedSystem.CalculateGivoniGoldmanRunning(currentSpeed, currentWeight, true);
            runningDrainRate = runningDrainRate * terrainFactor; // 应用地形系数
            
            // 应用风阻（v2.14.0）：逆风时增加消耗
            runningDrainRate = runningDrainRate * (1.0 + windDrag);
            
            baseDrainRateByVelocity = runningDrainRate * 0.2; // 转换为每0.2秒的消耗率
        }
        // 否则使用 Pandolf 步行模型
        else
        {
            baseDrainRateByVelocity = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                currentSpeed, 
                currentWeight, 
                gradePercent,
                terrainFactor,
                true // 使用 Santee 下坡修正
            );
            
            // 应用风阻（v2.14.0）：逆风时增加消耗
            baseDrainRateByVelocity = baseDrainRateByVelocity * (1.0 + windDrag);
        }
        
        // Pandolf 模型的结果是每秒的消耗率，需要转换为每0.2秒的消耗率
        baseDrainRateByVelocity = baseDrainRateByVelocity * 0.2;
        
        // 保存原始基础消耗率（用于恢复计算，在应用姿态修正之前）
        float originalBaseDrainRate = baseDrainRateByVelocity;
        
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
        
        // 速度相关项（用于平滑过渡）
        float speedRatio = Math.Clamp(currentSpeed / 5.2, 0.0, 1.0);
        float speedLinearDrainRate = 0.00005 * speedRatio * totalEfficiencyFactor * fatigueFactor;
        float speedSquaredDrainRate = 0.00005 * speedRatio * speedRatio * totalEfficiencyFactor * fatigueFactor;
        
        // 负重相关消耗
        float encumbranceBaseDrainRate = 0.001 * (encumbranceStaminaDrainMultiplier - 1.0);
        float encumbranceSpeedDrainRate = 0.0002 * (encumbranceStaminaDrainMultiplier - 1.0) * speedRatio * speedRatio;
        
        // 综合体力消耗率
        float baseDrainComponents = baseDrainRate + speedLinearDrainRate + speedSquaredDrainRate + encumbranceBaseDrainRate + encumbranceSpeedDrainRate;
        
        float totalDrainRate = 0.0;
        if (baseDrainRate < 0.0)
        {
            // 恢复时，直接使用恢复率（负数）
            totalDrainRate = baseDrainRate;
        }
        else
        {
            // 消耗时，应用 Sprint 倍数
            totalDrainRate = baseDrainComponents * sprintMultiplier;
            
            // 生理上限：防止负重+坡度爆炸
            const float MAX_DRAIN_RATE_PER_TICK = 0.02; // 每0.2秒最大消耗
            
            // 计算超出生理上限的消耗（用于疲劳积累）
            float excessDrainRate = totalDrainRate - MAX_DRAIN_RATE_PER_TICK;
            if (fatigueSystem && excessDrainRate > 0.0)
            {
                fatigueSystem.ProcessFatigueAccumulation(excessDrainRate);
            }
            
            // 限制实际消耗不超过生理上限
            totalDrainRate = Math.Min(totalDrainRate, MAX_DRAIN_RATE_PER_TICK);
        }
        
        // 输出基础消耗率（用于恢复计算，使用原始值，不包含姿态修正）
        baseDrainRateByVelocity = originalBaseDrainRate;
        
        return totalDrainRate;
    }
    
    // 计算姿态修正倍数
    // @param currentSpeed 当前速度 (m/s)
    // @param controller 角色控制器组件（用于获取姿态）
    // @return 姿态修正倍数
    static float CalculatePostureMultiplier(float currentSpeed, SCR_CharacterControllerComponent controller)
    {
        float postureMultiplier = StaminaConstants.POSTURE_STAND_MULTIPLIER; // 默认站立姿态
        if (currentSpeed > 0.05) // 只在移动时应用姿态修正
        {
            ECharacterStance currentStance = controller.GetStance();
            if (currentStance == ECharacterStance.CROUCH)
            {
                postureMultiplier = StaminaConstants.POSTURE_CROUCH_MULTIPLIER;
            }
            else if (currentStance == ECharacterStance.PRONE)
            {
                postureMultiplier = StaminaConstants.POSTURE_PRONE_MULTIPLIER;
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
    // @return 健康状态效率因子
    static float CalculateFitnessEfficiencyFactor()
    {
        float fitnessEfficiencyFactor = 1.0 - (RealisticStaminaSpeedSystem.FITNESS_EFFICIENCY_COEFF * RealisticStaminaSpeedSystem.FITNESS_LEVEL);
        return Math.Clamp(fitnessEfficiencyFactor, 0.7, 1.0);
    }
}
