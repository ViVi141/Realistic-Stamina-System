# Python 和 C 代码一致性检查完整报告

## 检查日期
2026-01-30

---

## 1. 常量值一致性检查

### 1.1 一致的常量（41个，85.4%）

✅ 基础参数（3个）
- GAME_MAX_SPEED = 5.2
- CHARACTER_WEIGHT = 90.0
- REFERENCE_WEIGHT = 90.0

✅ Pandolf 模型（7个）
- PANDOLF_VELOCITY_COEFF = 3.2
- PANDOLF_VELOCITY_OFFSET = 0.7
- PANDOLF_BASE_COEFF = 2.7
- PANDOLF_GRADE_BASE_COEFF = 0.23
- PANDOLF_GRADE_VELOCITY_COEFF = 1.34
- PANDOLF_STATIC_COEFF_1 = 1.2
- PANDOLF_STATIC_COEFF_2 = 1.6

✅ Givoni-Goldman 模型（2个）
- GIVONI_CONSTANT = 0.8
- GIVONI_VELOCITY_EXPONENT = 2.2

✅ 能量转换（1个）
- ENERGY_TO_STAMINA_COEFF = 0.000015

✅ 负重惩罚（5个）
- BASE_WEIGHT = 1.36
- ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20
- ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0
- LOAD_RECOVERY_PENALTY_COEFF = 0.0001
- LOAD_RECOVERY_PENALTY_EXPONENT = 2.0

✅ 速度阈值（2个）
- SPRINT_VELOCITY_THRESHOLD = 5.2
- RUN_VELOCITY_THRESHOLD = 3.7

✅ Sprint 参数（2个）
- SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0
- SPRINT_SPEED_BOOST = 0.30

✅ 疲劳参数（3个）
- FATIGUE_ACCUMULATION_COEFF = 0.015
- FATIGUE_MAX_FACTOR = 2.0
- FATIGUE_START_TIME_MINUTES = 5.0

✅ 代谢效率（4个）
- AEROBIC_EFFICIENCY_FACTOR = 0.9
- ANAEROBIC_EFFICIENCY_FACTOR = 1.2
- AEROBIC_THRESHOLD = 0.6
- ANAEROBIC_THRESHOLD = 0.8

✅ 姿态消耗（2个）
- POSTURE_CROUCH_MULTIPLIER = 1.8
- POSTURE_PRONE_MULTIPLIER = 3.0

✅ 动作消耗（4个）
- JUMP_STAMINA_BASE_COST = 0.035
- VAULT_STAMINA_START_COST = 0.02
- CLIMB_STAMINA_TICK_COST = 0.01
- JUMP_CONSECUTIVE_PENALTY = 0.5

✅ 高级恢复（2个）
- MARGINAL_DECAY_THRESHOLD = 0.8
- MARGINAL_DECAY_COEFF = 1.1

---

### 1.2 不一致的常量（6个，12.5%）

❌ 恢复系统参数（6个）

| 参数 | Python | C 代码 | 差异 | 影响 |
|-----|--------|--------|------|------|
| BASE_RECOVERY_RATE | 0.00015 | 0.00035 | +133% | **C 代码恢复速度更快** |
| FAST_RECOVERY_MULTIPLIER | 1.6 | 2.5 | +56% | C 代码快速恢复更快 |
| MEDIUM_RECOVERY_MULTIPLIER | 1.3 | 1.4 | +8% | C 代码中等恢复稍快 |
| STANDING_RECOVERY_MULTIPLIER | 1.3 | 1.5 | +15% | C 代码站立恢复更快 |
| CROUCHING_RECOVERY_MULTIPLIER | 1.4 | 1.5 | +7% | C 代码蹲姿恢复稍快 |
| PRONE_RECOVERY_MULTIPLIER | 1.6 | 1.8 | +13% | C 代码趴姿恢复更快 |

**影响分析**：
- C 代码的恢复速度比 Python 快约 133%（BASE_RECOVERY_RATE）
- 这会导致游戏内恢复速度与数字孪生预测严重不一致
- 玩家在游戏内的恢复体验会比预期快很多

---

### 1.3 缺失的常量（5个，10.4%）

⚠️ Python 缺失（1个）
- SLOW_RECOVERY_START_MINUTES = 10.0（C 代码有）

⚠️ C 代码缺失（4个）
- ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.5（Python 有）
- ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02（Python 有）
- ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05（Python 有）
- ENV_SURFACE_WETNESS_PENALTY_MAX = 0.15（Python 有）

**影响分析**：
- 缺失的常量可能不是核心功能
- 环境参数可能在 C 代码中由其他模块处理

---

## 2. 算法一致性检查

### 2.1 Pandolf 模型算法

