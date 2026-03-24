# Realistic Stamina System (RSS) v3.18.0

[中文 README](README_CN.md) | [English README (current)](README_EN.md) | [Mixed README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-3.18.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** is a realistic stamina & speed mod for Arma Reforger.  
It dynamically adjusts movement speed based on stamina, encumbrance, slope, environment, and more—using medical/physiological models (e.g. Pandolf energy expenditure).

## Authors

- **Author**: ViVi141
- **Email**: 747384120@qq.com
- **License**: [AGPL-3.0](LICENSE)

## Highlights

- **Dual-state “stress/performance” model**: performance plateau at high stamina, smooth degradation near exhaustion.
- **Wall-hit smoothing**: damping transition around the critical stamina threshold.
- **Slope-adaptive movement**: slope-aware pacing and energy cost.
- **Encumbrance system**: load affects “fuel economy” (stamina drain) more than hard speed caps.
- **Pandolf-based drain**: all walking/running/sprinting stamina costs are computed using the Pandolf energy formula instead of fixed rates.
- **Movement types**: Idle / Walk / Run / Sprint.
- **Environmental stress**: heat stress + rain wet weight.
- **Swimming stamina management**: 3D physical model (drag + vertical work + treading).
- **EPOC delay**: post-exercise oxygen consumption delay logic.
- **Debug/status output**: 1s status + 5s detailed debug info (client-side).
- **Modular architecture**: stamina update coordination, swimming state, debug display, etc.

For the full, detailed (and frequently updated) explanation, see:
- **Changelog**: `CHANGELOG.md`
- **Chinese full README**: `README_CN.md`

## v3.14.1 Updates

**2026-03-05**

### ✅ Added

- **Tactical sprint burst and cooldown** - First 8 s of sprint: encumbrance speed penalty ×0.2; 8–13 s buffer: linear transition to full penalty; 15 s cooldown after burst end or release, during which re-sprint does not trigger burst.
- **Indoor stairs encumbrance speed reduction** - When indoor and raw slope &gt; 0, encumbrance speed penalty ×0.4; new `GetRawSlopeAngle` for stairs detection.
- **Slope speed transition snap-up** - If target speed exceeds current smoothed value by ≥0.08, skip 5 s transition and snap to target (avoids frequent micro-acceleration on gentle slopes while climbing).
- **Camera inertia and head bob (first-person)** - Start lag/tilt, decel overshoot (nonlinear with load), vertical step bob and uphill sway; sprint FOV tied to tactical burst (Burst +5° / Cruise +3° / Limp −2°). See [CharacterCamera1stPerson.c](scripts/Game/Character/CharacterCamera1stPerson.c), [SCR_StaminaConstants.c](scripts/Game/Components/Stamina/SCR_StaminaConstants.c).

### 🔁 Changed

- **Slope and drain/speed** - Gentle downhill drain reduction cap 60% (was 50%); Tobler slope speed: uphill ×1.15, downhill ×1.15 with max 1.25, then 30% pull toward 1.0.

### 📚 Docs

- [体力系统计算逻辑文档](docs/体力系统计算逻辑文档.md) updated with tactical sprint, indoor stairs, slope transition, camera/head bob sections and constant tables.

## v3.12.0 Updates

**2026-02-09**

This release documents changes at the C script layer.

### ✅ Added
- **Environment temperature physics model** - temperature stepping, shortwave/longwave with cloud and cloud fraction corrections, sunrise/sunset & moon-phase inference; supports engine-temperature or module model selection (scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)
- **Improved indoor detection** - added upward raycast and horizontal enclosure checks to reduce false positives from open roofs/skylights (scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)
- **Configuration change sync pipeline** - listener registration, change detection, full parameter/settings array sync and broadcast (scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c, scripts/Game/Components/Stamina/SCR_RSS_Settings.c, scripts/Game/PlayerBase.c)
- **Network validation & smoothing** - client reports stamina/encumbrance; server authoritative validation and issuance of speed multiplier, including reconnect-delay synchronization (scripts/Game/PlayerBase.c, scripts/Game/Components/Stamina/SCR_NetworkSync.c)
- **Log throttling utility** - unified Debug/Verbose log throttling interface (scripts/Game/Components/Stamina/SCR_StaminaConstants.c)

### 🔁 Changed
- **Server-authoritative config** - client no longer writes JSON; defaults kept in-memory awaiting sync; server writes to disk and adds backup/repair flow (scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c)
- **Movement-phase-driven drain** - sprint state no longer affects choice of expenditure model; Pandolf formula used universally, server‑authoritative speed calculation remains (SCR_StaminaUpdateCoordinator.c)
- **Encumbrance parameter constraints** - added penalty exponent/upper bound and clamp presets (scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c, scripts/Game/Components/Stamina/SCR_RSS_Settings.c, scripts/Game/Components/Stamina/SCR_StaminaConstants.c)
- **Preset refresh** - Elite/Standard/Tactical presets updated and top-level defaults for the weather model added (scripts/Game/Components/Stamina/SCR_RSS_Settings.c)
- **Sprint drain default** - (legacy) sprint multiplier has no effect under the new Pandolf‑only model; setting retained for config compatibility.
- **Body weight included in drain input** - stamina drain input now uses total weight (equipment + body); improved debug output (scripts/Game/PlayerBase.c)

### 🐞 Fixed
- **Indoor detection false positives** - reduced via roof raycast and horizontal enclosure checks (scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)
- **Engine temperature extrema degradation** - fallback to physical/simulated estimates when daily min/max converge (scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)
- **Client disk overwrite** - client no longer writes local JSON to avoid overwriting server configs (scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c)

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

Key paths (see `README_CN.md` for the full tree):

- `scripts/Game/PlayerBase.c`: main controller (speed update + status display)
- `scripts/Game/Components/Stamina/`: modular stamina system
  - `SCR_RealisticStaminaSystem.c`: core math/models
  - `SCR_StaminaUpdateCoordinator.c`: orchestration for speed/drain/recovery
  - `SCR_SwimmingState.c`: swimming detection + wet weight tracking
  - `SCR_DebugDisplay.c`: debug/status formatting & output
  - plus other modules (terrain, EPOC, fatigue, encumbrance cache, UI bridge, etc.)
- `tools/`: python analysis/plot generators

## Installation

1. Copy the entire addon folder into Arma Reforger Workbench `addons/`
2. Open the project in Workbench
3. Build/compile
4. Enable the mod in-game

## Tools (Python)

The `tools/` directory contains the core optimization pipeline:

- `rss_super_pipeline.py` - Core NSGA-II optimization pipeline (10,000 trials)
- `stamina_constants.py` - Constant definition library
- `optimized_rss_config_*.json` - Optimized configuration files (3 presets)
- `requirements.txt` - Python dependencies
- `README.md` - Complete toolset documentation

Notes:
- The tools directory has been optimized to keep only the core NSGA-II pipeline
- All generated charts have been removed to reduce repository size
- Dependencies: Python + `numpy` + `matplotlib` for the optimization pipeline

## Contributing

See `CONTRIBUTING.md`.

## License

This project is licensed under **GNU AGPL-3.0**. See `LICENSE`.
