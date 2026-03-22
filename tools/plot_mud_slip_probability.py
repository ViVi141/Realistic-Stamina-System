# -*- coding: utf-8 -*-
"""
与模组 SCR_MudSlipEffects.c + GetTerrainFactorFromDensity 对齐的滑倒单步概率估算。

场景默认：负重 30 kg + 雨湿重 8 kg → RCOF 负重项按 38 kg（与 encumbrance 上限 55 一致）；
水平速度 5.0 m/s、冲刺、站立、体力 100%、无转弯/落地/起跳随机项。

单步概率：P = min(1 - exp(-lambda * dt), 0.6)，dt=0.05 s（玩家约 50 ms）。
lambda = K_phys * gap * K_global（线性 gap，与游戏内 MudSlipEffects 一致）；非铺装泥泞 ×1.18。
掷骰门槛：gap > ENV_SLIP_GAP_EPSILON_DICE（与 C 端一致）；镜头预警另用更小的 ε_cam，本脚本不绘制镜头。

输出两套：v=5 m/s 冲刺（文件名默认）与 v=2 m/s 行走非冲刺（前缀 v2_walk_），用于对照「降速则 P_1s 回落」。

体感：单步 P 需与「每秒约 n=1/dt 次独立判定」复合对照——
P_1s = 1 - (1-P)^n 表示约 1 秒内至少滑倒一次（脚本另存 *_1s_at_least_one.png）。

依赖：numpy, matplotlib
  pip install numpy matplotlib
"""

from __future__ import annotations

import math
import os

import numpy as np

# --- 与 SCR_StaminaConstants.c 对齐（修改常量时请同步 C 与 docs/泥泞滑倒判定模型.md）---
ENV_MUD_SLIPPERY_THRESHOLD = 0.3
ENV_MUD_PENALTY_MAX = 0.4
ENV_MUD_SLIP_GLOBAL_SCALE = 1.5
ENV_MUD_SLIP_MIN_SPEED_MS = 1.6
ENV_SLIP_ACOF_DRY_BASE = 0.52
ENV_SLIP_ACOF_OFFROAD_DROP = 0.07
ENV_SLIP_LUBRICATION_MAX = 0.62
ENV_SLIP_ACOF_MIN = 0.07
ENV_SLIP_ACOF_MAX = 0.58
ENV_SLIP_RCOF_BASE = 0.13
ENV_SLIP_RCOF_VSQ = 0.0055
ENV_SLIP_RCOF_SPRINT = 0.0406
ENV_SLIP_RCOF_WEIGHT = 0.065
ENV_SLIP_RCOF_CROUCH_MULT = 0.86
ENV_SLIP_RCOF_SLOPE_PER_DEG = 0.0026
ENV_SLIP_RCOF_SLOPE_MAX = 0.095
ENV_SLIP_RCOF_SLOPE_DOWNHILL_MULT = 1.14
ENV_SLIP_RCOF_SLOPE_UPHILL_MULT = 0.98
ENV_SLIP_GAP_EPSILON_CAMERA = 0.004
ENV_SLIP_GAP_EPSILON_DICE = 0.008
ENV_MUD_SLIP_PHYS_SCALE = 2.2
ENV_SLIP_RCOF_BALANCE_STAMINA = 0.085
ENV_SLIP_RCOF_TURN_OMEGA_V_COEFF = 0.0030
ENV_SLIP_RCOF_TURN_MAX = 0.07
ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC = 12.0

TERRAIN_FACTOR_PAVED = 1.0

# 玩家速度更新间隔（秒）与 PlayerBase 主循环量级一致
DT_SEC = 0.05
# 与 dt 对应的「每秒判定次数」（独立近似下用于复合概率）
TICKS_PER_SEC = int(round(1.0 / DT_SEC))
PROB_CAP = 0.6

# 场景默认
ENC_KG = 30.0 + 8.0
H_SPEED = 5.0
IS_CROUCH = False
STAMINA = 1.0
TURN_RATE_RAD_S = 0.0


def clamp(x: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, x))


