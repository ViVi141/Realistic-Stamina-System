# 军事体力系统模型 - 面向对象设计文档

## 1. 概述

本文档采用面向对象编程（OOP）的逻辑结构，定义了军事体力系统模型的完整架构，包括状态类、转换公式和约束条件。

## 2. 核心类定义

### 2.1 StaminaSystem（体力系统核心类）

```cpp
class StaminaSystem {
    // ==================== 属性（Properties）====================
    private:
        float stamina;              // 当前体力值 (0.0 - 100.0)
        float velocity;              // 当前速度 (m/s)
        float gradePercent;          // 坡度百分比 (-100.0 - 100.0)
        float encumbranceWeight;     // 负重重量 (kg)
        MovementState currentState;  // 当前移动状态
        HealthStatus healthStatus;   // 健康状态
        FatigueState fatigueState;   // 疲劳状态
        MetabolicZone metabolicZone; // 代谢区域
        
    // ==================== 常量（Constants）====================
    public:
        static const float CHARACTER_WEIGHT = 90.0;      // 角色体重 (kg)
        static const float BASE_WEIGHT = 1.36;            // 基准重量 (kg)
        static const float INITIAL_STAMINA = 85.0;        // 初始体力值
        static const float MAX_STAMINA = 100.0;           // 最大体力值
        static const float MIN_STAMINA = 0.0;              // 最小体力值
        
        // 速度阈值
        static const float SPRINT_VELOCITY_THRESHOLD = 5.2;  // Sprint速度阈值 (m/s)
        static const float RUN_VELOCITY_THRESHOLD = 3.7;     // Run速度阈值 (m/s)
        static const float WALK_VELOCITY_THRESHOLD = 3.2;    // Walk速度阈值 (m/s)
        
        // 精疲力尽阈值
        static const float EXHAUSTION_THRESHOLD = 0.0;       // 精疲力尽阈值
        static const float SPRINT_ENABLE_THRESHOLD = 20.0;   // Sprint启用阈值
        static const float EXHAUSTION_LIMP_SPEED = 1.0;      // 跛行速度 (m/s)
        
        // 坡度修正系数
        static const float GRADE_UPHILL_COEFF = 0.12;        // 上坡系数
        static const float GRADE_DOWNHILL_COEFF = 0.05;     // 下坡系数
        static const float HIGH_GRADE_THRESHOLD = 15.0;      // 高坡度阈值 (%)
        static const float HIGH_GRADE_MULTIPLIER = 1.2;     // 高坡度倍数
        
    // ==================== 方法（Methods）====================
    public:
        // 构造函数
        StaminaSystem();
        
        // 状态查询方法
        MovementState getCurrentState();
        bool isExhausted();
        bool canSprint();
        
        // 状态更新方法
        void update(float deltaTime);
        void setVelocity(float velocity);
        void setGradePercent(float gradePercent);
        void setEncumbranceWeight(float weight);
        
        // 消耗率计算方法
        float calculateBaseDrainRate();
        float calculateGradeMultiplier();
        float calculateTotalDrainRate(float deltaTime);
        
        // 恢复率计算方法
        float calculateRecoveryRate(float deltaTime);
};
```

### 2.2 MovementState（移动状态枚举）

```cpp
enum class MovementState {
    REST,    // 休息状态 (V < 3.2 m/s)
    WALK,    // 步行状态 (3.2 ≤ V < 3.7 m/s)
    RUN,     // 跑步状态 (3.7 ≤ V < 5.2 m/s)
    SPRINT,  // 冲刺状态 (V ≥ 5.2 m/s)
    EXHAUSTED // 精疲力尽状态 (S ≤ 0)
};
```

### 2.3 HealthStatus（健康状态类）

