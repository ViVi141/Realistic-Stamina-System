# Realistic Stamina System (RSS) v6.0.0

[中文 README（当前）](README_CN.md) | [English README](README_EN.md) | [混合版 README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** - 一个结合体力和负重动态调整移动速度的拟真模组，基于精确的医学/生理学模型（v6：Pandolf/ACSM + Critical Power–W′）。

**English**: A realistic stamina and speed system mod for Arma Reforger that dynamically adjusts movement speed based on stamina and encumbrance, using precise medical/physiological models.

- **模组 ID / GUID**: `Realistic Stamina System` / `68649101601CC93D`
- **建议游戏版本**: Arma Reforger **1.7+**
- **配置版本**: `SCR_RSS_ConfigManager.CURRENT_VERSION` = **6.0.0**

> 本文在保留历史特性说明与版本记录的同时，已把路径/类名对齐到当前仓库，并在关键处标注 **【v6】**。逐条变更仍以 [CHANGELOG.md](CHANGELOG.md) 为准。

## 作者信息

- **作者**: ViVi141
- **邮箱**: 747384120@qq.com
- **许可证**: [AGPL-3.0](LICENSE)

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE) 许可证。

## 专用服性能（当前实现）

- **玩家主循环**：`SCR_PlayerBaseLoop.Tick()` 默认 **17 ms** 调度一次（`RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS`）。
- **AI 主循环**：服务端基准 **100 ms**；按最近玩家距离可切到 **200 / 300 / 1500 ms** LOD（`SCR_RSS_AIUpdateInterval`）。
- **AI 行为层**：`SCR_RSS_AIManager` 另有 **500 ms** 行为节流，用于状态机 / 限速 / 意图过滤 / 战斗衰减。
- **HUD / 调试**：右上角 HUD 仅面向玩家；AI 不构建玩家体力 HUD。
- **当前 AI 范围**：现行实现以 `SCR_RSS_AIManager` + `AIStaminaState` / `AISpeedCap` / `AIIntentFilter` / `AICombatDecay` / `AIInjuryLink` 为主，旧文档中的群组代理 / 休息路点 / 远距队长代理不再是当前主路径。

## 功能说明

本模组根据玩家的体力值、负重、坡度、温度、湿重、风阻、泥泞、游泳状态等因素动态调整移动速度与体力变化。  
**当前 v6 主模型**以“代谢功率 → 动态 Critical Power → W′ 焦耳池 → 反解限速”为核心；引擎体力条仍保留为 UI 载体，但实际权威体力逻辑由 RSS 接管。

**体力标准参考**：引用 **ACFT (Army Combat Fitness Test)** 中 22–26 岁男性 2 英里测试 100 分用时 **15 分 27 秒**。仓库内提供 `tools/test_acft_2mile.py`、`bench_physio_anchors.py`、`rss_pipeline_v6.py validate` 等校验工具。

### 【v6】核心闭环（当前主模型）

每 tick（玩家约 17 ms；体力/恢复公式以 0.2 s 为记账基准）大致为：

```
v_meas → P(v) [SCR_RSS_MetabolismModel]
      → CP_eff / W′ [SCR_RSS_CriticalPowerModel]
      → v_max = invert(P_target) [SCR_RSS_DrainCalculator + SCR_RSS_SpeedCalculator]
      → SetSpeedLimit [SCR_RSS_SpeedBridge]
```

| 概念 | 当前实现 |
|------|----------|
| **有氧主条 (STA)** | 仍驱动引擎体力条 UI；权威值由 RSS 控制；极低体力（约 `< 5%`）进入跛行 |
| **W′ 焦耳池** | 超过动态 CP 的功率以焦耳放电；对外字段为 `wPrimePool01` |
| **Sprint** | 由可用功率、门槛体力、W′ 池与步态最低拉开共同决定；不是固定 `Run × 1.30` |
| **测速记账** | `v_drain / v_acct` 按 **实测速度 `v_meas`** 记账；限速只影响 `SetSpeedLimit` |
| **EPOC 采样** | 氧债峰值采样速度会钳到 `appliedLimit` 内，避免跑飞速度错误记氧债 |
| **SpeedBridge** | 优先 `SetSpeedLimit(source, limit)` 参与与灌木/铁丝网的 **min** 合并；非 `SCR_ChimeraCharacter` 才回退 `OverrideMaxSpeed` |
| **已移除** | 玩家主路径的 25%/35%「意志力平台期」、固定 `Run×30%` Sprint 公式、把限速直接写死到 `OverrideMaxSpeed` 覆盖 Foliage 的旧做法 |

### 主要特性

#### 1. v6 功率预算与行军档
- ✅ **CP–W′ 功率预算**：Pandolf/ACSM 代谢功率 → 动态 CP → W′ 放电/再填充（Elite 为 Skiba 双指数，Standard / Tactical 为线性恢复）
- ✅ **相位速度曲线**：玩家主速度曲线使用 `CalculateV6PhaseSpeedMultiplier()`；**不再**用意志力平台期
- ✅ **撞墙阻尼**：`SCR_RSS_CollapseTransition` 在跌破跛行阈值（`SMOOTH_TRANSITION_END = 0.05`）时给 5 秒阻尼过渡
- ✅ **当前运行时档位默认值**：因 `V6_USE_MARCH_GAIT_SPEEDS = false`，当前步态顶速回退到引擎 Walk / Run / Sprint ≈ **1.45 / 3.8 / 5.5 m/s**；若以后打开该开关，则 Params 内仍保留 **1.4 / 2.8 / 4.5 m/s** 行军档字段
- ✅ **Sprint 最低拉开**：`SPRINT_GAIT_MIN_OVER_RUN_RATIO = 1.25`，保证 Sprint 身份；但实际上限仍受可用功率与 W′ 约束

#### 2. 负重、坡度与陆地代谢
- ✅ **负重主要影响“油耗”**：负重既抬高陆地代谢功率，也压低动态 CP，不再只是简单扣速度
- ✅ **负重速度惩罚**：回退常量 `ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.28`、指数 `1.5`；实际运行时优先走 `SCR_RSS_Params` 与 `SCR_RSS_EncumbranceCache` 的分段惩罚
- ✅ **坡度自适应**：目标速度通过 Tobler 系函数调整，而不是“每度固定 -2.5%”的线性规则
- ✅ **陆地代谢模型**：低速/步行优先 Pandolf，高速 Run/Sprint 进入 ACSM 跑/冲模型，中间 C¹ 混合
- ✅ **测速记账按实测速度**：即使限速存在，代谢记账仍以 `v_meas` 为主，W′ 超速记账路径也围绕这一点构建

#### 3. Sprint / W′ 细节
- ✅ **Sprint 门禁**：有氧阈值回退 `SPRINT_ENABLE_THRESHOLD = 0.25`，另有 W′ 门槛 `anaerobic_sprint_enable_threshold = 0.20`
- ✅ **Sprint 不是固定 +30%**：当前主路径为步态目标 + 功率软顶 + 最低拉开；老 README 的 `Run×1.30` 只属于历史模型
- ✅ **Sprint 额外消耗不是“固定 3.0x”**：当前消耗主导项来自功率模型、`V6_SPRINT_AEROBIC_DRAIN_FACTOR = 0.72`、W′ 放电，以及 `sprint_stamina_drain_multiplier` 的有效倍率换算
- ✅ **负重冲刺惩罚更强**：`SPRINT_ENCUMBRANCE_PENALTY_MULT = 2.2`；不是“冲刺时负重惩罚更轻”的旧规则
- ✅ **时间 CD 已退居历史**：v6 主门禁看 STA + W′，旧 burst cooldown 字段主要作兼容保留

#### 4. 恢复、疲劳与 EPOC
- ✅ **恢复按“代谢净值”计算**：姿态、负重、热/冷、地表湿度、EPOC 等共同影响最终恢复率
- ✅ **EPOC 延迟**：停下后 **2 秒** 内进入 EPOC 延迟期（`EPOC_DELAY_SECONDS = 2.0`），这时不进入正常有氧恢复
- ✅ **低体力恢复门槛**：`MIN_RECOVERY_REST_TIME_SECONDS = 5.0`，而不是旧文档中的 10 秒
- ✅ **负重恢复惩罚**：回退 `LOAD_RECOVERY_PENALTY_COEFF = 0.0002`；当前实现是**扣减恢复**，没有“重载下额外恢复加速”逻辑
- ✅ **高体力边际衰减**：体力高于约 80% 时恢复效率下降，避免静止瞬间回满
- ✅ **累积疲劳**：回退 `FATIGUE_ACCUMULATION_COEFF = 0.025`、`FATIGUE_MAX_FACTOR = 2.5`
- ✅ **移动中不回血**：陆地速度达到 `RSS_IDLE_SPEED_THRESHOLD_MPS` 以上时，当前有氧恢复乘数会被置 0

