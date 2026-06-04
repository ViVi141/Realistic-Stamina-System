#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RSS v6 体力收支诊断图：暴露消耗/恢复/HUD ETA 与仿真之间的矛盾。

对齐 SCR_RSS_Settings.InitEliteStandardDefaults + SCR_RSS_MetabolismModel + UpdateCoordinator。
输出：tools/output/stamina_balance_diagnostic.png
"""

from __future__ import annotations

import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))

from rss_digital_twin_fix import (
    RSSConstants,
    RSSDigitalTwin,
    Stance,
    MovementType,
    merge_game_aligned_params,
    metabolism_power_watts,
    V6_STAMINA_DRAIN_CALIBRATION,
)
ELITE_PARAMS = {
    "energy_to_stamina_coeff": 7.173939269261512e-07,
    "base_recovery_rate": 1.5347845665467625e-04,
    "standing_recovery_multiplier": 1.1033940520181997,
    "crouching_recovery_multiplier": 1.6,
    "prone_recovery_multiplier": 2.3436692109309787,
    "load_recovery_penalty_coeff": 5.401551119196543e-05,
    "load_recovery_penalty_exponent": 2.0,
    "encumbrance_stamina_drain_coeff": 1.9633666302334787,
    "recovery_nonlinear_coeff": 0.7916386681694456,
    "fast_recovery_multiplier": 2.395424393975942,
    "medium_recovery_multiplier": 1.1374814244785123,
    "slow_recovery_multiplier": 0.4756093184784932,
    "max_recovery_per_tick": 5.831263335868464e-04,
    "min_recovery_stamina_threshold": 0.2,
    "min_recovery_rest_time_seconds": 3.0,
    "marginal_decay_threshold": 0.8,
    "marginal_decay_coeff": 1.1,
}

BODY_KG = 90.0
LOAD_KG = 30.0
TOTAL_KG = BODY_KG + LOAD_KG
TICK_SEC = 0.2
PLAYER_TICK = 0.017

V6_ACSM_REST_W = 50.0
V6_ACSM_LINEAR = 200.0
V6_ACSM_QUAD = 80.0
V6_BLEND_START = 2.0
V6_BLEND_END = 2.4
REF_WEIGHT = 90.0
PANDOLF_BASE = 2.7
PANDOLF_VCOEFF = 3.2
PANDOLF_VOFF = 0.7
PANDOLF_GB = 0.23
PANDOLF_GV = 1.34
FITNESS_BONUS = 0.80


def acsm_power_w(velocity_ms: float, total_kg: float) -> float:
    v = max(0.0, velocity_ms)
    mass_scale = max(total_kg, 45.0) / REF_WEIGHT
    pref = V6_ACSM_REST_W + V6_ACSM_LINEAR * v + V6_ACSM_QUAD * v * v
    return max(pref * mass_scale, 0.0)


def pandolf_power_w(
    velocity_ms: float,
    total_kg: float,
    grade_percent: float = 0.0,
    terrain_factor: float = 1.0,
) -> float:
    v = max(0.0, velocity_ms)
    if v < 0.1:
        return 100.0 * (total_kg / REF_WEIGHT)
    vt = v - PANDOLF_VOFF
    base_term = (PANDOLF_BASE * FITNESS_BONUS) + (PANDOLF_VCOEFF * vt * vt)
    g = grade_percent * 0.01
    grade_term = g * (PANDOLF_GB + PANDOLF_GV * v * v)
    grade_term = min(grade_term, base_term * 3.0)
    w_mult = max(total_kg / REF_WEIGHT, 0.1)
    terrain_factor = float(np.clip(terrain_factor, 0.5, 3.0))
    return max(w_mult * (base_term + grade_term) * terrain_factor * REF_WEIGHT, 0.0)


def metabolism_power_c(
    velocity_ms: float,
    total_kg: float,
    movement_phase: int = 2,
    grade_percent: float = 0.0,
    terrain_factor: float = 1.0,
) -> float:
    """与 SCR_RSS_MetabolismModel.MetabolismPowerWatts 同形。"""
    pandolf_w = pandolf_power_w(velocity_ms, total_kg, grade_percent, terrain_factor)
    prefer_acsm = movement_phase in (2, 3) or velocity_ms >= V6_BLEND_END
    if not prefer_acsm and velocity_ms < V6_BLEND_START:
        return pandolf_w
    acsm_w = acsm_power_w(velocity_ms, total_kg)
    if velocity_ms <= V6_BLEND_START:
        blend = 0.0
    elif velocity_ms >= V6_BLEND_END:
        blend = 1.0
    else:
        blend = (velocity_ms - V6_BLEND_START) / (V6_BLEND_END - V6_BLEND_START)
    if movement_phase == 3:
        blend = max(blend, 0.85)
    return pandolf_w * (1.0 - blend) + acsm_w * blend


def power_to_drain_per_tick(power_w: float, coeff: float) -> float:
    per_sec = max(power_w * coeff * V6_STAMINA_DRAIN_CALIBRATION, 0.0)
    return per_sec * TICK_SEC


def build_twin_constants() -> RSSConstants:
    merged = merge_game_aligned_params(ELITE_PARAMS)
    c = RSSConstants()
    for key, val in merged.items():
        attr = key.upper()
        if hasattr(c, attr):
            setattr(c, attr, val)
    c.ENERGY_TO_STAMINA_COEFF = merged["energy_to_stamina_coeff"]
    c.BASE_RECOVERY_RATE = merged["base_recovery_rate"]
    c.RECOVERY_NONLINEAR_COEFF = merged["recovery_nonlinear_coeff"]
    c.STANDING_RECOVERY_MULTIPLIER = merged["standing_recovery_multiplier"]
    c.CROUCHING_RECOVERY_MULTIPLIER = merged["crouching_recovery_multiplier"]
    c.PRONE_RECOVERY_MULTIPLIER = merged["prone_recovery_multiplier"]
    c.LOAD_RECOVERY_PENALTY_COEFF = merged["load_recovery_penalty_coeff"]
    c.LOAD_RECOVERY_PENALTY_EXPONENT = merged["load_recovery_penalty_exponent"]
    c.ENCUMBRANCE_STAMINA_DRAIN_COEFF = merged["encumbrance_stamina_drain_coeff"]
    c.FAST_RECOVERY_MULTIPLIER = merged["fast_recovery_multiplier"]
    c.MEDIUM_RECOVERY_MULTIPLIER = merged["medium_recovery_multiplier"]
    c.SLOW_RECOVERY_MULTIPLIER = merged["slow_recovery_multiplier"]
    c.MAX_RECOVERY_PER_TICK = merged["max_recovery_per_tick"]
    c.MIN_RECOVERY_STAMINA_THRESHOLD = merged["min_recovery_stamina_threshold"]
    c.MIN_RECOVERY_REST_TIME_SECONDS = merged["min_recovery_rest_time_seconds"]
    c.MARGINAL_DECAY_THRESHOLD = merged["marginal_decay_threshold"]
    c.MARGINAL_DECAY_COEFF = merged["marginal_decay_coeff"]
    return c


def recovery_per_tick(
    twin: RSSDigitalTwin,
    stamina: float,
    rest_min: float,
    exercise_min: float,
    speed: float,
    base_drain_for_rec: float,
) -> float:
    return twin._calculate_recovery_rate(
        stamina,
        rest_min,
        exercise_min,
        TOTAL_KG,
        base_drain_for_rec,
        False,
        Stance.STAND,
        twin.environment_factor,
        speed,
        MovementType.RUN,
    )


def v6_gross_drain_per_tick(speed: float, phase: int = 2) -> float:
    coeff = ELITE_PARAMS["energy_to_stamina_coeff"]
    pw = metabolism_power_c(speed, TOTAL_KG, phase)
    return power_to_drain_per_tick(pw, coeff)


def twin_legacy_drain_per_tick(twin: RSSDigitalTwin, speed: float) -> float:
    _, total = twin._calculate_drain_rate_c_aligned_legacy_pandolf(
        speed,
        TOTAL_KG,
        0.0,
        1.0,
        Stance.STAND,
        MovementType.RUN,
    )
    return total


def hud_eta_deplete_sec(stamina: float, drain_per_sec: float, recovery_per_sec: float) -> float:
    """与 ComputeStaminaEta 净逻辑一致。"""
    net_loss = drain_per_sec - recovery_per_sec
    if net_loss <= 1e-9:
        return float("inf")
    return min(stamina / net_loss, 7200.0)


def simulate_constant_run(
    twin: RSSDigitalTwin,
    speed: float,
    duration_s: float,
    rest_min_at_start: float,
    exercise_min_at_start: float,
) -> dict:
    """逐步积分 net = recovery - movement_drain（v6 陆地：enc_mult=1，无 EPOC）。"""
    sta = 1.0
    times = [0.0]
    stas = [sta]
    nets = []
    drains = []
    recovs = []
    t = 0.0
    rest_min = rest_min_at_start
    exercise_min = exercise_min_at_start
    dt = PLAYER_TICK
    coeff = ELITE_PARAMS["energy_to_stamina_coeff"]

    while t < duration_s:
        pw = metabolism_power_c(speed, TOTAL_KG, 2)
        gross = power_to_drain_per_tick(pw, coeff)
        base_for_rec = gross
        if speed >= 0.1:
            movement_drain = gross
        else:
            movement_drain = 0.0

        rec = recovery_per_tick(twin, sta, rest_min, exercise_min, speed, base_for_rec)
        rec = min(rec, twin.constants.MAX_RECOVERY_PER_TICK)
        net_tick = rec - movement_drain
        scale = dt / TICK_SEC
        sta = float(np.clip(sta + net_tick * scale, 0.0, 1.0))

        nets.append(net_tick * 5.0)
        drains.append(movement_drain * 5.0)
        recovs.append(rec * 5.0)
        t += dt
        times.append(t)
        stas.append(sta)
        exercise_min += dt / 60.0
        if speed < 0.1:
            rest_min += dt / 60.0
        else:
            rest_min = 0.0

    return {
        "times": np.array(times),
        "stas": np.array(stas),
        "nets": np.array(nets),
        "drains": np.array(drains),
        "recovs": np.array(recovs),
    }


def main() -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("需要 matplotlib：pip install matplotlib")
        sys.exit(1)

    out_dir = os.path.join(os.path.dirname(__file__), "output")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "stamina_balance_diagnostic.png")

    twin = RSSDigitalTwin(build_twin_constants())
    coeff = ELITE_PARAMS["energy_to_stamina_coeff"]
    speeds = np.linspace(0.0, 5.5, 120)

    gross_v6 = []
    gross_twin = []
    recovery_run = []
    recovery_rest = []
    net_run = []
    hud_eta_min = []

    for v in speeds:
        if v < 0.1:
            gross_v6.append(0.0)
            gross_twin.append(0.0)
            recovery_run.append(0.0)
            recovery_rest.append(
                recovery_per_tick(twin, 1.0, 5.0, 0.0, 0.0, -0.0025 * TICK_SEC) * 5.0
            )
            net_run.append(0.0)
            hud_eta_min.append(float("nan"))
            continue

        g6 = v6_gross_drain_per_tick(v) * 5.0
        gt = twin_legacy_drain_per_tick(twin, v) * 5.0
        rec_ex = recovery_per_tick(twin, 1.0, 0.0, 10.0, v, g6 / 5.0) * 5.0
        rec_st = recovery_per_tick(twin, 1.0, 5.0, 0.0, 0.0, g6 / 5.0) * 5.0
        gross_v6.append(g6)
        gross_twin.append(gt)
        recovery_run.append(rec_ex)
        recovery_rest.append(rec_st)
        net_run.append(rec_ex - g6)
        hud_eta_min.append(hud_eta_deplete_sec(1.0, g6, rec_ex) / 60.0)

    sim = simulate_constant_run(twin, 3.2, 600.0, 0.0, 10.0)
    sta = 1.0
    net_at_32 = recovery_per_tick(twin, sta, 0.0, 10.0, 3.2, v6_gross_drain_per_tick(3.2)) * 5.0
    gross_at_32 = v6_gross_drain_per_tick(3.2) * 5.0
    hud_32_min = hud_eta_deplete_sec(1.0, gross_at_32, net_at_32) / 60.0
    sim_deplete_min = None
    for i, s in enumerate(sim["stas"]):
        if s <= 0.05:
            sim_deplete_min = sim["times"][i] / 60.0
            break
    if sim_deplete_min is None:
        if sim["stas"][-1] > sim["stas"][0]:
            sim_deplete_min = float("inf")
        else:
            sim_deplete_min = 600.0 / 60.0

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(
        f"RSS v6 EliteStandard balance audit | load {LOAD_KG}kg | total {TOTAL_KG}kg",
        fontsize=13,
        fontweight="bold",
    )

    ax = axes[0, 0]
    ax.plot(speeds, gross_v6, "r-", linewidth=2, label="v6 gross drain (MetabolismModel)")
    ax.plot(speeds, gross_twin, "r--", linewidth=1.5, label="twin Pandolf+enc_mult (legacy)")
    ax.plot(speeds, recovery_run, "g-", linewidth=2, label="recovery (moving rest=0 ex=10min)")
    ax.plot(speeds, recovery_rest, "g--", linewidth=1.2, label="recovery (idle rest=5min)")
    ax.plot(speeds, net_run, "b-", linewidth=2, label="net = recovery - gross drain")
    ax.axhline(0.0, color="k", linewidth=0.8, alpha=0.4)
    ax.axvline(3.2, color="gray", linestyle=":", alpha=0.7, label="jog 3.2 m/s")
    cross = []
    for i in range(1, len(speeds)):
        if net_run[i - 1] < 0 and net_run[i] >= 0:
            cross.append(speeds[i])
        if net_run[i - 1] >= 0 and net_run[i] < 0:
            cross.append(speeds[i])
    if cross:
        ax.axvline(cross[0], color="purple", linestyle="--", alpha=0.8,
                   label=f"net zero ~ {cross[0]:.2f} m/s")
    ax.set_xlabel("speed (m/s)")
    ax.set_ylabel("STA rate (%/s)")
    ax.set_title("Fig1: speed sweep - drain vs recovery vs net")
    ax.legend(fontsize=7, loc="upper left")
    ax.grid(True, alpha=0.3)

    ax = axes[0, 1]
    ax2 = ax.twinx()
    ax.plot(speeds, np.array(hud_eta_min), "orange", linewidth=2, label="HUD ETA (net, post-fix)")
    net_eta_min = []
    for i, v in enumerate(speeds):
        if v < 0.1 or net_run[i] <= 1e-9:
            net_eta_min.append(float("nan"))
        else:
            net_eta_min.append((1.0 / net_run[i]) / 60.0)
    ax.plot(speeds, net_eta_min, "b-", linewidth=1.5, label="true net deplete ETA")
    ax2.plot(speeds, gross_v6, "r:", alpha=0.5, label="gross drain")
    ax.set_xlabel("speed (m/s)")
    ax.set_ylabel("ETA (min)")
    ax2.set_ylabel("gross drain (%/s)", color="red", alpha=0.6)
    ax.set_title("Fig2: HUD net ETA vs gross-drain ETA")
    ax.set_ylim(0, min(120, np.nanmax(hud_eta_min) * 1.2) if np.any(~np.isnan(hud_eta_min)) else 120)
    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax.legend(lines1 + lines2, labels1 + labels2, fontsize=7)
    ax.grid(True, alpha=0.3)

    ax = axes[1, 0]
    ax.plot(sim["times"] / 60.0, sim["stas"] * 100.0, "b-", linewidth=2)
    ax.set_xlabel("time (min)")
    ax.set_ylabel("STA (%)")
    ax.set_title(f"Fig3: constant 3.2 m/s x 10min | net={net_at_32 - gross_at_32:+.4f} %/s")
    ax.grid(True, alpha=0.3)
    trend = "UP" if sim["stas"][-1] > sim["stas"][0] + 0.01 else "DOWN"
    if abs(sim["stas"][-1] - sim["stas"][0]) < 0.01:
        trend = "FLAT"
    ax.text(
        0.02,
        0.05,
        f"100% -> {sim['stas'][-1]*100:.1f}% ({trend})\n"
        f"gross {gross_at_32:.4f}/s  rec {net_at_32:.4f}/s\n"
        f"HUD ETA {hud_32_min:.1f}min  sim deplete {sim_deplete_min}",
        transform=ax.transAxes,
        fontsize=9,
        verticalalignment="bottom",
        bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.8),
    )

    ax = axes[1, 1]
    labels = [
        "v6 gross /s",
        "moving recovery /s",
        "net /s",
        "HUD deplete (min)",
        "net ETA (min)",
        "twin legacy /s",
    ]
    net_val = net_at_32 - gross_at_32
    net_eta_32 = (1.0 / abs(net_val) / 60.0) if abs(net_val) > 1e-9 else float("inf")
    twin_g = twin_legacy_drain_per_tick(twin, 3.2) * 5.0
    values = [gross_at_32, net_at_32, net_val, hud_32_min, net_eta_32, twin_g]
    colors = ["#c0392b", "#27ae60", "#2980b9", "#e67e22", "#8e44ad", "#e74c3c"]
    bars = ax.barh(labels, values, color=colors, alpha=0.85)
    ax.set_xlabel("value (%/s or min)")
    ax.set_title("Fig4: key metrics @ 3.2 m/s")
    ax.grid(True, axis="x", alpha=0.3)
    for bar, val in zip(bars, values):
        if math.isfinite(val):
            ax.text(bar.get_width(), bar.get_y() + bar.get_height() / 2,
                    f" {val:.4f}", va="center", fontsize=8)

    plt.tight_layout()
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"已保存: {out_path}")
    print()
    print("=== @ 3.2 m/s, 30kg, EliteStandard ===")
    print(f"  v6 毛消耗:     {gross_at_32:.6f} %/s")
    print(f"  运动中恢复:   {net_at_32:.6f} %/s")
    print(f"  净变化:       {net_val:+.6f} %/s  → STA {'升' if net_val > 0 else '降'}")
    print(f"  HUD ETA(毛):  {hud_32_min:.1f} min")
    print(f"  净 ETA:       {net_eta_32:.1f} min" if math.isfinite(net_eta_32) else "  净 ETA:       ∞ (净恢复)")
    print(f"  孪生 legacy:  {twin_g:.6f} %/s  (Pandolf×enc_mult, 与 v6 不一致)")
    print(f"  10min 仿真:   {sim['stas'][-1]*100:.1f}%")


if __name__ == "__main__":
    main()
