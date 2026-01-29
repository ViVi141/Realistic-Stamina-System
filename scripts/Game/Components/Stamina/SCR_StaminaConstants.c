// 体力系统常量定义模块
// 存放所有体力系统相关的常量定义
// 模块化拆分：从 SCR_RealisticStaminaSystem.c 提取的常量定义

class StaminaConstants
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
    static const float RUN_BASE_DRAIN_RATE = 0.075; // pts/s（Run，优化后约22分钟耗尽，支撑20分钟连续运行）
    static const float WALK_BASE_DRAIN_RATE = 0.045; // pts/s（Walk，降低消耗率以突出与跑步的差距）
    static const float REST_RECOVERY_RATE = 0.250; // pts/s（Rest，恢复）
    
    // 转换为0.0-1.0范围的消耗率（每0.2秒）
    // 更新间隔为0.2秒，所以需要将pts/s转换为每0.2秒的消耗
    // 例如：0.480 pts/s = 0.480 / 100 × 0.2 = 0.00096（每0.2秒消耗0.096%）
    static const float SPRINT_DRAIN_PER_TICK = SPRINT_BASE_DRAIN_RATE / 100.0 * 0.2; // 每0.2秒
    static const float RUN_DRAIN_PER_TICK = RUN_BASE_DRAIN_RATE / 100.0 * 0.2; // 每0.2秒
    static const float WALK_DRAIN_PER_TICK = WALK_BASE_DRAIN_RATE / 100.0 * 0.2; // 每0.2秒
    static const float REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2; // 每0.2秒
    
    // 初始体力状态（满值）
    static const float INITIAL_STAMINA_AFTER_ACFT = 1.0; // 100.0 / 100.0 = 1.0（100%，满值）
    
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
    // 基于 US Army 实验数据（Knapik et al., 1996; Quesada et al., 2000; Vine et al., 2022）
    // 注意：这些值现在从配置管理器获取（GetEncumbranceSpeedPenaltyCoeff(), GetEncumbranceStaminaDrainCoeff()）
    static const float ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20; // 基于体重的速度惩罚系数（线性模型）
    static const float ENCUMBRANCE_SPEED_EXPONENT = 1.0; // 负重速度惩罚指数（1.0 = 线性）
    
    // 负重对体力消耗的影响系数（γ）- 基于体重的真实模型
    // 基于医学研究（Pandolf et al., 1977; Looney et al., 2018; Vine et al., 2022）
    // [修复] 与Python数字孪生保持一致，从1.5改为2.0
    static const float ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0; // 基于体重的体力消耗系数
    
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
    static const float FITNESS_LEVEL = 1.0; // 训练有素（well-trained）
    
    // 健康状态对能量效率的影响系数
    // 训练有素者（fitness=1.0）基础消耗减少，能量利用效率更高
    // 优化：提升到35%，让训练有素者的能量效率显著提升
    static const float FITNESS_EFFICIENCY_COEFF = 0.35; // 35%效率提升（训练有素时）
    
    // 健康状态对恢复速度的影响系数
    // 训练有素者（fitness=1.0）恢复速度增加20-30%
    static const float FITNESS_RECOVERY_COEFF = 0.25; // 25%恢复速度提升（训练有素时）
    
    // ==================== 多维度恢复模型参数（深度生理压制版本）====================
    // 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
    // 核心概念：从"净增加"改为"代谢净值"
    // 最终恢复率 = (基础恢复率 * 姿态修正) - (负重压制 + 氧债惩罚)
    
    // 基础恢复率（每0.2秒，在50%体力时的恢复率）
    // [修复] 与Python数字孪生保持一致，从0.00035降低到0.00015（降低57%）
    // 计算逻辑：1.0(体力) / 1333秒(22.2分钟) / 5(每秒tick数) = 0.00015
    // 注意：此值现在从配置管理器获取（GetBaseRecoveryRate()）
    static const float BASE_RECOVERY_RATE = 0.00015; // 每0.2秒恢复0.015%（与Python一致）
    
    // 恢复率非线性系数（基于当前体力百分比）
    static const float RECOVERY_NONLINEAR_COEFF = 0.5; // 非线性恢复系数（0.0-1.0）
    
    // 快速恢复期参数（刚停止运动后的快速恢复阶段）
    // 拟真平衡点：模拟"喘匀第一口氧气"
    // 生理学上，氧债的50%是在停止运动后的前30-60秒内偿还的
    // 模拟停止运动后前90秒的高效恢复
    static const float FAST_RECOVERY_DURATION_MINUTES = 1.5; // 快速恢复期持续时间（分钟）
    static const float FAST_RECOVERY_MULTIPLIER = 1.6; // [修复] 与Python一致，从2.5降低到1.6（降低36%）
    
    // 中等恢复期参数
    // 拟真平衡点：平衡快速恢复期和慢速恢复期
    static const float MEDIUM_RECOVERY_START_MINUTES = 1.5; // 中等恢复期开始时间（分钟）
    // [修复] 与Python数字孪生保持一致，从8.5改为5.0
    static const float MEDIUM_RECOVERY_DURATION_MINUTES = 5.0; // 中等恢复期持续时间（分钟）
    static const float MEDIUM_RECOVERY_MULTIPLIER = 1.3; // [修复] 与Python一致，从1.4降低到1.3（降低7%）
    
    // 慢速恢复期参数（长时间休息后的慢速恢复阶段）
    // 生理学依据：休息超过10分钟后，恢复速度逐渐减慢（接近上限时恢复变慢）
    // 优化：提升到0.8倍（从0.7倍）
    static const float SLOW_RECOVERY_START_MINUTES = 10.0; // 慢速恢复期开始时间（分钟）
    static const float SLOW_RECOVERY_MULTIPLIER = 0.6; // 慢速恢复期恢复速度倍数（从0.8降低到0.6，降低25%）
    
    // 年龄对恢复速度的影响系数
    static const float AGE_RECOVERY_COEFF = 0.2; // 年龄恢复系数
    static const float AGE_REFERENCE = 30.0; // 年龄参考值（30岁为标准）
    
    // 累积疲劳恢复参数
    // 游戏性优化：大幅降低疲劳惩罚（从0.15降到0.05），减少负重对恢复的惩罚感
    static const float FATIGUE_RECOVERY_PENALTY = 0.05; // 疲劳恢复惩罚系数（最大5%恢复速度减少）
    static const float FATIGUE_RECOVERY_DURATION_MINUTES = 20.0; // 疲劳完全恢复所需时间（分钟）
    
    // ==================== 姿态恢复加成参数（深度生理压制版本）====================
    // 深度生理压制：趴下不只是为了隐蔽，更是为了让心脏负荷最小化
    // 姿态加成设定的更有体感，但不过分
    // 站姿：确保能够覆盖静态站立消耗
    // 蹲姿：减少下肢肌肉紧张，+40%恢复速度
    // 趴姿：全身放松，最大化血液循环，+60%恢复速度
    // 逻辑：趴下是唯一的快速回血手段（重力分布均匀），强迫重装兵必须趴下
    // 注意：这些值现在从配置管理器获取（GetStandingRecoveryMultiplier(), GetProneRecoveryMultiplier()）
    static const float STANDING_RECOVERY_MULTIPLIER = 1.3; // [修复] 与Python一致，从1.5降低到1.3（降低13%）
    static const float CROUCHING_RECOVERY_MULTIPLIER = 1.4; // [修复] 与Python一致，从1.5降低到1.4（降低7%）
    static const float PRONE_RECOVERY_MULTIPLIER = 1.6; // [修复] 与Python一致，从1.8降低到1.6（降低11%）
    
    // ==================== 负重对恢复的静态剥夺机制（深度生理压制版本）====================
    // 医学解释：背负30kg装备站立时，斜方肌、腰椎和下肢肌肉仍在进行高强度静力收缩
    // 这种收缩产生的代谢废物（乳酸）排放速度远慢于空载状态
    // 数学实现：恢复率与负重百分比（BM%）挂钩
    // Penalty = (当前总重 / 身体耐受基准)^2 * 0.0004
    // 结果：穿着40kg装备站立恢复时，原本100%的基础恢复会被扣除70%
    // 战术意图：强迫重装兵必须趴下（通过姿态加成抵消负重扣除），否则回血极慢
    // 注意：这些值现在从配置管理器获取（GetLoadRecoveryPenaltyCoeff(), GetLoadRecoveryPenaltyExponent()）
    static const float LOAD_RECOVERY_PENALTY_COEFF = 0.0001; // [修复] 负重恢复惩罚系数，从 0.0004 降低到 0.0001
    static const float LOAD_RECOVERY_PENALTY_EXPONENT = 2.0; // 负重恢复惩罚指数（2.0 = 平方）
    static const float BODY_TOLERANCE_BASE = 90.0; // [修复] 身体耐受基准（kg），从 30.0 增加到 90.0（参考体重）
    
    // ==================== 边际效应衰减机制（深度生理压制版本）====================
    // 医学解释：身体从"半死不活"恢复到"喘匀气"很快（前80%），但要从"喘匀气"恢复到"肌肉巅峰竞技状态"非常慢（最后20%）
    // 数学实现：当体力>80%时，恢复率 = 原始恢复率 * (1.1 - 当前体力百分比)
    // 结果：最后10%的体力恢复速度会比前50%慢3-4倍
    // 战术意图：玩家经常会处于80%-90%的"亚健康"状态，只有长时间修整才能真正满血
    static const float MARGINAL_DECAY_THRESHOLD = 0.8; // 边际效应衰减阈值（80%体力）
    static const float MARGINAL_DECAY_COEFF = 1.1; // 边际效应衰减系数
    
    // ==================== 最低体力阈值限制（深度生理压制版本）====================
    // 医学解释：当体力过低时，身体处于极度疲劳状态，需要更长时间的休息才能开始恢复
    // 数学实现：体力低于20%时，必须处于静止状态10秒后才允许开始回血
    // 战术意图：防止玩家在极度疲劳时通过"跑两步停一下"的方式快速回血
    // [修复 v3.6.1] 将极度疲劳惩罚时间从 10秒 缩短为 3秒
    // 模拟深呼吸 3 秒后即可开始恢复，而不是发呆 10 秒
    static const float MIN_RECOVERY_STAMINA_THRESHOLD = 0.2; // 最低体力阈值（20%）
    static const float MIN_RECOVERY_REST_TIME_SECONDS = 3.0; // 最低体力时需要的休息时间（秒）
    
    // ==================== 运动持续时间影响（累积疲劳）参数 ====================
    // 基于个性化运动建模（Palumbo et al., 2018）
    // 注意：这些值现在从配置管理器获取（GetFatigueAccumulationCoeff(), GetFatigueMaxFactor()）
    static const float FATIGUE_ACCUMULATION_COEFF = 0.015; // 每分钟增加1.5%消耗（基于文献：30分钟增加约45%）
    static const float FATIGUE_START_TIME_MINUTES = 5.0; // 疲劳开始累积的时间（分钟），前5分钟无疲劳累积
    static const float FATIGUE_MAX_FACTOR = 2.0; // 最大疲劳因子（2.0倍，即消耗最多增加100%）
    
    // ==================== 代谢适应参数（Metabolic Adaptation）====================
    // 基于个性化运动建模（Palumbo et al., 2018）
    static const float AEROBIC_THRESHOLD = 0.6; // 有氧阈值（60% VO2max）
    static const float ANAEROBIC_THRESHOLD = 0.8; // 无氧阈值（80% VO2max）
    static const float AEROBIC_EFFICIENCY_FACTOR = 0.9; // 有氧区效率因子（90%，更高效）
    static const float MIXED_EFFICIENCY_FACTOR = 1.0; // 混合区效率因子（100%，标准）
    static const float ANAEROBIC_EFFICIENCY_FACTOR = 1.2; // 无氧区效率因子（120%，低效但高功率）
    
    // ==================== 负重配置常量 ====================
    // 基准负重（kg）- 角色携带的基础物品重量
    static const float BASE_WEIGHT = 1.36; // kg
    
    // 最大负重（kg）- 角色可以携带的最大重量（包含基准负重）
    static const float MAX_ENCUMBRANCE_WEIGHT = 40.5; // kg
    
    // 战斗负重（kg）- 战斗状态下的推荐负重阈值（包含基准负重）
    static const float COMBAT_ENCUMBRANCE_WEIGHT = 30.0; // kg
    
    // ==================== 动作体力消耗常量 ====================
    // 基于医学研究：跳跃和翻越动作的能量消耗远高于普通移动
    
    // 跳跃体力消耗（单次）
    static const float JUMP_STAMINA_BASE_COST = 0.035; // 3.5% 体力（单次跳跃，v2.5优化）
    
    // 翻越体力消耗（单次）
    static const float VAULT_STAMINA_START_COST = 0.02; // 2% 体力（翻越起始消耗，v2.5优化）
    
    // 持续攀爬消耗（每秒）
    static const float CLIMB_STAMINA_TICK_COST = 0.01; // 1% 体力/秒（持续攀爬消耗，v2.5优化）
    
    // 低体力跳跃阈值：体力 < 10% 时禁用跳跃（肌肉在力竭时无法提供爆发力）
    static const float JUMP_MIN_STAMINA_THRESHOLD = 0.10; // 10% 体力
    
    // 连续跳跃时间窗口（秒）：在此时间内的连续跳跃会增加消耗
    static const float JUMP_CONSECUTIVE_WINDOW = 2.0; // 2秒
    
    // 连续跳跃惩罚系数：每次连续跳跃额外增加50%消耗
    static const float JUMP_CONSECUTIVE_PENALTY = 0.5; // 50%
    
    // 跳跃检测阈值（垂直速度，m/s）
    static const float JUMP_VERTICAL_VELOCITY_THRESHOLD = 2.0; // m/s
    
    // 翻越检测阈值（垂直速度，m/s）
    static const float VAULT_VERTICAL_VELOCITY_THRESHOLD = 1.5; // m/s
    
    // ==================== 坡度影响参数 ====================
    // 基于 Pandolf 模型：坡度对能量消耗的影响
    static const float SLOPE_UPHILL_COEFF = 0.08; // 上坡影响系数（每度增加8%消耗）
    static const float SLOPE_DOWNHILL_COEFF = 0.03; // 下坡影响系数（每度减少3%消耗，约为上坡的1/3）
    static const float SLOPE_MAX_MULTIPLIER = 2.0; // 最大坡度影响倍数（上坡）
    static const float SLOPE_MIN_MULTIPLIER = 0.7; // 最小坡度影响倍数（下坡）
    
    // ==================== 负重×坡度交互项参数 ====================
    static const float ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15; // 负重×坡度交互系数
    
    // ==================== 速度×负重×坡度三维交互参数 ====================
    static const float SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10; // 速度×负重×坡度交互系数
    
    // ==================== Sprint（冲刺）相关参数 ====================
    // Sprint速度加成（相对于Run，v2.5优化）
    static const float SPRINT_SPEED_BOOST = 0.30; // Sprint时速度比Run快30%（v2.5优化）
    
    // Sprint最高速度倍数限制（基于现实情况）
    static const float SPRINT_MAX_SPEED_MULTIPLIER = 1.0; // Sprint最高速度倍数（100% = 游戏最大速度5.2 m/s）
    
    // Sprint体力消耗倍数（相对于Run，v2.5优化）
    static const float SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0; // Sprint时体力消耗是Run的3.0倍（v2.5优化）
    
    // ==================== Pandolf 模型常量 ====================
    // 完整 Pandolf 能量消耗模型（Pandolf et al., 1977）
    static const float PANDOLF_BASE_COEFF = 2.7; // 基础系数（W/kg）
    static const float PANDOLF_VELOCITY_COEFF = 3.2; // 速度系数（W/kg）
    static const float PANDOLF_VELOCITY_OFFSET = 0.7; // 速度偏移（m/s）
    static const float PANDOLF_GRADE_BASE_COEFF = 0.23; // 坡度基础系数（W/kg）
    static const float PANDOLF_GRADE_VELOCITY_COEFF = 1.34; // 坡度速度系数（W/kg）
    
    // Pandolf 静态站立消耗常量（v2.15.0：降低静态消耗，给30KG留出呼吸空间）
    static const float PANDOLF_STATIC_COEFF_1 = 1.2; // 基础系数（W/kg），从1.5降低到1.2（降低20%）
    static const float PANDOLF_STATIC_COEFF_2 = 1.6; // 负重系数（W/kg），从2.0降低到1.6（降低20%）
    
    // 能量到体力的转换系数
    // 注意：此值现在从配置管理器获取（GetEnergyToStaminaCoeff()）
    // [修复] 与Python数字孪生保持一致，从0.000035降低到0.000015
    static const float ENERGY_TO_STAMINA_COEFF = 0.000015; // 能量到体力的转换系数（与Python一致）
    
    // 参考体重（用于 Pandolf 模型）
    static const float REFERENCE_WEIGHT = 90.0; // 参考体重（kg）
    
    // ==================== Givoni-Goldman 跑步模型常量 ====================
    static const float GIVONI_CONSTANT = 0.8; // 跑步常数（W/kg·m²/s²），从1.0降低到0.8，减缓奔跑时的消耗速度
    static const float GIVONI_VELOCITY_EXPONENT = 2.2; // 速度指数（2.0-2.4，2.2为推荐值）
    
    // ==================== 地形系数常量 ====================
    static const float TERRAIN_FACTOR_PAVED = 1.0;        // 铺装路面
    static const float TERRAIN_FACTOR_DIRT = 1.1;         // 碎石路
    static const float TERRAIN_FACTOR_GRASS = 1.2;        // 高草丛
    static const float TERRAIN_FACTOR_BRUSH = 1.5;        // 重度灌木丛
    static const float TERRAIN_FACTOR_SAND = 1.8;         // 软沙地
    
    // ==================== 恢复启动延迟常量（深度生理压制版本）====================
    // 恢复启动延迟常量（深度生理压制版本）
    // 深度生理压制：停止运动后3秒内系统完全不处理恢复
    // 医学解释：剧烈运动停止后的前10-15秒，身体处于摄氧量极度不足状态（Oxygen Deficit）
    // 此时血液流速仍处于峰值，心脏负担极重，体能并不会开始"恢复"，而是在维持不崩塌
    // 目的：消除"跑两步停一下瞬间回血"的游击战式打法，同时提高游戏流畅度
    static const float RECOVERY_STARTUP_DELAY_SECONDS = 3.0; // 恢复启动延迟（秒）- 从5秒缩短到3秒
    
    // ==================== EPOC（过量耗氧）延迟参数 ====================
    // 生理学依据：运动停止后，心率不会立刻下降，前几秒应该维持高代谢水平（EPOC）
    // 参考：Brooks et al., 2000; LaForgia et al., 2006
    // 拟真平衡点：缩短到0.5秒，减少按键无响应感
    static const float EPOC_DELAY_SECONDS = 0.5; // EPOC延迟时间（秒）- 从2秒降到0.5秒
    static const float EPOC_DRAIN_RATE = 0.001; // EPOC期间的基础消耗率（每0.2秒）- 模拟维持高代谢水平

    // ==================== 姿态交互修正参数 ====================
    // 生理学依据：不同姿态对体力的消耗不同
    // 参考：Knapik et al., 1996; Pandolf et al., 1977
    static const float POSTURE_CROUCH_MULTIPLIER = 1.8; // 蹲姿行走消耗倍数（1.6-2.0倍，取1.8）
    static const float POSTURE_PRONE_MULTIPLIER = 3.0; // 匍匐爬行消耗倍数（与中速跑步相当）
    static const float POSTURE_STAND_MULTIPLIER = 1.0; // 站立行走消耗倍数（基准）
    
    // ==================== 游泳体力消耗参数 ====================
    // 生理学依据：游泳的能量消耗远高于陆地移动（约3-5倍）
    // 物理模型：阻力与速度平方成正比 F_d = 0.5 * ρ * v² * C_d * A
    // 参考：Holmér, I. (1974). Energy cost of arm stroke, leg kick, and whole crawl stroke.
    //       Journal of Applied Physiology, 37(5), 762-765.
    //       Pendergast, D. R., et al. (1977). Energy cost of swimming.
    
    // 游泳物理模型参数
    static const float SWIMMING_DRAG_COEFFICIENT = 0.5; // 阻力系数（C_d，简化值）
    static const float SWIMMING_WATER_DENSITY = 1000.0; // 水密度（ρ，kg/m³）
    static const float SWIMMING_FRONTAL_AREA = 0.5; // 正面面积（A，m²，简化值）
    static const float SWIMMING_BASE_POWER = 20.0; // 基础游泳功率（W，维持浮力和基本动作，从25W降至20W，提高水中存活率）
    
    // 负重阈值（负浮力效应）
    static const float SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0; // kg，超过此重量时静态消耗大幅增加（从20kg提高到25kg）
    static const float SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0; // 超过阈值时的静态消耗倍数（模拟踩水）
    static const float SWIMMING_FULL_PENALTY_WEIGHT = 40.0; // kg，达到此重量时应用满额惩罚
    static const float SWIMMING_LOW_INTENSITY_DISCOUNT = 0.7; // 低强度踩水时的消耗折扣（30%折扣）
    static const float SWIMMING_LOW_INTENSITY_VELOCITY = 0.2; // m/s，低于此速度视为低强度踩水
    
    // 游泳专用转换系数（游泳的代谢效率比陆地低，所以系数应略高于陆地）
    // 注意：不要乘以100，直接使用小数系数（0.0-1.0范围）
    static const float SWIMMING_ENERGY_TO_STAMINA_COEFF = 0.00005; // 游泳功率到体力消耗的转换系数
    static const float SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0; // 动态功率效率因子（人体游泳效率约0.05，这里取折中值）
    
    // 3D 游泳模型参数（垂直方向比水平方向更“费力”）
    static const float SWIMMING_VERTICAL_DRAG_COEFFICIENT = 1.2; // 垂直方向阻力系数（非流线型）
    static const float SWIMMING_VERTICAL_FRONTAL_AREA = 0.8; // 垂直方向受力面积（m²，简化）
    static const float SWIMMING_VERTICAL_SPEED_THRESHOLD = 0.05; // m/s，垂直速度阈值
    static const float SWIMMING_EFFECTIVE_GRAVITY_COEFF = 0.15; // 水中有效重力系数（上浮时对抗负浮力/装备）
    static const float SWIMMING_BUOYANCY_FORCE_COEFF = 0.10; // 浮力对抗系数（下潜时对抗浮力）
    static const float SWIMMING_VERTICAL_UP_MULTIPLIER = 2.5; // 上浮效率惩罚倍数
    static const float SWIMMING_VERTICAL_DOWN_MULTIPLIER = 1.5; // 下潜效率惩罚倍数
    static const float SWIMMING_MAX_DRAIN_RATE = 0.15; // 每秒最大消耗（3D模型允许更高峰值）
    
    // 负重对垂直方向的影响（非线性/更贴近体感）
    // - 上浮：负重越大越困难
    // - 下潜：负重越大越容易（对抗浮力需求下降）
    static const float SWIMMING_VERTICAL_UP_BASE_BODY_FORCE_COEFF = 0.02; // 即使不背负也存在的上浮“基础费力”
    static const float SWIMMING_VERTICAL_DOWN_LOAD_RELIEF_COEFF = 0.50; // 负重能抵消浮力的比例（0-1）
    
    // 生存压力常数：确保水中静止也不会被陆地恢复覆盖（单位：W）
    static const float SWIMMING_SURVIVAL_STRESS_POWER = 15.0;
    
    // 生理功率上限：防止异常速度/模组冲突导致 totalPower 溢出（单位：W）
    static const float SWIMMING_MAX_TOTAL_POWER = 2000.0;
    
    // 湿重效应参数
    static const float WET_WEIGHT_DURATION = 30.0; // 秒，上岸后湿重持续时间
    static const float WET_WEIGHT_MIN = 5.0; // kg，最小湿重（衣服吸水）
    static const float WET_WEIGHT_MAX = 10.0; // kg，最大湿重（完全湿透）
    
    // 游泳检测参数
    static const float SWIMMING_MIN_SPEED = 0.1; // m/s，游泳最小速度阈值
    static const float SWIMMING_VERTICAL_VELOCITY_THRESHOLD = -0.5; // m/s，垂直速度阈值（检测是否在水中）
    
    // ==================== 环境因子常量 ====================
    // 热应激参数（基于时间段）
    static const float ENV_HEAT_STRESS_START_HOUR = 10.0; // 热应激开始时间（小时）
    static const float ENV_HEAT_STRESS_PEAK_HOUR = 14.0; // 热应激峰值时间（小时，正午）
    static const float ENV_HEAT_STRESS_END_HOUR = 18.0; // 热应激结束时间（小时）
    static const float ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.5; // 热应激最大倍数（50%消耗增加，提高环境影响）
    static const float ENV_HEAT_STRESS_BASE_MULTIPLIER = 1.0; // 热应激基础倍数（无影响）
    static const float ENV_HEAT_STRESS_INDOOR_REDUCTION = 0.5; // 室内热应激减少比例（50%）
    
    // 降雨湿重参数
    static const float ENV_RAIN_WEIGHT_MIN = 2.0; // kg，小雨时的湿重
    static const float ENV_RAIN_WEIGHT_MAX = 8.0; // kg，暴雨时的湿重
    static const float ENV_RAIN_WEIGHT_DURATION = 60.0; // 秒，停止降雨后湿重持续时间
    static const float ENV_RAIN_WEIGHT_DECAY_RATE = 0.0167; // 每秒衰减率（60秒内完全消失）
    
    // 湿重饱和上限（防止游泳湿重+降雨湿重叠加导致数值爆炸）
    static const float ENV_MAX_TOTAL_WET_WEIGHT = 10.0; // kg，总湿重上限（游泳+降雨）
    
    // 环境因子检测频率（性能优化）
    static const float ENV_CHECK_INTERVAL = 5.0; // 秒，环境因子检测间隔
    
    // 室内检测参数
    static const float ENV_INDOOR_CHECK_HEIGHT = 10.0; // 米，向上检测高度（判断是否有屋顶）
    
    // ==================== 高级环境因子常量（v2.15.0）====================
    
    // 降雨强度相关常量
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE = 0.5; // kg/秒，基础湿重增加速率
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT = 1.5; // 降雨强度指数（非线性增长）
    static const float ENV_RAIN_INTENSITY_THRESHOLD = 0.01; // 降雨强度阈值（低于此值不计算湿重）
    static const float ENV_RAIN_INTENSITY_HEAVY_THRESHOLD = 0.8; // 暴雨阈值（呼吸阻力触发）
    static const float ENV_RAIN_INTENSITY_BREATHING_PENALTY = 0.05; // 暴雨时的无氧代谢增加比例
    
    // 风阻相关常量
    static const float ENV_WIND_RESISTANCE_COEFF = 0.05; // 风阻系数（体力消耗权重）
    static const float ENV_WIND_SPEED_THRESHOLD = 1.0; // m/s，风速阈值（低于此值忽略）
    static const float ENV_WIND_TAILWIND_BONUS = 0.02; // 顺风时的消耗减少比例
    static const float ENV_WIND_TAILWIND_SPEED_BONUS = 0.01; // 顺风时的速度加成比例
    
    // 路面泥泞度相关常量
    static const float ENV_MUD_PENALTY_MAX = 0.4; // 最大泥泞惩罚（40%地形阻力增加）
    static const float ENV_MUD_SLIPPERY_THRESHOLD = 0.3; // 积水阈值（高于此值触发滑倒风险）
    static const float ENV_MUD_SPRINT_PENALTY = 0.1; // 泥泞时Sprint速度惩罚
    static const float ENV_MUD_SLIP_RISK_BASE = 0.001; // 基础滑倒风险（每0.2秒）
    
    // 气温相关常量
    static const float ENV_TEMPERATURE_HEAT_THRESHOLD = 30.0; // °C，热应激阈值
    static const float ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02; // 每高1度，恢复率降低2%
    static const float ENV_TEMPERATURE_COLD_THRESHOLD = 0.0; // °C，冷应激阈值
    static const float ENV_TEMPERATURE_COLD_STATIC_PENALTY = 0.03; // 低温时静态消耗增加比例
    static const float ENV_TEMPERATURE_COLD_RECOVERY_PENALTY = 0.05; // 低温时恢复率降低比例
    
    // 地表湿度相关常量
    static const float ENV_SURFACE_WETNESS_SOAK_RATE = 1.0; // kg/秒，趴下时的湿重增加速率
    static const float ENV_SURFACE_WETNESS_THRESHOLD = 0.1; // 积水阈值（高于此值触发湿重增加）
    static const float ENV_SURFACE_WETNESS_MARGINAL_DECAY_ADVANCE = 0.1; // 边际效应衰减提前触发比例
    static const float ENV_SURFACE_WETNESS_PRONE_PENALTY = 0.15; // 湿地趴下时的恢复惩罚
    
    // ==================== 配置系统桥接方法 ====================
    
    // 获取能量到体力转换系数（从配置管理器）
    static float GetEnergyToStaminaCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.energy_to_stamina_coeff;
        }
        return 0.000035; // 默认值
    }
    
    // 获取基础恢复率（从配置管理器）
    static float GetBaseRecoveryRate()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.base_recovery_rate;
        }
        return 0.00035; // 默认值（更新为0.00035，从0.0004降低）
    }
    
    // 获取站姿恢复倍数（从配置管理器）
    static float GetStandingRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.standing_recovery_multiplier;
        }
        return 1.5; // 默认值（从2.0降低到1.5）
    }
    
    // 获取趴姿恢复倍数（从配置管理器）
    static float GetProneRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.prone_recovery_multiplier;
        }
        return 1.8; // 默认值（从2.2降低到1.8）
    }
    
    // 获取负重恢复惩罚系数（从配置管理器）
    static float GetLoadRecoveryPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.load_recovery_penalty_coeff;
        }
        return 0.0004; // 默认值
    }
    
    // 获取负重恢复惩罚指数（从配置管理器）
    static float GetLoadRecoveryPenaltyExponent()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.load_recovery_penalty_exponent;
        }
        return 2.0; // 默认值
    }
    
    // 获取负重速度惩罚系数（从配置管理器）
    static float GetEncumbranceSpeedPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_speed_penalty_coeff;
        }
        return 0.20; // 默认值
    }
    
    // 获取负重体力消耗系数（从配置管理器）
    static float GetEncumbranceStaminaDrainCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_stamina_drain_coeff;
        }
        return 1.5; // 默认值
    }
    
    // 获取Sprint体力消耗倍数（从配置管理器）
    static float GetSprintStaminaDrainMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.sprint_stamina_drain_multiplier;
        }
        return 3.0; // 默认值
    }
    
    // 获取疲劳累积系数（从配置管理器）
    static float GetFatigueAccumulationCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.fatigue_accumulation_coeff;
        }
        return 0.015; // 默认值
    }
    
    // 获取最大疲劳因子（从配置管理器）
    static float GetFatigueMaxFactor()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.fatigue_max_factor;
        }
        return 2.0; // 默认值
    }
    
    // 获取蹲姿消耗倍数（从配置管理器）
    // 修复：添加此桥接方法，使配置参数生效
    static float GetPostureCrouchMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.posture_crouch_multiplier;
        }
        return 2.0; // 默认值（蹲姿消耗是站姿的2倍）
    }

    // 获取趴姿消耗倍数（从配置管理器）
    // 修复：添加此桥接方法，使配置参数生效
    static float GetPostureProneMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.posture_prone_multiplier;
        }
        return 2.5; // 默认值（趴姿消耗是站姿的2.5倍）
    }

    // ==================== 恢复模型参数配置方法 ====================

    // 获取恢复非线性系数（从配置管理器）
    static float GetRecoveryNonlinearCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.recovery_nonlinear_coeff;
        }
        return 0.5; // 默认值
    }

    // 获取快速恢复倍数（从配置管理器）
    static float GetFastRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.fast_recovery_multiplier;
        }
        return 2.5; // 默认值
    }

    // 获取中等恢复倍数（从配置管理器）
    static float GetMediumRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.medium_recovery_multiplier;
        }
        return 1.4; // 默认值
    }

    // 获取慢速恢复倍数（从配置管理器）
    static float GetSlowRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.slow_recovery_multiplier;
        }
        return 0.6; // 默认值
    }

    // ==================== Sprint参数配置方法 ====================

    // 获取Sprint速度阈值（从配置管理器）
    static float GetSprintVelocityThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.sprint_velocity_threshold;
        }
        return 5.2; // 默认值（m/s）
    }

    // 获取Sprint速度加成（从配置管理器）
    static float GetSprintSpeedBoost()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.sprint_speed_boost;
        }
        return 0.30; // 默认值（30%）
    }

    // ==================== 边际效应参数配置方法 ====================

    // 获取边际效应衰减阈值（从配置管理器）
    static float GetMarginalDecayThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.marginal_decay_threshold;
        }
        return 0.8; // 默认值（80%体力）
    }

    // 获取边际效应衰减系数（从配置管理器）
    static float GetMarginalDecayCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.marginal_decay_coeff;
        }
        return 1.1; // 默认值
    }

    // ==================== 动作消耗参数配置方法 ====================

    // 获取跳跃基础消耗（从配置管理器）
    static float GetJumpStaminaBaseCost()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.jump_stamina_base_cost;
        }
        return 0.035; // 默认值（3.5%体力）
    }

    // 获取翻越起始消耗（从配置管理器）
    static float GetVaultStaminaStartCost()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.vault_stamina_start_cost;
        }
        return 0.02; // 默认值（2%体力）
    }

    // 获取攀爬消耗（从配置管理器）
    static float GetClimbStaminaTickCost()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.climb_stamina_tick_cost;
        }
        return 0.01; // 默认值（1%体力/秒）
    }

    // ==================== 环境因子参数配置方法 ====================

    // 获取热应激惩罚系数（从配置管理器）
    static float GetEnvTemperatureHeatPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.env_temperature_heat_penalty_coeff;
        }
        return 0.02; // 默认值（每高1度恢复率降低2%）
    }

    // 获取冷应激恢复惩罚系数（从配置管理器）
    static float GetEnvTemperatureColdRecoveryPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.env_temperature_cold_recovery_penalty_coeff;
        }
        return 0.05; // 默认值（每低1度恢复率降低5%）
    }

    // 获取地表湿度惩罚最大值（从配置管理器）
    static float GetEnvSurfaceWetnessPenaltyMax()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.env_surface_wetness_prone_penalty;
        }
        return 0.15; // 默认值（最大15%恢复惩罚）
    }

    // 获取调试状态的快捷静态方法
    static bool IsDebugEnabled()
    {
        // 工作台模式下强制开启以便开发
        #ifdef WORKBENCH
            return true;
        #endif
        
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_bDebugLogEnabled;
        
        return false;
    }
    
    // 获取详细日志状态
    static bool IsVerboseLoggingEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_bVerboseLogging;
        
        return false;
    }
    
    // 获取调试信息刷新频率
    static int GetDebugUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iDebugUpdateInterval;
        
        return 5000; // 默认5秒
    }
    
    // ==================== 以下配置仅在 Custom 预设下生效 ====================
    // 当使用 EliteStandard/StandardMilsim/TacticalAction 预设时，这些配置返回默认值
    // 只有选择 Custom 预设时，用户的自定义配置才会被读取
    
    // 检查是否为 Custom 预设
    static bool IsCustomPreset()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return true;
        return false;
    }
    
    // 获取体力消耗倍率（仅 Custom 预设生效）
    static float GetStaminaDrainMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fStaminaDrainMultiplier;
        
        return 1.0; // 非 Custom 预设返回默认值
    }
    
    // 获取体力恢复倍率（仅 Custom 预设生效）
    static float GetStaminaRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fStaminaRecoveryMultiplier;
        
        return 1.0; // 非 Custom 预设返回默认值
    }
    
    // 获取负重速度惩罚倍率（仅 Custom 预设生效）
    static float GetEncumbranceSpeedPenaltyMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fEncumbranceSpeedPenaltyMultiplier;
        
        return 1.0; // 非 Custom 预设返回默认值
    }
    
    // 获取Sprint速度倍率（仅 Custom 预设生效）
    static float GetSprintSpeedMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fSprintSpeedMultiplier;
        
        return 1.3; // 非 Custom 预设返回默认值
    }
    
    // 检查是否启用热应激系统（仅 Custom 预设生效）
    static bool IsHeatStressEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableHeatStress;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用降雨湿重系统（仅 Custom 预设生效）
    static bool IsRainWeightEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableRainWeight;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用风阻系统（仅 Custom 预设生效）
    static bool IsWindResistanceEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableWindResistance;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用泥泞惩罚系统（仅 Custom 预设生效）
    static bool IsMudPenaltyEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableMudPenalty;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用疲劳积累系统（仅 Custom 预设生效）
    static bool IsFatigueSystemEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableFatigueSystem;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用代谢适应系统（仅 Custom 预设生效）
    static bool IsMetabolicAdaptationEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableMetabolicAdaptation;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用室内检测系统（仅 Custom 预设生效）
    static bool IsIndoorDetectionEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableIndoorDetection;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 获取地形检测更新间隔
    static int GetTerrainUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iTerrainUpdateInterval;
        
        return 5000; // 默认5秒
    }
    
    // 获取环境因子更新间隔
    static int GetEnvironmentUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iEnvironmentUpdateInterval;
        
        return 5000; // 默认5秒
    }
    
    // ==================== 姿态转换成本常量（Posture Transition Cost 2.0 - 乳酸堆积模型）====================
    // 核心思路：单次动作很轻，连续动作剧增。模拟肌肉在连续负重深蹲时从有氧到无氧的转变。
    // 生理学依据：肌肉在连续爆发时会产生乳酸堆积，导致肌肉疲劳和力量下降
    // 参考文献：Knapik et al., 1996; Pandolf et al., 1977; Brooks et al., 2000
    
    // 姿态转换基础消耗（0.0-1.0范围）- v2.0 优化：下调基础消耗，引入疲劳堆积机制
    static const float STANCE_COST_PRONE_TO_STAND = 0.015; // 1.5% - 趴到站（模拟推地和起身的动作）
    static const float STANCE_COST_PRONE_TO_CROUCH = 0.010; // 1.0% - 趴到蹲（中等）
    static const float STANCE_COST_CROUCH_TO_STAND = 0.005; // 0.5% - 蹲到站（极轻，允许正常掩体观察）
    static const float STANCE_COST_STAND_TO_PRONE = 0.003; // 0.3% - 站到趴（主要是离心控制）
    static const float STANCE_COST_OTHER = 0.003; // 0.3% - 其他转换（如 STAND -> CROUCH）
    
    // ==================== 疲劳堆积器（Stance Fatigue Accumulator）====================
    // 核心机制：引入隐藏变量 m_fStanceFatigue，模拟肌肉在连续负重深蹲时的乳酸堆积
    // 增加：每次变换姿态，这个变量增加 1.0
    // 衰减：每秒钟自动减少 0.5
    // 影响：实际体力消耗 = BaseCost × (1.0 + m_fStanceFatigue)
    
    // 疲劳堆积增加量（每次姿态转换）
    static const float STANCE_FATIGUE_ACCUMULATION = 1.0; // 每次姿态转换增加 1.0 疲劳值
    
    // 疲劳堆积衰减速率（每秒）
    static const float STANCE_FATIGUE_DECAY = 0.3; // 每秒衰减 0.3 疲劳值，降低衰减速率以延长疲劳影响时间
    
    // 疲劳堆积最大值（防止无限累积）
    static const float STANCE_FATIGUE_MAX = 10.0; // 最大疲劳堆积值（10.0）
    
    // ==================== 负重因子（线性化）====================
    // v2.0 优化：将原来的 1.5 次幂改为线性，避免负重影响过快
    // WeightFactor = CurrentWeight / 90.0
    // 对于 28KG 负重（总重118KG），因子约为 1.31，而不是 1.5 次幂后的 1.5
    
    // 负重基准重量（kg）
    static const float STANCE_WEIGHT_BASE = 90.0; // 90kg 基准重量
    
    // ==================== 协同保护逻辑（针对 RSS 核心的优化）====================
    // 不触发"呼吸困顿期"：姿态转换属于瞬时消耗，不应该像 Sprint 停止后那样触发 5 秒禁止恢复的逻辑
    // 应该允许玩家在变换姿态后立即开始（被压制的）恢复
    
    // 体力阈值保护：当体力低于 10% 时，强制减慢姿态切换动画（如果能控制动画速度）或禁止直接从趴姿跳跃站起
    static const float STANCE_TRANSITION_MIN_STAMINA_THRESHOLD = 0.10; // 10% 体力阈值
}