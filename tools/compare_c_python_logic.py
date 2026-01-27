#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对比C代码和Python代码的计算逻辑一致性
检查可能导致体力不消耗的极端值
"""

import sys
import os

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from stamina_constants import *

def calculate_fitness_efficiency_factor():
    """计算健康状态效率因子（C代码逻辑）"""
    fitness_efficiency_factor = 1.0 - (FITNESS_EFFICIENCY_COEFF * FITNESS_LEVEL)
    return max(0.7, min(1.0, fitness_efficiency_factor))  # C代码中Clamp到0.7-1.0

def calculate_metabolic_efficiency_factor(speed_ratio):
    """计算代谢适应效率因子（C代码逻辑）"""
    if speed_ratio < AEROBIC_THRESHOLD:
        return AEROBIC_EFFICIENCY_FACTOR  # 0.9
    elif speed_ratio < ANAEROBIC_THRESHOLD:
        t = (speed_ratio - AEROBIC_THRESHOLD) / (ANAEROBIC_THRESHOLD - AEROBIC_THRESHOLD)
        return AEROBIC_EFFICIENCY_FACTOR + t * (ANAEROBIC_EFFICIENCY_FACTOR - AEROBIC_EFFICIENCY_FACTOR)
    else:
        return ANAEROBIC_EFFICIENCY_FACTOR  # 1.2

def calculate_pandolf_energy_expenditure(velocity, current_weight, grade_percent=0.0, terrain_factor=1.0):
    """计算Pandolf能量消耗（模拟C代码逻辑）"""
    # Pandolf模型常量
    PANDOLF_BASE_COEFF = 2.7
    PANDOLF_VELOCITY_COEFF = 3.2
    PANDOLF_VELOCITY_OFFSET = 0.7
    PANDOLF_GRADE_BASE_COEFF = 0.23
    PANDOLF_GRADE_VELOCITY_COEFF = 1.34
    REFERENCE_WEIGHT = 90.0
    
    velocity = max(velocity, 0.0)
    current_weight = max(current_weight, 0.0)
    
    # 如果速度为0或很小，返回恢复率（负数）
    if velocity < 0.1:
        return -0.0025
    
    # 计算基础项：2.7 + 3.2·(V-0.7)²
    velocity_term = velocity - PANDOLF_VELOCITY_OFFSET
    velocity_squared_term = velocity_term * velocity_term
    fitness_bonus = 1.0 - (0.2 * FITNESS_LEVEL)  # 训练有素者基础代谢降低20%
    base_term = (PANDOLF_BASE_COEFF * fitness_bonus) + (PANDOLF_VELOCITY_COEFF * velocity_squared_term)
    
    # 计算坡度项：G·(0.23 + 1.34·V²)
    grade_decimal = grade_percent * 0.01
    velocity_squared = velocity * velocity
    grade_term = grade_decimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocity_squared))
    
    # 应用地形系数
    terrain_factor = max(0.5, min(3.0, terrain_factor))
    
    # 完整的Pandolf能量消耗率
    weight_multiplier = current_weight / REFERENCE_WEIGHT
    weight_multiplier = max(0.5, min(2.0, weight_multiplier))
    
    energy_expenditure = weight_multiplier * (base_term + grade_term) * terrain_factor
    
    # 将能量消耗率转换为体力消耗率
    stamina_drain_rate = energy_expenditure * ENERGY_TO_STAMINA_COEFF
    
    # 限制消耗率
    stamina_drain_rate = max(0.0, min(0.05, stamina_drain_rate))
    
    return stamina_drain_rate

def calculate_stamina_consumption(current_speed, current_weight, grade_percent=0.0, 
                                  terrain_factor=1.0, posture_multiplier=1.0,
                                  total_efficiency_factor=1.0, fatigue_factor=1.0,
                                  sprint_multiplier=1.0, encumbrance_stamina_drain_multiplier=1.0):
    """计算体力消耗（模拟C代码逻辑）"""
    
    # 如果速度为0，计算静态站立消耗
    if current_speed < 0.1:
        body_weight = CHARACTER_WEIGHT  # 90kg
        load_weight = max(current_weight - body_weight, 0.0)
        
        # 简化的静态消耗计算
        static_drain_rate = 0.0135  # 约0.0135%/s（空载时）
        if load_weight > 0:
            static_drain_rate = 0.0135 + (load_weight / 90.0) * 0.02  # 负重增加消耗
        
        base_drain_rate_by_velocity = static_drain_rate * 0.2  # 转换为每0.2秒
    # 如果速度 > 2.2 m/s，使用Givoni-Goldman跑步模型
    elif current_speed > 2.2:
        # 简化的跑步模型
        GIVONI_CONSTANT = 0.15
        GIVONI_VELOCITY_EXPONENT = 2.2
        REFERENCE_WEIGHT = 90.0
        
        velocity_power = current_speed ** GIVONI_VELOCITY_EXPONENT
        weight_multiplier = current_weight / REFERENCE_WEIGHT
        weight_multiplier = max(0.5, min(2.0, weight_multiplier))
        
        running_energy_expenditure = weight_multiplier * GIVONI_CONSTANT * velocity_power
        running_drain_rate = running_energy_expenditure * ENERGY_TO_STAMINA_COEFF
        running_drain_rate = max(0.0, min(0.05, running_drain_rate))
        
        base_drain_rate_by_velocity = running_drain_rate * terrain_factor * 0.2
    # 否则使用Pandolf步行模型
    else:
        base_drain_rate_by_velocity = calculate_pandolf_energy_expenditure(
            current_speed, current_weight, grade_percent, terrain_factor
        )
    
    # 应用姿态修正
    if base_drain_rate_by_velocity > 0.0:
        base_drain_rate_by_velocity = base_drain_rate_by_velocity * posture_multiplier
    
    # 应用多维度修正因子
    if base_drain_rate_by_velocity < 0.0:
        base_drain_rate = base_drain_rate_by_velocity
    else:
        base_drain_rate = base_drain_rate_by_velocity * total_efficiency_factor * fatigue_factor
    
    # 速度相关项
    speed_ratio = max(0.0, min(1.0, current_speed / 5.2))
    speed_linear_drain_rate = 0.00005 * speed_ratio * total_efficiency_factor * fatigue_factor
    speed_squared_drain_rate = 0.00005 * speed_ratio * speed_ratio * total_efficiency_factor * fatigue_factor
    
    # 负重相关消耗
    encumbrance_base_drain_rate = 0.001 * (encumbrance_stamina_drain_multiplier - 1.0)
    encumbrance_speed_drain_rate = 0.0002 * (encumbrance_stamina_drain_multiplier - 1.0) * speed_ratio * speed_ratio
    
    # 综合体力消耗率
    base_drain_components = (base_drain_rate + speed_linear_drain_rate + 
                             speed_squared_drain_rate + encumbrance_base_drain_rate + 
                             encumbrance_speed_drain_rate)
    
    if base_drain_rate < 0.0:
        total_drain_rate = base_drain_rate
    else:
        total_drain_rate = base_drain_components * sprint_multiplier
        
        # 生理上限
        MAX_DRAIN_RATE_PER_TICK = 0.02
        total_drain_rate = min(total_drain_rate, MAX_DRAIN_RATE_PER_TICK)
    
    return total_drain_rate

def main():
    print("=" * 80)
    print("C代码和Python代码计算逻辑对比分析")
    print("=" * 80)
    print()
    
    # 1. 检查常量一致性
    print("1. 常量一致性检查")
    print("-" * 80)
    print(f"ENERGY_TO_STAMINA_COEFF (Python): {ENERGY_TO_STAMINA_COEFF:.6e}")
    print(f"ENERGY_TO_STAMINA_COEFF (C默认): 3.50e-05")
    is_consistent = abs(ENERGY_TO_STAMINA_COEFF - 3.5e-05) < 1e-8
    print(f"一致性: {'OK' if is_consistent else 'MISMATCH'}")
    print()
    
    # 2. 检查效率因子计算
    print("2. 效率因子计算检查")
    print("-" * 80)
    fitness_eff = calculate_fitness_efficiency_factor()
    print(f"健康状态效率因子: {fitness_eff:.4f} (范围: 0.7-1.0)")
    
    # 测试不同速度比下的代谢效率因子
    speed_ratios = [0.3, 0.6, 0.8, 1.0]
    print("\n代谢适应效率因子（不同速度比）:")
    for sr in speed_ratios:
        metabolic_eff = calculate_metabolic_efficiency_factor(sr)
        total_eff = fitness_eff * metabolic_eff
        print(f"  速度比 {sr:.1f}: 代谢效率={metabolic_eff:.4f}, 总效率={total_eff:.4f}")
    
    print()
    
    # 3. 测试不同场景下的体力消耗
    print("3. 体力消耗计算测试")
    print("-" * 80)
    
    test_scenarios = [
        {
            "name": "静止站立（空载）",
            "speed": 0.0,
            "weight": 90.0,
            "grade": 0.0,
            "terrain": 1.0,
            "posture": 1.0,
            "sprint": 1.0,
            "encumbrance": 1.0
        },
        {
            "name": "慢走（空载）",
            "speed": 1.5,
            "weight": 90.0,
            "grade": 0.0,
            "terrain": 1.0,
            "posture": 1.0,
            "sprint": 1.0,
            "encumbrance": 1.0
        },
        {
            "name": "跑步（空载）",
            "speed": 3.7,
            "weight": 90.0,
            "grade": 0.0,
            "terrain": 1.0,
            "posture": 1.0,
            "sprint": 1.0,
            "encumbrance": 1.0
        },
        {
            "name": "冲刺（空载）",
            "speed": 5.2,
            "weight": 90.0,
            "grade": 0.0,
            "terrain": 1.0,
            "posture": 1.0,
            "sprint": 3.0,
            "encumbrance": 1.0
        },
        {
            "name": "跑步（30kg负重）",
            "speed": 3.7,
            "weight": 120.0,
            "grade": 0.0,
            "terrain": 1.0,
            "posture": 1.0,
            "sprint": 1.0,
            "encumbrance": 1.5
        },
    ]
    
    for scenario in test_scenarios:
        speed_ratio = max(0.0, min(1.0, scenario["speed"] / 5.2))
        fitness_eff = calculate_fitness_efficiency_factor()
        metabolic_eff = calculate_metabolic_efficiency_factor(speed_ratio)
        total_eff = fitness_eff * metabolic_eff
        fatigue_factor = 1.0  # 假设无疲劳
        
        drain_rate = calculate_stamina_consumption(
            scenario["speed"],
            scenario["weight"],
            scenario["grade"],
            scenario["terrain"],
            scenario["posture"],
            total_eff,
            fatigue_factor,
            scenario["sprint"],
            scenario["encumbrance"]
        )
        
        # 转换为每秒消耗率（用于显示）
        drain_rate_per_second = drain_rate / 0.2 if drain_rate > 0 else drain_rate * 5.0
        
        print(f"{scenario['name']}:")
        print(f"  消耗率: {drain_rate:.6f} (每0.2秒) = {drain_rate_per_second:.6f} (每秒)")
        print(f"  总效率因子: {total_eff:.4f}, 疲劳因子: {fatigue_factor:.4f}")
        if abs(drain_rate) < 1e-6:
            print(f"  [WARNING] 消耗率接近0，可能导致体力不消耗！")
        print()
    
    # 4. 检查极端值
    print("4. 极端值检查")
    print("-" * 80)
    
    # 检查最小可能的效率因子
    min_fitness_eff = 0.7  # Clamp的最小值
    min_metabolic_eff = 0.9  # 有氧区
    min_total_eff = min_fitness_eff * min_metabolic_eff
    print(f"最小总效率因子: {min_total_eff:.4f}")
    
    # 检查最小可能的消耗率
    min_speed = 0.1  # 最小移动速度
    min_weight = 90.0  # 空载
    min_drain = calculate_stamina_consumption(
        min_speed, min_weight, 0.0, 1.0, 1.0, 
        min_total_eff, 1.0, 1.0, 1.0
    )
    print(f"最小移动速度({min_speed} m/s)时的消耗率: {min_drain:.6f} (每0.2秒)")
    
    if abs(min_drain) < 1e-6:
        print("[WARNING] 最小消耗率接近0，这可能导致体力不消耗！")
        print("   建议检查:")
        print("   1. ENERGY_TO_STAMINA_COEFF是否过小")
        print("   2. 效率因子是否过小")
        print("   3. 基础消耗计算是否正确")
    
    print()
    print("=" * 80)
    print("分析完成")
    print("=" * 80)

if __name__ == "__main__":
    main()
