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
    //    只要体力高于25%，士兵可以强行维持设定的目标速度（3.8 m/s）。
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
        
        float smoothTransitionStart = StaminaConfigBridge.GetSmoothTransitionStart();
        
        if (staminaPercent >= smoothTransitionStart)
        {
            // 意志力平台期（smoothTransitionStart%-100%）：保持恒定目标速度（3.8 m/s）
            // 模拟士兵通过意志力克服早期疲劳，维持恒定性能
            baseSpeedMultiplier = TARGET_RUN_SPEED_MULTIPLIER;
        }
        else if (staminaPercent >= SMOOTH_TRANSITION_END)
        {
            // 平滑过渡期（5%-smoothTransitionStart%）：使用SmoothStep建立缓冲区，避免突兀的"撞墙"效果
            // 让开始下降时更柔和，接近力竭时下降更快
            // t = (stamina - SMOOTH_TRANSITION_END) / (smoothTransitionStart - SMOOTH_TRANSITION_END)
            // smoothT = t² × (3 - 2t)，这是一个平滑的S型曲线
            float t = (staminaPercent - SMOOTH_TRANSITION_END) / (smoothTransitionStart - SMOOTH_TRANSITION_END); // 0.0-1.0
            t = Math.Clamp(t, 0.0, 1.0);
            float smoothT = t * t * (3.0 - 2.0 * t); // smoothstep函数
            
            // 在目标速度和跛行速度之间平滑过渡
            // 当体力从25%降到5%时，速度从3.8 m/s平滑降到跛行速度
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
    
    // CalculateEncumbranceSpeedPenalty removed — replaced by SCR_EncumbranceCache.UpdateCache()'s
    // three-segment non-linear model. See SCR_EncumbranceCache.c for the active implementation.
    
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
    // 恢复逻辑已提取到 SCR_RecoveryModel.c (RecoveryModel.CalculateRecoveryRate)
    
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
        const float g = StaminaConstants.JUMP_GRAVITY;
        float epot = m * g * h;
        float ekin = 0.5 * m * v * v;
        float joules = (epot + ekin) / Math.Max(eta, 0.01);
        // 1体力 ≈ 75kcal ≈ JUMP_STAMINA_TO_JOULES J
        return Math.Clamp(joules / StaminaConstants.JUMP_STAMINA_TO_JOULES, 0.0, StaminaConstants.JUMP_VAULT_MAX_DRAIN_CLAMP);
    }

    // 物理模型估算攀爬能量（每秒体力百分比）
    // m: 体重; v_vert: 垂直速度; F_iso: 四肢等长力; eta_iso: 等长效率
    static float ComputeClimbCostPhys(float m, float v_vert, float F_iso, float eta_iso)
    {
        const float g = StaminaConstants.JUMP_GRAVITY;
        float p_vert = m * g * v_vert;
        float p_iso = eta_iso * F_iso;
        float p_total = p_vert + p_iso + StaminaConstants.VAULT_BASE_METABOLISM_WATTS;
        float joules = p_total * 0.2; // step = UPDATE_INTERVAL = 0.2s
        return Math.Clamp(joules / StaminaConstants.JUMP_STAMINA_TO_JOULES, 0.0, StaminaConstants.JUMP_VAULT_MAX_DRAIN_CLAMP);
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
        
        // 计算有效负重（负载 = 装备重量 - 基准装备重量）
        // 修复：与 SCR_EncumbranceCache 保持一致，30kg 负载时 bodyMassPercent ≈ 0.318
        float effectiveWeight = Math.Max(currentWeight - BASE_WEIGHT, 0.0);
        
        // 计算有效负重占体重的百分比（Body Mass Percentage）
        float bodyMassPercent = effectiveWeight / CHARACTER_WEIGHT;
        
        // 应用真实医学模型：体力消耗倍数 = 1 + γ * (有效负重/体重)
        // γ = 1.5 表示40%体重有效负重时，消耗增加60%（1 + 1.5 × 0.4 = 1.6倍），符合文献
        // 例如（假设体重90kg，基准重量12kg）：
        // - 基准负重（12kg，有效0kg）：1.0倍（正常消耗）
        // - 战斗负重（30kg，有效18kg，20%体重）：1.0 + 1.5 × 0.2 = 1.3倍（消耗增加30%）
        // - 最大负重（40.5kg，有效28.5kg，32%体重）：1.0 + 1.5 × 0.32 = 1.48倍（消耗增加48%）
        float encumbranceStaminaDrainCoeff = StaminaConfigBridge.GetEncumbranceStaminaDrainCoeff();
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
        if (velocity < dynamicThreshold)
        {
            // 速度 < 动态阈值：恢复
            return -0.05; // Rest恢复（负数），每0.2秒，与恢复模型调参一致
        }
        
        // 所有移动阶段使用 Pandolf 公式计算消耗，再乘以负重因子。
        // 由于本函数并不直接感知坡度，将其设为0（平地）。
        const float gradePercent = 0.0;
        float pandolf = CalculatePandolfDrain(velocity, currentWeight, gradePercent);
        return pandolf * loadFactor;
    }
    
    // 计算坡度修正乘数（基于百分比坡度，改进的下坡逻辑）

    // ==================== Pandolf 消耗模型 ====================
    // 使用完整的 Pandolf 能量消耗公式来计算速度阶段上的体力掉点。
    // 本函数返回每0.2秒的体力消耗率（正值）。
    // @param velocity 当前水平速度 (m/s)
    // @param currentWeight 载具/装备总重 (kg)
    // @param gradePercent 坡度百分比（例如 5% = 5.0）
    // @return 耗率（正值，负表示恢复）
    static float CalculatePandolfDrain(float velocity, float currentWeight, float gradePercent)
    {
        // Pandolf公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
        // 其中 M = (body+load)/referenceWeight
        float bodyMass = CHARACTER_WEIGHT;
        float totalMass = bodyMass + currentWeight;
        float M = totalMass / REFERENCE_WEIGHT;
        float V = velocity;
        float G = gradePercent * 0.01; // 转换为小数
        
        float vOffset = PANDOLF_VELOCITY_OFFSET;
        // Align with CalculatePandolfEnergyExpenditure: apply fitness bonus (0.80)
        float fitnessBonus = StaminaConstants.FIXED_PANDOLF_FITNESS_BONUS;
        float baseCoeff = PANDOLF_BASE_COEFF * fitnessBonus;
        float velCoeff = PANDOLF_VELOCITY_COEFF;
        float gradeBase = PANDOLF_GRADE_BASE_COEFF;
        float gradeVel = PANDOLF_GRADE_VELOCITY_COEFF;
        
        float E = M * (baseCoeff + velCoeff * (V - vOffset) * (V - vOffset) + G * (gradeBase + gradeVel * V * V));
        // 转换为每0.2s体力点
        // 转换为每秒体力消耗率（从配置管理器获取最新系数）
        float energyToStamina = StaminaConfigBridge.GetEnergyToStaminaCoeff();
        float drainPerSec = E * energyToStamina;
        // 每次 UpdateSpeedBasedOnStamina 调用间隔 0.2 秒
        return drainPerSec * 0.2;
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
        
        // 速度接近 0 时不应返回“恢复”（负数）。移动阶段的低速通常来自限速/起步，
        // 生理上仍存在静态支撑与姿态维持成本；此处用静态负重站立消耗回退。
        if (velocity < 0.1)
        {
            float bodyWeight = CHARACTER_WEIGHT; // 90kg
            float loadWeight = Math.Max(currentWeight - bodyWeight, 0.0);
            float staticDrain = CalculateStaticStandingCost(bodyWeight, loadWeight);
            return Math.Max(staticDrain, 0.0);
        }
        
        // 计算基础项：2.7 + 3.2·(V-0.7)²
        // 优化：对于顶尖运动员，运动时的经济性（Running Economy）更高
        // 添加 fitness bonus 来降低基础代谢项
        float velocityTerm = velocity - PANDOLF_VELOCITY_OFFSET;
        float velocitySquaredTerm = velocityTerm * velocityTerm;
        // 固定值（FITNESS_LEVEL=1.0）：1.0 - 0.2 × 1.0 = 0.80，防止不平等游玩
        float fitnessBonus = StaminaConstants.FIXED_PANDOLF_FITNESS_BONUS;
        float baseTerm = (PANDOLF_BASE_COEFF * fitnessBonus) + (PANDOLF_VELOCITY_COEFF * velocitySquaredTerm);
        
        // 计算坡度项：G·(0.23 + 1.34·V²)（Pandolf 原始项）
        // 注意：坡度百分比需要转换为小数（例如 5% = 0.05）
        float gradeDecimal = gradePercent * 0.01; // 转换为小数
        float velocitySquared = velocity * velocity;
        float gradeTerm = gradeDecimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocitySquared));
        
        // 坡度保护：限制坡度项的最大贡献，防止极端坡度导致消耗爆炸
        // 坡度项不应超过基础项的3倍（即最多增加300%消耗）
        float maxGradeTerm = baseTerm * 3.0;
        gradeTerm = Math.Min(gradeTerm, maxGradeTerm);
        
        // 生理学修正：缓下坡更省能、陡下坡刹车耗能（Margaria / Santee 等）
        // 1) 缓下坡（约 0～-12%）：能耗最低区，放大 Pandolf 负坡度项的“省能”效果
        if (gradePercent < 0.0 && gradePercent > -StaminaConstants.GENTLE_DOWNHILL_GRADE_MAX)
        {
            gradeTerm = gradeTerm * StaminaConstants.GENTLE_DOWNHILL_SAVINGS_MULTIPLIER;
        }
        // 2) 陡下坡（< -15%）：离心收缩/刹车导致能耗回升，叠加正向惩罚项
        float steepDownhillPenalty = 0.0;
        if (useSanteeCorrection && gradePercent < -StaminaConstants.STEEP_DOWNHILL_GRADE_THRESHOLD)
        {
            float absGrade = Math.AbsFloat(gradePercent);
            float ramp = (absGrade - StaminaConstants.STEEP_DOWNHILL_GRADE_THRESHOLD) / 15.0;
            if (ramp > 1.0)
                ramp = 1.0;
            steepDownhillPenalty = baseTerm * ramp * StaminaConstants.STEEP_DOWNHILL_PENALTY_MAX_FRACTION;
        }
        
        // 应用地形系数：η
        // 地形系数直接影响移动消耗，铺装路面 η=1.0，草地 η=1.2，沙地 η=1.8
        terrainFactor = Math.Clamp(terrainFactor, 0.5, 3.0); // 限制在合理范围内
        
        // 完整的 Pandolf 能量消耗率：E = M·(基础项 + 坡度项 + 陡下坡惩罚) · η
        // 注意：M 是总重量（kg），但我们使用相对于基准体重的倍数
        // 使用标准体重（70kg）作为参考，计算相对重量倍数
        float weightMultiplier = currentWeight / REFERENCE_WEIGHT;
        // [修复] 与Python数字孪生保持一致，将下限从0.5改为0.1
        // 只防止负数，不限制上限（完全尊重玩家选择）
        weightMultiplier = Math.Max(weightMultiplier, 0.1); // 与Python一致

        // [修复] 根据原始 Pandolf 公式，必须乘以 REFERENCE_WEIGHT 才能得到总瓦特数（Watts）
        // 原始公式：E = M · (基础项 + 坡度项 + 陡下坡惩罚) · η；陡下坡惩罚仅在下坡 < -15% 时非零
        float energyExpenditure = weightMultiplier * (baseTerm + gradeTerm + steepDownhillPenalty) * terrainFactor * REFERENCE_WEIGHT;
        
        // debug: log intermediates when debug enabled
        if (StaminaConfigBridge.IsDebugEnabled())
        {
        }
        
        // 将能量消耗率（W/kg）转换为体力消耗率（%/s）
        // 优化：降低转换系数，让体力槽更耐用，达到ACFT标准（15:27完成2英里）
        // 从0.0001降低到0.000015，减少约85%的体力消耗速度
        float energyToStaminaCoeff = StaminaConfigBridge.GetEnergyToStaminaCoeff();
        // clamp coefficient to sane range (avoid config typo)
        energyToStaminaCoeff = Math.Clamp(energyToStaminaCoeff, 0.0, 0.1);
        float staminaDrainRate = energyExpenditure * energyToStaminaCoeff;
        
        // debug: log coefficient and drain
        
        // [修复] 完全移除 clip 上限，让 Pandolf 模型自然输出
        // 只防止负数，不限制上限
        staminaDrainRate = Math.Max(staminaDrainRate, 0.0);

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
            toblerMultiplier = toblerMultiplier * StaminaConstants.UPHILL_SPEED_BOOST;
        }
        else if (slopeAngleDegrees < 0.0)
        {
            toblerMultiplier = toblerMultiplier * StaminaConstants.DOWNHILL_SPEED_BOOST;
            toblerMultiplier = Math.Min(toblerMultiplier, StaminaConstants.DOWNHILL_SPEED_MAX_MULTIPLIER);
        }

        // 坡度速度倍率缩减 30%（设计意图）：
        // Tobler 函数在缓坡（3°~8°）的输出约 0.85~0.95，乘 UPHILL_SPEED_BOOST(1.15)
        // 后约 0.98~1.09。直接应用会导致玩家在肉眼不可察的微坡上也被减速。
        // 缩减 30% 后：缓坡≈0.99~1.06（几乎无感），15°陡坡≈0.73（仍明显降速）。
        // 这保持了"陡坡有体感、缓坡不折腾"的平衡。
        toblerMultiplier = 1.0 + 0.7 * (toblerMultiplier - 1.0);
        toblerMultiplier = Math.Clamp(toblerMultiplier, 0.15, StaminaConstants.DOWNHILL_SPEED_MAX_MULTIPLIER);

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
        return staminaPercent >= StaminaConfigBridge.GetSprintEnableThreshold();
    }
    
    // ==================== 地形系数系统（基于实际测试数据的插值映射）====================
    // 地形系数常量（引用 StaminaConstants 以消除重复）
    // 之前此处有独立的 TERRAIN_FACTOR_* 局部常量，与 StaminaConstants.TERRAIN_FACTOR_*
    // 重复定义（值完全一致）。移除去重，统一通过 StaminaConstants.TERRAIN_FACTOR_* 访问。
    
    // 根据密度值获取地形系数（插值映射，与 MaterialTerrainTable 内嵌密度表一致）
    // 典型 g/cm³：snow 0.35 | grass/heather 0.5 | grass_lush 1.2 | dirt/soil 1.33 | sand 1.63 |
    //            gravel 1.682 | pebbles 1.7~1.79 | asphalt/concrete 2.243~2.3 | cobble/stone 2.75 | tiles_stone 2.94
    // 说明：密度无法区分同区间不同材质（如木 0.65 与草 0.5），0.36~0.72 仍保持 η≈1.0 以兼容木箱等
    // @param density BallisticInfo.GetDensity()，约 0.02~3.0
    // @return 地形系数 η（约 1.0~1.85）
    static float GetTerrainFactorFromDensity(float density)
    {
        if (density <= 0.0)
            return StaminaConstants.TERRAIN_FACTOR_PAVED;

        // 低密：积雪 / 极干草地（CSV snow 0.35；grass_dry 0.3）— 明显难于铺装
        if (density <= 0.36)
        {
            float t = density / 0.36;
            return 1.12 + (1.0 - t) * 0.08;
        }

        // 0.36~0.72：草地/植被/木质道具等混叠，维持与历史行为接近的 η=1.0
        if (density <= 0.72)
            return StaminaConstants.TERRAIN_FACTOR_PAVED;

        // 0.72~1.13：室内地坪、地毯等
        if (density <= 1.13)
            return StaminaConstants.TERRAIN_FACTOR_PAVED;

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
            return StaminaConstants.TERRAIN_FACTOR_PAVED;

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
        
        // 确保重量有效
        bodyWeight = Math.Max(bodyWeight, 0.0);
        loadWeight = Math.Max(loadWeight, 0.0);
        
        // 空载时返回恢复率（负数）
        if (loadWeight < 5.0)
            return -0.0025; // 恢复率（负数）
        
        // 轻负重时返回小的恢复率
        if (loadWeight < 15.0)
            return -0.001; // 轻微恢复
        
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
        float energyToStaminaCoeff = StaminaConfigBridge.GetEnergyToStaminaCoeff();
        float staticDrainRate = staticEnergyExpenditure * energyToStaminaCoeff;
        
        // [修复] 完全移除 clip 上限，让 Pandolf 模型自然输出
        // 只防止负数，不限制上限
        staticDrainRate = Math.Max(staticDrainRate, 0.0);

        return staticDrainRate;
    }
    
}

// ============================================================
// 辅助数学函数（从 SCR_StaminaHelpers.c 合并）
// ============================================================