```cpp
class HealthStatus {
    private:
        float fitnessLevel;          // 训练水平 (0.0 - 1.0)
        float age;                   // 年龄 (岁)
        float fitnessEfficiencyCoeff; // 训练效率系数
        
    public:
        static const float DEFAULT_FITNESS_LEVEL = 1.0;      // 默认训练水平（训练有素）
        static const float DEFAULT_AGE = 22.0;               // 默认年龄
        static const float FITNESS_EFFICIENCY_COEFF = 0.18;  // 训练效率系数（18%）
        
        // 构造函数
        HealthStatus(float fitnessLevel = DEFAULT_FITNESS_LEVEL, 
                     float age = DEFAULT_AGE);
        
        // 计算效率因子
        float calculateEfficiencyFactor();
        
        // Getter方法
        float getFitnessLevel();
        float getAge();
};
```

### 2.4 FatigueState（疲劳状态类）

```cpp
class FatigueState {
    private:
        float exerciseDurationMinutes; // 运动持续时间 (分钟)
        float fatigueFactor;           // 疲劳因子
        
    public:
        static const float FATIGUE_START_TIME_MINUTES = 5.0;    // 疲劳开始时间
        static const float FATIGUE_ACCUMULATION_COEFF = 0.015;  // 疲劳累积系数
        static const float FATIGUE_MAX_FACTOR = 2.0;            // 最大疲劳因子
        
        // 构造函数
        FatigueState();
        
        // 更新疲劳状态
        void update(float deltaTime, bool isMoving);
        
        // 计算疲劳因子
        float calculateFatigueFactor();
        
        // Getter方法
        float getExerciseDuration();
        float getFatigueFactor();
};
```

### 2.5 MetabolicZone（代谢区域枚举）

```cpp
enum class MetabolicZone {
    AEROBIC,      // 有氧区 (V < 60% VO2max)
    MIXED,        // 混合区 (60% ≤ V < 80% VO2max)
    ANAEROBIC     // 无氧区 (V ≥ 80% VO2max)
};
```

### 2.6 MetabolicAdaptation（代谢适应类）

```cpp
class MetabolicAdaptation {
    private:
        MetabolicZone currentZone;
        float efficiencyFactor;
        
    public:
        static const float AEROBIC_THRESHOLD = 0.6;           // 有氧阈值
        static const float ANAEROBIC_THRESHOLD = 0.8;         // 无氧阈值
        static const float AEROBIC_EFFICIENCY_FACTOR = 0.9;    // 有氧效率因子
        static const float ANAEROBIC_EFFICIENCY_FACTOR = 1.2;  // 无氧效率因子
        
        // 构造函数
        MetabolicAdaptation();
        
        // 根据速度比计算代谢区域
        MetabolicZone determineZone(float speedRatio);
        
        // 计算效率因子
        float calculateEfficiencyFactor(float speedRatio);
        
        // Getter方法
        MetabolicZone getCurrentZone();
        float getEfficiencyFactor();
};
```

### 2.7 RecoveryModel（恢复模型类）

```cpp
class RecoveryModel {
    private:
        float restDurationMinutes;      // 休息持续时间 (分钟)
        float exerciseDurationMinutes;  // 运动持续时间 (分钟)
        float recoveryRate;             // 恢复率
        
    public:
        static const float BASE_RECOVERY_RATE = 0.00015;              // 基础恢复率
        static const float RECOVERY_NONLINEAR_COEFF = 0.5;            // 非线性系数
        static const float FAST_RECOVERY_DURATION_MINUTES = 2.0;      // 快速恢复期
        static const float FAST_RECOVERY_MULTIPLIER = 1.5;            // 快速恢复倍数
        static const float SLOW_RECOVERY_START_MINUTES = 10.0;        // 慢速恢复开始时间
        static const float SLOW_RECOVERY_MULTIPLIER = 0.7;            // 慢速恢复倍数
        
        // 构造函数
        RecoveryModel();
        
        // 更新恢复状态
        void update(float deltaTime, bool isMoving);
        
        // 计算恢复率
        float calculateRecoveryRate(float staminaPercent, float age, float fitnessLevel);
        
        // Getter方法
        float getRestDuration();
        float getRecoveryRate();
};
```