#### 5. 环境系统
- ✅ **热应激**：当前主路径基于**实时气温阈值**（≥26°C）计算，室内按倍数折半减免；不再以“10:00–18:00 时间段”作为唯一主判据
- ✅ **冷应激**：独立冷恢复惩罚与静态惩罚路径
- ✅ **降雨湿重**：按降雨强度连续累积（`0.5 × rainIntensity^1.5 kg/s`），而不是小雨/中雨/暴雨三段固定档位
- ✅ **总湿重**：游泳湿重 + 降雨湿重**直接相加**，再钳到 `ENV_MAX_TOTAL_WET_WEIGHT = 10 kg`
- ✅ **风阻、泥泞、地表湿度**：分别进入能量消耗、冲刺惩罚、趴姿恢复惩罚或滑倒风险链路

#### 6. 游泳系统
- ✅ **独立游泳模型**：不套用陆地坡度/步态；使用 `SCR_RSS_SwimmingStateManager` + `SCR_RSS_SwimmingStaminaModel`
- ✅ **动态功率主导**：水阻消耗主导项随速度近似 **v³** 增长，而不是简单“水平阻力 v² 直接等于最终消耗”
- ✅ **基础功率回退值**：`SWIMMING_BASE_POWER = 20W`（可被配置覆盖）
- ✅ **负重阈值**：`SWIMMING_ENCUMBRANCE_THRESHOLD = 25kg` 后静态踩水消耗显著上升
- ✅ **游泳湿重**：水中湿重非线性增长到最高 **10kg**；上岸后 **30 秒线性衰减**，不是旧文档中的固定上岸湿重值
- ✅ **速度检测**：位置差分测速，解决游泳命令下 `GetVelocity()` 为 0 的问题

#### 7. 跳跃 / 翻越 / 攀爬
- ✅ **跳跃物理成本**：通过 `ComputeJumpCostPhys()` 按总重量、抬升高度、估算水平速度、肌肉效率求成本，不再是固定 3.5%
- ✅ **翻越物理成本**：`ComputeClimbCostPhys()` 按物理近似计费，持续攀爬为定时再计费，不是固定 `1%/s`
- ✅ **连续跳跃惩罚**：当前窗口 **2 秒**、冷却 **2 秒**，每次追加 `JUMP_CONSECUTIVE_PENALTY = 0.5`
- ✅ **低体力禁跳**：体力 `< 10%` 时禁用跳跃

#### 8. 表现、物品、AI 与联机
- ✅ **HUD / 调试**：HUD 默认关，调试信息路径集中在 `SCR_RSS_DebugDisplay` 与 UpdateLoop 调试模块
- ✅ **第一人称镜头**：`CharacterCamera1stPerson.c` 负责相机惯性 / Sprint FOV / 头部物理等表现
- ✅ **CSB / Morphine**：物品、UserAction、DamageEffect 与表现脚本已完整分层
- ✅ **AI（实验性）**：当前主链是 `AIManager` → `AIStaminaState` → `AISpeedCap` → `AIIntentFilter` → `AICombatDecay`；`AIInjuryLink` 由体力协调器调用
- ✅ **服务端权威配置**：`SCR_RSS_ConfigManager` + `SCR_RSS_NetworkSyncManager` 管理同步、版本与迁移
- ✅ **外部模组 API**：`SCR_RSS_API` / `RSS_PlayerInfo` / `RSS_EnvironmentInfo`

## 项目结构

约 **98** 个 EnforceScript `.c`。当前真实树如下：

```
Realistic-Stamina-System/
├── .gitattributes / .gitignore
├── AUTHORS.md / CONTRIBUTING.md / CHANGELOG.md
├── LICENSE / LICENSE.txt
├── README.md / README_CN.md / README_EN.md
├── addon.gproj                           # GUID 68649101601CC93D
├── WORKSHOP_CHANGENOTE_v3.23.0.txt
├── WORKSHOP_CHANGENOTE_v3.23.1.txt
├── Assets/
├── Configs/EntityCatalog/
├── Prefabs/
│   ├── Characters/Core/Character_Base.et
│   └── Items/Medicine/CombatStimInjection_01/
├── UI/layouts/
│   ├── HUD/StatsPanel/
│   └── Menus/RSSSettings/
├── docs/
├── githooks/pre-commit
├── scripts/Game/
│   ├── Integration/
│   │   ├── PlayerBase.c
│   │   ├── PlayerBase_UpdateLoop.c
│   │   ├── SCR_StaminaOverride.c
│   │   ├── SCR_RSS_ServerBootstrap.c
│   │   ├── SCR_RSS_InventoryOverride.c
│   │   ├── SCR_PlayerBaseIntegrationHelpers.c
│   │   ├── SCR_PlayerBaseLoop.c
│   │   ├── SCR_PlayerBaseRpcHandler.c
│   │   ├── SCR_PlayerBaseVehicleHelper.c
│   │   └── SCR_RSS_StaminaComponentCompat.c
│   ├── RSS/
│   │   ├── Core/                         # ~30
│   │   │   ├── SCR_RSS_MetabolismModel.c / SCR_RSS_MetabolismMath.c
│   │   │   ├── SCR_RSS_CriticalPowerModel.c / SCR_RSS_AnaerobicBurst.c
│   │   │   ├── SCR_RSS_UpdateCoordinator.c / SCR_RSS_DrainCalculator.c
│   │   │   ├── SCR_RSS_SpeedCalculator.c / SCR_RSS_SpeedBridge.c
│   │   │   ├── SCR_RSS_RecoveryCalculator.c / SCR_RSS_EpocState.c / SCR_RSS_FatigueSystem.c
│   │   │   ├── SCR_RSS_StaminaConsumptionCalculator.c / SCR_RSS_StaminaNetRate.c
│   │   │   ├── SCR_RSS_CollapseTransition.c / SCR_RSS_SprintGate.c / SCR_RSS_SprintBlockSpeedTransition.c
│   │   │   ├── SCR_RSS_EncumbranceCache.c / SCR_RSS_StanceTransitionManager.c / SCR_RSS_ExerciseTracker.c
│   │   │   ├── SCR_RSS_SwimmingStaminaModel.c / SCR_RSS_WPrimeServerTick.c / SCR_RSS_CombatStimController.c
│   │   │   ├── SCR_RSS_Constants.c / SCR_RSS_ConfigBridge.c / SCR_RSS_StaminaHelpers.c / SCR_RSS_StaminaState.c
│   │   │   └── 调试/更新辅助文件（UpdateLoopDebugOutput / DebugBatchManager / …）
│   │   ├── Environment/                  # 18
│   │   │   ├── SCR_RSS_EnvironmentFactor.c / SCR_RSS_EnvironmentDebug.c
│   │   │   ├── SCR_RSS_AstronomyMath.c / SCR_RSS_WeatherApi.c / SCR_RSS_WeatherChangeDetector.c
│   │   │   ├── SCR_RSS_PenaltyMath.c / SCR_RSS_EnvConstants.c / SCR_RSS_TemperatureSampler.c
│   │   │   ├── SCR_RSS_RainWetWeight.c / SCR_RSS_IndoorDetection.c / SCR_RSS_EnvPendingUpdate.c
│   │   │   ├── SCR_RSS_TerrainDetection.c / SCR_RSS_MaterialTerrainTable.c
│   │   │   ├── SCR_RSS_SlopeSpeedTransition.c / SCR_RSS_JumpVaultDetection.c
│   │   │   ├── SCR_RSS_SwimmingState.c / SCR_RSS_SwimConstants.c
│   │   │   └── SCR_RSS_EnvLocationBootstrap.c
│   │   ├── AI/                           # 8（当前实现）
│   │   │   ├── SCR_RSS_AIManager.c
│   │   │   ├── SCR_RSS_AIStaminaState.c
│   │   │   ├── SCR_RSS_AISpeedCap.c
│   │   │   ├── SCR_RSS_AIIntentFilter.c
│   │   │   ├── SCR_RSS_AICombatDecay.c
│   │   │   ├── SCR_RSS_AIInjuryLink.c
│   │   │   ├── SCR_RSS_AIUpdateInterval.c
│   │   │   └── SCR_RSS_AIConstants.c
│   │   ├── NetworkConfig/                # 8
│   │   │   ├── SCR_RSS_Settings.c / SCR_RSS_Params.c / SCR_RSS_SettingsPresetBake.c / SCR_RSS_SettingsSync.c
│   │   │   ├── SCR_RSS_ConfigManager.c / SCR_RSS_NetworkSyncManager.c
│   │   │   ├── SCR_RSS_API.c / SCR_RSS_DataExport.c
│   │   ├── MudSlip/
│   │   │   ├── SCR_RSS_MudSlipEffects.c
│   │   │   └── SCR_RSS_MudSlipRunner.c
│   │   └── Presentation/                 # 12
│   │       ├── CharacterCamera1stPerson.c
│   │       ├── SCR_RSS_StaminaHUDComponent.c / SCR_RSS_UISignalBridge.c / SCR_RSS_DebugDisplay.c
│   │       ├── SCR_RSS_PresentationBridge.c / SCR_RSSAdminMenuUI.c
│   │       ├── SCR_RSSSettingsTab.c / SCR_RSSSettingsSubMenu.c / SCR_RSS_SettingsDescriptions.c
│   │       └── 屏效脚本（Regeneration / NoiseFilter / Desaturation）
│   ├── Components/Gadgets/
│   ├── UserActions/
│   └── Damage/DamageEffects/CharacterDamageEffects/
└── tools/
    ├── rss_pipeline_v6.py / rss_pipeline_v4.py / rss_digital_twin_fix.py
    ├── rss_sim/（PyO3） / rust_pipeline_v6/
    ├── test_v6_smoke.py / test_v5_smoke.py / test_acft_2mile.py / bench_physio_anchors.py
    ├── check_script_size.py / check_enforce_syntax.py / embed_json_to_c.py
    ├── optimized_rss_config_*_v4.json / *_v6.json
    └── README.md
```

