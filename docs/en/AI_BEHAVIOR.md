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
| **Disable AI Stamina Drain** | `m_bDisableAIStaminaCalc` | **On** (default): cheap limit + Tobler; offline vs player ~**mean 15% / max ~79%**. **Off**: drain pipeline + CP/Sprint caps; ~**mean 1% / max ~2%** (`bench_player_ai_speed_gap.py`) |

## 3. Tick (`DisableAIStaminaCalc=Off`)

1. Cheap limit → position speed → Y-delta grade → LOD terrain sample → shared heat  
2. `CalculateTotalDrainRate` → W′/fatigue (near/mid) → `UpdateStaminaValue` → `AIManager.Tick`  
3. Schedule next — **skip** player Phase B/C.

## 4. LOD

Full: 600 / 1000 / 2500 ms. Light: 800 / 1500 / 3000 ms. Terrain sample: 2 s / 5 s; far = terrain 1.0, skip W′/fatigue.

### Perf vs 6.2.6 (PerfProbe, µs/call)

Baseline **6.2.6 @ `883a051`** vs **6.3.0 tip** `RunNearestAi`.

| Scenario | 6.2.6 AI | 6.3.0 prod | ≈faster |
|----------|----------|------------|---------|
| Default limit | =A ~48 | **`03f` ≈ 16.7** | **~2.9×** |
| Drain on (est.) | =B ~55 | ~22 | ~2.5× |

Probe D≈1.7 / F≈7 are **not** production. Player: `UpdateSpeed` ~6×, A ~3.3× — see root `CHANGELOG.md` **[6.3.0]**.

---

*Doc version: 2026-09-06, aligned with 6.3.0 (AI pipeline + SCENARIO/group gait pin).*
