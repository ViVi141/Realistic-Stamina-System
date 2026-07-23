#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path
from typing import Dict, List, Optional

_RSS_SIM_MOD = None
_RSS_SIM_LOAD_ATTEMPTED = False


def _load_rss_sim_native():
    """Load compiled rss_sim extension, bypassing tools/rss_sim source namespace."""
    global _RSS_SIM_MOD, _RSS_SIM_LOAD_ATTEMPTED
    if _RSS_SIM_LOAD_ATTEMPTED:
        return _RSS_SIM_MOD
    _RSS_SIM_LOAD_ATTEMPTED = True

    existing = sys.modules.get("rss_sim")
    if existing is not None and hasattr(existing, "is_available"):
        _RSS_SIM_MOD = existing
        return _RSS_SIM_MOD

    search_dirs = []
    try:
        import site

        search_dirs.extend(site.getsitepackages())
        us = site.getusersitepackages()
        if us:
            search_dirs.append(us)
    except Exception:
        pass
    for p in sys.path:
        if p:
            search_dirs.append(p)

    candidates = []
    for d in search_dirs:
        try:
            root = Path(d)
        except Exception:
            continue
        if not root.is_dir():
            continue
        # Skip the Rust crate source tree (has Cargo.toml, no extension).
        if (root / "Cargo.toml").is_file() and root.name == "rss_sim":
            continue
        if root.name == "rss_sim" and (root.parent / "rss_pipeline_v6.py").is_file():
            continue
        for pat in ("rss_sim*.so", "rss_sim*.pyd", "rss_sim/*.so", "rss_sim/*.pyd"):
            candidates.extend(root.glob(pat))

    for path in candidates:
        if not path.is_file():
            continue
        # Ignore source-dir accidents
        if "tools" in path.parts and "rss_sim" in path.parts and path.suffix == "":
            continue
        try:
            spec = importlib.util.spec_from_file_location("rss_sim", str(path))
            if spec is None or spec.loader is None:
                continue
            mod = importlib.util.module_from_spec(spec)
            sys.modules["rss_sim"] = mod
            spec.loader.exec_module(mod)
            if hasattr(mod, "is_available"):
                _RSS_SIM_MOD = mod
                return _RSS_SIM_MOD
        except Exception:
            sys.modules.pop("rss_sim", None)
            continue

    # Last resort: normal import (works if no source shadow).
    try:
        if "rss_sim" in sys.modules and not hasattr(sys.modules["rss_sim"], "is_available"):
            del sys.modules["rss_sim"]
        import rss_sim  # type: ignore

        if hasattr(rss_sim, "is_available"):
            _RSS_SIM_MOD = rss_sim
            return _RSS_SIM_MOD
    except Exception:
        pass

    _RSS_SIM_MOD = None
    return None


def use_rust_backend() -> bool:
    mod = _load_rss_sim_native()
    if mod is None:
        return False
    try:
        return bool(mod.is_available())
    except Exception:
        return False


def get_rss_sim():
    return _load_rss_sim_native()


_RUST_BACKEND_LOGGED = False


def log_backend_once() -> None:
    global _RUST_BACKEND_LOGGED
    if _RUST_BACKEND_LOGGED:
        return
    _RUST_BACKEND_LOGGED = True
    if use_rust_backend():
        print("[V6] sim backend: Rust (rss_sim)")
    else:
        print(
            "[V6] sim backend: Python twin (SLOW). "
            "rss_sim native extension not loaded; build with maturin "
            "(AppLocker may block local Windows builds)."
        )


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

    stamina_trace = data.get("stamina_trace", [])
    speed_trace = data.get("speed_trace", [])
    return MissionResult(
        mission_name=str(data["mission_name"]),
        total_duration_s=float(data["total_duration_s"]),
        stamina_trace=[float(v) for v in stamina_trace],
        speed_trace=[float(v) for v in speed_trace],
        min_stamina=float(data["min_stamina"]),
        mean_stamina_active=float(data["mean_stamina_active"]),
        recovery_gain=float(data["recovery_gain"]),
        idle_duration_s=float(data["idle_duration_s"]),
        exhaustion_duration_s=float(data["exhaustion_duration_s"]),
        completion_possible=bool(data["completion_possible"]),
        observed_depletion_pct_per_s=float(data.get("observed_depletion_pct_per_s", 0.0)),
    )


def _dict_to_constraint_report(data: Dict):
    from rss_constraints_v6 import ConstraintCheck, ConstraintReport

    checks = []
    for row in data.get("checks", []):
        checks.append(
            ConstraintCheck(
                name=str(row["name"]),
                passed=bool(row["passed"]),
                detail=str(row["detail"]),
                hard=bool(row.get("hard", True)),
                margin=float(row.get("margin", 0.0)),
                hint=str(row.get("hint", "")),
            )
        )
    report = ConstraintReport(checks=checks)
    return report


class _ReportBatch(list):
    """ConstraintReport 列表；附带 backend 标记（rust/python）。"""

    backend: str = "unknown"


