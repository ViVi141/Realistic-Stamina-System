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
        cmd = (
            "PID=$(cat ~/rss_twin_run/rust_grid_full.pid 2>/dev/null || echo none); "
            "if [ \"$PID\" != none ] && kill -0 \"$PID\" 2>/dev/null; then "
            "echo RUNNING pid=$PID; "
            "ps -p \"$PID\" -o etime,pcpu,pmem,cmd --no-headers; "
            "else echo DONE_OR_MISSING pid=$PID; fi; "
            "echo '--- log ---'; "
            "tail -n 30 ~/rss_twin_run/rust_grid_full.log 2>/dev/null || echo NO_LOG"
        )
        _stdin, stdout, stderr = client.exec_command(cmd, timeout=30)
        sys.stdout.write(stdout.read().decode("utf-8", errors="replace"))
        err = stderr.read().decode("utf-8", errors="replace")
        if err:
            sys.stdout.write(err)
        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
