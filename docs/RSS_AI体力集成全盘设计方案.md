# RSS AI 体力集成 — 全盘设计方案

> 作者：系统设计  
> 日期：2026-05-15  
> 状态：**设计案**（未实现）

---

## 〇、设计原则

| # | 原则 | 含义 |
|---|------|------|
| P1 | **不侵入原生** | 不修改 `scripts/Game/AI/` 下任何代码，不修改 `.bt` 行为树 |
| P2 | **生理驱动** | AI 的行为变化是体力/疲劳/受伤的**自然结果**，不是"脚本说它累了" |
| P3 | **分层叠加** | 在原生 AI 的输出流上叠加生理约束，而非替代 |
| P4 | **玩家对称** | AI 受到的体力系统约束与玩家**同源**（同一个 `SCR_StaminaUpdateCoordinator` 计算管线） |
| P5 | **可配置** | 每个子系统有独立开关 + 参数，支持预设 |

---

## 一、体系总览

```
                    ┌──────────────────────────────────┐
                    │     Arma 原生 AI 行为树           │
                    │  DecideActivity / DecideBehavior  │
                    │  输出: "AI 想做什么"              │
                    └──────────────┬───────────────────┘
                                   │ 行为意图
                    ┌──────────────▼───────────────────┐
                    │   RSS 生理约束层 (本设计)         │
                    │                                  │
                    │  ┌─────────────────────────────┐ │
                    │  │ A. 体力状态机 (StaminaState) │ │
                    │  │    每个 AI 的体力状态标签    │ │
                    │  └─────────────┬───────────────┘ │
                    │                │                  │
                    │  ┌─────────────▼───────────────┐ │
                    │  │ B. 行为过滤 (IntentFilter)   │ │
                    │  │    体力不允许 → 降级/替换    │ │
                    │  └─────────────┬───────────────┘ │
                    │                │                  │
                    │  ┌─────────────▼───────────────┐ │
                    │  │ C. 移动限速 (SpeedCap)      │ │
                    │  │    五级档位: Sprint→Run→Jog │ │
                    │  │    →Walk→Idle (体力决定)     │ │
                    │  └─────────────┬───────────────┘ │
                    │                │                  │
                    │  ┌─────────────▼───────────────┐ │
                    │  │ D. 战斗衰减 (CombatDecay)   │ │
                    │  │    精度/射速/后坐力/反应    │ │
                    │  └─────────────┬───────────────┘ │
                    │                │                  │
                    │  ┌─────────────▼───────────────┐ │
                    │  │ E. 群组协同 (GroupSync)     │ │
                    │  │    轮换休息/掩护/体力均摊   │ │
                    │  └─────────────────────────────┘ │
                    └──────────────┬───────────────────┘
                                   │ 修正后的行为 + 限速
                    ┌──────────────▼───────────────────┐
                    │     引擎执行层                    │
                    │  SetMovementTypeWanted +          │
                    │  OverrideMaxSpeed                 │
                    └──────────────────────────────────┘
```

---

## 二、模块 A：体力状态机 `SCR_RSS_AIStaminaState`

### 2.1 概念

每个 AI 实体拥有一个**体力状态标签**（不是简单的 `float staminaPercent`），它是
一个枚举，由体力值和疲劳值联合决定。所有下游模块都只看这个状态标签做决策。

### 2.2 状态定义

```c
enum ERSS_AIStaminaState
{
    FRESH,          // 充沛: 体力 ≥ 80%, 疲劳 < 0.3
    WINDED,         // 微喘: 体力 50%~80%
    FATIGUED,       // 疲劳: 体力 25%~50%
    EXHAUSTED,      // 力竭: 体力 10%~25%
    COLLAPSED,      // 崩溃: 体力 < 10%
    RECOVERING      // 恢复中: 体力在回升但尚未到 FRESH
}
```

### 2.3 状态转移

