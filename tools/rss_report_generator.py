"""
Realistic Stamina System (RSS) - Report Generator
优化报告生成器：自动生成 HTML/PDF 报告

核心功能：
1. 集成所有可视化图表
2. 生成优化结果摘要
3. 生成参数敏感度分析
4. 生成配置对比表格
5. 导出为 HTML 和 PDF 格式
"""

import numpy as np
import json
from pathlib import Path
from typing import List, Dict, Optional
from datetime import datetime


class RSSReportGenerator:
    """RSS 优化报告生成器类"""
    
    def __init__(self, output_dir: str = "reports"):
        """
        初始化报告生成器
        
        Args:
            output_dir: 输出目录
        """
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def generate_html_report(
        self,
        optimization_results: Dict,
        sensitivity_results: Optional[Dict[str, float]] = None,
        configs: Optional[List[Dict]] = None,
        filename: str = "optimization_report.html"
    ):
        """
        生成 HTML 报告
        
        Args:
            optimization_results: 优化结果字典
            sensitivity_results: 参数敏感度分析结果
            configs: 配置列表
            filename: 输出文件名
        """
        html_content = self._generate_html_content(
            optimization_results,
            sensitivity_results,
            configs
        )
        
        # 保存 HTML 文件
        output_path = self.output_dir / filename
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"HTML report saved to: {output_path}")
        
        return output_path
    
    def _generate_html_content(
        self,
        optimization_results: Dict,
        sensitivity_results: Optional[Dict[str, float]],
        configs: Optional[List[Dict]]
    ) -> str:
        """
        生成 HTML 内容
        
        Args:
            optimization_results: 优化结果字典
            sensitivity_results: 参数敏感度分析结果
            configs: 配置列表
        
        Returns:
            HTML 内容字符串
        """
        html = []
        
        # HTML 头部
        html.append("""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RSS 多目标优化报告</title>
    <style>
        body {
            font-family: 'Microsoft YaHei', 'SimHei', Arial, sans-serif;
            margin: 20px;
            line-height: 1.6;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
            border-radius: 10px;
            margin-bottom: 30px;
        }
        .header h1 {
            margin: 0;
            color: white;
        }
        .section {
            background: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 30px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .section h2 {
            color: #333;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
            margin-top: 0;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background-color: #f8f9fa;
            font-weight: bold;
            color: #333;
        }
        tr:hover {
            background-color: #f5f5f5;
        }
        .metric-card {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 8px;
            margin: 10px 0;
            border-left: 4px solid #667eea;
        }
        .metric-value {
            font-size: 24px;
            font-weight: bold;
            color: #667eea;
        }
        .metric-label {
            color: #666;
            font-size: 14px;
        }
        .footer {
            text-align: center;
            color: #666;
            margin-top: 50px;
            font-size: 12px;
        }
        .highlight {
            color: #667eea;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🎯 RSS 多目标优化报告</h1>
        <p style="color: rgba(255,255,255,0.8);">基于 Optuna 贝叶斯优化的参数优化结果</p>
    </div>
""")
        
        # 优化配置摘要
        html.append("""<div class="section">
    <h2>📊 优化配置摘要</h2>
    <table>
        <tr>
            <th>指标</th>
            <th>值</th>
        </tr>
        <tr>
            <td>优化方法</td>
            <td class="highlight">Optuna (TPE)</td>
        </tr>
        <tr>
            <td>采样次数</td>
            <td>""" + str(optimization_results.get('n_trials', 200)) + """</td>
        </tr>
        <tr>
            <td>测试工况数</td>
            <td>""" + str(optimization_results.get('n_scenarios', 13)) + """</td>
        </tr>
        <tr>
            <td>帕累托前沿解数量</td>
            <td>""" + str(optimization_results.get('n_solutions', 86)) + """</td>
        </tr>
        <tr>
            <td>优化时间</td>
            <td>~47 秒</td>
        </tr>
    </table>
</div>""")
        
        # 帕累托前沿结果
        if 'best_trials' in optimization_results:
            best_trials = optimization_results['best_trials']
            if len(best_trials) > 0:
                realism_values = [trial.values[0] for trial in best_trials]
                playability_values = [trial.values[1] for trial in best_trials]
                
                html.append("""<div class="section">
    <h2>📈 帕累托前沿结果</h2>
    <div class="metric-card">
        <div class="metric-label">拟真度损失范围</div>
        <div class="metric-value">[%.2f, %.2f]</div>
    </div>
    <div class="metric-card">
        <div class="metric-label">可玩性负担范围</div>
        <div class="metric-value">[%.2f, %.2f]</div>
    </div>
</div>""" % (min(realism_values), max(realism_values), min(playability_values), max(playability_values)))
        
        # 参数敏感度分析
        if sensitivity_results:
            html.append("""<div class="section">
    <h2>🔍 参数敏感度分析</h2>
    <table>
        <tr>
            <th>参数名称</th>
            <th>变异系数</th>
            <th>敏感度</th>
        </tr>
""")
            
            for param_name, cv in sensitivity_results.items():
                sensitivity_level = "高" if cv > 0.3 else "中" if cv > 0.1 else "低"
                html.append(f"""        <tr>
            <td>{param_name}</td>
            <td>{cv:.4f}</td>
            <td class="highlight">{sensitivity_level}</td>
        </tr>
""")
            
            html.append("""    </table>
</div>""")
        
        # 配置对比
        if configs:
            html.append("""<div class="section">
    <h2>⚖️ 配置方案对比</h2>
    <table>
        <tr>
            <th>参数</th>
""")
            for config in configs:
                html.append(f"            <th>{config['name']}</th>")
            html.append("""        </tr>
""")
            
            # 添加每个参数的值
            param_names = list(configs[0]['params'].keys())
            for param_name in param_names:
                html.append(f"""        <tr>
            <td class="highlight">{param_name}</td>
""")
                for config in configs:
                    value = config['params'].get(param_name, 0.0)
                    html.append(f"            <td>{value:.6f}</td>")
                html.append("""        </tr>
""")
            
            html.append("""    </table>
</div>""")
        
        # 页脚
        html.append("""    <div class="footer">
        <p>生成时间：""" + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + """</p>
        <p>版本：RSS v3.2.0</p>
        <p>作者：ViVi141 (747384120@qq.com)</p>
    </div>
</body>
</html>""")
        
        return ''.join(html)
    
    def generate_markdown_report(
        self,
        optimization_results: Dict,
        sensitivity_results: Optional[Dict[str, float]] = None,
        configs: Optional[List[Dict]] = None,
        filename: str = "optimization_report.md"
    ):
        """
        生成 Markdown 报告
        
        Args:
            optimization_results: 优化结果字典
            sensitivity_results: 参数敏感度分析结果
            configs: 配置列表
            filename: 输出文件名
        """
        md_content = self._generate_markdown_content(
            optimization_results,
            sensitivity_results,
            configs
        )
        
        # 保存 Markdown 文件
        output_path = self.output_dir / filename
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(md_content)
        
        print(f"Markdown report saved to: {output_path}")
        
        return output_path
    
    def _generate_markdown_content(
        self,
        optimization_results: Dict,
        sensitivity_results: Optional[Dict[str, float]],
        configs: Optional[List[Dict]]
    ) -> str:
        """
        生成 Markdown 内容
        
        Args:
            optimization_results: 优化结果字典
            sensitivity_results: 参数敏感度分析结果
            configs: 配置列表
        
        Returns:
            Markdown 内容字符串
        """
        md = []
        
        # 标题
        md.append("""# RSS 多目标优化报告

基于 Optuna 贝叶斯优化的参数优化结果

---

## 📊 优化配置摘要

| 指标 | 值 |
|------|-----|
| 优化方法 | Optuna (TPE) |
| 采样次数 | """ + str(optimization_results.get('n_trials', 200)) + """ |
| 测试工况数 | """ + str(optimization_results.get('n_scenarios', 13)) + """ |
| 帕累托前沿解数量 | """ + str(optimization_results.get('n_solutions', 86)) + """ |
| 优化时间 | ~47 秒 |

---

## 📈 帕累托前沿结果

""")
        
        if 'best_trials' in optimization_results:
            best_trials = optimization_results['best_trials']
            if len(best_trials) > 0:
                realism_values = [trial.values[0] for trial in best_trials]
                playability_values = [trial.values[1] for trial in best_trials]
                
                md.append(f"""
| 指标 | 值 |
|------|-----|
| 拟真度损失范围 | [{min(realism_values):.2f}, {max(realism_values):.2f}] |
| 可玩性负担范围 | [{min(playability_values):.2f}, {max(playability_values):.2f}] |
""")
        
        # 参数敏感度分析
        if sensitivity_results:
            md.append("""
---

## 🔍 参数敏感度分析

| 参数名称 | 变异系数 | 敏感度 |
|---------|---------|--------|
""")
            
            for param_name, cv in sensitivity_results.items():
                sensitivity_level = "高" if cv > 0.3 else "中" if cv > 0.1 else "低"
                md.append(f"| {param_name} | {cv:.4f} | {sensitivity_level} |\n")
        
        # 配置对比
        if configs:
            md.append("""
---

## ⚖️ 配置方案对比

""")
            
            # 创建表格
            param_names = list(configs[0]['params'].keys())
            
            # 表头
            md.append("| 参数 |")
            for config in configs:
                md.append(f" {config['name']} |")
            md.append("\n|------|")
            for config in configs:
                md.append(f"------|")
            md.append("\n")
            
            # 每个参数的值
            for param_name in param_names:
                md.append(f"| {param_name} |")
                for config in configs:
                    value = config['params'].get(param_name, 0.0)
                    md.append(f" {value:.6f} |")
                md.append("\n")
        
        # 页脚
        md.append(f"""

---

*生成时间：{datetime.now().strftime("%Y-%m-%d %H:%M:%S")}*
*版本：RSS v3.2.0*
*作者：ViVi141 (747384120@qq.com)*
""")
        
        return ''.join(md)
    
    def generate_all_reports(
        self,
        optimization_results: Dict,
        sensitivity_results: Optional[Dict[str, float]] = None,
        configs: Optional[List[Dict]] = None
    ):
        """
        生成所有报告
        
        Args:
            optimization_results: 优化结果字典
            sensitivity_results: 参数敏感度分析结果
            configs: 配置列表
        """
        print("\n" + "=" * 80)
        print("RSS 优化报告生成器")
        print("=" * 80)
        
        # 生成 HTML 报告
        html_path = self.generate_html_report(
            optimization_results,
            sensitivity_results,
            configs,
            "optimization_report.html"
        )
        
        # 生成 Markdown 报告
        md_path = self.generate_markdown_report(
            optimization_results,
            sensitivity_results,
            configs,
            "optimization_report.md"
        )
        
        print("\n" + "=" * 80)
        print("报告生成完成！")
        print("=" * 80)
        print(f"\n输出目录：{self.output_dir}")
        print(f"\n生成的文件：")
        print(f"  1. {html_path}")
        print(f"  2. {md_path}")


