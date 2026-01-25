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
class RealisticStaminaSpeedSystem
{
    // ==================== 常量定义 ====================
    // 注意：所有常量定义已移至 SCR_StaminaConstants.c 模块
    // 使用 StaminaConstants.XXX 访问常量
    
    // 为了保持向后兼容，保留一些常用的常量别名
    // 这些别名直接引用 StaminaConstants 中的值
    // 注意：部分常量现在从配置管理器动态获取
    static const float GAME_MAX_SPEED = StaminaConstants.GAME_MAX_SPEED;
    static const float TARGET_RUN_SPEED = StaminaConstants.TARGET_RUN_SPEED;
    static const float TARGET_RUN_SPEED_MULTIPLIER = StaminaConstants.TARGET_RUN_SPEED_MULTIPLIER;
    static const float MIN_LIMP_SPEED_MULTIPLIER = StaminaConstants.MIN_LIMP_SPEED_MULTIPLIER;
    static const float STAMINA_EXPONENT = StaminaConstants.STAMINA_EXPONENT;
    static const float CHARACTER_WEIGHT = StaminaConstants.CHARACTER_WEIGHT;
    static const float MAX_ENCUMBRANCE_WEIGHT = StaminaConstants.MAX_ENCUMBRANCE_WEIGHT;
    static const float COMBAT_ENCUMBRANCE_WEIGHT = StaminaConstants.COMBAT_ENCUMBRANCE_WEIGHT;
    static const float SMOOTH_TRANSITION_START = StaminaConstants.SMOOTH_TRANSITION_START;
    static const float SMOOTH_TRANSITION_END = StaminaConstants.SMOOTH_TRANSITION_END;
    static const float MIN_SPEED_MULTIPLIER = StaminaConstants.MIN_SPEED_MULTIPLIER;
    static const float MAX_SPEED_MULTIPLIER = StaminaConstants.MAX_SPEED_MULTIPLIER;
    static const float INITIAL_STAMINA_AFTER_ACFT = StaminaConstants.INITIAL_STAMINA_AFTER_ACFT;
    static const float EXHAUSTION_THRESHOLD = StaminaConstants.EXHAUSTION_THRESHOLD;
    static const float EXHAUSTION_LIMP_SPEED = StaminaConstants.EXHAUSTION_LIMP_SPEED;
    static const float SPRINT_ENABLE_THRESHOLD = StaminaConstants.SPRINT_ENABLE_THRESHOLD;
    static const float SPRINT_SPEED_BOOST = StaminaConstants.SPRINT_SPEED_BOOST;
    // 注意：SPRINT_STAMINA_DRAIN_MULTIPLIER 现在从配置管理器获取
    static const float RECOVERY_THRESHOLD_NO_LOAD = StaminaConstants.RECOVERY_THRESHOLD_NO_LOAD;
    static const float DRAIN_THRESHOLD_COMBAT_LOAD = StaminaConstants.DRAIN_THRESHOLD_COMBAT_LOAD;
    static const float SPRINT_VELOCITY_THRESHOLD = StaminaConstants.SPRINT_VELOCITY_THRESHOLD;
    static const float RUN_VELOCITY_THRESHOLD = StaminaConstants.RUN_VELOCITY_THRESHOLD;
    static const float WALK_VELOCITY_THRESHOLD = StaminaConstants.WALK_VELOCITY_THRESHOLD;
    static const float FITNESS_LEVEL = StaminaConstants.FITNESS_LEVEL;
    static const float FITNESS_EFFICIENCY_COEFF = StaminaConstants.FITNESS_EFFICIENCY_COEFF;
    static const float FITNESS_RECOVERY_COEFF = StaminaConstants.FITNESS_RECOVERY_COEFF;
    // 注意：BASE_RECOVERY_RATE 现在从配置管理器获取
    static const float RECOVERY_NONLINEAR_COEFF = StaminaConstants.RECOVERY_NONLINEAR_COEFF;
    static const float FAST_RECOVERY_DURATION_MINUTES = StaminaConstants.FAST_RECOVERY_DURATION_MINUTES;
    static const float FAST_RECOVERY_MULTIPLIER = StaminaConstants.FAST_RECOVERY_MULTIPLIER;
    static const float MEDIUM_RECOVERY_START_MINUTES = StaminaConstants.MEDIUM_RECOVERY_START_MINUTES;
    static const float MEDIUM_RECOVERY_DURATION_MINUTES = StaminaConstants.MEDIUM_RECOVERY_DURATION_MINUTES;
    static const float MEDIUM_RECOVERY_MULTIPLIER = StaminaConstants.MEDIUM_RECOVERY_MULTIPLIER;
    static const float SLOW_RECOVERY_START_MINUTES = StaminaConstants.SLOW_RECOVERY_START_MINUTES;
    static const float SLOW_RECOVERY_MULTIPLIER = StaminaConstants.SLOW_RECOVERY_MULTIPLIER;
    static const float AGE_RECOVERY_COEFF = StaminaConstants.AGE_RECOVERY_COEFF;
    static const float AGE_REFERENCE = StaminaConstants.AGE_REFERENCE;
    static const float FATIGUE_RECOVERY_PENALTY = StaminaConstants.FATIGUE_RECOVERY_PENALTY;
    static const float FATIGUE_RECOVERY_DURATION_MINUTES = StaminaConstants.FATIGUE_RECOVERY_DURATION_MINUTES;
    // 注意：STANDING_RECOVERY_MULTIPLIER, PRONE_RECOVERY_MULTIPLIER 现在从配置管理器获取
    // 注意：LOAD_RECOVERY_PENALTY_COEFF, LOAD_RECOVERY_PENALTY_EXPONENT 现在从配置管理器获取
    static const float BODY_TOLERANCE_BASE = StaminaConstants.BODY_TOLERANCE_BASE;
    static const float MARGINAL_DECAY_THRESHOLD = StaminaConstants.MARGINAL_DECAY_THRESHOLD;
    static const float MARGINAL_DECAY_COEFF = StaminaConstants.MARGINAL_DECAY_COEFF;
    static const float MIN_RECOVERY_STAMINA_THRESHOLD = StaminaConstants.MIN_RECOVERY_STAMINA_THRESHOLD;
    static const float MIN_RECOVERY_REST_TIME_SECONDS = StaminaConstants.MIN_RECOVERY_REST_TIME_SECONDS;
    // 注意：FATIGUE_ACCUMULATION_COEFF, FATIGUE_MAX_FACTOR 现在从配置管理器获取
    static const float AEROBIC_THRESHOLD = StaminaConstants.AEROBIC_THRESHOLD;
    static const float ANAEROBIC_THRESHOLD = StaminaConstants.ANAEROBIC_THRESHOLD;
    static const float AEROBIC_EFFICIENCY_FACTOR = StaminaConstants.AEROBIC_EFFICIENCY_FACTOR;
    static const float ANAEROBIC_EFFICIENCY_FACTOR = StaminaConstants.ANAEROBIC_EFFICIENCY_FACTOR;
    // 注意：ENCUMBRANCE_SPEED_PENALTY_COEFF, ENCUMBRANCE_STAMINA_DRAIN_COEFF 现在从配置管理器获取
    static const float JUMP_STAMINA_BASE_COST = StaminaConstants.JUMP_STAMINA_BASE_COST;
    static const float VAULT_STAMINA_START_COST = StaminaConstants.VAULT_STAMINA_START_COST;
    static const float CLIMB_STAMINA_TICK_COST = StaminaConstants.CLIMB_STAMINA_TICK_COST;
    static const float JUMP_MIN_STAMINA_THRESHOLD = StaminaConstants.JUMP_MIN_STAMINA_THRESHOLD;
    static const float JUMP_CONSECUTIVE_WINDOW = StaminaConstants.JUMP_CONSECUTIVE_WINDOW;
    static const float JUMP_CONSECUTIVE_PENALTY = StaminaConstants.JUMP_CONSECUTIVE_PENALTY;
    static const float PANDOLF_BASE_COEFF = StaminaConstants.PANDOLF_BASE_COEFF;
    static const float PANDOLF_VELOCITY_COEFF = StaminaConstants.PANDOLF_VELOCITY_COEFF;
    static const float PANDOLF_VELOCITY_OFFSET = StaminaConstants.PANDOLF_VELOCITY_OFFSET;
    static const float PANDOLF_GRADE_BASE_COEFF = StaminaConstants.PANDOLF_GRADE_BASE_COEFF;
    static const float PANDOLF_GRADE_VELOCITY_COEFF = StaminaConstants.PANDOLF_GRADE_VELOCITY_COEFF;
    static const float PANDOLF_STATIC_COEFF_1 = StaminaConstants.PANDOLF_STATIC_COEFF_1;
    static const float PANDOLF_STATIC_COEFF_2 = StaminaConstants.PANDOLF_STATIC_COEFF_2;
    // 注意：ENERGY_TO_STAMINA_COEFF 现在从配置管理器获取
    static const float REFERENCE_WEIGHT = StaminaConstants.REFERENCE_WEIGHT;
    static const float GIVONI_CONSTANT = StaminaConstants.GIVONI_CONSTANT;
    static const float GIVONI_VELOCITY_EXPONENT = StaminaConstants.GIVONI_VELOCITY_EXPONENT;
    static const float RECOVERY_STARTUP_DELAY_SECONDS = StaminaConstants.RECOVERY_STARTUP_DELAY_SECONDS;
    static const float BASE_WEIGHT = StaminaConstants.BASE_WEIGHT;
    static const float SLOPE_UPHILL_COEFF = StaminaConstants.SLOPE_UPHILL_COEFF;
    static const float SLOPE_DOWNHILL_COEFF = StaminaConstants.SLOPE_DOWNHILL_COEFF;
    static const float ENCUMBRANCE_SLOPE_INTERACTION_COEFF = StaminaConstants.ENCUMBRANCE_SLOPE_INTERACTION_COEFF;
    static const float CHARACTER_AGE = StaminaConstants.CHARACTER_AGE;
    static const float COMBAT_LOAD_WEIGHT = StaminaConstants.COMBAT_LOAD_WEIGHT;
    static const float SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = StaminaConstants.SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF;
    static const float SPRINT_DRAIN_PER_TICK = StaminaConstants.SPRINT_DRAIN_PER_TICK;
    static const float GRADE_DOWNHILL_COEFF = StaminaConstants.GRADE_DOWNHILL_COEFF;
    
