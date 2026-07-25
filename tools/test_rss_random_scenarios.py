#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Random / grid twin scenarios — catch speed/W′/grade clamp regressions.

Usage:
  python tools/test_rss_random_scenarios.py
  python tools/test_rss_random_scenarios.py --n 5000 --seed 7
  python tools/test_rss_random_scenarios.py --quick
  python tools/test_rss_random_scenarios.py --grid --n 100000
  python tools/test_rss_random_scenarios.py --grid-full --i-accept-full-grid

Full design grid (Cartesian):
  3 presets × 99 grades × 45 loads × 14 terrains × 100 stamina × 10 W′ × 3 phases
  = 561,330,000 cells (~270h sequential at ~1.7ms/case). Prefer --grid sampling.
"""

from __future__ import annotations

import argparse
import math
import os
import sys
import time
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, as_completed, wait
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Tuple

import numpy as np

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from rss_digital_twin_fix import (  # noqa: E402
    MovementType,
    RSSConstants,
    RSSDigitalTwin,
    Stance,
    V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS,
    apply_cp_cruise_physics_cap,
    clamp_grade_percent_for_metabolic_speed,
    invert_speed_for_power_watts,
    merge_game_aligned_params,
    metabolism_power_watts,
    refresh_wprime_overspeed_armed,
    should_enforce_cp_cruise_physics_cap,
)
from rss_pipeline_v6 import load_preset_params  # noqa: E402

PRESETS = ("EliteStandard", "StandardMilsim", "TacticalAction")
BODY_KG = 90.0

# User-requested design space: 3×99×45×14×100×10×3
GRID_GRADES = np.linspace(-98.0, 98.0, 99)
GRID_LOADS = np.linspace(0.0, 44.0, 45)
GRID_TERRAINS = np.linspace(0.8, 2.2, 14)
GRID_STAMINA = np.linspace(0.0, 0.99, 100)
GRID_WPRIME = np.linspace(0.0, 0.9, 10)
GRID_PHASES = (MovementType.WALK, MovementType.RUN, MovementType.SPRINT)
GRID_SHAPE = (
    len(PRESETS),
    len(GRID_GRADES),
    len(GRID_LOADS),
    len(GRID_TERRAINS),
    len(GRID_STAMINA),
    len(GRID_WPRIME),
    len(GRID_PHASES),
)
GRID_TOTAL = int(np.prod(np.array(GRID_SHAPE, dtype=np.int64)))

_CONST_CACHE: Dict[str, RSSConstants] = {}


def grid_total() -> int:
    return GRID_TOTAL


def _unravel(index: int) -> Tuple[int, int, int, int, int, int, int]:
    """Map linear cell id → (preset, grade, load, terrain, stamina, w′, phase) indices."""
    dims = GRID_SHAPE
    idx = int(index) % GRID_TOTAL
    out = [0] * 7
    for i in range(6, -1, -1):
        out[i] = idx % dims[i]
        idx //= dims[i]
    return out[0], out[1], out[2], out[3], out[4], out[5], out[6]


def scenario_from_grid_index(index: int) -> dict:
    pi, gi, li, ti, si, wi, phi = _unravel(index)
    # Deterministic aux speeds from cell id (keeps overspeed/phys checks active)
    h = (int(index) * 1103515245 + 12345) & 0x7FFFFFFF
    # Independent mix so coast excess is not coupled to grade/unravel bits.
    h2 = (int(index) * 1664525 + 1013904223) & 0x7FFFFFFF
    v_limit = 0.35 + (h % 3400) / 1000.0
    v_meas = (h // 3400 % 5800) / 1000.0
    # Excess over limit: ~0.16 … ~2.95 m/s (covers soft/hard clamp bands).
    excess = 0.16 + (h2 % 2800) / 1000.0
    coast = v_limit + excess
    if coast > 5.8:
        coast = 5.8
    dt_pick = (0.017, 0.02, 0.05, 0.1)[h % 4]
    return {
        "preset": PRESETS[pi],
        "grade": float(GRID_GRADES[gi]),
        "load_kg": float(GRID_LOADS[li]),
        "terrain": float(GRID_TERRAINS[ti]),
        "stamina": float(GRID_STAMINA[si]),
        "w_pool": float(GRID_WPRIME[wi]),
        "phase": int(GRID_PHASES[phi]),
        "v_meas": float(v_meas),
        "v_limit": float(v_limit),
        "dt": float(dt_pick),
        "coast": float(coast),
        "grid_index": int(index),
    }


@dataclass
class Failure:
    case_id: int
    check: str
    detail: str


@dataclass
class WeirdStats:
    """Counterintuitive-but-passing zones (not failures)."""

    walk_near_run: int = 0
    metabolic_clamped: int = 0
    mid_wprime_disarmed: int = 0
    steep_phys_skip: int = 0
    downhill_coast_over: int = 0
    over_bin_le_035: int = 0
    over_bin_le_070: int = 0
    over_bin_le_150: int = 0
    over_bin_gt_150: int = 0
    crawl_v: int = 0
    high_power: int = 0
    max_downhill_excess: float = 0.0
    samples: List[Tuple[str, str]] = field(default_factory=list)

    def merge(self, other: "WeirdStats") -> None:
        self.walk_near_run += other.walk_near_run
        self.metabolic_clamped += other.metabolic_clamped
        self.mid_wprime_disarmed += other.mid_wprime_disarmed
        self.steep_phys_skip += other.steep_phys_skip
        self.downhill_coast_over += other.downhill_coast_over
        self.over_bin_le_035 += other.over_bin_le_035
        self.over_bin_le_070 += other.over_bin_le_070
        self.over_bin_le_150 += other.over_bin_le_150
        self.over_bin_gt_150 += other.over_bin_gt_150
        self.crawl_v += other.crawl_v
        self.high_power += other.high_power
        if other.max_downhill_excess > self.max_downhill_excess:
            self.max_downhill_excess = other.max_downhill_excess
        for cat, note in other.samples:
            n = sum(1 for c, _ in self.samples if c == cat)
            if n < 3:
                self.samples.append((cat, note))

    def _sample(self, cat: str, note: str) -> None:
        n = sum(1 for c, _ in self.samples if c == cat)
        if n < 3:
            self.samples.append((cat, note))

    def observe_from_sc(self, sc: dict) -> None:
        g = float(sc["grade"])
        if abs(g) > 45.0:
            self.metabolic_clamped += 1
            self._sample("metabolic_clamped", f"grade={g:.1f}")
        if abs(g) > 35.0:
            self.steep_phys_skip += 1
            self._sample("steep_phys_skip", f"|grade|={abs(g):.1f}")
        wp = float(sc["w_pool"])
        if 0.25 < wp <= 0.60:
            self.mid_wprime_disarmed += 1
            self._sample(
                "mid_wprime_disarmed",
                f"W′={wp:.2f} stamina={float(sc['stamina']):.2f}",
            )
        if g < -2.0 and float(sc["coast"]) > float(sc["v_limit"]) + 0.15:
            ex = float(sc["coast"]) - float(sc["v_limit"])
            self.downhill_coast_over += 1
            if ex <= 0.35:
                self.over_bin_le_035 += 1
            elif ex <= 0.70:
                self.over_bin_le_070 += 1
            elif ex <= 1.50:
                self.over_bin_le_150 += 1
            else:
                self.over_bin_gt_150 += 1
            if ex > self.max_downhill_excess:
                self.max_downhill_excess = ex
            self._sample(
                "downhill_coast_over",
                f"grade={g:.1f} coast={float(sc['coast']):.2f} "
                f"lim={float(sc['v_limit']):.2f} excess={ex:.2f}",
            )
        v = sc.get("_obs_v")
        p = sc.get("_obs_p")
        v_walk = sc.get("_obs_v_walk")
        v_run = sc.get("_obs_v_run")
        if v is not None and float(v) < 0.55:
            self.crawl_v += 1
            self._sample(
                "crawl_v",
                f"v={float(v):.3f} phase={sc['phase']} grade={g:.1f} "
                f"load={float(sc['load_kg']):.1f}",
            )
        if p is not None and float(p) > 3000.0:
            self.high_power += 1
            self._sample(
                "high_power",
                f"P={float(p):.0f}W v={float(v or 0):.3f} grade={g:.1f}",
            )
        if v_walk is not None and v_run is not None:
            gap = float(v_run) - float(v_walk)
            if 0.0 <= gap <= 0.05:
                self.walk_near_run += 1
                self._sample(
                    "walk_near_run",
                    f"walk={float(v_walk):.3f} run={float(v_run):.3f} "
                    f"grade={g:.1f} load={float(sc['load_kg']):.1f}",
                )

    def print_report(self, total: int) -> None:
        def pct(c: int) -> float:
            if total <= 0:
                return 0.0
            return c * 100.0 / float(total)

        print("--- weird-but-legal (pass, counterintuitive zones) ---")
        print(
            f"  metabolic_clamped(|grade|>45): {self.metabolic_clamped} "
            f"({pct(self.metabolic_clamped):.2f}%)"
        )
        print(
            f"  steep_phys_skip(|grade|>35): {self.steep_phys_skip} "
            f"({pct(self.steep_phys_skip):.2f}%)"
        )
        print(
            f"  mid_wprime_disarmed(0.25<W′≤0.60): {self.mid_wprime_disarmed} "
            f"({pct(self.mid_wprime_disarmed):.2f}%)"
        )
        print(
            f"  walk_near_run(demoted |Δ|≤0.05): {self.walk_near_run} "
            f"({pct(self.walk_near_run):.2f}%)"
        )
        print(f"  crawl_v(phase v<0.55): {self.crawl_v} ({pct(self.crawl_v):.2f}%)")
        print(
            f"  high_power(P>3000W): {self.high_power} ({pct(self.high_power):.2f}%)"
        )
        print(
            f"  downhill_coast_over(grade<-2, coast>lim+0.15): "
            f"{self.downhill_coast_over} ({pct(self.downhill_coast_over):.2f}%) "
            f"max_excess={self.max_downhill_excess:.3f} m/s"
        )
        print(
            f"    excess bins: ≤0.35={self.over_bin_le_035}  "
            f"≤0.70={self.over_bin_le_070}  ≤1.50={self.over_bin_le_150}  "
            f">1.50={self.over_bin_gt_150}"
        )
        if self.samples:
            print("  samples:")
            for cat, note in self.samples:
                print(f"    [{cat}] {note}")


def _finite(x: float) -> bool:
    return math.isfinite(float(x))


def _sample_scenario(rng: np.random.Generator) -> dict:
    preset = PRESETS[int(rng.integers(0, len(PRESETS)))]
    # Mix of gentle / steep / cliff grades (raw may exceed metabolic clamp)
    grade_mode = int(rng.integers(0, 5))
    if grade_mode == 0:
        grade = float(rng.uniform(-12.0, 12.0))
    elif grade_mode == 1:
        grade = float(rng.uniform(-35.0, -12.0))
    elif grade_mode == 2:
        grade = float(rng.uniform(12.0, 35.0))
    elif grade_mode == 3:
        grade = float(rng.choice([-99.0, -70.0, -50.0, 50.0, 70.0, 99.0]))
    else:
        grade = float(rng.uniform(-100.0, 100.0))

    load_kg = float(rng.uniform(0.0, 45.0))
    terrain = float(rng.uniform(0.8, 2.2))
    stamina = float(rng.uniform(0.0, 1.0))
    w_pool = float(rng.uniform(0.0, 1.0))
    # Bias toward mid-band disarmed (25–60%) where bugs clustered
    if rng.random() < 0.35:
        w_pool = float(rng.uniform(0.20, 0.58))

    phase_roll = int(rng.integers(0, 3))
    if phase_roll == 0:
        phase = MovementType.WALK
    elif phase_roll == 1:
        phase = MovementType.RUN
    else:
        phase = MovementType.SPRINT

    v_meas = float(rng.uniform(0.0, 5.8))
    v_limit = float(rng.uniform(0.3, 3.8))
    dt = float(rng.choice([0.017, 0.02, 0.05, 0.1]))
    coast = float(rng.uniform(max(v_limit, 0.5), 5.8))

    return {
        "preset": preset,
        "grade": grade,
        "load_kg": load_kg,
        "terrain": terrain,
        "stamina": stamina,
        "w_pool": w_pool,
        "phase": phase,
        "v_meas": v_meas,
        "v_limit": v_limit,
        "dt": dt,
        "coast": coast,
    }


def _constants_for_preset(preset: str) -> RSSConstants:
    cached = _CONST_CACHE.get(preset)
    if cached is not None:
        return cached
    params = dict(load_preset_params(preset))
    constants = RSSConstants(**merge_game_aligned_params(params))
    _CONST_CACHE[preset] = constants
    return constants


def _make_twin(preset: str, load_kg: float, w_pool: float, stamina: float) -> RSSDigitalTwin:
    twin = RSSDigitalTwin(_constants_for_preset(preset))
    twin.reset()
    twin.stamina = float(np.clip(stamina, 0.0, 1.0))
    max_j = float(twin.v6_cp_state.w_prime_max_joules)
    twin.v6_cp_state.w_prime_joules = max_j * float(np.clip(w_pool, 0.0, 1.0))
    # Start disarmed if mid/low pool so Schmitt must rearm properly
    if w_pool <= 0.60:
        twin.v6_cp_state.overspeed_armed = False
    else:
        twin.v6_cp_state.overspeed_armed = True
    twin.v6_cp_state.refresh_and_get_overspeed_armed()
    twin.v6_cp_state.set_runtime_context(load_kg, 0.0, 1.0, min(1.0, max(0.0, 1.0 - stamina)))
    return twin


def _check_grade_policy(sc: dict) -> Optional[str]:
    from rss_digital_twin_fix import V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP

    g = float(sc["grade"])
    enforce = should_enforce_cp_cruise_physics_cap(g)
    # 默认不压速：物理钳关闭，任意坡度都不应 enforce
    if not V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP:
        if enforce:
            return f"drain-only mode still enforces phys clamp grade={g:.2f}"
        return None
    if abs(g) > 35.0 and enforce:
        return f"steep |grade|={abs(g):.2f} still enforces phys clamp"
    if abs(g) <= 35.0 and not enforce:
        return f"grade={g:.2f} should allow phys clamp path"
    return None


def _check_metabolic_grade_clamp(sc: dict) -> Optional[str]:
    g = float(sc["grade"])
    c = clamp_grade_percent_for_metabolic_speed(g)
    if abs(c) > 45.0 + 1e-9:
        return f"metabolic clamp leaked |g|={abs(c):.2f}"
    if abs(g) <= 45.0 and abs(c - g) > 1e-9:
        return f"metabolic clamp altered in-range grade {g} -> {c}"
    if abs(g) > 45.0 and abs(abs(c) - 45.0) > 1e-9:
        return f"metabolic clamp expected ±45 got {c} from {g}"
    return None


def _check_phys_cap_behavior(sc: dict) -> Optional[str]:
    from rss_digital_twin_fix import V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP

    g = float(sc["grade"])
    lim = float(sc["v_limit"])
    coast = float(sc["coast"])
    dt = float(sc["dt"])
    out = apply_cp_cruise_physics_cap(coast, lim, dt, g)
    if not _finite(out):
        return f"phys cap non-finite {out}"
    # 默认不压速：物理钳恒为旁路
    if not V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP:
        if abs(out - coast) > 1e-6:
            return f"drain-only phys clamp mutated {coast:.2f}->{out:.2f}"
        return None
    enforce = should_enforce_cp_cruise_physics_cap(g, coast, lim)
    if abs(g) > 35.0:
        if abs(out - coast) > 1e-6:
            return f"steep skip mutated {coast:.2f}->{out:.2f} grade={g:.2f}"
        return None
    if not enforce:
        if abs(out - coast) > 1e-6:
            return f"coast skip mutated {coast:.2f}->{out:.2f} grade={g:.2f}"
        return None
    if coast > lim + 0.35:
        if out > lim + 1e-6:
            return f"hard clamp failed coast={coast:.2f} lim={lim:.2f} out={out:.2f}"
    else:
        expected_max = coast - 9.0 * max(0.01, min(0.5, dt))
        if expected_max < lim:
            expected_max = lim
        if out > expected_max + 1e-4:
            return (
                f"soft clamp no decelerate coast={coast:.2f} "
                f"lim={lim:.2f} out={out:.2f} expect<={expected_max:.2f}"
            )
        if out > coast + 1e-9:
            return f"clamp increased speed {coast:.2f}->{out:.2f}"
    return None


def _check_schmitt(sc: dict) -> Optional[str]:
    pool = float(sc["w_pool"])
    # From disarmed: mid-band must stay false
    if 0.0 <= pool <= 0.60:
        armed = refresh_wprime_overspeed_armed(pool, False)
        if armed:
            return f"pool={pool:.2f} rearmed from disarmed (expect stay false until >0.60)"
    # From armed: only disarm at/under ~0.25
    if pool > 0.25 + 1e-9:
        armed = refresh_wprime_overspeed_armed(pool, True)
        if not armed:
            return f"pool={pool:.2f} disarmed from armed unexpectedly"
    if pool <= 0.25:
        armed = refresh_wprime_overspeed_armed(pool, True)
        if armed:
            return f"pool={pool:.2f} should disarm from armed"
    return None


def _check_speeds(twin: RSSDigitalTwin, sc: dict) -> Optional[str]:
    total = BODY_KG + float(sc["load_kg"])
    grade = float(sc["grade"])
    terrain = float(sc["terrain"])
    sta = float(sc["stamina"])
    phase = int(sc["phase"])

    v = twin.calculate_actual_speed(
        sta,
        total,
        phase,
        2.0,
        grade_percent=grade,
        current_time=12.0,
        terrain_factor=terrain,
    )
    if not _finite(v):
        return f"calculate_actual_speed non-finite {v}"
    if v < 0.0 or v > 8.0:
        return f"calculate_actual_speed out of range {v:.3f}"
    sc["_obs_v"] = float(v)

    g_meta = clamp_grade_percent_for_metabolic_speed(grade)
    p = metabolism_power_watts(max(v, 0.1), total, g_meta, terrain, phase)
    if not _finite(p) or p < 0.0 or p > 20000.0:
        return f"metabolism_power weird P={p}"
    sc["_obs_p"] = float(p)

    inv = invert_speed_for_power_watts(
        max(200.0, twin.v6_cp_state.get_effective_critical_power_watts()),
        total,
        g_meta,
        terrain,
        2,
    )
    if not _finite(inv) or inv < 0.0 or inv > 8.0:
        return f"invert weird {inv}"

    # W′ empty: Walk must not beat Run (same load/grade/terrain)
    twin.v6_cp_state.w_prime_joules = 0.0
    twin.v6_cp_state.overspeed_armed = False
    twin.v6_cp_state.refresh_and_get_overspeed_armed()
    v_run = twin.calculate_actual_speed(
        max(sta, 0.15),
        total,
        MovementType.RUN,
        2.0,
        grade_percent=grade,
        current_time=20.0,
        terrain_factor=terrain,
    )
    v_walk = twin.calculate_actual_speed(
        max(sta, 0.15),
        total,
        MovementType.WALK,
        1.0,
        grade_percent=grade,
        current_time=20.0,
        terrain_factor=terrain,
    )
    if not _finite(v_run) or not _finite(v_walk):
        return f"walk/run non-finite run={v_run} walk={v_walk}"
    if v_walk > v_run + 0.03:
        return (
            f"Walk faster than demoted Run: walk={v_walk:.3f} run={v_run:.3f} "
            f"grade={grade:.1f} load={sc['load_kg']:.1f} terr={terrain:.2f}"
        )
    sc["_obs_v_walk"] = float(v_walk)
    sc["_obs_v_run"] = float(v_run)
    return None


def _check_short_step(twin: RSSDigitalTwin, sc: dict) -> Optional[str]:
    total = BODY_KG + float(sc["load_kg"])
    grade = clamp_grade_percent_for_metabolic_speed(float(sc["grade"]))
    terrain = float(sc["terrain"])
    speed = float(sc["v_meas"])
    phase = int(sc["phase"])
    t = 0.0
    for i in range(8):
        twin.step(
            speed,
            total,
            grade,
            terrain,
            Stance.STAND,
            phase,
            t,
            enable_randomness=False,
            time_delta_override=float(sc["dt"]),
        )
        t += float(sc["dt"])
        if not _finite(twin.stamina):
            return f"stamina non-finite after step {i}"
        if twin.stamina < -0.01 or twin.stamina > 1.01:
            return f"stamina out of range {twin.stamina}"
        pool = float(twin.v6_cp_state.pool01)
        if not _finite(pool) or pool < -0.01 or pool > 1.01:
            return f"W' pool out of range {pool}"
    return None


def _check_thrash_metric(sc: dict) -> Optional[str]:
    """Gentle downhill: mild coast must not fight; runaway clamps only if phys clamp on."""
    from rss_digital_twin_fix import V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP

    g = float(sc["grade"])
    if g >= -2.0 or g < -35.0:
        return None
    lim = float(sc["v_limit"])
    allow = float(V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS)
    mild_coast = lim + allow * 0.75
    dt = 0.02
    v = lim
    mild_events = 0
    for _ in range(40):
        v = min(mild_coast, v + 10.0 * dt)
        before = v
        v = apply_cp_cruise_physics_cap(v, lim, dt, g)
        if abs(v - before) > 1e-9:
            mild_events += 1
    if mild_events > 0:
        return f"mild downhill coast fought events={mild_events} grade={g:.2f}"

    # 不压速默认：不要求物理钳住窜速（透支改扣 STA）
    if not V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP:
        return None

    runaway = lim + allow + 0.50
    after = apply_cp_cruise_physics_cap(runaway, lim, dt, g)
    if after > lim + 1e-3:
        return (
            f"downhill runaway not clamped grade={g:.2f} "
            f"in={runaway:.2f} out={after:.2f} lim={lim:.2f}"
        )
    return None


CHECKS: List[Tuple[str, Callable]] = [
    ("grade_policy", lambda twin, sc: _check_grade_policy(sc)),
    ("metabolic_grade", lambda twin, sc: _check_metabolic_grade_clamp(sc)),
    ("phys_cap", lambda twin, sc: _check_phys_cap_behavior(sc)),
    ("schmitt", lambda twin, sc: _check_schmitt(sc)),
    ("speeds", lambda twin, sc: _check_speeds(twin, sc)),
    ("step", lambda twin, sc: _check_short_step(twin, sc)),
    ("thrash", lambda twin, sc: _check_thrash_metric(sc)),
]


def _eval_one(
    case_id: int, sc: dict
) -> Tuple[Optional[Tuple[int, str, str]], WeirdStats]:
    weird = WeirdStats()
    twin = _make_twin(sc["preset"], sc["load_kg"], sc["w_pool"], sc["stamina"])
    for name, fn in CHECKS:
        try:
            err = fn(twin, sc)
        except Exception as exc:  # noqa: BLE001
            err = f"exception: {exc}"
        if err:
            return (case_id, name, err + f" | sc={sc}"), WeirdStats()
    weird.observe_from_sc(sc)
    return None, weird


def _eval_scenario(
    case_id: int,
    sc: dict,
    failures: List[Failure],
    weird: WeirdStats,
) -> None:
    hit, w = _eval_one(case_id, sc)
    if hit is not None:
        failures.append(Failure(hit[0], hit[1], hit[2]))
    else:
        weird.merge(w)


def _worker_eval_scenarios(
    batch: Sequence[Tuple[int, dict]],
) -> Tuple[List[Tuple[int, str, str]], WeirdStats]:
    """Process-pool worker: evaluate a batch of (case_id, scenario)."""
    out: List[Tuple[int, str, str]] = []
    weird = WeirdStats()
    for case_id, sc in batch:
        hit, w = _eval_one(int(case_id), sc)
        if hit is not None:
            out.append(hit)
        else:
            weird.merge(w)
    return out, weird


def _worker_eval_grid_indices(
    batch: Sequence[Tuple[int, int]],
) -> Tuple[List[Tuple[int, str, str]], WeirdStats]:
    """Process-pool worker: (case_id, grid_index) → rebuild scenario in-worker."""
    out: List[Tuple[int, str, str]] = []
    weird = WeirdStats()
    for case_id, gi in batch:
        sc = scenario_from_grid_index(int(gi))
        hit, w = _eval_one(int(case_id), sc)
        if hit is not None:
            out.append(hit)
        else:
            weird.merge(w)
    return out, weird


def _chunked(items: Sequence, chunk_size: int) -> Iterable[Sequence]:
    n = len(items)
    cs = max(1, int(chunk_size))
    for i in range(0, n, cs):
        yield items[i : i + cs]


def _default_jobs() -> int:
    n = os.cpu_count()
    if n is None or n < 1:
        return 1
    # Leave one logical core for the OS/UI on interactive machines
    if n <= 2:
        return n
    return n - 1


def _run_parallel_scenarios(
    scenarios: Sequence[Tuple[int, dict]],
    jobs: int,
    chunk_size: int,
) -> Tuple[List[Failure], WeirdStats]:
    weird = WeirdStats()
    if jobs <= 1 or len(scenarios) < 32:
        fails: List[Failure] = []
        for case_id, sc in scenarios:
            _eval_scenario(case_id, sc, fails, weird)
        return fails, weird

    batches = list(_chunked(list(scenarios), chunk_size))
    fails = []
    done_cases = 0
    total = len(scenarios)
    t0 = time.perf_counter()
    with ProcessPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(_worker_eval_scenarios, batch) for batch in batches]
        for fut in as_completed(futures):
            batch_fails, batch_weird = fut.result()
            for case_id, name, detail in batch_fails:
                fails.append(Failure(case_id, name, detail))
            weird.merge(batch_weird)
            done_cases += chunk_size
            if done_cases >= 20000 and done_cases % 20000 < chunk_size:
                elapsed = time.perf_counter() - t0
                finished = min(done_cases, total)
                rate = finished / max(elapsed, 1e-6)
                eta = (total - finished) / max(rate, 1e-6)
                print(
                    f"  … ~{finished}/{total}  {rate:.0f}/s  fails={len(fails)}  "
                    f"jobs={jobs}  ETA={eta/60:.1f}min",
                    flush=True,
                )
    return fails, weird


def _run_parallel_grid_indices(
    pairs: Sequence[Tuple[int, int]],
    jobs: int,
    chunk_size: int,
) -> Tuple[List[Failure], WeirdStats]:
    weird = WeirdStats()
    if jobs <= 1 or len(pairs) < 32:
        fails: List[Failure] = []
        for case_id, gi in pairs:
            _eval_scenario(
                case_id, scenario_from_grid_index(int(gi)), fails, weird
            )
        return fails, weird

    batches = list(_chunked(list(pairs), chunk_size))
    fails = []
    done_cases = 0
    total = len(pairs)
    t0 = time.perf_counter()
    with ProcessPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(_worker_eval_grid_indices, batch) for batch in batches]
        for fut in as_completed(futures):
            batch_fails, batch_weird = fut.result()
            for case_id, name, detail in batch_fails:
                fails.append(Failure(case_id, name, detail))
            weird.merge(batch_weird)
            done_cases += chunk_size
            if done_cases >= 20000 and done_cases % 20000 < chunk_size:
                elapsed = time.perf_counter() - t0
                finished = min(done_cases, total)
                rate = finished / max(elapsed, 1e-6)
                eta = (total - finished) / max(rate, 1e-6)
                print(
                    f"  … ~{finished}/{total}  {rate:.0f}/s  fails={len(fails)}  "
                    f"jobs={jobs}  ETA={eta/60:.1f}min",
                    flush=True,
                )
    return fails, weird


def run_random_battery(
    n: int, seed: int, jobs: int = 1, chunk_size: int = 64
) -> Tuple[int, List[Failure], WeirdStats]:
    rng = np.random.default_rng(seed)
    scenarios = [(i, _sample_scenario(rng)) for i in range(n)]
    fails, weird = _run_parallel_scenarios(scenarios, jobs, chunk_size)
    return n, fails, weird


def run_grid_sample(
    n: int, seed: int, jobs: int = 1, chunk_size: int = 64
) -> Tuple[int, List[Failure], WeirdStats]:
    """Sample unique cells from the 3×99×45×14×100×10×3 design grid."""
    rng = np.random.default_rng(seed)
    n = max(1, int(n))
    if n >= GRID_TOTAL:
        return run_grid_full(
            shard_index=0, shard_count=1, jobs=jobs, chunk_size=chunk_size
        )
    if n <= 2_000_000:
        indices = rng.choice(GRID_TOTAL, size=n, replace=False)
    else:
        indices = ((rng.integers(0, GRID_TOTAL, size=n, dtype=np.int64)
                    + np.arange(n, dtype=np.int64) * 1_000_003) % GRID_TOTAL)
    pairs = [(i, int(gi)) for i, gi in enumerate(indices)]
    fails, weird = _run_parallel_grid_indices(pairs, jobs, chunk_size)
    return n, fails, weird


def run_grid_full(
    shard_index: int = 0,
    shard_count: int = 1,
    jobs: int = 1,
    chunk_size: int = 64,
) -> Tuple[int, List[Failure], WeirdStats]:
    """Iterate the full Cartesian grid (optionally sharded + multi-process).

    Streams batches so we never materialize all 561M indices in RAM.
    """
    shard_count = max(1, int(shard_count))
    shard_index = max(0, min(int(shard_index), shard_count - 1))
    # Approximate count for this shard
    total = (GRID_TOTAL - shard_index + shard_count - 1) // shard_count
    fails: List[Failure] = []
    weird = WeirdStats()
    if jobs <= 1:
        done = 0
        t0 = time.perf_counter()
        for gi in range(shard_index, GRID_TOTAL, shard_count):
            _eval_scenario(done, scenario_from_grid_index(gi), fails, weird)
            done += 1
            if done % 20000 == 0:
                elapsed = time.perf_counter() - t0
                rate = done / max(elapsed, 1e-6)
                eta = (total - done) / max(rate, 1e-6)
                print(
                    f"  … {done}/{total}  {rate:.0f}/s  fails={len(fails)}  "
                    f"ETA={eta/3600:.1f}h",
                    flush=True,
                )
        return done, fails, weird

    # Multi-process: keep a bounded window of in-flight chunk futures
    t0 = time.perf_counter()
    done = 0
    case_id = 0
    gi = shard_index
    max_inflight = max(jobs * 4, jobs)
    with ProcessPoolExecutor(max_workers=jobs) as pool:
        inflight = {}
        while gi < GRID_TOTAL or inflight:
            while gi < GRID_TOTAL and len(inflight) < max_inflight:
                batch = []
                while gi < GRID_TOTAL and len(batch) < chunk_size:
                    batch.append((case_id, gi))
                    case_id += 1
                    gi += shard_count
                if batch:
                    fut = pool.submit(_worker_eval_grid_indices, batch)
                    inflight[fut] = len(batch)
            if not inflight:
                break
            done_set, _ = wait(tuple(inflight.keys()), return_when=FIRST_COMPLETED)
            for finished in done_set:
                batch_n = inflight.pop(finished)
                batch_fails, batch_weird = finished.result()
                for cid, name, detail in batch_fails:
                    fails.append(Failure(cid, name, detail))
                weird.merge(batch_weird)
                done += batch_n
            if done > 0 and done % 20000 < chunk_size * jobs:
                elapsed = time.perf_counter() - t0
                rate = done / max(elapsed, 1e-6)
                eta = (total - done) / max(rate, 1e-6)
                print(
                    f"  … {done}/{total}  {rate:.0f}/s  fails={len(fails)}  "
                    f"jobs={jobs}  ETA={eta/3600:.1f}h",
                    flush=True,
                )
    return done, fails, weird


def run_quick() -> bool:
    _n, fails, _weird = run_random_battery(128, seed=20260725, jobs=1)
    if len(fails) > 0:
        return False
    corners = [0, GRID_TOTAL // 2, GRID_TOTAL - 1, 1234567 % GRID_TOTAL]
    fails2: List[Failure] = []
    weird2 = WeirdStats()
    for i, gi in enumerate(corners):
        _eval_scenario(i, scenario_from_grid_index(gi), fails2, weird2)
    return len(fails2) == 0


def main() -> int:
    ap = argparse.ArgumentParser(description="RSS twin random/grid scenario battery")
    ap.add_argument("--n", type=int, default=2000, help="cases (random or grid-sample)")
    ap.add_argument("--seed", type=int, default=42, help="RNG seed")
    ap.add_argument("--quick", action="store_true", help="128 random + grid corners (smoke)")
    ap.add_argument("--grid", action="store_true",
                    help="sample unique cells from 3×99×45×14×100×10×3 grid")
    ap.add_argument("--grid-full", action="store_true",
                    help="iterate entire 561,330,000-cell grid (needs confirmation)")
    ap.add_argument("--i-accept-full-grid", action="store_true",
                    help="acknowledge ~270h sequential full-grid cost")
    ap.add_argument("--shard", type=str, default="0/1",
                    help="full-grid shard as i/N (e.g. 0/8)")
    ap.add_argument("--max-print", type=int, default=12, help="max failures to print")
    ap.add_argument("--show-grid", action="store_true", help="print grid size and exit")
    ap.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=0,
        help="worker processes (0=auto cpu-1, 1=single-core)",
    )
    ap.add_argument(
        "--chunk-size",
        type=int,
        default=64,
        help="cases per process-pool task (default 64)",
    )
    args = ap.parse_args()

    jobs = int(args.jobs)
    if jobs <= 0:
        jobs = _default_jobs()
    chunk_size = max(8, int(args.chunk_size))

    print(
        f"design grid: {'×'.join(str(x) for x in GRID_SHAPE)} = {GRID_TOTAL:,} cells",
        flush=True,
    )
    if args.show_grid:
        print(f"est. ~1.7ms/case → full sequential ≈ {GRID_TOTAL * 0.0017 / 3600:.0f} h")
        print(f"auto jobs={_default_jobs()} (logical cpus={os.cpu_count()})")
        return 0

    seed = int(args.seed)
    t0 = time.perf_counter()

    if args.quick:
        ok = run_quick()
        print("=== RSS twin battery (quick) ===")
        if ok:
            print("PASS: quick random+grid corners")
            return 0
        print("FAIL: quick suite")
        return 1
    elif args.grid_full:
        if not args.i_accept_full_grid:
            print(
                "REFUSE: full grid is 561,330,000 cells (~11 days @1.7ms single-core). "
                "Use --grid --n 100000 -j 0, or pass --i-accept-full-grid "
                "(optionally --shard i/N)."
            )
            return 2
        parts = str(args.shard).split("/")
        shard_i = int(parts[0])
        shard_n = int(parts[1]) if len(parts) > 1 else 1
        total, fails, weird = run_grid_full(
            shard_i, shard_n, jobs=jobs, chunk_size=chunk_size
        )
        mode = f"grid-full shard={shard_i}/{shard_n}"
    elif args.grid:
        n = max(1, int(args.n))
        total, fails, weird = run_grid_sample(
            n, seed, jobs=jobs, chunk_size=chunk_size
        )
        mode = "grid-sample"
    else:
        n = max(1, int(args.n))
        total, fails, weird = run_random_battery(
            n, seed, jobs=jobs, chunk_size=chunk_size
        )
        mode = "random"

    elapsed = time.perf_counter() - t0
    print(f"=== RSS twin battery ({mode}) ===")
    print(
        f"cases={total} seed={seed} checks={len(CHECKS)} jobs={jobs} "
        f"chunk={chunk_size} elapsed={elapsed:.1f}s ({total/max(elapsed,1e-6):.0f}/s)"
    )
    weird.print_report(total)
    if not fails:
        print(f"PASS: {total}/{total} scenarios")
        return 0

    print(f"FAIL: {len(fails)}/{total} scenarios")
    by_check = {}
    for f in fails:
        by_check[f.check] = by_check.get(f.check, 0) + 1
    for k, v in sorted(by_check.items(), key=lambda kv: -kv[1]):
        print(f"  {k}: {v}")
    print("--- samples ---")
    for f in fails[: max(1, int(args.max_print))]:
        print(f"  [{f.case_id}] {f.check}: {f.detail}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
