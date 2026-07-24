# RSS 已知问题：Walk 下限过快 & 滑步

记录日期：2026-07-24  
更新：2026-07-24 — **关死 CP 巡航物理钳**（`V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP=false`）：与水平硬钳同原则，只走 `SetSpeedLimit`。另：**Run 步态地板** `V6_RUN_GAIT_FLOOR_MS=2.2`。此前：下坡不套 2.4；解除武装后 W′ 钳到 CP；EPOC 限速内采样。

相关试跑开关（`SCR_RSS_Constants`）：

- `V6_APPLY_STAMINA_SPEED_LIMIT = true`（负重/坡度仍写 `SetSpeedLimit`）
- `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`（**关**物理水平硬/软钳）
- `V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP = false`（**关** CP 巡航物理旁路钳）
- `V6_APPLY_CP_METABOLIC_SPEED_CAP = true`（CP/有氧巡航顶；经 SetSpeedLimit）
- `V6_USE_MARCH_GAIT_SPEEDS = false`（March 档关，目标改引擎顶）
- `V6_RUN_GAIT_FLOOR_MS = 2.2`（Run 意图 CP 巡航下限）

### 结论（滑步）

- **根因倾向**：`ClampOwnerHorizontalSpeed` / `PrepareControls` 每帧拧 `Physics` 水平速度，使位移与相位动画顶速脱节 → 滑步。
- **原则**：**只** `SetSpeedLimit` 合并限速；**禁止**直接改 `Physics` 速度（含曾加的 `EnforceCpCruisePhysicsCap`）。
- **Run 地板**：W′ 解除武装后 CP 反解若把 Run 压到 ≤~1.9 m/s，引擎易播 Walk 动画 → 用 `V6_RUN_GAIT_FLOOR_MS` 托住。
- 问题 2 状态：**按原则关闭物理钳**；`v_meas` 偶发高于 `v_limit` 属 `SetSpeedLimit` 只压指令速的引擎限制，用代谢记账吸收，不以物理钳强压。

---

## 1. Walk 速度下限偏高（≥ ~1 m/s），原版可以更慢

### 现象

- RSS 下 Walk 有效速度常被托在 **约 1.0 m/s 以上**（实机日志常见 `v_limit` / `v_meas` ≈ 1.2–1.3 m/s @ ~30 kg 缓坡）。
- **原版（无 RSS 压速或引擎原生 Walk）可以更慢**；玩家感觉 RSS Walk「抬得太高、做不到很差/很慢的踱步」。

### 可能来源（代码）

| 位置 | 行为 |
|------|------|
| `SCR_RSS_SpeedCalculator.CalculateFinalAbsoluteSpeed` | `currentSpeed < 0.5` 时把绝对目标抬到 `startMin`，且 **`startMin` 下限写死 0.8 m/s** |
| `SCR_RSS_Constants.ENGINE_WALK_TOP_MS = 1.45` | March 关闭后 Walk 目标锚在引擎空载顶附近，再乘负重/坡度仍偏快 |
| `GetMarchWalkSpeedMs()`（March 关） | 直接返回 `ENGINE_WALK_TOP_MS`，不再用更低的战术/疲惫 Walk |

### 期望方向（待做）

- Walk **允许低于 1 m/s**（至少不低于原版在同等负重/坡度下的慢速区间）。
- 取消或下调 `startMin ≥ 0.8` 的硬托底；疲惫/重装 Walk 应能明显慢于「舒适踱步」。
- 与原版对比标定：空载平地 / 30 kg / 上坡 三档 Walk 下限。

---

## 2. 滑步（历史）

### 现象（硬钳开启时）

- 物理水平速度被 RSS `SetSpeedLimit` + `ClampOwnerHorizontalSpeed` 压到低于 **当前相位动画步态顶速**时出现脚滑。
- 关 CP 硬顶、关 March、只留负重+坡度时，**只要硬钳仍开**，滑步仍在。

### 已确认约束

- 脚本侧 **没有**可靠的步频 / 动画播放速率 API；`CharacterCommand` 会覆盖动画变量。
- **禁止**再用 `SetDynamicSpeed(0.5)` 假按 Walk 对齐步态：会把相位锁死，Run 进不去。
- 硬钳只能拧位移，**不能**让动画迈步变慢 → 与 `SetSpeedLimit` 叠加时最易滑步。

### 处置

- 默认：`V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`。
- 限速只走 `SetSpeedLimit`（与灌木等 min 合并）。

---

## 关联日志特征（便于复测）

- Walk：`类型=Walk`，倍率应对 **Walk 顶**；若仍 ≥1.2 而原版更慢 → 问题 1（仍开）。
- 开硬钳后再看 Run：`v_meas` 被钉死但动画快 → 滑步复现；关硬钳后应匹配。
- `v_pos` 尖峰可忽略（位置差分噪声）。

---

## 3. 下坡停步 EPOC 暴罚（已修）

### 现象（修前）

- 下坡 Run：`v_meas` 2.5–3.7、`v_limit` ~1.8，`P_met` 可到 ~1.6 kW，有氧侧 `P_bill≈CP` → 跑动中 STA 仅约 **0.04%/s**。
- 一停：`EPOC=on`，净耗跳到约 **0.65%/s**（基础 EPOC × 1.5 封顶），因为峰值按 **全量 `v_meas` 的 `P_fat`** 采样。

### 处置

- STA/疲劳积分仍按 `v_meas` 记账；**仅 EPOC** 用 `GetEpocSamplePowerWatts` = 限速内意图速度功率。
- 无冲刺、且无 W′ 武装超速记账时，峰值再钳到 **CP**（与有氧 `P_bill` 对齐）→ 落入弱 EPOC（约 **0.125%/s**）。
- 峰值在当前功率明显低于峰值时快衰减（400 W/s）。
- 复测：同下坡巡航（`W'=0`、`超速记账=off`）后急停，应见弱 EPOC，而非约 0.65%/s。

---

## 4. 后期（W′ 耗尽后）移动速度抖动

### 现象

- 上/下坡 Run：`最终倍`/`v_limit` 钉死（如 0.50 / 1.82），但 `v_meas` 在 2.5↔3.5 抖，`P_met` 上千瓦；`超速记账=off`。

### 原因

1. `SetSpeedLimit` → `OverrideMaxSpeed` 只压**指令**速度；物理水平速度仍可跑飞。
2. 测引擎顶速曾每 2s `SetSpeedLimit(1.0)`：Chimera 在提速时**瞬间** `OverrideMaxSpeed(1)` → 尖峰。
3. 平路 `V6_AEROBIC_CRUISE_MAX_MS=2.4` 在下坡过紧（已改为下坡不套）。

### 处置

- 引擎顶速**只测一次**，禁止周期抬限。
- **不**用 `EnforceCpCruisePhysicsCap` / 水平硬钳压 `v_meas`（默认关）。
- 下坡不套 2.4 平路巡航帽；解除武装后 W′ 放电钳到 CP。
- Run：`V6_RUN_GAIT_FLOOR_MS` 防止 CP 反解落入 Walk 带。

### 复测

- 同坡 Run、`W'<25%`（解除武装）：`v_limit` ≥ 2.2，动画为 Run；偶发 `v_meas` 略高于限速可接受（勿开物理钳）。
- 若 Walk 感仍重：略抬 `V6_RUN_GAIT_FLOOR_MS`（上限 2.4）。
