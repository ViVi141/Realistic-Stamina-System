# 更新日志

## [6.3.0] - 2026-09-06

### 发布整合：6.2.6 → 6.3.0

本版合并 **6.2.6～6.2.40** 全部变更（详见 [docs/archive/CHANGELOG_6.2.6_to_6.2.40.md](docs/archive/CHANGELOG_6.2.6_to_6.2.40.md)）。配置版本 → **6.3.0**。

#### AI 体力与限速
- **专用管线** — `SCR_RSS_AIStaminaPipeline` + 共享热应激；默认廉价限速，开消耗时 Tobler/CP/Sprint 轻量帽（不跑玩家 `UpdateSpeed`）。
- **应用层** — Agent/群组 `MovementSpeedSetting`（Origin=**SCENARIO**）；巡航/跛行钉 WALK；群组 Override 按成员 maxGait **min** 聚合。
- **性能** — AI LOD / 轻量 early-out / 错峰；室内坡度走缓存；`SCR_RSS_EngineReuse`（坡度/测速/地形优先官方量）。
- **生产** — `[RSS][AI-SPD]` 与 Workbench Debug/HUD/DataExport 默认关。

#### 相对 6.2.6 的性能（PerfProbe，µs/次）

对照基线：**6.2.6 @ `883a051`**（当时玩家与 AI **同一全栈**）。优化侧：**6.3.0 tip**（`Run` / `RunNearestAi`，3000 iters）。

**可以怎么说（推荐对外表述）**

1. **默认配置**（`Disable AI Stamina Drain = On`，AI 不算消耗）  
   - **玩家**：相对 6.2.6 限速全栈（A≈48.5）→ 现 A≈15，约 **快 3.3×**（单次开销约省 **~69%**）；其中 `UpdateSpeed` 44→7.3，约 **快 6×**。  
   - **AI**：相对 6.2.6 同栈（≈48）→ 现生产 `ApplyCheapAiSpeed`（`03f`≈16.7），约 **快 2.9×**（约省 **~65%**）。

2. **AI 打开 drain**（`Disable AI Stamina Drain = Off`）  
   - 相对 6.2.6 **AI 全栈 B**（≈55）→ 现估 ~22（`03f + F − D`），约 **快 2.5×**（约省 **~60%**）；仍不走玩家 `UpdateSpeed` 全伺服。

| 对比 | 6.2.6 | 6.3.0 | 约快 | 约省开销 |
|------|-------|-------|------|----------|
| 默认 · 玩家限速全栈 A | ~48.5 | ~15 | ~3.3× | ~69% |
| 默认 · 玩家 `UpdateSpeed` | 44 | 7.3 | ~6× | ~83% |
| 默认 · AI（`03f`） | ~48 | **16.7** | **~2.9×** | **~65%** |
| AI 开 drain · vs 旧全栈 B | ~55 | **~22** | **~2.5×** | **~60%** |
| grade（室内射线） | ~37 | ~0 | ~∞ | ~100% |

探针 D≈1.7 / F≈7 是简化合成，**不是**生产路径；旧「AI ~30× / ~9×」已作废。

#### 玩家与联机
- W′ 引擎条过时、有氧长休回血过慢；蹲走/站走巡航分流；联机 HUD W′；HUD GUID 撞车与双层叠加。
- **专用服 HUD** — 无故残影 / 开关后双层：按名清扫孤儿根；`OnControlled(false)` 仅本机失控才 Destroy 单例；管理员客户端乐观应用 Hint 后再 Sync。
- **水壶（已知无效果）** — 军火库仍可取到，仅作**未删除的开发产物**：可上手/播喝水动画，**暂不回补 STA/W′**；保留是为后续「体力补充食物」管线做集成测试，不应当作可用补给。
- 进服测速崩溃：禁止不可靠 `GetVelocity` 解引；陆地测速权威对齐官方案例。

#### 仓库
- 文档整理入 `docs/`；6.1 及更早日志见 `docs/archive/CHANGELOG_pre_6.2.md`。

## [6.2.5] - 2026-09-01

### 修复：W′ 耗尽时蹲走与站走同速

