#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对 ENCUMBRANCE_SPEED_PENALTY_{COEFF,EXP,MAX} 做网格搜索，记录在 29kg 下对 Sprint 持续时间的影响。
输出：tools/validation_reports/encumbrance_tuning.csv
"""
import csv
import itertools
from pathlib import Path
from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin, MovementType, Stance

REPORT_DIR = Path(__file__).parent / 'validation_reports'
REPORT_DIR.mkdir(exist_ok=True)
CSV_PATH = REPORT_DIR / 'encumbrance_tuning.csv'

COEFFS = [0.15, 0.2, 0.25, 0.3, 0.4]
EXPS = [1.2, 1.5, 1.8, 2.0]
MAXS = [0.6, 0.75, 0.9]
LOAD_KG = 29.0
SPEED = 5.0
THRESHOLD = 0.25
DT = 0.2
MAX_SECONDS = 300.0


def simulate_sprint_duration(coeff, exp, max_pen):
    const = RSSConstants()
    const.ENCUMBRANCE_SPEED_PENALTY_COEFF = coeff
    const.ENCUMBRANCE_SPEED_PENALTY_EXPONENT = exp
    const.ENCUMBRANCE_SPEED_PENALTY_MAX = max_pen

    twin = RSSDigitalTwin(const)
    twin.reset()
    twin.stamina = 1.0
    body = getattr(const, 'CHARACTER_WEIGHT', 90.0)
    current_weight = body + LOAD_KG
    t = 0.0
    steps = int(MAX_SECONDS / DT)
    for _ in range(steps):
        twin.step(
            speed=SPEED,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.SPRINT,
            current_time=t,
            enable_randomness=False
        )
        t += DT
        if twin.stamina < THRESHOLD:
            return round(t, 3)
    return float('inf')


if __name__ == '__main__':
    rows = []
    for coeff, exp, max_pen in itertools.product(COEFFS, EXPS, MAXS):
        dur = simulate_sprint_duration(coeff, exp, max_pen)
        rows.append({'coeff': coeff, 'exp': exp, 'max_penalty': max_pen, 'sprint_duration_s': dur})

    with open(CSV_PATH, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=['coeff','exp','max_penalty','sprint_duration_s'])
        writer.writeheader()
        for r in rows:
            writer.writerow(r)

    print(f"Encumbrance tuning CSV generated: {CSV_PATH}")