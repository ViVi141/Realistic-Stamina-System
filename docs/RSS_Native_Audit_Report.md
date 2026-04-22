# RSS 全量「对照原生源码」审计报告（hybrid 交付）

**审计基线（官方）**：`C:\Users\74738\Documents\output\scripts`（Reforger 导出的 `scripts` 树）  
**审计对象（本 addon）**：`C:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\scripts\Game\**` 全部 `.c`（其中 `scripts/Game/RSS` 下约 46 个模块文件，不含 Integration）

**重要说明**：

- 本报告是「静态走查 + 与官方同签名/同生命周期点对照」的审计，不等价于线上多人压测；其中 **反作弊/作弊面** 只能给出“威胁模型 + 可验证的缓解策略”，需要在实际服务器条件下验证成本与效果。
- 下文的 **P0/P1/P2** 为审计分级：**P0 优先于多人正确性与强副作用；P1 为性能/规模/可运维；P2 为体验/工具/可维护性**。

## 1. 系统级数据流（用于定界问题归属）

- **Server JSON（profile）** → `SCR_RSS_ConfigManager`（仅服务端 I/O，客户端不读盘）
- **GameMode `RplProp` payload** → 客户端在 `onRpl` 中写入 `SCR_RSS_Settings`（内存）并 `SetServerConfigApplied(true)`  
  见：`scripts/Game/Integration/SCR_RSS_ServerBootstrap.c`
- **主循环/权威逻辑**：大量仍在 `modded` 的 `SCR_CharacterControllerComponent`（`PlayerBase.c`）中，以 `GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, …)` 形式推进  
- **“速度校验/导出”旁路**：`PlayerBase.c` 内仍保留 `RplRpc`（与 GameMode 配置复制不是同一子系统）  
- **专服数据导出（文件桥）**：`SCR_RSS_DataExport` 在服务端把玩家态写入 `$profile:RSS_PlayerData.json`（受配置开关与间隔控制）

## 2. 模块总览（仓库内如何分层）

- **`scripts/Game/Integration/`**：最高冲突面（`modded` 直接叠加在官方大类的入口）
- **`scripts/Game/RSS/`**：模组主体（核心体力/环境/表现桥接/网络配置/AI/泥泞等），其中 **`NetworkConfig/` 承担配置持久化、复制触发与部分网络同步管理器**
- **`scripts/Game/Components` / `Damage` / `UserActions` 等**：本仓库若存在，应按 P1+ 做专项审计（本报告以 RSS 与 Integration 为重心）

## 3. 8 个 `modded` 文件：对照表与结论

### 3.1 `modded class SCR_CharacterControllerComponent`（`scripts/Game/Integration/PlayerBase.c`）

**官方基线**：`C:\Users\74738\Documents\output\scripts\Game\Character\SCR_CharacterControllerComponent.c`（超大文件；本次审计关注“你覆盖的生命周期/网络/性能路径”与官方可扩展点是否被破坏）

| 项目 | 本模组实际覆盖/新增点 | `super` 链 | 权威域（Server/Client）与复制/网络点 | 与官方主要偏差点/风险点 |
| --- | --- | --- | --- | --- |
| `override void OnInit(IEntity owner)` | 初始化：加载配置（服务端）/客户端默认、注入 RSS 子系统、启动延迟循环、等待 GameMode 配置 | 先 `super.OnInit` | 服务端：`SCR_RSS_ConfigManager.Load()`；客户端：等待 `IsServerConfigApplied` | **高风险覆盖**：该入口承载过多子系统，易与其他 `modded SCR_CharacterControllerComponent` 冲突；`Load()` 在 GameMode/角色两侧都可能启动（依赖冷却/静态态） |
| `override void OnControlledByPlayer` | 跳输入监听、StaminaHUD `CallLater(InitStaminaHUD)` 等 | 先 `super` | 本地：监听输入；`InitStaminaHUD` 内调用 `SCR_StaminaHUDComponent.SyncHintDisplayWithSettings()`，与 `m_bHintDisplayEnabled` 一致（见 5.1 已修复说明） |
| `override void OnPrepareControls` | 额外 Jump 检测（与监听互补） | 先 `super` | 仅本地控制实体 | 与 ActionManager 的重复检测属于“冗余但可接受”，注意维护成本 |
| `UpdateSpeedBasedOnStamina`（非 override，自调度） | 核心 tick：环境/速度/AI/战斗兴奋剂/数据导出相关 RPC 触发 | 不涉及 `super` | 分域：AI/游泳/车辆分支；仅客户端+本地控制且开导出时 `Rpc` | **P0（作弊面）**：`RPC_ClientReportStamina` 仍信任客户端输入（在开启导出时）——官方典型模式是**服务器权威**或严格校验（见 6.2 P0-ACE-1） |
| CombatStim 逻辑 + `RPC_CombatStimSyncToOwner` | 服务器推进阶段并对 Owner 同步显示态 | 无 `super`（自定义） | 服务器→Owner 可靠 `RplRpc` | **若注射入口可被非权威触发**：会放大为玩法/反作弊问题；需确认所有“推进阶段/击杀”的入口都服务器权威 |
| 配置应用 | GameMode 复制在 `OnRssConfigReplicated` 写入 `SCR_RSS_Settings`；`ApplyFullConfig*` 已从 `PlayerBase` 删除（P2-DEAD-1 已落实） | N/A | 客户端 | 见 5.1 |

