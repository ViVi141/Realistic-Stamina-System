#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""运行 T1 对齐的优化器（300 trials），输出 realism 预设。"""

import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from rss_super_pipeline import RSSSuperPipeline

def main():
    start = time.time()
    pipeline = RSSSuperPipeline(
        n_trials=300,
        n_jobs=-1,
        use_database=False,
        batch_size=5,
        sprint_drop_target=(0.15, 0.40),
        enable_sprint_objective=False,
        enable_sprint_remaining_objective=True,
        sprint_remaining_penalty_scale=20000.0,
        sprint_prune_threshold=0.10,
        enable_joint_sprint_energy_constraint=True,
    )
    try:
        results = pipeline.optimize(study_name="t1_aligned_300")
        elapsed = time.time() - start
        print(f"\n优化完成，耗时 {elapsed:.0f}s ({elapsed / 60:.1f} min)")

        archetypes = pipeline.extract_archetypes()
        output_dir = SCRIPT_DIR
        pipeline.export_presets(archetypes, output_dir=str(output_dir))
        print(f"预设已导出到 {output_dir}")
    except Exception as e:
        print(f"优化失败: {e}")
        import traceback
        traceback.print_exc()
    finally:
        pipeline.shutdown()

if __name__ == "__main__":
    main()
