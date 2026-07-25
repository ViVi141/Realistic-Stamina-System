# RSS 已知问题：Walk 下限过快 & 滑步

记录日期：2026-07-24  
更新：2026-07-25 — **步态三带**：`≥Run地板` 保留；`Walk顶～Run地板` 软 Run（保留反解）；`<Walk顶` 才降 Walk。修复 37kg 缓坡把 ~2.0 悬崖压到 1.48。Walk `startMin`→`0.35`。

相关试跑开关（`SCR_RSS_Constants`）：

- `V6_APPLY_STAMINA_SPEED_LIMIT = true`（负重/坡度仍写 `SetSpeedLimit`）
- `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`（**关**物理水平硬/软钳）
- `V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP = false`（**关** CP 巡航物理旁路钳）
- `V6_APPLY_CP_METABOLIC_SPEED_CAP = true`（CP/有氧巡航顶；经 SetSpeedLimit）
- `V6_USE_MARCH_GAIT_SPEEDS = false`（March 档关，目标改引擎顶）
- `V6_RUN_GAIT_FLOOR_MS = 2.2`（Run 步态带下沿；低于则降 Walk）
- `V6_RUN_GAIT_DEMOTE_TO_WALK = true`（硬钳开时自动失效，改回抬地板）
- `V6_WALK_START_MIN_MS = 0.35`（Walk 绝对速度硬托底）

### 结论（滑步）

- **根因倾向**：`ClampOwnerHorizontalSpeed` / `PrepareControls` 每帧拧 `Physics` 水平速度，使位移与相位动画顶速脱节 → 滑步。
- **原则**：**只** `SetSpeedLimit` 合并限速；**禁止**直接改 `Physics` 速度（含曾加的 `EnforceCpCruisePhysicsCap`）。
- **Run / Walk 步态带**：W′ **解除武装** 时 CP 反解 ≥ 地板 → 巡航带（≤2.4）；< Walk 顶才降 Walk。W′ **武装** 的纯 Run **不套** 2.4 硬顶（避免 v_limit≈2.0 / v_meas≈2.5 轻滑步）。分母仍用当前相位顶速算 frac。
- **引擎顶速分母（试跑）**：`V6_ENGINE_TOP_LIVE_SAMPLE=true` 时每 tick `GetMaxSpeed` **不解限**取相位顶，再算 `frac`；首次仍解限标定一次。若 live ≪ 标定（&lt;`V6_ENGINE_TOP_LIVE_MIN_RATIO`）则回退标定，避免 `GetMaxSpeed` 被 `OverrideMaxSpeed` 缩放导致 frac→1。改回 `false` 即旧「只缓存一次」路径。
- **MovementMaxSpeed（试跑）**：`V6_TRY_MOVEMENT_MAX_SPEED` **默认 false**（实机无效：v_meas 仍≈引擎 Run 顶）。保留代码路径供复测。
- **Run 有氧巡航压速**：平路/上坡 Run（含「有 W′ 但不冲刺」）一律 `min(意图, CP反解, V6_AEROBIC_CRUISE_MAX_MS)` + Run 地板；**仅真 Sprint+W′ 武装**可超巡航顶。修复前：W′ 武装时跳过巡航 → 日志常见 `v_limit≈3.2` 狂烧 W′。
- **日志语义**：`超速记账=on(phys)` = `v_meas > v_limit+ε`（物理跑飞）；`on(sprint)` = 冲刺且 W′ 可超速记账。旧版仅在冲刺时显示 on，Run 跑飞会误显示 off。
- **非冲刺 W′**：纯 Run **未超速**时 `TickPower` 钳到 CP；**物理超速**（`v_meas>v_limit`）时放开，超额烧 W′。W′ 解除武装后超额改走 STA `超速罚`。
- 问题 2 状态：**按原则关闭物理钳**；`v_limit≈2.0` 而 `v_meas≈2.5` 属指令限速已生效、物理未贴限——用 W′/超速罚买单，不拧 Physics。

---

## 1. Walk 速度下限偏高（≥ ~1 m/s），原版可以更慢

### 现象

- RSS 下 Walk 有效速度常被托在 **约 1.0 m/s 以上**（实机日志常见 `v_limit` / `v_meas` ≈ 1.2–1.3 m/s @ ~30 kg 缓坡）。
- **原版（无 RSS 压速或引擎原生 Walk）可以更慢**；玩家感觉 RSS Walk「抬得太高、做不到很差/很慢的踱步」。

### 已改（2026-07-25）

| 位置 | 行为 |
|------|------|
| `CalculateFinalAbsoluteSpeed` / `GetMarchAbsoluteSpeedMs` | `startMin` / Walk clamp 下限改为 `V6_WALK_START_MIN_MS`（**0.35**，原 0.8 / 0.5） |
| `ENGINE_WALK_TOP_MS = 1.45` | March 关时仍为意图顶；疲惫/负重可乘到更低 |

### 复测

- 空载平地 / 30 kg / 上坡：Walk `v_limit` 应能明显低于 ~1.0 m/s（视负重与坡度）。
- 若仍偏快：再查 March 关时意图是否锚死在引擎顶、或坡度/负重惩罚未生效。

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
- 平路/上坡：W′ 解除武装后可用 `EnforceCpCruisePhysicsCap` 防 Run→Walk 窜速。
- **下坡**（`grade < -2%`）与极陡（`|grade|>35%`）**跳过**物理钳；超额走 STA 超速罚，避免重力与 `v_limit` 互殴抖动。
- 下坡不套 2.4 平路巡航帽；解除武装后 W′ 放电钳到 CP。
- Run：CP 反解 ≥ `V6_RUN_GAIT_FLOOR_MS` 留在巡航带；更低则 `ResolveRunCruiseCapMs` 降 Walk。

### 复测

- 同坡 Run、`W'<25%`（解除武装）：若 CP 仍够 → `v_limit` ≥ 2.2 且为 Run 感；若 CP 不够 → `v_limit` 落到 Walk 带（≤~1.45），勿再假抬 2.2。
- 偶发 `v_meas` 略高于限速可接受（勿开物理钳）。
- 若误降 Walk 过频：略降 `V6_RUN_GAIT_FLOOR_MS`；若假慢 Run：确认 `V6_RUN_GAIT_DEMOTE_TO_WALK=true` 且硬钳关。