## 3. 状态转换图

### 3.1 移动状态转换

```
状态转换图（State Transition Diagram）：

[REST] ──(V ≥ 3.2)──> [WALK]
[WALK] ──(V ≥ 3.7)──> [RUN]
[RUN]  ──(V ≥ 5.2)──> [SPRINT]
[SPRINT] ──(V < 5.2)──> [RUN]
[RUN]  ──(V < 3.7)──> [WALK]
[WALK] ──(V < 3.2)──> [REST]

所有状态 ──(S ≤ 0)──> [EXHAUSTED]
[EXHAUSTED] ──(S > 0)──> [REST]
```

### 3.2 状态转换约束

```cpp
class StateTransition {
    public:
        // 状态转换规则
        static MovementState transition(MovementState currentState, 
                                        float velocity, 
                                        float stamina);
        
        // 约束条件检查
        static bool canTransition(MovementState from, 
                                  MovementState to, 
                                  float velocity, 
                                  float stamina);
};
```

## 4. 转换公式（Transformation Formulas）

### 4.1 基础消耗率计算（Base Drain Rate Calculation）

```cpp
class DrainRateCalculator {
    public:
        // 基于速度阈值的分段消耗率
        static float calculateBaseDrainRate(float velocity) {
            if (velocity >= SPRINT_VELOCITY_THRESHOLD) {
                return SPRINT_BASE_DRAIN_RATE;  // 0.480 pts/s
            } else if (velocity >= RUN_VELOCITY_THRESHOLD) {
                return RUN_BASE_DRAIN_RATE;     // 0.105 pts/s
            } else if (velocity >= WALK_VELOCITY_THRESHOLD) {
                return WALK_BASE_DRAIN_RATE;   // 0.060 pts/s
            } else {
                return -REST_RECOVERY_RATE;     // -0.250 pts/s (恢复)
            }
        }
        
        // 坡度修正乘数
        static float calculateGradeMultiplier(float gradePercent) {
            float kGrade = 1.0;
            
            if (gradePercent > 0.0) {
                // 上坡：K_grade = 1 + (G × 0.12)
                kGrade = 1.0 + (gradePercent * GRADE_UPHILL_COEFF);
                
                // 高坡度额外修正（G > 15%）
                if (gradePercent > HIGH_GRADE_THRESHOLD) {
                    kGrade *= HIGH_GRADE_MULTIPLIER;
                }
            } else if (gradePercent < 0.0) {
                // 下坡：K_grade = 1 + (G × 0.05)
                kGrade = 1.0 + (gradePercent * GRADE_DOWNHILL_COEFF);
                kGrade = max(kGrade, 0.5); // 限制最多减少50%
            }
            
            return kGrade;
        }
        
        // 综合消耗率（融合模型）
        static float calculateTotalDrainRate(
            float baseDrainRate,
            float gradeMultiplier,
            float efficiencyFactor,
            float fatigueFactor,
            float sprintMultiplier) {
            
            // 恢复时，不应用修正
            if (baseDrainRate < 0.0) {
                return baseDrainRate;
            }
            
            // 消耗时，应用所有修正因子
            return baseDrainRate * gradeMultiplier * efficiencyFactor 
                   * fatigueFactor * sprintMultiplier;
        }
};
```

### 4.2 速度计算（Speed Calculation）