def get_terrain_factor_from_density(density: float) -> float:
    """与 SCR_RealisticStaminaSystem.GetTerrainFactorFromDensity 一致。"""
    if density <= 0.0:
        return TERRAIN_FACTOR_PAVED
    if density <= 0.36:
        t = density / 0.36
        return 1.12 + (1.0 - t) * 0.08
    if density <= 0.72:
        return TERRAIN_FACTOR_PAVED
    if density <= 1.13:
        return TERRAIN_FACTOR_PAVED
    if density <= 1.2:
        t = (density - 1.13) / (1.2 - 1.13)
        return 1.0 + t * 0.2
    if density <= 1.33:
        t = (density - 1.2) / (1.33 - 1.2)
        return 1.2 - t * 0.1
    if density <= 1.55:
        t = (density - 1.33) / (1.55 - 1.33)
        return 1.1 + t * 0.28
    if density <= 1.86:
        if density <= 1.63:
            t = (density - 1.55) / (1.63 - 1.55)
            return 1.38 + t * 0.3
        if density <= 1.79:
            t = (density - 1.63) / (1.79 - 1.63)
            return 1.68 + t * 0.06
        t2 = (density - 1.79) / (1.86 - 1.79)
        return 1.74 + t2 * (1.6 - 1.74)
    if density < 2.2:
        t = (density - 1.86) / (2.2 - 1.86)
        return 1.6 - t * 0.6
    if density <= 2.42:
        return TERRAIN_FACTOR_PAVED
    if density <= 2.76:
        t = (density - 2.42) / (2.76 - 2.42)
        return 1.0 + t * 0.48
    if density <= 2.94:
        t = (density - 2.76) / (2.94 - 2.76)
        return 1.48 + t * 0.32
    return clamp(1.8 + (density - 2.94) * 0.08, 1.8, 2.5)


def compute_acof(eta: float, mud: float) -> float:
    """ComputeAvailableCof：mud 为 GetMudFactor()，即地表泥泞度 0~1。"""
    mu_dry = ENV_SLIP_ACOF_DRY_BASE
    if eta > 1.0:
        t = min(eta - 1.0, 1.0)
        mu_dry = mu_dry - t * ENV_SLIP_ACOF_OFFROAD_DROP
    lub = 0.0
    thr = ENV_MUD_SLIPPERY_THRESHOLD
    if mud > thr:
        lub = (mud - thr) / (1.0 - thr)
    lub = clamp(lub, 0.0, 1.0)
    mu = mu_dry * (1.0 - ENV_SLIP_LUBRICATION_MAX * lub)
    return clamp(mu, ENV_SLIP_ACOF_MIN, ENV_SLIP_ACOF_MAX)


def turn_add(omega: float, horizontal_speed: float) -> float:
    w = min(max(0.0, omega), ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC)
    vmin = ENV_MUD_SLIP_MIN_SPEED_MS
    v = max(horizontal_speed, vmin)
    ta = w * v * ENV_SLIP_RCOF_TURN_OMEGA_V_COEFF
    return min(ta, ENV_SLIP_RCOF_TURN_MAX)


