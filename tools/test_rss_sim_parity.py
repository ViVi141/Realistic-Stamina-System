#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import json
import math
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"


def _import_rss_sim():
    try:
        import rss_sim  # type: ignore

        return rss_sim
    except Exception:
        pass

    site_paths = sorted((TOOLS / "rss_sim" / ".venv" / "lib").glob("python*/site-packages"))
    for p in site_paths:
        sys.path.insert(0, str(p))

    import rss_sim  # type: ignore

    return rss_sim


def _assert_close(name: str, a: float, b: float, tol: float = 1e-6):
    if not math.isclose(a, b, rel_tol=tol, abs_tol=tol):
        raise AssertionError(f"{name}: rust={a} python={b} tol={tol}")


def _mission_dict(duration_s: float = 60.0):
    return {
        "name": "35kg稳态跑步",
        "load_kg": 35.0,
        "description": "parity mission",
        "temperature": 20.0,
        "wind_speed": 0.0,
        "phases": [
            {
                "duration_s": duration_s,
                "speed_ms": 0.0,
                "movement": 2,
                "stance": 0,
                "grade_pct": 0.0,
                "terrain": 1.0,
                "label": "稳态Run",
            }
        ],
    }


def _load_params():
    data = json.loads((TOOLS / "optimized_rss_config_elitestandard_v4.json").read_text(encoding="utf-8"))
    return {k: float(v) for k, v in data.items() if not str(k).startswith("_")}


def _simulate_mission_python(twin, mission: dict, fast_mode: bool) -> dict:
    from rss_digital_twin_fix import MovementType, RSS_PLAYER_TICK_SEC

    twin.reset()
    twin._scenario_wind_drag = 0.0
    twin.environment_factor.temperature = float(mission.get("temperature", 20.0))
    twin.environment_factor.wind_speed = float(mission.get("wind_speed", 0.0))
    if twin.environment_factor.temperature < 5.0:
        c = twin.constants
        twin.environment_factor.cold_stress = (5.0 - twin.environment_factor.temperature) * getattr(
            c, "ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF", 0.05
        )
        twin.environment_factor.cold_static_penalty = (5.0 - twin.environment_factor.temperature) * getattr(
            c, "ENV_TEMPERATURE_COLD_STATIC_PENALTY", 0.03
        )
    else:
        twin.environment_factor.cold_stress = 0.0
        twin.environment_factor.cold_static_penalty = 0.0

    if twin.environment_factor.wind_speed >= 1.0:
        wind_coeff = getattr(twin.constants, "ENV_WIND_RESISTANCE_COEFF", 0.05)
        twin._scenario_wind_drag = min(1.0, twin.environment_factor.wind_speed * wind_coeff)

    dt = 0.05 if fast_mode else RSS_PLAYER_TICK_SEC
    current_time = 0.0
    current_weight = 90.0 + float(mission["load_kg"])
    stamina_trace = []
    speed_trace = []
    total_recovery_gain = 0.0
    idle_duration_s = 0.0
    exhaustion_duration_s = 0.0

    for phase in mission["phases"]:
        steps = int(float(phase["duration_s"]) / dt)
        movement = int(phase["movement"])
        if movement == MovementType.IDLE:
            idle_duration_s += float(phase["duration_s"])
        for _ in range(steps):
            prev_stamina = twin.stamina
            drain_speed = twin.game_player_tick(
                movement,
                current_weight,
                float(phase.get("grade_pct", 0.0)),
                float(phase.get("terrain", 1.0)),
                int(phase["stance"]),
                current_time,
                dt,
                wind_drag=twin._scenario_wind_drag,
                enable_randomness=False,
            )
            current_time += dt
            stamina_trace.append(twin.stamina)
            speed_trace.append(drain_speed)
            if twin.stamina > prev_stamina:
                total_recovery_gain += twin.stamina - prev_stamina
            if twin.stamina < 0.15:
                exhaustion_duration_s += dt

    active_stamina = []
    t = 0.0
    for phase in mission["phases"]:
        n_steps = int(float(phase["duration_s"]) / dt)
        for j in range(n_steps):
            idx = int(t / dt) + j
            if idx < len(stamina_trace) and int(phase["movement"]) != MovementType.IDLE:
                active_stamina.append(stamina_trace[idx])
        t += float(phase["duration_s"])

    min_stamina = min(stamina_trace) if stamina_trace else 0.0
    mean_stamina_active = float(sum(active_stamina) / len(active_stamina)) if active_stamina else min_stamina
    depletion_samples = []
    for k in range(1, len(stamina_trace)):
        prev_s = stamina_trace[k - 1]
        cur_s = stamina_trace[k]
        if cur_s < prev_s - 1e-9 and prev_s > 0.82:
            depletion_samples.append((prev_s - cur_s) / dt * 100.0)
    observed = float(sum(depletion_samples) / len(depletion_samples)) if depletion_samples else 0.0

    return {
        "min_stamina": min_stamina,
        "mean_stamina_active": mean_stamina_active,
        "observed_depletion_pct_per_s": observed,
        "stamina_trace": stamina_trace,
        "speed_trace": speed_trace,
        "recovery_gain": total_recovery_gain,
        "idle_duration_s": idle_duration_s,
        "exhaustion_duration_s": exhaustion_duration_s,
    }


