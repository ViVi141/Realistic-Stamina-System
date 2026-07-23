# Realistic Stamina System — Tools

Python 数字孪生与 **v6 优化/校验管线**（v4 保留作对照）。
**Phase-B（方案 A）**：仿真核已迁至 Rust（`rss_sim/` PyO3），Python 保留 Optuna NSGA-II 编排；`rss_sim_backend.py` 自动选用 Rust 后端并在不可用时回退 Python。
Rust CLI 入口（Phase-A）：`rust_pipeline_v6/`。

## 文件一览

| 文件 | 说明 |
|------|------|
| **`rss_pipeline_v6.py`** | **主入口**：`validate`（CI 门禁）/ `optimize`（NSGA-II） |
| `rss_constraints_v6.py` | 生理锚点硬约束 + soft 行军锚点 |
| `rss_pipeline_v4.py` | v4 三目标优化（历史对照） |
| `rss_pipeline_v5.py` | 薄包装 → `rss_pipeline_v6 validate` |
| `rss_digital_twin_fix.py` | 与 C 端对齐的数字孪生 |
| `bench_physio_anchors.py` | 锚点 bench（调用 constraints 模块） |
| `test_v6_smoke.py` | v6 CP–W′ / 代谢契约冒烟 |
| `schemas/rss_params_v5.schema.json` | v5/v6 可嵌入参数字段 |
| `optimized_rss_config_*_v4.json` | v4 三档预设（当前 C 端主要来源） |
| `optimized_rss_config_*_v6.json` | v6 优化产出预设（待 embed 到 C 端） |
| `compare_presets.py` | v4 vs v6 关键参数对比 |
| **`wb_compile_telemetry.py`** | Workbench 编译崩溃遥测（盯日志/进程，输出时间线报告） |
| `Run-WbCompileTelemetry.ps1` | 上述遥测的一键启动器 |
| `embed_json_to_c.py` | JSON → `SCR_RSS_Settings.c`（可选） |
| `rss_sim/` | **PyO3 仿真核**（`game_player_tick` / `simulate_mission` / 硬约束） |
| `rss_sim_backend.py` | 优化管线仿真后端：Rust 优先（summary_only + 并行），Python 回退 |
| `test_rss_sim_parity.py` | Rust vs Python 数值 parity 门禁 |
| `rust_pipeline_v6/` | Rust CLI 入口（`validate/calibrate/optimize/dual-run`），当前代理到 Python v6 管线 |

设计说明：`docs/RSS_v6_优化管线设计.md`

## 安装

```bash
cd tools
pip install -r requirements.txt
pip install maturin
cd rss_sim
maturin develop --release
```

未构建 `rss_sim` 时，管线自动回退到纯 Python 孪生（功能不变，速度较慢）。

## 校验（推荐 CI / 提交前）

```bash
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python test_rss_sim_parity.py
```

## Rust 入口（Phase-A 双跑）

```bash
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- validate --fast
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- dual-run --fast
```

> `dual-run` 会执行同参数下的校验输出一致性检查（Rust 入口 vs Python 入口）。

## 优化（生成 v6 预设 JSON）

```bash
python rss_pipeline_v6.py optimize --trials 400 --jobs 4 --output .
```

## v4 优化（对照）

```bash
python rss_pipeline_v4.py --trials 300 --jobs 4 --output .
python test_v4_smoke.py
```
