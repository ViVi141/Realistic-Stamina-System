# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README (current)](README_EN.md) | [Mixed README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** is a realistic stamina & speed mod for Arma Reforger.  
It dynamically adjusts movement speed based on stamina, encumbrance, slope, environment, and more—using medical/physiological models (e.g. Pandolf energy expenditure).


- **GUID**: `68649101601CC93D`
- **Config version**: **6.0.0**
- **Recommended game**: Arma Reforger **1.7+**

> The **Chinese README** (`README_CN.md`) is the full narrative (features, formulas, version history). This English file keeps release notes and an expanded feature/tech summary aligned to the current tree.

## Authors

- **Author**: ViVi141
- **Email**: 747384120@qq.com
- **License**: [AGPL-3.0](LICENSE)

## Highlights & feature detail

### v6 core loop
```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [Drain + Speed calculators]
      → SetSpeedLimit [SpeedBridge]  // min with foliage/wire
```
- **CP–W′ power budget**: Pandolf/ACSM → dynamic CP → W′ joules; Sprint ≈ `invert(min(sprint_cap, CP + W′/Δt))`
- **Phase march speeds**: Walk/Run/Sprint m/s targets; **no willpower plateau**
- **Limp + 5 s collapse damping** at ~5% STA (`SCR_RSS_CollapseTransition`)
- **v_drain closed loop** with `m_fAppliedSpeedLimitMs`
- Elite **Skiba** W′ refill; Standard/Tactical linear W/s

### Physiology & locomotion (also documented historically in README_CN)
- Slope-adaptive Tobler-style pacing; nonlinear grade cost; physiological drain caps
- Encumbrance mainly raises metabolic cost / CP pressure; configurable speed penalty
- Jump/vault load-scaled costs, consecutive-jump penalty, cooldown, disabled at very low STA
- Recovery stack: post-exercise breathing window, load recovery penalty, marginal decay at high STA, stance multipliers (prone best for heavy loads)
- Integral fatigue I(t) + EPOC ∝ peak intended power
- Metabolic adaptation / fitness-style efficiency parameters

### Environment & swim
- Heat/cold stress, rain wet weight (light/moderate/heavy + decay), wind, mud
- Indoor raycast detection; stepped air-temperature physics
- Swim 3D drag (`F_d ∝ v²`), vertical work, treading (~25 W), post-exit wet weight; position-delta speed when `GetVelocity()` is 0

### Presentation, items, AI, net
- Optional HUD (`m_bHintDisplayEnabled`, default OFF), settings UI, 1st-person camera inertia
- CSB combat stim / morphine gadgets
- Optional mud-slip (default OFF)
- Experimental AI via `SCR_RSS_AIManager` (state → speed cap → intent → combat decay → injury)
- Server-authoritative config; `SCR_RSS_API` exposes `wPrimePool01`

### Presets
EliteStandard · **StandardMilsim** (default) · TacticalAction · Custom — baked in `SCR_RSS_SettingsPresetBake`.

Authoritative math (Chinese): `docs/RSS_v6_计算逻辑权威版.md` · Full Chinese README: `README_CN.md` · Changelog: `CHANGELOG.md`

## v6.0.0 Updates

**2026-06-04** — See `CHANGELOG.md` **[6.0.0]**  
CP–W′ rewrite; remove willpower plateau; unified metabolism; SpeedBridge foliage merge path; v6 tool pipeline; config **6.0.0**.

## v5.0.0 Updates

**2026-06-04** — Dual-pool rewrite, `SCR_RSS_*` naming, archive of v3.23.1 scripts. See `CHANGELOG.md`.

## v3.14.1 Updates

**2026-03-05**

### ✅ Added

- **Tactical sprint burst and cooldown** - First 8 s of sprint: encumbrance speed penalty ×0.2; 8–13 s buffer: linear transition to full penalty; 15 s cooldown after burst end or release, during which re-sprint does not trigger burst.
- **Indoor stairs encumbrance speed reduction** - When indoor and raw slope &gt; 0, encumbrance speed penalty ×0.4; new `GetRawSlopeAngle` for stairs detection.
- **Slope speed transition snap-up** - If target speed exceeds current smoothed value by ≥0.08, skip 5 s transition and snap to target (avoids frequent micro-acceleration on gentle slopes while climbing).
- **Camera inertia and head bob (first-person)** - Start lag/tilt, decel overshoot (nonlinear with load), vertical step bob and uphill sway; sprint FOV tied to tactical burst (Burst +5° / Cruise +3° / Limp −2°). See [CharacterCamera1stPerson.c](scripts/Game/RSS/Presentation/CharacterCamera1stPerson.c), [SCR_RSS_Constants.c](scripts/Game/RSS/Core/SCR_RSS_Constants.c).

