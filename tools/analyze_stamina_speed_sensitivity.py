#!/usr/bin/env python3
"""Offline sensitivity: which factors move stamina <-> speed the most (v6 twin).

Baseline: StandardMilsim, Run, 30 kg load, flat, terrain=1, STA=100%, W'=full.

Reports elasticities / % deltas for:
  - command speed (V6 phase multiplier -> m/s)
  - metabolic power P(W)
  - aerobic drain %/s
  - CP-cruise sustainable speed when W' empty (if invertible)

Usage:
  python tools/analyze_stamina_speed_sensitivity.py
  python tools/analyze_stamina_speed_sensitivity.py --json tools/artifacts/diagnostics/sta_speed_sensitivity.json
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_digital_twin_fix import (  # noqa: E402
    MovementType,
    RSSConstants,
    RSSDigitalTwin,
    get_metabolic_speed_cap_ms,
    merge_game_aligned_params,
    metabolism_power_watts,
    stamina_drain_rate_per_second_from_power_watts,
)
from rss_pipeline_v6 import load_preset_params  # noqa: E402

BODY_KG = 90.0
GAME_MAX = 5.5


@dataclass
class FactorHit:
    factor: str
    label_zh: str
    axis: str  # speed | drain | power | cruise
    delta_pct: float
    baseline: float
    perturbed: float
    detail: str


def _abs_ms(twin: RSSDigitalTwin, sta: float, phase: int, enc_pen: float) -> float:
    mult = twin.calculate_v6_phase_speed_multiplier(sta, phase, enc_pen)
    return mult * GAME_MAX


def _enc_pen(twin: RSSDigitalTwin, load_kg: float) -> float:
    # current_weight in twin = equipment mass only (not body) for EncumbranceCache path
    # _encumbrance_speed_penalty_base expects current_weight as total equipment from inventory
    # Looking at code: effective_load = current_weight - bw - base_w — wait that subtracts body?
    # Actually: current_weight from GetTotalWeightOfAllStorages is equipment only.
    # But _encumbrance_speed_penalty_base does: effective_load = max(0, current_weight - bw - base_w)
    # That seems wrong if current_weight is equipment-only...
    # Check simulate: current_weight is equipment. Looking again:
    # effective_load = max(0.0, current_weight - bw - base_w) with bw=90
    # If current_weight is 30 (equipment), effective_load = 0. That can't be right.
    #
    # From EncumbranceCache.c: currentWeight from GetTotalWeightOfAllStorages (equipment),
    # effectiveWeight = max(currentWeight - BASE_WEIGHT, 0) — BASE_WEIGHT is ~1.36 not body!
    #
    # Twin has a bug OR current_weight includes body somewhere. Read twin again:
    # effective_load = max(0.0, current_weight - bw - base_w)  — uses CHARACTER_WEIGHT
    #
    # Wait SCR_RSS_EncumbranceCache:
    # effectiveWeight = Math.Max(currentWeight - SCR_RSS_Constants.BASE_WEIGHT, 0.0);
    # So twin's _encumbrance_speed_penalty_base incorrectly subtracts CHARACTER_WEIGHT.
    #
    # For analysis use the C-aligned formula locally:
    return _enc_pen_c_aligned(twin, load_kg)


def _enc_pen_c_aligned(twin: RSSDigitalTwin, equipment_kg: float) -> float:
    """Match SCR_RSS_EncumbranceCache.ComputeSpeedPenaltyFromEffectiveWeight."""
    base_w = float(getattr(twin.constants, "BASE_WEIGHT", 1.36))
    bw = float(getattr(twin.constants, "CHARACTER_WEIGHT", 90.0))
    coeff = float(getattr(twin.constants, "ENCUMBRANCE_SPEED_PENALTY_COEFF", 0.28))
    max_pen = float(getattr(twin.constants, "ENCUMBRANCE_SPEED_PENALTY_MAX", 0.75))
    effective = max(equipment_kg - base_w, 0.0)
    ratio = 0.0
    if bw > 0.0:
        ratio = effective / bw
    ratio = max(0.0, min(2.0, ratio))
    if ratio <= 0.3:
        raw = 0.15 * ratio
    elif ratio <= 0.6:
        seg = ratio - 0.3
        raw = 0.045 + 0.35 * (seg ** 1.5)
    else:
        seg = ratio - 0.6
        raw = 0.25 + 0.65 * (seg * seg)
    raw = raw * (coeff / 0.20)
    return max(0.0, min(max_pen, raw))


def _power(
    speed_ms: float,
    total_kg: float,
    grade: float,
    terrain: float,
    phase: int,
) -> float:
    return float(
        metabolism_power_watts(
            velocity_ms=speed_ms,
            total_weight_kg=total_kg,
            grade_percent=grade,
            terrain_factor=terrain,
            movement_phase=phase,
        )
    )


def _drain_pct_per_s(power_w: float, cp_w: float) -> float:
    # returns fraction of stamina bar per second (0-1 scale)
    return float(stamina_drain_rate_per_second_from_power_watts(power_w, cp_w))


def _cp_eff(cp0: float, load_kg: float, grade: float, fatigue_norm: float = 0.0) -> float:
    # Align docs §4.1
    cp = cp0 * (1.0 - 0.002 * max(0.0, load_kg - 10.0))
    if grade > 0.0:
        g = grade * 0.01
        cp = cp * (1.0 - 0.015 * (g * g))
    cp = cp * (1.0 - 0.18 * fatigue_norm)
    if cp < cp0 * 0.82:
        cp = cp0 * 0.82
    return cp


def _pct(new: float, old: float) -> float:
    if abs(old) < 1e-9:
        if abs(new) < 1e-9:
            return 0.0
        return 100.0
    return (new - old) / abs(old) * 100.0


def run_analysis(preset: str = "StandardMilsim") -> Dict:
    params = load_preset_params(preset)
    merged = merge_game_aligned_params(params)
    twin = RSSDigitalTwin(RSSConstants(**merged))
    cp0 = float(merged.get("critical_power_watts", params.get("critical_power_watts", 1860.0)))
    if hasattr(twin.constants, "CRITICAL_POWER_WATTS"):
        twin.constants.CRITICAL_POWER_WATTS = cp0
    # Prefer preset enc coeff if present on constants
    if "encumbrance_speed_penalty_coeff" in merged:
        twin.constants.ENCUMBRANCE_SPEED_PENALTY_COEFF = float(
            merged["encumbrance_speed_penalty_coeff"]
        )
    elif "encumbrance_speed_penalty_coeff" in params:
        twin.constants.ENCUMBRANCE_SPEED_PENALTY_COEFF = float(
            params["encumbrance_speed_penalty_coeff"]
        )

    # Baseline scenario
    load0 = 30.0
    grade0 = 0.0
    terrain0 = 1.0
    sta0 = 1.0
    phase0 = int(MovementType.RUN)
    fatigue0 = 0.0
    speed_cmd0 = 0.0  # filled below

    enc0 = _enc_pen(twin, load0)
    speed_cmd0 = _abs_ms(twin, sta0, phase0, enc0)
    total0 = BODY_KG + load0
    cp0_eff = _cp_eff(cp0, load0, grade0, fatigue0)
    p0 = _power(speed_cmd0, total0, grade0, terrain0, phase0)
    d0 = _drain_pct_per_s(p0, cp0_eff)

    # Cruise when W' empty (latched): ask cap at commanded speed
    cruise0 = get_metabolic_speed_cap_ms(
        current_speed_ms=speed_cmd0,
        movement_phase=phase0,
        total_weight_kg=total0,
        grade_percent=grade0,
        terrain_factor=terrain0,
        is_exhausted=False,
        effective_cp_watts=cp0_eff,
        w_prime_pool01=0.0,
        cruise_latched=True,
    )

    baseline = {
        "preset": preset,
        "load_kg": load0,
        "grade_pct": grade0,
        "terrain": terrain0,
        "stamina": sta0,
        "phase": "RUN",
        "enc_penalty": enc0,
        "cmd_speed_ms": speed_cmd0,
        "power_w": p0,
        "cp_eff_w": cp0_eff,
        "drain_frac_per_s": d0,
        "drain_pct_per_s": d0 * 100.0,
        "time_to_empty_s": (1.0 / d0) if d0 > 1e-6 else None,
        "cruise_cap_ms_wprime_empty": cruise0,
    }

    hits: List[FactorHit] = []

    def add_speed(factor, zh, new_ms, detail):
        hits.append(
            FactorHit(
                factor,
                zh,
                "speed",
                _pct(new_ms, speed_cmd0),
                speed_cmd0,
                new_ms,
                detail,
            )
        )

    def add_drain(factor, zh, new_d, detail):
        hits.append(
            FactorHit(
                factor,
                zh,
                "drain",
                _pct(new_d, d0),
                d0 * 100.0,
                new_d * 100.0,
                detail,
            )
        )

    def add_power(factor, zh, new_p, detail):
        hits.append(
            FactorHit(
                factor,
                zh,
                "power",
                _pct(new_p, p0),
                p0,
                new_p,
                detail,
            )
        )

    # --- Speed factors (command path: STA + enc + phase) ---
    for sta, label in ((0.50, "STA 50%"), (0.20, "STA 20%"), (0.05, "STA 5%跛行点"), (0.02, "STA 2%跛行")):
        add_speed(
            f"stamina_{sta}",
            f"体力降至{label}",
            _abs_ms(twin, sta, phase0, enc0),
            f"phase=RUN enc={enc0:.3f}",
        )

    for load, label in ((0.0, "空载"), (15.0, "15kg"), (45.0, "45kg"), (60.0, "60kg")):
        enc = _enc_pen(twin, load)
        add_speed(
            f"load_speed_{load}",
            f"负重改速度({label})",
            _abs_ms(twin, sta0, phase0, enc),
            f"enc_penalty={enc:.3f}",
        )

    for phase, pname in (
        (MovementType.WALK, "Walk"),
        (MovementType.SPRINT, "Sprint"),
    ):
        add_speed(
            f"phase_{pname}",
            f"步态改{pname}",
            _abs_ms(twin, sta0, phase, enc0),
            f"STA=100% enc={enc0:.3f}",
        )

    # Combined: low STA + heavy
    enc60 = _enc_pen(twin, 60.0)
    add_speed(
        "combo_sta5_load60",
        "组合: STA5%+60kg",
        _abs_ms(twin, 0.05, phase0, enc60),
        "worst command stack",
    )

    # --- Drain / power factors (at baseline command speed) ---
    for load, label in ((0.0, "空载"), (15.0, "15kg"), (45.0, "45kg"), (60.0, "60kg")):
        total = BODY_KG + load
        # keep same command speed for fair drain compare? Better: use that load's cmd speed
        enc = _enc_pen(twin, load)
        v = _abs_ms(twin, sta0, phase0, enc)
        cp = _cp_eff(cp0, load, grade0, fatigue0)
        p = _power(v, total, grade0, terrain0, phase0)
        d = _drain_pct_per_s(p, cp)
        add_power(f"load_power_{load}", f"负重改功率({label})", p, f"v={v:.2f} CP={cp:.0f}")
        add_drain(f"load_drain_{load}", f"负重改消耗({label})", d, f"v={v:.2f} P={p:.0f}W")

    for grade, label in ((5.0, "5%坡"), (10.0, "10%坡"), (15.0, "15%坡"), (20.0, "20%坡")):
        cp = _cp_eff(cp0, load0, grade, fatigue0)
        p = _power(speed_cmd0, total0, grade, terrain0, phase0)
        d = _drain_pct_per_s(p, cp)
        add_power(f"grade_power_{grade}", f"坡度改功率({label})", p, f"v={speed_cmd0:.2f}")
        add_drain(f"grade_drain_{grade}", f"坡度改消耗({label})", d, f"CP_eff={cp:.0f}")

    for terr, label in ((1.2, "软土1.2"), (1.5, "泥沼1.5"), (2.0, "极难2.0")):
        p = _power(speed_cmd0, total0, grade0, terr, phase0)
        d = _drain_pct_per_s(p, cp0_eff)
        add_power(f"terrain_power_{terr}", f"地形改功率({label})", p, f"v={speed_cmd0:.2f}")
        add_drain(f"terrain_drain_{terr}", f"地形改消耗({label})", d, "")

    # Heat: envCpMult reduces CP → more of P is above CP → higher aerobic clamp still at CP
    # but W' burns; for aerobic drain rate, P capped at CP so lower CP = lower aerobic drain
    # Actually drain uses min(P,CP)*coeff — lower CP reduces aerobic drain but increases W' use.
    # Report both aerobic drain and excess over CP.
    for heat_pen, label in ((0.15, "热应激中"), (0.30, "热应激峰")):
        env_cp = 1.0 - heat_pen * 0.35
        cp = cp0_eff * env_cp
        p = p0
        d = _drain_pct_per_s(p, cp)
        excess = max(p - cp, 0.0)
        add_drain(
            f"heat_drain_{heat_pen}",
            f"热应激改有氧消耗({label})",
            d,
            f"envCpMult={env_cp:.3f} excessW={excess:.0f}",
        )

    for fat, label in ((0.3, "疲劳0.3"), (0.6, "疲劳0.6"), (1.0, "疲劳满")):
        cp = _cp_eff(cp0, load0, grade0, fat)
        d = _drain_pct_per_s(p0, cp)
        add_drain(
            f"fatigue_drain_{fat}",
            f"累积疲劳改消耗({label})",
            d,
            f"CP_eff={cp:.0f}",
        )

    # Speed itself: +0.5 / +1.0 m/s overshoot (same load)
    for dv, label in ((0.5, "+0.5m/s"), (1.0, "+1.0m/s"), (-0.5, "-0.5m/s")):
        v = max(0.5, speed_cmd0 + dv)
        p = _power(v, total0, grade0, terrain0, phase0)
        d = _drain_pct_per_s(p, cp0_eff)
        add_power(f"speed_power_{dv}", f"速度扰动改功率({label})", p, f"v={v:.2f}")
        add_drain(f"speed_drain_{dv}", f"速度扰动改消耗({label})", d, f"v={v:.2f}")

    # Grade effect on cruise cap (W' empty)
    cruise_hits = []
    for grade, label in ((0.0, "平地"), (5.0, "5%"), (10.0, "10%"), (15.0, "15%")):
        cp = _cp_eff(cp0, load0, grade, fatigue0)
        cap = get_metabolic_speed_cap_ms(
            current_speed_ms=speed_cmd0,
            movement_phase=phase0,
            total_weight_kg=total0,
            grade_percent=grade,
            terrain_factor=terrain0,
            is_exhausted=False,
            effective_cp_watts=cp,
            w_prime_pool01=0.0,
            cruise_latched=True,
        )
        cruise_hits.append(
            {
                "label": label,
                "grade": grade,
                "cp_eff": cp,
                "cruise_ms": cap,
                "delta_pct_vs_flat": _pct(cap, cruise0) if cruise0 > 0 else None,
            }
        )

    # Rank by |delta| within axes
    def top(axis: str, n: int = 8) -> List[FactorHit]:
        subset = [h for h in hits if h.axis == axis]
        subset.sort(key=lambda h: abs(h.delta_pct), reverse=True)
        return subset[:n]

    return {
        "baseline": baseline,
        "top_speed": [asdict(h) for h in top("speed")],
        "top_drain": [asdict(h) for h in top("drain")],
        "top_power": [asdict(h) for h in top("power")],
        "cruise_by_grade": cruise_hits,
        "all": [asdict(h) for h in hits],
    }


def _print_report(data: Dict) -> None:
    b = data["baseline"]
    print("=== Baseline (StandardMilsim twin) ===")
    print(
        f"  load={b['load_kg']}kg  grade={b['grade_pct']}%  terrain={b['terrain']}  "
        f"STA={b['stamina']*100:.0f}%  phase={b['phase']}"
    )
    print(
        f"  enc_penalty={b['enc_penalty']:.3f}  cmd_speed={b['cmd_speed_ms']:.3f} m/s"
    )
    print(
        f"  P={b['power_w']:.0f} W  CP_eff={b['cp_eff_w']:.0f} W  "
        f"drain={b['drain_pct_per_s']:.2f} %/s"
    )
    if b["time_to_empty_s"] is not None:
        print(f"  time_to_empty≈{b['time_to_empty_s']:.0f} s (at this cmd speed)")
    print(f"  W'-empty cruise_cap={b['cruise_cap_ms_wprime_empty']} m/s")
    print()

    def dump(title: str, rows: List[Dict], unit: str) -> None:
        print(f"=== {title} (by |Δ%|) ===")
        print(f"{'factor':<28} {'Δ%':>8} {'base':>10} {'new':>10}  detail")
        print("-" * 78)
        for h in rows:
            print(
                f"{h['label_zh']:<28} {h['delta_pct']:>7.1f}% "
                f"{h['baseline']:>10.3f} {h['perturbed']:>10.3f}  {h['detail']}"
            )
        print()

    dump("对指令速度影响最大", data["top_speed"], "m/s")
    dump("对有氧消耗(%/s)影响最大", data["top_drain"], "%/s")
    dump("对代谢功率(W)影响最大", data["top_power"], "W")

    print("=== W'耗尽后 CP 巡航速 vs 坡度 (30kg Run) ===")
    for c in data["cruise_by_grade"]:
        print(
            f"  {c['label']:<6} grade={c['grade']:>4.0f}%  CP={c['cp_eff']:.0f}W  "
            f"cruise={c['cruise_ms']} m/s  Δvs平地={c['delta_pct_vs_flat']}"
        )
    print()
    print("=== 结论摘要 ===")
    print("  指令速度(玩家感到的限速): 主要看 步态相位 + 负重惩罚；STA 仅在 <5% 跛行区才大幅压速。")
    print("  体力油耗: 速度本身与坡度/地形/负重抬功率；CP 被疲劳/热应激压低时有氧记账变少但更易烧 W'。")
    print("  持续奔跑: W' 空后的巡航帽对坡度很敏感（代谢反解）。")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Offline stamina-speed sensitivity (v6 twin)")
    parser.add_argument("--preset", default="StandardMilsim")
    parser.add_argument("--json", default="")
    args = parser.parse_args(argv)

    data = run_analysis(args.preset)
    _print_report(data)

    if args.json:
        out = Path(args.json)
        if not out.is_absolute():
            out = ROOT / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