```
FRESH → WINDED      : 体力 < 80%
WINDED → FATIGUED   : 体力 < 50%
WINDED → FRESH      : 体力 ≥ 85%（滞回）
FATIGUED → EXHAUSTED: 体力 < 25%
FATIGUED → WINDED   : 体力 ≥ 55%（滞回）
EXHAUSTED → COLLAPSED: 体力 < 10%
EXHAUSTED → FATIGUED : 体力 ≥ 30%（滞回）
COLLAPSED → RECOVERING: 静止 ≥ 3s
RECOVERING → EXHAUSTED: 体力 ≥ 15%
RECOVERING → FATIGUED : 体力 ≥ 30%

任何状态 → RECOVERING（强制）: 体力 < 5% 持续 5s → 进入强制恢复
```

**滞回（hysteresis）的关键性**：避免在边界反复横跳。例如 `FATIGUED` 在 25% 触发，
但必须恢复到 30% 才退出——否则 AI 会在 24.9%~25.1% 之间反复切换行为。

### 2.4 体力消费的来源

AI 的体力消耗与玩家**完全相同**：

- `SCR_StaminaUpdateCoordinator`（已有）对 AI 同样计算速度 → 消耗率 → 恢复率
- Pandolf 公式同样适用
- 环境因子（温度/坡度/地形）同样适用
- 疲劳累积（`SCR_FatigueSystem`）同样适用

**与玩家的差异点：**

| | 玩家 | AI |
|--|------|----|
| 目标速度 | 按键决定 | 行为树决定 |
| 更新频率 | 固定 200ms | 依距离 LOD 动态调整（200ms~5s） |
| HUD 显示 | 有 | 无 |

---

## 三、模块 B：行为意图过滤 `SCR_RSS_AIIntentFilter`

### 3.1 思路

原生行为树通过 `SCR_AIUtilityComponent.EvaluateBehavior()` 选出一个行为。
RSS 不在决策过程中插入（不可行），而是在**行为被选定之后、执行之前**做过滤。

### 3.2 过滤规则表

在 `PlayerBase.c` 的体力 tick 中（每 200ms），对 AI 实体调用：

```
IntentFilter.Apply(owner, staminaState, threatState);
```

| 体力状态 | 被过滤的行为 | 替换行为 | 条件 |
|----------|------------|---------|------|
| EXHAUSTED | Attack / Suppress / MoveAndInvestigate / Pursue | 强制 Wait | 非 THREATENED |
| EXHAUSTED | Attack / Suppress | 降级为 FindFirePosition（原地射击） | THREATENED（保命时仍可战斗） |
| FATIGUED | Sprint 相关 CombatMove | CombatMove_Stop | 始终 |
| FATIGUED | MoveIndividually（追击） | 不拦截，但限速 RUN + OverrideMaxSpeed(×0.65) | 非 THREATENED |
| COLLAPSED | 所有移动行为 | Idle / Wait | 始终 |
| COLLAPSED | Attack | 不拦截（最后自卫） | THREATENED |
| RECOVERING | MoveAndInvestigate / Pursue | Wait | 体力恢复到 FATIGUED 前 |

### 3.3 实现方式

不修改 `EvaluateBehavior`。在 `PlayerBase.c` 的 tick 中：

```c
// 伪代码
if (state == ERSS_AIStaminaState.EXHAUSTED && !threatened)
{
    AIActionBase currentAction = utility.GetCurrentAction();
    if (IsAggressiveAction(currentAction))
    {
        // 将 Wait 行为优先级临时提到最高
        SCR_AIWaitBehavior wait = SCR_AIWaitBehavior.Cast(
            utility.FindActionOfType(SCR_AIWaitBehavior));
        if (wait)
            wait.SetPriority(PRIORITY_BEHAVIOR_WAIT + 1);
    }
}
```

---

## 四、模块 C：五级移动限速 `SCR_RSS_AISpeedCap`

### 4.1 设计

AI 通过 `AICharacterMovementComponent.SetMovementTypeWanted()` 表达移动意图。
RSS 在每帧体力更新后调用 `ApplySpeedCap`，根据体力状态覆写允许的移动类型。

### 4.2 限速表

