#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import annotations

import os
import sys

import paramiko

HOST = os.environ.get("RSS_VPS_HOST", "rdp.tasksmc.cn")
PORT = int(os.environ.get("RSS_VPS_PORT", "33322"))
USER = os.environ.get("RSS_VPS_USER", "vivi141")
PASSWORD = os.environ.get("RSS_VPS_PASSWORD", "")


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
        cmds = [
            "ps -ef | grep -E 'rustup|cargo|sim_grid|run_rust' | grep -v grep || true",
            "ls -la ~/rss_twin_run/ 2>/dev/null || true",
            "test -x ~/.cargo/bin/rustc && ~/.cargo/bin/rustc --version || echo NO_RUSTC",
            "test -f /tmp/rustup-init && ls -la /tmp/rustup-init || echo NO_RUSTUP_INIT",
            "tail -n 40 ~/rss_twin_run/rust_build.log 2>/dev/null || echo NO_BUILD_LOG",
            "tail -n 20 ~/rss_twin_run/rust_grid100k.log 2>/dev/null || echo NO_GRID_LOG",
        ]
        for c in cmds:
            print(">>>", c)
            _stdin, stdout, stderr = client.exec_command(c, timeout=30)
            out = stdout.read().decode("utf-8", errors="replace")
            err = stderr.read().decode("utf-8", errors="replace")
            sys.stdout.write(out)
            if err:
                sys.stdout.write(err)
            sys.stdout.flush()
        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
