# RSS 开发者指南（6.1.x）

面向在本仓库改脚本 / 调参 / 接外部模组的贡献者。玩法说明见 [README_CN.md](../README_CN.md)；社区 PR 流程见 [CONTRIBUTING.md](../CONTRIBUTING.md)。

**冲突时以源码为准**，其次本指南索引的「权威」文档。

---

## 1. 环境与编译

| 项 | 说明 |
|----|------|
| 引擎 | Arma Reforger **1.7+** + Workbench |
| 语言 | EnforceScript（`.c`） |
| 模组 GUID | `68649101601CC93D` |
| 配置版本 | `SCR_RSS_ConfigManager.CURRENT_VERSION`（当前与发布号对齐，如 6.1.3） |
| 原生参考树 | 本机可用 `C:\Users\74738\Documents\arma_reforger_code`（用户约定）对照官方 API |

提交前建议在仓库根目录执行（Windows PowerShell，勿用 `&&` 串命令）：

```powershell
python tools/check_script_size.py
python tools/check_enforce_syntax.py
python tools/test_v6_smoke.py
```

参数/孪生校验见 [tools/README.md](../tools/README.md)。

---

## 2. 目录地图

```text
scripts/Game/
  Integration/     # modded 薄壳：PlayerBase、StaminaOverride、RPC
  RSS/
    Core/          # 代谢、CP–W′、消耗/速度、协调器、常量
    Environment/   # 天气/地形/惩罚、EnvConstants
    NetworkConfig/ # Settings、Params、API、同步、Bootstrap
    AI/            # AIManager + 状态机/限速/意图/战斗衰减
    Presentation/  # HUD、相机表现
    MudSlip/       # 泥泞滑倒
    Items/         # 注射器等
  Components/ …    # 其它挂接（按需）
Prefabs/           # 角色等预制覆盖（如 Character_Base.et）
tools/             # 数字孪生、校验、预设 JSON → C 嵌入
docs/              # 本指南与权威说明
```

### 主循环入口（改体力必看）

1. `PlayerBase_UpdateLoop.c` → `UpdateSpeedBasedOnStamina`
2. `SCR_RSS_UpdateCoordinator` → 速度 / 消耗 / 恢复编排
3. `SCR_RSS_SpeedBridge` → `SetSpeedLimit`（与灌木等 **min** 合并）
4. `SCR_StaminaOverride` → 有氧权威 `m_fTargetStamina`；拦截引擎 drain
5. `SCR_RSS_SprintGate` → W′→`ApplyTransientEngineStamina`（晃动/Exhaustion，不改有氧权威）

AI：`SCR_RSS_AIManager.Tick`（服端、约 500 ms 行为层）。详见 [RSS_AI_行为说明.md](RSS_AI_行为说明.md)。

---

## 3. 硬性约束（先读再改）

完整条文：[RSS_CODING_STANDARDS.md](RSS_CODING_STANDARDS.md)、[scripts_file_size_limit.md](scripts_file_size_limit.md)。

| 规则 | 要点 |
|------|------|
| **禁止三元 `?:`** | 一律 `if-else` |
| **单文件 ≤ 65535 字节** | 超限可无报错崩溃；`PlayerBase.c` 已超，只减不增净体积 |
| **modded 扩展** | 同一 `SCR_CharacterControllerComponent` 仅 `PlayerBase.c` + `PlayerBase_UpdateLoop.c` |
| **限速** | 只走 `SetSpeedLimit` / SpeedBridge；**禁止**默认拧 `Physics` 水平速度（滑步） |
| **代谢压速** | 默认 `V6_APPLY_CP_METABOLIC_SPEED_CAP = false`（drain-only）；勿未经评审改默认 |
| **公式位置** | Pandolf / CP–W′ 等只在 `RSS/Core/`，勿塞进 Integration |
| **CPR 兼容** | 不在本模组；禁用原生 stamina 组件需独立 compat 模组 |

命名：类 `SCR_RSS_*`，枚举 `ERSS_*`，DTO `RSS_*`，文件名与主类一致。

---

## 4. 常见改动路径