- **根因** — W′ 空站立巡航把 `SetSpeedLimit` / Walk 覆盖套到蹲/趴，绝对顶与站走对齐，引擎姿态顶速无法拉开差距。
- **修复** — 非站立时释放 Walk 覆盖、跳过缩轴/模拟量巡航；清除限速源并恢复原生 MovementMaxSpeed；`OnPrepareControls` 仅在站立重申限速。
- 配置版本 / ConfigManager → **6.2.5**

## [6.2.4] - 2026-09-01

### 修复：HUD 残缺默认条与双层叠加

- **根因** — 本地 HUD 覆盖与服务器 Hint 门禁不一致：服务器关时 `Init` 直接 return 不 Destroy，留下布局默认「STA 100%」；再开服务器又 `CreateWidgets` 叠第二层。
- **修复** — 统一 `IsHudWanted()`（服务器 Hint 开 ∧ 本地未关）；不显示则 Destroy；显示则复用单例；更新路径走同一门禁。
- 配置版本 / ConfigManager → **6.2.4**

## [6.2.3] - 2026-09-01

### 修复：联机 HUD W′ 恒为 100%

- **根因** — 专服玩家体力主循环只在客户端跑，但 `TickPhaseBAnaerobic` 被 `Replication.IsServer()` 挡住，客户端从不消耗 W′；专服 `WPrimeServerTick` 粗估又把 RplProp 灌回 100% 盖掉本机池。
- **修复** — Phase B 在跑主循环的一侧 tick W′（联机=客户端）；仅服务端写 RplProp；本地控制角色忽略 `OnRssAnaerobicReplicated`；停用粗估补 tick。
- 配置版本 / ConfigManager → **6.2.3**

## [6.2.2] - 2026-09-01

### 修复：加载服务器后体力 Tick 崩溃（启发式空引用）

- **根因** — `GetVelocity`（CharacterController 或 Physics）在进服窗口脚本侧非空句柄仍 Access Violation；6.2.1 改用 `Physics.GetVelocity` 后崩溃点移至 `SampleEntityVelocity`。
- **启发式空引用** — `SCR_RSS_RuntimeGuard.IsEntityWorldUsable`（Game/World 交叉）+ `IsPhysicsHandlePresent`（二次 GetPhysics）；**禁止**再解引 `GetVelocity`/`SetVelocity`。
- **测速** — 陆地权威改为位置差分；`SampleEntityVelocity(owner, fallback)` 只做校验并返回 fallback。
- **水平钳** — `ClampOwnerHorizontalSpeed` / SoftClamp 在证实安全前跳过 native 写速（限速源仍生效）。
- 配置版本 / ConfigManager → **6.2.2**

## [6.2.1] - 2026-09-01

### 修复尝试：PhaseA GetVelocity 崩溃

- 就绪门 + 陆地改 `Physics.GetVelocity`；进服仍于 `SampleEntityVelocity` AV → 由 **6.2.2** 启发式方案取代。
- 配置版本 / ConfigManager → **6.2.1**

## [6.2.0] - 2026-08-30

### 修复：Walk 最终倍显示与轻微超速

- **显示** — 状态日志 / Debug「倍率」「最终倍」按 `appliedAbs / Run引擎顶` 归一；Walk 满档约 `0.38` 而非相位相对的 `0.999`。`SetSpeedLimit` 仍用相位相对倍率。
- **缩轴** — W′ 巡航下 Walk 超速阈值 `0.35→0.12` m/s，目标压到 Walk 顶 ×0.88；伺服遇 Walk 超速减半，减少 1.6–2.1 m/s 带内拖尾。
- **回归修复** — Walk 超速压轴误对 Run 巡航生效时，会把 `desired` 压到 ~1.27 并被 SpeedBridge 跳过 → `模拟量=off`、`v_meas` 卡在 3.5。现仅在 Walk/走路键时压轴。

### 修复：Walk 阶段速度尖峰

- **限速分母** — `SetSpeedLimit` 倍率改按引擎当前相位顶（`GetRssSpeedLimitEngineBaseMs`）；Walk 绝对顶用 Walk 引擎顶钳制。跨相位时跳过倍率斜率，避免 `0.63(Run)→0.999(Walk)` 被当成提速。
- **缩轴** — Walk/走路键仅在测速已落入 Walk 带时让路；若闪 Walk 仍 `v≫Walk顶`，继续按 Walk 顶缩 `CharacterForward`，防止松巡航缩轴后窜到 3m/s+。
- **只减不加** — 缩轴伺服不得抬过本 tick `v_limit`；代谢二次 Apply 只允许压低；`MovementMaxSpeed` 不再用步态地板抬过 `safeCap`。
- **Rpl BumpMe** — `SCR_PlayerBaseWPrimeTickHelper`（static）内禁止 `Replication.BumpMe`；改由 `PlayerBase_UpdateLoop` 实例上下文调用，消除 `BumpMe from a static context` 刷屏。

