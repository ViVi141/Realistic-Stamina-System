# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

Realistic Stamina System（RSS）是面向 **Arma Reforger** 的拟真体力与移动模组。当前版本使用 **Pandolf / ACSM / Critical Power–W′** 模型来驱动移动限速、体力消耗、恢复与 Sprint 门禁。

- 作者：ViVi141
- 邮箱：747384120@qq.com
- 模组 ID / GUID：`Realistic Stamina System` / `68649101601CC93D`
- 配置版本：`6.0.0`
- 建议游戏版本：Arma Reforger `1.7+`

## 项目简介

RSS 的目标不是简单地“改体力条”，而是把以下因素放进同一套体力预算里：

- 当前速度
- 负重
- 坡度
- 温度 / 风 / 雨 / 泥泞
- 游泳状态
- Sprint 爆发能力

当前 v6 主路径可以概括为：

```text
v_meas -> metabolic power -> effective CP / W' -> speed limit -> SetSpeedLimit
```

也就是说：

- 代谢按**实测速度**记账
- Sprint 不是固定 `Run × 1.30`
- 玩家主路径**没有意志力平台期**
- 限速通过 `SCR_RSS_SpeedBridge` 参与引擎 `SetSpeedLimit` 合并，不覆盖灌木 / 铁丝网减速

## 当前实现要点

### 速度与步态

- 玩家主速度曲线由 `SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier()` 驱动。
- 当体力高于跛行阈值时，维持当前步态目标速度。
- 当体力低于 `SMOOTH_TRANSITION_END = 0.05` 时，进入跛行区，并叠加 `SCR_RSS_CollapseTransition` 的 5 秒阻尼过渡。
- 当前代码中 `V6_USE_MARCH_GAIT_SPEEDS = false`，所以运行时默认顶速回退为：
  - Walk：约 `1.45 m/s`
  - Run：约 `3.8 m/s`
  - Sprint：约 `5.5 m/s`
- 预设里仍保留 `1.4 / 2.8 / 4.5 m/s` 的行军档字段；若后续打开行军档开关，会切回这些配置值。

### Sprint 与 W′

- Sprint 速度由步态目标、可用功率、W′ 剩余量和最低拉开共同决定。
- 有氧 Sprint 门槛回退常量是 `0.25`。
- W′ Sprint 门槛默认是 `0.20`。
- `SPRINT_GAIT_MIN_OVER_RUN_RATIO = 1.25`，保证 Sprint 与 Run 有身份差异。
- 当前消耗不是“固定 3 倍”；Sprint 额外体力压力来自功率模型、W′ 放电和参数换算。

### 负重与坡度

- 负重同时影响**速度惩罚**和**代谢功率 / CP 预算**。
- 负重速度惩罚回退常量为：
  - 系数：`0.28`
  - 指数：`1.5`
- 实际运行时优先取 `SCR_RSS_Params` 预设值。
- 坡度不是“每度固定扣速”；当前用 Tobler 系速度调整与非线性坡度代谢。

### 恢复、疲劳与 EPOC

- 恢复按“代谢净值”思路计算，不是简单静止就持续回血。
- 只要角色处于移动状态，正常有氧恢复会被压到 0。
- 停止运动后会进入 `2 秒` EPOC 延迟期。
- 低体力恢复静止门槛回退值是 `5 秒`。
- 当前回退常量：
  - `BASE_RECOVERY_RATE = 0.00010`
  - `LOAD_RECOVERY_PENALTY_COEFF = 0.0002`
  - `FATIGUE_ACCUMULATION_COEFF = 0.025`
  - `FATIGUE_MAX_FACTOR = 2.5`

### 环境系统

当前环境链路包括：

- 热应激
- 冷应激
- 风阻
- 降雨湿重
- 地表湿度
- 泥泞与滑倒风险
- 室内检测

关键点：

- 热应激当前主路径按**实时气温阈值**计算，不是旧版按时间段硬编码。
- 降雨湿重按强度连续累积，不是“小雨 / 中雨 / 暴雨”三档固定值。
- 总湿重 = 游泳湿重 + 降雨湿重，再钳到上限 `10kg`。

### 游泳

游泳使用独立能量模型，不套用陆地坡度 / 步态逻辑。

- `SCR_RSS_SwimmingStateManager` 负责游泳状态与湿重跟踪
- `SCR_RSS_SwimmingStaminaModel` 负责水中体力消耗
- 动态消耗主导项近似随速度 `v^3` 增长
- `SWIMMING_BASE_POWER` 回退值为 `20W`
- `SWIMMING_ENCUMBRANCE_THRESHOLD = 25kg`
- 水中湿重会逐步增长到最多 `10kg`，上岸后 `30 秒`线性衰减

### 跳跃 / 翻越

- 跳跃成本由 `ComputeJumpCostPhys()` 计算，按总重量、估算高度、估算水平速度和肌肉效率求解。
- 翻越 / 攀爬成本由 `ComputeClimbCostPhys()` 计算。
- 当前连续跳跃窗口是 `2 秒`，冷却也是 `2 秒`。
- 体力低于 `10%` 时禁止跳跃。

