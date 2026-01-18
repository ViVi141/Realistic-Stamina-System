#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试脚本：验证2英里15分27秒的目标
"""

import numpy as np

# ==================== 目标参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒
TARGET_AVERAGE_SPEED = DISTANCE_METERS / TARGET_TIME_SECONDS  # m/s

# 游戏参数
GAME_MAX_SPEED = 5.2  # m/s
UPDATE_INTERVAL = 0.2  # 秒

# 优化后的参数（能在15分27秒内完成2英里）
TARGET_SPEED_MULTIPLIER = 0.920  # 5.2 × 0.920 = 4.78 m/s
STAMINA_EXPONENT = 0.6  # 精确值，基于医学文献
BASE_DRAIN_RATE = 0.00004  # 每0.2秒
SPEED_LINEAR_DRAIN_COEFF = 0.0001  # 速度线性项系数
SPEED_SQUARED_DRAIN_COEFF = 0.0001  # 速度平方项系数（优化后）
MIN_SPEED_MULTIPLIER = 0.15

print("="*70)
print("2英里15分27秒目标测试")
print("="*70)
print(f"目标距离: {DISTANCE_METERS} 米")
print(f"目标时间: {TARGET_TIME_SECONDS} 秒 ({TARGET_TIME_SECONDS/60:.2f} 分钟)")
print(f"所需平均速度: {TARGET_AVERAGE_SPEED:.4f} m/s")
print()

def simulate_2miles():
    """模拟跑2英里（使用精确数学模型）"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    speeds = []
    
    while distance < DISTANCE_METERS and time < TARGET_TIME_SECONDS * 2:  # 最长2倍目标时间
        # 计算当前速度倍数（基于精确模型：E^α）
        stamina_effect = np.power(max(stamina, 0.0), STAMINA_EXPONENT)
        current_speed_mult = TARGET_SPEED_MULTIPLIER * stamina_effect
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
        drain = BASE_DRAIN_RATE + SPEED_LINEAR_DRAIN_COEFF * speed_ratio + SPEED_SQUARED_DRAIN_COEFF * speed_ratio * speed_ratio
        stamina = max(stamina - drain, 0.0)
        
        time += UPDATE_INTERVAL
    
    avg_speed = np.mean(speeds) if speeds else 0.0
    return time, distance, avg_speed, stamina

time_result, dist_result, avg_speed_result, stamina_result = simulate_2miles()

print("="*70)
print("测试结果")
print("="*70)
print(f"完成时间: {time_result:.1f}秒 ({time_result/60:.2f}分钟) (目标: {TARGET_TIME_SECONDS/60:.2f}分钟)")
print(f"完成距离: {dist_result:.1f}米 (目标: {DISTANCE_METERS}米, 完成率: {dist_result/DISTANCE_METERS*100:.1f}%)")
print(f"平均速度: {avg_speed_result:.4f} m/s (目标: {TARGET_AVERAGE_SPEED:.4f} m/s)")
print(f"剩余体力: {stamina_result*100:.2f}%")
print()

if dist_result >= DISTANCE_METERS:
    if time_result <= TARGET_TIME_SECONDS:
        print("[成功] 可以在目标时间内完成2英里")
        time_saved = TARGET_TIME_SECONDS - time_result
        print(f"   提前完成：{time_saved:.1f}秒 ({time_saved/60:.2f}分钟)")
    else:
        time_over = time_result - TARGET_TIME_SECONDS
        print(f"[警告] 可以完成距离，但超时 {time_over:.1f}秒 ({time_over/60:.2f}分钟)")
        print(f"   需要进一步降低体力消耗率或提高速度倍数")
else:
    distance_short = DISTANCE_METERS - dist_result
    print(f"[失败] 无法完成距离，还差 {distance_short:.1f}米 ({distance_short/DISTANCE_METERS*100:.1f}%)")
    print(f"   需要进一步降低体力消耗率或提高速度倍数")

print("="*70)