### PlayerBase / UpdateLoop 拆分

- **拆出 `SCR_RSS_StaminaTickTypes`** — `RSS_StatusMetabLogSnapshot` / `RSS_StaminaDebugOutputParams` / `RSS_StaminaTickLocals` 从 `PlayerBase_UpdateLoop.c` 迁至 `RSS/Core/`，行为不变。
- **拆出 `SCR_PlayerBaseCpCruiseController`** — Run 巡航缩轴、Walk 覆盖、限速斜率、坡度 EMA 及对应状态迁出；`PlayerBase` 保留薄委托，行为不变。
- **拆出 `SCR_PlayerBaseOverspeedClampHelper`** — Phase B 超速/W′ 耗尽巡航物理钳迁出，行为不变。
- **拆出 `SCR_PlayerBaseWPrimeTickHelper`** — Phase B 服务端 W′ TickPower / EPOC / 复制写回迁出，行为不变。
- **拆出 `SCR_PlayerBaseStaminaTickFinalize`** — Phase C 体力写入 / cardio / Debug flush 迁出（`RSS_StaminaTickFinalizeContext`）；`PlayerBase` 保留薄壳，行为不变。

### 剩余 W′ 继续跑，见底才锁巡航

- **施密特只管冲刺** — 约 25% 解除武装仍禁 Sprint，避免 25↔60% 冲刺抽搐。
- **巡航闩** — W′ 真正见底（≤5 J）才锁有氧巡航并缩轴；再武装（约 60%）才解开。解除武装但池未空时满推 Run、继续烧 W′，不再立刻落到 2.2 m/s 让 W′ 回充。
- **缩轴闭环** — 见底后若 `v_meas` 低于巡航帽，抬轴幅度，避免卡在 Run 地板 2.2。C/Python/Rust 同序。
- **巡航分母冻结** — 剩余 W′ 满推 Run（约 3.8）写入分母；见底后不再用 `v/模拟量` 反推（2.17/0.61 会把轴拧到 0.61↔0.78）。闭环只在 super 后积一次，带死区。

### 试跑：W′ 空缩放 CharacterForward（输入层）

- **SetActionValue** — `OnPrepareControls` 的 `super` 前后各按 `v_limit/Run顶` 缩 `CharacterForward`/`CharacterRight`（与手柄摇杆同一动作；官方测试用此口喂移动）。不抬半推。HUD `模拟量` 现为轴幅度（2.4/3.5≈0.69）。关：`V6_TRY_ACTION_VALUE_SCALE`。
- **冲刺耗尽后仍能冲** — W′ 解除武装时冲刺门禁改到 `super` 之前清 `CharacterSprint`；模拟量不再因按着 Shift / 仍停在 Sprint 相位而跳过。Walk 覆盖仍不改。
- **缩放让路** — 游泳、蹲/趴、Walk 覆盖不写轴；走路键/Walk 相位在测速已落入 Walk 带时让路，超速闪 Walk 仍缩轴。W′ 空按巡航帽与 `v_limit` 较低者缩（帽未写出时先用 2.4），不再等 SetSpeedLimit 缓降窗里继续满推 Run。灌木/更低的已应用帽仍取 min，不抬过限速。
- **缩放分母用满推测速** — 引擎 Run 顶 ~3.8，负重满 W 常 ~3.55；按 3.8 缩会到 2.26 而孪生是 2.40。武装满推采样，缩放时用 `v_meas/模拟量` 反推，比例对准巡航帽。

### 试跑：W′ 空用 SetMovement 模拟量压 Run

- **实机失败，已关** — 写出 `模拟量=1.39` 时 `v_meas` 仍 3.55（`v_limit=2.4`），并闪过 Walk 对 3.3+ m/s。过场 `SetMovement` 打不过按住 W。`V6_TRY_MOVEMENT_ANALOG_SCALE = false`。

### 两英里硬锚改到官方 ACFT 85 分

