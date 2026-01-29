#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
常量含义对比脚本
检查C和Python文件中常量对应的含义是否一致
"""

import re
import os
from typing import Dict, Tuple, List


def extract_c_constant_meaning(file_path: str) -> Dict[str, Dict]:
    """
    从C文件中提取常量的含义
    
    Args:
        file_path: C文件路径
    
    Returns:
        常量名到含义信息的映射
    """
    constants = {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        current_section = ""
        
        for i, line in enumerate(lines):
            # 提取模块/部分信息
            section_match = re.match(r'\s*//\s*=+\s*(.*?)\s*=+', line)
            if section_match:
                current_section = section_match.group(1).strip()
                continue
            
            # 匹配常量定义
            const_match = re.match(r'\s*static\s+const\s+float\s+(\w+)\s*=\s*[^;]+;\s*(//.*)?', line)
            if const_match:
                name = const_match.group(1)
                inline_comment = const_match.group(2).strip() if const_match.group(2) else ""
                
                # 提取上方的注释
                comments = []
                j = i - 1
                while j >= 0:
                    prev_line = lines[j].strip()
                    if prev_line.startswith('//'):
                        comments.insert(0, prev_line[2:].strip())
                        j -= 1
                    else:
                        break
                
                constants[name] = {
                    'inline_comment': inline_comment,
                    'comments': comments,
                    'section': current_section,
                    'line': i + 1
                }
                
    except Exception as e:
        print(f"提取C文件常量含义时出错: {e}")
    
    return constants


def extract_python_constant_meaning(file_path: str) -> Dict[str, Dict]:
    """
    从Python文件中提取常量的含义
    
    Args:
        file_path: Python文件路径
    
    Returns:
        常量名到含义信息的映射
    """
    constants = {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        current_section = ""
        in_rss_constants = False
        
        for i, line in enumerate(lines):
            # 检查是否在RSSConstants类中
            if 'class RSSConstants' in line:
                in_rss_constants = True
                continue
            
            # 检查是否退出RSSConstants类
            if in_rss_constants and line.strip() and not line.startswith('    ') and not line.startswith('    #'):
                if not line.strip().startswith('#') and not line.strip().endswith(':'):
                    in_rss_constants = False
                continue
            
            # 提取模块/部分信息
            if in_rss_constants:
                section_match = re.match(r'\s*#\s*(.*?)\s*$', line)
                if section_match:
                    current_section = section_match.group(1).strip()
                    continue
            
            # 匹配常量定义
            if in_rss_constants:
                const_match = re.match(r'\s*(\w+)\s*=\s*[^#]+\s*(#.*)?', line)
                if const_match:
                    name = const_match.group(1)
                    # 只处理大写常量
                    if name.isupper() and not name.startswith('_'):
                        inline_comment = const_match.group(2).strip() if const_match.group(2) else ""
                        
                        # 提取上方的注释
                        comments = []
                        j = i - 1
                        while j >= 0:
                            prev_line = lines[j].strip()
                            if prev_line.startswith('#'):
                                comments.insert(0, prev_line[1:].strip())
                                j -= 1
                            else:
                                break
                        
                        constants[name] = {
                            'inline_comment': inline_comment,
                            'comments': comments,
                            'section': current_section,
                            'line': i + 1
                        }
                        
    except Exception as e:
        print(f"提取Python文件常量含义时出错: {e}")
    
    return constants


def compare_constant_meanings(c_meanings: Dict[str, Dict], python_meanings: Dict[str, Dict]) -> Tuple[Dict, Dict, Dict]:
    """
    对比C和Python文件中常量的含义
    
    Args:
        c_meanings: C文件中的常量含义
        python_meanings: Python文件中的常量含义
    
    Returns:
        (匹配的常量含义, C有Python无的常量含义, Python有C无的常量含义)
    """
    matched = {}
    c_only = {}
    python_only = {}
    
    # 检查C文件中的常量
    for name, meaning in c_meanings.items():
        if name in python_meanings:
            matched[name] = (meaning, python_meanings[name])
        else:
            c_only[name] = meaning
    
    # 检查Python文件中独有的常量
    for name, meaning in python_meanings.items():
        if name not in c_meanings:
            python_only[name] = meaning
    
    return matched, c_only, python_only


def analyze_meaning_consistency(c_meaning: Dict, python_meaning: Dict) -> str:
    """
    分析常量含义的一致性
    
    Args:
        c_meaning: C文件中的常量含义
        python_meaning: Python文件中的常量含义
    
    Returns:
        一致性状态
    """
    # 检查模块/部分是否一致
    if c_meaning['section'] != python_meaning['section']:
        return "⚠️  模块不一致"
    
    # 检查内联注释是否一致
    c_comment = c_meaning['inline_comment'].lower().replace('//', '').strip()
    python_comment = python_meaning['inline_comment'].lower().replace('#', '').strip()
    
    if not c_comment and not python_comment:
        return "✅  含义一致"
    
    # 简单的关键词匹配
    c_keywords = set(re.findall(r'\b\w+\b', c_comment))
    python_keywords = set(re.findall(r'\b\w+\b', python_comment))
    
    if c_keywords.intersection(python_keywords):
        return "✅  含义一致"
    else:
        return "❌  含义可能不一致"


def generate_meaning_report(matched: Dict, c_only: Dict, python_only: Dict, c_file: str, python_file: str):
    """
    生成常量含义对比报告
    
    Args:
        matched: 匹配的常量含义
        c_only: C有Python无的常量含义
        python_only: Python有C无的常量含义
        c_file: C文件路径
        python_file: Python文件路径
    """
    print("=" * 120)
    print(f"常量含义对比报告")
    print(f"C文件: {os.path.basename(c_file)}")
    print(f"Python文件: {os.path.basename(python_file)}")
    print("=" * 120)
    
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
    
    # 匹配的常量含义
    print(f"\n\n匹配的常量含义:")
    print("-" * 120)
    print(f"{'常量名':<40} {'C模块':<30} {'Python模块':<30} {'状态':<20}")
    print("-" * 120)
    
    for name, (c_meaning, python_meaning) in matched.items():
        status = analyze_meaning_consistency(c_meaning, python_meaning)
        print(f"{name:<40} {c_meaning['section'][:28]:<30} {python_meaning['section'][:28]:<30} {status:<20}")
    
    # 详细分析
    print(f"\n\n详细含义分析:")
    print("=" * 120)
    
    for name, (c_meaning, python_meaning) in matched.items():
        print(f"\n常量: {name}")
        print(f"- C文件:")
        print(f"  模块: {c_meaning['section']}")
        print(f"  行号: {c_meaning['line']}")
        print(f"  内联注释: {c_meaning['inline_comment']}")
        if c_meaning['comments']:
            print(f"  注释:")
            for comment in c_meaning['comments']:
                print(f"    - {comment}")
        
        print(f"- Python文件:")
        print(f"  模块: {python_meaning['section']}")
        print(f"  行号: {python_meaning['line']}")
        print(f"  内联注释: {python_meaning['inline_comment']}")
        if python_meaning['comments']:
            print(f"  注释:")
            for comment in python_meaning['comments']:
                print(f"    - {comment}")
        
        print(f"- 状态: {analyze_meaning_consistency(c_meaning, python_meaning)}")
        print("-" * 80)
    
    # C文件独有的常量
    if c_only:
        print(f"\n\nC文件独有的常量:")
        print("-" * 120)
        print(f"{'常量名':<40} {'模块':<60} {'行号':<20}")
        print("-" * 120)
        for name, meaning in c_only.items():
            print(f"{name:<40} {meaning['section'][:58]:<60} {meaning['line']:<20}")
    
    # Python文件独有的常量
    if python_only:
        print(f"\n\nPython文件独有的常量:")
        print("-" * 120)
        print(f"{'常量名':<40} {'模块':<60} {'行号':<20}")
        print("-" * 120)
        for name, meaning in python_only.items():
            print(f"{name:<40} {meaning['section'][:58]:<60} {meaning['line']:<20}")
    
    print("=" * 120)
    print("报告结束")
    print("=" * 120)


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
    
    print(f"提取C文件常量含义: {c_file_path}")
    print(f"提取Python文件常量含义: {python_file_path}")
    
    # 提取常量含义
    c_meanings = extract_c_constant_meaning(c_file_path)
    python_meanings = extract_python_constant_meaning(python_file_path)
    
    print(f"\n提取结果:")
    print(f"- C文件常量数: {len(c_meanings)}")
    print(f"- Python文件常量数: {len(python_meanings)}")
    
    # 对比常量含义
    matched, c_only, python_only = compare_constant_meanings(c_meanings, python_meanings)
    
    # 生成报告
    generate_meaning_report(matched, c_only, python_only, c_file_path, python_file_path)


if __name__ == "__main__":
    main()