| 体力状态 | 允许的最高移动类型 | 对应速度 (m/s) | OverrideMaxSpeed 倍率 |
|----------|-------------------|---------------|----------------------|
| FRESH | SPRINT | ~5.5 | 1.0 |
| WINDED | RUN | ~3.8 | 1.0 |
| FATIGUED | RUN（超限速） | ~2.5（受限 RUN） | ×0.65 |
| EXHAUSTED | WALK | ~1.5 | ×0.40 |
| COLLAPSED | IDLE（无移动） | 0 | ×0.0 |
| RECOVERING | WALK | ~1.5 | ×0.40 |

### 4.3 特殊场景覆盖

| 场景 | 规则 |
|------|------|
| 被压制（THREATENED） | 不限速，允许 Sprint 逃命 |
| 在载具中 | 不限速 |
| 游泳 | 使用 RSS 已有游泳模型 |
| 群组队长被限速 | 由 GroupSync 模块协调 |

---

## 五、模块 D：战斗衰减 `SCR_RSS_AICombatDecay`

### 5.1 设计思想

体力状态影响 AI 战力，但不是单一的 `stamina% → skill` 映射。
**分维度衰减**——不同战斗属性在不同体力阶段开始恶化。

### 5.2 衰减矩阵

| 体力状态 | Perception | AimError | FireRate | 后坐力 | 反应延迟 | 投掷精度 |
|----------|-----------|---------|---------|--------|---------|---------|
| FRESH | 100% | 100% | 100% | 100% | 0ms | 100% |
| WINDED | 95% | 95% | 100% | 100% | +50ms | 100% |
| FATIGUED | 80% | 80% | 85% | 110% | +150ms | 90% |
| EXHAUSTED | 60% | 55% | 60% | 130% | +400ms | 70% |
| COLLAPSED | 40% | 30% | 30% | 160% | +800ms | 50% |
| RECOVERING | 80% | 80% | 90% | 105% | +100ms | 90% |

### 5.3 已有原生 API 映射

| 属性 | API | 可用性 |
|------|-----|--------|
| 感知 | `combat.SetPerceptionFactor(value)` | ✅ 公开 |
| 射速 | `combat.SetFireRateCoef(value, false)` | ✅ 公开 |
| 技能 | `combat.SetAISkill(skill)` | ✅ 公开 |
| 后坐力 | 引擎无直接 API → 折中：降低技能档位 | ⚠️ 间接 |
| 反应延迟 | 引擎无直接 API → 折中：降低感知系数 | ⚠️ 间接 |
| 投掷精度 | 引擎无直接 API → 通过武器选择逻辑间接影响 | ⚠️ 间接 |

---

## 六、模块 E：群组协同 `SCR_RSS_AIGroupSync`

### 6.1 核心思路：预分配，非事后

**关键洞察**：AI 群组接收到的路点（`AIWaypoint`）已经在 NavMesh 上（引擎验证过），路点附近 ±10m 范围内的点也几乎必然在 NavMesh 上。因此，路点本身就是最安全的休息位置参考。

**新方案**：路点配属时预扫描附近地形，缓存休息候选位置；路点完成时按需插入休息路点。不再在运行时做远距离实时搜索。

```
路点被配属到群组
  ↓
RSS 监听 Event_OnWaypointAdded → 对每个路点做 12m 半径扫描
  ↓
缓存 路点ID → 最佳休息位置 的映射
  ↓
群组完成一个路点 → 检查体力中位数 → 需要休息？
  ├─ 是 → 从缓存取下一个路点附近的休息位置 → 插入 WP_Wait
  └─ 否 → 正常继续
```

### 6.2 预扫描算法

在 `Event_OnWaypointAdded` 触发时，对每个新路点执行一次扫描：

```
ScanNearWaypoint(waypointPos, groupId):
1. 以 waypointPos 为中心，12m 为半径
2. 12 个方向（30°间隔）发射水平射线，头高 1.55m
3. 对每个击中墙壁/岩石/树的射线：
   a. 沿法线方向后退 0.8m
   b. SnapToGround → 检测落差是否 > 2m → 跳过悬崖
   c. 候选点与 waypointPos 水平距离 < 10m → 接受
4. 评分标准：
   - 障碍物距离 AI 越近（3~6m 最佳）→ 掩护效果好
   - 候选点离路点越近 → 路点可达则候选点可达
   - 优先"三面有遮挡"的方向
5. 取最高分
6. 无有效候选 → 缓存为 waypointPos 本身
```

