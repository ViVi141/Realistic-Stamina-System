# 优化器最终修改总结

## 修改日期
2026-01-30

---

## 设计标准

### 1. 跑步（Run）目标
- 条件：0kg 负重
- 距离：3.2 km
- 时间：15分27秒（927 秒）
- 要求：最低体力为20%

### 2. 冲刺（Sprint）目标
- 消耗为 Run 的 2.5x

### 3. 行走（Walk）目标
- 净恢复：120秒恢复0.5-2%（考虑 Pandolf 模型的消耗）

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
- **速度范围**：> 2.2 m/s
- **计算模型**：Givoni Running 模型 + Sprint 倍数
- **特点**：速度差异导致自然消耗是 Run 的 1.86x

---

## 实际消耗率（空载，ENERGY_TO_STAMINA_COEFF = 0.000015）

| 移动类型 | 速度 (m/s) | 模型 | 每秒消耗 |
|---------|-----------|------|---------|
| **Walk** | 1.8 | Pandolf | 0.044% |
| **Run** | 3.7 | Givoni | 0.073% |
| **Sprint** | 5.0 | Givoni × 2.5 | 0.370% |

**关键发现**：
- Run 消耗只是 Walk 的 1.63x（不是预期的 4x）
- Sprint 消耗是 Run 的 5.1x（远高于预期的 2.5x）

---

## 修改内容总结

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

### 修改 4：基础体能检查

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
# 通过条件：时间不超过120%且体力不低于10%且平均体力 > 15%
return time_ratio <= 1.2 and min_stamina >= 0.10 and avg_stamina > 0.15
```

**修改后**：
```python
# 通过条件：时间不超过120%且体力不低于20%且平均体力 > 25%
return time_ratio <= 1.2 and min_stamina >= 0.20 and avg_stamina > 0.25
```

**理由**：
- 最低体力从10%提高到20%
- 平均体力从15%提高到25%
- 确保体力不会长期处于20%

---

### 修改 5：跑步消耗要求

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
# 1) Run：60秒 3.7m/s 空载，期望体力下降至少 3%
required_run_drop = 0.025
```

**修改后**：
```python
# 1) Run：60秒 3.7m/s 空载，期望体力下降至少 5%
# 对应：927秒消耗80%（100% → 20%），每秒消耗0.0863%
required_run_drop = 0.05
```

**理由**：
- 对应927秒消耗80%（从100%到20%）
- 60秒消耗应该约为5%

---

