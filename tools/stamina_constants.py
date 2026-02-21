#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Realistic Stamina System (RSS) - 统一常量定义
所有Python脚本共享的常量定义，与C代码中的StaminaConstants完全同步
"""

# ==================== 游戏配置常量 ====================
GAME_MAX_SPEED = 5.2  # m/s，游戏最大速度
UPDATE_INTERVAL = 0.2  # 秒，更新间隔

# ==================== 角色特征常量 ====================
CHARACTER_WEIGHT = 90.0  # kg，角色体重（游戏内标准体重为90kg）
CHARACTER_AGE = 22.0  # 岁，角色年龄（基于ACFT标准，22-26岁男性）

# ==================== 负重配置常量 ====================
BASE_WEIGHT = 1.36  # kg，基准负重（基础物品重量：衣服、鞋子等）
MAX_ENCUMBRANCE_WEIGHT = 40.5  # kg，最大负重（包含基准负重）
COMBAT_ENCUMBRANCE_WEIGHT = 30.0  # kg，战斗负重（包含基准负重，标准基准）

# ==================== 健康状态/体能水平常量 ====================
FITNESS_LEVEL = 1.0  # 训练有素（well-trained）
FITNESS_EFFICIENCY_COEFF = 0.35  # 35%效率提升（训练有素时）
FITNESS_RECOVERY_COEFF = 0.25  # 25%恢复速度提升（训练有素时）

# ==================== 医学模型参数（双稳态-应激性能模型）====================
TARGET_RUN_SPEED = 3.7  # m/s
TARGET_RUN_SPEED_MULTIPLIER = TARGET_RUN_SPEED / GAME_MAX_SPEED  # 0.7115
TARGET_AVERAGE_SPEED = 3.47  # m/s，2英里在15分27秒内完成的平均速度
WILLPOWER_THRESHOLD = 0.25  # 25%
SMOOTH_TRANSITION_START = 0.25  # 25%（疲劳临界区起点）
SMOOTH_TRANSITION_END = 0.05  # 5%，平滑过渡结束点
MIN_LIMP_SPEED_MULTIPLIER = 1.0 / GAME_MAX_SPEED  # 1.0 m/s / 5.2 = 0.1923
STAMINA_EXPONENT = 0.6  # 体力影响指数

# ==================== 负重参数 ====================
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.2
ENCUMBRANCE_SPEED_PENALTY_EXPONENT = 1.5
ENCUMBRANCE_SPEED_PENALTY_MAX = 0.75
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0
MIN_SPEED_MULTIPLIER = 0.15  # 最小速度倍数
MAX_SPEED_MULTIPLIER = 1.0  # 最大速度倍数限制

# ==================== 速度阈值参数 ====================
SPRINT_VELOCITY_THRESHOLD = 5.2  # m/s，Sprint速度阈值
RUN_VELOCITY_THRESHOLD = 3.7  # m/s，Run速度阈值
WALK_VELOCITY_THRESHOLD = 3.2  # m/s，Walk速度阈值
RECOVERY_THRESHOLD_NO_LOAD = 2.5  # m/s，空载时恢复体力阈值
DRAIN_THRESHOLD_COMBAT_LOAD = 1.5  # m/s，负重30kg时开始消耗体力的阈值
COMBAT_LOAD_WEIGHT = 30.0  # kg，战斗负重（用于计算动态阈值）

# ==================== 基础消耗率（pts/s，每秒消耗的点数）====================
SPRINT_BASE_DRAIN_RATE = 0.480  # pts/s（Sprint）
RUN_BASE_DRAIN_RATE = 0.075  # pts/s（Run，优化后约22分钟耗尽）
WALK_BASE_DRAIN_RATE = 0.045  # pts/s（Walk，降低消耗率以突出与跑步的差距）
REST_RECOVERY_RATE = 0.250  # pts/s（Rest，恢复）

# 转换为每0.2秒的消耗率
SPRINT_DRAIN_PER_TICK = SPRINT_BASE_DRAIN_RATE * UPDATE_INTERVAL
RUN_DRAIN_PER_TICK = RUN_BASE_DRAIN_RATE * UPDATE_INTERVAL
WALK_DRAIN_PER_TICK = WALK_BASE_DRAIN_RATE * UPDATE_INTERVAL
REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE * UPDATE_INTERVAL

# 初始体力状态（满值）
INITIAL_STAMINA_AFTER_ACFT = 1.0  # 100.0 / 100.0 = 1.0（100%，满值）

# ==================== 精疲力尽阈值 ====================
EXHAUSTION_THRESHOLD = 0.0  # 0.0（0点）
EXHAUSTION_LIMP_SPEED = 1.0  # m/s（跛行速度）
SPRINT_ENABLE_THRESHOLD = 0.20  # 0.20（20点），体力≥20时才能Sprint

# ==================== Sprint（冲刺）相关参数 ====================
SPRINT_SPEED_BOOST = 0.30  # Sprint时速度比Run快30%
SPRINT_MAX_SPEED_MULTIPLIER = 1.0  # Sprint最高速度倍数
SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0

# ==================== Pandolf 模型常量 ====================
PANDOLF_BASE_COEFF = 1.5  # 基础系数（W/kg），恢复物理基线
PANDOLF_VELOCITY_COEFF = 1.5  # 速度系数（W/kg），恢复物理基线
PANDOLF_VELOCITY_OFFSET = 0.7  # 速度偏移（m/s）
PANDOLF_GRADE_BASE_COEFF = 0.23  # 坡度基础系数（W/kg）
PANDOLF_GRADE_VELOCITY_COEFF = 1.34  # 坡度速度系数（W/kg）
PANDOLF_STATIC_COEFF_1 = 1.2
PANDOLF_STATIC_COEFF_2 = 1.6
ENERGY_TO_STAMINA_COEFF = 1.5e-05
REFERENCE_WEIGHT = 90.0  # 参考体重（kg）

# ==================== Givoni-Goldman 跑步模型常量 ====================
GIVONI_CONSTANT = 0.15  # 跑步常数（W/kg·m²/s²），增加到0.15（Make running costly again）
GIVONI_VELOCITY_EXPONENT = 2.2  # 速度指数（2.0-2.4，2.2为推荐值）

# ==================== 坡度影响参数 ====================
SLOPE_UPHILL_COEFF = 0.08  # 上坡影响系数（每度增加8%消耗）
SLOPE_DOWNHILL_COEFF = 0.03  # 下坡影响系数（每度减少3%消耗）
SLOPE_MAX_MULTIPLIER = 2.0  # 最大坡度影响倍数（上坡）
SLOPE_MIN_MULTIPLIER = 0.7  # 最小坡度影响倍数（下坡）
ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15  # 负重×坡度交互系数
SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10  # 速度×负重×坡度交互系数

# ==================== 坡度修正系数（基于坡度百分比）====================
GRADE_UPHILL_COEFF = 0.12  # 每1%上坡增加12%消耗
GRADE_DOWNHILL_COEFF = 0.05  # 每1%下坡减少5%消耗
HIGH_GRADE_THRESHOLD = 15.0  # 15%（高坡度阈值）
HIGH_GRADE_MULTIPLIER = 1.2  # 高坡度额外1.2×乘数

# ==================== 地形系数常量 ====================
TERRAIN_FACTOR_PAVED = 1.0  # 铺装路面
TERRAIN_FACTOR_DIRT = 1.1  # 碎石路
TERRAIN_FACTOR_GRASS = 1.2  # 高草丛
TERRAIN_FACTOR_BRUSH = 1.5  # 重度灌木丛
TERRAIN_FACTOR_SAND = 1.8  # 软沙地

# ==================== 多维度恢复模型参数（深度生理压制版本）====================
# 核心概念：从"净增加"改为"代谢净值"
# 最终恢复率 = (基础恢复率 * 姿态修正) - (负重压制 + 氧债惩罚)
# 自动校准：使用二分搜索确定最优值
# 场景：Idle 60秒，起始体力0.10，目标结束体力0.40
BASE_RECOVERY_RATE = 0.00015
RECOVERY_NONLINEAR_COEFF = 0.5  # 恢复率非线性系数
# 拟真平衡点：模拟"喘匀第一口氧气"
# 生理学上，氧债的50%是在停止运动后的前30-60秒内偿还的
# 模拟停止运动后前90秒的高效恢复
FAST_RECOVERY_DURATION_MINUTES = 0.4
FAST_RECOVERY_MULTIPLIER = 1.6
# 拟真平衡点：平衡快速恢复期和慢速恢复期
MEDIUM_RECOVERY_START_MINUTES = 0.4
MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
MEDIUM_RECOVERY_MULTIPLIER = 1.3
SLOW_RECOVERY_START_MINUTES = 10.0  # 慢速恢复期开始时间（分钟）
SLOW_RECOVERY_MULTIPLIER = 0.6  # 慢速恢复期恢复速度倍数，从0.8降低到0.6，降低25%
AGE_RECOVERY_COEFF = 0.2  # 年龄恢复系数
AGE_REFERENCE = 30.0  # 年龄参考值（30岁为标准）
FATIGUE_RECOVERY_PENALTY = 0.05  # 疲劳恢复惩罚系数（最大5%恢复速度减少）
FATIGUE_RECOVERY_DURATION_MINUTES = 20.0  # 疲劳完全恢复所需时间（分钟）

# ==================== 姿态恢复加成参数（深度生理压制版本）====================
# 深度生理压制：趴下不只是为了隐蔽，更是为了让心脏负荷最小化
# 姿态加成设定的更有体感，但不过分
# 站姿：确保能够覆盖静态站立消耗
# 蹲姿：减少下肢肌肉紧张，提高恢复速度
# 趴姿：全身放松，最大化血液循环
# 逻辑：趴下是唯一的快速回血手段（重力分布均匀），强迫重装兵必须趴下
STANDING_RECOVERY_MULTIPLIER = 1.3
CROUCHING_RECOVERY_MULTIPLIER = 1.4
PRONE_RECOVERY_MULTIPLIER = 1.6

# ==================== 负重对恢复的静态剥夺机制（深度生理压制版本）====================
# 医学解释：背负30kg装备站立时，斜方肌、腰椎和下肢肌肉仍在进行高强度静力收缩
# 这种收缩产生的代谢废物（乳酸）排放速度远慢于空载状态
# 数学实现：恢复率与负重百分比（BM%）挂钩
# Penalty = (当前总重 / 身体耐受基准)^2 * COEFF
# 结果：穿着40kg装备站立恢复时，原本100%的基础恢复会被扣除较高比例
# 战术意图：强迫重装兵必须趴下（通过姿态加成抵消负重扣除），否则回血极慢
LOAD_RECOVERY_PENALTY_COEFF = 0.0001
LOAD_RECOVERY_PENALTY_EXPONENT = 2.0
BODY_TOLERANCE_BASE = 90.0

# ==================== 边际效应衰减机制（深度生理压制版本）====================
# 医学解释：身体从"半死不活"恢复到"喘匀气"很快（前80%），但要从"喘匀气"恢复到"肌肉巅峰竞技状态"非常慢（最后20%）
# 数学实现：当体力>80%时，恢复率 = 原始恢复率 * (1.1 - 当前体力百分比)
# 结果：最后10%的体力恢复速度会比前50%慢3-4倍
# 战术意图：玩家经常会处于80%-90%的"亚健康"状态，只有长时间修整才能真正满血
MARGINAL_DECAY_THRESHOLD = 0.8  # 边际效应衰减阈值（80%体力）
MARGINAL_DECAY_COEFF = 1.1  # 边际效应衰减系数

# ==================== 最低体力阈值限制（深度生理压制版本）====================
# 医学解释：当体力过低时，身体处于极度疲劳状态，需要更长时间的休息才能开始恢复
# 数学实现：体力低于20%时，必须处于静止状态10秒后才允许开始回血
# 战术意图：防止玩家在极度疲劳时通过"跑两步停一下"的方式快速回血
# [修复 v3.6.1] 将极度疲劳惩罚时间从 10秒 缩短为 3秒
# 模拟深呼吸 3 秒后即可开始恢复，而不是发呆 10 秒
MIN_RECOVERY_STAMINA_THRESHOLD = 0.2  # 最低体力阈值（20%）
MIN_RECOVERY_REST_TIME_SECONDS = 3.0  # 最低体力时需要的休息时间（秒）

# ==================== 运动持续时间影响（累积疲劳）参数 ====================
FATIGUE_ACCUMULATION_COEFF = 0.015  # 每分钟增加1.5%消耗
FATIGUE_START_TIME_MINUTES = 5.0  # 疲劳开始累积的时间（分钟）
FATIGUE_MAX_FACTOR = 2.0  # 最大疲劳因子（2.0倍）

# ==================== 代谢适应参数（Metabolic Adaptation）====================
AEROBIC_THRESHOLD = 0.6
ANAEROBIC_THRESHOLD = 0.8
AEROBIC_EFFICIENCY_FACTOR = 0.9
MIXED_EFFICIENCY_FACTOR = 1.0  # 混合区效率因子（100%，标准）
ANAEROBIC_EFFICIENCY_FACTOR = 1.2


# ==================== 恢复启动延迟常量（深度生理压制版本）====================
# 深度生理压制：停止运动后3秒内系统完全不处理恢复
# 医学解释：剧烈运动停止后的前10-15秒，身体处于摄氧量极度不足状态（Oxygen Deficit）
# 此时血液流速仍处于峰值，心脏负担极重，体能并不会开始"恢复"，而是在维持不崩塌
# 目的：消除"跑两步停一下瞬间回血"的游击战式打法，同时提高游戏流畅度
RECOVERY_STARTUP_DELAY_SECONDS = 3.0  # 恢复启动延迟（秒）- 从5秒缩短到3秒

# ==================== EPOC（过量耗氧）延迟参数 ====================
# 生理学依据：运动停止后，心率不会立刻下降，前几秒应该维持高代谢水平（EPOC）
# 参考：Brooks et al., 2000; LaForgia et al., 2006
# 拟真平衡点：与数字孪生模拟器保持一致
EPOC_DELAY_SECONDS = 0.5  # EPOC延迟时间（秒）- 与数字孪生模拟器一致
EPOC_DRAIN_RATE = 0.001  # EPOC期间的基础消耗率（每0.2秒）

# ==================== 姿态交互修正参数 ====================
POSTURE_CROUCH_MULTIPLIER = 0.8  # 蹲姿行走速度倍数（减速）
POSTURE_PRONE_MULTIPLIER = 0.5  # 匍匐爬行速度倍数（减速）
POSTURE_STAND_MULTIPLIER = 1.0  # 站立行走速度倍数（基准）

# ==================== 游泳体力消耗参数 ====================
SWIMMING_DRAG_COEFFICIENT = 0.5  # 阻力系数（C_d）
SWIMMING_WATER_DENSITY = 1000.0  # 水密度（ρ，kg/m³）
SWIMMING_FRONTAL_AREA = 0.5  # 正面面积（A，m²）
SWIMMING_BASE_POWER = 20.0  # 基础游泳功率（W），降低以提高水中存活率
SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0  # kg，超过此重量时静态消耗大幅增加
SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0  # 超过阈值时的静态消耗倍数
SWIMMING_FULL_PENALTY_WEIGHT = 40.0  # kg，达到此重量时应用满额惩罚
SWIMMING_LOW_INTENSITY_DISCOUNT = 0.7  # 低强度踩水时的消耗折扣
SWIMMING_LOW_INTENSITY_VELOCITY = 0.2  # m/s，低于此速度视为低强度踩水
SWIMMING_ENERGY_TO_STAMINA_COEFF = 0.00005  # 游泳功率到体力消耗的转换系数
SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0  # 动态功率效率因子
SWIMMING_VERTICAL_DRAG_COEFFICIENT = 1.2  # 垂直方向阻力系数
SWIMMING_VERTICAL_FRONTAL_AREA = 0.8  # 垂直方向受力面积（m²）
SWIMMING_VERTICAL_SPEED_THRESHOLD = 0.05  # m/s，垂直速度阈值
SWIMMING_EFFECTIVE_GRAVITY_COEFF = 0.15  # 水中有效重力系数
SWIMMING_BUOYANCY_FORCE_COEFF = 0.10  # 浮力对抗系数
SWIMMING_VERTICAL_UP_MULTIPLIER = 2.5  # 上浮效率惩罚倍数
SWIMMING_VERTICAL_DOWN_MULTIPLIER = 1.5  # 下潜效率惩罚倍数
SWIMMING_MAX_DRAIN_RATE = 0.15  # 每秒最大消耗
SWIMMING_VERTICAL_UP_BASE_BODY_FORCE_COEFF = 0.02  # 上浮基础费力系数
SWIMMING_VERTICAL_DOWN_LOAD_RELIEF_COEFF = 0.50  # 负重能抵消浮力的比例
SWIMMING_SURVIVAL_STRESS_POWER = 15.0  # 生存压力常数（W）
SWIMMING_MAX_TOTAL_POWER = 2000.0  # 生理功率上限（W）
WET_WEIGHT_DURATION = 30.0  # 秒，上岸后湿重持续时间
WET_WEIGHT_MIN = 5.0  # kg，最小湿重
WET_WEIGHT_MAX = 10.0  # kg，最大湿重
SWIMMING_MIN_SPEED = 0.1  # m/s，游泳最小速度阈值
SWIMMING_VERTICAL_VELOCITY_THRESHOLD = -0.5  # m/s，垂直速度阈值（检测是否在水中）

# ==================== 环境因子常量 ====================
ENV_HEAT_STRESS_START_HOUR = 10.0  # 热应激开始时间（小时）
ENV_HEAT_STRESS_PEAK_HOUR = 14.0  # 热应激峰值时间（小时，正午）
ENV_HEAT_STRESS_END_HOUR = 18.0  # 热应激结束时间（小时）
ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.5  # 热应激最大倍数（50%消耗增加，提高环境影响）
ENV_HEAT_STRESS_BASE_MULTIPLIER = 1.0  # 热应激基础倍数（无影响）
ENV_HEAT_STRESS_INDOOR_REDUCTION = 0.5  # 室内热应激减少比例（50%）
ENV_RAIN_WEIGHT_MIN = 2.0  # kg，小雨时的湿重
ENV_RAIN_WEIGHT_MAX = 8.0  # kg，暴雨时的湿重
ENV_RAIN_WEIGHT_DURATION = 60.0  # 秒，停止降雨后湿重持续时间
ENV_RAIN_WEIGHT_DECAY_RATE = 0.0167  # 每秒衰减率（60秒内完全消失）
ENV_MAX_TOTAL_WET_WEIGHT = 10.0  # kg，总湿重上限（游泳+降雨）
ENV_UPDATE_INTERVAL_SECONDS = 5.0  # 环境因子检测频率（秒）

# 环境因子检测参数
ENV_CHECK_INTERVAL = 5.0  # 秒，环境因子检测间隔
ENV_INDOOR_CHECK_HEIGHT = 10.0  # 米，向上检测高度（判断是否有屋顶）

# ==================== 高级环境因子常量（v2.15.0）====================
# 降雨强度相关常量
ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE = 0.5  # kg/秒，基础湿重增加速率
ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT = 1.5  # 降雨强度指数（非线性增长）
ENV_RAIN_INTENSITY_THRESHOLD = 0.01  # 降雨强度阈值（低于此值不计算湿重）
ENV_RAIN_INTENSITY_HEAVY_THRESHOLD = 0.8  # 暴雨阈值（呼吸阻力触发）
ENV_RAIN_INTENSITY_BREATHING_PENALTY = 0.05  # 暴雨时的无氧代谢增加比例

# 环境因子惩罚最大值（与EnvironmentFactor类一致）
ENV_HEAT_STRESS_PENALTY_MAX = 0.3  # 最大热应激惩罚
ENV_COLD_STRESS_PENALTY_MAX = 0.2  # 最大冷应激惩罚
ENV_SURFACE_WETNESS_PENALTY_MAX = 0.15  # 最大地表湿度惩罚

# 风阻相关常量
ENV_WIND_RESISTANCE_COEFF = 0.05  # 风阻系数（体力消耗权重）
ENV_WIND_SPEED_THRESHOLD = 1.0  # m/s，风速阈值（低于此值忽略）
ENV_WIND_TAILWIND_BONUS = 0.02  # 顺风时的消耗减少比例
ENV_WIND_TAILWIND_SPEED_BONUS = 0.01  # 顺风时的速度加成比例

# 路面泥泞度相关常量
ENV_MUD_PENALTY_MAX = 0.4  # 最大泥泞惩罚（40%地形阻力增加）
ENV_MUD_SLIPPERY_THRESHOLD = 0.3  # 积水阈值（高于此值触发滑倒风险）
ENV_MUD_SPRINT_PENALTY = 0.1  # 泥泞时Sprint速度惩罚
ENV_MUD_SLIP_RISK_BASE = 0.001  # 基础滑倒风险（每0.2秒）

# 气温相关常量
ENV_TEMPERATURE_HEAT_THRESHOLD = 30.0  # °C，热应激阈值
ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02  # 每高1度，恢复率降低2%
ENV_TEMPERATURE_COLD_THRESHOLD = 0.0  # °C，冷应激阈值
ENV_TEMPERATURE_COLD_STATIC_PENALTY = 0.03  # 低温时静态消耗增加比例
ENV_TEMPERATURE_COLD_RECOVERY_PENALTY = 0.05  # 低温时恢复率降低比例

# 地表湿度相关常量
ENV_SURFACE_WETNESS_SOAK_RATE = 1.0  # kg/秒，趴下时的湿重增加速率
ENV_SURFACE_WETNESS_THRESHOLD = 0.1  # 积水阈值（高于此值触发湿重增加）
ENV_SURFACE_WETNESS_MARGINAL_DECAY_ADVANCE = 0.1  # 边际效应衰减提前触发比例
ENV_SURFACE_WETNESS_PRONE_PENALTY = 0.15  # 湿地趴下时的恢复惩罚
