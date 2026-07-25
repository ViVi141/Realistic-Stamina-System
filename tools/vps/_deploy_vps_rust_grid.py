#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Build & run sim_grid_random on VPS via Docker (USTC mirrors). No host rustup needed."""

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

ROOT = Path(__file__).resolve().parent
SIM = ROOT / "rss_sim"

DOCKERFILE = r'''FROM rust:1.85-bookworm AS build
WORKDIR /src
ENV CARGO_HOME=/usr/local/cargo
ENV RUSTUP_DIST_SERVER=https://mirrors.ustc.edu.cn/rust-static
ENV RUSTUP_UPDATE_ROOT=https://mirrors.ustc.edu.cn/rust-static/rustup
RUN mkdir -p /usr/local/cargo \
 && printf '[source.crates-io]\nreplace-with = "ustc"\n\n[source.ustc]\nregistry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"\n' > /usr/local/cargo/config.toml
COPY Cargo.toml ./
COPY src ./src
RUN cargo build --release --bin sim_grid_random --no-default-features

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /src/target/release/sim_grid_random /usr/local/bin/sim_grid_random
COPY configs/ ./
ENTRYPOINT ["/usr/local/bin/sim_grid_random"]
'''

BUILD_SH = r'''#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

run_bin() {
  local BIN="$1"
  NPROC=$(nproc)
  if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
  echo "=== run n=100000 jobs=$JOBS bin=$BIN ==="
  "$BIN" --grid -n 100000 --seed 42 -j "$JOBS" --config-dir "$PWD/configs" | tee ~/rss_twin_run/rust_grid100k.log
}

setup_ustc_cargo() {
  mkdir -p "$HOME/.cargo"
  cat > "$HOME/.cargo/config.toml" <<'EOF'
[source.crates-io]
replace-with = "ustc"
[source.ustc]
registry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"
EOF
  export RUSTUP_DIST_SERVER=https://mirrors.ustc.edu.cn/rust-static
  export RUSTUP_UPDATE_ROOT=https://mirrors.ustc.edu.cn/rust-static/rustup
}

if command -v rustc >/dev/null 2>&1; then
  echo "=== host rustc $(rustc --version) + USTC crates ==="
  setup_ustc_cargo
  # shellcheck disable=SC1091
  [ -f "$HOME/.cargo/env" ] && . "$HOME/.cargo/env"
  cargo build --release --bin sim_grid_random --no-default-features
  run_bin "$PWD/target/release/sim_grid_random"
  exit 0
fi

echo "=== install rustup via USTC ==="
if curl -fsSL https://mirrors.ustc.edu.cn/rust-static/rustup/dist/x86_64-unknown-linux-gnu/rustup-init -o /tmp/rustup-init; then
  chmod +x /tmp/rustup-init
  setup_ustc_cargo
  /tmp/rustup-init -y --default-toolchain stable --profile minimal
  # shellcheck disable=SC1091
  . "$HOME/.cargo/env"
  cargo build --release --bin sim_grid_random --no-default-features
  run_bin "$PWD/target/release/sim_grid_random"
  exit 0
fi

echo "=== fallback: Docker Hub rust + USTC crates.io ==="
docker build -t rss-sim-grid .
NPROC=$(nproc)
if [ "$NPROC" -gt 1 ]; then JOBS=$((NPROC - 1)); else JOBS=1; fi
docker run --rm --cpus="$NPROC" \
  -v "$PWD/configs:/app:ro" \
  rss-sim-grid --grid -n 100000 --seed 42 -j "$JOBS" --config-dir /app | tee ~/rss_twin_run/rust_grid100k.log
'''


def add_dir(tar: tarfile.TarFile, local: Path, arc_prefix: str) -> None:
    for path in local.rglob("*"):
        if path.is_dir():
            continue
        rel = path.relative_to(local).as_posix()
        # skip target / huge artifacts
        if rel.startswith("target/") or "/target/" in rel:
            continue
        if path.suffix in {".pdb", ".rlib"}:
            continue
        tar.add(path, arcname=f"{arc_prefix}/{rel}")


def build_tarball() -> bytes:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        # Cargo project (source only)
        for name in ("Cargo.toml",):
            tar.add(SIM / name, arcname=f"rss_rust_grid/{name}")
        add_dir(tar, SIM / "src", "rss_rust_grid/src")

        # preset JSONs
        for p in ROOT.glob("optimized_rss_config_*_v*.json"):
            tar.add(p, arcname=f"rss_rust_grid/configs/{p.name}")

        for name, text, mode in (
            ("Dockerfile", DOCKERFILE, 0o644),
            ("build_and_run.sh", BUILD_SH, 0o755),
        ):
            data = text.encode("utf-8")
            info = tarfile.TarInfo(name=f"rss_rust_grid/{name}")
            info.size = len(data)
            info.mode = mode
            tar.addfile(info, io.BytesIO(data))
    return buf.getvalue()


def run(client: paramiko.SSHClient, cmd: str, timeout: int = 120) -> tuple[int, str, str]:
    _, stdout, stderr = client.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    code = stdout.channel.recv_exit_status()
    return code, out, err


def main() -> int:
    if not PASSWORD:
        print("Set RSS_VPS_PASSWORD")
        return 2
    payload = build_tarball()
    print(f"tarball={len(payload)} bytes")

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
        run(client, "mkdir -p ~/rss_twin_run")
        sftp = client.open_sftp()
        remote = "/home/vivi141/rss_twin_run/rss_rust_grid.tgz"
        with sftp.file(remote, "wb") as rf:
            rf.write(payload)
        sftp.close()
        print("uploaded", remote)

        code, out, err = run(
            client,
            "cd ~/rss_twin_run && rm -rf rss_rust_grid && tar -xzf rss_rust_grid.tgz && "
            "cd rss_rust_grid && chmod +x build_and_run.sh && ls -la && ls src/bin",
            timeout=60,
        )
        print(out)
        if err.strip():
            print(err)
        if code != 0:
            return code

        print("=== building on VPS (may take several minutes) ===", flush=True)
        # Long build: stream via get_pty
        transport = client.get_transport()
        assert transport is not None
        chan = transport.open_session()
        chan.set_combine_stderr(True)
        chan.exec_command("bash ~/rss_twin_run/rss_rust_grid/build_and_run.sh")
        buf = []
        while True:
            if chan.recv_ready():
                chunk = chan.recv(4096).decode("utf-8", errors="replace")
                sys.stdout.write(chunk)
                sys.stdout.flush()
                buf.append(chunk)
            if chan.exit_status_ready():
                while chan.recv_ready():
                    chunk = chan.recv(4096).decode("utf-8", errors="replace")
                    sys.stdout.write(chunk)
                    sys.stdout.flush()
                    buf.append(chunk)
                break
            time.sleep(0.2)
        code = chan.recv_exit_status()
        text = "".join(buf)
        if "PASS:" in text:
            print("\nVPS Rust grid: PASS")
        return code
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
