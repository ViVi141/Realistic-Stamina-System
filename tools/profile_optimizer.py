#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
性能分析脚本
使用cProfile分析优化器的性能瓶颈
"""

import cProfile
import pstats
import sys
from pathlib import Path

# 添加当前目录到路径
sys.path.insert(0, str(Path(__file__).parent))

from rss_super_pipeline import RSSSuperPipeline

def profile_optimizer():
    """
    分析优化器性能
    """
    print("开始性能分析...")
    print("=" * 80)
    
    # 创建优化器实例，使用较小的迭代次数进行分析
    pipeline = RSSSuperPipeline(
        n_trials=100,  # 使用较小的迭代次数进行分析
        n_jobs=-1,      # 使用所有CPU核心
        use_database=False
    )
    
    # 使用cProfile进行性能分析
    profiler = cProfile.Profile()
    profiler.enable()
    
    try:
        # 运行优化
        results = pipeline.optimize(study_name="profile_study")
        print(f"\n优化完成，找到 {len(results['best_trials'])} 个帕累托解")
    except Exception as e:
        print(f"优化过程中出错: {e}")
    finally:
        profiler.disable()
        
        # 保存分析结果
        stats_file = "profile_results.prof"
        profiler.dump_stats(stats_file)
        print(f"\n性能分析结果已保存到: {stats_file}")
        
        # 打印分析结果
        print("\n性能分析结果 (前20个耗时函数):")
        print("=" * 80)
        stats = pstats.Stats(profiler)
        stats.sort_stats('cumulative')
        stats.print_stats(20)

if __name__ == "__main__":
    profile_optimizer()
