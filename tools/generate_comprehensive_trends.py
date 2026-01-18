#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
综合趋势图生成器
包含2英里测试、不同负重情况对比、恢复速度分析等
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

# ==================== 医学模型参数（双稳态-应激性能模型，基于体重的真实模型）====================
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
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20  # 基于体重的速度惩罚系数（40%体重负重时，速度下降20%）
ENCUMBRANCE_SPEED_EXPONENT = 1.0  # 负重影响指数（1.0 = 线性）
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5  # 基于体重的体力消耗系数（40%体重负重时，消耗增加60%）
MIN_SPEED_MULTIPLIER = 0.15  # 最小速度倍数（兼容性保留）

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
# 训练有素者（fitness=1.0）基础消耗减少18%
FITNESS_EFFICIENCY_COEFF = 0.18  # 18%效率提升（训练有素时）

# 健康状态对恢复速度的影响系数
# 训练有素者（fitness=1.0）恢复速度增加25%
FITNESS_RECOVERY_COEFF = 0.25  # 25%恢复速度提升（训练有素时）

# ==================== 运动持续时间影响（累积疲劳）参数 ====================
# 基于个性化运动建模（Palumbo et al., 2018）
FATIGUE_ACCUMULATION_COEFF = 0.015  # 每分钟增加1.5%消耗（基于文献：30分钟增加约45%）
FATIGUE_START_TIME_MINUTES = 5.0  # 疲劳开始累积的时间（分钟），前5分钟无疲劳累积
FATIGUE_MAX_FACTOR = 2.0  # 最大疲劳因子（2.0倍，即消耗最多增加100%）

# ==================== 代谢适应参数（Metabolic Adaptation）====================
# 基于个性化运动建模（Palumbo et al., 2018）
AEROBIC_THRESHOLD = 0.6  # 有氧阈值（60% VO2max）
ANAEROBIC_THRESHOLD = 0.8  # 无氧阈值（80% VO2max）
AEROBIC_EFFICIENCY_FACTOR = 0.9  # 有氧区效率因子（90%，更高效）
MIXED_EFFICIENCY_FACTOR = 1.0  # 混合区效率因子（100%，标准）
ANAEROBIC_EFFICIENCY_FACTOR = 1.2  # 无氧区效率因子（120%，低效但高功率）

# ==================== 体力消耗参数（基于精确 Pandolf 模型）====================
BASE_DRAIN_RATE = 0.00004  # 每0.2秒，基础消耗
SPEED_LINEAR_DRAIN_COEFF = 0.0001  # 速度线性项系数
SPEED_SQUARED_DRAIN_COEFF = 0.0001  # 速度平方项系数（优化后降低）
ENCUMBRANCE_BASE_DRAIN_COEFF = 0.001  # 负重基础消耗系数
ENCUMBRANCE_SPEED_DRAIN_COEFF = 0.0002  # 负重速度交互项系数

# ==================== 多维度恢复模型参数 ====================
# 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
BASE_RECOVERY_RATE = 0.00015  # 基础恢复率（每0.2秒，在50%体力时的恢复率）
RECOVERY_NONLINEAR_COEFF = 0.5  # 恢复率非线性系数（体力越低恢复越快）
FAST_RECOVERY_DURATION_MINUTES = 2.0  # 快速恢复期持续时间（分钟）
FAST_RECOVERY_MULTIPLIER = 1.5  # 快速恢复期恢复速度倍数
SLOW_RECOVERY_START_MINUTES = 10.0  # 慢速恢复期开始时间（分钟）
SLOW_RECOVERY_MULTIPLIER = 0.7  # 慢速恢复期恢复速度倍数
AGE_RECOVERY_COEFF = 0.2  # 年龄恢复系数
AGE_REFERENCE = 30.0  # 年龄参考值（30岁为标准）
FATIGUE_RECOVERY_PENALTY = 0.3  # 疲劳恢复惩罚系数（最大30%恢复速度减少）
FATIGUE_RECOVERY_DURATION_MINUTES = 20.0  # 疲劳完全恢复所需时间（分钟）

