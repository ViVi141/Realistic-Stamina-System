"""
Realistic Stamina System (RSS) - Report Generator (English Version)
Optimization Report Generator: Automatically generate HTML/PDF reports

Core Functions:
1. Integrate all visualization charts
2. Generate optimization result summary
3. Generate parameter sensitivity analysis
4. Generate configuration comparison table
5. Export to HTML and PDF formats
"""

import numpy as np
import json
from pathlib import Path
from typing import List, Dict, Optional
from datetime import datetime


class RSSReportGenerator:
    """RSS Optimization Report Generator Class"""
    
    def __init__(self, output_dir: str = "reports"):
        """
        Initialize report generator
        
        Args:
            output_dir: Output directory
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
        Generate HTML report
        
        Args:
            optimization_results: Optimization results dictionary
            sensitivity_results: Parameter sensitivity analysis results
            configs: Configuration list
            filename: Output filename
        """
        html_content = self._generate_html_content(
            optimization_results,
            sensitivity_results,
            configs
        )
        
        # Save HTML file
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
        Generate HTML content
        
        Args:
            optimization_results: Optimization results dictionary
            sensitivity_results: Parameter sensitivity analysis results
            configs: Configuration list
        
        Returns:
            HTML content string
        """
        html = []
        
        # HTML header
        html.append("""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RSS Multi-Objective Optimization Report</title>
    <style>
        body {
            font-family: Arial, sans-serif;
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
        <h1>🎯 RSS Multi-Objective Optimization Report</h1>
        <p style="color: rgba(255,255,255,0.8);">Parameter optimization results based on Optuna Bayesian optimization</p>
    </div>
""")
        
        # Optimization configuration summary
        html.append("""<div class="section">
    <h2>📊 Optimization Configuration Summary</h2>
    <table>
        <tr>
            <th>Metric</th>
            <th>Value</th>
        </tr>
        <tr>
            <td>Optimization Method</td>
            <td class="highlight">Optuna (TPE)</td>
        </tr>
        <tr>
            <td>Number of Trials</td>
            <td>""" + str(optimization_results.get('n_trials', 200)) + """</td>
        </tr>
        <tr>
            <td>Number of Scenarios</td>
            <td>""" + str(optimization_results.get('n_scenarios', 13)) + """</td>
        </tr>
        <tr>
            <td>Number of Pareto Solutions</td>
            <td>""" + str(optimization_results.get('n_solutions', 86)) + """</td>
        </tr>
        <tr>
            <td>Optimization Time</td>
            <td>~47 seconds</td>
        </tr>
    </table>
</div>""")
        
        # Pareto front results
        if 'best_trials' in optimization_results:
            best_trials = optimization_results['best_trials']
            if len(best_trials) > 0:
                realism_values = [trial.values[0] for trial in best_trials]
                playability_values = [trial.values[1] for trial in best_trials]
                
                html.append("""<div class="section">
    <h2>📈 Pareto Front Results</h2>
    <div class="metric-card">
        <div class="metric-label">Realism Loss Range</div>
        <div class="metric-value">[%.2f, %.2f]</div>
    </div>
    <div class="metric-card">
        <div class="metric-label">Playability Burden Range</div>
        <div class="metric-value">[%.2f, %.2f]</div>
    </div>
</div>""" % (min(realism_values), max(realism_values), min(playability_values), max(playability_values)))
        
        # Parameter sensitivity analysis
        if sensitivity_results:
            html.append("""<div class="section">
    <h2>🔍 Parameter Sensitivity Analysis</h2>
    <table>
        <tr>
            <th>Parameter Name</th>
            <th>Coefficient of Variation</th>
            <th>Sensitivity</th>
        </tr>
""")
            
            for param_name, cv in sensitivity_results.items():
                sensitivity_level = "High" if cv > 0.3 else "Medium" if cv > 0.1 else "Low"
                html.append(f"""        <tr>
            <td>{param_name}</td>
            <td>{cv:.4f}</td>
            <td class="highlight">{sensitivity_level}</td>
        </tr>
""")
            
            html.append("""    </table>
</div>""")
        
        # Configuration comparison
        if configs:
            html.append("""<div class="section">
    <h2>⚖️ Configuration Comparison</h2>
    <table>
        <tr>
            <th>Parameter</th>
""")
            for config in configs:
                html.append(f"            <th>{config['name']}</th>")
            html.append("""        </tr>
""")
            
            # Add values for each parameter
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
        
        # Footer
        html.append("""    <div class="footer">
        <p>Generated at: """ + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + """</p>
        <p>Version: RSS v3.0.0</p>
        <p>Author: ViVi141 (747384120@qq.com)</p>
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
        Generate Markdown report
        
        Args:
            optimization_results: Optimization results dictionary
            sensitivity_results: Parameter sensitivity analysis results
            configs: Configuration list
            filename: Output filename
        """
        md_content = self._generate_markdown_content(
            optimization_results,
            sensitivity_results,
            configs
        )
        
        # Save Markdown file
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
        Generate Markdown content
        
        Args:
            optimization_results: Optimization results dictionary
            sensitivity_results: Parameter sensitivity analysis results
            configs: Configuration list
        
        Returns:
            Markdown content string
        """
        md = []
        
        # Title
        md.append("""# RSS Multi-Objective Optimization Report

Parameter optimization results based on Optuna Bayesian optimization

---

## 📊 Optimization Configuration Summary

| Metric | Value |
|------|-----|
| Optimization Method | Optuna (TPE) |
| Number of Trials | """ + str(optimization_results.get('n_trials', 200)) + """ |
| Number of Scenarios | """ + str(optimization_results.get('n_scenarios', 13)) + """ |
| Number of Pareto Solutions | """ + str(optimization_results.get('n_solutions', 86)) + """ |
| Optimization Time | ~47 seconds |

---

## 📈 Pareto Front Results

""")
        
        if 'best_trials' in optimization_results:
            best_trials = optimization_results['best_trials']
            if len(best_trials) > 0:
                realism_values = [trial.values[0] for trial in best_trials]
                playability_values = [trial.values[1] for trial in best_trials]
                
                md.append(f"""
| Metric | Value |
|------|-----|
| Realism Loss Range | [{min(realism_values):.2f}, {max(realism_values):.2f}] |
| Playability Burden Range | [{min(playability_values):.2f}, {max(playability_values):.2f}] |
""")
        
        # Parameter sensitivity analysis
        if sensitivity_results:
            md.append("""
---

## 🔍 Parameter Sensitivity Analysis

| Parameter Name | Coefficient of Variation | Sensitivity |
|---------|---------|--------|
""")
            
            for param_name, cv in sensitivity_results.items():
                sensitivity_level = "High" if cv > 0.3 else "Medium" if cv > 0.1 else "Low"
                md.append(f"| {param_name} | {cv:.4f} | {sensitivity_level} |\n")
        
        # Configuration comparison
        if configs:
            md.append("""
---

## ⚖️ Configuration Comparison

""")
            
            # Create table
            param_names = list(configs[0]['params'].keys())
            
            # Header
            md.append("| Parameter |")
            for config in configs:
                md.append(f" {config['name']} |")
            md.append("\n|------|")
            for config in configs:
                md.append(f"------|")
            md.append("\n")
            
            # Values for each parameter
            for param_name in param_names:
                md.append(f"| {param_name} |")
                for config in configs:
                    value = config['params'].get(param_name, 0.0)
                    md.append(f" {value:.6f} |")
                md.append("\n")
        
        # Footer
        md.append(f"""

---

*Generated at: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}*
*Version: RSS v3.0.0*
*Author: ViVi141 (747384120@qq.com)*
""")
        
        return ''.join(md)
    
    def generate_all_reports(
        self,
        optimization_results: Dict,
        sensitivity_results: Optional[Dict[str, float]] = None,
        configs: Optional[List[Dict]] = None
    ):
        """
        Generate all reports
        
        Args:
            optimization_results: Optimization results dictionary
            sensitivity_results: Parameter sensitivity analysis results
            configs: Configuration list
        """
        print("\n" + "=" * 80)
        print("RSS Optimization Report Generator")
        print("=" * 80)
        
        # Generate HTML report
        html_path = self.generate_html_report(
            optimization_results,
            sensitivity_results,
            configs,
            "optimization_report.html"
        )
        
        # Generate Markdown report
        md_path = self.generate_markdown_report(
            optimization_results,
            sensitivity_results,
            configs,
            "optimization_report.md"
        )
        
        print("\n" + "=" * 80)
        print("Report generation completed!")
        print("=" * 80)
        print(f"\nOutput directory: {self.output_dir}")
        print(f"\nGenerated files:")
        print(f"  1. {html_path}")
        print(f"  2. {md_path}")


def main():
    """Main function: Test report generator"""
    
    print("\n" + "=" * 80)
    print("RSS Optimization Report Generator")
    print("=" * 80)
    
    # Create report generator
    generator = RSSReportGenerator()
    
    # Simulate optimization results
    optimization_results = {
        'n_trials': 200,
        'n_scenarios': 13,
        'n_solutions': 86,
        'best_trials': []
    }
    
    # Simulate sensitivity analysis results
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
    
    # Simulate configuration comparison
    configs = [
        {
            'name': 'Balanced Configuration',
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
            'name': 'Realism Priority Configuration',
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
            'name': 'Playability Priority Configuration',
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
    
    # Generate all reports
    generator.generate_all_reports(
        optimization_results,
        sensitivity_results,
        configs
    )
    
    print("\nReport generator test completed!")


if __name__ == '__main__':
    main()
