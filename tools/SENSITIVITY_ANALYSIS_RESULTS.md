# 敏感性分析结果说明

## 概述

本文档说明当前敏感性分析结果为 0.0000 的情况，以及后续改进建议。

## 当前结果

### 敏感性分析结果（v3.6.1）

| 参数 | 敏感性 | 相关系数 | 评级 |
|------|--------|----------|------|
| base_recovery_rate | 0.0000 | 0.0000 | Low / Weak |
| standing_recovery_multiplier | 0.0000 | 0.0000 | Low / Weak |
| prone_recovery_multiplier | 0.0000 | 0.0000 | Low / Weak |
| load_recovery_penalty_coeff | 0.0000 | 0.0000 | Low / Weak |
| encumbrance_stamina_drain_coeff | 0.0000 | 0.0000 | Low / Weak |

### 交互效应分析结果

| 参数对 | 交互强度 | 相对变化 | 评级 |
|--------|----------|----------|------|
| standing_recovery_multiplier vs prone_recovery_multiplier | 0.0000 | 0.00% | Weak |
| base_recovery_rate vs standing_recovery_multiplier | 0.0000 | 0.00% | Weak |
| load_recovery_penalty_coeff vs encumbrance_stamina_drain_coeff | 0.0000 | 0.00% | Weak |
| fatigue_accumulation_coeff vs fatigue_max_factor | 0.0000 | 0.00% | Weak |
| base_recovery_rate vs load_recovery_penalty_coeff | 0.0000 | 0.00% | Weak |
| standing_recovery_multiplier vs encumbrance_stamina_drain_coeff | 0.0000 | 0.00% | Weak |
| sprint_stamina_drain_multiplier vs aerobic_efficiency_factor | 0.0000 | 0.00% | Weak |
| encumbrance_stamina_drain_coeff vs anaerobic_efficiency_factor | 0.0000 | 0.00% | Weak |
| fatigue_accumulation_coeff vs aerobic_efficiency_factor | 0.0000 | 0.00% | Weak |

## 原因分析

### 1. 场景过于简单

当前使用的场景是 **ACFT 2英里测试**（ACFT 2 Mile Run）：

- **固定速度**：3.47 m/s（恒定）
- **固定负重**：90.0 kg（身体重量）
- **固定坡度**：0.0%（平地）
- **固定地形**：1.0（铺装路面）
- **固定姿态**：STAND（站立）
- **固定移动类型**：RUN（跑步）

**问题**：由于所有输入都是固定的，参数变化对仿真结果的影响被最小化了。

### 2. 目标函数设计问题

当前目标函数计算：

```python
# 1. 完成时间偏差（权重 40%）
time_error = abs(results['total_time'] - scenario.target_finish_time) / scenario.target_finish_time

# 2. 恢复时间偏差（权重 30%）
recovery_error = abs(results['rest_duration'] - scenario.target_recovery_time) / scenario.target_recovery_time

# 3. 最低体力偏差（权重 30%）
min_stamina = results.get('min_stamina', results.get('final_stamina', 1.0))
stamina_error = abs(min_stamina - scenario.target_min_stamina) if scenario.target_min_stamina > 0 else 0.0

# 综合目标函数（越小越好）
objective = time_error * 0.40 + recovery_error * 0.30 + stamina_error * 0.30
```

**问题**：
- 完成时间主要由速度决定，而速度是固定的
- 恢复时间在固定速度场景中变化很小
- 最低体力在固定速度场景中变化很小

### 3. 参数影响被抵消

某些参数的影响可能被其他参数或系统逻辑抵消：

- **恢复率参数**：在固定速度场景中，恢复率的影响被最小化
- **负重惩罚参数**：在固定负重场景中，负重惩罚的影响被最小化
- **疲劳参数**：在短时间场景中，疲劳累积的影响被最小化

## 改进建议

### 1. 使用更复杂的场景

#### 建议 1：使用 Everon 拉练场景

