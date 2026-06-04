// Realistic Stamina System (RSS) - v2.13.0
// 拟真体力-速度系统（基于医学/生理学模型）
// 结合体力值和负重，动态调整移动速度
// 
// 医学模型参考：
// 1. Pandolf 负重行走代谢成本模型（Pandolf et al., 1977）
// 2. LCDA 步行能量消耗方程（Level walking meta-regression）
// 3. US Army 背包负重对速度与心肺响应影响实验（Knapik et al., 1996）
// 4. 耐力下降模型（Fatigue-based speed model，Minetti et al., 2002）
//
// 优化目标：2英里（3218.7米）在15分27秒（927秒）内完成
// 优化结果：完成时间 925.8秒（15.43分钟），提前1.2秒完成 ✅
//
// 注意：此文件包含辅助类，主要逻辑在 SCR_CharacterControllerComponent 中
class SCR_RSS_MetabolismMath
{
    // ==================== 常量定义 ====================
    // 注意：所有常量定义已移至 SCR_StaminaConstants.c 模块
    // 使用 SCR_RSS_Constants.XXX 访问常量
    
    // 为了保持向后兼容，保留一些常用的常量别名
    // 这些别名直接引用 SCR_RSS_Constants 中的值
    // 注意：部分常量现在从配置管理器动态获取
    static const float GAME_MAX_SPEED = SCR_RSS_Constants.GAME_MAX_SPEED;
    static const float TARGET_RUN_SPEED = SCR_RSS_Constants.TARGET_RUN_SPEED;
    static const float TARGET_RUN_SPEED_MULTIPLIER = SCR_RSS_Constants.TARGET_RUN_SPEED_MULTIPLIER;
    static const float MIN_LIMP_SPEED_MULTIPLIER = SCR_RSS_Constants.MIN_LIMP_SPEED_MULTIPLIER;
    static const float STAMINA_EXPONENT = SCR_RSS_Constants.STAMINA_EXPONENT;
    static const float CHARACTER_WEIGHT = SCR_RSS_Constants.CHARACTER_WEIGHT;
    static const float MAX_ENCUMBRANCE_WEIGHT = SCR_RSS_Constants.MAX_ENCUMBRANCE_WEIGHT;
    static const float COMBAT_ENCUMBRANCE_WEIGHT = SCR_RSS_Constants.COMBAT_ENCUMBRANCE_WEIGHT;
    static const float SMOOTH_TRANSITION_START = SCR_RSS_Constants.SMOOTH_TRANSITION_START;
    static const float SMOOTH_TRANSITION_END = SCR_RSS_Constants.SMOOTH_TRANSITION_END;
    static const float MIN_SPEED_MULTIPLIER = SCR_RSS_Constants.MIN_SPEED_MULTIPLIER;
    static const float MAX_SPEED_MULTIPLIER = SCR_RSS_Constants.MAX_SPEED_MULTIPLIER;
    static const float INITIAL_STAMINA_AFTER_ACFT = SCR_RSS_Constants.INITIAL_STAMINA_AFTER_ACFT;
    static const float EXHAUSTION_THRESHOLD = SCR_RSS_Constants.EXHAUSTION_THRESHOLD;
    static const float EXHAUSTION_LIMP_SPEED = SCR_RSS_Constants.EXHAUSTION_LIMP_SPEED;
    static const float SPRINT_ENABLE_THRESHOLD = SCR_RSS_Constants.SPRINT_ENABLE_THRESHOLD;
    static const float SPRINT_SPEED_BOOST = SCR_RSS_Constants.SPRINT_SPEED_BOOST;
    static const float RECOVERY_THRESHOLD_NO_LOAD = SCR_RSS_Constants.RECOVERY_THRESHOLD_NO_LOAD;
    static const float DRAIN_THRESHOLD_COMBAT_LOAD = SCR_RSS_Constants.DRAIN_THRESHOLD_COMBAT_LOAD;
    static const float SPRINT_VELOCITY_THRESHOLD = SCR_RSS_Constants.SPRINT_VELOCITY_THRESHOLD;
    static const float RUN_VELOCITY_THRESHOLD = SCR_RSS_Constants.RUN_VELOCITY_THRESHOLD;
    static const float WALK_VELOCITY_THRESHOLD = SCR_RSS_Constants.WALK_VELOCITY_THRESHOLD;
    static const float FITNESS_LEVEL = SCR_RSS_Constants.FITNESS_LEVEL;
    static const float FITNESS_EFFICIENCY_COEFF = SCR_RSS_Constants.FITNESS_EFFICIENCY_COEFF;
    static const float FITNESS_RECOVERY_COEFF = SCR_RSS_Constants.FITNESS_RECOVERY_COEFF;
    // 注意：BASE_RECOVERY_RATE 现在从配置管理器获取
    // 注意：RECOVERY_NONLINEAR_COEFF 现在从配置管理器获取
    // 注意：FAST_RECOVERY_MULTIPLIER 现在从配置管理器获取
    // 注意：MEDIUM_RECOVERY_MULTIPLIER 现在从配置管理器获取
    // 注意：SLOW_RECOVERY_MULTIPLIER 现在从配置管理器获取
    static const float FAST_RECOVERY_DURATION_MINUTES = SCR_RSS_Constants.FAST_RECOVERY_DURATION_MINUTES;
    static const float MEDIUM_RECOVERY_START_MINUTES = SCR_RSS_Constants.MEDIUM_RECOVERY_START_MINUTES;
    static const float MEDIUM_RECOVERY_DURATION_MINUTES = SCR_RSS_Constants.MEDIUM_RECOVERY_DURATION_MINUTES;
    static const float SLOW_RECOVERY_START_MINUTES = SCR_RSS_Constants.SLOW_RECOVERY_START_MINUTES;
    static const float AGE_RECOVERY_COEFF = SCR_RSS_Constants.AGE_RECOVERY_COEFF;
    static const float AGE_REFERENCE = SCR_RSS_Constants.AGE_REFERENCE;
    static const float FATIGUE_RECOVERY_PENALTY = SCR_RSS_Constants.FATIGUE_RECOVERY_PENALTY;
    static const float FATIGUE_RECOVERY_DURATION_MINUTES = SCR_RSS_Constants.FATIGUE_RECOVERY_DURATION_MINUTES;
    // 注意：STANDING_RECOVERY_MULTIPLIER, PRONE_RECOVERY_MULTIPLIER 现在从配置管理器获取
    // 注意：LOAD_RECOVERY_PENALTY_COEFF, LOAD_RECOVERY_PENALTY_EXPONENT 现在从配置管理器获取
    static const float BODY_TOLERANCE_BASE = SCR_RSS_Constants.BODY_TOLERANCE_BASE;
    // 注意：MARGINAL_DECAY_THRESHOLD, MARGINAL_DECAY_COEFF 现在从配置管理器获取
    static const float MIN_RECOVERY_STAMINA_THRESHOLD = SCR_RSS_Constants.MIN_RECOVERY_STAMINA_THRESHOLD;
    static const float MIN_RECOVERY_REST_TIME_SECONDS = SCR_RSS_Constants.MIN_RECOVERY_REST_TIME_SECONDS;
    // 注意：FATIGUE_ACCUMULATION_COEFF, FATIGUE_MAX_FACTOR 现在从配置管理器获取
    static const float AEROBIC_THRESHOLD = SCR_RSS_Constants.AEROBIC_THRESHOLD;
    static const float ANAEROBIC_THRESHOLD = SCR_RSS_Constants.ANAEROBIC_THRESHOLD;
    static const float AEROBIC_EFFICIENCY_FACTOR = SCR_RSS_Constants.AEROBIC_EFFICIENCY_FACTOR;
    static const float ANAEROBIC_EFFICIENCY_FACTOR = SCR_RSS_Constants.ANAEROBIC_EFFICIENCY_FACTOR;
    // 注意：ENCUMBRANCE_SPEED_PENALTY_COEFF, ENCUMBRANCE_STAMINA_DRAIN_COEFF 现在从配置管理器获取
    // 注意：跳跃/翻越/攀爬消耗使用新的物理能量模型计算，旧的基准费用已移除
    static const float JUMP_MIN_STAMINA_THRESHOLD = SCR_RSS_Constants.JUMP_MIN_STAMINA_THRESHOLD;
    static const float JUMP_CONSECUTIVE_WINDOW = SCR_RSS_Constants.JUMP_CONSECUTIVE_WINDOW;
    static const float JUMP_CONSECUTIVE_PENALTY = SCR_RSS_Constants.JUMP_CONSECUTIVE_PENALTY;
    static const float PANDOLF_BASE_COEFF = SCR_RSS_Constants.PANDOLF_BASE_COEFF;
    static const float PANDOLF_VELOCITY_COEFF = SCR_RSS_Constants.PANDOLF_VELOCITY_COEFF;
    static const float PANDOLF_VELOCITY_OFFSET = SCR_RSS_Constants.PANDOLF_VELOCITY_OFFSET;
    static const float PANDOLF_GRADE_BASE_COEFF = SCR_RSS_Constants.PANDOLF_GRADE_BASE_COEFF;
    static const float PANDOLF_GRADE_VELOCITY_COEFF = SCR_RSS_Constants.PANDOLF_GRADE_VELOCITY_COEFF;
    static const float PANDOLF_STATIC_COEFF_1 = SCR_RSS_Constants.PANDOLF_STATIC_COEFF_1;
    static const float PANDOLF_STATIC_COEFF_2 = SCR_RSS_Constants.PANDOLF_STATIC_COEFF_2;
    // 注意：ENERGY_TO_STAMINA_COEFF 现在从配置管理器获取
    static const float REFERENCE_WEIGHT = SCR_RSS_Constants.REFERENCE_WEIGHT;
    static const float RECOVERY_STARTUP_DELAY_SECONDS = SCR_RSS_Constants.RECOVERY_STARTUP_DELAY_SECONDS;
    static const float BASE_WEIGHT = SCR_RSS_Constants.BASE_WEIGHT;
    static const float SLOPE_UPHILL_COEFF = SCR_RSS_Constants.SLOPE_UPHILL_COEFF;
    static const float SLOPE_DOWNHILL_COEFF = SCR_RSS_Constants.SLOPE_DOWNHILL_COEFF;
    static const float ENCUMBRANCE_SLOPE_INTERACTION_COEFF = SCR_RSS_Constants.ENCUMBRANCE_SLOPE_INTERACTION_COEFF;
    static const float CHARACTER_AGE = SCR_RSS_Constants.CHARACTER_AGE;
    static const float COMBAT_LOAD_WEIGHT = SCR_RSS_Constants.COMBAT_LOAD_WEIGHT;
    static const float SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = SCR_RSS_Constants.SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF;
    static const float GRADE_DOWNHILL_COEFF = SCR_RSS_Constants.GRADE_DOWNHILL_COEFF;
    // Tobler 平地参考值：W(0)=6·e^(-0.175)≈5.039 km/h，用于坡度速度归一化使平地=1.0
    static const float TOBLER_W_AT_FLAT_KMH = 5.039;
    
