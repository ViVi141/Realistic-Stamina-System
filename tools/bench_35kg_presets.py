#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三档预设在 35kg 负重下的活动/耐力基准（PlayerBase 同序数字孪生）。"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from rss_pipeline_v4 import MissionLibrary, Mission, Phase, simulate_mission
from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    merge_game_aligned_params,
    MovementType,
    Stance,
    STAMINA_TICK_SEC,
)

PRESETS = {
    "EliteStandard": "optimized_rss_config_elitestandard_v4.json",
    "StandardMilsim": "optimized_rss_config_standardmilsim_v4.json",
    "TacticalAction": "optimized_rss_config_tacticalaction_v4.json",
}
LOAD_KG = 35.0
TOOLS_DIR = os.path.dirname(__file__)


def load_params(filename):
    path = os.path.join(TOOLS_DIR, filename)
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return {k: float(v) for k, v in data.items() if not k.startswith("_")}


def fmt_t(sec):
    if sec is None:
        return "未耗尽"
    minutes = int(sec // 60)
    seconds = int(sec % 60)
    return f"{minutes}分{seconds}秒 ({sec:.0f}s)"


def simulate_steady(label, movement, duration_s, params, grade=0.0, terrain=1.0):
    mission = Mission(
        name=label,
        load_kg=LOAD_KG,
        phases=[Phase(duration_s, 0.0, movement, Stance.STAND, grade, terrain, label)],
    )
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    result = simulate_mission(twin, mission)
    trace = result.stamina_trace
    dt = STAMINA_TICK_SEC
    sprint_cut = params.get("sprint_enable_threshold", 0.25)
    t_exhaust = None
    t_15 = None
    t_25 = None
    t_cut = None
    for i, stamina in enumerate(trace):
        t = i * dt
        if t_25 is None and stamina < 0.25:
            t_25 = t
        if t_cut is None and stamina <= sprint_cut:
            t_cut = t
        if t_15 is None and stamina < 0.15:
            t_15 = t
        if t_exhaust is None and stamina <= 0.01:
            t_exhaust = t
    return {
        "min": result.min_stamina,
        "t_to_sprint_cut": t_cut,
        "t_to_25pct": t_25,
        "t_to_15pct": t_15,
        "t_to_empty": t_exhaust,
    }


def mission_at_35(factory):
    mission = factory()
    mission.load_kg = LOAD_KG
    mission.name = mission.name + "@35kg"
    return mission


def main():
    print("=" * 78)
    print(
        f"三档预设 @ {LOAD_KG:.0f}kg (PlayerBase 同序, dt={STAMINA_TICK_SEC}s, "
        f"冲刺消耗=引擎原速 5.5m/s)"
    )
    print("=" * 78)

    summary_rows = []

    for preset_name, json_file in PRESETS.items():
        params = load_params(json_file)
        sprint_cut = params.get("sprint_enable_threshold", 0.25)
        print(f"\n## {preset_name}")
        print(f"  sprint_enable_threshold={sprint_cut:.3f}")
        print("-" * 78)
        print("### 持续运动（闭环，速度由游戏逻辑决定）")
        scenarios = [
            ("步行 (平路)", MovementType.WALK, 3600, 0.0),
            ("跑步 (平路)", MovementType.RUN, 1800, 0.0),
            ("冲刺 (平路)", MovementType.SPRINT, 600, 0.0),
            ("步行 12% 坡", MovementType.WALK, 3600, 12.0),
        ]
        for label, movement, duration, grade in scenarios:
            res = simulate_steady(label, movement, duration, params, grade=grade)
            print(f"  {label}")
            line = (
                f"    最低 {res['min'] * 100:.1f}% | "
                f"降至25% {fmt_t(res['t_to_25pct'])} | "
                f"降至15% {fmt_t(res['t_to_15pct'])} | "
                f"降至0% {fmt_t(res['t_to_empty'])}"
            )
            if movement == MovementType.SPRINT:
                line = (
                    f"    最低 {res['min'] * 100:.1f}% | "
                    f"禁止冲刺 {fmt_t(res['t_to_sprint_cut'])} | "
                    f"降至15% {fmt_t(res['t_to_15pct'])} | "
                    f"降至0% {fmt_t(res['t_to_empty'])}"
                )
                summary_rows.append((preset_name, "冲刺至禁止", res["t_to_sprint_cut"], None))
            if "跑步" in label:
                summary_rows.append((preset_name, "跑步至15%", res["t_to_15pct"], None))
            print(line)

        print("### 8 场景任务剖面 (35kg)")
        twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
        factories = [
            MissionLibrary.patrol_contact,
            MissionLibrary.desert_patrol,
            MissionLibrary.hell_desert_breakout,
            MissionLibrary.amphibious_landing,
            MissionLibrary.urban_clearance,
            MissionLibrary.mountain_approach,
            MissionLibrary.vehicle_dismount,
            MissionLibrary.heavy_evacuation,
        ]
        print(f"  {'场景':<14} {'最低%':>6} {'运动均体%':>10} {'力竭<15%':>10} {'运动时长':>8}")
        patrol_min = None
        for factory in factories:
            mission = mission_at_35(factory)
            twin.reset()
            result = simulate_mission(twin, mission)
            name = mission.name.replace("@35kg", "")
            exh_min = int(result.exhaustion_duration_s // 60)
            exh_sec = int(result.exhaustion_duration_s % 60)
            active_s = sum(
                p.duration_s
                for p in mission.phases
                if p.movement != MovementType.IDLE and p.speed_ms >= 0.1
            )
            print(
                f"  {name:<14} {result.min_stamina * 100:6.1f} "
                f"{result.mean_stamina_active * 100:10.1f} "
                f"{exh_min:3d}m{exh_sec:02d}s {int(active_s):4d}s"
            )
            if name == "巡逻接敌":
                patrol_min = result.min_stamina
        summary_rows.append((preset_name, "巡逻接敌最低%", patrol_min, None))

    print("\n" + "=" * 78)
    print("### 三档对比摘要 (35kg)")
    print(f"  {'预设':<16} {'指标':<18} {'结果'}")
    for preset, metric, v1, v2 in summary_rows:
        if "至" in metric or "禁止" in metric:
            val = fmt_t(v1)
        else:
            val = f"{v1 * 100:.1f}%" if v1 is not None else "N/A"
        print(f"  {preset:<16} {metric:<18} {val}")
    print("=" * 78)


if __name__ == "__main__":
    main()
