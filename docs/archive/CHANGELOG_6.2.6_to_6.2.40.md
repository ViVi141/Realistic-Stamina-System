# 6.2.6 – 6.2.40 detailed changelog (folded into 6.3.0)

> Split from root CHANGELOG. Public release is **6.3.0**. Summary: [/CHANGELOG.md](../../CHANGELOG.md).

## [6.2.40] - 2026-09-06

### 生产默认：关闭调试日志

- Workbench 嵌入预设不再强制 Debug/HUD/DataExport。
- `RSS_AI_SPEED_DIAG_ENABLED` 默认 **false**（不再刷 `[RSS][AI-SPD]`）。
- 配置版本 → **6.2.40**

## [6.2.39] - 2026-09-06

### AI 限速：群组 Override 成员 min 聚合

- **日志** — `wanted=WALK` 但 `ovr=RUN`、`engPh=2`、`v` 仍 ≈3.8：群组 Override 未落到 Walk。
- **根因** — 6.2.38 已写群组钉，但任一未疲劳队友 tick 会把群组 Setting 抬回 **RUN**，盖掉疲劳成员的 WALK。
- **修复** — 群组帽 = 各成员最近 `maxGait` 的 **min**；仅 IDLE/WALK 安装 SCENARIO 固定 Setting；成员均允许 RUN+ 时**撤钉**还给 BT。诊断增加 `grpCap`。
- 配置版本 → **6.2.39**

## [6.2.38] - 2026-09-06

### AI 限速：钉群组 Override（wanted=WALK 仍 ovr=RUN）

- **日志根因** — 单体 `SetMovementTypeWanted(WALK)` 成功，但 `GetMovementTypeOverride()` 仍为 **RUN**：群组 `SetGroupCharactersWantedMovementType` 盖掉单体。
- **修复** — 同步写 `AIGroupMovementComponent` + `SCR_AIGroupCharactersMovementSpeedSetting`（SCENARIO）；先群组后单体。
- 诊断增加 `grpOK`。配置版本 → **6.2.38**

## [6.2.37] - 2026-09-06

### AI 限速：官方步态钉死（压过航点）

- **根因** — Setting Origin 用 `COMMANDING`(3000) 时，航点 `WAYPOINT`(4000) 仍可盖掉限速；Range 上限 WALK 也不如固定 Setting 稳。
- **应用层** — Origin 升为 **SCENARIO**；巡航/跛行/IDLE 用 `SCR_AICharacterMovementSpeedSetting` **钉死**步态；RUN/SPRINT 仍用 Range。清掉旧 COMMANDING 残留。
- **重钉** — 角色层写完后再 `ForceMovementTypeWanted`；近距 LOD 200/250ms。
- **诊断** — `[RSS][AI-SPD]` 增加 `ovr` / `fixed`。
- 配置版本 → **6.2.37**

## [6.2.36] - 2026-09-06

### 陆地测速：禁止位置差分；改用官方案例 GetVelocity

- **硬约束** — 陆地计算/debug **不用**位置差分；禁 `Physics.GetVelocity`。
- **权威** — 与官方一致优先 `CharacterController.GetVelocity()`（相机 bob / 铁丝网等）；其次 `GetVelocityWS` / `GetRawVelocityWS`。超 `GAME_MAX_SPEED` **钳制**，不再整段判失败变成 0。
- **游泳** — 仍可 `allowPositionDelta=true`（游泳时 Controller 速度常为 0）。
- **AI 代谢** — WS/测速低估时按意图限速记账烧 W′；真 Idle 不虚烧。
- **AI 限速分母** — `SetSpeedLimit` 必须除以**当前 engPh 顶速**。巡航闩 WALK 但相位仍 Run 时，若按 walkTop 算 `frac≈1` 会乘 Run 顶 → 假「未限速」。
- **AI 限速落地** — 仅 `SetSpeedLimit` 对 AI 不够（`frac=0.64` 仍 `v=3.5`）。并行 `SetMovementMaxSpeed(绝对m/s)`；巡航/跛行再钉玩家同款 `SetDynamicSpeed(0.5)` Walk 覆盖。
- Debug 去掉 `v_pos`。
- 配置版本 → **6.2.36**

