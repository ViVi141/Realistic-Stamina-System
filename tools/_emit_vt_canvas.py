#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Emit Cursor canvas for Standard V-T charts."""
from __future__ import annotations

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
embed_path = os.path.join(HERE, "_vt_canvas_embed.json")
canvas_path = os.path.join(
    os.path.expanduser("~"),
    ".cursor",
    "projects",
    "c-Users-74738-Documents-My-Games-ArmaReforgerWorkbench-addons-Realistic-Stamina-System",
    "canvases",
    "standard-vt-chart.canvas.tsx",
)

with open(embed_path, encoding="utf-8") as f:
    e = json.load(f)

early = e["early90s"]
short = e["short32"]
tte = e["tte"]
vavg = e["v_avg"]


def fmt_tte(sec: float) -> str:
    if sec >= 3600:
        h = int(sec // 3600)
        m = int((sec % 3600) // 60)
        s = int(sec % 60)
        return f"{h}h{m:02d}m{s:02d}s"
    m = int(sec // 60)
    s = int(sec % 60)
    return f"{m:02d}:{s:02d}"


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
          gait until STA &lt;= 1%
        </Text>
      </Stack>

      <Grid columns={3} gap={12}>
"""
)
parts.append(
    f'        <Stat label="Walk TTE / mean v" value="{fmt_tte(tte["Walk"])}" '
    f'unit="{vavg["Walk"]:.2f} m/s" />\n'
)
parts.append(
    f'        <Stat label="Run TTE / mean v" value="{fmt_tte(tte["Run"])}" '
    f'unit="{vavg["Run"]:.2f} m/s" />\n'
)
parts.append(
    f'        <Stat label="Sprint TTE / mean v" value="{fmt_tte(tte["Sprint"])}" '
    f'unit="{vavg["Sprint"]:.2f} m/s" />\n'
)
parts.append(
    """      </Grid>

      <Card>
        <CardHeader
          title="Early V-T (0-90 s)"
          subtitle="Sprint W-prime step-down; Run holds then disarms"
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
            X: time (min) · Y: applied velocity (m/s) · after STA empty hold last sample
          </Text>
        </CardBody>
      </Card>

      <Divider />
      <H2>Readout</H2>
      <Stack gap={6}>
        <Text>
          Sprint opens near 2.98 m/s, then steps down with W-prime to about 1.76 m/s
          cruise — no bang-bang chatter.
        </Text>
        <Text>
          Run opens near 2.56 m/s, then settles near 1.94 m/s after disarm (30 kg enc
          below the 2.2 m/s unload floor).
        </Text>
        <Text>
          Walk holds about 1.30 m/s; full Walk TTE is hours (chart truncated at 32 min).
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

os.makedirs(os.path.dirname(canvas_path), exist_ok=True)
with open(canvas_path, "w", encoding="utf-8") as f:
    f.write("".join(parts))
print(canvas_path)