    // ==================== 数学工具函数 ====================
    // 注意：Pow() 函数已移至 SCR_RSS_StaminaHelpers.c 模块
    // 使用 SCR_RSS_StaminaHelpers.Pow() 调用
    
    // ==================== 核心计算函数 ====================
    
    // 根据体力百分比计算速度倍数（双稳态-应激性能模型）
    // 
    // 数学模型：基于双稳态-应激性能模型（Dual-State Stress Performance Model）
    // 核心理念：士兵的性能不应是体力的简单幂函数，而应是"意志力维持"与"生理极限崩溃"的结合
    //
    // 速度性能分段（Performance Plateau）：
    // 1. 平台期（Willpower Zone, Stamina 25% - 100%）：
    //    只要体力高于25%，士兵可以强行维持设定的目标速度（3.8 m/s）。
    //    这模拟了士兵在比赛或战斗中通过意志力克服早期疲劳。
    // 2. 衰减期（Failure Zone, Stamina 0% - 25%）：
    //    只有当体力掉入最后25%时，生理机能开始真正崩塌，速度迅速线性下降到跛行。
    //
    // @param staminaPercent 当前体力百分比 (0.0-1.0)
    // @return 速度倍数（相对于游戏最大速度）
    // @deprecated v6 保留 legacy 调用；新代码请用 CalculateV6PhaseSpeedMultiplier
    static float CalculateSpeedMultiplierByStamina(float staminaPercent)
    {
        return SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(staminaPercent, 2, 0.0);
    }

