// 体力系统常量定义模块
// 模块化拆分：常量集中定义，运行时回退值经 SCR_RSS_ConfigBridge 读取 JSON 预设
//
// HARD CONFIG: 基于科学论文/物理定律，不得修改
// SOFT CONFIG (fallback): 游戏性参数，运行时通过 SCR_RSS_ConfigBridge
//   从 SCR_RSS_Params (来自 JSON) 读取。此处 static const 仅作为
//   ConfigBridge 返回 -1 (未设置) 或 Debug 环境的回退默认值。
//   修改这些值不会影响运行时行为——必须修改 JSON 预设或 Settings 源码。

class SCR_RSS_Constants
{
    // 0kg 下实测：Sprint 最大 5.5 m/s，Run 最大 3.8 m/s
    static const float GAME_MAX_SPEED = 5.5; // m/s（0kg 冲刺最大速度）

    // false：启用上述 RSS 自定义表现。体力与速度等数值逻辑与此开关无关。
    static const bool RSS_PRESENTATION_NATIVE_ONLY = true;
    
    // 基于速度阈值的分段消耗率系统 - 以下阈值为游戏设计参数，可通过预设间接影响
    // 参考：Military Stamina System Model (90kg Male / 22yo)
    
    // 速度阈值（m/s）- 0kg 下实测 Sprint 5.5 / Run 3.8
    // 注意：这些值定义引擎移动阶段的切换点，并非实际步行速度。
    // Walk 阈值 3.2 m/s ≠ 步行速度（~1.2-1.5 m/s），而是引擎阶段编号的边界。
    static const float SPRINT_VELOCITY_THRESHOLD = 5.5; // @fallback m/s，Sprint速度阈值
    static const float RUN_VELOCITY_THRESHOLD = 3.8; // @fallback m/s，Run速度阈值
    static const float WALK_VELOCITY_THRESHOLD = 3.2; // @fallback m/s，Walk移动阶段阈值（非步行速度）
    
    // 基于负重的动态速度阈值（m/s）
    static const float RECOVERY_THRESHOLD_NO_LOAD = 2.5; // @fallback m/s，空载时恢复体力阈值
    static const float DRAIN_THRESHOLD_COMBAT_LOAD = 1.5; // @fallback m/s，负重30kg时开始消耗体力的阈值
    static const float COMBAT_LOAD_WEIGHT = 30.0; // @fallback kg，战斗负重（用于计算动态阈值）
    
    // 静止恢复（phase==0 回退路径）；Walk/Run/Sprint 消耗已统一走 Pandolf 公式
    static const float REST_RECOVERY_RATE = 0.250; // @fallback pts/s（Rest，恢复）
    static const float REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2; // @fallback 每0.2秒
    
    // 初始体力状态（满值）
    static const float INITIAL_STAMINA_AFTER_ACFT = 1.0; // 100%，满值
    
    // 精疲力尽阈值
    static const float EXHAUSTION_THRESHOLD = 0.0; // 0.0（0点）
    static const float EXHAUSTION_LIMP_SPEED = 1.0; // @fallback m/s（跛行速度）
    static const float SPRINT_ENABLE_THRESHOLD = 0.25; // @fallback 体力≥25%时才能Sprint；疲劳时肌肉无法爆发冲刺（原0.18）
    static const float WALK_RECOVERY_ZONE_THRESHOLD = 0.15; // 体力<15%时步行/慢跑转为恢复
    static const float WALK_RECOVERY_ZONE_PER_TICK = 0.002; // 低体力区域每0.2s恢复0.2%（即每秒1%）
    static const float WALK_RECOVERY_ZONE_RATE = WALK_RECOVERY_ZONE_PER_TICK; // @deprecated 请用 WALK_RECOVERY_ZONE_PER_TICK
    
    // 坡度修正系数
    // @deprecated v4 Pandolf 坡度系数；v6 坡度由 MetabolismModel/Tobler 处理，零引用保留兼容
    static const float GRADE_UPHILL_COEFF = 0.12; // 每1%上坡增加12%消耗
    static const float GRADE_DOWNHILL_COEFF = 0.05; // 每1%下坡减少5%消耗（假设）
    static const float HIGH_GRADE_THRESHOLD = 15.0; // 15%（高坡度阈值）
    static const float HIGH_GRADE_MULTIPLIER = 1.2; // 高坡度额外1.2×乘数
    
    // 目标平均速度（m/s）- 基于用户需求：2英里在15分27秒内完成
    // 2英里 ≈ 3218.7米，时间927秒，平均速度 ≈ 3.47 m/s
    static const float TARGET_AVERAGE_SPEED = 3.47; // m/s
    
    // 目标Run速度（m/s）- 0kg 下实测 Run 最大 3.8 m/s
    static const float TARGET_RUN_SPEED = 3.8; // m/s
    static const float TARGET_RUN_SPEED_MULTIPLIER = TARGET_RUN_SPEED / GAME_MAX_SPEED;
    
    // 意志力平台期阈值（体力百分比）
    // 体力高于此值时，保持恒定目标速度（模拟意志力克服早期疲劳）
    static const float WILLPOWER_THRESHOLD = 0.35; // 35% — Hardcore：疲劳感更早体现（原25%）
    
    // 平滑过渡起点（体力百分比）
    // 在35%-5%之间使用平滑过渡，避免突兀的"撞墙"效果
    // 将35%设为"疲劳临界区"的起点（原25%，Hardcore提至35%）
    static const float SMOOTH_TRANSITION_START = 0.35; // 35%（疲劳临界区起点，原25%）— Hardcore：疲劳过渡区从35%体力开始
    static const float SMOOTH_TRANSITION_END = 0.05; // 5%，平滑过渡结束点（真正的力竭点）
    