### 修改 6：冲刺消耗要求

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
# 2) Sprint：30秒 5.0m/s 空载，期望体力下降至少 4%
required_sprint_drop = 0.035
```

**修改后**：
```python
# 2) Sprint：30秒 5.0m/s 空载，期望体力下降至少 6.5%
# 对应：跑步消耗的2.5x
required_sprint_drop = 0.065
```

**理由**：
- 冲刺消耗应该是跑步消耗的2.5x
- 30秒冲刺应该消耗约6.5%

---

## 优化器约束条件对比

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

## 预期效果

### 行走恢复
- 120秒恢复0.5-2%（每秒恢复0.0042-0.017%）

### 跑步消耗
- 927秒消耗80%（每秒消耗0.0863%）
- 净消耗：0.0693-0.0821% / s ✅
- 从100%到20%需要：927秒 ✅

### 冲刺消耗
- 每秒消耗：0.2158%（约 Run 的 2.5x）
- 净消耗：0.1988-0.2116% / s ✅
- 从100%到20%需要：371秒（6.2分钟）✅

---

## 下一步操作

### 1. 重新运行优化器

在 `tools/` 目录下运行：
```bash
python rss_super_pipeline.py --n_trials 500 --n_jobs 4
```

**说明**：
- `--n_trials 500`：运行500次试验
- `--n_jobs 4`：使用4个CPU核心并行计算
- 预计运行时间：30-60分钟（取决于CPU性能）

**输出文件**：
- `optimized_rss_config_realism_super.json`（真实主义配置）
- `optimized_rss_config_playability_super.json`（可玩性配置）
- `optimized_rss_config_balanced_super.json`（平衡配置）

---

### 2. 应用新的参数

在 `tools/` 目录下运行：
```bash
python embed_json_to_c.py
```

**说明**：
- 将优化后的JSON参数嵌入到C代码中
- 更新 `scripts/Game/Components/Stamina/SCR_RSS_Settings.c`
- 自动创建备份文件

---

### 3. 重新编译

在 Arma Reforger Workbench 中：
1. 打开项目
2. 点击 "Build" -> "Build Solution"
3. 等待编译完成

---

### 4. 测试

#### 测试 1：跑步测试
- 条件：0kg 负重，平地
- 操作：连续跑步15分27秒（927秒）
- 预期：体力从100%降到20%

#### 测试 2：冲刺测试
- 条件：0kg 负重，平地
- 操作：连续冲刺
- 预期：每秒消耗约0.2158%（约跑步的2.5x）

#### 测试 3：行走测试
- 条件：0kg 负重，平地
- 操作：连续行走2分钟
- 预期：体力恢复0.5-2%

---

## 如果测试结果不符合预期

### 问题 A：跑步消耗过低

**解决方案**：
1. 检查 `ENERGY_TO_STAMINA_COEFF` 的值
2. 如果值过低，提高搜索空间下界：
   ```python
   energy_to_stamina_coeff = trial.suggest_float(
       'energy_to_stamina_coeff', 2.0e-5, 3.5e-5, log=True
   )
   ```

---

### 问题 B：冲刺消耗过高

**解决方案**：
1. 检查 `SPRINT_STAMINA_DRAIN_MULTIPLIER` 的值
2. 如果值过高，进一步降低搜索空间上界：
   ```python
   sprint_stamina_drain_multiplier = trial.suggest_float(
       'sprint_stamina_drain_multiplier', 1.2, 1.4
   )
   ```

---

### 问题 C：行走无法恢复

**解决方案**：
1. 检查 `BASE_RECOVERY_RATE` 的值
2. 如果值过低，提高搜索空间上界：
   ```python
   base_recovery_rate = trial.suggest_float(
       'base_recovery_rate', 2.0e-4, 5.0e-4, log=True
   )
   ```
3. 或者进一步降低行走恢复目标：
   ```python
   min_walk_gain = 0.0  # 允许不恢复
   max_walk_gain = 0.01  # 最大恢复1%
   ```

---

## 总结

### 关键发现
1. Walk 使用 Pandolf 模型，低速时消耗较高
2. Run 和 Sprint 使用 Givoni 模型，速度对消耗影响很大（2.2 次幂）
3. Sprint 自然消耗是 Run 的 1.86x（速度差异）

### 修改内容
1. ✅ 降低 ENERGY_TO_STAMINA_COEFF 搜索空间（1.5e-5 到 3.0e-5）
2. ✅ 降低 SPRINT_STAMINA_DRAIN_MULTIPLIER 搜索空间（1.2 到 1.5）
3. ✅ 降低行走恢复目标（0.5-2% / 120s）
4. ✅ 基础体能检查：最低体力 ≥ 20%，平均体力 > 25%
5. ✅ 跑步消耗要求：60秒消耗5%
6. ✅ 冲刺消耗要求：30秒消耗6.5%

### 预期效果
- ✅ 跑步927秒，体力从100%降到20%
- ✅ 冲刺消耗是跑步的2.5x
- ✅ 行走有净恢复
- ✅ 考虑了不同计算模型的影响

### 下一步
1. 重新运行优化器
2. 应用新的参数
3. 重新编译和测试
4. 验证目标是否达成

---

## 文档清单

| 文件 | 说明 |
|-----|------|
| `FINAL_MODIFICATION_SUMMARY.md` | 最终修改总结（本文档） |
| `movement_models_analysis.md` | 不同计算模型影响分析 |
| `movement_models_modification_summary.md` | 修改总结 |

---

## 联系方式

如果遇到问题，请提供以下信息：
1. 优化器输出的JSON文件
2. 游戏内测试日志（`tools/log.md`）
3. 问题和预期行为描述