    // 获取基于当前负重惩罚的“跛行”速度倍率
    // 该倍率对应于当前负重下的最大允许Walk速度，而非固定1m/s。
    // @param encumbrancePenalty 负重造成的速度降低比例 (0.0-1.0)
    // @return 速度倍率（相对于 GAME_MAX_SPEED）
    static float GetDynamicLimpMultiplier(float encumbrancePenalty)
    {
        // 先计算未受负重影响的步行上限
        float maxWalkSpeed = WALK_VELOCITY_THRESHOLD;
        
        // 施加负重惩罚（减速）
        maxWalkSpeed *= (1.0 - encumbrancePenalty);
        
        // 保证不低于最小跛行速度，也不超过奔跑阈值
        float minLimp = EXHAUSTION_LIMP_SPEED;
        float runThresh = RUN_VELOCITY_THRESHOLD;
        maxWalkSpeed = Math.Clamp(maxWalkSpeed, minLimp, runThresh);
        
        return maxWalkSpeed / GAME_MAX_SPEED;
    }
    
    // 计算负重百分比（供其他系统使用）
    static float CalculateEncumbrancePercent(IEntity owner)
    {
        // 获取角色实体
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
        {
            // 调试：角色实体不存在
            // Print("[RealisticSystem] 负重计算: 角色实体不存在");
            return 0.0;
        }
        
        // 获取库存组件
        // 根据 SCR_CharacterInventoryStorageComponent 源代码：
        // GetMaxLoad() 方法在 SCR_CharacterInventoryStorageComponent 中，返回 m_fMaxWeight
        // 需要使用 SCR_CharacterInventoryStorageComponent 而不是 SCR_UniversalInventoryStorageComponent
        SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
        
        if (!characterInventory)
        {
            // 调试：找不到角色库存组件
            // Print("[RealisticSystem] 负重计算: 找不到角色库存组件");
            return 0.0;
        }
        
        // 获取当前总重量和最大重量
        // 根据 SCR_CharacterInventoryStorageComponent 源代码：
        // GetMaxLoad() 返回 m_fMaxWeight（最大可携带重量）
        // GetTotalWeight() 在基类中，返回当前库存中所有物品的总重量
        float currentWeight = characterInventory.GetTotalWeight();
        float maxWeight = characterInventory.GetMaxLoad();
        

        
        // 如果maxWeight和currentWeight都为0，说明可能方法不存在或返回默认值
        // 在这种情况下，暂时返回0（无负重），等待找到正确的API
        if (maxWeight <= 0.0 && currentWeight <= 0.0)
        {
            // 说明可能GetTotalWeight()和GetMaxLoad()方法不可用或返回值不正确
            // 暂时返回0，表示没有负重
            return 0.0;
        }
        
        // 如果maxWeight为0但currentWeight不为0，使用我们定义的常量
        if (maxWeight <= 0.0)
        {
            maxWeight = MAX_ENCUMBRANCE_WEIGHT; // 使用我们定义的最大负重
        }
        else
        {
            // 使用我们定义的最大负重，而不是游戏引擎的值
            // 这样可以确保所有计算都基于统一的负重标准
            maxWeight = MAX_ENCUMBRANCE_WEIGHT;
        }
        
        if (currentWeight < 0.0)
        {
            // 调试：当前重量为负数
            return 0.0;
        }
        
        // 计算负重百分比（基于最大可携带重量）
        // 使用我们定义的最大负重常量：
        // 负重百分比 = 当前重量 / 最大重量
        float encumbrancePercent = Math.Clamp(currentWeight / MAX_ENCUMBRANCE_WEIGHT, 0.0, 1.0);
        
        return encumbrancePercent;
    }
    
    // 计算战斗负重百分比（基于战斗负重阈值）
    // 战斗负重用于判断角色是否处于战斗负重状态
    // 
    // @param owner 角色实体
    // @return 战斗负重百分比 (0.0-1.0+)，1.0表示达到战斗负重阈值，>1.0表示超过战斗负重
    static float CalculateCombatEncumbrancePercent(IEntity owner)
    {
        // 获取角色实体
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return 0.0;
        
        // 获取库存组件
        SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
        if (!characterInventory)
            return 0.0;
        
        // 获取当前总重量
        float currentWeight = characterInventory.GetTotalWeight();
        if (currentWeight < 0.0)
            return 0.0;
        
        // 计算战斗负重百分比（基于战斗负重阈值）
        // 战斗负重百分比 = 当前重量 / 战斗负重阈值
        // 如果返回值 > 1.0，表示超过战斗负重阈值
        float combatEncumbrancePercent = currentWeight / COMBAT_ENCUMBRANCE_WEIGHT;
        
        return combatEncumbrancePercent;
    }
    
