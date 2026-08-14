# Compile-crash isolation (shell method + per-file compile)

> [中文](../scripts_file_size_limit.md) | **English**

## Correction

> ~~Every `.c` file must be ≤ 65535 bytes (64 KB), or compile/runtime will crash.~~
>
> **Retracted. File size (64 KB) is NOT the cause of Workbench compile or runtime crashes.**

The earlier "64 KB hard limit causes crashes" theory does not hold and is withdrawn. This page keeps its title only for historical link compatibility.

## Correct isolation method

When Workbench crashes while compiling scripts (ICE / heap corruption / no useful error), the only reliable way is **per-file compile checking** with the **shell method** to isolate the exact file:

1. **Keep only a shell**: strip the suspect file to a shell — keep class/method **signatures**, delete bodies — so other modules can still reference it and the project still compiles.
2. **Restore and compile one file at a time** to find the file that triggers the crash.
3. **Bisect within that file**: shell → half restored → full, narrowing to the exact method/block.

> `tools/check_script_size.py` is now only a **shell importable by other modules** and no longer blocks on size. It provides:
>
> - `iter_script_files()` — enumerate `.c` files for per-file compile scripts
> - `tier_for()` — per-layer maintainability caps (advisory only, not a crash cause)
> - `has_bom()` — UTF-8 BOM detection (still a real syntax error, blocks commit)

## Maintainability guidance (not a crash cause)

These per-layer caps are for **maintainability** only (easier editing/review); they do not affect compile stability:

| Layer | Suggested cap |
|-------|---------------|
| Integration | ≤ 40 KB / ≤ 600 lines |
| StaminaOverride | ≤ 15 KB / ≤ 250 lines (intercept shell only) |
| RSS/Core etc. | ≤ 45 KB / ≤ 700 lines |

Prefer extracting domain logic into helpers when a file grows large, but size no longer blocks commits.

## Pre-commit checks

```powershell
python tools/check_script_size.py     # BOM block + maintainability hints (no size block)
python tools/check_enforce_syntax.py  # banned syntax + single-line if
python tools/test_v6_smoke.py
```

## Related docs

- [scripts_naming_and_layout_rules.md](../scripts_naming_and_layout_rules.md) (Chinese)
- [CODING_STANDARDS.md](CODING_STANDARDS.md) / [RSS_CODING_STANDARDS.md](../RSS_CODING_STANDARDS.md)
