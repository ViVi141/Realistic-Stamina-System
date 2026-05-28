#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从指定 git commit 恢复 SCR_RSS_Settings.c 三档 Init*Defaults。"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

COMMIT = "b2e1cd9155c9515ea756a458a01b8f4697bd54a6"
PRESETS = ["EliteStandard", "StandardMilsim", "TacticalAction"]


def main():
    root = Path(__file__).resolve().parents[1]
    c_path = root / "scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c"
    raw = subprocess.check_output(
        ["git", "show", f"{COMMIT}:scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c"],
        cwd=root,
    )
    text = raw.decode("utf-8")

    blocks = {}
    for preset in PRESETS:
        pat = (
            rf"(//[^\n]*\nprotected void Init{preset}Defaults\(bool shouldInit\)\s*\{{.*?\n\}})"
        )
        match = re.search(pat, text, re.DOTALL)
        if not match:
            print(f"ERROR: Init{preset}Defaults not found in {COMMIT}", file=sys.stderr)
            sys.exit(1)
        block = match.group(1)
        block = re.sub(
            r"//[^\n]*\nprotected void",
            f"            // 初始化 {preset} 预设默认值\nprotected void",
            block,
            count=1,
        )
        blocks[preset] = block

    backup = c_path.with_suffix(".c.backup")
    shutil.copy2(c_path, backup)

    content = c_path.read_text(encoding="utf-8")
    for preset in PRESETS:
        pat = (
            rf"            // 初始化 {preset} 预设默认值\n"
            rf"protected void Init{preset}Defaults\(bool shouldInit\)\s*\{{.*?\n\}}"
        )
        content, count = re.subn(pat, blocks[preset], content, count=1, flags=re.DOTALL)
        if count != 1:
            print(f"ERROR: replace Init{preset}Defaults failed ({count})", file=sys.stderr)
            sys.exit(1)

    c_path.write_text(content, encoding="utf-8")
    print(f"Restored 3 presets from {COMMIT}")
    print(f"Backup: {backup}")
    for preset in PRESETS:
        ec = re.search(
            rf"m_{preset}\.energy_to_stamina_coeff = ([^;]+);", blocks[preset]
        )
        val = ec.group(1) if ec else "?"
        print(f"  {preset}: energy_to_stamina_coeff = {val}")


if __name__ == "__main__":
    main()
