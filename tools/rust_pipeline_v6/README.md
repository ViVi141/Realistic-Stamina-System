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

Phase-A keeps Python v6 as execution backend while switching command entry and regression flow to Rust.
