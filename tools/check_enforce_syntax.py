#!/usr/bin/env python3
"""Scan EnforceScript sources for RSS-forbidden syntax and single-line if violations.

See docs/RSS_CODING_STANDARDS.md §2 and §6.

Usage:
    python tools/check_enforce_syntax.py          # report only, exit 1 on violations
    python tools/check_enforce_syntax.py --fix    # wrap single-line if/else-if bodies in {}
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS_DIR = os.path.join(ROOT, "scripts")

# §6 forbidden tokens / patterns (applied to code outside comments/strings)
FORBIDDEN_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("ScriptCaller", re.compile(r"\bScriptCaller\b")),
    ("autoptr", re.compile(r"\bautoptr\b")),
    ("try/catch", re.compile(r"\b(try|catch)\b")),
    ("user generic class", re.compile(r"\bclass\s+\w+\s*<")),
)

TERNARY_PATTERN = re.compile(r"\?[^?\n]*:")


@dataclass
class Violation:
    path: str
    line_no: int
    rule: str
    text: str


def strip_comments_and_strings(line: str) -> str:
    """Return a sanitized view of *line* with strings and comments blanked."""
    out: list[str] = []
    i = 0
    n = len(line)
    in_string = False
    escape = False
    while i < n:
        ch = line[i]
        if in_string:
            out.append(" ")
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            i += 1
            continue

        if ch == '"':
            in_string = True
            out.append(" ")
            i += 1
            continue

        if ch == "/" and i + 1 < n and line[i + 1] == "/":
            out.extend(" " * (n - i))
            break

        if ch == "/" and i + 1 < n and line[i + 1] == "*":
            out.append(" ")
            out.append(" ")
            i += 2
            while i + 1 < n and not (line[i] == "*" and line[i + 1] == "/"):
                out.append(" ")
                i += 1
            if i + 1 < n:
                out.append(" ")
                out.append(" ")
                i += 2
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def find_matching_paren(text: str, open_idx: int) -> int:
    depth = 0
    in_string = False
    escape = False
    for i in range(open_idx, len(text)):
        ch = text[i]
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
    return -1


def split_line_comment(line: str) -> tuple[str, str]:
    in_string = False
    escape = False
    for i, ch in enumerate(line):
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
            continue
        if ch == "/" and i + 1 < len(line) and line[i + 1] == "/":
            return line[:i].rstrip(), line[i:]
    return line.rstrip(), ""


def is_single_line_if_violation(line: str) -> bool:
    code, _ = split_line_comment(line.rstrip("\n\r"))
    stripped = code.lstrip()
    if not stripped:
        return False
    if stripped.startswith("//") or stripped.startswith("*"):
        return False

    indent_len = len(code) - len(code.lstrip())
    rest = code[indent_len:]

    if rest.startswith("if "):
        kw_len = 3
    elif rest.startswith("else if "):
        kw_len = 8
    else:
        return False

    pos = kw_len
    while pos < len(rest) and rest[pos] == " ":
        pos += 1
    if pos >= len(rest) or rest[pos] != "(":
        return False

    close_paren = find_matching_paren(rest, pos)
    if close_paren < 0:
        return False

    body = rest[close_paren + 1 :].strip()
    if not body:
        return False
    if body.startswith("{"):
        return False
    return True


def fix_single_line_if(line: str) -> tuple[str, bool]:
    code, suffix = split_line_comment(line.rstrip("\n\r"))
    newline = ""
    if line.endswith("\r\n"):
        newline = "\r\n"
    elif line.endswith("\n"):
        newline = "\n"

    stripped = code.lstrip()
    if not stripped or stripped.startswith("//") or stripped.startswith("*"):
        return line, False

    indent = code[: len(code) - len(code.lstrip())]
    rest = code[len(indent) :]

    if rest.startswith("if "):
        prefix = "if "
        pos = 3
    elif rest.startswith("else if "):
        prefix = "else if "
        pos = 8
    else:
        return line, False

    while pos < len(rest) and rest[pos] == " ":
        pos += 1
    if pos >= len(rest) or rest[pos] != "(":
        return line, False

    close_paren = find_matching_paren(rest, pos)
    if close_paren < 0:
        return line, False

    body = rest[close_paren + 1 :].strip()
    if not body or body.startswith("{"):
        return line, False

    condition = rest[pos : close_paren + 1]
    fixed = (
        f"{indent}{prefix}{condition}\n"
        f"{indent}{{\n"
        f"{indent}    {body}\n"
        f"{indent}}}{suffix}{newline}"
    )
    return fixed, True


def scan_file(path: str) -> list[Violation]:
    rel = os.path.relpath(path, ROOT)
    violations: list[Violation] = []
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line_no, raw in enumerate(fh, start=1):
            sanitized = strip_comments_and_strings(raw.rstrip("\n\r"))
            for name, pattern in FORBIDDEN_PATTERNS:
                if pattern.search(sanitized):
                    violations.append(
                        Violation(rel, line_no, name, raw.rstrip("\n\r").strip())
                    )
            if TERNARY_PATTERN.search(sanitized):
                violations.append(
                    Violation(rel, line_no, "?: ternary", raw.rstrip("\n\r").strip())
                )
            if is_single_line_if_violation(raw):
                violations.append(
                    Violation(rel, line_no, "single-line if", raw.rstrip("\n\r").strip())
                )
    return violations


def fix_file(path: str) -> int:
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()

    out: list[str] = []
    fixes = 0
    for line in lines:
        fixed, changed = fix_single_line_if(line)
        out.append(fixed)
        if changed:
            fixes += 1

    if fixes:
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.writelines(out)
    return fixes


def iter_script_files() -> list[str]:
    paths: list[str] = []
    for dirpath, _, files in os.walk(SCRIPTS_DIR):
        for name in files:
            if name.endswith(".c"):
                paths.append(os.path.join(dirpath, name))
    paths.sort()
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description="RSS EnforceScript syntax gate")
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Wrap single-line if/else-if bodies in braces (in place)",
    )
    args = parser.parse_args()

    paths = iter_script_files()

    if args.fix:
        total = 0
        touched: list[tuple[str, int]] = []
        for path in paths:
            count = fix_file(path)
            if count:
                touched.append((os.path.relpath(path, ROOT), count))
                total += count
        if touched:
            print(f"[FIX] Wrapped {total} single-line if/else-if in {{}}:")
            for rel, count in touched:
                print(f"  {count:>3}  {rel}")
        else:
            print("[FIX] No single-line if/else-if violations found.")
        paths = iter_script_files()

    all_violations: list[Violation] = []
    for path in paths:
        all_violations.extend(scan_file(path))

    if not all_violations:
        print("[OK] EnforceScript syntax check passed.")
        return 0

    by_rule: dict[str, list[Violation]] = {}
    for v in all_violations:
        by_rule.setdefault(v.rule, []).append(v)

    for rule in sorted(by_rule.keys()):
        items = by_rule[rule]
        print(f"[{rule}] {len(items)}")
        for v in items[:20]:
            print(f"  {v.path}:{v.line_no}: {v.text}")
        if len(items) > 20:
            print(f"  ... and {len(items) - 20} more")

    print()
    print(f"[BLOCK] {len(all_violations)} violation(s). See docs/RSS_CODING_STANDARDS.md §2/§6.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
