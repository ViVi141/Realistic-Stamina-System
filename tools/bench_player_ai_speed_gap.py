#!/usr/bin/env python3
"""Offline player vs AI speed-cap gap (6.2.32 Tobler + CP light caps).

Compares absolute target m/s under identical gear / grade / W' latch / phase.
Formulas mirror Enforce SCR_RSS_MetabolismMath Tobler + DrainCalculator InvertCruiseCapMs
(with a compact Pandolf-based power invert for cruise).

Usage (PowerShell, repo root):
  python tools/bench_player_ai_speed_gap.py
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

ROOT = Path(__file__).resolve().parent.parent
OUT_JSON = ROOT / "tools" / "artifacts" / "diagnostics" / "player_ai_speed_gap_6_2_32.json"

# --- constants (Standard milsim bake anchors + C defaults) ---
CHARACTER_WEIGHT = 90.0
GAME_MAX = 5.5
MARCH_WALK = 1.4
MARCH_RUN = 3.58
MARCH_SPRINT = 4.5
ENC_COEFF = 0.21385970069037538  # standard milsim v6
ENC_MAX = 0.75
CP0 = 1867.8678077890445
AEROBIC_CRUISE_MAX = 2.4
CP_HIKE_FLOOR = 1.0
CP_INVERT_GRADE_ABS_MAX = 25.0
CP_INVERT_TERRAIN_MAX = 2.0
UPHILL_BOOST = 1.15
DOWNHILL_BOOST = 1.15
DOWNHILL_MAX = 1.25
TOBLER_FLAT_KMH = 6.0 * math.exp(-3.5 * abs(0.05))

# Pandolf coeffs (C defaults)
P_BASE = 2.7
P_V = 3.2
P_V0 = 0.7
P_GB = 0.23
P_GV = 1.34
P_FIT = 0.80
P_ETA = 1.0  # simplified; game has load dampening — OK for relative gap


def enc_penalty(gear_kg: float) -> float:
    ratio = min(max(gear_kg / CHARACTER_WEIGHT, 0.0), 2.0)
    if ratio <= 0.3:
        raw = 0.15 * ratio
    elif ratio <= 0.6:
        seg = ratio - 0.3
        raw = 0.045 + 0.35 * (seg ** 1.5)
    else:
        seg = ratio - 0.6
        raw = 0.25 + 0.65 * (seg * seg)
    raw = raw * (ENC_COEFF / 0.20)
    return min(max(raw, 0.0), ENC_MAX)


def tobler_mult(angle_deg: float) -> float:
    s = math.tan(math.radians(angle_deg))
    s = min(max(s, -1.0), 1.0)
    w_kmh = 6.0 * math.exp(-3.5 * abs(s + 0.05))
    m = w_kmh / TOBLER_FLAT_KMH
    m = max(m, 0.15)
    if angle_deg > 0.0:
        m = m * UPHILL_BOOST
    elif angle_deg < 0.0:
        m = m * DOWNHILL_BOOST
        m = min(m, DOWNHILL_MAX)
    m = 1.0 + 0.7 * (m - 1.0)
    return min(max(m, 0.15), DOWNHILL_MAX)


def grade_to_angle(grade_pct: float) -> float:
    return math.degrees(math.atan2(grade_pct, 100.0))


def slope_adjust(base_ms: float, angle_deg: float) -> float:
    return base_ms * tobler_mult(angle_deg)


def pandolf_power(v: float, total_kg: float, grade_pct: float, terrain: float) -> float:
    """Compact Pandolf (W); good enough for cruise invert relative gaps."""
    if v < 0.05:
        return 1.2 * CHARACTER_WEIGHT
    load = max(total_kg - CHARACTER_WEIGHT, 0.0)
    g = grade_pct * 0.01
    # classic-ish: M*(a+b*v^2+c*g*v) style blend used in twin docs
    body = CHARACTER_WEIGHT
    mw = body + load
    term = (
        P_BASE
        + P_V * (v + P_V0) ** 2
        + P_GB * g
        + P_GV * g * v
    )
    p = mw * term * terrain * P_ETA * P_FIT
    return max(p, 1.0)


def invert_speed_for_power(
    power_w: float, total_kg: float, grade_pct: float, terrain: float, phase: int
) -> float:
    lo, hi = 0.0, 6.0
    for _ in range(24):
        mid = 0.5 * (lo + hi)
        # walk-like uses same power surface here
        p = pandolf_power(mid, total_kg, grade_pct, terrain)
        if p > power_w:
            hi = mid
        else:
            lo = mid
    return 0.5 * (lo + hi)


def invert_cruise_cap_ms(
    cp_w: float, total_kg: float, grade_pct: float, terrain: float, phase: int
) -> float:
    if cp_w <= 1.0:
        return 0.0
    g = min(max(grade_pct, -CP_INVERT_GRADE_ABS_MAX), CP_INVERT_GRADE_ABS_MAX)
    t = min(max(terrain, 0.5), CP_INVERT_TERRAIN_MAX)
    cap = invert_speed_for_power(cp_w, total_kg, g, t, phase)
    if cap < CP_HIKE_FLOOR:
        cap = CP_HIKE_FLOOR
    return cap


def resolve_run_cruise(raw: float, grade_pct: float) -> float:
    if raw <= 0.05:
        return raw
    cap = raw
    if grade_pct >= 0.0 and cap > AEROBIC_CRUISE_MAX:
        cap = AEROBIC_CRUISE_MAX
    return cap


def cp_eff(cp0: float, gear_kg: float, grade_pct: float) -> float:
    # load/slope decay mirrors CriticalPowerModel (simplified)
    excess = max(0.0, gear_kg - 25.0)
    cp = cp0 * (1.0 - 0.004 * excess)
    g = grade_pct * 0.01
    if g > 0.0:
        cp *= max(0.65, 1.0 - 2.5 * g * g)
    return max(cp, 200.0)


@dataclass
class Row:
    scenario: str
    gear_kg: float
    grade_pct: float
    phase: str
    wprime_latched: bool
    player_ms: float
    ai_old_ms: float
    ai_default_632_ms: float
    ai_drain_632_ms: float
    gap_old_pct: float
    gap_default_pct: float
    gap_drain_pct: float


def ai_old(gear: float, phase: str) -> float:
    pen = enc_penalty(gear)
    enc_m = max(1.0 - pen, 0.5)
    if phase == "walk":
        return MARCH_WALK * enc_m
    if phase == "sprint":
        # drain off: sprint clamped to run
        return MARCH_RUN * enc_m
    return MARCH_RUN * enc_m


def ai_default_632(gear: float, grade: float, phase: str) -> float:
    base = ai_old(gear, phase)
    return slope_adjust(base, grade_to_angle(grade))


def ai_drain_632(
    gear: float, grade: float, phase: str, latched: bool, terrain: float = 1.0
) -> float:
    pen = enc_penalty(gear)
    enc_m = max(1.0 - pen, 0.5)
    if phase == "walk":
        target = MARCH_WALK * enc_m
        ph = 1
    elif phase == "sprint":
        target = MARCH_SPRINT * enc_m
        ph = 3
    else:
        target = MARCH_RUN * enc_m
        ph = 2
    target = slope_adjust(target, grade_to_angle(grade))
    total = CHARACTER_WEIGHT + gear
    cp = cp_eff(CP0, gear, grade)
    if phase == "sprint" and not latched:
        # power soft-cap ≈ invert(sprint_cap) not modeled; use invert(cp*1.4) soft
        sprint_p = min(CP0 * 1.8, 3600.0)
        soft = invert_speed_for_power(sprint_p, total, grade, terrain, 3) * enc_m
        if soft > 0.05 and soft < target:
            target = soft
    apply_cruise = phase == "walk" or latched
    if apply_cruise:
        raw = invert_cruise_cap_ms(cp, total, grade, terrain, ph if ph >= 1 else 2)
        resolved = resolve_run_cruise(raw, grade)
        if resolved > 0.05 and resolved < target:
            target = resolved
    return target


def player_ms(
    gear: float, grade: float, phase: str, latched: bool, terrain: float = 1.0
) -> float:
    """Approx UpdateSpeed theoretical target (Tobler scale + enc + CP cruise/sprint)."""
    pen = enc_penalty(gear)
    # intent enc: run uses ~1+speed_ratio; use 1.0*pen for walk, ~1.5*pen mid for run
    if phase == "sprint":
        enc_p = min(pen * 1.35, ENC_MAX)
    elif phase == "walk":
        enc_p = min(pen * 1.0, ENC_MAX)
    else:
        enc_p = min(pen * 1.25, ENC_MAX)
    enc_m = max(1.0 - enc_p, 0.5)

    angle = grade_to_angle(grade)
    # player: Tobler on flat run ref → scale factor on phase mult
    slope_run = slope_adjust(MARCH_RUN, angle)
    scale = slope_run / MARCH_RUN
    if phase == "walk":
        base = MARCH_WALK * enc_m * scale
        ph = 1
    elif phase == "sprint":
        base = MARCH_SPRINT * enc_m * scale
        ph = 3
    else:
        base = MARCH_RUN * enc_m * scale
        ph = 2

    total = CHARACTER_WEIGHT + gear
    cp = cp_eff(CP0, gear, grade)
    target = base
    if phase == "sprint" and not latched:
        sprint_p = min(CP0 * 1.8, 3600.0)
        soft = invert_speed_for_power(sprint_p, total, grade, terrain, 3) * enc_m
        if soft > 0.05 and soft < target:
            target = soft
    apply_cruise = phase == "walk" or latched
    if apply_cruise:
        raw = invert_cruise_cap_ms(cp, total, grade, terrain, ph)
        resolved = resolve_run_cruise(raw, grade)
        if resolved > 0.05 and resolved < target:
            target = resolved
    return target


def gap_pct(player: float, ai: float) -> float:
    if player < 0.05:
        return 0.0
    return (ai - player) / player * 100.0


def main() -> None:
    scenarios: List[Tuple[str, float, float, str, bool]] = [
        ("flat_light_run_fresh", 15.0, 0.0, "run", False),
        ("flat_29kg_run_fresh", 29.0, 0.0, "run", False),
        ("up5_29kg_run_fresh", 29.0, 5.0, "run", False),
        ("up10_29kg_run_fresh", 29.0, 10.0, "run", False),
        ("up15_29kg_run_fresh", 29.0, 15.0, "run", False),
        ("up15_29kg_run_latched", 29.0, 15.0, "run", True),
        ("dn10_29kg_run_fresh", 29.0, -10.0, "run", False),
        ("flat_29kg_sprint_armed", 29.0, 0.0, "sprint", False),
        ("up10_29kg_sprint_armed", 29.0, 10.0, "sprint", False),
        ("flat_29kg_walk", 29.0, 0.0, "walk", False),
        ("up15_29kg_walk", 29.0, 15.0, "walk", False),
    ]

    rows: List[Row] = []
    for name, gear, grade, phase, latched in scenarios:
        p = player_ms(gear, grade, phase, latched)
        old = ai_old(gear, phase)
        dflt = ai_default_632(gear, grade, phase)
        drain = ai_drain_632(gear, grade, phase, latched)
        rows.append(
            Row(
                scenario=name,
                gear_kg=gear,
                grade_pct=grade,
                phase=phase,
                wprime_latched=latched,
                player_ms=p,
                ai_old_ms=old,
                ai_default_632_ms=dflt,
                ai_drain_632_ms=drain,
                gap_old_pct=gap_pct(p, old),
                gap_default_pct=gap_pct(p, dflt),
                gap_drain_pct=gap_pct(p, drain),
            )
        )

    print("=== Player vs AI target speed gap (offline, Standard milsim anchors) ===")
    print("gap% = (AI - player) / player * 100   (>0 => AI faster)")
    print()
    hdr = (
        f"{'scenario':28} {'P':>5} {'old':>5} {'def32':>5} {'drn32':>5} "
        f"{'gap_old':>8} {'gap_def':>8} {'gap_drn':>8}"
    )
    print(hdr)
    print("-" * len(hdr))
    for r in rows:
        print(
            f"{r.scenario:28} {r.player_ms:5.2f} {r.ai_old_ms:5.2f} "
            f"{r.ai_default_632_ms:5.2f} {r.ai_drain_632_ms:5.2f} "
            f"{r.gap_old_pct:7.1f}% {r.gap_default_pct:7.1f}% {r.gap_drain_pct:7.1f}%"
        )

    abs_old = [abs(r.gap_old_pct) for r in rows]
    abs_def = [abs(r.gap_default_pct) for r in rows]
    abs_drn = [abs(r.gap_drain_pct) for r in rows]

    def mean(xs: List[float]) -> float:
        return sum(xs) / max(len(xs), 1)

    print()
    print(
        f"mean |gap|:  old={mean(abs_old):.1f}%  "
        f"default_6.2.32={mean(abs_def):.1f}%  drain_6.2.32={mean(abs_drn):.1f}%"
    )
    print(
        f"max  |gap|:  old={max(abs_old):.1f}%  "
        f"default_6.2.32={max(abs_def):.1f}%  drain_6.2.32={max(abs_drn):.1f}%"
    )

    # focus rows: uphill fresh run (main complaint)
    uphill = [r for r in rows if "up" in r.scenario and r.phase == "run" and not r.wprime_latched]
    if uphill:
        print()
        print("Uphill Run fresh (main gap case):")
        for r in uphill:
            print(
                f"  {r.scenario}: player={r.player_ms:.2f}  "
                f"old AI +{r.gap_old_pct:.0f}%  "
                f"default +{r.gap_default_pct:.0f}%  "
                f"drain +{r.gap_drain_pct:.0f}%"
            )

    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "version": "6.2.32",
        "note": "Offline absolute-target comparison; not Enforce wall-clock.",
        "mean_abs_gap_pct": {
            "ai_old": mean(abs_old),
            "ai_default_632": mean(abs_def),
            "ai_drain_632": mean(abs_drn),
        },
        "rows": [r.__dict__ for r in rows],
    }
    OUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print()
    print(f"wrote {OUT_JSON}")


if __name__ == "__main__":
    main()