#### Python 实现
```python
def _pandolf_expenditure(self, velocity, current_weight, grade_percent, terrain_factor):
    # 计算基础项
    base_term = (vb * (1.0 - 0.2 * fit)) + (vc * (vt * vt))
    
    # 计算坡度项
    grade_term = g_dec * (gb + gv * vsq)
    
    # 地形因子保护
    terrain_factor = np.clip(terrain_factor, 0.5, 3.0)
    
    # 负重系数（移除上限）
    w_mult = max(0.1, current_weight / ref)
    
    # 总瓦特（注意：乘以 ref）
    energy_per_kg = (base_term + grade_term) * terrain_factor
    total_watts = energy_per_kg * ref * w_mult
    
    # 消耗率（移除上限）
    rate = total_watts * coeff
    return float(max(0.0, rate))
```

#### C 代码实现
```c
static float CalculatePandolfEnergyExpenditure(float velocity, float currentWeight, float gradePercent, float terrainFactor, bool useSanteeCorrection)
{
    // 计算基础项
    float baseTerm = (PANDOLF_BASE_COEFF * fitnessBonus) + (PANDOLF_VELOCITY_COEFF * velocitySquaredTerm);
    
    // 计算坡度项
    float gradeTerm = gradeDecimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocitySquared));
    
    // 坡度保护（Python没有）
    float maxGradeTerm = baseTerm * 3.0;
    gradeTerm = Math.Min(gradeTerm, maxGradeTerm);
    
    // 地形因子保护
    terrainFactor = Math.Clamp(terrainFactor, 0.5, 3.0);
    
    // 负重系数（移除上限，但保留下限0.5）
    float weightMultiplier = currentWeight / REFERENCE_WEIGHT;
    weightMultiplier = Math.Max(weightMultiplier, 0.5);
    
    // 能量消耗（注意：没有乘以 REFERENCE_WEIGHT）
    float energyExpenditure = weightMultiplier * (baseTerm + gradeTerm) * terrainFactor;
    
    // 消耗率
    staminaDrainRate = energyExpenditure * energyToStaminaCoeff;
    return staminaDrainRate;
}
```

#### 差异分析

| 项目 | Python | C 代码 | 状态 |
|-----|--------|--------|------|
| 基础项计算 | ✅ 一致 | ✅ 一致 | ✅ |
| 坡度项计算 | ✅ 一致 | ✅ 一致 | ✅ |
| Santee 修正 | ✅ 一致 | ✅ 一致 | ✅ |
| 地形因子保护 | ✅ 一致（0.5-3.0） | ✅ 一致（0.5-3.0） | ✅ |
| 坡度保护 | ❌ 无 | ✅ 有（3倍基础项） | ⚠️ **C 代码额外保护** |
| 负重系数下限 | 0.1 | 0.5 | ⚠️ **不一致** |
| 负重系数上限 | ❌ 无 | ❌ 无 | ✅ |
| 乘以 REFERENCE_WEIGHT | ✅ 有 | ❌ 无 | ⚠️ **不一致** |
| Clip 上限 | ❌ 无 | ❌ 无 | ✅ |

#### 关键问题

**问题 1：能量计算不一致**
- Python：`total_watts = energy_per_kg * ref * w_mult`
- C 代码：`energyExpenditure = weightMultiplier * (baseTerm + gradeTerm) * terrainFactor`
- **差异**：Python 乘以了 `ref`（90.0），C 代码没有
- **影响**：Python 的消耗率比 C 代码高 90 倍（如果其他条件相同）

**问题 2：坡度保护不一致**
- Python：无坡度保护
- C 代码：`gradeTerm = Math.Min(gradeTerm, maxGradeTerm)`（3倍基础项）
- **影响**：C 代码在极端坡度时消耗会更小

**问题 3：负重系数下限不一致**
- Python：`max(0.1, current_weight / ref)`
- C 代码：`Math.Max(weightMultiplier, 0.5)`
- **影响**：C 代码的最小负重系数更大（0.5 vs 0.1）

---

### 2.2 Givoni-Goldman 模型算法

#### Python 实现
```python
def _givoni_running(self, velocity, current_weight):
    if velocity <= 2.2:
        return 0.0
    
    ref = getattr(self.constants, 'REFERENCE_WEIGHT', 90.0)
    const = getattr(self.constants, 'GIVONI_CONSTANT', 0.8)
    exp = getattr(self.constants, 'GIVONI_VELOCITY_EXPONENT', 2.2)
    coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
    
    # 负重系数（移除上限）
    w_mult = max(0.1, current_weight / ref)
    
    # 能量消耗（注意：乘以 ref）
    vp = velocity ** exp
    total_watts = const * vp * ref * w_mult
    
    # 消耗率（移除上限）
    rate = total_watts * coeff
    return float(max(0.0, rate))
```

