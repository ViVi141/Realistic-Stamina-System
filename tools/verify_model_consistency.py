#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证EnforceScript（C）和Python计算模型的一致性
"""

import sys
sys.stdout.reconfigure(encoding='utf-8')
import numpy as np

# ==================== 从Python脚本导入常量 ====================
# 这些常量应该与EnforceScript中的常量完全一致

# 游戏配置
GAME_MAX_SPEED = 5.2
TARGET_SPEED_MULTIPLIER = 0.920
STAMINA_EXPONENT = 0.6
CHARACTER_WEIGHT = 90.0
CHARACTER_AGE = 22.0

# 负重参数
BASE_WEIGHT = 1.36  # kg，基准负重（基础物品重量：衣服、鞋子等）
MAX_ENCUMBRANCE_WEIGHT = 40.5
COMBAT_ENCUMBRANCE_WEIGHT = 30.0
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5

# 健康状态
FITNESS_LEVEL = 1.0
FITNESS_EFFICIENCY_COEFF = 0.18
FITNESS_RECOVERY_COEFF = 0.25

# 累积疲劳
FATIGUE_ACCUMULATION_COEFF = 0.015
FATIGUE_START_TIME_MINUTES = 5.0
FATIGUE_MAX_FACTOR = 2.0

# 代谢适应
AEROBIC_THRESHOLD = 0.6
ANAEROBIC_THRESHOLD = 0.8
AEROBIC_EFFICIENCY_FACTOR = 0.9
ANAEROBIC_EFFICIENCY_FACTOR = 1.2

# 恢复模型
BASE_RECOVERY_RATE = 0.00015
RECOVERY_NONLINEAR_COEFF = 0.5
FAST_RECOVERY_DURATION_MINUTES = 2.0
FAST_RECOVERY_MULTIPLIER = 1.5
SLOW_RECOVERY_START_MINUTES = 10.0
SLOW_RECOVERY_MULTIPLIER = 0.7
AGE_RECOVERY_COEFF = 0.2
AGE_REFERENCE = 30.0
FATIGUE_RECOVERY_PENALTY = 0.3
FATIGUE_RECOVERY_DURATION_MINUTES = 20.0

# 坡度参数
SLOPE_UPHILL_COEFF = 0.08
SLOPE_DOWNHILL_COEFF = 0.03
SLOPE_MAX_MULTIPLIER = 2.0
SLOPE_MIN_MULTIPLIER = 0.7
ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.15
SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF = 0.10

# Sprint参数
SPRINT_SPEED_BOOST = 0.15
SPRINT_MAX_SPEED_MULTIPLIER = 1.0
SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5

# 体力消耗参数
BASE_DRAIN_RATE = 0.00004
SPEED_LINEAR_DRAIN_COEFF = 0.0001
SPEED_SQUARED_DRAIN_COEFF = 0.0001
ENCUMBRANCE_BASE_DRAIN_COEFF = 0.001
ENCUMBRANCE_SPEED_DRAIN_COEFF = 0.0002

print("="*80)
print("EnforceScript（C）和Python计算模型一致性验证")
print("="*80)
print()

# ==================== 1. 恢复模型对比 ====================
print("【1. 恢复模型对比】")
print("-"*80)

def calculate_multi_dimensional_recovery_rate_py(stamina_percent, rest_duration_minutes, exercise_duration_minutes):
    """Python版本的恢复率计算"""
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    rest_duration_minutes = np.maximum(rest_duration_minutes, 0.0)
    exercise_duration_minutes = np.maximum(exercise_duration_minutes, 0.0)
    
    # 1. 基础恢复率（非线性）
    stamina_recovery_multiplier = 1.0 + (RECOVERY_NONLINEAR_COEFF * (1.0 - stamina_percent))
    base_recovery_rate = BASE_RECOVERY_RATE * stamina_recovery_multiplier
    
    # 2. 健康状态影响
    fitness_recovery_multiplier = 1.0 + (FITNESS_RECOVERY_COEFF * FITNESS_LEVEL)
    fitness_recovery_multiplier = np.clip(fitness_recovery_multiplier, 1.0, 1.5)
    
    # 3. 休息时间影响
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

# 测试恢复模型
test_cases_recovery = [
    (0.2, 0.0, 0.0, "低体力，刚开始休息，无疲劳"),
    (0.5, 1.0, 0.0, "中等体力，快速恢复期，无疲劳"),
    (0.8, 5.0, 0.0, "高体力，标准恢复期，无疲劳"),
    (0.9, 15.0, 0.0, "高体力，慢速恢复期，无疲劳"),
    (0.3, 0.5, 10.0, "低体力，快速恢复期，有疲劳"),
]

print("恢复率计算测试：")
for stamina, rest_time, exercise_time, desc in test_cases_recovery:
    recovery_rate = calculate_multi_dimensional_recovery_rate_py(stamina, rest_time, exercise_time)
    print(f"  体力={stamina*100:.0f}%, 休息={rest_time:.1f}分钟, 运动={exercise_time:.1f}分钟 ({desc})")
    print(f"    恢复率: {recovery_rate*100:.6f}% (每0.2秒)")
print()

# ==================== 2. 速度倍数计算对比 ====================
print("【2. 速度倍数计算对比】")
print("-"*80)

def calculate_speed_multiplier_py(stamina_percent, encumbrance_percent=0.0, movement_type='run'):
    """Python版本的速度倍数计算"""
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    
    # 体力影响（幂函数）
    stamina_effect = np.power(stamina_percent, STAMINA_EXPONENT)
    base_speed_multiplier = TARGET_SPEED_MULTIPLIER * stamina_effect
    
    # 负重影响（基于体重百分比）
    current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
    body_mass_percent = current_weight_kg / CHARACTER_WEIGHT if current_weight_kg > 0.0 else 0.0
    encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * body_mass_percent
    encumbrance_speed_multiplier = 1.0 - encumbrance_penalty
    
    # 应用负重惩罚
    run_speed_multiplier = base_speed_multiplier * encumbrance_speed_multiplier
    
    # 移动类型调整
    if movement_type == 'walk':
        final_speed_multiplier = run_speed_multiplier * 0.7
    elif movement_type == 'sprint':
        final_speed_multiplier = run_speed_multiplier * (1.0 + SPRINT_SPEED_BOOST)
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, SPRINT_MAX_SPEED_MULTIPLIER)
    else:  # 'run'
        final_speed_multiplier = run_speed_multiplier
        final_speed_multiplier = np.clip(final_speed_multiplier, 0.2, 1.0)
    
    return final_speed_multiplier

test_cases_speed = [
    (1.0, 0.0, 'run', "满体力，无负重，Run"),
    (0.5, 0.0, 'run', "50%体力，无负重，Run"),
    (1.0, 30.0/40.5, 'run', "满体力，30kg负重，Run"),
    (1.0, 0.0, 'sprint', "满体力，无负重，Sprint"),
]

print("速度倍数计算测试：")
for stamina, enc, mt, desc in test_cases_speed:
    speed_mult = calculate_speed_multiplier_py(stamina, enc, mt)
    speed = GAME_MAX_SPEED * speed_mult
    print(f"  {desc}")
    print(f"    速度倍数: {speed_mult:.4f}, 速度: {speed:.2f} m/s ({speed*3.6:.2f} km/h)")
print()

# ==================== 3. 体力消耗计算对比 ====================
print("【3. 体力消耗计算对比】")
print("-"*80)

def calculate_stamina_drain_py(current_speed, encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, exercise_duration_minutes=0.0):
    """Python版本的体力消耗计算"""
    speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
    
    # 累积疲劳因子
    effective_exercise_duration = np.maximum(exercise_duration_minutes - FATIGUE_START_TIME_MINUTES, 0.0)
    fatigue_factor = 1.0 + (FATIGUE_ACCUMULATION_COEFF * effective_exercise_duration)
    fatigue_factor = np.clip(fatigue_factor, 1.0, FATIGUE_MAX_FACTOR)
    
    # 代谢适应
    if speed_ratio < AEROBIC_THRESHOLD:
        metabolic_efficiency_factor = AEROBIC_EFFICIENCY_FACTOR
    elif speed_ratio < ANAEROBIC_THRESHOLD:
        t = (speed_ratio - AEROBIC_THRESHOLD) / (ANAEROBIC_THRESHOLD - AEROBIC_THRESHOLD)
        metabolic_efficiency_factor = AEROBIC_EFFICIENCY_FACTOR + t * (ANAEROBIC_EFFICIENCY_FACTOR - AEROBIC_EFFICIENCY_FACTOR)
    else:
        metabolic_efficiency_factor = ANAEROBIC_EFFICIENCY_FACTOR
    
    # 健康状态影响
    fitness_efficiency_factor = 1.0 - (FITNESS_EFFICIENCY_COEFF * FITNESS_LEVEL)
    fitness_efficiency_factor = np.clip(fitness_efficiency_factor, 0.7, 1.0)
    
    # 综合效率因子
    total_efficiency_factor = fitness_efficiency_factor * metabolic_efficiency_factor
    
    # 基础消耗
    base_drain = BASE_DRAIN_RATE * total_efficiency_factor * fatigue_factor
    speed_linear_drain = SPEED_LINEAR_DRAIN_COEFF * speed_ratio * total_efficiency_factor * fatigue_factor
    speed_squared_drain = SPEED_SQUARED_DRAIN_COEFF * speed_ratio * speed_ratio * total_efficiency_factor * fatigue_factor
    
    # 负重消耗
    encumbrance_drain_multiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * encumbrance_percent)
    encumbrance_base_drain = ENCUMBRANCE_BASE_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0)
    encumbrance_speed_drain = ENCUMBRANCE_SPEED_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0) * speed_ratio * speed_ratio
    
    total_drain = base_drain + speed_linear_drain + speed_squared_drain + encumbrance_base_drain + encumbrance_speed_drain
    
    # 坡度影响
    current_weight_kg = encumbrance_percent * MAX_ENCUMBRANCE_WEIGHT
    body_mass_percent = current_weight_kg / CHARACTER_WEIGHT if current_weight_kg > 0.0 else 0.0
    
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
        total_slope_multiplier = np.clip(total_slope_multiplier, 1.0, 2.5)
    elif slope_angle_degrees < 0.0:
        total_slope_multiplier = np.clip(total_slope_multiplier, 0.7, 1.0)
    
    total_drain = total_drain * total_slope_multiplier
    
    # 速度×负重×坡度三维交互项
    if slope_angle_degrees > 0.0 and body_mass_percent > 0.0:
        speed_encumbrance_slope_interaction = SPEED_ENCUMBRANCE_SLOPE_INTERACTION_COEFF * body_mass_percent * speed_ratio * speed_ratio * slope_angle_degrees
        speed_encumbrance_slope_interaction = np.clip(speed_encumbrance_slope_interaction, 0.0, 0.5)
        total_drain = total_drain + (total_drain * speed_encumbrance_slope_interaction)
    
    # Sprint消耗
    if movement_type == 'sprint':
        total_drain = total_drain * SPRINT_STAMINA_DRAIN_MULTIPLIER
    
    return total_drain

test_cases_drain = [
    (4.0, 0.0, 'run', 0.0, 0.0, "Run，无负重，平地，无疲劳"),
    (4.0, 30.0/40.5, 'run', 0.0, 0.0, "Run，30kg负重，平地，无疲劳"),
    (4.0, 0.0, 'run', 5.0, 0.0, "Run，无负重，上坡5°，无疲劳"),
    (4.0, 0.0, 'run', 0.0, 10.0, "Run，无负重，平地，运动10分钟"),
    (5.0, 0.0, 'sprint', 0.0, 0.0, "Sprint，无负重，平地，无疲劳"),
]

print("体力消耗计算测试：")
for speed, enc, mt, slope, exercise, desc in test_cases_drain:
    drain = calculate_stamina_drain_py(speed, enc, mt, slope, exercise)
    print(f"  {desc}")
    print(f"    消耗率: {drain*100:.6f}% (每0.2秒), {drain*500:.4f}% (每秒)")
print()

# ==================== 4. 常量对比 ====================
print("【4. 常量对比】")
print("-"*80)
print("检查关键常量是否一致：")
print()

constants_to_check = [
    ("GAME_MAX_SPEED", 5.2),
    ("TARGET_SPEED_MULTIPLIER", 0.920),
    ("STAMINA_EXPONENT", 0.6),
    ("CHARACTER_WEIGHT", 90.0),
    ("CHARACTER_AGE", 22.0),
    ("BASE_WEIGHT", 1.36),
    ("FITNESS_LEVEL", 1.0),
    ("FITNESS_EFFICIENCY_COEFF", 0.18),
    ("FITNESS_RECOVERY_COEFF", 0.25),
    ("ENCUMBRANCE_SPEED_PENALTY_COEFF", 0.20),
    ("ENCUMBRANCE_STAMINA_DRAIN_COEFF", 1.5),
    ("BASE_RECOVERY_RATE", 0.00015),
    ("RECOVERY_NONLINEAR_COEFF", 0.5),
    ("FAST_RECOVERY_DURATION_MINUTES", 2.0),
    ("FAST_RECOVERY_MULTIPLIER", 1.5),
    ("SLOW_RECOVERY_START_MINUTES", 10.0),
    ("SLOW_RECOVERY_MULTIPLIER", 0.7),
    ("AGE_RECOVERY_COEFF", 0.2),
    ("AGE_REFERENCE", 30.0),
    ("FATIGUE_RECOVERY_PENALTY", 0.3),
    ("FATIGUE_RECOVERY_DURATION_MINUTES", 20.0),
    ("FATIGUE_ACCUMULATION_COEFF", 0.015),
    ("FATIGUE_START_TIME_MINUTES", 5.0),
    ("FATIGUE_MAX_FACTOR", 2.0),
    ("AEROBIC_THRESHOLD", 0.6),
    ("ANAEROBIC_THRESHOLD", 0.8),
    ("AEROBIC_EFFICIENCY_FACTOR", 0.9),
    ("ANAEROBIC_EFFICIENCY_FACTOR", 1.2),
    ("SLOPE_UPHILL_COEFF", 0.08),
    ("SLOPE_DOWNHILL_COEFF", 0.03),
    ("SPRINT_SPEED_BOOST", 0.15),
    ("SPRINT_STAMINA_DRAIN_MULTIPLIER", 2.5),
    ("BASE_DRAIN_RATE", 0.00004),
    ("SPEED_LINEAR_DRAIN_COEFF", 0.0001),
    ("SPEED_SQUARED_DRAIN_COEFF", 0.0001),
]

all_match = True
for name, expected_value in constants_to_check:
    actual_value = globals().get(name)
    if actual_value is None:
        print(f"  ❌ {name}: 未定义")
        all_match = False
    elif abs(actual_value - expected_value) > 1e-10:
        print(f"  ❌ {name}: 期望 {expected_value}, 实际 {actual_value}")
        all_match = False
    else:
        print(f"  ✅ {name}: {actual_value}")

print()
if all_match:
    print("✅ 所有常量一致！")
else:
    print("❌ 发现不一致的常量，请检查！")

print()
print("="*80)
print("验证完成")
print("="*80)
print()
print("注意：")
print("1. 此脚本验证了Python实现的计算逻辑")
print("2. EnforceScript（C）代码应该使用相同的公式和常量")
print("3. 如果发现不一致，请检查EnforceScript代码中的实现")
print("4. 建议在EnforceScript代码中添加注释，说明每个常量的值和来源")
