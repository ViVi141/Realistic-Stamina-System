# RSS 已知问题：Walk 下限过快 & 滑步

记录日期：2026-07-24  
更新：2026-07-24 — 关水平硬钳后速画匹配；同日删除 `v_drain=min(v_meas,v_limit)` 补丁，代谢一律按 `v_meas`，数值交孪生/Rust 标定。

相关试跑开关（`SCR_RSS_Constants`）：

- `V6_APPLY_STAMINA_SPEED_LIMIT = true`（负重/坡度仍写 `SetSpeedLimit`）
- `V6_APPLY_HORIZONTAL_SPEED_CLAMP = false`（**关**物理水平硬/软钳）
- `V6_APPLY_CP_METABOLIC_SPEED_CAP = false`（CP/有氧硬顶关）
- `V6_USE_MARCH_GAIT_SPEEDS = false`（March 档关，目标改引擎顶）

### 结论（滑步）

- **根因倾向**：`ClampOwnerHorizontalSpeed` / `PrepareControls` 每帧拧 `Physics` 水平速度，使位移与相位动画顶速脱节 → 滑步。
- **有效做法**：只保留 `SetSpeedLimit` 合并限速（接近 v3.23.1），**不要**再硬钳水平速度。
- 问题 2 状态：**缓解 / 当前配置下可接受**（勿再默认打开水平硬钳，除非有新证据）。

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
