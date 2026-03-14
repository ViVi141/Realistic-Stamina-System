#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""T1 特种部队体能基准测试：对照真实文献数据评估当前优化参数。"""

import json
import math
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    MovementType,
    Stance,
    run_speed_at_weight,
    tobler_speed_multiplier,
)

BW = 90.0
DT = 0.2


def load_constants(json_path):
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    params = data.get("parameters", {})
    c = RSSConstants()
    for k, v in params.items():
        attr = k.upper()
        if hasattr(c, attr):
            setattr(c, attr, v)
    return c


def run_fixed(twin, speed, weight, duration_s, movement_type, grade_pct=0.0, tf=1.0):
    """固定速度仿真，返回最终体力和最低体力。"""
    twin.reset()
    t = 0.0
    while t < duration_s:
        twin.step(speed, weight, grade_pct, tf, Stance.STAND,
                  movement_type, t + DT, enable_randomness=False)
        t += DT
    return twin.stamina, min(twin.stamina_history) if twin.stamina_history else twin.stamina


def run_distance(twin, speed, weight, distance_m, movement_type, grade_pct=0.0, tf=1.0):
    """跑指定距离，返回耗时和最终体力。"""
    twin.reset()
    t = 0.0
    d = 0.0
    while d < distance_m and twin.stamina > 0.001:
        twin.step(speed, weight, grade_pct, tf, Stance.STAND,
                  movement_type, t + DT, enable_randomness=False)
        t += DT
        d += speed * DT
    return t, twin.stamina


def closed_loop_run(twin, weight, duration_s, movement_type, grade_pct=0.0, tf=1.0):
    """闭环仿真（体力影响速度），返回距离、最终体力、最低体力。"""
    twin.reset()
    t = 0.0
    total_dist = 0.0
    last_speed = 0.0
    while t < duration_s and twin.stamina > 0.001:
        spd = twin.calculate_actual_speed(
            twin.stamina, weight, movement_type, last_speed,
            grade_percent=grade_pct, current_time=t)
        twin.step(spd, weight, grade_pct, tf, Stance.STAND,
                  movement_type, t + DT, enable_randomness=False)
        total_dist += spd * DT
        last_speed = spd
        t += DT
    min_s = min(twin.stamina_history) if twin.stamina_history else twin.stamina
    return total_dist, twin.stamina, min_s


def recovery_test(twin, weight, drain_duration, drain_speed, drain_type,
                  rest_duration, rest_stance=Stance.STAND):
    """先消耗再恢复，返回消耗结束体力和恢复结束体力。"""
    twin.reset()
    t = 0.0
    while t < drain_duration:
        twin.step(drain_speed, weight, 0.0, 1.0, Stance.STAND,
                  drain_type, t + DT, enable_randomness=False)
        t += DT
    stamina_after_drain = twin.stamina

    while t < drain_duration + rest_duration:
        twin.step(0.0, weight, 0.0, 1.0, rest_stance,
                  MovementType.IDLE, t + DT, enable_randomness=False)
        t += DT
    return stamina_after_drain, twin.stamina