## [6.2.35] - 2026-09-06

### AI 限速应用层：Agent MovementSpeed Setting（抗 BT）

- **根因（应用层）** — AI 原生只有离散步态 `SetMovementTypeWanted`；BT `SCR_AICharacterSetMovementSpeed` 每帧重写 Wanted。只写 `SetSpeedLimit`/`OverrideMaxSpeed` 时，若仍停在 Sprint 相位，分数乘的是冲刺顶 → 看起来「原速」。Settings 组件在 **AIAgent** 上，以前在角色实体上 `FindComponent` 永远找不到。
- **计算层** — 仍算绝对 `targetMs`（负重/Tobler/CP 巡航闩）。
- **应用层** — 新 `SCR_RSS_AIMovementApply`：`SCR_AICharacterMovementSpeedSetting_Range`（Origin=COMMANDING）持久裁剪 BT → 再按该步态引擎顶写 `SetSpeedLimit`。巡航闩/跛行 → 最高 **WALK**。
- **AI 限速诊断日志** — `[RSS][AI-SPD]` 默认开（近距≤80m / 约2s）：分 CALC / APPLY / CFG 三行；`RSS_AI_SPEED_DIAG_ENABLED` 可关。
- 配置版本 → **6.2.35**

## [6.2.34] - 2026-09-06

### AI CP–W′ 巡航：先烧池再限速 + 即时限速

- **根因** — 管线先 `ApplyCheap` 再 `TickPower`：W′ 见底当 tick 仍按满 Run 顶；LOD 间隔内玩家已巡航、AI 仍原速。另：引擎 `SetSpeedLimit` 减速默认缓动，AI 600ms tick 下长时间停在满速。
- **顺序** — 测速 → CP 上下文 → **TickPower** → **ApplyCheap**（本 tick 即可闩巡航）。
- **W′** — 远距也算（不再因 farLod 跳过），否则近旁 AI 可能永远不见底。
- **巡航** — 与 Sprint 帽并列（非 else）；掉出 Run 带时 `SetMovementTypeWanted(WALK)`；开消耗时 `SetSpeedLimit(..., instant=true)`。
- **测速** — `GetVelocityWS` 近 0 时回退位置差分，避免 W′ 虚空不烧。
- 配置版本 → **6.2.34**

## [6.2.33] - 2026-09-06

### AI 同行偏快：冲刺顶 + 负重意图项

- **根因** — 行为树常默认冲刺；开消耗时只要 W′ 允许就给 Sprint 行军顶（~4.5 m/s），玩家慢跑（~3.x）会被拉开。另：AI 负重只乘 `1−base`，玩家还有意图速比项。
- **策略** — 未开 **AI Fatigue Behaviors** 时，AI 速度顶压到 **Run**（`SetMovementTypeWanted(RUN)` + `SetSpeedLimit`）；仅 Behaviors + `GetRssSprintAllowed` 才允许冲刺顶。
- **负重** — 与玩家同形：`base × (1 + intentSpeedRatio)`，冲刺再 `ScaleSprintEncumbrancePenalty`。
- **不**对 AI 做玩家侧 `ClampOwnerHorizontalSpeed`（AI 用 `SetMovementTypeWanted` / `SetSpeedLimit`）。
- 配置版本 → **6.2.33**

## [6.2.32] - 2026-09-06

### AI 脚程对齐：Tobler + CP/Sprint 轻量帽（仍不跑玩家 UpdateSpeed）