| 目标 | 优先改哪里 | 权威/备注 |
|------|------------|-----------|
| 消耗/恢复/CP–W′ 公式 | `SCR_RSS_Metabolism*`、`CriticalPowerModel`、`DrainCalculator`、`RecoveryCalculator` | [RSS_v6_计算逻辑权威版.md](RSS_v6_计算逻辑权威版.md) |
| Walk/Run/Sprint 意图速度 | `SCR_RSS_SpeedCalculator`、常量 `V6_*` | 同上；已知滑步问题见 [RSS_已知问题_限速与滑步.md](RSS_已知问题_限速与滑步.md) |
| 试跑开关 / 软硬常量 | `SCR_RSS_Constants.c`、`SCR_RSS_AIConstants.c`、`SCR_RSS_EnvConstants.c` | Constants 接近 50 KB，新增宜拆文件 |
| 三档预设数值 | `tools/optimized_rss_config_*_v6.json` → `embed_json_to_c` / Bake | [tools/README.md](../tools/README.md)、[RSS_v6_优化管线设计.md](RSS_v6_优化管线设计.md) |
| 服主菜单开关 | `SCR_RSS_Settings` + ConfigManager | 运行期配置优先于工具硬编码 |
| 外部模组读体力 | `SCR_RSS_API` | [RSS_API.md](RSS_API.md)；优先 `wPrimePool01` |
| 引擎 API 对照 | — | [engine_api_usage.md](engine_api_usage.md) |
| 灌木限速合并 | SpeedBridge + 引擎 `SetSpeedLimit` | [灌木丛移动减速机制.md](灌木丛移动减速机制.md) §13 |
| 泥泞滑倒 | `MudSlip/` + `SCR_RSS_EnvConstants` | [泥泞滑倒判定模型.md](泥泞滑倒判定模型.md) |
| AI 战斗效果 | `RSS/AI/*` + `m_bEnableAIStaminaCombatEffects` | [RSS_AI_行为说明.md](RSS_AI_行为说明.md) |

---

## 5. 调试与实机回归

- 调试日志 / Hint：设置里的 debug / hint；Drain 类日志常带 `[RSS][Drain]`。
- **最小回归**：Workbench 编译 → 单机空载冲刺至 W′ 空 → 停步恢复 → 负重 ~30 kg 缓坡 Run → 进灌木确认限速仍合并。
- 开服：改预设后确认客户端同步；AI 量大时先关 `m_bEnableAIStaminaCombatEffects` 或开 `m_bDisableAIAllCalc` 压测。
- 滑步复现：切勿临时打开水平物理钳；见已知问题文档。

---

## 6. 文档索引

| 文档 | 用途 |
|------|------|
| **本文件** | 上手与改动地图 |
| [RSS_CODING_STANDARDS.md](RSS_CODING_STANDARDS.md) | 编码规范（权威） |
| [RSS_v6_计算逻辑权威版.md](RSS_v6_计算逻辑权威版.md) | 生理/速度数学（权威） |
| [RSS_API.md](RSS_API.md) | 外部模组 API |
| [engine_api_usage.md](engine_api_usage.md) | 引擎 API 使用清单 |
| [RSS_AI_行为说明.md](RSS_AI_行为说明.md) | 现行 AI 行为 |
| [RSS_AI体力集成全盘设计方案.md](RSS_AI体力集成全盘设计方案.md) | **归档**设计稿，勿按路径改代码 |
| [RSS_已知问题_限速与滑步.md](RSS_已知问题_限速与滑步.md) | 限速与滑步约束 |
| [scripts_file_size_limit.md](scripts_file_size_limit.md) | 64 KB 硬限制与拆分 |
| [scripts_naming_and_layout_rules.md](scripts_naming_and_layout_rules.md) | 命名布局（冲突以 CODING_STANDARDS 为准） |
| [RSS_v6_优化管线设计.md](RSS_v6_优化管线设计.md) | 离线优化管线 |
| [CHANGELOG.md](../CHANGELOG.md) | 版本变更 |
---

## 7. PR 自检清单

- [ ] `check_script_size.py` / `check_enforce_syntax.py` 通过
- [ ] 无新增 `?:`；无净增已超限文件体积（尤其 `PlayerBase.c`）
- [ ] 公式在 Core；Integration 仅委托
- [ ] 限速走 SpeedBridge；未默认开物理水平钳 / 代谢帽（除非文档与常量同步说明）
- [ ] 相关 docs 或 CHANGELOG 已更新
- [ ] 写明官方 API 锚点，或「天气/体力拦截」例外理由

---

*对齐代码：6.1.x · 2026-08-09*