    // ==================== 数学工具函数 ====================
    // 注意：Pow() 函数已移至 SCR_StaminaHelpers.c 模块
    // 使用 StaminaHelpers.Pow() 调用
    
    // ==================== 核心计算函数 ====================
    
    // 根据体力百分比计算速度倍数（双稳态-应激性能模型）
    // 
    // 数学模型：基于双稳态-应激性能模型（Dual-State Stress Performance Model）
    // 核心理念：士兵的性能不应是体力的简单幂函数，而应是"意志力维持"与"生理极限崩溃"的结合
    //
    // 速度性能分段（Performance Plateau）：
    // 1. 平台期（Willpower Zone, Stamina 25% - 100%）：
    //    只要体力高于25%，士兵可以强行维持设定的目标速度（3.7 m/s）。
    //    这模拟了士兵在比赛或战斗中通过意志力克服早期疲劳。
    // 2. 衰减期（Failure Zone, Stamina 0% - 25%）：
    //    只有当体力掉入最后25%时，生理机能开始真正崩塌，速度迅速线性下降到跛行。
    //
    // @param staminaPercent 当前体力百分比 (0.0-1.0)
    // @return 速度倍数（相对于游戏最大速度）
    static float CalculateSpeedMultiplierByStamina(float staminaPercent)
    {
        // 确保体力百分比在有效范围内
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        float baseSpeedMultiplier = 0.0;
        
        if (staminaPercent >= SMOOTH_TRANSITION_START)
        {
            // 意志力平台期（25%-100%）：保持恒定目标速度（3.7 m/s）
            // 模拟士兵通过意志力克服早期疲劳，维持恒定性能
            baseSpeedMultiplier = TARGET_RUN_SPEED_MULTIPLIER;
        }
        else if (staminaPercent >= SMOOTH_TRANSITION_END)
        {
            // 平滑过渡期（5%-25%）：使用SmoothStep建立缓冲区，避免突兀的"撞墙"效果
            // 让开始下降时更柔和，接近力竭时下降更快
            // t = (stamina - SMOOTH_TRANSITION_END) / (SMOOTH_TRANSITION_START - SMOOTH_TRANSITION_END)
            // smoothT = t² × (3 - 2t)，这是一个平滑的S型曲线
            float t = (staminaPercent - SMOOTH_TRANSITION_END) / (SMOOTH_TRANSITION_START - SMOOTH_TRANSITION_END); // 0.0-1.0
            t = Math.Clamp(t, 0.0, 1.0);
            float smoothT = t * t * (3.0 - 2.0 * t); // smoothstep函数
            
            // 在目标速度和跛行速度之间平滑过渡
            // 当体力从25%降到5%时，速度从3.7m/s平滑降到跛行速度
            baseSpeedMultiplier = MIN_LIMP_SPEED_MULTIPLIER + (TARGET_RUN_SPEED_MULTIPLIER - MIN_LIMP_SPEED_MULTIPLIER) * smoothT;
        }
        else
        {
            // 生理崩溃期（0%-5%）：速度快速线性下降到跛行速度
            // 0.05时为平滑过渡终点速度，0时为1.0m/s（MIN_LIMP_SPEED_MULTIPLIER）
            float collapseFactor = staminaPercent / SMOOTH_TRANSITION_END; // 0.0-1.0
            // 计算平滑过渡终点的速度（在5%体力时，此时smoothT=0，速度为MIN_LIMP_SPEED_MULTIPLIER）
            baseSpeedMultiplier = MIN_LIMP_SPEED_MULTIPLIER * collapseFactor;
            // 确保不会低于最小速度
            baseSpeedMultiplier = Math.Max(baseSpeedMultiplier, MIN_LIMP_SPEED_MULTIPLIER * 0.8); // 最低不低于跛行速度的80%
        }
        
        // 应用最小速度限制（防止体力完全耗尽时无法移动）
        baseSpeedMultiplier = Math.Max(baseSpeedMultiplier, MIN_SPEED_MULTIPLIER);
        
        // 应用最大速度限制（防止超过游戏引擎限制）
        baseSpeedMultiplier = Math.Min(baseSpeedMultiplier, MAX_SPEED_MULTIPLIER);
        
        return baseSpeedMultiplier;
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
    
    // 检查是否超过战斗负重阈值
    // 
    // @param owner 角色实体
    // @return true表示超过战斗负重阈值，false表示未超过
    static bool IsOverCombatEncumbrance(IEntity owner)
    {
        return CalculateCombatEncumbrancePercent(owner) > 1.0;
    }
    
    // 计算负重对速度的影响（基于体重的真实模型）
    // 
    // 真实数学模型：基于 US Army 背包负重实验数据（Knapik et al., 1996; Quesada et al., 2000）
    // 速度惩罚基于体重百分比，而不是最大负重百分比
    // 速度惩罚 = β * (负重/体重)
    // 其中：
    //   β = ENCUMBRANCE_SPEED_PENALTY_COEFF（基于体重的速度惩罚系数）
    //   负重/体重 = 负重占体重的百分比（Body Mass Percentage, BM%）
    //
    // 实验数据参考（基于体重百分比）：
    // - 负重 0% BM 时：无影响
    // - 负重 20-30% BM 时：速度下降约 5-10%
    // - 负重 33% BM 时：速度下降约 6-7%（30kg ≈ 33%体重，90kg）
    // - 负重 40-50% BM 时：速度下降约 15-20%
    // - 负重 60-70% BM 时：速度下降约 24-30%
    // - 负重 100% BM 时：速度下降约 35-40%
    //
    // 使用线性模型，因为文献显示速度下降与体重百分比基本线性相关
    //
    // @param owner 角色实体
    // @return 速度惩罚值 (0.0-1.0)，表示速度减少的比例
    static float CalculateEncumbranceSpeedPenalty(IEntity owner)
    {
        // 获取当前负重（kg）
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return 0.0;
        
        SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
        if (!characterInventory)
            return 0.0;
        
        float currentWeight = characterInventory.GetTotalWeight();
        if (currentWeight < 0.0)
            return 0.0;
        
        // 计算有效负重（减去基准重量，基准重量是基本战斗装备）
        // 基准重量（12kg）是基本战斗装备的重量，不应该影响速度和消耗
        float effectiveWeight = Math.Max(currentWeight - BASE_WEIGHT, 0.0);
        
        // 计算有效负重占体重的百分比（Body Mass Percentage）
        float bodyMassPercent = effectiveWeight / CHARACTER_WEIGHT;
        
        // 应用真实医学模型：速度惩罚 = β * (负重/体重)
        // β = 0.20 表示40%体重负重时，速度下降20%（符合文献）
        float encumbranceSpeedPenaltyCoeff = StaminaConstants.GetEncumbranceSpeedPenaltyCoeff();
        float speedPenalty = encumbranceSpeedPenaltyCoeff * bodyMassPercent;
        
        // 限制惩罚值在合理范围内（0.0-0.5，最多减少50%速度）
        // 即使负重超过100%体重，速度惩罚也不超过50%
        speedPenalty = Math.Clamp(speedPenalty, 0.0, 0.5);
        
        return speedPenalty;
    }
    
    // 计算坡度对体力消耗的影响倍数（改进的多维度模型，包含负重交互）
    // 
    // 改进的数学模型：基于 Pandolf 负重行走代谢成本模型
    // 完整 Pandolf 模型公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
    // 其中：
    //   M = 总质量（体重 + 负重）
    //   V = 速度（m/s）
    //   G = 坡度（0 = 平地，正数 = 上坡，负数 = 下坡）
    // 
    // 改进模型：坡度影响 = 基础坡度影响 + 负重×坡度交互项
    // 基础坡度影响 = 1 + α·|G|·sign(G)
    // 负重×坡度交互 = interaction_coeff × (负重/体重) × |G|
    // 
    // 其中：
    //   α = SLOPE_UPHILL_COEFF（上坡）或 SLOPE_DOWNHILL_COEFF（下坡）
    //   sign(G) = +1（上坡）或 -1（下坡）
    //   interaction_coeff = ENCUMBRANCE_SLOPE_INTERACTION_COEFF
    //
    // 医学研究参考：
    // - 上坡：坡度每增加1度，能量消耗增加约5-10%
    // - 下坡：能量消耗减少，但减少幅度较小（约上坡的1/3）
    // - 负重越大，坡度的影响越明显（非线性交互）
    //
    // @param slopeAngleDegrees 坡度角度（度），正数=上坡，负数=下坡
    // @param bodyMassPercent 负重占体重的百分比（0.0-1.0+），用于计算交互项
    // @return 坡度影响倍数（0.7-2.5），表示相对于平地时的消耗倍数
    static float CalculateSlopeStaminaDrainMultiplier(float slopeAngleDegrees, float bodyMassPercent = 0.0)
    {
        // 限制坡度角度在合理范围内（-45度到+45度）
        slopeAngleDegrees = Math.Clamp(slopeAngleDegrees, -45.0, 45.0);
        
        // 基础坡度影响（不考虑负重）
        float baseSlopeMultiplier = 1.0;
        
        if (slopeAngleDegrees > 0.0)
        {
            // 上坡：消耗增加
            baseSlopeMultiplier = 1.0 + (SLOPE_UPHILL_COEFF * slopeAngleDegrees);
            // 限制最大倍数为2.0
            baseSlopeMultiplier = Math.Min(baseSlopeMultiplier, 2.0);
        }
        else if (slopeAngleDegrees < 0.0)
        {
            // 下坡：消耗减少
            baseSlopeMultiplier = 1.0 + (SLOPE_DOWNHILL_COEFF * slopeAngleDegrees);
            // 限制最小倍数为0.7
            baseSlopeMultiplier = Math.Max(baseSlopeMultiplier, 0.7);
        }
        
        // 负重×坡度交互项（负重越大，坡度影响越明显）
        // 交互项只在上坡时显著，下坡时影响较小
        float interactionMultiplier = 1.0;
        if (bodyMassPercent > 0.0 && slopeAngleDegrees > 0.0) // 只在上坡且有负重时应用交互项
        {
            // 交互项：负重×坡度
            // 例如：30kg负重（33%体重）+ 5°上坡 = 1 + 0.15 × 0.33 × 5 = 1.25倍额外影响
            float interactionTerm = ENCUMBRANCE_SLOPE_INTERACTION_COEFF * bodyMassPercent * slopeAngleDegrees;
            interactionMultiplier = 1.0 + interactionTerm;
            // 限制交互项倍数在合理范围内（1.0-1.5）
            interactionMultiplier = Math.Clamp(interactionMultiplier, 1.0, 1.5);
        }
        
        // 综合坡度影响 = 基础坡度影响 × 交互项
        float totalSlopeMultiplier = baseSlopeMultiplier * interactionMultiplier;
        
        // 最终限制：上坡最大2.5倍，下坡最小0.7倍
        if (slopeAngleDegrees > 0.0)
            return Math.Min(totalSlopeMultiplier, 2.5);
        else if (slopeAngleDegrees < 0.0)
            return Math.Max(totalSlopeMultiplier, 0.7);
        else
            return 1.0;
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
        // 数学实现：体力低于20%时，必须处于静止状态10秒后才允许开始回血
        float restDurationSeconds = restDurationMinutes * 60.0;
        if (staminaPercent < MIN_RECOVERY_STAMINA_THRESHOLD && restDurationSeconds < MIN_RECOVERY_REST_TIME_SECONDS)
        {
            return 0.0; // 体力低于20%且休息时间不足10秒时，不恢复
        }
        
        // ==================== 1. 基础恢复率（基于当前体力百分比，非线性）====================
        // 体力越低，恢复越快；体力越高，恢复越慢
        // 公式：recovery_rate = BASE_RECOVERY_RATE × (1.0 + RECOVERY_NONLINEAR_COEFF × (1.0 - stamina_percent))
        float staminaRecoveryMultiplier = 1.0 + (RECOVERY_NONLINEAR_COEFF * (1.0 - staminaPercent));
        float baseRecoveryRate = StaminaConstants.GetBaseRecoveryRate() * staminaRecoveryMultiplier;
        
        // ==================== 2. 健康状态/训练水平影响 ====================
        // 训练有素者（fitness=1.0）恢复速度增加25%
        float fitnessRecoveryMultiplier = 1.0 + (FITNESS_RECOVERY_COEFF * FITNESS_LEVEL);
        fitnessRecoveryMultiplier = Math.Clamp(fitnessRecoveryMultiplier, 1.0, 1.5); // 限制在100%-150%之间
        
        // ==================== 3. 休息时间影响（快速恢复期 vs 中等恢复期 vs 慢速恢复期）====================
        float restTimeMultiplier = 1.0;
        if (restDurationMinutes <= FAST_RECOVERY_DURATION_MINUTES)
        {
            // 快速恢复期（0-3分钟）：恢复速度增加200%（3倍）
            restTimeMultiplier = FAST_RECOVERY_MULTIPLIER;
        }
        else if (restDurationMinutes <= MEDIUM_RECOVERY_START_MINUTES + MEDIUM_RECOVERY_DURATION_MINUTES)
        {
            // 中等恢复期（3-10分钟）：恢复速度增加50%（1.5倍）
            restTimeMultiplier = MEDIUM_RECOVERY_MULTIPLIER;
        }
        else if (restDurationMinutes >= SLOW_RECOVERY_START_MINUTES)
        {
            // 慢速恢复期（≥10分钟）：恢复速度减少20%（0.8倍）
            // 线性过渡：从10分钟到20分钟，从1.0倍逐渐降至0.8倍
            const float transitionDuration = 10.0; // 过渡时间（分钟）
            float transitionProgress = Math.Min((restDurationMinutes - SLOW_RECOVERY_START_MINUTES) / transitionDuration, 1.0);
            restTimeMultiplier = 1.0 - (transitionProgress * (1.0 - SLOW_RECOVERY_MULTIPLIER));
        }
        // 否则：标准恢复期（10分钟内），restTimeMultiplier = 1.0
        
        // ==================== 4. 年龄影响 ====================
        // 年轻者恢复更快，年老者恢复较慢
        // 公式：age_recovery_multiplier = 1.0 + AGE_RECOVERY_COEFF × (AGE_REFERENCE - age) / AGE_REFERENCE
        float ageRecoveryMultiplier = 1.0 + (AGE_RECOVERY_COEFF * (AGE_REFERENCE - CHARACTER_AGE) / AGE_REFERENCE);
        ageRecoveryMultiplier = Math.Clamp(ageRecoveryMultiplier, 0.8, 1.2); // 限制在80%-120%之间
        
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
                stanceRecoveryMultiplier = StaminaConstants.GetStandingRecoveryMultiplier();
                break;
            case 1: // 蹲姿
                stanceRecoveryMultiplier = StaminaConstants.CROUCHING_RECOVERY_MULTIPLIER;
                break;
            case 2: // 趴姿
                stanceRecoveryMultiplier = StaminaConstants.GetProneRecoveryMultiplier();
                break;
            default:
                stanceRecoveryMultiplier = StaminaConstants.GetStandingRecoveryMultiplier();
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
            float loadRecoveryPenaltyCoeff = StaminaConstants.GetLoadRecoveryPenaltyCoeff();
            float loadRecoveryPenaltyExponent = StaminaConstants.GetLoadRecoveryPenaltyExponent();
            loadRecoveryPenalty = Math.Pow(loadRatio, loadRecoveryPenaltyExponent) * loadRecoveryPenaltyCoeff;
        }
        
        // ==================== 深度生理压制：边际效应衰减机制 ====================
        // 医学解释：身体从"半死不活"恢复到"喘匀气"很快（前80%），但要从"喘匀气"恢复到"肌肉巅峰竞技状态"非常慢（最后20%）
        // 数学实现：当体力>80%时，恢复率 = 原始恢复率 * (1.1 - 当前体力百分比)
        // 结果：最后10%的体力恢复速度会比前50%慢3-4倍
        // 战术意图：玩家经常会处于80%-90%的"亚健康"状态，只有长时间修整才能真正满血
        float marginalDecayMultiplier = 1.0;
        if (staminaPercent > MARGINAL_DECAY_THRESHOLD)
        {
            // 当体力>80%时，应用边际效应衰减
            // 恢复率 = (1.1 - 当前体力百分比)
            // 例如：体力90%时，恢复率 = 1.1 - 0.9 = 0.2（即20%的原始恢复速度）
            marginalDecayMultiplier = MARGINAL_DECAY_COEFF - staminaPercent;
            marginalDecayMultiplier = Math.Clamp(marginalDecayMultiplier, 0.2, 1.0); // 限制在20%-100%之间
        }
        
        // ==================== 综合恢复率计算（深度生理压制版本）====================
        // 核心概念：从"净增加"改为"代谢净值"
        // 最终恢复率 = (基础恢复率 × 姿态修正) - (负重压制 + 氧债惩罚)
        // 综合恢复率 = 基础恢复率 × 健康状态倍数 × 休息时间倍数 × 年龄倍数 × 疲劳恢复倍数 × 姿态恢复倍数 × 边际效应衰减 - 负重惩罚
        float totalRecoveryRate = baseRecoveryRate * fitnessRecoveryMultiplier * restTimeMultiplier * ageRecoveryMultiplier * fatigueRecoveryMultiplier * stanceRecoveryMultiplier * marginalDecayMultiplier;
        
        // 应用负重静态剥夺（从恢复率中减去负重惩罚）
        totalRecoveryRate = totalRecoveryRate - loadRecoveryPenalty;
        
        // 确保恢复率不为负（最低为0）
        totalRecoveryRate = Math.Max(totalRecoveryRate, 0.0);
        
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
    static float CalculateActionCost(float baseCost, float currentWeight)
    {
        // 限制输入参数
        baseCost = Math.Max(baseCost, 0.0);
        currentWeight = Math.Max(currentWeight, CHARACTER_WEIGHT); // 至少等于身体重量
        
        // 计算重量比例
        float weightRatio = currentWeight / CHARACTER_WEIGHT; // 例如：120kg / 90kg = 1.33
        
        // 使用 1.5 次幂，让超重状态下的跳跃变得极其昂贵
        // 使用 StaminaHelpers.Pow 函数计算 weightRatio^1.5
        float weightMultiplier = StaminaHelpers.Pow(weightRatio, 1.5);
        
        // 计算实际消耗
        float actualCost = baseCost * weightMultiplier;
        
        // 限制最大消耗（防止数值爆炸）
        return Math.Clamp(actualCost, 0.0, 0.15); // 最多15%体力
    }
    
    // 计算负重对体力消耗的影响倍数（基于体重的真实模型）
    // 
    // 真实数学模型：基于医学研究（Pandolf et al., 1977; Looney et al., 2018; Vine et al., 2022）
    // 体力消耗倍数基于体重百分比，而不是最大负重百分比
    // 体力消耗倍数 = 1 + γ * (负重/体重)
    // 其中：
    //   γ = ENCUMBRANCE_STAMINA_DRAIN_COEFF（基于体重的体力消耗系数）
    //   负重/体重 = 负重占体重的百分比（Body Mass Percentage, BM%）
    //
    // 医学研究参考（基于体重百分比）：
    // - 负重 0% BM 时：1.0×（基准）
    // - 负重 20-30% BM 时：1.3-1.5×
    // - 负重 33% BM 时：1.5×（30kg ≈ 33%体重，90kg）
    // - 负重 40-50% BM 时：1.5-1.8×
    // - 负重 60-70% BM 时：2.0-2.5×
    // - 负重 100% BM 时：2.5-3.0×
    //
    // 注意：即使负重导致速度下降，负重本身仍然会增加能量消耗
    // 这是因为负重增加了肌肉负荷，即使速度较低也需要更多能量来维持
    //
    // @param owner 角色实体
    // @return 体力消耗倍数（1.0-3.0），表示相对于无负重时的消耗倍数
    static float CalculateEncumbranceStaminaDrainMultiplier(IEntity owner)
    {
        // 获取当前负重（kg）
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return 1.0;
        
        SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
        if (!characterInventory)
            return 1.0;
        
        float currentWeight = characterInventory.GetTotalWeight();
        if (currentWeight < 0.0)
            return 1.0;
        
        // 计算有效负重（减去基准重量，基准重量是基本战斗装备）
        // 基准重量（12kg）是基本战斗装备的重量，不应该影响消耗
        float effectiveWeight = Math.Max(currentWeight - BASE_WEIGHT, 0.0);
        
        // 计算有效负重占体重的百分比（Body Mass Percentage）
        float bodyMassPercent = effectiveWeight / CHARACTER_WEIGHT;
        
        // 应用真实医学模型：体力消耗倍数 = 1 + γ * (有效负重/体重)
        // γ = 1.5 表示40%体重有效负重时，消耗增加60%（1 + 1.5 × 0.4 = 1.6倍），符合文献
        // 例如（假设体重90kg，基准重量12kg）：
        // - 基准负重（12kg，有效0kg）：1.0倍（正常消耗）
        // - 战斗负重（30kg，有效18kg，20%体重）：1.0 + 1.5 × 0.2 = 1.3倍（消耗增加30%）
        // - 最大负重（40.5kg，有效28.5kg，32%体重）：1.0 + 1.5 × 0.32 = 1.48倍（消耗增加48%）
        float encumbranceStaminaDrainCoeff = StaminaConstants.GetEncumbranceStaminaDrainCoeff();
        float staminaDrainMultiplier = 1.0 + (encumbranceStaminaDrainCoeff * bodyMassPercent);
        
        // 限制消耗倍数在合理范围内（1.0-3.0）
        staminaDrainMultiplier = Math.Clamp(staminaDrainMultiplier, 1.0, 3.0);
        
        return staminaDrainMultiplier;
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
            // 使用StaminaHelpers.Pow计算 weightRatio^1.2
            float weightRatioPower = StaminaHelpers.Pow(weightRatio, 1.2);
            loadFactor = 1.0 + (weightRatioPower * 1.5);
        }
        
        // 根据速度和动态阈值计算消耗率
        if (velocity >= SPRINT_VELOCITY_THRESHOLD || velocity >= 5.0)
        {
            // Sprint消耗 × 负重影响因子
            return SPRINT_DRAIN_PER_TICK * loadFactor;
        }
        else if (velocity >= RUN_VELOCITY_THRESHOLD || velocity >= 3.2)
        {
            // Run消耗（核心修正：大幅调低RUN的基础消耗，以补偿负重）
            // 基础消耗从0.105 pts/s降低到约0.08 pts/s（每0.2秒 = 0.00016）
            // 这是为了确保30kg负载下仍能坚持约16分钟
            return 0.00008 * loadFactor; // 每0.2秒，调整后的Run消耗
        }
        else if (velocity >= dynamicThreshold)
        {
            // Walk消耗（缓慢消耗）
            return 0.00002 * loadFactor; // 每0.2秒，调整后的Walk消耗
        }
        else
        {
            // 速度 < 动态阈值：恢复（保持较高的恢复率）
            return -0.00025; // Rest恢复（负数），每0.2秒
        }
    }
    
