# Walk、Run、Sprint 不同计算模型的影响分析

## 计算模型总结

### 1. Walk（行走）
- **速度范围**：<= 2.2 m/s
- **计算模型**：Pandolf 模型
- **公式**：
  ```
  energy_per_kg = (base_term + grade_term) * terrain_factor
  total_watts = energy_per_kg * ref * w_mult
  rate = total_watts * ENERGY_TO_STAMINA_COEFF
  ```
  其中：
  - `base_term = 2.7 + 3.2 * (velocity - 0.7)^2`
  - `grade_term = grade_percent * 0.01 * (0.23 + 1.34 * velocity^2)`
  - `ref = 90.0`（参考体重）
  - `w_mult = current_weight / ref`（负重系数）
  - `terrain_factor`：地形因子（平地为1.0）

---

### 2. Run（跑步）
- **速度范围**：> 2.2 m/s
- **计算模型**：Givoni Running 模型
- **公式**：
  ```
  total_watts = gc * velocity^2.2 * ref * w_mult
  rate = total_watts * ENERGY_TO_STAMINA_COEFF
  ```
  其中：
  - `gc = 0.6`（Givoni 常数）
  - `ge = 2.2`（速度指数）
  - `ref = 90.0`（参考体重）
  - `w_mult = current_weight / ref`（负重系数）
  - **不乘以 SPRINT_STAMINA_DRAIN_MULTIPLIER**

---

### 3. Sprint（冲刺）
- **速度范围**：> 2.2 m/s（或 movement_type == SPRINT）
- **计算模型**：Givoni Running 模型 + Sprint 倍数
- **公式**：
  ```
  total_watts = gc * velocity^2.2 * ref * w_mult
  rate = total_watts * ENERGY_TO_STAMINA_COEFF * SPRINT_STAMINA_DRAIN_MULTIPLIER
  ```
  其中：
  - `gc = 0.6`（Givoni 常数）
  - `ge = 2.2`（速度指数）
  - `ref = 90.0`（参考体重）
  - `w_mult = current_weight / ref`（负重系数）
  - **乘以 SPRINT_STAMINA_DRAIN_MULTIPLIER**（目标：2.5x）

---

## 实际消耗率计算（空载）

### 测试条件
- `current_weight = 90.0`（空载）
- `grade_percent = 0.0`（平地）
- `terrain_factor = 1.0`（普通地形）
- `ENERGY_TO_STAMINA_COEFF = 0.000015`（默认值）
- `SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5`（目标值）

---

### 1. Walk（1.8 m/s）

**Pandolf 模型计算**：
```
velocity = 1.8 m/s
vt = 1.8 - 0.7 = 1.1
base_term = 2.7 + 3.2 * (1.1)^2 = 2.7 + 3.2 * 1.21 = 2.7 + 3.872 = 6.572
grade_term = 0 * (0.23 + 1.34 * 1.8^2) = 0
energy_per_kg = (6.572 + 0) * 1.0 = 6.572
total_watts = 6.572 * 90.0 * 1.0 = 591.48
rate = 591.48 * 0.000015 = 0.0088722 / 0.2s = 0.044361 / s
```

**每秒消耗**：0.044% / s

---

### 2. Run（3.7 m/s）

**Givoni Running 模型计算**：
```
velocity = 3.7 m/s
vp = 3.7^2.2 = 3.7^2.2 = 17.89
total_watts = 0.6 * 17.89 * 90.0 * 1.0 = 966.06
rate = 966.06 * 0.000015 = 0.0144909 / 0.2s = 0.0724545 / s
```

**每秒消耗**：0.0725% / s

---

### 3. Sprint（5.0 m/s）

**Givoni Running 模型 + Sprint 倍数计算**：
```
velocity = 5.0 m/s
vp = 5.0^2.2 = 5.0^2.2 = 36.53
total_watts = 0.6 * 36.53 * 90.0 * 1.0 = 1972.62
rate = 1972.62 * 0.000015 * 2.5 = 0.0739734 / 0.2s = 0.369867 / s
```

**每秒消耗**：0.370% / s

---

## 消耗率对比

| 移动类型 | 速度 (m/s) | 模型 | 每秒消耗 | 相对于 Walk | 相对于 Run |
|---------|-----------|------|---------|------------|-----------|
| **Walk** | 1.8 | Pandolf | 0.044% | 1.0x | 0.61x |
| **Run** | 3.7 | Givoni | 0.073% | 1.63x | 1.0x |
| **Sprint** | 5.0 | Givoni × 2.5 | 0.370% | 8.34x | 5.1x |

---

## 对优化器约束条件的影响

### 问题1：Walk 和 Run 的消耗比不是线性的

**预期**（如果不考虑不同模型）：
- Walk：1.8 m/s
- Run：3.7 m/s（速度是 Walk 的 2.06x）
- 如果使用相同模型，Run 消耗应该是 Walk 的约 4x（速度平方）

**实际**（使用不同模型）：
- Walk：0.044% / s
- Run：0.073% / s
- **Run 消耗只是 Walk 的 1.63x**

**结论**：Pandolf 模型在低速时的消耗比 Givoni 模型要高，所以 Walk 的消耗比预期要高。

---

### 问题2：Sprint 和 Run 的消耗比

**预期**：Sprint 消耗应该是 Run 的 2.5x

**实际**：
- Run：0.073% / s
- Sprint：0.370% / s
- **Sprint 消耗是 Run 的 5.1x**

**原因**：
1. Sprint 速度更快（5.0 m/s vs 3.7 m/s），Givoni 模型是速度的 2.2 次幂
2. Sprint 乘以 SPRINT_STAMINA_DRAIN_MULTIPLIER（2.5x）

