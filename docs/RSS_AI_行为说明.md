# RSS 模组下 AI 受影响说明（现行）

本文说明 **Realistic Stamina System (RSS)** 对 **AI 角色** 的行为与实现入口。  
以 `scripts/Game/RSS/AI/` 源码为准。旧版群组机动 / GroupSync / 群组代理等模块**已移除**；历史设计见归档稿 [`RSS_AI体力集成全盘设计方案.md`](RSS_AI体力集成全盘设计方案.md)。

## 0. 总体思想

- AI 与玩家共用同一套 Pandolf / CP–W′ 体力主循环（服端）。
- 在体力数值之上，另有 **个体** 行为层（约 500 ms）：状态机 → 限速 → 意图过滤 → 战斗衰减。
- **不侵入** 原生行为树；通过 `SetMovementTypeWanted` + `SCR_RSS_SpeedBridge` / `SetSpeedLimit` 间接约束。
- 群组协同（统一步速、休息路点、远距代理等）当前**未启用**（`SCR_RSS_AIManager` 内 `isThreatened` 固定为 false）。

## 1. 实现文件

| 文件 | 职责 |
|------|------|
| `scripts/Game/Integration/PlayerBase.c` | 持有 `SCR_RSS_AIManager`；主循环调用 `Tick` |
| `scripts/Game/RSS/AI/SCR_RSS_AIManager.c` | 行为层节流 + 状态机 + SpeedCap / IntentFilter / CombatDecay 编排 |
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaState.c` | 6 态体力状态机（滞回） |
| `scripts/Game/RSS/AI/SCR_RSS_AISpeedCap.c` | 移动类型 + 经 SpeedBridge 的限速 |
| `scripts/Game/RSS/AI/SCR_RSS_AIIntentFilter.c` | 力竭时禁用 Attack/追击类意图 |
| `scripts/Game/RSS/AI/SCR_RSS_AICombatDecay.c` | 感知 / 射速 / 技能衰减 |
| `scripts/Game/RSS/AI/SCR_RSS_AIInjuryLink.c` | 受伤 → 消耗加速 / 恢复减慢 |
| `scripts/Game/RSS/AI/SCR_RSS_AIUpdateInterval.c` | 距玩家距离 LOD 刷新间隔、Workbench 预览过滤 |
| `scripts/Game/RSS/AI/SCR_RSS_AIConstants.c` | `RSS_AI_*`、`RSS_PERF_AI_*` 常量 |

## 2. 配置开关（`SCR_RSS_Settings`）

| 字段 | 作用 |
|------|------|
| `m_bEnableAIStaminaCombatEffects` | **总开关**：状态机、限速、意图过滤、战斗衰减、伤害联动（新建 JSON 默认常为 false；服主可在菜单开启） |
| `m_bDisableAIAllCalc` | 服端 AI 完全不跑 RSS 主循环 |
| `m_bDisableAIStaminaCalc` | 仍可算速度相关，跳过 Pandolf 消耗/恢复 |
| `m_bEnableMudSlipMechanism` | 泥泞滑倒（默认关；与 AI 行为层独立） |

## 3. 每 tick 顺序（服端 AI）

1. 主循环按 `SCR_RSS_AIUpdateInterval` 间隔更新速度/消耗（玩家 ~17 ms；AI 距离 LOD 或固定 100 ms）。
2. `SCR_RSS_AIManager.Tick`（行为层 **500 ms** 节流）：
   - 静止时长累计
   - `SCR_RSS_AIStaminaState.Tick`
   - `SCR_RSS_AISpeedCap.Apply`
   - `SCR_RSS_AIIntentFilter.Apply`
   - `SCR_RSS_AICombatDecay.Apply`
3. Pandolf 消耗/恢复 → `UpdateStaminaValue`（可含 **伤害联动** 倍率，见 InjuryLink）。

## 4. 体力状态机

| 状态 | 约略体力 | 移动 | 战斗 |
|------|----------|------|------|
| FRESH | ≥80%（滞回） | 不干预 | 100% |
| WINDED | 50–80% | 禁 Sprint | 略降 |
| FATIGUED | 25–50% | RUN + 约 65% 速 | 明显衰减 |
| EXHAUSTED | 10–25% | WALK + 约 40% 速 | 强衰减；可禁 Attack/追击 |
| COLLAPSED | <10% | 近乎 IDLE | 最重衰减 |
| RECOVERING | 回升中 | WALK + 连续曲线 | 中等衰减 |

阈值与倍率以 `SCR_RSS_AIConstants` 为准。  
静止累计用于 `COLLAPSED→RECOVERING` 与极低体力强制恢复；移动时清零。

## 5. 性能：距离 LOD

| 机制 | 常量 | 说明 |
|------|------|------|
| 距离 LOD | `RSS_PERF_AI_*` | 近 400 m：200 ms；中 1200 m：300 ms；远：1500 ms |
| LOD 总开关 | `RSS_PERF_AI_DISTANCE_LOD_ENABLED` | `false` 时 AI 固定 `RSS_AI_SPEED_UPDATE_INTERVAL_MS`（100 ms） |

## 6. 伤害联动

按血量分段调整消耗与恢复倍率（`SCR_RSS_AIInjuryLink.c` 与 `RSS_AI_INJURY_*`）。

---

*文档版本：2026-08-09，对齐现行 `SCR_RSS_AIManager` 个体管线。*
