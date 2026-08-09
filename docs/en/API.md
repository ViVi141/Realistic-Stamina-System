# Realistic Stamina System (RSS) — external mod API

> [中文](../RSS_API.md) | **English**
>
> Interface for other mods to read player stamina state and environment info from RSS.

## Implementation locations (matches code)

| Content | Path |
|---------|------|
| API entry | `scripts/Game/RSS/NetworkConfig/SCR_RSS_API.c` (`SCR_RSS_API`, `RSS_PlayerInfo`, `RSS_EnvironmentInfo`) |
| Data export | `scripts/Game/RSS/NetworkConfig/SCR_RSS_DataExport.c` (`SCR_RSS_DataExport`, `RSS_ExportData`, `RSS_ExportPlayerEntry`) |

## Dependencies

- Depend on the RSS mod (`Realistic Stamina System`)
- Target entity must be a character (`ChimeraCharacter`)

## Usage example

```c
IEntity player = SCR_PlayerController.GetLocalControlledEntity();
if (!player)
    return;

RSS_PlayerInfo playerInfo = SCR_RSS_API.GetPlayerInfo(player);
if (playerInfo.isValid)
{
    PrintFormat("STA: %1%% | speed mult: %2 | speed: %3 m/s",
        playerInfo.staminaPercent * 100.0,
        playerInfo.speedMultiplier,
        playerInfo.currentSpeed);
}

RSS_EnvironmentInfo envInfo = SCR_RSS_API.GetEnvironmentInfo(player);
if (envInfo.isValid)
{
    PrintFormat("temp: %1°C | rain: %2 | indoor: %3",
        envInfo.temperature,
        envInfo.rainIntensity,
        envInfo.isIndoor);
}
```

## API methods

### SCR_RSS_API.GetRssController(IEntity entity)

Returns the RSS-managed character controller (advanced).

| Param | Type | Notes |
|-------|------|-------|
| entity | IEntity | Character entity |
| **Returns** | SCR_CharacterControllerComponent | Controller or null |

---

### SCR_RSS_API.GetPlayerInfo(IEntity entity)

Current stamina and locomotion state.

| Param | Type | Notes |
|-------|------|-------|
| entity | IEntity | Character entity |
| **Returns** | RSS_PlayerInfo | Player info struct |

**RSS_PlayerInfo fields:**

| Field | Type | Notes |
|-------|------|-------|
| staminaPercent | float | Stamina fraction (0.0 ~ 1.0) |
| speedMultiplier | float | Current speed multiplier (0.15 ~ 1.0) |
| currentSpeed | float | Horizontal speed (m/s) |
| movementPhase | int | 0=idle, 1=walk, 2=run, 3=sprint |
| isSprinting | bool | Sprinting |
| isExhausted | bool | Exhausted |
| isSwimming | bool | Swimming |
| currentWeight | float | Encumbrance (kg) |
| isValid | bool | Whether data is valid |

Also prefer `wPrimePool01` when present in code/export (legacy `anaerobicPercent` deprecated) — see source.

---

### SCR_RSS_API.GetEnvironmentInfo(IEntity entity)

Environment at the character location.

| Param | Type | Notes |
|-------|------|-------|
| entity | IEntity | Character (indoor checks) |
| **Returns** | RSS_EnvironmentInfo | Environment struct |

**RSS_EnvironmentInfo fields:**

| Field | Type | Notes |
|-------|------|-------|
| temperature | float | Air temperature (°C) |
| rainIntensity | float | Rain (0.0 ~ 1.0) |
| windSpeed | float | Wind (m/s) |
| windDirection | float | Wind direction (degrees) |
| surfaceWetness | float | Surface wetness (0.0 ~ 1.0) |
| totalWetWeight | float | Total wet mass (kg) |
| isIndoor | bool | Indoors |
| heatStressMultiplier | float | Heat-stress multiplier |
| heatStressPenalty | float | Heat-stress penalty |
| coldStressPenalty | float | Cold-stress penalty |
| isValid | bool | Whether data is valid |

---

### SCR_RSS_API.IsRssManaged(IEntity entity)

Whether the entity is managed by RSS.

| Param | Type | Notes |
|-------|------|-------|
| entity | IEntity | Character entity |
| **Returns** | bool | Managed / valid |

## Data export (file bridge)

When `m_bDataExportEnabled` is on, the server writes player data JSON every `m_iDataExportIntervalMs`:

- **Path**: `$profile:RSS_PlayerData.json`
- **Format**: JSON with `timestamp` and `players` array
- **Use**: external apps (command consoles, etc.) poll the file

Example (fields align with `RSS_ExportPlayerEntry` / `RSS_ExportData`; see `SCR_RSS_DataExport.c`):

```json
{
  "timestamp": 1234567890,
  "players": [
    {
      "playerId": 1,
      "playerName": "Player1",
      "staminaPercent": 0.85,
      "speedMultiplier": 0.92,
      "currentSpeed": 4.2,
      "movementPhase": 2,
      "isSprinting": false,
      "isExhausted": false,
      "isSwimming": false,
      "currentWeight": 12.5,
      "temperature": 22.0,
      "rainIntensity": 0.0,
      "windSpeed": 2.1,
      "isIndoor": false
    }
  ]
}
```

`timestamp` comes from `GetGame().GetWorld().GetWorldTime()` (engine world time in ms; **not** Unix epoch seconds — interpret as needed or treat as a sequence id).

> If `GetEnvironmentInfo` is invalid at export time, `temperature` / `rainIntensity` / `windSpeed` / `isIndoor` fall back to code defaults (see `SCR_RSS_DataExport.ExportToFile()`).

Config (`RealisticStaminaSystem.json`):

- `m_bDataExportEnabled` (default false)
- `m_iDataExportIntervalMs` (default 1000)

## Notes

1. **Always check `isValid`** after `GetPlayerInfo` / `GetEnvironmentInfo`.
2. **Reuse of return values**: API returns statically cached structs; each call overwrites the previous — copy fields if you need to keep them.
3. **Side**: player entities on client yield local computed data; AI entities should be queried on the server.
4. **Export** runs server-side only; clients read the profile JSON.
5. **Server environment**: player STA/speed may be client-computed; environment (indoor, temp, rain, …) is computed on the server via `EnvironmentFactor.ForceUpdate()` before export so indoor/outdoor data stay correct.