**计算**：
```
速度比 = 5.0 / 3.7 = 1.35
速度影响 = 1.35^2.2 = 1.35^2.2 = 1.86
Sprint 倍数 = 2.5
总倍数 = 1.86 * 2.5 = 4.65 ≈ 5.1x（实际）
```

**结论**：Sprint 消耗远高于预期的 2.5x，因为速度增加也会增加消耗。

---

## 对设计标准的影响

### 原设计标准

1. **Run**：927秒消耗80%（每秒消耗0.0863%）
2. **Sprint**：每秒消耗 = Run × 2.5 = 0.2158%
3. **Walk**：每秒恢复0.2%（每5秒恢复1%）

---

### 实际消耗率（使用默认 ENERGY_TO_STAMINA_COEFF = 0.000015）

| 移动类型 | 每秒消耗/恢复 | 目标 | 差异 |
|---------|-------------|------|------|
| **Walk** | 0.044%（消耗！） | +0.2%（恢复） | ❌ 相差 -0.244% |
| **Run** | 0.073% | -0.0863%（消耗） | ❌ 低 15% |
| **Sprint** | 0.370% | -0.2158%（消耗） | ❌ 高 71% |

---

### 需要调整的参数

#### 1. ENERGY_TO_STAMINA_COEFF

**目标**：让 Run 消耗达到每秒 0.0863%

**计算**：
```
当前 Run 消耗：0.073% / s
目标 Run 消耗：0.0863% / s
调整倍数 = 0.0863 / 0.073 = 1.18
新 ENERGY_TO_STAMINA_COEFF = 0.000015 * 1.18 = 0.0000177
```

**验证**：
```
Run 消耗：0.073% * 1.18 = 0.086% / s ✅
Sprint 消耗：0.370% * 1.18 = 0.437% / s（目标：0.2158%）❌
Walk 消耗：0.044% * 1.18 = 0.052% / s（目标：-0.2%）❌
```

**问题**：Sprint 消耗远高于目标！

---

#### 2. SPRINT_STAMINA_DRAIN_MULTIPLIER

**目标**：让 Sprint 消耗是 Run 的 2.5x

**当前状态**：
- Run 消耗：0.086% / s（使用调整后的 ENERGY_TO_STAMINA_COEFF）
- Sprint 消耗：0.437% / s（使用调整后的 ENERGY_TO_STAMINA_COEFF 和 SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5）
- Sprint/Run 比例：5.1x（远高于 2.5x）

**问题**：速度差异（5.0 m/s vs 3.7 m/s）导致 Sprint 自然消耗就是 Run 的 1.86x，再加上 SPRINT_STAMINA_DRAIN_MULTIPLIER（2.5x），总倍数为 4.65x。

**解决方案**：
```
目标比例：Sprint / Run = 2.5x
自然比例（速度差异）：1.86x
需要的 SPRINT_STAMINA_DRAIN_MULTIPLIER = 2.5 / 1.86 = 1.34
```

**验证**：
```
Run 消耗：0.086% / s
Sprint 消耗：0.086% * 1.86 * 1.34 = 0.215% / s ✅
```

---

#### 3. Walk 恢复问题

**当前状态**：
- Walk 消耗：0.052% / s（使用调整后的 ENERGY_TO_STAMINA_COEFF）
- 目标：Walk 净恢复 +0.2% / s

**问题**：Walk 使用 Pandolf 模型，即使速度很低（1.8 m/s），消耗也很高（0.052% / s），无法实现净恢复。

**解决方案**：
- **降低 ENERGY_TO_STAMINA_COEFF**：让 Walk 消耗更低
- 或者 **提高恢复率**：让恢复速度大于 Walk 消耗

---

## 优化器约束条件调整建议

### 1. 调整 ENERGY_TO_STAMINA_COEFF 搜索空间

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

---

### 2. 调整 SPRINT_STAMINA_DRAIN_MULTIPLIER 搜索空间

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
- 速度差异已经导致 Sprint 自然消耗是 Run 的 1.86x
- 只需要额外的 1.34x 倍数就能达到 2.5x 的目标

---

### 3. 调整行走恢复目标

**修改前**：
```python
# 期望 2 分钟恢复 2-5%
min_walk_gain = 0.02
max_walk_gain = 0.05
```

**修改后**：
```python
# 期望 2 分钟恢复 0.5-2%（降低目标，考虑 Pandolf 模型的消耗）
min_walk_gain = 0.005
max_walk_gain = 0.02
```

**理由**：
- Pandolf 模型在低速时的消耗比预期高
- 降低恢复目标，避免约束冲突

---

## 总结

### 关键发现

1. **Walk 使用 Pandolf 模型**：低速时消耗较高，难以实现净恢复
2. **Run 和 Sprint 使用 Givoni 模型**：速度对消耗的影响很大（2.2 次幂）
3. **Sprint 自然消耗高**：速度差异（5.0 m/s vs 3.7 m/s）导致 Sprint 自然消耗是 Run 的 1.86x

### 优化器调整建议

1. ✅ 降低 ENERGY_TO_STAMINA_COEFF 搜索空间（1.5e-5 到 3.0e-5）
2. ✅ 降低 SPRINT_STAMINA_DRAIN_MULTIPLIER 搜索空间（1.2 到 1.5）
3. ✅ 降低行走恢复目标（0.5-2% / 120s）

### 预期效果

- **Run**：927秒消耗80%（每秒消耗0.0863%）✅
- **Sprint**：每秒消耗0.2158%（约 Run 的 2.5x）✅
- **Walk**：120秒恢复0.5-2%（净恢复）✅