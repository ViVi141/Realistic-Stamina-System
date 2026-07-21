#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Pipeline v6 — 约束优先 + 分层目标
=====================================
相对 v4 的改进：
  1. 硬约束（生理锚点 / 契约测试）→ TrialPruned，不参与 Pareto 污染
  2. 软目标四维：sustain / combat / recovery / param_drift（均可 minimize）
  3. 搜索空间：v4 恢复维度 + v6 CP–W′ 维度（schema 对齐）
  4. validate 模式：门禁 + 三档 preset 报告（无需 Optuna）

前置（设计见 docs/RSS_v6_优化管线设计.md）：
  - 数字孪生待补齐 ProcessFatigueIntegral → GetMaxStaminaCap clamp（sustain 目标才与实机一致）
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np

try:
    import optuna
    HAS_OPTUNA = True
except ImportError:
    HAS_OPTUNA = False
    optuna = None

from rss_constraints_v6 import (
    MARCH_4H_AEROBIC_MIN,
    SUSTAIN_OBS_MAX_PCT_PER_S,
    SUSTAIN_OBS_MIN_PCT_PER_S,
    evaluate_hard_constraints,
)
from rss_digital_twin_fix import (
    RSSConstants,
    RSSDigitalTwin,
    RSS_PLAYER_TICK_SEC,
    Stance,
    MovementType,
    merge_game_aligned_params,
    simulate_ideal_march_aerobic_end,
)
from rss_pipeline_v4 import (
    Mission,
    MissionLibrary,
    Phase,
    compute_metrics,
    simulate_mission,
    MissionResult,
    HARDCORE_PARAM_REFS,
)
from rss_sim_backend import run_mission_suite as run_mission_suite_backend

TOOLS_DIR = Path(__file__).resolve().parent
PRESET_FILES = {
    "EliteStandard": "optimized_rss_config_elitestandard_v4.json",
    "StandardMilsim": "optimized_rss_config_standardmilsim_v4.json",
    "TacticalAction": "optimized_rss_config_tacticalaction_v4.json",
}

V6_DEFAULTS = {
    "critical_power_watts": 400.0,
    "w_prime_max_joules": 20000.0,
    "w_prime_recovery_w_per_s": 12.0,
    "sprint_power_cap_watts": 1450.0,
    "v5_walk_speed_ms": 1.4,
    "v5_run_speed_ms": 2.8,
    "v5_sprint_speed_ms": 4.0,
}

SUSTAIN_RUN_DURATION_S = 1200.0
SUSTAIN_LOAD_KG = 35.0
V6_PARAM_REFS = dict(V6_DEFAULTS)

# Elite Run @ cap 观测掉条目标（%/s，与实机 [RSS][Drain] / rss_constraints_v6 一致）
SUSTAIN_TARGET_DEPLETE_PCT_PER_S = 1.0
SUSTAIN_DEPLETE_BAND_PCT_PER_S = 0.25


@dataclass
class V6Metrics:
    """四软目标（均 minimize）"""
    sustain_ease: float
    combat_ease: float
    recovery_ease: float
    param_drift: float

    def as_tuple(self) -> Tuple[float, float, float, float]:
        return (
            self.sustain_ease,
            self.combat_ease,
            self.recovery_ease,
            self.param_drift,
        )


def mission_sustain_run_35kg(duration_s: float = SUSTAIN_RUN_DURATION_S) -> Mission:
    return Mission(
        name="35kg稳态跑步",
        load_kg=SUSTAIN_LOAD_KG,
        description="35kg 平地 Run 稳态（代谢限速 + 疲劳上限代理）",
        phases=[
            Phase(duration_s, 0.0, MovementType.RUN, Stance.STAND, 0.0, 1.0, "稳态Run"),
        ],
    )


def all_missions_v6(fast_mode: bool = False) -> List[Mission]:
    missions = MissionLibrary.all_missions()
    sustain_s = 180.0 if fast_mode else SUSTAIN_RUN_DURATION_S
    missions.append(mission_sustain_run_35kg(sustain_s))
    return missions


