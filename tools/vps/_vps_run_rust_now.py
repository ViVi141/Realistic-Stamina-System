#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Resume Rust grid build+run on VPS (USTC rustup/crates)."""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path

import paramiko

HOST = os.environ.get("RSS_VPS_HOST", "rdp.tasksmc.cn")
PORT = int(os.environ.get("RSS_VPS_PORT", "33322"))
USER = os.environ.get("RSS_VPS_USER", "vivi141")
PASSWORD = os.environ.get("RSS_VPS_PASSWORD", "")

ROOT = Path(__file__).resolve().parent
SIM = ROOT / "rss_sim"

REMOTE_SH = r"""#!/bin/bash
set -euo pipefail
cd "$HOME/rss_twin_run/rss_rust_grid"
export RUSTUP_DIST_SERVER=https://mirrors.ustc.edu.cn/rust-static
export RUSTUP_UPDATE_ROOT=https://mirrors.ustc.edu.cn/rust-static/rustup
mkdir -p "$HOME/.cargo"
cat > "$HOME/.cargo/config.toml" <<'EOF'
[source.crates-io]
replace-with = "ustc"
[source.ustc]
registry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"
EOF
if ! command -v rustc >/dev/null 2>&1; then
  echo "Installing rustup from USTC..."
  curl -fsSL https://mirrors.ustc.edu.cn/rust-static/rustup/dist/x86_64-unknown-linux-gnu/rustup-init -o /tmp/rustup-init
  chmod +x /tmp/rustup-init
  /tmp/rustup-init -y --default-toolchain stable --profile minimal
fi
# shellcheck disable=SC1091
. "$HOME/.cargo/env"
echo "rustc=$(rustc --version)"
cargo build --release --bin sim_grid_random --no-default-features 2>&1 | tee "$HOME/rss_twin_run/rust_build.log"
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
echo "Running jobs=$JOBS"
./target/release/sim_grid_random --grid -n 100000 --seed 42 -j "$JOBS" --config-dir ./configs | tee "$HOME/rss_twin_run/rust_grid100k.log"
"""


def main() -> int:
    if not PASSWORD:
        print("Set RSS_VPS_PASSWORD")
        return 2

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        HOST,
        port=PORT,
        username=USER,
        password=PASSWORD,
        timeout=20,
        allow_agent=False,
        look_for_keys=False,
    )
    try:
        sftp = client.open_sftp()
        # ensure dirs
        for d in (
            "/home/vivi141/rss_twin_run/rss_rust_grid/src/bin",
            "/home/vivi141/rss_twin_run/rss_rust_grid/configs",
        ):
            try:
                sftp.stat(d)
            except OSError:
                # mkdir -p via ssh
                pass
        client.exec_command(
            "mkdir -p ~/rss_twin_run/rss_rust_grid/src/bin ~/rss_twin_run/rss_rust_grid/configs"
        )

        uploads = [
            (SIM / "Cargo.toml", "/home/vivi141/rss_twin_run/rss_rust_grid/Cargo.toml"),
            (
                SIM / "src" / "bin" / "sim_grid_random.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/bin/sim_grid_random.rs",
            ),
            (SIM / "src" / "twin.rs", "/home/vivi141/rss_twin_run/rss_rust_grid/src/twin.rs"),
            (
                SIM / "src" / "constants.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/constants.rs",
            ),
            (SIM / "src" / "lib.rs", "/home/vivi141/rss_twin_run/rss_rust_grid/src/lib.rs"),
            (SIM / "src" / "drain.rs", "/home/vivi141/rss_twin_run/rss_rust_grid/src/drain.rs"),
            (
                SIM / "src" / "metabolism.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/metabolism.rs",
            ),
            (
                SIM / "src" / "cp_wprime.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/cp_wprime.rs",
            ),
            (SIM / "src" / "fatigue.rs", "/home/vivi141/rss_twin_run/rss_rust_grid/src/fatigue.rs"),
            (SIM / "src" / "math.rs", "/home/vivi141/rss_twin_run/rss_rust_grid/src/math.rs"),
            (
                SIM / "src" / "environment.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/environment.rs",
            ),
            (
                SIM / "src" / "constraints.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/constraints.rs",
            ),
            (SIM / "src" / "mission.rs", "/home/vivi141/rss_twin_run/rss_rust_grid/src/mission.rs"),
            (
                SIM / "src" / "bin" / "sim_bug_hunt.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/bin/sim_bug_hunt.rs",
            ),
            (
                SIM / "src" / "bin" / "sim_cp_cruise.rs",
                "/home/vivi141/rss_twin_run/rss_rust_grid/src/bin/sim_cp_cruise.rs",
            ),
        ]
        for loc, rem in uploads:
            sftp.put(str(loc), rem)
            print("put", loc.name)

        for p in ROOT.glob("optimized_rss_config_*_v*.json"):
            sftp.put(str(p), f"/home/vivi141/rss_twin_run/rss_rust_grid/configs/{p.name}")
            print("put", p.name)

        with sftp.file("/home/vivi141/rss_twin_run/run_rust.sh", "w") as rf:
            rf.write(REMOTE_SH)
        sftp.chmod("/home/vivi141/rss_twin_run/run_rust.sh", 0o755)
        sftp.close()

        print("=== remote build+run ===", flush=True)
        transport = client.get_transport()
        assert transport is not None
        chan = transport.open_session()
        chan.set_combine_stderr(True)
        chan.exec_command("bash ~/rss_twin_run/run_rust.sh")
        while True:
            if chan.recv_ready():
                chunk = chan.recv(4096).decode("utf-8", errors="replace")
                sys.stdout.write(chunk)
                sys.stdout.flush()
            if chan.exit_status_ready():
                while chan.recv_ready():
                    chunk = chan.recv(4096).decode("utf-8", errors="replace")
                    sys.stdout.write(chunk)
                    sys.stdout.flush()
                break
            time.sleep(0.2)
        code = chan.recv_exit_status()
        print("\nEXIT", code)
        return code
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
