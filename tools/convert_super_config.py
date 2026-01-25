#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS 增强型优化配置转换工具
将 Super Pipeline 优化器生成的 JSON 配置文件转换为 C 代码，更新到 SCR_RSS_Settings.c 文件中

使用方法：
python convert_super_config.py
"""

import json
import os
import re

# 获取当前脚本目录
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# 获取项目根目录（脚本目录的父目录）
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)

# 配置文件路径（相对路径）- 使用 Super Pipeline 优化器生成的配置文件
CONFIG_FILES = {
    "EliteStandard": os.path.join(SCRIPT_DIR, "optimized_rss_config_realism_super.json"),
    "StandardMilsim": os.path.join(SCRIPT_DIR, "optimized_rss_config_balanced_super.json"),
    "TacticalAction": os.path.join(SCRIPT_DIR, "optimized_rss_config_playability_super.json")
}

# C文件路径（相对路径）
C_FILE_PATH = os.path.join(
    PROJECT_ROOT, 
    "scripts", 
    "Game", 
    "Components", 
    "Stamina", 
    "SCR_RSS_Settings.c"
)


def read_json_config(json_path):
    """读取JSON配置文件"""
    with open(json_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def format_parameter_value(param_value):
    """
    格式化参数值，确保精度和可读性
    
    Args:
        param_value: 参数值（float或int）
    
    Returns:
        格式化后的字符串
    """
    if isinstance(param_value, float):
        # 对于科学计数法，使用e格式；对于普通小数，保持合理精度
        if abs(param_value) < 0.001 or abs(param_value) >= 1000:
            formatted_value = f"{param_value:.15e}".rstrip('0').rstrip('.')
        else:
            formatted_value = f"{param_value:.15f}".rstrip('0').rstrip('.')
    else:
        formatted_value = str(param_value)
    
    return formatted_value


def update_preset_in_c_file(c_file_content, preset_name, parameters):
    """
    更新C文件中的预设参数
    
    Args:
        c_file_content: C文件内容
        preset_name: 预设名称（如 "EliteStandard", "StandardMilsim", "TacticalAction"）
        parameters: 参数字典
    
    Returns:
        更新后的C文件内容
    """
    # 找到预设初始化函数（匹配整个函数体，包括注释）
    # 使用更精确的正则表达式，匹配从函数开始到结束大括号
    preset_func_pattern = re.compile(
        rf"// 初始化 {preset_name} 预设默认值.*?protected void Init{preset_name}Defaults\(bool shouldInit\)\s*\{{.*?\n\}}",
        re.DOTALL
    )
    
    # 创建新的预设初始化函数内容
    new_preset_func = f"// 初始化 {preset_name} 预设默认值\n"
    new_preset_func += f"protected void Init{preset_name}Defaults(bool shouldInit)\n"
    new_preset_func += "{\n"
    new_preset_func += "\tif (!shouldInit)\n"
    new_preset_func += "\t\treturn;\n\n"
    new_preset_func += f"\t// {preset_name} preset parameters (optimized by NSGA-II Super Pipeline)\n"
    
    # 添加每个参数
    for param_name, param_value in parameters.items():
        # 将JSON参数名转换为C变量名（如果需要）
        c_param_name = param_name
        # 格式化数值
        formatted_value = format_parameter_value(param_value)
        
        new_preset_func += f"\tm_{preset_name}.{c_param_name} = {formatted_value};\n"
    
    new_preset_func += "}\n"
    
    # 替换C文件中的预设初始化函数
    updated_content = preset_func_pattern.sub(new_preset_func, c_file_content)
    
    return updated_content


def main():
    """主函数"""
    print("RSS 增强型优化配置转换工具")
    print("=" * 80)
    print("使用 Super Pipeline (NSGA-II) 优化器生成的配置文件 (*_super.json)")
    print("=" * 80)
    
    # 读取C文件内容
    if not os.path.exists(C_FILE_PATH):
        print(f"\n错误：找不到C文件: {C_FILE_PATH}")
        print("请确保项目结构正确")
        return
    
    with open(C_FILE_PATH, 'r', encoding='utf-8') as f:
        c_file_content = f.read()
    
    # 处理每个配置文件
    updated_presets = []
    failed_presets = []
    
    for preset_name, json_path in CONFIG_FILES.items():
        print(f"\n处理配置文件: {os.path.basename(json_path)}")
        
        # 检查文件是否存在
        if not os.path.exists(json_path):
            print(f"  警告：配置文件不存在，跳过: {json_path}")
            failed_presets.append(preset_name)
            continue
        
        # 读取JSON配置
        try:
            config_data = read_json_config(json_path)
            parameters = config_data.get("parameters", {})
            
            if not parameters:
                print(f"  警告：配置文件中没有找到参数，跳过")
                failed_presets.append(preset_name)
                continue
            
            # 显示优化信息
            objectives = config_data.get("objectives", {})
            if objectives:
                print(f"  优化目标值：")
                print(f"    拟真损失: {objectives.get('realism_loss', 'N/A'):.4f}")
                print(f"    可玩性负担: {objectives.get('playability_burden', 'N/A'):.2f}")
                print(f"    稳定性风险: {objectives.get('stability_risk', 'N/A'):.2f}")
            
            print(f"  更新预设: {preset_name} ({len(parameters)} 个参数)")
            
            # 更新C文件内容
            c_file_content = update_preset_in_c_file(c_file_content, preset_name, parameters)
            updated_presets.append(preset_name)
            
        except Exception as e:
            print(f"  错误：处理配置文件时出错: {e}")
            failed_presets.append(preset_name)
            continue
    
    # 保存更新后的C文件
    if updated_presets:
        with open(C_FILE_PATH, 'w', encoding='utf-8') as f:
            f.write(c_file_content)
        
        print("\n" + "=" * 80)
        print("配置更新完成！")
        print("=" * 80)
        print(f"更新的文件: {C_FILE_PATH}")
        print(f"成功更新的预设: {', '.join(updated_presets)}")
        
        if failed_presets:
            print(f"跳过的预设: {', '.join(failed_presets)}")
    else:
        print("\n" + "=" * 80)
        print("警告：没有成功更新任何预设！")
        print("=" * 80)
        print("请确保 Super Pipeline 优化器已生成配置文件 (*_super.json)")
        print("\n预期的文件：")
        for preset_name, json_path in CONFIG_FILES.items():
            print(f"  - {os.path.basename(json_path)} ({preset_name})")


if __name__ == "__main__":
    main()
