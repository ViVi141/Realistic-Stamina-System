#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
将JSON配置嵌入到C文件中的脚本
自动更新SCR_RSS_Settings.c中的预设参数
"""

import json
import re
from pathlib import Path
from typing import Dict, Any


class JsonToCEmbedder:
    """JSON到C文件的嵌入器"""
    
    def __init__(self, c_file_path: str):
        """
        初始化嵌入器
        
        Args:
            c_file_path: C文件路径
        """
        self.c_file_path = Path(c_file_path)
        self.c_content = self.c_file_path.read_text(encoding='utf-8')
        self.json_data = {}
        
        # 参数映射表：JSON参数名 -> C变量名
        self.param_mapping = {
            'energy_to_stamina_coeff': 'energy_to_stamina_coeff',
            'base_recovery_rate': 'base_recovery_rate',
            'standing_recovery_multiplier': 'standing_recovery_multiplier',
            'prone_recovery_multiplier': 'prone_recovery_multiplier',
            'load_recovery_penalty_coeff': 'load_recovery_penalty_coeff',
            'load_recovery_penalty_exponent': 'load_recovery_penalty_exponent',
            'encumbrance_speed_penalty_coeff': 'encumbrance_speed_penalty_coeff',
            'encumbrance_speed_penalty_exponent': 'encumbrance_speed_penalty_exponent',
            'encumbrance_speed_penalty_max': 'encumbrance_speed_penalty_max',
            'encumbrance_stamina_drain_coeff': 'encumbrance_stamina_drain_coeff',
            'sprint_stamina_drain_multiplier': 'sprint_stamina_drain_multiplier',
            'fatigue_accumulation_coeff': 'fatigue_accumulation_coeff',
            'fatigue_max_factor': 'fatigue_max_factor',
            'aerobic_efficiency_factor': 'aerobic_efficiency_factor',
            'anaerobic_efficiency_factor': 'anaerobic_efficiency_factor',
            'recovery_nonlinear_coeff': 'recovery_nonlinear_coeff',
            'fast_recovery_multiplier': 'fast_recovery_multiplier',
            'medium_recovery_multiplier': 'medium_recovery_multiplier',
            'slow_recovery_multiplier': 'slow_recovery_multiplier',
            'marginal_decay_threshold': 'marginal_decay_threshold',
            'marginal_decay_coeff': 'marginal_decay_coeff',
            'min_recovery_stamina_threshold': 'min_recovery_stamina_threshold',
            'min_recovery_rest_time_seconds': 'min_recovery_rest_time_seconds',
            'sprint_speed_boost': 'sprint_speed_boost',
            'posture_crouch_multiplier': 'posture_crouch_multiplier',
            'posture_prone_multiplier': 'posture_prone_multiplier',
                        'jump_consecutive_penalty': 'jump_consecutive_penalty',
            'slope_uphill_coeff': 'slope_uphill_coeff',
            'slope_downhill_coeff': 'slope_downhill_coeff',
            'swimming_base_power': 'swimming_base_power',
            'swimming_encumbrance_threshold': 'swimming_encumbrance_threshold',
            'swimming_static_drain_multiplier': 'swimming_static_drain_multiplier',
            'swimming_dynamic_power_efficiency': 'swimming_dynamic_power_efficiency',
            'swimming_energy_to_stamina_coeff': 'swimming_energy_to_stamina_coeff',
            'env_heat_stress_max_multiplier': 'env_heat_stress_max_multiplier',
            'env_rain_weight_max': 'env_rain_weight_max',
            'env_wind_resistance_coeff': 'env_wind_resistance_coeff',
            'env_mud_penalty_max': 'env_mud_penalty_max',
            'env_temperature_heat_penalty_coeff': 'env_temperature_heat_penalty_coeff',
            'env_temperature_cold_recovery_penalty_coeff': 'env_temperature_cold_recovery_penalty_coeff'
        }
    
    def load_json(self, json_file_path: str):
        """
        加载JSON配置文件
        
        Args:
            json_file_path: JSON文件路径
        """
        json_path = Path(json_file_path)
        if not json_path.exists():
            raise FileNotFoundError(f"JSON文件不存在: {json_file_path}")
        
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        self.json_data = data.get('parameters', {})
        print(f"已加载JSON文件: {json_file_path}")
        print(f"参数数量: {len(self.json_data)}")
    
    def update_preset(self, preset_name: str):
        """
        更新指定预设的参数
        
        Args:
            preset_name: 预设名称 (EliteStandard, StandardMilsim, TacticalAction)
        """
        if not self.json_data:
            raise ValueError("请先加载JSON文件！")
        
        # 确定方法名
        method_name = f"Init{preset_name}Defaults"
        print(f"\n正在更新 {preset_name} 预设...")
        
        # 构建参数赋值语句
        param_assignments = []
        for json_param, c_param in self.param_mapping.items():
            if json_param in self.json_data:
                value = self.json_data[json_param]
                # 格式化数值
                if isinstance(value, float):
                    # 使用科学计数法或小数表示
                    if abs(value) < 0.001 or abs(value) > 10000:
                        formatted_value = f"{value:.16e}"
                    else:
                        formatted_value = str(value)
                else:
                    formatted_value = str(value)
                
                # 生成正确的变量名：m_EliteStandard, m_StandardMilsim, m_TacticalAction
                var_name = f"m_{preset_name}.{c_param}"
                param_assignments.append(f"\t{var_name} = {formatted_value};")
        
        # 构建方法内容
        method_pattern = rf'protected void Init{preset_name}Defaults\(bool shouldInit\)\s*\{{[^}}]*\}}'
        method_match = re.search(method_pattern, self.c_content, re.DOTALL)
        
        if not method_match:
            print(f"警告：未找到方法 {method_name}")
            return False
        
        # 提取方法内容
        old_method = method_match.group(0)
        
        # 构建新方法
        new_method = f"""protected void Init{preset_name}Defaults(bool shouldInit)
{{
\tif (!shouldInit)
\t\treturn;

\t// {preset_name} preset parameters (optimized by NSGA-II Super Pipeline)
{chr(10).join(param_assignments)}
}}"""
        
        # 替换方法
        self.c_content = self.c_content.replace(old_method, new_method)
        print(f"已更新 {preset_name} 预设的 {len(param_assignments)} 个参数")
        
        return True
    
    def save_c_file(self, backup: bool = True):
        """
        保存C文件
        
        Args:
            backup: 是否创建备份文件
        """
        if backup:
            backup_path = self.c_file_path.with_suffix('.c.backup')
            backup_path.write_text(self.c_file_path.read_text(encoding='utf-8'), encoding='utf-8')
            print(f"\n已创建备份文件: {backup_path}")
        
        # 保存更新后的文件
        self.c_file_path.write_text(self.c_content, encoding='utf-8')
        print(f"已保存更新后的C文件: {self.c_file_path}")


def main():
    """主函数"""
    print("=" * 80)
    print("JSON配置嵌入到C文件脚本")
    print("=" * 80)
    
    # 文件路径
    project_root = Path(__file__).parent.parent
    c_file_path = project_root / "scripts" / "Game" / "Components" / "Stamina" / "SCR_RSS_Settings.c"
    
    # JSON文件路径（在tools目录下）
    json_files = {
        'EliteStandard': project_root / "tools" / "optimized_rss_config_realism_super.json",
        'StandardMilsim': project_root / "tools" / "optimized_rss_config_balanced_super.json",
        'TacticalAction': project_root / "tools" / "optimized_rss_config_playability_super.json"
    }
    
    # 检查文件是否存在
    print("\n检查文件...")
    if not c_file_path.exists():
        print(f"错误：C文件不存在: {c_file_path}")
        return
    
    missing_files = []
    for preset_name, json_path in json_files.items():
        if not json_path.exists():
            missing_files.append((preset_name, json_path))
    
    if missing_files:
        print("\n警告：以下JSON文件不存在：")
        for preset_name, json_path in missing_files:
            print(f"  - {preset_name}: {json_path}")
        
        print("\n请先运行优化器生成JSON文件！")
        return
    
    print("所有文件检查通过！")
    
    # 创建嵌入器
    embedder = JsonToCEmbedder(str(c_file_path))
    
    # 逐个更新预设
    for preset_name, json_path in json_files.items():
        print(f"\n处理 {preset_name} 预设...")
        
        try:
            # 加载JSON
            embedder.load_json(str(json_path))
            
            # 更新预设
            embedder.update_preset(preset_name)
            
        except Exception as e:
            print(f"错误：处理 {preset_name} 预设时出错: {e}")
            continue
    
    # 保存C文件
    print("\n" + "=" * 80)
    print("保存C文件...")
    print("=" * 80)
    embedder.save_c_file(backup=True)
    
    print("\n" + "=" * 80)
    print("完成！")
    print("=" * 80)
    print(f"\n已更新 {len(json_files)} 个预设配置")
    print(f"C文件位置: {c_file_path}")
    print("\n下一步：")
    print("1. 检查更新后的C文件")
    print("2. 在游戏中测试新的配置")
    print("3. 根据需要调整参数")


if __name__ == "__main__":
    main()