    // 计算多维度恢复率（基于个性化运动建模和生理学恢复模型）
    // 
    // 多维度恢复模型考虑以下因素：
    // 1. 当前体力百分比（非线性恢复）：体力越低恢复越快，体力越高恢复越慢
    // 2. 健康状态/训练水平：训练有素者恢复更快
    // 3. 休息时间：刚停止运动时恢复快（快速恢复期），长时间休息后恢复慢（慢速恢复期）
    // 4. 年龄：年轻者恢复更快
    // 5. 累积疲劳恢复：运动后的疲劳需要时间恢复
    // 6. 负重影响：重载下增加恢复速率上限，模拟士兵通过深呼吸快速调整的能力
    // 7. 姿态影响：不同休息姿势对肌肉紧张度和血液循环的影响
    //
    // @param staminaPercent 当前体力百分比（0.0-1.0）
    // @param restDurationMinutes 休息持续时间（分钟），从停止运动开始计算
    // @param exerciseDurationMinutes 运动持续时间（分钟），用于计算累积疲劳
    // @param currentWeight 当前负重（kg），用于优化重载下的恢复速率
    // @param stance 当前姿态（0=站立，1=蹲姿，2=趴姿）
    // @return 恢复率（每0.2秒），表示应该恢复的体力百分比
    static float CalculateMultiDimensionalRecoveryRate(float staminaPercent, float restDurationMinutes, float exerciseDurationMinutes, float currentWeight = 0.0, int stance = 0)
    {
        // 限制输入参数
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        restDurationMinutes = Math.Max(restDurationMinutes, 0.0);
        exerciseDurationMinutes = Math.Max(exerciseDurationMinutes, 0.0);
        
        // ==================== 深度生理压制：最低体力阈值限制 ====================
        // 医学解释：当体力过低时，身体处于极度疲劳状态，需要更长时间的休息才能开始恢复
        // [修复 v2.16.0] 改用动态 getter，确保 JSON 配置值实际生效
        float restDurationSeconds = restDurationMinutes * 60.0;
        float minStaminaThreshold = SCR_RSS_ConfigBridge.GetMinRecoveryStaminaThreshold();
        float minRestSeconds = SCR_RSS_ConfigBridge.GetMinRecoveryRestTimeSeconds();
        if (staminaPercent < minStaminaThreshold && restDurationSeconds < minRestSeconds)
        {
            return 0.0; // 体力低于阈值且休息时间不足时，不恢复
        }
        
        // ==================== 1. 基础恢复率（基于当前体力百分比，非线性）====================
        // 体力越低，恢复越快；体力越高，恢复越慢
        // 公式：recovery_rate = BASE_RECOVERY_RATE × (1.0 + RECOVERY_NONLINEAR_COEFF × (1.0 - stamina_percent))
        float recoveryNonlinearCoeff = SCR_RSS_ConfigBridge.GetRecoveryNonlinearCoeff();
        float staminaRecoveryMultiplier = 1.0 + (recoveryNonlinearCoeff * (1.0 - staminaPercent));
        float baseRecoveryRate = SCR_RSS_ConfigBridge.GetBaseRecoveryRate() * staminaRecoveryMultiplier;
        
        // ==================== 2. 健康状态/训练水平影响 ====================
        // 固定值（22岁训练有素男性，FITNESS_LEVEL=1.0）：预计算结果直接使用，防止不平等游玩
        // clamp(1.0 + 0.25 × 1.0, 1.0, 1.5) = 1.25
        float fitnessRecoveryMultiplier = SCR_RSS_Constants.FIXED_FITNESS_RECOVERY_MULTIPLIER;
        
        // ==================== 3. 休息时间影响（快速恢复期 vs 中等恢复期 vs 慢速恢复期）====================
        float restTimeMultiplier = 1.0;
        float fastRecoveryMultiplier = SCR_RSS_ConfigBridge.GetFastRecoveryMultiplier();
        float mediumRecoveryMultiplier = SCR_RSS_ConfigBridge.GetMediumRecoveryMultiplier();
        float slowRecoveryMultiplier = SCR_RSS_ConfigBridge.GetSlowRecoveryMultiplier();
        
        // 仅在实际累积休息后应用阶段倍数；rest=0 表示运动中/尚未进入 idle，不得触发「快速恢复」
        if (restDurationMinutes > 0.0)
        {
            if (restDurationMinutes <= FAST_RECOVERY_DURATION_MINUTES)
            {
                // 快速恢复期（0-24秒）：刚停下的 EPOC 后快速回血窗口
                restTimeMultiplier = fastRecoveryMultiplier;
            }
            else if (restDurationMinutes <= MEDIUM_RECOVERY_START_MINUTES + MEDIUM_RECOVERY_DURATION_MINUTES)
            {
                // 中等恢复期
                restTimeMultiplier = mediumRecoveryMultiplier;
            }
            else if (restDurationMinutes >= SLOW_RECOVERY_START_MINUTES)
            {
                // 慢速恢复期（≥10分钟）：线性过渡
                const float transitionDuration = 10.0; // 过渡时间（分钟）
                float transitionProgress = Math.Min((restDurationMinutes - SLOW_RECOVERY_START_MINUTES) / transitionDuration, 1.0);
                restTimeMultiplier = 1.0 - (transitionProgress * (1.0 - slowRecoveryMultiplier));
            }
        }
        // 否则：运动中或 rest 尚未累积，restTimeMultiplier = 1.0
        
        // ==================== 4. 年龄影响 ====================
        // 固定值（22岁标准男性）：预计算结果直接使用，防止不平等游玩
        // clamp(1.0 + 0.2 × (30-22)/30, 0.8, 1.2) = clamp(1.0533, 0.8, 1.2) = 1.053
        float ageRecoveryMultiplier = SCR_RSS_Constants.FIXED_AGE_RECOVERY_MULTIPLIER;
        
        // ==================== 5. 累积疲劳恢复影响 ====================
        // 运动后的疲劳需要时间恢复，影响恢复速度
        // 公式：fatigue_recovery_multiplier = 1.0 - FATIGUE_RECOVERY_PENALTY × min(exercise_duration / FATIGUE_RECOVERY_DURATION, 1.0)
        float fatigueRecoveryPenalty = FATIGUE_RECOVERY_PENALTY * Math.Min(exerciseDurationMinutes / FATIGUE_RECOVERY_DURATION_MINUTES, 1.0);
        float fatigueRecoveryMultiplier = 1.0 - fatigueRecoveryPenalty;
        fatigueRecoveryMultiplier = Math.Clamp(fatigueRecoveryMultiplier, 0.7, 1.0); // 限制在70%-100%之间
        
        // ==================== 6. 姿态恢复加成 ====================
        // 不同休息姿势对肌肉紧张度和血液循环的影响
        float stanceRecoveryMultiplier = 1.0;
        switch (stance)
        {
            case 0: // 站姿
                stanceRecoveryMultiplier = SCR_RSS_ConfigBridge.GetStandingRecoveryMultiplier();
                break;
            case 1: // 蹲姿
                stanceRecoveryMultiplier = SCR_RSS_ConfigBridge.GetCrouchingRecoveryMultiplier();
                break;
            case 2: // 趴姿
                stanceRecoveryMultiplier = SCR_RSS_ConfigBridge.GetProneRecoveryMultiplier();
                break;
            default:
                stanceRecoveryMultiplier = SCR_RSS_ConfigBridge.GetStandingRecoveryMultiplier();
                break;
        }
        
        // ==================== 深度生理压制：负重对恢复的静态剥夺机制 ====================
        // 医学解释：背负30kg装备站立时，斜方肌、腰椎和下肢肌肉仍在进行高强度静力收缩
        // 这种收缩产生的代谢废物（乳酸）排放速度远慢于空载状态
        // 数学实现：恢复率与负重百分比（BM%）挂钩
        // Penalty = (当前总重 / 身体耐受基准)^2 * 0.0004
        // 结果：穿着40kg装备站立恢复时，原本100%的基础恢复会被扣除70%
        // 战术意图：强迫重装兵必须趴下（通过姿态加成抵消负重扣除），否则回血极慢
        float loadRecoveryPenalty = 0.0;
        if (currentWeight > 0.0)
        {
            float loadRatio = currentWeight / BODY_TOLERANCE_BASE; // 负重比例（相对于身体耐受基准）
            loadRatio = Math.Clamp(loadRatio, 0.0, 2.0); // 限制在0-2倍之间
            // 负重惩罚 = (负重比例)^2 * 惩罚系数
            float loadRecoveryPenaltyCoeff = SCR_RSS_ConfigBridge.GetLoadRecoveryPenaltyCoeff();
            float loadRecoveryPenaltyExponent = SCR_RSS_ConfigBridge.GetLoadRecoveryPenaltyExponent();
            loadRecoveryPenalty = Math.Pow(loadRatio, loadRecoveryPenaltyExponent) * loadRecoveryPenaltyCoeff;
        }
        
        // ==================== 深度生理压制：边际效应衰减机制 ====================
        // 医学解释：身体从"半死不活"恢复到"喘匀气"很快（前80%），但要从"喘匀气"恢复到"肌肉巅峰竞技状态"非常慢（最后20%）
        // 数学实现：当体力>80%时，恢复率 = 原始恢复率 * (1.1 - 当前体力百分比)
        // 结果：最后10%的体力恢复速度会比前50%慢3-4倍
        // 战术意图：玩家经常会处于80%-90%的"亚健康"状态，只有长时间修整才能真正满血
        float marginalDecayMultiplier = 1.0;
        float marginalDecayThreshold = SCR_RSS_ConfigBridge.GetMarginalDecayThreshold();
        float marginalDecayCoeff = SCR_RSS_ConfigBridge.GetMarginalDecayCoeff();
        
        if (staminaPercent > marginalDecayThreshold)
        {
            // 当体力>80%时，应用边际效应衰减
            // 恢复率 = (1.1 - 当前体力百分比)
            // 例如：体力90%时，恢复率 = 1.1 - 0.9 = 0.2（即20%的原始恢复速度）
            marginalDecayMultiplier = marginalDecayCoeff - staminaPercent;
            marginalDecayMultiplier = Math.Clamp(marginalDecayMultiplier, 0.2, 1.0); // 限制在20%-100%之间
        }
        
        // ==================== 综合恢复率计算（深度生理压制版本）====================
        // 核心概念：统一为乘数链，避免“乘法链后减法”导致的逻辑歧义与调试困难。
        // 等价于原“乘积 - 负重惩罚”再钳位非负：loadFactor = max(0, 1 - penalty/product)，total = product * loadFactor。
        float productBeforeLoad = baseRecoveryRate * fitnessRecoveryMultiplier * restTimeMultiplier * ageRecoveryMultiplier * fatigueRecoveryMultiplier * stanceRecoveryMultiplier * marginalDecayMultiplier;
        float loadFactor = 1.0;
        if (productBeforeLoad > 0.0)
        {
            float penaltyRatio = loadRecoveryPenalty / productBeforeLoad;
            loadFactor = Math.Max(0.2, 1.0 - penaltyRatio); // 最低保留20%恢复率
        }
        else
        {
            loadFactor = 0.00005; // 确保恢复率不会完全归零
        }
        float totalRecoveryRate = productBeforeLoad * loadFactor;

        return totalRecoveryRate;
    }
    