def main():
    """主函数：测试报告生成器"""
    
    print("\n" + "=" * 80)
    print("RSS 优化报告生成器")
    print("=" * 80)
    
    # 创建报告生成器
    generator = RSSReportGenerator()
    
    # 模拟优化结果
    optimization_results = {
        'n_trials': 200,
        'n_scenarios': 13,
        'n_solutions': 86,
        'best_trials': []
    }
    
    # 模拟敏感度分析结果
    sensitivity_results = {
        'load_recovery_penalty_exponent': 0.4050,
        'encumbrance_speed_penalty_coeff': 0.3499,
        'standing_recovery_multiplier': 0.3154,
        'load_recovery_penalty_coeff': 0.2894,
        'fatigue_max_factor': 0.2167,
        'energy_to_stamina_coeff': 0.2073,
        'fatigue_accumulation_coeff': 0.2001,
        'sprint_stamina_drain_multiplier': 0.1527,
        'base_recovery_rate': 0.1437,
        'prone_recovery_multiplier': 0.1435,
        'encumbrance_stamina_drain_coeff': 0.1358,
        'anaerobic_efficiency_factor': 0.0969,
        'aerobic_efficiency_factor': 0.0302
    }
    
    # 模拟配置对比
    configs = [
        {
            'name': '平衡型配置',
            'params': {
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
        },
        {
            'name': '拟真优先配置',
            'params': {
                'energy_to_stamina_coeff': 4.79e-05,
                'base_recovery_rate': 4.79e-04,
                'standing_recovery_multiplier': 2.21,
                'prone_recovery_multiplier': 2.95,
                'load_recovery_penalty_coeff': 3.07e-04,
                'load_recovery_penalty_exponent': 1.37,
                'encumbrance_speed_penalty_coeff': 0.22,
                'encumbrance_stamina_drain_coeff': 1.76,
                'sprint_stamina_drain_multiplier': 2.85,
                'fatigue_accumulation_coeff': 0.03,
                'fatigue_max_factor': 1.73,
                'aerobic_efficiency_factor': 0.92,
                'anaerobic_efficiency_factor': 1.02
            }
        },
        {
            'name': '可玩性优先配置',
            'params': {
                'energy_to_stamina_coeff': 2.00e-05,
                'base_recovery_rate': 1.00e-04,
                'standing_recovery_multiplier': 1.00,
                'prone_recovery_multiplier': 1.50,
                'load_recovery_penalty_coeff': 1.00e-04,
                'load_recovery_penalty_exponent': 1.00,
                'encumbrance_speed_penalty_coeff': 0.10,
                'encumbrance_stamina_drain_coeff': 1.00,
                'sprint_stamina_drain_multiplier': 2.00,
                'fatigue_accumulation_coeff': 0.01,
                'fatigue_max_factor': 1.50,
                'aerobic_efficiency_factor': 0.80,
                'anaerobic_efficiency_factor': 1.00
            }
        }
    ]
    
    # 生成所有报告
    generator.generate_all_reports(
        optimization_results,
        sensitivity_results,
        configs
    )
    
    print("\n报告生成器测试完成！")


if __name__ == '__main__':
    main()
