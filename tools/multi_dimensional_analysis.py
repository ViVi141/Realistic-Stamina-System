#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
多维度趋势分析脚本
从多个维度观测体力-速度系统现象，以30KG战斗负重为基准
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置中文字体（用于图表）
rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False

# ==================== 游戏配置常量 ====================
GAME_MAX_SPEED = 5.2  # m/s，游戏最大速度
UPDATE_INTERVAL = 0.2  # 秒

# ==================== 角色特征常量 ====================
CHARACTER_WEIGHT = 90.0  # kg，角色体重（游戏内标准体重为90kg）
CHARACTER_AGE = 22.0  # 岁，角色年龄（基于ACFT标准，22-26岁男性）

# ==================== 健康状态/体能水平常量 ====================
# 基于个性化运动建模（Palumbo et al., 2018）
# 0.0 = 未训练（untrained）
# 0.5 = 普通健康（average fitness）
# 1.0 = 训练有素（well-trained）- 当前设置
FITNESS_LEVEL = 1.0  # 训练有素（well-trained）

# 健康状态对能量效率的影响系数
# 训练有素者（fitness=1.0）基础消耗减少，能量利用效率更高
# 优化：提升到35%，让训练有素者的能量效率显著提升
FITNESS_EFFICIENCY_COEFF = 0.35  # 35%效率提升（训练有素时，优化后）

# 健康状态对恢复速度的影响系数
# 训练有素者（fitness=1.0）恢复速度增加25%
FITNESS_RECOVERY_COEFF = 0.25  # 25%恢复速度提升（训练有素时）

# ==================== 运动持续时间影响（累积疲劳）参数 ====================
FATIGUE_ACCUMULATION_COEFF = 0.015  # 每分钟增加1.5%消耗
FATIGUE_START_TIME_MINUTES = 5.0  # 疲劳开始累积的时间（分钟）
FATIGUE_MAX_FACTOR = 2.0  # 最大疲劳因子（2.0倍）

# ==================== 代谢适应参数（Metabolic Adaptation）====================
AEROBIC_THRESHOLD = 0.6  # 有氧阈值（60% VO2max）
ANAEROBIC_THRESHOLD = 0.8  # 无氧阈值（80% VO2max）
AEROBIC_EFFICIENCY_FACTOR = 0.9  # 有氧区效率因子（90%，更高效）
MIXED_EFFICIENCY_FACTOR = 1.0  # 混合区效率因子（100%，标准）
ANAEROBIC_EFFICIENCY_FACTOR = 1.2  # 无氧区效率因子（120%，低效但高功率）

# ==================== 医学模型参数（双稳态-应激性能模型）====================
# 目标Run速度（m/s）- 双稳态-应激性能模型的核心目标速度
TARGET_RUN_SPEED = 3.7  # m/s
TARGET_RUN_SPEED_MULTIPLIER = TARGET_RUN_SPEED / GAME_MAX_SPEED  # 0.7115

# 意志力平台期阈值（体力百分比）
# 体力高于此值时，保持恒定目标速度（模拟意志力克服早期疲劳）
WILLPOWER_THRESHOLD = 0.25  # 25%

# 平滑过渡起点（体力百分比）
# 在25%-5%之间使用平滑过渡，避免突兀的"撞墙"效果
# 将25%设为"疲劳临界区"的起点，而不是终点
SMOOTH_TRANSITION_START = 0.25  # 25%（疲劳临界区起点）
SMOOTH_TRANSITION_END = 0.05  # 5%，平滑过渡结束点（真正的力竭点）

# 跛行速度倍数（最低速度）
MIN_LIMP_SPEED_MULTIPLIER = 1.0 / GAME_MAX_SPEED  # 1.0 m/s / 5.2 = 0.1923

# 旧模型参数（已废弃，保留用于兼容性）
TARGET_SPEED_MULTIPLIER = 0.920  # 已废弃
STAMINA_EXPONENT = 0.6  # 已废弃（双稳态模型不使用）

# 负重参数
# 负重对速度的惩罚系数（β）- 基于体重的真实模型
# 基于 US Army 实验数据（Knapik et al., 1996; Quesada et al., 2000; Vine et al., 2022）
# 真实模型：速度惩罚基于体重百分比，而不是最大负重百分比
# 速度惩罚 = β * (负重/体重)，其中 β = 0.20
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20  # 基于体重的速度惩罚系数（线性模型）
ENCUMBRANCE_SPEED_EXPONENT = 1.0  # 负重速度惩罚指数（1.0 = 线性）
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5  # 基于体重的体力消耗系数（40%体重负重时，消耗增加60%）
MIN_SPEED_MULTIPLIER = 0.15  # 最小速度倍数（防止体力完全耗尽时完全无法移动）
MAX_SPEED_MULTIPLIER = 1.0  # 最大速度倍数限制（防止超过游戏引擎限制）