    // 计算速度×负重×坡度三维交互项（基于 Pandolf 模型）
    // 
    // 完整 Pandolf 模型中的三维交互项：G·(0.23 + 1.34·V²)
    // 其中负重会影响总质量M，进而影响整个公式
    // 
    // 改进模型：三维交互项 = interaction_coeff × (负重/体重) × speed_ratio² × |坡度|
    // 这个项表示：在高速度、高负重、大坡度时，消耗会急剧增加
    //
    // @param speedRatio 速度比（0.0-1.0），当前速度/最大速度
    // @param bodyMassPercent 负重占体重的百分比（0.0-1.0+）
    // @param slopeAngleDegrees 坡度角度（度），正数=上坡，负数=下坡
    // @return 三维交互项系数（0.0-1.0），表示额外的消耗倍数
    static float CalculateSpeedEncumbranceSlopeInteraction(float speedRatio, float bodyMassPercent, float slopeAngleDegrees)
    {
        // 限制输入参数
        speedRatio = Math.Clamp(speedRatio, 0.0, 1.0);
        bodyMassPercent = Math.Max(bodyMassPercent, 0.0);
        slopeAngleDegrees = Math.Clamp(slopeAngleDegrees, -45.0, 45.0);
        
        // 只在上坡时计算交互项（下坡时交互项影响较小）
        if (slopeAngleDegrees <= 0.0 || bodyMassPercent <= 0.0)
            return 0.0;
        
        // 三维交互项：速度² × 负重 × 坡度
        // 例如：速度80%、30kg负重（33%体重）、5°上坡
        // = 0.10 × 0.33 × 0.8² × 5 = 0.10 × 0.33 × 0.64 × 5 = 0.106倍额外消耗
        float interactionTerm = SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF * bodyMassPercent * speedRatio * speedRatio * slopeAngleDegrees;
        
        // 限制交互项在合理范围内（0.0-0.5，最多增加50%消耗）
        return Math.Clamp(interactionTerm, 0.0, 0.5);
    }
    