    // 跛行速度倍数（最低速度）
    static const float MIN_LIMP_SPEED_MULTIPLIER = 1.0 / GAME_MAX_SPEED;
    
    
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
    static const float ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.28; // [SOFT fallback] Hardcore：负重速度惩罚更强（原0.20）
    static const float ENCUMBRANCE_SPEED_EXPONENT = 1.5; // [SOFT fallback]
    
    // 最小速度倍数（防止体力完全耗尽时完全无法移动）
    static const float MIN_SPEED_MULTIPLIER = 0.15; // 15%最低速度（约0.78 m/s，勉强行走）
    
    // 最大速度倍数限制（防止超过游戏引擎限制）
    static const float MAX_SPEED_MULTIPLIER = 1.0; // 100%最高速度（不超过游戏最大速度）
    
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
    
    // 游戏运行时直接使用这些常量，不再执行动态公式，确保所有玩家完全相同。
    // 修改上方任意角色属性后，必须同步更新这里的预计算值！
    //
    // FIXED_FITNESS_EFFICIENCY_FACTOR:
    static const float FIXED_FITNESS_EFFICIENCY_FACTOR = 0.70;
    //
    // FIXED_FITNESS_RECOVERY_MULTIPLIER:
    static const float FIXED_FITNESS_RECOVERY_MULTIPLIER = 1.25;
    //
    // FIXED_AGE_RECOVERY_MULTIPLIER:
    static const float FIXED_AGE_RECOVERY_MULTIPLIER = 1.053;
    //
    // FIXED_PANDOLF_FITNESS_BONUS:
    //   = 1.0 - 0.2 × FITNESS_LEVEL = 1.0 - 0.2 × 1.0 = 0.80
    //   （Pandolf 公式中训练有素者基础代谢降低20%）
    static const float FIXED_PANDOLF_FITNESS_BONUS = 0.80;
    
    // 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
    // 核心概念：从"净增加"改为"代谢净值"
    // 最终恢复率 = (基础恢复率 * 姿态修正) - (负重压制 + 氧债惩罚)
    
    // 基础恢复率（每0.2秒，在50%体力时的恢复率）
    // [修复] 与Python数字孪生保持一致，从0.00035降低到0.00015（降低57%）
    // 计算逻辑：1.0(体力) / 1333秒(22.2分钟) / 5(每秒tick数) = 0.00015
    // 注意：此值现在从配置管理器获取（GetBaseRecoveryRate()）
    // v4 optimizer: aligned with EliteStandard preset (2026-05)
    static const float BASE_RECOVERY_RATE = 0.00010; // 每0.2秒恢复0.01% — Hardcore：基础恢复降低35%（原0.000153）
    
    // 恢复率非线性系数（基于当前体力百分比）
    static const float RECOVERY_NONLINEAR_COEFF = 0.5; // Hardcore：降低非线性恢复加成（原0.792）
    
    // 快速恢复期参数（刚停止运动后的快速恢复阶段）
    // 拟真平衡点：模拟"喘匀第一口氧气"
    // 生理学上，氧债的50%是在停止运动后的前30-60秒内偿还的
    // 模拟停止运动后前90秒的高效恢复
    static const float FAST_RECOVERY_DURATION_MINUTES = 0.4;  // 快速恢复期（分钟）：缩短至24秒，减少“刚停即猛回”体感
    static const float FAST_RECOVERY_MULTIPLIER = 1.6; // Hardcore：快速恢复期仅1.6×（原2.395）
    
    // 中等恢复期参数
    // 拟真平衡点：平衡快速恢复期和慢速恢复期
    static const float MEDIUM_RECOVERY_START_MINUTES = 0.4;  // 与快速恢复期衔接
    // [修复] 与Python数字孪生保持一致，从8.5改为5.0
    static const float MEDIUM_RECOVERY_DURATION_MINUTES = 5.0; // 中等恢复期持续时间（分钟）
    static const float MEDIUM_RECOVERY_MULTIPLIER = 1.0; // Hardcore：中等恢复期无加成（原1.137）
    
    // 慢速恢复期参数（长时间休息后的慢速恢复阶段）
    // 生理学依据：休息超过10分钟后，恢复速度逐渐减慢（接近上限时恢复变慢）
    // 优化：提升到0.8倍（从0.7倍）
    static const float SLOW_RECOVERY_START_MINUTES = 10.0; // 慢速恢复期开始时间（分钟）
    static const float SLOW_RECOVERY_MULTIPLIER = 0.35; // Hardcore：慢速恢复期仅0.35×（原0.476）
    
    // 年龄对恢复速度的影响系数
    static const float AGE_RECOVERY_COEFF = 0.2; // 年龄恢复系数
    static const float AGE_REFERENCE = 30.0; // 年龄参考值（30岁为标准）
    
    // 累积疲劳恢复参数
    // 游戏性优化：大幅降低疲劳惩罚（从0.15降到0.05），减少负重对恢复的惩罚感
    static const float FATIGUE_RECOVERY_PENALTY = 0.05; // 疲劳恢复惩罚系数（最大5%恢复速度减少）
    static const float FATIGUE_RECOVERY_DURATION_MINUTES = 20.0; // 疲劳完全恢复所需时间（分钟）
    
    // 深度生理压制：趴下不只是为了隐蔽，更是为了让心脏负荷最小化
    // 姿态加成设定的更有体感，但不过分
    // 站姿：确保能够覆盖静态站立消耗
    // 蹲姿：减少下肢肌肉紧张，+40%恢复速度
    // 趴姿：全身放松，最大化血液循环，+60%恢复速度
    // 逻辑：趴下是唯一的快速回血手段（重力分布均匀），强迫重装兵必须趴下
    static const float STANDING_RECOVERY_MULTIPLIER = 0.85; // Hardcore：站姿无恢复加成，反而有轻微惩罚（原1.103）
    static const float CROUCHING_RECOVERY_MULTIPLIER = 1.6; // Hardcore：蹲姿恢复1.6×（原1.5 fallback）
    static const float PRONE_RECOVERY_MULTIPLIER = 1.9; // Hardcore：趴姿恢复降至1.9×（原2.344）
    
