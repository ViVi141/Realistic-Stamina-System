# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** - 一个结合体力和负重动态调整移动速度的拟真模组，基于精确的医学/生理学模型。

**English**: A realistic stamina and speed system mod for Arma Reforger that dynamically adjusts movement speed based on stamina and encumbrance, using precise medical/physiological models.

- **GUID**: `68649101601CC93D` · **Config**: **6.0.0** · **Game**: Reforger **1.7+**
- 完整中文说明（含历史版本记录与系统特性细则）：[README_CN.md](README_CN.md)
- English overview：[README_EN.md](README_EN.md)

## 作者信息

- **作者**: ViVi141
- **邮箱**: 747384120@qq.com
- **许可证**: [AGPL-3.0](LICENSE)

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE) 许可证。

## 功能说明

本模组用 **代谢功率预算** 驱动有氧体力条、无氧 W′ 储备与移动限速：负重、坡度、环境、姿态会改变功率与恢复，速度由 `SetSpeedLimit` 闭环限制。

**体能锚点**：ACFT（陆军战斗体能测试）22–26 岁男性 2 英里（约 100 分用时参考）。权威公式见 [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)。

### 【v6】核心闭环

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]   // 与灌木/铁丝网等原生减速取 min
```

| 概念 | 说明 |
|------|------|
| 有氧主条 (STA) | 写入引擎 `GetStamina()`；极低 STA（约 &lt;5%）跛行 |
| W′ 焦耳池 | `P &gt; CP` 时放电；对外 `wPrimePool01` |
| Sprint | `invert(min(sprint_cap, CP + W′/Δt))`；门禁 = 有氧阈值 + W′ 施密特闩锁 |
| v_drain | 消耗按 `min(v_meas, appliedLimit)`；超速超额可走 W′ 记账 |
| 已移除 | 25%/35% 意志力平台期；玩家主路径不再用独立 Sprint×3.0 假倍数 |

---

## 数学模型一览

| 模型 | 用途 | 主要位置 |
|------|------|----------|
| **Pandolf (1977)** | 步行 / 低速负重代谢功率 | `SCR_RSS_MetabolismModel` |
| **Santee / Margaria 下坡修正** | 缓下坡省能、陡下坡制动耗能 | MetabolismModel / Constants |
| **ACSM 跑/冲功率** | `P ≈ a + b·v + c·v²`；2.0–2.4 m/s 与 Pandolf **C¹ 混合** | MetabolismModel |
| **Critical Power–W′** | 动态 CP；超额功率以焦耳烧 W′ | `SCR_RSS_CriticalPowerModel` |
| **Skiba 双指数再填充** | Elite：`k_fast` / `k_slow` / `W′_lim` | CriticalPowerModel |
| **线性 W′ 再填充** | Standard / Tactical：`w_prime_recovery_w_per_s` | CriticalPowerModel |
| **功率反演 invert(P)** | 由可用功率反推 Sprint/代谢限速 | DrainCalculator / SpeedCalculator |
| **Tobler 坡度速度** | 上下坡目标速度缩放 + 平滑过渡 | SpeedCalculator / SlopeSpeedTransition |
| **积分疲劳 I(t)** | `dI/dt = w·P − R` → 压 CP / W′ 恢复系数 | `SCR_RSS_FatigueSystem` |
| **EPOC 氧债** | 停跑后延迟消耗 ∝ 峰值意图功率（相对 CP 钳制） | `SCR_RSS_EpocState` |
| **分段负重惩罚** | 装备质量 → 油耗倍率 + 有限速度比 | `SCR_RSS_EncumbranceCache` |
| **动态 CP 修正** | 负重 / 上坡² / 环境 / 疲劳 下调 CP（下坡不加 CP） | CriticalPowerModel |
| **游泳 3D 阻力** | `F_d ∝ ρ·v²·C_d·A` + 垂直功率 + 踩水基载 | `SCR_RSS_SwimmingStaminaModel` |
| **天文气温模型** | 太阳几何 + 日序；短波/长波/云量修正的气温步进；可与引擎气温切换；经纬度可由日出日落估计 | `SCR_RSS_AstronomyMath` / `TemperatureSampler` |
| **热/冷应激平方响应** | 温-风补偿，室内部分豁免（气温输入可来自天文模型） | Environment PenaltyMath |
| **降雨/游泳湿重** | 等效附加质量，衰减曲线 | RainWetWeight / SwimmingState |
| **泥泞滑倒概率** | 泥深 × 速度 × 姿态 → 滑倒/镜头应力 | MudSlip |
| **SmoothStep 撞墙阻尼** | 跌破跛行阈值约 5 s 过渡 | `SCR_RSS_CollapseTransition` |
| **姿态转换成本** | 站/蹲/趴切换窗口与乳酸式姿态疲劳 | StanceTransitionManager |
| **NSGA-II / NSGA-III（Optuna）** | 多目标搜参；v6 四～五目标 minimize（sustain / combat / recovery / param_drift 等）；硬约束失败 **prune** | `rss_pipeline_v6`（对照 `rss_pipeline_v4`） |
| **TPE 分档标量化** | `optimize-tiers`：按 Elite/Standard/Tactical 标量目标 + TPESampler 搜参 | `rss_pipeline_v6 optimize-tiers` |
| **约束优先分层（T0–T3）** | T0 生理锚点硬门禁；T2 九场景软目标；T3 参数漂移；三档从 Pareto 提取 | `rss_constraints_v6` |
| **数字孪生仿真** | 与游戏同序 tick（代谢/CP–W′/疲劳/限速），供约束评估与 VT 曲线 | `rss_digital_twin_fix` / `rss_sim` |
| **ACFT / 生理锚点** | 2 英里与行军剖面标定；`calibrate` + Bake 嵌入 | tools + PresetBake |

**三档预设（示意锚点，以 Bake/JSON 为准）**：Elite / StandardMilsim / TacticalAction — 不同 CP、W′_max、sprint_cap、W′ 恢复律。管线设计见 [docs/RSS_v6_优化管线设计.md](docs/RSS_v6_优化管线设计.md)。

---

## 机制与特性一览

### 代谢与双池

- 陆地 Walk/Run/Sprint 统一走 Pandolf/ACSM 功率 → STA 消耗（`energy_to_stamina_coeff`）
- W′ 仅在 `P &gt; CP`（含裕度规则）放电；`P≈CP` 巡航不回充（防再武装震荡）
- Sprint 门禁：有氧阈值 + W′ 关闭带 / 再开带施密特；短冲不靠时间 CD 锁死
- 超速记账：实测明显快于限速时，超额优先记入 W′
- 生理消耗上限：防止负重×坡度数值爆炸

### 速度与限速

- 相位行军档：Idle / Walk / Run / Sprint 配置目标 m/s
- 负重：主要加「油耗」，速度惩罚有上限（战斗负重仍可短冲）
- 坡度自适应步幅：上坡降目标速；非线性坡度消耗（缓坡轻、陡坡重）
- `SCR_RSS_SpeedBridge`：RSS 限速与原生灌木/障碍减速取 **min**
- 低 STA 跛行倍率 + 撞墙 5 s 阻尼
- 冲刺被拒时速度过渡（SprintBlockSpeedTransition）

### 恢复与疲劳

- 呼吸困顿 / 恢复冷却：停跑后短时不秒回
- 负重恢复惩罚、边际衰减（高 STA 回得慢）、低 STA 最低休息门槛
- 趴下休息：地面支撑减轻负重对恢复的压制
- 健康/fitness：训练水平影响效率与恢复
- 累积疲劳 I(t) + 代谢适应（有氧区更省、无氧区功率高）
- EPOC：峰值功率驱动的氧债尾迹

### 动作与姿态

- 跳跃 / 翻越额外消耗、连续跳惩罚、冷却、低 STA 禁跳
- 姿态转换成本与姿态疲劳叠加
- 载具 / 游泳分支独立结算

### 环境

- 气温：引擎读数或 **天文气温模型**（太阳几何 / 短波·长波·云量步进）；室内检测
- 热应激时段峰值、室内减免、抑制恢复
- 降雨湿重（强度分档 + 雨停衰减）、风阻
- 地形材质 / 泥泞度、地表湿度
- 游泳上岸湿重衰减

### 表现与物品

- Stamina HUD（右上状态条、回满 ETA 等）
- 可选自定义表现（默认可仅原生）：冲刺 FOV、泥泞镜头、CombatStim 屏效、冲刺发闷音
- CSB 战术兴奋针状态机；吗啡等消耗品对接
- 外部 API：`SCR_RSS_API`（`wPrimePool01` 等）

### AI / 网络 / 配置

- AI：状态机 + SpeedCap（与玩家同源相位曲线）+ 意图过滤 / 战伤链接 / 群组休整；可关全量或仅关体力计算
- 服务端权威配置 + RplProp；客户端体力上报与校验限流
- 管理员 Settings 页：三档预设 / Custom、HUD/Debug/泥泞/AI 开关
- 配置版本 `6.0.0`（不跨大版本自动迁移）

### 工具链

- Python/Rust 数字孪生；`rss_pipeline_v6`：**NSGA-II/III** 多目标 + **TPE** 分档、`validate` 硬约束 prune、`calibrate` / Bake
- JSON → `SettingsPresetBake` 嵌入

---

## 项目结构

```
RealisticStaminaSystem/
├── LICENSE                               # AGPL-3.0 许可证
├── README.md                             # 项目说明文档
├── AUTHORS.md                            # 作者信息
├── CONTRIBUTING.md                       # 贡献指南
├── CHANGELOG.md                          # 更新日志
├── .gitignore                            # Git 忽略文件配置
├── addon.gproj                           # Arma Reforger 项目配置文件
├── Configs/                              # 实体目录配置
│   └── EntityCatalog/                    # 军火库条目登记
├── scripts/                              # EnforceScript 脚本目录
│   └── Game/
│       ├── Integration/                  # modded 入口层（高冲突面集中管理）
│       │   ├── PlayerBase.c              # 主控制器（modded SCR_CharacterControllerComponent）
│       │   ├── SCR_StaminaOverride.c     # 体力系统覆盖（modded SCR_CharacterStaminaComponent）
│       │   ├── SCR_RSS_ServerBootstrap.c # 服务端引导（modded SCR_BaseGameMode）
│       │   ├── SCR_InventoryStorageManagerComponent_Override.c # 库存覆盖
│       │   └── SCR_PlayerBaseIntegrationHelpers.c # 集成辅助
│       ├── RSS/                          # 模组主体（6 领域分层）
│       │   ├── Core/                     # 体力核心（13 文件）
│       │   │   ├── SCR_RealisticStaminaSystem.c  # 双稳态速度模型＋Pandolf 能量公式
│       │   │   ├── SCR_RSS_UpdateCoordinator.c # 每 0.2s 主更新协调器
│       │   │   ├── SCR_SpeedCalculation.c        # 综合速度／Tobler 坡度／战术冲刺
│       │   │   ├── SCR_StaminaConsumption.c      # 体力消耗计算
│       │   │   ├── SCR_StaminaRecovery.c         # 体力恢复计算
│       │   │   ├── SCR_FatigueSystem.c           # 累积疲劳模型
│       │   │   ├── SCR_EpocState.c               # EPOC 状态管理
│       │   │   ├── SCR_ExerciseTracking.c        # 运动／休息时间累计跟踪
│       │   │   ├── SCR_EncumbranceCache.c        # 负重缓存（分段非线性惩罚）
│       │   │   ├── SCR_CollapseTransition.c      # 5s 阻尼过渡（25% 临界点）
│       │   │   ├── SCR_StanceTransitionManager.c # 姿态转换成本与窗口结算
│       │   │   ├── SCR_RSS_Constants.c        # 全系统常量与日志节流
│       │   │   └── SCR_StaminaHelpers.c          # 辅助函数
│       │   ├── Environment/              # 环境系统（9 文件）
│       │   │   ├── SCR_RSS_EnvironmentFactor.c       # 热应激／冷应激／降雨湿重／风阻／泥泞
│       │   │   ├── SCR_EnvironmentAstronomyMath.c # 天文／太阳几何
│       │   │   ├── SCR_EnvironmentPenaltyMath.c  # 温-风补偿（冷热对称平方响应）
│       │   │   ├── SCR_EnvironmentWeatherApi.c   # 天气 API 读取／风阻计算
│       │   │   ├── SCR_TerrainDetection.c        # 地形类型检测（AI LOD 分档）
│       │   │   ├── SCR_MaterialTerrainTable.c    # 地形材质映射表
│       │   │   ├── SCR_SlopeSpeedTransition.c    # 坡度速度 5s 平滑过渡
│       │   │   ├── SCR_RSS_SwimmingState.c           # 3D 游泳物理模型
│       │   │   └── SCR_RSS_JumpVaultDetection.c      # 跳跃／翻越检测
│       │   ├── AI/                      # AI 体力系统（7 文件）
│       │   │   ├── SCR_RSS_AIStaminaState.c           # 6-state stamina FSM
│       │   │   ├── SCR_RSS_AISpeedCap.c               # movement speed cap
│       │   │   ├── SCR_RSS_AIIntentFilter.c           # intent filter
│       │   │   ├── SCR_RSS_AICombatDecay.c            # combat decay
│       │   │   ├── SCR_RSS_AIGroupSync.c              # group waypoints / rest
│       │   │   ├── SCR_RSS_AIGroupStaminaProxy.c      # distant group proxy
│       │   │   └── SCR_RSS_AIInjuryLink.c             # injury-stamina link
│       │   ├── Integration/
│       │   │   └── SCR_AIGroup_RSS.c                  # modded SCR_AIGroup hooks
│       │   ├── NetworkConfig/           # 网络与配置（7 文件）
│       │   │   ├── SCR_RSS_NetworkSyncManager.c                 # 客户端上报／服务端权威校验／三层限流
│       │   │   ├── SCR_RSS_ConfigManager.c           # 单例配置管理器（版本标记／热重载）
│       │   │   ├── SCR_RSS_Settings.c                # 可序列化配置类（41+ 参数，3 预设）
│       │   │   ├── SCR_RSS_ConfigSyncUtils.c         # 配置同步工具
│       │   │   ├── SCR_RSS_API.c                     # 外部模组 API
│       │   │   ├── SCR_RSS_DataExport.c              # 数据导出
│       │   │   └── SCR_RSS_PlayerBaseConfigArrays.c  # 配置数组桥接
│       │   ├── MudSlip/                 # 泥泞滑倒（2 文件）
│       │   │   ├── SCR_RSS_MudSlipEffects.c              # 泥泞滑倒效果
│       │   │   └── SCR_RSS_MudSlipRunner.c           # 玩家侧泥泞滑倒推进器
│       │   └── Presentation/            # HUD 与表现（8 文件）
│       │       ├── SCR_RSS_StaminaHUDComponent.c         # 右上角实时状态条
│       │       ├── SCR_RSS_UISignalBridge.c              # 官方 UI 信号桥接
│       │       ├── SCR_RSS_DebugDisplay.c                # 调试信息输出
│       │       ├── CharacterCamera1stPerson.c        # 第一人称镜头惯性／头部物理
│       │       ├── SCR_RSS_PresentationBridge.c      # 表现桥接
│       │       ├── SCR_DesaturationEffect_CombatStimOD.c
│       │       ├── SCR_NoiseFilterEffect_Stamina.c
│       │       └── SCR_RegenerationScreenEffect_CombatStim.c
│       ├── Components/                 # 游戏组件
│       │   └── Gadgets/                # 战术物品（5 文件）
│       │       ├── SCR_CombatStimConstants.c         # CSB 兴奋剂常量
│       │       ├── SCR_CombatStimStateMachine.c      # CSB 状态机
│       │       ├── SCR_ConsumableCombatStimInjector.c # CSB 注射器
│       │       ├── SCR_ConsumableCustomInjector.c    # 自定义注射器
│       │       └── SCR_ConsumableMorphine.c          # 吗啡消耗品
│       ├── UserActions/                # 用户动作（3 文件）
│       │   ├── SCR_CombatStimUserAction.c
│       │   ├── SCR_CustomInjectorUserAction.c
│       │   └── SCR_MorphineUserAction.c
│       └── Damage/                     # 伤害效果
│           └── DamageEffects/
│               └── CharacterDamageEffects/
│                   ├── SCR_CustomInjectorDamageEffect.c
│                   └── SCR_MorphineDamageEffect.c
├── UI/                                   # UI布局文件
│   └── layouts/
│       └── HUD/
│           └── StatsPanel/
│               └── StaminaHUD.layout     # 状态HUD布局
├── Prefabs/                              # 预制体文件
│   ├── Characters/
│   │   └── Core/
│   │       └── Character_Base.et         # 角色基础预制体
│   └── Items/
│       └── Medicine/
│           └── CombatStimInjection_01/   # CSB 注射器预制体
└── tools/                                # v4/v6 优化管道（Python）+ Rust v6 入口
    ├── rss_pipeline_v4.py                # Optuna v4 主入口
    ├── rss_pipeline_v6.py                # v6 validate/calibrate/optimize 主入口
    ├── rust_pipeline_v6/                 # Rust CLI 入口（Phase-A 双跑）
    ├── rss_digital_twin_fix.py           # 数字孪生（供管线引用）
    ├── embed_json_to_c.py                # JSON → C 嵌入（可选）
    ├── test_v4_smoke.py                  # v4 烟雾测试（无 Optuna）
    ├── test_v6_smoke.py                  # v6 合约/生理锚点烟雾测试
    ├── quick_verify.py                   # 少量 trial 快速跑通
    ├── requirements.txt                  # numpy、optuna
    ├── optimized_rss_config_*_v4.json    # v4 三份预设
    ├── optimized_rss_config_*_v6.json    # v6 优化产物
    └── README.md                         # 工具说明