**与当前 RSS 代码的关键改进**：

| 参数 | 当前 (SCR_RSS_AIGroupRestCoordinator) | 新方案 |
|------|---------------------------------------|--------|
| 搜索时机 | 运行时（体力低时现找） | 路点配属时（提前扫描） |
| RAY_LEN | 22m | 12m |
| BACK_OFF | 1.2m | 0.8m |
| 选点策略 | 距离群组中心最近的击中 | 障碍物近 + 三点遮挡评分 |
| 落差检测 | 无 | > 2m 跳过 |
| 缓存 | 无（每次重新找） | 路点 ID → 位置 |

### 6.3 运行时插入时机

```
Event: GetOnCurrentWaypointChanged → 群组切到新路点
  不做操作（AI 还在移动中，没有停下）

Event: GetOnWaypointCompleted → 群组完成了上一个路点
  ↓
评估（每次路点完成触发一次）:

1. 群组状态检查
   - 群组是否存活成员 ≥ 1
   - 是否有成员处于 THREATENED → 跳过
   - 群组控制模式不是 AUTONOMOUS

2. 体力判定
   - 计算全组体力中位数
   - GROUP_FIT (≥70%) → 正常推进
   - GROUP_TIRING (40%~70%) → 检查下一个路点距离：如果 > 300m → 预插入一个休息
   - GROUP_FATIGUED (20%~40%) → 插入 60s WP_Wait 路点
   - GROUP_SPENT (<20%) → 插入 WP_Defend（半径 10m，无限期）直到全员 > 40%

3. 位置分配
   - 从缓存取下一个 `waypoint → restSpot` 映射
   - 有缓存 → 在该位置插入休息路点
   - 无缓存（动态路点无扫描记录）→ 路点位置本身就是合法 NavMesh 点
```

### 6.4 距离估算与预插入

当 `GROUP_TIRING` 且下一个路点据队长实体较远时，可以估算是否需要在路点链中预插入休息：

```
估算逻辑（OnWaypointCompleted 触发）:

当前路点位置 A，下一个路点位置 B
StraightDist = vector.Distance(A, B)
预计路径距离 ≈ StraightDist × 1.4（地形曲折系数）
预计速度 ≈ 群组默认移动速度 × 地形系数
预计耗时 = 预计路径距离 / 预计速度
人均消耗 = 预计耗时 × 当前消耗率（按 WALK 档位估算）
预计抵达体力 = 当前体力中位数 - 人均消耗

if 预计抵达体力 < GROUP_FATIGUED 阈值(25%):
    → 在 A→B 的中点位置插入一个 30s WP_Wait
    → 缓存该位置的邻近遮蔽点
```

注意：这是一个**估算**，不需要精确。误差只会导致：休息太早（多停一次）或休息太晚（体力已低但还能撑到下一站）。两种情况都不会卡死群组。

### 6.5 路点类型选择

| 体力中位数 | 插入路点类型 | 行为 | 超时 |
|-----------|-------------|------|------|
| GROUP_TIRING | WP_Wait | 群组停下等待 | 30s |
| GROUP_FATIGUED | WP_Wait | 群组停下等待 | 60s |
| GROUP_SPENT | WP_Defend | 群组防御姿态+原地恢复 | 无限（直至体力恢复） |

**关键决定**：不用 `WP_Defend` 作为默认休息路点。WP_Defend 会触发 `ActivityDefend`，AI 会被分配到射击位置并寻找目标，反而消耗体力。`WP_Wait` 的行为就是"原地等"——体力消耗为零，恢复正常。

### 6.6 Activity 感知

仅在以下控制模式允许插入休息路点：
- `IDLE`
- `FOLLOWING_WAYPOINT`
- `DEFEND`（原地休整）

