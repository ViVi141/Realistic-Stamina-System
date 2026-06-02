# RSS v5 体力系统修改计划

> 版本：草案 **v0.3**（2026-05-28，二次审查补充）  
> 背景：v4 数字孪生对齐 PlayerBase 后，35 kg 理想环境下的数据暴露 **速度—代谢—恢复** 三层语义冲突；本计划旨在 **同时** 支持「负重行军 4–5 小时」与「无氧冲刺秒级—分钟级限制」，并消除「Pandolf 外皮 + 单条体力池」的结构性矛盾。  
> 原则：**用游戏化分层机制制造可信负重体验**，而非抛弃游戏性追求死板生理仿真。  
> 稳定线：**3.23.x**（含 3.23.1 灌木桥接）与 **dev/v5** 实验分支分离，见第十二章。

---

## 一、问题陈述（现状诊断）

### 1.1 三套叙事混在一个体力条里

| 语义 | 玩家理解 | 当前实现 |
|------|----------|----------|
| **一般体力** | 能不能继续走/跑 | 0–1 单池 `stamina` |
| **冲刺资格** | 能不能按 Shift 爆发 | `sprint_enable_threshold` 切档 |
| **彻底力竭** | 能不能动 | `EXHAUSTION_THRESHOLD = 0` → 跛行，非完全静止 |

三者在参数与 UI 上未分离，导致「16 秒再冲刺」被误读为「从瘫倒复活」。

### 1.2 速度—消耗解耦（核心 bug）

| 移动 | RSS 限速（35 kg Elite） | 消耗测速（GetVelocity / 引擎原速） |
|------|-------------------------|-------------------------------------|
| 步行 | ~2.87 m/s | 实测 / 限速（较一致） |
| 跑步 | ~3.59 m/s | **固定 3.8 m/s** |
| 冲刺 | ~4.14 m/s | **固定 5.5 m/s** |

**看起来没那么快，代谢按更快档位扣**——生理上与游戏手感均不自洽。

### 1.3 Pandolf 被 coeff 压平

- Pandolf 在 2.87 m/s、125 kg 下约 **~2000 W**（不可持续数小时）。
- `energy_to_stamina_coeff`、`load_metabolic_dampening` 等将消耗压成 **−13%/min 线性扣条**。
- 结果：**既不是** 4–5 小时行军（应 ~1.2–1.5 m/s），**也不是** 高代谢数分钟崩溃。

### 1.4 优化器目标与生理锚点错位

- v4 优化 8 个 **战斗任务剖面**（combat_ease / recovery_ease），不是行军/冲刺生理基准。
- 数字孪生对齐实机后，优化器与实机一致，但 **一致 ≠ 生理正确**。

---

## 二、设计目标（v5 应达成什么）

### 2.1 双锚点生理模型

**锚点 A — 负重行军（35 kg，理想环境）**

| 指标 | 目标 |
|------|------|
| 可持续行军速度 | **1.2–1.5 m/s**（4.3–5.4 km/h） |
| 连续行军时长 | **≥ 3 h** 后明显疲劳，**4–5 h** 需 substantial 休息（非 14 min 归零） |
| 代谢功率 | Pandolf @ 1.4 m/s ≈ **350–500 W** 净消耗，接近 **动态平衡** 或缓降 |

**锚点 B — 无氧爆发（35 kg）**

| 指标 | 目标 |
|------|------|
| 满速冲刺维持 | **5–15 s**（职业运动员级，非 57 s） |
| 禁止冲刺后 | 降为跑步/跛行，**非** 继续 5.5 m/s 扣条 |
| 再冲刺冷却 | **分层**（见 2.4）：抽干无氧池 **120–180 s**；战术短冲（2–3 s 主动松开）**60–90 s** |
| 彻底力竭 | 与「禁止冲刺」分离定义（见 2.2）；默认保持跛行，非完全停步 |

**锚点 C — 接敌战斗（保留 v4 任务场景）**

- 8 场景剖面作为 **回归测试**，不是唯一优化目标。
- 三档预设差异：**行军耐受 > 爆发频率 > 恢复节奏**，而非单池扣条快慢。

### 2.2 语义分层（必须写进文档与 UI）

