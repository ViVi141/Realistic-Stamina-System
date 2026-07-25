#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Kill stuck rustup curl; rebuild+run with PATH fixed."""

from __future__ import annotations

import os
import sys
import time

import paramiko

HOST = os.environ.get("RSS_VPS_HOST", "rdp.tasksmc.cn")
PORT = int(os.environ.get("RSS_VPS_PORT", "33322"))
USER = os.environ.get("RSS_VPS_USER", "vivi141")
PASSWORD = os.environ.get("RSS_VPS_PASSWORD", "")

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
if [ -f "$HOME/.cargo/env" ]; then
  # shellcheck disable=SC1091
  . "$HOME/.cargo/env"
fi
if ! command -v rustc >/dev/null 2>&1; then
  echo "Installing rustup from USTC..."
  curl -fsSL https://mirrors.ustc.edu.cn/rust-static/rustup/dist/x86_64-unknown-linux-gnu/rustup-init -o /tmp/rustup-init
  chmod +x /tmp/rustup-init
  /tmp/rustup-init -y --default-toolchain stable --profile minimal
  # shellcheck disable=SC1091
  . "$HOME/.cargo/env"
fi
echo "rustc=$(rustc --version)"
echo "cargo=$(cargo --version)"
cargo build --release --bin sim_grid_random --no-default-features 2>&1 | tee "$HOME/rss_twin_run/rust_build.log"
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then
  JOBS=$((NPROC - 1))
else
  JOBS=1
fi
echo "Running jobs=$JOBS"
./target/release/sim_grid_random --grid -n 100000 --seed 42 -j "$JOBS" --config-dir ./configs | tee "$HOME/rss_twin_run/rust_grid100k.log"
"""


def main() -> int:
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
        print("=== kill stuck install ===", flush=True)
        client.exec_command(
            "pkill -f 'rss_twin_run/run_rust.sh' || true; "
            "pkill -f 'curl.*rustup-init' || true; "
            "pkill -f rustup-init || true"
        )
        time.sleep(1)

        sftp = client.open_sftp()
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
