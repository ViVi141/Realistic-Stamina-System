#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Realistic Stamina System (RSS) - 模拟器
拟真体力-速度系统模拟器
基于医学模型模拟角色在最大速度下，体力和其他指标随时间的变化趋势
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
rcParams['figure.titlesize'] = 14
rcParams['axes.titlesize'] = 11
rcParams['axes.labelsize'] = 10
rcParams['legend.fontsize'] = 9
rcParams['xtick.labelsize'] = 9
rcParams['ytick.labelsize'] = 9

# Whether to show figures interactively (default off; scripts are for PNG generation)
SHOW_PLOTS = False

# ==================== 所有常量已从stamina_constants导入 ====================
# 注意：以下常量已从stamina_constants模块导入，此处不再重复定义
# - 游戏配置常量（GAME_MAX_SPEED, UPDATE_INTERVAL）
# - 角色特征常量（CHARACTER_WEIGHT, CHARACTER_AGE）
# - 负重配置常量（BASE_WEIGHT, MAX_ENCUMBRANCE_WEIGHT, COMBAT_ENCUMBRANCE_WEIGHT）
# - 健康状态/体能水平常量（FITNESS_LEVEL, FITNESS_EFFICIENCY_COEFF, FITNESS_RECOVERY_COEFF）
# - 医学模型参数（TARGET_RUN_SPEED, WILLPOWER_THRESHOLD等）
# - 负重参数（ENCUMBRANCE_SPEED_PENALTY_COEFF等）
# - 速度阈值参数（SPRINT_VELOCITY_THRESHOLD等）
# - 基础消耗率（SPRINT_BASE_DRAIN_RATE等）
# - Pandolf模型常量（PANDOLF_BASE_COEFF等）
# - 坡度影响参数（SLOPE_UPHILL_COEFF等）
# - Sprint相关参数（SPRINT_SPEED_BOOST等）
# - 地形系数常量（TERRAIN_FACTOR_PAVED等）
# - 恢复模型参数（BASE_RECOVERY_RATE等）
# - 疲劳参数（FATIGUE_ACCUMULATION_COEFF等）
# - 代谢适应参数（AEROBIC_THRESHOLD等）
# - 动作体力消耗常量（JUMP_STAMINA_BASE_COST等）
# - EPOC参数（EPOC_DELAY_SECONDS等）
# - 姿态修正参数（POSTURE_CROUCH_MULTIPLIER等）
# - 游泳参数（SWIMMING_DRAG_COEFFICIENT等）
# - 环境因子常量（ENV_HEAT_STRESS_START_HOUR等）