**结论**：`PlayerBase` 是 RSS 的“主引擎层”，`super` 在生命周期覆盖点总体完整；**最大风险在：作弊面（P0-ACE-1）、与其它 modded 的叠加冲突**；原「配置应用死代码 + HUD 与 `m_bHintDisplayEnabled` 脱节」已按 5.1 修复节所述收敛。

### 3.2 `modded class SCR_BaseGameMode`（`scripts/Game/Integration/SCR_RSS_ServerBootstrap.c`）

**官方基线**：`C:\Users\74738\Documents\output\scripts\Game\GameMode\SCR_BaseGameMode.c`（头注释对 GameMode 单例/扩展方式有官方导向）

| 项目 | 本模组点 | `super` 链 | 权威域/复制点 | 偏差点/风险点 |
| --- | --- | --- | --- | --- |
| `override void OnGameStart` | 延迟 `DeferredRssConfigLoad` | 先 `super` | 仅服务端会 `Load()` + 构建并 `Replication.BumpMe()` | 合理：`OnGameStart` 复制可能未就绪，延迟+重试与官方“组件/延迟初始化”精神一致（但仍建议记录失败上限后的降级策略，见 6.1） |
| `RplProp` 字段 + `onRplName: "OnRssConfigReplicated"` | 以数组承载 presets/settings | 无（回调） | 服务器构建；客户端 `OnRss` 中合并进 `GetSettings()` | 客户端不读 JSON：符合你先前目标；**热重载/晚加入/首次构建空配置** 需要验收用例（见 6.1 P0-CFG-1） |
| `RssServerDataExportTick` / `RssServerDataExportScheduleIfNeeded` | 仅当 `m_bDataExportEnabled` 为真时启动自调度链（约 1s/次 `TryExport`）；关导出则不挂循环（见 P1-PERF-1 部分落实） | 无 | 仅服务器 | `DataExport` 内逐玩家 `ForceUpdate` + 写盘仍与玩家数/间隔成比例（P1 其余项待评估） |

### 3.3 `modded class SCR_CharacterStaminaComponent`（`scripts/Game/Integration/SCR_StaminaOverride.c`）

**官方基线**：`C:\Users\74738\Documents\output\scripts\Game\Character\SCR_CharacterStaminaComponent.c`（类体很短，大量行为在基类/引擎层）

| 项目 | 本模组点 | `super` 链 | 分域/复制点 | 偏差点/风险点 |
| --- | --- | --- | --- | --- |
| `override void OnStaminaDrain` | 默认 **禁用 native stamina**，通过 `CorrectStaminaToTarget` 强行对齐目标 | 条件调用 `super` | 不直接涉及 Rpl，但会改变**所有依赖原生体力演化的系统** | **P0（兼容面）**：任何依赖原生体力变化事件/资源消耗协同的脚本都可能被“静音化/偏离”；`CallLater` 监控是性能敏感点（P1） |

### 3.4 `modded class SCR_InventoryStorageManagerComponent`（`scripts/Game/Integration/SCR_InventoryStorageManagerComponent_Override.c`）

**官方基线**：`C:\Users\74738\Documents\output\scripts\Game\Inventory\SCR_InventoryStorageManagerComponent.c` 中 `OnItemAdded/OnItemRemoved` 先 `super` 再数绷带

| 项目 | 本模组点 | `super` 链 | 分域/复制点 | 偏差点/风险点 |
| --- | --- | --- | --- | --- |
| `OnItemAdded/OnItemRemoved` | `super` 后通知 `SCR_CharacterControllerComponent` 更新负重缓存 | 与官方同序（先 `super`） | 与库存复制一致：发生在本地回调链上 | 额外 `FindComponent`：注意空实体/切换角色窗口期（低概率） |

