# RSS Developer Guide

> [中文](../RSS_开发者指南.md) | **English**
>
> For contributors and integrators. Aligned with **6.1.x**. Player-facing docs: root `README_EN.md` / `README_CN.md`.

## 1. Quick navigation

| Goal | Start here |
|------|------------|
| Metabolism / CP–W′ / recovery | [`V6_CALCULATION_LOGIC.md`](V6_CALCULATION_LOGIC.md) → `scripts/Game/RSS/Core/` |
| Speed limits / gait / foot-slide | [`KNOWN_ISSUES_SPEED_SLIP.md`](KNOWN_ISSUES_SPEED_SLIP.md) → `SCR_RSS_SpeedBridge` / `SCR_RSS_Constants` |
| Engine API usage | [`ENGINE_API_USAGE.md`](ENGINE_API_USAGE.md) |
| Coding / layering | [`CODING_STANDARDS.md`](CODING_STANDARDS.md) |
| Split oversized files | [`SCRIPT_FILE_SIZE_LIMIT.md`](SCRIPT_FILE_SIZE_LIMIT.md) |
| AI behavior | [`AI_BEHAVIOR.md`](AI_BEHAVIOR.md) → `scripts/Game/RSS/AI/` |
| External mod API | [`API.md`](API.md) |
| Calibration / digital twin | [`../../tools/README.md`](../../tools/README.md), Chinese pipeline design `../RSS_v6_优化管线设计.md` |

**Precedence on conflict**: source code > this guide / calculation authority > historical README prose > archived design drafts.

Do not treat as implementation truth: Chinese archive `../RSS_AI体力集成全盘设计方案.md`.

---

## 2. Environment

- **Game**: Arma Reforger **1.7+**; open this addon in **Workbench** (`addon.gproj`).
- **Vanilla reference** (optional local path): `C:\Users\74738\Documents\arma_reforger_code`.
- **Python tools** (optional): under `tools/`, `pip install -r requirements.txt`; see [`../../tools/README.md`](../../tools/README.md).

Mod ID / GUID: `Realistic Stamina System` / `68649101601CC93D`. Config version: `SCR_RSS_ConfigManager.CURRENT_VERSION`.

---

## 3. Repository map

```text
scripts/Game/
  Integration/          # modded entry: PlayerBase.c + PlayerBase_UpdateLoop.c (only these two for the same class)
  RSS/
    Core/               # metabolism, CP–W′, drain/recovery, speed, coordinator, constants
    Environment/        # weather/terrain/penalties (RSS-owned; engine is sample-only)
    NetworkConfig/      # Settings / Params / API / sync
    AI/                 # per-agent state machine, caps, intent, combat decay
    Presentation/       # HUD, screen FX, camera
    MudSlip/            # mud slip
    Items/              # injectors, etc.
  Components/ Damage/ UserActions/
Prefabs/
docs/                   # Chinese authority + docs/en bilingual set
tools/                  # twin, validate, optimize
```

### Main loop (mental model)

```text
PlayerBase_UpdateLoop
  → measure v_meas
  → SCR_RSS_UpdateCoordinator (metabolism → CP/W′ → drain/recovery → speed intent)
  → SCR_RSS_SpeedBridge.SetSpeedLimit (min-merge with foliage, etc.)
  → SCR_StaminaOverride (aerobic authority → engine bar)
  → SCR_RSS_SprintGate (optional: W′ → transient GetStamina / Exhaustion FX)
```

**Default speed policy (6.1.x)**: `V6_APPLY_CP_METABOLIC_SPEED_CAP = false` (drain-only: overspend hits STA/W′; do not push CP cruise caps onto locomotion by default). Physics hard clamps stay off — do not twist horizontal `Physics` velocity to “stick” to `v_limit` (foot-slide).

---

## 4. Hard rules when editing

Authority: [`CODING_STANDARDS.md`](CODING_STANDARDS.md). Summary:

1. **No** EnforceScript ternary `?:`; single-line `if` must use `{}`.
2. **File size is not a crash cause** (no 64 KB hard cap); `PlayerBase.c` is large — keep extracting, never pile more logic.
3. **Thin Integration**: formulas live in `RSS/Core/` etc.; no inline Pandolf in `PlayerBase`.
4. Speed limits only via **`SCR_RSS_SpeedBridge` → `SetSpeedLimit`**; do not solo `OverrideMaxSpeed` and wipe foliage slowdown.
5. **Aerobic authority** is `m_fTargetStamina`; W′ must **not** change it; presentation may use `ApplyTransientEngineStamina`.
6. CPR-style “disabled vanilla stamina component” compat is **out of this mod** — use a separate compat mod.
7. Naming: `SCR_RSS_*` / `ERSS_*` / `RSS_*` DTOs; filename matches primary class.