### 🔁 Changed

- **Slope and drain/speed** - Gentle downhill drain reduction cap 60% (was 50%); Tobler slope speed: uphill ×1.15, downhill ×1.15 with max 1.25, then 30% pull toward 1.0.

### 📚 Docs

- Tactical sprint, indoor stairs, slope transition, and camera/head bob notes now live in source and `docs/RSS_v6_计算逻辑权威版.md` (legacy stamina-logic archives removed).

## v3.12.0 Updates

**2026-02-09**

This release documents changes at the C script layer.

### ✅ Added
- **Environment temperature physics model** - temperature stepping, shortwave/longwave with cloud and cloud fraction corrections, sunrise/sunset & moon-phase inference; supports engine-temperature or module model selection (scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c)
- **Improved indoor detection** - added upward raycast and horizontal enclosure checks to reduce false positives from open roofs/skylights (scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c)
- **Configuration change sync pipeline** - listener registration, change detection, full parameter/settings array sync and broadcast (scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c, scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c, scripts/Game/Integration/PlayerBase.c)
- **Network validation & smoothing** - client reports stamina/encumbrance; server authoritative validation and issuance of speed multiplier, including reconnect-delay synchronization (scripts/Game/Integration/PlayerBase.c, scripts/Game/RSS/NetworkConfig/SCR_RSS_NetworkSyncManager.c)
- **Log throttling utility** - unified Debug/Verbose log throttling interface (scripts/Game/RSS/Core/SCR_RSS_Constants.c)

### 🔁 Changed
- **Server-authoritative config** - client no longer writes JSON; defaults kept in-memory awaiting sync; server writes to disk and adds backup/repair flow (scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c)
- **Movement-phase-driven drain** - sprint state no longer affects choice of expenditure model; Pandolf formula used universally, server‑authoritative speed calculation remains (SCR_RSS_UpdateCoordinator.c)
- **Encumbrance parameter constraints** - added penalty exponent/upper bound and clamp presets (scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c, scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c, scripts/Game/RSS/Core/SCR_RSS_Constants.c)
- **Preset refresh** - Elite/Standard/Tactical presets updated and top-level defaults for the weather model added (scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c)
- **Sprint drain default** - (legacy) sprint multiplier has no effect under the new Pandolf‑only model; setting retained for config compatibility.
- **Body weight included in drain input** - stamina drain input now uses total weight (equipment + body); improved debug output (scripts/Game/Integration/PlayerBase.c)

### 🐞 Fixed
- **Indoor detection false positives** - reduced via roof raycast and horizontal enclosure checks (scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c)
- **Engine temperature extrema degradation** - fallback to physical/simulated estimates when daily min/max converge (scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c)
- **Client disk overwrite** - client no longer writes local JSON to avoid overwriting server configs (scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c)

## v3.11.1 Updates

**2026-02-02**

### 🔧 Config Fixes & Improvements
- ✅ **JSON overwrite fix** - User-modified hint/debug etc. no longer overwritten
- ✅ **ValidateSettings fix** - Only clamp out-of-range values, don't reset entire config
- ✅ **Custom preset case-insensitive** - "custom" / "CUSTOM" recognized
- ✅ **HUD default OFF** - First-time users: HUD off by default. Existing users with JSON: HUD keeps your previous setting
- ✅ **Config readability** - Constants extracted, descriptions streamlined, grouping improved

## v3.11.0 Updates

### 🌟 Core Functionality Updates
- ✅ **Stamina system optimization** - Improved stamina system response speed and start experience
- ✅ **Stamina system fix** - Fixed high-frequency monitoring and speed calculation issues in the stamina system
- ✅ **Encumbrance system enhancement** - Implemented real-time encumbrance cache updates on inventory changes
- ✅ **Encumbrance calculation fix** - Fixed issue where weapon weight was not included in total encumbrance
- ✅ **Environment awareness enhancement** - Added indoor environment slope ignore functionality
- ✅ **Configuration management optimization** - Fixed preset configuration logic to ensure system presets stay up-to-date
- ✅ **Parameter optimization** - Optimized RSS system parameters and adjusted configuration file paths

