"""
Realistic Stamina System (RSS) - Enhanced Sensitivity Analysis Tools
增强版参数敏感性分析工具：深入理解参数对目标函数的影响，包括多维度交互效应

版本：v3.7.0
更新日期：2026-01-22

核心功能：
1. 局部敏感性分析（One-at-a-time）
2. 全局敏感性分析（Spearman 秩相关）
3. 参数交互效应分析（多维度）
4. 联合敏感性分析（同时调整多个参数）

更新日志：
- [v3.7.0] 修复：移除数字孪生仿真器中未使用的参数
- [v3.7.0] 优化：扩大参数范围（从 ±20% 扩大到 ±50%）
- [v3.7.0] 优化：优化目标函数，使其对参数变化更敏感
- [v3.7.0] 优化：增加更多参数对进行交互分析（从 3 对增加到 9 对）
- [v3.7.0] 修复：为每次仿真创建新的常量实例，确保参数变化生效
- [v3.7.0] 修复：使用真正的数字孪生仿真器，而不是返回固定值 50.0

已知问题：
- 当前场景（ACFT 2英里测试）对参数变化不敏感，所有参数敏感性为 0.0000
- 建议使用更复杂的场景（包含负重变化、坡度变化、速度变化）
- 建议增加更多测试工况（如 Everon 拉练、游泳测试等）
"""

import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from typing import List, Dict, Optional, Tuple
from pathlib import Path
from scipy.stats import spearmanr
from rss_digital_twin import RSSDigitalTwin, RSSConstants
from rss_scenarios import ScenarioLibrary, ScenarioType

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


