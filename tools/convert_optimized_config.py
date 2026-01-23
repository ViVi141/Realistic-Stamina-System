#!/usr/bin/env python3
"""
RSS 优化配置转换工具
将优化器生成的 JSON 配置文件转换为 C 代码，更新到 SCR_RSS_Settings.c 文件中

使用方法：
python convert_optimized_config.py
"""

import json
import os
import re

# 配置文件路径
CONFIG_FILES = {
    "balanced": r"c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\tools\optimized_rss_config_balanced_optuna.json",
    "playability": r"c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\tools\optimized_rss_config_playability_optuna.json",
    "realism": r"c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\tools\optimized_rss_config_realism_optuna.json"
}

# C文件路径
C_FILE_PATH = r"c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\test_rubn_speed\scripts\Game\Components\Stamina\SCR_RSS_Settings.c"

# 预设映射
PRESET_MAP = {
    "balanced": "StandardMilsim",
    "playability": "TacticalAction",
    "realism": "EliteStandard"
}


def read_json_config(json_path):
    """读取JSON配置文件"""
    with open(json_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def update_preset_in_c_file(c_file_content, preset_name, parameters):
    """更新C文件中的预设参数"""
    # 找到预设初始化函数
    preset_func_pattern = re.compile(rf"protected void Init{preset_name}Defaults\(bool shouldInit\)\s*\{{[^}}]+\}}", re.DOTALL)
    
    # 创建新的预设初始化函数内容
    new_preset_func = f"protected void Init{preset_name}Defaults(bool shouldInit)\n"
    new_preset_func += "{\n"
    new_preset_func += "\tif (!shouldInit)\n"
    new_preset_func += "\t\treturn;\n\n"
    new_preset_func += f"\t// {preset_name} preset parameters (optimized by Optuna)\n"
    
    # 添加每个参数
    for param_name, param_value in parameters.items():
        # 将JSON参数名转换为C变量名（如果需要）
        c_param_name = param_name
        new_preset_func += f"\tm_{preset_name}.{c_param_name} = {param_value};\n"
    
    new_preset_func += "}\n"
    
    # 替换C文件中的预设初始化函数
    updated_content = preset_func_pattern.sub(new_preset_func, c_file_content)
    
    return updated_content


def main():
    """主函数"""
    print("RSS 优化配置转换工具")
    print("=" * 50)
    
    # 读取C文件内容
    with open(C_FILE_PATH, 'r', encoding='utf-8') as f:
        c_file_content = f.read()
    
    # 处理每个配置文件
    for config_type, json_path in CONFIG_FILES.items():
        print(f"\n处理配置文件: {json_path}")
        
        # 读取JSON配置
        config_data = read_json_config(json_path)
        parameters = config_data.get("parameters", {})
        
        # 获取对应的预设名称
        preset_name = PRESET_MAP.get(config_type, "Custom")
        print(f"更新预设: {preset_name}")
        
        # 更新C文件内容
        c_file_content = update_preset_in_c_file(c_file_content, preset_name, parameters)
    
    # 保存更新后的C文件
    with open(C_FILE_PATH, 'w', encoding='utf-8') as f:
        f.write(c_file_content)
    
    print("\n" + "=" * 50)
    print(f"配置更新完成！")
    print(f"更新的文件: {C_FILE_PATH}")
    print(f"更新的预设: {', '.join(PRESET_MAP.values())}")


if __name__ == "__main__":
    main()
