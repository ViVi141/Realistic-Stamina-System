# RSS v6.0.0 计算逻辑权威版

> **版本**: 6.0.0 | **日期**: 2026-06-04  
> 取代 v5 文档中「意志力平台期 / Givoni / 旧模块名」描述。历史 v3 正文见 [`体力系统计算逻辑文档.md`](体力系统计算逻辑文档.md)（已标记归档）。

---

## 1. 北极星闭环

每 **17 ms** tick：

```
v_meas → P(v) [MetabolismModel]
      → CP_eff / W′ [CriticalPowerModel]
      → v_max = invert(P_target) [DrainCalculator + SpeedCalculator]
      → SetSpeedLimit [SpeedBridge]
```

**测速消耗**：`v_drain = min(v_meas, v_limit_applied)`；**当 v_meas > v_limit + 0.12 m/s** 时，**W′** 按实测速度记账；**疲劳积分**仍用 `v_drain` 功率（超额先走 W′）。

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

- Walk / v < 2.0 m/s：Pandolf（Santee 陡下坡、fitness bonus）
- Run/Sprint：ACSM `P = a + b·v + c·v²`，在 2.0–2.4 m/s **C¹ 混合**

### 3.2 有氧消耗

```
drain_rate_per_s = P × energy_to_stamina_coeff
drain_per_tick = drain_rate_per_s × 0.2
```

**v6 已移除**玩家路径 `load_metabolic_dampening` 与 effort 补偿 fudge。

---

## 4. CP–W′

### 4.1 动态 CP

```
CP₀ = critical_power_watts（三档预设）
CP_load  = CP₀ × (1 − 0.002 × max(0, L_kg − 10))
CP_slope = CP_load × (1 − 0.015 × g²)   当 g>0（g=grade%×0.01）
CP_env   = CP_slope × envCpMult
CP_final = CP_env × (1 − 0.18 × Fatigue_norm) × fatigueCpMult
```

下坡 **不对 CP 加成**（坡度消耗由 Pandolf 承担，防双重计数）。

### 4.2 W′ 放电

```
dW′/dt = −max(0, P − CP_final)   [J/s]
```

### 4.3 W′ 再填充

| 档位 | 机制 |
|------|------|
| **Elite** (CP≤410 W) | Skiba 双指数：`k_fast=0.15`, `k_slow=0.008`, `W′_lim=0.5·W′_max` |
| **Standard/Tactical** | 线性 `w_prime_recovery_w_per_s`（不再用时间 CD 锁 Sprint） |

### 4.4 Sprint 速度

```
v_sprint = invert(P_available)
P_available = min(sprint_power_cap, CP + W′/Δt)
```

Elite 默认 `sprint_power_cap_watts = 1450`（35 kg 全 Sprint 至 ANA 门槛 ≤15 s）。

---

## 5. 有氧池与速度

- **无主条平台期**：Run 速度 = 相位目标 m/s（Elite 1.4/2.8/4.0）× 负重
- **低 STA**（<5%）：`GetDynamicLimpMultiplier` + `CollapseTransition` 5 s 阻尼
- **Sprint 门禁**：`IsSprintAllowedWithCp`（有氧阈值 + W′ 池阈值；**不再**因短冲上时间 CD）
  - 短冲松键后剩余 W′ 可立刻再冲；W′≤开门阈值时禁止，等 `P≤CP` 恢复后再开
  - `burst_cooldown_*` 预设字段保留兼容；负重爆发减免仍用 `TACTICAL_SPRINT_COOLDOWN`（15 s）

---

## 6. 疲劳与 EPOC

### 6.1 积分疲劳 I(t)

```
dI/dt = w·P − R
w = 1 + k_load·(L/W) + k_slope·G² + k_terrain·(η−1)
R = k_recovery × (1 − I/I_max)²   （静止/低 P）
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
| CP (W) | 400 | 420 | 480 |
| W′_max (J) | 20000 | 24000 | 28000 |
| sprint_cap (W) | 1450 | 1350 | 1500 |
| W′ 恢复 | Skiba | 线性 15 W/s | 线性 18 W/s |

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
