# RSS v3.22.6 — 深度代码审查报告

> 审查日期: 2026-05-10
> 审查范围: 全部 65 个 C/EnforceScript 源文件
> 审查方法: 逐文件阅读核心逻辑，分析 Bug/设计缺陷/性能问题

---

## 严重等级定义

| 等级 | 描述 |
|------|------|
| **CRITICAL** | 可能导致崩溃、数据损坏或逻辑严重错误 |
| **MAJOR** | 逻辑错误、数值异常、设计缺陷 |
| **MINOR** | 代码风格、冗余、潜在可优化点 |
| **INFO** | 值得关注的架构特性或注释 |

---

## 1. Core 核心模块 (13 文件)

### 1.1 SCR_StaminaConstants.c

#### ⚠️ MAJOR: 阈值重叠

- **位置**: `WALK_RECOVERY_ZONE_THRESHOLD = 0.15` 与 `SPRINT_ENABLE_THRESHOLD = 0.15`
- **问题**: 两个阈值完全相等，当 stamina = 0.15 时，"步行恢复区"和"允许冲刺"两条路径互相冲突
- **PlayerBase.c 实际使用**: `<` 比较符避免 off-by-one，但设计上仍有混淆风险

#### ⚠️ MAJOR: 恢复率单位不一致

```c
REST_RECOVERY_RATE = 0.250       // pts/s → 每 tick 0.0005
WALK_RECOVERY_ZONE_RATE = 0.002  // 每 tick 0.002（注释说"每0.2s恢复0.2%"）
```

- 步行恢复区的恢复率是正常静息恢复的 **4 倍**
- 注释 "即每秒1%" 与实际不符（0.002 × 5 = 0.01 = 1%/s → 注释正确，但计算逻辑: 0.002/tick × 5 ticks/s = 0.01/s = 1%/s ✓）

### 1.2 SCR_RealisticStaminaSystem.c

#### 🔴 CRITICAL: 地形系数常量局部隐藏全局常量

- **位置**: `TERRAIN_FACTOR_PAVED = 1.0` 在类内重新定义
- **问题**: 与 `StaminaConstants.TERRAIN_FACTOR_PAVED` 完全重复，两套副本一处修改不会同步

#### 🔴 CRITICAL: Pandolf 坡度项保护不充分

```c
float maxGradeTerm = baseTerm * 3.0;
gradeTerm = Math.Min(gradeTerm, maxGradeTerm);
```

- **根因**: 当 V=5.5m/s, G=1.0 (45°) 时 `gradeTerm = 1.0 × (0.23 + 1.34 × 30.25) = 40.765`
- 如果 `baseTerm` 很小（低速度时），`maxGradeTerm` 也很小，保护可能过度
- 同时如果 `weightMultiplier` 大 + `terrainFactor` 大 → 数值爆炸风险

#### ⚠️ MAJOR: Tobler 函数最终缩放无注释

```c
toblerMultiplier = 1.0 + 0.7 * (toblerMultiplier - 1.0);
```

- 在 `UPHILL_SPEED_BOOST = 1.15` 和 `DOWNHILL_SPEED_BOOST = 1.15` 之后应用
- 等效于"坡度限制缩减 30%"，但没有任何注释说明设计意图

### 1.3 SCR_StaminaUpdateCoordinator.c

#### 🔴 CRITICAL: 静态共享结果对象

```c
protected static ref SpeedCalculationResult s_pResultSpeedCalc;
protected static ref BaseDrainRateResult s_pResultBaseDrainRate;
protected static ref RecoveryContext s_pRecoveryCtx;
```

- **问题**: 高密度 AI (64+) 在群组代理外的场景中，同一帧多个 AI 的 `CalculateBaseDrainRate`/`BuildRecoveryContext` 会互相覆盖结果
- 注释自称"不得跨调用持有引用"，但无运行时保护

#### 🔴 CRITICAL: 向后兼容分支产生零消耗

```c
float runningDrainRate = RealisticStaminaSpeedSystem.CalculateGivoniGoldmanRunning(currentSpeed, currentWeightWithWet, true);
```

