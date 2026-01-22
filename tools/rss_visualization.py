"""
Realistic Stamina System (RSS) - Visualization Tools
可视化工具集：帮助更好地理解优化结果

核心功能：
1. 帕累托前沿散点图
2. 参数敏感度热力图
3. 收敛曲线图
4. 参数分布图
5. 雷达图对比不同配置
"""

import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from typing import List, Dict, Optional, Tuple
import json
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

print(f"使用字体：{font_family}")

# 设置样式
sns.set_style("whitegrid")
sns.set_palette("husl")


class RSSVisualizer:
    """RSS 可视化工具类"""
    
    def __init__(self, output_dir: str = "visualizations"):
        """
        初始化可视化工具
        
        Args:
            output_dir: 输出目录
        """
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def plot_pareto_front(
        self,
        pareto_front: np.ndarray,
        pareto_set: np.ndarray,
        title: str = "帕累托前沿",
        filename: str = "pareto_front.png",
        highlight_points: Optional[List[Tuple[int, str]]] = None
    ):
        """
        绘制帕累托前沿散点图
        
        Args:
            pareto_front: 帕累托前沿（n x 2 数组）
            pareto_set: 帕累托解集（n x m 数组）
            title: 图表标题
            filename: 输出文件名
            highlight_points: 要高亮的点 [(index, label), ...]
        """
        fig, ax = plt.subplots(figsize=(10, 8))
        
        # 绘制所有解
        ax.scatter(
            pareto_front[:, 0],
            pareto_front[:, 1],
            c='blue',
            alpha=0.6,
            s=50,
            label='Pareto Front Solutions'
        )
        
        # 高亮特定点
        if highlight_points:
            colors = ['red', 'green', 'orange']
            for idx, (point_idx, label) in enumerate(highlight_points):
                ax.scatter(
                    pareto_front[point_idx, 0],
                    pareto_front[point_idx, 1],
                    c=colors[idx % len(colors)],
                    s=200,
                    marker='*',
                    edgecolors='black',
                    linewidths=2,
                    label=label
                )
        
        # 设置标签和标题
        ax.set_xlabel('Realism Loss', fontsize=12)
        ax.set_ylabel('Playability Burden', fontsize=12)
        ax.set_title(title, fontsize=14, fontweight='bold')
        ax.legend(fontsize=10)
        ax.grid(True, alpha=0.3)
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Pareto Front chart saved to: {output_path}")
    
    def plot_sensitivity_heatmap(
        self,
        sensitivity: Dict[str, float],
        title: str = "参数敏感度分析",
        filename: str = "sensitivity_heatmap.png"
    ):
        """
        绘制参数敏感度热力图
        
        Args:
            sensitivity: 参数敏感度字典 {param_name: cv}
            title: 图表标题
            filename: 输出文件名
        """
        # 准备数据
        params = list(sensitivity.keys())
        values = list(sensitivity.values())
        
        # 创建热力图
        fig, ax = plt.subplots(figsize=(12, 8))
        
        # 按敏感度排序
        sorted_indices = np.argsort(values)[::-1]
        sorted_params = [params[i] for i in sorted_indices]
        sorted_values = [values[i] for i in sorted_indices]
        
        # 绘制热力图
        heatmap_data = np.array(sorted_values).reshape(-1, 1)
        sns.heatmap(
            heatmap_data,
            annot=True,
            fmt='.4f',
            cmap='YlOrRd',
            yticklabels=sorted_params,
            xticklabels=['变异系数'],
            cbar_kws={'label': '变异系数 (CV)'},
            ax=ax
        )
        
        # 设置标题
        ax.set_title(title, fontsize=14, fontweight='bold', pad=20)
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Sensitivity heatmap saved to: {output_path}")
    
    def plot_convergence_curve(
        self,
        trials_history: List[Dict],
        title: str = "优化收敛曲线",
        filename: str = "convergence_curve.png"
    ):
        """
        绘制优化收敛曲线
        
        Args:
            trials_history: 试验历史列表
            title: 图表标题
            filename: 输出文件名
        """
        fig, axes = plt.subplots(1, 2, figsize=(15, 6))
        
        # 提取数据
        trial_numbers = range(1, len(trials_history) + 1)
        realism_losses = [trial.values[0] for trial in trials_history]
        playability_burdens = [trial.values[1] for trial in trials_history]
        
        # 计算最佳值
        best_realism = []
        best_playability = []
        current_best_realism = float('inf')
        current_best_playability = float('inf')
        
        for i, (r, p) in enumerate(zip(realism_losses, playability_burdens)):
            current_best_realism = min(current_best_realism, r)
            current_best_playability = min(current_best_playability, p)
            best_realism.append(current_best_realism)
            best_playability.append(current_best_playability)
        
        # 绘制拟真度损失
        axes[0].plot(trial_numbers, realism_losses, 'b-', alpha=0.3, label='Current Value')
        axes[0].plot(trial_numbers, best_realism, 'r-', linewidth=2, label='Best Value')
        axes[0].set_xlabel('Trial Number', fontsize=12)
        axes[0].set_ylabel('Realism Loss', fontsize=12)
        axes[0].set_title('Realism Loss Convergence', fontsize=14, fontweight='bold')
        axes[0].legend(fontsize=10)
        axes[0].grid(True, alpha=0.3)
        
        # 绘制可玩性负担
        axes[1].plot(trial_numbers, playability_burdens, 'b-', alpha=0.3, label='Current Value')
        axes[1].plot(trial_numbers, best_playability, 'r-', linewidth=2, label='Best Value')
        axes[1].set_xlabel('Trial Number', fontsize=12)
        axes[1].set_ylabel('Playability Burden', fontsize=12)
        axes[1].set_title('Playability Burden Convergence', fontsize=14, fontweight='bold')
        axes[1].legend(fontsize=10)
        axes[1].grid(True, alpha=0.3)
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"收敛曲线图已保存到：{output_path}")
    
    def plot_parameter_distribution(
        self,
        pareto_set: np.ndarray,
        param_names: List[str],
        title: str = "参数分布图",
        filename: str = "parameter_distribution.png"
    ):
        """
        绘制参数分布图
        
        Args:
            pareto_set: 帕累托解集（n x m 数组）
            param_names: 参数名称列表
            title: 图表标题
            filename: 输出文件名
        """
        n_params = len(param_names)
        n_cols = 4
        n_rows = (n_params + n_cols - 1) // n_cols
        
        fig, axes = plt.subplots(n_rows, n_cols, figsize=(20, 5 * n_rows))
        axes = axes.flatten()
        
        for i, (param_name, ax) in enumerate(zip(param_names, axes)):
            param_values = pareto_set[:, i]
            
            # 绘制直方图
            ax.hist(param_values, bins=30, density=True, alpha=0.7, color='skyblue', edgecolor='black')
            
            # 添加核密度估计（仅当有足够数据时）
            if len(np.unique(param_values)) > 1:  # 确保有多个不同值
                from scipy.stats import gaussian_kde
                try:
                    kde = gaussian_kde(param_values)
                    x_range = np.linspace(param_values.min(), param_values.max(), 200)
                    ax.plot(x_range, kde(x_range), 'r-', linewidth=2, label='Kernel Density Estimation')
                except ValueError:
                    # 如果KDE计算失败，只绘制直方图
                    pass
            
            ax.set_xlabel(param_name, fontsize=10)
            ax.set_ylabel('Density', fontsize=10)
            ax.set_title(f'{param_name} Distribution', fontsize=12, fontweight='bold')
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
        
        print(f"Parameter distribution chart saved to: {output_path}")
    
    def plot_radar_chart(
        self,
        configs: List[Dict[str, Dict[str, float]]],
        param_names: List[str],
        title: str = "配置方案对比",
        filename: str = "radar_chart.png"
    ):
        """
        绘制雷达图对比不同配置
        
        Args:
            configs: 配置列表 [{'name': '配置1', 'params': {...}}, ...]
            param_names: 参数名称列表
            title: 图表标题
            filename: 输出文件名
        """
        # 准备数据
        n_configs = len(configs)
        n_params = len(param_names)
        
        # 归一化参数
        all_values = []
        for config in configs:
            values = [config['params'][name] for name in param_names]
            all_values.extend(values)
        
        min_val = min(all_values)
        max_val = max(all_values)
        
        # 计算角度
        angles = np.linspace(0, 2 * np.pi, n_params, endpoint=False).tolist()
        angles += angles[:1]
        
        # 创建雷达图
        fig, ax = plt.subplots(figsize=(10, 10), subplot_kw=dict(projection='polar'))
        
        colors = ['red', 'blue', 'green', 'orange', 'purple']
        
        for i, config in enumerate(configs):
            values = [config['params'][name] for name in param_names]
            
            # 归一化到 [0, 1]
            normalized_values = [(v - min_val) / (max_val - min_val + 1e-10) for v in values]
            normalized_values += normalized_values[:1]
            
            # 绘制雷达图
            ax.plot(angles, normalized_values, 'o-', linewidth=2, 
                   label=config['name'], color=colors[i % len(colors)])
            ax.fill(angles, normalized_values, alpha=0.15, color=colors[i % len(colors)])
        
        # 设置标签
        ax.set_xticks(angles[:-1])
        ax.set_xticklabels(param_names, fontsize=10)
        ax.set_ylim(0, 1)
        ax.set_title(title, fontsize=14, fontweight='bold', pad=20)
        ax.legend(loc='upper right', bbox_to_anchor=(1.3, 1.1), fontsize=10)
        ax.grid(True)
        
        # 保存图表
        output_path = self.output_dir / filename
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Radar chart saved to: {output_path}")
    
    def generate_all_visualizations(
        self,
        study,
        param_names: List[str],
        configs: Optional[List[Dict]] = None
    ):
        """
        生成所有可视化图表
        
        Args:
            study: Optuna 研究对象
            param_names: 参数名称列表
            configs: 配置列表（可选）
        """
        print("\n" + "=" * 80)
        print("生成所有可视化图表")
        print("=" * 80)
        
        # 提取帕累托前沿
        best_trials = study.best_trials
        pareto_front = np.array([trial.values for trial in best_trials])
        pareto_set = np.array([list(trial.params.values()) for trial in best_trials])
        
        # 1. 帕累托前沿散点图
        self.plot_pareto_front(
            pareto_front,
            pareto_set,
            title="帕累托前沿 - 拟真度 vs 可玩性"
        )
        
        # 2. 参数敏感度热力图
        sensitivity = self._calculate_sensitivity(best_trials, param_names)
        self.plot_sensitivity_heatmap(
            sensitivity,
            title="参数敏感度分析"
        )
        
        # 3. 收敛曲线图
        trials_history = [trial for trial in study.trials]
        self.plot_convergence_curve(
            trials_history,
            title="优化收敛曲线"
        )
        
        # 4. 参数分布图
        self.plot_parameter_distribution(
            pareto_set,
            param_names,
            title="帕累托前沿参数分布"
        )
        
        # 5. 雷达图（如果提供了配置）
        if configs:
            self.plot_radar_chart(
                configs,
                param_names,
                title="配置方案对比"
            )
        
        print("\n" + "=" * 80)
        print("所有可视化图表已生成！")
        print("=" * 80)
        print(f"\n输出目录：{self.output_dir}")
    
    def _calculate_sensitivity(
        self,
        best_trials: List,
        param_names: List[str]
    ) -> Dict[str, float]:
        """
        计算参数敏感度
        
        Args:
            best_trials: 最佳试验列表
            param_names: 参数名称列表
        
        Returns:
            参数敏感度字典
        """
        sensitivity = {}
        
        for name in param_names:
            values = [trial.params[name] for trial in best_trials]
            mean_val = np.mean(values)
            std_val = np.std(values)
            cv = std_val / mean_val if mean_val != 0 else 0.0
            sensitivity[name] = cv
        
        return sensitivity


