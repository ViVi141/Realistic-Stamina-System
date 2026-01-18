# 军事体力系统模型实现总结

## 已实现的功能

### 1. 新模型常量（SCR_RealisticStaminaSystem.c）

已添加以下常量：

- **速度阈值**：
  - `SPRINT_VELOCITY_THRESHOLD = 5.2` m/s
  - `RUN_VELOCITY_THRESHOLD = 3.7` m/s（匹配15:27的2英里配速）
  - `WALK_VELOCITY_THRESHOLD = 3.2` m/s

- **基础消耗率（pts/s）**：
  - `SPRINT_BASE_DRAIN_RATE = 0.480` pts/s
  - `RUN_BASE_DRAIN_RATE = 0.105` pts/s
  - `WALK_BASE_DRAIN_RATE = 0.060` pts/s
  - `REST_RECOVERY_RATE = 0.250` pts/s

- **转换为每0.2秒的消耗率**：
  - `SPRINT_DRAIN_PER_TICK = 0.00096`（每0.2秒）
  - `RUN_DRAIN_PER_TICK = 0.00021`（每0.2秒）
  - `WALK_DRAIN_PER_TICK = 0.00012`（每0.2秒）
  - `REST_RECOVERY_PER_TICK = 0.0005`（每0.2秒）

- **初始状态**：
  - `INITIAL_STAMINA_AFTER_ACFT = 0.85`（85%）

- **精疲力尽阈值**：
  - `EXHAUSTION_THRESHOLD = 0.0`（0点）
  - `EXHAUSTION_LIMP_SPEED = 1.0` m/s（跛行速度）
  - `SPRINT_ENABLE_THRESHOLD = 0.20`（20点）

- **坡度修正系数**：
  - `GRADE_UPHILL_COEFF = 0.12`（每1%上坡增加12%消耗）
  - `GRADE_DOWNHILL_COEFF = 0.05`（每1%下坡减少5%消耗）
  - `HIGH_GRADE_THRESHOLD = 15.0`（15%高坡度阈值）
  - `HIGH_GRADE_MULTIPLIER = 1.2`（高坡度额外1.2×乘数）

### 2. 新模型函数（SCR_RealisticStaminaSystem.c）

已添加以下函数：

1. **`CalculateBaseDrainRateByVelocity(float velocity)`**
   - 根据当前速度计算基础消耗率（基于速度阈值）
   - 返回每0.2秒的消耗率（负数表示恢复）

2. **`CalculateGradeMultiplier(float gradePercent)`**
   - 计算坡度修正乘数（基于百分比坡度）
   - 上坡：K_grade = 1 + (G × 0.12)
   - 下坡：K_grade = 1 + (G × 0.05)
   - 高坡度（G > 15%）：额外 × 1.2

3. **`IsExhausted(float staminaPercent)`**
   - 检查是否精疲力尽（S ≤ 0）

4. **`CanSprint(float staminaPercent)`**
   - 检查是否可以Sprint（S ≥ 20）

## 已实现的功能（融合模型）

### 1. 在PlayerBase.c中集成新模型 ✅

已在 `UpdateSpeedBasedOnStamina()` 函数中实现：

1. **精疲力尽逻辑** ✅：
   - 如果 `IsExhausted(staminaPercent)`，强制速度为 `EXHAUSTION_LIMP_SPEED`（1.0 m/s）
   - 如果 `!CanSprint(staminaPercent)`，禁用Sprint（强制切换到Run状态）

2. **基于速度阈值的分段消耗率（融合）** ✅：
   - 使用 `CalculateBaseDrainRateByVelocity(currentSpeed)` 计算基础消耗率
   - 保留多维度特性（健康状态、累积疲劳、代谢适应）
   - 应用效率因子和疲劳因子到基础消耗率
   - 保留原有的速度相关项（用于平滑过渡）

3. **新的坡度修正（融合）** ✅：
   - 将坡度角度（度）转换为坡度百分比：`gradePercent = tan(slopeAngleDegrees × π/180) × 100`
   - 使用 `CalculateGradeMultiplier(gradePercent)` 计算坡度修正
   - 保留原有的多维度交互项（用于精细调整）