def compute_rcof(
    horizontal_speed: float,
    encumbrance_kg: float,
    slope_angle_deg_signed: float,
    turn_rate_rad_s: float,
    stamina_percent: float,
    is_sprint: bool,
) -> float:
    vmin = ENV_MUD_SLIP_MIN_SPEED_MS
    ex = max(0.0, horizontal_speed - vmin)
    rcof = ENV_SLIP_RCOF_BASE + ENV_SLIP_RCOF_VSQ * ex * ex
    if is_sprint:
        rcof = rcof + ENV_SLIP_RCOF_SPRINT
    enc = clamp(encumbrance_kg, 0.0, 55.0)
    rcof = rcof + ENV_SLIP_RCOF_WEIGHT * (enc / 55.0)
    if IS_CROUCH:
        rcof = rcof * ENV_SLIP_RCOF_CROUCH_MULT
    slope_mag = abs(slope_angle_deg_signed)
    slope_add = ENV_SLIP_RCOF_SLOPE_PER_DEG * slope_mag
    if slope_add > ENV_SLIP_RCOF_SLOPE_MAX:
        slope_add = ENV_SLIP_RCOF_SLOPE_MAX
    if slope_angle_deg_signed < 0.0:
        slope_dir_mult = ENV_SLIP_RCOF_SLOPE_DOWNHILL_MULT
    else:
        if slope_angle_deg_signed > 0.0:
            slope_dir_mult = ENV_SLIP_RCOF_SLOPE_UPHILL_MULT
        else:
            slope_dir_mult = 1.0
    slope_add = slope_add * slope_dir_mult
    rcof = rcof + slope_add
    rcof = rcof + turn_add(turn_rate_rad_s, horizontal_speed)
    st = clamp(stamina_percent, 0.0, 1.0)
    rcof = rcof + ENV_SLIP_RCOF_BALANCE_STAMINA * (1.0 - st)
    return rcof


def mud_terrain_factor(eta: float, mud: float) -> float:
    """与 EnvironmentFactor.CalculateMudTerrainFactor：铺装 eta<=1 时为 0。"""
    if eta <= 1.0:
        return 0.0
    return mud * ENV_MUD_PENALTY_MAX


def compute_lambda_from_gap(gap: float, mtf: float) -> float:
    g = max(0.0, gap)
    lam = ENV_MUD_SLIP_PHYS_SCALE * g * ENV_MUD_SLIP_GLOBAL_SCALE
    if mtf > 0.0:
        lam = lam * 1.18
    return lam


def slip_risk_positive(mud: float) -> bool:
    """GetSlipRisk()>0 等价：mud >= slippery threshold。"""
    return mud >= ENV_MUD_SLIPPERY_THRESHOLD


def prob_one_tick(
    density: float,
    mud: float,
    slope_deg_signed: float,
    enc_kg: float = ENC_KG,
    h_speed: float = H_SPEED,
    is_sprint: bool = True,
) -> float:
    """
    单更新步内发生滑倒的概率（泊松近似）。
    若未过泥泞门槛或 v 过低或 gap<=ENV_SLIP_GAP_EPSILON_DICE，返回 0。
    """
    if not slip_risk_positive(mud):
        return 0.0
    if h_speed < ENV_MUD_SLIP_MIN_SPEED_MS:
        return 0.0
    eta = get_terrain_factor_from_density(density)
    acof = compute_acof(eta, mud)
    rcof = compute_rcof(
        h_speed, enc_kg, slope_deg_signed, TURN_RATE_RAD_S, STAMINA, is_sprint
    )
    gap = rcof - acof
    if gap <= ENV_SLIP_GAP_EPSILON_DICE:
        return 0.0
    mtf = mud_terrain_factor(eta, mud)
    lam = compute_lambda_from_gap(gap, mtf)
    p = 1.0 - math.exp(-lam * DT_SEC)
    return min(p, PROB_CAP)


def compound_at_least_one_in_one_second(p: np.ndarray | float) -> np.ndarray:
    """
    独立近似：n 次判定/秒，1 秒内至少一次滑倒概率 P_1s = 1 - (1-P)^n。
    与 docs/泥泞滑倒判定模型.md §6.1 一致。
    """
    arr = np.asarray(p, dtype=np.float64)
    arr = np.clip(arr, 0.0, 1.0)
    return 1.0 - np.power(1.0 - arr, float(TICKS_PER_SEC))


