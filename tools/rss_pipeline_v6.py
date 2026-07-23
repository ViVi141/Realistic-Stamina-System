#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Pipeline v6 — 约束优先 + 分层目标
=====================================
相对 v4 的改进：
  1. 硬约束（生理锚点 / 契约测试）→ TrialPruned，不参与 Pareto 污染
  2. 软目标五维：sustain / mobility / combat / recovery / param_drift（均可 minimize）
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
    MOBILITY_RUN_0KG_MAX_MS,
    MOBILITY_RUN_0KG_MIN_MS,
    MOBILITY_RUN_35KG_MAX_MS,
    MOBILITY_RUN_35KG_MIN_MS,
    SUSTAIN_OBS_MAX_PCT_PER_S,
    SUSTAIN_OBS_MIN_PCT_PER_S,
)
from rss_digital_twin_fix import (
    Stance,
    MovementType,
    merge_game_aligned_params,
    theoretical_speed_at_weight,
    RSSConstants,
)
from rss_pipeline_v4 import (
    Mission,
    MissionLibrary,
    Phase,
    compute_metrics,
    MissionResult,
    HARDCORE_PARAM_REFS,
)
from rss_sim_backend import (
    evaluate_hard_constraints,
    batch_evaluate_hard_constraints,
    log_backend_once,
    run_mission_suite as run_mission_suite_backend,
    simulate_ideal_march_aerobic_end,
    sustain_run_observed_pct,
    use_rust_backend,
)

TOOLS_DIR = Path(__file__).resolve().parent
PRESET_FILES = {
    "EliteStandard": "optimized_rss_config_elitestandard_v4.json",
    "StandardMilsim": "optimized_rss_config_standardmilsim_v4.json",
    "TacticalAction": "optimized_rss_config_tacticalaction_v4.json",
}

V6_DEFAULTS = {
    "critical_power_watts": 1000.0,
    "w_prime_max_joules": 20000.0,
    "w_prime_recovery_w_per_s": 12.0,
    "sprint_power_cap_watts": 2400.0,
    "v5_walk_speed_ms": 1.4,
    "v5_run_speed_ms": 2.8,
    "v5_sprint_speed_ms": 4.5,
}

SUSTAIN_RUN_DURATION_S = 1200.0
SUSTAIN_LOAD_KG = 35.0
V6_PARAM_REFS = dict(V6_DEFAULTS)

# Elite Run @ cap 观测掉条目标（%/s；代谢 CP 对齐后）
SUSTAIN_TARGET_DEPLETE_PCT_PER_S = 1.8
SUSTAIN_DEPLETE_BAND_PCT_PER_S = 0.50

# mobility 软目标：band 内朝目标速度收敛（拟真档偏好更慢）
MOBILITY_RUN_0KG_TARGET_MS = 2.80
MOBILITY_RUN_35KG_TARGET_MS = 2.45

TIER_PHILOSOPHY = {
    "EliteStandard": "低 combat_ease + 低 recovery_ease → 最拟真/最硬核",
    "StandardMilsim": "战斗/恢复折中 → 拟真与可玩性平衡",
    "TacticalAction": "高 combat_ease + 高 recovery_ease → 战斗最宽容",
}

NUM_MO_OBJECTIVES = 5
DEFAULT_MO_POPULATION = 92
DEFAULT_MO_TRIALS = 1000
DEFAULT_TIER_TRIALS = 300
DEFAULT_FEASIBILITY_TRIALS = 150

# 三档标尺：拉开 combat/recovery，并锚定 W′ 回充与负重阶梯
# （combat 受 CP 双池影响时仍靠 param ladder 保证档位叙事）
TIER_TARGETS = {
    "EliteStandard": {
        "combat_ease": 0.58,
        "recovery_ease": 0.00090,
        "encumbrance_speed_penalty_coeff": 0.34,
        "w_prime_recovery_w_per_s": 10.0,
        "energy_to_stamina_coeff": 1.05e-7,
        "base_recovery_rate": 9.0e-5,
        "critical_power_watts": 980.0,
        "w_prime_max_joules": 28500.0,
    },
    "StandardMilsim": {
        "combat_ease": 0.69,
        "recovery_ease": 0.00125,
        "encumbrance_speed_penalty_coeff": 0.28,
        "w_prime_recovery_w_per_s": 13.0,
        "energy_to_stamina_coeff": 9.5e-8,
        "base_recovery_rate": 1.10e-4,
        "critical_power_watts": 1000.0,
        "w_prime_max_joules": 30000.0,
    },
    "TacticalAction": {
        "combat_ease": 0.82,
        "recovery_ease": 0.00200,
        "encumbrance_speed_penalty_coeff": 0.22,
        "w_prime_recovery_w_per_s": 15.5,
        "energy_to_stamina_coeff": 7.5e-8,
        "base_recovery_rate": 1.25e-4,
        "critical_power_watts": 1030.0,
        "w_prime_max_joules": 31500.0,
    },
}

# 兼容旧引用
STANDARD_TIER_IDEAL = dict(TIER_TARGETS["StandardMilsim"])

# Elite→Standard→Tactical 参数阶梯（ASC=越轻松越大，DESC=越轻松越小）
TIER_LADDER_ASC = (
    "base_recovery_rate",
    "standing_recovery_multiplier",
    "crouching_recovery_multiplier",
    "prone_recovery_multiplier",
    "fast_recovery_multiplier",
    "medium_recovery_multiplier",
    "max_recovery_per_tick",
    "w_prime_recovery_w_per_s",
    "w_prime_max_joules",
    "critical_power_watts",
)
TIER_LADDER_DESC = (
    "energy_to_stamina_coeff",
    "encumbrance_speed_penalty_coeff",
    "encumbrance_stamina_drain_coeff",
    "load_recovery_penalty_coeff",
)


