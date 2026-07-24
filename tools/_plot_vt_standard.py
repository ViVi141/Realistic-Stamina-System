#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Render StandardMilsim 30kg V-T figure from embed JSON."""
from __future__ import annotations

import json
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
embed_path = os.path.join(HERE, "_vt_canvas_embed.json")
out_png = os.path.join(HERE, "vt_standard_30kg.png")

with open(embed_path, encoding="utf-8") as f:
    e = json.load(f)

fig, axes = plt.subplots(2, 1, figsize=(10, 7.2), constrained_layout=True)

ax = axes[0]
t = [float(x) for x in e["early90s"]["cats"]]
ax.plot(t, e["early90s"]["Walk"], label="Walk", color="#2a6f4e", lw=1.8)
ax.plot(t, e["early90s"]["Run"], label="Run", color="#2f5d8c", lw=1.8)
ax.plot(t, e["early90s"]["Sprint"], label="Sprint", color="#a33b2b", lw=1.8)
ax.set_ylabel("v (m/s)")
ax.set_xlabel("t (s)")
ax.set_title("Early V-T (0-90 s)")
ax.set_ylim(0, 3.4)
ax.grid(True, alpha=0.3)
ax.legend(frameon=False)

ax = axes[1]
tm = [float(x) for x in e["short32"]["cats"]]
ax.plot(tm, e["short32"]["Walk"], label="Walk", color="#2a6f4e", lw=1.6)
ax.plot(tm, e["short32"]["Run"], label="Run", color="#2f5d8c", lw=1.6)
ax.plot(tm, e["short32"]["Sprint"], label="Sprint", color="#a33b2b", lw=1.6)
ax.axhline(2.2, color="#888888", ls="--", lw=0.9, label="Run CP floor 2.2")
ax.set_ylabel("v (m/s)")
ax.set_xlabel("t (min)")
ax.set_title("V-T to ~STA empty window (0-32 min)")
ax.set_ylim(0, 3.4)
ax.grid(True, alpha=0.3)
ax.legend(frameon=False, loc="upper right")

tte = e["tte"]
sup = (
    "RSS v6 StandardMilsim @ 30 kg flat — continuous gait V-T\n"
    "TTE STA<=1%: Walk {walk:.2f} h | Run {run:.1f} min | Sprint {sp:.1f} min"
).format(
    walk=tte["Walk"] / 3600.0,
    run=tte["Run"] / 60.0,
    sp=tte["Sprint"] / 60.0,
)
fig.suptitle(sup, fontsize=11)
fig.savefig(out_png, dpi=140)
print(out_png)
