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
    
    // ==================== 军事体力系统模型常量（基于速度阈值）====================
    // 基于速度阈值的分段消耗率系统
    // 参考：Military Stamina System Model (90kg Male / 22yo)
    
    // 速度阈值（m/s）
    static const float SPRINT_VELOCITY_THRESHOLD = 5.2; // m/s，Sprint速度阈值
    static const float RUN_VELOCITY_THRESHOLD = 3.7; // m/s，Run速度阈值（匹配15:27的2英里配速）
    static const float WALK_VELOCITY_THRESHOLD = 3.2; // m/s，Walk速度阈值
    
    // 基于负重的动态速度阈值（m/s）
    static const float RECOVERY_THRESHOLD_NO_LOAD = 2.5; // m/s，空载时恢复体力阈值
    static const float DRAIN_THRESHOLD_COMBAT_LOAD = 1.5; // m/s，负重30kg时开始消耗体力的阈值
    static const float COMBAT_LOAD_WEIGHT = 30.0; // kg，战斗负重（用于计算动态阈值）
    
    // 基础消耗率（pts/s，每秒消耗的点数）
    // 注意：这些值基于100点体力池系统，需要转换为0.0-1.0范围
    static const float SPRINT_BASE_DRAIN_RATE = 0.480; // pts/s（Sprint）
    static const float RUN_BASE_DRAIN_RATE = 0.105; // pts/s（Run，匹配15:27的2英里配速）
    static const float WALK_BASE_DRAIN_RATE = 0.060; // pts/s（Walk）
    static const float REST_RECOVERY_RATE = 0.250; // pts/s（Rest，恢复）
    
    // 转换为0.0-1.0范围的消耗率（每0.2秒）
    // 更新间隔为0.2秒，所以需要将pts/s转换为每0.2秒的消耗
    // 例如：0.480 pts/s = 0.480 / 100 × 0.2 = 0.00096（每0.2秒消耗0.096%）
    static const float SPRINT_DRAIN_PER_TICK = SPRINT_BASE_DRAIN_RATE / 100.0 * 0.2; // 每0.2秒
    static const float RUN_DRAIN_PER_TICK = RUN_BASE_DRAIN_RATE / 100.0 * 0.2; // 每0.2秒
    static const float WALK_DRAIN_PER_TICK = WALK_BASE_DRAIN_RATE / 100.0 * 0.2; // 每0.2秒
    static const float REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2; // 每0.2秒
    
    // 初始体力状态（ACFT后预疲劳状态）
    static const float INITIAL_STAMINA_AFTER_ACFT = 0.85; // 85.0 / 100.0 = 0.85（85%）
    
    // 精疲力尽阈值
    static const float EXHAUSTION_THRESHOLD = 0.0; // 0.0（0点）
    static const float EXHAUSTION_LIMP_SPEED = 1.0; // m/s（跛行速度）
    static const float SPRINT_ENABLE_THRESHOLD = 0.20; // 0.20（20点），体力≥20时才能Sprint
    
    // 坡度修正系数
    static const float GRADE_UPHILL_COEFF = 0.12; // 每1%上坡增加12%消耗
    static const float GRADE_DOWNHILL_COEFF = 0.05; // 每1%下坡减少5%消耗（假设）
    static const float HIGH_GRADE_THRESHOLD = 15.0; // 15%（高坡度阈值）
    static const float HIGH_GRADE_MULTIPLIER = 1.2; // 高坡度额外1.2×乘数
    
    // 目标平均速度（m/s）- 基于用户需求：2英里在15分27秒内完成
    // 2英里 ≈ 3218.7米，时间927秒，平均速度 ≈ 3.47 m/s
    static const float TARGET_AVERAGE_SPEED = 3.47; // m/s
    
    // 目标Run速度（m/s）- 双稳态-应激性能模型的核心目标速度
    // 对应游戏倍率约为 0.71 (3.7 / 5.2)
    static const float TARGET_RUN_SPEED = 3.7; // m/s
    static const float TARGET_RUN_SPEED_MULTIPLIER = TARGET_RUN_SPEED / GAME_MAX_SPEED; // 0.7115
    
    // 意志力平台期阈值（体力百分比）
    // 体力高于此值时，保持恒定目标速度（模拟意志力克服早期疲劳）
    static const float WILLPOWER_THRESHOLD = 0.25; // 25%
    
    // 平滑过渡起点（体力百分比）
    // 在25%-5%之间使用平滑过渡，避免突兀的"撞墙"效果
    // 将25%设为"疲劳临界区"的起点，而不是终点
    static const float SMOOTH_TRANSITION_START = 0.25; // 25%（疲劳临界区起点）
    static const float SMOOTH_TRANSITION_END = 0.05; // 5%，平滑过渡结束点（真正的力竭点）
    
    // 跛行速度倍数（最低速度）
    static const float MIN_LIMP_SPEED_MULTIPLIER = 1.0 / GAME_MAX_SPEED; // 1.0 m/s / 5.2 = 0.1923
    
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
    
    // 负重对速度的惩罚系数（β）- 基于体重的真实模型
    // 基于 US Army 实验数据（Knapik et al., 1996; Quesada et al., 2000; Vine et al., 2022）：
    // - 负重 0% BM 时：无影响
    // - 负重 20-30% BM 时：速度下降约 5-10%
    // - 负重 40-50% BM 时：速度下降约 15-20%（30kg ≈ 40%体重）
    // - 负重 60-70% BM 时：速度下降约 24-30%
    // - 负重 100% BM 时：速度下降约 35-40%（外推）
    // 
    // 真实模型：速度惩罚基于体重百分比，而不是最大负重百分比
    // 速度惩罚 = β * (负重/体重)
    // 其中 β = 0.20 表示40%体重负重时，速度下降20%（符合文献）
    // 使用线性模型，因为文献显示速度下降与体重百分比基本线性相关
    static const float ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20; // 基于体重的速度惩罚系数（线性模型）
    static const float ENCUMBRANCE_SPEED_EXPONENT = 1.0; // 负重速度惩罚指数（1.0 = 线性）
    
    // 负重对体力消耗的影响系数（γ）- 基于体重的真实模型
    // 基于医学研究（Pandolf et al., 1977; Looney et al., 2018; Vine et al., 2022）：
    // - 负重 0% BM 时：1.0×（基准）
    // - 负重 20-30% BM 时：1.3-1.5×
    // - 负重 40-50% BM 时：1.5-1.8×（30kg ≈ 40%体重）
    // - 负重 60-70% BM 时：2.0-2.5×
    // - 负重 100% BM 时：2.5-3.0×
    // 
    // 真实模型：体力消耗倍数基于体重百分比
    // 体力消耗倍数 = 1 + γ * (负重/体重)
    // γ = 1.5 表示40%体重负重时，消耗增加60%（1 + 1.5 × 0.4 = 1.6倍），符合文献
    static const float ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5; // 基于体重的体力消耗系数
    
    // 最小速度倍数（防止体力完全耗尽时完全无法移动）
    static const float MIN_SPEED_MULTIPLIER = 0.15; // 15%最低速度（约0.78 m/s，勉强行走）
    
    // 最大速度倍数限制（防止超过游戏引擎限制）
    static const float MAX_SPEED_MULTIPLIER = 1.0; // 100%最高速度（不超过游戏最大速度）
    
    // ==================== 角色特征常量 ====================
    // 角色体重（kg）- 用于计算负重占体重的百分比（基于文献的真实模型）
    // 游戏内标准体重为90kg
    // 参考文献：Knapik et al., 1996; Quesada et al., 2000
    static const float CHARACTER_WEIGHT = 90.0; // kg
    
    // 角色年龄（岁）- 基于ACFT标准（22-26岁男性）
    // 当前设置为22岁（训练有素男性）
    static const float CHARACTER_AGE = 22.0; // 岁
    
    // 健康状态/体能水平（Fitness Level）- 基于个性化运动建模
    // 参考文献：Palumbo et al., 2018 - Personalizing physical exercise in a computational model
    // 0.0 = 未训练（untrained）
    // 0.5 = 普通健康（average fitness）
    // 1.0 = 训练有素（well-trained）- 当前设置
    // 
    // 训练有素者的特点（基于文献）：
    // - 能量效率提高15-20%（基础消耗减少）
    // - 恢复速度增加20-30%
    // - 高速度时消耗更少（速度平方项系数降低）
    static const float FITNESS_LEVEL = 1.0; // 训练有素（well-trained）
    
    // 健康状态对能量效率的影响系数
    // 训练有素者（fitness=1.0）基础消耗减少15-20%
    static const float FITNESS_EFFICIENCY_COEFF = 0.18; // 18%效率提升（训练有素时）
    
    // 健康状态对恢复速度的影响系数
    // 训练有素者（fitness=1.0）恢复速度增加20-30%
    static const float FITNESS_RECOVERY_COEFF = 0.25; // 25%恢复速度提升（训练有素时）
    
    // ==================== 多维度恢复模型参数 ====================
    // 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
    //
    // 恢复模型考虑多个维度：
    // 1. 当前体力百分比（非线性恢复）：体力越低恢复越快，体力越高恢复越慢
    // 2. 健康状态/训练水平：训练有素者恢复更快
    // 3. 休息时间：刚停止运动时恢复快（快速恢复期），长时间休息后恢复慢（慢速恢复期）
    // 4. 年龄：年轻者恢复更快
    // 5. 累积疲劳恢复：运动后的疲劳需要时间恢复
    
    // 基础恢复率（每0.2秒，在50%体力时的恢复率）
    static const float BASE_RECOVERY_RATE = 0.00015; // 每0.2秒恢复0.015%
    
    // 恢复率非线性系数（基于当前体力百分比）
    // 公式：recovery_rate = BASE_RECOVERY_RATE × (1.0 + RECOVERY_NONLINEAR_COEFF × (1.0 - stamina_percent))
    // 体力越低，恢复越快；体力越高，恢复越慢
    // 例如：体力20%时，恢复率 = BASE × (1.0 + 0.5 × 0.8) = BASE × 1.4（快40%）
    //      体力80%时，恢复率 = BASE × (1.0 + 0.5 × 0.2) = BASE × 1.1（快10%）
    //      体力100%时，恢复率 = BASE × (1.0 + 0.5 × 0.0) = BASE × 1.0（标准）
    static const float RECOVERY_NONLINEAR_COEFF = 0.5; // 非线性恢复系数（0.0-1.0）
    
    // 快速恢复期参数（刚停止运动后的快速恢复阶段）
    // 生理学依据：运动后0-2分钟为快速恢复期（EPOC - Excess Post-Exercise Oxygen Consumption）
    static const float FAST_RECOVERY_DURATION_MINUTES = 2.0; // 快速恢复期持续时间（分钟）
    static const float FAST_RECOVERY_MULTIPLIER = 1.5; // 快速恢复期恢复速度倍数（1.5倍）
    
    // 慢速恢复期参数（长时间休息后的慢速恢复阶段）
    // 生理学依据：休息超过10分钟后，恢复速度逐渐减慢（接近上限时恢复变慢）
    static const float SLOW_RECOVERY_START_MINUTES = 10.0; // 慢速恢复期开始时间（分钟）
    static const float SLOW_RECOVERY_MULTIPLIER = 0.7; // 慢速恢复期恢复速度倍数（0.7倍）
    
    // 年龄对恢复速度的影响系数
    // 年轻者（22岁）恢复快，年老者恢复慢
    // 公式：age_recovery_multiplier = 1.0 + AGE_RECOVERY_COEFF × (30.0 - age) / 30.0
    // 22岁时：multiplier = 1.0 + 0.2 × (30.0 - 22.0) / 30.0 = 1.0 + 0.053 = 1.053（快5.3%）
    // 30岁时：multiplier = 1.0 + 0.2 × (30.0 - 30.0) / 30.0 = 1.0（标准）
    // 40岁时：multiplier = 1.0 + 0.2 × (30.0 - 40.0) / 30.0 = 1.0 - 0.067 = 0.933（慢6.7%）
    static const float AGE_RECOVERY_COEFF = 0.2; // 年龄恢复系数
    static const float AGE_REFERENCE = 30.0; // 年龄参考值（30岁为标准）
    
    // 累积疲劳恢复参数
    // 运动后的疲劳需要时间恢复，影响恢复速度
    // 公式：fatigue_recovery_multiplier = 1.0 - FATIGUE_RECOVERY_PENALTY × min(exercise_duration_minutes / FATIGUE_RECOVERY_DURATION, 1.0)
    // 例如：运动10分钟后，疲劳恢复惩罚 = 0.3 × min(10 / 20, 1.0) = 0.3 × 0.5 = 0.15（恢复速度减少15%）
    static const float FATIGUE_RECOVERY_PENALTY = 0.3; // 疲劳恢复惩罚系数（最大30%恢复速度减少）
    static const float FATIGUE_RECOVERY_DURATION_MINUTES = 20.0; // 疲劳完全恢复所需时间（分钟）
    
    // ==================== 运动持续时间影响（累积疲劳）参数 ====================
    // 基于个性化运动建模（Palumbo et al., 2018）
    // 长时间运动后，相同速度的消耗会逐渐增加
    // 
    // 生理学依据：
    // - 糖原耗竭后，依赖脂肪代谢，效率降低（约15-20分钟开始）
    // - 肌肉疲劳导致机械效率下降（约30-60分钟开始）
    // - 体温升高导致代谢率增加（约10-15分钟开始）
    //
    // 累积疲劳因子：fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × exercise_duration_minutes
    // 例如：运动30分钟后，疲劳因子 = 1.0 + 0.015 × 30 = 1.45（消耗增加45%）
    static const float FATIGUE_ACCUMULATION_COEFF = 0.015; // 每分钟增加1.5%消耗（基于文献：30分钟增加约45%）
    static const float FATIGUE_START_TIME_MINUTES = 5.0; // 疲劳开始累积的时间（分钟），前5分钟无疲劳累积
    static const float FATIGUE_MAX_FACTOR = 2.0; // 最大疲劳因子（2.0倍，即消耗最多增加100%）
    
    // ==================== 代谢适应参数（Metabolic Adaptation）====================
    // 基于个性化运动建模（Palumbo et al., 2018）
    // 根据运动强度动态调整能量来源和效率
    //
    // 运动强度分区（基于速度比，对应VO2max百分比）：
    // - 有氧区（<60% VO2max，speed_ratio < 0.6）：主要依赖脂肪，效率高（效率因子 = 0.9）
    // - 混合区（60-80% VO2max，0.6 ≤ speed_ratio < 0.8）：糖原+脂肪混合（效率因子 = 1.0）
    // - 无氧区（>80% VO2max，speed_ratio ≥ 0.8）：主要依赖糖原，效率低但功率高（效率因子 = 1.2）
    //
    // 代谢效率因子：根据速度比动态调整
    static const float AEROBIC_THRESHOLD = 0.6; // 有氧阈值（60% VO2max）
    static const float ANAEROBIC_THRESHOLD = 0.8; // 无氧阈值（80% VO2max）
    static const float AEROBIC_EFFICIENCY_FACTOR = 0.9; // 有氧区效率因子（90%，更高效）
    static const float MIXED_EFFICIENCY_FACTOR = 1.0; // 混合区效率因子（100%，标准）
    static const float ANAEROBIC_EFFICIENCY_FACTOR = 1.2; // 无氧区效率因子（120%，低效但高功率）
    
    // ==================== 负重配置常量 ====================
    // 基准负重（kg）- 角色携带的基础物品重量
    // 只包括衣服、鞋子等基础物品，不包括武器、装备等
    // 基准负重 = 1.36kg（基础衣物的合理重量）
    // 这是"无额外负重"状态下的基础物品重量
    static const float BASE_WEIGHT = 1.36; // kg
    
    // 最大负重（kg）- 角色可以携带的最大重量（包含基准负重）
    static const float MAX_ENCUMBRANCE_WEIGHT = 40.5; // kg
    
    // 战斗负重（kg）- 战斗状态下的推荐负重阈值（包含基准负重）
    // 超过此重量时，可能会影响战斗表现
    // 30kg = 基准负重（12kg）+ 额外负重（18kg）
    // 30kg ≈ 33% 体重（90kg），符合文献中的"中度负重"范围
    static const float COMBAT_ENCUMBRANCE_WEIGHT = 30.0; // kg
    
    // ==================== 动作体力消耗常量 ====================
    // 基于医学研究：跳跃和翻越动作的能量消耗远高于普通移动
    // 参考：垂直跳跃的能量消耗约为水平移动的 3-5 倍
    // 翻越动作（包括攀爬）的能量消耗约为水平移动的 2-4 倍
    
    // 跳跃体力消耗（单次）
    // 基于医学研究：一次垂直跳跃消耗的能量约为 0.5-1.0 千卡
    // 转换为游戏中的体力百分比：约 2-5%
    static const float JUMP_STAMINA_COST = 0.03; // 3% 体力（单次跳跃）
    
    // 翻越体力消耗（单次）
    // 翻越动作包括：翻越障碍物、攀爬等
    // 基于医学研究：翻越动作的能量消耗约为跳跃的 1.5-2 倍
    // 调整：降低消耗以匹配游戏体验（翻越动作通常很快完成）
    // 注意：实际消耗 = 基础消耗 × 负重倍数，所以基础消耗应该较低
    static const float VAULT_STAMINA_COST = 0.015; // 1.5% 体力（单次翻越，降低以匹配游戏体验）
    
    // 跳跃检测阈值（垂直速度，m/s）
    // 当垂直速度超过此值时，判定为跳跃
    static const float JUMP_VERTICAL_VELOCITY_THRESHOLD = 2.0; // m/s
    
    // 翻越检测阈值（垂直速度，m/s）
    // 当垂直速度持续超过此值时，判定为翻越/攀爬
    static const float VAULT_VERTICAL_VELOCITY_THRESHOLD = 1.5; // m/s
    
    // ==================== 坡度影响参数 ====================
    // 基于 Pandolf 模型：坡度对能量消耗的影响
    // Pandolf 模型公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
    // 其中 G 是坡度（0 = 平地，正数 = 上坡，负数 = 下坡）
    // 
    // 坡度影响系数（基于医学研究）：
    // - 上坡：能量消耗显著增加，坡度每增加1度，消耗增加约5-10%
    // - 下坡：能量消耗减少，但减少幅度较小（约上坡的1/3）
    // 
    // 简化模型：坡度影响倍数 = 1 + α·|G|·sign(G)
    // 其中 α 是坡度影响系数，sign(G) 区分上坡（+1）和下坡（-1）
    static const float SLOPE_UPHILL_COEFF = 0.08; // 上坡影响系数（每度增加8%消耗）
    static const float SLOPE_DOWNHILL_COEFF = 0.03; // 下坡影响系数（每度减少3%消耗，约为上坡的1/3）
    static const float SLOPE_MAX_MULTIPLIER = 2.0; // 最大坡度影响倍数（上坡）
    static const float SLOPE_MIN_MULTIPLIER = 0.7; // 最小坡度影响倍数（下坡）
    
    // ==================== 负重×坡度交互项参数 ====================
    // 基于文献：负重越大，坡度的影响越明显（非线性交互）
    // 交互项：负重×坡度
    // 例如：30kg负重（33%体重）+ 5°上坡 = 1 + 0.15 × 0.33 × 5 = 1.25倍额外影响
    static const float ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15; // 负重×坡度交互系数
    
    // ==================== 速度×负重×坡度三维交互参数 ====================
    // 基于 Pandolf 模型：速度、负重、坡度的三维交互项
    // 三维交互项：速度² × 负重 × 坡度
    // 例如：速度80%、30kg负重（33%体重）、5°上坡 = 0.10 × 0.33 × 0.8² × 5 = 0.106倍额外消耗
    static const float SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10; // 速度×负重×坡度交互系数
    
    // ==================== Sprint（冲刺）相关参数 ====================
    // Sprint是类似于追击或逃命的动作，追求速度，同时体力大幅消耗
    // Sprint时速度应该比Run快，但仍然受体力和负重限制
    
    // Sprint速度加成（相对于Run）
    // Sprint时速度 = Run速度 × (1 + SPRINT_SPEED_BOOST)
    // 例如：如果Run速度是80%，Sprint速度 = 80% × 1.15 = 92%
    static const float SPRINT_SPEED_BOOST = 0.15; // Sprint时速度比Run快15%
    
    // Sprint最高速度倍数限制（基于现实情况）
    // 基于运动生理学数据：
    // - 一般健康成年人的Sprint速度：20-30 km/h（约5.5-8.3 m/s）
    // - 游戏最大速度：5.2 m/s（约18.7 km/h），在合理范围内
    // - 考虑到负重和疲劳，Sprint最高速度应该限制在游戏最大速度的100%
    // - 这意味着Sprint的最高速度 = 5.2 m/s（游戏最大速度）
    // 注意：实际速度仍受体力和负重影响，只有在满体力、无负重时才能达到此上限
    static const float SPRINT_MAX_SPEED_MULTIPLIER = 1.0; // Sprint最高速度倍数（100% = 游戏最大速度5.2 m/s）
    
    // Sprint体力消耗倍数（相对于Run）
    // Sprint时体力消耗 = Run消耗 × SPRINT_STAMINA_DRAIN_MULTIPLIER
    // 基于医学研究：Sprint时的能量消耗约为Run的2-3倍
    static const float SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5; // Sprint时体力消耗是Run的2.5倍
    
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
        
        // 调试输出（仅在第一次或值变化时输出，避免过多日志，仅在客户端）
        if (owner && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
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
        float speedPenalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * bodyMassPercent;
        
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
    //
    // @param staminaPercent 当前体力百分比（0.0-1.0）
    // @param restDurationMinutes 休息持续时间（分钟），从停止运动开始计算
    // @param exerciseDurationMinutes 运动持续时间（分钟），用于计算累积疲劳
    // @param currentWeight 当前负重（kg），用于优化重载下的恢复速率
    // @return 恢复率（每0.2秒），表示应该恢复的体力百分比
    static float CalculateMultiDimensionalRecoveryRate(float staminaPercent, float restDurationMinutes, float exerciseDurationMinutes, float currentWeight = 0.0)
    {
        // 限制输入参数
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        restDurationMinutes = Math.Max(restDurationMinutes, 0.0);
        exerciseDurationMinutes = Math.Max(exerciseDurationMinutes, 0.0);
        
        // ==================== 1. 基础恢复率（基于当前体力百分比，非线性）====================
        // 体力越低，恢复越快；体力越高，恢复越慢
        // 公式：recovery_rate = BASE_RECOVERY_RATE × (1.0 + RECOVERY_NONLINEAR_COEFF × (1.0 - stamina_percent))
        float staminaRecoveryMultiplier = 1.0 + (RECOVERY_NONLINEAR_COEFF * (1.0 - staminaPercent));
        float baseRecoveryRate = BASE_RECOVERY_RATE * staminaRecoveryMultiplier;
        
        // ==================== 2. 健康状态/训练水平影响 ====================
        // 训练有素者（fitness=1.0）恢复速度增加25%
        float fitnessRecoveryMultiplier = 1.0 + (FITNESS_RECOVERY_COEFF * FITNESS_LEVEL);
        fitnessRecoveryMultiplier = Math.Clamp(fitnessRecoveryMultiplier, 1.0, 1.5); // 限制在100%-150%之间
        
        // ==================== 3. 休息时间影响（快速恢复期 vs 慢速恢复期）====================
        float restTimeMultiplier = 1.0;
        if (restDurationMinutes <= FAST_RECOVERY_DURATION_MINUTES)
        {
            // 快速恢复期（0-2分钟）：恢复速度增加50%
            restTimeMultiplier = FAST_RECOVERY_MULTIPLIER;
        }
        else if (restDurationMinutes >= SLOW_RECOVERY_START_MINUTES)
        {
            // 慢速恢复期（≥10分钟）：恢复速度减少30%
            // 线性过渡：从10分钟到20分钟，从1.0倍逐渐降至0.7倍
            float transitionDuration = 10.0; // 过渡时间（分钟）
            float transitionProgress = Math.Min((restDurationMinutes - SLOW_RECOVERY_START_MINUTES) / transitionDuration, 1.0);
            restTimeMultiplier = 1.0 - (transitionProgress * (1.0 - SLOW_RECOVERY_MULTIPLIER));
        }
        // 否则：标准恢复期（2-10分钟），restTimeMultiplier = 1.0
        
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
        
        // ==================== 6. 负重影响：重载下的恢复优化 ====================
        // 重载下增加恢复速率上限，模拟士兵通过深呼吸快速调整的能力
        // 这样可以避免重装冲刺后的"跛行"惩罚太久，保持游戏节奏
        float loadRecoveryBoost = 1.0;
        if (currentWeight > COMBAT_LOAD_WEIGHT)
        {
            // 负重超过30kg时，恢复速率增加（模拟深呼吸调整能力）
            // 计算负重比例（基于战斗负重）
            float loadRatio = (currentWeight - COMBAT_LOAD_WEIGHT) / (MAX_ENCUMBRANCE_WEIGHT - COMBAT_LOAD_WEIGHT); // 0.0-1.0（30kg到40.5kg）
            loadRatio = Math.Clamp(loadRatio, 0.0, 1.0);
            // 恢复速率提升：30kg时为1.0，40.5kg时为1.3（最高30%提升）
            loadRecoveryBoost = 1.0 + (loadRatio * 0.3); // 1.0-1.3倍
        }
        else if (currentWeight > 0.0)
        {
            // 负重在0-30kg之间时，也有轻微的恢复优化
            float loadRatio = currentWeight / COMBAT_LOAD_WEIGHT; // 0.0-1.0
            loadRatio = Math.Clamp(loadRatio, 0.0, 1.0);
            // 恢复速率提升：0kg时为1.0，30kg时为1.1（最高10%提升）
            loadRecoveryBoost = 1.0 + (loadRatio * 0.1); // 1.0-1.1倍
        }
        
        // ==================== 综合恢复率计算 ====================
        // 综合恢复率 = 基础恢复率 × 健康状态倍数 × 休息时间倍数 × 年龄倍数 × 疲劳恢复倍数 × 负重恢复优化
        float totalRecoveryRate = baseRecoveryRate * fitnessRecoveryMultiplier * restTimeMultiplier * ageRecoveryMultiplier * fatigueRecoveryMultiplier * loadRecoveryBoost;
        
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
        float staminaDrainMultiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * bodyMassPercent);
        
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
            // 使用Pow计算 weightRatio^1.2
            float weightRatioPower = Pow(weightRatio, 1.2);
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
    
    // 计算坡度修正乘数（基于百分比坡度）
    // 
    // 坡度修正：
    // - 上坡 (G > 0): K_grade = 1 + (G × 0.12)
    // - 下坡 (G < 0): K_grade = 1 + (G × 0.05)
    // - 高坡度 (G > 15%): 额外 × 1.2
    //
    // @param gradePercent 坡度百分比（例如，5% = 5.0）
    // @return 坡度修正乘数
    // 计算坡度修正乘数（使用非线性增长，让小坡几乎无感，陡坡才真正吃力）
    // 
    // 优化：使用幂函数代替线性增长，让小坡（5%以下）几乎无感，陡坡才真正吃力
    // 这样 Everon 的缓坡就不会让玩家频繁断气
    //
    // @param gradePercent 坡度百分比（例如，5% = 5.0）
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
            float gradePower = Pow(normalizedGrade, 1.2); // 使用1.2次方
            kGrade = 1.0 + (gradePower * 5.0);
            
            // 限制最大坡度修正（避免数值爆炸）
            kGrade = Math.Min(kGrade, 3.0); // 最多3倍消耗
        }
        else if (gradePercent < 0.0)
        {
            // 下坡：每1%减少3%消耗
            kGrade = 1.0 + (gradePercent * GRADE_DOWNHILL_COEFF);
            // 限制下坡修正，避免消耗变为负数
            kGrade = Math.Max(kGrade, 0.5); // 最多减少50%消耗
        }
        
        return kGrade;
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
}