    // 医学解释：背负30kg装备站立时，斜方肌、腰椎和下肢肌肉仍在进行高强度静力收缩
    // 这种收缩产生的代谢废物（乳酸）排放速度远慢于空载状态
    // 数学实现：恢复率与负重百分比（BM%）挂钩
    // Penalty = (当前总重 / 身体耐受基准)^2 * 0.0004
    // 结果：穿着40kg装备站立恢复时，原本100%的基础恢复会被扣除70%
    // 战术意图：强迫重装兵必须趴下（通过姿态加成抵消负重扣除），否则回血极慢
    static const float LOAD_RECOVERY_PENALTY_COEFF = 0.0002; // Hardcore：负重恢复惩罚翻倍（原0.0001→0.0002）
    static const float LOAD_RECOVERY_PENALTY_EXPONENT = 2.0; // 负重恢复惩罚指数（2.0 = 平方）
    static const float BODY_TOLERANCE_BASE = 90.0; // [修复] 身体耐受基准（kg），从 30.0 增加到 90.0（参考体重）
    
    // 数学实现：当体力>80%时，恢复率 = 原始恢复率 * (1.1 - 当前体力百分比)
    // 结果：最后10%的体力恢复速度会比前50%慢3-4倍
    // 战术意图：玩家经常会处于80%-90%的"亚健康"状态，只有长时间修整才能真正满血
    static const float MARGINAL_DECAY_THRESHOLD = 0.8; // 边际效应衰减阈值（80%体力）
    static const float MARGINAL_DECAY_COEFF = 1.1; // 边际效应衰减系数
    
    // 医学解释：当体力过低时，身体处于极度疲劳状态，需要更长时间的休息才能开始恢复
    // 数学实现：体力低于20%时，必须处于静止状态10秒后才允许开始回血
    // 战术意图：防止玩家在极度疲劳时通过"跑两步停一下"的方式快速回血
    // [修复 v3.6.1] 将极度疲劳惩罚时间从 10秒 缩短为 3秒
    // 模拟深呼吸 3 秒后即可开始恢复，而不是发呆 10 秒
    static const float MIN_RECOVERY_STAMINA_THRESHOLD = 0.2; // 最低体力阈值（20%）
    static const float MIN_RECOVERY_REST_TIME_SECONDS = 5.0; // Hardcore：低体力需静止5秒才能开始恢复（原3.0）
    
    // 基于个性化运动建模（Palumbo et al., 2018）
    static const float FATIGUE_ACCUMULATION_COEFF = 0.025; // Hardcore：每分钟增加2.5%消耗（原1.5%）
    static const float FATIGUE_START_TIME_MINUTES = 5.0; // 疲劳开始累积的时间（分钟），前5分钟无疲劳累积
    static const float FATIGUE_MAX_FACTOR = 2.5; // Hardcore：最大疲劳因子2.5×（原2.0）
    
    // 基于个性化运动建模（Palumbo et al., 2018）
    static const float AEROBIC_THRESHOLD = 0.6; // 有氧阈值（60% VO2max）
    static const float ANAEROBIC_THRESHOLD = 0.8; // 无氧阈值（80% VO2max）
    static const float AEROBIC_EFFICIENCY_FACTOR = 0.9; // 有氧区效率因子（90%，更高效）
    static const float MIXED_EFFICIENCY_FACTOR = 1.0; // 混合区效率因子（100%，标准）
    static const float ANAEROBIC_EFFICIENCY_FACTOR = 1.2; // 无氧区效率因子（120%，低效但高功率）
    
    // 基准负重（kg）- 角色携带的基础物品重量
    static const float BASE_WEIGHT = 1.36; // kg
    
    // 最大负重（kg）- 角色可以携带的最大重量（包含基准负重）
    static const float MAX_ENCUMBRANCE_WEIGHT = 40.5; // kg
    
    // 战斗负重（kg）— 与 COMBAT_LOAD_WEIGHT 同义，保留别名供 HUD/百分比计算
    static const float COMBAT_ENCUMBRANCE_WEIGHT = COMBAT_LOAD_WEIGHT;
    
    // 基于医学研究：跳跃和翻越动作的能量消耗远高于普通移动
    //
    //   跳跃: E = (m·g·h + 0.5·m·v²) / eta    单位 J
    //   换算: 体力% = J / JUMP_STAMINA_TO_JOULES
    //
    // [HARD] 重力加速度（物理定律）
    static const float JUMP_GRAVITY = 9.81; // m/s²
    
    // [HARD] 1体力单位对应的能量焦耳数
    // 是整个跳跃/翻越物理模型的标尺锚点，不得随意修改
    static const float JUMP_STAMINA_TO_JOULES = 3.14e5; // J / 体力单位
    
    // [HARD] 跳跃肌肉效率（Margaria et al., 1963）
    // 此处为不经过 Settings 时的硬编码默认值
    static const float JUMP_MUSCLE_EFFICIENCY = 0.22;
    
    // [HARD] 攀爬/翻越等长收缩效率（Margaria et al., 1963）
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
    
    // [SOFT] 跳跃冷却时间（秒）：使用 GetWorldTime 时间戳判定，不依赖更新频率
    static const float JUMP_COOLDOWN_SEC = 2.0;
    
    // [SOFT] 翻越冷却时间（秒）：使用 GetWorldTime 时间戳判定，不依赖更新频率
    static const float VAULT_COOLDOWN_SEC = 5.0;
    