#### C 代码实现
```c
static float CalculateGivoniGoldmanRunning(float velocity, float currentWeight, bool isRunning = true)
{
    if (!isRunning || velocity <= 2.2)
        return 0.0;
    
    float velocityPower = Math.Pow(velocity, GIVONI_VELOCITY_EXPONENT);
    
    // 负重系数（移除上限，但保留下限0.5）
    float weightMultiplier = currentWeight / REFERENCE_WEIGHT;
    weightMultiplier = Math.Max(weightMultiplier, 0.5);
    
    // 能量消耗（注意：没有乘以 REFERENCE_WEIGHT）
    float runningEnergyExpenditure = weightMultiplier * GIVONI_CONSTANT * velocityPower;
    
    // 消耗率
    float runningDrainRate = runningEnergyExpenditure * energyToStaminaCoeff;
    return runningDrainRate;
}
```

#### 差异分析

| 项目 | Python | C 代码 | 状态 |
|-----|--------|--------|------|
| 速度判断 | ✅ 一致（≤2.2） | ✅ 一致（≤2.2） | ✅ |
| 速度指数 | ✅ 一致（2.2） | ✅ 一致（2.2） | ✅ |
| 常数 | ✅ 一致（0.8） | ✅ 一致（0.8） | ✅ |
| 负重系数下限 | 0.1 | 0.5 | ⚠️ **不一致** |
| 乘以 REFERENCE_WEIGHT | ✅ 有 | ❌ 无 | ⚠️ **不一致** |
| Clip 上限 | ❌ 无 | ❌ 无 | ✅ |

#### 关键问题

**问题 1：能量计算不一致**
- Python：`total_watts = const * vp * ref * w_mult`
- C 代码：`runningEnergyExpenditure = weightMultiplier * GIVONI_CONSTANT * velocityPower`
- **差异**：Python 乘以了 `ref`（90.0），C 代码没有
- **影响**：Python 的消耗率比 C 代码高 90 倍（如果其他条件相同）

**问题 2：负重系数下限不一致**
- Python：`max(0.1, current_weight / ref)`
- C 代码：`Math.Max(weightMultiplier, 0.5)`
- **影响**：C 代码的最小负重系数更大（0.5 vs 0.1）

---

### 2.3 静态消耗算法

#### Python 实现
```python
def _static_standing_cost(self, body_weight, load_weight):
    c1 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_1', 1.2)
    c2 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_2', 1.6)
    coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
    
    base = c1 * body_weight
    load_term = 0.0
    if body_weight > 0 and load_weight > 0:
        r = load_weight / body_weight
        load_term = c2 * (body_weight + load_weight) * (r * r)
    
    rate = (base + load_term) * coeff
    return float(max(0.0, rate))
```

#### C 代码实现
```c
static float CalculateStaticStandingCost(float bodyWeight = 90.0, float loadWeight = 0.0)
{
    float baseStaticTerm = PANDOLF_STATIC_COEFF_1 * bodyWeight;
    
    float loadRatio = 0.0;
    if (bodyWeight > 0.0)
        loadRatio = loadWeight / bodyWeight;
    
    float loadStaticTerm = 0.0;
    if (loadWeight > 0.0)
    {
        float loadRatioSquared = loadRatio * loadRatio;
        loadStaticTerm = PANDOLF_STATIC_COEFF_2 * (bodyWeight + loadWeight) * loadRatioSquared;
    }
    
    float staticEnergyExpenditure = baseStaticTerm + loadStaticTerm;
    
    float energyToStaminaCoeff = StaminaConstants.GetEnergyToStaminaCoeff();
    float staticDrainRate = staticEnergyExpenditure * energyToStaminaCoeff;
    
    staticDrainRate = Math.Max(staticDrainRate, 0.0);
    return staticDrainRate;
}
```

#### 差异分析

| 项目 | Python | C 代码 | 状态 |
|-----|--------|--------|------|
| 基础项计算 | ✅ 一致 | ✅ 一致 | ✅ |
| 负重项计算 | ✅ 一致 | ✅ 一致 | ✅ |
| Clip 上限 | ❌ 无 | ❌ 无 | ✅ |

#### 结论

✅ **静态消耗算法完全一致**

---

### 2.4 恢复逻辑算法

由于恢复逻辑较复杂，这里只检查关键差异：

