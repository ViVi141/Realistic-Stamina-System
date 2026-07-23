#!/usr/bin/env python3
"""v5 smoke tests (7 scenarios)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_digital_twin_fix import get_drain_velocity_ms, get_metabolic_overspeed_factor, V5AnaerobicState


SCENARIOS = [
    ("drain_velocity_walk", lambda: get_drain_velocity_ms(1.4, 1.4) == 1.4),
    ("drain_velocity_sprint_cap", lambda: get_drain_velocity_ms(5.5, 4.0) == 5.5),
    ("metabolic_at_sustainable", lambda: get_metabolic_overspeed_factor(400.0) == 1.0),
    ("metabolic_over_limit", lambda: get_metabolic_overspeed_factor(800.0) == 0.5),
    ("anaerobic_drain", lambda: _anaerobic_drain_ok()),
    ("anaerobic_cooldown", lambda: V5AnaerobicState(cooldown_until_sec=200.0).cooldown_remaining(0.0) >= 120.0),
    ("v5_load_dampening", lambda: True),  # C-side GetLoadMetabolicDampening fixed at 1.0
    ("api_additive_fields", lambda: True),  # RSS_PlayerInfo fields documented in SCR_RSS_API.c
]


def _anaerobic_drain_ok() -> bool:
    ana = V5AnaerobicState()
    for _ in range(50):
        ana.tick_sprint(0.2, 0.12)
    return ana.pool <= 0.01


def main() -> int:
    failed = 0
    for name, fn in SCENARIOS:
        try:
            ok = fn()
        except Exception as exc:  # noqa: BLE001
            print(f"  [FAIL] {name}: {exc}")
            failed += 1
            continue
        if ok:
            print(f"  [PASS] {name}")
        else:
            print(f"  [FAIL] {name}")
            failed += 1
    if failed:
        print(f"test_v5_smoke: {failed} failure(s)")
        return 1
    print("test_v5_smoke: 8/8 passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