# 兼容旧代码
RECOVERY_RATE = BASE_RECOVERY_RATE  # 每0.2秒

# ==================== Sprint（冲刺）相关参数 ====================
# Sprint是类似于追击或逃命的动作，追求速度，同时体力大幅消耗
SPRINT_SPEED_BOOST = 0.15  # Sprint时速度比Run快15%
SPRINT_MAX_SPEED_MULTIPLIER = 1.0  # Sprint最高速度倍数（100% = 游戏最大速度5.2 m/s）
SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5  # Sprint时体力消耗是Run的2.5倍

# ==================== 负重配置常量 ====================
BASE_WEIGHT = 1.36  # kg，基准负重（基础物品重量：衣服、鞋子等）
MAX_ENCUMBRANCE_WEIGHT = 40.5  # kg，最大负重（包含基准负重）
COMBAT_ENCUMBRANCE_WEIGHT = 30.0  # kg，战斗负重（包含基准负重，标准基准）

# ==================== 坡度影响参数（改进的多维度模型）====================
# 基于 Pandolf 模型：坡度对能量消耗的影响
SLOPE_UPHILL_COEFF = 0.08  # 上坡影响系数（每度增加8%消耗）
SLOPE_DOWNHILL_COEFF = 0.03  # 下坡影响系数（每度减少3%消耗，约为上坡的1/3）
SLOPE_MAX_MULTIPLIER = 2.0  # 最大坡度影响倍数（上坡）
SLOPE_MIN_MULTIPLIER = 0.7  # 最小坡度影响倍数（下坡）

# ==================== 负重×坡度交互项参数 ====================
# 基于文献：负重越大，坡度的影响越明显（非线性交互）
ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15  # 负重×坡度交互系数
# 例如：30kg负重（33%体重）+ 5°上坡 = 1 + 0.15 × 0.33 × 5 = 1.25倍额外影响

# ==================== 速度×负重×坡度三维交互参数 ====================
# 基于 Pandolf 模型：速度、负重、坡度的三维交互项
SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10  # 速度×负重×坡度交互系数

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
    - 'sprint': Run速度 × 1.15，最高限制在100%
    
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
        # Sprint依然保持随体力衰减，因为它是超负荷运动
        sprint_stamina_effect = np.power(stamina_percent, 0.4)
        final_speed_multiplier = SPRINT_MAX_SPEED_MULTIPLIER * max(0.5, sprint_stamina_effect)
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


