# 优化器与数字孪生对齐说明

## 结论先行

**需要改。** 优化器依赖的数字孪生（`rss_digital_twin_fix.py`）其**消耗模型**与当前 C 端 `SCR_StaminaConsumption` **不一致**。若希望优化结果对应真实游戏表现，建议：

1. **逻辑**：让数字孪生的消耗计算与 C 的 Consumption 对齐（Pandolf / Givoni / 静态 + 全局 ×0.2 + 效率/姿态/速度/负重/Sprint 等修正）。
2. **数值**：对齐逻辑后，**重新跑一轮优化**，让搜索到的参数针对「实际 C 端消耗」调优。

---

## 1. 当前差异

### 1.1 C 端消耗（`SCR_StaminaConsumption`）

- **静态**（v < 0.1）：`CalculateStaticStandingCost` → ×0.2 → 每 0.2s。
- **跑步**（v > 2.2）：`CalculateGivoniGoldmanRunning` × 地形 × (1+风阻) → ×0.2 → 每 0.2s。
- **步行**（0.1 ≤ v ≤ 2.2）：`CalculatePandolfEnergyExpenditure` × (1+风阻) → ×0.2 → 每 0.2s。
- **统一**：所有分支后再 **×0.2**（全局转换）。
- **后续**：再乘姿态、效率因子、疲劳因子，加速度项、负重项，再乘 Sprint 倍数等。

即：**Pandolf / Givoni / 静态** + **全局 ×0.2** + 各种修正。

### 1.2 数字孪生消耗（`_calculate_base_drain_rate`）

- 使用**速度阈值模型**：
  - Sprint：`SPRINT_DRAIN_PER_TICK * load_factor`
  - Run：`RUN_DRAIN_PER_TICK * load_factor`（或硬编码 `0.00008`）
  - Walk：`0.00002 * load_factor`
  - Rest：`-0.00025`
- **没有** Pandolf、Givoni、静态站立模型；
- **没有** 全局 ×0.2；
- **没有** 效率因子、姿态、速度相关项、负重项、Sprint 倍数等。

因此，孪生模拟的「消耗曲线」与 C 端实际消耗**不是同一套公式**。

### 1.3 优化器在做什么

- `rss_super_pipeline` 调 `RSSDigitalTwin` 做场景模拟（如 2 英里跑、30kg 负重等）。
- 优化的是 `energy_to_stamina_coeff`、`base_recovery_rate`、恢复倍数、负重系数、Sprint 倍数等。
- 这些参数会写入 C（如 `GetEnergyToStaminaCoeff`、恢复相关等），**但**  
  优化时的**消耗**来自数字孪生的阈值模型，**不是** C 的 Pandolf/Givoni/静态模型。

所以：**优化目标基于「错误」的消耗逻辑**，得到的参数未必适合当前 C 端实现。

---

## 2. 建议修改

### 2.1 逻辑层（优先）

在 `rss_digital_twin_fix.py` 中：

- 重写或替换 `_calculate_base_drain_rate`，使其与 `SCR_StaminaConsumption` 一致：
  - 按 v 分段：静态（<0.1）、跑步（>2.2）、步行（0.1–2.2）；
  - 分别用等价的 Pandolf / Givoni / 静态公式，并做 **×0.2** 及**全局 ×0.2**；
  - 再应用效率因子、姿态、速度项、负重项、Sprint 倍数等（至少与 C 相同的主要项）。
- 保证输出单位与 C 一致（每 0.2s 的消耗率），且与 `_calculate_recovery_rate` / `step` 的用法一致。

`tools/compare_c_python_logic.py` 里已有对 C 消耗的 Python 复刻，可作参考；但要补齐「全局 ×0.2」以及和孪生 `step` 的衔接。

### 2.2 数值层（对齐后）

- 逻辑对齐后，**重新跑** `rss_super_pipeline`（或你当前用的优化流程）。
- 这样搜出来的 `energy_to_stamina_coeff`、恢复相关、负重、Sprint 等，才是针对**真实 C 端消耗**调优的。
- 若 C 端常数有更新（如 `GIVONI_*`、`PANDOLF_*`、`ENERGY_TO_STAMINA_COEFF` 等），确保数字孪生和 `stamina_constants` 使用同一套值；优化器若覆盖其中部分，也要同步到 C。

---

## 3. 若暂时不改

- 当前优化器结果仍可当作「一组可行配置」使用，尤其是**恢复相关**参数，因为恢复逻辑孪生与 C 相对接近。
- 但**消耗相关**的优化（如 2 英里耗时、30kg 可玩性等）会基于**错误**的消耗曲线，与实机不一致。
- 若你以 `example` 为准且不跑优化，可以不改；若打算继续用优化器调参，建议按上面步骤做逻辑对齐并重跑优化。

---

## 4. 小结

| 项目           | 是否建议改 | 说明 |
|----------------|------------|------|
| 孪生消耗逻辑   | **是**     | 与 C 的 Pandolf/Givoni/静态 + 全局 ×0.2 + 修正 对齐 |
| 优化器流程     | **逻辑改后再跑** | 用对齐后的孪生重新优化，才能对应 C 端 |
| 搜索范围/目标  | 视情况     | 若 C 端常数或设计有变，再调整范围与约束 |

**总结**：优化器的逻辑与数值都需要改——先对齐数字孪生消耗与 C，再基于新逻辑重新跑优化并更新输出的参数/预设。

---

## 5. 已完成的修复（v1）

- **数字孪生** `rss_digital_twin_fix.py`：
  - 消耗模型已与 C 端 `SCR_StaminaConsumption` 对齐：Pandolf / Givoni / 静态 + 全局 ×0.2 + 效率 / 姿态 / 疲劳 / 速度项 / 负重 / Sprint。
  - 新增 `_calculate_drain_rate_c_aligned`，返回 `(base_for_recovery, total_drain)`；`step` 中代谢净值 `net_change = recovery_rate - total_drain`，与 C 端一致。
  - 保留 EPOC 处理；`_calculate_base_drain_rate` 改为委托上述逻辑。
- **常量**：补充 `PANDOLF_STATIC_*`、`GIVONI_*`、`AEROBIC/ANAEROBIC_THRESHOLD`、`FATIGUE_START_TIME_MINUTES`、`CONSUMPTION_POSTURE_*` 等，与 C 对齐。
- **建议**：逻辑对齐后，重新跑 `rss_super_pipeline` 做一轮优化，再更新预设/JSON。
