#!/usr/bin/env python3
"""Smoke test for rss_pipeline_v4.py — 8 missions + 三档预设排序检查"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from rss_pipeline_v4 import (
    MissionLibrary, simulate_mission, compute_metrics,
    RSSDigitalTwin, RSSConstants, merge_game_aligned_params,
    HARDCORE_PARAM_REFS,
)

twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params({})))
missions = MissionLibrary.all_missions()

print("=== V4 Mission Smoke Test (8 missions) ===\n")
results = []
for m in missions:
    temp_info = f"temp={m.temperature}°C" if m.temperature != 20.0 else ""
    wind_info = f"wind={m.wind_speed}m/s" if m.wind_speed > 0 else ""
    env_note = f" [{temp_info} {wind_info}]".replace("  ", " ").strip()
    if env_note == "[]":
        env_note = ""

    r = simulate_mission(twin, m)
    results.append(r)
    status = "PASS" if r.completion_possible else "FAIL"
    stamina_bar = "!" if r.min_stamina < 0.15 else ("*" if r.min_stamina < 0.30 else " ")
    print(
        f"{status} [{stamina_bar}] {m.name}{env_note}: min={r.min_stamina:.3f}  "
        f"active_mean={r.mean_stamina_active:.3f}  rec={r.recovery_gain:.4f}  "
        f"idle={r.idle_duration_s:.0f}s  exh={r.exhaustion_duration_s:.0f}s"
    )

params = merge_game_aligned_params(dict(HARDCORE_PARAM_REFS))
m = compute_metrics(results, params)
print(f"\n=== Hardcore Anchor Metrics ===")
print(f"combat_ease:       {m.combat_ease:.4f}")
print(f"recovery_ease:     {m.recovery_ease:.6f}")
print(f"parameter_realism: {m.parameter_realism:.4f}")

print("\n=== Preset JSON Tier Check ===")
tools_dir = os.path.dirname(__file__)
preset_files = {
    "EliteStandard": "optimized_rss_config_elitestandard_v4.json",
    "StandardMilsim": "optimized_rss_config_standardmilsim_v4.json",
    "TacticalAction": "optimized_rss_config_tacticalaction_v4.json",
}
metrics_by_tier = {}
for tier, fname in preset_files.items():
    path = os.path.join(tools_dir, fname)
    if not os.path.isfile(path):
        print(f"  SKIP {tier}: {fname} not found")
        continue
    with open(path, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    opt_params = {k: v for k, v in cfg.items() if not k.startswith("_")}
    twin_t = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(opt_params)))
    tier_results = [simulate_mission(twin_t, mission) for mission in missions]
    tier_m = compute_metrics(tier_results, opt_params)
    metrics_by_tier[tier] = tier_m
    print(
        f"  {tier}: ease={tier_m.combat_ease:.4f}  "
        f"recovery={tier_m.recovery_ease:.6f}  realism={tier_m.parameter_realism:.4f}  "
        f"base_rec={opt_params.get('base_recovery_rate', 0):.2e}  "
        f"stand={opt_params.get('standing_recovery_multiplier', 0):.3f}"
    )

if len(metrics_by_tier) == 3:
    e = metrics_by_tier["EliteStandard"]
    s = metrics_by_tier["StandardMilsim"]
    t = metrics_by_tier["TacticalAction"]
    ok_e = e.combat_ease <= s.combat_ease <= t.combat_ease
    ok_r = e.recovery_ease <= s.recovery_ease <= t.recovery_ease
    print(f"\nTier order: combat_ease OK={ok_e}  recovery_ease OK={ok_r}")
    if not (ok_e and ok_r):
        sys.exit(1)