def main() -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.colors import Normalize

    plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"] = False

    out_dir = os.path.join(os.path.dirname(__file__), "figures")
    os.makedirs(out_dir, exist_ok=True)

    n_h = 80
    n_d = 80
    n_s = 90
    mud_axis = np.linspace(0.0, 1.0, n_h)
    den_axis = np.linspace(0.25, 3.0, n_d)
    slope_axis = np.linspace(-25.0, 25.0, n_s)
    rho_fix = 1.33
    mud_fix = 0.65

    scenarios = [
        (5.0, True, "", "v=5 m/s 冲刺"),
        (2.0, False, "v2_walk_", "v=2 m/s 行走（非冲刺）"),
    ]

    written: list[str] = []
    for h_spd, is_sp, prefix, spd_label in scenarios:

        def pt(den: float, mud: float, slo: float) -> float:
            return prob_one_tick(
                den, mud, slo, ENC_KG, h_spd, is_sp
            )

        Z1 = np.zeros((n_h, n_d))
        for i, mud in enumerate(mud_axis):
            for j, den in enumerate(den_axis):
                Z1[i, j] = pt(float(den), float(mud), 0.0)

        fig1, ax1 = plt.subplots(figsize=(10, 6))
        im1 = ax1.imshow(
            Z1,
            origin="lower",
            aspect="auto",
            extent=[den_axis[0], den_axis[-1], mud_axis[0], mud_axis[-1]],
            cmap="inferno",
            norm=Normalize(vmin=0, vmax=PROB_CAP),
        )
        ax1.set_xlabel("弹道密度 rho (约 g/cm³)")
        ax1.set_ylabel("地表泥泞度 m (0~1，湿度/积水)")
        ax1.set_title(
            "单步滑倒概率 P（dt=50 ms）| 负重 30+8 kg，%s，平地，体力 100%%"
            % spd_label
        )
        plt.colorbar(im1, ax=ax1, label="P（单步）")
        fig1.tight_layout()
        fp1 = os.path.join(out_dir, prefix + "mud_slip_density_vs_wetness_flat.png")
        fig1.savefig(fp1, dpi=160)
        plt.close(fig1)
        written.append(fp1)

        Z1_1s = compound_at_least_one_in_one_second(Z1)
        fig1b, ax1b = plt.subplots(figsize=(10, 6))
        im1b = ax1b.imshow(
            Z1_1s,
            origin="lower",
            aspect="auto",
            extent=[den_axis[0], den_axis[-1], mud_axis[0], mud_axis[-1]],
            cmap="Reds",
            norm=Normalize(vmin=0, vmax=1.0),
        )
        ax1b.set_xlabel("弹道密度 rho (约 g/cm³)")
        ax1b.set_ylabel("地表泥泞度 m (0~1，湿度/积水)")
        ax1b.set_title(
            "约 1 秒内至少滑倒一次 P_1s | n=%d，%s，其余同左图"
            % (TICKS_PER_SEC, spd_label)
        )
        plt.colorbar(im1b, ax=ax1b, label="P_1s")
        fig1b.tight_layout()
        fp1b = os.path.join(
            out_dir, prefix + "mud_slip_density_vs_wetness_flat_1s_at_least_one.png"
        )
        fig1b.savefig(fp1b, dpi=160)
        plt.close(fig1b)
        written.append(fp1b)

        Z2 = np.zeros((n_h, n_s))
        for i, mud in enumerate(mud_axis):
            for j, slo in enumerate(slope_axis):
                Z2[i, j] = pt(rho_fix, float(mud), float(slo))

        fig2, ax2 = plt.subplots(figsize=(10, 6))
        im2 = ax2.imshow(
            Z2,
            origin="lower",
            aspect="auto",
            extent=[slope_axis[0], slope_axis[-1], mud_axis[0], mud_axis[-1]],
            cmap="inferno",
            norm=Normalize(vmin=0, vmax=PROB_CAP),
        )
        ax2.set_xlabel("沿运动方向坡度 (°)：正=上坡，负=下坡")
        ax2.set_ylabel("地表泥泞度 m (0~1)")
        ax2.set_title(
            "单步滑倒概率 P | rho=%.2f（土质），30+8 kg，%s" % (rho_fix, spd_label)
        )
        plt.colorbar(im2, ax=ax2, label="P（单步）")
        fig2.tight_layout()
        fp2 = os.path.join(out_dir, prefix + "mud_slip_slope_vs_wetness_rho133.png")
        fig2.savefig(fp2, dpi=160)
        plt.close(fig2)
        written.append(fp2)

        Z2_1s = compound_at_least_one_in_one_second(Z2)
        fig2b, ax2b = plt.subplots(figsize=(10, 6))
        im2b = ax2b.imshow(
            Z2_1s,
            origin="lower",
            aspect="auto",
            extent=[slope_axis[0], slope_axis[-1], mud_axis[0], mud_axis[-1]],
            cmap="Reds",
            norm=Normalize(vmin=0, vmax=1.0),
        )
        ax2b.set_xlabel("沿运动方向坡度 (°)：正=上坡，负=下坡")
        ax2b.set_ylabel("地表泥泞度 m (0~1)")
        ax2b.set_title(
            "约 1 秒内至少滑倒一次 P_1s | rho=%.2f，n=%d，%s"
            % (rho_fix, TICKS_PER_SEC, spd_label)
        )
        plt.colorbar(im2b, ax=ax2b, label="P_1s")
        fig2b.tight_layout()
        fp2b = os.path.join(
            out_dir, prefix + "mud_slip_slope_vs_wetness_rho133_1s_at_least_one.png"
        )
        fig2b.savefig(fp2b, dpi=160)
        plt.close(fig2b)
        written.append(fp2b)

        Z3 = np.zeros((n_s, n_d))
        for i, slo in enumerate(slope_axis):
            for j, den in enumerate(den_axis):
                Z3[i, j] = pt(float(den), mud_fix, float(slo))

        fig3, ax3 = plt.subplots(figsize=(10, 6))
        im3 = ax3.imshow(
            Z3,
            origin="lower",
            aspect="auto",
            extent=[den_axis[0], den_axis[-1], slope_axis[0], slope_axis[-1]],
            cmap="inferno",
            norm=Normalize(vmin=0, vmax=PROB_CAP),
        )
        ax3.set_xlabel("弹道密度 rho")
        ax3.set_ylabel("坡度 (°)：正=上坡，负=下坡")
        ax3.set_title(
            "单步滑倒概率 P | m=%.2f，30+8 kg，%s" % (mud_fix, spd_label)
        )
        plt.colorbar(im3, ax=ax3, label="P（单步）")
        fig3.tight_layout()
        fp3 = os.path.join(out_dir, prefix + "mud_slip_density_vs_slope_m065.png")
        fig3.savefig(fp3, dpi=160)
        plt.close(fig3)
        written.append(fp3)

        Z3_1s = compound_at_least_one_in_one_second(Z3)
        fig3b, ax3b = plt.subplots(figsize=(10, 6))
        im3b = ax3b.imshow(
            Z3_1s,
            origin="lower",
            aspect="auto",
            extent=[den_axis[0], den_axis[-1], slope_axis[0], slope_axis[-1]],
            cmap="Reds",
            norm=Normalize(vmin=0, vmax=1.0),
        )
        ax3b.set_xlabel("弹道密度 rho")
        ax3b.set_ylabel("坡度 (°)：正=上坡，负=下坡")
        ax3b.set_title(
            "约 1 秒内至少滑倒一次 P_1s | m=%.2f，n=%d，%s"
            % (mud_fix, TICKS_PER_SEC, spd_label)
        )
        plt.colorbar(im3b, ax=ax3b, label="P_1s")
        fig3b.tight_layout()
        fp3b = os.path.join(
            out_dir, prefix + "mud_slip_density_vs_slope_m065_1s_at_least_one.png"
        )
        fig3b.savefig(fp3b, dpi=160)
        plt.close(fig3b)
        written.append(fp3b)

    ex_p = 0.1
    ex_q = (1.0 - ex_p) ** TICKS_PER_SEC
    ex_p1s = 1.0 - ex_q
    for path in written:
        print(path)
    print(
        "Compound (indep. approx): P_step=%.2f -> Q_no_slip_in_1s=%.3f, P_at_least_one_in_1s=%.3f (n=%d)"
        % (ex_p, ex_q, ex_p1s, TICKS_PER_SEC)
    )


if __name__ == "__main__":
    main()