    // [SOFT] 持续攀爬额外消耗间隔（秒）：每隔此时间追加一次消耗
    static const float VAULT_CONTINUOUS_CLIMB_INTERVAL_SEC = 1.0;
    
    // [HARD] 跳跃检测垂直速度阈值（由游戏引擎物理参数决定，不可随意修改）
    static const float JUMP_VERTICAL_VELOCITY_THRESHOLD = 2.0; // m/s
    
    // [HARD] 翻越检测垂直速度阈值（由游戏引擎物理参数决定，不可随意修改）
    static const float VAULT_VERTICAL_VELOCITY_THRESHOLD = 1.5; // m/s
    
    // 基于 Pandolf 模型：坡度对能量消耗的影响
    static const float SLOPE_UPHILL_COEFF = 0.08; // 上坡影响系数（每度增加8%消耗）
    static const float SLOPE_DOWNHILL_COEFF = 0.03; // 下坡影响系数（每度减少3%消耗，约为上坡的1/3）
    static const float SLOPE_MAX_MULTIPLIER = 2.0; // 最大坡度影响倍数（上坡）
    static const float SLOPE_MIN_MULTIPLIER = 0.7; // 最小坡度影响倍数（下坡）
    // 坡度-消耗生理学修正（Margaria/Santee：缓下坡最省能，陡下坡刹车耗能）
    static const float GENTLE_DOWNHILL_GRADE_MAX = 12.0;       // 缓下坡上限（|坡度%|），约 -9%～-12% 为能耗最低
    static const float GENTLE_DOWNHILL_SAVINGS_MULTIPLIER = 1.25; // 缓下坡时坡度项“省能”放大（1.25 = 多 25% 节省）
    static const float STEEP_DOWNHILL_GRADE_THRESHOLD = 15.0;  // 超过此 |坡度%| 开始加陡下坡刹车惩罚
    static const float STEEP_DOWNHILL_PENALTY_MAX_FRACTION = 0.5; // 陡下坡惩罚上限（相对 baseTerm 的比例，-30% 坡度时约达此值）
    
    static const float ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15; // 负重×坡度交互系数
    
    static const float SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10; // 速度×负重×坡度交互系数
    
    static const float SPRINT_SPEED_BOOST              = 0.2561103503743016; // [SOFT fallback] v4 EliteStandard (2026-05)
    static const float SPRINT_MAX_SPEED_MULTIPLIER     = 1.0;  // [HARD] 100%游戏最大速度上限
    // 战术冲刺爆发时长：Sprint 前 N 秒内减轻负重对速度的惩罚，实现短时全速爆发体感
    static const float TACTICAL_SPRINT_BURST_DURATION = 8.0;   // 秒
    static const float TACTICAL_SPRINT_BURST_BUFFER_DURATION = 5.0; // 爆发结束后 5 秒缓冲区，负重惩罚从爆发系数线性过渡到 1.0，再进入平稳期
    // 爆发期负重惩罚乘数：1.0 = 不减免（对齐 21.6 kg / 30 m 冲刺文献）；旧值 0.2 会使短冲刺几乎无负重感
    static const float TACTICAL_SPRINT_BURST_ENCUMBRANCE_FACTOR = 1.0;
    static const float TACTICAL_SPRINT_COOLDOWN = 15.0;        // 爆发+缓冲区结束或松开冲刺后冷却秒数，期间再次冲刺直接进入平稳期，防止利用机制
    static const float INDOOR_STAIRS_ENCUMBRANCE_SPEED_FACTOR = 0.4; // 楼梯上负重速度惩罚乘数（0.4 = 降至40%）
    // 上坡/下坡速度倍率（在 Tobler 结果上再乘，玩家反馈：上坡再快一点、下坡更快更省力）
    static const float UPHILL_SPEED_BOOST = 1.15;   // 上坡目标速度倍率（1.15 = 提高约15%）
    static const float DOWNHILL_SPEED_BOOST = 1.15; // 下坡目标速度倍率
    static const float DOWNHILL_SPEED_MAX_MULTIPLIER = 1.25; // 下坡速度倍率上限（对应约6.5 m/s，避免数值爆炸）
    
