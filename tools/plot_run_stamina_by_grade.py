#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按 optimized_rss_config_realism_super.json 绘制 30 kg 负重下 Run 的时间-体力图。
分 4 张图分别展示：坡度、地面材质、逆风风速、环境温度 的影响。

修复后的模型特性：
  坡度：Tobler 速度调整（含 C 端 boost + 30% 阻尼）+ Pandolf 消耗 + 移动意图恢复压制
  地面材质：terrain_factor 直接乘入 Pandolf 能量消耗
  逆风风速：wind_drag = min(1.0, wind_mps * ENV_WIND_RESISTANCE_COEFF)，消耗 ×(1+drag)
  温度：AdjustEnergyForTemperature 消耗端补偿 + 热应激消耗倍数 + 冷应激恢复惩罚
"""

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
    tobler_speed_multiplier,
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
    if load_kg in SPEED_VALUES:
        return SPEED_VALUES[load_kg][speed_type]

    load_levels = sorted(SPEED_VALUES.keys())
    if load_kg <= load_levels[0]:
        return SPEED_VALUES[load_levels[0]][speed_type]
    if load_kg >= load_levels[-1]:
        return SPEED_VALUES[load_levels[-1]][speed_type]

    lower = max(x for x in load_levels if x < load_kg)
    upper = min(x for x in load_levels if x > load_kg)
    low_v = SPEED_VALUES[lower][speed_type]
    high_v = SPEED_VALUES[upper][speed_type]
    t = (float(load_kg) - float(lower)) / float(upper - lower)
    return low_v + (high_v - low_v) * t

LOAD_KG = 30
CHARACTER_WEIGHT = 90.0
DT = 0.2
MAX_DURATION_S = 1800.0  # 最多仿真 30 分钟，以便看到体力耗尽情况
STAMINA_DEPLETED_THRESHOLD = 0.005


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


def grade_percent_to_angle_deg(grade_percent: float) -> float:
    """坡度百分比 → 坡度角（度）。"""
    return math.degrees(math.atan(grade_percent / 100.0))


def setup_environment(twin, temperature_celsius=None, wind_speed_mps=0.0):
    """根据温度和风速设置 twin 的 environment_factor。"""
    c = twin.constants
    twin.environment_factor.heat_stress = 0.0
    twin.environment_factor.cold_stress = 0.0
    twin.environment_factor.cold_static_penalty = 0.0
    twin.environment_factor.temperature = 20.0
    twin.environment_factor.wind_speed = float(wind_speed_mps)
    if temperature_celsius is None:
        return
    twin.environment_factor.temperature = float(temperature_celsius)
    heat_threshold = 30.0
    cold_threshold = 0.0
    if temperature_celsius > heat_threshold:
        coeff = getattr(c, 'ENV_TEMPERATURE_HEAT_PENALTY_COEFF', 0.02)
        max_add = getattr(c, 'ENV_HEAT_STRESS_MAX_MULTIPLIER', 1.5) - 1.0
        twin.environment_factor.heat_stress = min(
            (temperature_celsius - heat_threshold) * coeff, max_add)
    if temperature_celsius < cold_threshold:
        cold_coeff = getattr(c, 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF', 0.05)
        twin.environment_factor.cold_stress = (
            cold_threshold - temperature_celsius) * cold_coeff
        cold_static = getattr(c, 'ENV_TEMPERATURE_COLD_STATIC_PENALTY', 0.03)
        twin.environment_factor.cold_static_penalty = (
            cold_threshold - temperature_celsius) * cold_static


def compute_wind_drag(constants, wind_speed_mps: float, tailwind: bool = False) -> float:
    """将风速 (m/s) 转换为 wind_drag 系数。tailwind=True 返回负值（省能）。"""
    if abs(wind_speed_mps) < 1.0:
        return 0.0
    if tailwind:
        tw_coeff = getattr(constants, 'ENV_WIND_TAILWIND_BONUS', 0.02)
        tw_max = getattr(constants, 'ENV_WIND_TAILWIND_MAX', 0.15)
        return -min(abs(wind_speed_mps) * tw_coeff, tw_max)
    wind_coeff = getattr(constants, 'ENV_WIND_RESISTANCE_COEFF', 0.05)
    return min(1.0, abs(wind_speed_mps) * wind_coeff)


def simulate_run(twin, speed, current_weight, grade_percent, terrain_factor,
                 wind_drag=0.0):
    """单次仿真循环，返回 (time_list, stamina_list)。"""
    time_list = [0.0]
    stamina_list = [1.0]
    t = 0.0
    while t < MAX_DURATION_S and twin.stamina > STAMINA_DEPLETED_THRESHOLD:
        twin.step(
            speed,
            current_weight,
            grade_percent,
            terrain_factor,
            Stance.STAND,
            MovementType.RUN,
            t + DT,
            enable_randomness=False,
            wind_drag=wind_drag,
        )
        t += DT
        time_list.append(t)
        stamina_list.append(max(0.0, twin.stamina))
    return time_list, stamina_list


def setup_axis(ax, title, xlabel="Time (s)"):
    ax.set_xlabel(xlabel, fontsize=12)
    ax.set_ylabel("Stamina", fontsize=12)
    ax.set_title(title, fontsize=13)
    ax.legend(loc="best", fontsize=9)
    ax.set_ylim(-0.05, 1.05)
    ax.grid(True, alpha=0.3)
    ax.axhline(y=0, color="gray", linestyle="--", alpha=0.5)


def save_fig(fig, suptitle, out_path):
    fig.suptitle(suptitle, fontsize=11, y=1.00)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    import matplotlib.pyplot as plt
    plt.close(fig)
    print(f"Saved: {out_path}")


# =========================================================================
# 图 1：各坡度（使用 C 端对齐的 Tobler + 移动意图恢复压制）
# =========================================================================
def plot_by_grade(constants, twin, flat_run_speed, current_weight):
    import matplotlib.pyplot as plt

    grade_list = [-20, -15, -10, -5, 0, 5, 10, 15, 20]
    results = []

    for grade_pct in grade_list:
        angle_deg = grade_percent_to_angle_deg(grade_pct)
        slope_mult = tobler_speed_multiplier(
            angle_deg,
            uphill_boost=getattr(constants, 'UPHILL_SPEED_BOOST', 1.15),
            downhill_boost=getattr(constants, 'DOWNHILL_SPEED_BOOST', 1.15),
            downhill_max=getattr(constants, 'DOWNHILL_SPEED_MAX_MULTIPLIER', 1.25),
            dampening=getattr(constants, 'TOBLER_DAMPENING', 0.7),
        )
        effective_speed = flat_run_speed * slope_mult

        twin.reset()
        setup_environment(twin)
        t_list, s_list = simulate_run(
            twin, effective_speed, current_weight,
            float(grade_pct), 1.0)
        results.append({
            "grade_percent": grade_pct,
            "slope_mult": slope_mult,
            "effective_speed": effective_speed,
            "time": t_list, "stamina": s_list,
        })

    fig, ax = plt.subplots(1, 1, figsize=(12, 7))
    colors = plt.cm.coolwarm(
        [i / max(len(grade_list) - 1, 1) for i in range(len(grade_list))])

    for i, r in enumerate(results):
        if not r["time"]:
            continue
        sign = "+" if r["grade_percent"] > 0 else ""
        label = (f"grade {sign}{r['grade_percent']}%"
                 f" ({r['effective_speed']:.2f} m/s,"
                 f" Tobler x{r['slope_mult']:.2f})")
        ax.plot(r["time"], r["stamina"], color=colors[i],
                label=label, linewidth=1.8)

    t_max = max((r["time"][-1] for r in results if r["time"]), default=600.0)
    ax.set_xlim(0, t_max * 1.02)
    setup_axis(ax,
               f"Run at {LOAD_KG} kg — Time vs Stamina by grade\n"
               f"(C-side Tobler with boost+dampen, flat run = {flat_run_speed:.2f} m/s)")
    save_fig(fig,
             f"Paved road (load={LOAD_KG} kg, terrain=1.0)"
             " — optimized_rss_config_realism_super.json",
             SCRIPT_DIR / "run_stamina_by_grade_30kg.png")

    print(f"\n=== Grade (flat_run_speed={flat_run_speed:.3f} m/s) ===")
    print(f"{'Grade%':>7} | {'Tobler':>6} | {'Speed':>7} | {'Depleted':>10}")
    print("-" * 48)
    for r in results:
        print(f"  {r['grade_percent']:+4d}%  |  x{r['slope_mult']:.2f} |"
              f" {r['effective_speed']:5.2f} m/s | {r['time'][-1]:>8.1f} s")


# =========================================================================
# 图 2：各地面材质
# =========================================================================
def plot_by_terrain(constants, twin, flat_run_speed, current_weight):
    import matplotlib.pyplot as plt

    terrain_configs = [
        (1.0,  "Paved road"),
        (1.2,  "Gravel / packed dirt"),
        (1.35, "Grass / light trail"),
        (1.5,  "Sand / mud"),
        (2.0,  "Deep snow / swamp"),
        (2.5,  "Dense brush"),
    ]
    results = []

    for tf, name in terrain_configs:
        twin.reset()
        setup_environment(twin)
        t_list, s_list = simulate_run(
            twin, flat_run_speed, current_weight, 0.0, tf)
        results.append({
            "terrain_factor": tf, "name": name,
            "time": t_list, "stamina": s_list,
        })

    fig, ax = plt.subplots(1, 1, figsize=(12, 7))
    colors = plt.cm.YlOrBr(
        [0.2 + 0.7 * i / max(len(terrain_configs) - 1, 1)
         for i in range(len(terrain_configs))])

    for i, r in enumerate(results):
        if not r["time"]:
            continue
        label = f"{r['name']} (tf={r['terrain_factor']:.2f})"
        ax.plot(r["time"], r["stamina"], color=colors[i],
                label=label, linewidth=1.8)

    t_max = max((r["time"][-1] for r in results if r["time"]), default=600.0)
    ax.set_xlim(0, t_max * 1.02)
    setup_axis(ax,
               f"Run at {LOAD_KG} kg — Time vs Stamina by terrain\n"
               f"(flat road, grade=0%, run speed = {flat_run_speed:.2f} m/s)")
    save_fig(fig,
             f"Terrain comparison (load={LOAD_KG} kg, grade=0%)"
             " — optimized_rss_config_realism_super.json",
             SCRIPT_DIR / "run_stamina_by_terrain_30kg.png")

    print(f"\n=== Terrain (run_speed={flat_run_speed:.3f} m/s, grade=0%) ===")
    print(f"{'Terrain':>22} | {'Factor':>6} | {'Depleted':>10}")
    print("-" * 48)
    for r in results:
        print(f"  {r['name']:>20} |  {r['terrain_factor']:.2f}  | {r['time'][-1]:>8.1f} s")


# =========================================================================
# 图 3：风速（逆风增加消耗 + 顺风节省消耗）
# =========================================================================
def plot_by_wind(constants, twin, flat_run_speed, current_weight):
    import matplotlib.pyplot as plt

    wind_configs = [
        ("tailwind 20 m/s", 20, True),
        ("tailwind 10 m/s", 10, True),
        ("tailwind 5 m/s",   5, True),
        ("no wind",           0, False),
        ("headwind 5 m/s",   5, False),
        ("headwind 10 m/s", 10, False),
        ("headwind 15 m/s", 15, False),
        ("headwind 20 m/s", 20, False),
    ]
    results = []

    for name, ws, is_tail in wind_configs:
        wd = compute_wind_drag(constants, float(ws), tailwind=is_tail)
        twin.reset()
        setup_environment(twin)
        t_list, s_list = simulate_run(
            twin, flat_run_speed, current_weight,
            0.0, 1.0, wind_drag=wd)
        results.append({
            "name": name, "wind_mps": ws, "tailwind": is_tail,
            "wind_drag": wd, "time": t_list, "stamina": s_list,
        })

    fig, ax = plt.subplots(1, 1, figsize=(12, 7))
    n = len(wind_configs)
    colors = plt.cm.coolwarm([i / max(n - 1, 1) for i in range(n)])

    for i, r in enumerate(results):
        if not r["time"]:
            continue
        mult = 1 + r["wind_drag"]
        label = f"{r['name']} (x{mult:.2f})"
        ax.plot(r["time"], r["stamina"], color=colors[i],
                label=label, linewidth=1.8)

    t_max = max((r["time"][-1] for r in results if r["time"]), default=600.0)
    ax.set_xlim(0, t_max * 1.02)
    setup_axis(ax,
               f"Run at {LOAD_KG} kg — Time vs Stamina by wind\n"
               f"(paved, grade=0%, run={flat_run_speed:.2f} m/s)"
               f" | tailwind saves up to 15%, headwind costs up to 100%")
    save_fig(fig,
             f"Wind comparison (load={LOAD_KG} kg, grade=0%, terrain=1.0)"
             " — optimized_rss_config_realism_super.json",
             SCRIPT_DIR / "run_stamina_by_wind_30kg.png")

    print(f"\n=== Wind (run_speed={flat_run_speed:.3f} m/s, grade=0%) ===")
    print(f"{'Direction':>18} | {'Drag':>6} | {'Mult':>5} | {'Depleted':>10}")
    print("-" * 52)
    for r in results:
        print(f"  {r['name']:>16} | {r['wind_drag']:+.3f} |"
              f" x{1 + r['wind_drag']:.2f} | {r['time'][-1]:>8.1f} s")


# =========================================================================
# 图 4：各环境温度（含 AdjustEnergyForTemperature 消耗端补偿）
# =========================================================================
def plot_by_temperature(constants, twin, flat_run_speed, current_weight):
    import matplotlib.pyplot as plt

    temps = [-20, -10, 0, 10, 20, 30, 35, 40, 45]
    results = []

    for temp_c in temps:
        twin.reset()
        setup_environment(twin, temperature_celsius=float(temp_c))
        t_list, s_list = simulate_run(
            twin, flat_run_speed, current_weight, 0.0, 1.0)
        hs = twin.environment_factor.heat_stress
        cs = twin.environment_factor.cold_stress
        t_eff = temp_c - 1.35 * math.sqrt(0.0)
        results.append({
            "temp_c": temp_c, "t_eff": t_eff,
            "heat_stress": hs, "cold_stress": cs,
            "time": t_list, "stamina": s_list,
        })

    fig, ax = plt.subplots(1, 1, figsize=(12, 7))
    colors = plt.cm.RdYlBu_r(
        [i / max(len(temps) - 1, 1) for i in range(len(temps))])

    for i, r in enumerate(results):
        if not r["time"]:
            continue
        effect_parts = []
        if r["heat_stress"] > 0:
            effect_parts.append(f"heat +{r['heat_stress']:.0%}")
        if r["cold_stress"] > 0:
            effect_parts.append(f"cold recov -{r['cold_stress']:.0%}")
        if r["t_eff"] < 18.0:
            effect_parts.append(f"T_eff={r['t_eff']:.0f}")
        elif r["t_eff"] > 27.0:
            effect_parts.append(f"T_eff={r['t_eff']:.0f}")
        effect = ""
        if effect_parts:
            effect = " " + ", ".join(effect_parts)
        label = f"{r['temp_c']:+d} °C{effect}"
        ax.plot(r["time"], r["stamina"], color=colors[i],
                label=label, linewidth=1.8)

    t_max = max((r["time"][-1] for r in results if r["time"]), default=600.0)
    ax.set_xlim(0, t_max * 1.02)
    setup_axis(ax,
               f"Run at {LOAD_KG} kg — Time vs Stamina by temperature\n"
               f"(AdjustEnergyForTemperature: cold<18C & hot>27C add extra Watts)")
    save_fig(fig,
             f"Temperature comparison (load={LOAD_KG} kg, grade=0%, terrain=1.0)"
             " — optimized_rss_config_realism_super.json",
             SCRIPT_DIR / "run_stamina_by_temperature_30kg.png")

    print(f"\n=== Temperature (run_speed={flat_run_speed:.3f} m/s, grade=0%) ===")
    print(f"{'Temp':>6} | {'T_eff':>5} | {'Heat':>6} | {'Cold':>6} | {'Depleted':>10}")
    print("-" * 52)
    for r in results:
        print(f" {r['temp_c']:+3d} °C | {r['t_eff']:+5.0f} |  {r['heat_stress']:.2f}  |"
              f"  {r['cold_stress']:.2f}  | {r['time'][-1]:>8.1f} s")


# =========================================================================
# main
# =========================================================================
def main():
    json_path = SCRIPT_DIR / "optimized_rss_config_realism_super.json"
    if not json_path.exists():
        print(f"未找到配置: {json_path}")
        return 1

    try:
        import matplotlib
        matplotlib.use("Agg")
    except ImportError:
        print("需要 matplotlib，请安装: pip install matplotlib")
        return 1

    constants = load_constants_from_json(json_path)
    twin = RSSDigitalTwin(constants)

    current_weight = CHARACTER_WEIGHT + float(LOAD_KG)
    flat_run_speed = get_speed(LOAD_KG, "run")
    print(f"Load: {LOAD_KG} kg, total weight: {current_weight:.0f} kg,"
          f" flat run speed: {flat_run_speed:.3f} m/s\n")

    plot_by_grade(constants, twin, flat_run_speed, current_weight)
    plot_by_terrain(constants, twin, flat_run_speed, current_weight)
    plot_by_wind(constants, twin, flat_run_speed, current_weight)
    plot_by_temperature(constants, twin, flat_run_speed, current_weight)

    print("\nAll plots saved.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