# ==================== 体力消耗参数（基于精确 Pandolf 模型）====================
BASE_DRAIN_RATE = 0.00004  # 每0.2秒，基础消耗
SPEED_LINEAR_DRAIN_COEFF = 0.0001  # 速度线性项系数
SPEED_SQUARED_DRAIN_COEFF = 0.0001  # 速度平方项系数（优化后降低）
ENCUMBRANCE_BASE_DRAIN_COEFF = 0.001  # 负重基础消耗系数
ENCUMBRANCE_SPEED_DRAIN_COEFF = 0.0002  # 负重速度交互项系数
RECOVERY_RATE = 0.00015  # 每0.2秒

# ==================== Sprint（冲刺）相关参数 ====================
# v2.6.0优化：从15%提升到30%，确保负重状态下仍有明显速度差距
SPRINT_SPEED_BOOST = 0.30  # Sprint时速度比Run快30%（v2.6.0优化，从15%提升）
SPRINT_MAX_SPEED_MULTIPLIER = 1.0  # Sprint最高速度倍数（100% = 游戏最大速度5.2 m/s）
# v2.6.0优化：从2.5倍提升到3.0倍，平衡速度提升带来的优势
SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0  # Sprint时体力消耗是Run的3.0倍（v2.6.0优化，从2.5倍提升）

# ==================== Pandolf 模型参数 ====================
# 完整的 Pandolf 能量消耗模型（Pandolf et al., 1977）
# 公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
# 其中：
# - E = 能量消耗率（W/kg）
# - M = 总重量（身体重量 + 负重）
# - V = 速度（m/s）
# - G = 坡度（坡度百分比，0 = 平地，正数 = 上坡，负数 = 下坡）
# 注意：坡度项 G·(0.23 + 1.34·V²) 已直接整合在公式中

PANDOLF_BASE_COEFF = 2.7  # 基础系数（W/kg）
PANDOLF_VELOCITY_COEFF = 3.2  # 速度系数（W/kg）
PANDOLF_VELOCITY_OFFSET = 0.7  # 速度偏移（m/s）
PANDOLF_GRADE_BASE_COEFF = 0.23  # 坡度基础系数（W/kg）
PANDOLF_GRADE_VELOCITY_COEFF = 1.34  # 坡度速度系数（W/kg）

# 参考体重（用于计算相对重量倍数）
REFERENCE_WEIGHT = 70.0  # kg

# 能量到体力的转换系数
ENERGY_TO_STAMINA_COEFF = 0.000035  # 1 W/kg ≈ 0.000035 %/s（优化后，支持16-18分钟连续运行）

# ==================== 旧坡度参数（已废弃，保留用于兼容性）====================
# 注意：这些参数已不再使用，因为坡度已整合在Pandolf模型中
# 但为了兼容旧的函数（如calculate_slope_stamina_drain_multiplier），保留这些常量
SLOPE_UPHILL_COEFF = 0.08  # 上坡影响系数（已废弃）
SLOPE_DOWNHILL_COEFF = 0.03  # 下坡影响系数（已废弃）
SLOPE_MAX_MULTIPLIER = 2.0  # 最大坡度影响倍数（已废弃）
SLOPE_MIN_MULTIPLIER = 0.7  # 最小坡度影响倍数（已废弃）
ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15  # 负重×坡度交互系数（已废弃）

# ==================== 负重配置常量 ====================
BASE_WEIGHT = 1.36  # kg，基准负重（基础物品重量：衣服、鞋子等）
MAX_ENCUMBRANCE_WEIGHT = 40.5  # kg，最大负重（包含基准负重）
COMBAT_ENCUMBRANCE_WEIGHT = 30.0  # kg，战斗负重（包含基准负重，标准基准）

# ==================== 速度阈值参数 ====================
# 基于速度阈值的分段消耗率系统（Military Stamina System Model）
SPRINT_VELOCITY_THRESHOLD = 5.2  # m/s，Sprint速度阈值
RUN_VELOCITY_THRESHOLD = 3.7  # m/s，Run速度阈值（匹配15:27的2英里配速）
WALK_VELOCITY_THRESHOLD = 3.2  # m/s，Walk速度阈值

# 基于负重的动态速度阈值（m/s）
RECOVERY_THRESHOLD_NO_LOAD = 2.5  # m/s，空载时恢复体力阈值
DRAIN_THRESHOLD_COMBAT_LOAD = 1.5  # m/s，负重30kg时开始消耗体力的阈值
COMBAT_LOAD_WEIGHT = 30.0  # kg，战斗负重（用于计算动态阈值）

# 基础消耗率（pts/s，每秒消耗的点数）
# 转换为0.0-1.0范围的消耗率（每0.2秒）
SPRINT_BASE_DRAIN_RATE = 0.480  # pts/s（Sprint）
RUN_BASE_DRAIN_RATE = 0.075  # pts/s（Run，优化后约22分钟耗尽，支撑20分钟连续运行）
WALK_BASE_DRAIN_RATE = 0.060  # pts/s（Walk）
REST_RECOVERY_RATE = 0.250  # pts/s（Rest，恢复）

# 转换为每0.2秒的消耗率
SPRINT_DRAIN_PER_TICK = SPRINT_BASE_DRAIN_RATE / 100.0 * 0.2  # 每0.2秒
RUN_DRAIN_PER_TICK = RUN_BASE_DRAIN_RATE / 100.0 * 0.2  # 每0.2秒
WALK_DRAIN_PER_TICK = WALK_BASE_DRAIN_RATE / 100.0 * 0.2  # 每0.2秒
REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2  # 每0.2秒

