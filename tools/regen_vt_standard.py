#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Regenerate StandardMilsim @ 30kg V-T data, PNG, and Cursor canvas."""
from __future__ import annotations

import json
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

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
PRESET = "StandardMilsim"
EMPTY_STA = 0.01
MAX_S = 6 * 3600
WEIGHT = 90.0 + LOAD_KG


def run_series(movement: int, sample_every_s: float):
    params = load_preset_params(PRESET)
    twin = RSSDigitalTwin(RSSConstants(**merge_game_aligned_params(params)))
    twin.reset()
    twin.environment_factor.temperature = 20.0
    twin.environment_factor.wind_speed = 0.0
    twin.environment_factor.cold_stress = 0.0
    twin.environment_factor.cold_static_penalty = 0.0
    twin._scenario_wind_drag = 0.0
    dt = STAMINA_TICK_SEC
    t = 0.0
    points = []
    step = 0
    sample_n = max(1, int(sample_every_s / dt))
    v_sum = 0.0
    n = 0
    t_empty = None
    while t < MAX_S:
        v = twin.game_player_tick(
            movement, WEIGHT, 0.0, 1.0, Stance.STAND, t, dt
        )
        if step % sample_n == 0 or step == 0:
            points.append({"t": round(t, 2), "v": round(v, 3)})
        t += dt
        step += 1
        v_sum += v
        n += 1
        if twin.stamina <= EMPTY_STA:
            t_empty = t
            points.append({"t": round(t, 2), "v": round(v, 3)})
            break
    return {
        "t_empty": t_empty,
        "v_avg": v_sum / max(n, 1),
        "points": points,
    }


def sample_at(points, t_query: float) -> float:
    if not points:
        return 0.0
    i = 0
    while i + 1 < len(points) and points[i + 1]["t"] <= t_query:
        i += 1
    return points[i]["v"]


def build_early(full: dict, tmax: float = 90.0, step: float = 2.0):
    cats, walk, run, sprint = [], [], [], []
    t = 0.0
    while t <= tmax + 1e-9:
        cats.append(str(int(t)) if abs(t - int(t)) < 1e-9 else f"{t:g}")
        walk.append(round(sample_at(full["Walk"]["points"], t), 3))
        run.append(round(sample_at(full["Run"]["points"], t), 3))
        sprint.append(round(sample_at(full["Sprint"]["points"], t), 3))
        t += step
    return {"cats": cats, "Walk": walk, "Run": run, "Sprint": sprint}


def build_short(full: dict, tmax_min: float = 32.0, step_min: float = 0.5):
    cats, walk, run, sprint = [], [], [], []
    t_min = 0.0
    while t_min <= tmax_min + 1e-9:
        t = t_min * 60.0
        cats.append(f"{t_min:g}")
        for name, arr in (
            ("Walk", walk),
            ("Run", run),
            ("Sprint", sprint),
        ):
            pts = full[name]["points"]
            t_end = full[name]["t_empty"] or tmax_min * 60.0
            if t > t_end:
                arr.append(round(pts[-1]["v"], 3))
            else:
                arr.append(round(sample_at(pts, t), 3))
        t_min += step_min
    return {"cats": cats, "Walk": walk, "Run": run, "Sprint": sprint}


