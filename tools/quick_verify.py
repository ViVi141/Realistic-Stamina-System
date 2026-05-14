#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快速验证：少量 Optuna trial，确认 rss_pipeline_v4 与数字孪生可运行。
输出写入临时目录，不覆盖仓库内 optimized_rss_config_*_v4.json。
"""
import os
import sys
import tempfile
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def main():
    try:
        from rss_pipeline_v4 import RSSOptimizerV4, extract_presets

        print("启动快速验证: 24 次 trial，单线程，加速模式（临时目录输出）")
        with tempfile.TemporaryDirectory() as tmp:
            opt = RSSOptimizerV4(n_trials=24, n_jobs=1, fast_mode=True)
            study = opt.run(study_name="rss_quick_verify")
            presets = extract_presets(study, tmp)
            n_best = len(study.best_trials) if study else 0
            print("总结:")
            print("  Pareto 前沿 trial 数: %d" % n_best)
            print("  写出预设键: %s" % (", ".join(sorted(presets.keys())) if presets else "(无)"))
    except Exception:
        print("快速验证失败：")
        traceback.print_exc()
        raise


if __name__ == "__main__":
    main()
