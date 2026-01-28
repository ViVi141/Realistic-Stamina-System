#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C文件与优化器逻辑对比工具
用于发现C文件和Python优化器之间的逻辑、数值和常量差异
"""

import re
import json
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Optional, Any


@dataclass
class ConstantValue:
    """常量值"""
    name: str
    value: float
    source: str  # "C" 或 "Python" 或 "Both"
    c_value: Optional[float] = None
    python_value: Optional[float] = None
    description: str = ""
    relative_error: float = 0.0  # 相对误差


@dataclass
class LogicDifference:
    """逻辑差异"""
    category: str
    c_logic: str
    python_logic: str
    severity: str  # "Critical", "Warning", "Info"
    description: str
    line_numbers: Dict[str, str]  # {"C": "line:xxx", "Python": "line:xxx"}


class CParser:
    """C文件解析器"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.constants: Dict[str, ConstantValue] = {}
        self.functions: Dict[str, str] = {}

    def parse_constants_file(self, file_path: Path) -> Dict[str, ConstantValue]:
        """解析C常量文件"""
        constants = {}

        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 匹配常量定义：static const float NAME = value;
        pattern = r'static\s+const\s+float\s+(\w+)\s*=\s*([0-9.eE+-]+)\s*;'
        matches = re.finditer(pattern, content)

        for match in matches:
            name = match.group(1)
            value = float(match.group(2))

            # 提取注释
            lines_before = content[:match.start()].split('\n')
            description = ""
            if lines_before:
                last_line = lines_before[-1].strip()
                if last_line.startswith('//'):
                    description = last_line[2:].strip()

            constants[name] = ConstantValue(
                name=name,
                value=value,
                source="C",
                c_value=value,
                python_value=None,
                description=description
            )

        return constants

    def parse_function(self, file_path: Path, function_name: str) -> Optional[str]:
        """解析C函数"""
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 查找函数定义
        pattern = rf'static\s+float\s+{function_name}\s*\([^)]*\)\s*{{([^}}]*(?:{{[^}}]*}}[^}}]*)*)}}'
        match = re.search(pattern, content, re.DOTALL)

        if match:
            return match.group(0)
        return None


