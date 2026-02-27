// 体力系统常量定义模块
// 存放所有体力系统相关的常量定义
// 模块化拆分：从 SCR_RealisticStaminaSystem.c 提取的常量定义
//
// ============================================================
// 硬配置 (HARD CONFIG) vs 软配置 (SOFT CONFIG) 分类总表
// ============================================================
//
// ── 硬配置 [HARD] ───────────────────────────────────────────
//   定义：基于已发表科学论文或不可变物理定律，有严格实验/理论依据。
//   随意改变会导致模型偏离生理现实，应视为系统常数，不得暴露为可配置项。
//
//   物理/流体力学常数（不可变）
//     SWIMMING_WATER_DENSITY    = 1000.0 kg/m³   水密度
//     SWIMMING_DRAG_COEFFICIENT = 0.5             流体阻力系数
//     SWIMMING_FRONTAL_AREA     = 0.5 m²          人体正面受力面积
//
//   Pandolf 1977 论文原始系数（不可改动）
//     PANDOLF_BASE_COEFF        = 2.7  W/kg        基础代谢系数
//     PANDOLF_VELOCITY_COEFF    = 3.2  W/kg        速度代谢系数
//     PANDOLF_VELOCITY_OFFSET   = 0.7  m/s         速度偏移
//     PANDOLF_GRADE_BASE_COEFF  = 0.23 W/kg        坡度基础系数
//     PANDOLF_GRADE_VELOCITY_COEFF = 1.34 W/kg     坡度速度系数
//
//   体力-速度影响指数 α（Minetti 2002；Weyand 2010，范围 0.55-0.65）
//     STAMINA_EXPONENT          = 0.6
//
//   有氧/无氧代谢阈值（运动生理学国际共识）
//     AEROBIC_THRESHOLD         = 0.6  (60% VO₂max)
//     ANAEROBIC_THRESHOLD       = 0.8  (80% VO₂max)
//     AEROBIC_EFFICIENCY_FACTOR = 0.9
//     ANAEROBIC_EFFICIENCY_FACTOR = 1.2
//
//   跳跃/翻越物理模型常数（Margaria 1963；经典力学）
//     JUMP_GRAVITY              = 9.81 m/s²        重力加速度（物理定律）
//     JUMP_STAMINA_TO_JOULES    = 3.14e5 J         1体力≈75kcal的换算基准（系统校准锚点）
//     JUMP_MUSCLE_EFFICIENCY    = 0.22             跳跃肌肉效率 20-25%（Margaria 1963）
//     VAULT_ISO_EFFICIENCY      = 0.12             攀爬等长收缩效率 10-15%（Margaria 1963）
//     JUMP_DETECTION_VELOCITY   = 2.0  m/s         跳跃垂直速度检测阈值（引擎物理）
//     VAULT_DETECTION_VELOCITY  = 1.5  m/s         翻越垂直速度检测阈值（引擎物理）
//     AEROBIC_EFFICIENCY_FACTOR = 0.9
//     ANAEROBIC_EFFICIENCY_FACTOR = 1.2
//
//   地形能量消耗比值（实验测量，铺路=基准）
//     TERRAIN_FACTOR_PAVED  = 1.0
//     TERRAIN_FACTOR_DIRT   = 1.1
//     TERRAIN_FACTOR_GRASS  = 1.2
//     TERRAIN_FACTOR_BRUSH  = 1.5
//     TERRAIN_FACTOR_SAND   = 1.8
//
//   热应激时间窗口（太阳辐射物理规律，不可调）
//     ENV_HEAT_STRESS_START_HOUR = 10.0
//     ENV_HEAT_STRESS_PEAK_HOUR  = 14.0
//     ENV_HEAT_STRESS_END_HOUR   = 18.0
//     ENV_TEMPERATURE_HEAT_THRESHOLD = 30.0 °C
//     ENV_TEMPERATURE_COLD_THRESHOLD =  0.0 °C
//
//   角色固定属性（22岁训练有素男性，游戏内无差异化，防止不平等游玩）
//     CHARACTER_WEIGHT          = 90.0  kg
//     CHARACTER_AGE             = 22.0  岁
//     FITNESS_LEVEL             = 1.0
//     FIXED_FITNESS_EFFICIENCY_FACTOR    = 0.70  （预计算）
//     FIXED_FITNESS_RECOVERY_MULTIPLIER  = 1.25  （预计算）
//     FIXED_AGE_RECOVERY_MULTIPLIER      = 1.053 （预计算）
//     FIXED_PANDOLF_FITNESS_BONUS        = 0.80  （预计算）
//
// ── 软配置 [SOFT] ───────────────────────────────────────────
//   定义：游戏性/体验调节参数，没有唯一正确值。
//   全部通过 SCR_RSS_Params 的 [Attribute] 字段暴露，
//   支持 EliteStandard / StandardMilsim / TacticalAction / Custom 四预设。
//   运行时经 GetXxx() 桥接方法从配置管理器读取。
//
//   A. 核心消耗/恢复标尺
//     energy_to_stamina_coeff           物理能量→游戏体力的缩放标尺
//     base_recovery_rate                基础恢复率（每tick）
//     sprint_stamina_drain_multiplier   Sprint 相对消耗倍数
//     sprint_speed_boost                Sprint 速度加成
//   B. 姿态系统
//     standing_recovery_multiplier / prone_recovery_multiplier
//     posture_crouch_multiplier / posture_prone_multiplier
//   C. 负重系统
//     encumbrance_speed_penalty_coeff / exponent / max
//     encumbrance_stamina_drain_coeff
//     load_recovery_penalty_coeff / exponent
//   D. 疲劳累积
//     fatigue_accumulation_coeff / fatigue_max_factor
//   E. 恢复曲线非线性
//     recovery_nonlinear_coeff
//     fast/medium/slow_recovery_multiplier
//     marginal_decay_threshold / coeff
//     min_recovery_stamina_threshold / rest_time_seconds
//   F. 游泳体验
//     swimming_base_power / encumbrance_threshold
//     swimming_static_drain_multiplier / dynamic_power_efficiency
//     swimming_energy_to_stamina_coeff
//   G. 环境因子
//     env_heat_stress_max_multiplier
//     env_rain_weight_max / env_wind_resistance_coeff
//     env_mud_penalty_max
//     env_temperature_heat/cold_penalty_coeff
//   H. 跳跃/翻越体验调节
//     jump_height_guess          跳跃重心抬升高度估算（m），值越大消耗越多
//     jump_horizontal_speed_guess 跳跃水平速度估算（m/s）
//     jump_efficiency            [NOTE: 生理学值 0.20-0.25，不建议修改实为HARD]
//     climb_iso_efficiency       [NOTE: 生理学值 0.10-0.15，不建议修改实为HARD]
//     JUMP_MIN_STAMINA_THRESHOLD  低体力禁止跳跃阈值（游戏设计）
//     JUMP_CONSECUTIVE_WINDOW     连续跳跃判定时间窗口（游戏设计）
//     JUMP_CONSECUTIVE_PENALTY    连续跳跃惩罚系数（游戏设计）
//     VAULT_VERT_LIFT_GUESS       翻越垂直抬升高度估算（标准障碍高度中值）
//     VAULT_LIMB_FORCE_RATIO      四肢等长力占体重比例（生物力学测量值）
//     VAULT_BASE_METABOLISM_WATTS 攀爬基础代谢附加功率（轻度静力运动均布值）
//     JUMP_VAULT_MAX_DRAIN_CLAMP  单次跳跃/翻越最大体力消耗上限
// ============================================================

