#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import json
from typing import Dict, List


def use_rust_backend() -> bool:
    try:
        import rss_sim  # type: ignore
    except Exception:
        return False
    try:
        return bool(rss_sim.is_available())
    except Exception:
        return False


def _phase_to_dict(phase) -> Dict:
    return {
        "duration_s": float(phase.duration_s),
        "speed_ms": float(phase.speed_ms),
        "movement": int(phase.movement),
        "stance": int(phase.stance),
        "grade_pct": float(phase.grade_pct),
        "terrain": float(phase.terrain),
        "label": str(phase.label),
    }


def _mission_to_dict(mission) -> Dict:
    return {
        "name": str(mission.name),
        "load_kg": float(mission.load_kg),
        "description": str(getattr(mission, "description", "")),
        "temperature": float(getattr(mission, "temperature", 20.0)),
        "wind_speed": float(getattr(mission, "wind_speed", 0.0)),
        "phases": [_phase_to_dict(p) for p in mission.phases],
    }


def _result_dict_to_mission_result(data: Dict):
    from rss_pipeline_v4 import MissionResult

    return MissionResult(
        mission_name=str(data["mission_name"]),
        total_duration_s=float(data["total_duration_s"]),
        stamina_trace=[float(v) for v in data["stamina_trace"]],
        speed_trace=[float(v) for v in data["speed_trace"]],
        min_stamina=float(data["min_stamina"]),
        mean_stamina_active=float(data["mean_stamina_active"]),
        recovery_gain=float(data["recovery_gain"]),
        idle_duration_s=float(data["idle_duration_s"]),
        exhaustion_duration_s=float(data["exhaustion_duration_s"]),
        completion_possible=bool(data["completion_possible"]),
        observed_depletion_pct_per_s=float(data.get("observed_depletion_pct_per_s", 0.0)),
    )


def _run_python_fallback(params: Dict, fast_mode: bool, missions: List):
    from rss_digital_twin_fix import (
        RSSConstants,
        RSSDigitalTwin,
        RSS_PLAYER_TICK_SEC,
        merge_game_aligned_params,
    )
    from rss_pipeline_v4 import simulate_mission

    merged = merge_game_aligned_params(params)
    twin = RSSDigitalTwin(RSSConstants(**merged))
    twin._dt = 0.05 if fast_mode else RSS_PLAYER_TICK_SEC

    results = []
    for mission in missions:
        results.append(simulate_mission(twin, mission))
    return results


def run_mission_suite(params: Dict, fast_mode: bool, missions: List) -> List:
    if use_rust_backend():
        try:
            import rss_sim  # type: ignore

            missions_json = json.dumps([_mission_to_dict(m) for m in missions], ensure_ascii=False)
            rust_results = rss_sim.run_mission_suite(params, missions_json, bool(fast_mode))
            return [_result_dict_to_mission_result(r) for r in rust_results]
        except Exception:
            return _run_python_fallback(params, fast_mode, missions)

    return _run_python_fallback(params, fast_mode, missions)
