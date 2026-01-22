# Realistic Stamina System (RSS) - 多目标优化系统 v3.8.0
## 完整使用指南

## 📋 目录

1. [系统概述](#系统概述)
2. [快速开始](#快速开始)
3. [核心模块说明](#核心模块说明)
4. [优化流程](#优化流程)
5. [可视化工具](#可视化工具)
6. [敏感性分析](#敏感性分析)
7. [报告生成](#报告生成)
8. [配置文件说明](#配置文件说明)
9. [常见问题](#常见问题)

---

## 系统概述

RSS 多目标优化系统是一个基于仿真的参数优化工具，用于平衡 Arma Reforger 模组的"拟真度"与"可玩性"。

### 核心特性

- ✅ **数字孪生仿真器**：完全复刻 EnforceScript 的体力系统逻辑
- ✅ **标准测试工况库**：13 个标准测试工况
- ✅ **多目标优化**：支持 NSGA-II 和 Optuna 贝叶斯优化
- ✅ **可视化工具集**：5 种专业可视化图表
- ✅ **参数敏感性分析**：局部/全局/交互效应分析
- ✅ **报告生成器**：自动生成 HTML 和 Markdown 报告

### 技术亮点

- **科学性**：基于医学模型（Pandolf、Givoni-Goldman）
- **效率**：200 次采样 vs 5000 次评估（25x 更快）
- **可解释性**：参数敏感度分析
- **灵活性**：3 种优化配置方案
- **工业级**：使用 Optuna 贝叶斯优化

---

## 快速开始

### 1. 安装依赖

```bash
pip install -r requirements.txt
```

### 2. 运行优化

#### 使用 Optuna 贝叶斯优化（推荐）

```bash
python rss_optimizer_optuna.py
```

#### 使用 NSGA-II 遗传算法

```bash
python rss_optimizer.py
```

### 3. 查看结果

优化完成后，会生成 3 个配置文件：

- `optimized_rss_config_balanced_optuna.json` - 平衡型配置
- `optimized_rss_config_realism_optuna.json` - 拟真优先配置
- `optimized_rss_config_playability_optuna.json` - 可玩性优先配置

### 4. 应用配置

将生成的 JSON 配置文件复制到 Arma Reforger 的服务器配置目录：

```
$profile:RealisticStaminaSystem.json
```

或在游戏内通过配置系统加载。

---

## 核心模块说明

### 1. 数字孪生仿真器（rss_digital_twin.py）

**功能**：完全复刻 EnforceScript 的体力系统逻辑

**核心方法**：

```python
from rss_digital_twin import RSSDigitalTwin, MovementType, Stance, RSSConstants

# 创建常量对象
constants = RSSConstants()

# 创建仿真器
twin = RSSDigitalTwin(constants)

# 运行仿真
result = twin.simulate(
    speed_profile=[(0, 3.5), (100, 3.5)],  # 速度配置
    current_weight=90.0,  # 当前重量（kg）
    grade_percent=0.0,  # 坡度（%）
    terrain_factor=1.0,  # 地形因子
    stance=Stance.STAND,  # 姿态
    movement_type=MovementType.RUN  # 运动类型
)
```

**输出**：

```python
{
    'final_stamina': 0.5,  # 最终体力（0-1）
    'min_stamina': 0.0,  # 最低体力
    'max_stamina': 1.0,  # 最高体力
    'total_distance': 350.0,  # 总距离（米）
    'average_speed': 3.5,  # 平均速度（m/s）
    'movement_duration': 100.0,  # 运动持续时间（秒）
    'rest_duration': 0.0,  # 休息持续时间（秒）
    'fatigue_level': 0.3,  # 疲劳水平（0-1）
    'total_energy_expenditure': 450.0  # 总能量消耗（kcal）
}
```

### 2. 标准测试工况库（rss_scenarios.py）

**功能**：定义标准测试工况

**可用工况**：

```python
from rss_scenarios import ScenarioLibrary

# ACFT 2英里测试
scenario = ScenarioLibrary.create_acft_2mile_scenario(load_weight=15.0)

# Everon 拉练
scenario = ScenarioLibrary.create_everon_ruck_march_scenario(
    load_weight=20.0, slope_degrees=10.0, distance=500.0
)

# 火力突击
scenario = ScenarioLibrary.create_fire_assault_scenario(
    load_weight=30.0, sprint_duration=5.0, rest_duration=10.0, cycles=6
)

# 趴姿恢复测试
scenario = ScenarioLibrary.create_prone_recovery_scenario(
    load_weight=20.0, recovery_duration=60.0
)

# 游泳体力测试
scenario = ScenarioLibrary.create_swimming_scenario(
    load_weight=10.0, swimming_duration=300.0, swimming_speed=1.0
)

# 环境因子测试（热应激）
scenario = ScenarioLibrary.create_environmental_heat_stress_scenario(
    load_weight=20.0, temperature_celsius=35.0, march_duration=600.0, march_speed=2.5
)

# 长时间耐力测试
scenario = ScenarioLibrary.create_long_endurance_scenario(
    load_weight=15.0, distance=10000.0, march_speed=2.0
)

# 创建标准测试套件
scenarios = ScenarioLibrary.create_standard_test_suite()
```

### 3. 多目标优化器

#### Optuna 贝叶斯优化器（rss_optimizer_optuna.py）

**功能**：使用 Optuna 贝叶斯优化进行多目标优化

**核心方法**：

```python
from rss_optimizer_optuna import RSSOptunaOptimizer

# 创建优化器
optimizer = RSSOptunaOptimizer(n_trials=200)

# 执行优化
results = optimizer.optimize(study_name="rss_optimization")

# 分析敏感度
sensitivity = optimizer.analyze_sensitivity()

# 选择最优解（平衡型）
balanced_solution = optimizer.select_solution(
    realism_weight=0.5,
    playability_weight=0.5
)

# 导出配置
optimizer.export_to_json(
    balanced_solution['params'],
    "optimized_rss_config_balanced.json"
)
```

**参数**：

- `n_trials`：采样次数（默认 200）
- `scenarios`：测试工况列表（默认使用标准测试套件）

#### NSGA-II 遗传算法优化器（rss_optimizer.py）

**功能**：使用 NSGA-II 遗传算法进行多目标优化

**核心方法**：

```python
from rss_optimizer import RSSOptimizer

# 创建优化器
optimizer = RSSOptimizer(
    pop_size=50,  # 种群大小
    n_gen=100      # 进化代数
)

# 执行优化
results = optimizer.optimize()

# 分析敏感度
sensitivity = optimizer.analyze_sensitivity()

# 选择最优解
balanced_solution = optimizer.select_solution(
    realism_weight=0.5,
    playability_weight=0.5
)

# 导出配置
optimizer.export_to_json(
    balanced_solution['params'],
    "optimized_rss_config_balanced.json"
)
```

**参数**：

- `pop_size`：种群大小（默认 50）
- `n_gen`：进化代数（默认 100）
- `seed`：随机种子（默认 1）

---

## 优化流程

### 完整优化流程

1. **定义目标函数**：
   - 目标 1：拟真度损失（Realism Loss）- 越小越好
   - 目标 2：可玩性负担（Playability Burden）- 越小越好

2. **定义搜索空间**：
   - 13 个可优化参数
   - 每个参数都有明确的上下界

3. **运行优化**：
   - 使用 Optuna 贝叶斯优化（推荐）
   - 或使用 NSGA-II 遗传算法

4. **分析结果**：
   - 查看帕累托前沿
   - 分析参数敏感度
   - 选择最优解

5. **生成报告**：
   - 使用可视化工具生成图表
   - 使用报告生成器生成报告

### 优化算法对比

| 特性 | NSGA-II | Optuna |
|------|---------|---------|
| 采样次数 | 5000 | 200 |
| 优化时间 | ~10 分钟 | ~47 秒 |
| 帕累托前沿解数量 | 50 | 86 |
| 解空间多样性 | 低 | 高 |
| 推荐场景 | 小规模问题 | 大规模问题 |

---

## 可视化工具

### 1. 帕累托前沿散点图

**功能**：显示拟真度 vs 可玩性的权衡

**使用方法**：

```python
from rss_visualization import RSSVisualizer

# 创建可视化工具
visualizer = RSSVisualizer()

# 绘制帕累托前沿
visualizer.plot_pareto_front(
    pareto_front=pareto_front,  # 帕累托前沿（n x 2 数组）
    pareto_set=pareto_set,  # 帕累托解集（n x m 数组）
    title="帕累托前沿 - 拟真度 vs 可玩性",
    filename="pareto_front.png"
)
```

### 2. 参数敏感度热力图

**功能**：显示每个参数的变异系数

**使用方法**：

```python
# 绘制参数敏感度热力图
visualizer.plot_sensitivity_heatmap(
    sensitivity=sensitivity,  # 参数敏感度字典 {param_name: cv}
    title="参数敏感度分析",
    filename="sensitivity_heatmap.png"
)
```

### 3. 收敛曲线图

**功能**：显示优化过程中的目标函数变化

**使用方法**：

```python
# 绘制收敛曲线
visualizer.plot_convergence_curve(
    trials_history=trials_history,  # 试验历史列表
    title="优化收敛曲线",
    filename="convergence_curve.png"
)
```

### 4. 参数分布图

**功能**：显示帕累托前沿上参数的分布

**使用方法**：

```python
# 绘制参数分布图
visualizer.plot_parameter_distribution(
    pareto_set=pareto_set,  # 帕累托解集（n x m 数组）
    param_names=param_names,  # 参数名称列表
    title="帕累托前沿参数分布",
    filename="parameter_distribution.png"
)
```

### 5. 雷达图

**功能**：对比不同配置方案的参数

**使用方法**：

```python
# 绘制雷达图
configs = [
    {'name': '平衡型配置', 'params': {...}},
    {'name': '拟真优先配置', 'params': {...}},
    {'name': '可玩性优先配置', 'params': {...}}
]

visualizer.plot_radar_chart(
    configs=configs,
    param_names=param_names,
    title="配置方案对比",
    filename="radar_chart.png"
)
```

### 生成所有可视化图表

```python
# 生成所有可视化图表
visualizer.generate_all_visualizations(
    study=study,  # Optuna 研究对象
    param_names=param_names,  # 参数名称列表
    configs=configs  # 配置列表（可选）
)
```

---

## 敏感性分析

### 1. 局部敏感性分析（One-at-a-time）

**功能**：分析单个参数对目标函数的影响

**使用方法**：

```python
from rss_sensitivity_analysis import RSSSensitivityAnalyzer

# 创建敏感性分析工具
analyzer = RSSSensitivityAnalyzer()

# 定义基础参数
base_params = {
    'energy_to_stamina_coeff': 4.15e-05,
    'base_recovery_rate': 4.67e-04,
    # ... 其他参数
}

# 定义参数范围
param_ranges = {
    'energy_to_stamina_coeff': (2e-5, 5e-5),
    'base_recovery_rate': (1e-4, 5e-4),
    # ... 其他参数范围
}

# 定义目标函数
def objective_func(params):
    # 计算目标函数值
    return objective_value

# 执行局部敏感性分析
local_sensitivity = analyzer.local_sensitivity_analysis(
    base_params=base_params,
    param_names=param_names,
    param_ranges=param_ranges,
    objective_func=objective_func,
    filename="local_sensitivity.png"
)
```

### 2. 全局敏感性分析

**功能**：分析参数与目标函数的相关性

**使用方法**：

```python
# 定义参数样本
param_samples = {
    'energy_to_stamina_coeff': np.random.normal(base_params['energy_to_stamina_coeff'], base_params['energy_to_stamina_coeff'] * 0.1, 100),
    # ... 其他参数样本
}

# 定义目标函数值
objective_values = np.array([objective_func(base_params) for _ in range(100)])

# 执行全局敏感性分析
global_sensitivity = analyzer.global_sensitivity_analysis(
    param_samples=param_samples,
    objective_values=objective_values,
    filename="global_sensitivity.png"
)
```

### 3. 交互效应分析

**功能**：分析参数之间的交互效应

**使用方法**：

```python
# 定义参数对
param_pairs = [
    ('standing_recovery_multiplier', 'prone_recovery_multiplier'),
    ('encumbrance_speed_penalty_coeff', 'encumbrance_stamina_drain_coeff'),
    ('fatigue_accumulation_coeff', 'fatigue_max_factor')
]

# 执行交互效应分析
interaction_analysis = analyzer.interaction_analysis(
    base_params=base_params,
    param_pairs=param_pairs,
    objective_func=objective_func,
    filename="interaction_analysis.png"
)
```

### 生成敏感性分析报告

```python
# 生成敏感性分析报告
analyzer.generate_sensitivity_report(
    local_sensitivity=local_sensitivity,
    global_sensitivity=global_sensitivity,
    interaction_analysis=interaction_analysis
)
```

---

## 报告生成

### 1. HTML 报告

**功能**：生成美观的 HTML 格式报告

**使用方法**：

```python
from rss_report_generator import RSSReportGenerator

# 创建报告生成器
generator = RSSReportGenerator()

# 生成 HTML 报告
html_path = generator.generate_html_report(
    optimization_results=optimization_results,  # 优化结果字典
    sensitivity_results=sensitivity_results,  # 参数敏感度分析结果
    configs=configs,  # 配置列表
    filename="optimization_report.html"
)
```

### 2. Markdown 报告

**功能**：生成 Markdown 格式报告

**使用方法**：

```python
# 生成 Markdown 报告
md_path = generator.generate_markdown_report(
    optimization_results=optimization_results,
    sensitivity_results=sensitivity_results,
    configs=configs,
    filename="optimization_report.md"
)
```

### 生成所有报告

```python
# 生成所有报告
generator.generate_all_reports(
    optimization_results=optimization_results,
    sensitivity_results=sensitivity_results,
    configs=configs
)
```

---

## 配置文件说明

### 配置文件结构

```json
{
  "version": "3.0.0",
  "description": "RSS 多目标优化配置（Optuna 贝叶斯优化）",
  "optimization_method": "Optuna (TPE)",
  "n_trials": 200,
  "n_scenarios": 13,
  "parameters": {
    "energy_to_stamina_coeff": 4.152464528479495e-05,
    "base_recovery_rate": 0.00046704938132892816,
    "standing_recovery_multiplier": 2.2600292934258603,
    "prone_recovery_multiplier": 2.7476261364912338,
    "load_recovery_penalty_coeff": 0.0002718580266574055,
    "load_recovery_penalty_exponent": 1.1100531256342978,
    "encumbrance_speed_penalty_coeff": 0.289763562741042,
    "encumbrance_stamina_drain_coeff": 1.8053851496127151,
    "sprint_stamina_drain_multiplier": 2.891974099614622,
    "fatigue_accumulation_coeff": 0.02990077103195176,
    "fatigue_max_factor": 2.9045958179399713,
    "aerobic_efficiency_factor": 0.9299799380976288,
    "anaerobic_efficiency_factor": 1.0016889041932973
  }
}
```

### 参数说明

| 参数 | 默认值 | 范围 | 说明 |
|------|---------|------|------|
| energy_to_stamina_coeff | 3.5e-05 | [2e-5, 5e-5] | 能量转换系数 |
| base_recovery_rate | 3.0e-04 | [1e-4, 5e-4] | 基础恢复率 |
| standing_recovery_multiplier | 2.0 | [1.0, 3.0] | 站姿恢复倍数 |
| prone_recovery_multiplier | 2.2 | [1.5, 3.0] | 趴姿恢复倍数 |
| load_recovery_penalty_coeff | 5.0e-04 | [1e-4, 1e-3] | 负重恢复惩罚系数 |
| load_recovery_penalty_exponent | 2.0 | [1.0, 3.0] | 负重恢复惩罚指数 |
| encumbrance_speed_penalty_coeff | 0.2 | [0.1, 0.3] | 负重速度惩罚系数 |
| encumbrance_stamina_drain_coeff | 1.5 | [1.0, 2.0] | 负重消耗系数 |
| sprint_stamina_drain_multiplier | 3.0 | [2.0, 4.0] | Sprint消耗倍数 |
| fatigue_accumulation_coeff | 0.015 | [0.005, 0.03] | 疲劳累积系数 |
| fatigue_max_factor | 2.0 | [1.5, 3.0] | 最大疲劳因子 |
| aerobic_efficiency_factor | 0.9 | [0.8, 1.0] | 有氧效率因子 |
| anaerobic_efficiency_factor | 1.2 | [1.0, 1.5] | 无氧效率因子 |

### 配置方案推荐

| 服务器类型 | 推荐配置 | 说明 |
|----------|----------|------|
| 硬核 Milsim 服务器 | 拟真优先配置 | 追求医学精确度 |
| 公共服务器 | 平衡型配置 | 平衡拟真与可玩性 |
| 休闲服务器 | 可玩性优先配置 | 降低体力系统负担 |

---

## 常见问题

### Q1: 如何选择优化算法？

**A**: 
- **Optuna**：推荐用于大多数场景，效率高，解空间多样性好
- **NSGA-II**：适用于小规模问题，但容易陷入"局部平原"

### Q2: 如何调整优化参数？

**A**: 
- **增加采样次数**：修改 `n_trials` 参数（默认 200）
- **增加种群大小**：修改 `pop_size` 参数（默认 50）
- **增加进化代数**：修改 `n_gen` 参数（默认 100）

### Q3: 如何自定义测试工况？

**A**: 
1. 在 `rss_scenarios.py` 中创建新的测试工况
2. 使用 `ScenarioLibrary.create_standard_test_suite()` 添加到标准套件
3. 运行优化器时传入自定义工况列表

### Q4: 如何解释参数敏感度？

**A**: 
- **高敏感度**（CV > 0.3）：对目标函数影响大，优先调整
- **中敏感度**（0.1 < CV <= 0.3）：对目标函数有中等影响
- **低敏感度**（CV <= 0.1）：对目标函数影响小，可以固定为默认值

### Q5: 如何应用优化后的配置？

**A**: 
1. 将生成的 JSON 配置文件复制到 Arma Reforger 的服务器配置目录
2. 或在游戏内通过配置系统加载
3. 重启服务器使配置生效

### Q6: 优化时间太长怎么办？

**A**: 
1. 减少采样次数（从 200 降到 100）
2. 减少测试工况数量（从 13 降到 8）
3. 使用更快的采样器（Optuna TPE）
4. 使用并行优化（Optuna 支持分布式优化）

### Q7: 可视化图表显示乱码怎么办？

**A**: 
1. 安装中文字体（SimHei、Microsoft YaHei）
2. 修改 `rss_visualization.py` 中的字体设置
3. 使用英文标签代替中文标签

---

## 📚 参考文献

1. **Pandolf, K. B., et al. (1977)**. Predicting energy expenditure with loads while standing or walking very slowly. *Journal of Applied Physiology*, 43(4), 577-581.

2. **Palumbo, M. C., et al. (2018)**. Personalizing physical exercise in a computational model of fuel homeostasis. *PLOS Computational Biology*, 14(4), e1006073.

3. **Deb, K., et al. (2002)**. A fast and elitist multiobjective genetic algorithm: NSGA-II. *IEEE Transactions on Evolutionary Computation*, 6(2), 182-197.

4. **Akiba, T., et al. (2019)**. Optuna: A Next-generation Hyperparameter Optimization Framework. *Proceedings of the 25th ACM SIGKDD Conference on Knowledge Discovery and Data Mining*, 2620-2629.

---

## 📞 联系方式

**作者**：ViVi141 (747384120@qq.com)
**许可证**：AGPL-3.0
**版本**：v3.8.0
**日期**：2026-01-22

---

**祝使用愉快！** 🎉
