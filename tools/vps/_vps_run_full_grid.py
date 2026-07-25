#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Upload sim_grid_random, rebuild, launch full-grid under nohup on VPS."""

from __future__ import annotations

import os
import sys
import time

import paramiko

HOST = os.environ.get("RSS_VPS_HOST", "rdp.tasksmc.cn")
PORT = int(os.environ.get("RSS_VPS_PORT", "33322"))
USER = os.environ.get("RSS_VPS_USER", "vivi141")
PASSWORD = os.environ.get("RSS_VPS_PASSWORD", "")

LOCAL_BIN = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "rss_sim",
    "src",
    "bin",
    "sim_grid_random.rs",
)
REMOTE_BIN = "/home/vivi141/rss_twin_run/rss_rust_grid/src/bin/sim_grid_random.rs"
REMOTE_SH = "/home/vivi141/rss_twin_run/run_full_grid.sh"

LAUNCH = r"""#!/bin/bash
set -euo pipefail
cd "$HOME/rss_twin_run/rss_rust_grid"
# shellcheck disable=SC1091
. "$HOME/.cargo/env"
export RUSTUP_DIST_SERVER=https://mirrors.ustc.edu.cn/rust-static
export RUSTUP_UPDATE_ROOT=https://mirrors.ustc.edu.cn/rust-static/rustup
mkdir -p "$HOME/.cargo"
cat > "$HOME/.cargo/config.toml" <<'EOF'
[source.crates-io]
replace-with = "ustc"
[source.ustc]
registry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"
EOF
echo "Building sim_grid_random..."
cargo build --release --bin sim_grid_random --no-default-features 2>&1 | tee "$HOME/rss_twin_run/rust_full_build.log"
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then
  JOBS=$((NPROC - 1))
else
  JOBS=1
fi
echo "jobs=$JOBS"
nohup ./target/release/sim_grid_random --grid-full -j "$JOBS" --config-dir ./configs \
  > "$HOME/rss_twin_run/rust_grid_full.log" 2>&1 &
echo $! > "$HOME/rss_twin_run/rust_grid_full.pid"
echo "PID=$(cat "$HOME/rss_twin_run/rust_grid_full.pid")"
sleep 1
head -n 20 "$HOME/rss_twin_run/rust_grid_full.log" || true
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
        # stop previous full-grid if any
        client.exec_command(
            "pkill -f 'sim_grid_random --grid-full' || true; "
            "pkill -f run_full_grid.sh || true"
        )
        time.sleep(0.5)

        sftp = client.open_sftp()
        sftp.put(LOCAL_BIN, REMOTE_BIN)
        print("put sim_grid_random.rs", flush=True)
        with sftp.file(REMOTE_SH, "w") as rf:
            rf.write(LAUNCH)
        sftp.chmod(REMOTE_SH, 0o755)
        sftp.close()

        print("=== build + launch nohup ===", flush=True)
        transport = client.get_transport()
        assert transport is not None
        chan = transport.open_session()
        chan.set_combine_stderr(True)
        chan.exec_command("bash ~/rss_twin_run/run_full_grid.sh")
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
        print("\nLAUNCH_EXIT", code)
        return code
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
