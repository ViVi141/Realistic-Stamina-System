#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Standard 预设 @ 30kg：Walk / Run / Sprint 分别持续到 STA 耗尽。"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from rss_pipeline_v6 import load_preset_params
from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    merge_game_aligned_params,
    MovementType,
    Stance,
    STAMINA_TICK_SEC,
)

LOAD_KG = 30.0
PRESET = "StandardMilsim"
MAX_S = 6.0 * 3600.0  # 安全上限 6h
EMPTY_STA = 0.01


def fmt_t(sec: float) -> str:
    if sec is None or sec < 0:
        return "—"
    h = int(sec // 3600)
    m = int((sec % 3600) // 60)
    s = int(sec % 60)
    if h > 0:
        return f"{h}h{m:02d}m{s:02d}s"
    return f"{m:02d}:{s:02d}"


def pool01(twin: RSSDigitalTwin) -> float:
    cp = getattr(twin, "v6_cp_state", None)
    if cp is None:
        return -1.0
    return float(getattr(cp, "pool01", -1.0))


def armed(twin: RSSDigitalTwin) -> bool:
    cp = getattr(twin, "v6_cp_state", None)
    if cp is None:
        return False
    return bool(getattr(cp, "overspeed_armed", False))


def run_until_empty(label: str, movement: int, params: dict) -> dict:
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    twin.reset()
    twin.environment_factor.temperature = 20.0
    twin.environment_factor.wind_speed = 0.0
    twin.environment_factor.cold_stress = 0.0
    twin.environment_factor.cold_static_penalty = 0.0
    twin._scenario_wind_drag = 0.0

    dt = STAMINA_TICK_SEC
    weight = 90.0 + LOAD_KG
    t = 0.0
    v_sum = 0.0
    n = 0
    t_w25 = None
    t_w0 = None
    t_disarm = None
    t_sta50 = None
    t_sta25 = None
    t_sta15 = None
    t_empty = None
    sample = []
    sample_every = max(1, int(60.0 / dt))
    step = 0
    was_armed = True

    while t < MAX_S:
        v = twin.game_player_tick(
            movement,
            weight,
            0.0,
            1.0,
            Stance.STAND,
            t,
            dt,
            wind_drag=0.0,
            enable_randomness=False,
        )
        t += dt
        step += 1
        v_sum += v
        n += 1
        sta = twin.stamina
        w = pool01(twin)
        arm = armed(twin)

        if was_armed and not arm and t_disarm is None:
            t_disarm = t
        was_armed = arm

        if t_sta50 is None and sta <= 0.50:
            t_sta50 = t
        if t_sta25 is None and sta <= 0.25:
            t_sta25 = t
        if t_sta15 is None and sta <= 0.15:
            t_sta15 = t
        if t_w25 is None and w >= 0 and w <= 0.25:
            t_w25 = t
        if t_w0 is None and w >= 0 and w <= 0.005:
            t_w0 = t
        if t_empty is None and sta <= EMPTY_STA:
            t_empty = t
            sample.append((t, sta, w, arm, v))
            break

        if step % sample_every == 0 or step == 1:
            sample.append((t, sta, w, arm, v))

    return {
        "label": label,
        "t_empty": t_empty,
        "t_sta50": t_sta50,
        "t_sta25": t_sta25,
        "t_sta15": t_sta15,
        "t_disarm": t_disarm,
        "t_w25": t_w25,
        "t_w0": t_w0,
        "sta_end": twin.stamina,
        "w_end": pool01(twin),
        "armed_end": armed(twin),
        "v_avg": v_sum / max(n, 1),
        "hit_cap": t_empty is None,
        "samples": sample,
    }


def main() -> int:
    params = load_preset_params(PRESET)
    cases = [
        ("Walk", MovementType.WALK),
        ("Run", MovementType.RUN),
        ("Sprint", MovementType.SPRINT),
    ]

    print("=" * 72)
    print(
        f"预设={PRESET}  负重={LOAD_KG:.0f}kg  平路  "
        f"分别持续至 STA≤{EMPTY_STA*100:.0f}%（上限 {MAX_S/3600:.0f}h）"
    )
    print(
        f"CP0={params.get('critical_power_watts', 0):.0f}W  "
        f"W'max={params.get('w_prime_max_joules', 0):.0f}J  "
        f"e2s={params.get('energy_to_stamina_coeff', 0):.3e}"
    )
    print("=" * 72)

    results = []
    for label, mov in cases:
        print(f"\n>>> {label} 仿真中…")
        r = run_until_empty(label, mov, params)
        results.append(r)
        if r["hit_cap"]:
            status = f"未耗尽（跑满 {fmt_t(MAX_S)}） STA={r['sta_end']*100:.1f}%"
        else:
            status = f"耗尽 @ {fmt_t(r['t_empty'])}"
        print(
            f"[{label:6}] {status}  "
            f"v≈{r['v_avg']:.2f} m/s  "
            f"W'终={r['w_end']*100:.1f}% armed={r['armed_end']}"
        )
        print(
            f"         STA50% {fmt_t(r['t_sta50'])} | "
            f"STA25% {fmt_t(r['t_sta25'])} | "
            f"STA15% {fmt_t(r['t_sta15'])} | "
            f"解除武装 {fmt_t(r['t_disarm'])} | "
            f"W'≤25% {fmt_t(r['t_w25'])} | "
            f"W'≈0 {fmt_t(r['t_w0'])}"
        )

    print("\n" + "=" * 72)
    print(f"{'状态':8} {'耗尽':>12} {'STA50%':>10} {'STA25%':>10} "
          f"{'解除武装':>10} {'均速':>8} {'终W′%':>8}")
    for r in results:
        empty = "未耗尽" if r["hit_cap"] else fmt_t(r["t_empty"])
        print(
            f"{r['label']:8} {empty:>12} {fmt_t(r['t_sta50']):>10} "
            f"{fmt_t(r['t_sta25']):>10} {fmt_t(r['t_disarm']):>10} "
            f"{r['v_avg']:7.2f}m/s {r['w_end']*100:7.1f}%"
        )
    print("=" * 72)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
