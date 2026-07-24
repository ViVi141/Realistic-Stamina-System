#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Standard 预设 @ 30kg：Walk → Run → Sprint 不间断连续拟真。"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from rss_pipeline_v4 import Mission, Phase, simulate_mission
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
# 不间断三段：步行稳态 → 跑步耗有氧/W′ → 冲刺抽干
WALK_S = 600.0
RUN_S = 600.0
SPRINT_S = 180.0


def fmt_t(sec: float) -> str:
    m = int(sec // 60)
    s = int(sec % 60)
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


def main() -> int:
    params = load_preset_params("StandardMilsim")
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    mission = Mission(
        name="Standard@30kg Walk→Run→Sprint 不间断",
        load_kg=LOAD_KG,
        phases=[
            Phase(WALK_S, 0.0, MovementType.WALK, Stance.STAND, 0.0, 1.0, "Walk"),
            Phase(RUN_S, 0.0, MovementType.RUN, Stance.STAND, 0.0, 1.0, "Run"),
            Phase(SPRINT_S, 0.0, MovementType.SPRINT, Stance.STAND, 0.0, 1.0, "Sprint"),
        ],
        description="平路 30kg，三相无 Idle",
    )

    print("=" * 72)
    print(
        f"预设=StandardMilsim  负重={LOAD_KG:.0f}kg  平路  "
        f"Walk {WALK_S:.0f}s → Run {RUN_S:.0f}s → Sprint {SPRINT_S:.0f}s"
    )
    print(
        f"CP0={params.get('critical_power_watts', 0):.0f}W  "
        f"W'max={params.get('w_prime_max_joules', 0):.0f}J  "
        f"e2s={params.get('energy_to_stamina_coeff', 0):.3e}"
    )
    print("=" * 72)

    twin.reset()
    twin.environment_factor.temperature = 20.0
    twin.environment_factor.wind_speed = 0.0
    twin.environment_factor.cold_stress = 0.0
    twin.environment_factor.cold_static_penalty = 0.0
    twin._scenario_wind_drag = 0.0

    dt = STAMINA_TICK_SEC
    t = 0.0
    weight = 90.0 + LOAD_KG
    rows = []
    phase_summaries = []

    for phase in mission.phases:
        steps = int(phase.duration_s / dt)
        sta0 = twin.stamina
        w0 = pool01(twin)
        v_sum = 0.0
        v_n = 0
        sta_min = sta0
        t_phase0 = t
        sample_every = max(1, int(30.0 / dt))
        step_i = 0

        for _ in range(steps):
            v = twin.game_player_tick(
                int(phase.movement),
                weight,
                phase.grade_pct,
                phase.terrain,
                phase.stance,
                t,
                dt,
                wind_drag=0.0,
                enable_randomness=False,
            )
            t += dt
            step_i += 1
            sta_min = min(sta_min, twin.stamina)
            v_sum += v
            v_n += 1
            if step_i % sample_every == 0 or step_i == 1 or step_i == steps:
                rows.append(
                    (
                        t,
                        phase.label,
                        twin.stamina,
                        pool01(twin),
                        armed(twin),
                        v,
                    )
                )

        phase_summaries.append(
            {
                "label": phase.label,
                "t0": t_phase0,
                "t1": t,
                "sta0": sta0,
                "sta1": twin.stamina,
                "sta_min": sta_min,
                "w0": w0,
                "w1": pool01(twin),
                "armed": armed(twin),
                "v_avg": v_sum / max(v_n, 1),
            }
        )

    print("\n--- 分段摘要 ---")
    for p in phase_summaries:
        d_sta = (p["sta1"] - p["sta0"]) * 100.0
        d_w = (p["w1"] - p["w0"]) * 100.0 if p["w0"] >= 0 else float("nan")
        print(
            f"[{p['label']:6}] {fmt_t(p['t0'])}-{fmt_t(p['t1'])}  "
            f"STA {p['sta0']*100:5.1f}%→{p['sta1']*100:5.1f}% "
            f"(Δ{d_sta:+.1f}, min {p['sta_min']*100:.1f}%)  "
            f"W' {p['w0']*100:5.1f}%→{p['w1']*100:5.1f}% (Δ{d_w:+.1f})  "
            f"armed={p['armed']}  v≈{p['v_avg']:.2f} m/s"
        )

    print("\n--- 时间轴采样（约每 30s）---")
    print(f"{'t':>7}  {'相':6}  {'STA%':>6}  {'Wprime%':>7}  {'armed':5}  {'v':>5}")
    for t_i, lab, sta, w, arm, v in rows:
        w_s = f"{w*100:5.1f}" if w >= 0 else "  n/a"
        print(
            f"{fmt_t(t_i):>7}  {lab:6}  {sta*100:6.1f}  {w_s:>7}  "
            f"{'yes' if arm else 'no':5}  {v:5.2f}"
        )

    # 也跑官方 MissionResult 校验连通
    twin2 = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    result = simulate_mission(twin2, mission)
    print("\n--- 总览 ---")
    print(
        f"总时长={result.total_duration_s:.0f}s  "
        f"最低STA={result.min_stamina*100:.1f}%  "
        f"运动均STA={result.mean_stamina_active*100:.1f}%  "
        f"全程可完成={result.completion_possible}  "
        f"观测掉条≈{result.observed_depletion_pct_per_s:.3f}%/s"
    )
    print("=" * 72)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