def calculate_speed_encumbrance_slope_interaction(speed_ratio, body_mass_percent, slope_angle_degrees):
    """
    计算速度×负重×坡度三维交互项（基于 Pandolf 模型）
    
    完整 Pandolf 模型中的三维交互项：G·(0.23 + 1.34·V²)
    改进模型：三维交互项 = interaction_coeff × (负重/体重) × speed_ratio² × |坡度|
    
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
    
    # 只在上坡时计算交互项（下坡时交互项影响较小）
    if slope_angle_degrees <= 0.0 or body_mass_percent <= 0.0:
        return 0.0
    
    # 三维交互项：速度² × 负重 × 坡度
    interaction_term = SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF * body_mass_percent * speed_ratio * speed_ratio * slope_angle_degrees
    
    # 限制交互项在合理范围内（0.0-0.5，最多增加50%消耗）
    return np.clip(interaction_term, 0.0, 0.5)


def calculate_stamina_drain(current_speed, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, current_weight_kg=0.0, exercise_duration_minutes=0.0):
    """
    计算体力消耗率（基于体重的真实 Pandolf 模型，包含累积疲劳和代谢适应）
    
    真实公式（Pandolf et al., 1977，Palumbo et al., 2018，基于体重百分比）：
    消耗 = (a + b·V + c·V² + d·M_enc + e·M_enc·V²) × slope_multiplier × sprint_multiplier × fatigue_factor × efficiency_factor
    
    其中：
    - M_enc 基于体重百分比：M_enc = 1 + γ × (W/体重)
    - fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × max(0, exercise_duration - FATIGUE_START_TIME)
    - efficiency_factor = fitness_efficiency_factor × metabolic_efficiency_factor
    
    Sprint时体力消耗 = Run消耗 × SPRINT_STAMINA_DRAIN_MULTIPLIER
    
    Args:
        current_speed: 当前速度（m/s）
        encumbrance_percent: 负重百分比（基于最大负重，用于兼容性）
        movement_type: 移动类型
        slope_angle_degrees: 坡度角度（度）
        current_weight_kg: 当前负重（kg），如果提供则基于体重百分比计算
        exercise_duration_minutes: 运动持续时间（分钟），用于计算累积疲劳
    """
    speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
    
    # ==================== 累积疲劳因子计算 ====================
    # 基于个性化运动建模（Palumbo et al., 2018）
    effective_exercise_duration = np.maximum(exercise_duration_minutes - FATIGUE_START_TIME_MINUTES, 0.0)
    fatigue_factor = 1.0 + (FATIGUE_ACCUMULATION_COEFF * effective_exercise_duration)
    fatigue_factor = np.clip(fatigue_factor, 1.0, FATIGUE_MAX_FACTOR)  # 限制在1.0-2.0之间
    
    # ==================== 代谢适应计算（Metabolic Adaptation）====================
    # 基于个性化运动建模（Palumbo et al., 2018）
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
    
    # 基础消耗（a）- 应用综合效率因子和累积疲劳因子
    base_drain = BASE_DRAIN_RATE * total_efficiency_factor * fatigue_factor
    
    # 速度线性项（b·V）- 应用综合效率因子和累积疲劳因子
    speed_linear_drain = SPEED_LINEAR_DRAIN_COEFF * speed_ratio * total_efficiency_factor * fatigue_factor
    
    # 速度平方项（c·V²）- 应用综合效率因子和累积疲劳因子
    speed_squared_drain = SPEED_SQUARED_DRAIN_COEFF * speed_ratio * speed_ratio * total_efficiency_factor * fatigue_factor
    
    # 负重相关消耗（基于体重的真实模型）
    if current_weight_kg > 0.0:
        # 计算有效负重（减去基准重量，基准重量是基本战斗装备）
        effective_weight = np.maximum(current_weight_kg - BASE_WEIGHT, 0.0)
        # 基于体重的真实模型：M_enc = 1 + γ × (有效负重/体重)
        body_mass_percent = effective_weight / CHARACTER_WEIGHT
        encumbrance_drain_multiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * body_mass_percent)
    elif encumbrance_percent > 0.0:
        # 兼容旧模型：如果只提供了encumbrance_percent，则转换为体重百分比
        current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
        effective_weight = np.maximum(current_weight_kg - BASE_WEIGHT, 0.0)
        body_mass_percent = effective_weight / CHARACTER_WEIGHT
        encumbrance_drain_multiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * body_mass_percent)
    else:
        encumbrance_drain_multiplier = 1.0
    
    # 负重基础消耗（d·M_enc）
    encumbrance_base_drain = ENCUMBRANCE_BASE_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0)
    
    # 负重速度交互项（e·M_enc·V²）
    encumbrance_speed_drain = ENCUMBRANCE_SPEED_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0) * speed_ratio * speed_ratio
    
    # 综合消耗（精确公式）
    total_drain = base_drain + speed_linear_drain + speed_squared_drain + encumbrance_base_drain + encumbrance_speed_drain
    
    # 应用改进的坡度影响倍数（包含负重×坡度交互项）
    if current_weight_kg > 0.0:
        effective_weight = np.maximum(current_weight_kg - BASE_WEIGHT, 0.0)
        body_mass_percent = effective_weight / CHARACTER_WEIGHT
    elif encumbrance_percent > 0.0:
        current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
        effective_weight = np.maximum(current_weight_kg - BASE_WEIGHT, 0.0)
        body_mass_percent = effective_weight / CHARACTER_WEIGHT
    else:
        body_mass_percent = 0.0
    
    slope_multiplier = calculate_slope_stamina_drain_multiplier(slope_angle_degrees, body_mass_percent)
    total_drain = total_drain * slope_multiplier
    
    # 应用速度×负重×坡度三维交互项
    speed_encumbrance_slope_interaction = calculate_speed_encumbrance_slope_interaction(speed_ratio, body_mass_percent, slope_angle_degrees)
    total_drain = total_drain + (total_drain * speed_encumbrance_slope_interaction)
    
    # Sprint时体力消耗大幅增加（类似于追击或逃命）
    if movement_type == 'sprint':
        total_drain = total_drain * SPRINT_STAMINA_DRAIN_MULTIPLIER
    
    return total_drain


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


def calculate_multi_dimensional_recovery_rate(stamina_percent, rest_duration_minutes, exercise_duration_minutes):
    """
    计算多维度恢复率（基于个性化运动建模和生理学恢复模型）
    
    考虑多个维度：
    1. 当前体力百分比（非线性恢复）：体力越低恢复越快，体力越高恢复越慢
    2. 健康状态/训练水平：训练有素者恢复更快
    3. 休息时间：刚停止运动时恢复快（快速恢复期），长时间休息后恢复慢（慢速恢复期）
    4. 年龄：年轻者恢复更快
    5. 累积疲劳恢复：运动后的疲劳需要时间恢复
    """
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    rest_duration_minutes = np.maximum(rest_duration_minutes, 0.0)
    exercise_duration_minutes = np.maximum(exercise_duration_minutes, 0.0)
    
    # 1. 基础恢复率（基于当前体力百分比，非线性）
    stamina_recovery_multiplier = 1.0 + (RECOVERY_NONLINEAR_COEFF * (1.0 - stamina_percent))
    base_recovery_rate = BASE_RECOVERY_RATE * stamina_recovery_multiplier
    
    # 2. 健康状态/训练水平影响
    fitness_recovery_multiplier = 1.0 + (FITNESS_RECOVERY_COEFF * FITNESS_LEVEL)
    fitness_recovery_multiplier = np.clip(fitness_recovery_multiplier, 1.0, 1.5)
    
    # 3. 休息时间影响（快速恢复期 vs 慢速恢复期）
    if rest_duration_minutes <= FAST_RECOVERY_DURATION_MINUTES:
        rest_time_multiplier = FAST_RECOVERY_MULTIPLIER
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
    
    # 综合恢复率
    total_recovery_rate = base_recovery_rate * fitness_recovery_multiplier * rest_time_multiplier * age_recovery_multiplier * fatigue_recovery_multiplier
    
    return total_recovery_rate


def simulate_recovery(start_stamina=0.2, target_stamina=1.0, exercise_duration_minutes=0.0):
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
        recovery_rate = calculate_multi_dimensional_recovery_rate(stamina, rest_duration_minutes, exercise_duration_minutes)
        stamina = min(stamina + recovery_rate, target_stamina)
        
        time += UPDATE_INTERVAL
        rest_duration_minutes += UPDATE_INTERVAL / 60.0
        
        # 逐渐减少累积疲劳（疲劳恢复速度是累积速度的2倍）
        if exercise_duration_minutes > 0.0:
            exercise_duration_minutes = max(exercise_duration_minutes - (UPDATE_INTERVAL / 60.0 * 2.0), 0.0)
        
        if time > 600:  # 最多模拟10分钟
            break
    
    return np.array(time_points), np.array(stamina_values)


def plot_comprehensive_trends():
    """生成综合趋势图"""
    fig = plt.figure(figsize=(16, 12))
    gs = fig.add_gridspec(3, 3, hspace=0.3, wspace=0.3)
    
    fig.suptitle('拟真体力-速度系统综合趋势分析\n（基于医学模型）', fontsize=18, fontweight='bold')
    
    # ========== 图1: 2英里测试 ==========
    ax1 = fig.add_subplot(gs[0, 0])
    time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist = simulate_2miles(0.0, 'run')
    
    ax1_twin = ax1.twinx()
    line1 = ax1.plot(time_2m / 60, stamina_2m * 100, 'b-', linewidth=2, label='体力')
    line2 = ax1_twin.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='速度')
    line3 = ax1_twin.plot(time_2m / 60, dist_2m / 1000, 'g--', linewidth=1.5, label='累计距离(km)')
    
    ax1.axhline(y=50, color='b', linestyle=':', alpha=0.5)
    ax1.axhline(y=25, color='b', linestyle=':', alpha=0.5)
    ax1.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    
    ax1.set_xlabel('时间（分钟）', fontsize=10)
    ax1.set_ylabel('体力（%）', fontsize=10, color='b')
    ax1_twin.set_ylabel('速度（m/s） / 距离（km）', fontsize=10)
    ax1.set_title(f'2英里测试\n完成时间: {final_time:.1f}秒 ({final_time/60:.2f}分钟)', fontsize=11, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    
    lines = line1 + line2 + line3
    labels = [line.get_label() for line in lines]
    ax1.legend(lines, labels, loc='upper right', fontsize=8)
    
    # ========== 图2: 不同负重对比（以30KG战斗负重为基准）==========
    ax2 = fig.add_subplot(gs[0, 1])
    # 计算不同重量对应的负重百分比
    weights_kg = [0.0, 15.0, 30.0, 40.5]  # 0kg, 15kg, 30kg(战斗负重), 40.5kg(最大负重)
    encumbrance_levels = [w / MAX_ENCUMBRANCE_WEIGHT for w in weights_kg]
    colors = ['blue', 'green', 'orange', 'red']
    labels = ['0kg', '15kg', '30kg(战斗)', '40.5kg(最大)']
    
    for enc, color, label in zip(encumbrance_levels, colors, labels):
        time_e, stamina_e, speed_e, dist_e, _, _ = simulate_2miles(enc, 'run', 0.0)
        line_style = '-' if enc == COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT else '-'
        line_width = 3 if enc == COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT else 2
        ax2.plot(time_e / 60, stamina_e * 100, color=color, linewidth=line_width, linestyle=line_style,
                label=label)
    
    # 标记30KG战斗负重基准线
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    ax2.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=1.5, alpha=0.5, label='目标时间')
    
    ax2.set_xlabel('时间（分钟）', fontsize=10)
    ax2.set_ylabel('体力（%）', fontsize=10)
    ax2.set_title('不同负重下的体力消耗对比\n（30KG为战斗负重基准）', fontsize=11, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=8)
    
    # ========== 图3: 不同移动类型的速度对比 ==========
    ax3 = fig.add_subplot(gs[0, 2])
    stamina_range = np.linspace(0.0, 1.0, 100)
    
    # 计算不同移动类型的速度
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
    
    ax3.set_xlabel('体力（%）', fontsize=10)
    ax3.set_ylabel('速度（m/s）', fontsize=10)
    ax3.set_title('不同移动类型的速度对比', fontsize=11, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(fontsize=8)
    
    # ========== 图4: 体力恢复速度 ==========
    ax4 = fig.add_subplot(gs[1, 0])
    start_levels = [0.1, 0.2, 0.3, 0.5, 0.7]
    colors_recovery = ['red', 'orange', 'yellow', 'lightblue', 'lightgreen']
    
    for start, color in zip(start_levels, colors_recovery):
        time_r, stamina_r = simulate_recovery(start, 1.0, exercise_duration_minutes=0.0)
        ax4.plot(time_r / 60, stamina_r * 100, color=color, linewidth=2, 
                label=f'从 {start*100:.0f}% 恢复')
    
    ax4.set_xlabel('时间（分钟）', fontsize=10)
    ax4.set_ylabel('体力（%）', fontsize=10)
    ax4.set_title('体力恢复速度分析', fontsize=11, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(fontsize=8)
    ax4.set_ylim([0, 105])
    
    # ========== 图5: 2英里测试速度曲线 ==========
    ax5 = fig.add_subplot(gs[1, 1])
    ax5.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='实际速度')
    ax5.axhline(y=TARGET_RUN_SPEED, color='orange', 
                linestyle='--', linewidth=1.5, label=f'目标Run速度 ({TARGET_RUN_SPEED:.2f} m/s)')
    ax5.axhline(y=TARGET_TIME_SECONDS / DISTANCE_METERS * DISTANCE_METERS / TARGET_TIME_SECONDS, 
                color='green', linestyle=':', linewidth=1.5, label=f'所需平均速度 ({DISTANCE_METERS/TARGET_TIME_SECONDS:.2f} m/s)')
    
    ax5.set_xlabel('时间（分钟）', fontsize=10)
    ax5.set_ylabel('速度（m/s）', fontsize=10)
    ax5.set_title('2英里测试速度变化', fontsize=11, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    ax5.legend(fontsize=8)
    
    # ========== 图6: 负重对速度的影响（以30KG为基准）==========
    ax6 = fig.add_subplot(gs[1, 2])
    enc_range = np.linspace(0.0, 1.0, 50)
    speed_penalty = [enc * ENCUMBRANCE_SPEED_PENALTY_COEFF * 100 for enc in enc_range]
    
    ax6.plot(enc_range * 100, speed_penalty, 'r-', linewidth=2, label='速度惩罚')
    ax6.fill_between(enc_range * 100, 0, speed_penalty, alpha=0.3, color='red')
    
    # 标记30KG战斗负重基准点
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    combat_speed_penalty = combat_enc_percent * ENCUMBRANCE_SPEED_PENALTY_COEFF * 100
    ax6.axvline(x=combat_enc_percent * 100, color='orange', linestyle='--', linewidth=2, label='30KG战斗负重基准')
    ax6.plot(combat_enc_percent * 100, combat_speed_penalty, 'o', color='orange', markersize=10, label=f'30KG惩罚: {combat_speed_penalty:.1f}%')
    
    ax6.set_xlabel('负重百分比（%）', fontsize=10)
    ax6.set_ylabel('速度惩罚（%）', fontsize=10)
    ax6.set_title('负重对速度的影响\n（30KG为战斗负重基准）', fontsize=11, fontweight='bold')
    ax6.grid(True, alpha=0.3)
    ax6.legend(fontsize=8)
    
    # ========== 图7: 多维度对比 - 30KG战斗负重基准下的不同移动类型和坡度 ==========
    ax7 = fig.add_subplot(gs[2, :])
    
    # 30KG战斗负重百分比
    combat_enc_percent = COMBAT_ENCUMBRANCE_WEIGHT / MAX_ENCUMBRANCE_WEIGHT
    
    # 测试场景：30KG负重，不同移动类型，不同坡度
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
    
    ax7.set_xlabel('体力（%）', fontsize=11)
    ax7.set_ylabel('速度（m/s）', fontsize=11)
    ax7.set_title('多维度对比：30KG战斗负重基准下的速度和体力消耗\n（不同移动类型和坡度）', fontsize=12, fontweight='bold')
    ax7.grid(True, alpha=0.3)
    ax7.legend(fontsize=8, loc='upper right')
    
    # 保存图表
    output_file = 'comprehensive_stamina_trends.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n综合趋势图已保存为: {output_file}")
    
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
    plt.show()


if __name__ == "__main__":
    plot_comprehensive_trends()
