# RSS 项目引擎 API 使用清单

> 统计范围：`scripts/` 目录下所有 `.c` 文件  
> 排除自定义类（`SCR_RSS_*`、`SCR_Stamina*`、`StaminaConstants`、`RealisticStaminaSpeedSystem` 等项目内部类）

---

## 1. 核心游戏框架

| API | 说明 | 来源文件（代表性） |
|-----|------|-----------------|
| `GetGame()` | 获取游戏全局实例 | PlayerBase.c |
| `GetGame().GetWorld()` | 获取当前世界对象（`World`） | PlayerBase.c |
| `GetGame().GetCallqueue()` | 获取全局调用队列 | PlayerBase.c, SCR_StaminaOverride.c |
| `GetGame().GetWorld().GetWorldTime()` | 获取世界时间（毫秒，返回 `float`） | PlayerBase.c |
| `GetGame().GetInputManager()` | 获取输入管理器 | PlayerBase.c |
| `GetGame().GetPlayerManager()` | 获取玩家管理器 | PlayerBase.c |
| `GetGame().GetWorkspace()` | 获取 UI 工作空间（`WorkspaceWidget`） | SCR_StaminaHUDComponent.c |

---

## 2. 实体系统

| API | 说明 | 来源文件 |
|-----|------|---------|
| `IEntity` | 所有实体的基础接口类型 | 全局通用 |
| `GetOwner()` | 获取当前组件所属的实体 | PlayerBase.c, SCR_InventoryStorageManagerComponent_Override.c |
| `IEntity.FindComponent(ComponentType)` | 在实体上查找指定类型组件 | PlayerBase.c, SCR_EncumbranceCache.c |
| `IEntity.GetOrigin()` | 获取实体世界坐标（`vector`） | SCR_TerrainDetection.c, SCR_StaminaUpdateCoordinator.c |
| `IEntity.GetWorld()` | 获取实体所在的世界对象 | SCR_TerrainDetection.c |
| `ChimeraCharacter` | Reforger 角色实体类 | PlayerBase.c, SCR_StanceTransitionManager.c |
| `ChimeraCharacter.Cast(entity)` | 类型安全转换为 `ChimeraCharacter` | PlayerBase.c |
| `ChimeraCharacter.GetAnimationComponent()` | 获取角色动画组件 | PlayerBase.c |
| `ChimeraCharacter.GetCompartmentAccessComponent()` | 获取载具舱室访问组件 | PlayerBase.c |

---

## 3. 角色控制器

| API | 说明 | 来源文件 |
|-----|------|---------|
| `SCR_CharacterControllerComponent` | Reforger 扩展的角色控制器（`modded` 基类） | PlayerBase.c |
| `GetStance()` | 获取当前姿态，返回 `ECharacterStance` | PlayerBase.c, SCR_StaminaConsumption.c |
| `GetStaminaComponent()` | 获取体力组件（`CharacterStaminaComponent`） | PlayerBase.c |
| `GetVelocity()` | 获取角色当前速度向量（`vector`） | PlayerBase.c |
| `OverrideMaxSpeed(multiplier)` | 覆盖角色最大速度倍率 | PlayerBase.c |
| `OnInit(IEntity owner)` | 生命周期：组件初始化回调 | PlayerBase.c |
| `OnControlledByPlayer(IEntity, bool)` | 生命周期：玩家取得/放弃控制回调 | PlayerBase.c |
| `OnPrepareControls(IEntity, ActionManager, float, bool)` | 生命周期：每帧控制准备回调 | PlayerBase.c |

---

## 4. 体力组件

| API | 说明 | 来源文件 |
|-----|------|---------|
| `CharacterStaminaComponent` | 引擎原生体力组件基类 | PlayerBase.c, SCR_StaminaOverride.c |
| `SCR_CharacterStaminaComponent` | Reforger 扩展体力组件（`modded` 对象） | PlayerBase.c, SCR_StaminaOverride.c |
| `GetStamina()` | 获取当前体力值（0.0 ~ 1.0） | SCR_StaminaOverride.c |
| `ApplyDrain(float pDrain)` | 应用体力消耗（引擎原生，被 override 拦截） | SCR_StaminaOverride.c |

---

## 5. 库存 / 负重系统

