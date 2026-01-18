#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
拟真体力-速度系统模拟器
基于医学模型模拟角色在最大速度下，体力和其他指标随时间的变化趋势
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置中文字体（用于图表）
rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False

# ==================== 游戏配置常量 ====================
GAME_MAX_SPEED = 5.2  # m/s，游戏最大速度

# ==================== 角色特征常量 ====================
CHARACTER_WEIGHT = 90.0  # kg，角色体重（游戏内标准体重为90kg）
CHARACTER_AGE = 22.0  # 岁，角色年龄（基于ACFT标准，22-26岁男性）

# ==================== 负重配置常量 ====================
BASE_WEIGHT = 1.36  # kg，基准负重（基础物品重量：衣服、鞋子等）
MAX_ENCUMBRANCE_WEIGHT = 40.5  # kg，最大负重（包含基准负重）
COMBAT_ENCUMBRANCE_WEIGHT = 30.0  # kg，战斗负重（包含基准负重，标准基准）

# ==================== 健康状态/体能水平常量 ====================
# 基于个性化运动建模（Palumbo et al., 2018）
# 0.0 = 未训练（untrained）
# 0.5 = 普通健康（average fitness）
# 1.0 = 训练有素（well-trained）- 当前设置
FITNESS_LEVEL = 1.0  # 训练有素（well-trained）

# 健康状态对能量效率的影响系数
# 训练有素者（fitness=1.0）基础消耗减少18%
FITNESS_EFFICIENCY_COEFF = 0.18  # 18%效率提升（训练有素时）

# 健康状态对恢复速度的影响系数
# 训练有素者（fitness=1.0）恢复速度增加25%
FITNESS_RECOVERY_COEFF = 0.25  # 25%恢复速度提升（训练有素时）

# ==================== 运动持续时间影响（累积疲劳）参数 ====================
# 基于个性化运动建模（Palumbo et al., 2018）
FATIGUE_ACCUMULATION_COEFF = 0.015  # 每分钟增加1.5%消耗
FATIGUE_START_TIME_MINUTES = 5.0  # 疲劳开始累积的时间（分钟），前5分钟无疲劳累积
FATIGUE_MAX_FACTOR = 2.0  # 最大疲劳因子（2.0倍）

# ==================== 代谢适应参数（Metabolic Adaptation）====================
# 基于个性化运动建模（Palumbo et al., 2018）
AEROBIC_THRESHOLD = 0.6  # 有氧阈值（60% VO2max）
ANAEROBIC_THRESHOLD = 0.8  # 无氧阈值（80% VO2max）
AEROBIC_EFFICIENCY_FACTOR = 0.9  # 有氧区效率因子（90%，更高效）
ANAEROBIC_EFFICIENCY_FACTOR = 1.2  # 无氧区效率因子（120%，低效但高功率）

# ==================== 医学模型参数 ====================
# 目标速度倍数（基于游戏最大速度5.2 m/s，目标平均速度3.47 m/s）
# 目标：2英里在15分27秒内完成（927秒，平均速度3.47 m/s）
# 经过参数优化后的最佳参数（能在15分27秒内完成2英里）
TARGET_SPEED_MULTIPLIER = 0.920  # 5.2 × 0.920 = 4.78 m/s

# 体力下降对速度的影响指数（α）
# 精确数学模型：S(E) = S_max * E^α
# 基于医学文献（Minetti et al., 2002; Weyand et al., 2010），α = 0.6 更符合实际
STAMINA_EXPONENT = 0.6  # 精确值，不使用近似

# 负重对速度的惩罚系数（β）
# 精确数学模型：速度惩罚 = β * (负重百分比)^γ
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.40  # 负重速度惩罚系数
ENCUMBRANCE_SPEED_EXPONENT = 1.0  # 负重影响指数（1.0 = 线性，可调整为 1.2 以模拟非线性）

# 负重对体力消耗的影响系数（γ）
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5

# 最小速度倍数（防止体力完全耗尽时完全无法移动）
MIN_SPEED_MULTIPLIER = 0.15

# ==================== 体力消耗参数 ====================
# 基础体力消耗率（每0.2秒）
# 经过优化后的最佳参数
BASE_DRAIN_RATE = 0.00004  # 每0.2秒消耗0.004%（每10秒消耗0.2%，极低消耗率以完成长距离跑步）

