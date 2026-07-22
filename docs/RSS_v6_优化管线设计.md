# RSS v6 优化管线设计

> 替代 v4「三目标 + 八场景、无生理硬约束」与 v5 stub「仅跑 bench」的分裂状态。  
> 实现入口：`tools/rss_pipeline_v6.py`；硬约束模块：`tools/rss_constraints_v6.py`。

---

## 1. 现状问题

| 问题 | 影响 |
|------|------|
| v4 NSGA-II 只优化 v4 参数，不含 CP–W′ / 疲劳积分 | 实机 ETA、掉条与优化目标脱节 |
| 八场景是 **软目标**，生理锚点未进优化器 | 可解出「战斗拟真但行军/冲刺离谱」的参数 |
| `parameter_realism` 混入可玩性魔法惩罚 | 目标语义不清，难以解释 Pareto 解 |
| 数字孪生缺 `ProcessFatigueIntegral` + cap clamp | sustain 目标与 ~1%/s 上限收缩不一致 |
| v5 `rss_pipeline_v5.py` 仅校验，无搜索 | 无法联合重标定 v6 |

---

## 2. 设计原则

1. **约束优先（Constraint-first）**  
   生理锚点、契约测试 **硬失败 → prune trial**，不进入 Pareto 前沿。

2. **分层场景（Tiered scenarios）**  
   - **T0 硬约束**：drain 限速、代谢超速、冲刺 ≤15 s、冷却 ≥120 s、v6 W′ _burst  
   - **T1 软约束（待孪生补齐后升格）**：35 kg / 4 h 行军 `aerobic_end ≥ 0.20`  
   - **T2 软目标**：v4 八场景 + **35 kg 稳态 Run 20 min**（sustain）  
   - **T3 软目标**：参数漂移（v4 Hardcore 参考 + v6 schema 默认）

3. **参数分域搜索**  
   - **A 域（v4）**：`energy_to_stamina_coeff`、恢复倍率、负重系数等（窄带围绕 Hardcore）  
   - **B 域（v6）**：`critical_power_watts`、`w_prime_*`、`sprint_power_cap_watts`  
   - **C 域（固定/分档）**：行军档 m/s、三档 preset 的 tier multiplier（优化后人工或二次搜索）

4. **三档仍从 Pareto 提取**  
   Elite → 低 sustain + 低 combat；Tactical → 高 sustain；Standard → 几何中点。

5. **实机门禁不可省**  
   自动化通过后仍需 Workbench：1 km 行军、短冲/抽干、HUD ETA 与 `[RSS][Drain]` 观测一致。

---

## 3. 硬约束清单（T0）

| ID | 条件 | 说明 |
|----|------|------|
| `drain_velocity_clamp` | `get_drain_velocity_ms(5.5, 4.0) == 4.0` | 与 C 端代谢限速一致 |
| `metabolic_overspeed_factor` | 800 W @ 400 W sustainable → 0.5 | 超速代谢折减 |
| `v5_sprint_burst_duration` | 满池抽干 ≤ 15 s | 无氧池（legacy） |
| `v5_sprint_cooldown` | 抽干后冷却 ≥ 120 s | |
| `v6_cp_sprint_burst_35kg` | W′ 冲刺 ≤ 15 s @ 35 kg | CP–W′ 模型 |
| `march_4h_aerobic_end_35kg` | 4 h 行军结束有氧 ≥ 0.20 | **当前 soft**（孪生无 fatigue cap） |

实现：`rss_constraints_v6.evaluate_hard_constraints()`。  
`MARCH_4H_HARD = True` 在孪生补齐 fatigue 后开启。

---

## 4. 软目标（四目标 minimize）

| 目标 | 含义 | 主要数据来源 |
|------|------|----------------|
| **sustain_ease** | 长时 Run 后体力余量（越低越吃紧） | Mission `35kg稳态跑步` 20 min |
| **combat_ease** | 战斗阶段加权余量 | v4 八场景 |
| **recovery_ease** | 静止回血速率 | v4 八场景 idle 聚合 |
| **param_drift** | 偏离 Hardcore + v6 默认标量 | v4 `parameter_realism` + L2(v6) |

**sustain 与实机对齐（待办）**  
孪生需按 `PlayerBase_UpdateLoop` 顺序实现：

```
ProcessFatigueIntegral → UpdateStaminaValue(cap clamp) → SetTargetStamina
```

并在 sustain mission 中记录 **观测掉条 %/s**（目标 band：Elite Run @ cap 约 0.8–1.2 %/s）。

---

## 5. 命令行

```bash
# CI / 日常门禁（无需 Optuna）
python tools/rss_pipeline_v6.py validate

# 快速粗扫（粗 dt）
python tools/rss_pipeline_v6.py validate --fast

# 多目标搜索（需 optuna）
python tools/rss_pipeline_v6.py optimize --trials 400 --jobs 4 --output tools/

# 冒烟（孪生 + CP）
python tools/test_v6_smoke.py
```

`rss_pipeline_v5.py` 保留为薄包装，内部调用 `rss_pipeline_v6 validate`。

---

## 6. 实施路线图

### Phase A — 已完成（本提交）

- [x] `rss_constraints_v6.py` 统一硬约束  
- [x] `rss_pipeline_v6.py`：`validate` + `optimize` 骨架  
- [x] 九场景（八 + sustain Run）  
- [x] 四目标 NSGA-II + prune  

### Phase B — 孪生对齐

- [x] `TwinFatigueSystem` + cap clamp
- [x] `GetMetabolicSpeedCapMs` / CP invert 限速（`game_player_tick`）
- [x] v6 `GetDrainVelocityMs` 消耗路径（不再用引擎 3.8 假速度）
- [x] sustain 硬约束 band + 软目标 1.0±0.25 %/s
- [x] `MARCH_4H_HARD`（Elite `energy_to_stamina_coeff` ≈ 1.55e-7 标定）  
- [x] Run sustain 观测 band 0.6–1.4 %/s（硬约束）

### Phase C — 标定与分档

- [x] Elite `energy_to_stamina_coeff` 联合标定：4h Walk `aerobic_end ≥ 0.20` + Run obs band（`rss_pipeline_v6.py calibrate`）
- [ ] 其余 preset 按比例或独立标定；`optimize` NSGA-II 微调 combat/recovery  
- [ ] 三档 v6 JSON → `embed_json_to_c.py` + `SCR_RSS_Params`  
- [ ] `bench_35kg_presets.py` 改用 v6 metrics 报告  

### Phase D — CI 与实机

- [ ] pre-commit / CI：`validate` + `test_v6_smoke` + `check_script_size`  
- [ ] Workbench 签核清单写入 `BIGBANG_READINESS.md`  

---

## 7. 与 v4 管线关系

| 项目 | v4 | v6 |
|------|----|----|
| 入口 | `rss_pipeline_v4.py` | `rss_pipeline_v6.py` |
| 硬约束 | 无 | 生理锚点 prune |
| 软目标数 | 3 | 4（+ sustain） |
| v6 参数 | 不搜 | 搜 CP/W′ |
| 预设文件 | `*_v4.json` | `*_v6.json`（optimize 产出） |

v4 管线 **保留** 作回归对照，新预设以 v6 为准。

---

## 8. 参考

- `docs/RSS_v5_体力系统修改计划.md` Phase 3 约束表  
- `tools/schemas/rss_params_v5.schema.json`  
- `docs/RSS_v6_计算逻辑权威版.md`  
- 实机诊断：`[RSS][Drain]` 的 `capRate` / `有效` / `观测`