- **默认仅限速** — `ApplyCheapAiSpeed` 始终用 `GetRawSlopeAngle` 做 Tobler，上坡不再比玩家明显偏快。
- **开消耗（AI 管线）** — 先算坡度/地形再限速；Sprint→`GetV6SprintSpeedMs`；Walk/W′ 巡航闩→`InvertCruiseCapMs`；写入 `appliedSpeedLimitMs` 供消耗记账。
- **设置说明** — `Disable AI Stamina Drain` 文案写入离线脚程差距：On≈均差15%/最大~79%；Off≈均差1%/最大~2%（`bench_player_ai_speed_gap.py`）。
- 配置版本 → **6.2.32**

## [6.2.31] - 2026-09-06

### 热路径：室内坡度抑制走缓存；grade 不再重复查坡 / new

- **根因（PerfProbe）** — `01g_slope_raw` 已 ~0.3µs，但 `01h_grade`/`01i_update_speed` 仍 ~27–33µs：`ShouldSuppressTerrainSlopeForEntity` 每调用做一次室内屋顶/OBB 射线（缓存形同虚设）。
- **`ShouldSuppress` → 读 `UpdateIndoorCache`**（过期才刷新）；热路径不再每 tick 双射线。
- **`UpdateSpeed`** — 已判室内后直接 `GetRawSlopeAngle` + `GradePercentFromSlopeDegrees`；去掉二次 `CalculateGradePercent`/`new`。
- **`CalculateGradePercentInto`** — UpdateLoop 复用 `gradeResult`；探针 `01h` 测纯 tan 路径。
- `GetFloorSurface` 为空属引擎常态，地形仍 Trace 回退（`03b` 本就很便宜）。
- 配置版本 → **6.2.31**

### 仓库整理（同日，无升配置号）

- 根目录 `WORKSHOP_*` → `docs/workshop/`；旧设计/审计 → `docs/archive/`；6.1 及更早 CHANGELOG → `docs/archive/CHANGELOG_pre_6.2.md`
- 删除未使用的 `SCR_RSS_StaminaHelpers.c`；旧 `AdminMenuUI` → `tools/archive/scripts/`
- 新增 `docs/README.md` 索引；三份 README 版本对齐 **6.2.31**；去掉重复 `LICENSE.txt`

## [6.2.30] - 2026-09-06

### 计算链路大改造：能复用官方已算量就复用

- **新增** `SCR_RSS_EngineReuse`：统一入口。
- **坡度** — `CommandMove.GetMovementSlopeAngle` → `GetFloorNormal` → Trace。
- **测速** — `Movement.GetVelocityWS` → 位置差分（仍禁用 `Physics.GetVelocity`）。
- **地形** — `GetFloorSurface`→材质表 → Trace；AI 稀采样同优先脚下材质。
- 探针增加 cmd 坡度 / VelocityWS / FloorSurface 分项。
- 配置版本 / ConfigManager → **6.2.30**

## [6.2.29] - 2026-09-06

### 性能：坡度优先复用引擎脚下法线

- **`GetFloorNormal` 优先** — `GetRawSlopeAngle` / `CalculateGradePercent` 先读 `CharacterMovementComponent.GetFloorNormal()`（移动物理已算）；失败才 `SCR_TerrainHelper` Trace；同一次法线同时算幅值+投影（去掉双 Trace）。
- **AI 管线** — 有脚下法线则复用；否则仍用 Y 差分；**不**对 AI 做 Trace 回退。
- **探针** — 新增 `01g2_floor_normal` / `01g3_trace_normal` 对比。
- 配置版本 / ConfigManager → **6.2.29**

## [6.2.28] - 2026-09-06

### AI 专用体力链路（精度对齐 + 避开 Speed 热点）

