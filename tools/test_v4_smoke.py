#!/usr/bin/env python3
"""Smoke test for rss_pipeline_v4.py"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))

from rss_pipeline_v4 import (
    MissionLibrary, simulate_mission, compute_metrics,
    RSSDigitalTwin, RSSConstants
)

twin = RSSDigitalTwin(RSSConstants())
missions = MissionLibrary.all_missions()

print("=== V4 Mission Smoke Test (default constants, 5 missions) ===\n")
results = []
for m in missions:
    r = simulate_mission(twin, m)
    results.append(r)
    status = "PASS" if r.completion_possible else "FAIL"
    print(f"{status} {m.name}: min={r.min_stamina:.3f}  active_mean={r.mean_stamina_active:.3f}  "
          f"rec={r.recovery_gain:.4f}  exh={r.exhaustion_duration_s:.0f}s")

params = {
    'energy_to_stamina_coeff': 9e-7,
    'base_recovery_rate': 1.5e-4,
    'standing_recovery_multiplier': 1.3,
    'crouching_recovery_multiplier': 1.4,
    'prone_recovery_multiplier': 1.6,
    'fast_recovery_multiplier': 1.6,
    'medium_recovery_multiplier': 1.3,
    'slow_recovery_multiplier': 0.6,
    'encumbrance_speed_penalty_coeff': 0.20,
    'encumbrance_stamina_drain_coeff': 1.5,
    'load_recovery_penalty_coeff': 1e-4,
    'posture_crouch_multiplier': 1.8,
    'posture_prone_multiplier': 2.5,
    'sprint_speed_boost': 0.30,
    'recovery_nonlinear_coeff': 0.5,
    'max_recovery_per_tick': 4e-4,
}
m = compute_metrics(results, params)
print(f"\n=== Metrics ===")
print(f"combat_endurance:   {m.combat_endurance:.4f}")
print(f"recovery_efficiency: {m.recovery_efficiency:.4f}")
print(f"parameter_realism:   {m.parameter_realism:.4f}")
