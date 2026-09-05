# RSS engine API usage catalog

> [中文](../engine_api_usage.md) | **English**
>
> **v6.2.31 (current)**: speed limits via `SCR_RSS_SpeedBridge` → `SetSpeedLimit` (min-merge with foliage, etc.). Aerobic authority is `m_fTargetStamina`, written through the `SCR_StaminaOverride` shell. W′ does **not** change aerobic authority, but by default `SCR_RSS_SprintGate` → `ApplyTransientEngineStamina` drives native sway/blur (`Exhaustion`). Prefer `SCR_RSS_EngineReuse` for slope / velocity / floor surface. Coding rules: [`CODING_STANDARDS.md`](CODING_STANDARDS.md).
>
> Tables below are largely historical enumerations; see Chinese doc + EngineReuse for the reuse path.

> Scope: all `.c` under `scripts/`  
> Excludes project-internal types from the “official API” framing (`SCR_RSS_*`, many `SCR_Stamina*`)  
> **Path note**: `PlayerBase.c` means `scripts/Game/Integration/PlayerBase.c` (`modded class SCR_CharacterControllerComponent`).

The Chinese doc has the full per-API tables with Chinese descriptions. Below: English anchors + section map. Prefer source when unsure.

## v6 anchors

| Role | Module | Official API |
|------|--------|--------------|
| Speed merge | `SCR_RSS_SpeedBridge.c` | `SCR_ChimeraCharacter.SetSpeedLimit` |
| Aerobic inject | `SCR_StaminaOverride.c` | `OnStaminaDrain` / `ApplyDrain` intercept + `AddStamina` |
| W′ engine FX | `SCR_RSS_SprintGate.c` | `ApplyTransientEngineStamina` (does not change `m_fTargetStamina`) |
| Drain/speed coord | `SCR_RSS_UpdateCoordinator.c` | Bridge + dual pools |
| Environment sample | `SCR_RSS_WeatherApi.c` / `SCR_RSS_EnvironmentFactor.c` | `TimeAndWeatherManagerEntity` read-only |
| Config replicate | `SCR_RSS_ServerBootstrap.c` | `[RplProp(onRplName:)]` on GameMode |
| HUD data | `SCR_RSS_StaminaHUDComponent.c` | `WorkspaceWidget` + `GUIColors` |

---

## Section map (full tables in Chinese doc)

| § | Topic | Typical official surfaces |
|---|-------|---------------------------|
| 1 | Core framework | `GetGame()`, world, callqueue, world time, input/player managers, workspace |
| 2 | Entities | `IEntity`, `FindComponent`, `GetOrigin`, `ChimeraCharacter`, anim/compartment |
| 3 | Character controller | `SCR_CharacterControllerComponent`, stance, velocity, `OverrideMaxSpeed` (via SpeedBridge), lifecycle |
| 4 | Stamina component | `CharacterStaminaComponent` / `SCR_CharacterStaminaComponent`, `GetStamina()`, `ApplyDrain` |
| 5 | Inventory / load | `GetTotalWeight`, `GetMaxLoad`, `GetTotalWeightOfAllStorages` |
| 6 | Input | `InputManager`, `ActionManager` |
| 7 | Networking | `[RplRpc]`, channels / receivers |
| 8 | Players | `PlayerManager`, `SCR_PlayerController.GetLocalControlledEntity` |
| 9 | Physics / terrain | `World.TraceMove`, `TraceParam`, vectors |
| 10 | Animation / commands | `CharacterAnimationComponent`, command handler, stance enums |
| 11+ | Further tables | See [`../engine_api_usage.md`](../engine_api_usage.md) for complete bilingual-needed rows |

### Stamina notes (critical)

- `GetStamina()`: aerobic authority is written via Override; W′ may add **transient** presentation on top (`SCR_RSS_SprintGate`).
- `ApplyDrain`: vanilla drain path; intercepted by Override.
- Never treat transient presentation as the aerobic authority (`m_fTargetStamina`).

### Speed notes (critical)

- Prefer `SetSpeedLimit(source, limit)` min-merge; do not solo `OverrideMaxSpeed` and erase foliage slowdown.
- Command limit ≠ guaranteed physics horizontal speed; see [`KNOWN_ISSUES_SPEED_SLIP.md`](KNOWN_ISSUES_SPEED_SLIP.md).

---

*When adding a new official API call site, update the Chinese catalog first, then refresh this English summary if the anchor table changes.*
