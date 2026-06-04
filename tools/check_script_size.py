#!/usr/bin/env python3
"""EnforceScript file size gate (65535 byte hard limit + RSS tier limits)."""

from __future__ import annotations

import os
import sys

LIMIT = 65535
WARN = 60000

# docs/RSS_CODING_STANDARDS.md §3
TIER_RULES: tuple[tuple[str, int, int], ...] = (
    ("Integration/SCR_StaminaOverride.c", 15 * 1024, 250),
    ("Integration/", 40 * 1024, 600),
    ("RSS/Core/", 45 * 1024, 700),
)


def tier_for(rel_posix: str) -> tuple[int, int] | None:
    if "Integration/SCR_StaminaOverride.c" in rel_posix:
        return TIER_RULES[0][1], TIER_RULES[0][2]
    norm = rel_posix.replace("\\", "/")
    if "/Integration/" in norm or norm.startswith("Integration/"):
        return TIER_RULES[1][1], TIER_RULES[1][2]
    if "/RSS/Core/" in norm or norm.startswith("RSS/Core/"):
        return TIER_RULES[2][1], TIER_RULES[2][2]
    return None


def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    scripts_dir = os.path.join(root, "scripts")
    hard_violations: list[tuple[str, int, str]] = []
    tier_violations: list[tuple[str, int, int, str, str]] = []
    bom_files: list[str] = []

    for dirpath, _, files in os.walk(scripts_dir):
        for name in files:
            if not name.endswith(".c"):
                continue
            path = os.path.join(dirpath, name)
            with open(path, "rb") as fh:
                if fh.read(3) == b"\xef\xbb\xbf":
                    bom_files.append(os.path.relpath(path, root))

            size = os.path.getsize(path)
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                lines = sum(1 for _ in fh)

            rel = os.path.relpath(path, root)
            rel_scripts = os.path.relpath(path, scripts_dir).replace("\\", "/")

            if size > WARN:
                level = "BLOCK" if size > LIMIT else "WARN"
                hard_violations.append((level, size, rel))

            tier = tier_for(rel_scripts)
            if tier:
                max_bytes, max_lines = tier
                if size > max_bytes or lines > max_lines:
                    tier_violations.append(
                        (rel, size, lines, f"{max_bytes // 1024}KB/{max_lines}L", rel_scripts)
                    )

    hard_violations.sort(key=lambda x: x[1], reverse=True)
    for level, size, rel in hard_violations:
        print(f"[{level}] {size:>6} bytes  {rel}")

    if tier_violations:
        tier_violations.sort(key=lambda x: x[1], reverse=True)
        print()
        print("[TIER] 超过 RSS_CODING_STANDARDS.md §3 分层上限（警告，不阻断提交）：")
        for rel, size, lines, cap, _ in tier_violations:
            print(f"  {size:>6}B {lines:>4}L  (≤{cap})  {rel}")

    if bom_files:
        print("[BLOCK] UTF-8 BOM detected (EnforceScript line-1 syntax error):")
        for rel in sorted(bom_files):
            print(f"        {rel}")
        print("\n提交被阻止：请保存为 UTF-8 无 BOM（VS Code: utf8）。")
        return 1

    if any(v[0] == "BLOCK" for v in hard_violations):
        print("\n提交被阻止：存在超过 65535 字节的 .c 文件。")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