### 3.5 `modded class CharacterCamera1stPerson`（`scripts/Game/RSS/Presentation/CharacterCamera1stPerson.c`）

**官方基线**：`C:\Users\74738\Documents\output\scripts\Game\Character\Cameras\FirstPerson\CharacterCamera1stPerson.c`

| 项目 | 本模组点 | `super` 链 | 分域/复制点 | 偏差点/风险点 |
| --- | --- | --- | --- | --- |
| `override void OnUpdate` | 在官方 FOV/矩阵计算之后叠加 Sprint FOV + 泥泞角抖动/FOV 微动 | 先 `super` | 仅本地镜头表现 | 典型风险：对 `pOutResult` 二次修改顺序是否与官方后处理/其它镜头模组叠加冲突（P2/兼容性） |

### 3.6 屏幕效果 `modded` 三件套（`SCR_DesaturationEffect` / `SCR_NoiseFilterEffect` / `SCR_RegenerationScreenEffect`）

**官方基线**：

- `C:\Users\74738\Documents\output\scripts\Game\UI\ScreenEffects\SCR_DesaturationEffect.c`
- `C:\Users\74738\Documents\output\scripts\Game\UI\ScreenEffects\SCR_NoiseFilterEffect.c`
- `C:\Users\74738\Documents\output\scripts\Game\UI\ScreenEffects\SCR_RegenerationScreenEffect.c`

| 类 | 覆盖点 | `super` 链 | 分域/复制点 | 偏差点/风险点 |
| --- | --- | --- | --- | --- |
| `SCR_DesaturationEffect` | `AddDesaturationEffect` | OD 时 bypass `super` 的默认去饱和，改为自定义 `s_fSaturation` 路径 | 仅本地后处理 | 与官方去饱和后处理是否仍一致：你绕过了 `super` 分支，需保证材质链仍按官方期望启用 |
| `SCR_NoiseFilterEffect` | `DisplayUpdate` | 先 `super` | 本地音频变量：`AudioSystem.SetVariableByName("CharacterLifeState", …)` | **P1 体验风险**：用 `INCAPACITATED` 模拟“发闷”会影响所有依赖该全局变量的听感/混音（有开关，默认 0 很好） |
| `SCR_RegenerationScreenEffect` | `UpdateEffect` + `ClearEffects` | 先改 `m_bRegenerationEffect`，后 `super.UpdateEffect`；`ClearEffects` 先 `super` 再清 RSS 状态 | 仅本地 | 与官方 `UpdateEffect` 时序已对齐到“可触发 RegenerationEffect 的同一 tick”，总体合理 |

## 4. `scripts/Game/RSS/**` 热路径走查（非 `modded`）

### 4.1 分域与一致性（多人）

- **配置**：客户端不读盘，靠 GameMode 复制 + `GetSettings()` 读内存，方向正确。  
- **“速度/体力导出 RPC”与 GameMode 配置复制是两套机制**：在审计与故障排查中不要混为一谈。  
- **AI/性能**：`SCR_RSS_AIStaminaBridge` 将 AI 的 tick 间隔与群代理/距离 LOD 挂钩，是典型“以系统负载换正确性/表现”的折中，属于合理工程化。

### 4.2 明显热点与规模风险

- **GameMode 与数据导出**：`RssServerDataExportScheduleIfNeeded` 仅在 `m_bDataExportEnabled` 为真时启动 `RssServerDataExportTick` 自调度链；`DataExport` 内“逐玩家环境强制刷新/写文件”仍是 **O(玩家数/间隔)** 成本（见 6.1 P1-PERF-1）。  
- **环境数学**：`SCR_EnvironmentFactor.c` 中大量 `while` 用于角度/小时归一，属于有界归约循环；风险主要在**调用频率**而不是死循环。  
- **Stamina 覆盖**：`SCR_StaminaOverride` 的 `CallLater` 轮询/纠正属于高频点，和玩家数/帧率/服务器 tick 相关（见 3.3）。

## 5. 关键“偏差点”与风险（按主题）

### 5.1 配置热更新 / HUD 开关 / 死代码（已修复与当前实现）

**说明**：以下问题已在后续提交中处理；本段保留为「曾发现问题 → 现行为」的审计留痕。