**禁止插入休息路点**：
- `AUTONOMOUS`（群组接敌中）
- 任何成员处于 `THREATENED`

### 6.7 队长体力不足

引擎 `SCR_AIGroup` 的队长系统基于 player ID，不提供 AI 间的队长交接 API。

不试图换队长。队长受到的限速与其他成员相同。由于原生群组队形逻辑（`KeepInFormation`），队长速度减慢 → 全队自然减速 → 体力中位数下降更快 → 提前触发预分配休息条件。

### 6.8 自适应群组步速（防掉队）

**问题**：不同兵种负重差异大（AT 兵 38kg vs 步枪兵 22kg），体力消耗不同，移动速度不同 → 队形拉散。

**方案**：群组步速由最慢成员的第 25 百分位决定。快的等慢的，不追。

```
OnWaypointCompleted 中执行:

1. 读取上路段的耗时
   TimeThisLeg = thisWaypoint.CompletedTime - lastWaypoint.CompletedTime

2. 收集每位成员的有效速度估算
   foreach member:
       weight = SCR_CharacterInventoryStorageComponent.GetTotalWeight()
       stamina = GetTargetStamina()
       // 有效移动倍率 = 负重比 × 体力指数曲线
       effectiveRatio = (90 / (90 + weight)) × (stamina ^ 0.6)
       speedEstimate = GAME_MAX_SPEED × effectiveRatio

3. 群组步速 = 取第 25 百分位
   将所有 speedEstimate 排序，取四分之一位置的值
   这样避免单人短时抖动拉低全队

4. 队长的 OverrideMaxSpeed = 群组步速
   队长跑不快的，全队自然跟随（原生 KeepInFormation 机制）

5. 战斗状态跳过
   AUTONOMOUS 模式下不限制步速
```

**整合位置**：`SCR_RSS_AIGroupSync` 的 `OnWaypointCompleted`，在"判定是否需要插入休息路点"之前执行步速计算。

**不依赖角色类型检测**：只用体重和体力两个数值，不需要 `HasRole()`。
**不修改原生队形逻辑**：全靠队长 OverrideMaxSpeed 间接控制。

### 6.10 缓存管理

```
m_mWaypointRestSpots: map<AIWaypoint, vector>
  - 在 Event_OnWaypointAdded 时填充
  - 在 Event_OnWaypointRemoved 时清理对应缓存
  - 在 Event_OnWaypointCompleted + 我们插入的休息路点完成 → 继续原路点链

缓存容量：每个群组 ≈ 路点数 × 4 bytes（vector）
典型值：5~10 个路点 × 4 = 20~40 bytes/群组，可忽略
```

### 6.11 回退路径

当遇到以下情况时，降级为"即时评估+短半径搜索"：

| 场景 | 回退 |
|------|------|
| 路点是动态生成的（`AddWaypointsDynamic`），未经过预扫描 | 以路点位置为休息点，不加遮蔽搜索 |
| 群组在路点之间遭遇激烈战斗后体力骤降 | 跳过预分配，在当前路点完成后即时插入休息 |
| 缓存被清空（世界重载等）而路点链还在 | 以群组当前位置为休息点 |
| 战斗结束后体力极低 | 以战斗结束时的位置为休息点 |

---

## 七、模块 F：伤害-体力联动 `SCR_RSS_AIInjuryLink`

### 7.1 设计

受伤的 AI 体力消耗更快、恢复更慢。

### 7.2 公式

```
effectiveDrainRate = baseDrainRate × injuryDrainMultiplier
effectiveRecoveryRate = baseRecoveryRate × injuryRecoveryMultiplier
```

| 血液剩余 | injuryDrainMultiplier | injuryRecoveryMultiplier |
|----------|----------------------|-------------------------|
| > 90% | 1.0 | 1.0 |
| 70%~90% | 1.15 | 0.85 |
| 50%~70% | 1.4 | 0.6 |
| 30%~50% | 1.8 | 0.3 |
| < 30% | 2.5 | 0.1 |

### 7.3 实现