```python
scenario = ScenarioLibrary.create_everon_ruck_march_scenario(
    load_weight=20.0,
    slope_degrees=10.0,
    distance=500.0
)
```

**优势**：
- 包含负重变化（20 kg）
- 包含坡度变化（10 度）
- 包含距离变化（500 米）

#### 建议 2：使用游泳测试场景

```python
scenario = ScenarioLibrary.create_swim_test_scenario(
    distance=100.0,
    water_temperature=20.0
)
```

**优势**：
- 包含水环境因素
- 包含温度因素
- 包含游泳特殊消耗

#### 建议 3：创建复合场景

```python
# 创建复合场景：包含速度变化、负重变化、坡度变化
speed_profile = [
    (0, 2.5),      # 起步：2.5 m/s
    (100, 3.7),    # 加速：3.7 m/s
    (200, 5.2),    # 冲刺：5.2 m/s
    (300, 3.7),    # 减速：3.7 m/s
    (400, 2.5),    # 慢跑：2.5 m/s
    (500, 0.0)     # 停止：0.0 m/s
]

scenario = TestScenario(
    name="复合测试场景",
    description="包含速度变化、负重变化、坡度变化的复合场景",
    scenario_type=ScenarioType.CUSTOM,
    speed_profile=speed_profile,
    current_weight=110.0,  # 身体重量 + 20 kg 负重
    grade_percent=5.0,  # 5% 坡度
    terrain_factor=1.2,  # 草地
    stance=Stance.STAND,
    movement_type=MovementType.RUN,
    target_finish_time=500.0,
    target_min_stamina=0.1,
    target_recovery_time=180.0,
    max_recovery_time=300.0,
    max_rest_ratio=0.10
)
```

### 2. 增加更多测试工况

#### 建议 1：多场景敏感性分析

```python
scenarios = [
    ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0),
    ScenarioLibrary.create_everon_ruck_march_scenario(load_weight=20.0),
    ScenarioLibrary.create_swim_test_scenario(distance=100.0),
    custom_scenario
]

for scenario in scenarios:
    sensitivity = analyzer.local_sensitivity_analysis(
        base_params=base_params,
        param_names=param_names,
        param_ranges=param_ranges,
        objective_func=lambda p: real_objective(p, scenario),
        filename=f"local_sensitivity_{scenario.name}.png"
    )
```

#### 建议 2：参数化场景敏感性分析

```python
# 测试不同负重下的敏感性
load_weights = [0.0, 10.0, 20.0, 30.0, 40.0]

for load_weight in load_weights:
    scenario = ScenarioLibrary.create_acft_2mile_scenario(load_weight=load_weight)
    sensitivity = analyzer.local_sensitivity_analysis(
        base_params=base_params,
        param_names=param_names,
        param_ranges=param_ranges,
        objective_func=lambda p: real_objective(p, scenario),
        filename=f"local_sensitivity_load_{load_weight}kg.png"
    )
```

### 3. 优化目标函数

#### 建议 1：增加更多指标

```python
def enhanced_objective(params, scenario):
    results = simulate_scenario(params, scenario)
    
    # 1. 完成时间偏差（权重 25%）
    time_error = abs(results['total_time'] - scenario.target_finish_time) / scenario.target_finish_time
    
    # 2. 恢复时间偏差（权重 20%）
    recovery_error = abs(results['rest_duration'] - scenario.target_recovery_time) / scenario.target_recovery_time
    
    # 3. 最低体力偏差（权重 20%）
    min_stamina = results.get('min_stamina', 1.0)
    stamina_error = abs(min_stamina - scenario.target_min_stamina)
    
    # 4. 平均速度偏差（权重 15%）
    avg_speed = results['total_distance'] / results['total_time']
    target_speed = scenario.speed_profile[0][1]
    speed_error = abs(avg_speed - target_speed) / target_speed
    
    # 5. 体力消耗率（权重 10%）
    stamina_drain_rate = (1.0 - results['final_stamina']) / results['total_time']
    target_drain_rate = (1.0 - scenario.target_min_stamina) / scenario.target_finish_time
    drain_error = abs(stamina_drain_rate - target_drain_rate) / target_drain_rate
    
    # 6. 负重惩罚（权重 10%）
    load_penalty = results.get('load_penalty', 0)
    expected_load_penalty = scenario.current_weight * 0.01
    load_error = abs(load_penalty - expected_load_penalty) / expected_load_penalty
    
    # 综合目标函数（越小越好）
    objective = (
        time_error * 0.25 +
        recovery_error * 0.20 +
        stamina_error * 0.20 +
        speed_error * 0.15 +
        drain_error * 0.10 +
        load_error * 0.10
    )
    
    return objective
```