    // 计算坡度修正乘数（基于百分比坡度，改进的下坡逻辑）
    // 
    // 坡度修正：
    // - 上坡 (G > 0): K_grade = 1 + (G × 0.12)（使用幂函数）
    // - 下坡 (G < 0): 
    //   * 缓坡（-15% < G < 0）：减少消耗（每1%减少3%消耗）
    //   * 陡坡（G ≤ -15%）：增加消耗（离心收缩负荷）
    //
    // 生理学修正（下坡陡坡）：
    // 实际上，穿着30kg重负荷下陡坡对膝盖和肌肉的离心收缩负荷极大，
    // 能量消耗有时并不比平地低。当坡度超过-15度时，消耗率反而略微回升。
    //
    // @param gradePercent 坡度百分比（例如，5% = 5.0，-15% = -15.0）
    // @return 坡度修正乘数
    static float CalculateGradeMultiplier(float gradePercent)
    {
        float kGrade = 1.0;
        
        if (gradePercent > 0.0)
        {
            // 上坡：使用幂函数代替线性增长
            // 公式：kGrade = 1.0 + (gradePercent × 0.01)^1.2 × 5.0
            // 这样5%坡度时：1.0 + (0.05^1.2) × 5.0 ≈ 1.0 + 0.047 × 5.0 ≈ 1.235倍
            // 而15%坡度时：1.0 + (0.15^1.2) × 5.0 ≈ 1.0 + 0.173 × 5.0 ≈ 1.865倍
            // 让小坡几乎无感，陡坡才真正吃力
            float normalizedGrade = gradePercent * 0.01; // 转换为0.0-1.0范围（假设最大100%）
            float gradePower = StaminaHelpers.Pow(normalizedGrade, 1.2); // 使用1.2次方
            kGrade = 1.0 + (gradePower * 5.0);
            
            // 限制最大坡度修正（避免数值爆炸）
            kGrade = Math.Min(kGrade, 3.0); // 最多3倍消耗
        }
        else if (gradePercent < 0.0)
        {
            // 下坡：根据坡度绝对值决定消耗修正
            float absGradePercent = Math.AbsFloat(gradePercent);
            
            if (absGradePercent <= 15.0)
            {
                // 缓坡（0% 到 -15%）：减少消耗
                // 每1%减少3%消耗
                kGrade = 1.0 + (gradePercent * GRADE_DOWNHILL_COEFF);
                // 限制下坡修正，避免消耗变为负数
                kGrade = Math.Max(kGrade, 0.5); // 最多减少50%消耗
            }
            else
            {
                // 陡坡（超过-15%）：增加消耗（离心收缩负荷）
                // 生理学依据：重负荷下陡坡对膝盖和肌肉的离心收缩负荷极大
                // 公式：kGrade = 1.0 + (absGradePercent - 15.0) × 0.02
                // 例如：-20%坡度时 = 1.0 + (20.0 - 15.0) × 0.02 = 1.1倍（消耗增加10%）
                //      -30%坡度时 = 1.0 + (30.0 - 15.0) × 0.02 = 1.3倍（消耗增加30%）
                float steepGradePenalty = (absGradePercent - 15.0) * 0.02;
                kGrade = 1.0 + steepGradePenalty;
                // 限制陡坡修正，最多增加50%消耗（1.5倍）
                kGrade = Math.Min(kGrade, 1.5);
            }
        }
        
        return kGrade;
    }
    