    // 通过镜头表现“负重感”和“疲劳感”，不依赖 UI。仅在第一人称下生效。
    // 起步/急停惯性
    static const float CAM_INERTIA_START_LAG_DURATION = 0.25;   // 起步滞后时长（秒），重装时按下 W 后镜头前倾+延迟感
    static const float CAM_INERTIA_START_TILT_DEG = 2.0;         // 起步时镜头前倾角度（度）
    static const float CAM_INERTIA_DECEL_OVERSHOOT_DURATION = 0.4; // 急停“前冲”持续时间（秒）
    static const float CAM_INERTIA_DECEL_OVERSHOOT_FORWARD_M = 0.06; // 急停时镜头向前多冲出的距离（米），最大负载时达到此值
    static const float CAM_INERTIA_OVERSHOOT_LOAD_MIN_KG = 10.0;  // 急停过冲曲线：低于此负重（kg）几乎无过冲
    static const float CAM_INERTIA_OVERSHOOT_LOAD_MAX_KG = 40.0;  // 急停过冲曲线：达到此负重时过冲为最大值
    static const float CAM_INERTIA_OVERSHOOT_EXPONENT = 1.2;       // 过冲随负重的非线性指数（1.0–1.2 减轻轻/重装手感差异）
    static const float CAM_INERTIA_ENCUMBRANCE_THRESHOLD = 0.25; // 负重占比超过此值（相对 BODY_TOLERANCE）才开始明显惯性
    // 步伐垂直颠簸（与负重、坡度关联）
    static const float CAM_BOB_VERTICAL_AMPLITUDE_BASE = 0.008;  // 基础垂直颠簸幅度（米），负重/疲劳时加深
    static const float CAM_BOB_VERTICAL_FREQ_BASE = 1.8;        // 基础步频（Hz），负重时变沉（频率略降）
    static const float CAM_BOB_ENCUMBRANCE_SCALE = 2.0;         // 负重对颠簸幅度的缩放（0~1 负重 * scale 加到幅度）
    static const float CAM_BOB_STAMINA_SCALE = 0.5;             // 低体力时额外加深颠簸（(1-stamina)*scale）
    // 上坡时左右摇摆（保持平衡感）
    static const float CAM_BOB_UPHILL_SWAY_AMPLITUDE = 0.012;   // 上坡左右摇摆幅度（米）
    static const float CAM_BOB_UPHILL_SWAY_FREQ = 0.6;           // 低频（Hz）
    static const float CAM_BOB_UPHILL_SLOPE_DEG_MIN = 8.0;      // 坡度超过此度数才启用上坡摇摆（度）
    // 生产稳定值 1.0。曾用于调试（3.0-5.0 放大观察效果），确认正常后固定在此值。
    // 如果需要检查镜头惯性/颠簸是否生效，可临时调高，记得改回。
    static const float CAM_DEBUG_STRENGTH = 1;
    // 冲刺时视野：与战术爆发联动（Burst +5° / Cruise +2°），疲劳/Limp 时收窄（-2°）
    static const float CAM_SPRINT_FOV_BURST_DEG = 5.0;   // 爆发期 (0~8s) FOV 加宽
    static const float CAM_SPRINT_FOV_CRUISE_DEG = 3.0;   // 稳定期/巡航期 (8s 后) FOV 轻度加宽
    static const float CAM_SPRINT_FOV_LIMP_DEG = -2.0;    // 疲劳/Limp (体力<阈值) FOV 略收窄
    static const float CAM_SPRINT_FOV_LIMP_STAMINA_THRESHOLD = 0.2; // 低于此体力视为 Limp，应用 FOV 收窄
    static const float CAM_SPRINT_FOV_BONUS_DEG = 5.0;    // [保留] 兼容旧逻辑，实际以 BURST/CRUISE/LIMP 为准
    static const float CAM_SPRINT_FOV_BLEND_UP_SEC = 0.2;   // 进入冲刺时 FOV 过渡时长（秒）
    static const float CAM_SPRINT_FOV_BLEND_DOWN_SEC = 0.25; // 退出冲刺时 FOV 过渡时长（秒）
    static const float CAM_SPRINT_FOV_MAX_RATE_DEG_PER_SEC = 8.0; // FOV 加成变化速率上限（度/秒）；略降以减轻边缘「抽动感」
    // 体力在阈值附近复制抖动时，跛行 FOV 会反复开关导致屏幕边缘闪烁；用滞回 + 目标平滑抑制
    static const float CAM_SPRINT_FOV_LIMP_HYST_STAMINA_LOW = 0.17;  // 低于此进入跛行 FOV
    static const float CAM_SPRINT_FOV_LIMP_HYST_STAMINA_HIGH = 0.24; // 高于此退出跛行 FOV
    static const float CAM_SPRINT_FOV_TARGET_SMOOTH_TAU_SEC = 0.14;   // 冲刺 FOV 目标指数平滑时间常数（秒）
    // 以下系数均来自 Pandolf et al., 1977 论文的实验测量结果，属于 HARD CONFIG。
    // Python 数字孪生必须与这些值完全同步，否则优化器输出与游戏实际行为将产生系统性偏差。
    //
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
    static const float ENERGY_TO_STAMINA_COEFF = 9.5e-07; // [SOFT fallback] Hardcore：能量→体力转换率+32%（原7.17e-07）
    
    // [HARD] 参考体重（Pandolf 模型归一化基准，与 CHARACTER_WEIGHT 保持一致）
    static const float REFERENCE_WEIGHT = 90.0; // [HARD] kg
    

    // 来自对照实验测量结果，属于 HARD CONFIG，不得随意改变。
    static const float TERRAIN_FACTOR_PAVED = 1.0; // [HARD] 铺装路面（基准）
    static const float TERRAIN_FACTOR_DIRT  = 1.1; // [HARD] 碎石路 +10%
    static const float TERRAIN_FACTOR_GRASS = 1.2; // [HARD] 草地   +20%
    static const float TERRAIN_FACTOR_BRUSH = 1.5; // [HARD] 灌木丛 +50%
    static const float TERRAIN_FACTOR_SAND  = 1.8; // [HARD] 软沙地 +80%
    
    // 恢复启动延迟常量（深度生理压制版本）
    // 深度生理压制：停止运动后3秒内系统完全不处理恢复
    // 医学解释：剧烈运动停止后的前10-15秒，身体处于摄氧量极度不足状态（Oxygen Deficit）
    // 此时血液流速仍处于峰值，心脏负担极重，体能并不会开始"恢复"，而是在维持不崩塌
    // 目的：消除"跑两步停一下瞬间回血"的游击战式打法，同时提高游戏流畅度
    static const float RECOVERY_STARTUP_DELAY_SECONDS = 3.0; // 恢复启动延迟（秒）- 从5秒缩短到3秒
    