- `CalculateGivoniGoldmanRunning()` 返回 `0.0`（v3.12.0 已弃用）
- 当 `currentMovementPhase < 0`（意外降级路径）时，消耗率 = 0，导致"无限奔跑"bug

### 1.4 SCR_EncumbranceCache.c

#### ⚠️ MAJOR: 缓存失效状态不一致

```c
if (!ownerEntity) {
    currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
    // ...不标记 m_bEncumbranceCacheValid = false
}
```

- ownerEntity 为 null 时仍然使用缓存的组件引用取重量，但 `m_bEncumbranceCacheValid` 未被标记为 false
- 后续 `IsCacheValid()` 返回 true，但重量数据可能不完整

#### ⚠️ MAJOR: 负重惩罚计算重复实现

- `EncumbranceCache.ComputeSpeedPenaltyFromEffectiveWeight()` 与 `SCR_PlayerBaseNetworkHelper.CalculateEncumbrancePenaltyFallback()` 完全相同的三段分段多项式
- 一处修改另一处不会同步

### 1.5 SCR_StaminaConsumption.c

#### ⚠️ MAJOR: 快路径 `baseDrainRateByVelocity` 输出与完整路径不一致

- **快路径**: `fastBase` 已经包含温度调整，`baseDrainRateByVelocity = fastBase / postureMultiplier`
- **完整路径**: `originalBaseDrainRate` 保存温度调整前的值
- 两者不同 → 恢复计算获得不同的 `baseDrainRateByVelocity`

### 1.6 SCR_FatigueSystem.c

#### ⚠️ MAJOR: 疲劳衰减条件有误

```c
if (fatigueTimeDelta > 0.0 && fatigueTimeDelta < 1.0)
```

- `fatigueTimeDelta < 1.0` 条件在基于 `CallLater` 的回调中可能永远不触发
- 因为 `m_fLastFatigueDecayTime` 只在成功衰减时才更新
- 60 秒的 `FATIGUE_DECAY_MIN_REST_TIME` 要求使疲劳恢复几乎不可达

### 1.7 SCR_SwimmingStaminaModel.c

#### ⚠️ MAJOR: 合速度立方溢出风险

```c
float vTotalCubed = vTotal * vTotal * vTotal;
```

- 当 `vTotal = 7.0m/s` 时，`vTotalCubed = 343`
- `horizontalPower = 0.5 × 1000 × 343 × 0.5 × 0.5 = 42,875W`（远超 `SWIMMING_MAX_TOTAL_POWER = 2000`）
- 后续 `Math.Clamp(totalPower, 0.0, 2000)` 能兜底，但乘法过程中可能溢出

### 1.8 SCR_DebugBatchManager.c

#### MINOR: 静态状态跨世界不重置

- `s_fNextDebugBatchTime` 在 Workbench 重载世界后保持旧值，导致新世界前几秒无调试输出
- 与 `EnvironmentFactor.ResetGlobalSignalsCache()` 同类问题

### 1.9 SCR_StanceTransitionManager.c

#### ⚠️ MAJOR: 窗口结算双入口可能导致状态冲突

- `TrySettleWindow`（体力循环每 tick 调用）和 `ProcessStanceTransition`（姿态切换时调用）可能在同一帧内改变 `m_fWindowStartTime`
- 调用顺序不确定时窗口状态可能不一致

---

## 2. Environment 环境系统 (9 文件)

### 2.1 SCR_EnvironmentFactor.c

#### 🔴 CRITICAL: Initialize 中大括号位置容易误导

```c
if (m_pCachedWeatherManager) {
    // ...
}
{
    m_pIndoorDetector = new SCR_EnvironmentIndoorDetection();
    m_fLastEnvironmentCheckTime = m_fLastEnvironmentCheckTime + Math.RandomFloat(...);
}
```

- 大括号 `{ }` 看起来像是 `if` 块，实际是独立代码块
- 逻辑上正确（`m_pIndoorDetector` 不依赖天气管理器）
- 但风格极易导致未来维护者误移或误解

