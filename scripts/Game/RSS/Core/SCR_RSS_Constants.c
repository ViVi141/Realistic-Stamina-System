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
    static const float WALK_RECOVERY_ZONE_RATE = 0.002; // 低体力区域每0.2s恢复0.2%（即每秒1%）
    
    // 坡度修正系数
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
    
    // 战斗负重（kg）- 战斗状态下的推荐负重阈值（包含基准负重）
    static const float COMBAT_ENCUMBRANCE_WEIGHT = 30.0; // kg
    
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
    static const float TACTICAL_SPRINT_BURST_ENCUMBRANCE_FACTOR = 0.2; // 爆发期内负重惩罚乘数（0.2 = 明显拉开与平稳期差距，29kg 下爆发更快）
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
    static const float EPOC_DRAIN_RATE = 0.001; // EPOC期间的基础消耗率（每0.2秒）- 模拟维持高代谢水平

    // 生理学依据：不同姿态对体力的消耗不同
    // 参考：Knapik et al., 1996; Pandolf et al., 1977
    // / GetPostureProneMultiplier() 替代，从配置管理器动态读取，static const 版本废弃删除。
    static const float POSTURE_STAND_MULTIPLIER = 1.0; // 站立行走消耗倍数（基准）
    
    // 生理学依据：游泳的能量消耗远高于陆地移动（约3-5倍）
    // 物理模型：阻力与速度平方成正比 F_d = 0.5 * ρ * v² * C_d * A
    //       Journal of Applied Physiology, 37(5), 762-765.
    
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
    static const float ENV_CHECK_INTERVAL = 10.0; // 秒，环境因子检测间隔（perf: 5→10，天气变化缓慢无需高频）
    
    // 室内检测参数
    static const float ENV_INDOOR_CHECK_HEIGHT = 10.0; // 米，向上检测高度（判断是否有屋顶）
    static const float ENV_SLOPE_SUPPRESS_ROOF_CHECK_HEIGHT = 35.0; // 米
    
    
    // 降雨强度相关常量
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE = 0.5; // kg/秒，基础湿重增加速率
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT = 1.5; // 降雨强度指数（非线性增长）
    static const float ENV_RAIN_INTENSITY_THRESHOLD = 0.01; // 原始阈值（仅用于 API 回退逻辑）
    static const float ENV_RAIN_VISUAL_EFFECT_THRESHOLD = 0.15;
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
    static const float ENV_MUD_SLIP_RISK_BASE = 0.005; // 基础滑倒风险（每0.2秒，与 SCR_RSS_EnvironmentFactor.CalculateSlipRisk 一致）
    // ## DESIGN NOTE: Mud slip camera & trigger thresholds
    //
    // Tuning recommendations for a future pass:
    //   - Shift frequency to 6-8Hz (below resonance band)
    static const float ENV_MUD_SLIP_MIN_SPEED_MS = 1.6; // 低于此水平速度不判滑倒（m/s）
    static const float ENV_MUD_SLIP_GLOBAL_SCALE = 1.5; // 总强度倍率（Poisson λ）；由 2.5×0.6 下调以压低 1s 复合触发率
    // RSS 主循环速度/体力 tick（毫秒）：玩家≈60Hz；AI 仅在服务器上降频
    static const int RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS = 17;
    static const int RSS_AI_SPEED_UPDATE_INTERVAL_MS = 100;
    //! 服端单兵：按距玩家距离分档刷新间隔
    static const bool RSS_PERF_AI_DISTANCE_LOD_ENABLED = true;
    static const float RSS_PERF_AI_LOD_NEAR_M = 400.0;
    static const float RSS_PERF_AI_LOD_FAR_M = 1200.0;
    static const int RSS_PERF_AI_LOD_NEAR_INTERVAL_MS = 200; // perf: 100→200，高密度 AI 减负。体力变化是秒级，200ms 精度足够
    static const int RSS_PERF_AI_LOD_MID_INTERVAL_MS = 300;
    static const int RSS_PERF_AI_LOD_FAR_INTERVAL_MS = 1500;
    // [SOFT] AI 体力状态机 — 转移阈值（可在预设中调整）
    static const float RSS_AI_STATE_FRESH_DOWN = 0.80;           // FRESH → WINDED
    static const float RSS_AI_STATE_WINDED_DOWN = 0.50;          // WINDED → FATIGUED
    static const float RSS_AI_STATE_WINDED_UP = 0.85;            // WINDED → FRESH（滞回）
    static const float RSS_AI_STATE_FATIGUED_DOWN = 0.25;        // FATIGUED → EXHAUSTED
    static const float RSS_AI_STATE_FATIGUED_UP = 0.55;          // FATIGUED → WINDED（滞回）
    static const float RSS_AI_STATE_EXHAUSTED_DOWN = 0.10;       // EXHAUSTED → COLLAPSED
    static const float RSS_AI_STATE_EXHAUSTED_UP = 0.30;         // EXHAUSTED → FATIGUED（滞回）
    static const float RSS_AI_STATE_COLLAPSE_THRESHOLD = 0.05;   // 强制恢复触发线
    static const float RSS_AI_STATE_FORCE_RECOVER_DELAY_SEC = 5.0; // 强制恢复前静止时长
    static const float RSS_AI_STATE_RECOVER_TO_EXHAUSTED = 0.15; // RECOVERING → EXHAUSTED
    static const float RSS_AI_STATE_RECOVER_TO_FATIGUED = 0.30;  // RECOVERING → FATIGUED

    // [SOFT] AI 移动限速 — 按体力状态的 OverrideMaxSpeed 上限
    static const float RSS_AI_SPEED_FATIGUED_LIMIT = 0.65;       // FATIGUED 时限速 65%
    static const float RSS_AI_SPEED_EXHAUSTED_LIMIT = 0.40;      // EXHAUSTED 时限速 40%
    static const float RSS_AI_SPEED_RECOVERING_MIN = 0.30;       // RECOVERING 时最低速度倍率
    static const float RSS_AI_SPEED_CONTINUOUS_MIN = 0.30;       // 连续衰减曲线下限

    // [SOFT] AI 战斗衰减矩阵 — 感知 / 射速 / 技能（6 状态）
    static const float RSS_AI_COMBAT_PERCEPTION_WINDED     = 0.95;
    static const float RSS_AI_COMBAT_FIRE_RATE_WINDED      = 1.00;
    static const float RSS_AI_COMBAT_SKILL_WINDED          = 0.95;
    static const float RSS_AI_COMBAT_PERCEPTION_FATIGUED   = 0.80;
    static const float RSS_AI_COMBAT_FIRE_RATE_FATIGUED    = 0.85;
    static const float RSS_AI_COMBAT_SKILL_FATIGUED        = 0.80;
    static const float RSS_AI_COMBAT_PERCEPTION_EXHAUSTED  = 0.60;
    static const float RSS_AI_COMBAT_FIRE_RATE_EXHAUSTED   = 0.60;
    static const float RSS_AI_COMBAT_SKILL_EXHAUSTED       = 0.55;
    static const float RSS_AI_COMBAT_PERCEPTION_COLLAPSED  = 0.40;
    static const float RSS_AI_COMBAT_FIRE_RATE_COLLAPSED   = 0.30;
    static const float RSS_AI_COMBAT_SKILL_COLLAPSED       = 0.30;
    static const float RSS_AI_COMBAT_PERCEPTION_RECOVERING = 0.80;
    static const float RSS_AI_COMBAT_FIRE_RATE_RECOVERING  = 0.90;
    static const float RSS_AI_COMBAT_SKILL_RECOVERING      = 0.80;

    // [SOFT] AI 伤害-体力联动
    static const float RSS_AI_INJURY_DRAIN_MILD = 1.15;
    static const float RSS_AI_INJURY_RECOVERY_MILD = 0.85;
    static const float RSS_AI_INJURY_DRAIN_MODERATE = 1.4;
    static const float RSS_AI_INJURY_RECOVERY_MODERATE = 0.6;
    static const float RSS_AI_INJURY_DRAIN_SEVERE = 1.8;
    static const float RSS_AI_INJURY_RECOVERY_SEVERE = 0.3;
    static const float RSS_AI_INJURY_DRAIN_CRITICAL = 2.5;
    static const float RSS_AI_INJURY_RECOVERY_CRITICAL = 0.1;

    // [SOFT] AI 行为过滤 — 常量
    static const float RSS_AI_INTENT_WAIT_PROMOTED_PRIORITY = 100.0;
    // 以下为旧版乘法模型遗留，滑倒判定已改为 ACOF/RCOF（见 SCR_RSS_MudSlipEffects）
    static const float ENV_MUD_SLIP_SPEED_COEFF = 0.14; // 未使用
    static const float ENV_MUD_SLIP_SPRINT_MULT = 2.3; // 未使用
    static const float ENV_MUD_SLIP_WEIGHT_COEFF = 0.014; // 未使用
    // 摩擦阈值模型（无量纲，与 μ 同量级）：RCOF > ACOF 时才有可能滑倒
    static const float ENV_SLIP_ACOF_DRY_BASE = 0.52; // 干燥铺装近似可用摩擦系数上限
    static const float ENV_SLIP_ACOF_OFFROAD_DROP = 0.07; // 地形系数高于 1 时干摩擦上界降低（松软地面）
    static const float ENV_SLIP_LUBRICATION_MAX = 0.62; // 泥泞润滑使 ACOF 最多降到约 (1-max)*μ_dry
    static const float ENV_SLIP_ACOF_MIN = 0.07;
    static const float ENV_SLIP_ACOF_MAX = 0.58;
    static const float ENV_SLIP_RCOF_BASE = 0.20; // 低速行走所需摩擦（原0.13→0.20：让泥地冲刺即可产生非零 gap）
    static const float ENV_SLIP_RCOF_VSQ = 0.0080; // 速度平方项（原0.0055→0.0080：提高高速段 RCOF 灵敏度）
    static const float ENV_SLIP_RCOF_SPRINT = 0.058; // 冲刺固定加项（原0.0406→0.058：恢复至原始值，让冲刺有更清晰的风险信号）
    static const float ENV_SLIP_RCOF_WEIGHT = 0.065; // 满负重时额外需求（0~55kg）
    static const float ENV_SLIP_RCOF_CROUCH_MULT = 0.86; // 蹲姿步幅小，降低 RCOF
    // 坡度：先按 |α| 得 slopeAdd，再乘以下坡/上坡系数（GetSlopeAngle：正=上坡，负=下坡）
    static const float ENV_SLIP_RCOF_SLOPE_PER_DEG = 0.0026;
    static const float ENV_SLIP_RCOF_SLOPE_MAX = 0.095;
    static const float ENV_SLIP_RCOF_SLOPE_DOWNHILL_MULT = 1.14; // 下坡略增需求（重心前移）
    static const float ENV_SLIP_RCOF_SLOPE_UPHILL_MULT = 0.98; // 上坡略低于同倾角下坡
    static const float ENV_SLIP_TURN_MIN_HORIZ_MS = 0.2; // 本帧与上一帧水平速均高于此才计 ω
    static const float ENV_SLIP_RCOF_TURN_OMEGA_V_COEFF = 0.0030; // 原仅 ω·k_ω 已弃用，见 git 历史
    static const float ENV_SLIP_RCOF_TURN_MAX = 0.07;
    static const float ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC = 12.0; // 防止单帧数值爆炸
    // 低体力：平衡能力下降 → RCOF 基线上升 + 小幅随机抖动 σ
    static const float ENV_SLIP_RCOF_BALANCE_STAMINA = 0.085; // 与 (1−stamina) 成正比
    static const float ENV_SLIP_RCOF_BALANCE_JITTER = 0.045; // RandomFloat01()×此项×(1−stamina)
    // 落地/坠地瞬间：上一帧明显下落、本帧已接近接地 → 重心不稳
    static const float ENV_SLIP_RCOF_LANDING = 0.11;
    static const float ENV_SLIP_LANDING_VY_PREV = -1.15;
    static const float ENV_SLIP_LANDING_VY_CUR = -0.42;
    // 起跳蹬地瞬间（垂直上速较大）
    static const float ENV_SLIP_RCOF_JUMP_UP_VY = 1.65;
    static const float ENV_SLIP_RCOF_JUMP_UP = 0.042;
    // 摩擦缺口 gap=RCOF−ACOF（无量纲）：镜头用较小 ε，掷骰用较大 ε，使「先晃后摔」有缓冲带
    static const float ENV_SLIP_GAP_EPSILON_CAMERA = 0.004; // 镜头应力：gap>此值才开始有预警（再经疲劳缩小 ε_eff）
    static const float ENV_SLIP_GAP_EPSILON_DICE = 0.012;   // 掷骰：gap≤此值不进入 Poisson（仅镜头仍可预警）；原0.008→0.012 减少碎石路误触
    // λ∝gap（线性标度）；单步 P 经约 20 次/秒复合后体感见 docs/泥泞滑倒判定模型.md §6.1
    static const float ENV_MUD_SLIP_PHYS_SCALE = 2.2; // 与 ENV_MUD_SLIP_GLOBAL_SCALE 联调（原 gap² 时代为 20）
    static const float ENV_MUD_SLIP_COOLDOWN_SEC = 9.0; // 两次滑倒之间的最短间隔（秒），从「趴→蹲/站」锚定时刻起算（见 RSS_MudSlipRunner）
    static const float ENV_MUD_SLIP_COOLDOWN_ANCHOR_FALLBACK_SEC = 3.0;
    static const float ENV_MUD_SLIP_STAMINA_FRAC = 0.065; // 触发时额外扣除体力比例（0~1）
    static const float ENV_MUD_SLIP_RAGDOLL_WARMUP_SEC = 2.2; // 首次 Refresh：保持布娃娃至少若干秒内不因“静止”被关掉
    static const int ENV_MUD_SLIP_RAGDOLL_BLEND_DELAY_MS = 1200; // 此后延迟再 Refresh(0) 开始混合回动画（毫秒）
    // 泥泞失稳：第一人称镜头角抖动预警（无踉跄动画时的主要反馈）；与 gap 映射，与滑倒 Poisson 独立
    static const float ENV_MUD_SLIP_CAM_SHAKE_GAP_SPAN = 0.14; // gap 从 ε 拉到 1.0 应力的跨度（无量纲）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_STAMINA_THRESHOLD = 0.2; // 低于此体力开始收紧 ε_eff
    static const float ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_EPS_SCALE = 0.35; // stamina→0 时 ε_eff = ε·scale；→threshold 时回到 ε
    static const float ENV_MUD_SLIP_CAM_SHAKE_ROLL_DEG = 6.0;   // 应力=1 时横滚峰值（度）；Roll 对「失稳」暗示强且较不易晕
    static const float ENV_MUD_SLIP_CAM_SHAKE_PITCH_DEG = 3.4;  // 俯仰峰值（度）
    static const float ENV_MUD_SLIP_CAM_SHAKE_YAW_DEG = 2.9;    // 偏航峰值（度）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FREQ_BASE = 6.0;  // 基础角频率 Hz 量级（原9.0→6.0：移出人前庭共振带 8-12Hz）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FREQ_STRESS = 6.0; // 应力越高频率越快（原12.0→6.0：典型应力0.2-0.5时频率7-9Hz<12Hz）
    static const float ENV_MUD_SLIP_CAM_SHAKE_SMOOTH_RATE = 16.0; // 应力平滑（越大越快跟上目标）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FOV_JITTER_DEG = 0.35; // 极力克制：FOV 高频抖易诱发晕动症，优先靠 Roll
    
    // 气温相关常量
    static const float ENV_TEMPERATURE_HEAT_THRESHOLD       = 30.0; // [HARD] °C，医学热应激阈值
    static const float ENV_TEMPERATURE_HEAT_PENALTY_COEFF   = 0.02; // [SOFT fallback] 每高1°C恢复率降低2%
    static const float ENV_TEMPERATURE_COLD_THRESHOLD       =  5.0; // [HARD] °C，北约STANAG冷应激阈值（原0°C几乎不触发，提升至5°C使温带地图生效）
    static const float ENV_TEMPERATURE_COLD_STATIC_PENALTY  = 0.03; // [SOFT fallback] 低温静态消耗增加
    static const float ENV_TEMPERATURE_COLD_RECOVERY_PENALTY = 0.05; // [SOFT fallback] 低温恢复率惩罚
    
    // 地表湿度相关常量
    static const float ENV_SURFACE_WETNESS_SOAK_RATE = 1.0; // kg/秒，趴下时的湿重增加速率
    static const float ENV_SURFACE_WETNESS_THRESHOLD = 0.1; // 积水阈值（高于此值触发湿重增加）
    static const float ENV_SURFACE_WETNESS_MARGINAL_DECAY_ADVANCE = 0.1; // 边际效应衰减提前触发比例
    static const float ENV_SURFACE_WETNESS_PRONE_PENALTY = 0.15;

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
        return WALK_RECOVERY_ZONE_RATE;
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
    static const float V5_SUSTAINABLE_WATTS_DEFAULT = 400.0;
    static const float V5_MIN_METABOLIC_SPEED_FACTOR = 0.35;
    static const float V5_TACTICAL_SHORT_BURST_SEC = 3.0;
    static const float V5_BURST_EARLY_RELEASE_BONUS = 0.45;
    static const float V5_COLLAPSE_AEROBIC_ENTER = 0.05;
    static const float V5_COLLAPSE_AEROBIC_EXIT = 0.12;
    static const float V5_WALK_SPEED_MS_DEFAULT = 1.4;
    static const float V5_RUN_SPEED_MS_DEFAULT = 2.8;
    static const float V5_SPRINT_SPEED_MS_DEFAULT = 4.0;
    static const float V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT = 0.20;
    static const float V5_BURST_COOLDOWN_FULL_DEFAULT = 180.0;
    static const float V5_BURST_COOLDOWN_SHORT_DEFAULT = 75.0;
    static const float V5_ANAEROBIC_RECOVERY_PER_SEC_DEFAULT = 0.08;

    // ==================== v6 学术拟真：ACSM + CP-W' ====================
    //! ACSM 跑步功率 P = scale * (REST + LINEAR*v + QUAD*v^2)，scale = totalWeight/REFERENCE
    static const float V6_ACSM_REST_W = 50.0;
    static const float V6_ACSM_LINEAR_W_PER_MS = 200.0;
    static const float V6_ACSM_QUAD_W_PER_MS2 = 80.0;
    static const float V6_ACSM_BLEND_START_MS = 2.0;
    static const float V6_ACSM_BLEND_END_MS = 2.4;
    static const float V6_INVERT_SPEED_MAX_MS = 6.0;
    static const float V6_STANDING_REST_WATTS = 100.0;

    //! CP-W' 默认（Elite；Standard/Tactical 在 Settings 中覆盖）
    static const float V6_CRITICAL_POWER_WATTS_DEFAULT = 400.0;
    static const float V6_W_PRIME_MAX_JOULES_DEFAULT = 20000.0;
    static const float V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT = 12.0;
    static const float V6_SPRINT_POWER_CAP_WATTS_DEFAULT = 1200.0;

    //! v6 ACSM 功率→STA 标定：联合 energy_to_stamina_coeff，使 38kg@3.2m/s 约 8–10s/1%
    static const float V6_STAMINA_DRAIN_CALIBRATION = 0.72;

    // v6 动态 CP 修正（LF/TF，系数经 bench 标定，下坡不对 CP 加成）
    static const float V6_CP_LOAD_REF_KG = 10.0;
    static const float V6_CP_LOAD_DECAY_PER_KG = 0.002;
    static const float V6_CP_SLOPE_K_UP = 0.015;
    static const float V6_CP_FATIGUE_K = 0.18;
    static const float V6_CP_ENV_FLOOR = 0.55;

    // Skiba W′ 双指数再填充（Elite：CP ≤ 此阈值时启用）
    static const float V6_SKIBA_ELITE_CP_THRESHOLD_W = 410.0;
    static const float V6_W_PRIME_K_FAST = 0.15;
    static const float V6_W_PRIME_K_SLOW = 0.008;
    static const float V6_W_PRIME_LIM_RATIO = 0.5;

    // 积分疲劳 I(t)
    static const float V6_FATIGUE_I_MAX = 1.0;
    static const float V6_FATIGUE_K_RECOVERY = 0.0008;
    static const float V6_FATIGUE_K_LOAD = 0.15;
    static const float V6_FATIGUE_K_SLOPE = 8.0;
    static const float V6_FATIGUE_K_TERRAIN = 0.25;

    //! @deprecated v6 玩家速度不再使用 Minetti 指数；AI 迁移后删除
    static const float STAMINA_EXPONENT_LEGACY = 0.6;

    //! 陆地静止/运动分界（m/s）：ExerciseTracker、ResolveMovementDrain、EPOC 取消、恢复对齐
    static const float RSS_IDLE_SPEED_THRESHOLD_MPS = 0.1;
    //! 超限速代谢记账：v_meas 超出 v_limit 超过此值则 W′ 按实测速度（疲劳仍 v_drain）
    static const float V6_OVERSPEED_ACCOUNTING_EPS_MPS = 0.12;
    //! 体力 tick 间隔（秒）；EstimateRecoveryTimeToFull 分段积分用
    static const float RSS_STAMINA_TICK_SEC = 0.2;
}