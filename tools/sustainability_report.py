#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
计算在 30kg 负载、标准环境（grade=0, terrain=1.0）下，不同移动类型持续到体力低于阈值（Collapse阈值25%）所需时间。
对比：默认常量 与 三个导出预设（balanced, realism, playability）
输出 CSV 到 tools/validation_reports/sustainability_report.csv
"""
import json
from pathlib import Path
from typing import Dict
from rss_digital_twin_fix import RSSDigitalTwin, RSSConstants, MovementType, Stance

PRESETS = [
    'optimized_rss_config_balanced_super.json',
    'optimized_rss_config_realism_super.json',
    'optimized_rss_config_playability_super.json'
]
REPORT_DIR = Path(__file__).parent / 'validation_reports'
REPORT_DIR.mkdir(exist_ok=True)
CSV_PATH = REPORT_DIR / 'sustainability_report_29kg.csv'

MOVES = [
    ('IDLE', MovementType.IDLE, 0.0),
    ('WALK', MovementType.WALK, 1.8),
    ('RUN', MovementType.RUN, 3.7),
    ('SPRINT', MovementType.SPRINT, 5.0)
]

THRESHOLD = 0.25  # Collapse threshold
LOAD_KG = 29.0
MAX_SECONDS = 7200.0  # 2 hours cap to avoid infinite loops
DT = 0.2


def load_json_params(json_path: Path) -> Dict:
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    return data.get('parameters', {})


def apply_params_to_constants(constants: RSSConstants, params: Dict):
    # simple mapping similar to verify script
    for k, v in params.items():
        if hasattr(constants, k.upper()) or hasattr(constants, k):
            # try both naming styles
            if hasattr(constants, k.upper()):
                setattr(constants, k.upper(), v)
            else:
                setattr(constants, k, v)


def simulate_until_threshold(constants: RSSConstants, movement_type: int, speed: float, load_kg: float, threshold: float) -> float:
    twin = RSSDigitalTwin(constants)
    twin.reset()
    twin.stamina = 1.0
    body = getattr(constants, 'CHARACTER_WEIGHT', 90.0)
    current_weight = body + load_kg
    t = 0.0
    steps = int(MAX_SECONDS / DT)
    for i in range(steps):
        twin.step(
            speed=speed,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=movement_type,
            current_time=t,
            enable_randomness=False
        )
        t += DT
        if twin.stamina < threshold:
            return t
    return float('inf')


if __name__ == '__main__':
    import csv

    rows = []

    # Default constants
    default_const = RSSConstants()
    for name, mtype, speed in MOVES:
        dur = simulate_until_threshold(default_const, mtype, speed, LOAD_KG, THRESHOLD)
        rows.append({'preset': 'DEFAULT', 'movement': name, 'duration_s': round(dur, 2), 'duration_min': round(dur/60.0, 2) if dur!=float('inf') else 'inf'})

    # Presets
    base = Path(__file__).parent
    for preset in PRESETS:
        p = base / preset
        if not p.exists():
            continue
        params = load_json_params(p)
        const = RSSConstants()
        # apply param names from JSON to constants (keys are lower_snake in JSON)
        for k, v in params.items():
            # map known keys
            key_map = {
                'energy_to_stamina_coeff': 'ENERGY_TO_STAMINA_COEFF',
                'base_recovery_rate': 'BASE_RECOVERY_RATE',
                'prone_recovery_multiplier': 'PRONE_RECOVERY_MULTIPLIER',
                'standing_recovery_multiplier': 'STANDING_RECOVERY_MULTIPLIER',
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
                'min_recovery_stamina_threshold': 'MIN_RECOVERY_STAMINA_THRESHOLD',
                'sprint_speed_boost': 'SPRINT_SPEED_BOOST',
                'posture_crouch_multiplier': 'POSTURE_CROUCH_MULTIPLIER',
                'posture_prone_multiplier': 'POSTURE_PRONE_MULTIPLIER',
            }
            key = key_map.get(k, None)
            if key and hasattr(const, key):
                setattr(const, key, v)
        for name, mtype, speed in MOVES:
            dur = simulate_until_threshold(const, mtype, speed, LOAD_KG, THRESHOLD)
            rows.append({'preset': preset.replace('.json',''), 'movement': name, 'duration_s': round(dur,2), 'duration_min': round(dur/60.0,2) if dur!=float('inf') else 'inf'})

    # write CSV
    with open(CSV_PATH, 'w', newline='', encoding='utf-8') as f:
        fieldnames = ['preset','movement','duration_s','duration_min']
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in rows:
            writer.writerow(r)

    print(f"Sustainability report generated: {CSV_PATH}")