# 速度相关的体力消耗系数（基于精确 Pandolf 模型）
SPEED_LINEAR_DRAIN_COEFF = 0.0001  # 速度线性项系数
SPEED_SQUARED_DRAIN_COEFF = 0.0001  # 速度平方项系数（优化后降低）

# 负重相关的体力消耗系数（基于精确 Pandolf 模型）
ENCUMBRANCE_BASE_DRAIN_COEFF = 0.001  # 负重基础消耗系数
ENCUMBRANCE_SPEED_DRAIN_COEFF = 0.0002  # 负重速度交互项系数

# 体力恢复率（每0.2秒，静止时）
RECOVERY_RATE = 0.00015  # 每0.2秒恢复0.015%（每10秒恢复0.75%，从86%到100%约需3分钟）

# 更新间隔（秒）
UPDATE_INTERVAL = 0.2  # 每0.2秒更新一次

# ==================== 坡度影响参数（改进的多维度模型）====================
SLOPE_UPHILL_COEFF = 0.08  # 上坡影响系数（每度增加8%消耗）
SLOPE_DOWNHILL_COEFF = 0.03  # 下坡影响系数（每度减少3%消耗）
SLOPE_MAX_MULTIPLIER = 2.0  # 最大坡度影响倍数（上坡）
SLOPE_MIN_MULTIPLIER = 0.7  # 最小坡度影响倍数（下坡）

# ==================== 负重×坡度交互项参数 ====================
ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15  # 负重×坡度交互系数

# ==================== 速度×负重×坡度三维交互参数 ====================
SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10  # 速度×负重×坡度交互系数

# ==================== Sprint（冲刺）相关参数 ====================
# Sprint是类似于追击或逃命的动作，追求速度，同时体力大幅消耗
SPRINT_SPEED_BOOST = 0.15  # Sprint时速度比Run快15%
SPRINT_MAX_SPEED_MULTIPLIER = 1.0  # Sprint最高速度倍数（100% = 游戏最大速度5.2 m/s）
SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5  # Sprint时体力消耗是Run的2.5倍

# ==================== 坡度影响参数 ====================
# 基于 Pandolf 模型：坡度对能量消耗的影响
SLOPE_UPHILL_COEFF = 0.08  # 上坡影响系数（每度增加8%消耗）
SLOPE_DOWNHILL_COEFF = 0.03  # 下坡影响系数（每度减少3%消耗，约为上坡的1/3）
SLOPE_MAX_MULTIPLIER = 2.0  # 最大坡度影响倍数（上坡）
SLOPE_MIN_MULTIPLIER = 0.7  # 最小坡度影响倍数（下坡）


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0, movement_type='run'):
    """
    根据体力百分比、负重和移动类型计算速度倍数（精确数学模型）
    
    精确公式：
    - 基础速度：S_base = S_max × E^α，其中 α = 0.6
    - 负重惩罚：P_enc = β × (W/W_max)^γ，其中 β = 0.40, γ = 1.0
    - Run速度：S_run = S_base × (1 - P_enc)
    - Sprint速度：S_sprint = S_run × (1 + SPRINT_BOOST)，限制在SPRINT_MAX内
    - Walk速度：S_walk = S_run × 0.7
    
    移动类型：
    - 'idle': 0.0
    - 'walk': Run速度 × 0.7
    - 'run': 基础速度 × (1 - 负重惩罚)
    - 'sprint': Run速度 × 1.15，最高限制在100%
    
    Args:
        stamina_percent: 当前体力百分比 (0.0-1.0)
        encumbrance_percent: 负重百分比 (0.0-1.0)，可选
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
    
    Returns:
        速度倍数（相对于游戏最大速度）
    """
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    
    # 精确计算：S_base = S_max × E^α（使用精确幂函数）
    stamina_effect = np.power(stamina_percent, STAMINA_EXPONENT)
    base_speed_multiplier = TARGET_SPEED_MULTIPLIER * stamina_effect
    base_speed_multiplier = max(base_speed_multiplier, MIN_SPEED_MULTIPLIER)
    
    # 应用精确负重惩罚：P_enc = β × (有效负重/W_max)^γ
    if encumbrance_percent > 0.0:
        encumbrance_percent = np.clip(encumbrance_percent, 0.0, 1.0)
        # 计算有效负重（减去基准重量）
        current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
        effective_weight = max(current_weight_kg - BASE_WEIGHT, 0.0)
        effective_encumbrance_percent = effective_weight / MAX_ENCUMBRANCE_WEIGHT if MAX_ENCUMBRANCE_WEIGHT > 0.0 else 0.0
        
        # 基于有效负重计算速度惩罚
        encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * np.power(effective_encumbrance_percent, ENCUMBRANCE_SPEED_EXPONENT)
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)  # 最多减少50%速度
        run_speed_multiplier = base_speed_multiplier * (1.0 - encumbrance_penalty)
    else:
        run_speed_multiplier = base_speed_multiplier
    
    # 根据移动类型调整速度
    if movement_type == 'idle':
        final_speed_multiplier = 0.0
    elif movement_type == 'walk':
        final_speed_multiplier = run_speed_multiplier * 0.7
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, 0.8)
    elif movement_type == 'sprint':
        # Sprint速度 = Run速度 × (1 + Sprint加成)
        final_speed_multiplier = run_speed_multiplier * (1.0 + SPRINT_SPEED_BOOST)
        # Sprint最高速度限制（基于现实情况）
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, SPRINT_MAX_SPEED_MULTIPLIER)
    else:  # 'run'
        final_speed_multiplier = run_speed_multiplier
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, 1.0)
    
    return final_speed_multiplier