### 📁 Project Cleanup and Optimization
- ✅ **Project file cleanup** - Removed all generated PNG charts
- ✅ **Tools directory optimization** - Kept only core NSGA-II optimization pipeline, removed outdated scripts
- ✅ **Configuration file update** - Removed old optimization configuration files and updated related paths

### 📚 Documentation Improvement
- ✅ **Toolset documentation** - Added tools/README.md - complete toolset documentation
- ✅ **Configuration verification report** - Added CONFIG_APPLICATION_VERIFICATION.md - configuration application verification report
- ✅ **Switch verification report** - Added DEBUG_AND_HINT_SWITCH_VERIFICATION.md - switch verification report

### 🎯 Version Consolidation
- ✅ **Version unification** - All changes from git commit d1ebb9c to present as v3.11.0

## Project Structure (High-level)

~98 EnforceScript `.c` files. Full tree: `README_CN.md`.

- `scripts/Game/Integration/`: `PlayerBase.c`, `PlayerBase_UpdateLoop.c`, `SCR_StaminaOverride.c`, `SCR_RSS_ServerBootstrap.c`, …
- `scripts/Game/RSS/Core/`: MetabolismModel, CriticalPowerModel, UpdateCoordinator, DrainCalculator, SpeedCalculator, SpeedBridge, Recovery, Fatigue, CollapseTransition, Constants, …
- `scripts/Game/RSS/Environment/`: EnvironmentFactor, weather/rain/temp, terrain, slope transition, swim, jump/vault
- `scripts/Game/RSS/AI/`: `SCR_RSS_AIManager` + StaminaState / SpeedCap / IntentFilter / CombatDecay / InjuryLink / UpdateInterval
- `scripts/Game/RSS/NetworkConfig/`: Settings, Params, SettingsPresetBake, ConfigManager, NetworkSyncManager, API, DataExport
- `scripts/Game/RSS/MudSlip/`, `Presentation/`, `Components/Gadgets/`, `UserActions/`, `Damage/…`
- `tools/`: `rss_pipeline_v6.py`, `rss_sim/` (PyO3), `rust_pipeline_v6/`, smoke/anchor tests, `embed_json_to_c.py`

## Installation

1. Copy the entire addon folder into Arma Reforger Workbench `addons/`
2. Open the project in Workbench
3. Build/compile
4. Enable the mod in-game

## Tools (Python + Rust)

The `tools/` directory contains the v4/v6 optimization stack and a Rust Phase-A entrypoint:

- `rss_pipeline_v4.py` — Optuna NSGA-II pipeline (multi-objective, 8 missions)
- `rss_pipeline_v6.py` — v6 validate/calibrate/optimize pipeline
- `rust_pipeline_v6/` — Rust CLI entrypoint (`validate`, `calibrate`, `optimize`, `dual-run`)
- `rss_digital_twin_fix.py` — digital twin used by the pipeline
- `embed_json_to_c.py` — optional embed of JSON presets into `SCR_RSS_SettingsPresetBake.c`
- `test_v4_smoke.py` / `test_v6_smoke.py` / `quick_verify.py` — smoke and short-run checks
- `optimized_rss_config_*_v4.json` / `optimized_rss_config_*_v6.json` — shipped and generated presets
- `requirements.txt` — `numpy`, `optuna`
- `README.md` — tool usage (Chinese)

See `tools/README.md` for commands.

## External API

```c
RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    PrintFormat("STA=%1 W'=%2", info.staminaPercent, info.wPrimePool01);
}
```

See `docs/RSS_API.md`. Prefer `wPrimePool01` over deprecated `anaerobicPercent`.

## Known limitations

- HUD off by default (`m_bHintDisplayEnabled`)
- Must use `SCR_RSS_SpeedBridge` / `SetSpeedLimit` so foliage slowdown is not overwritten
- Some older design notes under `docs/` may lag `RSS/AI/` — prefer source + `README_CN.md`
- See `docs/RSS_已知问题_限速与滑步.md`

## Contributing

See `CONTRIBUTING.md`.

## License

This project is licensed under **GNU AGPL-3.0**. See `LICENSE`.
