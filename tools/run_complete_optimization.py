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
    
    # 创建优化器
    optimizer = RSSOptunaOptimizer(n_trials=200)
    
    # 执行优化
    results = optimizer.optimize(study_name="rss_optimization_complete")
    
    # 分析敏感度
    sensitivity = optimizer.analyze_sensitivity()
    
    # 选择最优解
    balanced_solution = optimizer.select_solution(
        realism_weight=0.5,
        playability_weight=0.5
    )
    
    realism_solution = optimizer.select_solution(
        realism_weight=0.7,
        playability_weight=0.3
    )
    
    playability_solution = optimizer.select_solution(
        realism_weight=0.3,
        playability_weight=0.7
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
        'anaerobic_efficiency_factor'
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
        'energy_to_stamina_coeff': (2e-5, 5e-5),
        'base_recovery_rate': (1e-4, 5e-4),
        'standing_recovery_multiplier': (1.0, 3.0),
        'prone_recovery_multiplier': (1.5, 3.0),
        'load_recovery_penalty_coeff': (1e-4, 1e-3),
        'load_recovery_penalty_exponent': (1.0, 3.0),
        'encumbrance_speed_penalty_coeff': (0.1, 0.3),
        'encumbrance_stamina_drain_coeff': (1.0, 2.0),
        'sprint_stamina_drain_multiplier': (2.0, 4.0),
        'fatigue_accumulation_coeff': (0.005, 0.03),
        'fatigue_max_factor': (1.5, 3.0),
        'aerobic_efficiency_factor': (0.8, 1.0),
        'anaerobic_efficiency_factor': (1.0, 1.5)
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
        'n_trials': 200,
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