    // 计算爆发性动作的体力消耗（动态负重倍率）
    // 
    // 对于跳跃和翻越等爆发性动作，负重的影响是指数级的
    // 公式：实际消耗 = 基础消耗 * (currentWeight / CHARACTER_WEIGHT) ^ 1.5
    // 
    // 效果示例：
    // - 空载（90kg）：基础消耗 × 1.0 = 基础消耗
    // - 30kg负重（总重120kg）：基础消耗 × (120/90)^1.5 = 基础消耗 × 1.33^1.5 ≈ 基础消耗 × 1.54
    // - 40kg负重（总重130kg）：基础消耗 × (130/90)^1.5 = 基础消耗 × 1.44^1.5 ≈ 基础消耗 × 1.73
    // 
    // 这体现了重负荷下对抗重力的额外代价
    //
    // @param baseCost 基础消耗（0.0-1.0）
    // @param currentWeight 当前总重量（kg），包括身体重量和负重
    // @return 实际消耗（0.0-1.0）
    // 物理模型估算单次跳跃能量消耗（体力百分比）
    // m: 体重(kg); h: 抬升高度(m); v: 水平速度(m/s); eta: 肌肉效率(0.2-0.25)
    static float ComputeJumpCostPhys(float m, float h, float v, float eta)
    {
        const float g = SCR_RSS_Constants.JUMP_GRAVITY;
        float epot = m * g * h;
        float ekin = 0.5 * m * v * v;
        float joules = (epot + ekin) / Math.Max(eta, 0.01);
        // 1体力 ≈ 75kcal ≈ JUMP_STAMINA_TO_JOULES J
        return Math.Clamp(joules / SCR_RSS_Constants.JUMP_STAMINA_TO_JOULES, 0.0, SCR_RSS_Constants.JUMP_VAULT_MAX_DRAIN_CLAMP);
    }

    // 物理模型估算攀爬能量（每秒体力百分比）
    // m: 体重; v_vert: 垂直速度; F_iso: 四肢等长力; eta_iso: 等长效率
    static float ComputeClimbCostPhys(float m, float v_vert, float F_iso, float eta_iso)
    {
        const float g = SCR_RSS_Constants.JUMP_GRAVITY;
        float p_vert = m * g * v_vert;
        float p_iso = eta_iso * F_iso;
        float p_total = p_vert + p_iso + SCR_RSS_Constants.VAULT_BASE_METABOLISM_WATTS;
        float joules = p_total * 0.2; // step = UPDATE_INTERVAL = 0.2s
        return Math.Clamp(joules / SCR_RSS_Constants.JUMP_STAMINA_TO_JOULES, 0.0, SCR_RSS_Constants.JUMP_VAULT_MAX_DRAIN_CLAMP);
    }
    
