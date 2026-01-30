#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
校准 energy_to_stamina_coeff，使 0kg 下 Run 3.5km/15:27 结束时最低体力为 20%
需求：0KG 负载，Run 状态，15分27秒跑完 3.5km，最低体力 20%
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from rss_digital_twin_fix import RSSDigitalTwin, MovementType, RSSConstants, Stance


def load_json_params(json_path: str) -> dict:
    with open(json_path, 'r', encoding='utf-8') as f:
        return json.load(f).get('parameters', {})


def apply_params(constants: RSSConstants, params: dict, energy_coeff: float) -> None:
    """应用参数，energy_coeff 可覆盖"""
    mapping = {
        'energy_to_stamina_coeff': 'ENERGY_TO_STAMINA_COEFF',
        'base_recovery_rate': 'BASE_RECOVERY_RATE',
        'standing_recovery_multiplier': 'STANDING_RECOVERY_MULTIPLIER',
        'prone_recovery_multiplier': 'PRONE_RECOVERY_MULTIPLIER',
        'load_recovery_penalty_coeff': 'LOAD_RECOVERY_PENALTY_COEFF',
        'load_recovery_penalty_exponent': 'LOAD_RECOVERY_PENALTY_EXPONENT',
        'encumbrance_speed_penalty_coeff': 'ENCUMBRANCE_SPEED_PENALTY_COEFF',
        'encumbrance_stamina_drain_coeff': 'ENCUMBRANCE_STAMINA_DRAIN_COEFF',
        'sprint_stamina_drain_multiplier': 'SPRINT_STAMINA_DRAIN_MULTIPLIER',
        'fatigue_accumulation_coeff': 'FATIGUE_ACCUMULATION_COEFF',
        'fatigue_max_factor': 'FATIGUE_MAX_FACTOR',
        'aerobic_efficiency_factor': 'AEROBIC_EFFICIENCY_FACTOR',
        'anaerobic_efficiency_factor': 'ANAEROBIC_EFFICIENCY_FACTOR',
        'recovery_nonlinear_coeff': 'RECOVERY_NONLINEAR_COEFF',
        'fast_recovery_multiplier': 'FAST_RECOVERY_MULTIPLIER',
        'medium_recovery_multiplier': 'MEDIUM_RECOVERY_MULTIPLIER',
        'slow_recovery_multiplier': 'SLOW_RECOVERY_MULTIPLIER',
        'posture_crouch_multiplier': 'POSTURE_CROUCH_MULTIPLIER',
        'posture_prone_multiplier': 'POSTURE_PRONE_MULTIPLIER',
    }
    for jk, ck in mapping.items():
        if jk in params and hasattr(constants, ck):
            v = energy_coeff if jk == 'energy_to_stamina_coeff' else params[jk]
            setattr(constants, ck, v)


def simulate_run_3_5km(constants: RSSConstants) -> tuple:
    """模拟 0kg Run 3.5km in 15:27，返回 (最终体力, 最低体力)"""
    twin = RSSDigitalTwin(constants)
    twin.reset()
    twin.stamina = 1.0

    # 3.5km / 15:27 = 3500m / 927s ≈ 3.776 m/s
    duration_sec = 15 * 60 + 27  # 927
    speed = 3500.0 / duration_sec  # 3.776
    steps = int(duration_sec / 0.2)
    # 0kg = 体重90 + 基准装备 1.36 = 91.36
    current_weight = 90.0 + 1.36

    min_stamina = 1.0
    for i in range(steps):
        twin.step(
            speed=speed,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            current_time=i * 0.2,
            enable_randomness=False,
        )
        min_stamina = min(min_stamina, twin.stamina)

    return (twin.stamina, min_stamina)


def main():
    # 工作台使用 EliteStandard，校准 realism JSON
    json_path = Path(__file__).parent / 'optimized_rss_config_realism_super.json'
    params = load_json_params(str(json_path))

    constants = RSSConstants()

    target_min = 0.20
    low, high = 1.0e-6, 5.0e-4
    best_coeff = 2.0e-5
    best_min = 0.0

    for _ in range(40):  # 二分搜索
        mid = (low + high) / 2
        apply_params(constants, params, mid)
        _, min_st = simulate_run_3_5km(constants)
        if min_st < target_min:
            high = mid
        else:
            low = mid
            best_coeff = mid
            best_min = min_st

    # 再跑一次取最终值
    apply_params(constants, params, best_coeff)
    final_st, min_st = simulate_run_3_5km(constants)

    print("=== 校准结果：0KG Run 3.5km / 15:27 → 最低体力 20% ===")
    print(f"energy_to_stamina_coeff: {best_coeff:.6e}")
    print(f"模拟结果: 最终体力 {final_st*100:.1f}%, 最低体力 {min_st*100:.1f}%")
    print()
    print("请将上述 energy_to_stamina_coeff 写入对应预设 JSON（realism→EliteStandard/工作台）")
    print("并运行 embed_json_to_c.py 嵌入 C 文件。")


if __name__ == '__main__':
    main()