# ==================== 2英里测试参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0, movement_type='run'):
    """根据体力百分比、负重和移动类型计算速度倍数（双稳态-应激性能模型）"""
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    
    # 计算Run速度（使用双稳态模型，带平滑过渡）
    run_speed_multiplier = 0.0
    if stamina_percent >= SMOOTH_TRANSITION_START:
        # 意志力平台期（25%-100%）：保持恒定目标速度（3.7 m/s）
        run_speed_multiplier = TARGET_RUN_SPEED_MULTIPLIER
    elif stamina_percent >= SMOOTH_TRANSITION_END:
        # 平滑过渡期（5%-25%）：使用SmoothStep建立缓冲区，避免突兀的"撞墙"效果
        # 让开始下降时更柔和，接近力竭时下降更快
        t = (stamina_percent - SMOOTH_TRANSITION_END) / (SMOOTH_TRANSITION_START - SMOOTH_TRANSITION_END)  # 0.0-1.0
        t = np.clip(t, 0.0, 1.0)
        smooth_factor = t * t * (3.0 - 2.0 * t)  # smoothstep函数
        run_speed_multiplier = MIN_LIMP_SPEED_MULTIPLIER + (TARGET_RUN_SPEED_MULTIPLIER - MIN_LIMP_SPEED_MULTIPLIER) * smooth_factor
    else:
        # 生理崩溃期（0%-5%）：速度快速线性下降到跛行速度
        collapse_factor = stamina_percent / SMOOTH_TRANSITION_END  # 0.0-1.0
        # 计算平滑过渡终点的速度（在5%体力时，此时smoothT=0，速度为MIN_LIMP_SPEED_MULTIPLIER）
        run_speed_multiplier = MIN_LIMP_SPEED_MULTIPLIER * collapse_factor
        # 确保不会低于最小速度
        run_speed_multiplier = max(run_speed_multiplier, MIN_LIMP_SPEED_MULTIPLIER * 0.8)  # 最低不低于跛行速度的80%
    
    # 负重主要影响"油耗"（体力消耗）而不是直接降低"最高档位"（速度）
    # 负重对速度的影响大幅降低，让30kg负重时仍能短时间跑3.7 m/s，只是消耗更快
    if encumbrance_percent > 0.0:
        encumbrance_percent = np.clip(encumbrance_percent, 0.0, 1.0)
        encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * np.power(encumbrance_percent, ENCUMBRANCE_SPEED_EXPONENT)
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)
        # 应用负重惩罚对速度上限的微调（降低影响至20%）
        run_speed_multiplier = run_speed_multiplier - (encumbrance_penalty * 0.2)
    
    if movement_type == 'idle':
        final_speed_multiplier = 0.0
    elif movement_type == 'walk':
        final_speed_multiplier = run_speed_multiplier * 0.7
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, 0.8)
    elif movement_type == 'sprint':
        # v2.6.0优化：Sprint速度完全基于Run速度进行加乘
        # Sprint速度 = Run速度 × (1 + 30%)，完全继承Run的双稳态-平台期逻辑
        # 负重惩罚系数：Sprint使用0.15（Run使用0.2），模拟爆发力克服阻力
        sprint_multiplier = 1.0 + SPRINT_SPEED_BOOST  # 1.30
        final_speed_multiplier = run_speed_multiplier * sprint_multiplier
        # 应用负重惩罚（Sprint的惩罚系数为0.15，比Run的0.2低）
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.15, SPRINT_MAX_SPEED_MULTIPLIER)
    else:  # 'run'
        final_speed_multiplier = run_speed_multiplier
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.15, 1.0)
    
    return final_speed_multiplier


def calculate_slope_stamina_drain_multiplier(slope_angle_degrees, body_mass_percent=0.0):
    """计算坡度对体力消耗的影响倍数（改进的多维度模型，包含负重交互）"""
    slope_angle_degrees = np.clip(slope_angle_degrees, -45.0, 45.0)
    
    base_slope_multiplier = 1.0
    
    if slope_angle_degrees > 0.0:
        base_slope_multiplier = 1.0 + SLOPE_UPHILL_COEFF * slope_angle_degrees
        base_slope_multiplier = np.clip(base_slope_multiplier, 1.0, SLOPE_MAX_MULTIPLIER)
    elif slope_angle_degrees < 0.0:
        base_slope_multiplier = 1.0 + SLOPE_DOWNHILL_COEFF * slope_angle_degrees
        base_slope_multiplier = np.clip(base_slope_multiplier, SLOPE_MIN_MULTIPLIER, 1.0)
    
    interaction_multiplier = 1.0
    if body_mass_percent > 0.0 and slope_angle_degrees > 0.0:
        interaction_term = ENCUMBRANCE_SLOPE_INTERACTION_COEFF * body_mass_percent * slope_angle_degrees
        interaction_multiplier = 1.0 + interaction_term
        interaction_multiplier = np.clip(interaction_multiplier, 1.0, 1.5)
    
    total_slope_multiplier = base_slope_multiplier * interaction_multiplier
    
    if slope_angle_degrees > 0.0:
        return np.clip(total_slope_multiplier, 1.0, 2.5)
    elif slope_angle_degrees < 0.0:
        return np.clip(total_slope_multiplier, 0.7, 1.0)
    else:
        return 1.0


