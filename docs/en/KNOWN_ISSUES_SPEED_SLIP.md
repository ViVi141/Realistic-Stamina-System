# RSS known issues: Walk floor too fast & foot-slide

> [中文](../RSS_已知问题_限速与滑步.md) | **English**

Recorded: 2026-07-24  
Updated: 2026-08-09 — aligned with **6.1.x drain-only** defaults: `V6_APPLY_CP_METABOLIC_SPEED_CAP = false` (CP/aerobic cruise cap does not write `SetSpeedLimit` by default; overspend drains STA/W′ only).  
Updated: 2026-08-26 — **since v6.1.7 the default is `= true`** (after W′ depletion, CP-cruise command speed is pressed via `SetSpeedLimit`); entries below marked "default off / drain-only" apply to ≤6.1.5 only.

Related switches (`SCR_RSS_Constants`; source wins):

- `V6_APPLY_STAMINA_SPEED_LIMIT = true` (encumbrance/slope still write `SetSpeedLimit`)
- `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false` (**off** horizontal physics hard/soft clamp)
- `V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP = false` (**off** CP-cruise physics bypass clamp)
- `V6_APPLY_CP_METABOLIC_SPEED_CAP = true` (**default on since v6.1.7**: after W′ depletion, press CP/aerobic cruise via SetSpeedLimit; ≤6.1.5 default off, drain-only)
- `V6_USE_MARCH_GAIT_SPEEDS = false` (March band off)
- `V6_RUN_GAIT_FLOOR_MS = 2.2` (Run gait band floor; demote to Walk when metabolic cap is on and below floor)
- `V6_RUN_GAIT_DEMOTE_TO_WALK = true` (auto-disabled if hard clamp on → raise floor instead)
- `V6_WALK_START_MIN_MS = 0.35` (absolute Walk floor)

### Conclusion (foot-slide)

- **Likely root cause**: twisting horizontal `Physics` velocity every frame (`ClampOwnerHorizontalSpeed` / `PrepareControls`) desyncs displacement from phase animation top speed → foot-slide.
- **Rule**: **only** merge limits via `SetSpeedLimit`; **never** directly rewrite `Physics` velocity.
- **6.1.x default**: metabolic cruise cap off → Run intent can reach engine top; excess metabolism is paid in STA/W′, not by crushing `v_limit` to match physics.
- **Command vs physics**: `SetSpeedLimit` → `OverrideMaxSpeed` only presses **command** speed; `v_meas` may exceed `v_limit` (acceptable; do not enable physics clamp).
- **No reliable** cadence / anim playback-rate API on the script side; hard clamps only twist translation, not step rate.
- **Log semantics**: `超速记账=on(phys)` = `v_meas > v_limit+ε`; `on(sprint)` = sprinting with W′ overspeed accounting armed.

---

## 1. Walk floor too high (≥ ~1 m/s); vanilla can be slower

### Symptom

- Under RSS, effective Walk often sits **above ~1.0 m/s** (logs often show `v_limit` / `v_meas` ≈ 1.2–1.3 m/s @ ~30 kg mild slope).
- **Vanilla can be slower**; Walk feels “floored too high”.

### Fix (2026-07-25)

| Location | Behavior |
|----------|----------|
| `CalculateFinalAbsoluteSpeed` / `GetMarchAbsoluteSpeedMs` | `startMin` / Walk clamp floor → `V6_WALK_START_MIN_MS` (**0.35**) |
| `ENGINE_WALK_TOP_MS = 1.45` | Intent top when March off; fatigue/load can multiply lower |

### Retest

- Unloaded flat / 30 kg / uphill: Walk `v_limit` should clearly drop below ~1.0 m/s.
- If still fast: check intent stuck at engine top, or slope/load penalties not applying.

---

## 2. Foot-slide (historical)

### Symptom (hard clamp on)

- Horizontal physics pressed by RSS `SetSpeedLimit` + `ClampOwnerHorizontalSpeed` below **current phase animation gait top** → sliding feet.

### Confirmed constraints

- No reliable anim playback-rate API; `CharacterCommand` overwrites anim vars.
- **Do not** fake Walk alignment with `SetDynamicSpeed(0.5)` (locks phase).

### Mitigation

- Default: `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`.
- Limits only via `SetSpeedLimit` (min-merge with foliage, etc.).
- Since v6.1.7: CP metabolic cap on by default (`V6_APPLY_CP_METABOLIC_SPEED_CAP = true`); ≤6.1.5 default off.

---

## Log cues for retest

- Walk: `类型=Walk`; if still ≥1.2 while vanilla is slower → issue 1.
- With hard clamp on Run: `v_meas` pinned but anim fast → foot-slide; off clamp should match better.
- `v_pos` spikes can be ignored (position-diff noise).

---

## 3. Downhill stop EPOC spike (fixed)

### Symptom (before fix)

- Downhill Run: `v_meas` ≫ `v_limit`; on stop, EPOC sampled full `P_fat` → drain spike.

### Mitigation

- STA/fatigue integral still books on `v_meas`; **EPOC only** uses `GetEpocSamplePowerWatts` = intent power inside the speed limit.
- Without sprint and without W′ armed overspeed accounting, peak is further clamped to **CP**.
- Retest: same downhill cruise (`W'=0`, overspeed accounting off) then hard stop → weak EPOC.

---

## 4. Late-game speed jitter after W′ empty

### Symptom

- Up/downhill Run: `最终倍`/`v_limit` pinned, but `v_meas` jitters and `P_met` spikes; overspeed accounting off.

### Causes

1. `SetSpeedLimit` only presses command; physics can still overshoot.
2. Periodic uncap to sample engine top every 2 s caused Chimera `OverrideMaxSpeed(1)` spikes (periodic uncap banned).
3. Flat cruise cap was too tight downhill (when metabolic-cap path is on, downhill skips that hat).

### Mitigation (current defaults)

- Sample engine top **once**.
- **No** physics clamp; excess paid in STA/W′.
- Since v6.1.7 `V6_APPLY_CP_METABOLIC_SPEED_CAP` is on by default: flat/uphill presses cruise; downhill/extreme grades skip physics-clamp logic.
- Run gait band (only meaningful with metabolic cap on): CP invert ≥ `V6_RUN_GAIT_FLOOR_MS` stays Run; lower may demote to Walk.

### Retest

- Occasional `v_meas` slightly above limit is OK (do not enable physics clamp).
- With metabolic cap on: if Walk demotions are too frequent, lower `V6_RUN_GAIT_FLOOR_MS` slightly; confirm `V6_RUN_GAIT_DEMOTE_TO_WALK=true` and hard clamp off.
