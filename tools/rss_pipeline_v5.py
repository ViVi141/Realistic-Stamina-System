#!/usr/bin/env python3
"""RSS v6 optimizer entry — loads v5 schema + runs physio anchor validation."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

SCHEMA_PATH = Path(__file__).resolve().parent / "schemas" / "rss_params_v5.schema.json"
ROOT = Path(__file__).resolve().parent.parent


def load_v5_schema() -> dict:
    with SCHEMA_PATH.open(encoding="utf-8") as f:
        return json.load(f)


def run_bench() -> int:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "bench_physio_anchors.py")],
        cwd=str(ROOT),
        check=False,
    )
    return proc.returncode


def main() -> int:
    schema = load_v5_schema()
    fields = list(schema.get("properties", {}).keys())
    print("rss_pipeline_v5: v6 validation pipeline")
    print(f"  schema fields ({len(fields)}): critical_power_watts, w_prime_max_joules, ...")
    rc = run_bench()
    if rc != 0:
        print("  bench_physio_anchors: FAIL (hard constraint)")
        return rc
    print("  bench_physio_anchors: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