```
┌─────────────────────────────────────────────────────────┐
│  aerobic_stamina (有氧池)     0–1   行军、慢跑、长时间活动 │
│  anaerobic_burst (无氧池)     0–1   冲刺爆发资格与冷却    │
│  collapse_state (力竭态)      布尔  极低有氧 + 跛行/静止  │
└─────────────────────────────────────────────────────────┘
```

| 状态 | 行为 |
|------|------|
| `aerobic` 高 | 正常 Walk / Run 限速 |
| `anaerobic` 耗尽 | **禁止 Sprint**，Run 可继续（消耗有氧池） |
| `aerobic` 低（如 <15%） | 跛行 + 步行恢复区（保留现有 walk recovery zone） |
| `collapse` | 可选：完全禁止 Run，仅 Walk/Limp（配置项，默认 **关闭** 以免太废） |

**原则**：玩家看到的「体力条」默认显示 **有氧池**；无氧池 **不做第二条 RPG 式资源条**（见 2.4 UI）。

**玩家应能一眼读懂**：

- 「我不能冲了」≠「我要倒了」（无氧耗尽 vs 有氧过低）
- 「还要等 3 分钟」= 无氧池在回满，不是主条在发呆

### 2.3 行军节奏：避免 1.4 m/s 的枯燥感

1.4 m/s 是 **Elite 行军锚点**，不是全档位唯一玩法速度。

| 机制 | 说明 |
|------|------|
| **档位差异** | Tactical **1.5–1.7 m/s** 日常行军；Elite **1.3–1.5 m/s** 长时可持续 |
| **强行军姿态**（Phase 2+，可选输入） | 速度 **~1.8 m/s**，有氧消耗明显加快；持续 **10–15 min** 后触发高代谢降速，须降回 1.4 恢复 → 形成「快走—慢走」循环 |
| **硬撑**（见 3.1） | 降速触发后，玩家可短时维持更高速度，有氧池加速消耗（玩家选择，非系统 bug） |
| **地图/任务** | 不假设纯徒步行军跨全图；载具、部署、动态事件仍承担节奏责任 |

### 2.4 UI 与无氧呈现（避免 RPG 双条出戏）

| 元素 | 表现 |
|------|------|
| **主体力条** | 仅有氧池（现有大条） |
| **冲刺充能** | 冲刺键高亮 / 准星周围 **冷却环**（类似技能 CD）；**仅在冲刺相关时显示** |
| **强制降速反馈** | 速度条黄/红区 + 沉重喘息音效 + 轻微视角收窄；可选 HUD 提示「节奏过快，建议减速」 |
| **FOV** | 沿用现有 tactical sprint burst FOV，绑定无氧池而非第二进度条 |

**禁止**：常驻「法力+体力」双条布局。

### 2.5 无氧冷却分层（战术短冲 vs 抽干）

| 情形 | 冷却目标 | 设计意图 |
|------|----------|----------|
| **无氧池抽至禁止线**（冲刺至自动降速） | Elite **120–180 s**；Tactical **60–90 s** | 珍贵战术资源，不可 spam |
| **主动短冲 2–3 s 后松开** | 约为满冷却的 **40–50%**（如 Elite **60–90 s**） | 鼓励掩体间短冲，而非一次性抽干 |
| **冷却计算** | `cooldown = base × (1 - early_release_bonus × reserve_ratio)` | `reserve_ratio` = 松开时无氧剩余比例 |

### 2.6 非目标（避免 scope 爆炸）

- 不建模热射病、横纹肌溶解等医学级 collapse。
- 不重写 Arma 动画/引擎移动组件；仍通过 `SetSpeedLimit` + `GetVelocity` 桥接。
- 不在 v5.0 一次性重做 AI 群组代理（可 Phase 4 跟进）。

---

## 三、架构改动概要

### 3.1 代谢—速度闭环（Phase 1 核心）

**规则**：`CalculateStaminaConsumption` 的 `velocity` 输入 **必须** 与 Pandolf 所用速度一致。

```
消耗速度 v_drain = clamp(v_measured, 0, v_theoretical_max)
```