    // ==================== 军事体力系统模型：基于速度阈值的分段消耗率（重新校准）====================
    // 根据当前速度和负重计算基础消耗率（使用临界动力概念，更温和的负重公式）
    // 
    // 负重消耗重新校准（Load Calibration）：
    // - 基于临界动力（Critical Power）概念：将30kg定义为"标准战斗负载"
    // - 非线性增长：负重对体力的消耗不再是简单的倍数，而是：基础消耗 + (负重/体重)^1.2 * 1.5
    // - 这样可以确保30kg负载下仍能坚持约16分钟，满足ACFT测试要求
    //
    // 速度阈值分段（根据负重动态调整）：
    // - Sprint: V ≥ 5.0 m/s → 消耗 × loadFactor
    // - Run: 3.2 ≤ V < 5.0 m/s → 消耗 × loadFactor（核心修正：大幅调低RUN的基础消耗）
    // - Walk: V < 3.2 m/s → 消耗 × loadFactor
    // - Rest: V < 动态阈值 → -0.250 pts/s（恢复）
    //
    // 动态阈值计算：
    // - 空载（0kg）时：恢复阈值 = 2.5 m/s（速度 < 2.5 m/s 时恢复）
    // - 负重30kg时：消耗阈值 = 1.5 m/s（速度 > 1.5 m/s 时开始消耗）
    // - 其他负重：线性插值
    //
    // @param velocity 当前速度（m/s）
    // @param currentWeight 当前负重（kg）
    // @return 基础消耗率（每0.2秒，负数表示恢复）
    static float CalculateBaseDrainRateByVelocity(float velocity, float currentWeight = 0.0)
    {
        // 计算动态阈值（基于负重线性插值）
        // 空载（0kg）时：恢复阈值 = 2.5 m/s
        // 负重30kg时：消耗阈值 = 1.5 m/s
        float dynamicThreshold = 0.0;
        if (currentWeight <= 0.0)
        {
            // 空载：恢复阈值 = 2.5 m/s
            dynamicThreshold = RECOVERY_THRESHOLD_NO_LOAD;
        }
        else if (currentWeight >= COMBAT_LOAD_WEIGHT)
        {
            // 负重≥30kg：消耗阈值 = 1.5 m/s
            dynamicThreshold = DRAIN_THRESHOLD_COMBAT_LOAD;
        }
        else
        {
            // 线性插值：0kg（2.5 m/s） → 30kg（1.5 m/s）
            float t = currentWeight / COMBAT_LOAD_WEIGHT; // t in [0, 1]
            dynamicThreshold = RECOVERY_THRESHOLD_NO_LOAD * (1.0 - t) + DRAIN_THRESHOLD_COMBAT_LOAD * t;
        }
        
        // 重新计算负重比例（基于体重）
        float weightRatio = currentWeight / CHARACTER_WEIGHT; // 30kg时约为0.33
        
        // 负重影响因子：不再是简单的乘法，而是一个温和的增量
        // loadFactor = 1.0 + (weightRatio^1.2) * 1.5
        // 这样可以确保30kg时消耗不会翻倍，而是更合理的增长
        float loadFactor = 1.0;
        if (currentWeight > 0.0)
        {
            // 使用SCR_RSS_StaminaHelpers.Pow计算 weightRatio^1.2
            float weightRatioPower = SCR_RSS_StaminaHelpers.Pow(weightRatio, 1.2);
            loadFactor = 1.0 + (weightRatioPower * 1.5);
        }
        
        // 根据速度和动态阈值计算消耗率
        if (velocity < dynamicThreshold)
        {
            // 速度 < 动态阈值：恢复
            return -0.05; // Rest恢复（负数），每0.2秒，与恢复模型调参一致
        }
        
        // 所有移动阶段使用 Pandolf 公式计算消耗，再乘以负重因子。
        // 由于本函数并不直接感知坡度，将其设为0（平地）。
        const float gradePercent = 0.0;
        float pandolfPerS = SCR_RSS_MetabolismModel.CalculatePandolfEnergyExpenditure(
            velocity, currentWeight + CHARACTER_WEIGHT, gradePercent, 1.0, true, -1);
        return pandolfPerS * 0.2 * loadFactor;
    }

    // ==================== Pandolf / 静态（v6 委托 SCR_RSS_MetabolismModel）====================

    static float CalculatePandolfEnergyExpenditure(
        float velocity,
        float currentWeight,
        float gradePercent,
        float terrainFactor,
        bool useSanteeCorrection)
    {
        return SCR_RSS_MetabolismModel.CalculatePandolfEnergyExpenditure(
            velocity, currentWeight, gradePercent, terrainFactor, useSanteeCorrection, -1);
    }

    static float CalculateStaticStandingCost(float bodyWeight, float loadWeight)
    {
        return SCR_RSS_MetabolismModel.CalculateStaticStandingCost(bodyWeight, loadWeight);
    }

    // 计算坡度自适应目标速度（坡度-速度负反馈）
    // 
    // 问题分析：现实中人爬坡时，会自动缩短步幅、降低速度以维持心肺负荷（体力消耗）。
    // 目前的系统是"强行维持速度但暴扣体力"，这导致了代谢率在斜坡上迅速过载。
    //
    // 解决方案：实施"坡度-速度负反馈"。当上坡角度增加时，系统自动略微下调当前的"目标速度"，
    // 从而换取更持久的续航。这样玩家在Everon爬小缓坡时，会感觉到角色稍微"沉"了一点点，
    // 但体力掉速依然平稳。
    //
    // 使用托布勒徒步函数 (Tobler's Hiking Function, 1993)
    // 公式：W = 6 · e^(-3.5 · |S + 0.05|)，其中 W 为步行速度 (km/h)，S 为坡度 (tan θ)
    // 特点：最大速度出现在约 -3° 到 -5° 的小下坡上（约 6 km/h），上坡或过陡下坡都会快速衰减
    //
    // 归一化：以平地 (S=0) 为 1.0，使 0 kg 平地下能达到引擎最大速度 (5.5 m/s)。
    // 0kg 下实测：Sprint 最大 5.5 m/s，Run 最大 3.8 m/s。
    // 平地参考值：W(0) = 6·e^(-0.175) ≈ 5.04 km/h
    //
    // @param baseTargetSpeed 基础目标速度（m/s），例如 3.8 m/s
    // @param slopeAngleDegrees 坡度角度（度），正数=上坡，负数=下坡
    // @return 坡度自适应后的目标速度（m/s）
    static float CalculateSlopeAdjustedTargetSpeed(float baseTargetSpeed, float slopeAngleDegrees)
    {
        // 坡度 S = tan(θ)，即垂直位移/水平位移（rise/run）
        float S = Math.Tan(slopeAngleDegrees * Math.DEG2RAD);
        // 限制 S 范围，避免极端角度导致数值异常
        S = Math.Clamp(S, -1.0, 1.0);

        // Tobler: W(km/h) = 6 · e^(-3.5 · |S + 0.05|)
        float exponent = -3.5 * Math.AbsFloat(S + 0.05);
        float W_kmh = 6.0 * Math.Pow(2.718281828, exponent);

        // 以平地 (S=0) 为 1.0，使平地下满速（引擎 5.5 m/s）
        float toblerMultiplier = W_kmh / TOBLER_W_AT_FLAT_KMH;
        // 设置下限，避免陡坡时速度过慢
        toblerMultiplier = Math.Max(toblerMultiplier, 0.15);

        // 玩家反馈：上坡再快一点、下坡更快（重力主导）
        if (slopeAngleDegrees > 0.0)
        {
            toblerMultiplier = toblerMultiplier * SCR_RSS_Constants.UPHILL_SPEED_BOOST;
        }
        else if (slopeAngleDegrees < 0.0)
        {
            toblerMultiplier = toblerMultiplier * SCR_RSS_Constants.DOWNHILL_SPEED_BOOST;
            toblerMultiplier = Math.Min(toblerMultiplier, SCR_RSS_Constants.DOWNHILL_SPEED_MAX_MULTIPLIER);
        }

        // 坡度速度倍率缩减 30%（设计意图）：
        // Tobler 函数在缓坡（3°~8°）的输出约 0.85~0.95，乘 UPHILL_SPEED_BOOST(1.15)
        // 后约 0.98~1.09。直接应用会导致玩家在肉眼不可察的微坡上也被减速。
        // 缩减 30% 后：缓坡≈0.99~1.06（几乎无感），15°陡坡≈0.73（仍明显降速）。
        // 这保持了"陡坡有体感、缓坡不折腾"的平衡。
        toblerMultiplier = 1.0 + 0.7 * (toblerMultiplier - 1.0);
        toblerMultiplier = Math.Clamp(toblerMultiplier, 0.15, SCR_RSS_Constants.DOWNHILL_SPEED_MAX_MULTIPLIER);

        return baseTargetSpeed * toblerMultiplier;
    }
    
