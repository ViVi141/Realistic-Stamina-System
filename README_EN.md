# Realistic Stamina System (RSS) v3.11.0

[中文 README](README_CN.md) | [English README (current)](README_EN.md) | [Mixed README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-3.11.0-brightgreen)](CHANGELOG.md)

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

## v3.11.0 Updates

- ✅ **Project cleanup** - Removed all generated PNG charts
- ✅ **Tools directory optimization** - Kept only core NSGA-II pipeline
- ✅ **Documentation enhancement** - Added comprehensive verification reports
- ✅ **Version consolidation** - All changes from git commit d1ebb9c to present as v3.11.0

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

The `tools/` directory provides reproducible plots/analysis:

- `tools/simulate_stamina_system.py` → `stamina_system_trends.png`
- `tools/generate_comprehensive_trends.py` → 7 independent charts (comprehensive_*.png)
- `tools/multi_dimensional_analysis.py` → 8 independent charts (multi_*.png)

Notes:
- Scripts are **headless-friendly** (no GUI popups by default).
- Dependencies: Python + `numpy` + `matplotlib`.
- Charts are split into independent files for better readability.

## Contributing

See `CONTRIBUTING.md`.

## License

This project is licensed under **GNU AGPL-3.0**. See `LICENSE`.