### AI

当前 AI 不是旧文档里的群组代理方案，而是更直接的链路：

```text
SCR_RSS_AIManager -> AIStaminaState -> AISpeedCap -> AIIntentFilter -> AICombatDecay
```

说明：

- 玩家主循环默认 `17ms`
- AI 主循环默认 `100ms`
- AI 会按最近玩家距离切到 `200 / 300 / 1500ms` 的 LOD 间隔
- AI 行为层另外有 `500ms` 节流

## 预设与配置

当前预设：

- `EliteStandard`
- `StandardMilsim`（默认）
- `TacticalAction`
- `Custom`

配置入口：

- `scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c`
- `scripts/Game/RSS/NetworkConfig/SCR_RSS_Params.c`
- `scripts/Game/RSS/NetworkConfig/SCR_RSS_SettingsPresetBake.c`
- `scripts/Game/RSS/Core/SCR_RSS_ConfigBridge.c`

常见可调项：

- `critical_power_watts`
- `w_prime_max_joules`
- `w_prime_recovery_w_per_s`
- `sprint_power_cap_watts`
- `v5_walk_speed_ms`
- `v5_run_speed_ms`
- `v5_sprint_speed_ms`
- `encumbrance_speed_penalty_*`
- `encumbrance_stamina_drain_coeff`
- `base_recovery_rate`
- 各类环境开关与惩罚参数

## 项目结构

```text
scripts/Game/
├── Integration/
│   ├── PlayerBase.c
│   ├── PlayerBase_UpdateLoop.c
│   ├── SCR_StaminaOverride.c
│   └── ...
├── RSS/
│   ├── Core/
│   ├── Environment/
│   ├── AI/
│   ├── NetworkConfig/
│   ├── Presentation/
│   └── MudSlip/
├── Components/Gadgets/
├── UserActions/
└── Damage/

tools/
├── rss_pipeline_v6.py
├── rss_sim/
├── rust_pipeline_v6/
├── test_v6_smoke.py
├── test_acft_2mile.py
└── embed_json_to_c.py
```

## 安装

### Workbench

1. 将项目放到 Workbench `addons` 目录
2. 打开 `addon.gproj`
3. 编译项目
4. 在游戏或服务器中启用模组

### 服务器

推荐由服务端统一配置预设与开关，再通过同步下发给客户端。

## 使用

1. 启用模组
2. 选择预设或切换到 `Custom`
3. 按需要开启 HUD / Debug / MudSlip / AI 战斗效果 / 数据导出
4. 如需外部集成，使用 `SCR_RSS_API` 或 JSON 数据导出

## 外部 API

示例：

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

说明：

- `wPrimePool01` 是当前权威字段
- `anaerobicPercent` 仅保留兼容，不建议新代码继续使用
- 环境信息可通过 `SCR_RSS_API.GetEnvironmentInfo()` 获取

## 开发

### 核心入口

- 主循环：`scripts/Game/Integration/PlayerBase.c`、`PlayerBase_UpdateLoop.c`
- 体力条拦截：`scripts/Game/Integration/SCR_StaminaOverride.c`
- 速度 / 代谢 / CP–W′：`scripts/Game/RSS/Core/`
- 环境 / 游泳 / 跳跃：`scripts/Game/RSS/Environment/`
- AI：`scripts/Game/RSS/AI/`
- 配置 / 同步 / API：`scripts/Game/RSS/NetworkConfig/`
- HUD / 表现 / 设置：`scripts/Game/RSS/Presentation/`

### 开发约束

- 限速优先通过 `SCR_RSS_SpeedBridge`
- 代码风格与约束见 `docs/RSS_CODING_STANDARDS.md`
- 提交前建议运行：

```bash
cd tools
python check_script_size.py
python check_enforce_syntax.py
python test_v6_smoke.py
```

## 工具链

`tools/` 目录包含当前 v6 调参与校验工具：

- `rss_pipeline_v6.py`：验证 / 优化 / 锚点
- `rss_sim/`：PyO3 仿真核心
- `rust_pipeline_v6/`：Rust CLI 入口
- `test_v6_smoke.py`：v6 冒烟测试
- `test_acft_2mile.py`：ACFT 锚点测试
- `embed_json_to_c.py`：将 JSON 预设嵌回 C 端

示例：

```bash
cd tools
pip install -r requirements.txt
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python test_acft_2mile.py
```

## 文档

- 当前中文 README：`README_CN.md`
- 英文 README：`README_EN.md`
- v6 权威计算说明：`docs/RSS_v6_计算逻辑权威版.md`
- API：`docs/RSS_API.md`
- 编码规范：`docs/RSS_CODING_STANDARDS.md`
- 已知问题：`docs/RSS_已知问题_限速与滑步.md`
- 版本变更：`CHANGELOG.md`

## 许可证

本项目采用 [GNU AGPL-3.0](LICENSE) 许可证。
