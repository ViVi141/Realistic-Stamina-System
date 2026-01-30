#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
校准静止恢复：8% -> 81% 期望 3-4 分钟
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from rss_digital_twin_fix import RSSDigitalTwin, MovementType, RSSConstants, Stance


def load_params(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f).get('parameters', {})


def apply_params(c, params, overrides=None):
    mapping = {
        'base_recovery_rate': 'BASE_RECOVERY_RATE',
        'standing_recovery_multiplier': 'STANDING_RECOVERY_MULTIPLIER',
        'prone_recovery_multiplier': 'PRONE_RECOVERY_MULTIPLIER',
        'fast_recovery_multiplier': 'FAST_RECOVERY_MULTIPLIER',
        'medium_recovery_multiplier': 'MEDIUM_RECOVERY_MULTIPLIER',
        'slow_recovery_multiplier': 'SLOW_RECOVERY_MULTIPLIER',
        'recovery_nonlinear_coeff': 'RECOVERY_NONLINEAR_COEFF',
    }
    for jk, ck in mapping.items():
        v = (overrides or {}).get(jk, params.get(jk))
        if v is not None and hasattr(c, ck):
            setattr(c, ck, v)


def simulate_recovery(base_rate, fast_mult, standing_mult, start=0.08, target=0.81):
    """静止站立恢复：从 start 恢复到 target 所需秒数"""
    params = {
        'base_recovery_rate': 0.000247,
        'fast_recovery_multiplier': 1.9,
        'standing_recovery_multiplier': 1.35,
        'recovery_nonlinear_coeff': 0.64,
    }
    params['base_recovery_rate'] = base_rate
    params['fast_recovery_multiplier'] = fast_mult
    params['standing_recovery_multiplier'] = standing_mult

    c = RSSConstants()
    apply_params(c, {}, params)

    twin = RSSDigitalTwin(c)
    twin.reset()
    twin.stamina = start
    twin.rest_duration_minutes = 0.0  # 刚开始休息

    steps = 0
    while twin.stamina < target and steps < 2000:  # 最多 400 秒
        twin.step(
            speed=0.0,
            current_weight=91.36,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.IDLE,
            current_time=steps * 0.2,
            enable_randomness=False,
        )
        steps += 1

    return steps * 0.2


if __name__ == '__main__':
    # 二分 base_recovery_rate，目标 3-4 分钟 (180-240s，取 210s)
    params = load_params(Path(__file__).parent / 'optimized_rss_config_realism_super.json')
    low, high = 5e-5, 0.001
    for _ in range(30):
        mid = (low + high) / 2
        t = simulate_recovery(mid, 1.9, 1.35)
        if t > 210:  # 恢复太慢，需提高 base
            low = mid
        else:
            high = mid

    base = (low + high) / 2
    t_final = simulate_recovery(base, 1.9, 1.35)
    print(f"base_recovery_rate: {base:.6e}")
    print(f"8% -> 81% 预计耗时: {t_final:.0f}s ({t_final/60:.1f} min)")