#### ⚠️ MAJOR: ApplySettings 仅在 Initialize 时调用

- `ApplySettings()` 从 `SCR_RSS_Settings` 读取配置
- 仅 `Initialize()` 和 `OnConfigUpdated()` 中调用
- 如果管理员在 Settings 中修改了 `m_fTemperatureMixingHeight` 等参数，未调用 `OnConfigUpdated` 则不会生效

### 2.2 SCR_EnvironmentIndoorDetection.c

#### ⚠️ MAJOR: TraceParam 未完全重置

- `m_pTraceParamRoof` 和 `m_pTraceParamEnclosed` 复用时未重置 `ExcludeArray`、`UserData` 等字段
- `RaycastHasRoof` 使用 `EPhysicsLayerDefs.Projectile`，`IsHorizontallyEnclosed` 使用 `EPhysicsLayerPresets.Projectile`——两个层可能不同

### 2.3 SCR_TerrainDetection.c

#### MINOR: AI 距离 LOD 每 2 秒分配数组

```c
array<int> playerIds = new array<int>();
pm.GetPlayers(playerIds);
```

- 100 AI × 0.5次/秒 = 50次数组分配/秒
- 可考虑复用静态数组

### 2.4 SCR_SlopeSpeedTransition.c

#### ⚠️ MAJOR: SNAP_UP_THRESHOLD 导致缓坡过渡失效

- `SNAP_UP_THRESHOLD = 0.08`：目标倍率比当前值高 ≥0.08 时立即跳转
- 缓坡（2°-3°）时 Tobler 输出在 0.92~1.0 之间波动
- `gain = 0.08` 反复触发立即提速 → 5 秒平滑只在陡坡生效

---

## 3. NetworkConfig 网络与配置 (7 文件)

### 3.1 SCR_RSS_ConfigManager.c

#### 🔴 CRITICAL: 写文件无原子性保证

```c
void Save() {
    CreateConfigBackup();  // 备份旧文件
    // ...写入新文件...
    SaveToFile(CONFIG_PATH);
}
```

- 写入中途崩溃 → JSON 损坏；备份是旧版本（可恢复）
- 缺乏原子写入机制（先写临时文件再 rename）

#### ⚠️ MAJOR: 客户端配置超时警告逻辑

```c
float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
```

- `RSS_WaitForGameModeConfig` 超时 30 秒后每 30 秒打印一次警告
- 但 `m_fLastConfigTimeoutWarningTime` 在超时前被设为 `nowSec`，即使首次超时也立即打印
- 这是合理的，但注释 `"每 30 秒打印一次"` 说的是超时后，不是首次

### 3.2 SCR_RSS_Settings.c

#### ⚠️ MAJOR: 预设默认值与稳定基线

- `m_sSelectedPreset` 默认 `"StandardMilsim"`，但 EliteStandard 的注释说 `"Workbench 默认开启"`
- 工作台模式中 `SCR_RSS_ConfigManager.Load()` 覆盖为 `m_sSelectedPreset = "EliteStandard"`
- 两套默认值系统可能导致非工作台环境下加载顺序问题

### 3.3 SCR_NetworkSync.c

#### ⚠️ MAJOR: 客户端数据导出开关关闭后仍可能上报

- `RPC_ClientReportStamina` 在 `GetServerDataExportEnabled()` 为 false 时 return
- 但 `ShouldSync` 在 `isCriticalData` 路径下仍会通过
- 如果开关在 60Hz 关键数据上报期间被关闭，最后一个 RPC 可能携带着过期的 critical 数据

---

## 4. Integration 入口层 (6 文件)

### 4.1 PlayerBase.c

#### 🔴 CRITICAL: GetGame().GetWorld() 未判空

```c
World world = GetGame().GetWorld();
```

- `GetGame()` 开头有检查，但 `GetGame().GetWorld()` 可能返回 null
- 在服务器关闭/关卡切换阶段，`world.GetWorldTime()` 可能触发 Access Violation

#### 🔴 CRITICAL: staminaPercent 在组件失效后继续使用