## v6.0.0 版本更新 / v6.0.0 Updates

**2026-06-04**（详见 [CHANGELOG.md](CHANGELOG.md) **[6.0.0]**）

- **CP–W′** — `SCR_RSS_MetabolismModel`（Pandolf + ACSM）+ `SCR_RSS_CriticalPowerModel`（焦耳 W′、动态 CP、Elite Skiba 再填充）
- **移除意志力平台期** — 主速度曲线改为 `CalculateV6PhaseSpeedMultiplier`（低 STA 跛行）
- **v_drain 闭环** — 消耗与 `m_fAppliedSpeedLimitMs` / SpeedBridge 对齐
- **Integration** — `TickPower` + runtime context；Sprint 门禁走 CP 模型
- **疲劳 / EPOC** — 积分疲劳 I(t)；EPOC ∝ 峰值功率
- **AI** — `SCR_RSS_AISpeedCap` 与玩家同源 v6 相位曲线
- **工具** — `test_v6_smoke.py`、`test_acft_2mile.py`、Python/Rust 孪生与 `rss_pipeline_v6.py`
- **配置** — `m_sConfigVersion` / ConfigManager → **6.0.0**；Params +CP/W′/sprint cap

## v5.0.0 版本更新 / v5.0.0 Updates

**2026-06-04**（详见 [CHANGELOG.md](CHANGELOG.md) **[5.0.0]** / **[5.0.0-dev]**）

- **双池语义** — 有氧主条 + 无氧/W′ 前身语义；命名统一 `SCR_RSS_*`
- **归档** — v3.23.1 脚本归档后清理；Integration 拆分 `PlayerBase_UpdateLoop.c`
- **冲刺门禁 / 联机 / HUD** — 见 CHANGELOG；为 v6 CP–W′ 铺路

## v3.23.1 版本更新 / v3.23.1 Updates

**2026-05-28**（详见 [CHANGELOG.md](CHANGELOG.md) **[3.23.1]**）

- **1.7 灌木丛减速** — 新增 `SCR_RSS_SpeedBridge`，RSS 体力限速与原生 Foliage 减速合并，穿灌木不再被 RSS 覆盖。
- **HUD / 配置** — 回满 ETA 分段积分估算；`willpower_threshold` / `sprint_enable_threshold` / 蹲姿恢复可按预设配置。
- **稳定版预设** — 三档参数锁定 v3.23.0 基线；`CURRENT_VERSION` **3.23.1**。

## v3.23.0 版本更新 / v3.23.0 Updates

**2026-05-18**（详见 [CHANGELOG.md](CHANGELOG.md) **[3.23.0]**）

- **版本** - `CURRENT_VERSION` **3.23.0**（`SCR_RSS_ConfigManager.c`）、AI 子系统管理器重构。
- **AI 体力** - 6 态状态机 + 群组同步/休息路点/意图过滤/远距代理；设置中 AI 战斗标为 experimental。
- **泥泞** - 管理员可再次通过设置开启泥泞滑倒（此前版本曾全路径强制关闭）。
- **设置 UI** - RSS 设置页右侧说明面板；Stamina HUD 布局与字体修复。
- **工具** - `tools/` 收敛为 v4 数字孪生管线。

## v3.22.8 版本更新 / v3.22.8 Updates

**2026-05-10**（详见 [CHANGELOG.md](CHANGELOG.md) **[3.22.8]**）

- **版本** - `CURRENT_VERSION` **3.22.8**（`SCR_RSS_ConfigManager.c`）、三份 README 与 Workshop 说明对齐。
- **仅原生表现** - `RSS_PRESENTATION_NATIVE_ONLY` 默认 `true`：只保留引擎原生相机/屏效/相关音频，不应用 RSS 自定义表现；`false` 时恢复冲刺 FOV、泥泞镜头、CombatStim 首针/OD 屏效与可选冲刺发闷。
- **冲刺 FOV** - 体力滞回与目标平滑、变化速率上限 8°/s；泥泞机制关闭时镜头应力路径立即清零。

## v3.22.5 版本更新 / v3.22.5 Updates

**2026-05-09**（详见 [CHANGELOG.md](CHANGELOG.md) **[3.22.5]**）

- **版本** - `CURRENT_VERSION` **3.22.5**（`SCR_RSS_ConfigManager.c`）、三份 README 与 Workshop 说明对齐。
- **退出崩溃热修** - 客户端关游戏 / 断线时对空指针读取导致的 Access violation：`RSS_WaitForGameModeConfig` 判空与同步成功后移除重复 `CallLater`；GameMode 扩展析构与回调入口守卫；体力 HUD 在 Workspace 已销毁时不刷新 / 不强制移出层级；`InitStaminaHUD` 在组件已删除或无 Owner 时跳过。
- **Workbench 重载世界** - `OnGameStart` 清空 `EnvironmentFactor` 全局天气信号静态缓存、AI 休整注册表与 HUD 单例，避免「重载脚本 + 重载世界」后仍使用已销毁的 `GameSignalsManager` 或旧 Widget 导致 `0x0` 读。

## v3.21.1 版本更新 / v3.21.1 Updates

**2026-04-12**（详见 [CHANGELOG.md](CHANGELOG.md) **[3.21.1]**）

- **版本** - `CURRENT_VERSION` **3.21.1**（`SCR_RSS_ConfigManager.c`）、三份 README 与 `PlayerBase.c` 头注释对齐。
- **注射器与药效** - 由原先**肾上腺素式战术兴奋针**（RUSH/CRASH 等）改为 **苯甲酸钠咖啡因 20%（CSB）** 注射器；阶段为 **吸收等待 → 药效期**，药效期内体力消耗减免、并可按设计绕过部分天气对体力链的惩罚（见 CHANGELOG 与 `SCR_CombatStimConstants`）。
- **特效** - 删除与新药效无关或冗余的 **HUD 全屏叠层**、旧 ScreenEffects 挂钩及附属表现；药效不依赖屏幕特效，仍由体力与环境逻辑体现。

## v3.21.0 版本更新 / v3.21.0 Updates

**2026-04-12**（条目与当前仓库变更一致，详见 [CHANGELOG.md](CHANGELOG.md) **[3.21.0]**）