在 `SCR_StaminaConsumption` 和 `SCR_StaminaRecovery` 中，对非玩家实体检查
`SCR_CharacterDamageManagerComponent.GetBloodHitZone().GetHealth()`，
动态调整消耗/恢复系数。

---

## 八、模块 G：环境-体力联动（继承已有）

RSS 已有 `SCR_EnvironmentFactor`（热应激/冷应激/降雨湿重/风阻/泥泞度）。
AI 完全继承。

**AI 特有的环境行为：**

| 环境条件 | AI 行为修正 |
|----------|-----------|
| 温度 > 35°C | 恢复率×0.8；AI 倾向找阴影 |
| 温度 < -5°C | 消耗率×1.15 |
| 泥泞应激 > 0.6 | 非危险→强制 WALK（已有） |
| 室内 | 忽略风雨（已有） |

---

## 九、性能设计

### 9.1 分层更新频率

| AI 距最近玩家 | 体力全量计算 | 行为过滤 | 移动限速 | 战斗衰减 |
|--------------|------------|---------|---------|---------|
| < 100m | 200ms | 200ms | 200ms | 500ms |
| 100m~300m | 500ms | 500ms | 500ms | 2s |
| 300m~800m | 队长全量+队员同步 | 2s | 2s | 5s |
| > 800m | 队长全量+队员同步 | 5s | 5s | 关闭 |

（远距代理模式复用 `SCR_RSS_AIGroupStaminaProxy.IsSquadProxyModeActive` 的
已有基础设施）

### 9.2 内存

每 AI：`ERSS_AIStaminaState`（1 byte）+ 上一个状态（1 byte）+ 状态进入时间戳（4 bytes）= **6 bytes/AI**。
不增加任何 per-frame 动态分配。

---

## 十、文件规划

### 10.1 新文件

| 文件 | 模块 | 估算 |
|------|------|------|
| `scripts/Game/RSS/AI/SCR_RSS_AIStaminaState.c` | A 体力状态机 | ~120 行 |
| `scripts/Game/RSS/AI/SCR_RSS_AIIntentFilter.c` | B 行为意图过滤 | ~180 行 |
| `scripts/Game/RSS/AI/SCR_RSS_AISpeedCap.c` | C 五级移动限速 | ~150 行 |
| `scripts/Game/RSS/AI/SCR_RSS_AICombatDecay.c` | D 战斗衰减 | ~130 行 |
| `scripts/Game/RSS/AI/SCR_RSS_AIGroupSync.c` | E 群组协同：路点预扫描、缓存管理、插入休息路点 | ~400 行 |
| `scripts/Game/RSS/AI/SCR_RSS_AIInjuryLink.c` | F 伤害-体力联动 | ~80 行 |

### 10.2 修改的现有文件

| 文件 | 修改内容 |
|------|---------|
| `SCR_StaminaConstants.c` | 新增 AI 相关 `[SOFT]` 常量段 |
| `SCR_RSS_Settings.c` | 新增 `SCR_RSS_AIParams` 数据类 |
| `SCR_StaminaConfigBridge.c` | 新增 AI 配置读取方法 |
| `PlayerBase.c` | 在体力 tick 中调用 `IntentFilter.Apply`、`SpeedCap.Apply`、`GroupSync.Tick` |
| `SCR_StaminaUpdateCoordinator.c` | 对 AI 实体增加伤害→消耗/恢复联动 |
| `SCR_StaminaConsumption.c` | 增加 `injuryDrainMultiplier` 路径 |
| `SCR_StaminaRecovery.c` | 增加 `injuryRecoveryMultiplier` 路径 |

### 10.3 删除的现有文件

以下 7 个文件全部废弃，逻辑被新模块覆盖：

