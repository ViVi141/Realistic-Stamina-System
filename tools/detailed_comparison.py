#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C文件与优化器配置详细对比工具
对比C文件中的硬编码常量、优化器JSON配置文件、Python数字孪生文件
"""

import re
import json
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Optional


@dataclass
class ParameterValue:
    """参数值"""
    name: str
    c_hardcoded: Optional[float] = None
    json_config: Optional[Dict[str, float]] = None  # key=preset_name, value=value
    python_default: Optional[float] = None
    description: str = ""
    category: str = ""  # "优化参数" 或 "硬编码常量"


class DetailedComparator:
    """详细对比器"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.parameters: Dict[str, ParameterValue] = {}

    def parse_c_constants(self, file_path: Path) -> Dict[str, float]:
        """解析C文件常量"""
        constants = {}

        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 匹配常量定义
        pattern = r'static\s+const\s+float\s+(\w+)\s*=\s*([0-9.eE+-]+)\s*;'
        matches = re.finditer(pattern, content)

        for match in matches:
            name = match.group(1)
            value = float(match.group(2))
            constants[name] = value

        return constants

    def parse_json_configs(self, config_dir: Path) -> Dict[str, Dict[str, float]]:
        """解析JSON配置文件"""
        configs = {}

        # 读取所有JSON配置文件
        for json_file in config_dir.glob("optimized_rss_config_*.json"):
            preset_name = json_file.stem.replace("optimized_rss_config_", "")

            with open(json_file, 'r', encoding='utf-8') as f:
                data = json.load(f)

            if 'parameters' in data:
                for param_name, param_value in data['parameters'].items():
                    if isinstance(param_value, (int, float)):
                        if param_name not in configs:
                            configs[param_name] = {}
                        configs[param_name][preset_name] = float(param_value)

        return configs

    def parse_python_defaults(self, file_path: Path) -> Dict[str, float]:
        """解析Python默认值"""
        defaults = {}

        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 匹配常量定义（只匹配全大写的常量名）
        pattern = r'([A-Z_][A-Z0-9_]*)\s*=\s*([0-9.eE+-]+)'
        matches = re.finditer(pattern, content)

        for match in matches:
            name = match.group(1)
            value_str = match.group(2)

            try:
                value = float(value_str)
                defaults[name] = value
            except ValueError:
                continue

        return defaults

    def compare_all(self):
        """对比所有来源"""
        # 解析C文件
        c_constants = self.parse_c_constants(
            self.project_root / "scripts/Game/Components/Stamina/SCR_StaminaConstants.c"
        )

        # 解析JSON配置
        json_configs = self.parse_json_configs(
            self.project_root / "tools"
        )

        # 解析Python默认值
        python_defaults = self.parse_python_defaults(
            self.project_root / "tools/rss_digital_twin_fix.py"
        )

        # 收集所有参数名称
        all_names = set(c_constants.keys()) | set(json_configs.keys()) | set(python_defaults.keys())

        # 对比每个参数
        for name in all_names:
            param = ParameterValue(name=name)

            # C文件硬编码值
            if name in c_constants:
                param.c_hardcoded = c_constants[name]

            # JSON配置值
            if name in json_configs:
                param.json_config = json_configs[name]

            # Python默认值
            if name in python_defaults:
                param.python_default = python_defaults[name]

            # 判断类别
            if name in json_configs:
                param.category = "优化参数"
            else:
                param.category = "硬编码常量"

            self.parameters[name] = param

    def print_detailed_report(self):
        """打印详细报告"""
        print("=" * 100)
        print("C文件与优化器配置详细对比报告")
        print("=" * 100)
        print()

        # 1. 优化参数对比（只在JSON配置中出现的参数）
        optimized_params = [p for p in self.parameters.values() if p.category == "优化参数"]
        if optimized_params:
            print("【优化参数对比】（这些参数由优化器生成，C文件通过配置管理器动态获取）")
            print("-" * 100)
            print(f"{'参数名称':<40} {'JSON配置值':<60} {'Python默认':<15}")
            print("-" * 100)

            for param in sorted(optimized_params, key=lambda x: x.name):
                json_values = ""
                if param.json_config:
                    values = []
                    for preset, value in sorted(param.json_config.items()):
                        values.append(f"{preset}={value:.6e}")
                    json_values = ", ".join(values)

                python_val = f"{param.python_default:.6e}" if param.python_default is not None else "N/A"

                print(f"{param.name:<40} {json_values:<60} {python_val:<15}")
            print()

        # 2. 数值不匹配的优化参数
        mismatches = []
        for param in optimized_params:
            if param.python_default is not None and param.json_config:
                for preset, json_value in param.json_config.items():
                    if abs(param.python_default - json_value) > 1e-10:
                        mismatches.append((param, preset, json_value))

        if mismatches:
            print("【警告】Python默认值与JSON配置不匹配的优化参数")
            print("-" * 100)
            for param, preset, json_value in sorted(mismatches, key=lambda x: x[0].name):
                print(f"  {param.name}:")
                print(f"    Python默认: {param.python_default:.6e}")
                print(f"    {preset}配置: {json_value:.6e}")
                print(f"    相对误差:   {abs(param.python_default - json_value) / max(abs(param.python_default), abs(json_value), 1e-10) * 100:.2f}%")
            print()

        # 3. 硬编码常量对比（只在C文件中出现的参数）
        hardcoded_params = [p for p in self.parameters.values() if p.category == "硬编码常量"]
        if hardcoded_params:
            print("【硬编码常量对比】（这些参数在C文件中硬编码，不会被优化器修改）")
            print("-" * 100)
            print(f"{'参数名称':<40} {'C文件硬编码':<20} {'Python默认':<20} {'差异':<20}")
            print("-" * 100)

            mismatches_hardcoded = []
            for param in sorted(hardcoded_params, key=lambda x: x.name):
                c_value = param.c_hardcoded if param.c_hardcoded is not None else "N/A"
                py_value = param.python_default if param.python_default is not None else "N/A"

                diff = "匹配"
                if isinstance(c_value, float) and isinstance(py_value, float):
                    if abs(c_value - py_value) > 1e-10:
                        diff = f"{(c_value - py_value):.6e}"
                        mismatches_hardcoded.append(param)

                print(f"{param.name:<40} {str(c_value):<20} {str(py_value):<20} {diff:<20}")
            print()

            if mismatches_hardcoded:
                print("【严重】硬编码常量数值不匹配")
                print("-" * 100)
                for param in mismatches_hardcoded:
                    print(f"  {param.name}:")
                    print(f"    C文件:  {param.c_hardcoded}")
                    print(f"    Python:  {param.python_default}")
                    print(f"    差异:    {param.c_hardcoded - param.python_default}")
                print()

        # 4. 关键静态站立消耗相关常量
        print("【关键常量】静态站立消耗相关")
        print("-" * 100)
        key_constants = [
            "PANDOLF_STATIC_COEFF_1",
            "PANDOLF_STATIC_COEFF_2",
            "CHARACTER_WEIGHT",
            "ENERGY_TO_STAMINA_COEFF",
            "BASE_RECOVERY_RATE",
            "STANDING_RECOVERY_MULTIPLIER",
            "LOAD_RECOVERY_PENALTY_COEFF",
            "LOAD_RECOVERY_PENALTY_EXPONENT",
        ]

        for const_name in key_constants:
            if const_name in self.parameters:
                param = self.parameters[const_name]
                print(f"  {const_name}:")
                if param.c_hardcoded is not None:
                    print(f"    C文件硬编码: {param.c_hardcoded}")
                if param.json_config:
                    for preset, value in sorted(param.json_config.items()):
                        print(f"    {preset}: {value:.6e}")
                if param.python_default is not None:
                    print(f"    Python默认:  {param.python_default}")
                print()

        # 5. 汇总统计
        print("=" * 100)
        print("汇总统计")
        print("=" * 100)
        print(f"  优化参数数量:  {len(optimized_params)}")
        print(f"  硬编码常量数量: {len(hardcoded_params)}")
        print(f"  总计参数数量:  {len(self.parameters)}")
        print(f"  优化参数不匹配: {len(mismatches)}")
        print(f"  硬编码常量不匹配: {len(mismatches_hardcoded)}")
        print("=" * 100)


def main():
    """主函数"""
    project_root = Path(__file__).parent.parent

    print("正在解析C文件、JSON配置和Python文件...")
    comparator = DetailedComparator(project_root)
    comparator.compare_all()

    print("\n正在生成详细报告...")
    comparator.print_detailed_report()

    # 保存报告到文件
    report_file = project_root / "tools" / "detailed_comparison_report.txt"
    with open(report_file, 'w', encoding='utf-8') as f:
        import sys
        original_stdout = sys.stdout
        sys.stdout = f
        comparator.print_detailed_report()
        sys.stdout = original_stdout

    print(f"\n详细报告已保存到: {report_file}")


if __name__ == "__main__":
    main()