# Realistic Stamina System (RSS) v6.0.0

[中文 README（当前）](README_CN.md) | [English README](README_EN.md) | [简版 README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System（RSS）** 是面向 **Arma Reforger** 的拟真体力与移动速度模组。  
用运动生理学模型（Pandolf / ACSM / Critical Power–W′）驱动消耗、恢复与限速，而不是简单的常数扣条。

- **作者**: ViVi141（747384120@qq.com）
- **许可证**: [AGPL-3.0](LICENSE)
- **模组 ID / GUID**: `Realistic Stamina System` / `68649101601CC93D`
- **建议游戏版本**: Arma Reforger **1.7+**（灌木/铁丝网减速与 RSS 限速合并）
- **配置版本**: `SCR_RSS_ConfigManager` → `6.0.0`

> 历史版本说明见 [CHANGELOG.md](CHANGELOG.md)。本 README 只描述**当前仓库代码**。

---

## 核心闭环（v6）

每 tick（约 17 ms；体力结算按 0.2 s 协调）大致为：

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]  （与灌木/铁丝网取 min，不覆盖原生减速）
```

| 概念 | 说明 |
|------|------|
| **有氧主条 (STA)** | 仍驱动引擎体力条 UI；低 STA（约 &lt;5%）进入跛行 + 塌陷阻尼 |
| **W′ 功率储备** | 超过动态 CP 的功率以焦耳放电；归一化 `wPrimePool01` 对外暴露 |
| **Sprint** | `v_sprint = invert(min(sprint_cap, CP + W′/Δt))`；门禁看有氧阈值 + W′ 池 |
| **v_drain** | 消耗按 `min(v_meas, appliedLimit)` 记账，与限速闭环一致 |
| **已移除** | 25%/35%「意志力平台期」、固定无氧扣条等旧逻辑 |

权威计算说明：[docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)

---

## 主要特性

### 生理与移动
- Pandolf（步行）+ ACSM（跑/冲）混合代谢，中间 C¹ 衔接
- 动态 CP（负重 / 坡度 / 环境 / 积分疲劳）与 W′ 再填充（Elite：Skiba 双指数；Standard/Tactical：线性 W/s）
- 行军档 Walk / Run / Sprint 目标速度（m/s），负重主要抬升代谢成本
- 坡度：Tobler 式步幅反馈、非线性坡度消耗、坡速平滑过渡
- 跳跃 / 翻越额外消耗与连续跳跃惩罚
- 游泳 3D 阻力模型（水平阻力、垂直功率、踩水、上岸湿重）
- 积分疲劳 I(t) + EPOC（停跑后氧债 ∝ 峰值功率）

### 环境
- 热应激 / 冷应激、降雨湿重、风阻、泥泞惩罚
- 室内检测（射线）、地形材质表、气温物理步进（可与引擎气温混合）
- Custom 预设下可分别开关：热 / 雨 / 风 / 泥 / 疲劳 / 代谢适应 / 室内等

### 表现与物品
- 右上角体力 HUD（默认关）、管理菜单 / 设置页
- 第一人称镜头惯性与头部物理
- CSB 战术兴奋剂、吗啡等消耗品与伤害效果

### AI（实验性）
- `SCR_RSS_AIManager` 统一编排（服端、500 ms 行为节流）
- 体力状态机 → `AISpeedCap`（`SetSpeedLimit` 与灌木合并）→ 意图过滤 → 战斗衰减 → 伤害联动
- 开关：`m_bEnableAIStaminaCombatEffects`（工作台默认开；专用服 JSON 常默认关）
- 另有 `m_bDisableAIAllCalc` / `m_bDisableAIStaminaCalc` 降载选项

### 联机与配置
- 服务端权威配置；客户端上报、服务端校验（`SCR_RSS_NetworkSyncManager`）
- 三档系统预设 + Custom；参数经平面 float 数组序列化（缓解 Workbench ICE）
- 对外 API：`SCR_RSS_API`（含 `wPrimePool01`）；可选 `RSS_PlayerData.json` 导出

---

## 预设

`m_sSelectedPreset`：

| 预设 | 取向 |
|------|------|
| **EliteStandard** | 最硬核 / 最拟真（低 combat/recovery ease） |
| **StandardMilsim** | 默认折中（出厂默认） |
| **TacticalAction** | 更宽容，偏战术节奏 |
| **Custom** | 手调；系统预设可强制刷新回代码默认 |

数值由 `SCR_RSS_SettingsPresetBake` 嵌入（v6 优化器产物）。  
离线 JSON：`tools/optimized_rss_config_*_v6.json`（可用 `embed_json_to_c.py` 再嵌入）。

粗略量级（以 Bake 为准，会随优化更新）：

| | Elite | Standard | Tactical |
|--|------:|---------:|---------:|
| CP (W) | ~942 | ~1012 | ~1075 |
| W′_max (J) | ~20.4k | ~20.9k | ~20.4k |
| Sprint cap (W) | ~2830 | ~2880 | ~2525 |

生理锚点参考：ACFT 2 英里（22–26 岁男性 100 分 ≈ 15:27）。

---

## 项目结构（与仓库一致）

约 **98** 个 EnforceScript `.c` 文件。类名统一 `SCR_RSS_*`（modded 入口除外）。

```
Realistic-Stamina-System/
├── addon.gproj                 # 模组工程
├── Assets/                     # 资源
├── Configs/EntityCatalog/      # 军火库登记
├── Prefabs/                    # 角色 / 药品预制体
├── UI/layouts/                 # HUD、RSS 设置菜单
├── scripts/Game/
│   ├── Integration/            # modded 薄壳（高冲突面）
│   │   ├── PlayerBase.c
│   │   ├── PlayerBase_UpdateLoop.c
│   │   ├── SCR_StaminaOverride.c
│   │   ├── SCR_RSS_ServerBootstrap.c
│   │   └── …
│   ├── RSS/
│   │   ├── Core/               # 代谢 / CP–W′ / 消耗·恢复 / 速度 / 协调器（~30）
│   │   ├── Environment/        # 热冷雨风泥 / 坡 / 游泳 / 跳跃（~18）
│   │   ├── AI/                 # AIManager + 状态机 / 限速 / 意图 / 衰减（~8）
│   │   ├── NetworkConfig/      # Settings / Params / ConfigManager / API / Sync（~8）
│   │   ├── Presentation/       # HUD、镜头、屏幕效果、设置 UI（~12）
│   │   └── MudSlip/            # 泥泞滑倒（默认关）
│   ├── Components/Gadgets/     # CSB / 吗啡等
│   ├── UserActions/
│   └── Damage/…/
├── tools/                      # 数字孪生 + v6 优化管线（Python / Rust）
├── docs/                       # 计算逻辑、API、规范、已知问题
└── githooks/pre-commit
```

**分层约定**（摘要）：

- 公式只在 `RSS/Core/`（及 Environment 等域模块）
- `SCR_StaminaOverride` 仅拦截引擎体力条
- 速度只经 `SCR_RSS_SpeedBridge` → `SetSpeedLimit`，禁止单独 `OverrideMaxSpeed` 盖掉灌木减速
- 单文件硬上限 **65535 字节**；见 `docs/RSS_CODING_STANDARDS.md`

关键模块映射：

| 职责 | 文件 |
|------|------|
| 代谢功率 | `SCR_RSS_MetabolismModel.c` |
| CP–W′ | `SCR_RSS_CriticalPowerModel.c` / `SCR_RSS_AnaerobicBurst.c` |
| 消耗协调 | `SCR_RSS_UpdateCoordinator.c` / `SCR_RSS_DrainCalculator.c` |
| 速度 | `SCR_RSS_SpeedCalculator.c` / `SCR_RSS_SpeedBridge.c` |
| 恢复 / EPOC | `SCR_RSS_RecoveryCalculator.c` / `SCR_RSS_EpocState.c` |
| 主循环 | `PlayerBase_UpdateLoop.c` |
| 配置 | `SCR_RSS_Settings.c` / `SCR_RSS_Params.c` / `SCR_RSS_SettingsPresetBake.c` |

---

## 安装与使用

### 工作台开发
1. 将本仓库放到 Workbench `addons`（或等价路径）
2. 用 Workbench 打开 `addon.gproj`
3. 依赖原版游戏数据（`addon.gproj` Dependencies）

### 服务器
1. 启用模组；配置由**服务端**写入 / 权威下发
2. 在设置菜单或 JSON 中选择预设；Custom 下可改倍率与环境开关
3. HUD、泥泞滑倒、AI 战斗效果、数据导出均为可选开关（多数默认关）

### 外部模组 API

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

字段说明见 [docs/RSS_API.md](docs/RSS_API.md)。  
`anaerobicPercent` 已弃用，请读 `wPrimePool01`。

---

## 工具链（`tools/`）

用于与 C 端对齐的数字孪生、生理锚点校验、预设优化。

| 入口 | 用途 |
|------|------|
| `rss_pipeline_v6.py` | `validate` / `optimize` / `anchors` |
| `rss_sim/` | PyO3 仿真核（不可用时回退纯 Python） |
| `rust_pipeline_v6/` | Rust CLI（可 dual-run） |
| `test_v6_smoke.py` / `bench_physio_anchors.py` / `test_acft_2mile.py` | 冒烟与锚点 |
| `check_script_size.py` / `check_enforce_syntax.py` | 提交前门禁 |
| `embed_json_to_c.py` | JSON → `SettingsPresetBake` |

```bash
cd tools
pip install -r requirements.txt
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python check_script_size.py
```

详见 [tools/README.md](tools/README.md)。

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md) | v6 计算权威说明 |
| [docs/RSS_API.md](docs/RSS_API.md) | 外部 API |
| [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) | 命名 / 分层 / 文件大小 |
| [docs/灌木丛移动减速机制.md](docs/灌木丛移动减速机制.md) | 1.7+ 限速合并 |
| [docs/RSS_已知问题_限速与滑步.md](docs/RSS_已知问题_限速与滑步.md) | 已知问题 |
| [docs/泥泞滑倒判定模型.md](docs/泥泞滑倒判定模型.md) | 泥泞滑倒 |
| [CHANGELOG.md](CHANGELOG.md) | 全版本变更 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 贡献指南 |

部分 `docs/` 下的 v3/v5 AI 设计稿可能滞后于当前 `RSS/AI/` 实现；以本 README 与源码为准。

---

## 开发注意（EnforceScript）

- **禁止**三元运算符 `?:`
- 单行 `if` 必须带 `{}`
- 使用 `ref`，避免废弃的 `autoptr`
- 提交前建议跑 `check_script_size.py` 与 `check_enforce_syntax.py`

---

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE)。  
贡献即表示同意以 AGPL-3.0 发布你的修改。
