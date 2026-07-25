# Realistic Stamina System — Tools

Python 数字孪生与 **v6 优化/校验管线**（v4 保留作对照）。  
**Phase-B**：仿真核在 Rust（`rss_sim/` PyO3），Python 保留 Optuna；`rss_sim_backend.py` 优先 Rust、不可用则回退 Python。  
Rust CLI：`rust_pipeline_v6/`。

## 目录结构

```text
tools/
  README.md / requirements.txt
  # —— 日常入口（保持在根目录，便于 import）——
  rss_pipeline_v6.py          # validate / optimize / anchors
  rss_pipeline_v4.py / v5.py
  rss_digital_twin_fix.py
  rss_constraints_v6.py / rss_anchors_v6.py / rss_sim_backend.py
  test_v6_smoke.py / test_v4_smoke.py / test_v5_smoke.py
  test_rss_random_scenarios.py / test_rss_sim_parity.py
  optimized_rss_config_*_{v4,v6}.json
  embed_json_to_c.py / compare_presets.py / check_*.py
  bench_physio_anchors.py / bench_rss_sim_backend.py
  bench_standard_30kg_*.py / regen_vt_standard.py
  wb_compile_telemetry.py / Run-WbCompileTelemetry.ps1
  rss_sim/                    # PyO3 + sim_grid_random
  rust_pipeline_v6/
  schemas/

  artifacts/                  # 生成物（勿当源码）
    vt/                       # V-T JSON / PNG
    diagnostics/              # 诊断图与旧 mission JSON
  viz/                        # V-T canvas / PNG 辅助脚本
  vps/                        # VPS 部署与网格跑批
  archive/                    # bisect 备份 + legacy 脚本
```

设计说明：`docs/RSS_v6_优化管线设计.md`

## 安装

```bash
cd tools
pip install -r requirements.txt
pip install maturin
cd rss_sim
maturin develop --release
```

未构建 `rss_sim` 时，管线自动回退到纯 Python 孪生。

## 校验（推荐 CI / 提交前）

```bash
python rss_pipeline_v6.py validate
python test_v6_smoke.py
python test_rss_sim_parity.py
python test_rss_random_scenarios.py --quick
```

网格抽样（多核）：

```bash
python test_rss_random_scenarios.py --grid --n 100000 --seed 42 -j 0
```

Rust 网格（更快）：

```bash
cargo run --manifest-path rss_sim/Cargo.toml --release --bin sim_grid_random --no-default-features -- --grid -n 100000 -j 0 --config-dir .
```

## Rust 入口（Phase-A 双跑）

```bash
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- validate --fast
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- dual-run --fast
```

## 优化（生成 v6 预设 JSON）

```bash
python rss_pipeline_v6.py anchors
python rss_pipeline_v6.py optimize --trials 400 --jobs 4 --output .
```

## V-T 图

```bash
python regen_vt_standard.py
# 产物：artifacts/vt/
# 可选：python viz/_plot_vt_standard.py / viz/_emit_vt_canvas.py
```

## v4 优化（对照）

```bash
python rss_pipeline_v4.py --trials 300 --jobs 4 --output .
python test_v4_smoke.py
```