def calculate_slope_stamina_drain_multiplier(slope_angle_degrees, body_mass_percent=0.0):
    """
    计算坡度对体力消耗的影响倍数（改进的多维度模型，包含负重交互）
    
    改进的数学模型：基于 Pandolf 负重行走代谢成本模型
    改进模型：坡度影响 = 基础坡度影响 + 负重×坡度交互项
    
    Args:
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
        body_mass_percent: 负重占体重的百分比（0.0-1.0+），用于计算交互项
    
    Returns:
        坡度影响倍数（0.7-2.5），表示相对于平地时的消耗倍数
    """
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


def calculate_speed_encumbrance_slope_interaction(speed_ratio, body_mass_percent, slope_angle_degrees):
    """
    计算速度×负重×坡度三维交互项（基于 Pandolf 模型）
    
    Args:
        speed_ratio: 速度比（0.0-1.0），当前速度/最大速度
        body_mass_percent: 负重占体重的百分比（0.0-1.0+）
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
    
    Returns:
        三维交互项系数（0.0-0.5），表示额外的消耗倍数
    """
    speed_ratio = np.clip(speed_ratio, 0.0, 1.0)
    body_mass_percent = np.maximum(body_mass_percent, 0.0)
    slope_angle_degrees = np.clip(slope_angle_degrees, -45.0, 45.0)
    
    if slope_angle_degrees <= 0.0 or body_mass_percent <= 0.0:
        return 0.0
    
    interaction_term = SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF * body_mass_percent * speed_ratio * speed_ratio * slope_angle_degrees
    return np.clip(interaction_term, 0.0, 0.5)