- **配置写入**：`SCR_RSS_ServerBootstrap.OnRssConfigReplicated` 在客户端将复制载荷合并进 `SCR_RSS_Settings` 并 `SetServerConfigApplied(true)`。`PlayerBase` 中遗留的 `ApplyFullConfig*`、`m_sLastAppliedConfigHash` 及仅为其服务的 `SCR_PlayerBaseConfigHelper` 包装已删除（**P2-DEAD-1 已落实**；全仓库无 `ApplyFullConfig` 等符号）。
- **Hint HUD 与 `m_bHintDisplayEnabled`**：`SCR_StaminaHUDComponent.SyncHintDisplayWithSettings()` 在 **开启** 时 `Init`、**关闭** 时 `Destroy`；`OnRssConfigReplicated` 在应用设置后调用一次（专服改配置/热重载后客户端会同步）；`InitStaminaHUD()` 仅调用 `SyncHintDisplayWithSettings()`，不再无条件 `Init`。
- **仍建议人工验收**：晚加入/首连 10s 内与专服对账、热重载后 HUD 与数值一致（对应 **P0-CFG-1**），见 [docs/P0_CFG1_Acceptance_Checklist.md](P0_CFG1_Acceptance_Checklist.md)。

### 5.2 反作弊/作弊面（与官方“权威服务器”精神对照）

- `RPC_ClientReportStamina` 的设计目标是“外部分析/导出桥”，但它依旧允许客户端作为输入源。只要开启 `m_bDataExportEnabled`，**恶意客户端**可以上报任意体力与重量，并诱发服务器对“验证速度”的 `Rpc` 回应（其安全性取决于你服务器侧模型是否可伪造）。  
- **对照点**：官方玩法系统通常以服务器可观测的权威状态机为准；要把它变成“强一致反作弊”，需要独立的服务器重算/约束（见 6.2 P0-ACE-1）。

## 6. 可执行待办（P0–P2，含验收标准）

### P0

#### P0-CFG-1：GameMode 配置复制链路的“强验收”
- **问题**：`RplProp` 路径虽清晰，但缺少“失败可见性/可重复验收”，例如：服务器空配置/客户端首次进服/热重载后 10s 内一致性。  
- **影响面**：所有客户端的 RSS 行为是否正确启用；与其他模组同时加载时的时序。  
- **建议方向**：在 **Debug 仅** 下增加可开关的对账日志（configVersion/preset/哈希/数组长度），并定义固定验收脚本（多人 2+1 专服为最佳）。  
- **验收标准**：
  - 专服启动后，客户端在 **10s 内** `SCR_RSS_ConfigManager.IsServerConfigApplied()==true` 且 `GetSettings().m_sConfigVersion` 与专服 `SCR_RSS_ConfigManager` 侧一致。  
  - 专服在运行中 `Reload/ForceSync` 后，已连接与晚加入客户端均在 **10s 内**达到同一 settings 对账结果。  
  - 关闭服务器 JSON（或内容为空）的降级行为符合你们的产品定义，并且客户端不会卡死/刷屏。

#### P0-ACE-1：明确定义“数据导出/速度校验”的威胁模型
- **问题**：客户端上报→服务器回写验证速度，这不是传统意义的强反作弊。  
- **影响面**：公开服务器与竞技场景下的可用性/可信度。  
- **建议方向（择一或组合）**：
  - 将数据导出模式标记为“非安全/仅私有服务器”；或  
  - 在服务器上复算关键量（至少速度/状态机），客户端上报仅作对照样本；或  
  - 为 RPC 加签名/白名单/频率惩罚（成本更高）。  
- **验收标准**：
  - 在开启导出时，人工用“改本地内存/改 RPC 参数”无法稳定获得**持续错误的优势状态**（以你们能接受的检测阈值描述为准）。  
  - 在关闭导出时，客户端不应发送该 RPC 或服务器立即拒绝且成本可忽略（可用静态计数+日志验收）。

#### P0-COMP-1：体力覆盖 `SCR_StaminaOverride` 的“兼容性基线”
- **问题**：完全拦截 native 体力修改会影响其它依赖原生体力语义的系统。  
- **影响面**：与官方内容、其他模组、未来引擎更新的耦合。  
- **建议方向**：维护一份**已知不兼容/已验证兼容**清单；必要时为 `m_bAllowNativeStaminaSystem` 增加“可配置/按场景启用”策略。  
- **验收标准**：在你声明支持的官方任务/沙盒回归场景下，无软锁死、无资源系统异常、无未预期体力数值漂移。

### P1