| 情形 | v_drain |
|------|---------|
| Walk | `GetVelocity` 水平速度（现状） |
| Run | `min(GetVelocity, run_theoretical)`，**禁止** 固定 3.8 |
| Sprint | `min(GetVelocity, sprint_theoretical)`，**禁止** 固定 5.5 |
| Sprint 无氧池空 | 意图降级 Run，`v_drain` 随降级速度下降 |

**高代谢强制降速**（新逻辑）：

```
pandolf_w = Pandolf(v, weight, grade, terrain)
if pandolf_w > sustainable_watts(load, fitness):
    v_max_theoretical *= decay_factor   // 每 tick 或每 N 秒下调限速
```

- `sustainable_watts`：默认 **400 W**（可调，按预设档 350–450 W）。
- 35 kg @ 1.4 m/s 应 **≤ sustainable**；@ 2.87 m/s 应 **>> sustainable** → 数分钟内强制降到 ~1.4 m/s。

**强制降速的体验设计**（避免被误认为 bug / 输入延迟）：

1. **默认**：代谢超限时 **渐进** 下调 `SetSpeedLimit`（非一帧腰斩）。
2. **反馈**：同步触发 UI/音频/视角反馈（见 2.4）。
3. **硬撑**（可选，按住加速不松）：允许 **30–60 s** 内维持更高速度，有氧池 **额外惩罚系数 ×1.5–2.0**；硬撑结束后强制进入恢复窗口。
4. **自动建议**：检测到持续超速时，可提示切换步行（不强制夺控）。

### 3.2 双池动力学（Phase 2 核心）

**无氧池 `anaerobic_burst`**（新状态，挂 PlayerBase / ExerciseTracker）：

| 事件 | 变化 |
|------|------|
| 进入 Sprint | 按 `v_drain` 与负重 **秒级** 扣 anaerobic |
| anaerobic < sprint_enable | 禁止 Sprint（与现 threshold 对齐但 **独立池**） |
| 静止 / 慢走 | anaerobic 恢复；满池时间由 **分层冷却**（2.5）决定 |
| Tactical burst 8 s | **并入** anaerobic 池的快速消耗段（FOV/音效保留；不再独立第三套叙事） |

**有氧池 `aerobic_stamina`**（现有 `stamina` 语义收窄）：

- Walk / Run 消耗主要扣有氧池。
- Sprint 同时扣两池（无氧主、有氧次）。
- 恢复逻辑保留多维度恢复，但 **coeff 物理化**（见 3.3）。

### 3.3 系数物理化（Phase 1–2）

**目标**：去掉「Pandolf 算 2000 W 再 ×1e-6  fudge」的不透明路径。

1. 引入 `reference_sustainable_watts`（如 400 W）与 `reference_march_speed_ms`（如 1.4 m/s）。
2. 在参考点标定：`energy_to_stamina_coeff` 使 **1.4 m/s、125 kg、平路** 净消耗 ≈ **0%/min ± 小幅负**。
3. `load_metabolic_dampening` 降为 **1.0** 或仅用于 AI；玩家路径不再 dampen 负重代谢。
4. 高速度消耗由 **Pandolf 自然上升 + 强制降速** 实现，而非 coeff 救场。

### 3.4 负重速度曲线重标定（Phase 1）

35 kg 下 **满体力** 目标限速（理想平路）：

| 档位 | Walk | Run | Sprint（burst 内峰值） |
|------|------|-----|------------------------|
| Elite | 1.3–1.5 m/s | 2.5–3.0 m/s | 3.5–4.5 m/s × **5–15 s** |
| Standard | 1.4–1.6 m/s | 2.8–3.2 m/s | 略长 burst |
| Tactical | 1.5–1.7 m/s | 3.0–3.4 m/s | 更长 burst / 更短冷却 |

实现位置：

- `SCR_SpeedCalculation.c` / `RealisticStaminaSpeedSystem`：负重 encumbrance 曲线。
- `walk_speed_at_weight` / `run_speed_at_weight` / burst 倍率：与孪生同步。

---

## 四、分阶段实施计划

### Phase 0 — 基线与工具（1–2 周）

**交付**

