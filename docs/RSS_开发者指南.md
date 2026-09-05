# RSS 开发者指南

> **中文** | [English](en/DEVELOPER_GUIDE.md)
>
> 面向本仓库贡献者与二次开发者。当前对齐 **6.1.x**。玩家向说明见根目录 `README_CN.md`。  
> 英文文档索引：[docs/en/README.md](en/README.md)

## 1. 快速定位

| 你想做的事 | 从这里开始 |
|------------|------------|
| 改代谢 / CP–W′ / 恢复公式 | [`RSS_v6_计算逻辑权威版.md`](RSS_v6_计算逻辑权威版.md) → `scripts/Game/RSS/Core/` |
| 改限速 / 步态 / 滑步相关 | [`RSS_已知问题_限速与滑步.md`](RSS_已知问题_限速与滑步.md) → `SCR_RSS_SpeedBridge` / `SCR_RSS_Constants` |
| 改引擎 API 用法 | [`engine_api_usage.md`](engine_api_usage.md) |
| 改编码 / 分层 | [`RSS_CODING_STANDARDS.md`](RSS_CODING_STANDARDS.md) |
| 拆大文件 | [`scripts_file_size_limit.md`](scripts_file_size_limit.md) |
| 改 AI 行为 | [`RSS_AI_行为说明.md`](RSS_AI_行为说明.md) → `scripts/Game/RSS/AI/` |
| 对外模组读体力 | [`RSS_API.md`](RSS_API.md) |
| 标定 / 数字孪生 | [`../tools/README.md`](../tools/README.md)、[`RSS_v6_优化管线设计.md`](RSS_v6_优化管线设计.md) |

**冲突时优先级**：源码 > 本指南 / 计算逻辑权威版 > README 历史叙述 > 归档设计稿。

归档勿当实现依据：[`archive/RSS_AI体力集成全盘设计方案.md`](archive/RSS_AI体力集成全盘设计方案.md)。
现行 AI：[`RSS_AI_体力链路方案.md`](RSS_AI_体力链路方案.md)、[`RSS_AI_行为说明.md`](RSS_AI_行为说明.md)。
文档总索引：[`README.md`](README.md)。

---

## 2. 环境

- **游戏**：Arma Reforger **1.7+**；用 **Workbench** 打开本 addon（`addon.gproj`）。
- **原生对照**：引擎脚本参考仓库（若本机有）`C:\Users\74738\Documents\arma_reforger_code`。
- **Python 工具**（可选）：`tools/` 下 `pip install -r requirements.txt`；见 [`../tools/README.md`](../tools/README.md)。

模组 ID / GUID：`Realistic Stamina System` / `68649101601CC93D`。配置版本见 `SCR_RSS_ConfigManager.CURRENT_VERSION`。

---

## 3. 仓库地图

```text
scripts/Game/
  Integration/          # modded 入口：PlayerBase.c + PlayerBase_UpdateLoop.c（同 class 仅这两文件）；
                        # 另有引擎顶速采样助手 SCR_PlayerBaseEngineTopSampler.c（独立 class）
  RSS/
    Core/               # 代谢、CP–W′、消耗/恢复、速度、协调器、常量
    Environment/        # 天气/地形/惩罚（自建栈；引擎只采样）
    NetworkConfig/      # Settings / Params / API / 同步
    AI/                 # 个体状态机与限速/意图/战斗衰减
    Presentation/       # HUD、屏效、相机
    MudSlip/            # 泥泞滑倒
    Items/              # 注射器等
  Components/ Damage/ UserActions/   # 其它 modded / 扩展
Prefabs/                # 预制覆盖（如 Character_Base 负重上限）
docs/                   # 本目录
tools/                  # 孪生、校验、优化管线
```

### 主循环（心智模型）

```text
PlayerBase_UpdateLoop
  → 测速 v_meas
  → SCR_RSS_UpdateCoordinator（代谢 → CP/W′ → 消耗/恢复 → 速度意图）
  → SCR_RSS_SpeedBridge.SetSpeedLimit（与灌木等 min 合并）
  → SCR_StaminaOverride（有氧权威 → 引擎条）
  → SCR_RSS_SprintGate（可选：W′ → transient GetStamina / Exhaustion 表现）
```

**默认限速策略（v6.1.7 起）**：`V6_APPLY_CP_METABOLIC_SPEED_CAP = true`（W′ 耗尽后经 `SetSpeedLimit` 压 CP 巡航指令速度；≤6.1.5 为 drain-only：超额只扣 STA/W′）。巡航帽只写在当前步态带内；掉出 Run 带则不把 Walk 速度写进 Run，W′ 空时改切引擎 Walk 档（`V6_CP_OUT_OF_BAND_WALK_OVERRIDE`）。物理硬钳保持关闭，禁止为「贴限」拧 `Physics` 速度（滑步）。

---

## 4. 改代码时必须遵守

细则以 [`RSS_CODING_STANDARDS.md`](RSS_CODING_STANDARDS.md) 为准，摘要：

1. **禁止** EnforceScript 三元 `?:`；单行 `if` 必须 `{}`。
2. **文件大小非崩溃原因**（不设 64 KB 硬上限）；`PlayerBase.c` 偏大，宜继续外移、勿再堆逻辑。
3. **Integration 薄壳**：公式放 `RSS/Core/` 等；勿在 `PlayerBase` 内联 Pandolf。
4. 限速只走 **`SCR_RSS_SpeedBridge` → `SetSpeedLimit`**，勿单独 `OverrideMaxSpeed` 盖掉灌木减速。
5. **有氧权威**在 `m_fTargetStamina`；W′ **不改**有氧权威；表现可用 `ApplyTransientEngineStamina`。
6. CPR 等「关掉原生体力组件」的兼容 **不在本模组**；另做 compat 模组。
7. 命名：`SCR_RSS_*` / `ERSS_*` / `RSS_*` DTO；文件名与主类一致。