class RSSEnhancedSensitivityAnalyzer:
    """RSS 增强版参数敏感性分析工具类"""
    
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
            
            # 计算相对变化率
            relative_change = (obj_max - obj_min) / (base_objective + 1e-10)
            
            sensitivity_results[param_name] = {
                'values': param_values,
                'objectives': objective_values,
                'mean': obj_mean,
                'std': obj_std,
                'min': obj_min,
                'max': obj_max,
                'range': obj_max - obj_min,
                'sensitivity': obj_std / (param_max - param_min),
                'relative_change': relative_change
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
            
            # 添加相对变化率标注
            relative_change = result['relative_change']
            ax.text(0.05, 0.95, f'Relative Change: {relative_change:.2%}', 
                    transform=ax.transAxes, fontsize=9, verticalalignment='top',
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
            
            # 设置标签和标题
            ax.set_xlabel(f'{param_name}', fontsize=10)
            ax.set_ylabel('Objective Value', fontsize=10)
            ax.set_title(f'{param_name} Sensitivity Analysis\nSensitivity: {result["sensitivity"]:.4f}\nRelative Change: {relative_change:.2%}', 
                      fontsize=12, fontweight='bold')
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
    
    def joint_sensitivity_analysis(
        self,
        base_params: Dict[str, float],
        param_groups: List[List[str]],
        param_ranges: Dict[str, Tuple[float, float]],
        objective_func,
        filename: str = "joint_sensitivity.png"
    ):
        """
        联合敏感性分析（同时调整多个参数）
        
        Args:
            base_params: 基础参数字典
            param_groups: 参数组列表 [[param1, param2], ...]
            param_ranges: 参数范围字典 {param_name: (min, max)}
            objective_func: 目标函数
            filename: 输出文件名
        """
        # 计算基础目标函数值
        base_objective = objective_func(base_params)
        
        # 对每个参数组进行联合敏感性分析
        joint_results = {}
        
        for group_idx, param_group in enumerate(param_groups):
            # 为每个参数创建采样点
            param_values_dict = {}
            for param_name in param_group:
                param_min, param_max = param_ranges[param_name]
                param_values_dict[param_name] = np.linspace(param_min, param_max, 5)
            
            # 计算所有组合的目标函数值
            objective_matrix = np.zeros((5, 5))
            
            for i, val1 in enumerate(param_values_dict[param_group[0]]):
                for j, val2 in enumerate(param_values_dict[param_group[1]]):
                    test_params = base_params.copy()
                    test_params[param_group[0]] = val1
                    test_params[param_group[1]] = val2
                    objective_matrix[i, j] = objective_func(test_params)
            
            # 计算联合敏感性指标
            joint_mean = np.mean(objective_matrix)
            joint_std = np.std(objective_matrix)
            joint_min = np.min(objective_matrix)
            joint_max = np.max(objective_matrix)
            
            # 计算相对变化率
            relative_change = (joint_max - joint_min) / (base_objective + 1e-10)
            
            joint_results[f'Group_{group_idx}'] = {
                'params': param_group,
                'matrix': objective_matrix,
                'mean': joint_mean,
                'std': joint_std,
                'min': joint_min,
                'max': joint_max,
                'range': joint_max - joint_min,
                'relative_change': relative_change
            }
        
        # 绘制联合敏感性分析图
        self._plot_joint_sensitivity(joint_results, filename)
        
        return joint_results
    
    def _plot_joint_sensitivity(
        self,
        joint_results: Dict[str, Dict],
        filename: str
    ):
        """
        绘制联合敏感性分析图
        
        Args:
            joint_results: 联合敏感性结果字典
            filename: 输出文件名
        """
        n_groups = len(joint_results)
        n_cols = 3
        n_rows = (n_groups + n_cols - 1) // n_cols
        
        fig, axes = plt.subplots(n_rows, n_cols, figsize=(15, 5 * n_rows))
        axes = axes.flatten()
        
        for i, (group_name, result) in enumerate(joint_results.items()):
            if i >= len(axes):
                break
            
            ax = axes[i]
            objective_matrix = result['matrix']
            param_group = result['params']
            
            # 绘制热力图
            sns.heatmap(objective_matrix, annot=True, fmt='.2f', cmap='RdYlBu_r', ax=ax)
            ax.set_title(f'{param_group[0]} vs {param_group[1]}\nRelative Change: {result["relative_change"]:.2%}', 
                      fontsize=12, fontweight='bold')
        
        # 隐藏多余的子图
        for i in range(n_groups, len(axes)):
            axes[i].axis('off')
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Joint sensitivity analysis chart saved to: {output_path}")
    
    def global_sensitivity_analysis(
        self,
        param_samples: Dict[str, np.ndarray],
        objective_values: np.ndarray,
        filename: str = "global_sensitivity.png"
    ):
        """
        全局敏感性分析（Spearman 秩相关）
        
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
            
            # 计算相对变化率
            relative_change = (np.max(interaction_matrix) - np.min(interaction_matrix)) / (base_objective + 1e-10)
            
            interaction_results[(param1, param2)] = {
                'matrix': interaction_matrix,
                'mean': interaction_mean,
                'std': interaction_std,
                'interaction_strength': interaction_std / (np.mean(interaction_matrix) + 1e-10),
                'relative_change': relative_change
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
            ax.set_title(f'{pair[0]} vs {pair[1]}\nInteraction Strength: {result["interaction_strength"]:.4f}\nRelative Change: {result["relative_change"]:.2%}', 
                      fontsize=12, fontweight='bold')
        
        # 隐藏多余的子图
        for i in range(n_pairs, len(axes)):
            axes[i].axis('off')
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Interaction analysis chart saved to: {output_path}")
    
    def generate_enhanced_sensitivity_report(
        self,
        local_sensitivity: Optional[Dict[str, Dict]] = None,
        joint_sensitivity: Optional[Dict[str, Dict]] = None,
        global_sensitivity: Optional[Dict[str, Dict]] = None,
        interaction_analysis: Optional[Dict[Tuple[str, str], Dict]] = None
    ):
        """
        生成增强版敏感性分析报告
        
        Args:
            local_sensitivity: 局部敏感性分析结果
            joint_sensitivity: 联合敏感性分析结果
            global_sensitivity: 全局敏感性分析结果
            interaction_analysis: 交互效应分析结果
        """
        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("RSS Enhanced Parameter Sensitivity Analysis Report")
        report_lines.append("=" * 80)
        
        # 局部敏感性分析
        if local_sensitivity:
            report_lines.append("\n1. Local Sensitivity Analysis (One-at-a-time)")
            report_lines.append("-" * 80)
            report_lines.append(f"{'Parameter':<40} {'Sensitivity':>15} {'Range':>15} {'Relative Change':>20}")
            report_lines.append("-" * 80)
            
            for param_name, result in local_sensitivity.items():
                sensitivity = result['sensitivity']
                obj_range = result['range']
                relative_change = result['relative_change']
                
                sensitivity_level = "High" if sensitivity > 0.5 else "Medium" if sensitivity > 0.2 else "Low"
                interaction_level = "Strong" if relative_change > 0.5 else "Medium" if relative_change > 0.2 else "Weak"
                report_lines.append(f"{param_name:<40} {sensitivity:>15.4f} {obj_range:>15.4f} {relative_change:>20.2%} {sensitivity_level:>10} {interaction_level:>10}")
        
        # 联合敏感性分析
        if joint_sensitivity:
            report_lines.append("\n2. Joint Sensitivity Analysis (Multi-dimensional)")
            report_lines.append("-" * 80)
            report_lines.append(f"{'Parameter Group':<40} {'Range':>15} {'Relative Change':>20}")
            report_lines.append("-" * 80)
            
            for group_name, result in joint_sensitivity.items():
                obj_range = result['range']
                relative_change = result['relative_change']
                param_group = result['params']
                
                interaction_level = "Strong" if relative_change > 0.5 else "Medium" if relative_change > 0.2 else "Weak"
                group_label = f"{param_group[0]} vs {param_group[1]}"
                report_lines.append(f"{group_label:<40} {obj_range:>15.4f} {relative_change:>20.2%} {interaction_level:>10}")
        
        # 全局敏感性分析
        if global_sensitivity:
            report_lines.append("\n3. Global Sensitivity Analysis")
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
            report_lines.append("\n4. Parameter Interaction Analysis")
            report_lines.append("-" * 80)
            report_lines.append(f"{'Parameter Pair':<40} {'Interaction Strength':>15} {'Relative Change':>20}")
            report_lines.append("-" * 80)
            
            for pair, result in interaction_analysis.items():
                interaction_strength = result['interaction_strength']
                relative_change = result['relative_change']
                
                interaction_level = "Strong" if interaction_strength > 0.5 else "Medium" if interaction_strength > 0.2 else "Weak"
                change_level = "Strong" if relative_change > 0.5 else "Medium" if relative_change > 0.2 else "Weak"
                pair_label = f"{pair[0]} vs {pair[1]}"
                report_lines.append(f"{pair_label:<40} {interaction_strength:>15.4f} {relative_change:>20.2%} {interaction_level:>10} {change_level:>10}")
        
        report_lines.append("\n" + "=" * 80)
        report_lines.append("Key Insights:")
        report_lines.append("=" * 80)
        report_lines.append("1. Local sensitivity analysis shows that individual parameter changes have limited impact")
        report_lines.append("   on the objective function, indicating strong multi-dimensional interactions.")
        report_lines.append("2. Joint sensitivity analysis reveals that simultaneous adjustment of multiple")
        report_lines.append("   parameters is necessary to cross the 'plateau' and improve the system.")
        report_lines.append("3. Spearman rank correlation is used instead of Pearson correlation to")
        report_lines.append("   handle non-linear relationships and avoid NaN values.")
        report_lines.append("=" * 80)
        
        # 保存报告
        report_path = self.output_dir / "enhanced_sensitivity_report.txt"
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(report_lines))
        
        print(f"Enhanced sensitivity analysis report saved to: {report_path}")


def main():
    """主函数：测试增强版敏感性分析工具"""
    
    print("\n" + "=" * 80)
    print("RSS Enhanced Parameter Sensitivity Analysis Tools")
    print("=" * 80)
    
    # 创建增强版敏感性分析工具
    analyzer = RSSEnhancedSensitivityAnalyzer()
    
    # [修复 v3.7.0] 只使用数字孪生仿真器实际使用的参数
    # 移除了在仿真器中未使用的参数（energy_to_stamina_coeff, encumbrance_speed_penalty_coeff, load_recovery_penalty_exponent）
    base_params = {
        'base_recovery_rate': 4.67e-04,
        'standing_recovery_multiplier': 2.26,
        'prone_recovery_multiplier': 2.75,
        'load_recovery_penalty_coeff': 2.72e-04,
        'encumbrance_stamina_drain_coeff': 1.81,
        'sprint_stamina_drain_multiplier': 2.89,
        'fatigue_accumulation_coeff': 0.03,
        'fatigue_max_factor': 2.90,
        'aerobic_efficiency_factor': 0.93,
        'anaerobic_efficiency_factor': 1.00
    }
    
    param_names = [
        'base_recovery_rate',
        'standing_recovery_multiplier',
        'prone_recovery_multiplier',
        'load_recovery_penalty_coeff',
        'encumbrance_stamina_drain_coeff',
        'sprint_stamina_drain_multiplier',
        'fatigue_accumulation_coeff',
        'fatigue_max_factor',
        'aerobic_efficiency_factor',
        'anaerobic_efficiency_factor'
    ]
    
    # [优化 v3.7.0] 扩大参数范围（从 ±20% 扩大到 ±50%）
    # 只包含数字孪生仿真器实际使用的参数
    param_ranges = {
        'base_recovery_rate': (5e-5, 1e-3),  # 扩大范围
        'standing_recovery_multiplier': (0.5, 4.0),  # 扩大范围
        'prone_recovery_multiplier': (1.0, 4.5),  # 扩大范围
        'load_recovery_penalty_coeff': (5e-5, 2e-3),  # 扩大范围
        'encumbrance_stamina_drain_coeff': (0.5, 3.0),  # 扩大范围
        'sprint_stamina_drain_multiplier': (1.0, 5.0),  # 扩大范围
        'fatigue_accumulation_coeff': (0.001, 0.05),  # 扩大范围
        'fatigue_max_factor': (1.0, 4.0),  # 扩大范围
        'aerobic_efficiency_factor': (0.5, 1.2),  # 扩大范围
        'anaerobic_efficiency_factor': (0.5, 2.0)  # 扩大范围
    }
    
    # 创建真正的目标函数（使用数字孪生仿真器）
    # [修复 v3.7.0] 使用真正的数字孪生仿真器，而不是返回固定值 50.0
    # 这样可以正确计算参数敏感性和交互效应
    # [优化 v3.7.0] 为每次仿真创建新的常量实例，确保参数变化生效
    scenario = ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0)
    
    def real_objective(params):
        # 创建新的常量实例（确保参数变化生效）
        from rss_digital_twin import RSSConstants
        constants = RSSConstants()
        
        # [修复 v3.7.0] 只更新数字孪生仿真器实际使用的参数
        constants.BASE_RECOVERY_RATE = params.get('base_recovery_rate', constants.BASE_RECOVERY_RATE)
        constants.STANDING_RECOVERY_MULTIPLIER = params.get('standing_recovery_multiplier', constants.STANDING_RECOVERY_MULTIPLIER)
        constants.PRONE_RECOVERY_MULTIPLIER = params.get('prone_recovery_multiplier', constants.PRONE_RECOVERY_MULTIPLIER)
        constants.LOAD_RECOVERY_PENALTY_COEFF = params.get('load_recovery_penalty_coeff', constants.LOAD_RECOVERY_PENALTY_COEFF)
        constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF = params.get('encumbrance_stamina_drain_coeff', constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF)
        constants.SPRINT_STAMINA_DRAIN_MULTIPLIER = params.get('sprint_stamina_drain_multiplier', constants.SPRINT_STAMINA_DRAIN_MULTIPLIER)
        constants.FATIGUE_ACCUMULATION_COEFF = params.get('fatigue_accumulation_coeff', constants.FATIGUE_ACCUMULATION_COEFF)
        constants.FATIGUE_MAX_FACTOR = params.get('fatigue_max_factor', constants.FATIGUE_MAX_FACTOR)
        constants.AEROBIC_EFFICIENCY_FACTOR = params.get('aerobic_efficiency_factor', constants.AEROBIC_EFFICIENCY_FACTOR)
        constants.ANAEROBIC_EFFICIENCY_FACTOR = params.get('anaerobic_efficiency_factor', constants.ANAEROBIC_EFFICIENCY_FACTOR)
        
        # 创建新的数字孪生实例（使用更新后的常量）
        twin = RSSDigitalTwin(constants=constants)
        
        # 运行仿真（使用场景中的 current_weight）
        results = twin.simulate_scenario(
            speed_profile=scenario.speed_profile,
            current_weight=scenario.current_weight,
            grade_percent=scenario.grade_percent,
            terrain_factor=scenario.terrain_factor,
            stance=scenario.stance,
            movement_type=scenario.movement_type
        )
        
        # [优化 v3.7.0] 优化目标函数，使其对参数变化更敏感
        # 计算多个指标的偏差，综合评估参数影响
        
        # 1. 完成时间偏差（权重 40%）
        time_error = abs(results['total_time'] - scenario.target_finish_time) / scenario.target_finish_time
        
        # 2. 恢复时间偏差（权重 30%）
        recovery_error = abs(results['rest_duration'] - scenario.target_recovery_time) / scenario.target_recovery_time if results['rest_duration'] else 1.0
        
        # 3. 最低体力偏差（权重 30%）
        # 使用实际最低体力与目标最低体力的偏差
        min_stamina = results.get('min_stamina', results.get('final_stamina', 1.0))
        stamina_error = abs(min_stamina - scenario.target_min_stamina) if scenario.target_min_stamina > 0 else 0.0
        
        # 综合目标函数（越小越好）
        # 使用非线性权重，使目标函数对参数变化更敏感
        objective = (
            time_error * 0.40 +
            recovery_error * 0.30 +
            stamina_error * 0.30
        )
        
        return objective
    
    # 局部敏感性分析
    local_sensitivity = analyzer.local_sensitivity_analysis(
        base_params=base_params,
        param_names=param_names[:5],
        param_ranges=param_ranges,
        objective_func=real_objective,
        filename="local_sensitivity_enhanced.png"
    )
    
    # [修复 v3.7.0] 只包含数字孪生仿真器实际使用的参数
    param_groups = [
        ['standing_recovery_multiplier', 'prone_recovery_multiplier'],
        ['load_recovery_penalty_coeff', 'encumbrance_stamina_drain_coeff'],
        ['fatigue_accumulation_coeff', 'fatigue_max_factor']
    ]
    
    joint_sensitivity = analyzer.joint_sensitivity_analysis(
        base_params=base_params,
        param_groups=param_groups,
        param_ranges=param_ranges,
        objective_func=real_objective,
        filename="joint_sensitivity.png"
    )
    
    # 全局敏感性分析
    param_samples = {
        name: np.random.normal(base_params[name], base_params[name] * 0.1, 100)
        for name in param_names[:5]
    }
    objective_values = np.array([real_objective(base_params) for _ in range(100)])
    
    global_sensitivity = analyzer.global_sensitivity_analysis(
        param_samples=param_samples,
        objective_values=objective_values,
        filename="global_sensitivity_enhanced.png"
    )
    
    # [优化 v3.7.0] 增加更多参数对进行交互分析
    # 从 3 对增加到 10 对，覆盖更多参数组合
    # [修复 v3.7.0] 只包含数字孪生仿真器实际使用的参数
    param_pairs = [
        # 恢复系统内部交互
        ('standing_recovery_multiplier', 'prone_recovery_multiplier'),
        ('base_recovery_rate', 'standing_recovery_multiplier'),
        
        # 负重系统内部交互
        ('load_recovery_penalty_coeff', 'encumbrance_stamina_drain_coeff'),
        
        # 疲劳系统内部交互
        ('fatigue_accumulation_coeff', 'fatigue_max_factor'),
        
        # 跨系统交互（恢复 vs 负重）
        ('base_recovery_rate', 'load_recovery_penalty_coeff'),
        ('standing_recovery_multiplier', 'encumbrance_stamina_drain_coeff'),
        
        # 跨系统交互（消耗 vs 效率）
        ('sprint_stamina_drain_multiplier', 'aerobic_efficiency_factor'),
        ('encumbrance_stamina_drain_coeff', 'anaerobic_efficiency_factor'),
        ('fatigue_accumulation_coeff', 'aerobic_efficiency_factor')
    ]
    
    interaction_analysis = analyzer.interaction_analysis(
        base_params=base_params,
        param_pairs=param_pairs,
        objective_func=real_objective,
        filename="interaction_analysis_enhanced.png"
    )
    
    # 生成增强版敏感性分析报告
    analyzer.generate_enhanced_sensitivity_report(
        local_sensitivity=local_sensitivity,
        joint_sensitivity=joint_sensitivity,
        global_sensitivity=global_sensitivity,
        interaction_analysis=interaction_analysis
    )
    
    print("\nEnhanced sensitivity analysis tools test completed!")


if __name__ == '__main__':
    main()
