#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Realistic Stamina System (RSS) - 综合趋势图生成器
综合趋势图生成器
包含2英里测试、不同负重情况对比、恢复速度分析等
"""

import numpy as np
import sys
import os

# Headless-friendly backend (do not require GUI)
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 添加当前目录到路径，以便导入常量模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stamina_constants import *

# 设置中文字体（用于图表）
rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False
rcParams['figure.titlesize'] = 15
rcParams['axes.titlesize'] = 10
rcParams['axes.labelsize'] = 9
rcParams['legend.fontsize'] = 8
rcParams['xtick.labelsize'] = 8
rcParams['ytick.labelsize'] = 8

# Whether to show figures interactively (default off; scripts are for PNG generation)
SHOW_PLOTS = False

# ==================== 所有常量已从stamina_constants导入 ====================
# 兼容性保留（用于向后兼容）
BASE_DRAIN_RATE = RUN_DRAIN_PER_TICK  # 基础消耗率（每0.2秒）
RECOVERY_RATE = BASE_RECOVERY_RATE  # 恢复率（每0.2秒）
TARGET_SPEED_MULTIPLIER = TARGET_RUN_SPEED_MULTIPLIER  # 已废弃，保留用于兼容性

# ==================== 旧坡度参数（已废弃，保留用于兼容性）====================
# 注意：这些参数已不再使用，因为坡度已整合在Pandolf模型中
# 但为了兼容旧的函数（如calculate_slope_stamina_drain_multiplier），保留这些常量
SLOPE_UPHILL_COEFF = 0.08  # 上坡影响系数（已废弃）
SLOPE_DOWNHILL_COEFF = 0.03  # 下坡影响系数（已废弃）
SLOPE_MAX_MULTIPLIER = 2.0  # 最大坡度影响倍数（已废弃）
SLOPE_MIN_MULTIPLIER = 0.7  # 最小坡度影响倍数（已废弃）
ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15  # 负重×坡度交互系数（已废弃）

# ==================== 2英里测试参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0, movement_type='run', current_weight_kg=0.0):
    """
    根据体力百分比、负重和移动类型计算速度倍数（双稳态-应激性能模型）
    
    速度性能分段（Performance Plateau）：
    1. 平台期（Willpower Zone, Stamina 25% - 100%）：
       只要体力高于25%，士兵可以强行维持设定的目标速度（3.7 m/s）。
       这模拟了士兵在比赛或战斗中通过意志力克服早期疲劳。
    2. 衰减期（Failure Zone, Stamina 0% - 25%）：
       只有当体力掉入最后25%时，生理机能开始真正崩塌，速度迅速线性下降到跛行。
    - 负重惩罚：P_enc = β × (W/体重)，其中 β = 0.20（基于体重的速度惩罚系数）
    - Run速度：S_run = S_base × (1 - P_enc)
    - Sprint速度：S_sprint = S_run × (1 + SPRINT_BOOST)，限制在SPRINT_MAX内
    - Walk速度：S_walk = S_run × 0.7
    
    移动类型：
    - 'idle': 0.0
    - 'walk': Run速度 × 0.7
    - 'run': 基础速度 × (1 - 负重惩罚)
    - 'sprint': Run速度 × 1.30（v2.6.0优化，从1.15提升），最高限制在100%
    
    Args:
        stamina_percent: 体力百分比 (0.0-1.0)
        encumbrance_percent: 负重百分比（基于最大负重，用于兼容性）
        movement_type: 移动类型
        current_weight_kg: 当前负重（kg），如果提供则基于体重百分比计算
    """
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
    
    # 应用真实负重惩罚：基于体重百分比（降低影响）
    if current_weight_kg > 0.0:
        # 计算有效负重（减去基准重量，基准重量是基本战斗装备）
        effective_weight = np.maximum(current_weight_kg - BASE_WEIGHT, 0.0)
        # 基于体重的真实模型：P_enc = β × (有效负重/体重)
        body_mass_percent = effective_weight / CHARACTER_WEIGHT
        encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * body_mass_percent
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)  # 最多减少50%速度
        # 负重主要影响"油耗"（体力消耗）而不是直接降低"最高档位"（速度）
        # 应用负重惩罚对速度上限的微调（降低影响至20%）
        run_speed_multiplier = run_speed_multiplier - (encumbrance_penalty * 0.2)
    elif encumbrance_percent > 0.0:
        # 兼容旧模型：如果只提供了encumbrance_percent，则转换为体重百分比
        current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
        effective_weight = np.maximum(current_weight_kg - BASE_WEIGHT, 0.0)
        body_mass_percent = effective_weight / CHARACTER_WEIGHT
        encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * body_mass_percent
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)
        # 应用负重惩罚对速度上限的微调（降低影响至20%）
        run_speed_multiplier = run_speed_multiplier - (encumbrance_penalty * 0.2)
    
    # 根据移动类型调整速度
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
        # 注意：encumbrance_penalty已在run_speed_multiplier中应用（0.2倍），这里需要调整
        # 为了简化，我们直接使用run_speed_multiplier的结果，它已经包含了负重影响
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.15, SPRINT_MAX_SPEED_MULTIPLIER)
    else:  # 'run'
        final_speed_multiplier = run_speed_multiplier
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, 1.0)
    
    return final_speed_multiplier


def calculate_slope_stamina_drain_multiplier(slope_angle_degrees, body_mass_percent=0.0):
    """
    计算坡度对体力消耗的影响倍数（改进的多维度模型，包含负重交互）
    
    改进的数学模型：基于 Pandolf 负重行走代谢成本模型
    完整 Pandolf 模型公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
    
    改进模型：坡度影响 = 基础坡度影响 + 负重×坡度交互项
    基础坡度影响 = 1 + α·|G|·sign(G)
    负重×坡度交互 = interaction_coeff × (负重/体重) × |G|
    
    Args:
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
        body_mass_percent: 负重占体重的百分比（0.0-1.0+），用于计算交互项
    
    Returns:
        坡度影响倍数（0.7-2.5），表示相对于平地时的消耗倍数
    """
    slope_angle_degrees = np.clip(slope_angle_degrees, -45.0, 45.0)
    
    # 基础坡度影响（不考虑负重）
    base_slope_multiplier = 1.0
    
    if slope_angle_degrees > 0.0:
        # 上坡：消耗增加
        base_slope_multiplier = 1.0 + SLOPE_UPHILL_COEFF * slope_angle_degrees
        base_slope_multiplier = np.clip(base_slope_multiplier, 1.0, SLOPE_MAX_MULTIPLIER)
    elif slope_angle_degrees < 0.0:
        # 下坡：消耗减少
        base_slope_multiplier = 1.0 + SLOPE_DOWNHILL_COEFF * slope_angle_degrees
        base_slope_multiplier = np.clip(base_slope_multiplier, SLOPE_MIN_MULTIPLIER, 1.0)
    
    # 负重×坡度交互项（负重越大，坡度影响越明显）
    interaction_multiplier = 1.0
    if body_mass_percent > 0.0 and slope_angle_degrees > 0.0:  # 只在上坡且有负重时应用交互项
        # 交互项：负重×坡度
        interaction_term = ENCUMBRANCE_SLOPE_INTERACTION_COEFF * body_mass_percent * slope_angle_degrees
        interaction_multiplier = 1.0 + interaction_term
        interaction_multiplier = np.clip(interaction_multiplier, 1.0, 1.5)
    
    # 综合坡度影响 = 基础坡度影响 × 交互项
    total_slope_multiplier = base_slope_multiplier * interaction_multiplier
    
    # 最终限制：上坡最大2.5倍，下坡最小0.7倍
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


def calculate_stamina_drain(current_speed, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, current_weight_kg=0.0, exercise_duration_minutes=0.0):
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
        current_weight_kg: 当前负重（kg），如果提供则使用，否则从encumbrance_percent计算
        exercise_duration_minutes: 运动持续时间（分钟），用于计算累积疲劳
    """
    import math
    
    # Idle时恢复体力
    if movement_type == 'idle' or current_speed < 0.1:
        return -0.0025 * UPDATE_INTERVAL  # 恢复率（负数），每0.2秒
    
    # 计算当前负重（kg）
    if current_weight_kg <= 0.0:
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
    """模拟跑2英里（基于体重的真实模型，包含累积疲劳和代谢适应）"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    exercise_duration_minutes = 0.0  # 运动持续时间（分钟）
    time_points = []
    stamina_values = []
    speed_values = []
    distance_values = []
    
    # 计算当前负重（kg）
    current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
    
    max_time = TARGET_TIME_SECONDS * 2
    
    while distance < DISTANCE_METERS and time < max_time:
        # 计算当前速度倍数（根据移动类型，基于体重百分比）
        speed_mult = calculate_speed_multiplier_by_stamina(stamina, encumbrance_percent, movement_type, current_weight_kg)
        current_speed = GAME_MAX_SPEED * speed_mult
        
        # 记录数据
        time_points.append(time)
        stamina_values.append(stamina)
        speed_values.append(current_speed)
        distance_values.append(distance)
        
        # 更新距离
        distance += current_speed * UPDATE_INTERVAL
        
        # 如果已跑完距离，停止
        if distance >= DISTANCE_METERS:
            break
        
        # 更新运动持续时间（仅在移动时累积）
        if current_speed > 0.05:
            exercise_duration_minutes += UPDATE_INTERVAL / 60.0
        else:
            # 静止时，疲劳快速恢复（恢复速度是累积速度的2倍）
            exercise_duration_minutes = max(exercise_duration_minutes - (UPDATE_INTERVAL / 60.0 * 2.0), 0.0)
        
        # 更新体力（根据移动类型和坡度，基于体重百分比，包含累积疲劳和代谢适应）
        drain_rate = calculate_stamina_drain(current_speed, encumbrance_percent, movement_type, slope_angle_degrees, 
                                            current_weight_kg, exercise_duration_minutes)
        stamina = max(stamina - drain_rate, 0.0)
        
        time += UPDATE_INTERVAL
    
    return np.array(time_points), np.array(stamina_values), np.array(speed_values), np.array(distance_values), time, distance


def calculate_multi_dimensional_recovery_rate(stamina_percent, rest_duration_minutes, exercise_duration_minutes, current_weight=0.0, stance=0):
    """
    计算多维度恢复率（基于个性化运动建模和生理学恢复模型）
    
    考虑多个维度：
    1. 当前体力百分比（非线性恢复）：体力越低恢复越快，体力越高恢复越慢
    2. 健康状态/训练水平：训练有素者恢复更快
    3. 休息时间：刚停止运动时恢复快（快速恢复期），长时间休息后恢复慢（慢速恢复期）
    4. 年龄：年轻者恢复更快
    5. 累积疲劳恢复：运动后的疲劳需要时间恢复
    6. 姿态影响：不同休息姿势对肌肉紧张度和血液循环的影响
    7. 负重对恢复的静态剥夺：负重越大，恢复越慢
    8. 边际效应衰减：体力>80%时恢复速度显著降低
    """
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    rest_duration_minutes = np.maximum(rest_duration_minutes, 0.0)
    exercise_duration_minutes = np.maximum(exercise_duration_minutes, 0.0)
    
    # ==================== 深度生理压制：最低体力阈值限制 ====================
    rest_duration_seconds = rest_duration_minutes * 60.0
    if stamina_percent < MIN_RECOVERY_STAMINA_THRESHOLD and rest_duration_seconds < MIN_RECOVERY_REST_TIME_SECONDS:
        return 0.0  # 体力低于20%且休息时间不足10秒时，不恢复
    
    # 1. 基础恢复率（基于当前体力百分比，非线性）
    stamina_recovery_multiplier = 1.0 + (RECOVERY_NONLINEAR_COEFF * (1.0 - stamina_percent))
    base_recovery_rate = BASE_RECOVERY_RATE * stamina_recovery_multiplier
    
    # 2. 健康状态/训练水平影响
    fitness_recovery_multiplier = 1.0 + (FITNESS_RECOVERY_COEFF * FITNESS_LEVEL)
    fitness_recovery_multiplier = np.clip(fitness_recovery_multiplier, 1.0, 1.5)
    
    # 3. 休息时间影响（快速恢复期 vs 中等恢复期 vs 慢速恢复期）
    if rest_duration_minutes <= FAST_RECOVERY_DURATION_MINUTES:
        rest_time_multiplier = FAST_RECOVERY_MULTIPLIER
    elif rest_duration_minutes <= MEDIUM_RECOVERY_START_MINUTES + MEDIUM_RECOVERY_DURATION_MINUTES:
        rest_time_multiplier = MEDIUM_RECOVERY_MULTIPLIER
    elif rest_duration_minutes >= SLOW_RECOVERY_START_MINUTES:
        transition_duration = 10.0
        transition_progress = np.minimum((rest_duration_minutes - SLOW_RECOVERY_START_MINUTES) / transition_duration, 1.0)
        rest_time_multiplier = 1.0 - (transition_progress * (1.0 - SLOW_RECOVERY_MULTIPLIER))
    else:
        rest_time_multiplier = 1.0
    
    # 4. 年龄影响
    age_recovery_multiplier = 1.0 + (AGE_RECOVERY_COEFF * (AGE_REFERENCE - CHARACTER_AGE) / AGE_REFERENCE)
    age_recovery_multiplier = np.clip(age_recovery_multiplier, 0.8, 1.2)
    
    # 5. 累积疲劳恢复影响
    fatigue_recovery_penalty = FATIGUE_RECOVERY_PENALTY * np.minimum(exercise_duration_minutes / FATIGUE_RECOVERY_DURATION_MINUTES, 1.0)
    fatigue_recovery_multiplier = 1.0 - fatigue_recovery_penalty
    fatigue_recovery_multiplier = np.clip(fatigue_recovery_multiplier, 0.7, 1.0)
    
    # 6. 姿态恢复加成
    stance_recovery_multiplier = 1.0
    if stance == 0:
        stance_recovery_multiplier = STANDING_RECOVERY_MULTIPLIER
    elif stance == 1:
        stance_recovery_multiplier = CROUCHING_RECOVERY_MULTIPLIER
    elif stance == 2:
        stance_recovery_multiplier = PRONE_RECOVERY_MULTIPLIER
    
    # ==================== 深度生理压制：负重对恢复的静态剥夺机制 ====================
    load_recovery_penalty = 0.0
    if current_weight > 0.0:
        load_ratio = current_weight / BODY_TOLERANCE_BASE
        load_ratio = np.clip(load_ratio, 0.0, 2.0)
        load_recovery_penalty = np.power(load_ratio, LOAD_RECOVERY_PENALTY_EXPONENT) * LOAD_RECOVERY_PENALTY_COEFF
    
    # ==================== 深度生理压制：边际效应衰减机制 ====================
    marginal_decay_multiplier = 1.0
    if stamina_percent > MARGINAL_DECAY_THRESHOLD:
        marginal_decay_multiplier = MARGINAL_DECAY_COEFF - stamina_percent
        marginal_decay_multiplier = np.clip(marginal_decay_multiplier, 0.2, 1.0)
    
    # 综合恢复率（深度生理压制版本）
    # 核心概念：从"净增加"改为"代谢净值"
    # 最终恢复率 = (基础恢复率 × 姿态修正) - (负重压制 + 氧债惩罚)
    total_recovery_rate = base_recovery_rate * fitness_recovery_multiplier * rest_time_multiplier * age_recovery_multiplier * fatigue_recovery_multiplier * stance_recovery_multiplier * marginal_decay_multiplier
    
    # 应用负重静态剥夺
    total_recovery_rate = total_recovery_rate - load_recovery_penalty
    
    # 确保恢复率不为负
    total_recovery_rate = np.maximum(total_recovery_rate, 0.0)
    
    return total_recovery_rate


def simulate_recovery(start_stamina=0.2, target_stamina=1.0, exercise_duration_minutes=0.0, current_weight=0.0, stance=0):
    """模拟体力恢复过程（使用多维度恢复模型）"""
    stamina = start_stamina
    time = 0.0
    rest_duration_minutes = 0.0
    time_points = []
    stamina_values = []
    
    while stamina < target_stamina:
        time_points.append(time)
        stamina_values.append(stamina)
        
        # 使用多维度恢复模型计算恢复率
        recovery_rate = calculate_multi_dimensional_recovery_rate(stamina, rest_duration_minutes, exercise_duration_minutes, current_weight, stance)
        stamina = min(stamina + recovery_rate, target_stamina)
        
        time += UPDATE_INTERVAL
        rest_duration_minutes += UPDATE_INTERVAL / 60.0
        
        # 逐渐减少累积疲劳（疲劳恢复速度是累积速度的2倍）
        if exercise_duration_minutes > 0.0:
            exercise_duration_minutes = max(exercise_duration_minutes - (UPDATE_INTERVAL / 60.0 * 2.0), 0.0)
        
        if time > 1200:  # 最多模拟20分钟
            break
    
    return np.array(time_points), np.array(stamina_values)


def plot_2mile_test():
    """生成2英里测试图"""
    fig, ax1 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist = simulate_2miles(0.0, 'run')
    
    ax1_twin = ax1.twinx()
    line1 = ax1.plot(time_2m / 60, stamina_2m * 100, 'b-', linewidth=2, label='体力')
    line2 = ax1_twin.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='速度')
    line3 = ax1_twin.plot(time_2m / 60, dist_2m / 1000, 'g--', linewidth=1.5, label='累计距离(km)')
    
    ax1.axhline(y=50, color='b', linestyle=':', alpha=0.5)
    ax1.axhline(y=25, color='b', linestyle=':', alpha=0.5)
    ax1.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    
    ax1.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax1.set_ylabel('体力（%） / Stamina (%)', fontsize=12, color='b')
    ax1_twin.set_ylabel('速度（m/s） / Speed (m/s) | 距离（km） / Distance (km)', fontsize=12)
    ax1.set_title(
        f'2英里测试 / 2-mile test\n'
        f'完成时间 / Finish: {final_time:.1f}s ({final_time/60:.2f}min)',
        fontsize=14,
        fontweight='bold'
    )
    ax1.grid(True, alpha=0.3)
    
    lines = line1 + line2 + line3
    labels = [line.get_label() for line in lines]
    ax1.legend(lines, labels, loc='upper right', fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_2mile_test.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")
    return time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist


def plot_load_comparison():
    """生成不同负重对比图"""
    fig, ax2 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    weights_kg = [0.0, 15.0, 30.0, 40.5]
    encumbrance_levels = [w / MAX_ENCUMBRANCE_WEIGHT for w in weights_kg]
    colors = ['blue', 'green', 'orange', 'red']
    labels = ['0kg', '15kg', '30kg(战斗)', '40.5kg(最大)']
    
    for enc, color, label in zip(encumbrance_levels, colors, labels):
        time_e, stamina_e, speed_e, dist_e, _, _ = simulate_2miles(enc, 'run', 0.0)
        line_width = 3 if enc == COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT else 2
        ax2.plot(time_e / 60, stamina_e * 100, color=color, linewidth=line_width, label=label)
    
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    ax2.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=1.5, alpha=0.5, label='目标时间')
    
    ax2.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax2.set_ylabel('体力（%） / Stamina (%)', fontsize=12)
    ax2.set_title(
        '不同负重下的体力消耗对比 / Stamina drain by load\n'
        '（30KG为战斗负重基准 / 30kg baseline）',
        fontsize=14,
        fontweight='bold'
    )
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_load_comparison.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_movement_type_speed():
    """生成不同移动类型速度对比图"""
    fig, ax3 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    stamina_range = np.linspace(0.0, 1.0, 100)
    speed_run = [GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, 0.0, 'run') for s in stamina_range]
    speed_sprint = [GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, 0.0, 'sprint') for s in stamina_range]
    speed_walk = [GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, 0.0, 'walk') for s in stamina_range]
    
    ax3.plot(stamina_range * 100, speed_run, 'b-', linewidth=2, label='Run（跑步）')
    ax3.plot(stamina_range * 100, speed_sprint, 'r-', linewidth=2, label='Sprint（冲刺）')
    ax3.plot(stamina_range * 100, speed_walk, 'g-', linewidth=2, label='Walk（行走）')
    ax3.axhline(y=GAME_MAX_SPEED * SPRINT_MAX_SPEED_MULTIPLIER, color='orange', 
                linestyle='--', linewidth=1.5, label=f'Sprint最高速度 ({GAME_MAX_SPEED * SPRINT_MAX_SPEED_MULTIPLIER:.2f} m/s)')
    ax3.axhline(y=TARGET_RUN_SPEED, color='blue', 
                linestyle=':', linewidth=1.5, alpha=0.7, label=f'Run目标速度 ({TARGET_RUN_SPEED:.2f} m/s)')
    
    ax3.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax3.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax3.set_title('不同移动类型的速度对比 / Speed by movement type', fontsize=14, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_movement_type_speed.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_recovery_analysis():
    """生成体力恢复速度分析图"""
    fig, ax4 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    start_levels = [0.1, 0.2, 0.3, 0.5, 0.7]
    colors_recovery = ['red', 'orange', 'yellow', 'lightblue', 'lightgreen']
    
    for start, color in zip(start_levels, colors_recovery):
        time_r, stamina_r = simulate_recovery(start, 1.0, exercise_duration_minutes=0.0)
        ax4.plot(time_r / 60, stamina_r * 100, color=color, linewidth=2, 
                label=f'从 {start*100:.0f}% 恢复')
    
    ax4.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax4.set_ylabel('体力（%） / Stamina (%)', fontsize=12)
    ax4.set_title('体力恢复速度分析 / Recovery analysis', fontsize=14, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(fontsize=10, framealpha=0.85)
    ax4.set_ylim([0, 105])
    
    output_file = 'comprehensive_recovery_analysis.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_2mile_speed(time_2m, speed_2m):
    """生成2英里测试速度变化图"""
    fig, ax5 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    ax5.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='实际速度')
    ax5.axhline(y=TARGET_RUN_SPEED, color='orange', 
                linestyle='--', linewidth=1.5, label=f'目标Run速度 ({TARGET_RUN_SPEED:.2f} m/s)')
    ax5.axhline(y=DISTANCE_METERS/TARGET_TIME_SECONDS, 
                color='green', linestyle=':', linewidth=1.5, label=f'所需平均速度 ({DISTANCE_METERS/TARGET_TIME_SECONDS:.2f} m/s)')
    
    ax5.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax5.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax5.set_title('2英里测试速度变化 / Speed during 2-mile test', fontsize=14, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    ax5.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_2mile_speed.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_load_speed_penalty():
    """生成负重对速度的影响图"""
    fig, ax6 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    enc_range = np.linspace(0.0, 1.0, 50)
    speed_penalty = [enc * ENCUMBRANCE_SPEED_PENALTY_COEFF * 100 for enc in enc_range]
    
    ax6.plot(enc_range * 100, speed_penalty, 'r-', linewidth=2, label='速度惩罚')
    ax6.fill_between(enc_range * 100, 0, speed_penalty, alpha=0.3, color='red')
    
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    combat_speed_penalty = combat_enc_percent * ENCUMBRANCE_SPEED_PENALTY_COEFF * 100
    ax6.axvline(x=combat_enc_percent * 100, color='orange', linestyle='--', linewidth=2, label='30KG战斗负重基准')
    ax6.plot(combat_enc_percent * 100, combat_speed_penalty, 'o', color='orange', markersize=10, label=f'30KG惩罚: {combat_speed_penalty:.1f}%')
    
    ax6.set_xlabel('负重百分比（%） / Load (%)', fontsize=12)
    ax6.set_ylabel('速度惩罚（%） / Speed penalty (%)', fontsize=12)
    ax6.set_title(
        '负重对速度的影响 / Load vs speed penalty\n'
        '（30KG为战斗负重基准 / 30kg baseline）',
        fontsize=14,
        fontweight='bold'
    )
    ax6.grid(True, alpha=0.3)
    ax6.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_load_speed_penalty.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_multi_factor_comparison():
    """生成多维度对比图"""
    fig, ax7 = plt.subplots(figsize=(12, 6), constrained_layout=True)
    
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    scenarios = [
        ('run', 0.0, 'Run-平地', 'blue', '-'),
        ('run', 5.0, 'Run-上坡5°', 'orange', '-'),
        ('run', -5.0, 'Run-下坡5°', 'cyan', '-'),
        ('sprint', 0.0, 'Sprint-平地', 'red', '--'),
        ('sprint', 5.0, 'Sprint-上坡5°', 'purple', '--'),
    ]
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    
    for movement_type, slope, label, color, linestyle in scenarios:
        speeds = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, movement_type, COMBAT_ENCUMBRANCE_WEIGHT)
            speed = GAME_MAX_SPEED * speed_mult
            speeds.append(speed)
        ax7.plot(stamina_test * 100, speeds, color=color, linewidth=2, linestyle=linestyle, label=label)
    
    ax7.axhline(y=TARGET_RUN_SPEED, color='gray', linestyle=':', linewidth=1, alpha=0.5, label='目标Run速度 (3.7 m/s)')
    
    ax7.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax7.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax7.set_title(
        '多维度对比 / Multi-factor comparison (30kg baseline)\n'
        '（不同移动类型与坡度 / movement types & slopes）',
        fontsize=14,
        fontweight='bold'
    )
    ax7.grid(True, alpha=0.3)
    ax7.legend(fontsize=10, loc='upper right', ncol=2, framealpha=0.85)
    
    output_file = 'comprehensive_multi_factor_comparison.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_comprehensive_trends():
    """生成所有综合趋势图（拆分为多个独立图表）"""
    print("\n" + "="*70)
    print("生成综合趋势图（拆分为多个独立图表）")
    print("="*70)
    
    # 生成所有图表
    time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist = plot_2mile_test()
    plot_load_comparison()
    plot_movement_type_speed()
    plot_recovery_analysis()
    plot_2mile_speed(time_2m, speed_2m)
    plot_load_speed_penalty()
    plot_multi_factor_comparison()
    
    print("\n所有综合趋势图已生成完成！")
    
    # 打印统计信息
    print("\n" + "="*70)
    print("2英里测试统计（无负重，精确数学模型）:")
    print("="*70)
    print("模型参数：")
    print(f"  体力影响指数 (α): {STAMINA_EXPONENT}")
    print(f"  负重影响指数 (γ): {ENCUMBRANCE_SPEED_EXPONENT}")
    print("  使用精确幂函数计算（不使用近似）")
    print()
    print("Sprint参数：")
    print(f"  Sprint速度加成: {SPRINT_SPEED_BOOST*100:.0f}%")
    print(f"  Sprint最高速度倍数: {SPRINT_MAX_SPEED_MULTIPLIER*100:.0f}% ({GAME_MAX_SPEED * SPRINT_MAX_SPEED_MULTIPLIER:.2f} m/s)")
    print(f"  Sprint体力消耗倍数: {SPRINT_STAMINA_DRAIN_MULTIPLIER}x")
    print()
    print("负重配置：")
    print(f"  最大负重: {MAX_ENCUMBRANCE_WEIGHT} kg")
    print(f"  战斗负重基准: {COMBAT_ENCUMBRANCE_WEIGHT} kg ({COMBAT_ENCUMBRANCE_WEIGHT/MAX_ENCUMBRANCE_WEIGHT*100:.1f}%)")
    print()
    print("完成时间: {:.1f}秒 ({:.2f}分钟)".format(final_time, final_time/60))
    print(f"目标时间: {TARGET_TIME_SECONDS}秒 ({TARGET_TIME_SECONDS/60:.2f}分钟)")
    print(f"时间差异: {final_time - TARGET_TIME_SECONDS:.1f}秒 ({(final_time - TARGET_TIME_SECONDS)/60:.2f}分钟)")
    print(f"完成距离: {final_dist:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"平均速度: {np.mean(speed_2m):.2f} m/s")
    print(f"最终体力: {stamina_2m[-1]*100:.2f}%")
    print(f"最终速度: {speed_2m[-1]:.2f} m/s ({speed_2m[-1]/GAME_MAX_SPEED*100:.1f}%最大速度)")
    print()
    
    # 30KG战斗负重基准测试
    print("="*70)
    print("30KG战斗负重基准测试:")
    print("="*70)
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    time_combat, stamina_combat, speed_combat, dist_combat, final_time_combat, final_dist_combat = simulate_2miles(combat_enc_percent, 'run', 0.0)
    print(f"完成时间: {final_time_combat:.1f}秒 ({final_time_combat/60:.2f}分钟)")
    print(f"完成距离: {final_dist_combat:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"平均速度: {np.mean(speed_combat):.2f} m/s")
    print(f"最终体力: {stamina_combat[-1]*100:.2f}%")
    print(f"最终速度: {speed_combat[-1]:.2f} m/s ({speed_combat[-1]/GAME_MAX_SPEED*100:.1f}%最大速度)")
    print("="*70)
    
    # 显示图表
    if SHOW_PLOTS:
        plt.show()


if __name__ == "__main__":
    plot_comprehensive_trends()
