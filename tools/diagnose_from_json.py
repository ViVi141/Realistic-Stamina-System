#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从JSON配置文件诊断帕累托前沿问题
分析三个预设配置，检查它们是否真的完全相同
"""

import json
import numpy as np
from pathlib import Path
from typing import Dict, List


def load_configs() -> Dict[str, Dict]:
    """加载三个预设配置"""
    config_files = {
        'EliteStandard': 'optimized_rss_config_realism_super.json',
        'StandardMilsim': 'optimized_rss_config_balanced_super.json',
        'TacticalAction': 'optimized_rss_config_playability_super.json'
    }
    
    configs = {}
    script_dir = Path(__file__).parent
    
    for preset_name, filename in config_files.items():
        file_path = script_dir / filename
        if file_path.exists():
            with open(file_path, 'r', encoding='utf-8') as f:
                configs[preset_name] = json.load(f)
            print(f"[OK] 已加载: {filename}")
        else:
            print(f"[ERROR] 文件不存在: {filename}")
    
    return configs


def analyze_configs(configs: Dict[str, Dict]) -> Dict:
    """分析配置"""
    if len(configs) < 3:
        print("错误：无法加载所有配置文件")
        return None
    
    # 提取参数和目标值
    param_names = set()
    for config in configs.values():
        param_names.update(config['parameters'].keys())
    
    param_names = sorted(param_names)
    
    # 检查参数是否完全相同
    identical_params = []
    different_params = []
    
    for param_name in param_names:
        values = [config['parameters'][param_name] for config in configs.values()]
        unique_values = len(set(values))
        
        if unique_values == 1:
            identical_params.append((param_name, values[0]))
        else:
            different_params.append((param_name, values))
    
    # 检查目标值是否完全相同
    objectives = {}
    for preset_name, config in configs.items():
        objectives[preset_name] = config['objectives']
    
    realism_values = [obj['realism_loss'] for obj in objectives.values()]
    playability_values = [obj['playability_burden'] for obj in objectives.values()]
    stability_values = [obj['stability_risk'] for obj in objectives.values()]
    
    realism_unique = len(set(realism_values))
    playability_unique = len(set(playability_values))
    stability_unique = len(set(stability_values))
    
    return {
        'total_params': len(param_names),
        'identical_params': identical_params,
        'different_params': different_params,
        'objectives': {
            'realism': {'values': realism_values, 'unique': realism_unique},
            'playability': {'values': playability_values, 'unique': playability_unique},
            'stability': {'values': stability_values, 'unique': stability_unique}
        }
    }


def main():
    """主函数"""
    print("=" * 80)
    print("从JSON配置文件诊断帕累托前沿问题")
    print("=" * 80)
    print()
    
    # 加载配置
    configs = load_configs()
    
    if len(configs) < 3:
        print("\n错误：无法加载所有配置文件")
        return
    
    # 分析配置
    analysis = analyze_configs(configs)
    
    if analysis is None:
        return
    
    print("\n" + "=" * 80)
    print("诊断结果")
    print("=" * 80)
    
    # 参数分析
    print(f"\n参数分析：")
    print(f"  总参数数：{analysis['total_params']}")
    print(f"  完全相同参数数：{len(analysis['identical_params'])}")
    print(f"  有差异参数数：{len(analysis['different_params'])}")
    
    if len(analysis['different_params']) == 0:
        print(f"\n  [ERROR] 问题：所有参数完全相同！")
        print(f"     这说明三个预设选择了完全相同的解")
    else:
        print(f"\n  [OK] 有 {len(analysis['different_params'])} 个参数有差异")
        print(f"\n  有差异的参数：")
        for param_name, values in analysis['different_params']:
            print(f"    {param_name}:")
            for preset_name, value in zip(configs.keys(), values):
                print(f"      {preset_name}: {value}")
    
    # 目标值分析
    print(f"\n目标值分析：")
    obj = analysis['objectives']
    print(f"  拟真损失：")
    print(f"    值：{obj['realism']['values']}")
    print(f"    唯一值数：{obj['realism']['unique']}/{len(obj['realism']['values'])}")
    
    print(f"  可玩性负担：")
    print(f"    值：{obj['playability']['values']}")
    print(f"    唯一值数：{obj['playability']['unique']}/{len(obj['playability']['values'])}")
    
    print(f"  稳定性风险：")
    print(f"    值：{obj['stability']['values']}")
    print(f"    唯一值数：{obj['stability']['unique']}/{len(obj['stability']['values'])}")
    
    # 诊断结论
    print(f"\n" + "=" * 80)
    print("诊断结论")
    print("=" * 80)
    
    if len(analysis['different_params']) == 0 and \
       obj['realism']['unique'] == 1 and \
       obj['playability']['unique'] == 1 and \
       obj['stability']['unique'] == 1:
        print(f"\n[ERROR] 确认问题：")
        print(f"  1. 所有参数完全相同（{len(analysis['identical_params'])}个参数）")
        print(f"  2. 所有目标值完全相同")
        print(f"  3. 三个预设选择了完全相同的解")
        print(f"\n可能的原因：")
        print(f"  1. 帕累托前沿真的只有一个解")
        print(f"  2. 所有解的目标值完全相同，导致选择了同一个解")
        print(f"  3. 优化器收敛到了单一最优解")
        print(f"\n建议的解决方案：")
        print(f"  1. 增加优化迭代次数（n_trials: 3000 → 5000）")
        print(f"  2. 增加NSGA-II种群大小（population_size: 80 → 100）")
        print(f"  3. 增加变异概率（mutation_prob: 0.15 → 0.2）")
        print(f"  4. 检查目标函数设计，确保有足够的冲突")
    elif len(analysis['different_params']) < 5:
        print(f"\n[WARNING] 警告：参数多样性不足")
        print(f"  只有 {len(analysis['different_params'])} 个参数有差异")
        print(f"  建议增加优化迭代次数或调整NSGA-II参数")
    else:
        print(f"\n[OK] 配置有足够的多样性")
        print(f"  有 {len(analysis['different_params'])} 个参数有差异")


if __name__ == '__main__':
    main()
