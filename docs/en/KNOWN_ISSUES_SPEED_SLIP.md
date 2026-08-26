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
- `V6_RUN_GAIT_FLOOR_MS = 2.2` (Run gait band floor; below this and past the soft band, skip out-of-band cruise cap)
- `V6_RUN_GAIT_DEMOTE_TO_WALK = true` (true = do not write Walk m/s onto Run via `SetSpeedLimit`; after W′ empty, switch to engine Walk gait — see `V6_CP_OUT_OF_BAND_WALK_OVERRIDE`; hard clamp on still raises the floor)
- `V6_CP_OUT_OF_BAND_WALK_OVERRIDE = true` (after W′ empty and invert out of Run band, `SetDynamicSpeed(0.5)` forces Walk)
- `V6_GAIT_SPEED_LIMIT_MIN_FRAC = 0.50` (min `SetSpeedLimit` vs phase top; exhausted Run still uses this to avoid slide)
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

> **v6.1.7+ CP-cruise path**: after W′ depletion, Walk is intentionally floored at `V6_CP_HIKE_FLOOR_MS=1.0` so metabolic invert does not crush `v_limit` to ~0.5 m/s (Walk anim vs ~1.7 m/s physics = slide). “Should drop below 1.0” below applies only to the March intent floor (`V6_WALK_START_MIN_MS=0.35`), not to the disarmed metabolic cap.

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
- **Do not** write Walk m/s onto the Run phase via `SetSpeedLimit` (slide). Out of the Run band, use CapsLock-style `SetDynamicSpeed(0.5)+SetShouldApplyDynamicSpeedOverride` (`V6_CP_OUT_OF_BAND_WALK_OVERRIDE`). The wheel is locked while the override is on; restore after releasing W / W′ refill.

### Mitigation

- Default: `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`.
- Limits via `SetSpeedLimit` (min-merge with foliage, etc.); out-of-band Run also switches to engine Walk gait.
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
- Cruise invert: speed-servo grade clamped to 15%, terrain η excluded from invert; Walk floor `V6_CP_HIKE_FLOOR_MS=1.0` (~3.6 km/h packed hike). Mesh 15–18° + 29 kg no longer crushes **Walk** to a 0.4 m/s crawl; drain still uses measured grade.
- Run gait band: write the cruise cap only if CP invert ≥ `V6_RUN_GAIT_FLOOR_MS` (or the soft band). Out of band: do **not** write Walk m/s onto Run via `SetSpeedLimit`. After W′ empty while still holding movement, switch to engine Walk gait (`V6_CP_OUT_OF_BAND_WALK_OVERRIDE`; HUD `步态覆盖=on`, `类型=Walk`, ~1.0–1.45 m/s). Releasing W restores the mouse wheel. `SetSpeedLimit` stays ≥ 0.5× phase top.

### Retest

- Occasional `v_meas` slightly above limit is OK (do not enable physics clamp).
- Steep loaded, W′ disarmed (~<25%), still holding W: HUD `步态覆盖=on`, `类型=Walk`, `v_meas` about 1.0–1.45 m/s. Crossing a crest downhill **stays Walk** (no override flicker, no Walk anim vs 3 m/s physics). About 0.25 s after releasing W, `步态覆盖=off`.
- After override, uphill Walk (`v_meas`≈1.45, `v_limit`≈1.0): `超速记账=off`; W′ must **not** keep dumping. Downhill Walk with `P_met`<CP should recover. True slip (`v_meas`≥~1.65) may still show `on(phys)`.
- After switching to Walk (~10–13°, 29 kg, W′ empty): `v_limit` should be about **1.0 m/s** (hike floor) and `最终倍` ≥ 0.5× Walk top. Do not accept `v_limit≈0.5` with Walk anim vs ~1.7 m/s physics (slide).
- Standing Idle: `最终倍` should be about **0.999** (keep the limit source), not `0.0027` / HUD `倍率0x`. The first Run/Walk frame after pressing W should enter the gait band immediately, not crawl at `v_limit≈0.48`.
- Exhausted while still holding Run: after `Exhausted: limp speed`, `最终倍` should snap to about **0.5×** (exhausted jog ~1.8 m/s), not slew toward 0.15 at 1.25/s. After switching to Walk, `v_limit` should be about **1.0 m/s**. Downhill `v_meas` may still exceed command (physics clamp off).