| API | 说明 | 来源文件 |
|-----|------|---------|
| `SCR_CharacterInventoryStorageComponent` | 角色库存存储组件 | PlayerBase.c, SCR_EncumbranceCache.c |
| `SCR_CharacterInventoryStorageComponent.GetTotalWeight()` | 获取角色库存总重量（kg） | PlayerBase.c, SCR_JumpVaultDetection.c |
| `SCR_CharacterInventoryStorageComponent.GetMaxLoad()` | 获取最大可携带重量 | SCR_RealisticStaminaSystem.c |
| `SCR_InventoryStorageManagerComponent` | Reforger 扩展的库存管理器组件 | SCR_EncumbranceCache.c |
| `SCR_InventoryStorageManagerComponent.GetTotalWeightOfAllStorages()` | 获取所有存储槽总重量（官方推荐方式） | SCR_EncumbranceCache.c |
| `InventoryStorageManagerComponent` | 引擎原生库存管理器组件 | PlayerBase.c, SCR_StaminaConsumption.c |
| `ScriptedInventoryStorageManagerComponent` | 脚本化库存管理器基类（`modded` 基类） | SCR_InventoryStorageManagerComponent_Override.c |
| `BaseInventoryStorageComponent.GetTotalWeight()` | 单个存储格重量 | SCR_EncumbranceCache.c |

---

## 6. 输入系统

| API | 说明 | 来源文件 |
|-----|------|---------|
| `InputManager` | 全局输入管理器 | PlayerBase.c |
| `InputManager.AddActionListener(name, EActionTrigger, callback)` | 注册动作监听器 | PlayerBase.c |
| `InputManager.RemoveActionListener(name, EActionTrigger, callback)` | 移除动作监听器 | PlayerBase.c |
| `EActionTrigger.DOWN` | 动作触发枚举值：按下 | PlayerBase.c |
| `ActionManager` | 每帧动作状态管理器（`OnPrepareControls` 参数） | PlayerBase.c |
| `ActionManager.GetActionTriggered(name)` | 检测指定动作在当前帧是否触发 | PlayerBase.c |

---

## 7. 网络复制（RPC）

| API | 说明 | 来源文件 |
|-----|------|---------|
| `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]` | 可靠广播 RPC 装饰器 | PlayerBase.c |
| `[RplRpc(RplChannel.Reliable, RplRcver.Owner)]` | 发送给 Owner 客户端的 RPC | PlayerBase.c |
| `[RplRpc(RplChannel.Reliable, RplRcver.Server)]` | 客户端发给服务器的 RPC | PlayerBase.c |
| `RplChannel.Reliable` | 可靠网络通道枚举 | PlayerBase.c |
| `RplRcver.Broadcast` / `RplRcver.Owner` / `RplRcver.Server` | RPC 接收者枚举 | PlayerBase.c |

---

## 8. 玩家管理

| API | 说明 | 来源文件 |
|-----|------|---------|
| `PlayerManager` | 玩家管理器 | PlayerBase.c |
| `PlayerManager.GetPlayerIdFromControlledEntity(IEntity)` | 通过控制实体获取玩家 ID | PlayerBase.c |
| `PlayerManager.GetPlayerName(int playerId)` | 通过玩家 ID 获取显示名称 | PlayerBase.c |
| `SCR_PlayerController.GetLocalControlledEntity()` | 获取本地玩家当前控制的实体 | PlayerBase.c, SCR_StaminaOverride.c |

---

## 9. 物理 / 地形检测

| API | 说明 | 来源文件 |
|-----|------|---------|
| `World` | 世界类 | PlayerBase.c |
| `World.TraceMove(TraceParam, callback)` | 射线/移动追踪 | SCR_TerrainDetection.c |
| `TraceParam` | 射线追踪参数结构体（`Start`, `End`, `Flags`, `LayerMask`） | SCR_TerrainDetection.c |
| `TraceFlags.WORLD` / `TraceFlags.ENTS` | 追踪标志枚举 | SCR_TerrainDetection.c |
| `EPhysicsLayerPresets.Projectile` | 物理层预设枚举 | SCR_TerrainDetection.c |
| `vector` | 三维向量类型 | 全局通用 |
| `vector.Up` | 世界向上方向常量 | SCR_TerrainDetection.c |
| `vector.Zero` | 零向量常量 | PlayerBase.c, SCR_StaminaUpdateCoordinator.c |