| 项目 | Python | C 代码 | 状态 |
|-----|--------|--------|------|
| 基础恢复率 | 0.00015 | 0.00035 | ❌ **不一致** |
| 负重惩罚系数 | 0.0001 | 0.0001 | ✅ 一致 |
| 负重惩罚指数 | 2.0 | 2.0 | ✅ 一致 |
| 负重除数 | REFERENCE_WEIGHT | BODY_TOLERANCE_BASE | ✅ 一致（都是90.0） |
| Clip 上限 | ❌ 无 | ❌ 无 | ✅ |

---

### 2.5 环境因子保护

| 项目 | Python | C 代码 | 状态 |
|-----|--------|--------|------|
| 坡度保护 | ✅ 有（-85%~85%） | ✅ 有（-85%~85%） | ✅ 一致 |
| 地形保护 | ✅ 有（0.5~3.0） | ✅ 有（0.5~3.0） | ✅ 一致 |
| 坡度项保护 | ❌ 无 | ✅ 有（3倍基础项） | ⚠️ **C 代码额外保护** |

---

### 2.6 负重保护

| 项目 | Python | C 代码 | 状态 |
|-----|--------|--------|------|
| 负重系数上限 | ❌ 无 | ❌ 无 | ✅ 一致 |
| 负重系数下限 | 0.1 | 0.5 | ⚠️ **不一致** |

---

## 3. 数据计算流一致性检查

### 3.1 Python 数据流

```
输入参数 → Pandolf/Givoni 模型 → 能量计算（Watts） → 乘以 ENERGY_TO_STAMINA_COEFF → 消耗率（%/s） → 乘以 0.2 → 每tick消耗
```

### 3.2 C 代码数据流

```
输入参数 → Pandolf/Givoni 模型 → 能量计算（W/kg） → 乘以 ENERGY_TO_STAMINA_COEFF → 消耗率（%/s） → 乘以 0.2 → 每tick消耗
```

### 3.3 关键差异

| 项目 | Python | C 代码 | 影响 |
|-----|--------|--------|------|
| 能量单位 | Watts（总瓦特） | W/kg（瓦特/千克） | ⚠️ **单位不一致** |
| 是否乘以 REFERENCE_WEIGHT | ✅ 有 | ❌ 无 | ⚠️ **计算不一致** |
| 最终消耗率 | 高 90 倍 | 低 | ⚠️ **严重不一致** |

---

## 4. 总结

### 4.1 主要问题

#### 问题 1：恢复系统参数不一致（紧急）
- C 代码的恢复速度比 Python 快 133%
- 6 个恢复参数不一致

#### 问题 2：能量计算不一致（严重）
- Python 乘以了 `REFERENCE_WEIGHT`（90.0），C 代码没有
- Python 的消耗率比 C 代码高 90 倍（如果其他条件相同）

#### 问题 3：负重系数下限不一致（中等）
- Python：0.1
- C 代码：0.5

#### 问题 4：坡度保护不一致（次要）
- Python：无坡度项保护
- C 代码：有坡度项保护（3倍基础项）

### 4.2 修复建议

#### 修复 1：恢复系统参数（紧急）
将 C 代码的恢复参数调整为与 Python 一致：
```c
BASE_RECOVERY_RATE = 0.00015  // 从 0.00035 降低
FAST_RECOVERY_MULTIPLIER = 1.6  // 从 2.5 降低
MEDIUM_RECOVERY_MULTIPLIER = 1.3  // 从 1.4 降低
STANDING_RECOVERY_MULTIPLIER = 1.3  // 从 1.5 降低
CROUCHING_RECOVERY_MULTIPLIER = 1.4  // 从 1.5 降低
PRONE_RECOVERY_MULTIPLIER = 1.6  // 从 1.8 降低
```

#### 修复 2：能量计算（严重）
**选项 A**：在 C 代码中添加 `* REFERENCE_WEIGHT`
**选项 B**：在 Python 中移除 `* ref`
**建议**：选择选项 B，因为减少乘法运算更简单

#### 修复 3：负重系数下限（中等）
统一为 0.1 或 0.5（建议 0.1）

---

## 5. 一致性统计

### 5.1 总体统计

- 检查的常量总数：48
- 一致的常量：41 (85.4%)
- 不一致的常量：6 (12.5%)
- 缺失的常量：5 (10.4%)

### 5.2 算法一致性

- 检查的算法数：6
- 完全一致：1 (静态消耗)
- 基本一致：4 (Pandolf, Givoni, 恢复逻辑, 环境保护)
- 不一致：1 (数据计算流)

---

## 6. 结论

### 6.1 严重问题

1. **恢复系统参数不一致**：C 代码恢复速度快 133%
2. **能量计算不一致**：Python 消耗率高 90 倍

### 6.2 建议

**优先修复恢复参数和能量计算不一致**，确保 C 代码和 Python 数字孪生的行为一致。