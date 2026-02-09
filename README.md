# Realistic Stamina System (RSS) v3.12.0

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-3.12.0-brightgreen)](CHANGELOG.md)

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
├── scripts/                              # EnforceScript 脚本目录
│   └── Game/
│       ├── PlayerBase.c                  # 主控制器组件
│       └── Components/
│           └── Stamina/
│               ├── SCR_RealisticStaminaSystem.c  # 体力-速度系统核心
│               ├── SCR_StaminaOverride.c          # 体力系统覆盖
│               ├── SCR_SwimmingState.c            # 游泳状态管理模块
│               ├── SCR_StaminaUpdateCoordinator.c # 体力更新协调器模块
│               ├── SCR_SpeedCalculation.c        # 速度计算模块
│               ├── SCR_StaminaConsumption.c      # 体力消耗计算模块
│               ├── SCR_StaminaRecovery.c         # 体力恢复计算模块
│               ├── SCR_DebugDisplay.c            # 调试信息显示模块
│               ├── SCR_CollapseTransition.c      # "撞墙"阻尼过渡模块
│               ├── SCR_ExerciseTracking.c        # 运动持续时间跟踪模块
│               ├── SCR_JumpVaultDetection.c      # 跳跃和翻越检测模块
│               ├── SCR_EncumbranceCache.c        # 负重缓存管理模块
│               ├── SCR_FatigueSystem.c           # 疲劳积累系统模块
│               ├── SCR_TerrainDetection.c        # 地形检测模块
│               ├── SCR_EnvironmentFactor.c       # 环境因子模块
│               ├── SCR_EpocState.c               # EPOC状态管理模块
│               ├── SCR_NetworkSync.c             # 网络同步模块
│               ├── SCR_UISignalBridge.c          # UI信号桥接模块
│               ├── SCR_StaminaConstants.c        # 常量定义模块
│               ├── SCR_StaminaHelpers.c          # 辅助函数模块
│               ├── SCR_StaminaHUDComponent.c     # 实时状态HUD组件
│               ├── SCR_RSS_ConfigManager.c        # 配置管理器
│               ├── SCR_RSS_Settings.c            # 配置类
│               ├── SCR_InventoryStorageManagerComponent_Override.c # 库存存储管理器覆盖
│               └── SCR_StanceTransitionManager.c # 姿态转换管理器
├── UI/                                   # UI布局文件
│   └── layouts/
│       └── HUD/
│           └── StatsPanel/
│               └── StaminaHUD.layout     # 状态HUD布局
├── Prefabs/                              # 预制体文件
│   └── Characters/
│       └── Core/
│           └── Character_Base.et         # 角色基础预制体
└── tools/                                # 开发工具和脚本
    ├── rss_super_pipeline.py              # NSGA-II 主优化管道
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
- **移动相位驱动消耗** - 优先以移动相位/冲刺状态决定 Pandolf/Givoni 路径，并提供服务端权威速度倍数计算接口（scripts/Game/Components/Stamina/SCR_StaminaUpdateCoordinator.c）
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

当前版本的核心参数集中在 `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` 和 `SCR_RSS_Settings.c` 中。

## 技术亮点

- **科学性**：基于医学模型（Pandolf、Givoni-Goldman）
- **效率**：200 次采样 vs 5000 次评估（25x 更快）
- **可解释性**：参数敏感度分析
- **灵活性**：3 种优化配置方案
- **工业级**：使用 Optuna 贝叶斯优化