```cpp
class SpeedCalculator {
    public:
        // 根据体力计算速度倍数
        static float calculateSpeedMultiplier(float staminaPercent) {
            // S(E) = S_max × E^α
            // α = 0.6（基于医学文献）
            float staminaEffect = pow(staminaPercent, STAMINA_EXPONENT);
            return TARGET_SPEED_MULTIPLIER * staminaEffect;
        }
        
        // 根据负重计算速度惩罚
        static float calculateEncumbrancePenalty(float encumbranceWeight) {
            // 计算有效负重
            float effectiveWeight = max(encumbranceWeight - BASE_WEIGHT, 0.0);
            float bodyMassPercent = effectiveWeight / CHARACTER_WEIGHT;
            
            // P_enc = β × (有效负重/体重)^γ
            float penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * 
                           pow(bodyMassPercent, ENCUMBRANCE_SPEED_EXPONENT);
            
            return clamp(penalty, 0.0, 0.5); // 最多减少50%速度
        }
        
        // 精疲力尽时的速度限制
        static float calculateExhaustedSpeed(float staminaPercent) {
            if (staminaPercent <= EXHAUSTION_THRESHOLD) {
                return EXHAUSTION_LIMP_SPEED; // 1.0 m/s
            }
            return -1.0; // 未精疲力尽
        }
};
```

### 4.3 恢复率计算（Recovery Rate Calculation）

```cpp
class RecoveryRateCalculator {
    public:
        // 多维度恢复率计算
        static float calculateRecoveryRate(
            float staminaPercent,
            float restDurationMinutes,
            float exerciseDurationMinutes,
            float age,
            float fitnessLevel) {
            
            // 1. 基础恢复率（非线性）
            float baseRate = BASE_RECOVERY_RATE * 
                            pow(staminaPercent, RECOVERY_NONLINEAR_COEFF);
            
            // 2. 休息时间修正
            float restMultiplier = 1.0;
            if (restDurationMinutes <= FAST_RECOVERY_DURATION_MINUTES) {
                restMultiplier = FAST_RECOVERY_MULTIPLIER; // 快速恢复期
            } else if (restDurationMinutes >= SLOW_RECOVERY_START_MINUTES) {
                restMultiplier = SLOW_RECOVERY_MULTIPLIER; // 慢速恢复期
            }
            
            // 3. 年龄修正
            float ageMultiplier = 1.0 + (AGE_RECOVERY_COEFF * 
                                        (AGE_REFERENCE - age) / AGE_REFERENCE);
            
            // 4. 训练水平修正
            float fitnessMultiplier = 1.0 + (FITNESS_RECOVERY_COEFF * fitnessLevel);
            
            // 5. 疲劳恢复修正
            float fatigueMultiplier = 1.0;
            if (exerciseDurationMinutes > 0.0) {
                float fatiguePenalty = FATIGUE_RECOVERY_PENALTY * 
                                     min(exerciseDurationMinutes / 
                                         FATIGUE_RECOVERY_DURATION_MINUTES, 1.0);
                fatigueMultiplier = 1.0 - fatiguePenalty;
            }
            
            return baseRate * restMultiplier * ageMultiplier * 
                   fitnessMultiplier * fatigueMultiplier;
        }
};
```

## 5. 约束条件（Constraints）

### 5.1 体力值约束

```cpp
class StaminaConstraints {
    public:
        // 体力值范围约束
        static float clampStamina(float stamina) {
            return clamp(stamina, MIN_STAMINA, MAX_STAMINA);
        }
        
        // 精疲力尽检查
        static bool isExhausted(float stamina) {
            return stamina <= EXHAUSTION_THRESHOLD;
        }
        
        // Sprint启用检查
        static bool canSprint(float stamina) {
            return stamina >= SPRINT_ENABLE_THRESHOLD;
        }
};
```

### 5.2 速度约束

```cpp
class SpeedConstraints {
    public:
        // 速度范围约束
        static float clampSpeed(float speed) {
            return clamp(speed, 0.0, GAME_MAX_SPEED);
        }
        
        // 精疲力尽速度约束
        static float applyExhaustionSpeed(float speed, float stamina) {
            if (isExhausted(stamina)) {
                return EXHAUSTION_LIMP_SPEED; // 强制1.0 m/s
            }
            return speed;
        }
        
        // Sprint速度约束
        static float applySprintSpeed(float speed, float stamina, bool isSprinting) {
            if (isSprinting && !canSprint(stamina)) {
                // 禁用Sprint，强制切换到Run速度
                return speed * (1.0 / (1.0 + SPRINT_SPEED_BOOST));
            }
            return speed;
        }
};
```

