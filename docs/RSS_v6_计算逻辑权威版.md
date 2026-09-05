# RSS v6 计算逻辑权威版

> **中文** | [English](en/V6_CALCULATION_LOGIC.md)
>
> **版本**: 6.0.0 数学内核 | **对齐代码**: 6.1.x（2026-08-14 审计对齐）  
> 取代 v5 及更早文档中「意志力平台期 / Givoni / 旧模块名」描述。以本文件与源码为准。  
> ⚠️ 2026-08-14 数学审计：§1–§7 已按实际代码/烘焙值修正；历史漂移记录见 [archive/RSS_数学模型审计_2026-08-14.md](archive/RSS_数学模型审计_2026-08-14.md)。  
> **限速默认（v6.1.7 起）**：`V6_APPLY_CP_METABOLIC_SPEED_CAP = true`（W′ 耗尽后经 `SetSpeedLimit` 压 CP 巡航指令速度；≤6.1.5 为 drain-only：代谢超额只扣 STA/W′）。巡航反解坡度钳到 15%、η 不进速度伺服；Walk 反解地板 `V6_CP_HIKE_FLOOR_MS=1.0`（`GetMetabolicSpeedCapMs` 与 UpdateCoordinator 均走 `InvertCruiseCapMs`，禁止裸反解）。CP 巡航帽只写在**当前步态带内**（Run 约 0.5–1.0× 相位顶，且 ≥ `V6_RUN_GAIT_FLOOR_MS` 2.2 或软带 1.95–2.2）；反解掉出 Run 带时**不**把 Walk/爬行速度写到 Run 相位（滑步）。W′ 空且仍按住移动时改切引擎 Walk 档（`V6_CP_OUT_OF_BAND_WALK_OVERRIDE`：`SetDynamicSpeed(0.5)` + override，与 CapsLock 同款）。已知限制：滚轮 `SetDynamicSpeed` 可在步态带内绕过最大速度层（引擎限制）；物理钳保持关闭（防 Bang-Bang 振荡）。W′ 可经 transient 驱动引擎晃动/模糊（见 `SCR_RSS_SprintGate`）。

---

## 1. 北极星闭环