- [ ] 生理锚点基准脚本 `tools/bench_physio_anchors.py`  
  输出：35 kg 下 1.4 m/s 行军 4 h、冲刺 10 s、burst 冷却 180 s 等 **pass/fail**。
- [ ] 孪生 `rss_digital_twin_fix.py` 增加双池 stub（可先单池 + 正确 v_drain）。
- [ ] 文档：在本计划顶部链接；`体力系统计算逻辑文档.md` 增加「v5 变更中」 banner。
- [ ] **Workbench 实机体感清单**（不可仅依赖孪生）：  
  - 1 km 徒步行军主观节奏（1.4 vs 1.7 m/s）  
  - 交战中短冲 2–3 s vs 抽干 10 s 的冷却体感  
  - 强制降速时 UI 是否足够清晰  

**验收**

- 现有 v4 预设 bench 可回归；新 anchor bench 全 **fail**（预期）。
- Phase 0 结束前完成 **至少 1 次** 内部实机跑图 + 1 次短交战测试，记录参数调整意见。

---

### Phase 1 — 速度—消耗闭环（2–3 周，最高优先级）

**代码**

| 文件 | 改动 |
|------|------|
| `PlayerBase.c` | 消耗速度改为 RSS 测速或 `min(GetVelocity, theoretical)` |
| `SCR_StaminaUpdateCoordinator.c` | `CalculateLandBaseDrainRate` 统一 v_drain |
| `SCR_SpeedCalculation.c` | 35 kg Walk 目标 ~1.4 m/s；高代谢降速 hook |
| `SCR_StaminaConstants.c` | 新增 `SUSTAINABLE_WATTS_DEFAULT` 等 |
| `tools/rss_digital_twin_fix.py` | 移除 Run/Sprint 固定 3.8/5.5 drain |

**验收（35 kg，20°C，平路）**

| 测试 | Elite 目标 |
|------|------------|
| 1.4 m/s 强制行军 4 h | 有氧池 **> 20%**（允许下降，不归零） |
| 2.87 m/s 试图步行 | **≤ 3 min** 内限速降至 ~1.4 m/s 或更低 |
| Run 消耗速度 | drain 使用的 v **≤** 理论 Run 速，误差 < 5% |
| Sprint 消耗速度 | drain 使用的 v **≤** 理论 Sprint 速（burst 段除外） |

---

### Phase 2 — 无氧池与冲刺冷却（2–3 周）

**代码**

| 文件 | 改动 |
|------|------|
| 新：`SCR_AnaerobicBurstState.c` | anaerobic 池、分层冷却、**服务器权威**状态 |
| `PlayerBase.c` | Sprint 意图 gated by anaerobic + aerobic |
| `SCR_RSS_Params.c` | 新增 `anaerobic_drain_coeff`、`burst_recovery_base_rate`、`burst_cooldown_full_seconds`、`burst_cooldown_short_seconds` |
| 网络层 | anaerobic 池 **必须联机同步**（浮点 + 冷却态；防「敌人本地 CD 好了」） |
| `CharacterCamera1stPerson.c` | FOV 绑定 anaerobic 池（已有 burst FOV） |
| `SCR_UISignalBridge.c` / HUD | 冲刺冷却环 + 降速警告（**非**第二体力条） |

**验收**

| 测试 | Elite 目标 |
|------|------------|
| 35 kg 满速 Sprint | anaerobic 耗尽 **5–15 s** |
| 无氧抽干 → 站立 | **≥ 120 s**（Elite）方可再 Sprint |
| 短冲 2–3 s 松开 | **60–90 s**（Elite）可再 Sprint |
| Sprint 期间 | GetVelocity 峰值随 anaerobic 下降（速度崩塌） |
| 有氧池 | 短冲刺不应单独导致 14 min 行军崩溃 |

---

### Phase 3 — coeff 物理化与三档重优化（2 周）

**代码**

| 文件 | 改动 |
|------|------|
| `rss_pipeline_v5.py`（新） | 目标函数 = 生理锚点 pass 率 + 8 场景 soft 约束 |
| `SCR_RSS_Settings.c` | v5 预设嵌入 |
| `embed_json_to_c.py` | 支持 v5 schema |

**优化器新约束（硬）**