- **新增** `SCR_RSS_AIStaminaPipeline`：`DisableAIStaminaCalc=Off` 时 AI **不再进**玩家 Phase B/C；同源 `CalculateTotalDrainRate` / `UpdateStaminaValue` / W′ / 疲劳。
- **坡度** — 位置 Y 差分估 grade%（无射线），地形射线近 2s / 中 5s 稀采样；远距跳过 W′/疲劳。
- **热应激** — `SCR_RSS_AISharedEnvCache` 全服 1Hz TOD 近似，无逐 AI 环境全链。
- **限速** — 仍一律廉价（负重+相位）；文档：`docs/RSS_AI_体力链路方案.md`。
- **设置文案** — 菜单：`AI Fatigue Behaviors` / `Disable All AI RSS` / `Disable AI Stamina Drain`（去掉易误解的 Speed）；说明与 Attribute 对齐 6.2.28 语义。
- **修 AI 偏快** — 廉价限速改为「绝对 m/s ÷ 相位引擎顶」（与玩家同）；`SetSpeedLimit(1.0)` 不再摘掉源；默认不算消耗时冲刺顶压到 Run；战斗 SpeedCap 同步用绝对目标。
- **PerfProbe 全链路拆分** — `SCR_RSS_PerfProbe.Run(3000)` / `RunNearestAi(3000)`：限速原子（含 FromInputs/坡度/引擎顶/UpdateSpeed）、消耗原子、组合路径 A–G 与相对 `D_ai_cheap_abs` 倍率。
- 配置版本 / ConfigManager → **6.2.28**

## [6.2.27] - 2026-09-06

### 性能：AI「Speed」全量路径尖刺（战斗效果开 + Disable Stamina Off）

- **根因** — `Disable AI Stamina (Speed)=Off` 时每名 AI 仍跑玩家级 `UpdateSpeed`、地形射线、环境因子、CP 代谢二次限速；再叠加 Combat IntentFilter/`SetStateAllActionsOfType`，高密度时帧时间剧烈波动。
- **AI 限速与玩家脱钩** — 所有服端 AI 一律廉价限速（负重+相位）；全量体力只补位置测速后进 Phase B 消耗，**不再**走地形/环境/`UpdateSpeed`/CP 二次压速。
- **战斗层** — 状态未变跳过 IntentFilter/CombatDecay；行为节流近/中/远 0.75/1.5/3.0 s；传入已有 `ctrl` 避免 FindComponent。
- **错峰** — AI `CallLater` 按坐标哈希错开最多 180 ms；全量 LOD 600/1000/2500 ms。
- **Enforce 实测探针** — 控制台：`SCR_RSS_PerfProbe.Run()` / `Run(3000)`；日志 + `$profile:RSS_PerfProbe.txt`（`System.GetTickCount` 真测各链路）。
- 配置版本 / ConfigManager → **6.2.27**

## [6.2.26] - 2026-09-05

### 性能：多 AI 专服卡顿（默认路径）

- **根因** — 默认 `m_bDisableAIStaminaCalc=true` 只在 Phase A 末尾跳过消耗，此前每名 AI 仍跑地形射线、环境因子、`UpdateSpeed` 全链；近距 200ms tick 在百人 AI 场景下打满专服 CPU。另：`GetNearestPlayerDistanceM` 每 AI 每 tick `new array` + `GetPlayers`。
- **轻量路径** — `DisableAIStaminaCalc` 时 Phase A 开头 early-out：仅负重缓存 + V6 相位/跛行限速（`SCR_PlayerBaseAiLightTickHelper`），跳过地形/环境/代谢。
- **LOD** — 全量路径近/中/远间隔 400/700/2000 ms；轻量路径 500/1000/2500 ms。
- **距离查询** — 全服玩家原点 0.25s 共享缓存 + 静态复用数组；`TerrainDetector` 改走同一入口。
- 配置版本 / ConfigManager → **6.2.26**
- **服主** — 仍卡顿时可勾选「完全禁用 AI RSS」（`m_bDisableAIAllCalc`）彻底停循环。

## [6.2.25] - 2026-09-01

### 紧急修复：喝水 Access Violation 崩溃