- **配置与 README** - `CURRENT_VERSION` **3.21.0**（`SCR_RSS_ConfigManager.c`）、三份 README 与 `PlayerBase.c` 头注释版本对齐。
- **战术兴奋剂** - `SCR_CombatStimConstants`、`SCR_ConsumableCombatStimInjector`、`SCR_CombatStimUserAction`；`PlayerBase` 内 RUSH/CRASH/过量、体力与速度、**`RPC_CombatStimSyncToOwner`**；预制体 **`Prefabs/Items/Medicine/CombatStimInjection_01/`**。
- **注射器扩展** - 自定义注射器与吗啡相关消耗品 / UserAction / DamageEffect 脚本（见 CHANGELOG 文件列表）。
- **实体目录** - `Configs/EntityCatalog/US/InventoryItems_EntityCatalog_US.conf` 登记战斗兴奋剂军火库数据（若需在游戏内出现，还须按 CHANGELOG 说明接好派系 `US.conf` 目录引用）。

## v3.20.6 版本更新 / v3.20.6 Updates

**2026-04-09** · 发布区间（含两端）：`b18953d9421289a3bc994bfba621da04f5fcb41c` … `a79a8ef04777f73d938049374c68d8c9cb60b8e3`

### 配置与工具

- **预设与嵌入** - `SCR_RSS_Params` 与 `embed_json_to_c.py`、三份 `optimized_rss_config_*_super.json`、`SCR_RSS_Settings.c` 同步；嵌入脚本支持 JSON 缺省时的固定默认值，避免丢失生理学常数字段。
- **优化管线与绘图** - 优化管线与绘图工具更新（该时期脚本名见 CHANGELOG；现行工具见 [tools/README.md](tools/README.md)）。
- **配置版本** - `SCR_RSS_ConfigManager` 中 `CURRENT_VERSION` = **3.20.6**。

### 区间内其它变更（摘要）

- 自 **AI 交战体力效果**、专用服 AI 群组代理与 `RSS_PERF_*` 起，至 **3.20.5** 的网络同步、体力协调器与多模块性能/修复，**完整条目见 [CHANGELOG.md](CHANGELOG.md) 中 [3.20.6] 与 [3.19.3]～[3.20.5]**。

### （原 v3.20.5 亮点，仍含于本发布区间）

**2026-03-30**

#### ⚡ 性能优化

- **Pandolf 调用减少** - `CalculateLandBaseDrainRate` 中 T1 阻尼结果提升为外层变量复用，努力补偿分支在速度差 < 0.01 时跳过重复调用，最坏情况 4 次→3 次，常见情况降至 2 次（`SCR_RSS_UpdateCoordinator.c`）。
- **消除运行时除法** - `ShouldSync` / `AcceptClientReport` 新增三个预计算间隔常量替换 `1.0 / Hz` 浮点除法（`SCR_RSS_NetworkSyncManager.c`）。

#### 🔧 修复

- **网络速度插值实际无效** - `SMOOTH_TRANSITION_DURATION` 从 `0.05s` 提升至 `0.15s`，50ms 间隔下 lerpSpeed ≈ 0.33，插值真正生效，消除速度突变感（`SCR_RSS_NetworkSyncManager.c`）。
- **死代码清理** - 删除 `CalculateBaseDrainRate` 中计算后从未使用的 `loadFactor` 等三行（`SCR_RSS_UpdateCoordinator.c`）。

#### 🏗️ 重构

- **提取 `BuildRecoveryContext`** - 新增 `RecoveryContext` 结构体与同名私有方法，合并 `UpdateStaminaValue` 与 `GetNetStaminaRatePerSecond` 中约 60 行重复的恢复率参数组装逻辑（`SCR_RSS_UpdateCoordinator.c`）。

---

## v3.14.1 版本更新 / v3.14.1 Updates

**2026-03-05**

### ✅ 新增

- **战术冲刺爆发与冷却** - Sprint 前 8 秒爆发期内负重对速度惩罚降至 20%；8～13 秒缓冲区内线性过渡到正常惩罚；爆发结束或松开冲刺后 15 秒冷却内再次冲刺不触发爆发。
- **室内楼梯负重速度减免** - 室内且原始坡度 &gt; 0 时判定为楼梯，负重速度惩罚乘 0.4；新增 `GetRawSlopeAngle` 供室内楼梯判定使用。
- **坡度速度过渡立即提速** - 目标速度比当前平滑值高出 ≥ 0.08 时取消 5 秒平滑、立即提速，避免上坡中细微缓坡频繁加速。
- **镜头惯性与头部物理** - 第一人称下：起步滞后/前倾、急停前冲（随负重非线性）、步伐垂直颠簸与上坡左右摇摆；冲刺 FOV 与战术爆发联动（Burst +5° / Cruise +3° / Limp -2°）。见 [CharacterCamera1stPerson.c](scripts/Game/RSS/Presentation/CharacterCamera1stPerson.c)、[SCR_RSS_Constants.c](scripts/Game/RSS/Core/SCR_RSS_Constants.c)。

### 🔁 变更

- **坡度与消耗/速度** - 缓坡下坡消耗最多减少 60%（原 50%）；Tobler 坡度速度上坡再乘 1.15、下坡再乘 1.15 且上限 1.25，坡度对速度限制再向 1.0 拉近 30%。

### 📚 文档

- [体力系统计算逻辑文档](docs/体力系统计算逻辑文档.md) 补充战术冲刺、室内楼梯、坡度过渡、镜头与头部物理等小节与常量表。

---

## v3.12.0 版本更新 / v3.12.0 Updates

**2026-02-09**

本版本仅记录 C 脚本层面的变化。

### ✅ 新增
- **环境温度物理模型** - 新增温度步进、短波/长波与云量修正、太阳/日出日落与月相推断，支持引擎温度或模组模型切换（scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c）
- **室内检测增强** - 增加向上射线与水平封闭检测，降低开放屋顶/天窗误判（scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c）
- **配置变更同步链路** - 新增监听器注册、变更检测、全量参数/设置数组同步与广播（scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c、scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c、scripts/Game/Integration/PlayerBase.c）
- **网络校验与平滑** - 客户端上报体力/负重，服务器权威校验并下发速度倍率，含重连延迟同步（scripts/Game/Integration/PlayerBase.c、scripts/Game/RSS/NetworkConfig/SCR_RSS_NetworkSyncManager.c）
- **日志节流工具** - 统一 Debug/Verbose 日志节流接口（scripts/Game/RSS/Core/SCR_RSS_Constants.c）

### 🔁 变更
- **服务器权威配置** - 客户端不再写入 JSON，仅内存默认值等待同步；服务器写盘并增加备份/修复流程（scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c）
- **移动相位驱动消耗** - 优先以移动相位/冲刺状态决定 Pandolf 路径，并提供服务端权威速度倍数计算接口（scripts/Game/RSS/Core/SCR_RSS_UpdateCoordinator.c）。Givoni模型已弃用。
- **负重参数约束** - 新增负重惩罚指数/上限并对预设进行 clamp（scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c、scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c、scripts/Game/RSS/Core/SCR_RSS_Constants.c）
- **预设参数刷新** - Elite/Standard/Tactical 预设全面更新，并补充天气模型顶层默认值（scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c）
- **冲刺消耗默认值** - Sprint 消耗倍数默认改为 3.5，支持配置覆盖（scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c、scripts/Game/RSS/Core/SCR_RSS_Constants.c）
- **体重参与消耗** - 体力消耗输入改为“装备+身体”的总重，并优化调试输出（scripts/Game/Integration/PlayerBase.c）

### 🐞 修复
- **室内判定误报** - 通过屋顶射线与水平封闭检测降低开放屋顶/天窗误判（scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c）
- **引擎温度极值退化** - 当日最小/最大温度趋同会回退到物理/模拟估算，避免温度异常（scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c）
- **客户端写盘覆盖** - 客户端不再写入本地 JSON，避免覆盖服务器配置（scripts/Game/RSS/NetworkConfig/SCR_RSS_ConfigManager.c）

## v3.11.1 版本更新 / v3.11.1 Updates

**2026-02-02**

### 🔧 配置修复与优化
- ✅ **JSON 配置覆盖修复** - 用户修改的 hint/debug 等不再被覆盖
- ✅ **ValidateSettings 修正** - 参数越界时仅 clamp，不清空全部配置
- ✅ **Custom 预设大小写** - 支持 "custom" / "CUSTOM" 识别
- ✅ **HUD 默认关闭** - 首次下载默认不显示 HUD；已有 JSON 的老用户 HUD 按原设置保持
- ✅ **配置可读性** - 常量提取、描述精简、分组优化

## v3.11.0 版本更新 / v3.11.0 Updates

**2026-01-26**