```
35kg_ideal_march_4h        → aerobic_end >= 0.20
35kg_sprint_burst          → anaerobic_empty <= 15s
35kg_sprint_cooldown       → recovery >= 120s
drain_velocity_consistency → max error 5%
```

**优化器软目标（保留）**

- 8 场景 exhaustion 分布、三档 ordering。

---

### Phase 4 — AI / 文档 / 迁移（1–2 周）

- [ ] `SCR_RSS_AIStaminaState.c`：AI **简化 anaerobic 计数器**；行为树 Sprint 条件检查该值（冷却可略短于玩家以保证挑战，**禁止无限冲刺作弊**）。
- [ ] 更新 `数字孪生优化器计算逻辑文档.md`、`体力系统计算逻辑文档.md`。
- [ ] **v4 兼容预设** `LegacyV4Style`（参数接近 v4 Tactical，单池行为可选保留开关）。
- [ ] 首次进入 v5：**版本检测 + 弹窗**「体力系统已重构，建议重新选择预设」+ 链接社区说明（可直接引用本文第一节）。
- [ ] 可选：`collapse_state` 极硬核模式（单独 preset 开关）。

---

## 五、参数草案（v5 Elite @ 35 kg 起点）

| 参数 | v4 Elite | v5 草案 | 说明 |
|------|----------|---------|------|
| Walk 满速 | 2.87 m/s | **1.40 m/s** | 行军档 |
| Run 满速 | 3.59 m/s | **2.80 m/s** | 急行档 |
| Sprint burst 峰值 | 4.14 m/s（恒速扣条） | **4.0 m/s × 10 s** | 随后衰减 |
| energy_to_stamina_coeff | 9.4e-7 | **按 400 W @ 1.4 m/s 反算** | 物理锚点 |
| load_metabolic_dampening | 0.70 | **1.0** | 不再削弱负重代谢 |
| sprint_enable_threshold | 0.209（有氧） | **anaerobic 池 20%** | 池分离 |
| burst_cooldown_full | ~16 s（隐式） | **180 s**（Elite） | 无氧抽干 |
| burst_cooldown_short | （无） | **60–90 s**（Elite） | 2–3 s 战术短冲 |
| sustainable_watts | （无） | **400 W** | 强制降速阈值 |

> Standard / Tactical 在同一框架下调整：更高 sustainable_watts、更长 burst、更短 cooldown。

---

## 六、数字孪生同步清单

| 模块 | 必须对齐 |
|------|----------|
| `get_drain_velocity_ms` | Phase 1 起与 C 端一致 |
| `game_player_tick` | 双池 + 降速 |
| `bench_physio_anchors.py` | CI 门禁 |
| `bench_35kg_presets.py` | 修正 `Phase.speed_ms=0` 误判 IDLE |
| `test_v5_smoke.py` | 替代 v4 smoke |

---

## 七、风险与缓解

| 风险 | 缓解 |
|------|------|
| Walk 1.4 m/s 太慢，Arma 地图跨度过大 | Tactical 1.7 m/s；强行军姿态；载具/任务节奏；Phase 0 实机 1 km 测试 |
| 180 s 冷却导致不敢冲刺 | **分层冷却**（2.5）；短冲 60–90 s；Tactical 档更短 |
| 强制降速像 bug | 渐进降速 + UI/音频/视角反馈 + 硬撑可选（3.1） |
| 双池 UI 过复杂 | 主条仅有氧；冲刺 CD 环；禁止双资源条（2.4） |
| 与引擎动画最低速不匹配 | 保留 `ANIM_SPEED_COMPENSATION`；实测 Workbench |
| **联机无氧不同步** | **服务器权威** anaerobic + 冲刺许可；禁止纯本地预测 |
| AI 无限冲刺 / 从不冲刺 | Phase 4 行为树 gate + 略短 CD（非作弊） |
| 老玩家突然走不动 | `LegacyV4Style` 预设 + 首次弹窗 + 社区说明 |
| 优化器无解 | 先 Phase 1 手工标定 coeff，再开优化 |
| **参数只靠孪生、脱离实机** | Phase 0/3 强制 Workbench 体感门禁 |
| **双锚点 + 8 场景优化无解** | 冲突优先级：生理硬约束 > 灌木 bench > 8 场景 soft > 三档序；Phase 3 前禁止跑 v5 优化器 |
| **灌木 + 高代谢双重惩罚** | 高代谢只调 `RSS_StaminaSpeedLimitToken`；`final = min(rss, foliage, wire)`；验收含灌木内行军 |
| **一次改太多难回滚** | 分步发布：3.23.x 小步 / v5 beta / v5.0（见 12.4） |