- **日志** — `crash.log`：`SCR_RSS_CanteenDrinkEffect.ActivateEffect` 第 85 行 → `GadgetAnimationComponent.SyncWithCharacter(...)` 非法读 `0x3ff`。
- **结论** — 不可在脚本里对水壶 `GadgetAnimationComponent` 手动 `SyncWithCharacter` / 当主角色图 `CallCommand`；引擎未按该路径初始化，会直接崩。
- **回退** — 删除 `CMD_RSS_Drink` agr 覆盖；恢复角色 `BindCommand(CMD_Item_Action)` + `TryUseItemOverrideParams`（6.2.23 安全路径）。
- **已知限制（已接受）** — `CMD_Item_Action` 会同时打到 player_main 与水壶 `player.asi` 全身图，出现双播；在无崩溃方案下暂不继续折腾。
- 配置版本 / ConfigManager → **6.2.25**

## [6.2.24] - 2026-09-01

### 修复：医疗调用链对水壶必然双播

- **对照结论** — 吗啡等医疗：`CMD_HealSelf` 在 player_main（身体）与附着 `*_player.asi`（手臂互补）分层，所以不双播。
- **水壶差异** — `Canteen_*_player.asi` 是**全身**喝水；`CMD_Item_Action` 又在 player_main 存在。只要走医疗那套 `BindCommand` + `TryUseItemOverrideParams`，两套全身动画必然叠在一起。
- **修复** — 覆盖 agr/agf 为仅水壶有的 `CMD_RSS_Drink`；`GadgetAnimationComponent` 用 `player.asi`，脚本 `SyncWithCharacter` + `CallCommand`，**不再**对角色发 `CMD_Item_Action`；附着层改 `item.asi`（瓶体）；效果用 `CallLater` 结算。
- 配置版本 / ConfigManager → **6.2.24**

## [6.2.23] - 2026-09-01

### 修复：按官方医疗消耗品同一套动画调用链重做水壶

对照吗啡 / 绷带 / 止血带：
1. 角色 `CharacterAnimationComponent.BindCommand`（医疗是 `CMD_HealSelf`，水壶是官方图里的 `CMD_Item_Action`）
2. `GetAnimationParameters` → `TryUseItemOverrideParams`（与 `SCR_ConsumableMorphine` 相同）
3. 预制体：`player_main_1h.asi` + `AnimationAttachment(*_player.asi, BindingName Gadget)` + `ItemActionAnimAttributes`
4. **删除** `GadgetAnimationComponent`（医疗用品都没有；它是叠播/无动画乱源之一）

- 配置版本 / ConfigManager → **6.2.23**

## [6.2.22] - 2026-09-01

### 修复：6.2.20–6.2.21 喝水完全无动画

- **根因** — `TryUseItemOverrideParams` 的 command ID 必须能在**角色** `player_main` 上 `BindCommand`；`CMD_RSS_Drink` 只在水壶图里，角色侧绑定失败。物品侧 BindCommand 的 ID 也不能正确驱动角色物品使用状态机。另：顶层误用 `player_main_1h.asi` 会与水壶 `player.asi` 叠播。
- **修复** — 删除 `CMD_RSS_Drink` 的 agr/agf 覆盖，恢复官方 `CMD_Item_Action`；脚本在角色上 `BindCommand("CMD_Item_Action")`；预制体顶层 `AnimationInstance` 改回原版 `Canteen_*_player.asi`；`GadgetAnimationComponent` 用 `item.asi` + `AutoVariablesBind 1`；`MaxAnimLength 30`。
- 配置版本 / ConfigManager → **6.2.22**

## [6.2.21] - 2026-09-01

### 修复：6.2.20 喝水完全无动画