class StaminaConstants
{
    // ==================== [HARD] 游戏配置常量 ====================
    // 游戏最大速度（m/s）- 由游戏引擎提供
    static const float GAME_MAX_SPEED = 5.2; // m/s
    
    // ==================== [SOFT via Settings] 军事体力系统模型常量（基于速度阈值）====================
    // 基于速度阈值的分段消耗率系统 - 以下阈值为游戏设计参数，可通过预设间接影响
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
    
    // ==================== [HARD] 医学模型参数 ====================
    
    // [HARD] 体力下降对速度的影响指数（α）
    // 基于耐力下降模型：S(E) = S_max * E^α
    // 根据医学文献（Minetti et al., 2002; Weyand et al., 2010）：
    // - α = 0.55-0.65 时，更符合实际生理学数据
    // - α = 0.5 时，体力50%时速度为71%（平方根关系）
    // - α = 0.6 时，体力50%时速度为66%（更符合实际）
    // - α = 1.0 时，体力50%时速度为50%（线性，不符合实际）
    // 此值具有严格的生理学约束，不得随意更改
    static const float STAMINA_EXPONENT = 0.6; // [HARD] 体力影响指数（医学文献范围 0.55-0.65）
    
    // [SOFT] 负重对速度的惩罚系数（β）- 由配置管理器动态获取，此处为代码回退默认值
    // 基于 US Army 实验数据（Knapik et al., 1996; Quesada et al., 2000; Vine et al., 2022）
    // 注意：运行时通过 GetEncumbranceSpeedPenaltyCoeff() / GetEncumbranceStaminaDrainCoeff() 获取
    static const float ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20; // [SOFT fallback]
    static const float ENCUMBRANCE_SPEED_EXPONENT = 1.5; // [SOFT fallback]
    
