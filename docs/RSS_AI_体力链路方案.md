# RSS AI 专用体力链路（精度对齐方案）

> 版本对齐 **6.2.37**。实现：`SCR_RSS_AIStaminaPipeline` + `SCR_RSS_AISharedEnvCache` + `SCR_RSS_EngineReuse` + `SCR_RSS_AIMovementApply`。

## 目标

在专服多 AI 场景下，**体力数值**尽量贴近玩家 RSS（Pandolf / CP–W′ / 负重 / 坡度），**脚程**在开消耗时用轻量 Tobler/CP 帽靠近玩家，同时**绝不**走玩家级 `UpdateSpeed` 全链。

## 官方复用优先级（6.2.37+）

| 量 | 优先 | 回退 |
|----|------|------|
| 坡度 | `CommandMove.GetMovementSlopeAngle` → `GetFloorNormal` | Y 差分（AI）/ Trace（玩家） |
| 测速（陆地） | `CharacterController.GetVelocity`（官方案例）→ `GetVelocityWS`/`GetRawVelocityWS` | **禁止位置差分**；AI 代谢低估时用意图限速 |
| 测速（游泳） | `GetVelocityWS` | 允许位置差分 |
| 地形系数 | `GetFloorSurface`→材质表 | Trace（稀采样；FloorSurface 常空属常态） |
| 热应激 | 全服 1Hz TOD | — |
| 室内抑坡 | `UpdateIndoorCache`（2s） | 热路径禁止每 tick 屋顶射线（玩家） |
| AI 步态 | Agent `MovementSpeedSetting`（Origin=**SCENARIO**，巡航钉死固定 Setting）+ `SetMovementTypeWanted` | 角色层 `SetSpeedLimit` / `SetMovementMaxSpeed` |

## 开关语义（菜单文案 → 字段）

| 菜单项 | 字段 | AI 行为 |
|--------|------|---------|
| **Disable All AI RSS** = On | `m_bDisableAIAllCalc` | 不跑 RSS 循环（覆盖下列两项） |
| **Disable AI Stamina Drain** = On（默认） | `m_bDisableAIStaminaCalc` | 廉价限速 + Tobler；离线 vs 玩家约均差 **15%** / 最大 **~79%** |
| **Disable AI Stamina Drain** = Off | 同上 = false | 管线：消耗 + Tobler + CP/Sprint；离线 vs 玩家约均差 **1%** / 最大 **~2%** |
| **AI Fatigue Behaviors** = On | `m_bEnableAIStaminaCombatEffects` | 状态机/意图/战斗衰减（需 Drain=Off） |

## 与玩家对照

| 环节 | 玩家 | AI 管线 |
|------|------|---------|
| 限速意图 | `UpdateSpeed` 全伺服 | **计算**绝对 m/s；**应用** Agent Setting（SCENARIO；巡航固定钉 WALK）+ `SetSpeedLimit` / `SetMovementMaxSpeed` |
| 测速 | VelocityWS → 位置差分 | 同 |
| 坡度 | CmdSlope → FloorNormal → Trace | CmdSlope → FloorNormal → Y 差分（无 Trace） |
| 地形系数 | FloorSurface → Trace | 同优先；稀采样 |
| 热应激 | `EnvironmentFactor` | 全服 1Hz TOD |
| 代谢 / W′ | 全量 | 近中同核；远距跳过 W′/疲劳 |

## 调用路径

`PlayerBase_UpdateLoop` Phase A：若非玩家且未禁消耗 → 测速/坡度/地形 → **TickPower** → `ApplyCheapAiSpeedEx` → 有氧消耗 → `ScheduleNext`，**不进** Phase B/C。