def batch_evaluate_hard_constraints(
    params_list: List[Dict],
    fast_mode: bool = False,
) -> List:
    """批量硬约束：Rust 并行优先，否则 Python 顺序。

    返回 `_ReportBatch`（list 子类），`.backend` = \"rust\" | \"python\" | \"none\"。
    """
    from rss_constraints_v6 import evaluate_hard_constraints_python

    if not params_list:
        out = _ReportBatch()
        out.backend = "none"
        return out

    rss_sim = get_rss_sim()
    if rss_sim is not None and use_rust_backend() and hasattr(
        rss_sim, "batch_evaluate_hard_constraints"
    ):
        try:
            clean = []
            for p in params_list:
                clean.append(
                    {k: float(v) for k, v in p.items() if not str(k).startswith("_")}
                )
            raw = rss_sim.batch_evaluate_hard_constraints(
                json.dumps(clean, ensure_ascii=False),
                bool(fast_mode),
            )
            reports = _ReportBatch(_dict_to_constraint_report(row) for row in raw)
            reports.backend = "rust"
            return reports
        except Exception:
            pass

    reports = _ReportBatch(evaluate_hard_constraints_python(p) for p in params_list)
    reports.backend = "python"
    return reports


def _params_json(params: Optional[Dict]) -> str:
    if not params:
        return ""
    clean = {k: float(v) for k, v in params.items() if not str(k).startswith("_")}
    return json.dumps(clean, ensure_ascii=False)


def evaluate_hard_constraints(params: Optional[Dict] = None):
    """硬约束门禁：Rust 优先，失败回退 Python。"""
    rss_sim = get_rss_sim()
    if rss_sim is not None and use_rust_backend():
        try:
            pj = _params_json(params)
            data = rss_sim.evaluate_hard_constraints(pj if pj else None)
            return _dict_to_constraint_report(data)
        except Exception:
            pass

    from rss_constraints_v6 import evaluate_hard_constraints_python

    return evaluate_hard_constraints_python(params)


def simulate_ideal_march_aerobic_end(
    hours: float = 4.0,
    encumbrance_kg: float = 35.0,
    dt_sec: float = 2.0,
    params: Optional[Dict] = None,
) -> float:
    rss_sim = get_rss_sim()
    if rss_sim is not None and use_rust_backend():
        try:
            return float(
                rss_sim.simulate_ideal_march_aerobic_end(
                    _params_json(params),
                    float(hours),
                    float(encumbrance_kg),
                    float(dt_sec),
                )
            )
        except Exception:
            pass

    from rss_digital_twin_fix import simulate_ideal_march_aerobic_end as py_march

    return float(
        py_march(
            hours=hours,
            encumbrance_kg=encumbrance_kg,
            dt_sec=dt_sec,
            params=params,
        )
    )


def sustain_run_observed_pct(params: Dict, duration_s: float = 90.0, fast_mode: bool = False) -> float:
    rss_sim = get_rss_sim()
    if rss_sim is not None and use_rust_backend():
        try:
            mission = {
                "name": "35kg稳态跑步",
                "load_kg": 35.0,
                "phases": [
                    {
                        "duration_s": float(duration_s),
                        "speed_ms": 0.0,
                        "movement": 2,
                        "stance": 0,
                        "grade_pct": 0.0,
                        "terrain": 1.0,
                        "label": "稳态Run",
                    }
                ],
            }
            result = rss_sim.simulate_mission(
                params,
                json.dumps(mission, ensure_ascii=False),
                bool(fast_mode),
                True,
            )
            return float(result["observed_depletion_pct_per_s"])
        except Exception:
            pass

    from rss_digital_twin_fix import (
        RSSConstants,
        RSSDigitalTwin,
        RSS_PLAYER_TICK_SEC,
        merge_game_aligned_params,
        MovementType,
    )
    from rss_pipeline_v4 import Phase, Mission, simulate_mission

    merged = merge_game_aligned_params(params)
    twin = RSSDigitalTwin(RSSConstants(**merged))
    twin._dt = 0.05 if fast_mode else RSS_PLAYER_TICK_SEC
    mission = Mission(
        name="35kg稳态跑步",
        load_kg=35.0,
        phases=[Phase(duration_s, 0.0, MovementType.RUN, 0, 0.0, 1.0, "稳态Run")],
    )
    return float(simulate_mission(twin, mission).observed_depletion_pct_per_s)


def _run_python_fallback(params: Dict, fast_mode: bool, missions: List, summary_only: bool):
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
    if summary_only:
        for r in results:
            r.stamina_trace = []
            r.speed_trace = []
    return results


def run_mission_suite(
    params: Dict,
    fast_mode: bool,
    missions: List,
    summary_only: bool = True,
    parallel: bool = True,
) -> List:
    rss_sim = get_rss_sim()
    if rss_sim is not None and use_rust_backend():
        try:
            missions_json = json.dumps([_mission_to_dict(m) for m in missions], ensure_ascii=False)
            rust_results = rss_sim.run_mission_suite(
                params,
                missions_json,
                bool(fast_mode),
                bool(summary_only),
                bool(parallel),
            )
            return [_result_dict_to_mission_result(r) for r in rust_results]
        except Exception:
            return _run_python_fallback(params, fast_mode, missions, summary_only)

    return _run_python_fallback(params, fast_mode, missions, summary_only)
