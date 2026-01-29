# 考虑不同计算模型的优化器修改总结

## 修改日期
2026-01-30

---

## 关键发现

### Walk、Run、Sprint 使用不同的计算模型

#### 1. Walk（行走）
- **速度范围**：<= 2.2 m/s
- **计算模型**：Pandolf 模型
- **特点**：低速时消耗较高

#### 2. Run（跑步）
- **速度范围**：> 2.2 m/s
- **计算模型**：Givoni Running 模型
- **特点**：速度对消耗影响很大（2.2 次幂）

#### 3. Sprint（冲刺）
- **速度范围**：> 2.2 m/s（或 movement_type == SPRINT）
- **计算模型**：Givoni Running 模型 + Sprint 倍数
- **特点**：速度差异导致自然消耗是 Run 的 1.86x

---

## 实际消耗率计算（空载，ENERGY_TO_STAMINA_COEFF = 0.000015）

| 移动类型 | 速度 (m/s) | 模型 | 每秒消耗 |
|---------|-----------|------|---------|
| **Walk** | 1.8 | Pandolf | 0.044% |
| **Run** | 3.7 | Givoni | 0.073% |
| **Sprint** | 5.0 | Givoni × 2.5 | 0.370% |

**关键发现**：
- Run 消耗只是 Walk 的 1.63x（不是预期的 4x）
- Sprint 消耗是 Run 的 5.1x（远高于预期的 2.5x）

---

## 修改内容

### 修改 1：降低 ENERGY_TO_STAMINA_COEFF 搜索空间

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
energy_to_stamina_coeff = trial.suggest_float(
    'energy_to_stamina_coeff', 3.0e-5, 7e-5, log=True
)
```

**修改后**：
```python
energy_to_stamina_coeff = trial.suggest_float(
    'energy_to_stamina_coeff', 1.5e-5, 3.0e-5, log=True
)
```

**理由**：
- 降低搜索空间，让 Walk 消耗更低
- 允许 Walk 有净恢复
- Pandolf 模型在低速时的消耗比预期高

---

### 修改 2：降低 SPRINT_STAMINA_DRAIN_MULTIPLIER 搜索空间

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
sprint_stamina_drain_multiplier = trial.suggest_float(
    'sprint_stamina_drain_multiplier', 2.3, 2.7
)
```

**修改后**：
```python
sprint_stamina_drain_multiplier = trial.suggest_float(
    'sprint_stamina_drain_multiplier', 1.2, 1.5
)
```

**理由**：
- 速度差异（5.0 m/s vs 3.7 m/s）导致 Sprint 自然消耗是 Run 的 1.86x
- 只需要额外的 1.34x 倍数就能达到 2.5x 的目标
- 避免 Sprint 消耗过高

---

### 修改 3：降低行走恢复目标

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
min_walk_gain = 0.02  # 最小恢复 2%
max_walk_gain = 0.05  # 最大恢复 5%
```

**修改后**：
```python
min_walk_gain = 0.005  # 最小恢复 0.5%
max_walk_gain = 0.02  # 最大恢复 2%
```

**理由**：
- Pandolf 模型在低速时的消耗比预期高
- 降低恢复目标，避免约束冲突
- 允许 Walk 有净恢复

---

### 修改 4：基础体能检查（之前已完成）

**文件**：`tools/rss_super_pipeline.py`

**修改后**：
```python
# 通过条件：时间不超过120%且体力不低于20%且平均体力 > 25%
return time_ratio <= 1.2 and min_stamina >= 0.20 and avg_stamina > 0.25
```

---

### 修改 5：跑步和冲刺消耗要求（之前已完成）

**文件**：`tools/rss_super_pipeline.py`

**修改后**：
```python
# 1) Run：60秒 3.7m/s 空载，期望体力下降至少 5%
# 对应：927秒消耗80%（100% → 20%），每秒消耗0.0863%
required_run_drop = 0.05

# 2) Sprint：30秒 5.0m/s 空载，期望体力下降至少 6.5%
# 对应：跑步消耗的2.5x
required_sprint_drop = 0.065
```

---

## 预期效果

### 行走恢复
- 120秒恢复0.5-2%（每秒恢复0.0042-0.017%）

### 跑步消耗
- 927秒消耗80%（每秒消耗0.0863%）
- 净消耗：0.0693-0.0821% / s ✅

### 冲刺消耗
- 每秒消耗：0.2158%（约 Run 的 2.5x）
- 净消耗：0.1988-0.2116% / s ✅

---

## 优化器约束条件总结

| 约束 | 修改前 | 修改后 | 变化 |
|-----|--------|--------|------|
| ENERGY_TO_STAMINA_COEFF | 3.0e-5 - 7e-5 | 1.5e-5 - 3.0e-5 | 降低50% |
| SPRINT_STAMINA_DRAIN_MULTIPLIER | 2.3 - 2.7 | 1.2 - 1.5 | 降低50% |
| Walk 恢复率（120秒） | 2-5% | 0.5-2% | 降低75% |
| ACFT 测试最低体力 | ≥ 10% | ≥ 20% | 提高10% |
| ACFT 测试平均体力 | > 15% | > 25% | 提高10% |
| Run 消耗率（60秒） | 2.5% | 5% | 提高100% |
| Sprint 消耗率（30秒） | 3.5% | 6.5% | 提高86% |

---

## 下一步

### 1. 重新运行优化器
```bash
python rss_super_pipeline.py --n_trials 500 --n_jobs 4
```

### 2. 应用新的参数
```bash
python embed_json_to_c.py
```

### 3. 重新编译和测试
- 编译修改后的C代码
- 在游戏内测试消耗率和恢复率

### 4. 验证目标
- ✅ 跑步927秒，体力从100%降到20%
- ✅ 冲刺消耗是跑步的2.5x
- ✅ 行走有净恢复

---

## 总结

**关键发现**：
1. Walk 使用 Pandolf 模型，低速时消耗较高
2. Run 和 Sprint 使用 Givoni 模型，速度对消耗影响很大（2.2 次幂）
3. Sprint 自然消耗是 Run 的 1.86x（速度差异）

**修改内容**：
1. ✅ 降低 ENERGY_TO_STAMINA_COEFF 搜索空间（1.5e-5 到 3.0e-5）
2. ✅ 降低 SPRINT_STAMINA_DRAIN_MULTIPLIER 搜索空间（1.2 到 1.5）
3. ✅ 降低行走恢复目标（0.5-2% / 120s）
4. ✅ 基础体能检查：最低体力 ≥ 20%，平均体力 > 25%
5. ✅ 跑步消耗要求：60秒消耗5%
6. ✅ 冲刺消耗要求：30秒消耗6.5%

**预期效果**：
- ✅ 跑步927秒，体力从100%降到20%
- ✅ 冲刺消耗是跑步的2.5x
- ✅ 行走有净恢复
- ✅ 考虑了不同计算模型的影响

**下一步**：重新运行优化器，生成新的JSON配置！