#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import annotations

import os
import sys
import time

import paramiko

HOST = os.environ.get("RSS_VPS_HOST", "rdp.tasksmc.cn")
PORT = int(os.environ.get("RSS_VPS_PORT", "33322"))
USER = os.environ.get("RSS_VPS_USER", "vivi141")
PASSWORD = os.environ.get("RSS_VPS_PASSWORD", "")
LOCAL = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "rss_sim",
    "src",
    "bin",
    "sim_grid_random.rs",
)
REMOTE = "/home/vivi141/rss_twin_run/rss_rust_grid/src/bin/sim_grid_random.rs"

SH = r"""#!/bin/bash
set -euo pipefail
cd "$HOME/rss_twin_run/rss_rust_grid"
. "$HOME/.cargo/env"
cargo build --release --bin sim_grid_random --no-default-features 2>&1 | tee "$HOME/rss_twin_run/rust_stats_build.log"
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
./target/release/sim_grid_random --grid -n 100000 --seed 42 -j "$JOBS" --config-dir ./configs | tee "$HOME/rss_twin_run/rust_grid100k_stats.log"
"""


def main() -> int:
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        HOST, port=PORT, username=USER, password=PASSWORD, timeout=20,
        allow_agent=False, look_for_keys=False,
    )
    try:
        sftp = client.open_sftp()
        sftp.put(LOCAL, REMOTE)
        with sftp.file("/home/vivi141/rss_twin_run/run_stats100k.sh", "w") as rf:
            rf.write(SH)
        sftp.chmod("/home/vivi141/rss_twin_run/run_stats100k.sh", 0o755)
        sftp.close()
        transport = client.get_transport()
        assert transport is not None
        chan = transport.open_session()
        chan.set_combine_stderr(True)
        chan.exec_command("bash ~/rss_twin_run/run_stats100k.sh")
        while True:
            if chan.recv_ready():
                sys.stdout.write(chan.recv(4096).decode("utf-8", errors="replace"))
                sys.stdout.flush()
            if chan.exit_status_ready():
                while chan.recv_ready():
                    sys.stdout.write(chan.recv(4096).decode("utf-8", errors="replace"))
                    sys.stdout.flush()
                break
            time.sleep(0.2)
        return chan.recv_exit_status()
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