def calculate_stamina_drain(current_speed, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, exercise_duration_minutes=0.0):
    """
    计算体力消耗率（基于精确 Pandolf 模型，考虑健康状态、累积疲劳和代谢适应）
    
    精确数学模型（基于 Pandolf et al., 1977 + Palumbo et al., 2018）：
    消耗 = (a + b·V + c·V² + d·M_enc + e·M_enc·V²) × slope_multiplier × sprint_multiplier × fitness_factor × fatigue_factor × metabolic_factor
    
    其中：
    - a = 基础消耗
    - b = 速度线性项系数
    - c = 速度平方项系数
    - d = 负重基础消耗系数
    - e = 负重速度交互项系数
    - V = 速度（相对速度）
    - M_enc = 负重影响倍数
    - slope_multiplier = 坡度影响倍数
    - sprint_multiplier = Sprint消耗倍数（仅在Sprint时）
    - fitness_factor = 健康状态效率因子（训练有素者效率提高）
    - fatigue_factor = 累积疲劳因子（长时间运动后消耗增加）
    - metabolic_factor = 代谢适应因子（根据强度动态调整效率）
    
    Args:
        current_speed: 当前速度（m/s）
        encumbrance_percent: 负重百分比（0.0-1.0）
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
        exercise_duration_minutes: 运动持续时间（分钟），用于计算累积疲劳
    
    Returns:
        体力消耗率（每0.2秒）
    """
    # 计算速度比（相对于最大速度）
    speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
    
    # ==================== 累积疲劳因子计算 ====================
    effective_exercise_duration = np.maximum(exercise_duration_minutes - FATIGUE_START_TIME_MINUTES, 0.0)
    fatigue_factor = 1.0 + (FATIGUE_ACCUMULATION_COEFF * effective_exercise_duration)
    fatigue_factor = np.clip(fatigue_factor, 1.0, FATIGUE_MAX_FACTOR)
    
    # ==================== 代谢适应计算 ====================
    if speed_ratio < AEROBIC_THRESHOLD:
        metabolic_efficiency_factor = AEROBIC_EFFICIENCY_FACTOR  # 0.9
    elif speed_ratio < ANAEROBIC_THRESHOLD:
        t = (speed_ratio - AEROBIC_THRESHOLD) / (ANAEROBIC_THRESHOLD - AEROBIC_THRESHOLD)
        metabolic_efficiency_factor = AEROBIC_EFFICIENCY_FACTOR + t * (ANAEROBIC_EFFICIENCY_FACTOR - AEROBIC_EFFICIENCY_FACTOR)
    else:
        metabolic_efficiency_factor = ANAEROBIC_EFFICIENCY_FACTOR  # 1.2
    
    # ==================== 健康状态影响计算 ====================
    # 基于个性化运动建模（Palumbo et al., 2018）
    # 训练有素者（fitness=1.0）能量效率提高18%
    fitness_efficiency_factor = 1.0 - (FITNESS_EFFICIENCY_COEFF * FITNESS_LEVEL)
    fitness_efficiency_factor = np.clip(fitness_efficiency_factor, 0.7, 1.0)  # 限制在70%-100%之间
    
    # 综合效率因子 = 健康状态效率 × 代谢适应效率
    total_efficiency_factor = fitness_efficiency_factor * metabolic_efficiency_factor
    
    # 基础体力消耗率（对应 Pandolf 模型中的常数项 a）
    base_drain = BASE_DRAIN_RATE * total_efficiency_factor * fatigue_factor
    
    # 速度线性项（对应 Pandolf 模型中的 b·V 项）
    speed_linear_drain = SPEED_LINEAR_DRAIN_COEFF * speed_ratio * total_efficiency_factor * fatigue_factor
    
    # 速度平方项（对应 Pandolf 模型中的 c·V² 项）
    speed_squared_drain = SPEED_SQUARED_DRAIN_COEFF * speed_ratio * speed_ratio * total_efficiency_factor * fatigue_factor
    
    # 负重相关的体力消耗（基于精确 Pandolf 模型）
    # 计算有效负重（减去基准重量，基准重量是基础物品）
    current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
    effective_weight = max(current_weight_kg - BASE_WEIGHT, 0.0)
    effective_encumbrance_percent = effective_weight / MAX_ENCUMBRANCE_WEIGHT if MAX_ENCUMBRANCE_WEIGHT > 0.0 else 0.0
    
    # 基于有效负重计算消耗倍数
    encumbrance_drain_multiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * effective_encumbrance_percent)
    
    # 负重基础消耗（对应 Pandolf 模型中的 d·M_enc 项）
    encumbrance_base_drain = ENCUMBRANCE_BASE_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0)
    
    # 负重速度交互项（对应 Pandolf 模型中的 e·M_enc·V² 项）
    encumbrance_speed_drain = ENCUMBRANCE_SPEED_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0) * speed_ratio * speed_ratio
    
    # 综合体力消耗率（精确公式）
    total_drain = base_drain + speed_linear_drain + speed_squared_drain + encumbrance_base_drain + encumbrance_speed_drain
    
    # 计算有效负重占体重的百分比（用于计算交互项）
    body_mass_percent = effective_weight / CHARACTER_WEIGHT if effective_weight > 0.0 else 0.0
    
    # 应用改进的坡度影响倍数（包含负重×坡度交互项）
    slope_multiplier = calculate_slope_stamina_drain_multiplier(slope_angle_degrees, body_mass_percent)
    total_drain = total_drain * slope_multiplier
    
    # 应用速度×负重×坡度三维交互项
    speed_encumbrance_slope_interaction = calculate_speed_encumbrance_slope_interaction(speed_ratio, body_mass_percent, slope_angle_degrees)
    total_drain = total_drain + (total_drain * speed_encumbrance_slope_interaction)
    
    # Sprint时体力消耗大幅增加（类似于追击或逃命）
    if movement_type == 'sprint':
        total_drain = total_drain * SPRINT_STAMINA_DRAIN_MULTIPLIER
    
    return total_drain


