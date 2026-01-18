#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
参数优化脚本
目标：找到合适的参数，使0kg负重下在15分27秒跑完2英里
"""

import numpy as np

# ==================== 目标参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒
TARGET_AVERAGE_SPEED = DISTANCE_METERS / TARGET_TIME_SECONDS  # m/s

GAME_MAX_SPEED = 5.2  # m/s
UPDATE_INTERVAL = 0.2  # 秒

print("="*70)
print("参数优化 - 目标：15分27秒完成2英里")
print("="*70)
print(f"目标距离: {DISTANCE_METERS} 米")
print(f"目标时间: {TARGET_TIME_SECONDS} 秒 ({TARGET_TIME_SECONDS/60:.2f} 分钟)")
print(f"所需平均速度: {TARGET_AVERAGE_SPEED:.4f} m/s")
print(f"游戏最大速度: {GAME_MAX_SPEED} m/s")
print()

def simulate_run(speed_multiplier, base_drain_rate, speed_drain_coeff, recovery_rate=0.0):
    """模拟跑步过程"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    speeds = []
    
    while stamina > 0.001 and time < TARGET_TIME_SECONDS * 2:  # 最长2倍目标时间
        # 计算当前速度倍数（基于体力）
        stamina_effect = np.sqrt(max(stamina, 0.0))
        current_speed_mult = speed_multiplier * stamina_effect
        current_speed_mult = max(current_speed_mult, 0.15)  # 最小速度
        
        # 计算当前速度
        current_speed = GAME_MAX_SPEED * current_speed_mult
        speeds.append(current_speed)
        
        # 更新距离
        distance += current_speed * UPDATE_INTERVAL
        
        # 如果已跑完距离，停止
        if distance >= DISTANCE_METERS:
            break
        
        # 更新体力
        speed_ratio = current_speed / GAME_MAX_SPEED
        drain = base_drain_rate + speed_drain_coeff * speed_ratio * speed_ratio
        stamina = max(stamina - drain, 0.0)
        
        # 恢复（如果有）
        if recovery_rate > 0 and current_speed < 0.1:
            stamina = min(stamina + recovery_rate, 1.0)
        
        time += UPDATE_INTERVAL
    
    avg_speed = np.mean(speeds) if speeds else 0.0
    return time, distance, avg_speed, stamina

# 当前参数
current_speed_mult = 0.80
current_base_drain = 0.001
current_speed_drain_coeff = 0.003

print("="*70)
print("当前参数测试")
print("="*70)
time_current, dist_current, avg_speed_current, stamina_current = simulate_run(
    current_speed_mult, current_base_drain, current_speed_drain_coeff
)
print(f"速度倍数: {current_speed_mult}, 基础消耗: {current_base_drain}, 速度消耗系数: {current_speed_drain_coeff}")
print(f"完成时间: {time_current:.1f}秒 ({time_current/60:.2f}分钟)")
print(f"完成距离: {dist_current:.1f}米")
print(f"平均速度: {avg_speed_current:.4f} m/s")
print(f"剩余体力: {stamina_current*100:.2f}%")
print()

# 优化参数搜索
print("="*70)
print("参数优化搜索")
print("="*70)

best_params = None
best_error = float('inf')
found_count = 0

# 搜索范围（扩大范围，降低体力消耗）
speed_mults = np.arange(0.65, 0.72, 0.01)
base_drains = [0.0002, 0.0003, 0.0004, 0.0005]
speed_drain_coeffs = [0.001, 0.0015, 0.002, 0.0025]

print(f"搜索范围: {len(speed_mults)} × {len(base_drains)} × {len(speed_drain_coeffs)} = {len(speed_mults)*len(base_drains)*len(speed_drain_coeffs)} 种组合")
print()

for speed_mult in speed_mults:
    for base_drain in base_drains:
        for speed_drain_coeff in speed_drain_coeffs:
            time_result, dist_result, avg_speed_result, stamina_result = simulate_run(
                speed_mult, base_drain, speed_drain_coeff
            )
            
            # 计算误差（时间和距离的加权误差）
            time_error = abs(time_result - TARGET_TIME_SECONDS) / TARGET_TIME_SECONDS
            dist_error = abs(dist_result - DISTANCE_METERS) / DISTANCE_METERS if dist_result > 0 else 1.0
            
            # 如果距离不够，大幅惩罚
            if dist_result < DISTANCE_METERS:
                error = time_error + dist_error * 10  # 距离不够是严重问题
            else:
                # 如果距离足够，主要看时间误差
                error = time_error + dist_error * 0.1
            
            # 更新最佳参数
            # 放宽条件：至少完成95%距离（3218.7 * 0.95 ≈ 3057米），或者时间在合理范围内
            min_distance_required = DISTANCE_METERS * 0.95
            if dist_result >= min_distance_required or (time_result <= TARGET_TIME_SECONDS * 1.2 and dist_result >= DISTANCE_METERS * 0.9):
                # 计算评分：优先考虑完成距离，然后是时间
                score = 0
                if dist_result >= DISTANCE_METERS:
                    # 完成距离，时间越接近目标越好
                    time_score = 1.0 - abs(time_result - TARGET_TIME_SECONDS) / TARGET_TIME_SECONDS
                    score = 100 + time_score * 100
                else:
                    # 未完成距离，距离越接近越好
                    dist_score = dist_result / DISTANCE_METERS
                    score = dist_score * 100
                
                if score > best_error or best_params is None:
                    best_error = score
                    best_params = (speed_mult, base_drain, speed_drain_coeff, time_result, dist_result, avg_speed_result)
                    
                    found_count += 1
                    if found_count <= 10:
                        print(f"  候选参数 {found_count}: 速度倍数={speed_mult:.3f}, "
                              f"基础消耗={base_drain:.4f}, 速度消耗={speed_drain_coeff:.4f}, "
                              f"时间={time_result:.1f}秒 ({time_result/60:.2f}分), 距离={dist_result:.1f}米 ({dist_result/DISTANCE_METERS*100:.1f}%), "
                              f"平均速度={avg_speed_result:.4f} m/s, 评分={score:.1f}")

if best_params:
    print(f"找到最佳参数组合:")
    print(f"  速度倍数: {best_params[0]:.3f}")
    print(f"  基础消耗率: {best_params[1]:.4f} (每0.2秒)")
    print(f"  速度消耗系数: {best_params[2]:.4f}")
    print()
    print(f"预期结果:")
    print(f"  完成时间: {best_params[3]:.1f}秒 ({best_params[3]/60:.2f}分钟) (目标: {TARGET_TIME_SECONDS/60:.2f}分钟)")
    print(f"  完成距离: {best_params[4]:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"  平均速度: {best_params[5]:.4f} m/s (目标: {TARGET_AVERAGE_SPEED:.4f} m/s)")
    print()
    print("="*70)
    print("需要修改的代码参数")
    print("="*70)
    print(f"SCR_RealisticStaminaSystem.c:")
    print(f"  TARGET_SPEED_MULTIPLIER = {best_params[0]:.3f}; // 原值: 0.80")
    print()
    print(f"PlayerBase.c:")
    print(f"  baseDrainRate = {best_params[1]:.4f}; // 原值: 0.001")
    print(f"  speedDrainRate = {best_params[2]:.4f} * speedRatio * speedRatio; // 原值: 0.003")
else:
    print("未找到合适的参数组合，可能需要扩大搜索范围或调整模型")

print("="*70)
