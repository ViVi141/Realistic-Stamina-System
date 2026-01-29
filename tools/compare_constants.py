#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
常量对比脚本
动态获取C和Python文件中的常量值并对比是否一致
"""

import re
import os
from typing import Dict, Tuple


def extract_c_constants(file_path: str) -> Dict[str, float]:
    """
    从C文件中提取常量值
    
    Args:
        file_path: C文件路径
    
    Returns:
        常量名到值的映射
    """
    constants = {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 匹配 static const float CONSTANT_NAME = value;
        pattern = r'static\s+const\s+float\s+(\w+)\s*=\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s*;'
        matches = re.findall(pattern, content)
        
        for match in matches:
            name = match[0]
            value = float(match[1])
            constants[name] = value
            
    except Exception as e:
        print(f"提取C文件常量时出错: {e}")
    
    return constants


def extract_python_constants(file_path: str) -> Dict[str, float]:
    """
    从Python文件中提取常量值
    
    Args:
        file_path: Python文件路径
    
    Returns:
        常量名到值的映射
    """
    constants = {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 匹配 CONSTANT_NAME = value
        pattern = r'(\w+)\s*=\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s*'
        matches = re.findall(pattern, content)
        
        for match in matches:
            name = match[0]
            # 跳过非常量名（如变量、函数参数等）
            if name.isupper() and not name.startswith('_'):
                try:
                    value = float(match[1])
                    constants[name] = value
                except ValueError:
                    pass  # 跳过非数值常量
                    
    except Exception as e:
        print(f"提取Python文件常量时出错: {e}")
    
    return constants


def compare_constants(c_constants: Dict[str, float], python_constants: Dict[str, float]) -> Tuple[Dict, Dict, Dict]:
    """
    对比C和Python文件中的常量值
    
    Args:
        c_constants: C文件中的常量
        python_constants: Python文件中的常量
    
    Returns:
        (匹配的常量, C有Python无的常量, Python有C无的常量)
    """
    matched = {}
    c_only = {}
    python_only = {}
    
    # 检查C文件中的常量
    for name, value in c_constants.items():
        if name in python_constants:
            matched[name] = (value, python_constants[name])
        else:
            c_only[name] = value
    
    # 检查Python文件中独有的常量
    for name, value in python_constants.items():
        if name not in c_constants:
            python_only[name] = value
    
    return matched, c_only, python_only


def generate_report(matched: Dict, c_only: Dict, python_only: Dict, c_file: str, python_file: str):
    """
    生成对比报告
    
    Args:
        matched: 匹配的常量
        c_only: C有Python无的常量
        python_only: Python有C无的常量
        c_file: C文件路径
        python_file: Python文件路径
    """
    print("=" * 80)
    print(f"常量对比报告")
    print(f"C文件: {os.path.basename(c_file)}")
    print(f"Python文件: {os.path.basename(python_file)}")
    print("=" * 80)
    
    # 统计信息
    total_matched = len(matched)
    total_c_only = len(c_only)
    total_python_only = len(python_only)
    total_constants = total_matched + total_c_only + total_python_only
    
    print(f"\n统计信息:")
    print(f"- 总常量数: {total_constants}")
    print(f"- 匹配的常量: {total_matched}")
    print(f"- C文件独有: {total_c_only}")
    print(f"- Python文件独有: {total_python_only}")
    
    # 匹配的常量
    print(f"\n\n匹配的常量:")
    print("-" * 80)
    print(f"{'常量名':<40} {'C值':<20} {'Python值':<20} {'状态':<10}")
    print("-" * 80)
    
    for name, (c_value, python_value) in matched.items():
        if abs(c_value - python_value) < 1e-6:
            status = "✅ 一致"
        else:
            status = "❌ 不一致"
        print(f"{name:<40} {c_value:<20} {python_value:<20} {status:<10}")
    
    # C文件独有的常量
    if c_only:
        print(f"\n\nC文件独有的常量:")
        print("-" * 80)
        print(f"{'常量名':<40} {'值':<20}")
        print("-" * 80)
        for name, value in c_only.items():
            print(f"{name:<40} {value:<20}")
    
    # Python文件独有的常量
    if python_only:
        print(f"\n\nPython文件独有的常量:")
        print("-" * 80)
        print(f"{'常量名':<40} {'值':<20}")
        print("-" * 80)
        for name, value in python_only.items():
            print(f"{name:<40} {value:<20}")
    
    print("=" * 80)
    print("报告结束")
    print("=" * 80)


def main():
    """
    主函数
    """
    # 文件路径
    c_file = r'scripts/Game/Components/Stamina/SCR_StaminaConstants.c'
    python_file = r'tools/rss_digital_twin_fix.py'
    
    # 获取绝对路径
    base_dir = os.path.dirname(os.path.abspath(__file__))
    base_dir = os.path.dirname(base_dir)  # 上一级目录
    
    c_file_path = os.path.join(base_dir, c_file)
    python_file_path = os.path.join(base_dir, python_file)
    
    print(f"提取C文件常量: {c_file_path}")
    print(f"提取Python文件常量: {python_file_path}")
    
    # 提取常量
    c_constants = extract_c_constants(c_file_path)
    python_constants = extract_python_constants(python_file_path)
    
    print(f"\n提取结果:")
    print(f"- C文件常量数: {len(c_constants)}")
    print(f"- Python文件常量数: {len(python_constants)}")
    
    # 对比常量
    matched, c_only, python_only = compare_constants(c_constants, python_constants)
    
    # 生成报告
    generate_report(matched, c_only, python_only, c_file_path, python_file_path)


if __name__ == "__main__":
    main()
