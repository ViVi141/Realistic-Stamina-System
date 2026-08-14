#!/usr/bin/env python3
"""Thin shell / importable helpers for compile-crash isolation.

文件大小（64 KB）**不是** EnforceScript 编译/运行时崩溃的原因；本脚本不再
因文件体积阻断提交。它只保留一个可被其他模块引用的壳子，用于「壳子法 +
逐个文件编译」逐步排查崩溃文件：

    from check_script_size import iter_script_files
    for path in iter_script_files():
        ...  # 逐个编译 / 壳子二分

仍阻断提交的仅剩 UTF-8 BOM（EnforceScript 第 1 行语法错误，真实存在）。
"""

from __future__ import annotations

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS_DIR = os.path.join(ROOT, "scripts")

# 可维护性分层建议上限（仅供参考，非崩溃原因）。见 docs/RSS_CODING_STANDARDS.md §3
TIER_RULES: tuple[tuple[str, int, int], ...] = (
    ("Integration/SCR_StaminaOverride.c", 15 * 1024, 250),
    ("Integration/", 40 * 1024, 600),
    ("RSS/Core/", 45 * 1024, 700),
)

# 仅作提示用的大小阈值（不再阻断）
ADVISORY_BYTES = 60 * 1024


def iter_script_files(root: str | None = None) -> list[str]:
    """枚举 scripts/ 下所有 `.c` 文件（绝对路径，已排序）。

    供「逐个文件编译 / 壳子二分」脚本引用。
    """
    base = root or SCRIPTS_DIR
    paths: list[str] = []
    for dirpath, _, files in os.walk(base):
        for name in files:
            if name.endswith(".c"):
                paths.append(os.path.join(dirpath, name))
    paths.sort()
    return paths


def rel_to_scripts(path: str) -> str:
    """返回相对 scripts/ 的 posix 路径（供 tier_for 使用）。"""
    return os.path.relpath(path, SCRIPTS_DIR).replace("\\", "/")


def tier_for(rel_posix: str) -> tuple[int, int] | None:
    """返回 `(max_bytes, max_lines)` 分层建议上限；不在分层内返回 None。

    仅作可维护性参考，与编译稳定性无关。
    """
    if "Integration/SCR_StaminaOverride.c" in rel_posix:
        return TIER_RULES[0][1], TIER_RULES[0][2]
    norm = rel_posix.replace("\\", "/")
    if "/Integration/" in norm or norm.startswith("Integration/"):
        return TIER_RULES[1][1], TIER_RULES[1][2]
    if "/RSS/Core/" in norm or norm.startswith("RSS/Core/"):
        return TIER_RULES[2][1], TIER_RULES[2][2]
    return None


def has_bom(path: str) -> bool:
    """检测文件是否带 UTF-8 BOM（EnforceScript 第 1 行语法错误）。"""
    with open(path, "rb") as fh:
        return fh.read(3) == b"\xef\xbb\xbf"


def main() -> int:
    bom_files: list[str] = []
    advisory: list[tuple[int, int, str, tuple[int, int] | None]] = []

    for path in iter_script_files():
        rel = os.path.relpath(path, ROOT)
        if has_bom(path):
            bom_files.append(rel)
        size = os.path.getsize(path)
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            lines = sum(1 for _ in fh)
        advisory.append((size, lines, rel, tier_for(rel_to_scripts(path))))

    if bom_files:
        print("[BLOCK] UTF-8 BOM detected (EnforceScript line-1 syntax error):")
        for rel in sorted(bom_files):
            print(f"        {rel}")
        print("\n提交被阻止：请保存为 UTF-8 无 BOM（VS Code: utf8）。")
        return 1

    print("[INFO] 文件大小非崩溃原因；本脚本不再因体积阻断提交。")
    print("[INFO] 可维护性分层建议（仅供参考，非崩溃原因）：")
    for size, lines, rel, tier in sorted(advisory, key=lambda x: -x[0]):
        if tier is None:
            if size > ADVISORY_BYTES:
                print(f"  {size:>6}B {lines:>4}L  (无分层建议)  {rel}")
            continue
        max_bytes, max_lines = tier
        if size > max_bytes or lines > max_lines:
            print(f"  {size:>6}B {lines:>4}L  (建议 ≤{max_bytes // 1024}KB/{max_lines}L)  {rel}")

    print("\n[OK] 无 BOM 阻断项。排查编译崩溃请用「壳子法 + 逐个文件编译」。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
