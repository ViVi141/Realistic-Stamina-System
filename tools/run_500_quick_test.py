#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快速验证脚本 - 500次迭代
"""

import sys
import multiprocessing
from rss_super_pipeline import RSSSuperPipeline

def main():
    print("\n" + "=" * 80)
    print("RSS 快速验证 - 500次迭代")
    print("=" * 80)
    
    # 获取CPU核心数
    n_jobs = multiprocessing.cpu_count()
    print(f"\n检测到 CPU 核心数: {n_jobs}")
    
    # 创建流水线 - 500次快速迭代
    pipeline = RSSSuperPipeline(
        n_trials=500,           # 500次迭代
        n_jobs=n_jobs,          # 使用所有可用核心
        use_database=False,     # 使用内存存储以提高性能
        batch_size=5            # 批处理大小
    )
    
    # 执行优化
    results = pipeline.optimize(study_name="quick_500_validation")
    
    # 提取预设
    archetypes = pipeline.extract_archetypes()
    
    # 导出预设配置
    pipeline.export_presets(archetypes, output_dir=".")
    
    # 导出BUG报告
    pipeline.export_bug_report("stability_bug_report_quick_500.json")
    
    # 关闭并行工作器
    pipeline.shutdown()
    
    print("\n" + "=" * 80)
    print("快速验证完成！")
    print("=" * 80)
    print("\n生成的文件：")
    print("  1. optimized_rss_config_realism_super.json - EliteStandard 预设")
    print("  2. optimized_rss_config_balanced_super.json - StandardMilsim 预设")
    print("  3. optimized_rss_config_playability_super.json - TacticalAction 预设")
    print("  4. stability_bug_report_quick_500.json - BUG报告")
    
    # 显示摘要信息
    print("\n优化结果摘要：")
    print(f"  帕累托解数量: {results['n_solutions']}")
    print(f"  总BUG数量: {len(results['bug_reports'])}")
    
    if results['n_solutions'] > 0:
        print("\n三个预设的目标值：")
        for preset_name, config in archetypes.items():
            print(f"\n  {preset_name}:")
            print(f"    可玩性负担: {config['playability_burden']:.4f}")
            print(f"    稳定性风险: {config['stability_risk']:.4f}")
            print(f"    生理学合理性: {config['physiological_realism']:.4f}")

if __name__ == '__main__':
    main()