---

## 10. 动画 / 角色命令

| API | 说明 | 来源文件 |
|-----|------|---------|
| `CharacterAnimationComponent` | 角色动画组件 | PlayerBase.c, SCR_SwimmingState.c |
| `CharacterAnimationComponent.GetCommandHandler()` | 获取命令处理器组件 | PlayerBase.c, SCR_SpeedCalculation.c |
| `CharacterCommandHandlerComponent` | 命令处理器组件（用于判断游泳/攀爬等状态） | SCR_SwimmingState.c |
| `ECharacterStance` | 姿态枚举 | SCR_StaminaConsumption.c, SCR_StaminaRecovery.c |
| `ECharacterStance.STAND` / `CROUCH` / `PRONE` | 姿态枚举值 | SCR_StanceTransitionManager.c |

---

## 11. UI / HUD 系统

| API | 说明 | 来源文件 |
|-----|------|---------|
| `WorkspaceWidget` | UI 根工作空间 | SCR_StaminaHUDComponent.c |
| `WorkspaceWidget.CreateWidgets(resourcePath)` | 从 `.layout` 文件创建 UI 树 | SCR_StaminaHUDComponent.c |
| `Widget` | 基础 UI 控件类型 | SCR_StaminaHUDComponent.c |
| `Widget.FindAnyWidget(name)` | 按名称递归查找子控件 | SCR_StaminaHUDComponent.c |
| `Widget.Remove()` | 销毁 Widget | SCR_StaminaHUDComponent.c |
| `TextWidget` | 文本 UI 控件 | SCR_StaminaHUDComponent.c |
| `TextWidget.Cast(widget)` | 类型转换为 `TextWidget` | SCR_StaminaHUDComponent.c |
| `TextWidget.SetText(string)` | 设置显示文本 | SCR_StaminaHUDComponent.c |
| `TextWidget.SetColor(Color)` | 设置文本颜色 | SCR_StaminaHUDComponent.c |
| `Color` | 颜色类型 | SCR_StaminaHUDComponent.c |
| `Color.FromRGBA(r, g, b, a)` | 从 RGBA 分量构造颜色 | SCR_StaminaHUDComponent.c |
| `GUIColors.RED_BRIGHT2` / `ORANGE_BRIGHT2` / `DEFAULT` | 引擎预设 GUI 颜色常量 | SCR_StaminaHUDComponent.c |

---

## 12. 信号系统（UI 信号桥）

| API | 说明 | 来源文件 |
|-----|------|---------|
| `SignalsManagerComponent` | 信号管理器组件（用于 Layout 数据绑定） | SCR_UISignalBridge.c, PlayerBase.c |
| `SignalsManagerComponent.Cast(component)` | 类型转换 | SCR_UISignalBridge.c |
| `SignalsManagerComponent.GetSignalIdx(name)` | 通过名称获取信号索引 | SCR_UISignalBridge.c |
| `SignalsManagerComponent.SetSignalValue(idx, float)` | 按索引设置信号值 | SCR_UISignalBridge.c, SCR_JumpVaultDetection.c |
| `SignalsManagerComponent.GetSignalValue(idx)` | 按索引读取信号值 | SCR_JumpVaultDetection.c |

---

## 13. 天气 / 环境系统

| API | 说明 | 来源文件 |
|-----|------|---------|
| `TimeAndWeatherManagerEntity` | 时间与天气管理器实体 | SCR_EnvironmentFactor.c |
| `TimeAndWeatherManagerEntity.GetWindSpeed()` | 获取当前风速（m/s） | SCR_EnvironmentFactor.c |
| `TimeAndWeatherManagerEntity.GetWindDirection()` | 获取当前风向（角度） | SCR_EnvironmentFactor.c |

---

## 14. 调用队列

| API | 说明 | 来源文件 |
|-----|------|---------|
| `CallQueue.CallLater(func, delayMs, repeat)` | 延迟/循环调用函数 | PlayerBase.c, SCR_StaminaOverride.c |
| `CallQueue.Remove(func)` | 取消已注册的延迟调用 | SCR_StaminaOverride.c |

