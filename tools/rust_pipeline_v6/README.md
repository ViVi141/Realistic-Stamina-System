# RSS v6 Rust Pipeline (Phase-A migration)

Rust entrypoint for v6 pipeline commands.

## Commands

```bash
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- validate --fast
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- calibrate --preset EliteStandard --hours 4 --target 0.2
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- optimize --trials 400 --jobs 4 --output tools/
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- dual-run --fast
cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- freeze-baseline
```

## Scope

Phase-A keeps Python v6 as execution backend while switching command entry and regression flow to Rust.

`freeze-baseline` writes migration baselines into `tools/rust_pipeline_v6/baseline/`:
- `validate_fast.stdout.txt`
- `test_v6_smoke.stdout.txt`
- `calibrate_elite.stdout.txt`
- `preset_v6_schema_snapshot.json`

## Current module split

- `python_backend.rs`: current Python execution bridge
- `pipeline.rs`: CLI command routing (`validate/calibrate/optimize/dual-run`)
- `baseline.rs`: baseline freezing for migration parity checks
- `digital_twin.rs`: Rust domain types placeholder for v6 physics/state migration
- `mission.rs`: Rust mission/result domain types placeholder
- `constraints.rs`: Rust constraint report domain types placeholder
- `io.rs`: shared JSON IO and schema snapshot helpers