- **评分** — 22–26 岁男性官方尺：18:00=60 分、13:30=100 分；85 分线性插值 **15:11.25**。去掉项目自定义「18:00=70 / 15:30=85」
- **硬门禁** — `zero_load_2mile_pt_ge85`：零负重 Run 2 mi ≤15:11。配速约 3.54 m/s 需 CP≥1760 W，否则 W′ 中途烧空会掉到 20 分钟开外
- **搜索带** — CP 1720–2050；Elite v5_run 3.533–3.548（钉在 85 分），Standard / Tactical 依次加快；35 kg Run 上限 3.20→3.40（Tactical 负重慢跑会到 ~3.30）
- **optimize-tiers 重跑** — TPE 300 trials/档 + repair + embed。Elite CP 1866 / W′ 28830 / sprint_cap 3591 / v5_run 3.535（2mi **15:11 / 85.1 分**）；Standard 1868 / 32669 / 3647 / 3.58（14:59 / 86.8）；Tactical 1911 / 32774 / 3928 / 3.65（14:42 / 89.3）。冒烟 36/36

### W′ 武装不再因「未超速」免单

- **帽内 P>CP 进 TickPower** — W′ 武装时 `v_limit` 是步态盖不是 CP 巡航速。14°/29 kg Run 即使 `v_meas≤v_limit`（`超速记账=off`）也按真实 `P_met` 烧 W′；仅解除武装后的巡航仍钳到 CP。C/Python/Rust 同序。下坡滑行 `P≤CP` 仍钉 CP，禁止回充白嫖。

### 步态覆盖认软带（防 jog-CP 滑步）

- **W′ 空且反解 < Run 地板（2.2）即切 Walk** — jog-CP 疲劳后反解常落在 2.12（软带，Resolve 不回 -1），旧触发只认 `-1`，覆盖永不亮，Run 动画对 2.5–3.5 m/s 物理 = 滑步。C/Python/Rust 同序。不改限速/超限记账。

### CP 慢跑口径重标定

- **硬约束 `run_wprime_armed_29kg_60s`** — 29 kg 平路 Run 60 s 后 W′ 须仍武装（池 >25%）、速度仍在 Run 带（≥2.2 m/s）、未切 Walk 覆盖。情景钉死 Elite 慢跑盖子（enc=0.34、v5_run=3.05），不吃 trial 的低负重/高速。CP 按「能慢跑」而非「能行军」搜索；Python/Rust 孪生同序 `game_player_tick`
- **搜索空间** — `critical_power_watts` 1480–1850（原 750–1100）；`sprint_power_cap_watts` 上沿 4000，配合冲刺 ≤15 s
- **35 kg Run 观测下限** — 慢跑口径下平路可接近 CP，硬门下限改为 0%/s，只封上限 2.6%/s
- **门禁用 Python 慢跑约束** — Rust 同参 W′ 消耗偏少，`rss_sim_backend` 在 Rust 全过后再跑 Python `run_wprime_armed_29kg_60s`，与 C 孪生对齐
- **optimize-tiers 重跑** — TPE 300 trials/档 + repair + embed。Elite CP 1519 / W′ 23426 / sprint_cap 3712；Standard 1528 / 30336 / 3735；Tactical 1595 / 31223 / 3882。冒烟 35/35

### 文档-代码漂移同步（2026-08-26）

- **CP 限速开关描述对齐** — 权威文档 / 开发者指南 / 已知问题（中英 6 处）从「6.1.x drain-only 默认关」更新为「v6.1.7 起默认开」（代码 `V6_APPLY_CP_METABOLIC_SPEED_CAP = true` 自 6.1.7 生效，文档滞后）
- **Skiba 分派描述对齐** — 权威文档 §4.3/§7 移除「三档全走 Skiba、线性为死代码」的过期警告，改为「已按 `w_prime_recovery_mode` 档位分派」（6.1.6 已修，文档滞后）
- **数字孪生三端同步** — `rss_digital_twin_fix.py` 与 `rss_sim/constants.rs` 的 `V6_APPLY_CP_METABOLIC_SPEED_CAP` 由 `false` 对齐为 `true`；`test_v6_smoke.py` 的 `downhill_phys_clamp_policy` / `overspeed_excess_drain` 断言更新为 6.1.7 策略（12× 路径），26/26 通过

### v4 残留与零引用弃用清理