def calculate_pandolf_energy_expenditure(velocity, current_weight, grade_percent=0.0):
    """
    计算完整的 Pandolf 能量消耗模型（Pandolf et al., 1977）
    
    完整公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
    其中：
    - E = 能量消耗率（W/kg）
    - M = 总重量（身体重量 + 负重）
    - V = 速度（m/s）
    - G = 坡度（坡度百分比，0 = 平地，正数 = 上坡，负数 = 下坡）
    
    注意：坡度项 G·(0.23 + 1.34·V²) 已直接整合在公式中
    
    Args:
        velocity: 当前速度（m/s）
        current_weight: 当前负重（kg），包含身体重量和额外负重
        grade_percent: 坡度百分比（例如，5% = 5.0，-15% = -15.0），默认0.0（平地）
    
    Returns:
        体力消耗率（%/s），每0.2秒的消耗率需要乘以 0.2
    """
    import math
    
    # 确保速度和重量有效
    velocity = max(velocity, 0.0)
    current_weight = max(current_weight, 0.0)
    
    # 如果速度为0或很小，返回恢复率（负数）
    if velocity < 0.1:
        return -0.0025  # 恢复率（负数）
    
    # 计算基础项：2.7 + 3.2·(V-0.7)²
    # 优化：对于顶尖运动员，运动时的经济性（Running Economy）更高
    # 添加 fitness bonus 来降低基础代谢项
    velocity_term = velocity - PANDOLF_VELOCITY_OFFSET
    velocity_squared_term = velocity_term * velocity_term
    fitness_bonus = 1.0 - (0.2 * FITNESS_LEVEL)  # 训练有素者基础代谢降低20%
    base_term = (PANDOLF_BASE_COEFF * fitness_bonus) + (PANDOLF_VELOCITY_COEFF * velocity_squared_term)
    
    # 计算坡度项：G·(0.23 + 1.34·V²)
    # 注意：坡度百分比需要转换为小数（例如 5% = 0.05）
    grade_decimal = grade_percent * 0.01  # 转换为小数
    velocity_squared = velocity * velocity
    grade_term = grade_decimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocity_squared))
    
    # 完整的 Pandolf 能量消耗率：E = M·(基础项 + 坡度项)
    # 注意：M 是总重量（kg），但我们使用相对于基准体重的倍数
    # 使用标准体重（70kg）作为参考，计算相对重量倍数
    weight_multiplier = current_weight / REFERENCE_WEIGHT
    weight_multiplier = np.clip(weight_multiplier, 0.5, 2.0)  # 限制在0.5-2.0倍之间
    
    energy_expenditure = weight_multiplier * (base_term + grade_term)
    
    # 将能量消耗率（W/kg）转换为体力消耗率（%/s）
    stamina_drain_rate = energy_expenditure * ENERGY_TO_STAMINA_COEFF
    
    # 限制消耗率在合理范围内（避免数值爆炸）
    stamina_drain_rate = np.clip(stamina_drain_rate, -0.005, 0.05)  # 最多每秒消耗5%
    
    return stamina_drain_rate


def calculate_base_drain_rate_by_velocity(velocity, current_weight=0.0):
    """
    根据当前速度和负重计算基础消耗率（使用临界动力概念，更温和的负重公式）
    
    负重消耗重新校准（Load Calibration）：
    - 基于临界动力（Critical Power）概念：将30kg定义为"标准战斗负载"
    - 非线性增长：负重对体力的消耗不再是简单的倍数，而是：基础消耗 + (负重/体重)^1.2 * 1.5
    """
    # 计算动态阈值（基于负重线性插值）
    if current_weight <= 0.0:
        dynamic_threshold = RECOVERY_THRESHOLD_NO_LOAD
    elif current_weight >= COMBAT_LOAD_WEIGHT:
        dynamic_threshold = DRAIN_THRESHOLD_COMBAT_LOAD
    else:
        t = current_weight / COMBAT_LOAD_WEIGHT
        dynamic_threshold = RECOVERY_THRESHOLD_NO_LOAD * (1.0 - t) + DRAIN_THRESHOLD_COMBAT_LOAD * t
    
    # 重新计算负重比例（基于体重）
    weight_ratio = current_weight / CHARACTER_WEIGHT if current_weight > 0.0 else 0.0
    
    # 负重影响因子：温和的增量
    load_factor = 1.0
    if current_weight > 0.0:
        weight_ratio_power = np.power(weight_ratio, 1.2)
        load_factor = 1.0 + (weight_ratio_power * 1.5)
    
    # 根据速度和动态阈值计算消耗率
    if velocity >= SPRINT_VELOCITY_THRESHOLD or velocity >= 5.0:
        return SPRINT_DRAIN_PER_TICK * load_factor
    elif velocity >= RUN_VELOCITY_THRESHOLD or velocity >= 3.2:
        # Run消耗（核心修正：大幅调低RUN的基础消耗，以补偿负重）
        return 0.00008 * load_factor  # 每0.2秒，调整后的Run消耗
    elif velocity >= dynamic_threshold:
        return 0.00002 * load_factor  # 每0.2秒，调整后的Walk消耗
    else:
        return -0.00025  # Rest恢复（负数），每0.2秒