### 🌟 核心功能更新
- ✅ **体力系统优化** - 优化体力系统响应速度和起步体验
- ✅ **体力系统修复** - 修复体力系统的高频监听和速度计算问题
- ✅ **负重系统增强** - 实现库存变更时实时更新负重缓存
- ✅ **负重计算修复** - 修复武器重量未计入总负重的问题
- ✅ **环境感知增强** - 添加室内环境忽略坡度影响功能
- ✅ **配置管理优化** - 修复预设配置逻辑，确保系统预设值保持最新
- ✅ **参数优化** - 优化RSS系统参数并调整配置文件路径

### 📁 项目清理与优化
- ✅ **项目文件清理** - 删除所有生成的 PNG 图表文件
- ✅ **Tools 目录优化** - 仅保留核心 NSGA-II 优化管道，删除过时脚本
- ✅ **配置文件更新** - 移除旧的优化配置文件和更新相关路径

### 📚 文档完善
- ✅ **工具集文档** - 新增 tools/README.md - 完整工具集文档
- ✅ **配置验证报告** - 新增 CONFIG_APPLICATION_VERIFICATION.md - 配置应用验证报告
- ✅ **开关验证报告** - 新增 DEBUG_AND_HINT_SWITCH_VERIFICATION.md - 开关验证报告

### 🎯 版本整并
- ✅ **版本统一** - 将从提交 d1ebb9c 到现在的所有变更作为 v3.11.0

详细信息请参考 [CHANGELOG.md](CHANGELOG.md) / See [CHANGELOG.md](CHANGELOG.md) for details.

## 技术文档 / Technical Documentation

- [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md) — v6 CP–W′ 计算逻辑（权威）
- [docs/体力系统计算逻辑文档.md](docs/体力系统计算逻辑文档.md) — 历史归档（v3/v5 旧逻辑）
- [docs/数字孪生优化器计算逻辑文档.md](docs/数字孪生优化器计算逻辑文档.md) — 数字孪生仿真器公式与决策树
- [docs/RSS_API.md](docs/RSS_API.md) — 外部模组 API
- [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) — 命名 / 分层 / 文件大小限制
- [tools/README.md](tools/README.md) — 工具链使用说明

> `docs/` 下部分旧设计稿仍用于保留历史背景，但**当前行为请以源码与本文“当前机制”章节为准**。

## 技术实现

### 模型架构

本项目当前使用以下模型族：

- **Pandolf 能量消耗模型**：低速 / 步行 / 负重 / 坡度代谢主路径
- **ACSM 跑/冲模型**：高速度跑动的代谢功率；与 Pandolf 在中间速度区 C¹ 混合
- **Critical Power–W′**：可持续功率上限 + 焦耳池，用于 Sprint / 超限速 / 冲刺门禁
- **个性化运动建模参数化思想**：疲劳、恢复非线性、代谢适应等
- **环境物理近似**：热冷、风、雨、湿重、泥泞、游泳阻力

### 实现方式

- **引擎体力条拦截**：`SCR_StaminaOverride` 拦截引擎体力修改，只允许 RSS 受控写回目标体力
- **主循环**：`PlayerBase.c` 持有状态与模块引用，`PlayerBase_UpdateLoop.c` 执行主更新；`SCR_PlayerBaseLoop.Tick()` 负责 Callqueue 调度
- **限速桥接**：`SCR_RSS_SpeedBridge` 优先走 `SetSpeedLimit(source, limit)`；只有不是 `SCR_ChimeraCharacter` 时才回退 `OverrideMaxSpeed`
- **玩家 / AI 调度**：玩家默认 17 ms；AI 100 ms 基线 + 距离 LOD；AI 行为链 500 ms 节流
- **HUD / Debug**：Presentation 层负责 HUD、FOV、调试输出与设置页，不把业务公式塞进 UI 层

### 当前速度与消耗逻辑（v6）

#### 1. 陆地速度曲线
- 玩家速度主路径：`SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier()`
- 当 `staminaPercent >= SMOOTH_TRANSITION_END (0.05)`：按当前步态目标速度 × 负重修正输出速度倍率
- 当 `staminaPercent < 0.05`：插值到动态跛行倍率，并可叠加 `SCR_RSS_CollapseTransition` 的 5 秒阻尼
- **旧的 `stamina^0.6` 平台期曲线不是玩家当前主路径**；`CalculateSpeedMultiplierByStamina()` 仅为兼容入口，内部转发到 v6 曲线

#### 2. 步态目标与 Sprint
- 当前运行时由于 `V6_USE_MARCH_GAIT_SPEEDS = false`：
  - Walk 顶速来自 `ENGINE_WALK_TOP_MS = 1.45`
  - Run 顶速来自 `TARGET_RUN_SPEED = 3.8`
  - Sprint 顶速来自 `GAME_MAX_SPEED = 5.5`
- Params 内仍保留 `v5_walk_speed_ms = 1.4`、`v5_run_speed_ms = 2.8`、`v5_sprint_speed_ms = 4.5`，若未来打开行军档开关即可启用
- Sprint 速度由 `GetV6SprintSpeedMs()` 决定：步态基准、W′ 是否充足、可用功率软顶、与 Run 的最低 1.25× 拉开共同决定

#### 3. 负重惩罚
- 回退常量：`ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.28`、`ENCUMBRANCE_SPEED_EXPONENT = 1.5`
- 运行时优先使用 `SCR_RSS_Params` / `SCR_RSS_ConfigBridge` / `SCR_RSS_EncumbranceCache`
- Sprint 负重惩罚会被 `SPRINT_ENCUMBRANCE_PENALTY_MULT = 2.2` 放大，而不是减轻

#### 4. 代谢记账
- `SCR_RSS_DrainCalculator` 明确：`v_drain / v_acct` 一律按**实测速度 `v_meas`**，不再 `min(v_meas, v_limit)`
- `SCR_RSS_MetabolismModel` 负责把速度、总重量、坡度、地形、步态转换为代谢功率
- `SCR_RSS_CriticalPowerModel` 负责把超出 CP 的部分转为 W′ 放电，并在恢复阶段回填

#### 5. 恢复 / EPOC / 疲劳
- 正常恢复计算入口：`SCR_RSS_RecoveryCalculator.CalculateRecoveryRate()`
- 只要 `currentSpeed >= RSS_IDLE_SPEED_THRESHOLD_MPS`，当前有氧恢复乘数就会归零
- 停下后 **2 秒** 内走 EPOC 延迟路径；不是旧文档的 5 秒
- `BASE_RECOVERY_RATE = 0.00010`、`LOAD_RECOVERY_PENALTY_COEFF = 0.0002`、`MIN_RECOVERY_REST_TIME_SECONDS = 5.0`
- `FATIGUE_ACCUMULATION_COEFF = 0.025`、`FATIGUE_MAX_FACTOR = 2.5`

#### 6. 环境
- 热应激主路径按**实时气温**超过 26°C 后线性上升，最大乘数 1.5；室内按倍数折半减免
- 降雨湿重按 `0.5 × rainIntensity^1.5 kg/s` 累积，停止降雨后 60 秒线性衰减
- 总湿重 = 游泳湿重 + 降雨湿重，最大 10kg
- 风阻、泥泞、冷应激、地表湿度等均有独立惩罚函数

#### 7. 游泳
- 游泳独立于陆地步态与坡度逻辑
- 动态消耗主导项近似随速度 **v³** 增长；`SWIMMING_BASE_POWER` 回退 20W
- 湿重在水中按平方根曲线累积到 10kg；上岸 30 秒线性衰减
- 大负重踩水成本显著上升，`SWIMMING_ENCUMBRANCE_THRESHOLD = 25kg`

#### 8. 跳跃 / 翻越
- 跳跃成本：`ComputeJumpCostPhys()`，按总重量、估算高度、估算水平速度、肌肉效率求解
- 翻越 / 攀爬成本：`ComputeClimbCostPhys()` + 持续攀爬定时重复计费
- 连续跳跃窗口 2 秒、冷却 2 秒、低体力 `<10%` 禁跳

### 关键参数（当前回退值；运行时仍可能被预设覆盖）

