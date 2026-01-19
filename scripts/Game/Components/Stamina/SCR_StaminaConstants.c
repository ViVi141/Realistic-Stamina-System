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
    static const float WALK_BASE_DRAIN_RATE = 0.060; // pts/s（Walk）
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
    static const float ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20; // 基于体重的速度惩罚系数（线性模型）
    static const float ENCUMBRANCE_SPEED_EXPONENT = 1.0; // 负重速度惩罚指数（1.0 = 线性）
    
    // 负重对体力消耗的影响系数（γ）- 基于体重的真实模型
    // 基于医学研究（Pandolf et al., 1977; Looney et al., 2018; Vine et al., 2022）
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
    static const float FITNESS_LEVEL = 1.0; // 训练有素（well-trained）
    
    // 健康状态对能量效率的影响系数
    // 训练有素者（fitness=1.0）基础消耗减少，能量利用效率更高
    // 优化：提升到35%，让训练有素者的能量效率显著提升
    static const float FITNESS_EFFICIENCY_COEFF = 0.35; // 35%效率提升（训练有素时）
    
    // 健康状态对恢复速度的影响系数
    // 训练有素者（fitness=1.0）恢复速度增加20-30%
    static const float FITNESS_RECOVERY_COEFF = 0.25; // 25%恢复速度提升（训练有素时）
    
    // ==================== 多维度恢复模型参数 ====================
    // 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
    
    // 基础恢复率（每0.2秒，在50%体力时的恢复率）
    static const float BASE_RECOVERY_RATE = 0.00015; // 每0.2秒恢复0.015%
    
    // 恢复率非线性系数（基于当前体力百分比）
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
    static const float AGE_RECOVERY_COEFF = 0.2; // 年龄恢复系数
    static const float AGE_REFERENCE = 30.0; // 年龄参考值（30岁为标准）
    
    // 累积疲劳恢复参数
    static const float FATIGUE_RECOVERY_PENALTY = 0.3; // 疲劳恢复惩罚系数（最大30%恢复速度减少）
    static const float FATIGUE_RECOVERY_DURATION_MINUTES = 20.0; // 疲劳完全恢复所需时间（分钟）
    
    // ==================== 运动持续时间影响（累积疲劳）参数 ====================
    // 基于个性化运动建模（Palumbo et al., 2018）
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
    static const float JUMP_CONSECUTIVE_WINDOW = 3.0; // 3秒
    
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
    
    // Pandolf 静态站立消耗常量
    static const float PANDOLF_STATIC_COEFF_1 = 1.5; // 基础系数（W/kg）
    static const float PANDOLF_STATIC_COEFF_2 = 2.0; // 负重系数（W/kg）
    
    // 能量到体力的转换系数
    static const float ENERGY_TO_STAMINA_COEFF = 0.000035; // 能量到体力的转换系数（优化后，支持16-18分钟连续运行）
    
    // 参考体重（用于 Pandolf 模型）
    static const float REFERENCE_WEIGHT = 90.0; // 参考体重（kg）
    
    // ==================== Givoni-Goldman 跑步模型常量 ====================
    static const float GIVONI_CONSTANT = 0.3; // 跑步常数（W/kg·m²/s²），需要根据实际测试校准
    static const float GIVONI_VELOCITY_EXPONENT = 2.2; // 速度指数（2.0-2.4，2.2为推荐值）
    
    // ==================== 地形系数常量 ====================
    static const float TERRAIN_FACTOR_PAVED = 1.0;        // 铺装路面
    static const float TERRAIN_FACTOR_DIRT = 1.1;         // 碎石路
    static const float TERRAIN_FACTOR_GRASS = 1.2;        // 高草丛
    static const float TERRAIN_FACTOR_BRUSH = 1.5;        // 重度灌木丛
    static const float TERRAIN_FACTOR_SAND = 1.8;         // 软沙地
    
    // ==================== 恢复启动延迟常量 ====================
    static const float RECOVERY_STARTUP_DELAY_SECONDS = 3.0; // 恢复启动延迟（秒）- 用于负重恢复优化
    
    // ==================== EPOC（过量耗氧）延迟参数 ====================
    // 生理学依据：运动停止后，心率不会立刻下降，前几秒应该维持高代谢水平（EPOC）
    // 参考：Brooks et al., 2000; LaForgia et al., 2006
    static const float EPOC_DELAY_SECONDS = 4.0; // EPOC延迟时间（秒）- 运动停止后延迟4秒才开始恢复
    static const float EPOC_DRAIN_RATE = 0.001; // EPOC期间的基础消耗率（每0.2秒）- 模拟维持高代谢水平
    
    // ==================== 姿态交互修正参数 ====================
    // 生理学依据：不同姿态对体力的消耗不同
    // 参考：Knapik et al., 1996; Pandolf et al., 1977
    static const float POSTURE_CROUCH_MULTIPLIER = 1.8; // 蹲姿行走消耗倍数（1.6-2.0倍，取1.8）
    static const float POSTURE_PRONE_MULTIPLIER = 3.0; // 匍匐爬行消耗倍数（与中速跑步相当）
    static const float POSTURE_STAND_MULTIPLIER = 1.0; // 站立行走消耗倍数（基准）
}