---

## 八、建议实施顺序（给开发者）

```
Phase 0 基线脚本
    ↓
Phase 1 v_drain 闭环 + 行军速度降档 + 高代谢降速   ← 最大收益/风险比
    ↓
Phase 2 无氧池 + 冷却
    ↓
Phase 3 优化器重跑 + 三档嵌入
    ↓
Phase 4 AI + 文档 + 发布说明
```

**第一刀（若只改一处）**：`PlayerBase` / `StaminaUpdateCoordinator` 中 Run/Sprint 消耗改用 **min(GetVelocity, theoretical_speed)**，并在 Pandolf 功率 > sustainable_watts 时下调 `SetSpeedLimit`——无需双池即可修复「10 km/h 走 14 分钟」与「5.5 m/s 冲 57 秒」最荒谬的部分。

---

## 九、开放问题 — 评审决议（v0.2）

| # | 问题 | **决议** | 备注 |
|---|------|----------|------|
| 1 | 彻底力竭 | **保持 `EXHAUSTION_THRESHOLD = 0` + 跛行** | 完全停步仅作 optional `collapse_state` 极硬核开关 |
| 2 | 三档哲学 | **采纳** | Elite：4 h 行军 + 长冷却短 burst；Tactical：1.7 m/s 行军 + 短冲 CD **60–90 s**、满冷却 **90–120 s** |
| 3 | tactical sprint 8 s | **并入 anaerobic 池** | FOV/音效保留；消除第三套叙事 |
| 4 | 联机 | **必须服务器权威同步** | 带宽可忽略；公平性优先 |
| 5 | 行军枯燥 | **强行军姿态 + 档位速度差** | Phase 2 可选；Phase 0 实机验证 |
| 6 | v4 过渡 | **`LegacyV4Style` + 首次弹窗** | 非永久双轨，便于社区过渡 |

*后续迭代仍可在 Workbench 实机后微调第五节参数表，但不改变上述架构决议。*

---

## 十、设计评审摘要（2026-05-28）

**共识**：v5 方向正确——语义分层、双锚点、速度—消耗闭环是一步到位解，非打补丁。

**需重点把关的体验点**（已写入上文）：

1. 1.4 m/s 必须用档位差 + 强行军/硬撑打破单调，不能全玩家一刀切。
2. 180 s 冷却必须分层，否则交战外跑图无人敢冲。
3. 强制降速必须有反馈，否则像 bug。
4. UI 用冲刺 CD 环，不用 RPG 双条。
5. 联机 + AI 必须跟无氧池，否则公平性与沉浸一并崩。

**最大风险**：不是技术，是 **参数调校 + 玩家接受度** → Phase 0 孪生 + **实机** 双门禁；社区沟通可直接引用 **第一节问题陈述**。

---

## 十一、相关文件索引

| 类型 | 路径 |
|------|------|
| 主循环 | `scripts/Game/Integration/PlayerBase.c` |
| 消耗协调 | `scripts/Game/RSS/Core/SCR_StaminaUpdateCoordinator.c` |
| 限速 | `scripts/Game/RSS/Core/SCR_SpeedCalculation.c` |
| 常量 | `scripts/Game/RSS/Core/SCR_StaminaConstants.c` |
| 预设 | `scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c` |
| 孪生 | `tools/rss_digital_twin_fix.py` |
| 优化器 | `tools/rss_pipeline_v4.py` → v5 |
| 现有 bench | `tools/calc_35kg_ideal_movement.py` |
| 灌木桥接（3.23.1+） | `scripts/Game/RSS/Core/SCR_RSS_CharacterSpeedBridge.c` |
| 灌木机制文档 | `docs/灌木丛移动减速机制.md` |

---