    // [SOFT] 负重对体力消耗的影响系数（γ）- 由配置管理器动态获取
    // 基于医学研究（Pandolf et al., 1977; Looney et al., 2018; Vine et al., 2022）
    static const float ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0; // [SOFT fallback]
    
    // 最小速度倍数（防止体力完全耗尽时完全无法移动）
    static const float MIN_SPEED_MULTIPLIER = 0.15; // 15%最低速度（约0.78 m/s，勉强行走）
    
    // 最大速度倍数限制（防止超过游戏引擎限制）
    static const float MAX_SPEED_MULTIPLIER = 1.0; // 100%最高速度（不超过游戏最大速度）
    
    // ==================== [HARD] 角色固定属性（防止不平等游玩）====================
    // 所有玩家角色强制使用相同的生理属性基线。
    // 这些值是 HARD CONFIG，禁止通过任何配置暴露为可变项。
    // 修改任意值后，必须同步更新下方的 FIXED_* 预计算常量！
    //
    // [HARD] 角色体重（kg）- 游戏内标准体重
    // 参考文献：Knapik et al., 1996; Quesada et al., 2000
    static const float CHARACTER_WEIGHT = 90.0; // [HARD] kg，游戏引擎固定体重
    
    // [HARD] 角色年龄（岁）- 基于ACFT标准（22-26岁男性）
    // 固定为22岁，确保所有玩家体力表现完全一致
    static const float CHARACTER_AGE = 22.0; // [HARD] 岁，固定不得更改
    
    // [HARD] 健康状态/体能水平 - 固定为训练有素（well-trained）
    // 参考文献：Palumbo et al., 2018
    // 固定为 1.0，确保所有玩家体力表现完全一致
    static const float FITNESS_LEVEL = 1.0; // [HARD] 训练有素，固定不得更改
    
    // [HARD] 健康状态对能量效率的影响系数（科学参数，不可调）
    static const float FITNESS_EFFICIENCY_COEFF = 0.35; // [HARD] 35%效率提升系数
    
    // 健康状态对恢复速度的影响系数
    // 训练有素者（fitness=1.0）恢复速度增加20-30%
    static const float FITNESS_RECOVERY_COEFF = 0.25; // 25%恢复速度提升（训练有素时）
    
    // ==================== 预计算人物属性固定结果（防止不平等游玩）====================
    // 以下四个值由上方固定角色属性（AGE=22, FITNESS_LEVEL=1.0, WEIGHT=90kg）直接预计算得出。
    // 游戏运行时直接使用这些常量，不再执行动态公式，确保所有玩家完全相同。
    // 修改上方任意角色属性后，必须同步更新这里的预计算值！
    //
    // FIXED_FITNESS_EFFICIENCY_FACTOR:
    //   = clamp(1.0 - FITNESS_EFFICIENCY_COEFF × FITNESS_LEVEL, 0.7, 1.0)
    //   = clamp(1.0 - 0.35 × 1.0, 0.7, 1.0) = clamp(0.65, 0.7, 1.0) = 0.70
    static const float FIXED_FITNESS_EFFICIENCY_FACTOR = 0.70;
    //
    // FIXED_FITNESS_RECOVERY_MULTIPLIER:
    //   = clamp(1.0 + FITNESS_RECOVERY_COEFF × FITNESS_LEVEL, 1.0, 1.5)
    //   = clamp(1.0 + 0.25 × 1.0, 1.0, 1.5) = clamp(1.25, 1.0, 1.5) = 1.25
    static const float FIXED_FITNESS_RECOVERY_MULTIPLIER = 1.25;
    //
    // FIXED_AGE_RECOVERY_MULTIPLIER:
    //   = clamp(1.0 + AGE_RECOVERY_COEFF × (AGE_REFERENCE - CHARACTER_AGE) / AGE_REFERENCE, 0.8, 1.2)
    //   = clamp(1.0 + 0.2 × (30.0 - 22.0) / 30.0, 0.8, 1.2) = clamp(1.0533, 0.8, 1.2) = 1.053
    static const float FIXED_AGE_RECOVERY_MULTIPLIER = 1.053;
    //
    // FIXED_PANDOLF_FITNESS_BONUS:
    //   = 1.0 - 0.2 × FITNESS_LEVEL = 1.0 - 0.2 × 1.0 = 0.80
    //   （Pandolf 公式中训练有素者基础代谢降低20%）
    static const float FIXED_PANDOLF_FITNESS_BONUS = 0.80;
    
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
    static const float FAST_RECOVERY_DURATION_MINUTES = 0.4;  // 快速恢复期（分钟）：缩短至24秒，减少“刚停即猛回”体感
    static const float FAST_RECOVERY_MULTIPLIER = 1.6; // [修复] 与Python一致，从2.5降低到1.6（降低36%）
    
