#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动优化参数脚本
找到能在15分27秒内完成2英里的最佳参数组合
"""

import numpy as np

# 目标参数
DISTANCE_METERS = 3218.7  # 米
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒
TARGET_AVERAGE_SPEED = DISTANCE_METERS / TARGET_TIME_SECONDS
GAME_MAX_SPEED = 5.2
UPDATE_INTERVAL = 0.2
MIN_SPEED_MULTIPLIER = 0.15

def simulate_2miles(speed_mult, base_drain, speed_drain_coeff):
    """模拟跑2英里"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    speeds = []
    
    while distance < DISTANCE_METERS and time < TARGET_TIME_SECONDS * 3:
        stamina_effect = np.sqrt(max(stamina, 0.0))
        current_speed_mult = speed_mult * stamina_effect
        current_speed_mult = max(current_speed_mult, MIN_SPEED_MULTIPLIER)
        current_speed = GAME_MAX_SPEED * current_speed_mult
        speeds.append(current_speed)
        
        distance += current_speed * UPDATE_INTERVAL
        if distance >= DISTANCE_METERS:
            break
        
        speed_ratio = current_speed / GAME_MAX_SPEED
        drain = base_drain + speed_drain_coeff * speed_ratio * speed_ratio
        stamina = max(stamina - drain, 0.0)
        time += UPDATE_INTERVAL
    
    avg_speed = np.mean(speeds) if speeds else 0.0
    return time, distance, avg_speed

# 搜索最佳参数
print("="*70)
print("自动优化参数 - 目标：15分27秒完成2英里")
print("="*70)
print(f"目标: {DISTANCE_METERS}米 在 {TARGET_TIME_SECONDS}秒内完成")
print(f"所需平均速度: {TARGET_AVERAGE_SPEED:.4f} m/s")
print()

best_params = None
best_score = float('inf')

# 搜索范围（扩大范围，降低体力消耗）
speed_mults = np.arange(0.70, 0.82, 0.01)  # 提高速度倍数范围
base_drains = [0.0001, 0.00015, 0.0002, 0.00025]  # 进一步降低基础消耗
speed_drain_coeffs = [0.0006, 0.0008, 0.001, 0.0012]  # 进一步降低速度消耗

print(f"搜索范围: {len(speed_mults)} × {len(base_drains)} × {len(speed_drain_coeffs)} = {len(speed_mults)*len(base_drains)*len(speed_drain_coeffs)} 种组合")
print("正在搜索...")
print()

for speed_mult in speed_mults:
    for base_drain in base_drains:
        for speed_drain_coeff in speed_drain_coeffs:
            time_result, dist_result, avg_speed_result = simulate_2miles(
                speed_mult, base_drain, speed_drain_coeff
            )
            
            # 评分：优先完成距离，然后看时间
            if dist_result >= DISTANCE_METERS:
                # 完成距离，评分 = 时间误差（越小越好）
                time_error = abs(time_result - TARGET_TIME_SECONDS) / TARGET_TIME_SECONDS
                score = time_error * 1000
                
                if score < best_score:
                    best_score = score
                    best_params = (speed_mult, base_drain, speed_drain_coeff, time_result, dist_result, avg_speed_result)
                    
                    # 显示前5个最佳结果
                    if len([x for x in globals() if x == 'found_count']) == 0:
                        globals()['found_count'] = 0
                    globals()['found_count'] = globals().get('found_count', 0) + 1
                    if globals()['found_count'] <= 5:
                        print(f"  找到参数 {globals()['found_count']}: 速度倍数={speed_mult:.3f}, "
                              f"基础消耗={base_drain:.5f}, 速度消耗={speed_drain_coeff:.4f}, "
                              f"时间={time_result:.1f}秒 ({time_result/60:.2f}分), 距离={dist_result:.1f}米, "
                              f"平均速度={avg_speed_result:.4f} m/s")

if best_params:
    print()
    print("="*70)
    print("最佳参数组合")
    print("="*70)
    print(f"速度倍数: {best_params[0]:.3f} (满体力速度: {GAME_MAX_SPEED * best_params[0]:.2f} m/s)")
    print(f"基础消耗率: {best_params[1]:.5f} (每0.2秒)")
    print(f"速度消耗系数: {best_params[2]:.4f}")
    print()
    print(f"预期结果:")
    print(f"  完成时间: {best_params[3]:.1f}秒 ({best_params[3]/60:.2f}分钟) (目标: {TARGET_TIME_SECONDS/60:.2f}分钟)")
    print(f"  完成距离: {best_params[4]:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"  平均速度: {best_params[5]:.4f} m/s (目标: {TARGET_AVERAGE_SPEED:.4f} m/s)")
    print()
    print("="*70)
    print("代码修改")
    print("="*70)
    print("SCR_RealisticStaminaSystem.c:")
    print(f"  static const float TARGET_SPEED_MULTIPLIER = {best_params[0]:.3f}; // 原值: 0.80")
    print()
    print("PlayerBase.c:")
    print(f"  float baseDrainRate = {best_params[1]:.5f}; // 原值: 0.001")
    print(f"  float speedDrainRate = {best_params[2]:.4f} * speedRatio * speedRatio; // 原值: 0.003")
else:
    print("未找到能完成2英里的参数组合")
    print("可能需要进一步降低体力消耗率或提高速度倍数")

print("="*70)