def main():
    json_path = SCRIPT_DIR / "optimized_rss_config_realism_super.json"
    if not json_path.exists():
        print(f"未找到: {json_path}")
        return
    constants = load_constants(json_path)
    twin = RSSDigitalTwin(constants)

    print("=" * 80)
    print("T1 特种部队体能基准测试 — 对照真实文献数据")
    print("=" * 80)

    w0 = BW
    w30 = BW + 30.0
    w45 = BW + 45.0
    rs0 = run_speed_at_weight(constants, w0)
    rs30 = run_speed_at_weight(constants, w30)
    rs45 = run_speed_at_weight(constants, w45)
    print(f"\n基本参数: 体重={BW}kg, energy_coeff={constants.ENERGY_TO_STAMINA_COEFF:.2e}")
    print(f"  空载 Run 速度: {rs0:.2f} m/s")
    print(f"  30kg Run 速度: {rs30:.2f} m/s")
    print(f"  45kg Run 速度: {rs45:.2f} m/s")

    # ========== 1. 空载跑步距离测试（对照文献表一） ==========
    print("\n" + "=" * 80)
    print("测试 1: 空载跑步 — 按距离消耗（文献对照）")
    print("=" * 80)
    ref_table = [
        (200, "8-10%", 0.90, 0.92),
        (400, "16-20%", 0.80, 0.84),
        (600, "24-30%", 0.70, 0.76),
        (800, "32-40%", 0.60, 0.68),
        (1000, "40-50%", 0.50, 0.60),
        (1500, "60-70%", 0.30, 0.40),
        (2000, "75-85%", 0.15, 0.25),
    ]
    print(f"{'距离':>6} | {'耗时':>7} | {'剩余体力':>8} | {'文献目标':>12} | {'判定':>4}")
    print("-" * 60)
    for dist, ref_str, ref_low, ref_high in ref_table:
        t_s, final = run_distance(twin, rs0, w0, dist, MovementType.RUN)
        if final >= ref_low and final <= ref_high:
            verdict = "PASS"
        elif final > ref_high:
            verdict = "偏轻"
        else:
            verdict = "偏重"
        print(f" {dist:>5}m | {t_s:>5.0f} s | {final:>7.1%} | {ref_str:>12} | {verdict}")

    # ========== 2. 30kg 负重跑步距离测试（对照文献表二） ==========
    print("\n" + "=" * 80)
    print("测试 2: 30kg 负重跑步 — 按距离消耗")
    print("=" * 80)
    ref_30kg = [
        (600, "70-76%", 0.70, 0.76),
        (1000, "50-60%", 0.50, 0.60),
    ]
    print(f"{'距离':>6} | {'耗时':>7} | {'剩余体力':>8} | {'文献目标':>12} | {'判定':>4}")
    print("-" * 60)
    for dist, ref_str, ref_low, ref_high in ref_30kg:
        t_s, final = run_distance(twin, rs30, w30, dist, MovementType.RUN)
        if final >= ref_low and final <= ref_high:
            verdict = "PASS"
        elif final > ref_high:
            verdict = "偏轻"
        else:
            verdict = "偏重"
        print(f" {dist:>5}m | {t_s:>5.0f} s | {final:>7.1%} | {ref_str:>12} | {verdict}")

    # ========== 3. ACFT 2英里（3.2km）空载测试 ==========
    print("\n" + "=" * 80)
    print("测试 3: ACFT 2 英里 (3.2km) 空载 — T1 标准 <13min")
    print("=" * 80)
    t_acft, final_acft = run_distance(twin, rs0, w0, 3200, MovementType.RUN)
    print(f"  耗时: {t_acft:.0f}s ({t_acft / 60:.1f} min)")
    print(f"  剩余体力: {final_acft:.1%}")
    if t_acft / 60 < 13:
        print(f"  T1 标准 (<13min): PASS")
    else:
        print(f"  T1 标准 (<13min): FAIL")

    # ========== 4. 12英里负重行军（Ruck March）==========
    print("\n" + "=" * 80)
    print("测试 4: 12英里 (19.3km) 负重行军 45lb(20kg) — T1 标准 <3h")
    print("=" * 80)
    w_ruck = BW + 20.0
    walk_speed = 1.8
    dist_ruck, final_ruck, min_ruck = closed_loop_run(
        twin, w_ruck, 3 * 3600, MovementType.WALK, grade_pct=0.0)
    t_needed = 19300.0 / walk_speed if walk_speed > 0 else 99999
    print(f"  1.8 m/s 行军 3h 距离: {dist_ruck:.0f}m ({dist_ruck / 1000:.1f}km)")
    print(f"  理论用时: {t_needed / 3600:.1f}h")
    print(f"  3h 后体力: {final_ruck:.1%}, 最低: {min_ruck:.1%}")

    # ========== 5. 30kg 连续跑耐力 ==========
    print("\n" + "=" * 80)
    print("测试 5: 30kg 连续跑耐力 — 多时间节点")
    print("=" * 80)
    print(f"{'时间':>6} | {'剩余体力':>8} | {'T1 期望':>12}")
    print("-" * 40)
    checkpoints = [
        (60, ">85%"),
        (120, ">70%"),
        (180, ">55%"),
        (300, ">30%"),
        (600, ">5%"),
    ]
    for dur_s, expect in checkpoints:
        final_s, min_s = run_fixed(twin, rs30, w30, dur_s, MovementType.RUN)
        print(f" {dur_s:>4} s | {final_s:>7.1%} | {expect:>12}")

    # ========== 6. Sprint 30秒测试 ==========
    print("\n" + "=" * 80)
    print("测试 6: 空载 Sprint 30s — 期望剩余 60-85%")
    print("=" * 80)
    sprint_speed = constants.GAME_MAX_SPEED * (1.0 + constants.SPRINT_SPEED_BOOST)
    final_sprint, min_sprint = run_fixed(
        twin, sprint_speed, w0, 30.0, MovementType.SPRINT)
    drop = 1.0 - final_sprint
    print(f"  Sprint 速度: {sprint_speed:.2f} m/s")
    print(f"  30s 后剩余: {final_sprint:.1%} (消耗 {drop:.1%})")
    if 0.15 <= drop <= 0.40:
        print(f"  优化器约束 (15-40%): PASS")
    else:
        print(f"  优化器约束 (15-40%): FAIL")

    # ========== 7. 恢复测试 ==========
    print("\n" + "=" * 80)
    print("测试 7: 恢复能力 — 跑 5min 后站立休息 / 趴下休息")
    print("=" * 80)
    for rest_stance, stance_name in [(Stance.STAND, "站立"), (Stance.PRONE, "趴下")]:
        drain_end, recov_end = recovery_test(
            twin, w30, 300, rs30, MovementType.RUN, 180, rest_stance)
        gain = recov_end - drain_end
        print(f"  30kg 跑 5min → {stance_name}休息 3min:")
        print(f"    跑后体力: {drain_end:.1%} → 恢复后: {recov_end:.1%} (+{gain:.1%})")

    # ========== 8. 坡度影响 ==========
    print("\n" + "=" * 80)
    print("测试 8: 30kg 坡度跑 5min — 闭环")
    print("=" * 80)
    print(f"{'坡度':>6} | {'距离':>7} | {'剩余体力':>8} | {'最低体力':>8}")
    print("-" * 45)
    for grade in [-15, -10, -5, 0, 5, 10, 15, 20]:
        dist_g, final_g, min_g = closed_loop_run(
            twin, w30, 300, MovementType.RUN, grade_pct=grade)
        sign = "+" if grade > 0 else ""
        print(f" {sign}{grade:>4}% | {dist_g:>5.0f} m | {final_g:>7.1%} | {min_g:>7.1%}")

    # ========== 9. 战术动作参考（对照文献表三） ==========
    print("\n" + "=" * 80)
    print("测试 9: 战术动作消耗（空载，对照文献）")
    print("=" * 80)
    tactics = [
        ("掩体冲刺 40m", 40, sprint_speed, MovementType.SPRINT, "4-6%"),
        ("街区穿越 120m", 120, rs0, MovementType.RUN, "6-8%"),
        ("阵地转移 300m", 300, rs0, MovementType.RUN, "14-18%"),
        ("接敌机动 600m", 600, rs0, MovementType.RUN, "25-30%"),
        ("快速撤离 800m", 800, rs0, MovementType.RUN, "35-40%"),
    ]
    print(f"{'动作':>14} | {'耗时':>7} | {'消耗':>6} | {'文献':>8} | {'判定':>4}")
    print("-" * 55)
    for name, dist, speed, mt, ref in tactics:
        t_s, final = run_distance(twin, speed, w0, dist, mt)
        cost = 1.0 - final
        ref_parts = ref.replace("%", "").split("-")
        ref_lo = float(ref_parts[0]) / 100
        ref_hi = float(ref_parts[1]) / 100
        if ref_lo <= cost <= ref_hi:
            v = "PASS"
        elif cost < ref_lo:
            v = "偏轻"
        else:
            v = "偏重"
        print(f" {name:>13} | {t_s:>5.0f} s | {cost:>5.1%} | {ref:>8} | {v}")

    # ========== 10. 极端环境 ==========
    print("\n" + "=" * 80)
    print("测试 10: 极端环境 — 30kg 5min Run")
    print("=" * 80)

    def setup_env(tw, temp=20.0, wind=0.0):
        tw.environment_factor.heat_stress = 0.0
        tw.environment_factor.cold_stress = 0.0
        tw.environment_factor.cold_static_penalty = 0.0
        tw.environment_factor.temperature = temp
        tw.environment_factor.wind_speed = wind
        if temp > 30.0:
            coeff = getattr(tw.constants, 'ENV_TEMPERATURE_HEAT_PENALTY_COEFF', 0.02)
            max_add = getattr(tw.constants, 'ENV_HEAT_STRESS_MAX_MULTIPLIER', 1.5) - 1.0
            tw.environment_factor.heat_stress = min((temp - 30.0) * coeff, max_add)
        if temp < 0.0:
            cold_coeff = getattr(tw.constants, 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF', 0.05)
            tw.environment_factor.cold_stress = (0.0 - temp) * cold_coeff

    envs = [
        ("标准 20°C", 20.0),
        ("酷热 40°C", 40.0),
        ("严寒 -15°C", -15.0),
    ]
    for name, temp in envs:
        twin.reset()
        setup_env(twin, temp)
        final_e, min_e = run_fixed(twin, rs30, w30, 300, MovementType.RUN)
        print(f"  {name}: 剩余 {final_e:.1%}, 最低 {min_e:.1%}")

    print("\n" + "=" * 80)
    print("基准测试完成")
    print("=" * 80)


if __name__ == "__main__":
    main()
