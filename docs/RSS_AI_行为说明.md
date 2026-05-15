# RSS 模组下 AI 受影响说明（v3.23+）

本文说明 **Realistic Stamina System (RSS)** 对 **AI 角色** 的行为与实现入口。旧版 `SCR_RSS_AIStaminaBridge`、`SCR_RSS_AIGroupRestCoordinator`、`SCR_RSS_AIRestRecoveryRegistry` 等文件已在 v3.23.0 重构中移除，由下列模块替代。

## 1. 实现文件

| 文件 | 职责 |
|------|------|
| `scripts/Game/Integration/PlayerBase.c` | 体力主循环、AI 桥接链、群组代理 gate |
| `scripts/Game/Integration/SCR_AIGroup_RSS.c` | `modded SCR_AIGroup`：注册/注销 GroupSync 路点事件 |
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaState.c` | 6 态体力状态机（滞回） |
| `scripts/Game/RSS/AI/SCR_RSS_AISpeedCap.c` | 移动类型 + `OverrideMaxSpeed` |
| `scripts/Game/RSS/AI/SCR_RSS_AIIntentFilter.c` | 力竭时压低 Attack/追击，恢复时还原优先级 |
| `scripts/Game/RSS/AI/SCR_RSS_AICombatDecay.c` | 感知 / 射速 / 技能衰减 |
| `scripts/Game/RSS/AI/SCR_RSS_AIGroupSync.c` | 路点预扫描、休息路点插入、自适应队长步速 |
| `scripts/Game/RSS/AI/SCR_RSS_AIGroupStaminaProxy.c` | 远距非交战群组：仅队长全量算体力 |
| `scripts/Game/RSS/AI/SCR_RSS_AIInjuryLink.c` | 受伤 → 消耗加速 / 恢复减慢 |
| `scripts/Game/RSS/Core/SCR_StaminaConstants.c` | `RSS_AI_*`、`RSS_PERF_AI_*` 常量 |

## 2. 配置开关（`SCR_RSS_Settings`）

| 字段 | 作用 |
|------|------|
| `m_bEnableAIStaminaCombatEffects` | **总开关**：状态机、限速、意图过滤、战斗衰减、群组协同、伤害联动 |
| `m_bDisableAIAllCalc` | 服端 AI 完全不跑 RSS 主循环 |
| `m_bDisableAIStaminaCalc` | 仍算速度倍率，跳过 Pandolf 消耗/恢复 |
| `m_bEnableMudSlipMechanism` | 泥泞滑倒（默认关，与 AI 泥泞巡逻逻辑独立） |

## 3. 每 tick 顺序（服端 AI）

在 `UpdateSpeedBasedOnStamina` 中（与玩家共用 Pandolf 环境模型）：

1. **群组代理**（远距 >800m、非交战）：队员同步队长体力与速度后 `return`；队长每 5s 全量算一次消耗/恢复。
2. 速度倍率 → `OverrideMaxSpeed`
3. **AI 桥接**（500ms 节流）：状态机 → 限速 → 意图过滤 → 战斗衰减 → 休息路点提前完成
4. Pandolf 消耗/恢复 → `UpdateStaminaValue`（含 **伤害联动** 倍率）

## 4. 体力状态机

| 状态 | 约略体力 | 移动 | 战斗 |
|------|----------|------|------|
| FRESH | ≥80%（滞回） | 不干预 | 100% |
| WINDED | 50–80% | 禁 Sprint | 略降 |
| FATIGUED | 25–50% | RUN + 65% 速 | 明显衰减 |
| EXHAUSTED | 10–25% | WALK + 40% 速 | 强衰减；非 THREATENED 禁 Attack/追击 |
| COLLAPSED | <10% | 近乎 IDLE | 最重衰减 |
| RECOVERING | 回升中 | WALK + 连续曲线 | 中等衰减 |

`THREATENED` 时不应用移动限速（保命优先）。

静止时长：静止累计用于 `COLLAPSED→RECOVERING`（3s）与极低体力强制恢复（5s）；移动时清零。

## 5. 群组协同（事件驱动）

`SCR_AIGroup_RSS` 在群组 `EOnInit` 时调用 `SCR_RSS_AIGroupSync.RegisterForGroup`。

| 事件 | 行为 |
|------|------|
| 路点添加 | 12 方向射线预扫描掩体休息点并缓存 |
| 路点完成 | 按群组中位数体力插入 `RSS_AIRest` Wait/Defend；自适应队长步速 |
| 休息路点进行中 | 中位数体力 ≥70% 时提前 `CompleteWaypoint` |

群组体力分级：`GROUP_FIT` ≥70% / `GROUP_TIRING` 40–70% / `GROUP_FATIGUED` 20–40% / `GROUP_SPENT` <20%。  
插入休息需全组非 `THREATENED`，且有 90s 冷却。

## 6. 性能：距离 LOD + 群组代理

| 机制 | 常量 | 说明 |
|------|------|------|
| 距离 LOD | `RSS_PERF_AI_DISTANCE_LOD_*` | 近 400m：200ms；中 1200m：300ms；远：1500ms |
| 群组代理 | `RSS_PERF_AI_GROUP_PROXY_*` | >800m 且非交战：队长 5s 全量，队员同步 |
| 交战缓存 | `RSS_PERF_AI_GROUP_BATTLE_CACHE_SEC` | 1s 内不重复扫描全组成员 |

`RSS_PERF_AI_DISTANCE_LOD_ENABLED = false` 时 AI 使用固定 `RSS_AI_SPEED_UPDATE_INTERVAL_MS`（100ms）。

## 7. 伤害联动

按 `SCR_CharacterBloodHitZone` 血量分段调整 `UpdateStaminaValue` 中的消耗与恢复倍率（见 `SCR_RSS_AIInjuryLink.c` 与 `RSS_AI_INJURY_*` 常量）。

---

*文档版本：v3.23 AI 补全实现。以仓库内上述脚本为准。*
