#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
参数优化脚本：找到能在15分27秒内完成2英里的最佳参数组合
"""

import numpy as np

# ==================== 目标参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒
TARGET_AVERAGE_SPEED = DISTANCE_METERS / TARGET_TIME_SECONDS  # m/s

# 游戏参数
GAME_MAX_SPEED = 5.2  # m/s
UPDATE_INTERVAL = 0.2  # 秒
STAMINA_EXPONENT = 0.6  # 精确值
MIN_SPEED_MULTIPLIER = 0.15

print("="*70)
print("2英里15分27秒参数优化")
print("="*70)
print(f"目标距离: {DISTANCE_METERS} 米")
print(f"目标时间: {TARGET_TIME_SECONDS} 秒 ({TARGET_TIME_SECONDS/60:.2f} 分钟)")
print(f"所需平均速度: {TARGET_AVERAGE_SPEED:.4f} m/s")
print()


def simulate_2miles(speed_mult, base_drain, speed_linear_coeff, speed_squared_coeff, stamina_exp=0.6):
    """模拟跑2英里（使用精确模型）"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    speeds = []
    
    max_time = TARGET_TIME_SECONDS * 2
    
    while distance < DISTANCE_METERS and time < max_time:
        # 计算当前速度倍数（基于精确模型：E^α）
        stamina_effect = np.power(max(stamina, 0.0), stamina_exp)
        current_speed_mult = speed_mult * stamina_effect
        current_speed_mult = max(current_speed_mult, MIN_SPEED_MULTIPLIER)
        
        # 计算当前速度
        current_speed = GAME_MAX_SPEED * current_speed_mult
        speeds.append(current_speed)
        
        # 更新距离
        distance += current_speed * UPDATE_INTERVAL
        
        # 如果已跑完距离，停止
        if distance >= DISTANCE_METERS:
            break
        
        # 更新体力（使用精确 Pandolf 模型）
        speed_ratio = current_speed / GAME_MAX_SPEED
        drain = base_drain + speed_linear_coeff * speed_ratio + speed_squared_coeff * speed_ratio * speed_ratio
        stamina = max(stamina - drain, 0.0)
        
        time += UPDATE_INTERVAL
    
    avg_speed = np.mean(speeds) if speeds else 0.0
    return time, distance, avg_speed, stamina


# 参数搜索范围
speed_mult_range = np.linspace(0.85, 0.95, 11)  # 0.85 到 0.95
base_drain_range = np.linspace(0.00002, 0.00008, 7)  # 0.00002 到 0.00008
speed_squared_coeff_range = np.linspace(0.0001, 0.0005, 9)  # 0.0001 到 0.0005
speed_linear_coeff = 0.0001  # 固定线性项系数

best_params = None
best_time = float('inf')
best_distance = 0.0

print("开始参数搜索...")
print(f"搜索范围：")
print(f"  速度倍数: {speed_mult_range[0]:.3f} - {speed_mult_range[-1]:.3f}")
print(f"  基础消耗: {base_drain_range[0]:.6f} - {base_drain_range[-1]:.6f}")
print(f"  速度平方项系数: {speed_squared_coeff_range[0]:.6f} - {speed_squared_coeff_range[-1]:.6f}")
print()

total_combinations = len(speed_mult_range) * len(base_drain_range) * len(speed_squared_coeff_range)
current = 0

for speed_mult in speed_mult_range:
    for base_drain in base_drain_range:
        for speed_squared_coeff in speed_squared_coeff_range:
            current += 1
            if current % 50 == 0:
                print(f"进度: {current}/{total_combinations} ({current/total_combinations*100:.1f}%)")
            
            time, distance, avg_speed, stamina = simulate_2miles(
                speed_mult, base_drain, speed_linear_coeff, speed_squared_coeff, STAMINA_EXPONENT
            )
            
            # 评估：优先选择能在目标时间内完成且最接近目标时间的参数
            if distance >= DISTANCE_METERS:
                if time <= TARGET_TIME_SECONDS:
                    # 能在目标时间内完成，选择最接近目标时间的
                    if abs(time - TARGET_TIME_SECONDS) < abs(best_time - TARGET_TIME_SECONDS):
                        best_time = time
                        best_distance = distance
                        best_params = {
                            'speed_multiplier': speed_mult,
                            'base_drain_rate': base_drain,
                            'speed_linear_coeff': speed_linear_coeff,
                            'speed_squared_coeff': speed_squared_coeff,
                            'time': time,
                            'distance': distance,
                            'avg_speed': avg_speed,
                            'final_stamina': stamina
                        }
                elif best_time == float('inf'):
                    # 如果还没有找到能在目标时间内完成的，记录最好的结果
                    if time < best_time:
                        best_time = time
                        best_distance = distance
                        best_params = {
                            'speed_multiplier': speed_mult,
                            'base_drain_rate': base_drain,
                            'speed_linear_coeff': speed_linear_coeff,
                            'speed_squared_coeff': speed_squared_coeff,
                            'time': time,
                            'distance': distance,
                            'avg_speed': avg_speed,
                            'final_stamina': stamina
                        }

print("\n" + "="*70)
print("优化结果")
print("="*70)

if best_params:
    print(f"最佳参数组合：")
    print(f"  TARGET_SPEED_MULTIPLIER = {best_params['speed_multiplier']:.4f}")
    print(f"  BASE_DRAIN_RATE = {best_params['base_drain_rate']:.6f}")
    print(f"  SPEED_LINEAR_DRAIN_COEFF = {best_params['speed_linear_coeff']:.6f}")
    print(f"  SPEED_SQUARED_DRAIN_COEFF = {best_params['speed_squared_coeff']:.6f}")
    print()
    print(f"测试结果：")
    print(f"  完成时间: {best_params['time']:.1f}秒 ({best_params['time']/60:.2f}分钟)")
    print(f"  目标时间: {TARGET_TIME_SECONDS}秒 ({TARGET_TIME_SECONDS/60:.2f}分钟)")
    print(f"  时间差异: {best_params['time'] - TARGET_TIME_SECONDS:.1f}秒 ({(best_params['time'] - TARGET_TIME_SECONDS)/60:.2f}分钟)")
    print(f"  完成距离: {best_params['distance']:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"  平均速度: {best_params['avg_speed']:.4f} m/s (目标: {TARGET_AVERAGE_SPEED:.4f} m/s)")
    print(f"  最终体力: {best_params['final_stamina']*100:.2f}%")
    
    if best_params['time'] <= TARGET_TIME_SECONDS:
        print("\n[成功] 可以在目标时间内完成2英里")
    else:
        print(f"\n[警告] 最佳结果：超时 {best_params['time'] - TARGET_TIME_SECONDS:.1f}秒")
        print("   建议进一步降低体力消耗率或提高速度倍数")
else:
    print("未找到能在目标时间内完成的参数组合")
    print("建议扩大搜索范围或调整目标")

print("="*70)