    // 中等恢复期参数
    // 拟真平衡点：平衡快速恢复期和慢速恢复期
    static const float MEDIUM_RECOVERY_START_MINUTES = 0.4;  // 与快速恢复期衔接
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
    
    // ==================== [HARD/SOFT 混合] 动作体力消耗常量 ====================
    // 基于医学研究：跳跃和翻越动作的能量消耗远高于普通移动
    //
    // ── 物理模型公式 ──────────────────────────────────────────────────────────
    //   跳跃: E = (m·g·h + 0.5·m·v²) / eta    单位 J
    //   翻越: P = (m·g·v_vert + eta_iso·F_iso + BASE_METABOLISM) / eta_iso  单位 W
    //   换算: 体力% = J / JUMP_STAMINA_TO_JOULES
    //
    // [HARD] 重力加速度（物理定律）
    static const float JUMP_GRAVITY = 9.81; // m/s²
    
    // [HARD] 1体力单位对应的能量焦耳数
    // 推导：游戏体力池设计为 1.0 ≈ 75 kcal，75 kcal × 4186 J/kcal ≈ 314,000 J
    // 是整个跳跃/翻越物理模型的标尺锚点，不得随意修改
    static const float JUMP_STAMINA_TO_JOULES = 3.14e5; // J / 体力单位
    
    // [HARD] 跳跃肌肉效率（Margaria et al., 1963）
    // 生理学测量值：20-25%，中心值 0.22；Settings 中 jump_efficiency 应保持在此范围内
    // 此处为不经过 Settings 时的硬编码默认值
    static const float JUMP_MUSCLE_EFFICIENCY = 0.22;
    
    // [HARD] 攀爬/翻越等长收缩效率（Margaria et al., 1963）
    // 生理学测量值：10-15%，中心值 0.12；Settings 中 climb_iso_efficiency 应保持在此范围内
    // 此处同时作为 GetClimbIsoEfficiency() 的硬编码回退值
    static const float VAULT_ISO_EFFICIENCY = 0.12;
    
    // [HARD] 翻越垂直抬升估算（m），基于标准障碍高度（0.4-0.6m），取中值 0.5
    static const float VAULT_VERT_LIFT_GUESS = 0.5;
    
    // [HARD] 四肢等长力占总体重的比例，生物力学测量值（上肢负担约为体重的 45-55%）
    static const float VAULT_LIMB_FORCE_RATIO = 0.5;
    
    // [HARD] 攀爬基础代谢附加功率（W），对应轻度静力运动基础代谢（约 40-60W）
    static const float VAULT_BASE_METABOLISM_WATTS = 50.0;
    
    // [HARD] 单次跳跃/翻越最大体力消耗上限，由 JUMP_STAMINA_TO_JOULES 标尺推导的物理边界
    static const float JUMP_VAULT_MAX_DRAIN_CLAMP = 0.15;
    
    // [SOFT] 低体力跳跃阈值：体力 < 10% 时禁用跳跃（肌肉在力竭时无法提供爆发力）
    static const float JUMP_MIN_STAMINA_THRESHOLD = 0.10;
    
    // [SOFT] 连续跳跃时间窗口（秒）：在此时间内的连续跳跃会触发惩罚
    static const float JUMP_CONSECUTIVE_WINDOW = 2.0;
    
    // [SOFT] 连续跳跃惩罚系数：每次连续跳跃额外增加50%消耗（模拟乳酸堆积）
    static const float JUMP_CONSECUTIVE_PENALTY = 0.5;
    
    // [HARD] 跳跃检测垂直速度阈值（由游戏引擎物理参数决定，不可随意修改）
    static const float JUMP_VERTICAL_VELOCITY_THRESHOLD = 2.0; // m/s
    
    // [HARD] 翻越检测垂直速度阈值（由游戏引擎物理参数决定，不可随意修改）
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
    