- **根因** — `CMD_RSS_Drink` 只在水壶 `Canteen_*.agr` 里，不在 `player_main`；6.2.20 在玩家 `CharacterAnimationComponent` 上 `BindCommand` 必然失败（cmdId = -1），`TryUseItemOverrideParams` 无法触发 Drinking 状态。
- **修复** — 按 `ItemUseParameters` 注释，在物品 **`GadgetAnimationComponent`** 上 `BindCommand("CMD_RSS_Drink")`，再 `SetCharGraphBindingName("Gadget")` 只打附着层，避免 player_main 双播。
- 配置版本 / ConfigManager → **6.2.21**

## [6.2.20] - 2026-09-01

### 修复：CMD_Item_Action 双图响应（资源级）

- **根因** — 凡走 `CMD_Item_Action` 的路径（BindCommand / TryUseItem / ItemActionAnimAttributes）都会同时打到 **player_main** 与 **Canteen 附着图**，必然双播；脚本层无法拆开。
- **修复** — 覆盖官方 `Canteen_US/Soviet.agr|.agf`：命令改为仅水壶图存在的 **`CMD_RSS_Drink`**；脚本 `BindCommand("CMD_RSS_Drink")` + `TryUseItemOverrideParams`；移除 `ItemActionAnimAttributes`。
- 配置版本 / ConfigManager → **6.2.20**

## [6.2.19] - 2026-09-01

### 修复：双动画几乎无停顿（非 MaxAnimLength）

- **根因** — 吗啡用 `CMD_HealSelf`：player_main 与附着层是互补分层。水壶 `CMD_Item_Action` 在 **player_main 与 Canteen 附着图都存在**；脚本 `BindCommand` + `OverrideParams` 会两边同时响应 → 先附着全身正确喝，再主图握枪错喝，几乎无间隔。
- **修复** — 去掉全部 BindCommand/OverrideParams；仅 `TryUseItem` + `ItemActionAnimAttributes`（`ActionAnimDuration 30`）由引擎经 `TagLItemAction` 协调；`GadgetAnimationComponent` 用 `item.asi`（仅瓶体）；`AutoVariablesBind 0`；布料 `Animate 0`。
- 配置版本 / ConfigManager → **6.2.19**

## [6.2.18] - 2026-09-01

### 修复：先正确全身喝、再握枪错喝

- **根因** — `SetMaxAnimLength(5)` 与喝水时长相同；引擎到期会再发一次 `CMD_Item_Action`（`CommandIntArg = -1`，本用于打断循环动画）。水壶 `Drinking` 非循环，`IsCommand` 会再次切入 Drinking，此时角色已在收枪回握 → 第二段「握枪喝」。
- **修复** — `MaxAnimLength` 改为 **30s**（远大于 clip），让动画自然 `RemainingTimeLess → Idle`，不再二次触发命令。
- 配置版本 / ConfigManager → **6.2.18**

## [6.2.17] - 2026-09-01

### 修复：仅水壶动画 / 仍双播

- **全身丢失** — 6.2.16 误用 `item.asi`（仅模型 `i_*`）；改回附着 `Canteen_*_player.asi`（全身 `p_*`）。
- **双播** — 去掉 `ItemActionAnimAttributes`（与 `TryUseItem`/`OverrideParams` 各触发一次 `CMD_Item_Action`）；父预制体改继承 `Item_Base`（移除 `Canteen_base` 的 `GadgetAnimationComponent`）；仅 `TryUseItemOverrideParams` 单路径；`ActivateAction` 拒绝 `IsUsingItem`。
- 配置版本 / ConfigManager → **6.2.17**

## [6.2.16] - 2026-09-01

### 修复：喝水双动画（附着层 clip 类型错误）

- **根因** — Bohemia 为水壶做了两套 clip：`p_*`（`Canteen_*_player.asi`，全身收枪喝水）与 `i_*`（`Canteen_*_item.asi`，仅水壶模型）。手持消耗品应像吗啡：`player_main` 走 `CMD_Item_Action` 管全身，Gadget 附着层只播 `item.asi`。我们附着层误用 `player.asi`，与 `player_main` 叠了两遍全身喝水。
- **修复** — `AnimationAttachment` 改为 `Canteen_US_item.asi` / `Canteen_Soviet_item.asi`；`BaseLoadoutClothComponent Animate 0`（避免背心挂件再响应命令）。
- 配置版本 / ConfigManager → **6.2.16**

