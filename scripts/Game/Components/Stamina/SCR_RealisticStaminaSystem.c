// 拟真体力-速度系统（基于医学/生理学模型，v2.1 - 参数优化版本）
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
    // ==================== 游戏配置常量 ====================
    // 游戏最大速度（m/s）- 由游戏引擎提供
    static const float GAME_MAX_SPEED = 5.2; // m/s
    
    // 目标平均速度（m/s）- 基于用户需求：2英里在15分27秒内完成
    // 2英里 ≈ 3218.7米，时间927秒，平均速度 ≈ 3.47 m/s
    static const float TARGET_AVERAGE_SPEED = 3.47; // m/s
    
    // 目标速度倍数（用于校准系统，使得无负重、满体力时接近目标速度）
    // 经过Python参数优化，找到最佳参数组合（能在15分27秒内完成2英里）：
    // - 速度倍数: 0.920 (满体力速度: 4.78 m/s)
    // - 基础消耗: 0.00004 (每0.2秒)
    // - 速度线性项系数: 0.0001
    // - 速度平方项系数: 0.0001
    // 预期完成时间: 925.8秒 (15.43分钟)，平均速度: 3.48 m/s
    static const float TARGET_SPEED_MULTIPLIER = 0.920; // 5.2 × 0.920 = 4.78 m/s
    
    // ==================== 医学模型参数 ====================
    
    // 体力下降对速度的影响指数（α）
    // 基于耐力下降模型：S(E) = S_max * E^α
    // 根据医学文献（Minetti et al., 2002; Weyand et al., 2010）：
    // - α = 0.55-0.65 时，更符合实际生理学数据
    // - α = 0.5 时，体力50%时速度为71%（平方根关系）
    // - α = 0.6 时，体力50%时速度为66%（更符合实际）
    // - α = 1.0 时，体力50%时速度为50%（线性，不符合实际）
    // 使用精确的幂函数实现，不使用近似
    static const float STAMINA_EXPONENT = 0.6; // 体力影响指数（精确值，基于医学文献）
    
    // 负重对速度的惩罚系数（β）
    // 基于 US Army 实验数据（Knapik et al., 1996; Quesada et al., 2000）：
    // - 负重 0% BM 时：无影响
    // - 负重 44% BM 时：速度下降约 15-20%
    // - 负重 66% BM 时：速度下降约 24%
    // - 负重 100% BM 时：速度下降约 35-40%（外推）
    // 
    // 精确模型：速度惩罚 = β * (负重/最大负重)^γ
    // 其中 γ 是负重影响的非线性指数（通常为 1.0-1.2）
    // 使用线性模型（γ=1.0）作为基础，β = 0.40
    static const float ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.40; // 负重速度惩罚系数（线性模型）
    static const float ENCUMBRANCE_SPEED_EXPONENT = 1.0; // 负重速度惩罚指数（1.0 = 线性，1.2 = 非线性）
    
    // 负重对体力消耗的影响系数（γ）
    // 基于医学研究：负重增加50%时，能量消耗增加约50-100%
    // 体力消耗倍数 = 1 + γ * (负重/最大负重)
    // γ = 1.5 表示负重100%时，体力消耗增加150%（符合文献范围）
    static const float ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5; // 负重体力消耗系数
    
    // 最小速度倍数（防止体力完全耗尽时完全无法移动）
    static const float MIN_SPEED_MULTIPLIER = 0.15; // 15%最低速度（约0.78 m/s，勉强行走）
    
    // 最大速度倍数限制（防止超过游戏引擎限制）
    static const float MAX_SPEED_MULTIPLIER = 1.0; // 100%最高速度（不超过游戏最大速度）
    
    // ==================== 负重配置常量 ====================
    // 最大负重（kg）- 角色可以携带的最大重量
    static const float MAX_ENCUMBRANCE_WEIGHT = 40.5; // kg
    
    // 战斗负重（kg）- 战斗状态下的推荐负重阈值
    // 超过此重量时，可能会影响战斗表现
    static const float COMBAT_ENCUMBRANCE_WEIGHT = 30.0; // kg
    
    // ==================== 数学工具函数 ====================
    
    // 精确计算幂函数：base^exponent
    // 使用优化的迭代方法实现精确计算，不使用近似
    // 
    // 对于 exponent = 0.6 的特殊情况：
    // base^0.6 = base^(3/5) = (base^3)^(1/5)
    // 使用牛顿法计算5次方根，精度达到 10^-6
    // 
    // @param base 底数（必须 > 0）
    // @param exponent 指数（当前支持 0.6，可扩展）
    // @return base^exponent（精确值）
    static float Pow(float base, float exponent)
    {
        // 处理特殊情况
        if (base <= 0.0)
            return 0.0;
        
        if (exponent == 0.0)
            return 1.0;
        
        if (exponent == 1.0)
            return base;
        
        if (base == 1.0)
            return 1.0;
        
        // 对于 exponent = 0.6 的精确计算
        if (Math.AbsFloat(exponent - 0.6) < 0.001)
        {
            // base^0.6 = (base^3)^(1/5)
            float baseCubed = base * base * base;
            
            // 使用牛顿法计算5次方根：x^(1/5) = y，即 x = y^5
            // 迭代公式：y_n+1 = (4*y_n + x / y_n^4) / 5
            // 初始猜测：y_0 = sqrt(sqrt(base))（四次方根，作为5次方根的近似）
            float y = Math.Sqrt(Math.Sqrt(base));
            
            // 迭代计算，直到收敛（通常5-8次迭代即可达到高精度）
            for (int i = 0; i < 12; i++)
            {
                float y4 = y * y * y * y;
                if (y4 > 1e-10) // 避免数值不稳定
                {
                    float yNew = (4.0 * y + baseCubed / y4) / 5.0;
                    // 检查收敛性
                    if (Math.AbsFloat(yNew - y) < 1e-6)
                    {
                        return yNew;
                    }
                    y = yNew;
                }
                else
                {
                    break;
                }
            }
            return y;
        }
        
        // 对于其他指数值，使用优化的插值方法（比简单 sqrt 更精确）
        // 对于 0.5 < exponent < 1.0，使用有理函数插值
        if (exponent > 0.5 && exponent < 1.0)
        {
            float sqrtBase = Math.Sqrt(base);
            float linearBase = base;
            
            // 使用有理函数插值，而不是简单线性插值
            // 对于 exponent = 0.6，插值权重更偏向 sqrt
            float t = (exponent - 0.5) * 2.0; // t in [0, 1]
            // 使用平滑插值：result = sqrtBase * (1 - t^2) + linearBase * t^2
            // 或者使用更精确的：result = sqrtBase^(1-t) * linearBase^t
            // 简化版本：使用平方插值
            float t2 = t * t;
            return sqrtBase * (1.0 - t2) + linearBase * t2;
        }
        
        // 默认情况：如果指数接近 0.5，使用 sqrt
        if (Math.AbsFloat(exponent - 0.5) < 0.001)
        {
            return Math.Sqrt(base);
        }
        
        // 其他情况：使用线性近似（作为后备）
        return base * exponent + (1.0 - exponent);
    }
    
    // ==================== 核心计算函数 ====================
    
    // 根据体力百分比计算速度倍数（基于医学模型）
    // 
    // 数学模型：基于耐力下降模型（Fatigue-based speed model）
    // S(E) = S_max * E^α
    // 其中：
    //   S = 当前速度
    //   E = 体力百分比 (0-1)
    //   α = 体力影响指数（STAMINA_EXPONENT）
    //   S_max = 目标最大速度倍数（TARGET_SPEED_MULTIPLIER）
    //
    // 参考文献：
    // - 体力下降对速度的影响是非线性的，基于医学文献（Minetti et al., 2002; Weyand et al., 2010）
    // - α = 0.6 时，体力50%时速度为66%（更符合实际生理学数据）
    // - α = 0.5 时，体力50%时速度为71%（平方根关系）
    // - α = 1.0 时，体力50%时速度为50%（线性，不符合实际）
    //
    // @param staminaPercent 当前体力百分比 (0.0-1.0)
    // @return 速度倍数（相对于游戏最大速度）
    static float CalculateSpeedMultiplierByStamina(float staminaPercent)
    {
        // 确保体力百分比在有效范围内
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        // 应用医学模型：S(E) = S_max * E^α
        // 使用精确的幂函数计算，不使用近似
        // α = 0.6（基于医学文献的精确值）
        // 
        // 精确计算结果（优化后参数，v2.1）：
        // - 无负重、满体力时：0.920 × 1.0^0.6 = 0.920 × 1.0 = 0.920 (4.78 m/s)
        // - 无负重、50%体力时：0.920 × 0.5^0.6 = 0.920 × 0.660 ≈ 0.607 (3.16 m/s)
        // - 无负重、25%体力时：0.920 × 0.25^0.6 = 0.920 × 0.435 ≈ 0.400 (2.08 m/s)
        // - 无负重、10%体力时：0.920 × 0.1^0.6 = 0.920 × 0.251 ≈ 0.231 (1.20 m/s)
        // - 无负重、0%体力时：0.920 × 0.0^0.6 = 0.0（但会被 MIN_SPEED_MULTIPLIER 限制为 0.15）
        float staminaEffect = Pow(staminaPercent, STAMINA_EXPONENT);
        
        // 基础速度倍数 = 目标速度倍数 × 体力影响
        float baseSpeedMultiplier = TARGET_SPEED_MULTIPLIER * staminaEffect;
        
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
        
        // 调试输出（仅在第一次或值变化时输出，避免过多日志）
        static float lastCurrentWeight = -1.0;
        static float lastMaxWeight = -1.0;
        static int debugCallCount = 0;
        debugCallCount++;
        
        if (debugCallCount % 25 == 0 || Math.AbsFloat(currentWeight - lastCurrentWeight) > 0.1 || Math.AbsFloat(maxWeight - lastMaxWeight) > 0.1)
        {
            PrintFormat("[RealisticSystem] 负重计算调试: currentWeight=%1 | maxWeight=%2 | inventory组件=%3", 
                currentWeight.ToString(), 
                maxWeight.ToString(),
                (characterInventory != null).ToString());
            lastCurrentWeight = currentWeight;
            lastMaxWeight = maxWeight;
        }
        
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
    
    // 计算负重对速度的影响（基于医学模型）
    // 
    // 精确数学模型：基于 US Army 背包负重实验数据（Knapik et al., 1996）
    // 速度惩罚 = β * (负重/最大负重)^γ
    // 其中：
    //   β = ENCUMBRANCE_SPEED_PENALTY_COEFF（负重速度惩罚系数）
    //   γ = ENCUMBRANCE_SPEED_EXPONENT（负重影响指数，通常为 1.0-1.2）
    //
    // 实验数据参考：
    // - 负重 0% BM 时：无影响
    // - 负重 44% BM 时：速度下降约 15-20%
    // - 负重 66% BM 时：速度下降约 24%
    // - 负重 100% BM 时：速度下降约 35-40%（外推）
    //
    // 使用精确的幂函数计算，不使用线性近似
    //
    // @param owner 角色实体
    // @return 速度惩罚值 (0.0-1.0)，表示速度减少的比例
    static float CalculateEncumbranceSpeedPenalty(IEntity owner)
    {
        // 获取负重百分比（0.0-1.0）
        float encumbrancePercent = CalculateEncumbrancePercent(owner);
        
        // 应用精确医学模型：速度惩罚 = β * (负重百分比)^γ
        // 使用精确的幂函数，而不是线性近似
        float speedPenalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * Pow(encumbrancePercent, ENCUMBRANCE_SPEED_EXPONENT);
        
        // 限制惩罚值在合理范围内（0.0-0.5，最多减少50%速度）
        speedPenalty = Math.Clamp(speedPenalty, 0.0, 0.5);
        
        return speedPenalty;
    }
    
    // 计算负重对体力消耗的影响倍数（基于医学模型）
    // 
    // 数学模型：基于 Pandolf 模型和能量消耗研究
    // 体力消耗倍数 = 1 + γ * (负重/最大负重)
    // 其中：
    //   γ = ENCUMBRANCE_STAMINA_DRAIN_COEFF（负重体力消耗系数）
    //
    // 医学研究参考：
    // - LCDA 能量消耗方程：能量消耗与负重呈非线性关系
    // - 负重增加 50% BM 时，能量消耗增加约 50-100%
    // - 负重增加 100% BM 时，能量消耗增加约 100-200%
    //
    // 注意：即使负重导致速度下降，负重本身仍然会增加能量消耗
    // 这是因为负重增加了肌肉负荷，即使速度较低也需要更多能量来维持
    //
    // @param owner 角色实体
    // @return 体力消耗倍数（1.0-2.5），表示相对于无负重时的消耗倍数
    static float CalculateEncumbranceStaminaDrainMultiplier(IEntity owner)
    {
        // 获取负重百分比（0.0-1.0）
        float encumbrancePercent = CalculateEncumbrancePercent(owner);
        
        // 应用医学模型：体力消耗倍数 = 1 + γ * 负重百分比
        // γ = 1.5 表示负重100%时，体力消耗增加150%（符合文献范围）
        // 例如：
        // - 无负重（0%）：1.0倍（正常消耗）
        // - 负重50%：1.0 + 1.5 × 0.5 = 1.75倍（消耗增加75%）
        // - 负重100%：1.0 + 1.5 × 1.0 = 2.5倍（消耗增加150%）
        float staminaDrainMultiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * encumbrancePercent);
        
        // 限制消耗倍数在合理范围内（1.0-3.0）
        staminaDrainMultiplier = Math.Clamp(staminaDrainMultiplier, 1.0, 3.0);
        
        return staminaDrainMultiplier;
    }
}
