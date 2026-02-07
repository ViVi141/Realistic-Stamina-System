import threading
import time
from rss_super_pipeline import RSSSuperPipeline


def run_optimization_background(n_trials=1000, study_name='rss_super_1000_trials'):
    pipeline = RSSSuperPipeline(n_trials=n_trials, n_jobs=-1, use_database=True)

    def worker():
        try:
            print(f"[LongRun] 开始后台优化：{n_trials} 试验（study={study_name}），使用数据库持久化以便 checkpoint")
            pipeline.optimize(study_name=study_name)
            print("[LongRun] 优化完成")
        except Exception as e:
            print(f"[LongRun] 优化失败：{e}")

    t = threading.Thread(target=worker, daemon=True)
    t.start()

    # 监控线程：每5分钟导出当前最优预设（如果存在）
    def monitor():
        while t.is_alive():
            time.sleep(300)  # 5分钟
            try:
                if pipeline.study is not None:
                    print("[LongRun] 监控：尝试导出当前帕累托解")
                    try:
                        archetypes = pipeline.extract_archetypes()
                        pipeline.export_presets(archetypes, output_dir=f"./checkpoints_{int(time.time())}")
                        print("[LongRun] 已导出中间预设（checkpoint）")
                    except Exception as e:
                        print(f"[LongRun] 导出中间预设失败：{e}")
                else:
                    print("[LongRun] 监控：study尚未初始化，等待...")
            except Exception as e:
                print(f"[LongRun] 监控线程错误：{e}")

    m = threading.Thread(target=monitor, daemon=True)
    m.start()

    return pipeline, t, m


if __name__ == '__main__':
    run_optimization_background(n_trials=1000, study_name='rss_super_1000_trials')
    # 主线程保持活动，便于后台线程运行
    try:
        while True:
            time.sleep(60)
    except KeyboardInterrupt:
        print('\n[LongRun] 用户中断，退出')
