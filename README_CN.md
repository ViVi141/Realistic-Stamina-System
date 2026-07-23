# Realistic Stamina System (RSS) v6.0.0

[中文 README（当前）](README_CN.md) | [English README](README_EN.md) | [简版 README](README.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**Realistic Stamina System（RSS）** — 面向 **Arma Reforger** 的拟真体力与移动速度模组。  
根据体力、负重、坡度、环境等因素动态调整移动速度与消耗，核心采用运动生理学模型（Pandolf / ACSM / Critical Power–W′），而非简单常数扣条。

- **作者**: ViVi141（747384120@qq.com）· GitHub [@ViVi141](https://github.com/ViVi141)
- **许可证**: [AGPL-3.0](LICENSE)
- **模组 ID / GUID**: `Realistic Stamina System` / `68649101601CC93D`
- **建议游戏版本**: Arma Reforger **1.7+**（灌木/铁丝网减速与 RSS 限速合并）
- **配置版本**: `SCR_RSS_ConfigManager.CURRENT_VERSION` = **6.0.0**

> 逐版本变更请看 [CHANGELOG.md](CHANGELOG.md)。本 README 描述**当前仓库代码**；部分历史公式/路径名已在文中标注「v6 变更」。

---

## 功能说明

本模组根据玩家的体力值、负重、坡度与环境动态调整移动速度与消耗。体力充沛时可维持行军档目标速度；体力下降后消耗与限速闭环收紧；极低体力进入跛行。负重主要抬升「油耗」（代谢功率 / CP 压力），同时保留可配置的速度惩罚。

**体力标准参考**：引用 **ACFT（Army Combat Fitness Test）** 中 22–26 岁男性 2 英里测试 100 分用时 **15 分 27 秒**（可用 `tools/test_acft_2mile.py` / 锚点管线校验）。

### 主要特性

#### v6 功率预算与速度闭环
- **CP–W′ 功率预算**：Pandolf/ACSM 代谢功率 → 动态 Critical Power → W′（焦耳）放电/再填充
  - Elite：Skiba 双指数再填充；Standard / Tactical：线性 `w_prime_recovery_w_per_s`
  - Sprint 目标速度 ≈ `invert(min(sprint_power_cap, CP + W′/Δt))`
- **相位行军档**：Walk / Run / Sprint 使用配置的 m/s 目标；**已移除 25%/35%「意志力平台期」**
- **低体力跛行**：STA 低于约 5%（`SMOOTH_TRANSITION_END`）时按跛行倍率衰减
- **5 秒「撞墙」阻尼**（`SCR_RSS_CollapseTransition`）：刚跌破跛行阈值时用 5 秒 SmoothStep 过渡，避免「引擎突然断油」感
- **v_drain 闭环**：消耗按 `min(v_meas, appliedLimit)` 记账，与 `SetSpeedLimit` 一致
- **SpeedBridge 限速合并**：经 `SCR_RSS_SpeedBridge` → `SetSpeedLimit` 写入独立 source，与原生灌木/铁丝网减速取 **min**；禁止单独 `OverrideMaxSpeed` 盖掉 Foliage

#### 坡度与负重
- **坡度自适应步幅**：上坡自动降低目标速度（坡度–速度负反馈 / Tobler 系逻辑）
- **非线性坡度消耗**：小坡几乎无感，陡坡才真正吃力（避免缓坡频繁断气）
- **生理上限保护**：防止负重+坡度组合导致数值爆炸
- **负重影响**：负重主要抬升代谢成本与动态 CP 压力；速度惩罚系数可配（预设约 0.2–0.34 量级）
- **趴下休息负重优化**：趴姿时负重对恢复影响降低（地面支撑装备），重装更适合趴下回血

#### 移动、跳跃与冲刺
- **移动类型**：Idle / Walk / Run / Sprint（陆地）+ 游泳独立模型
- **跳跃 / 翻越额外消耗**：动态负重倍率；连续跳跃惩罚（窗口内加罚）；冷却；低体力（&lt;10%）禁用跳跃
- **Sprint 门禁**：有氧阈值 + W′ 池阈值（`IsSprintAllowedWithCp`）；短冲松键后剩余 W′ 可再冲
- **战术冲刺爆发减免**（负重速度惩罚短时减轻）与冷却字段仍保留兼容

#### 恢复、疲劳与代谢
- **深度恢复模型（代谢净值）**
  - **呼吸困顿期**：停跑后短时间不立刻回满（消除「跑两步停一下」）
  - **负重恢复惩罚**：Penalty ≈ (总重/耐受基准)^exponent × coeff
  - **边际效应衰减**：高体力区恢复变慢（亚健康区手感）
  - **最低体力阈值 + 休息时间**：极低体力需休息一段时间才进入有效恢复
- **姿态恢复倍率**：站 / 蹲 / 趴可配；趴姿通常最快
- **积分疲劳 I(t)**：长时间运动抬升 CP 压力与消耗
- **EPOC**：停跑后氧债消耗 ∝ 峰值意图代谢功率
- **代谢适应 / 健康状态建模**：有氧区更省、无氧区功率高但效率低（参数可配）

#### 环境
- **热应激**：日间升温时段抬升消耗、压制恢复；室内可部分豁免
- **冷应激**：对称惩罚路径（见 Environment 模块）
- **降雨湿重**：小/中/暴雨对应湿重；雨停后衰减
- **风阻 / 泥泞惩罚**：可按 Custom 开关
- **室内检测**：向上射线等，降低开放屋顶误判
- **气温物理步进**：短波/长波、云量、海拔等（可选用引擎气温作边界）
- **总湿重**：游泳湿重 + 降雨湿重，带饱和上限

#### 游泳
- **3D 阻力模型**：水平阻力 ∝ v²；垂直上浮/下潜额外功率；静态踩水基础功率（默认约 25 W）
- **负重阈值**：超过约 25 kg 时静态消耗显著增加
- **上岸湿重**：上岸后获得湿重并在数十秒内衰减
- **测速**：位置差分（`GetOrigin()`），避免游泳命令下 `GetVelocity()` 为 0

#### 表现、物品与 AI
- **HUD**：右上角体力/速度/负重等（`m_bHintDisplayEnabled`，默认关）
- **设置 UI / 管理菜单**：预设与说明面板
- **第一人称镜头惯性 / 头部物理**；冲刺 FOV 与疲劳态联动（可受原生表现开关影响）
- **CSB（苯甲酸钠咖啡因）注射器、吗啡**等消耗品与伤害效果
- **泥泞滑倒**（可选，默认关）：湿滑地形布娃娃/镜头失稳
- **AI（实验性）**：`SCR_RSS_AIManager` 编排状态机 → 限速 → 意图过滤 → 战斗衰减 → 伤害联动；专用服可关

#### 联机、配置与性能
- 服务端权威配置；客户端上报、服务端校验（`SCR_RSS_NetworkSyncManager`）
- 不参与本地 RSS 的复制体可跳过每 tick 体力循环；AI 不建玩家 Hint
- AI 可按距离/开关降载（`m_bDisableAIAllCalc` 等）
- 对外 API：`SCR_RSS_API`（含 `wPrimePool01`）；可选导出 `RSS_PlayerData.json`

---

## 核心闭环（v6）

每 tick（约 17 ms；体力结算按 0.2 s 协调）大致为：

```
v_meas → P(v) [SCR_RSS_MetabolismModel]
      → CP_eff / W′ [SCR_RSS_CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SCR_RSS_SpeedBridge]   // 与灌木/铁丝网取 min
```

| 概念 | 说明 |
|------|------|
| **有氧主条 (STA)** | 驱动引擎体力条 UI；低 STA → 跛行 + Collapse 阻尼 |
| **W′** | 超过动态 CP 的功率以焦耳放电；对外 `wPrimePool01` |
| **Sprint** | 功率反解速度；门禁看有氧 + W′ |
| **已移除** | 意志力平台期、固定无氧扣条、用 `OverrideMaxSpeed` 单独盖 Foliage |

权威计算：[docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)

---

## 预设

`SCR_RSS_Settings.m_sSelectedPreset`：

| 预设 | 取向 |
|------|------|
| **EliteStandard** | 最硬核 / 最拟真 |
| **StandardMilsim** | 默认折中 |
| **TacticalAction** | 更宽容，偏战术节奏 |
| **Custom** | 手调；可强制刷新系统预设回代码默认 |

数值由 `SCR_RSS_SettingsPresetBake` 嵌入（v6 优化器）。离线 JSON：`tools/optimized_rss_config_*_v6.json`。

粗略量级（以 Bake 为准）：

| | Elite | Standard | Tactical |
|--|------:|---------:|---------:|
| CP (W) | ~942 | ~1012 | ~1075 |
| W′_max (J) | ~20.4k | ~20.9k | ~20.4k |
| Sprint cap (W) | ~2830 | ~2880 | ~2525 |

Custom 常用倍率/开关（节选）：体力消耗/恢复倍率、负重速度惩罚、Sprint 倍率、热/雨/风/泥、疲劳、代谢适应、室内检测、HUD、泥泞滑倒、AI 战斗效果、数据导出等。

---

## 项目结构（与仓库一致）

约 **98** 个 EnforceScript `.c`。领域类统一 `SCR_RSS_*`（modded 入口除外）。

```
Realistic-Stamina-System/
├── addon.gproj
├── Assets/ Configs/ Prefabs/ UI/ photos/
├── scripts/Game/
│   ├── Integration/                 # modded 薄壳（高冲突面）
│   │   ├── PlayerBase.c
│   │   ├── PlayerBase_UpdateLoop.c  # 主更新循环
│   │   ├── SCR_StaminaOverride.c    # 拦截引擎体力条
│   │   ├── SCR_RSS_ServerBootstrap.c
│   │   ├── SCR_RSS_InventoryOverride.c
│   │   ├── SCR_PlayerBaseIntegrationHelpers.c
│   │   ├── SCR_PlayerBaseLoop.c / RpcHandler / VehicleHelper
│   │   └── SCR_RSS_StaminaComponentCompat.c
│   ├── RSS/
│   │   ├── Core/                    # ~30：代谢 / CP–W′ / 消耗·恢复 / 速度 / 协调器
│   │   │   ├── SCR_RSS_MetabolismModel.c / MetabolismMath.c
│   │   │   ├── SCR_RSS_CriticalPowerModel.c / AnaerobicBurst.c
│   │   │   ├── SCR_RSS_UpdateCoordinator.c / DrainCalculator.c
│   │   │   ├── SCR_RSS_SpeedCalculator.c / SpeedBridge.c
│   │   │   ├── SCR_RSS_RecoveryCalculator.c / EpocState.c / FatigueSystem.c
│   │   │   ├── SCR_RSS_CollapseTransition.c / SprintGate.c / …
│   │   │   ├── SCR_RSS_Constants.c / ConfigBridge.c / StaminaHelpers.c
│   │   │   └── …
│   │   ├── Environment/             # ~18：热冷雨风泥 / 坡 / 游泳 / 跳跃 / 地形
│   │   ├── AI/                      # ~8：AIManager + 状态机 / 限速 / 意图 / 衰减 / 伤害
│   │   ├── NetworkConfig/           # Settings / Params / Bake / ConfigManager / API / Sync
│   │   ├── Presentation/            # HUD、镜头、屏效、设置 UI
│   │   └── MudSlip/                 # 泥泞滑倒
│   ├── Components/Gadgets/          # CSB / 吗啡等
│   ├── UserActions/
│   └── Damage/…/
├── tools/                           # 数字孪生 + v6 管线（Python / Rust）
├── docs/
├── githooks/pre-commit
├── AUTHORS.md  CONTRIBUTING.md  CHANGELOG.md  LICENSE
└── WORKSHOP_CHANGENOTE_*.txt
```

**分层约定**：
- 公式在 `RSS/Core/`（及 Environment 等域模块），不进 Override 壳
- 速度只经 `SCR_RSS_SpeedBridge`
- 单文件硬上限 **65535 字节**；见 [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md)

---

## 技术实现

### 模型架构

- **[Pandolf](https://journals.physiology.org/doi/abs/10.1152/jappl.1977.43.4.577)**：步行/低速陆地代谢（含负重×坡度项）；陡下坡有 Santee 系修正
- **ACSM 跑/冲模型**：较高速度；与 Pandolf 在约 2.0–2.4 m/s **C¹ 混合**
- **Critical Power + W′**：可持续功率上限与无氧储备（焦耳）
- **[个性化运动建模](https://doi.org/10.1371/journal.pcbi.1006073)**（Palumbo et al.）：疲劳、代谢适应等思想仍用于参数化子系统
- **多维交互**：速度 × 负重 × 坡度 × 环境进入功率与恢复

> v6 **不再**使用「双稳态意志力平台期」作为主速度曲线；`CalculateSpeedMultiplierByStamina` 已转发到 `CalculateV6PhaseSpeedMultiplier`。

### 实现方式

- `modded SCR_CharacterStaminaComponent`（`SCR_StaminaOverride`）：拦截 `OnStaminaDrain` / 受控写回目标体力
- `modded SCR_CharacterControllerComponent`（`PlayerBase.c` + `PlayerBase_UpdateLoop.c`）：主循环、测速、限速、环境与 AI 编排
- 速度：`SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit` → `SCR_ChimeraCharacter.SetSpeedLimit(source, limit)`
- 调度：约每 **0.2 s** 结算体力相关；表现/行为另有节流
- 游泳使用独立 3D 模型，不套用陆地坡度/地形逻辑

### 速度与消耗（摘要）

1. **行军档目标**：`GetMarchWalk/Run/SprintSpeedMs` ×（1 − 负重速度惩罚）→ 换算为相对引擎最大速度的倍率  
2. **低 STA**：低于跛行阈值线性/SmoothStep 收到动态跛行倍率；Collapse 提供 5 s 阻尼  
3. **Sprint**：功率反解 + 与 Run 的最低拉开；门禁看 STA + W′  
4. **陆地消耗**：`P = MetabolismModel(v_drain, load, grade, …)` → `drain ∝ P × energy_to_stamina_coeff`  
5. **超 CP**：差额由 W′ 放电；再填充按预设（Skiba / 线性）  
6. **游泳**：`F_d = 0.5·ρ·v²·C_d·A` + 垂直功率 + 踩水；上岸湿重并入环境总湿重  

### 跳跃 / 翻越（当前常量量级）

见 `SCR_RSS_Constants` / Params（以代码为准）：
- 肌肉效率、跳跃高度猜测、焦耳→体力标尺
- 连续跳跃窗口与惩罚、冷却、最低体力阈值（约 10%）
- 单次消耗硬钳位，防止极端负重爆炸

### 恢复（当前设计意图）

- 最终恢复 ≈ 姿态修正后的基础恢复 − 负重惩罚 − EPOC/氧债等
- 站姿往往慢于蹲/趴；重装鼓励趴下
- 高 STA 区边际衰减；极低 STA 需休息门槛

### 关键入口文件

| 职责 | 文件 |
|------|------|
| 主循环 | `PlayerBase_UpdateLoop.c` |
| 体力拦截 | `SCR_StaminaOverride.c` |
| 代谢 / CP–W′ | `SCR_RSS_MetabolismModel.c` / `SCR_RSS_CriticalPowerModel.c` |
| 协调 / 消耗 | `SCR_RSS_UpdateCoordinator.c` / `SCR_RSS_DrainCalculator.c` |
| 速度 | `SCR_RSS_SpeedCalculator.c` / `SCR_RSS_SpeedBridge.c` |
| 恢复 / EPOC / 疲劳 | `SCR_RSS_RecoveryCalculator.c` / `EpocState` / `FatigueSystem` |
| 环境 | `SCR_RSS_EnvironmentFactor.c` 及 Astronomy/Weather/Rain/… |
| 配置 | `SCR_RSS_Settings.c` / `Params` / `SettingsPresetBake` / `ConfigManager` |
| API | `SCR_RSS_API.c` |

### 调试信息

开启 `m_bDebugLogEnabled` / Verbose 后，控制台可输出移动类型、体力、速度倍率、负重、坡度、环境因子（时间、热应激、降雨、室内、湿重）等。格式化集中在 `SCR_RSS_DebugDisplay` / UpdateLoop 调试模块。

---

## 安装方法

1. 将本仓库放到 Arma Reforger Workbench 的 `addons`（或等价路径）
2. 打开 `addon.gproj` 并编译
3. 在游戏 / 服务器中启用模组（依赖原版数据，见 `addon.gproj` Dependencies）

## 使用方法

1. 加载模组后系统自动接管体力与限速
2. 在设置菜单或服务器 JSON 中选择预设（默认 **StandardMilsim**）
3. 需要时打开 HUD、调试日志、泥泞滑倒、AI 战斗效果、数据导出
4. Custom 下可调消耗/恢复倍率与环境开关；系统预设可用强制刷新回到 Bake 默认

配置优先级：运行期动态配置（当前预设/服务器）优先；Custom 增量补全，避免粗暴覆盖用户已设项。

### 外部模组 API

```c
IEntity player = SCR_PlayerController.GetLocalControlledEntity();
if (!player)
    return;

RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    PrintFormat("STA=%1 W'=%2 sprintAllowed=%3 speed=%4",
        info.staminaPercent,
        info.wPrimePool01,
        info.sprintAllowed,
        info.currentSpeed);
}

RSS_EnvironmentInfo env = SCR_RSS_API.GetEnvironmentInfo(player);
```

详见 [docs/RSS_API.md](docs/RSS_API.md)。`anaerobicPercent` 已弃用，请读 **`wPrimePool01`**。

### 调整参数

- **玩法向**：设置 UI / `$profile` 下 RSS JSON / 平面参数数组
- **预设嵌入**：改 `tools/optimized_rss_config_*_v6.json` → `embed_json_to_c.py` → `SCR_RSS_SettingsPresetBake.c`
- **硬常量 / 开关**：`SCR_RSS_Constants.c`、`SCR_RSS_ConfigBridge.c`
- **禁止**在业务里绕过 SpeedBridge 直接 `OverrideMaxSpeed` 盖 Foliage

常见常量（fallback，实际常被预设覆盖）：跛行阈值、最低速度倍率、跳跃冷却/惩罚、角色体重基准、各类 `RSS_PERF_*` 性能参数等。

---

## 系统优势

- **拟真**：功率–限速闭环，负重/坡度/环境进入同一代谢叙事
- **可调**：三档预设 + Custom + 大量开关，服主可定难度
- **可维护**：领域分层、`SCR_RSS_*` 命名、文件大小门禁
- **可校验**：Python/Rust 数字孪生与生理锚点测试

战术层面：需要管理体力与负重；重装更依赖趴下恢复；冲刺受 W′ 约束，不能无限「意志力跑」。

---

## 已知问题与限制

- HUD 默认关闭，需手动开启
- 引擎对部分速度倍率仍有上限/合并规则；必须以 SpeedBridge 参与 min 合并
- 部分表现（自定义 FOV/屏效）可能受「原生表现优先」开关影响
- 极低体力/高负重/陡坡组合仍可能手感偏硬——属设计取向，可用 Tactical / Custom 放宽
- 已知限速与滑步问题见 [docs/RSS_已知问题_限速与滑步.md](docs/RSS_已知问题_限速与滑步.md)
- 部分旧文档仍写 v3 路径或群组 AI 旧模块名；**以本 README 与 `scripts/` 为准**

---

## 工具链（`tools/`）

| 入口 | 用途 |
|------|------|
| `rss_pipeline_v6.py` | `validate` / `optimize` / `anchors` |
| `rss_sim/` | PyO3 仿真核（可回退纯 Python） |
| `rust_pipeline_v6/` | Rust CLI / dual-run |
| `test_v6_smoke.py` / `bench_physio_anchors.py` / `test_acft_2mile.py` | 冒烟与锚点 |
| `check_script_size.py` / `check_enforce_syntax.py` | 提交前门禁 |
| `embed_json_to_c.py` | JSON → SettingsPresetBake |

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
| [docs/体力系统计算逻辑文档.md](docs/体力系统计算逻辑文档.md) | 历史归档（v3/v5） |
| [docs/数字孪生优化器计算逻辑文档.md](docs/数字孪生优化器计算逻辑文档.md) | 孪生公式 |
| [docs/RSS_API.md](docs/RSS_API.md) | 外部 API |
| [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) | 命名 / 分层 / 大小限制 |
| [docs/灌木丛移动减速机制.md](docs/灌木丛移动减速机制.md) | 1.7+ 限速合并 |
| [docs/泥泞滑倒判定模型.md](docs/泥泞滑倒判定模型.md) | 泥泞滑倒 |
| [docs/RSS_已知问题_限速与滑步.md](docs/RSS_已知问题_限速与滑步.md) | 已知问题 |
| [CHANGELOG.md](CHANGELOG.md) | 全版本变更 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 贡献指南 |

---

## 近期版本要点（详情见 CHANGELOG）

- **6.0.0** — CP–W′ 学术拟真重构；移除意志力平台期；代谢统一；AI 限速与玩家相位曲线对齐；工具 v6 管线
- **5.0.0** — 双池（有氧 + 无氧/W′ 前身语义）大重写；命名统一 `SCR_RSS_*`；归档 v3.23.1
- **3.23.1** — Reforger 1.7 灌木减速合并；稳定版预设锁定
- **3.23.0** — AI 子系统管理器、泥泞开关、设置 UI 等

更早的 v2.x–v3.22 细节（游泳湿重修复、时间单位、环境温度模型、CSB 药效改版等）全部保留在 [CHANGELOG.md](CHANGELOG.md)，不再整页粘贴进 README。

---

## 开发说明

### 编译要求
- Arma Reforger Workbench + EnforceScript

### EnforceScript 注意
- **禁止**三元 `?:`；单行 `if` 必须 `{}`
- 使用 `ref`，避免废弃 `autoptr`
- 提交前跑 `check_script_size.py`、`check_enforce_syntax.py`

---

## 参考文献

1. **Pandolf, K. B., Givoni, B. A., & Goldman, R. F. (1977)**. Predicting energy expenditure with loads while standing or walking very slowly. *Journal of Applied Physiology*, 43(4), 577–581.  
   https://journals.physiology.org/doi/abs/10.1152/jappl.1977.43.4.577  
   - 应用：陆地步行/负重/坡度代谢项

2. **Palumbo, M. C., et al. (2018)**. Personalizing physical exercise in a computational model of fuel homeostasis. *PLOS Computational Biology*, 14(4), e1006073.  
   https://doi.org/10.1371/journal.pcbi.1006073  
   - 应用：个性化效率、疲劳与代谢适应等建模思想

3. Critical Power / W′ 与 Skiba 再填充相关运动生理学文献（见 v6 计算文档中的模型说明）

---

## 贡献

欢迎 Issue 与 Pull Request。贡献即同意以 AGPL-3.0 发布修改。见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[GNU Affero General Public License v3.0](LICENSE)

## 作者

- **ViVi141** — 747384120@qq.com

---

**注意：** 本模组深度介入移动与体力机制，可能与其他改速度/体力的模组冲突。建议先在测试环境验证。
