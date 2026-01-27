#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复数字孪生模拟器中的硬编码值
让优化器的参数能够正确生效
"""

import re
from pathlib import Path

# 读取文件
file_path = Path(__file__).parent / "rss_digital_twin_fix.py"
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

print("=" * 80)
print("修复数字孪生模拟器中的硬编码值")
print("=" * 80)

# 1. 添加缺失的常量定义
print("\n【步骤1】添加缺失的常量定义...")
print("-" * 80)

# 在 RSSConstants 类中添加缺失的常量
constants_class_end = content.find("class EnvironmentFactor:")

if constants_class_end == -1:
    print("✗ 错误：找不到 EnvironmentFactor 类")
    exit(1)

# 添加缺失的常量定义
new_constants = """
    # 能量转换系数
    ENERGY_TO_STAMINA_COEFF = 3.50e-05  # 能量到体力的转换系数

    # 负重恢复惩罚系数
    LOAD_RECOVERY_PENALTY_COEFF = 0.0004  # 负重恢复惩罚系数
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0  # 负重恢复惩罚指数

    # 负重对消耗的影响系数
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0  # 负重对体力消耗的影响系数

    # Sprint 消耗倍数
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0  # Sprint 体力消耗倍数

    # 疲劳系统参数
    FATIGUE_ACCUMULATION_COEFF = 0.015  # 疲劳累积系数
    FATIGUE_MAX_FACTOR = 2.0  # 最大疲劳因子

    # 代谢适应参数
    AEROBIC_EFFICIENCY_FACTOR = 0.9  # 有氧效率因子
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2  # 无氧效率因子

    # Sprint 速度加成
    SPRINT_SPEED_BOOST = 0.30  # Sprint 速度加成

    # 姿态速度倍数
    POSTURE_CROUCH_MULTIPLIER = 0.7  # 蹲姿速度倍数
    POSTURE_PRONE_MULTIPLIER = 0.3  # 趴姿速度倍数

    # 动作消耗参数
    JUMP_STAMINA_BASE_COST = 0.035  # 跳跃基础消耗
    VAULT_STAMINA_START_COST = 0.02  # 翻越起始消耗
    CLIMB_STAMINA_TICK_COST = 0.01  # 攀爬每秒消耗
    JUMP_CONSECUTIVE_PENALTY = 0.5  # 连续跳跃惩罚

    # 坡度参数
    SLOPE_UPHILL_COEFF = 0.08  # 上坡系数
    SLOPE_DOWNHILL_COEFF = 0.03  # 下坡系数

    # 游泳系统参数
    SWIMMING_BASE_POWER = 20.0  # 游泳基础功率
    SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0  # 游泳负重阈值
    SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0  # 游泳静态消耗倍数
    SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0  # 游泳动态功率效率
    SWIMMING_ENERGY_TO_STAMINA_COEFF = 5e-05  # 游泳能量转换系数

    # 环境因子参数
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02  # 温度热应激惩罚系数
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05  # 温度冷应激恢复惩罚系数
    ENV_WIND_RESISTANCE_COEFF = 0.05  # 风阻系数
    ENV_MUD_PENALTY_MAX = 0.4  # 泥泞最大惩罚

    # 边际效应衰减参数
    MARGINAL_DECAY_THRESHOLD = 0.8  # 边际效应衰减阈值
    MARGINAL_DECAY_COEFF = 1.1  # 边际效应衰减系数

    # 最低恢复参数
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2  # 最低恢复体力阈值
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0  # 最低恢复休息时间

"""

# 在 EnvironmentFactor 类之前插入新常量
content = content[:constants_class_end] + new_constants + "\n" + content[constants_class_end:]
print("✓ 添加了缺失的常量定义")

# 2. 修复硬编码的负重恢复惩罚系数
print("\n【步骤2】修复硬编码的负重恢复惩罚系数...")
print("-" * 80)

# 替换第482行的硬编码值
old_482 = "load_recovery_penalty = (load_ratio ** 2.0) * 0.0004"
new_482 = "load_recovery_penalty = (load_ratio ** self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF"
content = content.replace(old_482, new_482)
print(f"✓ 修复第482行: {old_482} -> {new_482}")

# 替换第590行的硬编码值
old_590 = "load_recovery_penalty = np.power(load_ratio, 2.0) * 0.0004"
new_590 = "load_recovery_penalty = np.power(load_ratio, self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF"
content = content.replace(old_590, new_590)
print(f"✓ 修复第590行: {old_590} -> {new_590}")

# 3. 修复硬编码的消耗率（RUN速度）
print("\n【步骤3】修复硬编码的消耗率...")
print("-" * 80)

# 替换第297行和第412行的硬编码值
old_run_drain = "return 0.00008 * load_factor"
new_run_drain = "return self.constants.RUN_DRAIN_PER_TICK * load_factor"
content = content.replace(old_run_drain, new_run_drain)
print(f"✓ 修复消耗率: {old_run_drain} -> {new_run_drain}")

# 4. 修复硬编码的静态恢复率
print("\n【步骤4】修复硬编码的静态恢复率...")
print("-" * 80)

# 替换第301行和第417行的硬编码值
old_static_recovery = "return -0.00025"
new_static_recovery = "return -self.constants.REST_RECOVERY_PER_TICK"
content = content.replace(old_static_recovery, new_static_recovery)
print(f"✓ 修复静态恢复率: {old_static_recovery} -> {new_static_recovery}")

# 5. 保存修复后的文件
print("\n【步骤5】保存修复后的文件...")
print("-" * 80)

backup_path = Path(__file__).parent / "rss_digital_twin_fix.py.backup"
with open(backup_path, 'w', encoding='utf-8') as f:
    f.write(content)
print(f"[OK] 备份文件已保存: {backup_path}")

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)
print(f"[OK] 修复后的文件已保存: {file_path}")

print("\n" + "=" * 80)
print("修复完成！")
print("=" * 80)

print("\n【修复内容总结】")
print("-" * 80)
print("1. ✓ 添加了缺失的常量定义（约30个参数）")
print("2. ✓ 修复了硬编码的负重恢复惩罚系数（2处）")
print("3. ✓ 修复了硬编码的消耗率（2处）")
print("4. ✓ 修复了硬编码的静态恢复率（2处）")

print("\n【下一步】")
print("-" * 80)
print("1. 重新运行优化器：python rss_super_pipeline.py")
print("2. 验证生成的配置是否符合预期")
print("3. 在游戏中测试新的配置")

print("\n是否需要立即重新运行优化器？(y/n)")

print("\n是否需要立即重新运行优化器？(y/n)")