| 类别 | 参数 | 当前回退值 / 说明 |
|------|------|------------------|
| 顶速 | `GAME_MAX_SPEED` | `5.5 m/s` |
| Run 顶速 | `TARGET_RUN_SPEED` | `3.8 m/s` |
| Walk 顶速 | `ENGINE_WALK_TOP_MS` | `1.45 m/s` |
| 跛行阈值 | `SMOOTH_TRANSITION_END` | `0.05` |
| 最低速度保护 | `MIN_SPEED_MULTIPLIER` | `0.15` |
| 有氧 Sprint 门槛 | `SPRINT_ENABLE_THRESHOLD` | `0.25` |
| W′ Sprint 门槛 | `V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT` | `0.20` |
| Sprint 最低相对 Run 拉开 | `SPRINT_GAIT_MIN_OVER_RUN_RATIO` | `1.25` |
| Sprint 负重惩罚放大 | `SPRINT_ENCUMBRANCE_PENALTY_MULT` | `2.2` |
| 负重速度惩罚系数 | `ENCUMBRANCE_SPEED_PENALTY_COEFF` | `0.28` |
| 负重速度惩罚指数 | `ENCUMBRANCE_SPEED_EXPONENT` | `1.5` |
| 基础恢复率 | `BASE_RECOVERY_RATE` | `0.00010 / 0.2s` |
| 负重恢复惩罚 | `LOAD_RECOVERY_PENALTY_COEFF` | `0.0002` |
| 低体力恢复静止门槛 | `MIN_RECOVERY_REST_TIME_SECONDS` | `5.0s` |
| 疲劳累积系数 | `FATIGUE_ACCUMULATION_COEFF` | `0.025` |
| 疲劳最大因子 | `FATIGUE_MAX_FACTOR` | `2.5` |
| 玩家循环 | `RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS` | `17ms` |
| AI 循环 | `RSS_AI_SPEED_UPDATE_INTERVAL_MS` | `100ms` |

## 系统特性

### 体力与速度
- **当前机制**：体力高于跛行阈值时，维持当前步态目标速度；低于阈值时迅速进入跛行 / 撞墙阻尼区
- **历史逻辑提醒**：旧版 README 中的 25%–100% 平台期、`Run × 1.30` Sprint、`Walk = Run × 0.7` 等公式仅适用于旧代模型

### 负重与战术
- 负重不仅拖慢角色，也抬高代谢功率、压低可持续功率预算
- 重装兵更依赖趴姿恢复与 Sprint 节制；轻装兵更容易利用 W′ 做短冲

### 环境与游泳
- 热、冷、风、雨、泥、地表湿度都会进入体力链路
- 游泳是独立能量系统，不是把陆地体力逻辑“搬到水里”

### 调试与 HUD
- HUD 默认关闭，可在设置中打开
- 详细调试会输出当前速度、体力、步态、坡度、环境与湿重等信息
- `SCR_RSS_DebugDisplay` 属于 Presentation 层，不在 Core 层

## 外部模组 API

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
```

- `wPrimePool01` 是当前权威字段
- `anaerobicPercent` 仍保留，但只是兼容镜像，不建议新代码继续使用
- 环境信息见 `SCR_RSS_API.GetEnvironmentInfo()` 与 [docs/RSS_API.md](docs/RSS_API.md)

## 安装方法

1. 将整个 `RealisticStaminaSystem` 文件夹复制到 Arma Reforger Workbench 的 `addons` 目录
2. 在 Workbench 中打开 `addon.gproj`
3. 编译模组
4. 在游戏或服务器中启用模组

## 使用方法

1. 启动游戏并加载模组
2. 选择预设（默认 `StandardMilsim`），或切到 `Custom` 自行调参
3. 需要时开启 HUD、Debug、MudSlip、AI 战斗效果、数据导出
4. 若接入外部应用，可通过 `SCR_RSS_API` 或 JSON 数据导出读取状态

## 调整系统参数

### 推荐入口
- **玩法参数**：`SCR_RSS_Settings` / `SCR_RSS_Params`
- **系统预设 Bake**：`SCR_RSS_SettingsPresetBake.c`
- **JSON/工具链重新嵌入**：`tools/optimized_rss_config_*_v6.json` + `tools/embed_json_to_c.py`
- **硬常量回退**：`SCR_RSS_Constants.c`
- **运行时桥接**：`SCR_RSS_ConfigBridge.c`

### 常见可调项
- Sprint / W′：`critical_power_watts`、`w_prime_max_joules`、`w_prime_recovery_w_per_s`、`sprint_power_cap_watts`
- 行军档：`v5_walk_speed_ms`、`v5_run_speed_ms`、`v5_sprint_speed_ms`
- 负重：`encumbrance_speed_penalty_*`、`encumbrance_stamina_drain_coeff`、`load_metabolic_dampening`
- 恢复：`base_recovery_rate`、站/蹲/趴倍率、`max_recovery_per_tick`
- 环境：热/冷/雨/风/泥 / 气温 / 室内 / 地表湿度等开关与系数

### 调度说明
当前不是直接 `CallLater(UpdateSpeedBasedOnStamina, 200, false)` 的旧做法，而是：

```c
GetGame().GetCallqueue().CallLater(
    SCR_PlayerBaseLoop.Tick,
    GetSpeedUpdateIntervalMs(),
    false,
    this);
