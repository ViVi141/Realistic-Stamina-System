# RSS v6 Rust Pipeline (Phase-A)

Rust entrypoint for v6 pipeline commands.

## Commands

```bash
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- validate --fast
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- calibrate --preset EliteStandard --hours 4 --target 0.2
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- optimize --trials 400 --jobs 4 --output tools/
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- dual-run --fast
```

## Scope

- **Phase-A**：CLI 代理到 Python v6（`dual-run` 校验入口一致性）。
- **Phase-B（方案 A）**：仿真核已独立为 `tools/rss_sim`（PyO3）；`optimize` 的 trial 仿真经 `rss_sim_backend` 走 Rust，Optuna 仍在 Python。

构建 Rust 仿真核后，`validate --fast` 与 `optimize` 自动加速；未构建时行为与纯 Python 一致。
