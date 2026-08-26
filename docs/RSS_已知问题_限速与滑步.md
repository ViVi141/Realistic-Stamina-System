# RSS 已知问题：Walk 下限过快 & 滑步

> **中文** | [English](en/KNOWN_ISSUES_SPEED_SLIP.md)

记录日期：2026-07-24  
更新：2026-08-09 — 对齐 **6.1.x drain-only** 默认：`V6_APPLY_CP_METABOLIC_SPEED_CAP = false`（CP/有氧巡航顶默认不写 `SetSpeedLimit`；透支只扣 STA/W′）。  
更新：2026-08-26 — **v6.1.7 起默认改为 `= true`**（W′ 耗尽后经 `SetSpeedLimit` 压 CP 巡航指令速度）；文中标注「默认关 / drain-only」的条目仅适用于 ≤6.1.5。

相关开关（`SCR_RSS_Constants`，以源码为准）：

- `V6_APPLY_STAMINA_SPEED_LIMIT = true`（负重/坡度等仍写 `SetSpeedLimit`）
- `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`（**关**物理水平硬/软钳）
- `V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP = false`（**关** CP 巡航物理旁路钳）
- `V6_APPLY_CP_METABOLIC_SPEED_CAP = true`（**v6.1.7 起默认开**：W′ 耗尽后经 SetSpeedLimit 压 CP/有氧巡航顶；≤6.1.5 默认关，drain-only）
- `V6_USE_MARCH_GAIT_SPEEDS = false`（March 档关）
- `V6_RUN_GAIT_FLOOR_MS = 2.2`（Run 步态带下沿；低于此且过软带则跳过越步态巡航帽）
- `V6_RUN_GAIT_DEMOTE_TO_WALK = true`（true=掉出 Run 带不把 Walk 速度写进 Run 的 `SetSpeedLimit`；W′ 空时改切引擎 Walk 档，见 `V6_CP_OUT_OF_BAND_WALK_OVERRIDE`；硬钳开时仍抬地板）
- `V6_CP_OUT_OF_BAND_WALK_OVERRIDE = true`（W′ 空且反解掉出 Run 带时 `SetDynamicSpeed(0.5)` 切 Walk）
- `V6_GAIT_SPEED_LIMIT_MIN_FRAC = 0.50`（`SetSpeedLimit` 相对相位顶下限；条空跛行在 Run 上仍托此值防滑步）
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

> **v6.1.7+ CP 巡航路径**：W′ 空时 Walk 故意托在 `V6_CP_HIKE_FLOOR_MS=1.0`，避免代谢裸反解把 `v_limit` 拧到 ~0.5 m/s（Walk 动画对 1.7 m/s 物理 = 滑步）。下面「应低于 1.0」只适用于 March 意图下限（`V6_WALK_START_MIN_MS=0.35`），不适用于解除武装后的代谢帽。

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
- **不要**用 `SetSpeedLimit` 把 Walk 米每秒写进 Run 相位（滑步）。掉出 Run 带时改走 `SetDynamicSpeed(0.5)+SetShouldApplyDynamicSpeedOverride`（与 CapsLock 同款，见 `V6_CP_OUT_OF_BAND_WALK_OVERRIDE`）；覆盖期间滚轮被锁，松开 W / W′ 回来后还原。

### 处置

- 默认：`V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`。
- 限速只走 `SetSpeedLimit`（与灌木等 min 合并）；掉出 Run 带另切引擎 Walk 档。
- v6.1.7 起默认写 CP 代谢帽（`V6_APPLY_CP_METABOLIC_SPEED_CAP = true`）；≤6.1.5 默认不写。

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
- v6.1.7 起默认开启 `V6_APPLY_CP_METABOLIC_SPEED_CAP`：平路/上坡压巡航顶；下坡与极陡跳过物理钳逻辑。
- 巡航反解：速度伺服坡度钳到 15%、η 不进反解；Walk 地板 `V6_CP_HIKE_FLOOR_MS=1.0`（约 3.6 km/h）。网格 15–18° + 29 kg 不再把 **Walk** 拧成 0.4 m/s 爬行；消耗仍按实测坡度。
- Run 步态带：CP 反解 ≥ `V6_RUN_GAIT_FLOOR_MS`（或软带）才写巡航帽；掉出 Run 带则 **不** 把 Walk 速度写进 Run 的 `SetSpeedLimit`。W′ 空且仍按住移动时改切引擎 Walk 档（`V6_CP_OUT_OF_BAND_WALK_OVERRIDE`，HUD `步态覆盖=on`，`类型=Walk`，约 1.0–1.45 m/s）。松开 W 后还原滚轮。`SetSpeedLimit` 不得低于相位顶的 0.5×。

### 复测

- 偶发 `v_meas` 略高于限速可接受（勿开物理钳）。
- 陡坡重装、W′ 降到解除武装（约 <25%）、仍按住 W：HUD `步态覆盖=on`，`类型=Walk`，`v_meas` 约 1.0–1.45 m/s。过脊下坡**仍保持 Walk**（不得 `步态覆盖` 连闪、Walk 动画对 3 m/s）。松开 W 约 0.25 s 后 `步态覆盖=off`。
- 切 Walk（约 10–13°、29 kg、W′ 空）：`v_limit` 应约 **1.0 m/s**（徒步地板），`最终倍` ≥ 0.5× Walk 顶；不得再出现 `v_limit≈0.5` + Walk 动画对 1.7 m/s 物理（滑步）。
- 站立 Idle：`最终倍` 应约 **0.999**（保持限速源），不得为 `0.0027` / HUD `倍率0x`。按 W 起步第一帧 Run/Walk 应立刻进入步态带，不得先爬 `v_limit≈0.48`。
- 条空仍按住 Run：出现 `Exhausted: limp speed` 后 `最终倍` 应立刻约 **0.5×**（疲惫慢跑 ~1.8 m/s），不得按 1.25/s 往 0.15 拧。切 Walk 后 `v_limit` 约 **1.0 m/s**。下坡 `v_meas` 仍可高于指令（物理钳关）。
