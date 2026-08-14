# RSS v6 calculation logic (authoritative)

> [中文](../RSS_v6_计算逻辑权威版.md) | **English**
>
> **Math kernel**: 6.0.0 | **Code alignment**: 6.1.x (2026-08-14 audit-aligned)  
> Supersedes v5 and older “willpower plateau / Givoni / legacy module names”. Source wins.  
> ⚠️ 2026-08-14 math audit: §1–§7 corrected to actual code/baked values; drift log in [RSS_数学模型审计_2026-08-14.md](../RSS_数学模型审计_2026-08-14.md).  
> **6.1.x speed default**: `V6_APPLY_CP_METABOLIC_SPEED_CAP = false` (drain-only: metabolic overspend hits STA/W′; do not press CP-cruise `SetSpeedLimit` by default). W′ may drive engine sway/blur via transient (`SCR_RSS_SprintGate`).

---

## 1. North-star loop

Per-tick dual loop (speed servo **17 ms** / player CallLater; stamina integration **0.2 s**; AI 100 ms):

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]
```

> **Execution-order note**: `SetSpeedLimit` (invert) runs in PhaseA first; `P(v)`/`TickPower` (CP–W′ update) run in PhaseB after, so invert uses the **previous tick's CP/W′ snapshot**. This is a standard 1-tick delay in a discrete control loop (17 ms, imperceptible), not a defect.

**Drain speed**: metabolism `v_drain = v_meas` (no min to v_limit); EPOC peak sampling and fatigue integral still use `min(v_meas, v_limit)`. When **`v_meas > v_limit + 0.12 m/s`**, **W′** books measured speed (excess goes to W′ first).

---

## 2. Module map

| Role | File |
|------|------|
| Metabolic power P | `SCR_RSS_MetabolismModel.c` |
| CP–W′ | `SCR_RSS_CriticalPowerModel.c` / `SCR_RSS_AnaerobicBurst.c` |
| Drain coordination | `SCR_RSS_UpdateCoordinator.c` |
| Speed | `SCR_RSS_SpeedCalculator.c` / `SCR_RSS_DrainCalculator.c` |
| Aerobic recovery / EPOC | `SCR_RSS_RecoveryCalculator.c` / `SCR_RSS_EpocState.c` |
| Integral fatigue | `SCR_RSS_FatigueSystem.c` |
| Main loop | `PlayerBase_UpdateLoop.c` |
| Config | `SCR_RSS_Settings.c` / `SCR_RSS_Params.c` |

---

## 3. Metabolic power

### 3.1 Pandolf + ACSM blend

- Walk / Idle: `v ≤ 1.97 m/s` uses **LCDA** (backpacking); `>1.97` uses Pandolf (Santee steep downhill, fitness bonus)
- Run/Sprint: ACSM `P = a + b·v + c·v²`, **linear crossfade** over 2.0–2.4 m/s (C⁰, not C¹)

### 3.2 Aerobic drain

```
aerobic_drain_rate_per_s = min(P, CP) × energy_to_stamina_coeff × 0.72   // 0.72 = V6_STAMINA_DRAIN_CALIBRATION
drain_per_0p2s = drain_rate_per_s × 0.2
```

Player-path `load_metabolic_dampening` **still exists** (`MetabolismPowerWatts` tail step, gated by `GetLoadMetabolicDampening()`); effort-compensation fudge was removed.

---

## 4. CP–W′

### 4.1 Dynamic CP

```
CP₀ = critical_power_watts (three presets)
CP_load  = CP₀ × (1 − 0.002 × max(0, L_kg − 10))
CP_slope = CP_load × (1 − 0.015 × g²)   when g>0 (g=grade%×0.01)
CP_env   = CP_slope × envCpMult
CP_final = CP_env × (1 − 0.18 × Fatigue_norm)    // floor 0.82; no separate fatigueCpMult factor
```

Downhill does **not** boost CP (slope cost stays in Pandolf — avoid double count).

### 4.2 W′ discharge

```
dW′/dt = −max(0, P − CP_final)   [J/s]
```

### 4.3 W′ refill

| Preset | Mechanism |
|--------|-----------|
| **Elite** (CP≤410 W) | Skiba biexponential: `k_fast=0.15`, `k_slow=0.010`, `W′_lim=0.5·W′_max` |
| **Standard/Tactical** | Linear `w_prime_recovery_w_per_s` (no timed Sprint CD lock) |

> ⚠️ **Current implementation**: `UsesSkibaRecovery()` checks `cp0 ≤ 2000 W`, so all three presets (CP ≤ 1030 W) use Skiba; the linear branch is currently dead code (pending fix, see audit §2.1).

### 4.4 Sprint speed

```
v_sprint = invert(P_available)
P_available = min(sprint_power_cap, CP + W′/Δt)
```

Elite baked `sprint_power_cap_watts = 2355` (35 kg full Sprint to ANA gate ≤15 s).

---

## 5. Aerobic pool and speed

- **No main-bar plateau**: Run speed = phase target m/s (Elite 1.4/2.8/4.0) × load
- **Low STA** (<5%): `GetDynamicLimpMultiplier` + `CollapseTransition` 5 s damping
- **Sprint gate**: `IsSprintAllowedWithCp` (aerobic threshold + **W′ overspeed Schmitt latch**; **no** short-burst timed CD)
  - Off band: pool ≤ `anaerobic_sprint_enable_threshold + V6_WPRIME_OVERSPEED_HYSTERESIS` (default ≈25%)
  - Rearm band: pool > `threshold + V6_WPRIME_OVERSPEED_REARM` (default ≈60%); prevents Sprint↔Run chatter around ≈20–25% while holding Sprint
  - Release before hitting off band: leftover W′ can sprint again immediately; after off band must recover to rearm
  - `burst_cooldown_*` kept for compat; load burst relief still uses `TACTICAL_SPRINT_COOLDOWN` (15 s)

---

## 6. Fatigue and EPOC

### 6.1 Integral fatigue I(t)

```
dI/dt = w·max(P−CP, 0) − R    // integrates only power above CP; downhill slope term zeroed
w = 1 + k_load·(L/W) + k_slope·G² + k_terrain·(η−1)
R = k_recovery × (1 − I/I_max)² × P   (stationary / low P)
```

Outputs: `GetCpFatigueMultiplier()`, `GetFatigueIntegralNorm()` → CP and W′ k_fast/k_slow.

### 6.2 EPOC

Post-exercise delayed drain ∝ **peak intent metabolic power** (`GetEpocSamplePowerWatts` → `EpocState.UpdateExercisePowerSample`):

- Sample speed = `min(v_meas, v_limit)` (with hard clamp off, do not feed downhill overshoot power into O₂ debt)
- Stored peak further clamped to `CP × (1 + EPOC_MAX_POWER_EXCESS_RATIO)`; without sprint / W′ overspeed accounting, further clamp to **CP**
- Peak ≤ `CP × EPOC_AEROBIC_CP_RATIO` → weak EPOC (`× EPOC_AEROBIC_DRAIN_MULT`)
- Fast peak decay when current sample is clearly below peak (`EPOC_PEAK_DECAY_FAST_*`)

---

## 7. Three presets (35 kg anchors)

| Param | Elite | Standard | Tactical |
|-------|-------|----------|----------|
| CP (W) | 889.74 | 1010.81 | 1029.80 |
| W′_max (J) | 21322 | 31038 | 31655 |
| sprint_cap (W) | 2355 | 2724 | 2748 |
| W′ recovery | Skiba | linear 11.86 W/s | linear 14.28 W/s |

> Actual values are `SCR_RSS_SettingsPresetBake.c` + `tools/optimized_rss_config_*_v6.json` (both consistent). Skiba/linear W′ dispatch is not yet tiered (see §4.3 note).

---

## 8. Acceptance commands

```bash
python tools/test_v5_smoke.py
python tools/test_v6_smoke.py
python tools/bench_physio_anchors.py
python tools/test_acft_2mile.py
python tools/check_script_size.py
```

---

## 9. Explicitly removed (v6)

- Willpower plateau (25%/35% constant-speed Run)
- Fixed anaerobic `0.12/s` bar drain
- AI-only `stamina^0.6` curve
- Duplicate `CalculatePandolfDrain`
- W′_ratio → speed_mult piecewise table (conflicts with invert loop)

---

## 10. Phase 4+ optional (off by default)

HPTF heat stress, Fitts operational fatigue, SOPMOD gear table, pack sway, Yerkes-Dodson, SAFTE — see Phase 4+ plan.
