#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""诊断 Elite @ 35kg 冲刺时长：闭环 vs 开环 vs 环境/负重敏感度。"""
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
)

TOOLS_DIR = os.path.dirname(__file__)
PRESET_FILE = "optimized_rss_config_elitestandard_v4.json"


def load_params():
    path = os.path.join(TOOLS_DIR, PRESET_FILE)
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return {k: float(v) for k, v in data.items() if not k.startswith("_")}


def simulate_sprint(
    load_kg=35.0,
    duration_s=600.0,
    speed_mode="closed",
    fixed_speed=4.8,
    heat_mult=1.0,
    terrain_factor=1.0,
):
    params = load_params()
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    twin.reset()
    cw = 90.0 + load_kg
    dt = STAMINA_TICK_SEC
    t = 0.0
    last_speed = 0.0
    thresh = params.get("sprint_enable_threshold", 0.25)
    will = params.get("willpower_threshold", 0.35)
    events = {}

    while t <= duration_s:
        if speed_mode == "closed":
            speed = twin.calculate_actual_speed(
                twin.stamina, cw, MovementType.SPRINT, last_speed, 0.0, t
            )
        else:
            speed = fixed_speed

        for key, val in [("willpower", will), ("sprint_cut", thresh), ("25pct", 0.25), ("15pct", 0.15)]:
            if key not in events and twin.stamina <= val:
                events[key] = (t, twin.stamina, speed)

        base_for_rec, total_drain = twin._calculate_drain_rate_c_aligned(
            speed, cw, 0.0, terrain_factor, 0, MovementType.SPRINT
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
            speed,
            movement_type=MovementType.SPRINT,
        )
        recovery_rate = min(max(float(recovery_rate), 0.0), 0.01)
        net = recovery_rate - total_drain
        tick_scale = dt / STAMINA_TICK_SEC
        twin.stamina = max(0.0, min(1.0, twin.stamina + net * tick_scale))
        twin.exercise_duration_minutes += dt / 60.0
        twin.last_speed = speed
        last_speed = speed
        t += dt
        if twin.stamina <= 0.001:
            events["empty"] = (t, twin.stamina, speed)
            break

    return events


def fmt_event(events, key):
    if key not in events:
        return "-"
    sec, stam, spd = events[key]
    return f"{int(sec // 60)}m{int(sec % 60):02d}s ({sec:.0f}s) @ {stam * 100:.1f}% spd={spd:.2f}"


def main():
    print("EliteStandard @ 35kg 冲刺诊断")
    print("=" * 72)

    events = simulate_sprint(speed_mode="closed")
    print("\n[闭环] 速度随体力/负重动态变化（最接近游戏逻辑）")
    print(f"  意志力平台期 (willpower {load_params()['willpower_threshold']:.2f}): {fmt_event(events, 'willpower')}")
    print(f"  禁止冲刺 (sprint_enable {load_params()['sprint_enable_threshold']:.2f}): {fmt_event(events, 'sprint_cut')}")
    print(f"  降至 25%: {fmt_event(events, '25pct')}")
    print(f"  降至 15%: {fmt_event(events, '15pct')}")

    print("\n[开环] 固定速度（旧 bench 方法，偏高估）")
    for spd in [4.2, 4.8, 5.0, 5.2, 5.5]:
        ev = simulate_sprint(speed_mode="fixed", fixed_speed=spd)
        print(f"  {spd:.1f} m/s → 禁止冲刺: {fmt_event(ev, 'sprint_cut')}")

    print("\n[闭环 + 热应激乘数] drain × heat_mult")
    for heat in [1.0, 1.2, 1.3, 1.5]:
        ev = simulate_sprint(speed_mode="closed", heat_mult=heat)
        print(f"  ×{heat:.1f} → 禁止冲刺: {fmt_event(ev, 'sprint_cut')}")

    print("\n[闭环 + 地形系数]")
    for terrain in [1.0, 1.1, 1.2, 1.3]:
        ev = simulate_sprint(speed_mode="closed", terrain_factor=terrain)
        print(f"  terrain={terrain:.1f} → 禁止冲刺: {fmt_event(ev, 'sprint_cut')}")

    print("\n[闭环 + 更高负重]")
    for load in [35, 40, 45, 50]:
        ev = simulate_sprint(load_kg=load, speed_mode="closed")
        print(f"  {load}kg → 禁止冲刺: {fmt_event(ev, 'sprint_cut')}")

    print("\n说明: 玩家体感「只能冲刺约 1 分钟」通常对应 sprint_enable_threshold (~23%)")
    print("      或意志力平台期 (~35%)，而非完全耗尽。")
    print("=" * 72)


if __name__ == "__main__":
    main()