```

## 版本更新 / Version Updates

### 从 3.23.1 → 6.0.0（跨大版本）

对 Workshop / 从稳定版 **3.23.1** 直接升级的玩家与服主：中间曾有 **5.0.0**（双池铺路），对外发布以 **6.0.0** 为准。配置版本跳到 `6.0.0`，**不跨大版本自动迁移**；请用游戏内预设重新选档，或导入 `tools/optimized_rss_config_*_v6.json`。

#### 核心模型（相对 3.23.1）

| 主题 | 3.23.1 | 6.0.0 |
|------|--------|-------|
| 主模型 | Pandolf 为主 + 意志力平台期速度曲线 | Pandolf + **ACSM** → **动态 CP** → **W′ 焦耳池** |
| Sprint | 有氧阈值 + 时间/冷却语义 | `invert(P_available)`；有氧阈值 + **W′ 施密特闩锁**（非短冲时间 CD） |
| 速度曲线 | 双稳态 / 平台期恒速 Run | **相位行军档** m/s；低 STA 仅跛行（**已移除平台期**） |
| 消耗测速 | 易与限速脱节 | **v_drain** = `min(v_meas, appliedLimit)`，与 `SetSpeedLimit` 闭环 |
| 灌木减速 | SpeedBridge 与原生取 min | **保留**（`SCR_RSS_SpeedBridge`） |

#### 相对 3.23.1 的主要变更

**新增 / 重构**

- **CP–W′**：`SCR_RSS_MetabolismModel`、`SCR_RSS_CriticalPowerModel`；Elite **Skiba** 双指数再填充，Standard/Tactical **线性** W′ 恢复
- **双池语义落地**：有氧 STA 主条 + W′（对外 `wPrimePool01`）；联机 RplProp 同步储备
- **积分疲劳 I(t)** 与 **EPOC**（峰值意图功率驱动氧债）
- **撞墙 5 s 阻尼**、Sprint 被拒速度过渡、代谢强制降速（功率反演限速）
- **AI**：`SCR_RSS_AISpeedCap` 与玩家同源 v6 相位曲线；编排入口 `SCR_RSS_AIManager`
- **工具**：`rss_pipeline_v6`、Python/Rust 孪生、`test_v6_smoke` / ACFT / 生理锚点；预设 Bake 嵌入
- **命名**：领域类统一 `SCR_RSS_*`；Integration 拆分（`PlayerBase_UpdateLoop` 等）

**移除 / 废弃（升级注意）**

- 意志力平台期（约 25%/35% 恒速 Run）
- 玩家主路径独立 Sprint×固定倍数假消耗（改为功率预算）
- 旧 Givoni 分支 / 部分 Legacy 消耗桩
- 配置键与语义变化（如 `anaerobicPercent` → `wPrimePool01`）；Custom JSON **勿**直接当 v6 用

**仍保留（3.23.1 能力）**

- 1.7+ 灌木/铁丝网与 RSS 限速合并  
- 环境（热/雨/风/泥/室内）、游泳、跳跃翻越、泥泞滑倒（可关）  
- HUD / 管理员 Settings / CSB / 外部 API（字段已对齐 v6）

完整逐条记录：[CHANGELOG.md](CHANGELOG.md) **[6.0.0]**、**[5.0.0]**、**[3.23.1]**。机制清单见上文「数学模型 / 机制与特性」。

### 更早版本摘要

- **v3.23.0** — AI 子系统、v4 管线  
- **v3.22.x** — 表现原生开关、崩溃防护、设置面板等  

更多详情：[CHANGELOG.md](CHANGELOG.md) · 长篇中文说明：[README_CN.md](README_CN.md)

## Tools 模块 - 多目标优化系统 / Tools - Multi-objective Optimization

RSS Tools：NSGA-II / Optuna，在拟真与可玩性之间搜参（v6 入口 `rss_pipeline_v6.py`）。

| 服务器类型 | 推荐配置 |
|----------|----------|
| 硬核 Milsim | Elite / 拟真优先 |
| 公共服 | StandardMilsim（默认） |
| 休闲 | Tactical / 可玩性优先 |

说明：[tools/README.md](tools/README.md) · 管线设计：[docs/RSS_v6_优化管线设计.md](docs/RSS_v6_优化管线设计.md)

## 安装方法

1. 将整个 `RealisticStaminaSystem` 文件夹复制到 Arma Reforger 工作台的 `addons` 目录
2. 在 Arma Reforger 工作台中打开项目
3. 编译模组
4. 在游戏中选择并启用此模组

## 使用方法

1. 启动游戏并加载模组
2. 系统自动监控体力、W′、负重与环境
3. 移动速度由代谢限速与原生减速合并决定
4. 可选开启 Stamina HUD / Debug

## 调整系统参数

核心常量：`scripts/Game/RSS/Core/SCR_RSS_Constants.c`  
可调预设：`scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c` / `SCR_RSS_SettingsPresetBake.c`  
离线优化产物：`tools/optimized_rss_config_*_v6.json`

## 技术亮点

- **科学性**：Pandolf + ACSM + Critical Power–W′（Skiba / 线性再填充）
- **闭环**：消耗测速与 `SetSpeedLimit` 同源（v_drain）
- **可标定**：数字孪生 + 生理锚点 / ACFT 烟雾测试
- **可运营**：三档预设 + 管理员 Settings + 外部 API