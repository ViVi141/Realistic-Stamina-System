#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
计算体力恢复时间
"""

# ==================== 恢复参数 ====================
RECOVERY_RATE = 0.00015  # 每0.2秒恢复0.015%
UPDATE_INTERVAL = 0.2  # 秒

print("="*70)
print("体力恢复时间计算")
print("="*70)
print()

# 计算每秒恢复率
recovery_per_second = RECOVERY_RATE / UPDATE_INTERVAL
print(f"恢复参数：")
print(f"  每0.2秒恢复: {RECOVERY_RATE * 100:.4f}%")
print(f"  每秒恢复: {recovery_per_second * 100:.4f}%")
print()

# 计算每1%体力需要的时间
time_per_percent = 1.0 / recovery_per_second
print(f"每1%体力恢复时间: {time_per_percent:.2f}秒 ({time_per_percent/60:.2f}分钟)")
print()

# 计算从90%到100%的时间
stamina_start = 0.90
stamina_end = 1.00
stamina_diff = stamina_end - stamina_start
time_total = stamina_diff / recovery_per_second

print(f"从90%恢复到100%体力：")
print(f"  需要恢复: {stamina_diff * 100:.1f}%")
print(f"  总时间: {time_total:.2f}秒 ({time_total/60:.2f}分钟)")
print(f"  每1%需要: {time_per_percent:.2f}秒")
print()

# 验证：模拟恢复过程
print("验证（模拟恢复过程）：")
stamina = stamina_start
time = 0.0
steps = 0
target_steps = int(stamina_diff / RECOVERY_RATE)

while stamina < stamina_end and steps < target_steps + 100:
    stamina += RECOVERY_RATE
    stamina = min(stamina, 1.0)
    time += UPDATE_INTERVAL
    steps += 1
    
    # 每恢复1%输出一次
    current_percent = int(stamina * 100)
    if current_percent > int((stamina - RECOVERY_RATE) * 100) and current_percent <= 100:
        print(f"  {current_percent}% 体力 - 用时: {time:.2f}秒 (累计)")
        if current_percent == 100:
            break

print()
print(f"模拟结果：")
print(f"  最终时间: {time:.2f}秒 ({time/60:.2f}分钟)")
print(f"  最终体力: {stamina * 100:.2f}%")
print()

# 计算每1%的平均时间
if steps > 0:
    avg_time_per_percent = time / (stamina_diff * 100)
    print(f"  平均每1%时间: {avg_time_per_percent:.2f}秒")
print()

# 注意：负重不影响恢复速度
print("注意：")
print("  恢复速度不受负重影响")
print("  恢复速度是固定的：每0.2秒恢复0.015%")
print("  负重只影响移动时的体力消耗速度")
print("="*70)
