#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS配置文件对比脚本
RSS Config Comparison Script

功能：
1. 对比多个配置文件的差异
2. 识别相同的配置
3. 生成对比报告
"""

import json
import sys
from pathlib import Path

# 设置输出编码为UTF-8
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))

def load_config(config_file):
    """加载配置文件"""
    with open(config_file, 'r', encoding='utf-8') as f:
        return json.load(f)

def compare_configs(config_files):
    """对比多个配置文件"""
    print("\n" + "="*80)
    print("RSS配置文件对比")
    print("="*80)

    # 加载所有配置
    configs = {}
    for config_file, config_name in config_files:
        if config_file.exists():
            configs[config_name] = load_config(config_file)
        else:
            print(f"⚠️  警告：找不到配置文件 {config_file}")

    if len(configs) < 2:
        print("\n❌ 错误：需要至少2个配置文件进行对比")
        return

    # 获取配置名称列表
    config_names = list(configs.keys())

    # 对比目标函数值
    print("\n" + "-"*80)
    print("目标函数值对比")
    print("-"*80)

    print(f"\n{'配置':<25} {'playability_burden':<20} {'stability_risk':<20}")
    print("-"*80)

    for name in config_names:
        objectives = configs[name]['objectives']
        print(f"{name:<25} {objectives['playability_burden']:<20.2f} {objectives['stability_risk']:<20.2f}")

    # 检查目标函数值是否相同
    print("\n" + "-"*80)
    print("目标函数值一致性检查")
    print("-"*80)

    first_objectives = configs[config_names[0]]['objectives']
    all_same = True

    for name in config_names[1:]:
        objectives = configs[name]['objectives']
        if objectives != first_objectives:
            all_same = False
            print(f"❌ {name} 的目标函数值与 {config_names[0]} 不同")
        else:
            print(f"✅ {name} 的目标函数值与 {config_names[0]} 相同")

    if all_same:
        print(f"\n⚠️  警告：所有配置文件的目标函数值完全相同！")
        print(f"   这可能表示：")
        print(f"   1. 优化器只找到了一个最优解")
        print(f"   2. 帕累托前沿多样性不足")

    # 对比参数值
    print("\n" + "-"*80)
    print("参数值对比（关键参数）")
    print("-"*80)

    key_params = [
        'prone_recovery_multiplier',
        'standing_recovery_multiplier',
        'fast_recovery_multiplier',
        'medium_recovery_multiplier',
        'slow_recovery_multiplier',
        'base_recovery_rate',
        'encumbrance_stamina_drain_coeff',
    ]

    print(f"\n{'参数':<35} {config_names[0]:<15}", end="")
    for name in config_names[1:]:
        print(f" {name:<15}", end="")
    print()
    print("-"*80)

    for param in key_params:
        print(f"{param:<35}", end="")
        first_value = None
        all_same_param = True

        for i, name in enumerate(config_names):
            value = configs[name]['parameters'].get(param, 'N/A')
            if i == 0:
                first_value = value
                print(f" {str(value):<15}", end="")
            else:
                if value != first_value:
                    all_same_param = False
                    print(f" {str(value):<15}", end="")
                else:
                    print(f" {'相同':<15}", end="")

        if not all_same_param:
            print(" ⚠️  参数值不同")
        else:
            print()

    # 检查所有参数是否相同
    print("\n" + "-"*80)
    print("所有参数一致性检查")
    print("-"*80)

    first_params = configs[config_names[0]]['parameters']
    all_params_same = True

    for name in config_names[1:]:
        params = configs[name]['parameters']
        if params == first_params:
            print(f"❌ {name} 的所有参数与 {config_names[0]} 完全相同")
        else:
            print(f"✅ {name} 的参数与 {config_names[0]} 有差异")
            all_params_same = False

    if all_params_same:
        print(f"\n⚠️  警告：所有配置文件的参数完全相同！")
        print(f"   这意味着优化器只生成了一个帕累托解")

    # 总结
    print("\n" + "="*80)
    print("对比总结")
    print("="*80)

    if all_same and all_params_same:
        print("\n❌ 所有配置文件完全相同！")
        print("\n原因分析：")
        print("1. 优化器只找到了一个最优解")
        print("2. 帕累托前沿多样性不足")
        print("3. 目标函数缩放导致数值精度丢失")
        print("\n解决方案：")
        print("1. 增加优化迭代次数（--trials 2000 或更多）")
        print("2. 调整NSGA-II参数（种群大小、变异概率）")
        print("3. 检查目标函数是否过于一致")
    else:
        print("\n✅ 配置文件有差异")
        print("   可以根据不同需求选择合适的配置")

    print("\n" + "="*80)

def main():
    """主函数"""
    print("\n" + "="*80)
    print("RSS配置文件对比脚本")
    print("="*80)

    config_files = [
        (project_root / "optimized_rss_config_playability_super.json", "Playability"),
        (project_root / "optimized_rss_config_balanced_super.json", "Balanced"),
        (project_root / "optimized_rss_config_realism_super.json", "Realism"),
    ]

    compare_configs(config_files)

if __name__ == "__main__":
    main()