    // ==================== [SOFT via Settings] Sprint（冲刺）相关参数 ====================
    // 运行时通过 GetSprintSpeedBoost() / GetSprintStaminaDrainMultiplier() 从配置管理器读取
    static const float SPRINT_SPEED_BOOST              = 0.30; // [SOFT fallback] Sprint速度比Run快30%
    static const float SPRINT_MAX_SPEED_MULTIPLIER     = 1.0;  // [HARD] 100%游戏最大速度上限
    static const float SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.5;  // [SOFT fallback] Sprint消耗是Run的3.5×
    
    // ==================== [HARD] Pandolf 能量消耗模型系数（Pandolf et al., 1977 原始值）====================
    // 以下系数均来自 Pandolf et al., 1977 论文的实验测量结果，属于 HARD CONFIG。
    // Python 数字孪生必须与这些值完全同步，否则优化器输出与游戏实际行为将产生系统性偏差。
    //
    //   公式：P = body_weight × [PANDOLF_STATIC_COEFF_1 + PANDOLF_STATIC_COEFF_2×(load/body)]
    //         + body_weight × [PANDOLF_BASE_COEFF×FITNESS + PANDOLF_VELOCITY_COEFF×(v-0.7)²]
    //         + body_weight × grade × [PANDOLF_GRADE_BASE_COEFF + PANDOLF_GRADE_VELOCITY_COEFF×v²]
    //
    static const float PANDOLF_BASE_COEFF               = 2.7;  // [HARD] 基础代谢系数（W/kg），论文原始值
    static const float PANDOLF_VELOCITY_COEFF            = 3.2;  // [HARD] 速度代谢系数（W/kg），论文原始值
    static const float PANDOLF_VELOCITY_OFFSET           = 0.7;  // [HARD] 速度偏移（m/s），论文原始值
    static const float PANDOLF_GRADE_BASE_COEFF          = 0.23; // [HARD] 坡度基础系数（W/kg），论文原始值
    static const float PANDOLF_GRADE_VELOCITY_COEFF      = 1.34; // [HARD] 坡度速度系数（W/kg），论文原始值
    
    // [SOFT fallback] Pandolf 静态站立消耗常量
    // v2.15.0 游戏性调参：降低静态消耗，给30kg负重留出恢复空间
    // 非论文原始值，属游戏平衡性调整
    static const float PANDOLF_STATIC_COEFF_1 = 1.2; // [SOFT fallback] 静态基础系数（原1.5→1.2，-20%）
    static const float PANDOLF_STATIC_COEFF_2 = 1.6; // [SOFT fallback] 静态负重系数（原2.0→1.6，-20%）
    
    // [SOFT fallback] 能量到体力的转换系数
    // 此值由配置管理器动态获取（GetEnergyToStaminaCoeff()），此处仅为代码回退默认值
    static const float ENERGY_TO_STAMINA_COEFF = 0.000015; // [SOFT fallback] 与 Python 数字孪生一致
    
    // [HARD] 参考体重（Pandolf 模型归一化基准，与 CHARACTER_WEIGHT 保持一致）
    static const float REFERENCE_WEIGHT = 90.0; // [HARD] kg
    
    // ==================== [LEGACY - UNUSED] Givoni-Goldman 跑步模型常量 ====================
    // 运行时完全不使用，仅保留以便向后兼容旧配置或历史分析。
    // Python 端对应值为 0.8 / 2.2，两端已对齐。
    static const float GIVONI_CONSTANT          = 0.8; // [LEGACY unused] W/kg·m²/s²
    static const float GIVONI_VELOCITY_EXPONENT = 2.2; // [LEGACY unused] 速度指数
    
    // ==================== [HARD] 地形系数常量（实验测量比值，基准=铺装路面）====================
    // 来自对照实验测量结果，属于 HARD CONFIG，不得随意改变。
    static const float TERRAIN_FACTOR_PAVED = 1.0; // [HARD] 铺装路面（基准）
    static const float TERRAIN_FACTOR_DIRT  = 1.1; // [HARD] 碎石路 +10%
    static const float TERRAIN_FACTOR_GRASS = 1.2; // [HARD] 草地   +20%
    static const float TERRAIN_FACTOR_BRUSH = 1.5; // [HARD] 灌木丛 +50%
    static const float TERRAIN_FACTOR_SAND  = 1.8; // [HARD] 软沙地 +80%
    
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
    // [REMOVED] POSTURE_CROUCH_MULTIPLIER / POSTURE_PRONE_MULTIPLIER 已由 GetPostureCrouchMultiplier()
    // / GetPostureProneMultiplier() 替代，从配置管理器动态读取，static const 版本废弃删除。
    static const float POSTURE_STAND_MULTIPLIER = 1.0; // 站立行走消耗倍数（基准）
    