def _v6_param_drift(params: Dict) -> float:
    score = 0.0
    for key, ref in V6_PARAM_REFS.items():
        val = float(params.get(key, ref))
        if ref <= 0.0:
            continue
        rel = abs(val - ref) / ref
        score += rel * rel
    return float(math.sqrt(score))


def _sustain_band_penalty(observed_pct_per_s: float) -> float:
    if observed_pct_per_s <= 0.0001:
        return 2.0
    err = abs(observed_pct_per_s - SUSTAIN_TARGET_DEPLETE_PCT_PER_S)
    err -= SUSTAIN_DEPLETE_BAND_PCT_PER_S
    if err <= 0.0:
        return 0.0
    return float(err * err * 4.0)


def compute_v6_metrics(results: List[MissionResult], params: Dict) -> V6Metrics:
    v4 = compute_metrics(results, params)

    sustain_ease = 0.5
    for r in results:
        if r.mission_name == "35kg稳态跑步":
            band_pen = _sustain_band_penalty(r.observed_depletion_pct_per_s)
            reserve_pen = max(0.0, r.min_stamina - 0.05) * 0.5
            sustain_ease = band_pen + reserve_pen
            sustain_ease = max(0.0, min(2.0, sustain_ease))
            break

    drift = float(v4.parameter_realism) + _v6_param_drift(params) * 2.0

    return V6Metrics(
        sustain_ease=sustain_ease,
        combat_ease=float(v4.combat_ease),
        recovery_ease=float(v4.recovery_ease),
        param_drift=drift,
    )


def load_preset_params(name: str) -> Dict:
    path = TOOLS_DIR / PRESET_FILES[name]
    with path.open(encoding="utf-8") as f:
        data = json.load(f)
    params = {k: float(v) for k, v in data.items() if not k.startswith("_")}
    params.update(V6_DEFAULTS)
    return params


def run_mission_suite(params: Dict, fast_mode: bool = False) -> List[MissionResult]:
    missions = all_missions_v6(fast_mode)
    return run_mission_suite_backend(params, fast_mode, missions)


def _sustain_run_observed_pct(params: Dict, duration_s: float = 90.0) -> float:
    merged = merge_game_aligned_params(params)
    twin = RSSDigitalTwin(RSSConstants(**merged))
    twin._dt = RSS_PLAYER_TICK_SEC
    mission = Mission(
        name="35kg稳态跑步",
        load_kg=SUSTAIN_LOAD_KG,
        phases=[Phase(duration_s, 0.0, MovementType.RUN, 0, 0.0, 1.0, "稳态Run")],
    )
    return float(simulate_mission(twin, mission).observed_depletion_pct_per_s)


def calibrate_energy_coeff(
    base_params: Dict,
    march_hours: float = 4.0,
    march_target: float = MARCH_4H_AEROBIC_MIN,
    coeff_lo: float = 5.0e-8,
    coeff_hi: float = 9.5e-7,
    coarse_dt: float = 5.0,
    verify_dt: float = 2.0,
    max_iters: int = 24,
) -> Tuple[float, float, float]:
    """二分 energy_to_stamina_coeff：4h Walk aerobic_end ≥ target，Run obs 在 band 内。"""
    lo = coeff_lo
    hi = coeff_hi
    best_coeff = hi
    best_march = 0.0
    for _ in range(max_iters):
        mid = math.sqrt(lo * hi)
        trial = dict(base_params)
        trial["energy_to_stamina_coeff"] = mid
        march_end = simulate_ideal_march_aerobic_end(
            hours=march_hours,
            encumbrance_kg=SUSTAIN_LOAD_KG,
            dt_sec=coarse_dt,
            params=trial,
        )
        obs = _sustain_run_observed_pct(trial, duration_s=90.0)
        if obs < SUSTAIN_OBS_MIN_PCT_PER_S or obs > SUSTAIN_OBS_MAX_PCT_PER_S:
            hi = mid
            continue
        if march_end >= march_target:
            best_coeff = mid
            best_march = march_end
            lo = mid
        else:
            hi = mid
        if hi / lo < 1.02:
            break

    verify_params = dict(base_params)
    verify_params["energy_to_stamina_coeff"] = best_coeff
    verify_march = simulate_ideal_march_aerobic_end(
        hours=march_hours,
        encumbrance_kg=SUSTAIN_LOAD_KG,
        dt_sec=verify_dt,
        params=verify_params,
    )
    verify_obs = _sustain_run_observed_pct(verify_params, duration_s=90.0)
    return best_coeff, verify_march, verify_obs


