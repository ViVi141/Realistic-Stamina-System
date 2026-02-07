#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成验证报告 CSV：对优化器导出的 JSON 预设运行数字孪生验证并汇总结果
输出目录：tools/validation_reports/
"""
import csv
import json
from pathlib import Path
from verify_json_params import load_json_params, apply_params_to_constants, run_acft_test, run_walk_recovery_test
from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin
from rss_super_pipeline import RSSSuperPipeline

REPORT_DIR = Path(__file__).parent / "validation_reports"
REPORT_DIR.mkdir(exist_ok=True)
CSV_PATH = REPORT_DIR / "validation_summary.csv"

PRESETS = [
    'optimized_rss_config_balanced_super.json',
    'optimized_rss_config_realism_super.json',
    'optimized_rss_config_playability_super.json'
]

pipeline = RSSSuperPipeline(n_trials=1)

rows = []
for name in PRESETS:
    p = Path(__file__).parent / name
    if not p.exists():
        continue
    params = load_json_params(str(p))
    const = RSSConstants()
    apply_params_to_constants(const, params)
    twin = RSSDigitalTwin(const)

    # Run 60s run
    twin.reset(); twin.stamina = 1.0
    for i in range(300):
        twin.step(speed=3.7, current_weight=90.0, grade_percent=0.0, terrain_factor=1.0,
                  stance=0, movement_type=0, current_time=i*0.2, enable_randomness=False)
    run_drop = 1.0 - twin.stamina

    # Sprint 30s
    twin.reset(); twin.stamina = 1.0
    for i in range(150):
        twin.step(speed=5.0, current_weight=90.0, grade_percent=0.0, terrain_factor=1.0,
                  stance=0, movement_type=0, current_time=i*0.2, enable_randomness=False)
    sprint_drop = 1.0 - twin.stamina

    # ACFT
    twin.reset(); twin.stamina = 1.0
    acft = run_acft_test(twin)
    acft_min = acft.get('min_stamina', 0.0)

    # Walk recovery
    walk_delta = run_walk_recovery_test(twin)

    # Session metrics (engagement_loss, session_fail)
    session_loss, session_fail = pipeline._evaluate_session_metrics(RSSDigitalTwin(const))

    row = {
        'preset': name,
        'energy_to_stamina_coeff': params.get('energy_to_stamina_coeff'),
        'run_drop_pct': round(run_drop*100, 3),
        'sprint_drop_pct': round(sprint_drop*100, 3),
        'acft_min_pct': round(acft_min*100, 3),
        'acft_ok': acft_min >= 0.20,
        'walk_delta_pct': round(walk_delta*100, 3),
        'walk_ok': 0.002 <= walk_delta <= 0.03,
        'engagement_loss': round(float(session_loss), 3),
        'session_fail': bool(session_fail)
    }
    rows.append(row)

    # 保存单独 JSON 报告
    with open(REPORT_DIR / f"{p.stem}_report.json", 'w', encoding='utf-8') as f:
        json.dump(row, f, indent=2, ensure_ascii=False)

# 写CSV
with open(CSV_PATH, 'w', newline='', encoding='utf-8') as f:
    writer = csv.DictWriter(f, fieldnames=[
        'preset','energy_to_stamina_coeff','run_drop_pct','sprint_drop_pct','acft_min_pct','acft_ok','walk_delta_pct','walk_ok','engagement_loss','session_fail'
    ])
    writer.writeheader()
    for r in rows:
        writer.writerow(r)

print(f"CSV 报告已生成：{CSV_PATH}")
print(f"单文件 JSON 报告位于：{REPORT_DIR}")