@dataclass
class V6Metrics:
    """五软目标（均 minimize）"""
    sustain_ease: float
    mobility_ease: float
    combat_ease: float
    recovery_ease: float
    param_drift: float

    def as_tuple(self) -> Tuple[float, float, float, float, float]:
        return (
            self.sustain_ease,
            self.mobility_ease,
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


def _mobility_band_penalty(speed_ms: float, min_ms: float, max_ms: float) -> float:
    if speed_ms < min_ms:
        d = (min_ms - speed_ms) / max(min_ms, 0.1)
        return float(d * d * 4.0)
    if speed_ms > max_ms:
        d = (speed_ms - max_ms) / max(max_ms, 0.1)
        return float(d * d * 4.0)
    return 0.0


def _mobility_target_penalty(
    speed_ms: float,
    min_ms: float,
    max_ms: float,
    target_ms: float,
) -> float:
    """band 外硬罚 + band 内朝目标速度软罚（提供 Pareto 梯度）。"""
    band_pen = _mobility_band_penalty(speed_ms, min_ms, max_ms)
    if band_pen > 0.0:
        return band_pen
    half = max((max_ms - min_ms) * 0.5, 0.01)
    d = (speed_ms - target_ms) / half
    return float(d * d * 0.25)


def _mobility_ease(params: Dict) -> float:
    merged = merge_game_aligned_params(params)
    constants = RSSConstants(**merged)
    s0 = theoretical_speed_at_weight(constants, 90.0, MovementType.RUN)
    s35 = theoretical_speed_at_weight(constants, 125.0, MovementType.RUN)
    pen = _mobility_target_penalty(
        s0,
        MOBILITY_RUN_0KG_MIN_MS,
        MOBILITY_RUN_0KG_MAX_MS,
        MOBILITY_RUN_0KG_TARGET_MS,
    )
    pen += _mobility_target_penalty(
        s35,
        MOBILITY_RUN_35KG_MIN_MS,
        MOBILITY_RUN_35KG_MAX_MS,
        MOBILITY_RUN_35KG_TARGET_MS,
    )
    return float(min(2.0, pen))


def constraint_violation_score(params: Dict) -> float:
    """硬约束违反量（0 = 可行）。用于两阶段可行性搜索。"""
    report = evaluate_hard_constraints(params)
    if report.all_hard_passed:
        return 0.0
    if hasattr(report, "violation_score"):
        return float(report.violation_score)
    return float(len(report.failed_hard))


def _rel_abs(value: float, target: float) -> float:
    denom = max(abs(target), 1e-12)
    return abs(float(value) - float(target)) / denom


def scalarize_tier_metrics(
    metrics: V6Metrics,
    enc_coeff: float,
    tier: str,
    params: Dict = None,
    two_mile_time_s: float = None,
) -> float:
    """按档位理想点标量化（minimize）。含 param ladder 软拉扯，拉开三档叙事。"""
    if tier not in TIER_TARGETS:
        raise ValueError(f"unknown tier: {tier}")
    ideal = TIER_TARGETS[tier]
    p = params or {}

    score = (
        abs(metrics.combat_ease - ideal["combat_ease"]) * 10.0
        + abs(metrics.recovery_ease - ideal["recovery_ease"]) * 90000.0
        + abs(enc_coeff - ideal["encumbrance_speed_penalty_coeff"]) * 8.0
        + metrics.sustain_ease * 0.25
        + metrics.mobility_ease * 0.45
        + metrics.param_drift * 0.08
    )

    if two_mile_time_s is not None:
        from rss_constraints_v6 import two_mile_ease_from_time

        # 软目标：15:30 / 85 分；硬底线 18:00 / 70 分已由硬约束保证
        score += two_mile_ease_from_time(float(two_mile_time_s)) * 12.0

    # 参数阶梯锚点：即使 combat_ease 被 CP 双池压扁，档位仍可区分
    score += _rel_abs(
        float(p.get("w_prime_recovery_w_per_s", ideal["w_prime_recovery_w_per_s"])),
        ideal["w_prime_recovery_w_per_s"],
    ) * 3.5
    score += _rel_abs(
        float(p.get("energy_to_stamina_coeff", ideal["energy_to_stamina_coeff"])),
        ideal["energy_to_stamina_coeff"],
    ) * 4.0
    score += _rel_abs(
        float(p.get("base_recovery_rate", ideal["base_recovery_rate"])),
        ideal["base_recovery_rate"],
    ) * 3.0
    score += _rel_abs(
        float(p.get("critical_power_watts", ideal["critical_power_watts"])),
        ideal["critical_power_watts"],
    ) * 2.0
    score += _rel_abs(
        float(p.get("w_prime_max_joules", ideal["w_prime_max_joules"])),
        ideal["w_prime_max_joules"],
    ) * 1.5

    if tier == "EliteStandard":
        # 额外压低战斗余量 / 恢复宽松度
        score += metrics.combat_ease * 1.2
        score += metrics.recovery_ease * 40.0
    elif tier == "TacticalAction":
        score += (1.0 - metrics.combat_ease) * 1.2
        score += max(0.0, ideal["recovery_ease"] - metrics.recovery_ease) * 60.0

    return float(score)


def _trial_metrics_tuple(trial, params: Dict, fast_mode: bool) -> Tuple[float, float, float, float, float]:
    user = trial.user_attrs.get("metrics_v6")
    if user is not None and len(user) == NUM_MO_OBJECTIVES:
        return tuple(float(v) for v in user)
    if trial.values is not None and len(trial.values) == NUM_MO_OBJECTIVES:
        return tuple(float(v) for v in trial.values)
    results = run_mission_suite(params, fast_mode=fast_mode)
    return compute_v6_metrics(results, params).as_tuple()


def make_mo_sampler(sampler_name: str, population_size: int):
    if not HAS_OPTUNA:
        raise RuntimeError("optuna not installed")
    name = sampler_name.lower()
    if name in ("nsga3", "nsgaiii"):
        return optuna.samplers.NSGAIIISampler(population_size=population_size)
    if name in ("nsga2", "nsgaii", ""):
        return optuna.samplers.NSGAIISampler(population_size=population_size)
    raise ValueError(f"unknown sampler: {sampler_name} (use nsga2 or nsga3)")


def suggest_search_params(trial, defaults: Dict = None) -> Dict:
    params = dict(defaults or V6_DEFAULTS)
    for space in (RSSOptimizerV6.SEARCH_SPACE_V4, RSSOptimizerV6.SEARCH_SPACE_V6):
        for name, (low, high, log_scale) in space.items():
            if log_scale and low > 0 and high > 0:
                params[name] = trial.suggest_float(name, low, high, log=True)
            else:
                params[name] = trial.suggest_float(name, low, high)
    return params


@dataclass
class OptimizeStats:
    total: int = 0
    pruned: int = 0
    complete: int = 0
    prune_reasons: Dict[str, int] = None

    def __post_init__(self):
        if self.prune_reasons is None:
            self.prune_reasons = {}

    def record_prune(self, reason: str) -> None:
        self.pruned += 1
        key = reason or "unknown"
        self.prune_reasons[key] = self.prune_reasons.get(key, 0) + 1

    def summary_lines(self) -> List[str]:
        lines = [
            f"  trials: total={self.total} complete={self.complete} pruned={self.pruned}",
        ]
        if self.total > 0:
            rate = 100.0 * self.pruned / self.total
            lines.append(f"  prune_rate: {rate:.1f}%")
        if self.prune_reasons:
            top = sorted(self.prune_reasons.items(), key=lambda x: -x[1])[:5]
            lines.append("  top_prune_reasons: " + ", ".join(f"{k}={v}" for k, v in top))
        return lines


def _search_space_items() -> List[Tuple[str, float, float, bool]]:
    items: List[Tuple[str, float, float, bool]] = []
    for space in (RSSOptimizerV6.SEARCH_SPACE_V4, RSSOptimizerV6.SEARCH_SPACE_V6):
        for name, (low, high, log_scale) in space.items():
            items.append((name, float(low), float(high), bool(log_scale)))
    return items


def sample_lhs_params(n_samples: int, seed: int = 42) -> List[Dict]:
    """Latin Hypercube 采样（覆盖 SEARCH_SPACE_V4+V6）。"""
    rng = np.random.default_rng(seed)
    items = _search_space_items()
    n_dim = len(items)
    if n_samples <= 0 or n_dim <= 0:
        return []

    cut = (np.arange(n_samples) + rng.random(n_samples)) / float(n_samples)
    samples = np.zeros((n_samples, n_dim), dtype=float)
    for d in range(n_dim):
        perm = rng.permutation(n_samples)
        samples[:, d] = cut[perm]

    out: List[Dict] = []
    for i in range(n_samples):
        params = dict(V6_DEFAULTS)
        for d, (name, low, high, log_scale) in enumerate(items):
            u = float(samples[i, d])
            if log_scale and low > 0.0 and high > 0.0:
                params[name] = float(
                    math.exp(math.log(low) + u * (math.log(high) - math.log(low)))
                )
            else:
                params[name] = float(low + u * (high - low))
        out.append(params)
    return out


def search_feasible_seeds(
    n_trials: int,
    fast_mode: bool,
    target_count: int = 12,
) -> List[Dict]:
    """阶段 A：LHS 批量硬约束扫描（Rust 并行）+ 不足时 TPE 补种。"""
    print(f"[V6] feasibility phase: {n_trials} samples (target seeds={target_count})")
    log_backend_once()

    seeds: List[Dict] = []
    seen = set()

    def _add_seed(params: Dict) -> None:
        key = tuple(sorted((k, round(float(v), 8)) for k, v in params.items()))
        if key in seen:
            return
        seen.add(key)
        seeds.append(dict(params))

    lhs_n = max(0, int(n_trials))
    if lhs_n > 0:
        batch = sample_lhs_params(lhs_n, seed=42)
        try:
            from rss_anchors_v6 import compile_march_cp_anchors

            anchor = compile_march_cp_anchors()
            for p in batch:
                cp = float(p.get("critical_power_watts", 0.0))
                if cp < anchor.min_cp0:
                    lifted = float(anchor.min_cp0) + (cp - 750.0) * 0.25
                    if lifted > 1100.0:
                        lifted = 1100.0
                    p["critical_power_watts"] = lifted
        except Exception:
            pass

        t0 = time.time()
        reports = batch_evaluate_hard_constraints(batch, fast_mode=bool(fast_mode))
        elapsed = time.time() - t0
        feasible = 0
        for params, report in zip(batch, reports):
            if report.all_hard_passed:
                feasible += 1
                _add_seed(params)
                if len(seeds) >= target_count:
                    break
        backend = getattr(reports, "backend", getattr(reports, "_backend", "unknown"))
        print(
            f"[V6] LHS batch ({backend}): {feasible}/{len(batch)} feasible "
            f"in {elapsed:.1f}s → seeds={len(seeds)}"
        )

    remain = target_count - len(seeds)
    if remain > 0 and HAS_OPTUNA:
        tpe_trials = max(remain * 8, min(80, max(20, n_trials // 3)))
        print(f"[V6] feasibility TPE top-up: {tpe_trials} trials (need {remain} more)")
        study = optuna.create_study(
            study_name="rss_v6_feasibility_tpe",
            direction="minimize",
            sampler=optuna.samplers.TPESampler(seed=42),
        )

        def objective(trial) -> float:
            params = suggest_search_params(trial)
            return constraint_violation_score(params)

        study.optimize(objective, n_trials=tpe_trials, show_progress_bar=False)
        ranked = sorted(
            [t for t in study.trials if t.value is not None],
            key=lambda t: float(t.value),
        )
        for trial in ranked:
            if float(trial.value) > 0.0:
                break
            _add_seed(dict(trial.params))
            if len(seeds) >= target_count:
                break

    seeds = seeds[:target_count]
    print(f"[V6] feasibility phase: found {len(seeds)} feasible seeds")
    return seeds


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
    mobility = _mobility_ease(params)

    return V6Metrics(
        sustain_ease=sustain_ease,
        mobility_ease=mobility,
        combat_ease=float(v4.combat_ease),
        recovery_ease=float(v4.recovery_ease),
        param_drift=drift,
    )


def load_preset_params(name: str) -> Dict:
    slug = name.lower()
    v4_path = TOOLS_DIR / PRESET_FILES[name]
    v6_path = TOOLS_DIR / f"optimized_rss_config_{slug}_v6.json"
    params = dict(V6_DEFAULTS)
    if v4_path.is_file():
        with v4_path.open(encoding="utf-8") as f:
            data = json.load(f)
        params.update({k: float(v) for k, v in data.items() if not str(k).startswith("_")})
    if v6_path.is_file():
        with v6_path.open(encoding="utf-8") as f:
            data = json.load(f)
        params.update({k: float(v) for k, v in data.items() if not str(k).startswith("_")})
    return params


def run_mission_suite(params: Dict, fast_mode: bool = False) -> List[MissionResult]:
    missions = all_missions_v6(fast_mode)
    return run_mission_suite_backend(params, fast_mode, missions)


def _sustain_run_observed_pct(params: Dict, duration_s: float = 90.0) -> float:
    return sustain_run_observed_pct(params, duration_s=duration_s, fast_mode=False)


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
            f"sustain={m.sustain_ease:.3f} mobility={m.mobility_ease:.3f} "
            f"combat={m.combat_ease:.3f} recovery={m.recovery_ease:.6f} "
            f"drift={m.param_drift:.3f}{extra}"
        )

    print("\n[V6] validate: all hard constraints passed")
    return 0


class RSSOptimizerV6:
    """v6 — 硬约束 prune + 五目标 MOEA（NSGA-II / NSGA-III）"""

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
        # 零负重 Run 2mi&lt;20:00 约需 CP≳965 + W′≈30k（与冲刺≤15s 折中）
        "critical_power_watts": (750.0, 1100.0, False),
        "w_prime_max_joules": (18000.0, 33000.0, False),
        "w_prime_recovery_w_per_s": (8.0, 16.0, False),
        "sprint_power_cap_watts": (2200.0, 3000.0, False),
    }

    def __init__(
        self,
        n_trials: int = DEFAULT_MO_TRIALS,
        n_jobs: int = -1,
        fast_mode: bool = False,
        sampler: str = "nsga3",
        population_size: int = DEFAULT_MO_POPULATION,
        feasibility_trials: int = 0,
    ):
        self.n_trials = n_trials
        self.n_jobs = n_jobs if n_jobs > 0 else max(1, (os.cpu_count() or 2) - 1)
        self.fast_mode = fast_mode
        self.sampler = sampler
        self.population_size = population_size
        self.feasibility_trials = feasibility_trials
        self.study = None
        self.stats = OptimizeStats()

    def suggest_params(self, trial) -> Dict:
        return suggest_search_params(trial)

    def objective(self, trial) -> Tuple[float, float, float, float, float]:
        self.stats.total += 1
        params = self.suggest_params(trial)

        report = evaluate_hard_constraints(params)
        if not report.all_hard_passed:
            failed = report.prune_reason() if hasattr(report, "prune_reason") else ", ".join(
                c.name for c in report.failed_hard
            )
            trial.set_user_attr("pruned_reason", failed)
            hints = [c.hint for c in report.failed_hard if c.hint]
            if hints:
                trial.set_user_attr("prune_hints", hints[:3])
            self.stats.record_prune(failed)
            raise optuna.TrialPruned()

        results = run_mission_suite(params, fast_mode=self.fast_mode)
        metrics = compute_v6_metrics(results, params)
        trial.set_user_attr("metrics_v6", metrics.as_tuple())
        trial.set_user_attr("min_stamina", min(r.min_stamina for r in results))
        self.stats.complete += 1
        return metrics.as_tuple()

    def run(self, study_name: str = "rss_v6") -> "optuna.Study":
        if not HAS_OPTUNA:
            raise RuntimeError("optuna not installed. Run: pip install optuna")

        seeds: List[Dict] = []
        if self.feasibility_trials > 0:
            seeds = search_feasible_seeds(
                self.feasibility_trials,
                self.fast_mode,
                target_count=min(self.population_size, 24),
            )

        study = optuna.create_study(
            study_name=study_name,
            directions=["minimize"] * NUM_MO_OBJECTIVES,
            sampler=make_mo_sampler(self.sampler, self.population_size),
        )
        for seed in seeds:
            study.enqueue_trial(seed)

        print(
            f"[V6] optimize (MO): {self.n_trials} trials × "
            f"{len(all_missions_v6(self.fast_mode))} missions"
        )
        log_backend_once()
        if self.fast_mode:
            print("[V6] fast_mode=ON (shorter missions / coarse dt)")
        print(
            f"[V6] sampler={self.sampler} population={self.population_size} "
            f"feasibility_seeds={len(seeds)} n_jobs={self.n_jobs}"
        )
        print("[V6] hard: physio anchors + mobility (prune on fail)")
        print("[V6] soft minimize: sustain, mobility, combat, recovery, param_drift")
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
        for line in self.stats.summary_lines():
            print(line)

        for i, t in enumerate(study.best_trials[:3]):
            print(
                f"  #{i + 1}: sustain={t.values[0]:.3f} mobility={t.values[1]:.3f} "
                f"combat={t.values[2]:.3f} recovery={t.values[3]:.6f} drift={t.values[4]:.3f}"
            )
        return study


class TierOptimizerV6:
    """分档标量化单目标优化（TPE）— 推荐用于三档 preset 导出。"""

    def __init__(
        self,
        tier: str,
        n_trials: int = DEFAULT_TIER_TRIALS,
        n_jobs: int = 1,
        fast_mode: bool = False,
        feasibility_trials: int = 0,
    ):
        if tier not in TIER_PHILOSOPHY:
            raise ValueError(f"unknown tier: {tier}")
        self.tier = tier
        self.n_trials = n_trials
        self.n_jobs = n_jobs if n_jobs > 0 else 1
        self.fast_mode = fast_mode
        self.feasibility_trials = feasibility_trials
        self.study = None
        self.stats = OptimizeStats()

    def objective(self, trial) -> float:
        self.stats.total += 1
        params = suggest_search_params(trial)

        report = evaluate_hard_constraints(params)
        if not report.all_hard_passed:
            failed = report.prune_reason() if hasattr(report, "prune_reason") else ", ".join(
                c.name for c in report.failed_hard
            )
            trial.set_user_attr("pruned_reason", failed)
            hints = [c.hint for c in report.failed_hard if c.hint]
            if hints:
                trial.set_user_attr("prune_hints", hints[:3])
            self.stats.record_prune(failed)
            raise optuna.TrialPruned()

        two_mile_time_s = None
        from rss_constraints_v6 import TWO_MILE_MAX_SEC

        for c in report.checks:
            if c.name == "zero_load_2mile_pt_ge70":
                two_mile_time_s = float(TWO_MILE_MAX_SEC - c.margin)
                break

        results = run_mission_suite(params, fast_mode=self.fast_mode)
        metrics = compute_v6_metrics(results, params)
        enc = float(params.get("encumbrance_speed_penalty_coeff", 0.28))
        score = scalarize_tier_metrics(
            metrics, enc, self.tier, params, two_mile_time_s=two_mile_time_s
        )
        trial.set_user_attr("metrics_v6", metrics.as_tuple())
        trial.set_user_attr("enc_coeff", enc)
        if two_mile_time_s is not None:
            trial.set_user_attr("two_mile_time_s", two_mile_time_s)
        self.stats.complete += 1
        return score

    def run(self, study_name: str = "rss_v6_tier") -> "optuna.Study":
        if not HAS_OPTUNA:
            raise RuntimeError("optuna not installed. Run: pip install optuna")

        seeds: List[Dict] = []
        if self.feasibility_trials > 0:
            seeds = search_feasible_seeds(
                self.feasibility_trials,
                self.fast_mode,
                target_count=8,
            )

        study = optuna.create_study(
            study_name=f"{study_name}_{self.tier}",
            direction="minimize",
            sampler=optuna.samplers.TPESampler(seed=42),
        )
        for seed in seeds:
            study.enqueue_trial(seed)

        print(
            f"[V6] optimize-tier {self.tier}: {self.n_trials} trials "
            f"(TPE scalarized, seeds={len(seeds)})"
        )
        t0 = time.time()
        study.optimize(
            self.objective,
            n_trials=self.n_trials,
            n_jobs=self.n_jobs,
            show_progress_bar=True,
        )
        elapsed = time.time() - t0
        self.study = study
        best = study.best_trial
        m = best.user_attrs.get("metrics_v6", ())
        print(f"[V6] tier done ({elapsed:.0f}s) score={best.value:.4f}")
        if m:
            print(
                f"       combat={m[2]:.3f} recovery={m[3]:.6f} "
                f"mobility={m[1]:.4f} enc={best.user_attrs.get('enc_coeff', 0):.3f}"
            )
        for line in self.stats.summary_lines():
            print(line)
        return study


def _select_tier_indices(trials) -> Tuple[int, int, int]:
    """按拟真轴选三档：强制 recovery 单调，combat/enc 尽量拉开。"""
    n = len(trials)
    values = np.array([t.values for t in trials])
    combat = values[:, 2]
    recovery = values[:, 3]
    enc = np.array([
        float(t.params.get("encumbrance_speed_penalty_coeff", 0.28)) for t in trials
    ])
    w_rec = np.array([
        float(t.params.get("w_prime_recovery_w_per_s", 12.0)) for t in trials
    ])

    best = None
    best_score = None
    # 穷举三元组合在 Pareto 不大时可行；过大则抽样
    max_n = min(n, 48)
    order = list(range(n))
    if n > max_n:
        # 保留 combat 两端 + recovery 两端 + 随机中段
        picks = set()
        picks.update(np.argsort(combat)[:8].tolist())
        picks.update(np.argsort(-combat)[:8].tolist())
        picks.update(np.argsort(recovery)[:8].tolist())
        picks.update(np.argsort(-recovery)[:8].tolist())
        while len(picks) < max_n:
            picks.add(int(np.random.randint(0, n)))
        order = sorted(picks)

    for i in order:
        for j in order:
            if j == i:
                continue
            for k in order:
                if k == i or k == j:
                    continue
                # 硬条件：recovery 与 W′ 回充 Elite≤Std≤Tac
                if not (recovery[i] <= recovery[j] <= recovery[k]):
                    continue
                if not (w_rec[i] <= w_rec[j] <= w_rec[k]):
                    continue
                # 软条件：combat 单调 + enc 递减
                combat_gap = (combat[k] - combat[i]) - abs(combat[j] - (combat[i] + combat[k]) * 0.5)
                enc_term = (enc[i] - enc[j]) + (enc[j] - enc[k])
                spread = (combat[k] - combat[i]) + (recovery[k] - recovery[i]) * 80.0
                score = (
                    -spread * 2.0
                    - combat_gap
                    - enc_term * 3.0
                    + abs(combat[j] - (combat[i] + combat[k]) * 0.5) * 2.0
                    + abs(recovery[j] - (recovery[i] + recovery[k]) * 0.5) * 120.0
                )
                if best_score is None or score < best_score:
                    best_score = score
                    best = (i, j, k)

    if best is not None:
        return int(best[0]), int(best[1]), int(best[2])

    # 回退：原启发式（可能产生 WARN）
    elite_pool_size = max(3, n // 5)
    elite_pool = np.argsort(combat)[:elite_pool_size]
    elite_idx = int(min(elite_pool, key=lambda i: (combat[i], recovery[i], -enc[i])))

    tac_pool_size = max(3, n // 5)
    tac_sorted = np.argsort(-combat)
    tac_pool = []
    for c in tac_sorted:
        ci = int(c)
        if ci != elite_idx:
            tac_pool.append(ci)
        if len(tac_pool) >= tac_pool_size:
            break
    if not tac_pool:
        tac_pool = [i for i in range(n) if i != elite_idx]
    tac_idx = int(max(tac_pool, key=lambda i: (recovery[i], combat[i], -enc[i])))

    remaining = [i for i in range(n) if i not in (elite_idx, tac_idx)]
    if not remaining:
        remaining = [0]
    target_c = (combat[elite_idx] + combat[tac_idx]) * 0.5
    target_r = (recovery[elite_idx] + recovery[tac_idx]) * 0.5
    target_e = (enc[elite_idx] + enc[tac_idx]) * 0.5
    std_idx = int(
        min(
            remaining,
            key=lambda i: (
                abs(combat[i] - target_c)
                + abs(recovery[i] - target_r) * 80.0
                + abs(enc[i] - target_e) * 4.0
            ),
        )
    )
    return elite_idx, std_idx, tac_idx


def _numeric_preset_params(raw: Dict) -> Dict[str, float]:
    return {
        k: float(v)
        for k, v in raw.items()
        if not str(k).startswith("_") and isinstance(v, (int, float))
    }


def _apply_tier_param_ladders(presets: Dict[str, Dict]) -> Dict[str, Dict]:
    """对三档参数做 ASC/DESC 重排，保证 Elite→Standard→Tactical 阶梯。"""
    names = ("EliteStandard", "StandardMilsim", "TacticalAction")
    for name in names:
        if name not in presets:
            raise ValueError(f"missing preset for ladder repair: {name}")

    fixed = {n: dict(presets[n]) for n in names}

    def _reassign(key: str, ascending: bool) -> None:
        vals = [float(fixed[n][key]) for n in names if key in fixed[n]]
        if len(vals) != 3:
            return
        ordered = sorted(vals) if ascending else sorted(vals, reverse=True)
        for name, value in zip(names, ordered):
            fixed[name][key] = float(value)

    for key in TIER_LADDER_ASC:
        _reassign(key, ascending=True)
    for key in TIER_LADDER_DESC:
        _reassign(key, ascending=False)

    # 姿态恢复：prone ≥ crouch ≥ stand（各档内）
    for name in names:
        p = fixed[name]
        stand = float(p.get("standing_recovery_multiplier", 0.8))
        crouch = float(p.get("crouching_recovery_multiplier", 1.4))
        prone = float(p.get("prone_recovery_multiplier", 1.6))
        ordered = sorted([stand, crouch, prone])
        p["standing_recovery_multiplier"] = ordered[0]
        p["crouching_recovery_multiplier"] = ordered[1]
        p["prone_recovery_multiplier"] = ordered[2]

    return fixed


def _refresh_preset_metrics(params: Dict, fast_mode: bool = False) -> Dict:
    """写入可导出的 preset dict（含 _metrics_v6）。"""
    merged = merge_game_aligned_params(params)
    results = run_mission_suite(params, fast_mode=fast_mode)
    metrics = compute_v6_metrics(results, params)
    export = {k: merged[k] for k in HARDCORE_PARAM_REFS if k in merged}
    for k in V6_DEFAULTS:
        if k in params:
            export[k] = float(params[k])
        elif k in merged:
            export[k] = float(merged[k])
    for k, v in params.items():
        if str(k).startswith("_"):
            continue
        if isinstance(v, (int, float)):
            export[k] = float(v)
    export["_metrics_v6"] = {
        "sustain_ease": metrics.sustain_ease,
        "mobility_ease": metrics.mobility_ease,
        "combat_ease": metrics.combat_ease,
        "recovery_ease": metrics.recovery_ease,
        "param_drift": metrics.param_drift,
    }
    return export


def _write_preset_export(preset_name: str, export: Dict, out_path: Path) -> None:
    export = dict(export)
    export["_philosophy"] = TIER_PHILOSOPHY[preset_name]
    slug = preset_name.lower()
    v6_path = out_path / f"optimized_rss_config_{slug}_v6.json"
    with v6_path.open("w", encoding="utf-8") as f:
        json.dump(export, f, indent=2, ensure_ascii=False)

    v4_export = {k: export[k] for k in HARDCORE_PARAM_REFS if k in export}
    metrics = export.get("_metrics_v6", {})
    v4_export["_metrics"] = {
        "combat_ease": float(metrics.get("combat_ease", 0.0)),
        "recovery_ease": float(metrics.get("recovery_ease", 0.0)),
        "parameter_realism": float(metrics.get("param_drift", 0.0)),
    }
    v4_export["_philosophy"] = TIER_PHILOSOPHY[preset_name]
    v4_path = out_path / PRESET_FILES[preset_name]
    with v4_path.open("w", encoding="utf-8") as f:
        json.dump(v4_export, f, indent=2, ensure_ascii=False)

    m = export.get("_metrics_v6", {})
    print(
        f"[V6] {preset_name}: combat={m.get('combat_ease', 0):.3f} "
        f"recovery={m.get('recovery_ease', 0):.6f} "
        f"enc={export.get('encumbrance_speed_penalty_coeff', 0):.3f} "
        f"w_rec={export.get('w_prime_recovery_w_per_s', 0):.2f}"
    )
    print(f"       → {v6_path.name} + {v4_path.name}")


def _nudge_combat_monotonicity(
    working: Dict[str, Dict],
    max_iters: int = 12,
) -> Dict[str, Dict]:
    """微调 energy_to_stamina，使 combat Elite≤Standard≤Tactical。"""
    names = ("EliteStandard", "StandardMilsim", "TacticalAction")

    def _combat_of(params: Dict) -> float:
        results = run_mission_suite(params, fast_mode=False)
        return float(compute_v6_metrics(results, params).combat_ease)

    for _ in range(max_iters):
        combats = [_combat_of(working[n]) for n in names]
        if combats[0] <= combats[1] <= combats[2]:
            return working

        trial = {n: dict(working[n]) for n in names}
        # Elite 战斗更硬 → 提高消耗；Tactical 更松 → 降低消耗；Standard 取中
        if combats[0] > combats[1]:
            trial["EliteStandard"]["energy_to_stamina_coeff"] = float(
                trial["EliteStandard"]["energy_to_stamina_coeff"]
            ) * 1.03
            trial["StandardMilsim"]["energy_to_stamina_coeff"] = float(
                trial["StandardMilsim"]["energy_to_stamina_coeff"]
            ) * 0.98
        if combats[1] > combats[2]:
            trial["StandardMilsim"]["energy_to_stamina_coeff"] = float(
                trial["StandardMilsim"]["energy_to_stamina_coeff"]
            ) * 1.02
            trial["TacticalAction"]["energy_to_stamina_coeff"] = float(
                trial["TacticalAction"]["energy_to_stamina_coeff"]
            ) * 0.97

        # 夹在搜索空间内
        for n in names:
            e2s = float(trial[n]["energy_to_stamina_coeff"])
            trial[n]["energy_to_stamina_coeff"] = max(1.0e-7, min(2.5e-7, e2s))

        if not all(
            evaluate_hard_constraints(trial[n]).all_hard_passed for n in names
        ):
            break
        working = trial

    return working


def repair_tier_presets(out_path: Path = None, fast_mode: bool = False) -> Dict:
    """修复三档参数阶梯并重算指标（解决 recovery 倒置 / W′ 回充倒置）。"""
    out_path = Path(out_path or TOOLS_DIR)
    raw = {}
    for name in TIER_PHILOSOPHY:
        raw[name] = _numeric_preset_params(load_preset_params(name))

    print("[V6] repair-tiers — apply ASC/DESC param ladders")
    laddered = _apply_tier_param_ladders(raw)

    # 先整体应用；若任一项硬约束失败，再按 key 逐个回退
    working = {n: dict(laddered[n]) for n in TIER_PHILOSOPHY}
    for name in TIER_PHILOSOPHY:
        report = evaluate_hard_constraints(working[name])
        if report.all_hard_passed:
            continue
        failed = ", ".join(c.name for c in report.failed_hard)
        print(f"[V6] repair {name}: hard fail after full ladder ({failed}) → per-key")
        working[name] = dict(raw[name])
        for key in list(TIER_LADDER_ASC) + list(TIER_LADDER_DESC):
            if key not in laddered[name]:
                continue
            trio = {n: dict(working[n]) for n in TIER_PHILOSOPHY}
            for n in TIER_PHILOSOPHY:
                if key in laddered[n]:
                    trio[n][key] = float(laddered[n][key])
            if all(
                evaluate_hard_constraints(trio[n]).all_hard_passed
                for n in TIER_PHILOSOPHY
            ):
                working = trio

    print("[V6] repair-tiers — nudge combat monotonicity")
    working = _nudge_combat_monotonicity(working)

    repaired = {}
    for name in TIER_PHILOSOPHY:
        export = _refresh_preset_metrics(working[name], fast_mode=fast_mode)
        export["_philosophy"] = TIER_PHILOSOPHY[name]
        repaired[name] = export
        _write_preset_export(name, export, out_path)

    _print_v6_tier_order_check(repaired)
    return repaired


def _print_v6_tier_order_check(presets: Dict) -> bool:
    if not all(k in presets for k in TIER_PHILOSOPHY):
        return False
    elite = presets["EliteStandard"]["_metrics_v6"]
    std = presets["StandardMilsim"]["_metrics_v6"]
    tac = presets["TacticalAction"]["_metrics_v6"]
    ok_combat = elite["combat_ease"] <= std["combat_ease"] + 0.005
    ok_combat = ok_combat and (std["combat_ease"] <= tac["combat_ease"] + 0.005)
    # 严格单调仍打印；近贴（CP 双池导致 combat 扁平）用 0.005 容差判 OK
    strict_combat = (
        elite["combat_ease"] <= std["combat_ease"] <= tac["combat_ease"]
    )
    ok_recovery = elite["recovery_ease"] <= std["recovery_ease"] <= tac["recovery_ease"]
    ok_enc = (
        presets["EliteStandard"].get("encumbrance_speed_penalty_coeff", 0.0)
        >= presets["StandardMilsim"].get("encumbrance_speed_penalty_coeff", 0.0)
        >= presets["TacticalAction"].get("encumbrance_speed_penalty_coeff", 0.0)
    )
    ok_wrec = (
        float(presets["EliteStandard"].get("w_prime_recovery_w_per_s", 0.0))
        <= float(presets["StandardMilsim"].get("w_prime_recovery_w_per_s", 0.0))
        <= float(presets["TacticalAction"].get("w_prime_recovery_w_per_s", 0.0))
    )
    enc_tag = "enc↓" if ok_enc else "enc?"
    wrec_tag = "w_rec↑" if ok_wrec else "w_rec?"
    combat_tag = "combat↑" if strict_combat else ("combat~" if ok_combat else "combat?")
    status = "OK" if (ok_combat and ok_recovery and ok_enc and ok_wrec) else "WARN"
    print(
        f"[V6] 档位排序 [{status}] ({enc_tag}, {wrec_tag}, {combat_tag}): "
        f"combat {elite['combat_ease']:.3f} ≤ {std['combat_ease']:.3f} ≤ {tac['combat_ease']:.3f}; "
        f"recovery {elite['recovery_ease']:.6f} ≤ {std['recovery_ease']:.6f} ≤ {tac['recovery_ease']:.6f}; "
        f"enc {presets['EliteStandard'].get('encumbrance_speed_penalty_coeff', 0):.3f} / "
        f"{presets['StandardMilsim'].get('encumbrance_speed_penalty_coeff', 0):.3f} / "
        f"{presets['TacticalAction'].get('encumbrance_speed_penalty_coeff', 0):.3f}; "
        f"w_rec {float(presets['EliteStandard'].get('w_prime_recovery_w_per_s', 0)):.2f} / "
        f"{float(presets['StandardMilsim'].get('w_prime_recovery_w_per_s', 0)):.2f} / "
        f"{float(presets['TacticalAction'].get('w_prime_recovery_w_per_s', 0)):.2f}"
    )
    return status == "OK"


def _write_preset_pair(
    preset_name: str,
    trial,
    out_path: Path,
    fast_mode: bool = False,
) -> Dict:
    params = dict(V6_DEFAULTS)
    params.update(trial.params)
    merged = merge_game_aligned_params(params)
    metrics = _trial_metrics_tuple(trial, params, fast_mode)

    v6_export = {k: merged[k] for k in HARDCORE_PARAM_REFS if k in merged}
    for k in V6_DEFAULTS:
        v6_export[k] = params[k]
    v6_export["_metrics_v6"] = {
        "sustain_ease": metrics[0],
        "mobility_ease": metrics[1],
        "combat_ease": metrics[2],
        "recovery_ease": metrics[3],
        "param_drift": metrics[4],
    }
    v6_export["_philosophy"] = TIER_PHILOSOPHY[preset_name]

    slug = preset_name.lower()
    v6_path = out_path / f"optimized_rss_config_{slug}_v6.json"
    with v6_path.open("w", encoding="utf-8") as f:
        json.dump(v6_export, f, indent=2, ensure_ascii=False)

    v4_export = {k: v6_export[k] for k in HARDCORE_PARAM_REFS if k in v6_export}
    v4_export["_metrics"] = {
        "combat_ease": metrics[2],
        "recovery_ease": metrics[3],
        "parameter_realism": metrics[4],
    }
    v4_export["_philosophy"] = TIER_PHILOSOPHY[preset_name]
    v4_path = out_path / PRESET_FILES[preset_name]
    with v4_path.open("w", encoding="utf-8") as f:
        json.dump(v4_export, f, indent=2, ensure_ascii=False)

    print(
        f"[V6] {preset_name}: combat={metrics[2]:.3f} recovery={metrics[3]:.6f} "
        f"mobility={metrics[1]:.4f} enc={params.get('encumbrance_speed_penalty_coeff', 0):.3f}"
    )
    print(f"       → {v6_path.name} + {v4_path.name}")
    return v6_export


def extract_presets_v6(study, output_dir: str = ".") -> Dict:
    if not study or not study.best_trials:
        print("[V6] no Pareto trials, skip preset export")
        return {}

    trials = study.best_trials
    values = np.array([t.values for t in trials])
    n = len(trials)

    elite_idx, std_idx, tac_idx = _select_tier_indices(trials)
    unique = len({elite_idx, std_idx, tac_idx})
    if unique < 3:
        print(f"[V6] WARN: tier diversity {unique}/3")
        print(
            f"       combat [{values[:,2].min():.3f}, {values[:,2].max():.3f}] "
            f"recovery [{values[:,3].min():.6f}, {values[:,3].max():.6f}]"
        )

    picks = {
        "EliteStandard": elite_idx,
        "StandardMilsim": std_idx,
        "TacticalAction": tac_idx,
    }

    out: Dict = {}
    out_path = Path(output_dir)
    out_path.mkdir(parents=True, exist_ok=True)

    preset_exports: Dict = {}
    for preset_name, idx in picks.items():
        export = _write_preset_pair(preset_name, trials[idx], out_path, fast_mode=False)
        preset_exports[preset_name] = export
        out[preset_name] = str(out_path / f"optimized_rss_config_{preset_name.lower()}_v6.json")

    _print_v6_tier_order_check(preset_exports)
    return out


def _post_export_hard_check() -> int:
    print("\n[V6] post-export hard constraint check:")
    for preset_name in TIER_PHILOSOPHY:
        params = load_preset_params(preset_name)
        report = evaluate_hard_constraints(params)
        status = "PASS" if report.all_hard_passed else "FAIL"
        print(f"  [{preset_name}] {status}")
        if not report.all_hard_passed:
            for c in report.failed_hard:
                print(f"    - {c.name}: {c.detail}")
            return 1
    return 0


def _maybe_embed_c(embed: bool) -> int:
    if not embed:
        return 0
    import subprocess
    import sys

    embed_script = TOOLS_DIR / "embed_json_to_c.py"
    if not embed_script.exists():
        print("[V6] embed_json_to_c.py not found, skip --embed-c")
        return 0
    print("[V6] embedding presets into SCR_RSS_SettingsPresetBake.c ...")
    return int(subprocess.call([sys.executable, str(embed_script)]))


def _resolve_feasibility_trials(args: argparse.Namespace) -> int:
    # 默认开启两阶段：LHS 批量可行域 → MOEA
    if getattr(args, "feasibility_trials", None) is not None and args.feasibility_trials >= 0:
        return int(args.feasibility_trials)
    if getattr(args, "no_two_phase", False):
        return 0
    return DEFAULT_FEASIBILITY_TRIALS


def cmd_anchors(_args: argparse.Namespace) -> int:
    from rss_anchors_v6 import anchors_summary_lines, compile_march_cp_anchors

    print("[V6] anchors — LCDA Walk → CP 编译")
    log_backend_once()
    anchor = compile_march_cp_anchors()
    for line in anchors_summary_lines(anchor):
        print(f"  {line}")
    return 0


def cmd_optimize(args: argparse.Namespace) -> int:
    feas = _resolve_feasibility_trials(args)
    opt = RSSOptimizerV6(
        n_trials=args.trials,
        n_jobs=args.jobs,
        fast_mode=args.fast,
        sampler=args.sampler,
        population_size=args.population,
        feasibility_trials=feas,
    )
    study = opt.run()
    if args.output:
        extract_presets_v6(study, args.output)
        rc = _post_export_hard_check()
        if rc != 0:
            return rc
        rc = _maybe_embed_c(args.embed_c)
        if rc != 0:
            return rc
    return 0


def cmd_optimize_tiers(args: argparse.Namespace) -> int:
    feas = _resolve_feasibility_trials(args)
    out_path = Path(args.output)
    out_path.mkdir(parents=True, exist_ok=True)
    preset_exports: Dict = {}

    tiers = list(TIER_PHILOSOPHY.keys())
    if args.tier:
        if args.tier not in TIER_PHILOSOPHY:
            print(f"[V6] unknown tier: {args.tier}")
            return 1
        tiers = [args.tier]

    print("[V6] optimize-tiers — 分档标量化 TPE（推荐 preset 工作流）")
    for tier in tiers:
        opt = TierOptimizerV6(
            tier=tier,
            n_trials=args.trials,
            n_jobs=args.jobs,
            fast_mode=args.fast,
            feasibility_trials=feas,
        )
        study = opt.run()
        export = _write_preset_pair(
            tier, study.best_trial, out_path, fast_mode=args.fast
        )
        preset_exports[tier] = export

    if len(preset_exports) == len(TIER_PHILOSOPHY):
        ok = _print_v6_tier_order_check(preset_exports)
        if not ok:
            print("[V6] tier order WARN → running repair-tiers")
            preset_exports = repair_tier_presets(out_path, fast_mode=args.fast)

    rc = _post_export_hard_check()
    if rc != 0:
        return rc
    rc = _maybe_embed_c(args.embed_c)
    if rc != 0:
        return rc
    return 0


def cmd_repair_tiers(args: argparse.Namespace) -> int:
    out_path = Path(args.output)
    out_path.mkdir(parents=True, exist_ok=True)
    repaired = repair_tier_presets(out_path, fast_mode=args.fast)
    ok = all(
        k in repaired and "_metrics_v6" in repaired[k] for k in TIER_PHILOSOPHY
    )
    order_ok = False
    if ok:
        order_ok = _print_v6_tier_order_check(repaired)
    rc = _post_export_hard_check()
    if rc != 0:
        return rc
    rc = _maybe_embed_c(args.embed_c)
    if rc != 0:
        return rc
    if not order_ok:
        print("[V6] repair-tiers: tier order still WARN after ladder")
        return 2
    print("[V6] repair-tiers: OK")
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

    o = sub.add_parser("optimize", help="MOEA (NSGA-II/III) + Pareto tier extraction")
    o.add_argument("--trials", type=int, default=DEFAULT_MO_TRIALS)
    o.add_argument("--jobs", type=int, default=-1)
    o.add_argument("--output", type=str, default=str(TOOLS_DIR))
    o.add_argument("--fast", action="store_true")
    o.add_argument(
        "--sampler",
        choices=["nsga2", "nsga3"],
        default="nsga3",
        help="Multi-objective sampler (default: nsga3)",
    )
    o.add_argument(
        "--population",
        type=int,
        default=DEFAULT_MO_POPULATION,
        help="MOEA population size",
    )
    o.add_argument(
        "--feasibility-trials",
        type=int,
        default=-1,
        dest="feasibility_trials",
        help="Phase-A LHS samples (-1=auto 150; 0 disables with --no-two-phase)",
    )
    o.add_argument(
        "--two-phase",
        action="store_true",
        default=True,
        help="LHS feasible seeds before MOEA (default ON)",
    )
    o.add_argument(
        "--no-two-phase",
        action="store_true",
        help="Skip feasibility phase",
    )
    o.add_argument(
        "--embed-c",
        action="store_true",
        help="After export, merge v6 JSON into SCR_RSS_Settings.c",
    )
    o.set_defaults(func=cmd_optimize)

    t = sub.add_parser(
        "optimize-tiers",
        help="Per-tier scalarized TPE (recommended for preset export)",
    )
    t.add_argument("--trials", type=int, default=DEFAULT_TIER_TRIALS)
    t.add_argument("--jobs", type=int, default=1)
    t.add_argument("--output", type=str, default=str(TOOLS_DIR))
    t.add_argument("--fast", action="store_true")
    t.add_argument(
        "--tier",
        choices=list(TIER_PHILOSOPHY.keys()),
        default=None,
        help="Optimize a single tier only",
    )
    t.add_argument(
        "--feasibility-trials",
        type=int,
        default=-1,
        dest="feasibility_trials",
    )
    t.add_argument("--two-phase", action="store_true", default=True)
    t.add_argument("--no-two-phase", action="store_true")
    t.add_argument("--embed-c", action="store_true")
    t.set_defaults(func=cmd_optimize_tiers)

    a = sub.add_parser("anchors", help="Compile LCDA Walk → min CP0 / tier suggestions")
    a.set_defaults(func=cmd_anchors)

    r = sub.add_parser(
        "repair-tiers",
        help="Fix Elite≤Standard≤Tactical ladders on exported presets",
    )
    r.add_argument("--output", type=str, default=str(TOOLS_DIR))
    r.add_argument("--fast", action="store_true")
    r.add_argument("--embed-c", action="store_true")
    r.set_defaults(func=cmd_repair_tiers)

    return p


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
