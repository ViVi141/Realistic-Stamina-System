# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README (current)](README_EN.md) | [Short hub](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** is a stamina and movement-speed mod for **Arma Reforger**.  
It drives drain, recovery, and speed limits with exercise-physiology models (Pandolf / ACSM / Critical Power–W′), not flat constant deductions.

- **Author**: ViVi141 (747384120@qq.com)
- **License**: [AGPL-3.0](LICENSE)
- **Mod ID / GUID**: `Realistic Stamina System` / `68649101601CC93D`
- **Recommended game version**: Arma Reforger **1.7+** (foliage/wire slowdown merges with RSS limits)
- **Config version**: `SCR_RSS_ConfigManager` → `6.0.0`

> Release history lives in [CHANGELOG.md](CHANGELOG.md). This README documents the **current tree only**.

---

## Core loop (v6)

Each tick (~17 ms; stamina settlement coordinated at 0.2 s):

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]  (min with foliage/wire; does not overwrite native slowdown)
```

| Concept | Role |
|------|------|
| **Aerobic bar (STA)** | Still drives the engine stamina UI; very low STA (~&lt;5%) → limp + collapse damping |
| **W′ power reserve** | Power above dynamic CP discharges in joules; exposed as normalized `wPrimePool01` |
| **Sprint** | `v_sprint = invert(min(sprint_cap, CP + W′/Δt))`; gated by aerobic + W′ thresholds |
| **v_drain** | Drain uses `min(v_meas, appliedLimit)` so accounting matches the applied cap |
| **Removed** | 25%/35% “willpower plateau”, fixed anaerobic bar drain, etc. |

Authoritative math (Chinese): [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)

---

## Features

### Physiology & locomotion
- Pandolf (walk) + ACSM (run/sprint) metabolism with C¹ blend in between
- Dynamic CP (load / grade / environment / integral fatigue) and W′ refill (Elite: Skiba biexponential; Standard/Tactical: linear W/s)
- March gait targets Walk / Run / Sprint (m/s); load mainly raises metabolic cost
- Slope: Tobler-style pace feedback, nonlinear grade cost, smoothed slope-speed transition
- Jump / vault extra cost and rapid-repeat penalty
- Swimming 3D drag model (horizontal drag, vertical work, treading, post-exit wet weight)
- Integral fatigue I(t) + EPOC (post-exercise drain ∝ peak power)

### Environment
- Heat / cold stress, rain wet weight, wind resistance, mud penalty
- Indoor detection (raycast), terrain material table, stepped air-temperature physics (optional engine temp boundary)
- Under Custom: toggles for heat / rain / wind / mud / fatigue / metabolic adaptation / indoor, etc.

### Presentation & items
- Top-right stamina HUD (off by default), admin / settings UI
- First-person camera inertia and head physics
- CSB combat stim, morphine, and related damage effects

### AI (experimental)
- `SCR_RSS_AIManager` orchestration (server-side, 500 ms behavior throttle)
- Stamina state machine → `AISpeedCap` (`SetSpeedLimit`, merges with foliage) → intent filter → combat decay → injury link
- Toggle: `m_bEnableAIStaminaCombatEffects` (Workbench often ON; dedicated-server JSON often OFF)
- Load shedding: `m_bDisableAIAllCalc` / `m_bDisableAIStaminaCalc`

### Networking & config
- Server-authoritative settings; client report + server validation (`SCR_RSS_NetworkSyncManager`)
- Three system presets + Custom; params serialized as flat float arrays (Workbench ICE relief)
- External API: `SCR_RSS_API` (includes `wPrimePool01`); optional `RSS_PlayerData.json` export

---

## Presets

`m_sSelectedPreset`:

| Preset | Intent |
|------|------|
| **EliteStandard** | Hardest / most realistic (low combat/recovery ease) |
| **StandardMilsim** | Default balanced profile |
| **TacticalAction** | More forgiving, action-friendly pacing |
| **Custom** | Manual tuning; system presets can be force-refreshed to code defaults |

Values are baked in `SCR_RSS_SettingsPresetBake` (v6 optimizer output).  
Offline JSON: `tools/optimized_rss_config_*_v6.json` (re-embed with `embed_json_to_c.py`).

Approximate magnitudes (Bake is source of truth; numbers change with retunes):

| | Elite | Standard | Tactical |
|--|------:|---------:|---------:|
| CP (W) | ~942 | ~1012 | ~1075 |
| W′_max (J) | ~20.4k | ~20.9k | ~20.4k |
| Sprint cap (W) | ~2830 | ~2880 | ~2525 |

Physiological anchor: ACFT 2-mile (ages 22–26, male, 100 pts ≈ 15:27).

---

## Repository layout (matches the tree)

About **98** EnforceScript `.c` files. Domain classes use `SCR_RSS_*` (except modded entry points).

```
Realistic-Stamina-System/
├── addon.gproj
├── Assets/
├── Configs/EntityCatalog/
├── Prefabs/
├── UI/layouts/
├── scripts/Game/
│   ├── Integration/            # modded shells (high conflict surface)
│   │   ├── PlayerBase.c
│   │   ├── PlayerBase_UpdateLoop.c
│   │   ├── SCR_StaminaOverride.c
│   │   ├── SCR_RSS_ServerBootstrap.c
│   │   └── …
│   ├── RSS/
│   │   ├── Core/               # metabolism / CP–W′ / drain·recovery / speed / coordinator (~30)
│   │   ├── Environment/        # heat·cold·rain·wind·mud / slope / swim / jump (~18)
│   │   ├── AI/                 # AIManager + state / cap / intent / decay (~8)
│   │   ├── NetworkConfig/      # Settings / Params / ConfigManager / API / Sync (~8)
│   │   ├── Presentation/       # HUD, camera, FX, settings UI (~12)
│   │   └── MudSlip/            # mud slip (off by default)
│   ├── Components/Gadgets/
│   ├── UserActions/
│   └── Damage/…/
├── tools/                      # digital twin + v6 pipeline (Python / Rust)
├── docs/
└── githooks/pre-commit
```

**Layering (summary)**:

- Formulas live under `RSS/Core/` (and domain modules such as Environment)
- `SCR_StaminaOverride` only intercepts the engine stamina bar
- Speed goes through `SCR_RSS_SpeedBridge` → `SetSpeedLimit` only; do not `OverrideMaxSpeed` alone (that kills foliage slowdown)
- Hard file size cap **65535 bytes**; see `docs/RSS_CODING_STANDARDS.md`

| Role | File |
|------|------|
| Metabolic power | `SCR_RSS_MetabolismModel.c` |
| CP–W′ | `SCR_RSS_CriticalPowerModel.c` / `SCR_RSS_AnaerobicBurst.c` |
| Drain coordination | `SCR_RSS_UpdateCoordinator.c` / `SCR_RSS_DrainCalculator.c` |
| Speed | `SCR_RSS_SpeedCalculator.c` / `SCR_RSS_SpeedBridge.c` |
| Recovery / EPOC | `SCR_RSS_RecoveryCalculator.c` / `SCR_RSS_EpocState.c` |
| Main loop | `PlayerBase_UpdateLoop.c` |
| Config | `SCR_RSS_Settings.c` / `SCR_RSS_Params.c` / `SCR_RSS_SettingsPresetBake.c` |

---

## Install & usage

### Workbench
1. Place this repo under Workbench `addons` (or equivalent)
2. Open `addon.gproj`
3. Depend on base game data (`addon.gproj` Dependencies)

### Dedicated server
1. Enable the mod; config is **server-authored** and replicated
2. Pick a preset in the settings UI or JSON; Custom unlocks multipliers and environment toggles
3. HUD, mud slip, AI combat effects, and data export are optional (mostly off by default)

### External mod API

```c
IEntity player = SCR_PlayerController.GetLocalControlledEntity();
if (!player)
    return;

RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    PrintFormat("STA=%1 W'=%2 sprintAllowed=%3",
        info.staminaPercent,
        info.wPrimePool01,
        info.sprintAllowed);
}

RSS_EnvironmentInfo env = SCR_RSS_API.GetEnvironmentInfo(player);
```

See [docs/RSS_API.md](docs/RSS_API.md).  
`anaerobicPercent` is deprecated; read `wPrimePool01`.

---

## Tooling (`tools/`)

Digital twin aligned with C, physiological anchors, preset optimization.

| Entry | Purpose |
|------|------|
| `rss_pipeline_v6.py` | `validate` / `optimize` / `anchors` |
| `rss_sim/` | PyO3 sim core (falls back to pure Python) |
| `rust_pipeline_v6/` | Rust CLI (optional dual-run) |
| `test_v6_smoke.py` / `bench_physio_anchors.py` / `test_acft_2mile.py` | Smoke & anchors |
| `check_script_size.py` / `check_enforce_syntax.py` | Pre-commit gates |
| `embed_json_to_c.py` | JSON → `SettingsPresetBake` |

```bash
cd tools
pip install -r requirements.txt
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python check_script_size.py
```

Details: [tools/README.md](tools/README.md).

---

## Docs index

| Doc | Topic |
|------|------|
| [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md) | Authoritative v6 math (CN) |
| [docs/RSS_API.md](docs/RSS_API.md) | External API |
| [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) | Naming / layering / size limits |
| [docs/灌木丛移动减速机制.md](docs/灌木丛移动减速机制.md) | 1.7+ speed-limit merge |
| [docs/RSS_已知问题_限速与滑步.md](docs/RSS_已知问题_限速与滑步.md) | Known issues |
| [docs/泥泞滑倒判定模型.md](docs/泥泞滑倒判定模型.md) | Mud slip |
| [CHANGELOG.md](CHANGELOG.md) | Full history |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contributing |

Some older v3/v5 design notes under `docs/` may lag behind `RSS/AI/`; prefer this README and the source.

---

## EnforceScript notes

- **No** ternary `?:`
- Single-line `if` must use `{}`
- Prefer `ref` over deprecated `autoptr`
- Run `check_script_size.py` and `check_enforce_syntax.py` before commit

---

## License

[GNU Affero General Public License v3.0](LICENSE).  
By contributing, you agree your changes are licensed under AGPL-3.0.
