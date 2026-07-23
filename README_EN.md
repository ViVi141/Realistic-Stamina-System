# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README (current)](README_EN.md) | [Short hub](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** is a stamina and movement-speed mod for **Arma Reforger**.  
It dynamically adjusts speed and drain from stamina, encumbrance, slope, and environment using exercise-physiology models (Pandolf / ACSM / Critical Power–W′), not flat constant deductions.

- **Author**: ViVi141 (747384120@qq.com) · GitHub [@ViVi141](https://github.com/ViVi141)
- **License**: [AGPL-3.0](LICENSE)
- **Mod ID / GUID**: `Realistic Stamina System` / `68649101601CC93D`
- **Recommended game**: Arma Reforger **1.7+** (foliage/wire slowdown merges with RSS limits)
- **Config version**: `SCR_RSS_ConfigManager.CURRENT_VERSION` = **6.0.0**

> Full release history: [CHANGELOG.md](CHANGELOG.md). This README documents the **current tree**; older formula/path names are called out where v6 changed them.

The Chinese README is the most detailed narrative companion: [README_CN.md](README_CN.md).

---

## What it does

When stamina is high, march-gait target speeds are available. As STA and W′ fall, the drain↔speed-limit loop tightens. Near empty STA you limp. Load mainly raises metabolic cost and CP pressure; configurable speed penalties remain.

**Physiological anchor**: ACFT 2-mile, ages 22–26 male, 100 pts ≈ **15:27** (`tools/test_acft_2mile.py` / anchor pipeline).

### Feature highlights

#### v6 power budget & speed loop
- **CP–W′**: Pandolf/ACSM power → dynamic Critical Power → W′ (joules) discharge/refill  
  - Elite: Skiba biexponential; Standard/Tactical: linear `w_prime_recovery_w_per_s`  
  - Sprint ≈ `invert(min(sprint_power_cap, CP + W′/Δt))`
- **March gaits**: Walk/Run/Sprint m/s targets; **no 25%/35% willpower plateau**
- **Limp**: below ~5% STA (`SMOOTH_TRANSITION_END`)
- **5 s collapse damping** (`SCR_RSS_CollapseTransition`): SmoothStep when crossing the limp threshold (“legs get heavy”, not “engine cut”)
- **v_drain**: `min(v_meas, appliedLimit)` for accounting
- **SpeedBridge**: `SetSpeedLimit` as a separate source, **min** with foliage/wire — do not solo `OverrideMaxSpeed`

#### Slope & encumbrance
- Slope-adaptive pacing (Tobler-style feedback)
- Nonlinear grade cost (gentle grades mild; steep grades hurt)
- Physiological drain caps to avoid load×grade explosions
- Load mainly raises “fuel use”; speed penalty coeffs are preset-tunable
- Prone rest reduces load’s recovery penalty (ground supports gear)

#### Movement, jump, sprint
- Idle / Walk / Run / Sprint + separate swim model
- Jump/vault costs with load scaling, consecutive-jump penalty, cooldown, disabled below ~10% STA
- Sprint gate: aerobic + W′ thresholds; remaining W′ can re-burst after short releases
- Tactical sprint burst load-relief + cooldown fields kept for compatibility

#### Recovery, fatigue, metabolism
- Net metabolic recovery (not naïve “always add”)
  - Post-exercise breathing deficit window
  - Load recovery penalty ∝ (load ratio)^exponent
  - Marginal decay at high STA
  - Min STA + rest-time gates at deep fatigue
- Stance multipliers (stand / crouch / prone)
- Integral fatigue I(t) and EPOC ∝ peak intended power
- Metabolic adaptation / fitness-style efficiency hooks (parameterized)

#### Environment
- Heat stress (daytime peak, indoor partial relief) and cold stress
- Rain wet weight (light/moderate/heavy + decay after rain)
- Wind resistance and mud penalties (Custom toggles)
- Indoor raycast detection; stepped air-temperature physics
- Combined swim + rain wet weight with saturation cap

#### Swimming
- 3D drag ∝ v², vertical work, treading baseline (~25 W)
- Heavy-load treading penalty above ~25 kg
- Post-exit wet weight decay
- Position-delta speed (`GetOrigin()`) when `GetVelocity()` is zero under swim commands

#### Presentation, items, AI
- Optional top-right HUD (off by default), settings/admin UI
- 1st-person camera inertia / head bob; sprint FOV vs fatigue (may respect native-only presentation flags)
- CSB combat stim, morphine, related damage effects
- Optional mud-slip ragdoll (off by default)
- **Experimental AI**: `SCR_RSS_AIManager` → state machine → speed cap → intent filter → combat decay → injury link

#### Networking, config, performance
- Server-authoritative settings; client report + server validation
- Proxies can skip local RSS ticks; AI skips player Hint widgets
- AI load shedding toggles
- API: `SCR_RSS_API` (`wPrimePool01`); optional `RSS_PlayerData.json` export

---

## Core loop (v6)

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [Drain + Speed calculators]
      → SetSpeedLimit [SpeedBridge]
```

Authoritative math (Chinese): [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)

---

## Presets

| Preset | Intent |
|------|------|
| **EliteStandard** | Hardest / most realistic |
| **StandardMilsim** | Default balanced |
| **TacticalAction** | More forgiving |
| **Custom** | Manual; system presets can be force-refreshed |

Baked in `SCR_RSS_SettingsPresetBake` (v6 optimizer). Offline JSON under `tools/optimized_rss_config_*_v6.json`.

Approximate Bake magnitudes: Elite CP ~942 W / W′ ~20.4 kJ / sprint cap ~2830 W; Standard ~1012 / 20.9k / 2880; Tactical ~1075 / 20.4k / 2525.

---

## Repository layout

~**98** EnforceScript `.c` files. Domain types: `SCR_RSS_*`.

```
scripts/Game/
  Integration/     # modded shells (PlayerBase, StaminaOverride, ServerBootstrap, …)
  RSS/
    Core/          # metabolism, CP–W′, drain/recovery, speed, coordinator (~30)
    Environment/   # heat/cold/rain/wind/mud, slope, swim, jump (~18)
    AI/            # AIManager + state/cap/intent/decay/injury (~8)
    NetworkConfig/ # Settings, Params, Bake, ConfigManager, API, Sync
    Presentation/  # HUD, camera, FX, settings UI
    MudSlip/
  Components/Gadgets/  UserActions/  Damage/…
tools/   docs/   Prefabs/   UI/   Configs/   Assets/
```

Layering: formulas in `RSS/*`; Override only intercepts the engine bar; speed only via `SpeedBridge`; hard file size **65535** bytes — see `docs/RSS_CODING_STANDARDS.md`.

---

## Technical notes

- **Pandolf** for walk/low speed (load×grade); **ACSM** for run/sprint; **C¹ blend** ~2.0–2.4 m/s
- **CP + W′** for sustainable vs burst power
- Palumbo-style personalization ideas inform fatigue/adaptation parameters
- `CalculateSpeedMultiplierByStamina` now forwards to **`CalculateV6PhaseSpeedMultiplier`** (no willpower plateau)
- Update cadence ~**0.2 s** for stamina settlement
- Swim uses a separate 3D model (no land slope/terrain path)

### External API

```c
RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    PrintFormat("STA=%1 W'=%2 allowed=%3",
        info.staminaPercent, info.wPrimePool01, info.sprintAllowed);
}
```

See [docs/RSS_API.md](docs/RSS_API.md). Prefer **`wPrimePool01`** over deprecated `anaerobicPercent`.

### Tuning
- Gameplay: settings UI / profile JSON / flat param arrays
- Retune presets: JSON → `embed_json_to_c.py` → `SettingsPresetBake`
- Hard constants: `SCR_RSS_Constants.c`, `ConfigBridge`

---

## Install & use

1. Place under Workbench `addons`, open `addon.gproj`, compile  
2. Enable on client/server (base-game dependency in `addon.gproj`)  
3. Pick preset (default **StandardMilsim**); optionally enable HUD, debug, mud slip, AI combat effects, export  

---

## Tools

```bash
cd tools
pip install -r requirements.txt
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python check_script_size.py
```

See [tools/README.md](tools/README.md) for Optuna optimize, Rust `rss_sim`, dual-run, etc.

---

## Docs

| Doc | Topic |
|------|------|
| [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md) | Authoritative v6 math (CN) |
| [docs/RSS_API.md](docs/RSS_API.md) | External API |
| [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) | Naming / size limits |
| [docs/灌木丛移动减速机制.md](docs/灌木丛移动减速机制.md) | 1.7+ limit merge |
| [docs/RSS_已知问题_限速与滑步.md](docs/RSS_已知问题_限速与滑步.md) | Known issues |
| [CHANGELOG.md](CHANGELOG.md) | Full history |
| [README_CN.md](README_CN.md) | Full Chinese narrative |

---

## Recent versions (see CHANGELOG)

- **6.0.0** — CP–W′ rewrite; remove willpower plateau; unified metabolism; v6 tool pipeline  
- **5.0.0** — Dual-pool rewrite; `SCR_RSS_*` naming; archive v3.23.1  
- **3.23.1** — Reforger 1.7 foliage merge; stable presets  

Older v2.x–v3.22 notes live only in CHANGELOG (not duplicated here).

---

## EnforceScript

- No ternary `?:`; braced single-line `if`
- Prefer `ref` over `autoptr`
- Run size/syntax checkers before commit

---

## References

1. Pandolf, Givoni & Goldman (1977), *J Appl Physiol* — load carriage energy expenditure  
2. Palumbo et al. (2018), *PLOS Comput Biol* — personalized exercise / fuel homeostasis  
3. Critical Power / W′ & Skiba refill literature (see v6 math doc)

---

## Contributing & license

Issues and PRs welcome under [AGPL-3.0](LICENSE). See [CONTRIBUTING.md](CONTRIBUTING.md).

**Note:** Deep integration with stamina/speed may conflict with other locomotion mods — test first.
