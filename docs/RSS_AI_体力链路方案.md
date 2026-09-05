# RSS AI 专用体力链路（精度对齐方案）

> 版本对齐 **6.2.28**。实现：`SCR_RSS_AIStaminaPipeline` + `SCR_RSS_AISharedEnvCache`。

## 目标

在专服多 AI 场景下，**体力数值**尽量贴近玩家 RSS（Pandolf / CP–W′ / 负重 / 坡度），同时**绝不**走玩家级 `UpdateSpeed`、逐 AI 环境全链、CP 二次限速等热点。

## 开关语义（菜单文案 → 字段）

| 菜单项 | 字段 | AI 行为 |
|--------|------|---------|
| **Disable All AI RSS** = On | `m_bDisableAIAllCalc` | 不跑 RSS 循环（覆盖下列两项） |
| **Disable AI Stamina Drain** = On（默认） | `m_bDisableAIStaminaCalc` | 仅廉价限速（负重+相位） |
| **Disable AI Stamina Drain** = Off | 同上 = false | **本管线**：廉价限速 + 同源代谢消耗/恢复 |
| **AI Fatigue Behaviors** = On | `m_bEnableAIStaminaCombatEffects` | 状态机/意图/战斗衰减（需 Drain=Off） |

## 与玩家对照

| 环节 | 玩家 | AI 管线 |
|------|------|---------|
| 限速意图 | `UpdateSpeed` + CP 巡航帽 + 坡度射线 | `ApplyCheapAiSpeed`：绝对行军 m/s÷相位顶（keepSource）；默认不算消耗时冲刺压到 Run |
| 测速 | 位置差分 | 同 |
| 坡度 | 射线/环境 `CalculateGradePercent` | **Y 差分估 grade%** + 低通 |
| 地形系数 | 每 tick 射线 | 近 2s / 中 5s 稀采样；远距固定 1.0 |
| 热应激 | `EnvironmentFactor` 全链 | **全服 1Hz** TOD 抛物线近似 |
| 代谢功率 / 消耗 | `CalculateTotalDrainRate` | **同核** |
| W′ / 疲劳 | Phase B 全量 | 近中距同核；**远距跳过 W′/疲劳**（有氧仍算） |
| 游泳/湿重/跳跃/泥泞 | 有 | **无**（AI 场景极少需要） |
| 伤害联动 | `UpdateStaminaValue` 内 | 同 |
| 战斗 FSM | `AIManager.Tick` | 同（需 Combat 开） |

## 精度取舍（基于敏感度）

保留对油耗/限速影响最大的项：

1. **实测速度**（功率∝速度）
2. **负重**（限速 + 功率）
3. **坡度近似**（功率对坡度最敏感）
4. **同源 Pandolf→STA 记账**

可接受损失：

- 坡度无射线 → 台阶/短坡略钝，长坡趋势对齐
- 地形稀采样 / 远距 1.0 → 泥地长时间远距 AI 略省油
- 热应激简化 → 主要影响 CP/W′ 分流，有氧 STA 差通常很小
- 无 `UpdateSpeed` 连续拧速 → STA≥5% 时玩家本就不拧速；跛行仍走廉价限速

## 调用路径

`PlayerBase_UpdateLoop` Phase A：若非玩家且未禁消耗 → `m_pAIStaminaPipeline.Tick(ctx)` → `ScheduleNext`，**不进** Phase B/C。

## LOD

沿用 `RSS_PERF_AI_LOD_*` 刷新间隔 + CallLater 错峰；远距（>1200 m）跳过 W′/疲劳与热应激。
