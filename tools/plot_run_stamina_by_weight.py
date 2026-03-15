#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按 optimized_rss_config_realism_super.json 绘制铺装路面上各负重 Run 与 Sprint 的时间-体力图。
铺装路面：grade_percent=0，terrain_factor=1.0。
上图：Run（各负重用该负重下的 run 速度）；下图：Sprint（各负重用该负重下的 sprint 速度）。
"""

import json
import sys
from pathlib import Path

# 保证可导入同目录下的孪生模块
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    MovementType,
    Stance,
)

# 硬编码速度值（m/s）
SPEED_VALUES = {
    0: {"walk": 3.02, "run": 3.81, "sprint": 5.08},    # 0kg
    10: {"walk": 2.94, "run": 3.79, "sprint": 5.04},   # 10kg
    20: {"walk": 2.87, "run": 3.81, "sprint": 5.01},   # 20kg
    30: {"walk": 2.81, "run": 3.81, "sprint": 4.96},   # 30kg
    40: {"walk": 2.32, "run": 3.22, "sprint": 4.39},   # 40kg
    50: {"walk": 1.82, "run": 2.64, "sprint": 3.80},   # 50kg
    55: {"walk": 1.56, "run": 2.35, "sprint": 3.40}    # 55kg
}

def get_speed(load_kg, speed_type):
    """获取指定负载下的速度值"""
    # 找到最接近的负载级别
    load_levels = sorted(SPEED_VALUES.keys())
    closest_load = min(load_levels, key=lambda x: abs(x - load_kg))
    return SPEED_VALUES[closest_load][speed_type]


def load_constants_from_json(json_path: Path) -> RSSConstants:
    """从 JSON 配置构建 RSSConstants（仅覆盖 JSON 中存在的字段）。"""
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    params = data.get("parameters", {})
    constants = RSSConstants()
    for k, v in params.items():
        attr = k.upper()
        if hasattr(constants, attr):
            setattr(constants, attr, v)
    return constants


def main():
    json_path = SCRIPT_DIR / "optimized_rss_config_realism_super.json"
    if not json_path.exists():
        print(f"未找到配置: {json_path}")
        return 1

    constants = load_constants_from_json(json_path)
    twin = RSSDigitalTwin(constants)

    # 铺装路面：平地，地形因子 1.0
    grade_percent = 0.0
    terrain_factor = 1.0
    stance = Stance.STAND
    movement_type = MovementType.RUN
    dt = 0.2
    max_duration_s = 1800.0   # 最多仿真 30 分钟，以便看到体力耗尽情况
    stamina_depleted_threshold = 0.005  # 低于此视为耗尽

    # 各负重 (kg)：0, 10, 20, 29, 30, 40
    load_kg_list = [0, 10, 20, 29, 30, 40]
    results_run = []
    results_sprint = []

    for load_kg in load_kg_list:
        current_weight = 90.0 + float(load_kg)
        run_speed = get_speed(load_kg, "run")
        twin.reset()
        time_list = [0.0]
        stamina_list = [1.0]
        t = 0.0
        while t < max_duration_s and twin.stamina > stamina_depleted_threshold:
            twin.step(
                run_speed,
                current_weight,
                grade_percent,
                terrain_factor,
                stance,
                movement_type,
                t + dt,
                enable_randomness=False,
                wind_drag=0.0,
            )
            t += dt
            time_list.append(t)
            stamina_list.append(max(0.0, twin.stamina))
        results_run.append({
            "load_kg": load_kg,
            "speed": run_speed,
            "time": time_list,
            "stamina": stamina_list,
        })

    # Sprint：各负重用该负重下的冲刺速度
    movement_type_sprint = MovementType.SPRINT
    for load_kg in load_kg_list:
        current_weight = 90.0 + float(load_kg)
        sprint_speed = get_speed(load_kg, "sprint")
        twin.reset()
        time_list = [0.0]
        stamina_list = [1.0]
        t = 0.0
        while t < max_duration_s and twin.stamina > stamina_depleted_threshold:
            twin.step(
                sprint_speed,
                current_weight,
                grade_percent,
                terrain_factor,
                stance,
                movement_type_sprint,
                t + dt,
                enable_randomness=False,
                wind_drag=0.0,
            )
            t += dt
            time_list.append(t)
            stamina_list.append(max(0.0, twin.stamina))
        results_sprint.append({
            "load_kg": load_kg,
            "speed": sprint_speed,
            "time": time_list,
            "stamina": stamina_list,
        })

    # 绘图：上下两子图（Run / Sprint）
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("需要 matplotlib，请安装: pip install matplotlib")
        return 1

    fig, (ax_run, ax_sprint) = plt.subplots(2, 1, figsize=(10, 10), sharex=True)
    colors = plt.cm.viridis([i / max(len(load_kg_list) - 1, 1) for i in range(len(load_kg_list))])

    for i, r in enumerate(results_run):
        t = r["time"]
        s = r["stamina"]
        if not t or not s:
            continue
        label = f"{r['load_kg']} kg (run {r['speed']:.2f} m/s)"
        ax_run.plot(t, s, color=colors[i], label=label, linewidth=1.5)
    ax_run.set_ylabel("Stamina", fontsize=12)
    ax_run.set_title("Run on paved road: Time vs Stamina by load", fontsize=12)
    ax_run.legend(loc="best", fontsize=9)
    ax_run.set_ylim(-0.05, 1.05)
    ax_run.grid(True, alpha=0.3)
    ax_run.axhline(y=0, color="gray", linestyle="--", alpha=0.5)

    for i, r in enumerate(results_sprint):
        t = r["time"]
        s = r["stamina"]
        if not t or not s:
            continue
        label = f"{r['load_kg']} kg (sprint {r['speed']:.2f} m/s)"
        ax_sprint.plot(t, s, color=colors[i], label=label, linewidth=1.5)
    ax_sprint.set_xlabel("Time (s)", fontsize=12)
    ax_sprint.set_ylabel("Stamina", fontsize=12)
    ax_sprint.set_title("Sprint on paved road: Time vs Stamina by load", fontsize=12)
    ax_sprint.legend(loc="best", fontsize=9)
    ax_sprint.set_ylim(-0.05, 1.05)
    t_max_run = max((r["time"][-1] for r in results_run if r["time"]), default=600.0)
    t_max_sprint = max((r["time"][-1] for r in results_sprint if r["time"]), default=600.0)
    ax_sprint.set_xlim(0, max(t_max_run, t_max_sprint) * 1.02)
    ax_sprint.grid(True, alpha=0.3)
    ax_sprint.axhline(y=0, color="gray", linestyle="--", alpha=0.5)

    fig.suptitle("Paved road (grade=0, terrain=1.0) — optimized_rss_config_realism_super.json", fontsize=11, y=1.00)
    fig.tight_layout()
    out_path = SCRIPT_DIR / "run_stamina_by_weight_paved.png"
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")

    # 打印简要统计
    print("\nPaved road Run: time to depletion (s):")
    for r in results_run:
        t, s = r["time"], r["stamina"]
        if t and s:
            print(f"  {r['load_kg']} kg: depleted at t={t[-1]:.1f} s (run_speed={r['speed']:.3f} m/s)")
        else:
            print(f"  {r['load_kg']} kg: no data")
    print("\nPaved road Sprint: time to depletion (s):")
    for r in results_sprint:
        t, s = r["time"], r["stamina"]
        if t and s:
            print(f"  {r['load_kg']} kg: depleted at t={t[-1]:.1f} s (sprint_speed={r['speed']:.3f} m/s)")
        else:
            print(f"  {r['load_kg']} kg: no data")

    return 0


if __name__ == "__main__":
    sys.exit(main())