## [6.2.15] - 2026-09-01

### 修复：确认原版非双段喝水；改引擎单路径

- **查证** — `Canteen_US.agf` 仅 `Idle → Drinking → Idle` 一段 clip（`p_erc_drink_UScanteen.anm` 等）；`p_*`（玩家）与 `i_*`（物品模型）为不同绑定，非故意连播；原版无 consumable，游戏中从未触发喝水。
- **双播原因** — 吗啡用 `CMD_HealSelf`（仅 player_main）；水壶 `CMD_Item_Action` 在 player_main 与 Canteen 附着图均 `IsCommand` 监听；脚本 `BindCommand` + `OverrideParams` 会各触发一次。
- **修复** — 去掉 `UpdateAnimationCommands` / `GetAnimationParameters`，仅 `TryUseItem(item, true, true)` + 预制体 `ItemActionAnimAttributes`。
- 配置版本 / ConfigManager → **6.2.15**

## [6.2.14] - 2026-09-01

### 修复：喝水仍播两次动画

- **根因** — `Canteen_base` 继承的 `GadgetAnimationComponent`（`AutoVariablesBind 1`）在物品实体上再响应一次 `CMD_Item_Action`；顶层 `ItemAnimationAttributes` 误用 `Canteen_*_player.asi`（吗啡/绷带用 `player_main_1h.asi`），与玩家 `player_main` 物品动作叠层。
- **修复** — 对齐医疗消耗品：`player_main_1h.asi` + `ItemActionAnimAttributes`（`player_main.agr` / `CMD_Item_Action`）；显式覆盖 `GadgetAnimationComponent` 为 `AutoVariablesBind 0`；保留脚本 `TryUseItemOverrideParams` 单路径。
- 配置版本 / ConfigManager → **6.2.14**

## [6.2.13] - 2026-09-01

### 修复：6.2.12 后无法喝水 / 拿水壶

- **根因** — `TryUseItem(item, true, true)` 对手持消耗品 gadget 不触发 `ActivateAction` 动画链；`m_eAnimVariable NONE` 又导致无法从背包上手。
- **修复** — 恢复 `GetAnimationParameters` + `UpdateAnimationCommands` + `TryUseItemOverrideParams`（与吗啡/工兵铲同模式）；`m_eAnimVariable` 改回 `ADRIANOV`；移除预制体 `GadgetAnimationComponent`（避免与玩家侧 `CMD_Item_Action` 双播），不再使用 `ItemActionAnimAttributes`。
- 配置版本 / ConfigManager → **6.2.13**

## [6.2.12] - 2026-09-01

### 修复：按一次 R 播放两段喝水动画

- **根因** — `SCR_RSS_CanteenDrinkEffect` 在玩家 AnimGraph 上 `BindCommand("CMD_Item_Action")` 并 `TryUseItemOverrideParams`，与 `GadgetAnimationComponent` / `ItemAnimationAttributes` 上同名命令叠加：先播正确收枪双手喝，再播抓枪握把喝。
- **修复** — 改回引擎路径：`ItemActionAnimAttributes` + `TryUseItem(item, true, true)`；移除手动 BindCommand/OverrideParams；`m_eAnimVariable` 改为 `NONE`（不再借用 ADRIANOV 吗啡手型）。
- 配置版本 / ConfigManager → **6.2.12**

## [6.2.11] - 2026-09-01

### 修复：R 提示仍无 / 第二次喝水动画异常