    // ==================== 游泳体力消耗参数 ====================
    // 生理学依据：游泳的能量消耗远高于陆地移动（约3-5倍）
    // 物理模型：阻力与速度平方成正比 F_d = 0.5 * ρ * v² * C_d * A
    // 参考：Holmér, I. (1974). Energy cost of arm stroke, leg kick, and whole crawl stroke.
    //       Journal of Applied Physiology, 37(5), 762-765.
    //       Pendergast, D. R., et al. (1977). Energy cost of swimming.
    
    // [HARD] 游泳物理模型参数（流体力学常数，不可变）
    static const float SWIMMING_DRAG_COEFFICIENT = 0.5;    // [HARD] 阻力系数 C_d（标准近似值）
    static const float SWIMMING_WATER_DENSITY    = 1000.0; // [HARD] 水密度 ρ（kg/m³，物理定律）
    static const float SWIMMING_FRONTAL_AREA     = 0.5;    // [HARD] 正面受力面积（m²，上身投影标准近似）
    static const float SWIMMING_BASE_POWER       = 20.0;   // [SOFT fallback] 基础游泳功率（W），可通过 swimming_base_power 覆盖
    
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
    
    // ==================== [HARD/SOFT 混合] 环境因子常量 ====================
    // 时间窗口为 HARD（太阳辐射物理规律）；强度倍数为 SOFT（通过配置管理器读取）
    static const float ENV_HEAT_STRESS_START_HOUR      = 10.0; // [HARD] 热应激开始（日照曲线）
    static const float ENV_HEAT_STRESS_PEAK_HOUR       = 14.0; // [HARD] 峰值（地表温度滞后约2h）
    static const float ENV_HEAT_STRESS_END_HOUR        = 18.0; // [HARD] 热应激结束
    static const float ENV_HEAT_STRESS_MAX_MULTIPLIER  = 1.5;  // [SOFT fallback] 最大倍数，可通过 env_heat_stress_max_multiplier 覆盖
    static const float ENV_HEAT_STRESS_BASE_MULTIPLIER = 1.0;  // [HARD] 基准倍数（无热影响）
    static const float ENV_HEAT_STRESS_INDOOR_REDUCTION = 0.5; // [SOFT fallback] 室内减免比例
    
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
    static const float ENV_TEMPERATURE_HEAT_THRESHOLD       = 30.0; // [HARD] °C，医学热应激阈值
    static const float ENV_TEMPERATURE_HEAT_PENALTY_COEFF   = 0.02; // [SOFT fallback] 每高1°C恢复率降低2%
    static const float ENV_TEMPERATURE_COLD_THRESHOLD       =  0.0; // [HARD] °C，医学冷应激阈值
    static const float ENV_TEMPERATURE_COLD_STATIC_PENALTY  = 0.03; // [SOFT fallback] 低温静态消耗增加
    static const float ENV_TEMPERATURE_COLD_RECOVERY_PENALTY = 0.05; // [SOFT fallback] 低温恢复率惩罚
    
    // 地表湿度相关常量
    static const float ENV_SURFACE_WETNESS_SOAK_RATE = 1.0; // kg/秒，趴下时的湿重增加速率
    static const float ENV_SURFACE_WETNESS_THRESHOLD = 0.1; // 积水阈值（高于此值触发湿重增加）
    static const float ENV_SURFACE_WETNESS_MARGINAL_DECAY_ADVANCE = 0.1; // 边际效应衰减提前触发比例
    static const float ENV_SURFACE_WETNESS_PRONE_PENALTY = 0.15; // 湿地趴下时的恢复惩罚
    
    // ==================== 配置系统桥接方法 ====================
    
    // 获取能量到体力转换系数（从配置管理器）
    // [修复 v2.16.0] 降低最小值至 1e-8：优化器产出约 8.9e-7，此前 1e-6 的截断导致游戏实际消耗比
    // 优化器模拟值高 +12%（1e-6 / 8.9e-7 ≈ 1.12）。温度单位 bug 修复后此保护不再必要。
    static const float ENERGY_TO_STAMINA_COEFF_MIN = 0.00000001;  // 1e-08，仅防止零值或负值
    static float GetEnergyToStaminaCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
            {
                float coeff = params.energy_to_stamina_coeff;
                return Math.Max(coeff, ENERGY_TO_STAMINA_COEFF_MIN);
            }
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