def fmt_tte(sec: float) -> str:
    if sec >= 3600:
        h = int(sec // 3600)
        m = int((sec % 3600) // 60)
        s = int(sec % 60)
        return f"{h}h{m:02d}m{s:02d}s"
    m = int(sec // 60)
    s = int(sec % 60)
    return f"{m:02d}:{s:02d}"


def main() -> int:
    print("Simulating Walk / Run / Sprint ...")
    full = {
        "Walk": run_series(MovementType.WALK, 30.0),
        "Run": run_series(MovementType.RUN, 2.0),
        "Sprint": run_series(MovementType.SPRINT, 1.0),
    }
    for name, data in full.items():
        print(
            f"  {name}: TTE={fmt_tte(data['t_empty'])} "
            f"v_avg={data['v_avg']:.3f} n={len(data['points'])}"
        )

    early = build_early(full)
    short = build_short(full)
    embed = {
        "meta": {"preset": PRESET, "load_kg": LOAD_KG, "grade": 0},
        "tte": {k: round(full[k]["t_empty"], 1) for k in full},
        "v_avg": {k: round(full[k]["v_avg"], 3) for k in full},
        "early90s": early,
        "short32": short,
    }

    embed_path = os.path.join(HERE, "_vt_canvas_embed.json")
    with open(embed_path, "w", encoding="utf-8") as f:
        json.dump(embed, f)

    raw_path = os.path.join(HERE, "_vt_standard_30kg.json")
    with open(raw_path, "w", encoding="utf-8") as f:
        json.dump(
            {
                "meta": embed["meta"],
                "full": {
                    k: {
                        "t_empty": full[k]["t_empty"],
                        "v_avg": full[k]["v_avg"],
                        "points": full[k]["points"],
                    }
                    for k in full
                },
            },
            f,
        )

    out_png = os.path.join(HERE, "vt_standard_30kg.png")
    fig, axes = plt.subplots(2, 1, figsize=(10, 7.2), constrained_layout=True)
    ax = axes[0]
    t = [float(x) for x in early["cats"]]
    ax.plot(t, early["Walk"], label="Walk", color="#2a6f4e", lw=1.8)
    ax.plot(t, early["Run"], label="Run", color="#2f5d8c", lw=1.8)
    ax.plot(t, early["Sprint"], label="Sprint", color="#a33b2b", lw=1.8)
    ax.set_ylabel("v (m/s)")
    ax.set_xlabel("t (s)")
    ax.set_title("Early V-T (0-90 s)")
    ax.set_ylim(0, 3.4)
    ax.grid(True, alpha=0.3)
    ax.legend(frameon=False)

    ax = axes[1]
    tm = [float(x) for x in short["cats"]]
    ax.plot(tm, short["Walk"], label="Walk", color="#2a6f4e", lw=1.6)
    ax.plot(tm, short["Run"], label="Run", color="#2f5d8c", lw=1.6)
    ax.plot(tm, short["Sprint"], label="Sprint", color="#a33b2b", lw=1.6)
    ax.axhline(2.2, color="#888888", ls="--", lw=0.9, label="Run CP floor 2.2")
    ax.set_ylabel("v (m/s)")
    ax.set_xlabel("t (min)")
    ax.set_title("V-T to ~STA empty window (0-32 min)")
    ax.set_ylim(0, 3.4)
    ax.grid(True, alpha=0.3)
    ax.legend(frameon=False, loc="upper right")

    tte = embed["tte"]
    fig.suptitle(
        "RSS v6 StandardMilsim @ 30 kg flat — continuous gait V-T\n"
        f"TTE STA<=1%: Walk {tte['Walk']/3600:.2f} h | "
        f"Run {tte['Run']/60:.1f} min | Sprint {tte['Sprint']/60:.1f} min",
        fontsize=11,
    )
    fig.savefig(out_png, dpi=140)
    print("PNG:", out_png)

    canvas_path = os.path.join(
        os.path.expanduser("~"),
        ".cursor",
        "projects",
        "c-Users-74738-Documents-My-Games-ArmaReforgerWorkbench-addons-Realistic-Stamina-System",
        "canvases",
        "standard-vt-chart.canvas.tsx",
    )
    os.makedirs(os.path.dirname(canvas_path), exist_ok=True)
    parts = []
    parts.append(
        """import {
  Card,
  CardBody,
  CardHeader,
  Divider,
  Grid,
  H1,
  H2,
  LineChart,
  Stack,
  Stat,
  Text,
} from "cursor/canvas";
"""
    )
    parts.append(f"const EARLY_CATS = {json.dumps(early['cats'])};\n")
    parts.append(f"const EARLY_WALK = {json.dumps(early['Walk'])};\n")
    parts.append(f"const EARLY_RUN = {json.dumps(early['Run'])};\n")
    parts.append(f"const EARLY_SPRINT = {json.dumps(early['Sprint'])};\n")
    parts.append(f"const SHORT_CATS = {json.dumps(short['cats'])};\n")
    parts.append(f"const SHORT_WALK = {json.dumps(short['Walk'])};\n")
    parts.append(f"const SHORT_RUN = {json.dumps(short['Run'])};\n")
    parts.append(f"const SHORT_SPRINT = {json.dumps(short['Sprint'])};\n")
    parts.append(
        """
export default function StandardVtChart() {
  return (
    <Stack gap={20} style={{ padding: 20 }}>
      <Stack gap={6}>
        <H1>Standard preset — gait V-T</H1>
        <Text tone="secondary" size="small">
          Source: RSS digital twin · StandardMilsim · 30 kg · flat · continuous
          gait until STA &lt;= 1% · post optimize-tiers re-export
        </Text>
      </Stack>

      <Grid columns={3} gap={12}>
"""
    )
    parts.append(
        f'        <Stat label="Walk TTE / mean v" value="{fmt_tte(tte["Walk"])}" '
        f'unit="{embed["v_avg"]["Walk"]:.2f} m/s" />\n'
    )
    parts.append(
        f'        <Stat label="Run TTE / mean v" value="{fmt_tte(tte["Run"])}" '
        f'unit="{embed["v_avg"]["Run"]:.2f} m/s" />\n'
    )
    parts.append(
        f'        <Stat label="Sprint TTE / mean v" value="{fmt_tte(tte["Sprint"])}" '
        f'unit="{embed["v_avg"]["Sprint"]:.2f} m/s" />\n'
    )
    parts.append(
        """      </Grid>

      <Card>
        <CardHeader
          title="Early V-T (0-90 s)"
          subtitle="Sprint W-prime step-down then Run CP cruise floor"
        />
        <CardBody>
          <LineChart
            categories={EARLY_CATS}
            series={[
              { name: "Walk", data: EARLY_WALK, tone: "success" },
              { name: "Run", data: EARLY_RUN, tone: "info" },
              { name: "Sprint", data: EARLY_SPRINT, tone: "danger" },
            ]}
            height={260}
            beginAtZero
            yMax={3.4}
            valueSuffix=" m/s"
            referenceLines={[{ value: 2.2, label: "Run CP floor", tone: "neutral" }]}
          />
          <Text tone="secondary" size="small" style={{ marginTop: 8 }}>
            X: time (s) · Y: applied velocity (m/s)
          </Text>
        </CardBody>
      </Card>

      <Card>
        <CardHeader
          title="Session V-T (0-32 min)"
          subtitle="Walk steady; Run/Sprint cruise then limp near STA empty"
        />
        <CardBody>
          <LineChart
            categories={SHORT_CATS}
            series={[
              { name: "Walk", data: SHORT_WALK, tone: "success" },
              { name: "Run", data: SHORT_RUN, tone: "info" },
              { name: "Sprint", data: SHORT_SPRINT, tone: "danger" },
            ]}
            height={280}
            beginAtZero
            yMax={3.4}
            valueSuffix=" m/s"
            referenceLines={[{ value: 2.2, label: "Run CP floor", tone: "neutral" }]}
          />
          <Text tone="secondary" size="small" style={{ marginTop: 8 }}>
            X: time (min) · Y: applied velocity (m/s)
          </Text>
        </CardBody>
      </Card>

      <Divider />
      <H2>Readout</H2>
      <Stack gap={6}>
        <Text>
          After W-prime disarm, Sprint demotes to the same Run CP cruise (no late
          speed-up). Floor reference 2.2 m/s.
        </Text>
        <Text tone="secondary" size="small">
          Static PNG: tools/vt_standard_30kg.png
        </Text>
      </Stack>
    </Stack>
  );
}
"""
    )
    with open(canvas_path, "w", encoding="utf-8") as f:
        f.write("".join(parts))
    print("Canvas:", canvas_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
