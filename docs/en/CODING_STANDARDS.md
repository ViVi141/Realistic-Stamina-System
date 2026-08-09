# RSS Coding Standards (authoritative)

> [中文](../RSS_CODING_STANDARDS.md) | **English**
>
> Supersedes conflicting items in [`../scripts_naming_and_layout_rules.md`](../scripts_naming_and_layout_rules.md).  
> Version: **6.1.x** (naming/layering from v5; stamina presentation in §5)

## 1. Naming

| Kind | Rule | Example |
|------|------|---------|
| RSS class | `SCR_RSS_<Domain><Name>` | `SCR_RSS_DrainCalculator` |
| Enum | `ERSS_<Name>` | `ERSS_MovementPhase` |
| DTO | `RSS_<Name>` | `RSS_PlayerInfo` |
| Filename | Matches primary class | `SCR_RSS_DrainCalculator.c` |
| modded entry | Engine class name; stable filenames OK | `PlayerBase.c` + `PlayerBase_UpdateLoop.c` (only these two for the same modded class) |
| Members | `m_f`/`m_i`/`m_b`/`m_p`/`m_e`/`m_s`/`m_v` | `m_fAerobicStamina` |

## 2. Formatting

- 4-space indent; K&R braces
- **Forbidden**: ternary `?:`
- Single-line `if` must use `{}`
- Public static: `//!` + `@param` / `@return` (**Chinese** comments preferred in this repo)

## 3. File size

| Layer | Cap |
|-------|-----|
| All `.c` files | **65535 bytes** (hard crash) |
| Integration | ≤ 40 KB / ≤ 600 lines |
| StaminaOverride | ≤ 15 KB / ≤ 250 lines (intercept shell only) |
| RSS/Core etc. | ≤ 45 KB / ≤ 700 lines |

Run: `python tools/check_script_size.py`  
Run: `python tools/check_enforce_syntax.py` (banned syntax + single-line `if`)

## 4. Official-first + two exceptions

**Default**: prefer official APIs (`SetSpeedLimit`, `RplProp`, `CallLater`, `GetTotalWeightOfAllStorages`, …).

| Exception | Strategy |
|-----------|----------|
| **Weather/environment** | RSS-owned stack; `TimeAndWeatherManagerEntity` sample-only |
| **Engine stamina bar** | **Intercept only** (`OnStaminaDrain` / `ApplyDrain`); `AddStamina` is not overridable |

## 5. Stamina intercept (aerobic / W′)

- **Aerobic pool** → RSS compute → `SetTargetStamina` / controlled `AddStamina`; authority is `m_fTargetStamina`
- **W′ (anaerobic)** → `SCR_RSS_CriticalPowerModel` / `SCR_RSS_AnaerobicBurst`; **does not** change aerobic authority
- **W′ → engine FX** (on by default): `SCR_RSS_SprintGate` writes transient `GetStamina()` via `ApplyTransientEngineStamina` for native sway/`Exhaustion`; formulas stay in `RSS/Core/`
- CPR-style disabled vanilla `CharacterStaminaComponent` compat is **not** in this mod; use a separate compat mod

## 6. EnforceScript bans

- `?:`, `ScriptCaller`, single file over 64 KB
- Prefer `ref` over deprecated `autoptr`
- No try/catch, no user generic classes

## 7. Layers

```
Integration/       → thin modded shell + RPC (`PlayerBase.c` / `PlayerBase_UpdateLoop.c` only; do not add more `PlayerBase_*.c`)
RSS/Core/          → metabolism, dual pools, speed, coordinator
RSS/Environment/   → RSS environment stack
RSS/NetworkConfig/ → config, sync, API
RSS/AI/            → AI stamina
RSS/Presentation/  → HUD, screen FX
RSS/Items/         → injectors, UserActions
RSS/MudSlip/       → mud slip
```

## 8. PR checklist

- [ ] `check_script_size.py` passes
- [ ] `check_enforce_syntax.py` passes
- [ ] Official API anchor or exception rationale stated
- [ ] No inline Pandolf / environment penalties in Integration
