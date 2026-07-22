#!/usr/bin/env python3
"""v5/v6 physiological anchor benchmarks — delegates to rss_constraints_v6."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_constraints_v6 import evaluate_physio_anchors


def main() -> int:
    report = evaluate_physio_anchors()
    for line in report.summary_lines():
        print(line)
    if report.all_hard_passed:
        print("bench_physio_anchors: all runnable checks passed")
        return 0
    print(f"bench_physio_anchors: {len(report.failed_hard)} hard failure(s)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