    // 计算基于完整 Pandolf 模型的体力消耗率（包括坡度项、地形系数、Santee下坡修正）
    // 
    // 完整 Pandolf 模型公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²)) · η
    // 其中：
    //   E = 能量消耗率（W/kg）
    //   M = 总重量（身体重量 + 负重）
    //   V = 速度（m/s）
    //   G = 坡度（坡度百分比，例如 5% = 0.05，正数=上坡，负数=下坡）
    //   η = 地形系数（Terrain Factor，1.0-1.8）
    // 
    // 简化版本（平地，G=0，η=1.0）：E = M·(2.7 + 3.2·(V-0.7)²)
    // 
    // 对于游戏实现，我们需要：
    // 1. 将能量消耗率（W/kg）转换为体力消耗率（%/s）
    // 2. 考虑负重的相对影响（相对于身体重量）
    // 3. 将坡度项 G·(0.23 + 1.34·V²) 整合到计算中
    // 4. 应用地形系数 η（影响移动消耗）
    // 5. 应用 Santee 下坡修正（下坡超过-15%时的离心收缩）
    //
    // @param velocity 当前速度（m/s）
    // @param currentWeight 当前总重量（kg），包括身体重量和负重
    // @param gradePercent 坡度百分比（例如，5% = 5.0，-15% = -15.0），默认0.0（平地）
    // @param terrainFactor 地形系数（η，1.0-1.8），默认1.0（铺装路面）
    // @param useSanteeCorrection 是否使用 Santee 下坡修正，默认true
    // @return 体力消耗率（%/s，每0.2秒的消耗率需要乘以 0.2）
    static float CalculatePandolfEnergyExpenditure(float velocity, float currentWeight, float gradePercent = 0.0, float terrainFactor = 1.0, bool useSanteeCorrection = true)
    {
        // Pandolf 模型常量（使用类常量，避免变量名冲突）
        
        // 确保速度和重量有效
        velocity = Math.Max(velocity, 0.0);
        currentWeight = Math.Max(currentWeight, 0.0);
        
        // 如果速度为0或很小，返回恢复率（负数）
        if (velocity < 0.1)
            return -0.0025; // 恢复率（负数）
        
        // 计算基础项：2.7 + 3.2·(V-0.7)²
        // 优化：对于顶尖运动员，运动时的经济性（Running Economy）更高
        // 添加 fitness bonus 来降低基础代谢项
        float velocityTerm = velocity - PANDOLF_VELOCITY_OFFSET;
        float velocitySquaredTerm = velocityTerm * velocityTerm;
        float fitnessBonus = 1.0 - (0.2 * FITNESS_LEVEL); // 训练有素者基础代谢降低20%
        float baseTerm = (PANDOLF_BASE_COEFF * fitnessBonus) + (PANDOLF_VELOCITY_COEFF * velocitySquaredTerm);
        
        // 计算坡度项：G·(0.23 + 1.34·V²)
        // 注意：坡度百分比需要转换为小数（例如 5% = 0.05）
        float gradeDecimal = gradePercent * 0.01; // 转换为小数
        float velocitySquared = velocity * velocity;
        float gradeTerm = gradeDecimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocitySquared));
        
        // 应用 Santee 下坡修正（如果启用）
        // 当下坡超过 -15% 时，需要用力"刹车"，消耗增加
        if (useSanteeCorrection && gradePercent < 0.0)
        {
            float santeeCorrection = CalculateSanteeDownhillCorrection(gradePercent);
            // 修正系数 < 1.0 表示消耗增加（因为修正系数在分母位置）
            // 例如：修正系数 0.8 表示消耗 = 原始消耗 / 0.8 = 1.25倍
            if (santeeCorrection < 1.0 && santeeCorrection > 0.0)
            {
                gradeTerm = gradeTerm / santeeCorrection; // 下坡陡坡时，消耗增加
            }
        }
        
        // 应用地形系数：η
        // 地形系数直接影响移动消耗，铺装路面 η=1.0，草地 η=1.2，沙地 η=1.8
        terrainFactor = Math.Clamp(terrainFactor, 0.5, 3.0); // 限制在合理范围内
        
        // 完整的 Pandolf 能量消耗率：E = M·(基础项 + 坡度项) · η
        // 注意：M 是总重量（kg），但我们使用相对于基准体重的倍数
        // 使用标准体重（70kg）作为参考，计算相对重量倍数
        float weightMultiplier = currentWeight / REFERENCE_WEIGHT;
        weightMultiplier = Math.Clamp(weightMultiplier, 0.5, 2.0); // 限制在0.5-2.0倍之间
        
        float energyExpenditure = weightMultiplier * (baseTerm + gradeTerm) * terrainFactor;
        
        // 将能量消耗率（W/kg）转换为体力消耗率（%/s）
        // 优化：降低转换系数，让体力槽更耐用，达到ACFT标准（15:27完成2英里）
        // 从0.0001降低到0.000035，减少约65%的体力消耗速度
        float energyToStaminaCoeff = StaminaConstants.GetEnergyToStaminaCoeff();
        float staminaDrainRate = energyExpenditure * energyToStaminaCoeff;
        
        // 限制消耗率在合理范围内（避免数值爆炸）
        staminaDrainRate = Math.Clamp(staminaDrainRate, 0.0, 0.05); // 最多每秒消耗5%
        
        return staminaDrainRate;
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
    // @param baseTargetSpeed 基础目标速度（m/s），例如3.7 m/s
    // @param slopeAngleDegrees 坡度角度（度），正数=上坡，负数=下坡
    // @return 坡度自适应后的目标速度（m/s）
    static float CalculateSlopeAdjustedTargetSpeed(float baseTargetSpeed, float slopeAngleDegrees)
    {
        if (slopeAngleDegrees <= 0.0)
        {
            // 下坡或平地：不主动减速
            return baseTargetSpeed;
        }
        
        // 爬坡自适应：每增加1度坡度，目标速度自动下降2.5%
        // 这样10度坡时，速度会降到约 3.7 × 0.75 = 2.775 m/s，虽然慢了，但体力能多撑一倍时间
        // 适应因子：1.0 - (slopeAngle × 0.025)，最小不低于0.6（即最多降低40%速度）
        float adaptationFactor = Math.Max(0.6, 1.0 - (slopeAngleDegrees * 0.025));
        
        return baseTargetSpeed * adaptationFactor;
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
        return staminaPercent >= SPRINT_ENABLE_THRESHOLD;
    }
    
    // ==================== 地形系数系统（基于实际测试数据的插值映射）====================
    // 地形系数常量（基于 Pandolf 模型研究）
    static const float TERRAIN_FACTOR_PAVED = 1.0;        // 铺装路面
    static const float TERRAIN_FACTOR_DIRT = 1.1;         // 碎石路
    static const float TERRAIN_FACTOR_GRASS = 1.2;        // 高草丛
    static const float TERRAIN_FACTOR_BRUSH = 1.5;        // 重度灌木丛
    static const float TERRAIN_FACTOR_SAND = 1.8;         // 软沙地
    
    // 根据密度值获取地形系数（基于实际测试数据的插值映射）
    // 测试数据点：(0.65,1.0), (1.13,1.0), (1.2,1.2), (1.33,1.1), (1.55,1.3), 
    //            (1.6,1.4), (2.24,1.0), (2.3,1.0), (2.7,1.5), (2.94,1.8)
    // 见 docs/terrain_density_mapping.md 了解详细映射关系
    // 
    // @param density 地面材质密度值（0.5-3.0）
    // @return 地形系数 (η)（1.0-1.8）
    static float GetTerrainFactorFromDensity(float density)
    {
        if (density <= 0.0)
            return TERRAIN_FACTOR_PAVED; // 默认值
        
        // 特殊情况：高密度平整表面（沥青、混凝土）
        if (density >= 2.2 && density <= 2.4)
            return TERRAIN_FACTOR_PAVED; // 沥青(2.24)、混凝土(2.3) → 1.0
        
        // 特殊情况：低密度平整表面（木箱）
        if (density <= 0.7)
            return TERRAIN_FACTOR_PAVED; // 木箱(0.65) → 1.0
        
        // 区间 1: 0.7 < density <= 1.2
        // 插值：室内地板(1.13, 1.0) → 草地(1.2, 1.2)
        if (density <= 1.2)
        {
            if (density <= 1.13)
                return TERRAIN_FACTOR_PAVED; // 室内地板区间
            // 线性插值
            float t = (density - 1.13) / (1.2 - 1.13);
            return 1.0 + t * 0.2; // 1.0 → 1.2
        }
        
        // 区间 2: 1.2 < density <= 1.33
        // 插值：草地(1.2, 1.2) → 土质(1.33, 1.1)
        if (density <= 1.33)
        {
            float t = (density - 1.2) / (1.33 - 1.2);
            return 1.2 - t * 0.1; // 1.2 → 1.1
        }
        
        // 区间 3: 1.33 < density <= 1.6
        // 插值：土质(1.33, 1.1) → 床垫(1.55, 1.3) → 海岸鹅卵石(1.6, 1.4)
        if (density <= 1.55)
        {
            // 子区间：土质 → 床垫
            float t = (density - 1.33) / (1.55 - 1.33);
            return 1.1 + t * 0.2; // 1.1 → 1.3
        }
        else // density <= 1.6
        {
            // 子区间：床垫 → 海岸鹅卵石
            float t = (density - 1.55) / (1.6 - 1.55);
            return 1.3 + t * 0.1; // 1.3 → 1.4
        }
        
        // 区间 4: 1.6 < density < 2.2
        // 插值：海岸鹅卵石(1.6, 1.4) → 平滑过渡到沥青区间(2.2, 1.0)
        if (density < 2.2)
        {
            float t = (density - 1.6) / (2.2 - 1.6);
            return 1.4 - t * 0.4; // 1.4 → 1.0
        }
        
        // 区间 5: 2.4 < density <= 2.7
        // 插值：混凝土区间结束(2.4, 1.0) → 铁棚(2.7, 1.5)
        if (density <= 2.7)
        {
            float t = (density - 2.4) / (2.7 - 2.4);
            return 1.0 + t * 0.5; // 1.0 → 1.5
        }
        
        // 区间 6: 2.7 < density
        // 插值：铁棚(2.7, 1.5) → 陶片屋顶(2.94, 1.8)
        if (density <= 2.94)
        {
            float t = (density - 2.7) / (2.94 - 2.7);
            return 1.5 + t * 0.3; // 1.5 → 1.8
        }
        
        // 外推：超出已知范围，限制在合理范围内
        return Math.Clamp(1.8 + (density - 2.94) * 0.1, 1.8, 2.5);
    }
    
    // ==================== 静态负重站立消耗（Pandolf 静态项）====================
    // 基于 Pandolf 模型：当 V=0 时，背负重物原地站立也会消耗体力
    // Pandolf 静态项公式：E_standing = 1.5·W_body + 2.0·(W_body + L)·(L/W_body)²
    // 其中：
    //   W_body = 身体重量（kg）
    //   L = 负重（kg）
    // 
    // @param bodyWeight 身体重量（kg），默认90kg
    // @param loadWeight 负重（kg），当前携带的物品重量
    // @return 静态站立消耗率（%/s），负数表示恢复，正数表示消耗
    static float CalculateStaticStandingCost(float bodyWeight = 90.0, float loadWeight = 0.0)
    {
        // Pandolf 静态项常量（使用类常量，避免变量名冲突）
        
        // 计算静态项：1.5·W_body + 2.0·(W_body + L)·(L/W_body)²
        float baseStaticTerm = PANDOLF_STATIC_COEFF_1 * bodyWeight;
        
        float loadRatio = 0.0;
        if (bodyWeight > 0.0)
            loadRatio = loadWeight / bodyWeight;
        
        float loadStaticTerm = 0.0;
        if (loadWeight > 0.0)
        {
            float loadRatioSquared = loadRatio * loadRatio;
            loadStaticTerm = PANDOLF_STATIC_COEFF_2 * (bodyWeight + loadWeight) * loadRatioSquared;
        }
        
        float staticEnergyExpenditure = baseStaticTerm + loadStaticTerm;
        
        // 将能量消耗率（W）转换为体力消耗率（%/s）
        // 参考：CalculatePandolfEnergyExpenditure 的转换系数（使用类常量）
        float energyToStaminaCoeff = StaminaConstants.GetEnergyToStaminaCoeff();
        float staticDrainRate = staticEnergyExpenditure * energyToStaminaCoeff;
        
        // 限制消耗率在合理范围内
        // 空载时约0.0135%/s，40kg负重时约0.04%/s
        staticDrainRate = Math.Clamp(staticDrainRate, 0.0, 0.05); // 最多每秒消耗5%
        
        return staticDrainRate;
    }
    
    // ==================== Santee 下坡修正模型（2001）====================
    // 基于 Santee et al. (2001) 的下坡修正模型
    // Pandolf 原型在处理下坡（负坡度）时不够精确
    // Santee 修正：当 G < -15% 时，需要用力"刹车"以防摔倒（离心收缩）
    // 修正系数：μ = 1.0 - [G·(1 - G/15)/2]
    // 其中 G 为负坡度百分比（例如 -15% = -15.0）
    // 
    // @param gradePercent 坡度百分比（例如，-15% = -15.0，正数=上坡，负数=下坡）
    // @return 下坡修正系数（0.7-1.0），用于修正 Pandolf 模型的坡度项
    static float CalculateSanteeDownhillCorrection(float gradePercent)
    {
        // 只处理下坡（负坡度）
        if (gradePercent >= 0.0)
            return 1.0; // 上坡或平地，不需要修正
        
        float absGrade = Math.AbsFloat(gradePercent);
        
        // 缓坡（0% 到 -15%）：使用标准 Pandolf 模型，不需要修正
        if (absGrade <= 15.0)
            return 1.0;
        
        // 陡坡（超过 -15%）：应用 Santee 修正
        // 修正系数：μ = 1.0 - [G·(1 - G/15)/2]
        // 其中 G 为负数，所以需要处理符号
        float correctionTerm = absGrade * (1.0 - absGrade / 15.0) / 2.0;
        float correctionFactor = 1.0 - correctionTerm;
        
        // 限制修正系数在合理范围内（0.5-1.0）
        // 修正系数越小，消耗越大（因为需要更多"刹车"力）
        return Math.Clamp(correctionFactor, 0.5, 1.0);
    }
    
    // ==================== Givoni-Goldman 跑步模式切换 ====================
    // 基于 Givoni-Goldman 跑步能量消耗模型
    // Pandolf 模型在速度 V > 2.2 m/s（约 8 km/h）时会失效（设计时针对步行）
    // 对于 Sprint/Run 模式，应该切换到 Givoni-Goldman 跑步模型
    // 
    // 跑步公式：E_run = (W_body + L)·(Givoni_Constant)·V^α
    // 其中：
    //   α = 速度指数（通常 2.0-2.4，跑步效率低于步行）
    //   Givoni_Constant = 跑步常数（需要校准）
    // 
    // @param velocity 当前速度（m/s）
    // @param currentWeight 当前总重量（kg），包括身体重量和负重
    // @param isRunning 是否为跑步模式（true=Run/Sprint，false=Walk）
    // @return 跑步模式下的能量消耗率（%/s）
    static float CalculateGivoniGoldmanRunning(float velocity, float currentWeight, bool isRunning = true)
    {
        // 只在跑步模式下（V > 2.2 m/s）使用 Givoni-Goldman 模型
        if (!isRunning || velocity <= 2.2)
            return 0.0; // 使用标准 Pandolf 模型
        
        // Givoni-Goldman 模型常量（使用类常量，避免变量名冲突）
        
        // 计算速度的幂：V^α
        // 使用 StaminaHelpers.Pow 函数计算 V^2.2
        float velocityPower = StaminaHelpers.Pow(velocity, GIVONI_VELOCITY_EXPONENT);
        
        // Givoni-Goldman 公式：E_run = (W_body + L)·Constant·V^α
        // 使用相对于基准体重的倍数
        float weightMultiplier = currentWeight / REFERENCE_WEIGHT;
        weightMultiplier = Math.Clamp(weightMultiplier, 0.5, 2.0); // 限制在0.5-2.0倍之间
        
        float runningEnergyExpenditure = weightMultiplier * GIVONI_CONSTANT * velocityPower;
        
        // 将能量消耗率（W/kg）转换为体力消耗率（%/s）（从配置管理器获取）
        float energyToStaminaCoeff = StaminaConstants.GetEnergyToStaminaCoeff();
        float runningDrainRate = runningEnergyExpenditure * energyToStaminaCoeff;
        
        // 限制消耗率在合理范围内
        runningDrainRate = Math.Clamp(runningDrainRate, 0.0, 0.05); // 最多每秒消耗5%
        
        return runningDrainRate;
    }
    
    // ==================== 游泳体力消耗计算（物理阻力模型 - 优化版）====================
    // 基于物理阻力模型：F_d = 0.5 * ρ * v² * C_d * A
    // 生理学依据：游泳的能量消耗远高于陆地移动（约3-5倍）
    // 参考：Holmér, I. (1974). Energy cost of arm stroke, leg kick, and whole crawl stroke.
    //       Pendergast, D. R., et al. (1977). Energy cost of swimming.
    //
    // 优化内容：
    // 1. 降低基础功率：从50W降至25W（更高效的生存踩水）
    // 2. 提高负重门槛：从20kg提高到25kg（20kg是标准战斗装具）
    // 3. 使用幂函数惩罚曲线：让轻度负重时更轻松，极端负重时才迅速崩溃
    // 4. 低强度踩水折扣：对于极低强度的踩水，引入"有氧代谢效率"折扣
    //
    // @param velocity 当前游泳速度（m/s）
    // @param currentWeight 当前总重量（kg），包括身体重量和负重
    // @return 游泳体力消耗率（%/s，每0.2秒的消耗率需要乘以 0.2）
    static float CalculateSwimmingStaminaDrain(float velocity, float currentWeight)
    {
        // 确保速度和重量有效
        velocity = Math.Max(velocity, 0.0);
        currentWeight = Math.Max(currentWeight, 0.0);
        
        float bodyWeight = StaminaConstants.CHARACTER_WEIGHT; // 90kg
        float effectiveWeight = Math.Max(currentWeight - bodyWeight, 0.0); // 有效负重（去除身体重量）
        
        // ==================== 1. 静态消耗优化（踩水维持）====================
        // 将基础功率降低，模拟更高效的生存踩水
        float baseMaintenancePower = StaminaConstants.SWIMMING_BASE_POWER; // 25W（从50W降至25W）
        
        float staticMultiplier = 1.0;
        // 使用 25kg 作为硬核负重的起点
        float threshold = StaminaConstants.SWIMMING_ENCUMBRANCE_THRESHOLD; // 25kg
        
        if (effectiveWeight > threshold)
        {
            // 只有超过 threshold 之后才开始指数级增加
            // 模拟：负重越大，为了维持头部在水面上所需的功率呈非线性上升
            float loadFactor = (effectiveWeight - threshold) / (StaminaConstants.SWIMMING_FULL_PENALTY_WEIGHT - threshold);
            loadFactor = Math.Clamp(loadFactor, 0.0, 1.0);
            
            // 使用平方律：让 25-30kg 之间依然相对温和，35kg之后才真正痛苦
            float powerExponent = 2.0; // 平方律
            staticMultiplier = 1.0 + (Math.Pow(loadFactor, powerExponent) * (StaminaConstants.SWIMMING_STATIC_DRAIN_MULTIPLIER - 1.0));
        }
        
        float staticPower = baseMaintenancePower * staticMultiplier;
        
        // ==================== 2. 动态消耗（v³ 阻尼）====================
        // 修正：物理公式计算的是机械功，人体转化效率极低（约 3-5%）
        // 实际上游泳 1.5m/s 的代谢消耗大约在 800W-1000W 左右
        float dynamicPower = 0.0;
        if (velocity > StaminaConstants.SWIMMING_MIN_SPEED)
        {
            float velocityCubed = velocity * velocity * velocity; // v³
            dynamicPower = 0.5 * StaminaConstants.SWIMMING_WATER_DENSITY * velocityCubed * 
                          StaminaConstants.SWIMMING_DRAG_COEFFICIENT * StaminaConstants.SWIMMING_FRONTAL_AREA;
            
            // 增加效率因子：人体游泳效率大约只有 0.05，所以消耗要除以 0.05 (即乘以 20)
            // 但为了游戏性平衡，我们取一个折中值
            dynamicPower = dynamicPower * StaminaConstants.SWIMMING_DYNAMIC_POWER_EFFICIENCY;
            
            // 动态阻力随负重的修正（士兵体积变大）
            float dragWeightBonus = 1.0 + (effectiveWeight / bodyWeight * 0.3);
            dragWeightBonus = Math.Clamp(dragWeightBonus, 1.0, 1.3);
            dynamicPower = dynamicPower * dragWeightBonus;
        }
        
        // ==================== 3. 转换与保护 ====================
        // 增加“生存压力”常数：避免水中静止被恢复逻辑覆盖
        float totalPower = staticPower + dynamicPower + StaminaConstants.SWIMMING_SURVIVAL_STRESS_POWER;
        
        // 生理功率上限：防止异常速度/模组冲突导致 v³ 产生天文数字并在后续乘法中溢出
        totalPower = Math.Clamp(totalPower, 0.0, StaminaConstants.SWIMMING_MAX_TOTAL_POWER);
        
        // ==================== 转换为体力消耗率 ====================
        // 直接使用小数系数，不要乘以 100（这是关键修复）
        // ENERGY_TO_STAMINA_COEFF 本身就是为了将功率转换为 0.0-1.0 这种小数比例而设计的
        float swimmingDrainRate = totalPower * StaminaConstants.SWIMMING_ENERGY_TO_STAMINA_COEFF;
        
        // ==================== 4. 低强度折扣（survival floating）====================
        // 关键修正：对于极低强度的踩水（静态功率），引入"有氧代谢效率"
        // 模拟士兵在水中通过深呼吸调整，不完全依赖消耗体力槽
        if (velocity < StaminaConstants.SWIMMING_LOW_INTENSITY_VELOCITY && effectiveWeight < threshold)
        {
            swimmingDrainRate = swimmingDrainRate * StaminaConstants.SWIMMING_LOW_INTENSITY_DISCOUNT; // 浮水且负重轻时，额外给予30%的消耗折扣
        }
        
        // 限制每秒最大消耗：12% 每秒是非常恐怖的（8秒淹死），适合极限冲刺
        swimmingDrainRate = Math.Clamp(swimmingDrainRate, 0.0, 0.12);
        
        return swimmingDrainRate;
    }

    // ==================== 游泳体力消耗计算（3D 模型：水平 + 垂直）====================
    // 目标：考虑方向/垂直运动（上浮/下潜）对体力的额外消耗
    //
    // P_total = P_static + P_horizontal + P_vertical (+ survival stress)
    // - P_static：维持呼吸/浮水的基础功率，随负重非线性增长
    // - P_horizontal：水平阻力功率 ~ v_xz^3
    // - P_vertical：垂直方向（上浮/下潜）更昂贵
    //
    // @param velocityVector 速度向量（m/s），包含垂直分量
    // @param currentWeight 当前总重量（kg）
    // @return 游泳体力消耗率（0.0-1.0 / s）
    static float CalculateSwimmingStaminaDrain3D(vector velocityVector, float currentWeight)
    {
        currentWeight = Math.Max(currentWeight, 0.0);
        
        // 分解速度
        float vH = Math.Sqrt((velocityVector[0] * velocityVector[0]) + (velocityVector[2] * velocityVector[2])); // 水平速度（XZ）
        float vY = velocityVector[1]; // 垂直速度（Y）
        vH = Math.Max(vH, 0.0);
        
        float bodyWeight = StaminaConstants.CHARACTER_WEIGHT; // 90kg
        float effectiveWeight = Math.Max(currentWeight - bodyWeight, 0.0); // 有效负重
        
        // ==================== A. 静态/生存功率 ====================
        float baseMaintenancePower = StaminaConstants.SWIMMING_BASE_POWER; // 25W
        float threshold = StaminaConstants.SWIMMING_ENCUMBRANCE_THRESHOLD; // 25kg
        
        float staticMultiplier = 1.0;
        if (effectiveWeight > threshold)
        {
            float loadFactor = (effectiveWeight - threshold) / (StaminaConstants.SWIMMING_FULL_PENALTY_WEIGHT - threshold);
            loadFactor = Math.Clamp(loadFactor, 0.0, 1.0);
            staticMultiplier = 1.0 + (Math.Pow(loadFactor, 2.0) * (StaminaConstants.SWIMMING_STATIC_DRAIN_MULTIPLIER - 1.0));
        }
        float staticPower = baseMaintenancePower * staticMultiplier;
        
        // ==================== B. 水平阻力功率（v^3）====================
        float horizontalPower = 0.0;
        if (vH > StaminaConstants.SWIMMING_MIN_SPEED)
        {
            float vHCubed = vH * vH * vH;
            horizontalPower = 0.5 * StaminaConstants.SWIMMING_WATER_DENSITY * vHCubed *
                              StaminaConstants.SWIMMING_DRAG_COEFFICIENT * StaminaConstants.SWIMMING_FRONTAL_AREA;
            
            // 人体效率/游戏平衡折中
            horizontalPower = horizontalPower * StaminaConstants.SWIMMING_DYNAMIC_POWER_EFFICIENCY;
            
            // 负重增加阻力（迎水面积/阻力系数增加）
            float dragWeightBonus = 1.0 + ((effectiveWeight / bodyWeight) * 0.4);
            dragWeightBonus = Math.Clamp(dragWeightBonus, 1.0, 1.4);
            horizontalPower = horizontalPower * dragWeightBonus;
        }
        
        // ==================== C. 垂直功率（上浮/下潜）====================
        float verticalPower = 0.0;
        float absVY = Math.AbsFloat(vY);
        if (absVY > StaminaConstants.SWIMMING_VERTICAL_SPEED_THRESHOLD)
        {
            // 垂直阻力功率：P = 0.5 * rho * |v|^3 * Cd * A
            float vYCubedAbs = absVY * absVY * absVY;
            float verticalDragPower = 0.5 * StaminaConstants.SWIMMING_WATER_DENSITY * vYCubedAbs *
                                      StaminaConstants.SWIMMING_VERTICAL_DRAG_COEFFICIENT * StaminaConstants.SWIMMING_VERTICAL_FRONTAL_AREA;
            
            if (vY > 0.0)
            {
                // 上浮：负重越大越困难（负浮力/装备重力项更明显）
                // - baseline：不背负也需要一定功率做“上浮动作/姿态调整”
                // - loadTerm：随负重增加而非线性上升（这里用线性+系数，强度交给 multiplier 调整）
                float baseUpForce = bodyWeight * 9.81 * StaminaConstants.SWIMMING_VERTICAL_UP_BASE_BODY_FORCE_COEFF;
                float loadUpForce = effectiveWeight * 9.81 * StaminaConstants.SWIMMING_EFFECTIVE_GRAVITY_COEFF;
                float upPower = (baseUpForce + loadUpForce) * vY;
                verticalPower = (upPower + verticalDragPower) * StaminaConstants.SWIMMING_VERTICAL_UP_MULTIPLIER;
            }
            else
            {
                // 下潜：对抗浮力 + 垂直阻力
                // 负重会“抵消”一部分浮力需求：负重越大，下潜越容易
                float buoyancyForce = bodyWeight * 9.81 * StaminaConstants.SWIMMING_BUOYANCY_FORCE_COEFF;
                float loadReliefForce = effectiveWeight * 9.81 * StaminaConstants.SWIMMING_VERTICAL_DOWN_LOAD_RELIEF_COEFF;
                loadReliefForce = Math.Clamp(loadReliefForce, 0.0, buoyancyForce);
                
                float netBuoyancyForce = buoyancyForce - loadReliefForce;
                float downPower = netBuoyancyForce * absVY;
                verticalPower = (downPower + verticalDragPower) * StaminaConstants.SWIMMING_VERTICAL_DOWN_MULTIPLIER;
            }
        }
        
        // ==================== D. 总功率 + 保护 ====================
        float totalPower = staticPower + horizontalPower + verticalPower + StaminaConstants.SWIMMING_SURVIVAL_STRESS_POWER;
        totalPower = Math.Clamp(totalPower, 0.0, StaminaConstants.SWIMMING_MAX_TOTAL_POWER);
        
        // 转换为体力消耗率（0.0-1.0 / s）
        float drainRate = totalPower * StaminaConstants.SWIMMING_ENERGY_TO_STAMINA_COEFF;
        
        // 低强度折扣：几乎不动且负重轻时
        if (vH < StaminaConstants.SWIMMING_LOW_INTENSITY_VELOCITY && absVY < 0.1 && effectiveWeight < threshold)
            drainRate = drainRate * StaminaConstants.SWIMMING_LOW_INTENSITY_DISCOUNT;
        
        return Math.Clamp(drainRate, 0.0, StaminaConstants.SWIMMING_MAX_DRAIN_RATE);
    }
}
