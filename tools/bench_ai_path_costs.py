#!/usr/bin/env python3
"""Bench RSS AI / stamina path costs (link-level).

Compares relative CPU of server-side paths that matter for multi-AI hitching:

  light_only          DisableAIStaminaCalc=On  (cheap speed, no drain)
  drain_cheap_speed   6.2.27 Speed=Off         (cheap speed + Pandolf/W')
  legacy_heavy_speed  pre-fix Speed=Off        (UpdateSpeed+terrain+env+CP cap)
  combat_behavior     AI combat FSM apply       (IntentFilter/CombatDecay proxy)
  lod_uncached        nearest-player scan       (alloc + N players every tick)
  lod_cached          shared 0.25s origin cache

Also projects wall time for N agents using distance LOD intervals.

Usage (PowerShell, from repo root or tools/):

  python tools/bench_ai_path_costs.py
  python tools/bench_ai_path_costs.py --agents 150 --iterations 5000
  python tools/bench_ai_path_costs.py --json tools/artifacts/diagnostics/ai_path_bench.json

Notes:
  - This Python script is a *relative* model for CI / offline comparison.
  - For real Enforce timings in Workbench/game Script Console, use:
      SCR_RSS_PerfProbe.Run();
      SCR_RSS_PerfProbe.Run(3000);
    Output: script log + $profile:RSS_PerfProbe.txt
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence, Tuple

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

# ---------------------------------------------------------------------------
# Twin imports (optional — bench still runs synthetic-only if twin fails)
# ---------------------------------------------------------------------------

_TWIN_OK = False
_metabolism_power_watts = None
_stamina_drain_from_power = None
_get_metabolic_speed_cap_ms = None
_calculate_pandolf_power_watts = None

try:
    from rss_digital_twin_fix import (  # type: ignore
        calculate_pandolf_power_watts,
        get_metabolic_speed_cap_ms,
        metabolism_power_watts,
        stamina_drain_rate_per_second_from_power_watts,
    )

    _metabolism_power_watts = metabolism_power_watts
    _stamina_drain_from_power = stamina_drain_rate_per_second_from_power_watts
    _get_metabolic_speed_cap_ms = get_metabolic_speed_cap_ms
    _calculate_pandolf_power_watts = calculate_pandolf_power_watts
    _TWIN_OK = True
except Exception as exc:  # noqa: BLE001 — bench must not die on twin import
    print(f"[bench] twin import skipped: {exc}")


# ---------------------------------------------------------------------------
# Timing helpers
# ---------------------------------------------------------------------------


@dataclass
class PathTiming:
    name: str
    label_zh: str
    iterations: int
    total_sec: float
    ns_per_call: float
    ms_per_1k_calls: float
    relative_to_light: float = 1.0
    notes: str = ""


@dataclass
class ScenarioProjection:
    name: str
    agents: int
    ticks_per_sec_effective: float
    calls_per_sec: float
    ms_per_sec_cpu: float
    pct_of_16ms_frame: float


@dataclass
class BenchReport:
    twin_available: bool
    timings: List[PathTiming] = field(default_factory=list)
    projections: List[ScenarioProjection] = field(default_factory=list)
    meta: Dict = field(default_factory=dict)


def _time_callable(fn: Callable[[], None], iterations: int, warmup: int) -> float:
    for _ in range(max(0, warmup)):
        fn()
    t0 = time.perf_counter()
    for _ in range(iterations):
        fn()
    return time.perf_counter() - t0


def _make_timing(
    name: str,
    label_zh: str,
    fn: Callable[[], None],
    iterations: int,
    warmup: int,
    notes: str = "",
) -> PathTiming:
    total = _time_callable(fn, iterations, warmup)
    ns = (total / max(iterations, 1)) * 1e9
    return PathTiming(
        name=name,
        label_zh=label_zh,
        iterations=iterations,
        total_sec=total,
        ns_per_call=ns,
        ms_per_1k_calls=ns / 1e6 * 1000.0,
        notes=notes,
    )


# ---------------------------------------------------------------------------
# Synthetic script-op kernels (relative cost model)
# ---------------------------------------------------------------------------

# Calibrated so ratios roughly track Enforce hotspots (not absolute game ms).
_SYN_ITERS = {
    "apply_speed_limit": 80,
    "phase_speed_mult": 40,
    "encumbrance_poll": 120,
    "pos_delta_speed": 60,
    "terrain_ray": 900,
    "env_update": 700,
    "update_speed_coord": 1100,
    "cp_metab_cap": 500,
    "find_component": 150,
    "intent_filter": 2000,
    "combat_decay": 600,
}


def _busy(n: int) -> float:
    """CPU busy-work (float ops); keeps GIL busy like script math."""
    x = 1.0
    for i in range(n):
        x = math.sin(x + i * 0.001) * 0.999 + 1.000001
    return x


def _kernel_apply_speed() -> None:
    _busy(_SYN_ITERS["apply_speed_limit"])


def _kernel_phase_mult() -> None:
    _busy(_SYN_ITERS["phase_speed_mult"])


def _kernel_encumbrance() -> None:
    _busy(_SYN_ITERS["encumbrance_poll"])


def _kernel_pos_delta() -> None:
    _busy(_SYN_ITERS["pos_delta_speed"])


def _kernel_terrain_ray() -> None:
    _busy(_SYN_ITERS["terrain_ray"])


def _kernel_env_update() -> None:
    _busy(_SYN_ITERS["env_update"])


def _kernel_update_speed() -> None:
    _busy(_SYN_ITERS["update_speed_coord"])


def _kernel_cp_cap() -> None:
    _busy(_SYN_ITERS["cp_metab_cap"])


def _kernel_find_component() -> None:
    _busy(_SYN_ITERS["find_component"])


def _kernel_intent_filter() -> None:
    _busy(_SYN_ITERS["intent_filter"])


def _kernel_combat_decay() -> None:
    _busy(_SYN_ITERS["combat_decay"])


def _kernel_player_scan(n_players: int) -> None:
    ids = list(range(n_players))
    origin = (10.0, 0.0, -5.0)
    best = 1e18
    for i in ids:
        dx = origin[0] - float(i)
        dz = origin[2] - float(i) * 0.5
        d2 = dx * dx + dz * dz
        if d2 < best:
            best = d2
    _busy(40 + n_players * 8)


# ---------------------------------------------------------------------------
# Twin kernels
# ---------------------------------------------------------------------------


def _kernel_twin_metabolism() -> None:
    if not _TWIN_OK:
        _busy(200)
        return
    assert _metabolism_power_watts is not None
    _metabolism_power_watts(
        velocity_ms=3.5,
        total_weight_kg=100.0,
        grade_percent=5.0,
        terrain_factor=1.2,
        movement_phase=2,
    )


def _kernel_twin_drain_from_power() -> None:
    if not _TWIN_OK:
        _busy(80)
        return
    assert _stamina_drain_from_power is not None
    _stamina_drain_from_power(450.0, 320.0)


def _kernel_twin_pandolf() -> None:
    if not _TWIN_OK:
        _busy(180)
        return
    assert _calculate_pandolf_power_watts is not None
    _calculate_pandolf_power_watts(3.5, 100.0, 5.0, 1.2)


def _kernel_twin_metab_cap() -> None:
    if not _TWIN_OK:
        _busy(350)
        return
    assert _get_metabolic_speed_cap_ms is not None
    _get_metabolic_speed_cap_ms(
        current_speed_ms=3.8,
        movement_phase=2,
        total_weight_kg=100.0,
        grade_percent=5.0,
        terrain_factor=1.2,
        is_exhausted=False,
        effective_cp_watts=320.0,
        w_prime_pool01=0.4,
    )


# ---------------------------------------------------------------------------
# Composed paths (match Enforce control flow)
# ---------------------------------------------------------------------------


def path_light_only() -> None:
    """DisableAIStaminaCalc=On: encumbrance + phase mult + SetSpeedLimit."""
    _kernel_encumbrance()
    _kernel_phase_mult()
    _kernel_apply_speed()


def path_drain_cheap_speed() -> None:
    """6.2.27: cheap speed + pos delta + twin drain (+ W' stand-in)."""
    path_light_only()
    _kernel_pos_delta()
    _kernel_twin_metabolism()
    _kernel_twin_drain_from_power()
    _busy(220)


def path_legacy_heavy_speed() -> None:
    """Pre-fix Speed=Off: full Phase A speed stack + drain."""
    path_light_only()
    _kernel_pos_delta()
    _kernel_terrain_ray()
    _kernel_env_update()
    _kernel_update_speed()
    _kernel_cp_cap()
    _kernel_twin_metabolism()
    _kernel_twin_drain_from_power()
    _kernel_twin_metab_cap()
    _busy(220)


def path_combat_behavior() -> None:
    """AIManager behavior tick when state changes."""
    _kernel_find_component()
    _kernel_intent_filter()
    _kernel_combat_decay()
    _busy(100)


def path_lod_uncached(n_players: int) -> Callable[[], None]:
    def _fn() -> None:
        _kernel_player_scan(n_players)

    return _fn


def path_lod_cached(n_players: int) -> Callable[[], None]:
    """Cache hit: almost free; miss every ~4 light ticks at 0.25s TTL."""

    state = {"i": 0}

    def _fn() -> None:
        state["i"] += 1
        if state["i"] % 4 == 1:
            _kernel_player_scan(n_players)
        else:
            _busy(12)

    return _fn


# ---------------------------------------------------------------------------
# Multi-AI projection
# ---------------------------------------------------------------------------

_LOD_MS = {
    "full_near": 600,
    "full_mid": 1000,
    "full_far": 2500,
    "light_near": 800,
    "light_mid": 1500,
    "light_far": 3000,
}


def _effective_hz(interval_ms: int, stagger_ms: int = 90) -> float:
    return 1000.0 / (interval_ms + stagger_ms * 0.5)


def project_load(
    path_ms_per_call: float,
    agents: int,
    near_frac: float,
    mid_frac: float,
    far_frac: float,
    near_ms: int,
    mid_ms: int,
    far_ms: int,
    name: str,
) -> ScenarioProjection:
    n_near = int(round(agents * near_frac))
    n_mid = int(round(agents * mid_frac))
    n_far = max(0, agents - n_near - n_mid)
    calls = (
        n_near * _effective_hz(near_ms)
        + n_mid * _effective_hz(mid_ms)
        + n_far * _effective_hz(far_ms)
    )
    ms_sec = calls * path_ms_per_call
    return ScenarioProjection(
        name=name,
        agents=agents,
        ticks_per_sec_effective=calls / max(agents, 1),
        calls_per_sec=calls,
        ms_per_sec_cpu=ms_sec,
        pct_of_16ms_frame=(ms_sec / 1000.0) / 0.016 * 100.0,
    )


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------


def _print_table(timings: Sequence[PathTiming]) -> None:
    print()
    print("=== Path micro-bench (per call) ===")
    hdr = (
        f"{'path':<22} {'zh':<28} {'ns/call':>12} "
        f"{'ms/1k':>10} {'rel':>8}  notes"
    )
    print(hdr)
    print("-" * len(hdr))
    for t in timings:
        print(
            f"{t.name:<22} {t.label_zh:<28} {t.ns_per_call:12.0f} "
            f"{t.ms_per_1k_calls:10.3f} {t.relative_to_light:8.2f}x  {t.notes}"
        )


def _print_projections(projs: Sequence[ScenarioProjection]) -> None:
    print()
    print("=== Multi-AI CPU projection (single thread script budget) ===")
    print(
        f"{'scenario':<42} {'agents':>6} {'calls/s':>10} "
        f"{'ms CPU/s':>10} {'% of 16ms':>10}"
    )
    print("-" * 82)
    for p in projs:
        print(
            f"{p.name:<42} {p.agents:6d} {p.calls_per_sec:10.1f} "
            f"{p.ms_per_sec_cpu:10.2f} {p.pct_of_16ms_frame:9.1f}%"
        )
    print()
    print(
        "pct_of_16ms: estimated share of one 60 FPS frame if all RSS AI work "
        "ran on one script thread (>>100% means hitching)."
    )


def run_bench(
    iterations: int,
    warmup: int,
    agents: int,
    players: int,
    near_frac: float,
    mid_frac: float,
) -> BenchReport:
    report = BenchReport(
        twin_available=_TWIN_OK,
        meta={
            "iterations": iterations,
            "warmup": warmup,
            "agents": agents,
            "players": players,
            "near_frac": near_frac,
            "mid_frac": mid_frac,
            "version_target": "6.2.27",
        },
    )

    specs: List[Tuple[str, str, Callable[[], None], str]] = [
        ("light_only", "轻量限速(Speed禁用On)", path_light_only, "enc+phase+SetSpeedLimit"),
        (
            "drain_cheap_speed",
            "廉价限速+消耗(6.2.27)",
            path_drain_cheap_speed,
            "light+pos+Pandolf+W'",
        ),
        (
            "legacy_heavy_speed",
            "旧版全量Speed链",
            path_legacy_heavy_speed,
            "terrain+env+UpdateSpeed+CP",
        ),
        (
            "combat_behavior",
            "战斗行为层(状态变更)",
            path_combat_behavior,
            "IntentFilter+CombatDecay",
        ),
        (
            "lod_uncached",
            "距离LOD无缓存",
            path_lod_uncached(players),
            f"GetPlayers x{players}",
        ),
        (
            "lod_cached",
            "距离LOD共享缓存",
            path_lod_cached(players),
            "TTL~0.25s",
        ),
        ("twin_metabolism", "孪生 metabolism_power", _kernel_twin_metabolism, "math only"),
        ("twin_metab_cap", "孪生 CP speed cap", _kernel_twin_metab_cap, "math only"),
    ]

    timings: List[PathTiming] = []
    for name, zh, fn, notes in specs:
        timings.append(_make_timing(name, zh, fn, iterations, warmup, notes))

    light = next(t for t in timings if t.name == "light_only")
    light_ns = max(light.ns_per_call, 1.0)
    for t in timings:
        t.relative_to_light = t.ns_per_call / light_ns

    report.timings = timings

    by_name = {t.name: t for t in timings}
    ms = {k: v.ns_per_call / 1e6 for k, v in by_name.items()}

    far_frac = max(0.0, 1.0 - near_frac - mid_frac)
    scenarios = [
        (
            "A light_only @ light LOD",
            ms["light_only"],
            _LOD_MS["light_near"],
            _LOD_MS["light_mid"],
            _LOD_MS["light_far"],
        ),
        (
            "B drain_cheap_speed @ full LOD (6.2.27)",
            ms["drain_cheap_speed"],
            _LOD_MS["full_near"],
            _LOD_MS["full_mid"],
            _LOD_MS["full_far"],
        ),
        (
            "C legacy_heavy_speed @ full LOD (old)",
            ms["legacy_heavy_speed"],
            _LOD_MS["full_near"],
            _LOD_MS["full_mid"],
            _LOD_MS["full_far"],
        ),
        (
            "D B + combat every near tick",
            ms["drain_cheap_speed"] + ms["combat_behavior"] * 0.35,
            _LOD_MS["full_near"],
            _LOD_MS["full_mid"],
            _LOD_MS["full_far"],
        ),
        (
            "E C + combat (worst old config)",
            ms["legacy_heavy_speed"] + ms["combat_behavior"] * 0.5,
            400,
            700,
            2000,
        ),
    ]

    for name, path_ms, n_ms, m_ms, f_ms in scenarios:
        report.projections.append(
            project_load(
                path_ms_per_call=path_ms,
                agents=agents,
                near_frac=near_frac,
                mid_frac=mid_frac,
                far_frac=far_frac,
                near_ms=n_ms,
                mid_ms=m_ms,
                far_ms=f_ms,
                name=name,
            )
        )

    return report


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Bench RSS AI path costs (light / drain / legacy speed / combat / LOD)."
    )
    parser.add_argument("--iterations", type=int, default=3000, help="Calls per path")
    parser.add_argument("--warmup", type=int, default=200, help="Warmup calls per path")
    parser.add_argument("--agents", type=int, default=120, help="AI count for projection")
    parser.add_argument("--players", type=int, default=8, help="Players for LOD scan cost")
    parser.add_argument(
        "--near-frac",
        type=float,
        default=0.45,
        help="Fraction of AI within near LOD (<=400m)",
    )
    parser.add_argument(
        "--mid-frac",
        type=float,
        default=0.35,
        help="Fraction of AI in mid LOD",
    )
    parser.add_argument(
        "--json",
        type=str,
        default="",
        help="Optional JSON output path",
    )
    args = parser.parse_args(argv)

    if args.near_frac + args.mid_frac > 1.0:
        print("error: near-frac + mid-frac must be <= 1.0")
        return 2

    print(f"twin_available={_TWIN_OK}")
    print(
        f"iterations={args.iterations} warmup={args.warmup} "
        f"agents={args.agents} players={args.players}"
    )

    report = run_bench(
        iterations=args.iterations,
        warmup=args.warmup,
        agents=args.agents,
        players=args.players,
        near_frac=args.near_frac,
        mid_frac=args.mid_frac,
    )

    _print_table(report.timings)
    _print_projections(report.projections)

    by_name = {t.name: t for t in report.timings}
    light = by_name["light_only"].ns_per_call
    cheap = by_name["drain_cheap_speed"].ns_per_call
    legacy = by_name["legacy_heavy_speed"].ns_per_call
    print("=== Speed path takeaway ===")
    print(f"  legacy / light      = {legacy / light:.2f}x")
    print(f"  drain_cheap / light = {cheap / light:.2f}x")
    print(f"  legacy / drain_cheap = {legacy / max(cheap, 1.0):.2f}x  (Speed hitch driver)")
    print()

    if args.json:
        out = Path(args.json)
        if not out.is_absolute():
            out = ROOT / out
        out.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "twin_available": report.twin_available,
            "meta": report.meta,
            "timings": [asdict(t) for t in report.timings],
            "projections": [asdict(p) for p in report.projections],
        }
        out.write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        print(f"wrote {out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