## 十二、二次审查 — 缺陷与风险补充（v0.3）

### 12.1 计划逻辑缺口

**双锚点可能互相打架**

| 目标 | 倾向 |
|------|------|
| 锚点 A：1.4 m/s 走 4 h | 极低净消耗、高 `sustainable_watts` 容忍 |
| 锚点 B：5–15 s 冲刺 + 120–180 s 冷却 | 高无氧消耗、短 burst |
| 锚点 C：8 场景战斗剖面 | 仍可能要求「能跑能打一阵」 |

Phase 3 Optuna 同时硬约束时 **可行域可能为空**。**冲突优先级（写死）**：

```
生理硬约束 > 灌木叠加 bench > 8 场景 soft > 三档 ordering
```

**LegacyV4Style 定义须明确**

不能仅是「第四套 JSON」。Legacy = **v5 引擎 + 三开关**：

| 开关 | 作用 |
|------|------|
| 数值包 | 接近 v3.23 / v4 Tactical 的 coeff |
| 无氧池 | 关 → 退回单池 sprint_enable 语义 |
| 强制降速 | 关 → 不做 sustainable_watts 压速 |

避免「Legacy 预设名」造成第四套主循环长期维护。

**「有氧池 = 现有 stamina」未拆引擎层**

当前权威值：`SCR_CharacterStaminaComponent.m_fTargetStamina`（单 float）；HUD / 相机 / 载具读 `GetTargetStamina()`。

Phase 2 前须定稿：

| 项 | 决策项 |
|----|--------|
| 无氧池存储 | 新组件 / `ExerciseTracker` / RplProp |
| 引擎 `GetStamina()` | 仅反映有氧，或文档声明映射关系 |
| FOV / walk recovery / sprint 门槛 | 绑有氧还是无氧（表格化） |

否则易出现「逻辑双池、存储单池」。

**强制降速 vs 灌木限速（3.23.1 已合并）**

`SCR_RSS_CharacterSpeedBridge`：`final = min(RSS token, 灌木, 铁丝网)`。

v5 高代谢 **只下调 RSS_StaminaSpeedLimitToken**，不直接改全局 theoretical 以免与灌木 **双重惩罚**。验收增加：**灌木内 + 35 kg 持续移动**。

---

### 12.2 技术实现风险（计划原表未覆盖或偏轻）

**网络同步 — 决议了「必须同步」，缺实现方案**

| 缺口 | 后果 |
|------|------|
| RplProp / RPC / 复用体力同步？ | 实现期扯皮 |
| 客户端预测 Sprint 被服务器驳回 | 输入黏滞 |
| AI 无氧 Phase 4 才做 | Phase 2 上线后「人 AI 不一致」窗口 |

Phase 2 验收须二选一写死：**仅服务器许可 Sprint**，或 **客户端预测 + 服务器校正**。

**GetVelocity 与速度崩塌顺序**

`min(GetVelocity, theoretical)` 修消耗后，动画仍可能短时间高于 theoretical。

**崩塌顺序（写进 Phase 1/2）**：

```
1) 降 SetSpeedLimit（RSS token）
2) 再读 GetVelocity 算 v_drain
3) 再扣有氧 / 无氧池
```

**游泳 / 载具 / 战斗兴奋剂**

v5 锚点均为陆地 35 kg 站姿。v5.0 须一句话定案：

| 系统 | 建议默认 |
|------|----------|
| 游泳 | 共用有氧池；无氧 Sprint 禁用；保留现有游泳模型 |
| 载具 | 体力冻结或简化（与现 `VehicleHelper` 一致） |
| 战斗兴奋剂 | 只改有氧消耗倍率，不改无氧 CD |

**PARAMS_ARRAY_SIZE 与 schema**

每增 v5 参数须动 `WriteParamsToArray` / `MigrateConfig` / 联机 Custom。建议 Phase 0 引入 **`m_sStaminaSchemaVersion`**（与 `m_sConfigVersion` 分离），避免 49/50 槽静默错位重演。

**数字孪生门禁**

- 孪生 17 ms tick vs 游戏 0.2 s 刻度：对照时注明 scale。
- Phase 1 完成前 **禁止** 跑 v5 优化器。
- 增加 **C ↔ Python v_drain 同输入 dump** 作为 CI 门禁。

