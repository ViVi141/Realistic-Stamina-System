# Realistic Stamina System (RSS) v3.11.1

[中文 README](README_CN.md) | [English README (current)](README_EN.md) | [Mixed README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-3.11.1-brightgreen)](CHANGELOG.md)

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
- **Movement types**: Idle / Walk / Run / Sprint.
- **Environmental stress**: heat stress + rain wet weight.
- **Swimming stamina management**: 3D physical model (drag + vertical work + treading).
- **EPOC delay**: post-exercise oxygen consumption delay logic.
- **Debug/status output**: 1s status + 5s detailed debug info (client-side).
- **Modular architecture**: stamina update coordination, swimming state, debug display, etc.

For the full, detailed (and frequently updated) explanation, see:
- **Changelog**: `CHANGELOG.md`
- **Chinese full README**: `README_CN.md`

## v3.11.1 Updates

**2026-02-02**

### 🔧 Config Fixes & Improvements
- ✅ **JSON overwrite fix** - User-modified hint/debug etc. no longer overwritten
- ✅ **ValidateSettings fix** - Only clamp out-of-range values, don't reset entire config
- ✅ **Custom preset case-insensitive** - "custom" / "CUSTOM" recognized
- ✅ **HUD default OFF** - New configs don't show HUD by default
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