- **Constants** — 移除零引用：`WALK_RECOVERY_ZONE_RATE`、`EPOC_DRAIN_RATE`（别名）、`GRADE_UPHILL/DOWNHILL_COEFF`、`HIGH_GRADE_THRESHOLD/MULTIPLIER`（v4 Pandolf 坡度系数）、`WILLPOWER_THRESHOLD`（v4 意志力平台期）、`STAMINA_EXPONENT_LEGACY`（v4 Minetti 指数）
- **ConfigBridge** — 移除 6 个零引用兼容别名（`GetV5*SpeedMs`、`GetAnaerobicSprintEnableThreshold`、`GetAnaerobicDrainPerSec`、`GetAnaerobicEfficiencyFactor`）
- **SpeedCalculator / MetabolismMath** — 移除 `GetV5AbsoluteSpeedMs`、`CalculateSpeedMultiplierByStamina`（含 v4 双稳态模型注释块）；README_CN 同步
- **PlayerBase** — 移除零引用的 `GetSprintCooldownUntil()`（`GetSprintStartTime()` 仍被引用，保留）
- **保留** — 仍被引用的弃用项不动：`V5_BURST_COOLDOWN_*`（迁移/烘焙用）、`GetCooldownUntilSec/IsOnCooldown`（复制槽）、`anaerobicPercent`（公开 API 字段）

### 巨型文件拆分（续）

- **PlayerBase.c** — 引擎顶速采样簇（解限标定缓存 + 实时污染检测，约 110 行）拆至 `SCR_PlayerBaseEngineTopSampler.c`；公开转发签名不变
- **EnvironmentFactor.c** — 全局信号读取子域（`ERSS_EnvSignal` 枚举、静态信号缓存、注册/重置、4 个 ReadSignal）拆至 `SCR_RSS_EnvSignalReader.c`；`ResetGlobalSignalsCache()` 公开 API 保留为转发

### 新功能：手持物品影响消耗