---

## 5. Common edit entry points

| Topic | Key files |
|-------|-----------|
| Tunables / physio constants | `SCR_RSS_Constants.c`, `SCR_RSS_AIConstants.c`, `SCR_RSS_EnvConstants.c` |
| Drain coordination | `SCR_RSS_UpdateCoordinator.c` |
| Speed intent / invert | `SCR_RSS_SpeedCalculator.c`, `SCR_RSS_DrainCalculator.c` |
| CP–W′ | `SCR_RSS_CriticalPowerModel.c`, `SCR_RSS_AnaerobicBurst.c` |
| Aerobic intercept shell | `SCR_StaminaOverride.c` (keep thin) |
| W′ → sway/blur | `SCR_RSS_SprintGate.c` |
| Host settings | `SCR_RSS_Settings.c`, Bake / `tools/optimized_rss_config_*_v6.json` |
| AI | `SCR_RSS_AIManager.c` and siblings |
| Mud | `SCR_RSS_MudSlipRunner.c`, `SCR_RSS_MudSlipEffects.c` |

Preset numbers: prefer the tool pipeline then `embed_json_to_c` (see tools README) to avoid Bake vs JSON drift.

---

## 6. Pre-commit checks

From repo root (Windows PowerShell):

```powershell
python tools/check_script_size.py
python tools/check_enforce_syntax.py
python tools/test_v6_smoke.py
python tools/rss_pipeline_v6.py validate
```

Workbench: compile addon → solo sprint/recovery → (if config changed) verify server sync.

Minimal manual checklist:

- [ ] Unloaded flat Walk / Run / Sprint
- [ ] ~30 kg mild slope endurance + stop EPOC (no spike penalty)
- [ ] Foliage slowdown still applies (RSS did not overwrite)
- [ ] Sway/blur after W′ empty (if `V6_WPRIME_ENGINE_FX_ENABLED` on)
- [ ] (If AI touched) enable `m_bEnableAIStaminaCombatEffects` and check state caps

---

## 7. Doc index (bilingual)

| Chinese | English | Role |
|---------|---------|------|
| [`RSS_CODING_STANDARDS.md`](../RSS_CODING_STANDARDS.md) | [`CODING_STANDARDS.md`](CODING_STANDARDS.md) | Coding authority |
| [`scripts_file_size_limit.md`](../scripts_file_size_limit.md) | [`SCRIPT_FILE_SIZE_LIMIT.md`](SCRIPT_FILE_SIZE_LIMIT.md) | Compile-crash isolation (shell method) |
| [`RSS_v6_计算逻辑权威版.md`](../RSS_v6_计算逻辑权威版.md) | [`V6_CALCULATION_LOGIC.md`](V6_CALCULATION_LOGIC.md) | Math authority |
| [`engine_api_usage.md`](../engine_api_usage.md) | [`ENGINE_API_USAGE.md`](ENGINE_API_USAGE.md) | Engine API catalog |
| [`RSS_已知问题_限速与滑步.md`](../RSS_已知问题_限速与滑步.md) | [`KNOWN_ISSUES_SPEED_SLIP.md`](KNOWN_ISSUES_SPEED_SLIP.md) | Speed / slip constraints |
| [`RSS_AI_行为说明.md`](../RSS_AI_行为说明.md) | [`AI_BEHAVIOR.md`](AI_BEHAVIOR.md) | Current AI behavior |
| [`RSS_API.md`](../RSS_API.md) | [`API.md`](API.md) | External mod API |
| [`RSS_开发者指南.md`](../RSS_开发者指南.md) | this file | Dev entry |
| [`../CHANGELOG.md`](../../CHANGELOG.md) | same | Changelog |

Chinese-only deep dives (no EN yet): foliage slowdown, mud model, v6 optimize pipeline design, naming layout supplement, archived AI design.

---

## 8. Do not

- Per-frame twist horizontal `Physics` velocity to match `v_limit`.
- Keep growing `PlayerBase.c` in Integration.
- Put business formulas inside `SCR_StaminaOverride`.
- Create files from archived AI design draft class names.
- Commit secrets / machine-local absolute paths in experimental configs unless the doc explicitly requires them.

---

*Maintain in sync with 6.1.x source; on large behavior changes, update calculation/coding authority first, then this index.*