# 兼容性保留（用于向后兼容）
BASE_DRAIN_RATE = RUN_DRAIN_PER_TICK  # 基础消耗率（每0.2秒）
RECOVERY_RATE = BASE_RECOVERY_RATE  # 恢复率（每0.2秒）
TARGET_SPEED_MULTIPLIER = TARGET_RUN_SPEED_MULTIPLIER  # 已废弃，保留用于兼容性


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0, movement_type='run'):
    """
    根据体力百分比、负重和移动类型计算速度倍数（双稳态-应激性能模型）
    
    速度性能分段（Performance Plateau）：
    1. 平台期（Willpower Zone, Stamina 25% - 100%）：
       只要体力高于25%，士兵可以强行维持设定的目标速度（3.7 m/s）。
       这模拟了士兵在比赛或战斗中通过意志力克服早期疲劳。
    2. 衰减期（Failure Zone, Stamina 0% - 25%）：
       只有当体力掉入最后25%时，生理机能开始真正崩塌，速度平滑下降到跛行。
       使用SmoothStep函数实现25%-5%的平滑过渡缓冲区。
    
    注意：在C代码实现中，还包含5秒阻尼过渡机制。
    当体力从25%以上降至25%以下时，会启动5秒时间阻尼过渡，
    让玩家感觉"腿越来越重"，而不是"引擎突然断油"。
    Python脚本主要用于参数验证和图表生成，不包含实时时间跟踪。
    
    Args:
        stamina_percent: 当前体力百分比 (0.0-1.0)
        encumbrance_percent: 负重百分比 (0.0-1.0)，可选（用于计算负重惩罚）
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
    
    Returns:
        速度倍数（相对于游戏最大速度）
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
        # t = (stamina - SMOOTH_TRANSITION_END) / (SMOOTH_TRANSITION_START - SMOOTH_TRANSITION_END)
        # smoothT = t² × (3 - 2t)，这是一个平滑的S型曲线
        t = (stamina_percent - SMOOTH_TRANSITION_END) / (SMOOTH_TRANSITION_START - SMOOTH_TRANSITION_END)  # 0.0-1.0
        t = np.clip(t, 0.0, 1.0)
        smooth_factor = t * t * (3.0 - 2.0 * t)  # smoothstep函数
        
        # 在目标速度和跛行速度之间平滑过渡
        # 当体力从25%降到5%时，速度从3.7m/s平滑降到跛行速度
        run_speed_multiplier = MIN_LIMP_SPEED_MULTIPLIER + (TARGET_RUN_SPEED_MULTIPLIER - MIN_LIMP_SPEED_MULTIPLIER) * smooth_factor
    else:
        # 生理崩溃期（0%-5%）：速度快速线性下降到跛行速度
        # 0.05时为平滑过渡终点速度，0时为1.0m/s（MIN_LIMP_SPEED_MULTIPLIER）
        collapse_factor = stamina_percent / SMOOTH_TRANSITION_END  # 0.0-1.0
        # 计算平滑过渡终点的速度（在5%体力时，此时smoothT=0，速度为MIN_LIMP_SPEED_MULTIPLIER）
        run_speed_multiplier = MIN_LIMP_SPEED_MULTIPLIER * collapse_factor
        # 确保不会低于最小速度
        run_speed_multiplier = max(run_speed_multiplier, MIN_LIMP_SPEED_MULTIPLIER * 0.8)  # 最低不低于跛行速度的80%
    
    # 负重主要影响"油耗"（体力消耗）而不是直接降低"最高档位"（速度）
    # 负重对速度的影响基于体重百分比（真实模型）
    # 速度惩罚 = β * (负重/体重)，其中 β = 0.20
    if encumbrance_percent > 0.0:
        encumbrance_percent = np.clip(encumbrance_percent, 0.0, 1.0)
        # 计算当前重量（kg）
        current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
        # 计算有效负重（减去基准重量）
        effective_weight = max(current_weight_kg - BASE_WEIGHT, 0.0)
        # 计算有效负重占体重的百分比（Body Mass Percentage）
        body_mass_percent = effective_weight / CHARACTER_WEIGHT if CHARACTER_WEIGHT > 0.0 else 0.0
        
        # 应用真实医学模型：速度惩罚 = β * (负重/体重)
        # β = 0.20 表示40%体重负重时，速度下降20%（符合文献）
        encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * body_mass_percent
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)  # 最多减少50%速度
        # 应用负重惩罚（直接从速度倍数中减去）
        run_speed_multiplier = run_speed_multiplier * (1.0 - encumbrance_penalty)
    
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
        # 获取负重惩罚（假设encumbrance_percent已传入，需要计算encumbrance_speed_penalty）
        # 注意：Python脚本中可能没有计算encumbrance_speed_penalty，这里简化处理
        final_speed_multiplier = run_speed_multiplier * sprint_multiplier
        # 应用负重惩罚（Sprint的惩罚系数为0.15，比Run的0.2低）
        # 注意：这里简化处理，如果需要精确模拟，需要计算encumbrance_speed_penalty
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.15, SPRINT_MAX_SPEED_MULTIPLIER)
    else:  # 'run'
        final_speed_multiplier = run_speed_multiplier
        final_speed_multiplier = np.clip(final_speed_multiplier, MIN_SPEED_MULTIPLIER, MAX_SPEED_MULTIPLIER)
    
    return final_speed_multiplier


def calculate_grade_multiplier(grade_percent):
    """
    计算坡度修正乘数（使用非线性增长，让小坡几乎无感，陡坡才真正吃力）
    
    优化：使用幂函数代替线性增长，让小坡（5%以下）几乎无感，陡坡才真正吃力
    这样 Everon 的缓坡就不会让玩家频繁断气
    
    Args:
        grade_percent: 坡度百分比（例如，5% = 5.0）
    
    Returns:
        坡度修正乘数
    """
    k_grade = 1.0
    
    if grade_percent > 0.0:
        # 上坡：使用幂函数代替线性增长
        # 公式：kGrade = 1.0 + (gradePercent × 0.01)^1.2 × 5.0
        # 这样5%坡度时：1.0 + (0.05^1.2) × 5.0 ≈ 1.0 + 0.047 × 5.0 ≈ 1.235倍
        # 而15%坡度时：1.0 + (0.15^1.2) × 5.0 ≈ 1.0 + 0.173 × 5.0 ≈ 1.865倍
        # 让小坡几乎无感，陡坡才真正吃力
        normalized_grade = grade_percent * 0.01  # 转换为0.0-1.0范围（假设最大100%）
        grade_power = np.power(normalized_grade, 1.2)  # 使用1.2次方
        k_grade = 1.0 + (grade_power * 5.0)
        
        # 限制最大坡度修正（避免数值爆炸）
        k_grade = min(k_grade, 3.0)  # 最多3倍消耗
    elif grade_percent < 0.0:
        # 下坡：每1%减少3%消耗
        k_grade = 1.0 + (grade_percent * SLOPE_DOWNHILL_COEFF)
        # 限制下坡修正，避免消耗变为负数
        k_grade = max(k_grade, 0.5)  # 最多减少50%消耗
    
    return k_grade


def calculate_slope_adjusted_target_speed(base_target_speed, slope_angle_degrees):
    """
    计算坡度自适应目标速度（坡度-速度负反馈）
    
    问题分析：现实中人爬坡时，会自动缩短步幅、降低速度以维持心肺负荷（体力消耗）。
    目前的系统是"强行维持速度但暴扣体力"，这导致了代谢率在斜坡上迅速过载。
    
    解决方案：实施"坡度-速度负反馈"。当上坡角度增加时，系统自动略微下调当前的"目标速度"，
    从而换取更持久的续航。这样玩家在Everon爬小缓坡时，会感觉到角色稍微"沉"了一点点，
    但体力掉速依然平稳。
    
    Args:
        base_target_speed: 基础目标速度（m/s），例如3.7 m/s
        slope_angle_degrees: 坡度角度（度），正数=上坡，负数=下坡
    
    Returns:
        坡度自适应后的目标速度（m/s）
    """
    if slope_angle_degrees <= 0.0:
        # 下坡或平地：不主动减速
        return base_target_speed
    
    # 爬坡自适应：每增加1度坡度，目标速度自动下降2.5%
    # 这样10度坡时，速度会降到约 3.7 × 0.75 = 2.775 m/s，虽然慢了，但体力能多撑一倍时间
    # 适应因子：1.0 - (slopeAngle × 0.025)，最小不低于0.6（即最多降低40%速度）
    adaptation_factor = max(0.6, 1.0 - (slope_angle_degrees * 0.025))
    
    return base_target_speed * adaptation_factor


# 保留旧函数以兼容性（已废弃）
def calculate_slope_stamina_drain_multiplier(slope_angle_degrees, body_mass_percent=0.0):
    """
    计算坡度对体力消耗的影响倍数（已废弃，请使用calculate_grade_multiplier）
    
    为兼容性保留，但建议使用 calculate_grade_multiplier() 替代
    """
    # 将坡度角度转换为坡度百分比
    import math
    grade_percent = math.tan(slope_angle_degrees * math.pi / 180.0) * 100.0
    return calculate_grade_multiplier(grade_percent)


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
    # 这个转换系数需要根据游戏的体力系统来调整
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
    - 这样可以确保30kg负载下仍能坚持约16分钟，满足ACFT测试要求
    
    速度阈值分段（根据负重动态调整）：
    - Sprint: V ≥ 5.0 m/s → 消耗 × loadFactor
    - Run: 3.2 ≤ V < 5.0 m/s → 消耗 × loadFactor（核心修正：大幅调低RUN的基础消耗）
    - Walk: V < 3.2 m/s → 消耗 × loadFactor
    - Rest: V < 动态阈值 → -0.250 pts/s（恢复）
    
    动态阈值计算：
    - 空载（0kg）时：恢复阈值 = 2.5 m/s（速度 < 2.5 m/s 时恢复）
    - 负重30kg时：消耗阈值 = 1.5 m/s（速度 > 1.5 m/s 时开始消耗）
    - 其他负重：线性插值
    
    Args:
        velocity: 当前速度（m/s）
        current_weight: 当前负重（kg）
    
    Returns:
        基础消耗率（每0.2秒，负数表示恢复）
    """
    # 计算动态阈值（基于负重线性插值）
    # 空载（0kg）时：恢复阈值 = 2.5 m/s
    # 负重30kg时：消耗阈值 = 1.5 m/s
    if current_weight <= 0.0:
        dynamic_threshold = RECOVERY_THRESHOLD_NO_LOAD
    elif current_weight >= COMBAT_ENCUMBRANCE_WEIGHT:
        dynamic_threshold = DRAIN_THRESHOLD_COMBAT_LOAD
    else:
        t = current_weight / COMBAT_ENCUMBRANCE_WEIGHT  # t in [0, 1]
        dynamic_threshold = RECOVERY_THRESHOLD_NO_LOAD * (1.0 - t) + DRAIN_THRESHOLD_COMBAT_LOAD * t
    
    # 重新计算负重比例（基于体重）
    weight_ratio = current_weight / CHARACTER_WEIGHT if current_weight > 0.0 else 0.0  # 30kg时约为0.33
    
    # 负重影响因子：不再是简单的乘法，而是一个温和的增量
    # loadFactor = 1.0 + (weightRatio^1.2) * 1.5
    # 这样可以确保30kg时消耗不会翻倍，而是更合理的增长
    load_factor = 1.0
    if current_weight > 0.0:
        weight_ratio_power = np.power(weight_ratio, 1.2)
        load_factor = 1.0 + (weight_ratio_power * 1.5)
    
    # 根据速度和动态阈值计算消耗率
    if velocity >= SPRINT_VELOCITY_THRESHOLD or velocity >= 5.0:
        # Sprint消耗 × 负重影响因子
        return SPRINT_DRAIN_PER_TICK * load_factor
    elif velocity >= RUN_VELOCITY_THRESHOLD or velocity >= 3.2:
        # Run消耗（核心修正：大幅调低RUN的基础消耗，以补偿负重）
        # 基础消耗从0.105 pts/s降低到约0.08 pts/s（每0.2秒 = 0.00016）
        # 这是为了确保30kg负载下仍能坚持约16分钟
        return 0.00008 * load_factor  # 每0.2秒，调整后的Run消耗
    elif velocity >= dynamic_threshold:
        # Walk消耗（缓慢消耗）
        return 0.00002 * load_factor  # 每0.2秒，调整后的Walk消耗
    else:
        # 速度 < 动态阈值：恢复（保持较高的恢复率）
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
            
            # 注意：坡度项已整合在Pandolf模型中，不需要额外的交互项
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
        'idle': '静止 / Idle',
        'walk': '行走 / Walk',
        'run': '跑步 / Run',
        'sprint': '冲刺 / Sprint'
    }

    slope_desc = "平地 / Flat"
    if slope_angle_degrees > 0:
        slope_desc = "上坡 / Uphill"
    else:
        if slope_angle_degrees < 0:
            slope_desc = "下坡 / Downhill"
    
    print("模拟参数（精确数学模型）：")
    print(f"  模拟时间: {simulation_time}秒 ({simulation_time/60:.1f}分钟)")
    print(f"  负重: {encumbrance_percent*100:.0f}%")
    print(f"  移动类型: {movement_type_names.get(movement_type, movement_type)}")
    print(f"  坡度: {slope_angle_degrees:.1f}° ({slope_desc})")
    print(f"  游戏最大速度: {GAME_MAX_SPEED} m/s")
    print(f"  目标速度倍数: {TARGET_RUN_SPEED_MULTIPLIER}")
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
    
    # 创建图表（调整尺寸以符合3840×2160限制；使用constrained_layout避免文字互相挤压）
    fig, axes = plt.subplots(2, 2, figsize=(12.8, 9), constrained_layout=True)
    movement_title = movement_type_names.get(movement_type, movement_type)
    slope_title = f"{slope_angle_degrees:.1f}° ({slope_desc})"
    fig.suptitle(
        f"Realistic Stamina System (RSS) - 趋势图 / Trends\n"
        f"({movement_title}, Slope / 坡度: {slope_title}, α={STAMINA_EXPONENT})",
        fontsize=12,
        fontweight='bold'
    )
    
    # 子图1：体力随时间变化
    ax1 = axes[0, 0]
    ax1.plot(time_max / 60, stamina_max * 100, 'r-', linewidth=2, label='保持最大速度 / Max speed')
    ax1.plot(time_rest / 60, stamina_rest * 100, 'b-', linewidth=2, label='静止恢复 / Rest')
    ax1.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax1.set_ylabel('体力（%） / Stamina (%)', fontsize=12)
    ax1.set_title('体力随时间变化 / Stamina vs Time', fontsize=10, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='best')
    ax1.set_ylim([0, 105])
    
    # 子图2：速度随时间变化
    ax2 = axes[0, 1]
    ax2.plot(time_max / 60, speed_max, 'r-', linewidth=2, label='实际速度 / Speed')
    ax2.axhline(y=GAME_MAX_SPEED, color='g', linestyle='--', linewidth=1.5,
                label=f'游戏最大速度 / Max ({GAME_MAX_SPEED} m/s)')
    ax2.axhline(y=GAME_MAX_SPEED * TARGET_RUN_SPEED_MULTIPLIER, color='orange', linestyle='--', linewidth=1.5, 
                label=f'目标速度 / Target ({GAME_MAX_SPEED * TARGET_RUN_SPEED_MULTIPLIER:.2f} m/s)')
    ax2.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax2.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax2.set_title('速度随时间变化 / Speed vs Time (max-speed mode)', fontsize=10, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='best')
    
    # 子图3：速度倍数随时间变化
    ax3 = axes[1, 0]
    ax3.plot(time_max / 60, speed_mult_max * 100, 'purple', linewidth=2, label='速度倍数 / Speed multiplier')
    ax3.axhline(y=TARGET_RUN_SPEED_MULTIPLIER * 100, color='orange', linestyle='--', linewidth=1.5,
                label=f'目标 / Target ({TARGET_RUN_SPEED_MULTIPLIER*100:.0f}%)')
    ax3.axhline(y=MIN_SPEED_MULTIPLIER * 100, color='red', linestyle='--', linewidth=1.5,
                label=f'最小 / Min ({MIN_SPEED_MULTIPLIER*100:.0f}%)')
    ax3.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax3.set_ylabel('速度倍数（%） / Multiplier (%)', fontsize=12)
    ax3.set_title('速度倍数随时间变化 / Multiplier vs Time', fontsize=10, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(loc='best')
    
    # 子图4：体力消耗率随时间变化
    ax4 = axes[1, 1]
    ax4.plot(time_max / 60, drain_max / UPDATE_INTERVAL * 100, 'red', linewidth=2, label='消耗率 / Drain rate (%/s)')
    ax4.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax4.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax4.set_ylabel('体力变化率（%/秒） / Rate (%/s)', fontsize=12)
    ax4.set_title('体力消耗率随时间变化 / Stamina rate vs Time', fontsize=10, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(loc='best')
    
    # constrained_layout 已启用，无需 tight_layout（避免与constrained_layout冲突）
    
    # 保存图表
    output_file = 'stamina_system_trends.png'
    plt.savefig(output_file, dpi=200)
    print(f"\n图表已保存为: {output_file}")
    
    # 显示图表（默认关闭，避免阻塞自动生成）
    if SHOW_PLOTS:
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
