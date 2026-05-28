#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三档预设 @ 35kg 冲刺时长诊断：对比多种速度假设，找出与实测 ~2min 对齐的条件。"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    merge_game_aligned_params,
    MovementType,
    STAMINA_TICK_SEC,
    sprint_speed_at_weight,
    run_speed_at_weight,
)

TOOLS_DIR = os.path.dirname(__file__)
PRESETS = {
    "EliteStandard": "optimized_rss_config_elitestandard_v4.json",
    "StandardMilsim": "optimized_rss_config_standardmilsim_v4.json",
    "TacticalAction": "optimized_rss_config_tacticalaction_v4.json",
}
LOAD_KG = 35.0


def load_params(filename):
    path = os.path.join(TOOLS_DIR, filename)
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return {k: float(v) for k, v in data.items() if not k.startswith("_")}


def sprint_sim(params, mode="closed", load_kg=LOAD_KG, heat_mult=1.0, terrain=1.0, dt=0.2):
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    twin.reset()
    cw = 90.0 + load_kg
    t = 0.0
    last = 0.0
    sprint_start = 0.0
    thresh = params.get("sprint_enable_threshold", 0.25)
    will = params.get("willpower_threshold", 0.35)
    max_sprint = sprint_speed_at_weight(twin.constants, cw)
    max_run = run_speed_at_weight(twin.constants, cw)
    events = {}

    while t <= 600.0:
        if mode == "closed":
            twin._sprint_start_time = sprint_start
            spd = twin.calculate_actual_speed(twin.stamina, cw, MovementType.SPRINT, last, 0.0, t)
        elif mode == "fixed55":
            spd = 5.5
        elif mode == "max_sprint":
            spd = max_sprint
        elif mode == "engine_overshoot":
            calc = twin.calculate_actual_speed(twin.stamina, cw, MovementType.SPRINT, last, 0.0, t)
            spd = min(5.5, max(calc, max_sprint * 1.05))
        elif mode == "burst55":
            burst = getattr(twin.constants, "TACTICAL_SPRINT_BURST_DURATION", 8.0)
            if t < burst:
                spd = 5.5
            else:
                spd = twin.calculate_actual_speed(twin.stamina, cw, MovementType.SPRINT, last, 0.0, t)
        else:
            spd = float(mode)

        for key, val in [("will", will), ("cut", thresh)]:
            if key not in events and twin.stamina <= val:
                events[key] = t

        base_for_rec, total_drain = twin._calculate_drain_rate_c_aligned(
            spd, cw, 0.0, terrain, 0, MovementType.SPRINT
        )
        total_drain = min(float(total_drain) * heat_mult, 0.1)
        recovery_rate = twin._calculate_recovery_rate(
            twin.stamina,
            twin.rest_duration_minutes,
            twin.exercise_duration_minutes,
            cw,
            base_for_rec,
            False,
            0,
            twin.environment_factor,
            spd,
            movement_type=MovementType.SPRINT,
        )
        recovery_rate = min(max(float(recovery_rate), 0.0), 0.01)
        net = recovery_rate - total_drain
        tick_scale = dt / STAMINA_TICK_SEC
        twin.stamina = max(0.0, min(1.0, twin.stamina + net * tick_scale))
        twin.exercise_duration_minutes += dt / 60.0
        twin.last_speed = spd
        if twin.stamina < 1.0:
            twin.stamina_history.append(twin.stamina)

        last = spd
        t += dt
        if twin.stamina <= 0.001:
            break

    return events, max_sprint, max_run


def fmt_sec(s):
    if s is None:
        return "-"
    return f"{int(s // 60)}m{int(s % 60):02d}s"


def main():
    print(f"三档预设 @ {LOAD_KG:.0f}kg 冲刺诊断（禁止冲刺 = sprint_enable_threshold）")
    print("=" * 88)

    modes = [
        ("closed", "闭环(RSS理论速度)"),
        ("max_sprint", "开环@满体力冲刺速度"),
        ("fixed55", "开环@5.5m/s引擎上限"),
        ("burst55", "前8s@5.5m/s后闭环"),
        ("engine_overshoot", "闭环×1.05 capped 5.5"),
    ]

    for preset, fname in PRESETS.items():
        params = load_params(fname)
        print(f"\n## {preset}")
        print(f"  sprint_enable={params['sprint_enable_threshold']:.3f}  willpower={params['willpower_threshold']:.3f}")
        _, max_sp, max_run = sprint_sim(params, mode="closed")
        print(f"  满体力理论: run={max_run:.2f} m/s  sprint={max_sp:.2f} m/s")
        print(f"  {'模式':<22} {'禁止冲刺':>10} {'意志力':>10}")
        for mode_key, mode_label in modes:
            ev, _, _ = sprint_sim(params, mode=mode_key)
            print(f"  {mode_label:<22} {fmt_sec(ev.get('cut')):>10} {fmt_sec(ev.get('will')):>10}")

        print("  环境叠加 (闭环):")
        for heat, terr in [(1.3, 1.0), (1.5, 1.0), (1.3, 1.2), (1.5, 1.2)]:
            ev, _, _ = sprint_sim(params, mode="closed", heat_mult=heat, terrain=terr)
            print(f"    heat×{heat} terrain×{terr} → cut={fmt_sec(ev.get('cut'))} will={fmt_sec(ev.get('will'))}")

    print("\n" + "=" * 88)
    print("若实测三档均在 ~2min 内无法冲刺，最可能原因：")
    print("  1) 引擎测速(GetVelocity)接近 5.5 m/s，高于 RSS 理论冲刺速度")
    print("  2) 热应激/地形系数在 Everon 等地图常态 >1.2")
    print("  3) 数字孪生闭环低估了爆发期实际速度")
    print("=" * 88)


if __name__ == "__main__":
    main()
