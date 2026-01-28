#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS 参数鲁棒性验证脚本 (交叉验证)
用于检测生成的配置是否存在过拟合现象
"""

import json
import sys
from pathlib import Path
from rss_digital_twin_fix import RSSDigitalTwin, MovementType, Stance, RSSConstants

def load_config(json_path):
    with open(json_path, 'r') as f:
        data = json.load(f)
    return data['parameters']

def apply_params(constants, params):
    for k, v in params.items():
        # 将 JSON 参数名转换为常量名 (大写)
        const_name = k.upper()
        if hasattr(constants, const_name):
            setattr(constants, const_name, v)
    return constants

def run_test(name, twin, speed, weight, duration, movement_type, expected_drain_min, expected_drain_max):
    print(f"测试场景: [{name}]")
    print(f"  条件: 速度 {speed}m/s | 负重 {weight}kg | 时长 {duration}s")

    twin.reset()
    twin.stamina = 1.0
    current_time = 0.0
    steps = int(duration / 0.2)

    for _ in range(steps):
        twin.step(speed, weight, 0.0, 1.0, Stance.STAND, movement_type, current_time, enable_randomness=True)
        current_time += 0.2

    end_stamina = twin.stamina
    drain = 1.0 - end_stamina

    print(f"  结果: 剩余体力 {end_stamina*100:.1f}% | 消耗 {drain*100:.1f}%")

    if expected_drain_min <= drain <= expected_drain_max:
        print("  ✅ [通过] 表现符合预期")
        return True
    else:
        if drain < expected_drain_min:
            print(f"  ❌ [失败] 消耗太少 (过拟合/无限体力风险) - 期望至少 {expected_drain_min*100}%")
        else:
            print(f"  ❌ [失败] 消耗太快 (不可玩风险) - 期望至多 {expected_drain_max*100}%")
        return False

def main():
    # 读取你最关心的预设 (例如 StandardMilsim)
    config_file = "optimized_rss_config_balanced_super.json"
    if not Path(config_file).exists():
        print(f"找不到配置文件 {config_file}，请先运行优化器。")
        return

    print("="*60)
    print(f"正在验证配置: {config_file}")
    print("="*60)

    params = load_config(config_file)
    constants = RSSConstants()
    apply_params(constants, params)
    twin = RSSDigitalTwin(constants)

    # ==========================================
    # 盲测场景 (这些场景不在优化器的训练集中)
    # ==========================================

    score = 0
    total = 3

    # 盲测 1: "医疗兵救人" (短距离极重负载冲刺)
    # 50kg 负重，冲刺 10秒。
    # 预期：应该消耗巨大，但不应该瞬间归零。
    # 逻辑：过拟合的参数可能因为没见过 50kg，导致消耗计算溢出或过小。
    if run_test("重装急救冲刺", twin, 5.2, 90+50, 10.0, MovementType.SPRINT, 0.15, 0.40):
        score += 1
    print("-" * 40)

    # 盲测 2: "狙击手转移" (中等负载，长时间蹲姿行走)
    # 20kg 负重，蹲姿行走 60秒。
    # 预期：蹲姿应该省力，消耗应极低，甚至轻微恢复。
    # 逻辑：检测姿态系数是否合理。
    # 注意：模拟器需要支持 simulate_scenario 这种复杂调用，这里简化为 step 调用，
    # 但 RSSDigitalTwin.step 内部计算消耗时会用到 posture_multiplier。
    # 我们手动设置 stance = CROUCH (1)
    print(f"测试场景: [狙击手蹲姿转移]")
    twin.reset()
    twin.stamina = 0.8
    for _ in range(int(60/0.2)):
        twin.step(1.5, 90+20, 0.0, 1.0, Stance.CROUCH, MovementType.WALK, 0.0, True)

    end_stamina = twin.stamina
    if 0.75 <= end_stamina <= 0.95: # 允许轻微消耗或轻微恢复
        print(f"  结果: 剩余 {end_stamina*100:.1f}%")
        print("  ✅ [通过] 蹲姿逻辑正常")
        score += 1
    else:
        print(f"  结果: 剩余 {end_stamina*100:.1f}%")
        print("  ❌ [失败] 蹲姿数值异常 (可能过拟合导致蹲姿回血太快或消耗太剧烈)")
    print("-" * 40)

    # 盲测 3: "极限边界" (空载，刚好在有氧阈值下跑)
    # 速度 3.0m/s (低于 Run 3.7)。
    # 预期：应该是非常缓慢的消耗，或者是平衡状态。绝不能快速掉体力。
    if run_test("有氧巡逻", twin, 3.0, 90.0, 120.0, MovementType.RUN, -0.05, 0.10):
        # -0.05 代表允许恢复 5%，0.10 代表消耗 10%
        score += 1
    print("-" * 40)

    print(f"\n最终验证得分: {score}/{total}")
    if score == total:
        print("🎉 恭喜！该配置具有良好的鲁棒性，未发现明显过拟合。")
    else:
        print("⚠️ 警告：该配置在盲测中表现不佳，建议增加优化迭代次数或收紧参数范围。")

if __name__ == "__main__":
    main()