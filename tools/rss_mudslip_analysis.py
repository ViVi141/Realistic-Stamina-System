#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
泥泞滑倒概率分析脚本
对照 C 端 SCR_MudSlipEffects.c + SCR_StaminaConstants.c 完整实现摩擦阈值模型。
输出：单参数曲线 + 双参数热力图，用于调参和定量理解。

运行：python rss_mudslip_analysis.py
"""

import numpy as np
import math
from dataclasses import dataclass
from typing import Tuple

# =============================================================================
# 常量 — 与 SCR_StaminaConstants.c 严格一致
# =============================================================================
@dataclass(frozen=True)
class MudSlipConstants:
    # ACOF
    ACOF_DRY_BASE: float = 0.52
    ACOF_OFFROAD_DROP: float = 0.07
    LUBRICATION_MAX: float = 0.62
    ACOF_MIN: float = 0.07
    ACOF_MAX: float = 0.58

    # RCOF 基线
    RCOF_BASE: float = 0.20
    RCOF_VSQ: float = 0.0080
    RCOF_SPRINT: float = 0.058
    RCOF_WEIGHT: float = 0.065   # 0~55kg
    RCOF_CROUCH_MULT: float = 0.86

    # 坡度
    RCOF_SLOPE_PER_DEG: float = 0.0026
    RCOF_SLOPE_MAX: float = 0.095
    RCOF_SLOPE_DOWNHILL_MULT: float = 1.14
    RCOF_SLOPE_UPHILL_MULT: float = 0.98

    # 急转
    RCOF_TURN_OMEGA_V_COEFF: float = 0.0030
    RCOF_TURN_MAX: float = 0.07
    RCOF_TURN_RATE_CAP_RADSEC: float = 12.0

    # 体力
    RCOF_BALANCE_STAMINA: float = 0.085
    RCOF_BALANCE_JITTER: float = 0.045

    # 落地/起跳
    RCOF_LANDING: float = 0.11
    LANDING_VY_PREV: float = -1.15
    LANDING_VY_CUR: float = -0.42
    RCOF_JUMP_UP_VY: float = 1.65
    RCOF_JUMP_UP: float = 0.042

    # 缺口→事件率
    GAP_EPSILON_DICE: float = 0.012
    GAP_EPSILON_CAMERA: float = 0.004
    MUD_SLIP_PHYS_SCALE: float = 2.2
    MUD_SLIP_GLOBAL_SCALE: float = 1.5
    OFFROAD_LAMBDA_MULT: float = 1.18

    # 其他
    MIN_SPEED_MS: float = 1.6
    MUD_SLIPPERY_THRESHOLD: float = 0.3
    SLIP_RISK_BASE: float = 0.005
    COOLDOWN_SEC: float = 9.0

C = MudSlipConstants()


# =============================================================================
# 核心模型 — 与 C 端一一对应
# =============================================================================

def compute_acof(mud_factor: float, terrain_factor: float) -> float:
    """可用摩擦系数。mud=泥泞度 0-1, terrain_factor=地形系数(1.0铺装)"""
    mu_dry = C.ACOF_DRY_BASE
    if terrain_factor > 1.0:
        t = min(terrain_factor - 1.0, 1.0)
        mu_dry -= t * C.ACOF_OFFROAD_DROP

    lub = 0.0
    if mud_factor > C.MUD_SLIPPERY_THRESHOLD:
        lub = (mud_factor - C.MUD_SLIPPERY_THRESHOLD) / (1.0 - C.MUD_SLIPPERY_THRESHOLD)
    lub = np.clip(lub, 0.0, 1.0)

    mu = mu_dry * (1.0 - C.LUBRICATION_MAX * lub)
    return np.clip(mu, C.ACOF_MIN, C.ACOF_MAX)


def compute_rcof(
    speed_ms: float,
    is_sprint: bool,
    encumbrance_kg: float,
    is_crouch: bool,
    slope_deg: float,
    turn_rate_rad_s: float,
    stamina: float,
    vy: float,
    prev_vy: float,
    apply_jitter: bool = False,
    jitter_rand: float = 0.0,
) -> float:
    """所需摩擦系数。返回值越低越安全，越高越易滑倒。"""
    ex = max(speed_ms - C.MIN_SPEED_MS, 0.0)
    rcof = C.RCOF_BASE
    rcof += C.RCOF_VSQ * ex * ex
    if is_sprint:
        rcof += C.RCOF_SPRINT

    enc = np.clip(encumbrance_kg, 0.0, 55.0)
    rcof += C.RCOF_WEIGHT * (enc / 55.0)

    if is_crouch:
        rcof *= C.RCOF_CROUCH_MULT

    slope_mag = abs(slope_deg)
    slope_add = min(C.RCOF_SLOPE_PER_DEG * slope_mag, C.RCOF_SLOPE_MAX)
    if slope_deg < 0:
        slope_add *= C.RCOF_SLOPE_DOWNHILL_MULT
    elif slope_deg > 0:
        slope_add *= C.RCOF_SLOPE_UPHILL_MULT
    rcof += slope_add

    omega = max(turn_rate_rad_s, 0.0)
    omega = min(omega, C.RCOF_TURN_RATE_CAP_RADSEC)
    v_turn = max(speed_ms, C.MIN_SPEED_MS)
    turn_add = min(omega * v_turn * C.RCOF_TURN_OMEGA_V_COEFF, C.RCOF_TURN_MAX)
    rcof += turn_add

    stamina_def = max(1.0 - stamina, 0.0)
    rcof += C.RCOF_BALANCE_STAMINA * stamina_def
    if apply_jitter and stamina_def > 0.01:
        rcof += jitter_rand * C.RCOF_BALANCE_JITTER * stamina_def

    if prev_vy < C.LANDING_VY_PREV and vy > C.LANDING_VY_CUR:
        rcof += C.RCOF_LANDING
    if vy > C.RCOF_JUMP_UP_VY:
        rcof += C.RCOF_JUMP_UP

    return rcof


def compute_slip_probability(
    acof: float,
    rcof: float,
    dt: float = 1.0 / 60.0,
    offroad: bool = False,
) -> float:
    """单帧滑倒概率（Poisson 过程）。dt 默认 16.7ms (60fps)。"""
    gap = rcof - acof
    if gap <= C.GAP_EPSILON_DICE:
        return 0.0

    lam = C.MUD_SLIP_PHYS_SCALE * gap
    lam *= C.MUD_SLIP_GLOBAL_SCALE
    if offroad:
        lam *= C.OFFROAD_LAMBDA_MULT

    return min(1.0 - math.exp(-lam * dt), 0.6)


def per_second_probability(
    acof: float,
    rcof: float,
    offroad: bool = False,
) -> float:
    """每秒等效滑倒概率（60 帧复合）。"""
    gap = rcof - acof
    if gap <= C.GAP_EPSILON_DICE:
        return 0.0

    lam = C.MUD_SLIP_PHYS_SCALE * gap * C.MUD_SLIP_GLOBAL_SCALE
    if offroad:
        lam *= C.OFFROAD_LAMBDA_MULT

    # 每秒 Poisson 复合
    p_frame = min(1.0 - math.exp(-lam * (1.0 / 60.0)), 0.6)
    if p_frame <= 0:
        return 0.0
    return 1.0 - (1.0 - p_frame) ** 60


# =============================================================================
# 默认场景
# =============================================================================
@dataclass
class DefaultScene:
    speed: float = 3.0
    sprint: bool = False
    encumbrance: float = 20.0
    crouch: bool = False
    slope: float = 0.0
    turn_rate: float = 0.0
    stamina: float = 1.0
    vy: float = 0.0
    prev_vy: float = 0.0
    mud_factor: float = 0.6
    terrain_factor: float = 1.0
    offroad: bool = False

DEFAULT = DefaultScene()


def evaluate(scene: DefaultScene) -> Tuple[float, float, float, float]:
    """返回 (ACOF, RCOF, gap, P_per_sec)"""
    acof = compute_acof(scene.mud_factor, scene.terrain_factor)
    rcof = compute_rcof(
        scene.speed, scene.sprint, scene.encumbrance,
        scene.crouch, scene.slope, scene.turn_rate,
        scene.stamina, scene.vy, scene.prev_vy,
    )
    gap = rcof - acof
    p = per_second_probability(acof, rcof, scene.offroad)
    return acof, rcof, gap, p


# =============================================================================
# 绘图
# =============================================================================
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib import cm
    HAVE_MPL = True
except ImportError:
    HAVE_MPL = False


def _safe_sqrt(x):
    return np.sqrt(np.maximum(x, 0))


def _sweep_1d(name: str, param_values: np.ndarray, scene: DefaultScene, param_setter):
    """单参数扫描"""
    acof_arr = np.zeros_like(param_values)
    rcof_arr = np.zeros_like(param_values)
    gap_arr = np.zeros_like(param_values)
    p_arr = np.zeros_like(param_values)
    for i, v in enumerate(param_values):
        s = DefaultScene(
            speed=scene.speed, sprint=scene.sprint,
            encumbrance=scene.encumbrance, crouch=scene.crouch,
            slope=scene.slope, turn_rate=scene.turn_rate,
            stamina=scene.stamina, vy=scene.vy, prev_vy=scene.prev_vy,
            mud_factor=scene.mud_factor, terrain_factor=scene.terrain_factor,
            offroad=scene.offroad,
        )
        param_setter(s, v)
        acof_arr[i], rcof_arr[i], gap_arr[i], p_arr[i] = evaluate(s)
    return acof_arr, rcof_arr, gap_arr, p_arr


def _sweep_2d(name_x, vals_x, setter_x, name_y, vals_y, setter_y, scene):
    """双参数网格扫描，返回概率 2D 数组"""
    P = np.zeros((len(vals_y), len(vals_x)))
    G = np.zeros((len(vals_y), len(vals_x)))
    for j, vy in enumerate(vals_y):
        for i, vx in enumerate(vals_x):
            s = DefaultScene(
                speed=scene.speed, sprint=scene.sprint,
                encumbrance=scene.encumbrance, crouch=scene.crouch,
                slope=scene.slope, turn_rate=scene.turn_rate,
                stamina=scene.stamina, vy=scene.vy, prev_vy=scene.prev_vy,
                mud_factor=scene.mud_factor, terrain_factor=scene.terrain_factor,
                offroad=scene.offroad,
            )
            setter_x(s, vx)
            setter_y(s, vy)
            _, _, gap, p = evaluate(s)
            P[j, i] = p
            G[j, i] = gap
    return P, G


def plot_all(output_path: str = "mudslip_analysis.png"):
    """生成全部分析图"""
    if not HAVE_MPL:
        print("[WARN] matplotlib 未安装，跳过绘图。仅输出数值。")
        return

    fig = plt.figure(figsize=(20, 18))
    gs = fig.add_gridspec(4, 3, hspace=0.35, wspace=0.30)

    # 基准场景参数
    base = DEFAULT

    # ── 1. 单参数曲线 ──
    # 行 1-2: 速度, 泥泞度, 负重, 体力, 坡度, 转向率
    param_sets = [
        ("Speed (m/s)", np.linspace(1.6, 7.0, 50),
         lambda s, v: setattr(s, "speed", v), "c", (0, 0)),
        ("Mud Factor", np.linspace(0, 1.0, 50),
         lambda s, v: setattr(s, "mud_factor", v), "orange", (0, 1)),
        ("Encumbrance (kg)", np.linspace(0, 55, 50),
         lambda s, v: setattr(s, "encumbrance", v), "g", (0, 2)),
        ("Stamina %", np.linspace(1.0, 0.0, 50),
         lambda s, v: setattr(s, "stamina", v), "r", (1, 0)),
        ("Slope (deg, +up/-down)", np.linspace(-30, 30, 50),
         lambda s, v: setattr(s, "slope", v), "purple", (1, 1)),
        ("Turn Rate (rad/s)", np.linspace(0, 12, 50),
         lambda s, v: setattr(s, "turn_rate", v), "brown", (1, 2)),
    ]

    for label, values, setter, color, pos in param_sets:
        ax = fig.add_subplot(gs[pos[0], pos[1]])
        acof, rcof, gap, p = _sweep_1d(label, values, base, setter)
        ax2 = ax.twinx()
        l1 = ax.plot(values, p * 100, color=color, lw=2, label="P(slip)/s %")
        ax.set_xlabel(label)
        ax.set_ylabel("P(slip) %", color=color)
        ax.tick_params(axis="y", labelcolor=color)
        ax2.plot(values, gap, "k--", lw=1, alpha=0.5, label="gap")
        ax2.set_ylabel("gap (RCOF-ACOF)", color="k")
        ax2.tick_params(axis="y", labelcolor="k")
        ax2.axhline(y=0.012, color="gray", ls=":", lw=0.8)
        ax.set_title(label, fontsize=10)
        ax.grid(True, alpha=0.3)

    # ── 2. 双参数热力图 ──
    heatmaps = [
        ("Speed × Mud", np.linspace(1.6, 7.0, 40), lambda s, v: setattr(s, "speed", v),
         np.linspace(0, 1.0, 40), lambda s, v: setattr(s, "mud_factor", v),
         (2, 0), "viridis", "P(slip)/s %"),
        ("Speed × Encumbrance", np.linspace(1.6, 7.0, 40), lambda s, v: setattr(s, "speed", v),
         np.linspace(0, 55, 40), lambda s, v: setattr(s, "encumbrance", v),
         (2, 1), "plasma", "P(slip)/s %"),
        ("Stamina × Mud", np.linspace(0, 1.0, 40), lambda s, v: setattr(s, "stamina", v),
         np.linspace(0, 1.0, 40), lambda s, v: setattr(s, "mud_factor", v),
         (2, 2), "inferno", "P(slip)/s %"),
        ("Speed × Slope", np.linspace(1.6, 7.0, 40), lambda s, v: setattr(s, "speed", v),
         np.linspace(-30, 30, 40), lambda s, v: setattr(s, "slope", v),
         (3, 0), "coolwarm", "P(slip)/s %"),
        ("Turn × Mud", np.linspace(0, 12, 40), lambda s, v: setattr(s, "turn_rate", v),
         np.linspace(0, 1.0, 40), lambda s, v: setattr(s, "mud_factor", v),
         (3, 1), "magma", "P(slip)/s %"),
        ("Encumbrance × Stamina", np.linspace(0, 55, 40), lambda s, v: setattr(s, "encumbrance", v),
         np.linspace(0, 1.0, 40), lambda s, v: setattr(s, "stamina", v),
         (3, 2), "Spectral_r", "P(slip)/s %"),
    ]

    for label, xs, sx, ys, sy, pos, cmap, clabel in heatmaps:
        ax = fig.add_subplot(gs[pos[0], pos[1]])
        P, G = _sweep_2d(label, xs, sx, label, ys, sy, base)
        im = ax.pcolormesh(xs, ys, P * 100, cmap=cmap, shading="auto")
        cbar = fig.colorbar(im, ax=ax, shrink=0.8)
        cbar.set_label(clabel, fontsize=8)
        ax.set_xlabel(label.split("×")[0].strip())
        ax.set_ylabel(label.split("×")[1].strip())
        ax.set_title(label, fontsize=10)

    fig.suptitle("RSS Mud Slip Probability Analysis", fontsize=14, y=1.01)
    fig.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"[OK] 图表已保存: {output_path}")


# =============================================================================
# 主入口
# =============================================================================
def main():
    print("=" * 65)
    print("RSS Mud Slip Probability Analysis")
    print("=" * 65)

    # 1. 打印基准场景
    acof, rcof, gap, p = evaluate(DEFAULT)
    print(f"\n基准场景:")
    print(f"  speed={DEFAULT.speed}m/s, mud={DEFAULT.mud_factor}, "
          f"encumbrance={DEFAULT.encumbrance}kg, stamina={DEFAULT.stamina}")
    print(f"  sprint={DEFAULT.sprint}, crouch={DEFAULT.crouch}, slope={DEFAULT.slope}deg")
    print(f"  ACOF={acof:.4f},  RCOF={rcof:.4f},  gap={gap:.4f}")
    print(f"  P(slip)/s={p*100:.2f}%")

    # 2. 参数灵敏度表 — 使用"临界"基线和"全恶"基线
    # 临界基线: gap≈0（接近触发阈值），可观察单参数微调的影响
    MARGIN = DefaultScene(
        speed=5.5, sprint=True, encumbrance=30, mud_factor=0.8,
        stamina=0.5, turn_rate=4.0, terrain_factor=1.0)
    _, _, gap_margin, p_margin = evaluate(MARGIN)

    for baseline_name, baseline in [("临界基线", MARGIN), ("基准场景", DEFAULT)]:
        print(f"\n参数灵敏度（{baseline_name}, gap={evaluate(baseline)[2]:.4f}, P/s={evaluate(baseline)[3]*100:.2f}%）:")
        print(f"{'参数':<22} {'值域':<18} {'Pmin':>8} {'Pmax':>8} {'ΔP':>8}")
        print("-" * 65)
        sens_params = [
            ("Speed (m/s)", np.linspace(1.6, 7.0, 50), lambda s, v: setattr(s, "speed", v)),
            ("Mud factor", np.linspace(0, 1.0, 50), lambda s, v: setattr(s, "mud_factor", v)),
            ("Encumbrance (kg)", np.linspace(0, 55, 50), lambda s, v: setattr(s, "encumbrance", v)),
            ("Stamina", np.linspace(1.0, 0.0, 50), lambda s, v: setattr(s, "stamina", v)),
            ("Slope (deg)", np.linspace(-30, 30, 50), lambda s, v: setattr(s, "slope", v)),
            ("Turn rate (rad/s)", np.linspace(0, 12, 50), lambda s, v: setattr(s, "turn_rate", v)),
            ("Terrain factor", np.linspace(1.0, 3.0, 50), lambda s, v: setattr(s, "terrain_factor", v)),
        ]
        for label, values, setter in sens_params:
            _, _, _, p_vals = _sweep_1d(label, values, baseline, setter)
            pmin = p_vals.min() * 100
            pmax = p_vals.max() * 100
            vmin = values[0]
            vmax = values[-1]
            print(f"  {label:<20} [{vmin:<7.1f} ~ {vmax:<7.1f}] {pmin:>7.3f}% {pmax:>7.3f}% {pmax-pmin:>7.3f}%")

    # 3. 场景组合分析
    print(f"\n场景组合分析:")
    print(f"{'场景':<28} {'ACOF':>6} {'RCOF':>6} {'gap':>6} {'P/s':>8}")
    print("-" * 58)

    scenarios = [
        ("铺装路慢走(1.6m/s 10kg)", DefaultScene(speed=1.6, encumbrance=10, mud_factor=0, terrain_factor=1.0)),
        ("铺装路快跑(5m/s 20kg)", DefaultScene(speed=5.0, encumbrance=20, mud_factor=0, terrain_factor=1.0)),
        ("泥地行走(3m/s 20kg)", DefaultScene(speed=3.0, encumbrance=20, mud_factor=0.7, terrain_factor=1.0)),
        ("泥地冲刺(5.5m/s 20kg sprint)", DefaultScene(speed=5.5, sprint=True, encumbrance=20, mud_factor=0.7, terrain_factor=1.0)),
        ("泥地下坡(3m/s 20kg -10deg)", DefaultScene(speed=3.0, encumbrance=20, mud_factor=0.7, slope=-10, terrain_factor=1.0)),
        ("泥地急转(3m/s 20kg turn=6)", DefaultScene(speed=3.0, encumbrance=20, mud_factor=0.7, turn_rate=6.0, terrain_factor=1.0)),
        ("泥地低体力(3m/s 20kg stamina=0.2)", DefaultScene(speed=3.0, encumbrance=20, mud_factor=0.7, stamina=0.2, terrain_factor=1.0)),
        ("泥地重装(3m/s 40kg)", DefaultScene(speed=3.0, encumbrance=40, mud_factor=0.7, terrain_factor=1.0)),
        ("草地泥泞(3m/s 20kg tf=1.2)", DefaultScene(speed=3.0, encumbrance=20, mud_factor=0.7, terrain_factor=1.2)),
        ("泥地全恶(冲刺40kg低体急转)", DefaultScene(speed=5.0, sprint=True, encumbrance=40, mud_factor=0.9,
                                         stamina=0.15, turn_rate=8.0, terrain_factor=1.3)),
        ("泥地蹲姿(3m/s 20kg crouch)", DefaultScene(speed=3.0, encumbrance=20, mud_factor=0.7, crouch=True, terrain_factor=1.0)),
    ]

    for label, scene in scenarios:
        acof, rcof, gap, p = evaluate(scene)
        print(f"  {label:<26} {acof:>6.3f} {rcof:>6.3f} {gap:>6.4f} {p*100:>7.3f}%")

    # 4. 生成图表
    plot_all()

    # 5. 冷知识
    print(f"\n关键指标:")
    # 多少 gap 开始有非零概率
    gap_thresh = C.GAP_EPSILON_DICE
    lam_at_thresh = C.MUD_SLIP_PHYS_SCALE * gap_thresh * C.MUD_SLIP_GLOBAL_SCALE
    p_at_thresh = 1.0 - math.exp(-lam_at_thresh * (1/60))
    print(f"  gap > {gap_thresh:.3f} 才开始 Poisson 过程")
    print(f"  gap = {gap_thresh:.3f} 时 λ={lam_at_thresh:.4f}, 单帧 P={p_at_thresh*100:.3f}%")
    print(f"  冷却时间: {C.COOLDOWN_SEC}s (滑倒后完全免疫)")
    print(f"  最高单帧概率: 60%（硬上限）")

    print("=" * 65)


if __name__ == "__main__":
    main()