#### P1-PERF-1：数据导出/环境强制刷新成本
- **问题**：`DataExport` 在服务器侧可能对每个玩家做 `EnvironmentFactor.ForceUpdate` + 写盘。  
- **影响面**：多玩家+短间隔+大地图服务器 CPU/IO。  
- **部分落实**：`m_bDataExportEnabled` 为假时不再挂 **1s 固定** `CallLater` 循环；为真时由 `RssServerDataExportScheduleIfNeeded` + `RssServerDataExportTick` 自调度。  
- **仍建议**：批处理、分桶、按玩家子集/按时间片摊销 `ForceUpdate` 与写盘。  
- **验收标准**：在 N 个玩家、导出间隔 1s 的压测中，服务器帧耗时与 profile 的磁盘写入占用低于约定阈值（由你们用 Reforger profiling 定标）。

#### P1-AUD-1：`SCR_NoiseFilterEffect` 全球音频变量副作用
- **问题**：`CharacterLifeState` 变量是全局式副作用，可能影响听感/敌我分辨。  
- **影响面**：PVP 可玩性与沉浸选项的矛盾。  
- **建议方向**：继续默认关闭，并在 UI/文档明确“竞技禁用”；如要开启，提供最小化影响范围（例如只在特定混音 bus、或加本地 gate）。  
- **验收标准**：开启开关前后，同一录音对比可重复；PVP 测试组不报告“听感信息损失类问题”为阻断项或列为已知风险。

### P2

#### P2-DEAD-1：清理无引用配置应用辅助函数/过期注释（**已落实**）
- **原问题**：`ApplyFullConfig*` 无引用。  
- **现状态**：`PlayerBase` 与 `SCR_PlayerBaseConfigHelper` 中相关死代码已移除；`ResetClientConfigAwaitingSync` 等注释已指向 GameMode 复制。  
- **回归**：`grep` 无 `ApplyFullConfig` / `ShouldSkipApply` 等；Workbench 编译通过即视为满足原验收标准。

#### P2-TOOLS-1：`tools/` 与 C 常数/JSON 的同步策略
- **问题**：Python 侧优化器/图表工具与 C 内参数生成链条一旦漂移，会生成“漂亮但不适配”的配置。  
- **影响面**：调参与回归成本。  
- **建议方向**：在 CI/本地用 `check_c_json_relation.py` / `consistency_check.py` 形成固定门闸（无需在本审计中实装，但要形成流程）。  
- **验收标准**：同一份 `constants_master.json` 变更后，工具链与 C 常量同步 PR 可一键验证。

## 7. 推荐修复顺序（与当前进度的关系）

1) **P0-CFG-1**（可重复专服+客户端验收；仍属流程/手测，见 `docs/P0_CFG1_Acceptance_Checklist.md`）  
2) **P0-ACE-1** 产品立场与默认策略（见 `docs/P0_ACE1_Product_Notes.md`；代码中 `RPC_ClientReportStamina` 已有 `//!` 非强反作弊说明）  
3) **~~5.1 HUD/死代码~~**（**已落实**：`SyncHintDisplayWithSettings` + 删除 `ApplyFullConfig*`，见 5.1）  
4) **P1-PERF-1**（`RssServerDataExportTick` 已改为**仅** `m_bDataExportEnabled` 时自调度；逐玩家 `ForceUpdate` / 分桶等仍可继续优化）  
5) **P2-TOOLS-1**（工作流/一致性，未变）

## 8. 官方参考文件（报告撰写时高频引用的基线入口）

- `C:\Users\74738\Documents\output\scripts\Game\GameMode\SCR_BaseGameMode.c`（GameMode 单例/扩展与注释导向）
- `C:\Users\74738\Documents\output\scripts\Game\Character\SCR_CharacterControllerComponent.c`（你覆盖的类体规模与扩展面）
- `C:\Users\74738\Documents\output\scripts\Game\Inventory\SCR_InventoryStorageManagerComponent.c`（`OnItemAdded/Removed` 的 super 序）
- `C:\Users\74738\Documents\output\scripts\Game\UI\ScreenEffects\SCR_*Effect.c`（屏幕效果/音频变量链路）
- `C:\Users\74738\Documents\output\scripts\GameCode\Serialization\SCR_SerializationContextJson.c`（JSON 序列化 API 基线，用于与 `SCR_RSS_ConfigManager` 的对照讨论）

---

**本文件定位**：可交付的审计结论与待办清单；不作为运行时配置说明。修订记录：5.1 / P2-DEAD-1 / 第 7 节与实现同步（复查清单计划）。

**相关增量文档**：[P0-CFG-1 验收步骤](P0_CFG1_Acceptance_Checklist.md)、[P0-ACE-1 产品与默认策略](P0_ACE1_Product_Notes.md)。