class PythonParser:
    """Python文件解析器"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.constants: Dict[str, ConstantValue] = {}

    def parse_constants_class(self, file_path: Path) -> Dict[str, ConstantValue]:
        """解析Python常量类"""
        constants = {}

        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 匹配常量定义：NAME = value（只匹配全大写的常量名）
        pattern = r'([A-Z_][A-Z0-9_]*)\s*=\s*([0-9.eE+-]+)'
        matches = re.finditer(pattern, content)

        for match in matches:
            name = match.group(1)
            value_str = match.group(2)

            # 验证是否为有效的数字
            try:
                value = float(value_str)
            except ValueError:
                continue  # 跳过无效的数字

            # 提取注释
            lines_before = content[:match.start()].split('\n')
            description = ""
            for line in reversed(lines_before[-3:]):
                line = line.strip()
                if line.startswith('#'):
                    description = line[1:].strip()
                    break

            constants[name] = ConstantValue(
                name=name,
                value=value,
                source="Python",
                c_value=None,
                python_value=value,
                description=description
            )

        return constants

    def parse_function(self, file_path: Path, function_name: str) -> Optional[str]:
        """解析Python函数"""
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 查找函数定义
        pattern = rf'def\s+{function_name}\s*\([^)]*\)\s*:(.*?)(?=\n\s*def\s|\nclass\s|\Z)'
        match = re.search(pattern, content, re.DOTALL)

        if match:
            return match.group(0)
        return None

    def parse_json_config(self, file_path: Path) -> Dict[str, Any]:
        """解析JSON配置文件"""
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)


class Comparator:
    """对比器"""

    def __init__(self, c_parser: CParser, python_parser: PythonParser):
        self.c_parser = c_parser
        self.python_parser = python_parser
        self.differences: List[LogicDifference] = []
        self.missing_constants: List[str] = []
        self.value_mismatches: List[ConstantValue] = []

    def compare_constants(self, c_constants: Dict[str, ConstantValue],
                         python_constants: Dict[str, ConstantValue]) -> Tuple[Dict[str, ConstantValue], List[str], List[ConstantValue]]:
        """对比常量"""
        merged = {}
        missing_in_python = []
        missing_in_c = []
        mismatches = []

        # C文件中的常量
        for name, const in c_constants.items():
            if name in python_constants:
                # 两者都有，对比数值
                c_value = const.c_value
                py_value = python_constants[name].python_value

                if c_value is not None and py_value is not None and c_value != py_value:
                    relative_error = abs(c_value - py_value) / max(abs(c_value), abs(py_value), 1e-10)
                    mismatches.append(ConstantValue(
                        name=name,
                        value=py_value,  # 使用Python值作为基准
                        source="Both",
                        c_value=c_value,
                        python_value=py_value,
                        description=const.description,
                        relative_error=relative_error
                    ))
                else:
                    merged[name] = ConstantValue(
                        name=name,
                        value=c_value,
                        source="Both",
                        c_value=c_value,
                        python_value=py_value,
                        description=const.description
                    )
            else:
                missing_in_python.append(name)

        # Python中的常量（C文件中可能没有）
        for name, const in python_constants.items():
            if name not in c_constants:
                missing_in_c.append(name)

        return merged, missing_in_python + missing_in_c, mismatches

    def compare_static_standing_cost_logic(self) -> List[LogicDifference]:
        """对比静态站立消耗逻辑"""
        differences = []

        # C文件逻辑
        c_function = self.c_parser.parse_function(
            self.c_parser.project_root / "scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c",
            "CalculateStaticStandingCost"
        )

        # Python文件逻辑
        py_function = self.python_parser.parse_function(
            self.python_parser.project_root / "tools/rss_digital_twin_fix.py",
            "_static_standing_cost"
        )

        if c_function and py_function:
            # 提取关键逻辑
            c_coeff_1 = re.search(r'PANDOLF_STATIC_COEFF_1', c_function)
            c_coeff_2 = re.search(r'PANDOLF_STATIC_COEFF_2', c_function)
            py_coeff_1 = re.search(r'PANDOLF_STATIC_COEFF_1', py_function)
            py_coeff_2 = re.search(r'PANDOLF_STATIC_COEFF_2', py_function)

            # 检查是否都使用了相同的常量
            if not (c_coeff_1 and py_coeff_1 and c_coeff_2 and py_coeff_2):
                differences.append(LogicDifference(
                    category="Static Standing Cost",
                    c_logic="使用 PANDOLF_STATIC_COEFF_1 和 PANDOLF_STATIC_COEFF_2" if c_coeff_1 else "未找到PANDOLF静态系数",
                    python_logic="使用 PANDOLF_STATIC_COEFF_1 和 PANDOLF_STATIC_COEFF_2" if py_coeff_1 else "未找到PANDOLF静态系数",
                    severity="Critical",
                    description="静态站立消耗公式的常量使用不一致",
                    line_numbers={}
                ))

        return differences

    def compare_recovery_rate_logic(self) -> List[LogicDifference]:
        """对比恢复率逻辑"""
        differences = []

        # C文件逻辑
        c_function = self.c_parser.parse_function(
            self.c_parser.project_root / "scripts/Game/Components/Stamina/SCR_StaminaRecovery.c",
            "CalculateRecoveryRate"
        )

        # Python文件逻辑
        py_function = self.python_parser.parse_function(
            self.python_parser.project_root / "tools/rss_digital_twin_fix.py",
            "_calculate_recovery_rate"
        )

        if c_function and py_function:
            # 检查是否有静态消耗保护逻辑
            c_has_protection = 'baseDrainRateByVelocity > 0.0' in c_function
            py_has_protection = 'base_drain_rate > 0.0' in py_function

            if c_has_protection != py_has_protection:
                differences.append(LogicDifference(
                    category="Recovery Rate Protection",
                    c_logic="有静态消耗保护逻辑" if c_has_protection else "缺少静态消耗保护逻辑",
                    python_logic="有静态消耗保护逻辑" if py_has_protection else "缺少静态消耗保护逻辑",
                    severity="Critical",
                    description="静态消耗保护逻辑不一致（修复后应该一致）",
                    line_numbers={}
                ))

            # 检查保护阈值
            c_threshold = re.search(r'adjustedRecoveryRate\s*<\s*([0-9.eE+-]+)', c_function)
            py_threshold = re.search(r'adjusted_recovery_rate\s*<\s*([0-9.eE+-]+)', py_function)

            if c_threshold and py_threshold:
                c_val = float(c_threshold.group(1))
                py_val = float(py_threshold.group(1))
                if c_val != py_val:
                    differences.append(LogicDifference(
                        category="Recovery Rate Threshold",
                        c_logic=f"阈值 = {c_val}",
                        python_logic=f"阈值 = {py_val}",
                        severity="Warning",
                        description=f"保护阈值不一致：C={c_val}, Python={py_val}",
                        line_numbers={}
                    ))

        return differences


def print_report(constants: Dict[str, ConstantValue],
                 missing: List[str],
                 mismatches: List[ConstantValue],
                 differences: List[LogicDifference]):
    """打印对比报告"""
    print("=" * 80)
    print("C文件与优化器逻辑对比报告")
    print("=" * 80)
    print()

    # 1. 数值不匹配
    if mismatches:
        print("【严重】数值不匹配的常量")
        print("-" * 80)
        for const in sorted(mismatches, key=lambda x: x.relative_error, reverse=True):
            print(f"  {const.name}:")
            print(f"    C文件值:        {const.c_value}")
            print(f"    Python值:       {const.python_value}")
            print(f"    相对误差:       {const.relative_error * 100:.2f}%")
            print(f"    描述:          {const.description}")
            print()
    else:
        print("【正常】没有数值不匹配的常量")
        print()

    # 2. 缺失常量
    if missing:
        print("【警告】缺失的常量")
        print("-" * 80)
        for name in sorted(missing):
            print(f"  {name}")
        print()
    else:
        print("【正常】没有缺失的常量")
        print()

    # 3. 逻辑差异
    if differences:
        print("【严重】逻辑差异")
        print("-" * 80)
        for diff in differences:
            print(f"  [{diff.severity}] {diff.category}")
            print(f"    描述: {diff.description}")
            print(f"    C文件逻辑:  {diff.c_logic}")
            print(f"    Python逻辑: {diff.python_logic}")
            print()
    else:
        print("【正常】没有逻辑差异")
        print()

    # 4. 匹配的常量
    print("【信息】匹配的常量（部分示例）")
    print("-" * 80)
    count = 0
    for name, const in sorted(constants.items()):
        if count >= 10:  # 只显示前10个
            break
        print(f"  {const.name:40s} = {const.value}")
        count += 1
    if len(constants) > 10:
        print(f"  ... (还有 {len(constants) - 10} 个)")
    print()

    print("=" * 80)
    print(f"总计: {len(constants)} 个匹配, {len(missing)} 个缺失, {len(mismatches)} 个不匹配, {len(differences)} 个逻辑差异")
    print("=" * 80)


def main():
    """主函数"""
    project_root = Path(__file__).parent.parent

    print("正在解析C文件...")
    c_parser = CParser(project_root)
    c_constants = c_parser.parse_constants_file(
        project_root / "scripts/Game/Components/Stamina/SCR_StaminaConstants.c"
    )

    print("正在解析Python文件...")
    python_parser = PythonParser(project_root)
    python_constants = python_parser.parse_constants_class(
        project_root / "tools/rss_digital_twin_fix.py"
    )

    print("正在对比...")
    comparator = Comparator(c_parser, python_parser)

    # 对比常量
    merged_constants, missing_constants, value_mismatches = comparator.compare_constants(
        c_constants, python_constants
    )

    # 对比逻辑
    static_differences = comparator.compare_static_standing_cost_logic()
    recovery_differences = comparator.compare_recovery_rate_logic()

    all_differences = static_differences + recovery_differences

    # 打印报告
    print_report(merged_constants, missing_constants, value_mismatches, all_differences)

    # 保存详细报告到文件
    report_file = project_root / "tools" / "comparison_report.txt"
    with open(report_file, 'w', encoding='utf-8') as f:
        import sys
        original_stdout = sys.stdout
        sys.stdout = f
        print_report(merged_constants, missing_constants, value_mismatches, all_differences)
        sys.stdout = original_stdout

    print(f"\n详细报告已保存到: {report_file}")


if __name__ == "__main__":
    main()