def calculate_stamina_drain(current_speed, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, exercise_duration_minutes=0.0):
    """
    计算体力消耗率（基于完整的 Pandolf 模型，考虑累积疲劳和代谢适应）
    
    完整公式（Pandolf et al., 1977）：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
    其中：
    - E = 能量消耗率（W/kg）
    - M = 总重量（身体重量 + 负重）
    - V = 速度（m/s）
    - G = 坡度（坡度百分比，0 = 平地，正数 = 上坡，负数 = 下坡）
    
    注意：坡度项 G·(0.23 + 1.34·V²) 已直接整合在公式中，不需要单独的坡度倍数
    
    Sprint时体力消耗 = Run消耗 × 3.0倍（v2.6.0优化，从2.5倍提升）
    
    Args:
        current_speed: 当前速度（m/s）
        encumbrance_percent: 负重百分比（0.0-1.0），基于最大负重
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
        exercise_duration_minutes: 运动持续时间（分钟），用于计算累积疲劳
    
    Returns:
        体力消耗率（每0.2秒）
    """
    import math
    
    # Idle时恢复体力
    if movement_type == 'idle' or current_speed < 0.1:
        return -0.0025 * UPDATE_INTERVAL  # 恢复率（负数），每0.2秒
    
    # 计算当前负重（kg）
    current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
    # 总重量 = 身体重量 + 当前负重（包含基准重量）
    total_weight = CHARACTER_WEIGHT + current_weight_kg
    
    # 将坡度角度转换为坡度百分比
    grade_percent = math.tan(slope_angle_degrees * math.pi / 180.0) * 100.0 if slope_angle_degrees != 0.0 else 0.0
    
    # 使用完整的 Pandolf 模型计算基础消耗率
    base_drain_rate_per_second = calculate_pandolf_energy_expenditure(current_speed, total_weight, grade_percent)
    
    # ==================== 累积疲劳因子计算 ====================
    # 基于个性化运动建模（Palumbo et al., 2018）
    effective_exercise_duration = np.maximum(exercise_duration_minutes - FATIGUE_START_TIME_MINUTES, 0.0)
    fatigue_factor = 1.0 + (FATIGUE_ACCUMULATION_COEFF * effective_exercise_duration)
    fatigue_factor = np.clip(fatigue_factor, 1.0, FATIGUE_MAX_FACTOR)
    
    # ==================== 代谢适应计算 ====================
    # 基于个性化运动建模（Palumbo et al., 2018）
    speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
    if speed_ratio < AEROBIC_THRESHOLD:
        # 有氧区（<60% VO2max）：主要依赖脂肪，效率高
        metabolic_efficiency_factor = AEROBIC_EFFICIENCY_FACTOR  # 0.9（更高效）
    elif speed_ratio < ANAEROBIC_THRESHOLD:
        # 混合区（60-80% VO2max）：糖原+脂肪混合
        t = (speed_ratio - AEROBIC_THRESHOLD) / (ANAEROBIC_THRESHOLD - AEROBIC_THRESHOLD)
        metabolic_efficiency_factor = AEROBIC_EFFICIENCY_FACTOR + t * (ANAEROBIC_EFFICIENCY_FACTOR - AEROBIC_EFFICIENCY_FACTOR)
    else:
        # 无氧区（≥80% VO2max）：主要依赖糖原，效率低但功率高
        metabolic_efficiency_factor = ANAEROBIC_EFFICIENCY_FACTOR  # 1.2（低效但高功率）
    
    # ==================== 健康状态影响计算 ====================
    # 基于个性化运动建模（Palumbo et al., 2018）
    # 训练有素者（fitness=1.0）能量效率提高18%
    fitness_efficiency_factor = 1.0 - (FITNESS_EFFICIENCY_COEFF * FITNESS_LEVEL)
    fitness_efficiency_factor = np.clip(fitness_efficiency_factor, 0.7, 1.0)  # 限制在70%-100%之间
    
    # 综合效率因子 = 健康状态效率 × 代谢适应效率
    total_efficiency_factor = fitness_efficiency_factor * metabolic_efficiency_factor
    
    # 应用效率因子和疲劳因子
    # 注意：恢复时（base_drain_rate_per_second < 0），不应用效率因子（恢复不受效率影响）
    if base_drain_rate_per_second < 0.0:
        # 恢复时，直接使用恢复率（负数）
        total_drain_rate_per_second = base_drain_rate_per_second
    else:
        # 消耗时，应用效率因子和疲劳因子
        total_drain_rate_per_second = base_drain_rate_per_second * total_efficiency_factor * fatigue_factor
    
    # Sprint时体力消耗 = Run消耗 × 3.0倍（v2.6.0优化，从2.5倍提升）
    if movement_type == 'sprint':
        total_drain_rate_per_second = total_drain_rate_per_second * SPRINT_STAMINA_DRAIN_MULTIPLIER
    
    # 转换为每0.2秒的消耗率
    total_drain_rate_per_tick = total_drain_rate_per_second * UPDATE_INTERVAL
    
    # 限制消耗率在合理范围内
    total_drain_rate_per_tick = np.clip(total_drain_rate_per_tick, -0.005, 0.05)
    
    return total_drain_rate_per_tick


