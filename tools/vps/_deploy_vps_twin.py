#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Deploy twin grid runner to Shandong VPS and start a multi-core sample job."""

from __future__ import annotations

import io
import os
import sys
import tarfile
import time
from pathlib import Path

import paramiko

HOST = os.environ.get("RSS_VPS_HOST", "rdp.tasksmc.cn")
PORT = int(os.environ.get("RSS_VPS_PORT", "33322"))
USER = os.environ.get("RSS_VPS_USER", "vivi141")
PASSWORD = os.environ.get("RSS_VPS_PASSWORD", "")

TOOLS = Path(__file__).resolve().parent

PACK_NAMES = [
    "test_rss_random_scenarios.py",
    "rss_digital_twin_fix.py",
    "rss_pipeline_v6.py",
    "rss_pipeline_v4.py",
    "rss_sim_backend.py",
    "rss_anchors_v6.py",
    "rss_constraints_v6.py",
    "requirements.txt",
    "optimized_rss_config_elitestandard_v6.json",
    "optimized_rss_config_standardmilsim_v6.json",
    "optimized_rss_config_tacticalaction_v6.json",
    "optimized_rss_config_elitestandard_v4.json",
    "optimized_rss_config_standardmilsim_v4.json",
    "optimized_rss_config_tacticalaction_v4.json",
]

RUN_SH = r'''#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
export PYTHONUNBUFFERED=1
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
echo "=== host nproc=$NPROC jobs=$JOBS ==="
python3 test_rss_random_scenarios.py --show-grid
python3 test_rss_random_scenarios.py --grid --n 100000 --seed 42 -j "$JOBS" --chunk-size 128
'''

DOCKERFILE = r'''FROM docker.mirrors.ustc.edu.cn/library/python:3.12-slim-bookworm
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -i https://mirrors.ustc.edu.cn/pypi/simple \
    --trusted-host mirrors.ustc.edu.cn -r requirements.txt
COPY . .
CMD ["python3", "test_rss_random_scenarios.py", "--grid", "--n", "100000", "--seed", "42", "-j", "0", "--chunk-size", "128"]
'''


def build_tarball() -> bytes:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        for name in PACK_NAMES:
            path = TOOLS / name
            if not path.is_file():
                raise FileNotFoundError(path)
            tar.add(path, arcname=f"rss_twin_tools/{name}")

        for name, text, mode in (
            ("run_grid_sample.sh", RUN_SH, 0o755),
            ("Dockerfile", DOCKERFILE, 0o644),
        ):
            data = text.encode("utf-8")
            info = tarfile.TarInfo(name=f"rss_twin_tools/{name}")
            info.size = len(data)
            info.mode = mode
            tar.addfile(info, io.BytesIO(data))
    return buf.getvalue()


def ssh_connect() -> paramiko.SSHClient:
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
        banner_timeout=30,
    )
    return client


def run(client: paramiko.SSHClient, cmd: str, timeout: int = 120) -> tuple[int, str, str]:
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout, get_pty=False)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    code = stdout.channel.recv_exit_status()
    return code, out, err


def main() -> int:
    if not PASSWORD:
        print("Set RSS_VPS_PASSWORD env var before running.")
        return 2
    payload = build_tarball()
    print(f"tarball={len(payload)} bytes")

    client = ssh_connect()
    try:
        code, out, err = run(client, "mkdir -p ~/rss_twin_run")
        sftp = client.open_sftp()
        remote = "/home/vivi141/rss_twin_run/rss_twin_tools.tgz"
        with sftp.file(remote, "wb") as rf:
            rf.write(payload)
        sftp.close()
        print("uploaded", remote)

        code, out, err = run(
            client,
            "cd ~/rss_twin_run && rm -rf rss_twin_tools && tar -xzf rss_twin_tools.tgz && "
            "cd rss_twin_tools && chmod +x run_grid_sample.sh && ls -la",
        )
        print(out)
        if err.strip():
            print(err)
        if code != 0:
            return code

        # Prefer host Python (already has numpy); Docker USTC fallback if import fails
        prep = r'''
set -e
cd ~/rss_twin_run/rss_twin_tools
python3 - <<'PY'
import importlib
ok = True
for m in ("numpy",):
    try:
        importlib.import_module(m)
        print(m, "ok")
    except Exception as e:
        print(m, "MISSING", e)
        ok = False
raise SystemExit(0 if ok else 1)
PY
'''
        code, out, err = run(client, prep, timeout=60)
        print(out, err)
        use_docker = code != 0

        if use_docker:
            print("numpy missing on host → build via Docker USTC mirror")
            # Configure docker mirror if needed, build and run
            docker_cmd = r'''
set -e
cd ~/rss_twin_run/rss_twin_tools
mkdir -p ~/.docker
cat > ~/.docker/daemon.json.tmp <<'EOF'
{"registry-mirrors":["https://docker.mirrors.ustc.edu.cn"]}
EOF
# user may not edit daemon; pull via mirror-prefixed image instead
docker pull docker.mirrors.ustc.edu.cn/library/python:3.12-slim-bookworm
docker build -t rss-twin-grid .
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
nohup docker run --rm --cpus="$NPROC" -e PYTHONUNBUFFERED=1 rss-twin-grid \
  python3 test_rss_random_scenarios.py --grid --n 100000 --seed 42 -j "$JOBS" --chunk-size 128 \
  > ~/rss_twin_run/grid100k.log 2>&1 &
echo $! > ~/rss_twin_run/grid100k.pid
echo DOCKER_STARTED pid=$(cat ~/rss_twin_run/grid100k.pid)
'''
            code, out, err = run(client, docker_cmd, timeout=600)
            print(out)
            print(err)
            if code != 0:
                return code
        else:
            # Host already has numpy; use venv + USTC PyPI (PEP668). No Rust needed.
            host_cmd = r'''
set -e
cd ~/rss_twin_run/rss_twin_tools
if [ ! -d .venv ]; then
  python3 -m venv .venv
fi
. .venv/bin/activate
python -m pip install -q -U pip -i https://mirrors.ustc.edu.cn/pypi/simple --trusted-host mirrors.ustc.edu.cn
python -m pip install -q -r requirements.txt -i https://mirrors.ustc.edu.cn/pypi/simple --trusted-host mirrors.ustc.edu.cn
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
# smoke import
python -c "from test_rss_random_scenarios import GRID_TOTAL; print('import_ok', GRID_TOTAL)"
nohup python test_rss_random_scenarios.py --grid --n 100000 --seed 42 -j "$JOBS" --chunk-size 128 \
  > ~/rss_twin_run/grid100k.log 2>&1 &
echo $! > ~/rss_twin_run/grid100k.pid
echo HOST_STARTED pid=$(cat ~/rss_twin_run/grid100k.pid) jobs=$JOBS
'''
            code, out, err = run(client, host_cmd, timeout=600)
            print(out)
            print(err)
            if code != 0:
                return code

        # Poll log for a bit
        for _ in range(30):
            time.sleep(5)
            code, out, err = run(
                client,
                "tail -n 20 ~/rss_twin_run/grid100k.log; "
                "ps -p $(cat ~/rss_twin_run/grid100k.pid 2>/dev/null) -o pid,etime,cmd 2>/dev/null || echo DONE_OR_DEAD",
                timeout=30,
            )
            print("--- poll ---")
            print(out)
            if "PASS:" in out or "FAIL:" in out:
                break
            if "DONE_OR_DEAD" in out and "PASS:" not in out and "design grid" in out:
                # process ended; show full log
                code, out, err = run(client, "cat ~/rss_twin_run/grid100k.log", timeout=30)
                print(out)
                break
        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