### 5.3 坡度约束

```cpp
class GradeConstraints {
    public:
        // 坡度百分比范围约束
        static float clampGradePercent(float gradePercent) {
            return clamp(gradePercent, -100.0, 100.0);
        }
        
        // 坡度修正乘数约束
        static float clampGradeMultiplier(float multiplier) {
            return clamp(multiplier, 0.5, 5.0); // 限制在0.5-5.0倍
        }
};
```

### 5.4 负重约束

```cpp
class EncumbranceConstraints {
    public:
        // 负重重量范围约束
        static float clampEncumbranceWeight(float weight) {
            return max(weight, 0.0); // 不能为负
        }
        
        // 有效负重计算（减去基准重量）
        static float calculateEffectiveWeight(float totalWeight) {
            return max(totalWeight - BASE_WEIGHT, 0.0);
        }
        
        // 体重百分比约束
        static float clampBodyMassPercent(float percent) {
            return clamp(percent, 0.0, 1.0); // 0-100%
        }
};
```

## 6. 状态机实现（State Machine Implementation）

### 6.1 状态机类

```cpp
class StaminaStateMachine {
    private:
        StaminaSystem* system;
        MovementState currentState;
        
    public:
        // 构造函数
        StaminaStateMachine(StaminaSystem* system);
        
        // 状态更新
        void update(float deltaTime);
        
        // 状态转换
        void transitionTo(MovementState newState);
        
        // 状态查询
        MovementState getCurrentState();
        
        // 状态转换逻辑
        MovementState determineNextState(float velocity, float stamina);
};
```

### 6.2 状态转换逻辑

```cpp
MovementState StaminaStateMachine::determineNextState(
    float velocity, 
    float stamina) {
    
    // 精疲力尽状态优先
    if (stamina <= EXHAUSTION_THRESHOLD) {
        return MovementState::EXHAUSTED;
    }
    
    // 根据速度确定状态
    if (velocity >= SPRINT_VELOCITY_THRESHOLD) {
        // 检查是否可以Sprint
        if (stamina >= SPRINT_ENABLE_THRESHOLD) {
            return MovementState::SPRINT;
        } else {
            // 体力不足，强制切换到Run
            return MovementState::RUN;
        }
    } else if (velocity >= RUN_VELOCITY_THRESHOLD) {
        return MovementState::RUN;
    } else if (velocity >= WALK_VELOCITY_THRESHOLD) {
        return MovementState::WALK;
    } else {
        return MovementState::REST;
    }
}
```

## 7. 完整系统流程（Complete System Flow）

### 7.1 更新流程

```cpp
void StaminaSystem::update(float deltaTime) {
    // 1. 获取当前状态
    MovementState currentState = determineState(velocity, stamina);
    
    // 2. 应用约束条件
    stamina = StaminaConstraints::clampStamina(stamina);
    velocity = SpeedConstraints::applyExhaustionSpeed(velocity, stamina);
    velocity = SpeedConstraints::applySprintSpeed(velocity, stamina, 
                                                   currentState == SPRINT);
    
    // 3. 计算消耗率或恢复率
    float rate = 0.0;
    if (velocity > 0.05) {
        // 移动时：计算消耗率
        float baseDrain = DrainRateCalculator::calculateBaseDrainRate(velocity);
        float gradeMultiplier = DrainRateCalculator::calculateGradeMultiplier(
            gradePercent);
        float efficiencyFactor = healthStatus.calculateEfficiencyFactor() * 
                                metabolicAdaptation.calculateEfficiencyFactor(
                                    velocity / GAME_MAX_SPEED);
        float fatigueFactor = fatigueState.calculateFatigueFactor();
        float sprintMultiplier = (currentState == SPRINT) ? 
                                 SPRINT_STAMINA_DRAIN_MULTIPLIER : 1.0;
        
        rate = DrainRateCalculator::calculateTotalDrainRate(
            baseDrain, gradeMultiplier, efficiencyFactor, 
            fatigueFactor, sprintMultiplier);
    } else {
        // 静止时：计算恢复率
        rate = RecoveryRateCalculator::calculateRecoveryRate(
            stamina, recoveryModel.getRestDuration(), 
            fatigueState.getExerciseDuration(),
            healthStatus.getAge(), healthStatus.getFitnessLevel());
    }
    
    // 4. 更新体力值
    stamina += rate * deltaTime;
    stamina = StaminaConstraints::clampStamina(stamina);
    
    // 5. 更新疲劳状态
    fatigueState.update(deltaTime, velocity > 0.05);
    
    // 6. 更新恢复模型
    recoveryModel.update(deltaTime, velocity > 0.05);
}
```