def load_optimization_results(
    json_path: str
) -> Tuple[Dict[str, float], str]:
    """
    加载优化结果
    
    Args:
        json_path: JSON 文件路径
    
    Returns:
        (params, config_name)
    """
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    params = data['parameters']
    config_name = data.get('description', 'Unknown')
    
    return params, config_name


def main():
    """主函数：生成所有可视化图表"""
    
    print("\n" + "=" * 80)
    print("RSS 可视化工具")
    print("=" * 80)
    
    # 创建可视化工具
    visualizer = RSSVisualizer()
    
    # 加载优化结果
    configs = []
    config_files = [
        "optimized_rss_config_balanced_optuna.json",
        "optimized_rss_config_realism_optuna.json",
        "optimized_rss_config_playability_optuna.json"
    ]
    
    for config_file in config_files:
        config_path = Path(config_file)
        if config_path.exists():
            params, config_name = load_optimization_results(str(config_path))
            configs.append({'name': config_name, 'params': params})
            print(f"已加载配置：{config_name}")
    
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
    
    # 生成雷达图
    if len(configs) >= 2:
        visualizer.plot_radar_chart(
            configs,
            param_names,
            title="配置方案对比"
        )
    
    print("\n可视化工具测试完成！")


if __name__ == '__main__':
    main()
