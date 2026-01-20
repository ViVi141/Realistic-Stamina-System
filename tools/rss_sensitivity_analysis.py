"""
Realistic Stamina System (RSS) - Sensitivity Analysis Tools
参数敏感性分析工具：深入理解参数对目标函数的影响

核心功能：
1. 局部敏感性分析（One-at-a-time）
2. 全局敏感性分析（Sobol、Morris）
3. 参数交互效应分析
"""

import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from typing import List, Dict, Optional, Tuple
from pathlib import Path

# 设置中文字体支持 - 修复字体问题
import matplotlib.font_manager as fm
# 尝试加载中文字体
chinese_fonts = ['SimHei', 'Microsoft YaHei', 'PingFang SC', 'STHeiti', 'KaiTi']
available_fonts = [f.name for f in fm.fontManager.ttflist]

# 选择可用的中文字体
font_family = 'Arial'
for font in chinese_fonts:
    if font in available_fonts:
        font_family = font
        break

plt.rcParams['font.sans-serif'] = [font_family, 'Arial']
plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['font.family'] = font_family

print(f"Using font: {font_family}")

# 设置样式
sns.set_style("whitegrid")
sns.set_palette("husl")


class RSSSensitivityAnalyzer:
    """RSS 参数敏感性分析工具类"""
    
    def __init__(self, output_dir: str = "sensitivity_analysis"):
        """
        初始化敏感性分析工具
        
        Args:
            output_dir: 输出目录
        """
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def local_sensitivity_analysis(
        self,
        base_params: Dict[str, float],
        param_names: List[str],
        param_ranges: Dict[str, Tuple[float, float]],
        objective_func,
        filename: str = "local_sensitivity.png"
    ):
        """
        局部敏感性分析（One-at-a-time）
        
        Args:
            base_params: 基础参数字典
            param_names: 参数名称列表
            param_ranges: 参数范围字典 {param_name: (min, max)}
            objective_func: 目标函数
            filename: 输出文件名
        """
        # 计算基础目标函数值
        base_objective = objective_func(base_params)
        
        # 对每个参数进行敏感性分析
        sensitivity_results = {}
        
        for param_name in param_names:
            param_min, param_max = param_ranges[param_name]
            param_values = np.linspace(param_min, param_max, 20)
            
            # 计算目标函数值
            objective_values = []
            for value in param_values:
                test_params = base_params.copy()
                test_params[param_name] = value
                obj_val = objective_func(test_params)
                objective_values.append(obj_val)
            
            # 计算敏感性指标
            obj_mean = np.mean(objective_values)
            obj_std = np.std(objective_values)
            obj_min = np.min(objective_values)
            obj_max = np.max(objective_values)
            
            sensitivity_results[param_name] = {
                'values': param_values,
                'objectives': objective_values,
                'mean': obj_mean,
                'std': obj_std,
                'min': obj_min,
                'max': obj_max,
                'range': obj_max - obj_min,
                'sensitivity': obj_std / (param_max - param_min)
            }
        
        # 绘制敏感性分析图
        self._plot_local_sensitivity(sensitivity_results, filename)
        
        return sensitivity_results
    
    def _plot_local_sensitivity(
        self,
        sensitivity_results: Dict[str, Dict],
        filename: str
    ):
        """
        绘制局部敏感性分析图
        
        Args:
            sensitivity_results: 敏感性结果字典
            filename: 输出文件名
        """
        n_params = len(sensitivity_results)
        n_cols = 4
        n_rows = (n_params + n_cols - 1) // n_cols
        
        fig, axes = plt.subplots(n_rows, n_cols, figsize=(20, 5 * n_rows))
        axes = axes.flatten()
        
        for i, (param_name, result) in enumerate(sensitivity_results.items()):
            if i >= len(axes):
                break
            
            ax = axes[i]
            param_values = result['values']
            objective_values = result['objectives']
            
            # 绘制目标函数曲线
            ax.plot(param_values, objective_values, 'b-', linewidth=2, label='Objective Value')
            ax.fill_between(param_values, objective_values, alpha=0.3)
            
            # 标记最小值和最大值
            ax.scatter([result['min']], [result['min']], c='red', s=100, marker='o', edgecolors='black', linewidths=2, label='Min Value')
            ax.scatter([result['max']], [result['max']], c='green', s=100, marker='s', edgecolors='black', linewidths=2, label='Max Value')
            
            # 设置标签和标题
            ax.set_xlabel(f'{param_name}', fontsize=10)
            ax.set_ylabel('Objective Value', fontsize=10)
            ax.set_title(f'{param_name} Sensitivity Analysis\nSensitivity: {result["sensitivity"]:.4f}', fontsize=12, fontweight='bold')
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3)
        
        # 隐藏多余的子图
        for i in range(n_params, len(axes)):
            axes[i].axis('off')
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Local sensitivity analysis chart saved to: {output_path}")
    
    def global_sensitivity_analysis(
        self,
        param_samples: Dict[str, np.ndarray],
        objective_values: np.ndarray,
        filename: str = "global_sensitivity.png"
    ):
        """
        全局敏感性分析（Sobol）
        
        Args:
            param_samples: 参数样本字典 {param_name: array}
            objective_values: 目标函数值数组
            filename: 输出文件名
        """
        # 计算每个参数的敏感性
        sensitivity_results = {}
        
        for param_name, samples in param_samples.items():
            # 计算均值和标准差
            mean_val = np.mean(samples)
            std_val = np.std(samples)
            
            # 计算与目标函数的相关性 - 使用 Spearman 秩相关系数
            from scipy.stats import spearmanr
            try:
                correlation, p_value = spearmanr(samples, objective_values)
                # 处理 nan 值
                if np.isnan(correlation):
                    correlation = 0.0
            except:
                correlation = 0.0
            
            # 计算敏感性指标
            sensitivity = std_val / (mean_val + 1e-10)
            
            sensitivity_results[param_name] = {
                'mean': mean_val,
                'std': std_val,
                'correlation': correlation,
                'sensitivity': sensitivity
            }
        
        # 绘制全局敏感性分析图
        self._plot_global_sensitivity(sensitivity_results, filename)
        
        return sensitivity_results
    
    def _plot_global_sensitivity(
        self,
        sensitivity_results: Dict[str, Dict],
        filename: str
    ):
        """
        绘制全局敏感性分析图
        
        Args:
            sensitivity_results: 敏感性结果字典
            filename: 输出文件名
        """
        # 准备数据
        param_names = list(sensitivity_results.keys())
        sensitivities = [result['sensitivity'] for result in sensitivity_results.values()]
        correlations = [result['correlation'] for result in sensitivity_results.values()]
        
        # 创建图表
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))
        
        # 敏感性柱状图
        colors = ['red' if abs(c) > 0.3 else 'orange' if abs(c) > 0.1 else 'green' for c in correlations]
        ax1.bar(range(len(param_names)), sensitivities, color=colors, alpha=0.7, edgecolor='black')
        ax1.set_xlabel('Parameter Name', fontsize=12)
        ax1.set_ylabel('Sensitivity', fontsize=12)
        ax1.set_title('Parameter Sensitivity Analysis', fontsize=14, fontweight='bold')
        ax1.set_xticks(range(len(param_names)))
        ax1.set_xticklabels(param_names, rotation=45, ha='right')
        ax1.grid(True, alpha=0.3, axis='y')
        
        # 相关性柱状图
        ax2.bar(range(len(param_names)), correlations, color=colors, alpha=0.7, edgecolor='black')
        ax2.set_xlabel('Parameter Name', fontsize=12)
        ax2.set_ylabel('Correlation with Objective', fontsize=12)
        ax2.set_title('Parameter Correlation with Objective', fontsize=14, fontweight='bold')
        ax2.set_xticks(range(len(param_names)))
        ax2.set_xticklabels(param_names, rotation=45, ha='right')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.axhline(y=0, color='red', linestyle='--', linewidth=2, alpha=0.5)
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Global sensitivity analysis chart saved to: {output_path}")
    
    def interaction_analysis(
        self,
        base_params: Dict[str, float],
        param_pairs: List[Tuple[str, str]],
        objective_func,
        filename: str = "interaction_analysis.png"
    ):
        """
        参数交互效应分析
        
        Args:
            base_params: 基础参数字典
            param_pairs: 参数对列表 [(param1, param2), ...]
            objective_func: 目标函数
            filename: 输出文件名
        """
        # 计算基础目标函数值
        base_objective = objective_func(base_params)
        
        # 对每个参数对进行交互分析
        interaction_results = {}
        
        for param1, param2 in param_pairs:
            param1_range = (base_params[param1] * 0.8, base_params[param1] * 1.2)
            param2_range = (base_params[param2] * 0.8, base_params[param2] * 1.2)
            
            # 计算交互效应
            interaction_matrix = np.zeros((5, 5))
            
            for i, val1 in enumerate(np.linspace(param1_range[0], param1_range[1], 5)):
                for j, val2 in enumerate(np.linspace(param2_range[0], param2_range[1], 5)):
                    test_params = base_params.copy()
                    test_params[param1] = val1
                    test_params[param2] = val2
                    interaction_matrix[i, j] = objective_func(test_params)
            
            # 计算交互效应指标
            interaction_mean = np.mean(interaction_matrix)
            interaction_std = np.std(interaction_matrix)
            
            interaction_results[(param1, param2)] = {
                'matrix': interaction_matrix,
                'mean': interaction_mean,
                'std': interaction_std,
                'interaction_strength': interaction_std / (np.mean(interaction_matrix) + 1e-10)
            }
        
        # 绘制交互效应图
        self._plot_interaction_analysis(interaction_results, filename)
        
        return interaction_results
    
    def _plot_interaction_analysis(
        self,
        interaction_results: Dict[Tuple[str, str], Dict],
        filename: str
    ):
        """
        绘制交互效应分析图
        
        Args:
            interaction_results: 交互效应结果字典
            filename: 输出文件名
        """
        n_pairs = len(interaction_results)
        n_cols = 3
        n_rows = (n_pairs + n_cols - 1) // n_cols
        
        fig, axes = plt.subplots(n_rows, n_cols, figsize=(15, 5 * n_rows))
        axes = axes.flatten()
        
        for i, (pair, result) in enumerate(interaction_results.items()):
            if i >= len(axes):
                break
            
            ax = axes[i]
            interaction_matrix = result['matrix']
            
            # 绘制热力图
            sns.heatmap(interaction_matrix, annot=True, fmt='.2f', cmap='RdYlBu_r', ax=ax)
            ax.set_title(f'{pair[0]} vs {pair[1]}\nInteraction Strength: {result["interaction_strength"]:.4f}', fontsize=12, fontweight='bold')
        
        # 隐藏多余的子图
        for i in range(n_pairs, len(axes)):
            axes[i].axis('off')
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Interaction analysis chart saved to: {output_path}")
    
    def generate_sensitivity_report(
        self,
        local_sensitivity: Optional[Dict[str, Dict]] = None,
        global_sensitivity: Optional[Dict[str, Dict]] = None,
        interaction_analysis: Optional[Dict[Tuple[str, str], Dict]] = None
    ):
        """
        生成敏感性分析报告
        
        Args:
            local_sensitivity: 局部敏感性分析结果
            global_sensitivity: 全局敏感性分析结果
            interaction_analysis: 交互效应分析结果
        """
        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("RSS Parameter Sensitivity Analysis Report")
        report_lines.append("=" * 80)
        
        # 局部敏感性分析
        if local_sensitivity:
            report_lines.append("\n1. Local Sensitivity Analysis (One-at-a-time)")
            report_lines.append("-" * 80)
            report_lines.append(f"{'Parameter':<40} {'Sensitivity':>15} {'Range':>15} {'Mean':>15}")
            report_lines.append("-" * 80)
            
            for param_name, result in local_sensitivity.items():
                sensitivity = result['sensitivity']
                obj_range = result['range']
                obj_mean = result['mean']
                
                sensitivity_level = "High" if sensitivity > 0.5 else "Medium" if sensitivity > 0.2 else "Low"
                report_lines.append(f"{param_name:<40} {sensitivity:>15.4f} {obj_range:>15.4f} {obj_mean:>15.4f} {sensitivity_level:>10}")
        
        # 全局敏感性分析
        if global_sensitivity:
            report_lines.append("\n2. Global Sensitivity Analysis")
            report_lines.append("-" * 80)
            report_lines.append(f"{'Parameter':<40} {'Sensitivity':>15} {'Correlation':>15}")
            report_lines.append("-" * 80)
            
            for param_name, result in global_sensitivity.items():
                sensitivity = result['sensitivity']
                correlation = result['correlation']
                
                sensitivity_level = "High" if sensitivity > 0.3 else "Medium" if sensitivity > 0.1 else "Low"
                correlation_level = "Strong" if abs(correlation) > 0.7 else "Medium" if abs(correlation) > 0.3 else "Weak"
                report_lines.append(f"{param_name:<40} {sensitivity:>15.4f} {correlation:>15.4f} {sensitivity_level:>10} {correlation_level:>10}")
        
        # 交互效应分析
        if interaction_analysis:
            report_lines.append("\n3. Parameter Interaction Analysis")
            report_lines.append("-" * 80)
            report_lines.append(f"{'Parameter Pair':<40} {'Interaction Strength':>15}")
            report_lines.append("-" * 80)
            
            for pair, result in interaction_analysis.items():
                interaction_strength = result['interaction_strength']
                interaction_level = "Strong" if interaction_strength > 0.5 else "Medium" if interaction_strength > 0.2 else "Weak"
                report_lines.append(f"{pair[0]} vs {pair[1]:<40} {interaction_strength:>15.4f} {interaction_level:>10}")
        
        report_lines.append("\n" + "=" * 80)
        
        # 保存报告
        report_path = self.output_dir / "sensitivity_report.txt"
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(report_lines))
        
        print(f"敏感性分析报告已保存到：{report_path}")


