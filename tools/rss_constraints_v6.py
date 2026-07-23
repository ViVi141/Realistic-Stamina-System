#!/usr/bin/env python3
"""v6 优化管线 — 硬约束与生理锚点（与 bench_physio_anchors / 实机契约一致）。"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from rss_digital_twin_fix import (
    V5AnaerobicState,
    get_drain_velocity_ms,
    get_metabolic_overspeed_factor,
    simulate_ideal_march_aerobic_end,
    simulate_v6_sprint_seconds,
)

# 4h 行军：Elite coeff 已联合标定（Walk 4h + Run sustain obs）
MARCH_4H_AEROBIC_MIN = 0.20
MARCH_4H_HARD = True

SPRINT_BURST_MAX_SEC = 15.0
SPRINT_COOLDOWN_MIN_SEC = 120.0
DRAIN_VEL_TOLERANCE = 0.001
OVERSPEED_EXPECTED = 0.5

# 35 kg Run 稳态观测掉条（%/s；CP 代谢对齐后限速不再假性压速，疲劳 cap 早期掉条更高）
SUSTAIN_OBS_MIN_PCT_PER_S = 0.80
SUSTAIN_OBS_MAX_PCT_PER_S = 2.60
SUSTAIN_OBS_HARD = True

# v6 满体力 Run 理论速度契约（m/s，theoretical_speed_at_weight 不动点）
MOBILITY_RUN_0KG_MIN_MS = 2.65
MOBILITY_RUN_0KG_MAX_MS = 2.95
MOBILITY_RUN_35KG_MIN_MS = 2.15
MOBILITY_RUN_35KG_MAX_MS = 2.85
MOBILITY_HARD = True

TOOLS_DIR = Path(__file__).resolve().parent
ELITE_PRESET_JSON = TOOLS_DIR / "optimized_rss_config_elitestandard_v4.json"


def _load_elite_preset_params() -> Dict:
    if not ELITE_PRESET_JSON.is_file():
        return {}
    with ELITE_PRESET_JSON.open(encoding="utf-8") as f:
        data = json.load(f)
    return {k: float(v) for k, v in data.items() if not str(k).startswith("_")}


@dataclass
class ConstraintCheck:
    name: str
    passed: bool
    detail: str
    hard: bool = True


@dataclass
class ConstraintReport:
    checks: List[ConstraintCheck]

    @property
    def all_hard_passed(self) -> bool:
        for c in self.checks:
            if c.hard and not c.passed:
                return False
        return True

    @property
    def failed_hard(self) -> List[ConstraintCheck]:
        return [c for c in self.checks if c.hard and not c.passed]

    def summary_lines(self) -> List[str]:
        lines: List[str] = []
        for c in self.checks:
            tag = "PASS" if c.passed else ("FAIL" if c.hard else "SKIP")
            hard_tag = "hard" if c.hard else "soft"
            lines.append(f"  [{tag}] ({hard_tag}) {c.name}: {c.detail}")
        return lines


def check_drain_velocity_contract() -> ConstraintCheck:
    v = get_drain_velocity_ms(5.5, 4.0)
    ok = abs(v - 4.0) <= DRAIN_VEL_TOLERANCE
    return ConstraintCheck(
        "drain_velocity_clamp",
        ok,
        f"expected 4.0 m/s, got {v:.4f}",
    )


def check_metabolic_overspeed_contract() -> ConstraintCheck:
    f = get_metabolic_overspeed_factor(800.0, sustainable_watts=400.0)
    ok = abs(f - OVERSPEED_EXPECTED) <= DRAIN_VEL_TOLERANCE
    return ConstraintCheck(
        "metabolic_overspeed_factor",
        ok,
        f"expected {OVERSPEED_EXPECTED}, got {f:.4f} (v5 legacy API)",
        hard=False,
    )


def check_v5_sprint_burst_duration() -> ConstraintCheck:
    ana = V5AnaerobicState()
    t = 0.0
    while ana.pool > 0.0 and t < 20.0:
        ana.tick_sprint(0.2, 0.12)
        t += 0.2
    ok = t <= SPRINT_BURST_MAX_SEC
    return ConstraintCheck(
        "v5_sprint_burst_duration",
        ok,
        f"empty pool in {t:.1f}s (max {SPRINT_BURST_MAX_SEC}s)",
    )


def check_v5_sprint_cooldown() -> ConstraintCheck:
    ana = V5AnaerobicState(pool=0.0, cooldown_until_sec=180.0)
    rem = ana.cooldown_remaining(0.0)
    ok = rem >= SPRINT_COOLDOWN_MIN_SEC
    return ConstraintCheck(
        "v5_sprint_cooldown",
        ok,
        f"cooldown remaining {rem:.0f}s (min {SPRINT_COOLDOWN_MIN_SEC}s)",
    )


def check_v6_cp_sprint_burst(load_kg: float = 35.0, cp0: float = 780.0,
                             w_prime_max: float = 20000.0,
                             sprint_cap_w: float = 2400.0) -> ConstraintCheck:
    t = simulate_v6_sprint_seconds(load_kg=load_kg, cp0=cp0,
                                   w_prime_max=w_prime_max,
                                   sprint_cap_w=sprint_cap_w)
    ok = t <= SPRINT_BURST_MAX_SEC
    return ConstraintCheck(
        "v6_cp_sprint_burst_35kg",
        ok,
        f"W' burst {t:.2f}s (max {SPRINT_BURST_MAX_SEC}s)",
    )


def check_sustain_run_observed(
    params: Optional[Dict] = None,
    duration_s: float = 90.0,
) -> ConstraintCheck:
    """Elite preset 35 kg Run：条上观测掉条落在实机 band 内。"""
    from rss_digital_twin_fix import (
        RSSDigitalTwin,
        RSSConstants,
        merge_game_aligned_params,
        RSS_PLAYER_TICK_SEC,
    )
    from rss_pipeline_v4 import simulate_mission, MovementType, Phase, Mission

    merged = merge_game_aligned_params(_load_elite_preset_params())
    if params:
        merged.update({k: float(v) for k, v in params.items() if not str(k).startswith("_")})
    twin = RSSDigitalTwin(RSSConstants(**merged))
    twin._dt = RSS_PLAYER_TICK_SEC
    mission = Mission(
        name="35kg稳态跑步",
        load_kg=35.0,
        phases=[Phase(duration_s, 0.0, MovementType.RUN, 0, 0.0, 1.0, "稳态Run")],
    )
    result = simulate_mission(twin, mission)
    obs = result.observed_depletion_pct_per_s
    ok = SUSTAIN_OBS_MIN_PCT_PER_S <= obs <= SUSTAIN_OBS_MAX_PCT_PER_S
    if not SUSTAIN_OBS_HARD and not ok:
        return ConstraintCheck(
            "sustain_run_observed_35kg",
            True,
            f"obs={obs:.3f}%/s outside [{SUSTAIN_OBS_MIN_PCT_PER_S}, {SUSTAIN_OBS_MAX_PCT_PER_S}] (soft)",
            hard=False,
        )
    return ConstraintCheck(
        "sustain_run_observed_35kg",
        ok,
        f"obs={obs:.3f}%/s (band {SUSTAIN_OBS_MIN_PCT_PER_S}–{SUSTAIN_OBS_MAX_PCT_PER_S})",
        hard=SUSTAIN_OBS_HARD,
    )


def check_mobility_run_speed(
    load_kg: float,
    min_ms: float,
    max_ms: float,
    label: str,
    params: Optional[Dict] = None,
) -> ConstraintCheck:
    """满体力 Run 理论速度落在 mobility band 内（与 v6 UpdateSpeed 一致）。"""
    from rss_digital_twin_fix import (
        RSSConstants,
        merge_game_aligned_params,
        theoretical_speed_at_weight,
    )

    merged = merge_game_aligned_params(_load_elite_preset_params())
    if params:
        merged.update({k: float(v) for k, v in params.items() if not str(k).startswith("_")})
    constants = RSSConstants(**merged)
    total_weight = 90.0 + float(load_kg)
    speed_ms = theoretical_speed_at_weight(constants, total_weight, 2)
    ok = min_ms <= speed_ms <= max_ms
    name = f"mobility_run_{label}"
    if not MOBILITY_HARD and not ok:
        return ConstraintCheck(
            name,
            True,
            f"speed={speed_ms:.3f} m/s outside [{min_ms}, {max_ms}] (soft)",
            hard=False,
        )
    return ConstraintCheck(
        name,
        ok,
        f"speed={speed_ms:.3f} m/s (band {min_ms}–{max_ms})",
        hard=MOBILITY_HARD,
    )


def check_march_4h_aerobic_end(
    encumbrance_kg: float = 35.0,
    hours: float = 4.0,
    params: Optional[Dict] = None,
) -> ConstraintCheck:
    aerobic_end = simulate_ideal_march_aerobic_end(
        hours=hours, encumbrance_kg=encumbrance_kg, params=params)
    ok = aerobic_end >= MARCH_4H_AEROBIC_MIN
    if not MARCH_4H_HARD and not ok:
        return ConstraintCheck(
            "march_4h_aerobic_end_35kg",
            True,
            f"aerobic_end={aerobic_end:.3f} < {MARCH_4H_AEROBIC_MIN} (soft; coeff calibration pending)",
            hard=False,
        )
    return ConstraintCheck(
        "march_4h_aerobic_end_35kg",
        ok,
        f"aerobic_end={aerobic_end:.3f} (min {MARCH_4H_AEROBIC_MIN})",
        hard=MARCH_4H_HARD,
    )


def check_march_cruise_below_cp(
    load_kg: float = 38.0,
    speed_ms: float = 1.7,
    params: Optional[Dict] = None,
    cp0: float = 780.0,
) -> ConstraintCheck:
    """负重巡航步行：代谢功率不得持续超过 CP（否则会错误消耗 W′）。

    锚点：38 kg @ 1.7 m/s 平地 Walk，与游戏 MetabolismPowerWatts（含负重阻尼）一致。
    """
    from rss_digital_twin_fix import (
        MovementType,
        compute_cp_watts,
        metabolism_power_watts,
        merge_game_aligned_params,
    )

    trial = dict(params) if params else {}
    cp0 = float(trial.get("critical_power_watts", cp0))
    merged = merge_game_aligned_params(
        {k: float(v) for k, v in trial.items() if not str(k).startswith("_")}
    )
    damp = float(merged.get("load_metabolic_dampening", 0.70))
    total_w = 90.0 + float(load_kg)
    p_acct = metabolism_power_watts(
        speed_ms, total_w, 0.0, 1.0, MovementType.WALK, damp
    )
    cp_eff = compute_cp_watts(cp0, float(load_kg), 0.0, 1.0, 0.0)
    ok = p_acct <= cp_eff + 1.0
    return ConstraintCheck(
        "march_cruise_below_cp_38kg_1p7",
        ok,
        f"P={p_acct:.1f}W vs CPeff={cp_eff:.1f}W (CP0={cp0:.0f})",
        hard=True,
    )


def evaluate_physio_anchors(
    load_kg: float = 35.0,
    cp0: float = 780.0,
    params: Optional[Dict] = None,
) -> ConstraintReport:
    trial_params = None
    w_prime_max = 20000.0
    sprint_cap_w = 2400.0
    if params:
        trial_params = dict(params)
        cp0 = float(params.get("critical_power_watts", cp0))
        w_prime_max = float(params.get("w_prime_max_joules", w_prime_max))
        sprint_cap_w = float(params.get("sprint_power_cap_watts", sprint_cap_w))
    elif cp0 != 780.0:
        trial_params = {"critical_power_watts": cp0}

    checks = [
        check_drain_velocity_contract(),
        check_metabolic_overspeed_contract(),
        check_v5_sprint_burst_duration(),
        check_v5_sprint_cooldown(),
        check_v6_cp_sprint_burst(load_kg, cp0, w_prime_max, sprint_cap_w),
        check_march_cruise_below_cp(params=trial_params, cp0=cp0),
        check_sustain_run_observed(trial_params, duration_s=90.0),
        check_mobility_run_speed(
            0.0, MOBILITY_RUN_0KG_MIN_MS, MOBILITY_RUN_0KG_MAX_MS, "0kg", trial_params
        ),
        check_mobility_run_speed(
            35.0, MOBILITY_RUN_35KG_MIN_MS, MOBILITY_RUN_35KG_MAX_MS, "35kg", trial_params
        ),
        check_march_4h_aerobic_end(load_kg, params=trial_params),
    ]
    return ConstraintReport(checks=checks)


def evaluate_hard_constraints_python(params: Optional[Dict] = None) -> ConstraintReport:
    """硬约束门禁（纯 Python 参考实现）。"""
    cp0 = 400.0
    if params:
        cp0 = float(params.get("critical_power_watts", cp0))
    return evaluate_physio_anchors(load_kg=35.0, cp0=cp0, params=params)


def evaluate_hard_constraints(params: Optional[Dict] = None) -> ConstraintReport:
    """硬约束门禁。Rust 优先，失败回退 Python。"""
    from rss_sim_backend import evaluate_hard_constraints as backend_eval

    return backend_eval(params)
