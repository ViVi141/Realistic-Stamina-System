# RSS AI behavior (current)

> [中文](../RSS_AI_行为说明.md) | **English**
>
> How **Realistic Stamina System (RSS)** affects **AI characters**.  
> Source: `scripts/Game/RSS/AI/`. Precision pipeline design (Chinese): [`../RSS_AI_体力链路方案.md`](../RSS_AI_体力链路方案.md).

## 0. Design intent

- **Values**: with `DisableAIStaminaCalc=Off`, AI uses `SCR_RSS_AIStaminaPipeline` — same Pandolf / CP–W′ / `UpdateStaminaValue` core as players, **without** player `UpdateSpeed` or per-AI full environment.
- **Speed limit**: always cheap (encumbrance + V6 phase / limp).
- **Behavior layer** (optional Combat): FSM → SpeedCap → IntentFilter → CombatDecay.
- No vanilla BT edits.

## 1. Key files

| File | Role |
|------|------|
| `PlayerBase_UpdateLoop.c` | AI branch: light limit or `AIStaminaPipeline.Tick` |
| `SCR_RSS_AIStaminaPipeline.c` | AI drain / recovery / W′ / fatigue |
| `SCR_RSS_AISharedEnvCache.c` | Server-wide 1 Hz heat approx |
| `SCR_PlayerBaseAiLightTickHelper.c` | Cheap speed limit |
| `SCR_RSS_AIManager.c` | Behavior orchestration |
| `SCR_RSS_AIConstants.c` | LOD / FSM constants |

## 2. Settings

| Menu label | Field | Effect |
|------------|-------|--------|
| **AI Fatigue Behaviors** | `m_bEnableAIStaminaCombatEffects` | FSM, tier cap, intent filter, combat decay, injury link (needs drain On) |
| **Disable All AI RSS** | `m_bDisableAIAllCalc` | Stop AI RSS loop entirely |
| **Disable AI Stamina Drain** | `m_bDisableAIStaminaCalc` | **On** (default): cheap limit only. **Off**: AI drain pipeline (no player UpdateSpeed) |

## 3. Tick (`DisableAIStaminaCalc=Off`)

1. Cheap limit → position speed → Y-delta grade → LOD terrain sample → shared heat  
2. `CalculateTotalDrainRate` → W′/fatigue (near/mid) → `UpdateStaminaValue` → `AIManager.Tick`  
3. Schedule next — **skip** player Phase B/C.

## 4. LOD

Full: 600 / 1000 / 2500 ms. Light: 800 / 1500 / 3000 ms. Terrain sample: 2 s / 5 s; far = terrain 1.0, skip W′/fatigue.

---

*Doc version: 2026-09-06, aligned with 6.2.28 AI stamina pipeline.*
