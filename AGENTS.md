# AGENTS.md

## Cursor Cloud specific instructions

### What this repo is

Realistic Stamina System (RSS) is an **Arma Reforger addon** (Enfusion Engine).
The game addon itself (`scripts/*.c` EnforceScript, `Prefabs/`, `Configs/`, `Assets/`,
`UI/`, `GUI/`) can only be built/run inside the **Arma Reforger Workbench on Windows**
and cannot be compiled or launched in this Linux cloud VM.

The Linux-runnable, developer-testable part lives entirely under `tools/`:
a Python "digital twin" plus the v4/v6 optimization & validation pipeline, and a thin
Rust CLI entrypoint (`tools/rust_pipeline_v6/`) that proxies to the Python v6 pipeline.

### Environment

- Python 3.12 and Rust/cargo 1.83 are preinstalled.
- Python deps (`numpy`, `optuna`) come from `tools/requirements.txt` and are installed by
  the startup update script into the user site (no virtualenv). `numpy` is already present
  in the base image; the update script only needs `optuna` + its deps.

### Lint / gate checks (fast, no deps beyond stdlib)

- `python3 tools/check_script_size.py` — EnforceScript file-size gate. Exit 0 unless a
  `.c` file exceeds 65535 bytes or has a UTF-8 BOM. TIER/WARN lines are advisory only and
  do NOT fail the check.
- `python3 tools/check_enforce_syntax.py` — bans ternary `?:`, `try/catch`, `autoptr`,
  `ScriptCaller`, generic classes, and single-line `if` bodies. `--fix` auto-wraps braces.

### Automated tests

- `python3 tools/test_v6_smoke.py` — v6 CP–W′ / metabolism smoke (12 checks).
- `python3 tools/test_v5_smoke.py`, `python3 tools/test_acft_2mile.py` — additional smoke.
- `python3 tools/rss_regression_tests.py` — Pandolf / Tobler / preset regression (9 checks).
- `python3 tools/rss_pipeline_v6.py validate --fast` — v6 CI gate (physiological anchors +
  3 presets). This is the authoritative validate gate; `--fast` still takes ~50s.

### "Run the application" (pipeline entrypoints)

- Validate (CI gate): `python3 tools/rss_pipeline_v6.py validate --fast`
- Optimize (generates v6 preset JSON, long): `python3 tools/rss_pipeline_v6.py optimize --trials 400 --jobs 4 --output .`
- Quick optimize sanity check (~50s, writes to a temp dir): `python3 tools/quick_verify.py`
- Rust entrypoint: `cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- validate --fast`
- Rust parity check (runs the Python validate twice and diffs): `cargo run --manifest-path tools/rust_pipeline_v6/Cargo.toml -- dual-run --fast`

### Non-obvious caveats

- `validate --fast` and `dual-run --fast` are **not** instant — each simulation run is
  ~50s; `dual-run` runs it twice (~2.5 min) because it re-executes the whole validate for
  parity. Do not assume they hung.
- The Rust CLI shells out to `python3` and locates the repo root by walking up to find
  `tools/rss_pipeline_v6.py`; run cargo commands from inside the repo.
- Pre-commit hook (`githooks/pre-commit`, enable with `git config core.hooksPath githooks`)
  runs `test_v6_smoke.py`, `rss_pipeline_v6.py validate --fast`, and `check_script_size.py`.
  Run these before committing to match CI expectations.
- `.c` files are EnforceScript (game code), not C; do not try to compile them with a C
  compiler.