    // 获取负重速度惩罚指数（从配置管理器）
    static float GetEncumbranceSpeedPenaltyExponent()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_speed_penalty_exponent;
        }
        return 1.5; // 默认值
    }

    // 获取负重速度惩罚上限（从配置管理器）
    static float GetEncumbranceSpeedPenaltyMax()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_speed_penalty_max;
        }
        return 0.75; // 默认值
    }
    
    // 获取Sprint体力消耗倍数（从配置管理器）
    // 确保不低于1.0，避免为0导致体力不消耗
    static float GetSprintStaminaDrainMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
            {
                float v = params.sprint_stamina_drain_multiplier;
                return Math.Max(v, 1.0);
            }
        }
        return 3.5; // [FIX] 默认值与 SPRINT_STAMINA_DRAIN_MULTIPLIER static const 统一为 3.5
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

    // 获取最低体力阈值（从配置管理器）
    // [修复 v2.16.0] 此前为硬编码 static const 0.2，JSON 配置值被完全忽略
    static float GetMinRecoveryStaminaThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Clamp(params.min_recovery_stamina_threshold, 0.0, 0.5);
        }
        return MIN_RECOVERY_STAMINA_THRESHOLD; // 回退到硬编码默认值 0.2
    }

    // 获取最低体力时所需静止时间（从配置管理器）
    // [修复 v2.16.0] 此前为硬编码 static const 3.0s，JSON 配置值（6.98s~16.44s）被完全忽略，
    // 导致游戏恢复门控比优化器预期宽松 2~5 倍。
    static float GetMinRecoveryRestTimeSeconds()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Max(params.min_recovery_rest_time_seconds, 0.0);
        }
        return MIN_RECOVERY_REST_TIME_SECONDS; // 回退到硬编码默认值 3.0s
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


    // 是否使用物理模型计算跳跃消耗

    // 跳跃肌肉效率
    static float GetJumpEfficiency()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            // [修复 v2.17.0] params.jump_efficiency 若为0（未经 embed 写入时的运行时默认值），
            // [Attribute defvalue] 仅 Workbench 编辑器生效，代码 new SCR_RSS_Params() 不触发，
            // 导致 eta=0 -> Math.Max(0,0.01)=0.01 -> 消耗×22 -> 触发15%上限。
            // 生理学约束 Margaria 1963: 20-25%；低于 0.15 视为未初始化，回退 HARD 默认值。
            if (params && params.jump_efficiency >= 0.15)
                return Math.Clamp(params.jump_efficiency, 0.15, 0.30);
        }
        return JUMP_MUSCLE_EFFICIENCY; // [HARD fallback] 0.22
    }

    // 跳跃重心抬升高度猜测
    static float GetJumpHeightGuess()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.jump_height_guess;
        }
        return 0.5; // default 0.5 m
    }

    // 跳跃水平速度猜测
    static float GetJumpHorizSpeedGuess()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.jump_horizontal_speed_guess;
        }
        return 0.0;
    }

    // 获取攀爬/翻越等长收缩效率（从配置管理器）
    // [NOTE] 此值具有生理学约束（Margaria 1963: 0.10-0.15），不建议超出此范围
    // Settings 中 climb_iso_efficiency 的 defvalue 为 0.12（论文中心值）
    static float GetClimbIsoEfficiency()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Clamp(params.climb_iso_efficiency, 0.05, 0.25); // 生理学安全范围限制
        }
        return VAULT_ISO_EFFICIENCY; // 硬编码回退值 0.12
    }

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

    // 获取降雨单独触发的湿重上限（从配置管理器）
    // [NOTE] 降雨湿重上限应低于游泳湿重（降雨无法像全身浸泡那样快速浸透装备）
    // 现实参考：军事BDU+战术背心在持续暴雨下约吸水 3-5 kg；NATO研究全套装备游泳约 5.7 kg
    // 因此降雨单独上限设 4-5 kg，游泳可达 10 kg，组合池上限仍为 10 kg
    static float GetEnvRainWeightMax()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.env_rain_weight_max >= 1.0)
                return params.env_rain_weight_max;
        }
        return 5.0; // 硬编码回退值（暴雨下约 5 kg，保守估计）
    }

    // 获取调试状态的快捷静态方法
    static bool IsDebugEnabled()
    {
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
    
    // 获取调试信息刷新频率（默认 1 秒，统一波次输出）
    static int GetDebugUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iDebugUpdateInterval;
        
        return 1000; // 默认 1 秒
    }

    // ==================== 统一调试批次（每秒一波次）====================
    protected static float s_fNextDebugBatchTime = 0.0;
    protected static bool s_bDebugBatchActive = false;
    protected static ref array<string> s_aDebugBatchLines = null;

    // 启动本秒的调试批次，返回是否应输出（每秒一次）
    static bool StartDebugBatch()
    {
        if (!IsDebugEnabled())
            return false;
        World world = GetGame().GetWorld();
        if (!world)
            return false;
        float t = world.GetWorldTime() / 1000.0;
        float interval = GetDebugUpdateInterval() / 1000.0;
        if (interval <= 0.0)
            interval = 1.0;
        if (t < s_fNextDebugBatchTime)
            return false;
        s_fNextDebugBatchTime = t + interval;
        s_bDebugBatchActive = true;
        s_bTempStepAddedThisBatch = false;
        s_bEngineTODAddedThisBatch = false;
        if (!s_aDebugBatchLines)
            s_aDebugBatchLines = new array<string>();
        s_aDebugBatchLines.Clear();
        return true;
    }

    // 添加一行到当前批次（需先调用 StartDebugBatch 启动批次）
    static void AddDebugBatchLine(string line)
    {
        if (!s_bDebugBatchActive || !s_aDebugBatchLines)
            return;
        s_aDebugBatchLines.Insert(line);
    }

    // 每批次仅添加一次（用于 TempStep、EngineTOD 等可能被多次调用的输出）
    protected static bool s_bTempStepAddedThisBatch = false;
    protected static bool s_bEngineTODAddedThisBatch = false;

    static void AddDebugBatchLineOnce(string tag, string line)
    {
        if (!s_bDebugBatchActive || !s_aDebugBatchLines)
            return;
        if (tag == "TempStep" && s_bTempStepAddedThisBatch)
            return;
        if (tag == "EngineTOD" && s_bEngineTODAddedThisBatch)
            return;
        s_aDebugBatchLines.Insert(line);
        if (tag == "TempStep")
            s_bTempStepAddedThisBatch = true;
        if (tag == "EngineTOD")
            s_bEngineTODAddedThisBatch = true;
    }

    // 当前是否处于调试批次窗口内
    static bool IsDebugBatchActive()
    {
        return s_bDebugBatchActive;
    }

    protected static float s_fLastBatchFlushTime = -999.0;

    // 在帧末刷新批次：输出所有累积行并清空
    static void FlushDebugBatch()
    {
        if (!s_bDebugBatchActive || !s_aDebugBatchLines || s_aDebugBatchLines.Count() == 0)
            return;
        s_bDebugBatchActive = false;
        World world = GetGame().GetWorld();
        if (world)
            s_fLastBatchFlushTime = world.GetWorldTime() / 1000.0;
        for (int i = 0; i < s_aDebugBatchLines.Count(); i++)
            Print(s_aDebugBatchLines.Get(i));
        s_aDebugBatchLines.Clear();
    }

    // 本轮秒内是否刚刷新过批次（用于避免 status 重复）
    static bool WasBatchJustFlushed()
    {
        World world = GetGame().GetWorld();
        if (!world)
            return false;
        float t = world.GetWorldTime() / 1000.0;
        return (t - s_fLastBatchFlushTime) < 0.5;
    }

    // 统一调试日志节流（基于 DebugUpdateInterval）
    static bool ShouldLog(inout float nextTime)
    {
        if (!IsDebugEnabled())
            return false;

        return ShouldLogInternal(nextTime);
    }

    // 统一详细日志节流（需要 Debug + Verbose）
    static bool ShouldVerboseLog(inout float nextTime)
    {
        if (!IsDebugEnabled() || !IsVerboseLoggingEnabled())
            return false;

        return ShouldLogInternal(nextTime);
    }

    // 内部时间节流实现
    protected static bool ShouldLogInternal(inout float nextTime)
    {
        World world = GetGame().GetWorld();
        if (!world)
            return false;

        float currentTime = world.GetWorldTime() / 1000.0;
        float interval = GetDebugUpdateInterval() / 1000.0;
        if (interval <= 0.0)
            interval = 5.0;

        if (currentTime < nextTime)
            return false;

        nextTime = currentTime + interval;
        return true;
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
    // [v2.17.0] 0.3→0.5：加快衰减，约2秒内恢复到接近0，减少惩罚持续时间
    static const float STANCE_FATIGUE_DECAY = 0.5; // 每秒衰减 0.5 疲劳值
    
    // 疲劳堆积最大值（防止无限累积）
    // [v2.17.0] 10.0→3.0：稳态连续切换上限 ×4，35kg 趴→站最差约 8.3%，避免无限叠加
    static const float STANCE_FATIGUE_MAX = 3.0; // 最大疲劳堆积值
    
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