def main():
    """主函数：测试敏感性分析工具"""
    
    print("\n" + "=" * 80)
    print("RSS 参数敏感性分析工具")
    print("=" * 80)
    
    # 创建敏感性分析工具
    analyzer = RSSSensitivityAnalyzer()
    
    # 模拟数据
    base_params = {
        'energy_to_stamina_coeff': 4.15e-05,
        'base_recovery_rate': 4.67e-04,
        'standing_recovery_multiplier': 2.26,
        'prone_recovery_multiplier': 2.75,
        'load_recovery_penalty_coeff': 2.72e-04,
        'load_recovery_penalty_exponent': 1.11,
        'encumbrance_speed_penalty_coeff': 0.29,
        'encumbrance_stamina_drain_coeff': 1.81,
        'sprint_stamina_drain_multiplier': 2.89,
        'fatigue_accumulation_coeff': 0.03,
        'fatigue_max_factor': 2.90,
        'aerobic_efficiency_factor': 0.93,
        'anaerobic_efficiency_factor': 1.00
    }
    
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
        return 50.0 + np.random.normal(0, 5)
    
    # 局部敏感性分析
    local_sensitivity = analyzer.local_sensitivity_analysis(
        base_params,
        param_names[:5],  # 只分析前5个参数
        param_ranges,
        mock_objective,
        "local_sensitivity.png"
    )
    
    # 全局敏感性分析
    param_samples = {
        name: np.random.normal(base_params[name], base_params[name] * 0.1, 100)
        for name in param_names[:5]
    }
    objective_values = np.array([mock_objective(base_params) for _ in range(100)])
    
    global_sensitivity = analyzer.global_sensitivity_analysis(
        param_samples,
        objective_values,
        "global_sensitivity.png"
    )
    
    # 交互效应分析
    param_pairs = [
        ('standing_recovery_multiplier', 'prone_recovery_multiplier'),
        ('encumbrance_speed_penalty_coeff', 'encumbrance_stamina_drain_coeff'),
        ('fatigue_accumulation_coeff', 'fatigue_max_factor')
    ]
    
    interaction_analysis = analyzer.interaction_analysis(
        base_params,
        param_pairs,
        mock_objective,
        "interaction_analysis.png"
    )
    
    # 生成报告
    analyzer.generate_sensitivity_report(
        local_sensitivity=local_sensitivity,
        global_sensitivity=global_sensitivity,
        interaction_analysis=interaction_analysis
    )
    
    print("\n敏感性分析工具测试完成！")


if __name__ == '__main__':
    main()
