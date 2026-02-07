#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快速验证脚本：运行 RSSSuperPipeline 少量迭代以检查稳定性与 BUG 检测
"""
import traceback

try:
    from rss_super_pipeline import RSSSuperPipeline

    print("启动快速验证: 50 次迭代，单线程 (n_jobs=1)")
    pipeline = RSSSuperPipeline(n_trials=50, n_jobs=1, use_database=False)
    results = pipeline.optimize(study_name="test_quick_verify")

    print("优化完成，提取并导出 BUG 报告...")
    pipeline.export_bug_report("test_quick_verify_bug_report.json")

    print("总结: ")
    print(f"  帕累托解数量: {results.get('n_solutions', 0)}")
    print(f"  总 BUG 数量: {len(pipeline.bug_reports)}")

except Exception as e:
    print("快速验证失败：")
    traceback.print_exc()
    raise
