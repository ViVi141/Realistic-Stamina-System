#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS 增强型配置可靠性分析工具
分析 NSGA-II 优化生成的三个预设配置的可靠性和合理性
"""

import json
import numpy as np
from pathlib import Path
from typing import Dict, List, Tuple


def load_config(file_path: str) -> Dict:
    """加载JSON配置文件"""
    with open(file_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def compare_configs(configs: Dict[str, Dict]) -> Dict:
    """
    比较配置文件的参数差异
    
    Args:
        configs: 配置字典 {preset_name: config_dict}
    
    Returns:
        分析结果字典
    """
    # 提取所有参数名
    param_names = set()
    for config in configs.values():
        param_names.update(config['parameters'].keys())
    
    param_names = sorted(param_names)
    
    # 分析每个参数
    analysis = {
        'identical_params': [],  # 完全相同的参数
        'similar_params': [],    # 非常相似的参数（差异<1%）
        'different_params': [], # 有明显差异的参数
        'param_statistics': {},   # 参数统计信息
        'objective_comparison': {} # 目标函数比较
    }
    
    for param_name in param_names:
        values = [config['parameters'][param_name] for config in configs.values()]
        
        # 计算统计信息
        mean_val = np.mean(values)
        std_val = np.std(values)
        cv = std_val / mean_val if mean_val != 0 else 0.0  # 变异系数
        min_val = min(values)
        max_val = max(values)
        range_val = max_val - min_val
        
        analysis['param_statistics'][param_name] = {
            'mean': mean_val,
            'std': std_val,
            'cv': cv,
            'min': min_val,
            'max': max_val,
            'range': range_val,
            'values': values
        }
        
        # 分类参数
        if cv < 0.01:  # 变异系数<1%，视为完全相同
            analysis['identical_params'].append(param_name)
        elif cv < 0.05:  # 变异系数<5%，视为非常相似
            analysis['similar_params'].append(param_name)
        else:
            analysis['different_params'].append(param_name)
    
    # 比较目标函数
    for preset_name, config in configs.items():
        analysis['objective_comparison'][preset_name] = config['objectives']
    
    return analysis


def evaluate_parameter_reasonableness(configs: Dict[str, Dict]) -> Dict:
    """
    评估参数合理性
    
    Args:
        configs: 配置字典
    
    Returns:
        合理性评估结果
    """
    evaluation = {
        'warnings': [],
        'errors': [],
        'recommendations': []
    }
    
    # 定义合理范围（基于优化器搜索空间和物理常识，已更新以匹配新的搜索范围）
    reasonable_ranges = {
        'energy_to_stamina_coeff': (2.5e-5, 7e-5),  # 更新：从3e-5降低到2.5e-5
        'base_recovery_rate': (1.5e-4, 5e-4),  # 更新：从4e-4提高到5e-4
        'standing_recovery_multiplier': (1.0, 2.5),
        'prone_recovery_multiplier': (1.2, 2.5),
        'fast_recovery_multiplier': (2.0, 3.5),
        'medium_recovery_multiplier': (1.2, 2.0),
        'slow_recovery_multiplier': (0.5, 0.8),
        'sprint_stamina_drain_multiplier': (2.0, 4.0),
        'encumbrance_speed_penalty_coeff': (0.1, 0.3),
        'encumbrance_stamina_drain_coeff': (0.8, 2.0)  # 更新：从1.0降低到0.8
    }
    
    for preset_name, config in configs.items():
        params = config['parameters']
        objectives = config['objectives']
        
        # 检查参数是否在合理范围内
        for param_name, (min_val, max_val) in reasonable_ranges.items():
            if param_name in params:
                value = params[param_name]
                if value < min_val or value > max_val:
                    evaluation['warnings'].append(
                        f"{preset_name}.{param_name} = {value:.6e} 超出合理范围 [{min_val:.6e}, {max_val:.6e}]"
                    )
        
        # 检查目标函数值
        if objectives['playability_burden'] > 500:
            evaluation['warnings'].append(
                f"{preset_name}: playability_burden = {objectives['playability_burden']:.2f} 过高（30KG测试可能未通过）"
            )
        
        if objectives['stability_risk'] > 0:
            evaluation['errors'].append(
                f"{preset_name}: stability_risk = {objectives['stability_risk']:.2f} > 0（检测到BUG）"
            )
        
        # 检查消耗/恢复平衡
        energy_coeff = params['energy_to_stamina_coeff']
        recovery_rate = params['base_recovery_rate']
        consumption_recovery_ratio = energy_coeff / (recovery_rate + 1e-10)
        
        if consumption_recovery_ratio < 0.05:
            evaluation['warnings'].append(
                f"{preset_name}: 消耗/恢复比率过低 ({consumption_recovery_ratio:.4f})，可能消耗过少"
            )
        elif consumption_recovery_ratio > 0.3:
            evaluation['warnings'].append(
                f"{preset_name}: 消耗/恢复比率过高 ({consumption_recovery_ratio:.4f})，可能消耗过快"
            )
    
    return evaluation


def generate_analysis_report(configs: Dict[str, Dict], analysis: Dict, evaluation: Dict) -> str:
    """
    生成分析报告
    
    Args:
        configs: 配置字典
        analysis: 比较分析结果
        evaluation: 合理性评估结果
    
    Returns:
        报告文本
    """
    report = []
    report.append("=" * 80)
    report.append("RSS 增强型配置可靠性分析报告")
    report.append("=" * 80)
    report.append("")
    
    # 1. 目标函数比较
    report.append("## 1. 目标函数比较")
    report.append("-" * 80)
    report.append(f"{'预设':<20} {'拟真损失':<15} {'可玩性负担':<15} {'稳定性风险':<15}")
    report.append("-" * 80)
    for preset_name, objectives in analysis['objective_comparison'].items():
        report.append(
            f"{preset_name:<20} "
            f"{objectives['realism_loss']:<15.4f} "
            f"{objectives['playability_burden']:<15.2f} "
            f"{objectives['stability_risk']:<15.2f}"
        )
    report.append("")
    
    # 2. 参数差异分析
    report.append("## 2. 参数差异分析")
    report.append("-" * 80)
    report.append(f"完全相同的参数数量: {len(analysis['identical_params'])}")
    report.append(f"非常相似的参数数量: {len(analysis['similar_params'])}")
    report.append(f"有明显差异的参数数量: {len(analysis['different_params'])}")
    report.append("")
    
    if len(analysis['identical_params']) > 0:
        report.append("### 完全相同的参数（可能表示优化不足）:")
        for param in analysis['identical_params'][:10]:  # 只显示前10个
            stats = analysis['param_statistics'][param]
            report.append(f"  - {param}: {stats['mean']:.6e} (所有预设相同)")
        if len(analysis['identical_params']) > 10:
            report.append(f"  ... 还有 {len(analysis['identical_params']) - 10} 个参数完全相同")
        report.append("")
    
    if len(analysis['different_params']) > 0:
        report.append("### 有明显差异的参数（这是好的，表示预设有区别）:")
        for param in analysis['different_params'][:15]:  # 只显示前15个
            stats = analysis['param_statistics'][param]
            report.append(
                f"  - {param}: "
                f"范围=[{stats['min']:.6e}, {stats['max']:.6e}], "
                f"变异系数={stats['cv']:.4f}"
            )
        if len(analysis['different_params']) > 15:
            report.append(f"  ... 还有 {len(analysis['different_params']) - 15} 个参数有差异")
        report.append("")
    
    # 3. 关键参数分析
    report.append("## 3. 关键参数分析")
    report.append("-" * 80)
    key_params = [
        'energy_to_stamina_coeff',
        'base_recovery_rate',
        'standing_recovery_multiplier',
        'prone_recovery_multiplier',
        'fast_recovery_multiplier',
        'sprint_stamina_drain_multiplier',
        'encumbrance_stamina_drain_coeff'
    ]
    
    for param in key_params:
        if param in analysis['param_statistics']:
            stats = analysis['param_statistics'][param]
            report.append(f"\n{param}:")
            report.append(f"  平均值: {stats['mean']:.6e}")
            report.append(f"  范围: [{stats['min']:.6e}, {stats['max']:.6e}]")
            report.append(f"  变异系数: {stats['cv']:.4f} ({'相同' if stats['cv'] < 0.01 else '相似' if stats['cv'] < 0.05 else '不同'})")
            for preset_name, config in configs.items():
                value = config['parameters'][param]
                report.append(f"    {preset_name}: {value:.6e}")
    
    report.append("")
    
    # 4. 合理性评估
    report.append("## 4. 合理性评估")
    report.append("-" * 80)
    
    if evaluation['errors']:
        report.append("### [ERROR] 错误（需要立即修复）:")
        for error in evaluation['errors']:
            report.append(f"  - {error}")
        report.append("")
    
    if evaluation['warnings']:
        report.append("### [WARNING] 警告（需要关注）:")
        for warning in evaluation['warnings']:
            report.append(f"  - {warning}")
        report.append("")
    
    if not evaluation['errors'] and not evaluation['warnings']:
        report.append("[OK] 所有参数都在合理范围内，未发现明显问题")
        report.append("")
    
    # 5. 可靠性评估
    report.append("## 5. 可靠性评估")
    report.append("-" * 80)
    
    reliability_score = 100.0
    
    # 扣分项
    if len(analysis['identical_params']) > 20:
        reliability_score -= 20
        report.append(f"[WARNING] 过多参数完全相同 ({len(analysis['identical_params'])} 个)，可能表示优化不足")
    
    if len(analysis['different_params']) < 10:
        reliability_score -= 15
        report.append(f"[WARNING] 差异参数过少 ({len(analysis['different_params'])} 个)，预设区分度不足")
    
    if any(obj['playability_burden'] > 500 for obj in analysis['objective_comparison'].values()):
        reliability_score -= 25
        report.append("[WARNING] 可玩性负担过高（30KG测试可能未通过），需要重新优化")
    
    if any(obj['stability_risk'] > 0 for obj in analysis['objective_comparison'].values()):
        reliability_score -= 30
        report.append("[ERROR] 检测到稳定性风险（BUG），需要修复")
    
    # 加分项
    if all(obj['stability_risk'] == 0.0 for obj in analysis['objective_comparison'].values()):
        report.append("[OK] 所有预设稳定性风险为0（未检测到BUG）")
    
    if len(analysis['different_params']) >= 15:
        report.append(f"[OK] 预设有足够的区分度 ({len(analysis['different_params'])} 个差异参数)")
    
    report.append("")
    report.append(f"### 可靠性评分: {max(0, reliability_score):.1f}/100")
    
    if reliability_score >= 80:
        report.append("[OK] 可靠性：高 - 配置可以安全使用")
    elif reliability_score >= 60:
        report.append("[WARNING] 可靠性：中 - 配置可以使用，但建议进一步优化")
    else:
        report.append("[ERROR] 可靠性：低 - 建议重新运行优化")
    
    report.append("")
    
    # 6. 建议
    report.append("## 6. 改进建议")
    report.append("-" * 80)
    
    if len(analysis['identical_params']) > 20:
        report.append("1. 增加优化迭代次数（n_trials），以获得更多样化的解")
        report.append("2. 调整目标函数权重，增加解的多样性")
        report.append("3. 检查优化器是否收敛过早")
    
    if any(obj['playability_burden'] > 500 for obj in analysis['objective_comparison'].values()):
        report.append("4. 30KG测试未通过，建议：")
        report.append("   - 降低 energy_to_stamina_coeff（减少消耗）")
        report.append("   - 提高 base_recovery_rate（加快恢复）")
        report.append("   - 降低 encumbrance_stamina_drain_coeff（减少负重惩罚）")
    
    if len(analysis['different_params']) < 10:
        report.append("5. 预设区分度不足，建议：")
        report.append("   - 使用不同的权重组合选择解")
        report.append("   - 从帕累托前沿的不同区域选择解")
    
    report.append("")
    report.append("=" * 80)
    
    return "\n".join(report)


def main():
    """主函数"""
    print("RSS 增强型配置可靠性分析")
    print("=" * 80)
    
    # 配置文件路径
    config_files = {
        'EliteStandard': 'optimized_rss_config_realism_super.json',
        'StandardMilsim': 'optimized_rss_config_balanced_super.json',
        'TacticalAction': 'optimized_rss_config_playability_super.json'
    }
    
    # 加载配置
    configs = {}
    script_dir = Path(__file__).parent
    
    for preset_name, filename in config_files.items():
        file_path = script_dir / filename
        if file_path.exists():
            configs[preset_name] = load_config(str(file_path))
            print(f"[OK] 已加载: {filename}")
        else:
            print(f"[ERROR] 文件不存在: {filename}")
    
    if len(configs) < 3:
        print("错误：无法加载所有配置文件")
        return
    
    # 执行分析
    print("\n执行分析...")
    analysis = compare_configs(configs)
    evaluation = evaluate_parameter_reasonableness(configs)
    
    # 生成报告
    report = generate_analysis_report(configs, analysis, evaluation)
    
    # 输出报告
    print("\n" + report)
    
    # 保存报告到文件
    report_file = script_dir / 'super_config_analysis_report.txt'
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write(report)
    
    print(f"\n报告已保存到: {report_file}")


if __name__ == '__main__':
    main()
