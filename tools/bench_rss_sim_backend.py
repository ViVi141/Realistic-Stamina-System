#!/usr/bin/env python3
"""Quick benchmark: Rust vs Python mission suite."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_pipeline_v4 import MissionLibrary
from rss_pipeline_v6 import load_preset_params, mission_sustain_run_35kg
from rss_sim_backend import run_mission_suite, use_rust_backend


def main() -> int:
    params = load_preset_params("EliteStandard")
    missions = MissionLibrary.all_missions()[:2]
    missions.append(mission_sustain_run_35kg(60.0))

    print(f"backend={'rust' if use_rust_backend() else 'python'}")
    print(f"missions={len(missions)} fast_mode=True")

    t0 = time.perf_counter()
    run_mission_suite(params, True, missions)
    elapsed = time.perf_counter() - t0
    print(f"elapsed={elapsed:.3f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