    // 生理学依据：运动停止后，心率不会立刻下降，前几秒应该维持高代谢水平（EPOC）
    // 参考：Brooks et al., 2000; LaForgia et al., 2006
    // 拟真向延长至 2 秒；若影响手感可再降低（现实为分钟级）
    static const float EPOC_DELAY_SECONDS = 2.0; // EPOC延迟时间（秒）
    static const float EPOC_DRAIN_PER_TICK = 0.001; // EPOC期间的基础消耗率（每0.2秒）- 模拟维持高代谢水平
    static const float EPOC_DRAIN_RATE = EPOC_DRAIN_PER_TICK; // @deprecated 请用 EPOC_DRAIN_PER_TICK
    //! EPOC 相对 CP 的超额倍率上限：0.5 → 最多 1.5× 基础 EPOC（约 0.75%/s）
    static const float EPOC_MAX_POWER_EXCESS_RATIO = 0.5;
    //! 运动中峰值功率向当前功率衰减（W/s），避免一次冲刺污染后续停步罚
    static const float EPOC_PEAK_DECAY_WATTS_PER_SEC = 100.0;
    //! 峰值 ≤ CP×此比值视为有氧巡航：只用弱 EPOC，不加超额
    static const float EPOC_AEROBIC_CP_RATIO = 1.08;
    static const float EPOC_AEROBIC_DRAIN_MULT = 0.25;

    // 生理学依据：不同姿态对体力的消耗不同
    // 参考：Knapik et al., 1996; Pandolf et al., 1977
    // / GetPostureProneMultiplier() 替代，从配置管理器动态读取，static const 版本废弃删除。
    static const float POSTURE_STAND_MULTIPLIER = 1.0; // 站立行走消耗倍数（基准）
    // SWIMMING_*/WET_WEIGHT_* 已迁至 SCR_RSS_SwimConstants.c
    // ENV_* 已迁至 SCR_RSS_EnvConstants.c

    // 获取战术冲刺爆发时长（秒）；爆发期内负重对速度惩罚减轻
    static float GetTacticalSprintBurstDuration()
    {
        return TACTICAL_SPRINT_BURST_DURATION;
    }

    // 获取战术冲刺爆发后缓冲区时长（秒）；8s 后这 5s 内负重惩罚从爆发系数线性过渡到 1.0
    static float GetTacticalSprintBurstBufferDuration()
    {
        return TACTICAL_SPRINT_BURST_BUFFER_DURATION;
    }

    // 获取战术冲刺爆发期内负重惩罚乘数（0~1，越小负重影响越弱）
    static float GetTacticalSprintBurstEncumbranceFactor()
    {
        return TACTICAL_SPRINT_BURST_ENCUMBRANCE_FACTOR;
    }

    // 获取战术冲刺冷却时长（秒）；爆发结束或松开冲刺后在此时间内再次冲刺不触发爆发，直接平稳期
    static float GetTacticalSprintCooldown()
    {
        return TACTICAL_SPRINT_COOLDOWN;
    }

    static float GetIndoorStairsEncumbranceSpeedFactor()
    {
        return INDOOR_STAIRS_ENCUMBRANCE_SPEED_FACTOR;
    }

    // 获取低体力步行恢复区域阈值（体力百分比，默认 0.15 = 15%）
    static float GetWalkRecoveryZoneThreshold()
    {
        return WALK_RECOVERY_ZONE_THRESHOLD;
    }

    // 获取低体力步行恢复速率（每 0.2s tick，默认 0.002 = 1%/s）
    static float GetWalkRecoveryZoneRate()
    {
        return WALK_RECOVERY_ZONE_PER_TICK;
    }

    static float GetCamInertiaStartLagDuration() { return CAM_INERTIA_START_LAG_DURATION; }
    static float GetCamInertiaStartTiltDeg() { return CAM_INERTIA_START_TILT_DEG; }
    static float GetCamInertiaDecelOvershootDuration() { return CAM_INERTIA_DECEL_OVERSHOOT_DURATION; }
    static float GetCamInertiaDecelOvershootForwardM() { return CAM_INERTIA_DECEL_OVERSHOOT_FORWARD_M; }
    static float GetCamInertiaOvershootLoadMinKg() { return CAM_INERTIA_OVERSHOOT_LOAD_MIN_KG; }
    static float GetCamInertiaOvershootLoadMaxKg() { return CAM_INERTIA_OVERSHOOT_LOAD_MAX_KG; }
    static float GetCamInertiaOvershootExponent() { return CAM_INERTIA_OVERSHOOT_EXPONENT; }
    static float GetCamInertiaEncumbranceThreshold() { return CAM_INERTIA_ENCUMBRANCE_THRESHOLD; }
    static float GetCamSprintFovBurstDeg() { return CAM_SPRINT_FOV_BURST_DEG; }
    static float GetCamSprintFovCruiseDeg() { return CAM_SPRINT_FOV_CRUISE_DEG; }
    static float GetCamSprintFovLimpDeg() { return CAM_SPRINT_FOV_LIMP_DEG; }
    static float GetCamSprintFovLimpStaminaThreshold() { return CAM_SPRINT_FOV_LIMP_STAMINA_THRESHOLD; }
    static float GetCamBobVerticalAmplitudeBase() { return CAM_BOB_VERTICAL_AMPLITUDE_BASE; }
    static float GetCamBobVerticalFreqBase() { return CAM_BOB_VERTICAL_FREQ_BASE; }
    static float GetCamBobEncumbranceScale() { return CAM_BOB_ENCUMBRANCE_SCALE; }
    static float GetCamBobStaminaScale() { return CAM_BOB_STAMINA_SCALE; }
    static float GetCamBobUphillSwayAmplitude() { return CAM_BOB_UPHILL_SWAY_AMPLITUDE; }
    static float GetCamBobUphillSwayFreq() { return CAM_BOB_UPHILL_SWAY_FREQ; }
    static float GetCamBobUphillSlopeDegMin() { return CAM_BOB_UPHILL_SLOPE_DEG_MIN; }
    static float GetCamDebugStrength() { return CAM_DEBUG_STRENGTH; }
    static float GetCamSprintFovBonusDeg() { return CAM_SPRINT_FOV_BONUS_DEG; }
    static float GetCamSprintFovBlendUpSec() { return CAM_SPRINT_FOV_BLEND_UP_SEC; }
    static float GetCamSprintFovBlendDownSec() { return CAM_SPRINT_FOV_BLEND_DOWN_SEC; }
    static float GetCamSprintFovMaxRateDegPerSec() { return CAM_SPRINT_FOV_MAX_RATE_DEG_PER_SEC; }
    static float GetCamSprintFovLimpHystStaminaLow() { return CAM_SPRINT_FOV_LIMP_HYST_STAMINA_LOW; }
    static float GetCamSprintFovLimpHystStaminaHigh() { return CAM_SPRINT_FOV_LIMP_HYST_STAMINA_HIGH; }
    static float GetCamSprintFovTargetSmoothTauSec() { return CAM_SPRINT_FOV_TARGET_SMOOTH_TAU_SEC; }
    //! @return true 时使用仅原生表现（跳过 RSS 自定义屏效/相机/相关音频覆盖）
    static bool IsRssPresentationNativeOnly() { return RSS_PRESENTATION_NATIVE_ONLY; }
    // 核心思路：单次动作很轻，连续动作剧增。模拟肌肉在连续负重深蹲时从有氧到无氧的转变。
    // 生理学依据：肌肉在连续爆发时会产生乳酸堆积，导致肌肉疲劳和力量下降
    