**`sustainable_watts = 400` 恒定**

未随负重 / 热 / 坡 / 泥泞 / 灌木调整时，与环境系统 **两套叙事**。Phase 1 可仅 **平路理想环境**；`sustainable_watts(load, heat)` 延后 Phase 4+。

---

### 12.3 体验与产品风险（收紧）

| 风险 | 仍缺 |
|------|------|
| 1.4 m/s 枯燥 | Everon 尺度定量（km/h 任务时间）；无「自动节奏」仍靠任务设计 |
| 180 s 冷却 | 接敌瞬间不能冲的挫败；8 s FOV burst 并入无氧后 FOV 可能空放 |
| 硬撑 | 与 walk recovery zone 叠加规则未写；新手易当 bug |
| 三档 | 玩家只选 Tactical → Elite 成摆设；Workshop 须写服务器推荐档 |
| v4→v5 弹窗 | 服务器 JSON 旧 coeff 与 v5 默认混用，仅弹窗不够 → 须 schema 迁移 |

---

### 12.4 阶段顺序与发布策略

**负面叠加风险**

```
Phase 1 降 Walk 1.4 m/s  →  社区「太慢」
Phase 2 无氧 180 s         →  在已变慢基础上再「不能冲」
Phase 3 优化器             →  固化未调妥手感
```

**建议分步发布（与 3.23.x 稳定线分离）**

| 版本线 | 内容 | 分支 |
|--------|------|------|
| **3.23.x** | 灌木桥接、HUD、稳定预设 | `main` |
| **3.23.2 / 3.24**（可选） | 仅 v_drain 闭环 + 灌木 bench；**不改** 1.4 m/s、无双池 | `dev/v5-phase1` |
| **v5.0-beta** | 降速 + coeff 物理化 + 单池 | |
| **v5.0** | 双池 + UI + 联机同步 | |

避免一次上线改动面过大、难以回滚。

---

### 12.5 与 3.23.1 稳定版关系

| 3.23.1 已有 | v5 须遵守 |
|-------------|-----------|
| `SCR_RSS_CharacterSpeedBridge` | 所有 v5 限速经桥接，禁止回退裸 `OverrideMaxSpeed` |
| 预设 b2e1cd9 | v5 全换参；Legacy ≠ b2e1cd9 除非 Legacy 关闭 v5 特性 |
| HUD ETA / ConfigBridge | 双池后 ETA 绑 **有氧**；无氧 CD 单独 UI |

**不要在 3.23.1 稳定 `main` 上直接合入 Phase 1 降速 + 双池**；v5 在 `dev/v5` 迭代，稳定线只收灌木/HUD/热修类改动。

---

### 12.6 v0.3 验收补充清单

Phase 0 / 1 除原计划外，增加：

- [ ] 灌木内 35 kg 行军 10 min（speed + drain 合理）
- [ ] 下坡 / 强行军 + 强制降速 UI 反馈可读
- [ ] 联机双人：Sprint CD / 无氧状态一致（Phase 2）
- [ ] C ↔ Python 同帧 v_drain 误差 < 5%
- [ ] 游泳 / 载具 / 兴奋剂 smoke（不回归崩溃）
- [ ] `m_sStaminaSchemaVersion` 迁移路径文档化

---

### 12.7 总评（二次审查）

| 维度 | 评价 |
|------|------|
| **方向** | 正确；双池 + v_drain + 分行军/爆发是正路 |
| **完整度** | v0.2 约 70% → v0.3 补存储/灌木/发布/Legacy/游泳网络细节 |
| **最大未解风险** | 一次改太多 + 孪生/实机/联机不一致 + 与 3.23.1 稳定线冲突 |
| **最大技术坑** | 单 `m_fTargetStamina` 硬叠双池；无氧同步方案空白 |

**推荐下一步**：在 `dev/v5` 做 Phase 0 + Phase 1 **仅 v_drain + 灌木 bench**；`main` 维持 3.23.x。

---

*本计划为架构草案 v0.3；Phase 0 孪生 + 实机完成后，第五节参数表将用实测数值替换「草案」列。*