def simulate_stamina_system(simulation_time=300, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, max_speed_mode=True):
    """
    模拟体力系统
    
    Args:
        simulation_time: 模拟时间（秒）
        encumbrance_percent: 负重百分比（0.0-1.0）
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
        max_speed_mode: 是否保持最大速度模式（True）或静止模式（False）
    
    Returns:
        time_points: 时间点数组
        stamina_values: 体力值数组
        speed_values: 速度值数组
        speed_multiplier_values: 速度倍数数组
        drain_rate_values: 消耗率数组
    """
    # 初始化
    num_steps = int(simulation_time / UPDATE_INTERVAL)
    time_points = np.linspace(0, simulation_time, num_steps)
    
    stamina_values = np.zeros(num_steps)
    speed_values = np.zeros(num_steps)
    speed_multiplier_values = np.zeros(num_steps)
    drain_rate_values = np.zeros(num_steps)
    
    # 初始值
    stamina_values[0] = 1.0  # 100%体力
    speed_multiplier_values[0] = calculate_speed_multiplier_by_stamina(stamina_values[0], encumbrance_percent, movement_type)
    speed_values[0] = GAME_MAX_SPEED * speed_multiplier_values[0]
    
    # 运动持续时间跟踪（用于累积疲劳）
    exercise_duration_minutes = 0.0
    
    # 模拟循环
    for i in range(1, num_steps):
        # 获取当前体力
        current_stamina = stamina_values[i-1]
        
        # 计算当前速度倍数（包含负重和移动类型影响）
        current_speed_multiplier = calculate_speed_multiplier_by_stamina(current_stamina, encumbrance_percent, movement_type)
        speed_multiplier_values[i] = current_speed_multiplier
        
        # 计算当前速度
        if max_speed_mode and movement_type != 'idle':
            # 最大速度模式：使用当前体力对应的最大速度
            current_speed = GAME_MAX_SPEED * current_speed_multiplier
        else:
            # 静止模式：速度为0
            current_speed = 0.0
        
        speed_values[i] = current_speed
        
        # 更新运动持续时间（仅在移动时累积）
        if max_speed_mode and current_speed > 0.05:
            exercise_duration_minutes += UPDATE_INTERVAL / 60.0
        else:
            # 静止时，疲劳快速恢复（恢复速度是累积速度的2倍）
            exercise_duration_minutes = max(exercise_duration_minutes - (UPDATE_INTERVAL / 60.0 * 2.0), 0.0)
        
        # 计算体力消耗/恢复
        if max_speed_mode and current_speed > 0.05:
            # 移动时：消耗体力（包含移动类型和坡度影响，改进的多维度模型，包含累积疲劳和代谢适应）
            # 计算当前负重（kg）
            current_weight_kg = encumbrance_percent * 40.5  # MAX_ENCUMBRANCE_WEIGHT
            body_mass_percent = current_weight_kg / CHARACTER_WEIGHT if current_weight_kg > 0.0 else 0.0
            
            drain_rate = calculate_stamina_drain(current_speed, encumbrance_percent, movement_type, slope_angle_degrees, exercise_duration_minutes)
            
            # 应用速度×负重×坡度三维交互项
            speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
            speed_encumbrance_slope_interaction = calculate_speed_encumbrance_slope_interaction(speed_ratio, body_mass_percent, slope_angle_degrees)
            drain_rate = drain_rate + (drain_rate * speed_encumbrance_slope_interaction)
            
            drain_rate_values[i] = drain_rate
            new_stamina = current_stamina - drain_rate
        else:
            # 静止时：恢复体力
            drain_rate_values[i] = -RECOVERY_RATE  # 负数表示恢复
            new_stamina = current_stamina + RECOVERY_RATE
        
        # 限制体力值范围
        stamina_values[i] = np.clip(new_stamina, 0.0, 1.0)
    
    return time_points, stamina_values, speed_values, speed_multiplier_values, drain_rate_values


