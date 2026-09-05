# RSS 模组下 AI 受影响说明（现行）

> **中文** | [English](en/AI_BEHAVIOR.md)

本文说明 **Realistic Stamina System (RSS)** 对 **AI 角色** 的行为与实现入口。  
以 `scripts/Game/RSS/AI/` 源码为准。旧版群组机动 / GroupSync / 群组代理等模块**已移除**；历史设计见 [`archive/RSS_AI体力集成全盘设计方案.md`](archive/RSS_AI体力集成全盘设计方案.md)。  
**AI 专用体力精度方案**见 [`RSS_AI_体力链路方案.md`](RSS_AI_体力链路方案.md)。

## 0. 总体思想

- **数值层**：`DisableAIStaminaCalc=Off` 时走 `SCR_RSS_AIStaminaPipeline`——与玩家共用 Pandolf / CP–W′ / `UpdateStaminaValue` 核，但避开 `UpdateSpeed` 与逐 AI 环境全链。
- **限速层**：廉价（负重意图项 + 相位 / 跛行）+ **Tobler**；开消耗时再套 CP 巡航 / Sprint 反解。未开 Fatigue Behaviors 时冲刺意图压到 Run。
- **行为层**（可选）：状态机 → SpeedCap → 意图过滤 → 战斗衰减（需 Combat 开）。
- **不侵入** 原生行为树；通过 Agent `MovementSpeedSetting`（SCENARIO）+ `SetMovementTypeWanted` + SpeedBridge 间接约束。

## 1. 实现文件

| 文件 | 职责 |
|------|------|
| `scripts/Game/Integration/PlayerBase_UpdateLoop.c` | AI 分支：轻量限速 or `AIStaminaPipeline.Tick` |
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaPipeline.c` | AI 专用消耗/恢复/W′/疲劳管线 |
| `scripts/Game/RSS/AI/SCR_RSS_AISharedEnvCache.c` | 全服 1Hz 热应激近似 |
| `scripts/Game/RSS/Core/SCR_RSS_UpdateLoopDebugOutput.c` | `[RSS][AI-SPD]` 近距限速诊断（生产默认关） |
| `scripts/Game/Integration/SCR_PlayerBaseAiLightTickHelper.c` | 计算层目标 m/s → 交应用层 |
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
| **Disable AI Stamina Drain** | `m_bDisableAIStaminaCalc` | **On**（默认）：廉价限速 + Tobler，冲刺意图压到 Run。**Off**：消耗管线 + Tobler + CP 帽。未开 Fatigue Behaviors 时仍压冲刺到 **Run**（避免 BT 冲刺拉开慢跑玩家）；Behaviors On 且 W′ 允许才给 Sprint 顶 |
| Mud Slip Mechanic | `m_bEnableMudSlipMechanism` | 泥泞（玩家侧；AI 管线不算泥泞） |

## 3. 每 tick 顺序（服端 AI，`DisableAIStaminaCalc=Off`）

1. Phase A：`AIStaminaPipeline.Tick`
   - 位置测速 → Y 估坡 → LOD 地形采样 → 共享热应激
   - CP 上下文 → **W′ TickPower** → 廉价限速（Tobler + CP 巡航闩 / Sprint）
   - `CalculateTotalDrainRate` → 疲劳积分（近中）→ `UpdateStaminaValue`
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
| 全量 LOD | 近/中/远 200 / 800 / 2500 ms（算消耗时） |
| 轻量 LOD | 250 / 1200 / 3000 ms（仅限速） |
| 地形采样 | 近 2 s / 中 5 s；远距固定 terrain=1 |
| 错峰 | CallLater 最多 180 ms |
| 玩家原点缓存 | 0.25 s 全服共享 |

彻底停循环：勾选 `m_bDisableAIAllCalc`。

### 相对 6.2.6（PerfProbe）

**默认**（不算消耗）：AI 同栈 ~48→`03f`≈16.7，约 **快 2.9× / 省 ~65%**；玩家限速栈约 **快 3.3× / 省 ~69%**。  
**开 drain**：相对旧 AI 全栈 B≈55→~22，约 **快 2.5× / 省 ~60%**。

详见根目录 [CHANGELOG.md](../CHANGELOG.md) **[6.3.0]**。

## 6. 伤害联动

按血量分段调整消耗与恢复（`SCR_RSS_AIInjuryLink`，在 `UpdateStaminaValue` 内应用）。

---

*文档版本：2026-09-06，对齐 6.3.0（AI 群组 Override 按成员 min 钉步态；生产默认关诊断）。*