def main() -> int:
    rss_sim = _import_rss_sim()

    from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin, merge_game_aligned_params

    params = _load_params()
    merged = merge_game_aligned_params(params)

    rust_drain = float(rss_sim.get_drain_velocity_ms(5.5, 4.0))
    py_drain = 4.0
    _assert_close("get_drain_velocity_ms(5.5,4.0)", rust_drain, py_drain, 1e-12)
    print(f"[PASS] drain velocity parity rust={rust_drain:.6f} python={py_drain:.6f}")

    mission_dict = _mission_dict(60.0)
    py_twin = RSSDigitalTwin(RSSConstants(**merged))
    py_result = _simulate_mission_python(py_twin, mission_dict, False)

    rust_result = rss_sim.simulate_mission(
        params, json.dumps(mission_dict, ensure_ascii=False), False, False
    )
    _assert_close("simulate_mission.min_stamina", float(rust_result["min_stamina"]), float(py_result["min_stamina"]), 1e-5)
    _assert_close(
        "simulate_mission.mean_stamina_active",
        float(rust_result["mean_stamina_active"]),
        float(py_result["mean_stamina_active"]),
        1e-5,
    )
    _assert_close(
        "simulate_mission.observed_depletion_pct_per_s",
        float(rust_result["observed_depletion_pct_per_s"]),
        float(py_result["observed_depletion_pct_per_s"]),
        1e-5,
    )
    print(
        "[PASS] simulate_mission parity "
        f"min={rust_result['min_stamina']:.6f} obs={rust_result['observed_depletion_pct_per_s']:.6f}"
    )

    py_twin_fast = RSSDigitalTwin(RSSConstants(**merged))
    py_fast_result = _simulate_mission_python(py_twin_fast, mission_dict, True)

    rust_suite = rss_sim.run_mission_suite(
        params,
        json.dumps([mission_dict], ensure_ascii=False),
        True,
        False,
        False,
    )
    if len(rust_suite) != 1:
        raise AssertionError(f"run_mission_suite expected 1 result, got {len(rust_suite)}")
    rust_fast = rust_suite[0]

    _assert_close("run_mission_suite[0].min_stamina", float(rust_fast["min_stamina"]), float(py_fast_result["min_stamina"]), 1e-5)
    _assert_close(
        "run_mission_suite[0].observed_depletion_pct_per_s",
        float(rust_fast["observed_depletion_pct_per_s"]),
        float(py_fast_result["observed_depletion_pct_per_s"]),
        1e-5,
    )
    print(
        "[PASS] run_mission_suite fast parity "
        f"min={rust_fast['min_stamina']:.6f} obs={rust_fast['observed_depletion_pct_per_s']:.6f}"
    )

    print("[OK] rss_sim parity checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