## 8. 类关系图（Class Diagram）

```
┌─────────────────────────────────────────────────────────────┐
│                    StaminaSystem                             │
│  ────────────────────────────────────────────────────────   │
│  - stamina: float                                           │
│  - velocity: float                                          │
│  - gradePercent: float                                       │
│  - encumbranceWeight: float                                  │
│  - currentState: MovementState                              │
│  ────────────────────────────────────────────────────────   │
│  + update(deltaTime: float)                                 │
│  + calculateTotalDrainRate(): float                        │
│  + calculateRecoveryRate(): float                          │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ uses
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│ HealthStatus │   │ FatigueState │   │ RecoveryModel │
│              │   │              │   │              │
│ + fitness    │   │ + duration   │   │ + restDur    │
│ + age        │   │ + factor     │   │ + recovery   │
└──────────────┘   └──────────────┘   └──────────────┘
        │                   │                   │
        │                   │                   │
        └───────────────────┼───────────────────┘
                            │
                            ▼
                ┌───────────────────────┐
                │ MetabolicAdaptation    │
                │                       │
                │ + zone                │
                │ + efficiencyFactor    │
                └───────────────────────┘
```

## 9. 使用示例（Usage Example）

```cpp
// 创建体力系统实例
StaminaSystem* staminaSystem = new StaminaSystem();

// 设置初始状态
staminaSystem->setVelocity(3.7);  // Run速度
staminaSystem->setGradePercent(5.0);  // 5%上坡
staminaSystem->setEncumbranceWeight(30.0);  // 30kg负重

// 游戏循环中更新
while (gameRunning) {
    float deltaTime = getDeltaTime();
    
    // 更新体力系统
    staminaSystem->update(deltaTime);
    
    // 获取当前状态
    MovementState state = staminaSystem->getCurrentState();
    
    // 根据状态调整游戏逻辑
    if (state == MovementState::EXHAUSTED) {
        // 精疲力尽：强制跛行速度
        setMaxSpeed(EXHAUSTION_LIMP_SPEED);
    } else if (state == MovementState::SPRINT) {
        // Sprint：应用Sprint速度
        if (staminaSystem->canSprint()) {
            setMaxSpeed(calculateSprintSpeed());
        } else {
            // 体力不足，强制切换到Run
            setMaxSpeed(calculateRunSpeed());
        }
    }
    
    // 渲染体力条
    renderStaminaBar(staminaSystem->getStamina());
}
```

## 10. 总结

本文档采用面向对象编程的逻辑结构，定义了：

1. **核心类**：StaminaSystem、HealthStatus、FatigueState、RecoveryModel等
2. **状态枚举**：MovementState、MetabolicZone
3. **转换公式**：消耗率计算、恢复率计算、速度计算
4. **约束条件**：体力值约束、速度约束、坡度约束、负重约束
5. **状态机**：状态转换逻辑和状态机实现

该设计提供了清晰的架构，便于理解、维护和扩展。