def cmd_calibrate(args: argparse.Namespace) -> int:
    print("[V6] calibrate — Elite energy_to_stamina_coeff（4h 行军 + Run sustain obs）")
    base = load_preset_params(args.preset)
    t0 = time.time()
    coeff, march_end, obs = calibrate_energy_coeff(
        base,
        march_hours=args.hours,
        march_target=args.target,
        coarse_dt=args.coarse_dt,
        verify_dt=args.verify_dt,
    )
    elapsed = time.time() - t0
    print(f"  preset={args.preset}")
    print(f"  energy_to_stamina_coeff={coeff:.16e}")
    print(f"  march_{int(args.hours)}h_aerobic_end={march_end:.4f} (min {args.target})")
    print(f"  sustain_run_obs={obs:.3f} %/s (band {SUSTAIN_OBS_MIN_PCT_PER_S}–{SUSTAIN_OBS_MAX_PCT_PER_S})")
    print(f"  elapsed={elapsed:.1f}s")

    ok_march = march_end >= args.target
    ok_obs = SUSTAIN_OBS_MIN_PCT_PER_S <= obs <= SUSTAIN_OBS_MAX_PCT_PER_S
    if not ok_march or not ok_obs:
        print("[V6] calibrate: FAILED to meet dual constraints")
        return 1

    if args.write:
        json_path = TOOLS_DIR / PRESET_FILES[args.preset]
        with json_path.open(encoding="utf-8") as f:
            data = json.load(f)
        data["energy_to_stamina_coeff"] = coeff
        if "_calibration" not in data:
            data["_calibration"] = {}
        data["_calibration"]["v6_energy_coeff"] = {
            "march_hours": args.hours,
            "march_aerobic_end": march_end,
            "sustain_obs_pct_per_s": obs,
        }
        with json_path.open("w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        print(f"[V6] wrote {json_path}")

    print("[V6] calibrate: OK")
    return 0


def cmd_validate(args: argparse.Namespace) -> int:
    print("[V6] validate — 生理锚点 + 三档 preset 软指标")
    report = evaluate_hard_constraints()
    for line in report.summary_lines():
        print(line)
    if not report.all_hard_passed:
        print("[V6] validate: HARD constraints FAILED")
        return 1

    for preset_name in PRESET_FILES:
        params = load_preset_params(preset_name)
        results = run_mission_suite(params, fast_mode=args.fast)
        m = compute_v6_metrics(results, params)
        extra = ""
        for r in results:
            if r.mission_name == "35kg稳态跑步":
                extra = f" obs={r.observed_depletion_pct_per_s:.2f}%/s"
                break
        print(
            f"\n  [{preset_name}] "
            f"sustain={m.sustain_ease:.3f} combat={m.combat_ease:.3f} "
            f"recovery={m.recovery_ease:.6f} drift={m.param_drift:.3f}{extra}"
        )

    print("\n[V6] validate: all hard constraints passed")
    return 0


class RSSOptimizerV6:
    """v6 — 硬约束 prune + 四目标 NSGA-II"""

    SEARCH_SPACE_V4 = {
        "energy_to_stamina_coeff": (1.0e-7, 2.5e-7, True),
        "base_recovery_rate": (7e-5, 1.3e-4, True),
        "standing_recovery_multiplier": (0.70, 0.95, False),
        "crouching_recovery_multiplier": (1.3, 1.9, False),
        "prone_recovery_multiplier": (1.5, 2.2, False),
        "fast_recovery_multiplier": (1.3, 1.85, False),
        "medium_recovery_multiplier": (0.80, 1.15, False),
        "slow_recovery_multiplier": (0.28, 0.48, False),
        "encumbrance_speed_penalty_coeff": (0.18, 0.38, False),
        "encumbrance_stamina_drain_coeff": (2.2, 3.0, False),
        "load_recovery_penalty_coeff": (1e-4, 3.5e-4, True),
        "posture_crouch_multiplier": (2.5, 3.5, False),
        "posture_prone_multiplier": (3.0, 4.2, False),
        "sprint_speed_boost": (0.18, 0.26, False),
        "recovery_nonlinear_coeff": (0.28, 0.55, False),
        "max_recovery_per_tick": (2.0e-4, 4.2e-4, False),
        "willpower_threshold": (0.28, 0.40, False),
        "sprint_enable_threshold": (0.18, 0.30, False),
    }

    SEARCH_SPACE_V6 = {
        "critical_power_watts": (360.0, 440.0, False),
        "w_prime_max_joules": (16000.0, 24000.0, False),
        "w_prime_recovery_w_per_s": (8.0, 16.0, False),
        "sprint_power_cap_watts": (1300.0, 1550.0, False),
    }

    def __init__(self, n_trials: int = 400, n_jobs: int = -1, fast_mode: bool = False):
        self.n_trials = n_trials
        self.n_jobs = n_jobs if n_jobs > 0 else max(1, (os.cpu_count() or 2) - 1)
        self.fast_mode = fast_mode
        self.study = None

    def suggest_params(self, trial) -> Dict:
        params = dict(V6_DEFAULTS)
        for space in (self.SEARCH_SPACE_V4, self.SEARCH_SPACE_V6):
            for name, (low, high, log_scale) in space.items():
                if log_scale and low > 0 and high > 0:
                    params[name] = trial.suggest_float(name, low, high, log=True)
                else:
                    params[name] = trial.suggest_float(name, low, high)
        return params

    def objective(self, trial) -> Tuple[float, float, float, float]:
        params = self.suggest_params(trial)

        report = evaluate_hard_constraints(params)
        if not report.all_hard_passed:
            failed = ", ".join(c.name for c in report.failed_hard)
            trial.set_user_attr("pruned_reason", failed)
            raise optuna.TrialPruned()

        results = run_mission_suite(params, fast_mode=self.fast_mode)
        metrics = compute_v6_metrics(results, params)

        trial.set_user_attr("min_stamina", min(r.min_stamina for r in results))
        return metrics.as_tuple()

    def run(self, study_name: str = "rss_v6"):
        if not HAS_OPTUNA:
            raise RuntimeError("optuna not installed. Run: pip install optuna")

        study = optuna.create_study(
            study_name=study_name,
            directions=["minimize"] * 4,
            sampler=optuna.samplers.NSGAIISampler(population_size=48),
        )

        print(f"[V6] optimize: {self.n_trials} trials × {len(all_missions_v6(self.fast_mode))} missions")
        print("[V6] hard: physio anchors (prune on fail)")
        print("[V6] soft minimize: sustain_ease, combat_ease, recovery_ease, param_drift")
        t0 = time.time()

        study.optimize(
            self.objective,
            n_trials=self.n_trials,
            n_jobs=self.n_jobs,
            show_progress_bar=True,
        )

        elapsed = time.time() - t0
        self.study = study
        print(f"\n[V6] done ({elapsed:.0f}s), Pareto size={len(study.best_trials)}")

        for i, t in enumerate(study.best_trials[:3]):
            print(
                f"  #{i + 1}: sustain={t.values[0]:.3f} combat={t.values[1]:.3f} "
                f"recovery={t.values[2]:.6f} drift={t.values[3]:.3f}"
            )
        return study


def extract_presets_v6(study, output_dir: str = ".") -> Dict:
    if not study or not study.best_trials:
        print("[V6] no Pareto trials, skip preset export")
        return {}

    trials = study.best_trials
    values = np.array([t.values for t in trials])
    n = len(trials)

    elite_idx = int(np.lexsort((values[:, 2], values[:, 1], values[:, 0]))[0])

    used = {elite_idx}
    tac_candidates = np.argsort(-values[:, 0])
    tac_idx = elite_idx
    for c in tac_candidates:
        if int(c) not in used:
            tac_idx = int(c)
            break
    used.add(tac_idx)

    mid_pool = [i for i in range(n) if i not in used]
    if not mid_pool:
        mid_pool = [0]
    target = (values[elite_idx] + values[tac_idx]) * 0.5
    std_idx = int(min(mid_pool, key=lambda i: np.linalg.norm(values[i] - target)))

    picks = {
        "EliteStandard": elite_idx,
        "StandardMilsim": std_idx,
        "TacticalAction": tac_idx,
    }

    out: Dict = {}
    out_path = Path(output_dir)
    out_path.mkdir(parents=True, exist_ok=True)

    for preset_name, idx in picks.items():
        trial = trials[idx]
        params = dict(V6_DEFAULTS)
        params.update(trial.params)
        merged = merge_game_aligned_params(params)
        export = {k: merged[k] for k in HARDCORE_PARAM_REFS if k in merged}
        for k in V6_DEFAULTS:
            export[k] = params[k]
        export["_metrics_v6"] = {
            "sustain_ease": trial.values[0],
            "combat_ease": trial.values[1],
            "recovery_ease": trial.values[2],
            "param_drift": trial.values[3],
        }
        fname = f"optimized_rss_config_{preset_name.lower()}_v6.json"
        fpath = out_path / fname
        with fpath.open("w", encoding="utf-8") as f:
            json.dump(export, f, indent=2)
        out[preset_name] = str(fpath)
        print(f"[V6] wrote {fpath}")

    return out


def cmd_optimize(args: argparse.Namespace) -> int:
    opt = RSSOptimizerV6(
        n_trials=args.trials,
        n_jobs=args.jobs,
        fast_mode=args.fast,
    )
    study = opt.run()
    if args.output:
        extract_presets_v6(study, args.output)
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="RSS v6 optimization pipeline")
    sub = p.add_subparsers(dest="command", required=True)

    v = sub.add_parser("validate", help="Hard constraints + preset soft metrics")
    v.add_argument("--fast", action="store_true", help="Coarse dt for mission suite")
    v.set_defaults(func=cmd_validate)

    c = sub.add_parser(
        "calibrate",
        help="Binary-search energy_to_stamina_coeff (4h march + sustain obs band)",
    )
    c.add_argument("--preset", default="EliteStandard", choices=list(PRESET_FILES.keys()))
    c.add_argument("--hours", type=float, default=4.0)
    c.add_argument("--target", type=float, default=MARCH_4H_AEROBIC_MIN)
    c.add_argument("--coarse-dt", type=float, default=5.0, dest="coarse_dt")
    c.add_argument("--verify-dt", type=float, default=2.0, dest="verify_dt")
    c.add_argument("--write", action="store_true", help="Write coeff to preset JSON")
    c.set_defaults(func=cmd_calibrate)

    o = sub.add_parser("optimize", help="NSGA-II with hard-constraint pruning")
    o.add_argument("--trials", type=int, default=400)
    o.add_argument("--jobs", type=int, default=-1)
    o.add_argument("--output", type=str, default=str(TOOLS_DIR))
    o.add_argument("--fast", action="store_true")
    o.set_defaults(func=cmd_optimize)

    return p


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
