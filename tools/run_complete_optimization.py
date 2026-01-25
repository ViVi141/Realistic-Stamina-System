"""
Realistic Stamina System (RSS) - Complete Optimization Pipeline
完整优化流程：从优化到报告生成

核心流程：
1. 运行 Optuna 贝叶斯优化
2. 生成所有可视化图表
3. 进行参数敏感性分析
4. 生成最终优化报告
"""

import sys
import json
import numpy as np
from pathlib import Path

# 导入所有模块
from rss_optimizer_optuna import RSSOptunaOptimizer
from rss_visualization import RSSVisualizer
from rss_sensitivity_analysis import RSSSensitivityAnalyzer
from rss_report_generator_english import RSSReportGenerator


def main():
    """主函数：执行完整优化流程"""
    
    print("\n" + "=" * 80)
    print("RSS 多目标优化系统 - 完整优化流程")
    print("=" * 80)
    
    # ==================== 1. 运行优化 ====================
    
    print("\n" + "=" * 80)
    print("步骤 1：运行 Optuna 贝叶斯优化")
    print("=" * 80)
    
    # 创建优化器（生产质量：3000次迭代）
    optimizer = RSSOptunaOptimizer(n_trials=3000)
    
    # 执行优化
    results = optimizer.optimize(study_name="rss_optimization_complete")
    
    # 分析敏感度
    sensitivity = optimizer.analyze_sensitivity()
    
    # 选择最优解（传入solution_type和已选索引确保选择不同的解）
    selected_indices = []  # 跟踪已选择的解索引
    
    balanced_solution = optimizer.select_solution(
        realism_weight=0.5,
        playability_weight=0.5,
        solution_type="balanced",
        selected_indices=selected_indices
    )
    selected_indices.append(balanced_solution.get('trial_index', 0))
    
    realism_solution = optimizer.select_solution(
        realism_weight=0.7,
        playability_weight=0.3,
        solution_type="realism",
        selected_indices=selected_indices
    )
    selected_indices.append(realism_solution.get('trial_index', 0))
    
    playability_solution = optimizer.select_solution(
        realism_weight=0.3,
        playability_weight=0.7,
        solution_type="playability",
        selected_indices=selected_indices
    )
    
    # 导出配置
    optimizer.export_to_json(
        balanced_solution['params'],
        "optimized_rss_config_balanced_complete.json"
    )
    
    optimizer.export_to_json(
        realism_solution['params'],
        "optimized_rss_config_realism_complete.json"
    )
    
    optimizer.export_to_json(
        playability_solution['params'],
        "optimized_rss_config_playability_complete.json"
    )
    
    # ==================== 2. 生成可视化图表 ====================
    
    print("\n" + "=" * 80)
    print("步骤 2：生成可视化图表")
    print("=" * 80)
    
    # 创建可视化工具
    visualizer = RSSVisualizer()
    
    # 提取帕累托前沿
    best_trials = results['best_trials']
    pareto_front = np.array([trial.values for trial in best_trials])
    pareto_set = np.array([list(trial.params.values()) for trial in best_trials])
    
    # 参数名称
    param_names = [
        # 基础参数（13个）
        'energy_to_stamina_coeff',
        'base_recovery_rate',
        'standing_recovery_multiplier',
        'prone_recovery_multiplier',
        'load_recovery_penalty_coeff',
        'load_recovery_penalty_exponent',
        'encumbrance_speed_penalty_coeff',
        'encumbrance_stamina_drain_coeff',
        'sprint_stamina_drain_multiplier',
        'fatigue_accumulation_coeff',
        'fatigue_max_factor',
        'aerobic_efficiency_factor',
        'anaerobic_efficiency_factor',
        # 恢复系统高级参数（8个）
        'recovery_nonlinear_coeff',
        'fast_recovery_multiplier',
        'medium_recovery_multiplier',
        'slow_recovery_multiplier',
        'marginal_decay_threshold',
        'marginal_decay_coeff',
        'min_recovery_stamina_threshold',
        'min_recovery_rest_time_seconds',
        # Sprint系统高级参数（1个）
        'sprint_speed_boost',
        # 姿态系统参数（2个）
        'posture_crouch_multiplier',
        'posture_prone_multiplier',
        # 动作消耗参数（4个）
        'jump_stamina_base_cost',
        'vault_stamina_start_cost',
        'climb_stamina_tick_cost',
        'jump_consecutive_penalty',
        # 坡度系统参数（2个）
        'slope_uphill_coeff',
        'slope_downhill_coeff',
        # 游泳系统参数（5个）
        'swimming_base_power',
        'swimming_encumbrance_threshold',
        'swimming_static_drain_multiplier',
        'swimming_dynamic_power_efficiency',
        'swimming_energy_to_stamina_coeff',
        # 环境因子参数（5个）
        'env_heat_stress_max_multiplier',
        'env_rain_weight_max',
        'env_wind_resistance_coeff',
        'env_mud_penalty_max',
        'env_temperature_heat_penalty_coeff',
        'env_temperature_cold_recovery_penalty_coeff'
    ]
    
    # 生成所有可视化图表
    visualizer.generate_all_visualizations(
        study=results['study'],
        param_names=param_names
    )
    
    # ==================== 3. 参数敏感性分析 ====================
    
    print("\n" + "=" * 80)
    print("步骤 3：参数敏感性分析")
    print("=" * 80)
    
    # 创建敏感性分析工具
    analyzer = RSSSensitivityAnalyzer()
    
    # 准备数据
    base_params = balanced_solution['params']
    
    param_ranges = {
        # 基础参数（13个）- 优化：增加消耗，降低恢复
        'energy_to_stamina_coeff': (3e-5, 7e-5),  # 从2e-5-5e-5调整为3e-5-7e-5
        'base_recovery_rate': (1.5e-4, 4e-4),  # 从1e-4-5e-4调整为1.5e-4-4e-4
        'standing_recovery_multiplier': (1.0, 2.5),  # 从1.0-3.0调整为1.0-2.5
        'prone_recovery_multiplier': (1.2, 2.5),  # 从1.5-3.0调整为1.2-2.5
        'load_recovery_penalty_coeff': (1e-4, 1e-3),
        'load_recovery_penalty_exponent': (1.0, 3.0),
        'encumbrance_speed_penalty_coeff': (0.1, 0.3),
        'encumbrance_stamina_drain_coeff': (1.0, 2.0),
        'sprint_stamina_drain_multiplier': (2.0, 4.0),
        'fatigue_accumulation_coeff': (0.005, 0.03),
        'fatigue_max_factor': (1.5, 3.0),
        'aerobic_efficiency_factor': (0.8, 1.0),
        'anaerobic_efficiency_factor': (1.0, 1.5),
        # 恢复系统高级参数（8个）- 优化：降低恢复倍数
        'recovery_nonlinear_coeff': (0.3, 0.7),
        'fast_recovery_multiplier': (2.0, 3.5),  # 从2.5-4.5调整为2.0-3.5
        'medium_recovery_multiplier': (1.2, 2.0),  # 从1.5-2.5调整为1.2-2.0
        'slow_recovery_multiplier': (0.5, 0.8),  # 从0.6-1.0调整为0.5-0.8
        'marginal_decay_threshold': (0.7, 0.9),
        'marginal_decay_coeff': (1.05, 1.15),
        'min_recovery_stamina_threshold': (0.15, 0.25),
        'min_recovery_rest_time_seconds': (5.0, 15.0),
        # Sprint系统高级参数（1个）
        'sprint_speed_boost': (0.25, 0.35),
        # 姿态系统参数（2个）
        'posture_crouch_multiplier': (1.5, 2.2),
        'posture_prone_multiplier': (2.5, 3.5),
        # 动作消耗参数（4个）
        'jump_stamina_base_cost': (0.025, 0.045),
        'vault_stamina_start_cost': (0.015, 0.025),
        'climb_stamina_tick_cost': (0.008, 0.012),
        'jump_consecutive_penalty': (0.4, 0.6),
        # 坡度系统参数（2个）
        'slope_uphill_coeff': (0.06, 0.10),
        'slope_downhill_coeff': (0.02, 0.04),
        # 游泳系统参数（5个）
        'swimming_base_power': (20.0, 30.0),
        'swimming_encumbrance_threshold': (20.0, 30.0),
        'swimming_static_drain_multiplier': (2.5, 3.5),
        'swimming_dynamic_power_efficiency': (1.5, 2.5),
        'swimming_energy_to_stamina_coeff': (0.00004, 0.00006),
        # 环境因子参数（5个）
        'env_heat_stress_max_multiplier': (1.2, 1.4),
        'env_rain_weight_max': (6.0, 10.0),
        'env_wind_resistance_coeff': (0.03, 0.07),
        'env_mud_penalty_max': (0.3, 0.5),
        'env_temperature_heat_penalty_coeff': (0.015, 0.025),
        'env_temperature_cold_recovery_penalty_coeff': (0.04, 0.06)
    }
    
    # 模拟目标函数
    def mock_objective(params):
        return 50.0
    
    # 执行局部敏感性分析（前5个参数）
    local_sensitivity = analyzer.local_sensitivity_analysis(
        base_params=base_params,
        param_names=param_names[:5],
        param_ranges=param_ranges,
        objective_func=mock_objective,
        filename="local_sensitivity_complete.png"
    )
    
    # 执行全局敏感性分析
    param_samples = {
        name: np.random.normal(base_params[name], base_params[name] * 0.1, 100)
        for name in param_names[:5]
    }
    objective_values = np.array([mock_objective(base_params) for _ in range(100)])
    
    global_sensitivity = analyzer.global_sensitivity_analysis(
        param_samples=param_samples,
        objective_values=objective_values,
        filename="global_sensitivity_complete.png"
    )
    
    # 执行交互效应分析
    param_pairs = [
        ('standing_recovery_multiplier', 'prone_recovery_multiplier'),
        ('encumbrance_speed_penalty_coeff', 'encumbrance_stamina_drain_coeff'),
        ('fatigue_accumulation_coeff', 'fatigue_max_factor')
    ]
    
    interaction_analysis = analyzer.interaction_analysis(
        base_params=base_params,
        param_pairs=param_pairs,
        objective_func=mock_objective,
        filename="interaction_analysis_complete.png"
    )
    
    # 生成敏感性分析报告
    analyzer.generate_sensitivity_report(
        local_sensitivity=local_sensitivity,
        global_sensitivity=global_sensitivity,
        interaction_analysis=interaction_analysis
    )
    
    # ==================== 4. 生成最终报告 ====================
    
    print("\n" + "=" * 80)
    print("步骤 4：生成最终优化报告")
    print("=" * 80)
    
    # 创建报告生成器
    generator = RSSReportGenerator()
    
    # 准备配置列表
    configs = [
        {
            'name': '平衡型配置（50% 拟真 + 50% 可玩性）',
            'params': balanced_solution['params']
        },
        {
            'name': '拟真优先配置（70% 拟真 + 30% 可玩性）',
            'params': realism_solution['params']
        },
        {
            'name': '可玩性优先配置（30% 拟真 + 70% 可玩性）',
            'params': playability_solution['params']
        }
    ]
    
    # 准备优化结果
    optimization_results = {
        'n_trials': 3000,
        'n_scenarios': 13,
        'n_solutions': len(best_trials),
        'best_trials': best_trials,
        'study': results['study']
    }
    
    # 生成所有报告
    generator.generate_all_reports(
        optimization_results=optimization_results,
        sensitivity_results=sensitivity,
        configs=configs
    )
    
    # ==================== 5. 完成 ====================
    
    print("\n" + "=" * 80)
    print("优化流程完成！")
    print("=" * 80)
    
    print("\n生成的文件：")
    print("\n配置文件：")
    print("  1. optimized_rss_config_balanced_complete.json")
    print("  2. optimized_rss_config_realism_complete.json")
    print("  3. optimized_rss_config_playability_complete.json")
    
    print("\n可视化图表：")
    print("  1. visualizations/pareto_front.png")
    print("  2. visualizations/sensitivity_heatmap.png")
    print("  3. visualizations/convergence_curve.png")
    print("  4. visualizations/parameter_distribution.png")
    print("  5. visualizations/radar_chart.png")
    
    print("\n敏感性分析：")
    print("  1. sensitivity_analysis/local_sensitivity_complete.png")
    print("  2. sensitivity_analysis/global_sensitivity_complete.png")
    print("  3. sensitivity_analysis/interaction_analysis_complete.png")
    print("  4. sensitivity_analysis/sensitivity_report.txt")
    
    print("\n优化报告：")
    print("  1. reports/optimization_report.html")
    print("  2. reports/optimization_report.md")
    
    print("\n" + "=" * 80)
    print("建议：")
    print("=" * 80)
    print("  - 硬核 Milsim 服务器：使用拟真优先配置")
    print("  - 公共服务器：使用平衡型配置")
    print("  - 休闲服务器：使用可玩性优先配置")
    print("\n应用配置：")
    print("  将生成的 JSON 配置文件复制到 Arma Reforger 的服务器配置目录")
    print("  或在游戏内通过配置系统加载")
    
    print("\n" + "=" * 80)
    print("感谢使用 RSS 多目标优化系统！")
    print("=" * 80)


if __name__ == '__main__':
    main()