---

## 5. 常见改动入口

| 主题 | 关键文件 |
|------|----------|
| 试跑开关 / 生理常量 | `SCR_RSS_Constants.c`、`SCR_RSS_AIConstants.c`、`SCR_RSS_EnvConstants.c` |
| 消耗协调 | `SCR_RSS_UpdateCoordinator.c` |
| 速度意图 / 反解 | `SCR_RSS_SpeedCalculator.c`、`SCR_RSS_DrainCalculator.c` |
| CP–W′ | `SCR_RSS_CriticalPowerModel.c`、`SCR_RSS_AnaerobicBurst.c` |
| 有氧拦截壳 | `SCR_StaminaOverride.c`（保持薄） |
| W′→晃动/模糊 | `SCR_RSS_SprintGate.c` |
| 服主配置 | `SCR_RSS_Settings.c`、Bake / `tools/optimized_rss_config_*_v6.json` |
| AI | `SCR_RSS_AIManager.c` 及同目录模块 |
| 泥泞 | `SCR_RSS_MudSlipRunner.c`、`SCR_RSS_MudSlipEffects.c` |

改预设数值：优先走工具管线再 `embed_json_to_c`（见 tools README），避免手改 Bake 与 JSON 长期分叉。

---

## 6. 提交前检查

在仓库根目录（Windows PowerShell）：

```powershell
python tools/check_script_size.py
python tools/check_enforce_syntax.py
python tools/test_v6_smoke.py
python tools/rss_pipeline_v6.py validate
```

Workbench：编译本 addon → 单机冲刺/恢复 →（若动配置）看服端同步。

最小手测清单：

- [ ] 空载平地 Walk / Run / Sprint
- [ ] ~30 kg 缓坡续航与停步 EPOC（勿暴罚）
- [ ] 灌木区限速仍生效（RSS 未盖掉 Foliage）
- [ ] W′ 耗尽后晃动/模糊（若未关 `V6_WPRIME_ENGINE_FX_ENABLED`）
- [ ]（若动 AI）开启 `m_bEnableAIStaminaCombatEffects` 后状态限速

---

## 7. 文档索引

双语入口：[docs/en/README.md](en/README.md)。

| 中文 | English | 用途 |
|------|---------|------|
| [`RSS_CODING_STANDARDS.md`](RSS_CODING_STANDARDS.md) | [`en/CODING_STANDARDS.md`](en/CODING_STANDARDS.md) | 编码权威 |
| [`scripts_file_size_limit.md`](scripts_file_size_limit.md) | [`en/SCRIPT_FILE_SIZE_LIMIT.md`](en/SCRIPT_FILE_SIZE_LIMIT.md) | 编译崩溃排查（壳子法） |
| [`RSS_v6_计算逻辑权威版.md`](RSS_v6_计算逻辑权威版.md) | [`en/V6_CALCULATION_LOGIC.md`](en/V6_CALCULATION_LOGIC.md) | 数学权威 |
| [`engine_api_usage.md`](engine_api_usage.md) | [`en/ENGINE_API_USAGE.md`](en/ENGINE_API_USAGE.md) | 引擎 API 清单 |
| [`RSS_已知问题_限速与滑步.md`](RSS_已知问题_限速与滑步.md) | [`en/KNOWN_ISSUES_SPEED_SLIP.md`](en/KNOWN_ISSUES_SPEED_SLIP.md) | 限速/滑步已知约束 |
| [`RSS_AI_行为说明.md`](RSS_AI_行为说明.md) | [`en/AI_BEHAVIOR.md`](en/AI_BEHAVIOR.md) | AI 现行行为 |
| [`RSS_API.md`](RSS_API.md) | [`en/API.md`](en/API.md) | 外部模组 API |
| [`RSS_开发者指南.md`](RSS_开发者指南.md) | [`en/DEVELOPER_GUIDE.md`](en/DEVELOPER_GUIDE.md) | 开发入口 |
| [`scripts_naming_and_layout_rules.md`](scripts_naming_and_layout_rules.md) | — | 命名/分层补充（暂仅中文） |
| [`灌木丛移动减速机制.md`](灌木丛移动减速机制.md) | — | 原生灌木 + RSS 合并（暂仅中文） |
| [`泥泞滑倒判定模型.md`](泥泞滑倒判定模型.md) | — | 泥泞模型（暂仅中文） |
| [`RSS_v6_优化管线设计.md`](RSS_v6_优化管线设计.md) | — | 优化管线设计（暂仅中文） |
| [`../CHANGELOG.md`](../CHANGELOG.md) | same | 版本变更 |

---

## 8. 不要做的事

- 为对齐 `v_limit` 去每帧改水平 `Physics` 速度。
- 在 Integration 继续膨胀 `PlayerBase.c`。
- 把业务公式写进 `SCR_StaminaOverride`。
- 按归档 AI 全盘设计稿里的旧类名新建文件。
- 提交含密钥 / 本机绝对路径的临时实验配置（除非文档明确需要）。

---

*维护：与 6.1.x 源码同步；大行为变更时先改权威计算/编码文档，再改本指南索引。*