```
✗ SCR_RSS_AIStaminaBridge.c        → A + C
✗ SCR_RSS_AIGroupStaminaProxy.c    → 距离 LOD 逻辑并入 A
✗ SCR_RSS_AIStaminaCombatEffects.c → D
✗ SCR_RSS_AIGroupRestCoordinator.c → E（预分配替代事后搜索）
✗ SCR_RSS_AIRestRecoveryRegistry.c → E（路点 Wait 替代恢复锁定）
✗ SCR_RSS_AICoverSeeker.c          → E（预扫描替代运行时 cover 查询）
✗ SCR_RSS_AIMudSlipPolicy.c        → 薄封装逻辑并入 C
```

---

## 十一、调用时序

### 11.1 每帧（200ms）体力更新 — PlayerBase.c 体力 tick

对每个 AI 实体调用：

```
1. SCR_StaminaUpdateCoordinator.Update(ctrl, deltaTime)
   → 计算消耗/恢复/体力净值
   → 如果是 AI 实体 + 伤害联动启用:
       → SCR_RSS_AIInjuryLink.Apply(owner)

2. SCR_RSS_AIStaminaState.Tick(owner, staminaPercent, fatiguePercent)
   → 更新状态机标签

3. SCR_RSS_AISpeedCap.Apply(owner, state)
   → 根据状态设置 OverrideMaxSpeed + MovementTypeWanted

4. SCR_RSS_AIIntentFilter.Apply(owner, state, threatState)
   → 根据状态过滤/替换当前行为

5. SCR_RSS_AICombatDecay.Apply(owner, state)
   → 根据状态调整感知/射速/技能
```

### 11.2 事件驱动 — 群组协同

```
Event: SCR_AIGroup.GetOnWaypointAdded(group, waypoint)
  → SCR_RSS_AIGroupSync.OnWaypointAdded(group, waypoint)
  → 执行预扫描（12m 半径射线搜索）
  → 缓存 waypoint → restSpot 映射

Event: SCR_AIGroup.GetOnWaypointCompleted(group, waypoint)
  → SCR_RSS_AIGroupSync.OnWaypointCompleted(group, waypoint)
  → 计算全组体力中位数
  → 根据 GROUP_TIRING / FATIGUED / SPENT 判定
  → 找到下一个路点 → 从缓存读取 restSpot
  → 插入 WP_Wait 路点或 WP_Defend 路点

Event: SCR_AIGroup.GetOnWaypointRemoved(group, waypoint)
  → SCR_RSS_AIGroupSync.OnWaypointRemoved(group, waypoint)
  → 清理缓存中的对应映射
```

---

## 十二、配置预设

三种预设对应不同的 AI 体力敏感度：

| 参数组 | EliteStandard (硬核) | StandardMilsim (平衡) | TacticalAction (流畅) |
|--------|---------------------|----------------------|----------------------|
| 体力状态阈值 | FRESH≥85%, WINDED≥55%, FATIGUED≥30% | FRESH≥80%, WINDED≥50%, FATIGUED≥25% | FRESH≥70%, WINDED≥40%, FATIGUED≥20% |
| EXHAUSTED 行为 | Attack 完全禁止 | Attack 降级（原地射击） | Attack 降级（射速减半） |
| 战斗衰减 | 全部六维生效 | 四维（无投掷/反应） | 二维（仅精度/射速） |
| 群组轮换休息 | 启用 | 启用 | 禁用（个体自管） |
| 伤害联动 | 全系数 | 减半系数 | 禁用 |
| 限速 | 五级全开 | 四级（跳过 IDLE 强制） | 三级（FRESH / WINDED / FATIGUED 不限速） |

---

## 十三、原生 API 验证结果

### 问题 1：外部代码能 AddAction 吗？

**✅ 可行，但有约束。**

`SCR_AIBaseUtilityComponent` 继承引擎 `AIBaseUtilityComponent`，`AddAction()` 和 `FindActionOfType()` 都是继承自引擎 C++ 层的公开方法，已有大量外部调用先例：

- `SCR_AICombatComponent.c:162` — `m_Utility.AddAction(behavior)`
- `SCR_AIPickupInventoryItemsBehavior.c:40` — `utility.AddAction(m_MoveBehavior)`
- `SCR_AIProvideAmmoBehavior.c:32` — `utility.AddAction(m_MoveBehavior)`

