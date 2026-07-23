# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** is a realistic stamina and locomotion mod for **Arma Reforger**.
It uses a **Pandolf / ACSM / Critical Power–W′** model stack to drive speed limits, drain, recovery, and sprint eligibility.

- **Author**: ViVi141 (`747384120@qq.com`)
- **GUID**: `68649101601CC93D`
- **Config version**: **6.0.0**
- **Recommended game**: Arma Reforger **1.7+**

> The Chinese README is the authoritative long-form document for current behavior and historical notes: [README_CN.md](README_CN.md).

## Current v6 model

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]
```

### Important current behavior
- **No willpower plateau** on the player main path
- **Limp threshold** at about `5%` STA, with a **5 s collapse damping** transition
- **Sprint is not fixed `Run × 1.30`**; it depends on gait target, available power, CP/W′, and the minimum sprint-over-run separation
- **Drain accounting uses measured speed `v_meas`**, not `min(v_meas, v_limit)`
- **SpeedBridge** writes a dedicated `SetSpeedLimit` source so foliage / wire slowdown can still merge by `min`

## Runtime defaults vs stored preset gait fields

Current code ships with `V6_USE_MARCH_GAIT_SPEEDS = false`, so the live gait tops fall back to engine-aligned values:

- Walk ≈ `1.45 m/s`
- Run ≈ `3.8 m/s`
- Sprint ≈ `5.5 m/s`

Preset fields still exist in `SCR_RSS_Params` / `SCR_RSS_SettingsPresetBake`:

- `v5_walk_speed_ms = 1.4`
- `v5_run_speed_ms = 2.8`
- `v5_sprint_speed_ms = 4.5`

Those fields become active if march-gait mode is re-enabled later.

## Feature summary

### Physiology and locomotion
- Pandolf at low speed / walking; ACSM at higher run/sprint speeds; C¹ blend between them
- Dynamic **Critical Power** and **W′ joule pool**
- Encumbrance affects both **metabolic cost** and **power budget**, not only top speed
- Tobler-style slope pacing plus nonlinear grade cost
- Physical jump / vault cost model, consecutive-jump penalty, cooldown, low-STA jump lockout
- Fatigue integral, EPOC delay, stance-based recovery, load recovery penalty

### Environment and swimming
- Heat / cold stress, rain wet weight, wind, mud, indoor detection, surface wetness
- Rain wet weight accumulates continuously; swim wet weight accumulates separately; total wet weight is clamped
- Swimming uses its own model with vertical work, drag-dominated dynamic cost, treading baseline power, and post-exit wet weight decay

### AI, UI, and networking
- Experimental AI pipeline: `SCR_RSS_AIManager` → state → speed cap → intent filter → combat decay
- Optional HUD / settings UI / camera presentation layer
- Server-authoritative config and sync
- External API via `SCR_RSS_API`, including `wPrimePool01`

## Repository layout

- `scripts/Game/Integration/` — modded entry points (`PlayerBase.c`, `PlayerBase_UpdateLoop.c`, `SCR_StaminaOverride.c`, ...)
- `scripts/Game/RSS/Core/` — metabolism, CP–W′, drain, recovery, speed, coordinator, constants
- `scripts/Game/RSS/Environment/` — temperature, rain, indoor, terrain, slope, swim, jump/vault
- `scripts/Game/RSS/AI/` — current AI implementation (8 files)
- `scripts/Game/RSS/NetworkConfig/` — settings, params, bake, config manager, sync, API, data export
- `scripts/Game/RSS/Presentation/` — HUD, camera, settings UI, screen effects
- `tools/` — Python v6 pipeline, PyO3 `rss_sim`, Rust CLI, smoke/anchor checks, embed helper

## API example

```c
RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    PrintFormat("STA=%1 W'=%2 sprintAllowed=%3",
        info.staminaPercent,
        info.wPrimePool01,
        info.sprintAllowed);
}
```

Prefer **`wPrimePool01`** over deprecated `anaerobicPercent`.

## Tools

```bash
cd tools
pip install -r requirements.txt
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python check_script_size.py
```

## Docs

- [README_CN.md](README_CN.md) — full current Chinese README
- [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md) — authoritative v6 math
- [docs/RSS_API.md](docs/RSS_API.md) — API
- [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) — code/documentation constraints
- [CHANGELOG.md](CHANGELOG.md) — version-by-version history

## Notes

- Some older docs preserve v2/v3/v4 behavior for historical reference; for current behavior, prefer source code and the current Chinese README.
- Presentation can be affected by native-only presentation switches.
- This mod may conflict with other mods that directly override stamina or movement speed.

## License

This project is licensed under **GNU AGPL-3.0**. See [LICENSE](LICENSE).