def plot_trends(simulation_time=300, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0):
    """
    绘制趋势图
    
    Args:
        simulation_time: 模拟时间（秒）
        encumbrance_percent: 负重百分比（0.0-1.0）
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
    """
    movement_type_names = {
        'idle': '静止',
        'walk': '行走',
        'run': '跑步',
        'sprint': '冲刺'
    }
    
    print("模拟参数（精确数学模型）：")
    print(f"  模拟时间: {simulation_time}秒 ({simulation_time/60:.1f}分钟)")
    print(f"  负重: {encumbrance_percent*100:.0f}%")
    print(f"  移动类型: {movement_type_names.get(movement_type, movement_type)}")
    print(f"  坡度: {slope_angle_degrees:.1f}° ({'上坡' if slope_angle_degrees > 0 else '下坡' if slope_angle_degrees < 0 else '平地'})")
    print(f"  游戏最大速度: {GAME_MAX_SPEED} m/s")
    print(f"  目标速度倍数: {TARGET_SPEED_MULTIPLIER}")
    print(f"  体力影响指数 (α): {STAMINA_EXPONENT}")
    print(f"  负重影响指数 (γ): {ENCUMBRANCE_SPEED_EXPONENT}")
    if movement_type == 'sprint':
        print(f"  Sprint速度加成: {SPRINT_SPEED_BOOST*100:.0f}%")
        print(f"  Sprint消耗倍数: {SPRINT_STAMINA_DRAIN_MULTIPLIER}x")
    print()
    
    # 模拟最大速度模式
    print(f"模拟：保持最大速度模式 ({movement_type_names.get(movement_type, movement_type)})...")
    time_max, stamina_max, speed_max, speed_mult_max, drain_max = simulate_stamina_system(
        simulation_time, encumbrance_percent, movement_type, slope_angle_degrees, max_speed_mode=True
    )
    
    # 模拟静止模式（对比）
    print("模拟：静止恢复模式...")
    time_rest, stamina_rest, speed_rest, speed_mult_rest, drain_rest = simulate_stamina_system(
        simulation_time, encumbrance_percent, 'idle', 0.0, max_speed_mode=False
    )
    
    # 创建图表
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    movement_title = movement_type_names.get(movement_type, movement_type)
    slope_title = f"{slope_angle_degrees:.1f}° ({'上坡' if slope_angle_degrees > 0 else '下坡' if slope_angle_degrees < 0 else '平地'})"
    fig.suptitle(f'拟真体力-速度系统趋势图\n（{movement_title}，坡度: {slope_title}，精确数学模型，α=0.6）', fontsize=16, fontweight='bold')
    
    # 子图1：体力随时间变化
    ax1 = axes[0, 0]
    ax1.plot(time_max / 60, stamina_max * 100, 'r-', linewidth=2, label='保持最大速度')
    ax1.plot(time_rest / 60, stamina_rest * 100, 'b-', linewidth=2, label='静止恢复')
    ax1.set_xlabel('时间（分钟）', fontsize=12)
    ax1.set_ylabel('体力（%）', fontsize=12)
    ax1.set_title('体力随时间变化', fontsize=13, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_ylim([0, 105])
    
    # 子图2：速度随时间变化
    ax2 = axes[0, 1]
    ax2.plot(time_max / 60, speed_max, 'r-', linewidth=2, label='实际速度')
    ax2.axhline(y=GAME_MAX_SPEED, color='g', linestyle='--', linewidth=1.5, label=f'游戏最大速度 ({GAME_MAX_SPEED} m/s)')
    ax2.axhline(y=GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER, color='orange', linestyle='--', linewidth=1.5, 
                label=f'目标速度 ({GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER:.2f} m/s)')
    ax2.set_xlabel('时间（分钟）', fontsize=12)
    ax2.set_ylabel('速度（m/s）', fontsize=12)
    ax2.set_title('速度随时间变化（保持最大速度模式）', fontsize=13, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    
    # 子图3：速度倍数随时间变化
    ax3 = axes[1, 0]
    ax3.plot(time_max / 60, speed_mult_max * 100, 'purple', linewidth=2, label='速度倍数')
    ax3.axhline(y=TARGET_SPEED_MULTIPLIER * 100, color='orange', linestyle='--', linewidth=1.5,
                label=f'目标速度倍数 ({TARGET_SPEED_MULTIPLIER*100:.0f}%)')
    ax3.axhline(y=MIN_SPEED_MULTIPLIER * 100, color='red', linestyle='--', linewidth=1.5,
                label=f'最小速度倍数 ({MIN_SPEED_MULTIPLIER*100:.0f}%)')
    ax3.set_xlabel('时间（分钟）', fontsize=12)
    ax3.set_ylabel('速度倍数（%）', fontsize=12)
    ax3.set_title('速度倍数随时间变化', fontsize=13, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend()
    
    # 子图4：体力消耗率随时间变化
    ax4 = axes[1, 1]
    ax4.plot(time_max / 60, drain_max / UPDATE_INTERVAL * 100, 'red', linewidth=2, label='消耗率（每秒）')
    ax4.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax4.set_xlabel('时间（分钟）', fontsize=12)
    ax4.set_ylabel('体力变化率（%/秒）', fontsize=12)
    ax4.set_title('体力消耗率随时间变化', fontsize=13, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend()
    
    plt.tight_layout()
    
    # 保存图表
    output_file = 'stamina_system_trends.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n图表已保存为: {output_file}")
    
    # 显示图表
    plt.show()
    
    # 打印关键指标
    print("\n" + "="*60)
    print("关键指标统计（保持最大速度模式）:")
    print("="*60)
    time_to_50_percent = None
    time_to_25_percent = None
    time_to_exhaustion = None
    
    for i, s in enumerate(stamina_max):
        if s <= 0.5 and time_to_50_percent is None:
            time_to_50_percent = time_max[i]
        if s <= 0.25 and time_to_25_percent is None:
            time_to_25_percent = time_max[i]
        if s <= 0.01 and time_to_exhaustion is None:
            time_to_exhaustion = time_max[i]
    
    if time_to_50_percent:
        print(f"体力降至50%所需时间: {time_to_50_percent:.1f}秒 ({time_to_50_percent/60:.2f}分钟)")
    else:
        print("体力降至50%所需时间: 未达到")
    if time_to_25_percent:
        print(f"体力降至25%所需时间: {time_to_25_percent:.1f}秒 ({time_to_25_percent/60:.2f}分钟)")
    else:
        print("体力降至25%所需时间: 未达到")
    if time_to_exhaustion:
        print(f"体力耗尽可能时间: {time_to_exhaustion:.1f}秒 ({time_to_exhaustion/60:.2f}分钟)")
    else:
        print("体力耗尽可能时间: 未完全耗尽")
    print(f"最终体力值: {stamina_max[-1]*100:.2f}%")
    print(f"最终速度: {speed_max[-1]:.2f} m/s ({speed_max[-1]/GAME_MAX_SPEED*100:.1f}%最大速度)")
    print("="*60)


if __name__ == "__main__":
    # 运行模拟
    # 参数：模拟时间（秒），负重百分比（0.0-1.0），移动类型，坡度角度（度）
    # 模拟时间设置为20分钟（1200秒），以观察完整趋势
    
    # 示例1：Run模式，无负重，平地
    print("="*70)
    print("示例1：Run模式，无负重，平地")
    print("="*70)
    plot_trends(simulation_time=1200, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0)
    
    # 可以取消注释以下示例来测试不同场景：
    # 
    # # 示例2：Sprint模式，无负重，平地
    # print("\n" + "="*70)
    # print("示例2：Sprint模式，无负重，平地")
    # print("="*70)
    # plot_trends(simulation_time=300, encumbrance_percent=0.0, movement_type='sprint', slope_angle_degrees=0.0)
    # 
    # # 示例3：Run模式，无负重，上坡5度
    # print("\n" + "="*70)
    # print("示例3：Run模式，无负重，上坡5度")
    # print("="*70)
    # plot_trends(simulation_time=1200, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=5.0)
    # 
    # # 示例4：Sprint模式，无负重，上坡5度
    # print("\n" + "="*70)
    # print("示例4：Sprint模式，无负重，上坡5度")
    # print("="*70)
    # plot_trends(simulation_time=300, encumbrance_percent=0.0, movement_type='sprint', slope_angle_degrees=5.0)
