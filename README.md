# Realistic Stamina System (RSS) v3.22.6

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-3.22.6-brightgreen)](CHANGELOG.md)

**Realistic Stamina System (RSS)** - 一个结合体力和负重动态调整移动速度的拟真模组，基于精确的医学/生理学模型。

**English**: A realistic stamina and speed system mod for Arma Reforger that dynamically adjusts movement speed based on stamina and encumbrance, using precise medical/physiological models.

## 作者信息

- **作者**: ViVi141
- **邮箱**: 747384120@qq.com
- **许可证**: [AGPL-3.0](LICENSE)

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE) 许可证。

## 功能说明

本模组根据玩家的体力值和负重动态调整移动速度，实现更真实的游戏体验。当体力充沛时，玩家可以全速移动；当体力下降时，移动速度会逐渐减慢。同时，负重也会影响移动速度。

**体力标准参考**：本模组的体力标准引用自 **ACFT (Army Combat Fitness Test)** 美国陆军战斗体能测试中22-26岁男性2英里测试100分用时15分27秒。

## 主要特性

- ✅ **双稳态-应激性能模型**：平台期（25%-100%体力）保持恒定速度，衰减期（0%-25%）平滑下降
- ✅ **5秒阻尼过渡（"撞墙"瞬间优化）**：在体力触及25%临界点时，使用5秒时间阻尼过渡
- ✅ **坡度自适应步幅逻辑**：上坡时自动降低目标速度（坡度-速度负反馈）
- ✅ **非线性坡度消耗**：小坡几乎无感，陡坡才真正吃力
- ✅ **生理上限保护**：体力消耗有生理上限，防止数值爆炸
- ✅ **动态速度调整**：根据体力百分比动态调整移动速度
- ✅ **负重影响系统**：负重主要影响"油耗"（体力消耗）而非直接降低"最高档位"（速度）
- ✅ **移动类型系统**：支持Idle/Walk/Run/Sprint四种移动类型
- ✅ **坡度影响系统**：上坡/下坡会影响体力消耗
- ✅ **跳跃/翻越体力消耗**：跳跃和翻越动作会消耗额外体力，包含连续跳跃惩罚机制
- ✅ **Sprint机制**：Sprint速度比Run快30%，但体力消耗增加3.0倍
- ✅ **健康状态系统**：训练有素者能量效率和恢复速度更高
- ✅ **累积疲劳系统**：长时间运动后，相同速度的消耗逐渐增加
- ✅ **代谢适应系统**：根据运动强度动态调整能量效率
- ✅ **深度生理压制恢复系统**：包含呼吸困顿期、负重恢复惩罚、边际效应衰减等机制
- ✅ **环境因子系统**：热应激、降雨湿重、风阻、泥泞度、气温、地表湿度系统
- ✅ **实时状态 HUD 显示**：屏幕右上角紧凑状态条，显示10项关键信息
- ✅ **配置版本管理**：自动检测并迁移旧版本配置，保留用户设置
- ✅ **游泳体力管理**：3D物理模型，包含水平阻力、垂直上浮/下潜功率、静态踩水功率

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
│       │   │   ├── SCR_StaminaUpdateCoordinator.c # 每 0.2s 主更新协调器
│       │   │   ├── SCR_SpeedCalculation.c        # 综合速度／Tobler 坡度／战术冲刺
│       │   │   ├── SCR_StaminaConsumption.c      # 体力消耗计算
│       │   │   ├── SCR_StaminaRecovery.c         # 体力恢复计算
│       │   │   ├── SCR_FatigueSystem.c           # 累积疲劳模型
│       │   │   ├── SCR_EpocState.c               # EPOC 状态管理
│       │   │   ├── SCR_ExerciseTracking.c        # 运动／休息时间累计跟踪
│       │   │   ├── SCR_EncumbranceCache.c        # 负重缓存（分段非线性惩罚）
│       │   │   ├── SCR_CollapseTransition.c      # 5s 阻尼过渡（25% 临界点）
│       │   │   ├── SCR_StanceTransitionManager.c # 姿态转换成本与窗口结算
│       │   │   ├── SCR_StaminaConstants.c        # 全系统常量与日志节流
│       │   │   └── SCR_StaminaHelpers.c          # 辅助函数
│       │   ├── Environment/              # 环境系统（9 文件）
│       │   │   ├── SCR_EnvironmentFactor.c       # 热应激／冷应激／降雨湿重／风阻／泥泞
│       │   │   ├── SCR_EnvironmentAstronomyMath.c # 天文／太阳几何
│       │   │   ├── SCR_EnvironmentPenaltyMath.c  # 温-风补偿（冷热对称平方响应）
│       │   │   ├── SCR_EnvironmentWeatherApi.c   # 天气 API 读取／风阻计算
│       │   │   ├── SCR_TerrainDetection.c        # 地形类型检测（AI LOD 分档）
│       │   │   ├── SCR_MaterialTerrainTable.c    # 地形材质映射表
│       │   │   ├── SCR_SlopeSpeedTransition.c    # 坡度速度 5s 平滑过渡
│       │   │   ├── SCR_SwimmingState.c           # 3D 游泳物理模型
│       │   │   └── SCR_JumpVaultDetection.c      # 跳跃／翻越检测
│       │   ├── AI/                      # AI 体力系统（7 文件）
│       │   │   ├── SCR_RSS_AIStaminaBridge.c          # AI 体力桥接
│       │   │   ├── SCR_RSS_AIGroupStaminaProxy.c      # 群组代理（远距仅队长全量计算）
│       │   │   ├── SCR_RSS_AIStaminaCombatEffects.c   # 体力对 AI 射击精度／射速影响
│       │   │   ├── SCR_RSS_AIGroupRestCoordinator.c   # AI 群组休息协调
│       │   │   ├── SCR_RSS_AIRestRecoveryRegistry.c   # AI 恢复注册表
│       │   │   ├── SCR_RSS_AICoverSeeker.c            # AI 掩体寻找
│       │   │   └── SCR_RSS_AIMudSlipPolicy.c          # AI 泥泞滑倒策略
│       │   ├── NetworkConfig/           # 网络与配置（7 文件）
│       │   │   ├── SCR_NetworkSync.c                 # 客户端上报／服务端权威校验／三层限流
│       │   │   ├── SCR_RSS_ConfigManager.c           # 单例配置管理器（版本迁移／热重载）
│       │   │   ├── SCR_RSS_Settings.c                # 可序列化配置类（41+ 参数，3 预设）
│       │   │   ├── SCR_RSS_ConfigSyncUtils.c         # 配置同步工具
│       │   │   ├── SCR_RSS_API.c                     # 外部模组 API
│       │   │   ├── SCR_RSS_DataExport.c              # 数据导出
│       │   │   └── SCR_RSS_PlayerBaseConfigArrays.c  # 配置数组桥接
│       │   ├── MudSlip/                 # 泥泞滑倒（2 文件）
│       │   │   ├── SCR_MudSlipEffects.c              # 泥泞滑倒效果
│       │   │   └── SCR_RSS_MudSlipRunner.c           # 玩家侧泥泞滑倒推进器
│       │   └── Presentation/            # HUD 与表现（8 文件）
│       │       ├── SCR_StaminaHUDComponent.c         # 右上角实时状态条
│       │       ├── SCR_UISignalBridge.c              # 官方 UI 信号桥接
│       │       ├── SCR_DebugDisplay.c                # 调试信息输出
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
└── tools/                                # 开发工具和脚本
    ├── rss_super_pipeline.py              # NSGA-II 主优化管道
    ├── rss_digital_twin_fix.py            # 数字孪生仿真器
    ├── stamina_constants.py               # 常数定义工具库
    ├── requirements.txt                   # Python依赖包
    ├── optimized_rss_config_*.json        # 优化后的配置文件（3个预设）
    └── README.md                          # Tools模块完整使用指南
