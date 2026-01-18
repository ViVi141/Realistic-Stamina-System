#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
参数调整脚本
目标：0kg负重下在15分27秒跑完2英里
"""

import numpy as np

# ==================== 目标参数 ====================
DISTANCE_MILES = 2.0  # 英里
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_MINUTES = 15
TARGET_TIME_SECONDS = 27
TARGET_TIME_TOTAL = TARGET_TIME_MINUTES * 60 + TARGET_TIME_SECONDS  # 927秒
TARGET_AVERAGE_SPEED = DISTANCE_METERS / TARGET_TIME_TOTAL  # m/s

print("="*60)
print("参数调整计算")
print("="*60)
print(f"目标距离: {DISTANCE_MILES} 英里 ({DISTANCE_METERS} 米)")
print(f"目标时间: {TARGET_TIME_MINUTES}分{TARGET_TIME_SECONDS}秒 ({TARGET_TIME_TOTAL} 秒)")
print(f"所需平均速度: {TARGET_AVERAGE_SPEED:.4f} m/s")
print()

# 游戏参数
GAME_MAX_SPEED = 5.2  # m/s

# 当前参数
CURRENT_TARGET_SPEED_MULTIPLIER = 0.80  # 当前满体力速度倍数
CURRENT_FULL_SPEED = GAME_MAX_SPEED * CURRENT_TARGET_SPEED_MULTIPLIER  # 4.16 m/s

print(f"游戏最大速度: {GAME_MAX_SPEED} m/s")
print(f"当前满体力速度倍数: {CURRENT_TARGET_SPEED_MULTIPLIER} ({CURRENT_FULL_SPEED} m/s)")
print()

# 计算所需的速度倍数
# 考虑到体力会下降，平均速度会低于满体力速度
# 根据医学模型：S(E) = S_max * sqrt(E)
# 如果我们假设体力线性下降（简化），平均速度 ≈ S_max * (2/3)
# 但实际模型是平方根，更复杂

# 方法1：假设满体力速度需要达到 TARGET_AVERAGE_SPEED * 1.1（留10%余量）
ESTIMATED_FULL_SPEED = TARGET_AVERAGE_SPEED * 1.1
ESTIMATED_SPEED_MULTIPLIER = ESTIMATED_FULL_SPEED / GAME_MAX_SPEED

print("="*60)
print("建议参数调整")
print("="*60)
print(f"建议满体力速度: {ESTIMATED_FULL_SPEED:.4f} m/s")
print(f"建议速度倍数: {ESTIMATED_SPEED_MULTIPLIER:.4f}")
print()

# 验证：如果使用平方根模型，计算平均速度
print("="*60)
print("模型验证（使用平方根模型）")
print("="*60)

# 测试不同的速度倍数
test_multipliers = np.arange(0.60, 0.75, 0.01)
best_multiplier = None
best_avg_speed = 0
best_distance = 0

for test_mult in test_multipliers:
    # 模拟跑步过程
    stamina = 1.0
    distance = 0.0
    time = 0.0
    speeds = []
    
    # 简化的体力消耗率（基于当前速度）
    BASE_DRAIN = 0.001  # 每0.2秒
    SPEED_DRAIN_COEFF = 0.003
    
    while stamina > 0.01 and time < TARGET_TIME_TOTAL:
        # 计算当前速度
        stamina_effect = np.sqrt(stamina)
        current_speed_mult = test_mult * stamina_effect
        current_speed_mult = max(current_speed_mult, 0.15)  # 最小速度
        current_speed = GAME_MAX_SPEED * current_speed_mult
        
        speeds.append(current_speed)
        
        # 更新距离
        distance += current_speed * 0.2
        
        # 更新体力
        speed_ratio = current_speed / GAME_MAX_SPEED
        drain = BASE_DRAIN + SPEED_DRAIN_COEFF * speed_ratio * speed_ratio
        stamina = max(stamina - drain, 0.0)
        
        # 更新时间
        time += 0.2
        
        # 如果已跑完距离，停止
        if distance >= DISTANCE_METERS:
            break
    
    if len(speeds) > 0:
        avg_speed = np.mean(speeds)
        if distance >= DISTANCE_METERS:
            # 找到能完成的倍数
            if best_multiplier is None or time < best_multiplier[1]:
                best_multiplier = (test_mult, time)
                best_avg_speed = avg_speed
                best_distance = distance
                print(f"速度倍数 {test_mult:.3f}: 完成时间 {time:.1f}秒, 平均速度 {avg_speed:.4f} m/s, 距离 {distance:.1f}米")

if best_multiplier:
    print()
    print("="*60)
    print("推荐参数")
    print("="*60)
    print(f"推荐速度倍数: {best_multiplier[0]:.3f}")
    print(f"预期完成时间: {best_multiplier[1]:.1f}秒 ({best_multiplier[1]/60:.2f}分钟)")
    print(f"预期平均速度: {best_avg_speed:.4f} m/s")
    print(f"预期距离: {best_distance:.1f}米")
else:
    print("未找到合适的参数，可能需要调整体力消耗率")

print("="*60)
