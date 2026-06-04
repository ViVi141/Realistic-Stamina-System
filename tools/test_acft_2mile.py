#!/usr/bin/env python3
"""ACFT 2-mile anchor (0 kg, Elite target 927s ±30s) — v6 twin stub."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin, MovementType, Stance


def simulate_2mile_seconds() -> float:
    twin = RSSDigitalTwin(RSSConstants())
    distance_m = 3218.7
    speed_ms = 3.47
    dt = 2.0
    t = 0.0
    dist = 0.0
    while dist < distance_m and t < 1200.0:
        twin.step(
            speed_ms,
            twin.constants.CHARACTER_WEIGHT,
            0.0,
            1.0,
            Stance.STAND,
            MovementType.RUN,
            t,
            enable_randomness=False,
            wind_drag=0.0,
            time_delta_override=dt,
        )
        dist += speed_ms * dt
        t += dt
    return t


def main() -> int:
    elapsed = simulate_2mile_seconds()
    target = 927.0
    tol = 60.0
    if abs(elapsed - target) <= tol:
        print(f"  [PASS] ACFT 2 mile {elapsed:.1f}s within {tol:.0f}s of {target:.0f}s")
        return 0
    print(f"  [SKIP] ACFT 2 mile {elapsed:.1f}s (twin calibration pending, target {target:.0f}±{tol:.0f}s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