**约束**：无 `SetCurrentBehavior()` 公开方法。`SetExecutedAction()` 由行为树节点 `SCR_AIUpdateExecutedAction` 在游戏循环中调用。这意味着 RSS 无法直接"替换"当前正在执行的行为，但可以用 `AddAction` 插入新行为 + 通过提升优先级（`SetPriority()`）让下一帧决策选中它。

**实现策略**：不替换行为，而是插入 Wait 行为并提高其优先级，原生决策循环会在下一 tick 自然完成切换。

---

### 问题 2：SetAISkill 会触发网络同步吗？

**✅ 不会。本地修改，无自动同步。**

```c
// SCR_AICombatComponent.c:209-216
void SetAISkill(EAISkill skill)
{
    m_eAISkill = skill;
}
```

该方法仅有 `#ifdef AI_DEBUG` 调试日志，**无 `RplRpc` 或 `RplProp` 装饰器**。其他相关方法同理：

| 方法 | RplRpc/RplProp | 网络效果 |
|------|---------------|----------|
| `SetAISkill(EAISkill)` | 无 | 本地修改 |
| `SetFireRateCoef(float)` | 无 | 本地修改 |
| `SetPerceptionFactor(float)` | 无 | 本地修改 |

**但这不影响设计方案**。AI 的瞄准计算在 **服务器端** 执行（`SCR_AIGetAimErrorOffset.c:146` 中 `m_CombatComponent.GetAISkill()` 被用于计算瞄准误差），因此服务器端修改即可影响 AI 战斗表现，无需同步到客户端。

---

### 问题 3：AddWaypointsDynamic 与已有活动路点的交互

**✅ 安全可用。** 实现会调用 `AddWaypoint(wp)`（继承引擎 `AIGroup` 方法），将新路点追加到群组路点队列末尾。群组按路点队列顺序执行，不会打断当前活动路点。

`CompleteWaypoint` 同样是引擎方法，调用后标记当前路点完成，群组自动推进到下一个路点。另有 `Event_OnWaypointCompleted` 事件可用于监听完成状态。

---

### 问题 4：EMovementType 是否包含 JOG？

**❌ 不包含。** 原生枚举只有 4 档：

```c
enum EMovementType
{
    IDLE,
    WALK,
    RUN,
    SPRINT,
}
```

**设计修正**：将五级移动策略中的 JOG 档改为 **"受限 RUN"** 实现：

| 原始设计 | 修正 |
|---------|------|
| FATIGUED → JOG | FATIGUED → RUN + `OverrideMaxSpeed(×0.65)` |
| 前：SPRINT→RUN→JOG→WALK→IDLE | 改后：SPRINT→RUN(超限速)→WALK→IDLE |

具体而言：AI 的行为树会请求 `SetMovementTypeWanted(RUN)`，但 RSS 通过 `OverrideMaxSpeed(×0.65)` 限制实际速度上限，效果上等同于 JOG。

---

### 问题 5：AI 群组 SetLeaderAgent 存在吗？

**❌ 不存在。** `SCR_AIGroup` 的领导者系统基于 **player ID**（`m_iLeaderID`）：

```c
int m_iLeaderID = -1;
void RPC_SetLeaderID(int playerID);  // [RplRpc, Broadcast]
```

`GetLeaderAgent()` 是引擎 C++ 的只读方法（根据群组层级自动决策），脚本侧**没有设置 AI 队长的公开 API**。

**设计修正**：移除"队长体力不足时传位"的设计。改为：

- **队长体力不足时**：RSS 不试图换队长，而是通过 `ApplyOnFootMovementPolicy` 对队长限速，全队自然减慢（因为群组队形逻辑会限制其他成员的速度）
- **轮换休息**：不使用"队长传位"，而是通过现有 `SCR_AIGroup.AddWaypoint` + `CompleteWaypoint` 控制群组节奏，RSS 决定在何处、何时插入休整路点
- **影响**：轮换休息设计简化——不再有 A/B 班交接队长的环节，改为"群组到达休整点，全员 Wait，体力达标后 CompleteWaypoint，继续前进"