4. **设置初始体力值** ✅：
   - 在 `OnInit()` 中，设置初始体力值为 `INITIAL_STAMINA_AFTER_ACFT`（0.85，85%）

### 2. 融合模型特点

融合模型结合了两种模型的优势：

- **新模型特性**：
  - ✅ 基于速度阈值的分段消耗率（Sprint/Run/Walk/Rest）
  - ✅ 简化的坡度修正（K_grade = 1 + G × 0.12）
  - ✅ 精疲力尽逻辑（S ≤ 0时跛行，S < 20时禁用Sprint）
  - ✅ 初始体力值85%（ACFT后预疲劳状态）

- **保留的多维度特性**：
  - ✅ 健康状态效率因子（训练有素者效率提高18%）
  - ✅ 累积疲劳因子（运动5分钟后开始累积疲劳）
  - ✅ 代谢适应效率因子（有氧/混合/无氧区）
  - ✅ 负重影响（基于体重的真实模型）
  - ✅ 多维度恢复模型（非线性恢复、休息时间、年龄、疲劳恢复）
  - ✅ 速度×负重×坡度三维交互项（精细调整）

### 2. 模型选择机制

建议添加一个标志来选择使用哪个模型：

```enforce
// 在 SCR_RealisticStaminaSystem.c 中添加
static const bool USE_MILITARY_STAMINA_MODEL = true; // true = 使用新模型，false = 使用旧模型
```

### 3. 坡度百分比转换

需要将坡度角度（度）转换为坡度百分比：

```enforce
// 坡度百分比 = tan(坡度角度) × 100
float gradePercent = Math.Tan(slopeAngleDegrees * Math.DEG2RAD) * 100.0;
```

## 实现建议

### 方案1：完全替换现有模型
- 优点：模型简单清晰，易于理解和调试
- 缺点：失去现有的多维度模型优势

### 方案2：融合两种模型（推荐）
- 保留现有模型的多维度特性（健康状态、累积疲劳、代谢适应）
- 添加基于速度阈值的分段消耗率
- 添加精疲力尽逻辑
- 调整坡度修正以匹配新规范

### 方案3：创建新模型作为选项
- 保留现有模型
- 添加新模型作为可选模式
- 用户可以选择使用哪个模型

## 下一步行动

1. ✅ 添加新模型常量和函数
2. ✅ 在PlayerBase.c中集成新模型
3. ✅ 添加精疲力尽逻辑
4. ✅ 添加基于速度阈值的分段消耗率
5. ✅ 设置初始体力值
6. ⏳ 测试融合模型是否符合ACFT目标（15:27完成2英里）

## 融合模型实现总结

### 核心改进

1. **基于速度阈值的分段消耗率**：
   - Sprint (≥5.2 m/s): 0.480 pts/s
   - Run (≥3.7 m/s): 0.105 pts/s
   - Walk (≥3.2 m/s): 0.060 pts/s
   - Rest (<3.2 m/s): -0.250 pts/s（恢复）

2. **新的坡度修正**：
   - 上坡：K_grade = 1 + (G × 0.12)
   - 下坡：K_grade = 1 + (G × 0.05)
   - 高坡度（G > 15%）：额外 × 1.2

3. **精疲力尽逻辑**：
   - S ≤ 0：强制速度1.0 m/s（跛行）
   - S < 20：禁用Sprint

4. **初始状态**：
   - 初始体力值：85%（ACFT后预疲劳状态）

### 保留的多维度特性

- 健康状态效率因子
- 累积疲劳因子
- 代谢适应效率因子
- 负重影响（基于体重）
- 多维度恢复模型
- 速度×负重×坡度三维交互项

## 注意事项

1. **体力池范围**：
   - 新模型使用0.0-100.0点数系统
   - 现有模型使用0.0-1.0百分比系统
   - 需要确认是否调整现有系统

2. **更新间隔**：
   - 现有模型更新间隔为0.2秒
   - 新模型消耗率基于每秒（pts/s）
   - 需要转换为每0.2秒的消耗率

3. **ACFT目标校准**：
   - 在 V = 3.7, G = 0 时，S 应从 85 → 0 在约 927 秒内（15:27）
   - 需要验证新模型是否符合此目标
