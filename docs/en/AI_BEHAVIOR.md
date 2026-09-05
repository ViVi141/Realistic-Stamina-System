# RSS AI behavior (current)

> [中文](../RSS_AI_行为说明.md) | **English**
>
> How **Realistic Stamina System (RSS)** affects **AI characters**, and where it is implemented.  
> Source of truth: `scripts/Game/RSS/AI/`. Legacy group locomotion / GroupSync / group proxy modules are **removed**; historical design (Chinese archive): `../RSS_AI体力集成全盘设计方案.md`.

## 0. Design intent

- AI shares the same Pandolf / CP–W′ stamina main loop as players (server).
- On top of stamina values, a **per-agent** behavior layer (~500 ms): state machine → speed cap → intent filter → combat decay.
- **No** edits to vanilla behavior trees; constrain via `SetMovementTypeWanted` + `SCR_RSS_SpeedBridge` / `SetSpeedLimit`.
- Group cohesion (unified pace, rest waypoints, distant proxy, etc.) is **not active** (`isThreatened` is hard-coded `false` in `SCR_RSS_AIManager`).

## 1. Implementation files

| File | Role |
|------|------|
| `scripts/Game/Integration/PlayerBase.c` | Owns `SCR_RSS_AIManager`; main loop calls `Tick` |
| `scripts/Game/RSS/AI/SCR_RSS_AIManager.c` | Behavior throttle + state machine + SpeedCap / IntentFilter / CombatDecay |
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaState.c` | 6-state stamina FSM (hysteresis) |
| `scripts/Game/RSS/AI/SCR_RSS_AISpeedCap.c` | Movement type + SpeedBridge limits |
| `scripts/Game/RSS/AI/SCR_RSS_AIIntentFilter.c` | Disable Attack/chase intents when exhausted |
| `scripts/Game/RSS/AI/SCR_RSS_AICombatDecay.c` | Perception / fire rate / skill decay |
| `scripts/Game/RSS/AI/SCR_RSS_AIInjuryLink.c` | Injury → faster drain / slower recovery |
| `scripts/Game/RSS/AI/SCR_RSS_AIUpdateInterval.c` | Distance LOD intervals, Workbench preview filter |
| `scripts/Game/RSS/AI/SCR_RSS_AIConstants.c` | `RSS_AI_*`, `RSS_PERF_AI_*` |

## 2. Settings (`SCR_RSS_Settings`)

| Field | Effect |
|-------|--------|
| `m_bEnableAIStaminaCombatEffects` | **Master switch**: FSM, caps, intent filter, combat decay, injury link (new JSON often defaults false; host can enable) |
| `m_bDisableAIAllCalc` | Server AI skips RSS main loop entirely |
| `m_bDisableAIStaminaCalc` | **Default ON**: light path — encumbrance + gait limit only; skip terrain/env/Pandolf |
| `m_bEnableMudSlipMechanism` | Mud slip (default off; independent of AI behavior layer) |

## 3. Per-tick order (server AI)

1. Main loop updates speed/drain on `SCR_RSS_AIUpdateInterval` (players ~17 ms; AI distance LOD or fixed 100 ms).
2. `SCR_RSS_AIManager.Tick` (behavior layer **500 ms** throttle):
   - accumulate stationary time
   - `SCR_RSS_AIStaminaState.Tick`
   - `SCR_RSS_AISpeedCap.Apply`
   - `SCR_RSS_AIIntentFilter.Apply`
   - `SCR_RSS_AICombatDecay.Apply`
3. Pandolf drain/recovery → `UpdateStaminaValue` (may include **injury** multipliers).

## 4. Stamina state machine

| State | Approx. STA | Movement | Combat |
|-------|-------------|----------|--------|
| FRESH | ≥80% (hysteresis) | no intervention | 100% |
| WINDED | 50–80% | no Sprint | slight decay |
| FATIGUED | 25–50% | RUN + ~65% speed | clear decay |
| EXHAUSTED | 10–25% | WALK + ~40% speed | heavy decay; may block Attack/chase |
| COLLAPSED | <10% | near IDLE | heaviest decay |
| RECOVERING | recovering | WALK + continuous curve | medium decay |

Thresholds/multipliers: `SCR_RSS_AIConstants`.  
Stationary time drives `COLLAPSED→RECOVERING` and forced recovery at very low STA; cleared when moving.

## 5. Performance: distance LOD

| Mechanism | Constants | Notes |
|-----------|-----------|-------|
| Full-path LOD | `RSS_PERF_AI_LOD_*` | Near 400 m→400 ms; mid→700 ms; far→2000 ms |
| Light LOD | `RSS_PERF_AI_LIGHT_*` | When `DisableAIStaminaCalc`: 500 / 1000 / 2500 ms |
| Player origin cache | `RSS_PERF_AI_PLAYER_POS_CACHE_TTL_SEC` | Shared 0.25 s cache; avoids per-AI `GetPlayers` alloc |
| LOD master | `RSS_PERF_AI_DISTANCE_LOD_ENABLED` | If `false`, AI uses fixed `RSS_AI_SPEED_UPDATE_INTERVAL_MS` (100 ms) |

Default `DisableAIStaminaCalc` uses `SCR_PlayerBaseAiLightTickHelper` (no terrain/env/Pandolf). For zero AI RSS cost, enable `m_bDisableAIAllCalc`.

## 6. Injury link

Blood-based drain/recovery multipliers (`SCR_RSS_AIInjuryLink.c`, `RSS_AI_INJURY_*`).

---

*Doc version: 2026-09-05, aligned with 6.2.26 AI light-path perf fix.*