    // 姿态转换基础消耗（0.0-1.0范围）- v2.0 优化：下调基础消耗，引入疲劳堆积机制
    static const float STANCE_COST_PRONE_TO_STAND = 0.015; // 1.5% - 趴到站（模拟推地和起身的动作）
    static const float STANCE_COST_PRONE_TO_CROUCH = 0.010; // 1.0% - 趴到蹲（中等）
    static const float STANCE_COST_CROUCH_TO_STAND = 0.005; // 0.5% - 蹲到站（极轻，允许正常掩体观察）
    static const float STANCE_COST_STAND_TO_PRONE = 0.003; // 0.3% - 站到趴（主要是离心控制）
    static const float STANCE_COST_OTHER = 0.003; // 0.3% - 其他转换（如 STAND -> CROUCH）
    
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

    // 姿态窗口结算：连续两次切换时间差 < 1 秒视为同窗口，>= 1 秒则结束窗口并结算
    static const float STANCE_WINDOW_GAP = 1.0; // 间隔超过此值（秒）则结束当前窗口、结算体力并开新窗口

    // v2.0 优化：将原来的 1.5 次幂改为线性，避免负重影响过快
    // WeightFactor = CurrentWeight / 90.0
    // 对于 28KG 负重（总重118KG），因子约为 1.31，而不是 1.5 次幂后的 1.5
    
    // 负重基准重量（kg）
    static const float STANCE_WEIGHT_BASE = 90.0; // 90kg 基准重量
    
    // 不触发"呼吸困顿期"：姿态转换属于瞬时消耗，不应该像 Sprint 停止后那样触发 5 秒禁止恢复的逻辑
    // 应该允许玩家在变换姿态后立即开始（被压制的）恢复
    
    // 体力阈值保护：当体力低于 10% 时，强制减慢姿态切换动画（如果能控制动画速度）或禁止直接从趴姿跳跃站起
    static const float STANCE_TRANSITION_MIN_STAMINA_THRESHOLD = 0.10; // 10% 体力阈值

    // ==================== v5 双池 / 代谢锚点 ====================
    static const float V5_SUSTAINABLE_WATTS_DEFAULT = 780.0;
    static const float V5_MIN_METABOLIC_SPEED_FACTOR = 0.35;
    static const float V5_TACTICAL_SHORT_BURST_SEC = 3.0;
    static const float V5_BURST_EARLY_RELEASE_BONUS = 0.45;
    static const float V5_COLLAPSE_AEROBIC_ENTER = 0.05;
    static const float V5_COLLAPSE_AEROBIC_EXIT = 0.12;
    static const float V5_WALK_SPEED_MS_DEFAULT = 1.4;
    //! March Run 意图顶（可略高于有氧巡航）；持续超过 V6_AEROBIC_CRUISE_MAX_MS 需烧 W′
    static const float V5_RUN_SPEED_MS_DEFAULT = 2.8;
    //! 步态基准：Sprint 相对 Run 至少拉开 ~60%（实机再经负重/功率软顶）
    static const float V5_SPRINT_SPEED_MS_DEFAULT = 4.5;

    //! 允许冲刺时，Sprint 相对（已负重缩放的）Run 的最低倍率——保档位身份
    static const float SPRINT_GAIT_MIN_OVER_RUN_RATIO = 1.25;
    //! 冲刺负重惩罚放大（相对 Run）。2.2 ≈ 21.6 kg 战斗装 30 m 用时相对空载 +32%（军事冲刺文献）
    static const float SPRINT_ENCUMBRANCE_PENALTY_MULT = 2.2;
    static const float V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT = 0.20;
    static const float V5_BURST_COOLDOWN_FULL_DEFAULT = 180.0;
    static const float V5_BURST_COOLDOWN_SHORT_DEFAULT = 75.0;
    static const float V5_ANAEROBIC_RECOVERY_PER_SEC_DEFAULT = 0.08;

    // ==================== v6 学术拟真：ACSM + CP-W' ====================
    //! 有氧巡航硬顶（m/s）：W′ 不可用时 Run 不得超过；超过必须吃 W′。Walk 不套此帽。
    static const float V6_AEROBIC_CRUISE_MAX_MS = 2.4;
    //! ACSM 跑步功率 P = scale * (REST + LINEAR*v + QUAD*v^2)，scale = totalWeight/REFERENCE
    static const float V6_ACSM_REST_W = 50.0;
    static const float V6_ACSM_LINEAR_W_PER_MS = 200.0;
    static const float V6_ACSM_QUAD_W_PER_MS2 = 80.0;
    static const float V6_ACSM_BLEND_START_MS = 2.0;
    static const float V6_ACSM_BLEND_END_MS = 2.4;
    static const float V6_INVERT_SPEED_MAX_MS = 6.0;
    static const float V6_STANDING_REST_WATTS = 100.0;

