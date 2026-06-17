#!/usr/bin/env python3
"""RSS v6 validation entry — delegates to rss_pipeline_v6 validate."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    script = ROOT / "tools" / "rss_pipeline_v6.py"
    proc = subprocess.run(
        [sys.executable, str(script), "validate"],
        cwd=str(ROOT),
        check=False,
    )
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