每 tick 双回路（速度伺服 **17 ms** / 玩家 CallLater；体力积分 **0.2 s**；AI 100 ms）：

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]
```

> **执行顺序说明**：`SetSpeedLimit`（invert）在 PhaseA 先执行，`P(v)`/`TickPower`（更新 CP–W′）在 PhaseB 后执行，故 invert 使用的是**上一 tick 的 CP/W′ 快照**。这是离散控制环的标准 1-tick 延迟（17 ms，对玩家不可感知），非缺陷。

**测速消耗**：代谢 `v_drain = v_meas`（已移除 min 到 v_limit）；EPOC 峰值采样与疲劳积分仍用 `min(v_meas, v_limit)`。**当 v_meas > v_limit + 0.12 m/s** 时，**W′** 按实测速度记账（超额先走 W′）。

---

## 2. 模块映射

| 职责 | 文件 |
|------|------|
| 代谢功率 P | `SCR_RSS_MetabolismModel.c` |
| CP–W′ | `SCR_RSS_CriticalPowerModel.c` / `SCR_RSS_AnaerobicBurst.c` |
| 消耗协调 | `SCR_RSS_UpdateCoordinator.c` |
| 速度 | `SCR_RSS_SpeedCalculator.c` / `SCR_RSS_DrainCalculator.c` |
| 有氧恢复 / EPOC | `SCR_RSS_RecoveryCalculator.c` / `SCR_RSS_EpocState.c` |
| 积分疲劳 | `SCR_RSS_FatigueSystem.c` |
| 主循环 | `PlayerBase_UpdateLoop.c` |
| 配置 | `SCR_RSS_Settings.c` / `SCR_RSS_Params.c` |

---

## 3. 代谢功率

### 3.1 Pandolf + ACSM 混合

- Walk / Idle：`v ≤ 1.97 m/s` 走 **LCDA**（背包式）；`>1.97` 走 Pandolf（Santee 陡下坡、fitness bonus）
- Run/Sprint：ACSM `P = a + b·v + c·v²`，在 2.0–2.4 m/s **线性交叉淡入**（C⁰，非 C¹）

### 3.2 有氧消耗

```
aerobic_drain_rate_per_s = min(P, CP) × energy_to_stamina_coeff × 0.72   // 0.72 = V6_STAMINA_DRAIN_CALIBRATION
drain_per_0p2s = drain_rate_per_s × 0.2
```

玩家路径的 `load_metabolic_dampening` 仍存在（`MetabolismPowerWatts` 尾步，由 `GetLoadMetabolicDampening()` 门控）；effort 补偿 fudge 已移除。

**手持重物加成**：`SCR_RSS_EncumbranceCache` 称重真正握在手里的东西——`GetHeldGadgetComponent()` 且 `GetMode()==IN_HAND` 的 gadget（过滤 `GetHeldGadget()` 回退的隐藏腕表/指南针）；无 IN_HAND gadget 时采当前武器（`GetCurrentWeapon` / 槽位 `GetWeaponEntity`）。重量用 `InventoryItemComponent.GetTotalWeight()`（含附件）。武器已在 `GetTotalWeightOfAllStorages()` 中计入 Pandolf 负载，此处只加消耗乘数 `1 + V6_HELD_ITEM_DRAIN_COEFF(2.0) × (heldKg / 90)`，上限 `V6_HELD_ITEM_DRAIN_MULT_MAX(1.5)`。陆地快/完整路径与游泳消耗侧均施加。

---

## 4. CP–W′

### 4.1 动态 CP

```
CP₀ = critical_power_watts（三档预设）
CP_load  = CP₀ × (1 − 0.002 × max(0, L_kg − 10))
CP_slope = CP_load × (1 − 0.015 × g²)   当 g>0（g=grade%×0.01）
CP_env   = CP_slope × envCpMult
CP_final = CP_env × (1 − 0.18 × Fatigue_norm)    // 下限 0.82；无独立 fatigueCpMult 因子
```

下坡 **不对 CP 加成**（坡度消耗由 Pandolf 承担，防双重计数）。

### 4.2 W′ 放电

```
dW′/dt = −max(0, P − CP_final)   [J/s]
```

### 4.3 W′ 再填充

| 档位 | 机制 |
|------|------|
| **Elite** (CP≤410 W) | Skiba 双指数：`k_fast=0.15`, `k_slow=0.010`, `W′_lim=0.5·W′_max` |
| **Standard/Tactical** | 线性 `w_prime_recovery_w_per_s`（不再用时间 CD 锁 Sprint） |

> ✅ **已修复（2026-08-14 审计 §2.1 / 第 4 项）**：`UsesSkibaRecovery()` 按档位显式分派（`SCR_RSS_ConfigBridge.GetWPrimeRecoveryMode()`，源自 `w_prime_recovery_mode` 字段）：Elite=Skiba，Standard/Tactical=线性。

### 4.4 Sprint 速度

```
v_sprint = invert(P_available)
P_available = min(sprint_power_cap, CP + W′/Δt)
```

Elite 烘焙 `sprint_power_cap_watts = 2355`（35 kg 全 Sprint 至 ANA 门槛 ≤15 s）。

---

## 5. 有氧池与速度

- **无主条平台期**：Run 速度 = 相位目标 m/s（Elite 1.4/2.8/4.0）× 负重
- **低 STA**（<5%）：`GetDynamicLimpMultiplier`（Walk 顶 1.45 m/s，地板 1.0）+ `CollapseTransition` 5 s 阻尼。条空仍按住 Run 时限速托 0.5× 相位顶（疲惫慢跑），避免把爬行倍率写进 Run 相位。
- **Sprint 门禁**：`IsSprintAllowedWithCp`（有氧阈值 + **W′ 超速施密特闩锁**；**不再**因短冲上时间 CD）
  - 关闭带：池 ≤ `anaerobic_sprint_enable_threshold + V6_WPRIME_OVERSPEED_HYSTERESIS`（默认 ≈25%）
  - 再开带：池 > `threshold + V6_WPRIME_OVERSPEED_REARM`（默认 ≈60%）；禁止在 ≈20–25% 按住冲刺时 Sprint↔Run 震荡
  - 未触及关闭带时松键：剩余 W′ 可立刻再冲；触及关闭带后须恢复到再开带
  - `burst_cooldown_*` 预设字段保留兼容；负重爆发减免仍用 `TACTICAL_SPRINT_COOLDOWN`（15 s）

---

## 6. 疲劳与 EPOC

### 6.1 积分疲劳 I(t)

```
dI/dt = w·max(P−CP, 0) − R    // 仅对超 CP 功率积分；下坡坡项归零
w = 1 + k_load·(L/W) + k_slope·G² + k_terrain·(η−1)
R = k_recovery × (1 − I/I_max)² × P   （静止/低 P 时）
```

输出：`GetCpFatigueMultiplier()`、`GetFatigueIntegralNorm()` → CP 与 W′ k_fast/k_slow。

### 6.2 EPOC

停跑后延迟期消耗 ∝ **峰值意图代谢功率**（`GetEpocSamplePowerWatts` → `EpocState.UpdateExercisePowerSample`）：

- 采样速度 = `min(v_meas, v_limit)`（硬钳关时不下坡跑飞功率进氧债）
- 存峰再钳到 `CP × (1 + EPOC_MAX_POWER_EXCESS_RATIO)`；无冲刺/无 W′ 超速记账时进一步钳到 **CP**
- 峰值 ≤ `CP × EPOC_AEROBIC_CP_RATIO` → 弱 EPOC（`× EPOC_AEROBIC_DRAIN_MULT`）
- 当前采样明显低于峰值时快衰减（`EPOC_PEAK_DECAY_FAST_*`）

---

## 7. 三档预设（35 kg 锚点）

| 参数 | Elite | Standard | Tactical |
|------|-------|----------|----------|
| CP (W) | 889.74 | 1010.81 | 1029.80 |
| W′_max (J) | 21322 | 31038 | 31655 |
| sprint_cap (W) | 2355 | 2724 | 2748 |
| W′ 恢复 | Skiba | 线性 11.86 W/s | 线性 14.28 W/s |

> 实值以 `SCR_RSS_SettingsPresetBake.c` 与 `tools/optimized_rss_config_*_v6.json` 为准（二者一致）。W′ 恢复的 Skiba/线性分派已按档位生效（见 §4.3）。

---

## 8. 验收命令

```bash
python tools/test_v5_smoke.py
python tools/test_v6_smoke.py
python tools/bench_physio_anchors.py
python tools/test_acft_2mile.py
python tools/check_script_size.py
```

---

## 9. 明确移除（v6）

- 意志力平台期（25%/35% 恒速 Run）
- 无氧固定 `0.12/s` 扣条
- AI 独立 `stamina^0.6` 曲线
- `CalculatePandolfDrain` 重复实现
- W′_ratio → speed_mult 分段表（与 invert 闭环冲突）

---

## 10. Phase 4+ 可选（默认关闭）

HPTF 热应激、Fitts 操作疲劳、SOPMOD 装备表、背包晃动、Yerkes-Dodson、SAFTE — 见 plan Phase 4+。