```

## 系统特性

### 体力-速度关系

系统会根据体力百分比动态调整速度，实现以下效果：

1. **体力充沛时（75-100%）**：
   - 速度恒定，适合长时间奔跑

2. **体力中等时（50-75%）**：
   - 仍保持目标速度，开始感到疲劳

3. **体力偏低时（25-50%）**：
   - 仍保持目标速度，需要休息恢复体力

4. **精疲力尽时（0%-25%）**：
   - 速度平滑过渡到跛行速度，强烈建议休息

### 负重-速度关系

负重系统会根据携带物品的重量影响移动速度：

- **无负重（0 kg）**：无影响
- **轻度负重（0-15 kg）**：轻微影响，速度减少 < 7.5%
- **中度负重（15-30 kg）**：明显影响，速度减少 7.5-15%
- **战斗负重阈值（30 kg）**：达到战斗负重阈值，可能影响战斗表现
- **重度负重（30-40.5 kg）**：严重影响，速度减少 15-20%
- **最大负重（40.5 kg）**：达到最大负重，速度最多减少 20%

### 移动类型系统

系统支持四种陆地移动类型，每种类型有不同的速度特性和体力消耗：

#### 1. Idle（静止）
- **速度倍数**：0.0（完全静止）
- **体力消耗**：无（静止时恢复体力）
- **适用场景**：站立、蹲伏、休息

#### 2. Walk（行走）
- **速度倍数**：Run速度 × 0.7（约为Run的70%）
- **体力消耗**：低
- **适用场景**：正常移动、探索、节省体力

#### 3. Run（跑步）
- **速度倍数**：基础速度 × (1 - 负重惩罚)
- **体力消耗**：中等
- **适用场景**：正常行军、长距离移动

#### 4. Sprint（冲刺）
- **速度倍数**：Run速度 × 1.30（比Run快30%）
- **体力消耗**：高
- **适用场景**：追击、逃命、短距离冲刺

## 版本更新 / Version Updates

**v3.22.6** - 2026-05-10

表现层：`RSS_PRESENTATION_NATIVE_ONLY`（默认开启）仅保留原生相机/屏效，屏蔽 RSS 自定义；冲刺 FOV 滞回与平滑、泥泞关闭时镜头应力立即清零。详见 [CHANGELOG.md](CHANGELOG.md) **[3.22.6]**。

**v3.22.5** - 2026-05-09

热修复：退出游戏 / 断线时客户端对空指针读取导致的崩溃（`RSS_WaitForGameModeConfig` 与 GameMode `CallLater` 清理、HUD 在 Workspace 已销毁时的防护）；Workbench 中重载脚本/世界时清空环境信号与 HUD 等静态缓存，避免悬空 `GameSignalsManager`。详见 [CHANGELOG.md](CHANGELOG.md) **[3.22.5]**。

**v3.22.1** - 2026-05-08

本版本包含自 v3.21.0 以来的 54 个提交，是迄今为止规模最大的单次更新。

### 🌟 游戏内管理员设置面板
- **Settings 集成** — RSS 管理员面板作为系统 Settings 的独立 Tab（与 Video/Audio/Interface 并列），仅管理员可见
- **预设与开关** — 支持 EliteStandard / StandardMilSim / TacticalAction 预设切换；Debug 日志、HUD、数据导出、泥泞滑倒、AI 战斗效果、AI 完全禁用、AI 体力计算禁用等开关
- **配置即时生效** — 修改后立即保存 JSON 并通过 GameMode RplProp 轻量复制到所有客户端（仅传预设名+开关，47 参数仅在 Custom 模式传输）
- **本地 HUD 开关** — 所有玩家可在 Settings 中独立开关自己的 HUD，不依赖服务器设置
- **持久化** — 每次修改自动保存 JSON，重启后保留

### 🤖 AI 控制增强
- **完全禁用 AI RSS** — 新增 `Disable AI RSS (All)` 开关，将体力完全交还引擎处理
- **仅禁用 AI 体力计算** — 新增 `Disable AI Stamina (Speed)` 开关，保留 RSS 速度倍率但跳过消耗/恢复
- **AI 群组休息协调** — 低体力时以群组为单位自动休整：队长寻隐蔽点、插入动态防守路点、全员体力达标后自动完成

### ⚡ 性能优化
- **AI 桥方法 500ms 节流** — 高密度 AI 场景下 `FindComponent`/`Cast` 开销降低约 60%
- **引擎信号系统** — `EnvironmentFactor` 改用 `GlobalSignalsManager` 静态信号索引（全实例共享，仅首次注册），替代每实体 C++ 桥接调用
- **环境检测间隔调整** — `ENV_CHECK_INTERVAL` 5→10s，室内检测 1→2s，负重缓存 0.2→0.5s
- **AI 距离 LOD** — 近距刷新间隔 100→200ms
- **云因子缓存** — 30s TTL，避免每 5s 字符串匹配
- **纬度缓存** — 地图常数初始化后永不变化，减少 `GetCurrentLatitude()` 调用
- **随机相位偏移** — 环境检测起始时间随机化，避免多实体在同帧集中触发

### 🐞 关键修复
- **角色实体删除崩溃** — 修复删除玩家/AI 实体后立即崩溃的问题。根因：`ce2fb4a` 引入无条件 `GetDamageManager()` 每 tick 调用 + 缺少析构函数清理 `CallLater` 回调。修复：添加 `~SCR_CharacterControllerComponent()` 析构函数，调用 `Callqueue.Remove()` 取消 8 个待执行回调 + `ActionListener` 移除 + `m_bIsDeleted` 守卫
- **`SCR_CharacterStaminaComponent` 析构** — 添加析构函数调用 `StopStaminaMonitor()` 取消 `MonitorStamina` 循环
- **库存组件空指针** — `OnItemRemoved`/`OnItemAdded` 中 `GetOwner()` 增加空检查

### 🔧 文件体积合规
- **EnforceScript 65535 字节限制** — 拆分 `PlayerBase.c`（载具辅助）、`RealisticStaminaSystem.c`（游泳模型）、`SCR_RSS_Settings.c`（Params 数据类）为独立文件

### 📊 v4 优化器参数
- 加载 v4 NSGA-II 优化管道产出的 EliteStandard 参数到 C 静态常量
- 修复关键 `RSSConstants` 参数 bug + 极端场景修复 + 去重

### 🏗️ 代码质量
- 移除 `SettingsBindingBase` 依赖、`ScriptCaller`（EnforceScript 无泛型委托）
- 三元运算符替换为 if/else（EnforceScript 不支持 `?:`）
- 内联单行展开为多行（解析器限制）
- GUID 补零前缀 + 唯一 slot GUID
- 移除重复 `OnTabHide` 声明

### 🔄 v3.21.1 变更（已并入）
- CSB 20% 咖啡因注射器（替代旧版肾上腺素兴奋针）
- NONE→DELAY→ACTIVE 阶段模型
- 移除全屏 HUD 叠层特效

**v3.12.0** - 2026-02-09

本版本仅记录 C 脚本层面的变化。

### ✅ 新增
- **环境温度物理模型** - 新增温度步进、短波/长波与云量修正、太阳/日出日落与月相推断，支持引擎温度或模组模型切换（scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c）
- **室内检测增强** - 增加向上射线与水平封闭检测，降低开放屋顶/天窗误判（scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c）
- **配置变更同步链路** - 新增监听器注册、变更检测与全量参数/设置数组同步与广播（scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c、scripts/Game/Components/Stamina/SCR_RSS_Settings.c、scripts/Game/PlayerBase.c）
- **网络校验与平滑** - 客户端上报体力/负重，服务器权威校验并下发速度倍率，含重连延迟同步（scripts/Game/PlayerBase.c、scripts/Game/Components/Stamina/SCR_NetworkSync.c）
- **日志节流工具** - 统一 Debug/Verbose 日志节流接口（scripts/Game/Components/Stamina/SCR_StaminaConstants.c）

### 🔁 变更
- **服务器权威配置** - 客户端不再写入 JSON，仅内存默认值等待同步；服务器写盘并增加备份/修复流程（scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c）
- **移动相位驱动消耗** - 优先以移动相位/冲刺状态决定 Pandolf 路径，并提供服务端权威速度倍数计算接口（scripts/Game/Components/Stamina/SCR_StaminaUpdateCoordinator.c）。Givoni 模型已弃用。
- **负重参数约束** - 新增负重惩罚指数/上限并对预设进行 clamp（scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c、scripts/Game/Components/Stamina/SCR_RSS_Settings.c、scripts/Game/Components/Stamina/SCR_StaminaConstants.c）
- **预设参数刷新** - Elite/Standard/Tactical 预设全面更新，并补充天气模型顶层默认值（scripts/Game/Components/Stamina/SCR_RSS_Settings.c）
- **冲刺消耗默认值** - Sprint 消耗倍数默认改为 3.5，支持配置覆盖（scripts/Game/Components/Stamina/SCR_RSS_Settings.c、scripts/Game/Components/Stamina/SCR_StaminaConstants.c）
- **体重参与消耗** - 体力消耗输入改为“装备+身体”的总重，并优化调试输出（scripts/Game/PlayerBase.c）

### 🐞 修复
- **室内判定误报** - 通过屋顶射线与水平封闭检测降低开放屋顶/天窗误判（scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c）
- **引擎温度极值退化** - 当日最小/最大温度趋同会回退到物理/模拟估算，避免温度异常（scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c）
- **客户端写盘覆盖** - 客户端不再写入本地 JSON，避免覆盖服务器配置（scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c）

**v3.11.1** - 2026-02-02

### 🔧 配置修复与优化
- ✅ **JSON 配置覆盖修复** - 用户修改的 hint/debug 等不再被覆盖
- ✅ **ValidateSettings 修正** - 参数越界时仅 clamp，不清空全部配置
- ✅ **Custom 预设大小写** - 支持 "custom" / "CUSTOM" 识别
- ✅ **HUD 默认关闭** - 首次下载默认不显示 HUD；已有 JSON 的老用户 HUD 按原设置保持
- ✅ **配置可读性** - 常量提取、描述精简、分组优化

**v3.11.0** - 2026-01-26

### 🌟 核心功能更新 / Core Functionality Updates
- ✅ **Stamina system optimization** - 优化体力系统响应速度和起步体验
- ✅ **Stamina system fix** - 修复体力系统的高频监听和速度计算问题
- ✅ **Encumbrance system enhancement** - 实现库存变更时实时更新负重缓存
- ✅ **Encumbrance calculation fix** - 修复武器重量未计入总负重的问题
- ✅ **Environment awareness enhancement** - 添加室内环境忽略坡度影响功能
- ✅ **Configuration management optimization** - 修复预设配置逻辑，确保系统预设值保持最新
- ✅ **Parameter optimization** - 优化RSS系统参数并调整配置文件路径

### 📁 项目清理与优化 / Project Cleanup and Optimization
- ✅ **Project cleanup** - 删除所有生成的 PNG 图表文件
- ✅ **Tools directory optimization** - 仅保留核心 NSGA-II 优化管道，删除过时脚本
- ✅ **Configuration file update** - 移除旧的优化配置文件和更新相关路径

### 📚 文档完善 / Documentation Improvement
- ✅ **Toolset documentation** - 新增 tools/README.md - 完整工具集文档
- ✅ **Configuration verification report** - 新增 CONFIG_APPLICATION_VERIFICATION.md - 配置应用验证报告
- ✅ **Switch verification report** - 新增 DEBUG_AND_HINT_SWITCH_VERIFICATION.md - 开关验证报告

### 🎯 版本整并 / Version Consolidation
- ✅ **Version unification** - 将从提交 d1ebb9c 到现在的所有变更作为 v3.11.0

更多详情请参考 [CHANGELOG.md](CHANGELOG.md) / For more details, see [CHANGELOG.md](CHANGELOG.md)

## Tools 模块 - 多目标优化系统 / Tools - Multi-objective Optimization

RSS Tools 是一个完整的 NSGA-II 多目标优化系统，用于平衡"拟真度"与"可玩性"。

### 配置方案推荐

| 服务器类型 | 推荐配置 | 说明 |
|----------|----------|------|
| 硬核 Milsim 服务器 | 拟真优先配置 | 追求医学精确度 |
| 公共服务器 | 平衡型配置 | 平衡拟真与可玩性 |
| 休闲服务器 | 可玩性优先配置 | 降低体力系统负担 |

### 详细文档

完整的 Tools 模块使用指南请参考：[tools/README.md](tools/README.md)

## 安装方法

1. 将整个 `RealisticStaminaSystem` 文件夹复制到 Arma Reforger 工作台的 `addons` 目录
2. 在 Arma Reforger 工作台中打开项目
3. 编译模组
4. 在游戏中选择并启用此模组

## 使用方法

1. 启动游戏并加载模组
2. 系统会自动监控体力值和负重
3. 移动速度会根据体力百分比和负重自动调整
4. 状态信息会每秒一次输出到控制台

## 调整系统参数

当前版本的核心参数集中在 `scripts/Game/RSS/Core/SCR_StaminaConstants.c` 和 `scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c` 中。

## 技术亮点

- **科学性**：基于医学模型（Pandolf；Givoni-Goldman 列为历史/兼容参考）
- **效率**：200 次采样 vs 5000 次评估（25x 更快）
- **可解释性**：参数敏感度分析
- **灵活性**：3 种优化配置方案
- **工业级**：使用 Optuna 贝叶斯优化
