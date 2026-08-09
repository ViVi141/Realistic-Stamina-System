# EnforceScript `.c` file size hard limit

> [中文](../scripts_file_size_limit.md) | **English**

## Rule

> **Every `.c` script file must be ≤ 65535 bytes (64 KB). Exceeding this can crash Workbench compile or runtime with no useful error.**

This is an EnforceScript compiler/runtime hard limit, not a game-design knob. It cannot be bypassed by config or CLI flags.

---

## Current high-risk files (measured 2026-08-09)

> Reproduce sizes from repo root with the PowerShell below. 65535 bytes is the hard cap.

| File | Size | vs 64 KB |
|------|------|----------|
| `scripts/Game/Integration/PlayerBase.c` | ~78385 bytes (~76.5 KB) | **~12.5 KB over** |
| `scripts/Game/RSS/Core/SCR_RSS_Constants.c` | ~52570 bytes (~51.3 KB) | ~12.7 KB headroom |
| `scripts/Game/Integration/PlayerBase_UpdateLoop.c` | ~49135 bytes (~48.0 KB) | ~16 KB headroom |
| `scripts/Game/RSS/Core/SCR_RSS_UpdateCoordinator.c` | ~44666 bytes (~43.6 KB) | ~20 KB headroom |
| `scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c` | ~43639 bytes (~42.6 KB) | ~21 KB headroom |

> PowerShell (repo root):
> ```powershell
> Get-ChildItem -Path scripts -Recurse -Filter '*.c' | ForEach-Object {
>     $color = 'Yellow'
>     if ($_.Length -gt 65535) { $color = 'Red' }
>     if ($_.Length -gt 60000) {
>         Write-Host "$($_.Length) bytes  $($_.FullName)" -ForegroundColor $color
>     }
> }
> ```
>
> Or: `python tools/check_script_size.py`

---

## Split plan (aligned with current repo)

### Priority 1: `PlayerBase.c` (only file still over 64 KB)

`modded class SCR_CharacterControllerComponent` entry is still ~**76 KB**. Keep moving mud proxies, RPC, CSB-sized blocks into `RSS/` helpers (see existing `SCR_RSS_MudSlipRunner.c`). Same modded class may only grow via `PlayerBase.c` + `PlayerBase_UpdateLoop.c`.

### Priority 2: `SCR_RSS_Constants.c` / `PlayerBase_UpdateLoop.c`

Near 45–50 KB: split domain constants before adding more; keep pushing pure orchestration out of UpdateLoop into `SCR_RSS_UpdateCoordinator`, etc.

### Priority 3: `SCR_RSS_EnvironmentFactor.c`

Environment stack already has `SCR_RSS_*` satellites; new logic should go to WeatherApi / PenaltyMath / EnvConstants.

### Priority 4: `SCR_RSS_Settings.c` / Config

If growth continues, extract `SCR_RSS_Params` into `SCR_RSS_Params.c` (Params = data model; Settings = config + serialization).

---

### Historical split notes

| Direction | Notes |
|-----------|-------|
| Monolith `SCR_RealisticStaminaSystem.c` | **Removed**; responsibilities live in `SCR_RSS_*` Core |
| Swim model | `SCR_SwimmingStaminaModel.c` (or equivalent RSS Core file) |
| Params extract | Still optional |
| PlayerBase slim-down | Mud / RPC / presentation extract (priority 1) |

#### Optional `SCR_RSS_Params` extract

1. Create `scripts/Game/RSS/NetworkConfig/SCR_RSS_Params.c`
2. Move `class SCR_RSS_Params { ... }` intact
3. Keep references in `SCR_RSS_Settings.c`
4. Leave `WriteParamsToArray` / `ApplyParamsFromArray` on Settings

---

## Execution order

```
1. PlayerBase.c → keep extracting mud / RPC / presentation (still > 64 KB)
2. PlayerBase_UpdateLoop / Constants → control growth
3. EnvironmentFactor → keep satellites sharing load
4. Settings → split Params if needed
```

Extract principles:

1. **One domain per change**, compile + regress each time.
2. Keep extracted functions **`static`** as before.
3. **Do not split for size vanity** when cohesive and far under the cap.
4. Must still **compile** after the split.

---

## Pre-commit check

```powershell
Get-ChildItem -Path scripts -Recurse -Filter '*.c' |
    Where-Object { $_.Length -gt 60000 } |
    Sort-Object Length -Descending |
    Format-Table Length, Name

$violations = Get-ChildItem -Path scripts -Recurse -Filter '*.c' |
    Where-Object { $_.Length -gt 65535 }
if ($violations) {
    Write-Host "BLOCKED: Files exceed 65535 byte limit:" -ForegroundColor Red
    $violations | Format-Table Length, FullName
    exit 1
}
```

Or `python tools/check_script_size.py`.

---

## Related docs

- [scripts_naming_and_layout_rules.md](../scripts_naming_and_layout_rules.md) (Chinese)
- [CODING_STANDARDS.md](CODING_STANDARDS.md) / [RSS_CODING_STANDARDS.md](../RSS_CODING_STANDARDS.md)

Record size before/after in CHANGELOG when splitting.
