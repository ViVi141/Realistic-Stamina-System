#!/usr/bin/env python3
"""v6 smoke tests (CP-W' + metabolism twin)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_digital_twin_fix import (
    V6CriticalPowerState,
    V6_SPRINT_POWER_CAP_WATTS_DEFAULT,
    compute_cp_watts,
    get_drain_velocity_ms,
    invert_speed_for_power_watts,
    metabolism_power_watts,
    simulate_v6_sprint_seconds,
)


SCENARIOS = [
    ("drain_applied_limit", lambda: get_drain_velocity_ms(5.5, 4.0) == 4.0),
    ("metabolism_power_positive", lambda: metabolism_power_watts(1.4, 125.0) > 100.0),
    ("invert_speed_monotonic", lambda: invert_speed_for_power_watts(500.0, 125.0, movement_phase=2) > 0.8),
    ("cp_load_penalty", lambda: compute_cp_watts(400.0, 35.0, 0.0) < 400.0),
    ("wprime_discharge", lambda: _wprime_discharge_ok()),
    ("elite_sprint_duration", lambda: simulate_v6_sprint_seconds(35.0, 400.0) <= 15.0),
]


def _wprime_discharge_ok() -> bool:
    st = V6CriticalPowerState(cp0=400.0, load_kg=0.0)
    dt = 0.017
    cap = 1450.0
    for _ in range(900):
        cp = st.cp_watts()
        burst = st.w_prime_joules / dt
        p = min(cap, cp + burst)
        st.tick(p, True, 0.0, dt)
    return st.pool01 < 0.25


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
        print(f"test_v6_smoke: {failed} failure(s)")
        return 1
    print(f"test_v6_smoke: {len(SCENARIOS)}/{len(SCENARIOS)} passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
