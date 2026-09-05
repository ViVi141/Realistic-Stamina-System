# RSS 模组下 AI 受影响说明（现行）

> **中文** | [English](en/AI_BEHAVIOR.md)

本文说明 **Realistic Stamina System (RSS)** 对 **AI 角色** 的行为与实现入口。  
以 `scripts/Game/RSS/AI/` 源码为准。旧版群组机动 / GroupSync / 群组代理等模块**已移除**；历史设计见 [`archive/RSS_AI体力集成全盘设计方案.md`](archive/RSS_AI体力集成全盘设计方案.md)。  
**AI 专用体力精度方案**见 [`RSS_AI_体力链路方案.md`](RSS_AI_体力链路方案.md)。

## 0. 总体思想

- **数值层**：`DisableAIStaminaCalc=Off` 时走 `SCR_RSS_AIStaminaPipeline`——与玩家共用 Pandolf / CP–W′ / `UpdateStaminaValue` 核，但避开 `UpdateSpeed` 与逐 AI 环境全链。
- **限速层**：廉价（负重 + 相位 / 跛行）+ **Tobler 坡度**；开消耗时再套 CP 巡航 / Sprint 反解（与玩家脚程靠近，仍不跑 `UpdateSpeed`）。

- **行为层**（可选）：状态机 → SpeedCap → 意图过滤 → 战斗衰减（需 Combat 开）。
- **不侵入** 原生行为树；通过 `SetMovementTypeWanted` + SpeedBridge 间接约束。

## 1. 实现文件

| 文件 | 职责 |
|------|------|
| `scripts/Game/Integration/PlayerBase_UpdateLoop.c` | AI 分支：轻量限速 or `AIStaminaPipeline.Tick` |
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaPipeline.c` | AI 专用消耗/恢复/W′/疲劳管线 |
| `scripts/Game/RSS/AI/SCR_RSS_AISharedEnvCache.c` | 全服 1Hz 热应激近似 |
| `scripts/Game/Integration/SCR_PlayerBaseAiLightTickHelper.c` | 廉价限速 |
| `scripts/Game/RSS/AI/SCR_RSS_AIManager.c` | 行为层节流 + FSM 编排 |
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaState.c` | 6 态体力状态机 |
| `scripts/Game/RSS/AI/SCR_RSS_AISpeedCap.c` | 移动类型 + 限速 |
| `scripts/Game/RSS/AI/SCR_RSS_AIIntentFilter.c` | 力竭时禁用 Attack/追击 |
| `scripts/Game/RSS/AI/SCR_RSS_AICombatDecay.c` | 感知 / 射速 / 技能衰减 |
| `scripts/Game/RSS/AI/SCR_RSS_AIInjuryLink.c` | 受伤 → 消耗/恢复倍率 |
| `scripts/Game/RSS/AI/SCR_RSS_AIUpdateInterval.c` | 距离 LOD |
| `scripts/Game/RSS/AI/SCR_RSS_AIConstants.c` | `RSS_AI_*` / `RSS_PERF_AI_*` |

## 2. 配置开关（`SCR_RSS_Settings`）

| 菜单文案 | 字段 | 作用 |
|----------|------|------|
| **AI Fatigue Behaviors** | `m_bEnableAIStaminaCombatEffects` | 状态机、步态限速、意图过滤、战斗衰减、伤势联动（需消耗开启） |
| **Disable All AI RSS** | `m_bDisableAIAllCalc` | 服端 AI 完全不跑 RSS 主循环 |
| **Disable AI Stamina Drain** | `m_bDisableAIStaminaCalc` | **On**（默认）：廉价限速 + Tobler。**Off**：消耗管线 + Tobler + CP/Sprint 帽 |
| Mud Slip Mechanic | `m_bEnableMudSlipMechanism` | 泥泞（玩家侧；AI 管线不算泥泞） |

## 3. 每 tick 顺序（服端 AI，`DisableAIStaminaCalc=Off`）

1. Phase A：`AIStaminaPipeline.Tick`
   - 廉价限速 → 位置测速 → Y 估坡 → LOD 地形采样 → 共享热应激
   - `CalculateTotalDrainRate` → W′（近中）→ 疲劳积分（近中）→ `UpdateStaminaValue`
   - `AIManager.Tick`（Combat 开时，行为 LOD 节流）
2. 直接 `ScheduleNext`，**不进**玩家 Phase B/C。

`DisableAIStaminaCalc=On`：仅 `ApplyCheapAiSpeed` 后结束。

## 4. 体力状态机

| 状态 | 约略体力 | 移动 | 战斗 |
|------|----------|------|------|
| FRESH | ≥80%（滞回） | 不干预 | 100% |
| WINDED | 50–80% | 禁 Sprint | 略降 |
| FATIGUED | 25–50% | RUN + 约 65% 速 | 明显衰减 |
| EXHAUSTED | 10–25% | WALK + 约 40% 速 | 强衰减；可禁 Attack/追击 |
| COLLAPSED | <10% | 近乎 IDLE | 最重衰减 |
| RECOVERING | 回升中 | WALK + 连续曲线 | 中等衰减 |

## 5. 性能：距离 LOD

| 机制 | 说明 |
|------|------|
| 全量 LOD | 近/中/远 600 / 1000 / 2500 ms（算消耗时） |
| 轻量 LOD | 800 / 1500 / 3000 ms（仅限速） |
| 地形采样 | 近 2 s / 中 5 s；远距固定 terrain=1 |
| 错峰 | CallLater 最多 180 ms |
| 玩家原点缓存 | 0.25 s 全服共享 |

彻底停循环：勾选 `m_bDisableAIAllCalc`。

## 6. 伤害联动

按血量分段调整消耗与恢复（`SCR_RSS_AIInjuryLink`，在 `UpdateStaminaValue` 内应用）。

---

*文档版本：2026-09-06，对齐 6.2.32（AI Tobler/CP 脚程对齐）。*
