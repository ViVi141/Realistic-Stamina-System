#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""文献/代谢锚点编译器：LCDA Walk 功率 → 最低 CP0 / 三档建议。"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List

from rss_digital_twin_fix import (
    MovementType,
    V6_CP_LOAD_DECAY_PER_KG,
    V6_CP_LOAD_REF_KG,
    metabolism_power_watts,
)

MARCH_LOAD_KG = 38.0
MARCH_SPEED_MS = 1.7
DEFAULT_DAMP = 0.70
DEFAULT_HEADROOM_W = 10.0


@dataclass
class MarchCpAnchor:
    load_kg: float
    speed_ms: float
    damp: float
    power_w: float
    min_cp0: float
    elite_cp0: float
    standard_cp0: float
    tactical_cp0: float


def march_metabolic_power_w(
    load_kg: float = MARCH_LOAD_KG,
    speed_ms: float = MARCH_SPEED_MS,
    damp: float = DEFAULT_DAMP,
) -> float:
    total_w = 90.0 + float(load_kg)
    return float(
        metabolism_power_watts(
            speed_ms,
            total_w,
            0.0,
            1.0,
            MovementType.WALK,
            float(damp),
        )
    )


def min_cp0_for_march_cruise(
    load_kg: float = MARCH_LOAD_KG,
    speed_ms: float = MARCH_SPEED_MS,
    damp: float = DEFAULT_DAMP,
    headroom_w: float = DEFAULT_HEADROOM_W,
) -> float:
    """CPeff = CP0 * (1 - decay * excess_load)；要求 CPeff >= P + headroom。"""
    try:
        from rss_sim_backend import get_rss_sim, use_rust_backend

        rss_sim = get_rss_sim()
        if rss_sim is not None and use_rust_backend() and hasattr(rss_sim, "min_cp0_for_march_cruise"):
            return float(
                rss_sim.min_cp0_for_march_cruise(
                    float(load_kg),
                    float(speed_ms),
                    float(damp),
                    float(headroom_w),
                )
            )
    except Exception:
        pass

    p_acct = march_metabolic_power_w(load_kg, speed_ms, damp)
    excess = max(0.0, float(load_kg) - V6_CP_LOAD_REF_KG)
    factor = max(0.05, 1.0 - V6_CP_LOAD_DECAY_PER_KG * excess)
    return (p_acct + float(headroom_w)) / factor


def compile_march_cp_anchors(
    load_kg: float = MARCH_LOAD_KG,
    speed_ms: float = MARCH_SPEED_MS,
    damp: float = DEFAULT_DAMP,
    headroom_w: float = DEFAULT_HEADROOM_W,
    elite_margin: float = 0.0,
    standard_extra: float = 30.0,
    tactical_extra: float = 70.0,
) -> MarchCpAnchor:
    """
    Elite = ceil(min_cp0 + elite_margin)
    Standard / Tactical = Elite + 阶梯余量（保持档位 CP 梯度）
    """
    power_w = march_metabolic_power_w(load_kg, speed_ms, damp)
    min_cp0 = min_cp0_for_march_cruise(load_kg, speed_ms, damp, headroom_w)
    elite = float(int(min_cp0 + elite_margin + 0.999))
    standard = elite + float(standard_extra)
    tactical = elite + float(tactical_extra)
    return MarchCpAnchor(
        load_kg=float(load_kg),
        speed_ms=float(speed_ms),
        damp=float(damp),
        power_w=power_w,
        min_cp0=min_cp0,
        elite_cp0=elite,
        standard_cp0=standard,
        tactical_cp0=tactical,
    )


def anchors_summary_lines(anchor: MarchCpAnchor = None) -> List[str]:
    a = anchor if anchor is not None else compile_march_cp_anchors()
    return [
        f"march {a.load_kg:.0f}kg @ {a.speed_ms:.2f} m/s (damp={a.damp:.2f})",
        f"  LCDA Walk power P={a.power_w:.1f} W",
        f"  min_cp0={a.min_cp0:.1f} W",
        f"  suggested Elite={a.elite_cp0:.0f} Standard={a.standard_cp0:.0f} "
        f"Tactical={a.tactical_cp0:.0f}",
    ]


def suggested_v6_defaults(anchor: MarchCpAnchor = None) -> Dict[str, float]:
    a = anchor if anchor is not None else compile_march_cp_anchors()
    return {
        "critical_power_watts": float(a.standard_cp0),
        "_elite_critical_power_watts": float(a.elite_cp0),
        "_tactical_critical_power_watts": float(a.tactical_cp0),
        "_march_power_w": float(a.power_w),
        "_min_cp0": float(a.min_cp0),
    }