- **无提示** — `DefaultPlayerController` 覆盖 GUID 错误（应为 `{6E2BB64764E3BE9B}`）；改用 `SCR_RSS_CharacterHasCanteenInHandCondition` 检测手持水壶，不依赖 conf 里 modded enum。
- **第二次动画** — 原版 `ApplyItemEffect` 无论是否删除物品都会 `ModeClear(IN_HAND)`；`SCR_RSS_CanteenConsumableComponent` 仅在 `deleteItem` 时清手。
- 配置版本 / ConfigManager → **6.2.11**

## [6.2.10] - 2026-09-01

### 修复：R 键喝水动画重复 / 无左下角提示

- **双动画** — 预制体 `ItemActionAnimAttributes` 与 `SCR_RSS_CanteenDrinkEffect` 均触发 `CMD_Item_Action`；移除前者，并在 `CanApplyEffect` 中拒绝已在 `IsUsingItem` 的角色。
- **无提示** — 新增 `SCR_EConsumableType.DRINK`；覆盖 `DefaultPlayerController` 注册 `GadgetActivate` → `#AR-RSS-DrinkCanteen`（中/英本地化）。
- 配置版本 / ConfigManager → **6.2.10**

## [6.2.9] - 2026-09-01

### 修复：水壶无法从背包取出 / 无上手动画

- **根因** — 原版水壶是背心挂载布料（体积 1000、`SLOT_LOADOUT_STORAGE`），未登记 `EquipGadgetAction` / `SLOT_GADGETS_STORAGE`，背包里既装不下也不走手持装备链。
- **修复** — 体积改为 150；槽位改为 `SLOT_GADGETS_STORAGE`；增加 `EquipGadgetAction`；`m_eAnimVariable ADRIANOV` 启用上手动画；保留布料挂载与喝水消耗品。
- 配置版本 / ConfigManager → **6.2.9**

## [6.2.8] - 2026-09-01

### 功能：军火箱可取水壶

- **根因** — 原版 EntityCatalog / HQ 装备 overwrite 未登记水壶，仅改预制体无法在军火箱出现。
- **修复** — 合并完整 US/USSR `InventoryItems` 目录并登记 `Canteen_US_01` / `Canteen_Soviet_01`（`EQUIPMENT`）；HQ `ArsenalContentOverwrite_Equipment_HQ_Tent` 同步加入；顺带保留 CSB 针剂条目（独立 ArsenalItem GUID，避免与吗啡撞车）。
- 配置版本 / ConfigManager → **6.2.8**

## [6.2.7] - 2026-09-01

### 修复：HUD 残缺条 / 双层叠加（资源 GUID 撞车）

- **根因** — `StaminaHUD.layout.meta` 的 Name 仍为原版 `StatsPanelGrid.layout`，GUID `CD4F57077E64ECE5` 覆盖游戏右上角 StatsPanel：引擎自带一层默认「STA 100%」；RSS `CreateWidgets` 再叠第二层。服务器 Hint 关只 Destroy RSS 单例，原版被覆盖的那层仍留残影。
- **修复** — 独立 GUID `B4E8F1A29C7D6530` + meta 改为 `StaminaHUD.layout`；更新路径统一 `IsHudWanted` 门禁；Hint 配置变更时 Sync；布局默认文案改为 `--`。
- 配置版本 / ConfigManager → **6.2.7**

## [6.2.6] - 2026-09-01

### 修复：W′ 回满后引擎条过时 + 有氧回血过慢

- **引擎条** — `V6_WPRIME_ENGINE_FX_START` 改为 0.85（恢复良好即不再压条）；解除 transient 时 `Snap` 对齐有氧，避免平滑残留旧 W′ 地板写入 `GetStamina()`。
- **有氧恢复** — 休息≥10 分钟的慢速期（×~0.35）仅在体力已达边际衰减阈值（默认约 80%）后生效；中低体力仍用中等基线，避免等 W′ 回满后从 ~38% 爬满要约 50 分钟。
- 配置版本 / ConfigManager → **6.2.6**