```

玩家默认为 17ms，AI 按距离 LOD 动态变化。

## 系统优势

### 拟真体验
- 速度、消耗、恢复属于同一套功率/环境叙事，而不是彼此割裂的常数表
- 负重、坡度、天气、游泳都会真实改变体力预算与冲刺能力
- 撞墙阻尼与跛行阈值使力竭手感更平滑

### 游戏平衡
- W′ 让短冲有价值，但无法无限利用
- 重装与轻装、平地与坡地、干燥与潮湿环境的玩法差异更明确
- 预设 / Custom 允许服主在“硬核拟真”与“战术动作”之间选取平衡点

## 已知问题与限制

### 当前限制
- HUD 默认关闭，需要手动开启
- 历史文档中仍保留了大量 v2/v3/v4 参数与版本叙事，阅读当前行为时请优先看本文“当前机制”章节
- 自定义表现（镜头/FOV/屏效）会受 `RSS_PRESENTATION_NATIVE_ONLY` 等表现开关影响
- 与其他同时修改速度/体力的模组可能存在冲突

### 技术注意
- 当前核心行为依赖 `SCR_RSS_SpeedBridge` 参与 `SetSpeedLimit` 的 min 合并；若旁路它直接覆写限速，容易破坏灌木/铁丝网减速
- `SCR_StaminaOverride` 的主动监控间隔是 **200ms**，不是旧文档中的 50ms
- `README` 中保留的早期版本节、版本历史和很多旧参数表，只作为归档材料

## 开发说明

### 编译要求
- Arma Reforger Workbench
- EnforceScript 编译器

### 代码结构（当前主入口）

- **主循环与实体状态**：`scripts/Game/Integration/PlayerBase.c`、`PlayerBase_UpdateLoop.c`
- **引擎体力条拦截**：`scripts/Game/Integration/SCR_StaminaOverride.c`
- **速度 / 代谢 / CP–W′**：`SCR_RSS_SpeedCalculator.c`、`SCR_RSS_MetabolismModel.c`、`SCR_RSS_CriticalPowerModel.c`
- **恢复 / EPOC / 疲劳**：`SCR_RSS_RecoveryCalculator.c`、`SCR_RSS_EpocState.c`、`SCR_RSS_FatigueSystem.c`
- **环境 / 游泳 / 跳跃**：`Environment/` 目录下各模块
- **AI**：`SCR_RSS_AIManager.c`、`SCR_RSS_AISpeedCap.c` 等
- **配置 / 网络 / API**：`NetworkConfig/` 目录
- **HUD / 设置 / 表现**：`Presentation/` 目录

### 开发约束
- 速度限速优先通过 `SCR_RSS_SpeedBridge`
- 单文件大小受 Workbench / EnforceScript 限制，见 [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md)
- 禁止三元 `?:`；单行 `if` 需带 `{}`；提交前建议跑 `check_script_size.py` 与 `check_enforce_syntax.py`

## 参考文献

本项目基于以下学术研究和数学模型：

1. **Pandolf, K. B., Givoni, B. A., & Goldman, R. F. (1977)**. Predicting energy expenditure with loads while standing or walking very slowly. *Journal of Applied Physiology*, 43(4), 577-581. https://journals.physiology.org/doi/abs/10.1152/jappl.1977.43.4.577
   - **应用**：本项目使用完整的 Pandolf 能量消耗模型计算体力消耗
   - **公式**：`E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))`
   - **说明**：坡度项 `G·(0.23 + 1.34·V²)` 已直接整合在公式中

2. **Palumbo, M. C., Morettini, M., Tieri, P., Diele, F., Sacchetti, M., & Castiglione, F. (2018)**. Personalizing physical exercise in a computational model of fuel homeostasis. *PLOS Computational Biology*, 14(4), e1006073. https://doi.org/10.1371/journal.pcbi.1006073
   - **应用**：本项目借用了个性化运动建模的思路来组织健康状态、疲劳累积、恢复非线性与代谢适应等参数化子系统
   - **说明**：具体数值以当前 `SCR_RSS_Params` / `SCR_RSS_Constants` / 预设 Bake 为准，不再沿用旧 README 中的固定百分比叙述

3. **Critical Power / W′ 与 Skiba 再填充**（运动生理学）
   - **应用**【v6】：`SCR_RSS_CriticalPowerModel` 动态 CP、W′ 焦耳池与 Elite 双指数再填充；详见 [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)

## 版本历史

> 下列条目保留其各自版本当时的设计背景、参数与路径命名，用于归档；**并不等同于当前 v6 运行时行为**。阅读现行机制请以上文“当前实现 / 技术实现”章节为准。

- **v6.0.0** (当前版本) - CP–W′ 拟真重构（见上文 v6.0.0 章节与 CHANGELOG）

- **v3.2.0** - 时间单位错误修复（历史）
  - **游泳湿重系统修复（Swimming Wet Weight Fix）**
    - 修复 `SCR_EnvironmentFactor.UpdateEnvironmentFactors()` 方法没有接收游泳湿重参数的问题
    - 修复前：总湿重计算只考虑降雨湿重，完全忽略游泳湿重
    - 修复后：正确接收并使用 `swimmingWetWeight` 参数，使用 `SwimmingStateManager.CalculateTotalWetWeight()` 计算总湿重
    - 修复前：上岸后湿重不增加，体力消耗不受影响
    - 修复后：上岸后立即获得 7.5kg 湿重，30秒内线性衰减到 0kg
    - 更新调试信息，添加总湿重显示

  - **时间单位错误修复（Time Unit Bug Fixes）**
    - 修复 `GetWorldTime()` 返回毫秒但多处代码没有转换为秒的问题
    - 修复前：所有时间相关的计算都错误了 1000 倍
    - 修复后：所有时间计算正确使用秒作为单位
    - 影响范围：
      - `PlayerBase.c`: 6 处修复（FatigueSystem、SwimmingStateManager、EPOC延迟）
      - `SCR_RSS_JumpVaultDetection.c`: 1 处修复（连续跳跃检测）
      - `SCR_RSS_ConfigManager.c`: 1 处修复（配置重载冷却）
      - `SCR_RSS_DebugDisplay.c`: 3 处修复（调试日志时间检查）
      - `SCR_RSS_UpdateCoordinator.c`: 1 处修复（速度计算时间）

  - **ExerciseTracker 时间单位修复**
    - 修复 `ExerciseTracker` 期望接收毫秒但传入了秒的问题
    - 修复前：时间被除以 1000.0 两次，运动时间和休息时间累积慢 1000 倍
    - 修复后：正确传入毫秒，`ExerciseTracker` 内部转换为秒
    - 影响范围：
      - `PlayerBase.c:113`: `ExerciseTracker.Initialize()` 传入毫秒
      - `PlayerBase.c:493`: `currentTimeForExercise` 使用毫秒
    - 修复前的影响：
      - 疲劳恢复需要 16.7 小时后才开始（而不是 60 秒）
      - 快速恢复期持续 50 小时（而不是 3 分钟）
      - 中等恢复期持续 167 小时（而不是 10 分钟）
    - 修复后：所有时间相关功能正常工作

  - **代码统计**：12处修复，涉及5个文件

  - **已知问题**：
    - **体力恢复速度可能过快**
      - 当前恢复速度基于 Optuna 优化的参数，可能在实际游戏中感觉过快
      - 作者对目前体力恢复速度感到不满意，可能会在以后调整
      - 恢复速度相关参数：
        - `base_recovery_rate`: 基础恢复率（每 0.2 秒）
        - `standing_recovery_multiplier`: 站姿恢复倍数
        - `prone_recovery_multiplier`: 趴姿恢复倍数
        - `FAST_RECOVERY_MULTIPLIER`: 快速恢复期倍数（3.5x）
        - `MEDIUM_RECOVERY_MULTIPLIER`: 中等恢复期倍数（1.8x）
      - 管理员可以通过修改配置文件中的 `m_sSelectedPreset` 字段切换预设
      - 或者在 `Custom` 预设中手动调整恢复速度相关参数

  - 详细修复内容请参考 [CHANGELOG.md](CHANGELOG.md)

- **v2.17.0** (2026-01-20) - Python模拟器完整优化
  - **地形因素影响系统**：铺装路面（1.0x）、碎石路（1.1x）、高草丛（1.2x）、重度灌木丛（1.5x）、软沙地（1.8x）
  - **环境因素影响系统**：
    - 温度影响：基于时间段的热应激计算（10:00-18:00，正午14:00达到峰值）
    - 风速影响：顺风减少消耗（最多10%），逆风增加消耗（最多5%）
    - 表面湿度影响：湿地趴下时的恢复惩罚（15%）
  - **动作成本计算系统**：跳跃消耗（3.5%体力，连续跳跃50%惩罚）、攀爬消耗（1%/秒）
  - **特殊运动模式系统**：游泳消耗（静态踩水25W基础+动态v³阻尼）、静态站立消耗（Pandolf静态项）
  - **高级修正模型系统**：Santee下坡修正（Givoni-Goldman 模型已弃用）
  - **战斗负重系统**：战斗负重百分比计算（基于30kg阈值）
  - **模拟器参数扩展**：新增地形类型、温度、风速、表面湿度、姿态、时间、室内等参数
  - **代码统计**：simulate_stamina_system.py新增12个函数（约450行）
- **v2.16.0** - 休息时间累积修复和虚拟气温曲线
  - 修复时间单位错误：GetWorldTime()返回毫秒，需要除以1000转换为秒
  - 虚拟气温曲线：使用余弦函数模拟昼夜温度变化，峰值出现在14:00
  - 热应激阈值优化：将热应激挂钩到阈值（26°C），而非线性时间（10:00-18:00）
  - 静态消耗优化：降低PANDOLF静态系数（从1.5/2.0降到1.2/1.6）
  - 跳跃和攀爬冷却拦截：跳跃冷却2秒，攀爬冷却5秒
- **v2.15.0** - 配置系统
  - 使用[BaseContainerProps]和[Attribute]属性实现自动序列化
  - 使用官方的SCR_JsonLoadContext和SCR_JsonSaveContext
  - 配置文件路径：$profile:RealisticStaminaSystem.json
  - 支持热重载（无需重启服务器）
  - 调试日志门禁系统：零开销门禁、时间间隔控制、工作台模式自动开启调试
  - 配置管理器（SCR_RSS_ConfigManager）：单例模式，自动加载/保存配置
  - 配置类（SCR_RSS_Settings）：包含所有可配置参数（调试、体力、移动、环境、高级、性能）
  - 常量桥接方法（SCR_RSS_Constants.c）：15个静态方法用于获取配置值
- **v2.14.1** - 室内检测系统
  - 使用建筑物边界框检测角色是否在室内
  - 检测逻辑：检查角色位置是否在建筑物的世界坐标边界框内
  - 局限性说明：边界框可能包含建筑物周围的空间，无法区分"在边界框内"和"真正在建筑物内部"
- **v2.14.0** - 高级环境因子系统
  - 精确降雨强度系统：使用GetRainIntensity()API替代字符串匹配
  - 风阻模型：基于风速和风向计算逆风阻力
  - 路面泥泞度系统：基于积水程度计算泥泞惩罚
  - 实时气温热应激模型：使用GetTemperatureAirMinOverride()API
  - 地表湿度和静态恢复惩罚：趴下时受地表湿度影响
  - SCR_RSS_EnvironmentFactor.c - 高级环境因子模块（约1250行）
- **v2.13.0** - 深度生理压制恢复系统
  - 核心概念：从"净增加"改为"代谢净值"
  - 最终恢复率 = (基础恢复率 × 姿态修正) - (负重压制 + 氧债惩罚)
  - 新增呼吸困顿期（RECOVERY_COOLDOWN）：停止运动后5秒内系统完全不处理恢复
  - 新增负重对恢复的静态剥夺机制（LOAD_RECOVERY_PENALTY）：负重越大，恢复越慢
  - 新增边际效应衰减机制（MARGINAL_DECAY）：体力>80%时恢复速度显著降低
  - 新增最低体力阈值限制（MIN_RECOVERY_STAMINA_THRESHOLD）：体力<20%时需要10秒休息才能开始恢复
  - 恢复系统参数全面调整：
    - BASE_RECOVERY_RATE：0.0005 → 0.0003（基础恢复时间从6.6分钟延长到11分钟）
    - RECOVERY_STARTUP_DELAY_SECONDS：1.5 → 5.0（呼吸困顿期从1.5秒延长到5秒）
    - STANDING_RECOVERY_MULTIPLIER：1.0 → 0.4（站姿恢复效率从100%降到40%）
    - CROUCHING_RECOVERY_MULTIPLIER：1.3 → 1.5（蹲姿恢复效率从130%提升到150%）
    - PRONE_RECOVERY_MULTIPLIER：1.7 → 2.2（趴姿恢复效率从170%提升到220%）
  - 更新Python趋势图脚本以反映新的恢复参数
  - 创建深度生理压制恢复系统测试脚本
- **v2.11.0** - 进一步模块化重构和调试信息优化
  - 代码精简：PlayerBase.c 减少 40%（1362行 → 817行）
  - 新增游泳状态管理模块（SCR_RSS_SwimmingState.c）
  - 新增体力更新协调器模块（SCR_RSS_UpdateCoordinator.c）
  - 扩展调试显示模块（SCR_RSS_DebugDisplay.c）
  - 统一调试信息输出接口
- **v2.10.0** - 环境因子系统（热应激和降雨湿重）
- **v2.9.0** - 游泳体力管理完善与游泳速度检测修复
  - 游泳体力管理：引入3D物理阻力/浮力模型，区分水平/垂直代价，包含湿重效应
  - 修复游泳速度始终为0：改为使用 `GetOrigin()` 位置差分测速（游泳命令位移不更新 `GetVelocity()` 的情况下仍可正确取速）
  - 速度显示与体力消耗使用同一套游泳速度输入，调试与体验更一致

- **v2.6.0** - Sprint速度系统优化和跳跃/翻越机制增强
  - 新增趴下休息时的负重优化：当角色趴下休息时，负重影响降至最低（地面支撑装备重量）
  - 新增跳跃冷却机制：3秒冷却时间，防止重复触发
  - 新增连续跳跃惩罚机制（无氧欠债）：3秒内连续跳跃，每次额外增加50%消耗
  - 新增低体力禁用跳跃：体力 < 10% 时禁用跳跃
  - Sprint速度系统优化（统一增量模型）：
    - Sprint速度增量从15%提升到30%，确保负重状态下仍有明显差距
    - Sprint计算完全基于Run的完整逻辑（双稳态-平台期、5秒阻尼过渡等）
    - Sprint负重惩罚系数从20%降为15%（模拟爆发力克服阻力）
    - 重构Sprint计算逻辑，确保始终比Run快30%的固定阶梯
    - 效果：28KG负重下，Run 3.6 m/s vs Sprint 4.7 m/s（差距 1.1 m/s）
  - Sprint体力消耗优化：从2.5倍提升到3.0倍，平衡速度提升带来的优势
  - 跳跃和翻越消耗优化（动态负重倍率）：
    - 跳跃基础消耗从3%提升到3.5%
    - 翻越起始消耗从1.5%提升到2%
    - 使用动态负重倍率：`实际消耗 = 基础消耗 × (currentWeight / 90.0) ^ 1.5`
  - 跳跃和翻越UI交互增强：跳跃后更新Exhaustion信号，触发深呼吸音效和视觉模糊效果

- **v2.5** - Pandolf 模型医疗级扩展和系统优化
  - 新增地形系数系统：根据地面类型动态调整消耗（铺装路面 1.0 → 深雪 2.1-3.0）
  - 新增静态负重站立消耗：负重下站立时减缓恢复速度（基于 Pandolf 静态项公式）
  - 新增 Santee 下坡修正模型：精确处理下坡时的体力消耗（超过 -15% 时需"刹车"）
  - （历史记录）新增 Givoni-Goldman 跑步模式切换：速度 >2.2 m/s 时自动切换到跑步模型
  - 新增恢复启动延迟机制：负重下恢复需静止 3 秒后才生效，防止机制滥用
  - 新增网络同步容差优化：连续偏差累计触发（2 秒容差），速度插值平滑处理
  - 新增疲劳积累系统：超出最大消耗转化为疲劳，降低最大体力上限（最多 30%）
  - 新增 UI 信号桥接系统：让官方 UI 特效（视觉模糊、呼吸声）响应自定义体力系统状态
  - 性能优化：地形检测频率根据移动状态动态调整

- **v2.4** - 累积疲劳和代谢适应系统
  - 新增累积疲劳系统：长时间运动后，相同速度的消耗逐渐增加（基于[Palumbo et al., 2018](https://doi.org/10.1371/journal.pcbi.1006073)）
    - 前5分钟无疲劳累积
    - 之后每分钟增加1.5%消耗（30分钟后增加45%）
    - 最大疲劳因子2.0（消耗最多增加100%）
    - 静止时疲劳快速恢复（恢复速度是累积速度的2倍）
  - 新增代谢适应系统：根据运动强度动态调整能量效率（基于[Palumbo et al., 2018](https://doi.org/10.1371/journal.pcbi.1006073)）
    - 有氧区（<60% VO2max）：效率因子0.9（更高效，主要依赖脂肪）
    - 混合区（60-80% VO2max）：效率因子0.9→1.2（线性插值，糖原+脂肪混合）
    - 无氧区（≥80% VO2max）：效率因子1.2（低效但高功率，主要依赖糖原）
  - 优化综合效率因子：健康状态效率 × 代谢适应效率，更真实地模拟能量代谢
  - 更新所有Python脚本：同步累积疲劳和代谢适应系统到所有趋势图生成脚本

- **v2.8.0** - 深度模块化重构
  - 新增4个核心计算模块（速度计算、体力消耗、体力恢复、调试显示）
  - PlayerBase.c从1554行减少到1283行（减少17%）
  - UpdateSpeedBasedOnStamina函数从990行减少到约500行（减少49%）
  - 解决游戏工作台多行代码支持问题
  - 代码结构更清晰，可维护性显著提升
  - 修复EnforceScript语法兼容性问题

- **v2.7.0** - 代码模块化重构
  - 创建10个模块化组件，提高代码可维护性
  - PlayerBase.c从2037行减少到1464行（减少28%）
  - 所有常量定义和辅助函数提取到独立模块

- **v2.3** - 健康状态系统和多维交互模型
  - 新增健康状态系统：实现[个性化运动建模](https://doi.org/10.1371/journal.pcbi.1006073)（基于Palumbo et al., 2018）
    - 训练有素者（fitness=1.0）能量效率提高18%（基础消耗、速度线性项、速度平方项均减少18%）
    - 训练有素者恢复速度增加25%
    - 角色设置为22岁男性，训练有素水平
  - 改进多维交互模型：添加速度×负重×坡度三维交互项
  - 更新所有Python脚本：同步健康状态系统到所有趋势图生成脚本

- **v2.2** - 移动类型和坡度系统
  - 添加移动类型系统（Idle/Walk/Run/Sprint）
  - 添加Sprint机制（Sprint速度/消耗参数在后续版本持续优化）
  - 添加坡度影响系统（上坡/下坡影响体力消耗）
  - 添加跳跃和翻越体力消耗
  - 增强状态显示（包含移动类型和坡度信息）
  - 增强调试信息（包含更多详细信息）
  - 优化代码结构，注释掉拦截体力系统的调试信息

- **v2.1** - 参数优化版本
  - 优化参数以达到2英里15分27秒目标
  - 速度参数优化以达成2英里15分27秒目标（历史版本参数与当前版本不同）
  - 速度平方项系数从0.0003降低至0.0001
  - 完成时间：925.8秒（15.43分钟），提前1.2秒完成目标 ✅
  - 使用精确数学模型（α=0.6，Pandolf模型）

- **v2.0** - Realistic Stamina System (RSS) - 拟真体力-速度系统
  - 实现动态速度调整系统（根据体力百分比）
  - 实现负重影响系统（负重影响速度）
  - 实现精确数学模型（不使用近似）
  - 实现综合状态显示（速度、体力、倍率）
  - 实现平滑速度过渡机制
  - 实现原生体力系统完全覆盖

- **v1.0** - 初始版本（已废弃）
  - 实现固定速度修改功能（50%）
  - 实现速度显示功能（每秒一次）

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE) 许可证。

## 作者

- **ViVi141**
- Email: 747384120@qq.com

---

**注意：** 本模组修改了游戏的核心速度机制，可能会影响游戏体验。建议在测试环境中使用。