```c
float staminaPercent = 1.0;
if (m_pStaminaComponent)
    staminaPercent = m_pStaminaComponent.GetTargetStamina();
// ...100 行不使用 m_pStaminaComponent...
```

- 如果 `m_pStaminaComponent` 在初始化后被置 null（实体删除），`staminaPercent` 使用 `1.0` 持续运行
- 不会崩，但会导致体力被错误地设置为满值

#### ⚠️ MAJOR: 析构函数中 GetOwner() 可能返回部分销毁的实体

```c
void ~SCR_CharacterControllerComponent() {
    IEntity owner = GetOwner();
    if (owner && !IsPlayerControlled())
        SCR_RSS_AIRestRecoveryRegistry.CleanupEntity(owner);
}
```

- `GetOwner()` 返回的实体可能已被部分销毁
- `CleanupEntity` 使用 IEntity 指针比较，内存被复用可能误清理

### 4.2 SCR_StaminaOverride.c

#### ⚠️ MAJOR: AI 实体保持 50ms 监控循环

```c
void MonitorStamina() {
    // ...
    GetGame().GetCallqueue().CallLater(MonitorStamina, STAMINA_MONITOR_INTERVAL_MS, false);
}
```

- 每个 AI 实体都通过 `SetAllowNativeStaminaSystem(false)` 启动此循环
- 64 AI × 20次/秒 = 1280 次 `GetStamina()` + `CorrectStaminaToTarget()` 调用/秒
- 群组代理模式下队员也应该禁用监控循环

---

## 5. MudSlip 泥泞滑倒 (2 文件)

### 5.1 SCR_MudSlipEffects.c + SCR_RSS_MudSlipRunner.c

#### ⚠️ MAJOR: 镜头颤抖可能导致晕动症

- 频率: 9-12Hz（人体前庭系统最敏感区间）
- Roll 幅度: 最高 6°，偏航 2.9°
- 无随时间衰减机制——只要泥泞应力持续，晃动就一直存在

---

## 6. Gadgets 战术物品 (5 文件)

### 6.1 SCR_CombatStimStateMachine.c

#### 🔴 CRITICAL: OD 致死路径不恢复流血倍率

```c
if (shouldDie) {
    damageMgr.Kill(selfInstigator);
    return;
}
```

- `Kill()` 即时执行，`m_iCombatStimPhase` 在设置前就已经 return
- `RSS_CombatStim_UpdateBleedingScale()` 的恢复逻辑不会执行
- `m_fRSS_CombatStimBleedingBaseline` 不会被重置回基线

---

## 问题汇总

| 等级 | 数量 | 代表性问题 |
|------|:----:|-----------|
| 🔴 CRITICAL | 7 | World null 检查缺失、静态对象竞态、Pandolf 数值溢出、OD 致死路径 |
| ⚠️ MAJOR | 12 | 阈值重叠、快路径值偏差、窗口结算冲突、AI 监控开销、镜头晕动 |
| MINOR | 8 | 常量重复、死代码分支、调试值遗留、注释误导 |

### 修复优先级排序（影响面从高到低）

1. **PlayerBase.c — World null 检查**：`GetGame().GetWorld()` 后判空，防退出崩溃
2. **SCR_StaminaUpdateCoordinator — 静态对象竞态**：改为新分配或加重入检测
3. **SCR_StaminaOverride — AI 监控开销**：AI 实体无需 50ms 循环
4. **SCR_RealisticStaminaSystem — Tobler 0.7 缩放**：明确设计意图
5. **SCR_EncumbranceCache — 状态不一致**：owner 为 null 时标记无效
6. **SCR_SlopeSpeedTransition — SNAP_THRESHOLD 误触发**：提高阈值或加滞回
7. **SCR_StaminaConsumption — 快路径输出**：与完整路径对齐
8. **SCR_FatigueSystem — 数学错误**：移除 `fatigueTimeDelta < 1.0` 条件
9. **SCR_CombatStimStateMachine — OD 路径**：确保 Kill 前重置倍率
10. **SCR_NetworkSync — 关闭后继续上报**：加二次检查