def simulate_2miles(encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0):
    """模拟跑2英里（包含累积疲劳和代谢适应）"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    exercise_duration_minutes = 0.0  # 运动持续时间（分钟）
    time_points = []
    stamina_values = []
    speed_values = []
    distance_values = []
    
    max_time = TARGET_TIME_SECONDS * 2
    
    while distance < DISTANCE_METERS and time < max_time:
        speed_mult = calculate_speed_multiplier_by_stamina(stamina, encumbrance_percent, movement_type)
        current_speed = GAME_MAX_SPEED * speed_mult
        
        time_points.append(time)
        stamina_values.append(stamina)
        speed_values.append(current_speed)
        distance_values.append(distance)
        
        distance += current_speed * UPDATE_INTERVAL
        
        if distance >= DISTANCE_METERS:
            break
        
        # 更新运动持续时间（仅在移动时累积）
        if current_speed > 0.05:
            exercise_duration_minutes += UPDATE_INTERVAL / 60.0
        else:
            exercise_duration_minutes = max(exercise_duration_minutes - (UPDATE_INTERVAL / 60.0 * 2.0), 0.0)
        
        drain_rate = calculate_stamina_drain(current_speed, encumbrance_percent, movement_type, slope_angle_degrees, exercise_duration_minutes)
        stamina = max(stamina - drain_rate, 0.0)
        
        time += UPDATE_INTERVAL
    
    return np.array(time_points), np.array(stamina_values), np.array(speed_values), np.array(distance_values), time, distance


def plot_multi_dimensional_analysis():
    """生成多维度趋势分析图"""
    fig = plt.figure(figsize=(20, 14))
    gs = fig.add_gridspec(4, 3, hspace=0.35, wspace=0.3)
    
    fig.suptitle('拟真体力-速度系统多维度趋势分析\n（以30KG战斗负重为基准）', fontsize=20, fontweight='bold')
    
    # 30KG战斗负重百分比
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    
    # ========== 图1: 不同负重下的2英里测试对比（以30KG为基准）==========
    ax1 = fig.add_subplot(gs[0, 0])
    weights_kg = [0.0, 15.0, 30.0, 40.5]
    encumbrance_levels = [w / MAX_ENCUMBRANCE_WEIGHT for w in weights_kg]
    colors = ['blue', 'green', 'orange', 'red']
    labels = ['0kg', '15kg', '30kg(战斗)', '40.5kg(最大)']
    
    for enc, color, label in zip(encumbrance_levels, colors, labels):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(enc, 'run', 0.0)
        line_width = 3 if enc == combat_enc_percent else 2
        ax1.plot(time_e / 60, stamina_e * 100, color=color, linewidth=line_width, label=label)
    
    ax1.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=1.5, alpha=0.5, label='目标时间')
    ax1.set_xlabel('时间（分钟）', fontsize=11)
    ax1.set_ylabel('体力（%）', fontsize=11)
    ax1.set_title('不同负重下的2英里测试体力消耗\n（30KG为战斗负重基准）', fontsize=12, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=9)
    
    # ========== 图2: 30KG战斗负重下不同移动类型的速度对比 ==========
    ax2 = fig.add_subplot(gs[0, 1])
    stamina_range = np.linspace(0.0, 1.0, 100)
    movement_types = ['walk', 'run', 'sprint']
    movement_labels = ['Walk', 'Run', 'Sprint']
    colors_movement = ['green', 'blue', 'red']
    
    for mt, label, color in zip(movement_types, movement_labels, colors_movement):
        speeds = [GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, combat_enc_percent, mt) for s in stamina_range]
        ax2.plot(stamina_range * 100, speeds, color=color, linewidth=2, label=label)
    
    ax2.axhline(y=GAME_MAX_SPEED * SPRINT_MAX_SPEED_MULTIPLIER, color='orange', linestyle='--', linewidth=1.5, alpha=0.7, label='Sprint最高速度')
    ax2.set_xlabel('体力（%）', fontsize=11)
    ax2.set_ylabel('速度（m/s）', fontsize=11)
    ax2.set_title(f'30KG战斗负重下不同移动类型速度对比', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=9)
    
    # ========== 图3: 30KG战斗负重下不同坡度的体力消耗对比 ==========
    ax3 = fig.add_subplot(gs[0, 2])
    slopes = [-10.0, -5.0, 0.0, 5.0, 10.0]
    colors_slope = ['cyan', 'lightblue', 'gray', 'orange', 'red']
    labels_slope = ['下坡10°', '下坡5°', '平地', '上坡5°', '上坡10°']
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    avg_speed = GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(0.6, combat_enc_percent, 'run')  # 60%体力时的平均速度
    
    for slope, color, label in zip(slopes, colors_slope, labels_slope):
        drains = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, 'run')
            speed = GAME_MAX_SPEED * speed_mult
            drain = calculate_stamina_drain(speed, combat_enc_percent, 'run', slope)
            drains.append(drain / UPDATE_INTERVAL * 100)  # 转换为每秒消耗率
        ax3.plot(stamina_test * 100, drains, color=color, linewidth=2, label=label)
    
    ax3.set_xlabel('体力（%）', fontsize=11)
    ax3.set_ylabel('体力消耗率（%/秒）', fontsize=11)
    ax3.set_title('30KG战斗负重下不同坡度的体力消耗', fontsize=12, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(fontsize=9)
    
    # ========== 图4: 30KG战斗负重下不同移动类型和坡度的速度对比 ==========
    ax4 = fig.add_subplot(gs[1, :])
    scenarios = [
        ('run', 0.0, 'Run-平地', 'blue', '-'),
        ('run', 5.0, 'Run-上坡5°', 'orange', '-'),
        ('run', -5.0, 'Run-下坡5°', 'cyan', '-'),
        ('sprint', 0.0, 'Sprint-平地', 'red', '--'),
        ('sprint', 5.0, 'Sprint-上坡5°', 'purple', '--'),
        ('sprint', -5.0, 'Sprint-下坡5°', 'pink', '--'),
    ]
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    
    for movement_type, slope, label, color, linestyle in scenarios:
        speeds = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, movement_type)
            speed = GAME_MAX_SPEED * speed_mult
            speeds.append(speed)
        ax4.plot(stamina_test * 100, speeds, color=color, linewidth=2, linestyle=linestyle, label=label)
    
    ax4.axhline(y=TARGET_RUN_SPEED, color='gray', linestyle=':', linewidth=1, alpha=0.5, label='目标Run速度 (3.7 m/s)')
    ax4.set_xlabel('体力（%）', fontsize=12)
    ax4.set_ylabel('速度（m/s）', fontsize=12)
    ax4.set_title('30KG战斗负重基准：不同移动类型和坡度下的速度对比', fontsize=13, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(fontsize=9, loc='upper right', ncol=3)
    
    # ========== 图5: 30KG战斗负重下不同移动类型和坡度的体力消耗对比 ==========
    ax5 = fig.add_subplot(gs[2, :])
    
    for movement_type, slope, label, color, linestyle in scenarios:
        drains = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, movement_type)
            speed = GAME_MAX_SPEED * speed_mult
            drain = calculate_stamina_drain(speed, combat_enc_percent, movement_type, slope)
            drains.append(drain / UPDATE_INTERVAL * 100)  # 转换为每秒消耗率
        ax5.plot(stamina_test * 100, drains, color=color, linewidth=2, linestyle=linestyle, label=label)
    
    ax5.set_xlabel('体力（%）', fontsize=12)
    ax5.set_ylabel('体力消耗率（%/秒）', fontsize=12)
    ax5.set_title('30KG战斗负重基准：不同移动类型和坡度下的体力消耗对比', fontsize=13, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    ax5.legend(fontsize=9, loc='upper left', ncol=3)
    
    # ========== 图6: 30KG战斗负重下不同移动类型的2英里测试完成时间 ==========
    ax6 = fig.add_subplot(gs[3, 0])
    movement_types_test = ['walk', 'run', 'sprint']
    movement_labels_test = ['Walk', 'Run', 'Sprint']
    colors_test = ['green', 'blue', 'red']
    completion_times = []
    final_staminas = []
    
    for mt, label, color in zip(movement_types_test, movement_labels_test, colors_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, mt, 0.0)
        if final_dist >= DISTANCE_METERS:
            completion_times.append(final_time / 60)
            final_staminas.append(stamina_e[-1] * 100 if len(stamina_e) > 0 else 0)
        else:
            completion_times.append(None)
            final_staminas.append(None)
    
    valid_indices = [i for i, t in enumerate(completion_times) if t is not None]
    valid_times = [completion_times[i] for i in valid_indices]
    valid_labels = [movement_labels_test[i] for i in valid_indices]
    valid_colors = [colors_test[i] for i in valid_indices]
    
    if valid_times:
        bars = ax6.bar(valid_labels, valid_times, color=valid_colors, alpha=0.7, width=0.6)
    else:
        # 如果所有情况下都无法完成，显示提示文本
        ax6.text(0.5, 0.5, '30KG负重下\n无法完成2英里测试', 
                ha='center', va='center', fontsize=12, color='red', 
                transform=ax6.transAxes, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    ax6.axhline(y=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    ax6.set_ylabel('完成时间（分钟）', fontsize=11)
    ax6.set_title('30KG战斗负重下不同移动类型的2英里测试', fontsize=12, fontweight='bold')
    ax6.grid(True, alpha=0.3, axis='y')
    ax6.legend(fontsize=9)
    
    # ========== 图7: 30KG战斗负重下不同坡度的2英里测试完成时间 ==========
    ax7 = fig.add_subplot(gs[3, 1])
    slopes_test = [-5.0, 0.0, 5.0]
    labels_slope_test = ['下坡5°', '平地', '上坡5°']
    colors_slope_test = ['cyan', 'gray', 'orange']
    completion_times_slope = []
    
    for slope, label, color in zip(slopes_test, labels_slope_test, colors_slope_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, 'run', slope)
        if final_dist >= DISTANCE_METERS:
            completion_times_slope.append(final_time / 60)
        else:
            completion_times_slope.append(None)
    
    valid_indices_slope = [i for i, t in enumerate(completion_times_slope) if t is not None]
    valid_times_slope = [completion_times_slope[i] for i in valid_indices_slope]
    valid_labels_slope = [labels_slope_test[i] for i in valid_indices_slope]
    valid_colors_slope = [colors_slope_test[i] for i in valid_indices_slope]
    
    if valid_times_slope:
        bars = ax7.bar(valid_labels_slope, valid_times_slope, color=valid_colors_slope, alpha=0.7, width=0.6)
    else:
        # 如果所有情况下都无法完成，显示提示文本
        ax7.text(0.5, 0.5, '30KG负重下\n无法完成2英里测试', 
                ha='center', va='center', fontsize=12, color='red', 
                transform=ax7.transAxes, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    ax7.axhline(y=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    ax7.set_ylabel('完成时间（分钟）', fontsize=11)
    ax7.set_title('30KG战斗负重下不同坡度的2英里测试', fontsize=12, fontweight='bold')
    ax7.grid(True, alpha=0.3, axis='y')
    ax7.legend(fontsize=9)
    
    # ========== 图8: 30KG战斗负重基准下的速度-体力-消耗率三维关系 ==========
    ax8 = fig.add_subplot(gs[3, 2])
    stamina_range_3d = np.linspace(0.2, 1.0, 50)
    
    # 计算速度和消耗率
    speeds_3d = []
    drains_3d = []
    for st in stamina_range_3d:
        speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, 'run')
        speed = GAME_MAX_SPEED * speed_mult
        speeds_3d.append(speed)
        drain = calculate_stamina_drain(speed, combat_enc_percent, 'run', 0.0)
        drains_3d.append(drain / UPDATE_INTERVAL * 100)
    
    ax8_twin = ax8.twinx()
    line1 = ax8.plot(stamina_range_3d * 100, speeds_3d, 'b-', linewidth=2, label='速度')
    line2 = ax8_twin.plot(stamina_range_3d * 100, drains_3d, 'r-', linewidth=2, label='消耗率')
    
    ax8.set_xlabel('体力（%）', fontsize=11)
    ax8.set_ylabel('速度（m/s）', fontsize=11, color='b')
    ax8_twin.set_ylabel('体力消耗率（%/秒）', fontsize=11, color='r')
    ax8.set_title('30KG战斗负重：速度与消耗率关系', fontsize=12, fontweight='bold')
    ax8.grid(True, alpha=0.3)
    
    lines = line1 + line2
    labels = [line.get_label() for line in lines]
    ax8.legend(lines, labels, loc='upper right', fontsize=9)
    
    # 保存图表
    output_file = 'multi_dimensional_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n多维度趋势图已保存为: {output_file}")
    
    # 打印统计信息
    print("\n" + "="*70)
    print("多维度分析统计（以30KG战斗负重为基准）:")
    print("="*70)
    print(f"战斗负重基准: {COMBAT_ENCUMBRANCE_WEIGHT} kg ({combat_enc_percent*100:.1f}%)")
    print(f"最大负重: {MAX_ENCUMBRANCE_WEIGHT} kg")
    print()
    
    # 30KG战斗负重下不同移动类型的2英里测试
    print("30KG战斗负重下不同移动类型的2英里测试:")
    print("-"*70)
    for mt, label in zip(movement_types_test, movement_labels_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, mt, 0.0)
        if final_dist >= DISTANCE_METERS:
            print(f"{label}: 完成时间 {final_time:.1f}秒 ({final_time/60:.2f}分钟), 最终体力 {stamina_e[-1]*100:.1f}%, 平均速度 {np.mean(speed_e):.2f} m/s")
        else:
            print(f"{label}: 无法完成（距离 {final_dist:.1f}米/{DISTANCE_METERS}米）")
    print()
    
    # 30KG战斗负重下不同坡度的2英里测试
    print("30KG战斗负重下不同坡度的2英里测试（Run模式）:")
    print("-"*70)
    for slope, label in zip(slopes_test, labels_slope_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, 'run', slope)
        if final_dist >= DISTANCE_METERS:
            print(f"{label}: 完成时间 {final_time:.1f}秒 ({final_time/60:.2f}分钟), 最终体力 {stamina_e[-1]*100:.1f}%, 平均速度 {np.mean(speed_e):.2f} m/s")
        else:
            print(f"{label}: 无法完成（距离 {final_dist:.1f}米/{DISTANCE_METERS}米）")
    print("="*70)
    
    # 显示图表
    plt.show()


if __name__ == "__main__":
    plot_multi_dimensional_analysis()