---

## 15. JSON 配置系统

| API | 说明 | 来源文件 |
|-----|------|---------|
| `SCR_JsonLoadContext` | JSON 反序列化上下文 | SCR_RSS_ConfigManager.c |
| `SCR_JsonLoadContext.LoadFromFile(path)` | 从文件路径加载 JSON | SCR_RSS_ConfigManager.c |
| `SCR_JsonLoadContext.ReadValue(key, ref value)` | 读取 JSON 键值 | SCR_RSS_ConfigManager.c |
| `SCR_JsonSaveContext` | JSON 序列化上下文 | SCR_RSS_ConfigManager.c |
| `SCR_JsonSaveContext.WriteValue(key, value)` | 写入 JSON 键值 | SCR_RSS_ConfigManager.c |
| `SCR_JsonSaveContext.SaveToFile(path)` | 保存 JSON 到文件 | SCR_RSS_ConfigManager.c |
| `$profile:` 路径前缀 | 指向服务器 Profile 目录的路径宏 | SCR_RSS_ConfigManager.c |

---

## 16. 配置属性装饰器

| API | 说明 | 来源文件 |
|-----|------|---------|
| `[Attribute(defvalue, UIWidgets, desc)]` | 在 Workbench 中暴露可编辑属性 | SCR_RSS_Settings.c |
| `UIWidgets.ComboBox` | 下拉选择控件类型 | SCR_RSS_Settings.c |
| `UIWidgets.CheckBox` | 复选框控件类型 | SCR_RSS_Settings.c |
| `UIWidgets.EditBox` | 文本输入控件类型 | SCR_RSS_Settings.c |
| `[BaseContainerProps]` | 标注容器类可在 Workbench 中序列化 | SCR_RSS_Settings.c |

---

## 17. 数学库

| API | 说明 |
|-----|------|
| `Math.Clamp(val, min, max)` | 限制值范围 |
| `Math.Round(val)` | 四舍五入 |
| `Math.Min(a, b)` / `Math.Max(a, b)` | 最小/最大值 |
| `Math.Pow(base, exp)` | 幂运算 |
| `Math.Sqrt(val)` | 平方根 |
| `Math.AbsFloat(val)` / `Math.AbsInt(val)` | 绝对值 |

---

## 18. 调试 / 日志

| API | 说明 | 来源文件 |
|-----|------|---------|
| `Print(string)` | 输出调试字符串到控制台 | 全局通用 |
| `PrintFormat(format, ...)` | 格式化输出到控制台 | 全局通用 |
| `#ifdef WORKBENCH` | 编译宏：仅在工作台模式下编译 | SCR_RSS_ConfigManager.c |

---

## 19. 游戏模式

| API | 说明 | 来源文件 |
|-----|------|---------|
| `SCR_BaseGameMode` | Reforger 游戏模式基类（`modded`） | SCR_RSS_ServerBootstrap.c |
| `SCR_BaseGameMode.OnGameStart()` | 游戏开始回调（用于服务器引导配置加载） | SCR_RSS_ServerBootstrap.c |

---

## 20. 载具检测

| API | 说明 | 来源文件 |
|-----|------|---------|
| `CompartmentAccessComponent` | 载具舱室访问组件 | PlayerBase.c |
| `CompartmentAccessComponent.GetCompartment()` | 获取当前乘坐的舱室（返回 null 则不在载具中） | PlayerBase.c |

---

## 汇总统计

| 分类 | API 数量 |
|------|--------|
| 核心游戏框架 | 7 |
| 实体系统 | 9 |
| 角色控制器 | 8 |
| 体力组件 | 4 |
| 库存/负重 | 8 |
| 输入系统 | 6 |
| 网络复制 | 5 |
| 玩家管理 | 4 |
| 物理/地形 | 9 |
| 动画/命令 | 5 |
| UI/HUD | 11 |
| 信号系统 | 5 |
| 天气/环境 | 3 |
| 调用队列 | 2 |
| JSON 配置 | 7 |
| 属性装饰器 | 5 |
| 数学库 | 7 |
| 调试/日志 | 3 |
| 游戏模式 | 2 |
| 载具检测 | 2 |
| **合计** | **~111** |
