#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
将JSON配置嵌入到C文件中的脚本
自动更新SCR_RSS_Settings.c中的预设参数
"""

import json
import re
from pathlib import Path


class JsonToCEmbedder:
    """JSON到C文件的嵌入器"""

    # Optuna 导出的 JSON 常省略固定项；与 rss_super_pipeline / SCR_RSS_Params defvalue 对齐，
    # 缺键时仍写入 C，避免 Init*Defaults 被整段替换后丢失生理学常数或 Sprint 阈值。
    DEFAULT_PARAM_VALUES = {
        'encumbrance_speed_penalty_exponent': 1.5,
        'sprint_stamina_drain_multiplier': 3.5,
        'aerobic_efficiency_factor': 0.9,
        'anaerobic_efficiency_factor': 1.2,
        'jump_efficiency': 0.22,
        'climb_iso_efficiency': 0.12,
        'sprint_velocity_threshold': 5.5,
        'env_surface_wetness_prone_penalty': 0.15,
    }
    
    def __init__(self, c_file_path: str):
        """
        初始化嵌入器
        
        Args:
            c_file_path: C文件路径
        """
        self.c_file_path = Path(c_file_path)
        self.c_content = self.c_file_path.read_text(encoding='utf-8')
        self.json_data = {}
        
        # 参数映射表：JSON 键名 -> SCR_RSS_Params 成员名（与 SCR_RSS_Settings.c 中
        # Init*Defaults 赋值顺序、PARAMS_ARRAY_SIZE=47 对齐）。
        # JSON 缺键时：先查 DEFAULT_PARAM_VALUES（固定生理学/管线项），否则跳过并告警。
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
            'load_metabolic_dampening': 'load_metabolic_dampening',
            'max_recovery_per_tick': 'max_recovery_per_tick',
            # 优化管线常固定为 3.5；若未来 JSON 含此键则写入 C
            'sprint_stamina_drain_multiplier': 'sprint_stamina_drain_multiplier',
            'fatigue_accumulation_coeff': 'fatigue_accumulation_coeff',
            'fatigue_max_factor': 'fatigue_max_factor',
            # 默认 0.9 / 1.2；若 JSON 显式提供则嵌入
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
            'sprint_velocity_threshold': 'sprint_velocity_threshold',
            'posture_crouch_multiplier': 'posture_crouch_multiplier',
            'posture_prone_multiplier': 'posture_prone_multiplier',
            # [HARD] jump_efficiency / climb_iso_efficiency：Margaria 常数；JSON 有则写入
            'jump_efficiency': 'jump_efficiency',
            'jump_height_guess': 'jump_height_guess',
            'jump_horizontal_speed_guess': 'jump_horizontal_speed_guess',
            'climb_iso_efficiency': 'climb_iso_efficiency',
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
            'env_temperature_cold_recovery_penalty_coeff': 'env_temperature_cold_recovery_penalty_coeff',
            'env_surface_wetness_prone_penalty': 'env_surface_wetness_prone_penalty',
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
        
        # 构建参数赋值语句（JSON 优先，否则使用 DEFAULT_PARAM_VALUES）
        param_assignments = []
        missing_keys = []
        unknown_json_keys = set(self.json_data.keys()) - set(self.param_mapping.keys())
        if unknown_json_keys:
            print(f"提示：JSON 中存在未映射的键（已忽略）: {sorted(unknown_json_keys)}")

        for json_param, c_param in self.param_mapping.items():
            if json_param in self.json_data:
                value = self.json_data[json_param]
            elif json_param in self.DEFAULT_PARAM_VALUES:
                value = self.DEFAULT_PARAM_VALUES[json_param]
            else:
                missing_keys.append(json_param)
                continue

            if isinstance(value, float):
                if abs(value) < 0.001 or abs(value) > 10000:
                    formatted_value = f"{value:.16e}"
                else:
                    formatted_value = str(value)
            else:
                formatted_value = str(value)

            var_name = f"m_{preset_name}.{c_param}"
            param_assignments.append(f"\t{var_name} = {formatted_value};")

        if missing_keys:
            print(f"警告：{preset_name} 的 JSON 缺少以下可优化键（且无内置默认值），已跳过: {missing_keys}")
        
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