    // 检查是否精疲力尽
    // 
    // @param staminaPercent 当前体力百分比（0.0-1.0）
    // @return true表示精疲力尽，false表示未精疲力尽
    static bool IsExhausted(float staminaPercent)
    {
        return staminaPercent <= EXHAUSTION_THRESHOLD;
    }
    
    // 检查是否可以Sprint
    // 
    // @param staminaPercent 当前体力百分比（0.0-1.0）
    // @return true表示可以Sprint，false表示不能Sprint
    static bool CanSprint(float staminaPercent)
    {
        return staminaPercent >= SCR_RSS_ConfigBridge.GetSprintEnableThreshold();
    }
    
    // ==================== 地形系数系统（基于实际测试数据的插值映射）====================
    // 地形系数常量（引用 SCR_RSS_Constants 以消除重复）
    // 之前此处有独立的 TERRAIN_FACTOR_* 局部常量，与 SCR_RSS_Constants.TERRAIN_FACTOR_*
    // 重复定义（值完全一致）。移除去重，统一通过 SCR_RSS_Constants.TERRAIN_FACTOR_* 访问。
    
    // 根据密度值获取地形系数（插值映射，与 MaterialTerrainTable 内嵌密度表一致）
    // 典型 g/cm³：snow 0.35 | grass/heather 0.5 | grass_lush 1.2 | dirt/soil 1.33 | sand 1.63 |
    //            gravel 1.682 | pebbles 1.7~1.79 | asphalt/concrete 2.243~2.3 | cobble/stone 2.75 | tiles_stone 2.94
    // 说明：密度无法区分同区间不同材质（如木 0.65 与草 0.5），0.36~0.72 仍保持 η≈1.0 以兼容木箱等
    // @param density BallisticInfo.GetDensity()，约 0.02~3.0
    // @return 地形系数 η（约 1.0~1.85）
    static float GetTerrainFactorFromDensity(float density)
    {
        if (density <= 0.0)
            return SCR_RSS_Constants.TERRAIN_FACTOR_PAVED;

        // 低密：积雪 / 极干草地（CSV snow 0.35；grass_dry 0.3）— 明显难于铺装
        if (density <= 0.36)
        {
            float t = density / 0.36;
            return 1.12 + (1.0 - t) * 0.08;
        }

        // 0.36~0.72：草地/植被/木质道具等混叠，维持与历史行为接近的 η=1.0
        if (density <= 0.72)
            return SCR_RSS_Constants.TERRAIN_FACTOR_PAVED;

        // 0.72~1.13：室内地坪、地毯等
        if (density <= 1.13)
            return SCR_RSS_Constants.TERRAIN_FACTOR_PAVED;

        // 1.13~1.2：地毯(1.13)→草丛(grass_lush 1.2)
        if (density <= 1.2)
        {
            float t = (density - 1.13) / (1.2 - 1.13);
            return 1.0 + t * 0.2;
        }

        // 1.2~1.33：草丛→土/泥（dirt/soil/dirt_road 1.33）
        if (density <= 1.33)
        {
            float t = (density - 1.2) / (1.33 - 1.2);
            return 1.2 - t * 0.1;
        }

        // 1.33~1.55：土、作物（soil_forest*、grass_crops* 等）
        if (density <= 1.55)
        {
            float t = (density - 1.33) / (1.55 - 1.33);
            return 1.1 + t * 0.28;
        }

        // 1.55~1.86：沙/砾/卵石峰值区（sand 1.63, gravel 1.682, pebbles_* 1.7~1.79）— 对齐 TERRAIN_FACTOR_SAND 量级
        if (density <= 1.86)
        {
            if (density <= 1.63)
            {
                float t = (density - 1.55) / (1.63 - 1.55);
                return 1.38 + t * 0.3;
            }
            if (density <= 1.79)
            {
                float t = (density - 1.63) / (1.79 - 1.63);
                return 1.68 + t * 0.06;
            }
            float t2 = (density - 1.79) / (1.86 - 1.79);
            return 1.74 + t2 * (1.6 - 1.74);
        }

        // 1.86~2.2：过渡到铺装（沥青前缘）
        if (density < 2.2)
        {
            float t = (density - 1.86) / (2.2 - 1.86);
            return 1.6 - t * 0.6;
        }

        // 2.2~2.42：沥青、混凝土（2.243、2.3）
        if (density <= 2.42)
            return SCR_RSS_Constants.TERRAIN_FACTOR_PAVED;

        // 2.42~2.76：块石/石材路面（cobblestone/stone 2.75）
        if (density <= 2.76)
        {
            float t = (density - 2.42) / (2.76 - 2.42);
            return 1.0 + t * 0.48;
        }

        // 2.76~2.94：陶土/石砖等（tiles_stone 2.94）
        if (density <= 2.94)
        {
            float t = (density - 2.76) / (2.94 - 2.76);
            return 1.48 + t * 0.32;
        }

        return Math.Clamp(1.8 + (density - 2.94) * 0.08, 1.8, 2.5);
    }

}