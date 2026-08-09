# Realistic Stamina System (RSS) — external mod API

> [中文](../RSS_API.md) | **English**
>
> Read player stamina / W′ / environment from RSS in other mods.

## Implementation locations

| Content | Path |
|---------|------|
| API entry | `scripts/Game/RSS/NetworkConfig/SCR_RSS_API.c` |
| Data export | `scripts/Game/RSS/NetworkConfig/SCR_RSS_DataExport.c` |

## Dependencies

- Depend on RSS (`Realistic Stamina System`)
- Target must be a character (`ChimeraCharacter`)

## Aerobic STA vs W′ (read this)

| Concept | Field | Authority | Notes |
|---------|-------|-----------|-------|
| **Aerobic STA** | `staminaPercent` | `GetTargetStamina()` / `m_fTargetStamina` | **Not** affected by W′→engine presentation mapping |
| **W′ normalized** | `wPrimePool01` | AnaerobicBurst / CP–W′ pool | `0`=empty, `1`=full; **prefer this** |
| **W′ joules** | `wPrimeJoules` / `wPrimeMaxJoules` | same + preset max | Absolute joules + capacity |
| Legacy alias | `anaerobicPercent` | same as `wPrimePool01` | **Deprecated** |
| Engine bar FX | (API **does not** expose) | transient `GetStamina()` | Native sway/blur only; **not** aerobic authority |

```c
RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    float sta = info.staminaPercent;
    float w01 = info.wPrimePool01;
    float wJ = info.wPrimeJoules;
    float wMax = info.wPrimeMaxJoules;
}
```

Advanced: `ctrl.RSS_GetWPrimeBurst().GetPool()` / `GetWPrimeJoules()` via `GetRssController`.

---

## Usage example

```c
IEntity player = SCR_PlayerController.GetLocalControlledEntity();
if (!player)
    return;

RSS_PlayerInfo playerInfo = SCR_RSS_API.GetPlayerInfo(player);
if (playerInfo.isValid)
{
    PrintFormat("STA=%1%% W'=%2%% (%3/%4 J) sprintOk=%5",
        playerInfo.staminaPercent * 100.0,
        playerInfo.wPrimePool01 * 100.0,
        playerInfo.wPrimeJoules,
        playerInfo.wPrimeMaxJoules,
        playerInfo.sprintAllowed);
}
```

## Methods

### GetRssController / GetPlayerInfo / GetEnvironmentInfo / IsRssManaged

Same as Chinese doc. **`RSS_PlayerInfo` fields:**

| Field | Type | Notes |
|-------|------|-------|
| staminaPercent | float | **Aerobic** 0~1 |
| speedMultiplier | float | Speed multiplier |
| currentSpeed | float | Horizontal m/s |
| movementPhase | int | 0 idle … 3 sprint |
| isSprinting | bool | Sprinting |
| isExhausted | bool | Exhausted (aerobic-side) |
| isSwimming | bool | Swimming |
| currentWeight | float | kg |
| wPrimePool01 | float | **W′** 0~1 |
| wPrimeJoules | float | **W′** joules now |
| wPrimeMaxJoules | float | Preset **W′_max** joules |
| anaerobicPercent | float | deprecated = `wPrimePool01` |
| sprintCooldownRemainingSec | float | Often 0 |
| sprintAllowed | bool | Sprint gate |
| isValid | bool | Valid payload |

Full environment field table: see Chinese [`../RSS_API.md`](../RSS_API.md).

## Notes

1. Always check `isValid`.
2. Static cache — copy fields if you need to keep them.
3. Players: client OK for local compute; AI: prefer server.
4. Do **not** use engine `GetStamina()` as aerobic or W′ authority.
5. File export JSON may omit W′; use `GetPlayerInfo` for W′ in script.
