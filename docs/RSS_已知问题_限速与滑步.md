# RSS 已知问题：Walk 下限过快 & 滑步

记录日期：2026-07-24  
更新：2026-08-09 — 对齐 **6.1.x drain-only** 默认：`V6_APPLY_CP_METABOLIC_SPEED_CAP = false`（CP/有氧巡航顶默认不写 `SetSpeedLimit`；透支只扣 STA/W′）。

相关开关（`SCR_RSS_Constants`，以源码为准）：

- `V6_APPLY_STAMINA_SPEED_LIMIT = true`（负重/坡度等仍写 `SetSpeedLimit`）
- `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`（**关**物理水平硬/软钳）
- `V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP = false`（**关** CP 巡航物理旁路钳）
- `V6_APPLY_CP_METABOLIC_SPEED_CAP = false`（**默认关**：不与引擎抢位移；开则经 SetSpeedLimit 压 CP/有氧巡航顶）
- `V6_USE_MARCH_GAIT_SPEEDS = false`（March 档关）
- `V6_RUN_GAIT_FLOOR_MS = 2.2`（Run 步态带下沿；代谢帽开启时低于则降 Walk）
- `V6_RUN_GAIT_DEMOTE_TO_WALK = true`（硬钳开时自动失效，改回抬地板）
- `V6_WALK_START_MIN_MS = 0.35`（Walk 绝对速度硬托底）

### 结论（滑步）

- **根因倾向**：`ClampOwnerHorizontalSpeed` / `PrepareControls` 每帧拧 `Physics` 水平速度，使位移与相位动画顶速脱节 → 滑步。
- **原则**：**只** `SetSpeedLimit` 合并限速；**禁止**直接改 `Physics` 速度。
- **6.1.x 默认**：代谢巡航帽关 → Run 意图可到引擎顶；超额代谢由 STA/W′ 买单，不靠压 `v_limit` 对齐物理。
- **指令 vs 物理**：`SetSpeedLimit` → `OverrideMaxSpeed` 只压**指令**；`v_meas` 仍可高于 `v_limit`（可接受，勿开物理钳）。
- **脚本侧无**可靠步频 / 动画播放速率 API；硬钳只能拧位移，不能让迈步变慢。
- **日志语义**：`超速记账=on(phys)` = `v_meas > v_limit+ε`；`on(sprint)` = 冲刺且 W′ 可超速记账。

---

## 1. Walk 速度下限偏高（≥ ~1 m/s），原版可以更慢

### 现象

- RSS 下 Walk 有效速度常被托在 **约 1.0 m/s 以上**（实机日志常见 `v_limit` / `v_meas` ≈ 1.2–1.3 m/s @ ~30 kg 缓坡）。
- **原版可以更慢**；玩家感觉 RSS Walk「抬得太高」。

### 已改（2026-07-25）

| 位置 | 行为 |
|------|------|
| `CalculateFinalAbsoluteSpeed` / `GetMarchAbsoluteSpeedMs` | `startMin` / Walk clamp 下限改为 `V6_WALK_START_MIN_MS`（**0.35**） |
| `ENGINE_WALK_TOP_MS = 1.45` | March 关时仍为意图顶；疲惫/负重可乘到更低 |

### 复测

- 空载平地 / 30 kg / 上坡：Walk `v_limit` 应能明显低于 ~1.0 m/s。
- 若仍偏快：查意图是否锚死在引擎顶、或坡度/负重惩罚未生效。

---

## 2. 滑步（历史）

### 现象（硬钳开启时）

- 物理水平速度被 RSS `SetSpeedLimit` + `ClampOwnerHorizontalSpeed` 压到低于 **当前相位动画步态顶速**时出现脚滑。

### 已确认约束

- 无可靠动画播放速率 API；`CharacterCommand` 会覆盖动画变量。
- **禁止**用 `SetDynamicSpeed(0.5)` 假按 Walk 对齐步态（会锁死相位）。

### 处置

- 默认：`V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`。
- 限速只走 `SetSpeedLimit`（与灌木等 min 合并）。
- 默认不写 CP 代谢帽（`V6_APPLY_CP_METABOLIC_SPEED_CAP = false`）。

---

## 关联日志特征（便于复测）

- Walk：`类型=Walk`；若仍 ≥1.2 而原版更慢 → 问题 1。
- 开硬钳后再看 Run：`v_meas` 钉死但动画快 → 滑步复现；关硬钳后应匹配。
- `v_pos` 尖峰可忽略（位置差分噪声）。

---

## 3. 下坡停步 EPOC 暴罚（已修）

### 现象（修前）

- 下坡 Run：`v_meas` 远高于 `v_limit`，停步后 `EPOC` 按全量 `P_fat` 采样 → 净耗暴涨。

### 处置

- STA/疲劳积分仍按 `v_meas` 记账；**仅 EPOC** 用 `GetEpocSamplePowerWatts` = 限速内意图速度功率。
- 无冲刺、且无 W′ 武装超速记账时，峰值再钳到 **CP**。
- 复测：同下坡巡航（`W'=0`、`超速记账=off`）后急停，应见弱 EPOC。

---

## 4. 后期（W′ 耗尽后）移动速度抖动

### 现象

- 上/下坡 Run：`最终倍`/`v_limit` 钉死，但 `v_meas` 抖、`P_met` 上千瓦；`超速记账=off`。

### 原因

1. `SetSpeedLimit` 只压指令；物理仍可跑飞。
2. 曾每 2s 解限测顶速 → Chimera 瞬间 `OverrideMaxSpeed(1)` 尖峰（已禁周期解限）。
3. 平路巡航帽在下坡过紧（代谢帽路径下已改为下坡不套）。

### 处置（当前默认）

- 引擎顶速**只测一次**。
- **不开**物理钳；超额走 STA/W′。
- 若临时开启 `V6_APPLY_CP_METABOLIC_SPEED_CAP`：平路/上坡可压巡航顶；下坡与极陡跳过物理钳逻辑。
- Run 步态带（仅代谢帽开启时有意义）：CP 反解 ≥ `V6_RUN_GAIT_FLOOR_MS` 留 Run；更低可降 Walk。

### 复测

- 偶发 `v_meas` 略高于限速可接受（勿开物理钳）。
- 开启代谢帽后：若误降 Walk 过频，略降 `V6_RUN_GAIT_FLOOR_MS`；确认 `V6_RUN_GAIT_DEMOTE_TO_WALK=true` 且硬钳关。