- **实现手持重物额外消耗**（原 `SCR_RSS_StaminaConsumptionCalculator` TODO）— `SCR_RSS_EncumbranceCache` 称重 IN_HAND gadget（过滤 `GetHeldGadget` 的隐藏腕表回退）或当前武器；`InventoryItemComponent.GetTotalWeight()`；消耗乘数 `1 + 2.0 × (heldKg / 90)`，上限 1.5；陆地快/完整路径与游泳消耗侧均施加；4 kg 步枪 ≈ +9% 消耗（武器质量已在 Pandolf 负重中，此处只加倍率）
- **手持称重纠偏** — 过滤 `GetHeldGadget()` 隐藏挂件误计；无 IN_HAND gadget 时采当前武器；游泳路径补施加倍率；`CalibrateUncappedEngineTopsOnce` / `EnsureSignalsRegistered` / `CalculatePostureMultiplier` 补空指针防护
- **专服崩溃防护** — 手持采样要求实体仍在世界中、武器优先 `GetCurrentSlot`；`GetMaxSpeed`/信号读取/负重轮询/RPC/战斗兴奋剂在 `GetGame`/`GetWorld` 为空时直接返回，避免进退服与 AI tick 窗口 Access violation
- **全仓专服崩溃扫描** — 新增 `SCR_RSS_RuntimeGuard`；跳跃/翻越、室内检测、载具恢复、数据导出、调试批次、屏效/滤镜、体力 tick 的 `CallLater`、GameMode 引导队列、析构限速恢复均在世界/队列为空时跳过。`World` 为 sealed 原生类型，不能作 `out` 参数，取世界用 `GetWorldOrNull()`
- **W′ 耗尽后陡坡爬行** — 巡航限速反解不再按网格局部 15–18° 把人钉到 0.4 m/s。速度伺服坡度钳到 15%、地形 η 不进反解、Walk 地板 1.0 m/s（约 3.6 km/h 负重徒步）；消耗仍按实测坡度。帽内高于 CP 时 STA 承担 P−CP，避免「体力 80% 却爬行且不掉条」
- **步态带内限速 + 能量税** — CP 巡航帽只写在当前步态带内（`SetSpeedLimit` ≥ 0.5× 相位顶）。Run 反解掉出 2.2 m/s 带时不再把 Walk/爬行速度压到 Run 相位（滑步）；W′ 空仍硬跑则 STA 按 P−CP 收步态税（约 10×、上限 0.4%/s），**即使物理略超 v_limit 也不改走更便宜的 12× 小超额**。低体力步行恢复区只在 Walk 生效（不再把 Run 2.7 m/s 当成慢跑回血）。不写 `SetMovementTypeWanted`
- **Walk 代谢帽对齐徒步地板** — `GetMetabolicSpeedCapMs` 改为走 `InvertCruiseCapMs`（坡度/η 钳 + 1.0 m/s 地板），不再用裸反解把 Walk `v_limit` 拧到 ~0.5 m/s；`GetMetabolicCorrectedSpeedMultiplier` 低于帽时仍托 0.5× 步态下限；`MovementMaxSpeed` 试跑不再用裸 abs 冲掉该地板
- **Idle 不再钉死起步** — 站立相位写 `0.999` 而不是 `0`（过渡器曾把 0 托成 0.01 m/s ≈ 倍率 0.0027）。从静止起步若上一帧低于 0.5× 步态带则立刻抬到目标，不再按 1.25/s 爬行
- **条空跛行不再拧出步态带** — 精疲力尽后仍按住 Run 时，`SetSpeedLimit` / `MovementMaxSpeed` 立刻托 0.5× 相位顶（疲惫慢跑），不再按 1.25/s 往 0.15 爬行倍率缓降（Run 动画对爬行指令 = 滑步）。跛行绝对速改用 Walk 顶 1.45 m/s（地板 1.0），不再误用相位阈值 3.2。切 Walk 后才是 ~1 m/s 跛行；下坡重力滑行仍在（物理钳关）
- **Sprint→Run 不再写 1.0 清源** — 玩家路径始终 `SetSpeedLimit≤0.999`，避免冲刺落地瞬间 uncapped（日志 `最终倍=1`）
- **步行恢复只认 Walk** — `Run惯性`（引擎 Idle）不再把 HUD/判定当成步行回血；过脊时坡度符号翻转立刻跟上，减轻下坡仍按上坡反解把 `最终倍` 拧到 0.5×
- **硬跑步态税降到现实量级** — W′ 空后 29 kg 上坡硬跑不再顶满 2.5%/s（约 32 s 抽干 80%）。改为 10×、上限 0.4%/s（陡坡硬跑约 3–4 min 掉完 80%；下坡更低）
- **掉出 Run 带改切引擎 Walk 档** — W′ 解除武装且 CP 反解低于 Run 地板、仍按住移动时，用 CapsLock 同款切 Walk。**按住 W 则保持 Walk**（下坡反解回到 Run 带也不自动改跑，避免过脊 Walk 动画对 3 m/s 物理）。松开 W（0.25 s 去抖）或 W′ 再武装后还原滚轮。
- **孪生对齐 Walk 覆盖 + 引擎顶** — Python/Rust `game_player_tick` 在 W′ 解除武装且反解掉出 Run 带时切 Walk 并锁存；Walk 引擎顶用 `ENGINE_WALK_TOP_MS`（1.45），不再误用 Run 3.8 把步态下限抬到 ~1.9 m/s（4h 行军假抽干）。
- **optimize-tiers 重跑** — 孪生对齐后 TPE 300 trials/档 + repair：Elite CP 907.8→804.7、Standard 1031.0→873.6、Tactical 1080.6→873.6；Tactical CP/sprint_cap 提到与 Standard 齐平以满足档位阶梯；已 embed `SCR_RSS_SettingsPresetBake.c`。
- **步态覆盖不再假超速烧 W′** — 覆盖期间测速仍在引擎 Walk 顶内（约 ≤1.65 m/s）时，相对徒步地板 1.0 的 phys 超速不计：功率钳到 CP（上坡不烧 W′），下坡允许回充；Walk 12× STA 税同样免除。真滑步（≥~1.65）仍记账。C/Python/Rust 三端同步。

### 版本

- 配置版本 / ConfigManager → **6.2.0**


---

更早版本：6.2.5 及以下见下文；**6.2.6～6.2.40** 详见 [docs/archive/CHANGELOG_6.2.6_to_6.2.40.md](docs/archive/CHANGELOG_6.2.6_to_6.2.40.md)；6.1.x 及以前见 [docs/archive/CHANGELOG_pre_6.2.md](docs/archive/CHANGELOG_pre_6.2.md)。
