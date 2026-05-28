#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""35kg 理想环境下三档预设移动数据（PlayerBase 同序数字孪生）。"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    merge_game_aligned_params,
    MovementType,
    Stance,
    RSS_PLAYER_TICK_SEC,
    STAMINA_TICK_SEC,
    walk_speed_at_weight,
    run_speed_at_weight,
    sprint_speed_at_weight,
    encumbrance_speed_penalty_base,
)

TOOLS_DIR = os.path.dirname(__file__)
LOAD_KG = 35.0
BODY_WEIGHT = 90.0
IDEAL_TEMP_C = 20.0
IDEAL_WIND = 0.0
IDEAL_TERRAIN = 1.0
IDEAL_GRADE = 0.0

PRESETS = {
    "EliteStandard": "optimized_rss_config_elitestandard_v4.json",
    "StandardMilsim": "optimized_rss_config_standardmilsim_v4.json",
    "TacticalAction": "optimized_rss_config_tacticalaction_v4.json",
}


def load_params(filename):
    path = os.path.join(TOOLS_DIR, filename)
    with open(path, "r", encoding="utf-8") as fp:
        data = json.load(fp)
    return {k: float(v) for k, v in data.items() if not k.startswith("_")}


def fmt_time(sec):
    if sec is None:
        return "—"
    minutes = int(sec // 60)
    seconds = int(sec % 60)
    return f"{minutes}m{seconds:02d}s"


def pct_per_min(net_per_tick, dt=RSS_PLAYER_TICK_SEC):
    """单 tick 净变化 → 每分钟百分比。"""
    ticks_per_min = 60.0 / dt
    return net_per_tick * ticks_per_min * 100.0


def make_twin(params):
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    twin.environment_factor.temperature = IDEAL_TEMP_C
    twin.environment_factor.wind_speed = IDEAL_WIND
    twin.environment_factor.heat_stress = 0.0
    twin.environment_factor.cold_stress = 0.0
    twin.environment_factor.cold_static_penalty = 0.0
    return twin


def speed_snapshot(twin, params, movement, load_kg=LOAD_KG):
    """满体力、理想环境下的速度与消耗测速。"""
    cw = BODY_WEIGHT + load_kg
    twin.reset()
    twin.stamina = 1.0
    intent = movement
    is_sprinting, effective_phase, can_sprint, _ = twin.resolve_movement_state(
        intent, twin.stamina
    )
    theoretical = twin.calculate_actual_speed(
        twin.stamina, cw, effective_phase, 0.0, grade_percent=IDEAL_GRADE, current_time=0.0
    )
    if effective_phase == MovementType.SPRINT:
        engine_phase = MovementType.SPRINT
    elif effective_phase == MovementType.RUN:
        engine_phase = MovementType.RUN
    else:
        engine_phase = effective_phase
    engine_orig = twin.estimate_engine_original_max_speed(engine_phase, twin.constants)
    limit_mult = twin.compute_speed_limit_multiplier(theoretical, engine_orig)
    drain_vel = twin.get_drain_velocity_ms(
        effective_phase, is_sprinting, can_sprint, engine_orig, theoretical
    )
    measured_est = 0.0
    if drain_vel > 0.05:
        measured_est = min(7.0, engine_orig * limit_mult)
    return {
        "theoretical_ms": theoretical,
        "engine_orig_ms": engine_orig,
        "drain_vel_ms": drain_vel,
        "limit_mult": limit_mult,
        "measured_est_ms": measured_est,
    }


def net_drain_at_full(twin, movement, load_kg=LOAD_KG):
    """满体力稳态单 tick 净体力变化（负=消耗）。"""
    cw = BODY_WEIGHT + load_kg
    twin.reset()
    twin.stamina = 1.0
    dt = RSS_PLAYER_TICK_SEC
    prev = twin.stamina
    twin.game_player_tick(
        movement, cw, IDEAL_GRADE, IDEAL_TERRAIN, Stance.STAND, 0.0, dt,
        wind_drag=0.0, enable_randomness=False,
    )
    return twin.stamina - prev


def simulate_continuous(movement, params, max_s=7200.0, load_kg=LOAD_KG):
    """持续同一移动意图直到 max_s 或体力见底。"""
    twin = make_twin(params)
    cw = BODY_WEIGHT + load_kg
    dt = RSS_PLAYER_TICK_SEC
    sprint_cut = params.get("sprint_enable_threshold", 0.25)
    willpower = params.get("willpower_threshold", 0.35)
    t = 0.0
    distance = 0.0
    milestones = {
        "sprint_cut": None,
        "willpower": None,
        "pct_25": None,
        "pct_15": None,
        "empty": None,
    }
    speed_sum = 0.0
    speed_n = 0
    min_stamina = 1.0

    while t <= max_s and twin.stamina > 0.001:
        drain_speed = twin.game_player_tick(
            movement, cw, IDEAL_GRADE, IDEAL_TERRAIN, Stance.STAND, t, dt,
            wind_drag=0.0, enable_randomness=False,
        )
        distance += drain_speed * dt
        speed_sum += drain_speed
        speed_n += 1
        min_stamina = min(min_stamina, twin.stamina)

        if milestones["sprint_cut"] is None and twin.stamina <= sprint_cut:
            milestones["sprint_cut"] = t
        if milestones["willpower"] is None and twin.stamina <= willpower:
            milestones["willpower"] = t
        if milestones["pct_25"] is None and twin.stamina < 0.25:
            milestones["pct_25"] = t
        if milestones["pct_15"] is None and twin.stamina < 0.15:
            milestones["pct_15"] = t
        if milestones["empty"] is None and twin.stamina <= 0.01:
            milestones["empty"] = t
        t += dt

    avg_speed = speed_sum / speed_n if speed_n > 0 else 0.0
    return {
        "milestones": milestones,
        "min_stamina": min_stamina,
        "distance_m": distance,
        "avg_drain_speed_ms": avg_speed,
        "duration_s": t,
    }


def simulate_stand_recovery(params, start_stamina, duration_s=600.0, load_kg=LOAD_KG):
    """站立静止恢复率。"""
    twin = make_twin(params)
    cw = BODY_WEIGHT + load_kg
    twin.stamina = float(start_stamina)
    twin.exercise_duration_minutes = 5.0
    dt = RSS_PLAYER_TICK_SEC
    prev = twin.stamina
    twin.game_player_tick(
        MovementType.IDLE, cw, IDEAL_GRADE, IDEAL_TERRAIN, Stance.STAND, 0.0, dt,
        wind_drag=0.0, enable_randomness=False,
    )
    return twin.stamina - prev


def simulate_recover_to_sprint(params, load_kg=LOAD_KG):
    """冲刺至禁止后，站立恢复到可再冲刺所需时间。"""
    twin = make_twin(params)
    cw = BODY_WEIGHT + load_kg
    sprint_cut = params.get("sprint_enable_threshold", 0.25)
    dt = RSS_PLAYER_TICK_SEC
    t = 0.0
    cut_time = None

    while t <= 600.0:
        twin.game_player_tick(
            MovementType.SPRINT, cw, IDEAL_GRADE, IDEAL_TERRAIN, Stance.STAND, t, dt,
            wind_drag=0.0, enable_randomness=False,
        )
        if cut_time is None and twin.stamina <= sprint_cut:
            cut_time = t
            break
        t += dt

    if cut_time is None:
        return None

    recover_t = 0.0
    while recover_t <= 1800.0:
        twin.game_player_tick(
            MovementType.IDLE, cw, IDEAL_GRADE, IDEAL_TERRAIN, Stance.STAND,
            cut_time + recover_t, dt, wind_drag=0.0, enable_randomness=False,
        )
        if twin.stamina > sprint_cut:
            return cut_time, recover_t, twin.stamina
        recover_t += dt
    return cut_time, None, twin.stamina


def print_preset_block(name, params):
    twin = make_twin(params)
    cw = BODY_WEIGHT + LOAD_KG
    enc_pen = encumbrance_speed_penalty_base(twin.constants, cw)
    load_ratio = LOAD_KG / BODY_WEIGHT

    print(f"\n{'=' * 88}")
    print(f"## {name}")
    print(f"{'=' * 88}")
    print(f"负重: {LOAD_KG:.0f} kg (总重 {cw:.0f} kg, 负重比 {load_ratio * 100:.1f}%)")
    print(f"环境: {IDEAL_TEMP_C:.0f}°C 平路 地形×{IDEAL_TERRAIN:.1f} 无风 站姿")
    print(
        f"阈值: sprint_enable={params.get('sprint_enable_threshold', 0.25):.3f}  "
        f"willpower={params.get('willpower_threshold', 0.35):.3f}  "
        f"exhaustion={params.get('exhaustion_threshold', 0.0):.3f}"
    )
    print(f"负重速度惩罚基数 enc_penalty_base = {enc_pen * 100:.2f}%")

    print("\n### 满体力速度 (m/s)")
    print(f"  {'模式':<8} {'RSS理论限速':>10} {'引擎原速':>10} {'消耗测速':>10} {'限速倍率':>10} {'km/h':>8}")
    movement_labels = [
        ("步行", MovementType.WALK),
        ("跑步", MovementType.RUN),
        ("冲刺", MovementType.SPRINT),
    ]
    for label, mov in movement_labels:
        snap = speed_snapshot(twin, params, mov)
        kmh = snap["drain_vel_ms"] * 3.6
        print(
            f"  {label:<8} {snap['theoretical_ms']:>10.2f} {snap['engine_orig_ms']:>10.2f} "
            f"{snap['drain_vel_ms']:>10.2f} {snap['limit_mult']:>10.3f} {kmh:>8.2f}"
        )
    print(
        f"  (参考: walk={walk_speed_at_weight(twin.constants, cw):.2f}  "
        f"run={run_speed_at_weight(twin.constants, cw):.2f}  "
        f"sprint={sprint_speed_at_weight(twin.constants, cw):.2f} m/s)"
    )

    print("\n### 满体力稳态净消耗 (%/min, 负=掉体力)")
    print(f"  {'模式':<8} {'净变化/min':>12} {'约耗尽至0%':>14}")
    for label, mov in movement_labels:
        net = net_drain_at_full(twin, mov)
        rate = pct_per_min(net)
        if rate >= 0:
            empty_est = "— (净恢复)"
        else:
            empty_est = fmt_time(100.0 / abs(rate) * 60.0)
        print(f"  {label:<8} {rate:>+12.2f} {empty_est:>14}")

    print("\n### 持续运动耐力 (闭环 PlayerBase 同序, dt=17ms)")
    print(
        f"  {'模式':<8} {'禁止冲刺':>10} {'意志力':>10} {'<25%':>10} "
        f"{'<15%':>10} {'至0%':>10} {'距离(m)':>10} {'均速':>8}"
    )
    for label, mov in movement_labels:
        res = simulate_continuous(mov, params)
        ms = res["milestones"]
        cut_col = fmt_time(ms["sprint_cut"]) if mov == MovementType.SPRINT else "—"
        print(
            f"  {label:<8} {cut_col:>10} {fmt_time(ms['willpower']):>10} "
            f"{fmt_time(ms['pct_25']):>10} {fmt_time(ms['pct_15']):>10} "
            f"{fmt_time(ms['empty']):>10} {res['distance_m']:>10.0f} "
            f"{res['avg_drain_speed_ms']:>8.2f}"
        )

    print("\n### 站立静止恢复 (%/min)")
    print(f"  {'当前体力':<12} {'恢复/min':>12} {'回满100%':>12} {'回可冲刺':>12}")
    sprint_cut = params.get("sprint_enable_threshold", 0.25)
    for start in (0.50, 0.25, 0.15, sprint_cut):
        net = simulate_stand_recovery(params, start)
        rate = pct_per_min(net)
        if rate <= 0:
            to_full = "—"
            to_sprint = "—"
        else:
            to_full = fmt_time((1.0 - start) / (rate / 100.0 / 60.0))
            if start >= sprint_cut:
                to_sprint = "已可冲刺"
            else:
                to_sprint = fmt_time((sprint_cut - start) / (rate / 100.0 / 60.0))
        print(f"  {start * 100:>6.0f}%     {rate:>+12.2f} {to_full:>12} {to_sprint:>12}")

    rec = simulate_recover_to_sprint(params)
    if rec and rec[1] is not None:
        cut_t, rec_t, final = rec
        print(
            f"\n### 冲刺循环"
            f"\n  冲刺至禁止: {fmt_time(cut_t)} → 站立恢复至可再冲刺: {fmt_time(rec_t)} "
            f"(恢复后 {final * 100:.1f}%)"
            f"\n  单周期 (冲刺+恢复): {fmt_time(cut_t + rec_t)}"
        )


def main():
    print("=" * 88)
    print("35kg 理想环境 — 三档预设移动数据")
    print("模型: PlayerBase 同序 | 冲刺/跑步消耗=引擎原速 (5.5/3.8 m/s) | dt=17ms")
    print("=" * 88)

    summary = []
    for name, fname in PRESETS.items():
        params = load_params(fname)
        print_preset_block(name, params)
        sprint_res = simulate_continuous(MovementType.SPRINT, params)
        run_res = simulate_continuous(MovementType.RUN, params)
        summary.append({
            "name": name,
            "sprint_cut": sprint_res["milestones"]["sprint_cut"],
            "sprint_dist": sprint_res["distance_m"],
            "run_15": run_res["milestones"]["pct_15"],
            "sprint_cut_thresh": params.get("sprint_enable_threshold"),
        })

    print(f"\n{'=' * 88}")
    print("### 三档对比摘要")
    print(f"  {'预设':<16} {'冲刺禁止阈值':>12} {'禁止冲刺':>10} {'冲刺距离':>10} {'跑步<15%':>10}")
    for row in summary:
        print(
            f"  {row['name']:<16} {row['sprint_cut_thresh']:>12.3f} "
            f"{fmt_time(row['sprint_cut']):>10} {row['sprint_dist']:>9.0f}m "
            f"{fmt_time(row['run_15']):>10}"
        )
    print("=" * 88)


if __name__ == "__main__":
    main()