    // LCDA backpacking + graded walking（Looney et al. 2019/2022；Walk/站立，v≤1.97）
    // M(W/kg_body) = M_rest + (0.19 + η*(1.78*S^0.58 + 0.27*S^4) + M_grade) * (1 + 1.96*L_bp^1.36)
    // M_grade = 34*S*G*(1 - 1.05^(1 - 1.1^(100*G+32)))，G=rise/run
    static const float LCDA_REST_W_PER_KG = 1.05;
    static const float LCDA_STAND_NET_W_PER_KG = 0.19;
    static const float LCDA_SPEED_FRAC_COEFF = 1.78;
    static const float LCDA_SPEED_FRAC_EXP = 0.58;
    static const float LCDA_SPEED_QUARTIC_COEFF = 0.27;
    static const float LCDA_LOAD_COEFF = 1.96;
    static const float LCDA_LOAD_EXP = 1.36;
    static const float LCDA_GRADE_COEFF = 34.0;
    static const float LCDA_GRADE_BASE_A = 1.05;
    static const float LCDA_GRADE_BASE_B = 1.1;
    static const float LCDA_GRADE_OFFSET = 32.0;
    static const float LCDA_MAX_SPEED_MS = 1.97;

    //! CP-W' 默认：与 LCDA Walk 巡航同尺度（约支撑 38kg@1.7m/s 不扣 W′）
    static const float V6_CRITICAL_POWER_WATTS_DEFAULT = 780.0;
    static const float V6_W_PRIME_MAX_JOULES_DEFAULT = 20000.0;
    static const float V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT = 12.0;
    static const float V6_SPRINT_POWER_CAP_WATTS_DEFAULT = 2400.0;

    //! v6 ACSM 功率→STA 标定：联合 energy_to_stamina_coeff，使 38kg@3.2m/s 约 8–10s/1%
    static const float V6_STAMINA_DRAIN_CALIBRATION = 0.72;

    // v6 动态 CP 修正（LF/TF，系数经 bench 标定，下坡不对 CP 加成）
    static const float V6_CP_LOAD_REF_KG = 10.0;
    static const float V6_CP_LOAD_DECAY_PER_KG = 0.002;
    static const float V6_CP_SLOPE_K_UP = 0.015;
    static const float V6_CP_FATIGUE_K = 0.18;
    static const float V6_CP_ENV_FLOOR = 0.55;

    // Skiba W′ 双指数再填充（CP ≤ 此阈值启用；代谢尺度 CP≈700–850 仍走 Skiba）
    static const float V6_SKIBA_ELITE_CP_THRESHOLD_W = 2000.0;
    static const float V6_W_PRIME_K_FAST = 0.15;
    static const float V6_W_PRIME_K_SLOW = 0.008;
    static const float V6_W_PRIME_LIM_RATIO = 0.5;

    // 积分疲劳 I(t)
    static const float V6_FATIGUE_I_MAX = 1.0;
    static const float V6_FATIGUE_K_RECOVERY = 0.0008;
    static const float V6_FATIGUE_K_LOAD = 0.10;
    static const float V6_FATIGUE_K_SLOPE = 8.0;
    static const float V6_FATIGUE_K_TERRAIN = 0.18;
    //! dI/dt 缩放（原 0.0001 过快顶满 70% cap）
    static const float V6_FATIGUE_INTEGRAL_SCALE = 0.000055;

    //! Sprint 有氧 STA 扣减校准（W′ 放电时叠加 WPRIME 系数）
    static const float V6_SPRINT_AEROBIC_DRAIN_FACTOR = 0.72;
    static const float V6_SPRINT_WPRIME_STA_RELIEF = 0.65;

    //! @deprecated v6 玩家速度不再使用 Minetti 指数；AI 迁移后删除
    static const float STAMINA_EXPONENT_LEGACY = 0.6;

    //! 陆地静止/运动分界（m/s）：ExerciseTracker、ResolveMovementDrain、EPOC 取消、恢复对齐
    static const float RSS_IDLE_SPEED_THRESHOLD_MPS = 0.1;
    //! 超限速代谢记账：v_meas 超出 v_limit 超过此值则 W′ 按实测速度（疲劳仍 v_drain）
    static const float V6_OVERSPEED_ACCOUNTING_EPS_MPS = 0.12;
    //! 超速关闭阈值：池 ≤ 冲刺阈值 + 此值时解除超速武装（CP 巡航）
    static const float V6_WPRIME_OVERSPEED_HYSTERESIS = 0.05;
    //! 超速再武装：池须回到 冲刺阈值 + 此值，避免 W′≈关闭带附近抖动把均速抬到精英级
    static const float V6_WPRIME_OVERSPEED_REARM = 0.40;
    //! 体力 tick 间隔（秒）；EstimateRecoveryTimeToFull 分段积分用
    static const float RSS_STAMINA_TICK_SEC = 0.2;

    //! 负重 Run/Sprint 额外掉条：负载超过起点后线性升至满幅（不作用于 Walk）
    //! 目标：~29 kg 连续 Run 在十余分钟内明显力竭，而非 CP 封顶后磨 1–2 小时。
    static const float LOADED_RUN_DRAIN_START_KG = 12.0;
    static const float LOADED_RUN_DRAIN_REF_KG = 30.0;
    static const float LOADED_RUN_DRAIN_MAX_MULT = 4.5;
}
