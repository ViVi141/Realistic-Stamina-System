#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
优化器参数诊断脚本
检查数字孪生模拟器是否正确使用了优化器的参数
"""

import json
import sys
from pathlib import Path

# 读取优化器生成的配置
config_file = Path(__file__).parent / "optimized_rss_config_playability_super.json"
with open(config_file, 'r') as f:
    config = json.load(f)

print("=" * 80)
print("优化器生成的配置参数分析")
print("=" * 80)

params = config['parameters']

print("\n【关键参数对比】")
print("-" * 80)

# 能量转换系数
energy_coeff = params['energy_to_stamina_coeff']
print(f"1. ENERGY_TO_STAMINA_COEFF: {energy_coeff:.6e}")
print(f"   - stamina_constants.py 默认值: 3.50e-05")
print(f"   - 差异: {energy_coeff / 3.50e-05:.2f}x")
print(f"   - 影响: 越高 → 同样的能量消耗转换为更多体力损失 → 消耗更快")

# 基础恢复率
base_recovery = params['base_recovery_rate']
print(f"\n2. BASE_RECOVERY_RATE: {base_recovery:.6e}")
print(f"   - stamina_constants.py 默认值: 4.00e-04")
print(f"   - 差异: {base_recovery / 4.00e-04:.2f}x")
print(f"   - 影响: 越高 → 恢复越快")
print(f"   - 当前值仅为默认值的 {base_recovery / 4.00e-04 * 100:.1f}%")

# 负重恢复惩罚系数
load_penalty = params['load_recovery_penalty_coeff']
print(f"\n3. LOAD_RECOVERY_PENALTY_COEFF: {load_penalty:.6e}")
print(f"   - stamina_constants.py 默认值: 4.00e-04")
print(f"   - 差异: {load_penalty / 4.00e-04:.2f}x")
print(f"   - 影响: 越高 → 负重时恢复越慢")

# 姿态恢复倍数
print(f"\n4. 姿态恢复倍数:")
print(f"   - PRONE: {params['prone_recovery_multiplier']:.2f} (默认: 2.2)")
print(f"   - STANDING: {params['standing_recovery_multiplier']:.2f} (默认: 2.0)")
print(f"   - 影响: 决定趴下/站立时的恢复速度倍数")

# 负重对消耗的影响
drain_coeff = params['encumbrance_stamina_drain_coeff']
print(f"\n5. ENCUMBRANCE_STAMINA_DRAIN_COEFF: {drain_coeff:.2f}")
print(f"   - stamina_constants.py 默认值: 2.0")
print(f"   - 差异: {drain_coeff / 2.0:.2f}x")
print(f"   - 影响: 越高 → 负重时消耗越快")

# Sprint消耗倍数
sprint_mult = params['sprint_stamina_drain_multiplier']
print(f"\n6. SPRINT_STAMINA_DRAIN_MULTIPLIER: {sprint_mult:.2f}")
print(f"   - stamina_constants.py 默认值: 3.0")
print(f"   - 差异: {sprint_mult / 3.0:.2f}x")
print(f"   - 影响: 越高 → Sprint消耗越快")

print("\n" + "=" * 80)
print("【问题分析】")
print("=" * 80)

print("\n根据您的反馈：")
print("✗ 恢复速度大大超过预期")
print("✗ 消耗速度太小")

print("\n可能的根本原因：")
print("-" * 80)

# 计算实际恢复率
print("\n1. 基础恢复率过低：")
print(f"   - 优化器值: {base_recovery:.6e}")
print(f"   - 默认值: {4.00e-04:.6e}")
print(f"   - 仅相当于默认值的 {base_recovery / 4.00e-04 * 100:.1f}%")
print(f"   - 这应该导致恢复变慢，而不是变快！")

print("\n2. 能量转换系数偏高：")
print(f"   - 优化器值: {energy_coeff:.6e}")
print(f"   - 默认值: {3.50e-05:.6e}")
print(f"   - 相当于默认值的 {energy_coeff / 3.50e-05:.2f}x")
print(f"   - 这应该导致消耗变快，而不是变慢！")

print("\n3. 数字孪生模拟器可能存在的问题：")
print("   - 硬编码的值可能覆盖了优化器的参数")
print("   - 参数传递路径可能不正确")
print("   - 计算公式可能与C代码不一致")

print("\n" + "=" * 80)
print("【建议的修复方案】")
print("=" * 80)

print("\n方案1：检查数字孪生模拟器")
print("-" * 80)
print("1. 检查 rss_digital_twin_fix.py 中是否使用了硬编码值")
print("2. 确认所有优化器参数都正确传递到数字孪生")
print("3. 验证计算公式与C代码一致")

print("\n方案2：调整优化器的搜索空间")
print("-" * 80)
print("1. 提高 BASE_RECOVERY_RATE 的下限（当前: 1.5e-4）")
print("2. 降低 ENERGY_TO_STAMINA_COEFF 的上限（当前: 7e-5）")
print("3. 添加更严格的约束条件")

print("\n方案3：增加恢复时间和消耗时间的约束")
print("-" * 80)
print("1. 添加恢复时间约束（例如：从0%恢复到80%需要多少秒）")
print("2. 添加消耗时间约束（例如：从100%跑到0%需要多少秒）")
print("3. 将这些约束添加到优化器的目标函数中")

print("\n" + "=" * 80)
print("【下一步行动】")
print("=" * 80)

print("\n我建议按照以下步骤进行：")
print("1. 首先检查数字孪生模拟器中的硬编码值")
print("2. 修复参数传递问题")
print("3. 重新运行优化器")
print("4. 验证生成的配置是否符合预期")

print("\n是否需要我执行这些步骤？(y/n)")