#### 建议 2：使用非线性权重

```python
def nonlinear_objective(params, scenario):
    results = simulate_scenario(params, scenario)
    
    # 计算偏差
    time_error = abs(results['total_time'] - scenario.target_finish_time) / scenario.target_finish_time
    recovery_error = abs(results['rest_duration'] - scenario.target_recovery_time) / scenario.target_recovery_time
    stamina_error = abs(results.get('min_stamina', 1.0) - scenario.target_min_stamina)
    
    # 使用非线性权重，使目标函数对参数变化更敏感
    objective = (
        time_error ** 2 * 0.40 +  # 平方权重
        recovery_error ** 1.5 * 0.30 +  # 1.5 次幂权重
        stamina_error ** 1.5 * 0.30
    )
    
    return objective
```

### 4. 检查数字孪生仿真器

#### 建议 1：验证参数应用

```python
def verify_parameter_application(params):
    constants = RSSConstants()
    
    # 更新参数
    constants.BASE_RECOVERY_RATE = params.get('base_recovery_rate', constants.BASE_RECOVERY_RATE)
    
    # 验证参数是否正确应用
    assert constants.BASE_RECOVERY_RATE == params['base_recovery_rate'], "参数未正确应用"
    
    # 创建数字孪生实例
    twin = RSSDigitalTwin(constants=constants)
    
    # 验证数字孪生是否使用正确的参数
    assert twin.constants.BASE_RECOVERY_RATE == params['base_recovery_rate'], "数字孪生未使用正确参数"
    
    print("参数应用验证通过")
```

#### 建议 2：添加参数影响日志

```python
def log_parameter_impact(params):
    twin = RSSDigitalTwin()
    
    # 记录原始结果
    original_results = twin.simulate_scenario(scenario)
    
    # 更新参数
    twin.constants.BASE_RECOVERY_RATE = params['base_recovery_rate']
    
    # 记录修改后的结果
    modified_results = twin.simulate_scenario(scenario)
    
    # 计算影响
    impact = {
        'total_time': modified_results['total_time'] - original_results['total_time'],
        'rest_duration': modified_results['rest_duration'] - original_results['rest_duration'],
        'min_stamina': modified_results['min_stamina'] - original_results['min_stamina']
    }
    
    print(f"参数影响：{impact}")
    
    return impact
```

## 结论

当前敏感性分析结果为 0.0000 是由于：

1. **场景过于简单**：固定速度、固定负重、固定坡度
2. **目标函数设计问题**：目标函数对参数变化不敏感
3. **参数影响被抵消**：某些参数的影响被其他参数或系统逻辑抵消

**建议**：
1. 使用更复杂的场景（包含负重变化、坡度变化、速度变化）
2. 增加更多测试工况（如 Everon 拉练、游泳测试等）
3. 优化目标函数（增加更多指标、使用非线性权重）
4. 检查数字孪生仿真器（验证参数应用、添加参数影响日志）

## 参考资料

- [rss_sensitivity_analysis_enhanced.py](file:///c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\tools\rss_sensitivity_analysis_enhanced.py)
- [rss_digital_twin.py](file:///c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\tools\rss_digital_twin.py)
- [rss_scenarios.py](file:///c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\tools\rss_scenarios.py)
- [CHANGELOG.md](file:///c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\CHANGELOG.md)
