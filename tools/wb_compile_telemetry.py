#!/usr/bin/env python3
# Copyright 2026 Realistic Stamina System contributors
# SPDX-License-Identifier: MIT
"""Arma Reforger Workbench compile crash telemetry.

Watches Workbench process + log tails, builds a timeline, and writes a report
when a crash is detected (or on Ctrl+C).

Example:
  python tools/wb_compile_telemetry.py
  python tools/wb_compile_telemetry.py --wait-process
  python tools/wb_compile_telemetry.py --logs-root "C:/Users/.../ArmaReforgerWorkbench/logs"
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import IO, Optional


DEFAULT_LOGS_ROOT = Path(
    os.path.expandvars(r"%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\logs")
)
WB_PROCESS_NAMES = (
    "ArmaReforgerWorkbenchSteamDiag.exe",
    "ArmaReforgerWorkbenchSteam.exe",
    "ArmaReforgerWorkbench.exe",
)

RE_SCRIPT_LEVEL = re.compile(
    r'SCRIPT\s+\(([EWF])\):\s+@"([^"]+),(\d+)":\s*(.*)$'
)
RE_ENGINE_CRASH = re.compile(r"ENGINE\s+\((F)\):\s*(Crashed)", re.IGNORECASE)
RE_MODULE_LOADED = re.compile(
    r"Module:\s+(\w+);\s+loaded\s+(\d+)x files;\s+(\d+)x classes;\s+used\s+(\d+)K"
)
RE_COMPILING = re.compile(r"Compiling\s+(GameLib|Game)\s+scripts", re.IGNORECASE)
RE_SEH = re.compile(r"Exception code:\s*(0x[0-9a-fA-F]+)")
RE_RSS_PATH = re.compile(r"scripts/Game/RSS/", re.IGNORECASE)


@dataclass
class TelemetryEvent:
    ts_wall: str
    source: str
    kind: str
    summary: str
    raw: str
    level: str = ""
    path: str = ""
    line: int = 0
    rss: bool = False


@dataclass
class ProcessSample:
    ts_wall: str
    name: str
    pid: int
    working_set_mb: float
    private_mb: float
    cpu_sec: float
    responding: bool


@dataclass
class SessionState:
    log_dir: str = ""
    started_wall: str = ""
    process_name: str = ""
    process_pid: int = 0
    events: list[TelemetryEvent] = field(default_factory=list)
    samples: list[ProcessSample] = field(default_factory=list)
    script_errors: int = 0
    script_warnings: int = 0
    rss_errors: int = 0
    modules_loaded: list[str] = field(default_factory=list)
    crash_seen: bool = False
    seh_code: str = ""
    game_module_ok: bool = False


def wall_now() -> str:
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")


def try_import_psutil():
    try:
        import psutil  # type: ignore

        return psutil
    except ImportError:
        return None


def find_workbench_processes(psutil_mod) -> list:
    found = []
    if psutil_mod is None:
        return found
    names = {n.lower() for n in WB_PROCESS_NAMES}
    for proc in psutil_mod.process_iter(["pid", "name"]):
        try:
            info = proc.info
            name = info.get("name")
            if name and name.lower() in names:
                found.append(proc)
        except (psutil_mod.Error, OSError):
            continue
    return found


def sample_process(psutil_mod, proc) -> Optional[ProcessSample]:
    if psutil_mod is None or proc is None:
        return None
    try:
        with proc.oneshot():
            mem = proc.memory_info()
            cpu = proc.cpu_times()
            name = proc.name()
            pid = proc.pid
            responding = True
            if hasattr(proc, "status"):
                status = proc.status()
                if status == psutil_mod.STATUS_ZOMBIE:
                    responding = False
            return ProcessSample(
                ts_wall=wall_now(),
                name=name,
                pid=pid,
                working_set_mb=mem.rss / (1024.0 * 1024.0),
                private_mb=getattr(mem, "private", mem.rss) / (1024.0 * 1024.0),
                cpu_sec=cpu.user + cpu.system,
                responding=responding,
            )
    except (psutil_mod.Error, OSError):
        return None


def list_log_sessions(logs_root: Path) -> list[Path]:
    if not logs_root.is_dir():
        return []
    sessions = []
    for child in logs_root.iterdir():
        if child.is_dir() and child.name.startswith("logs_"):
            sessions.append(child)
    sessions.sort(key=lambda p: p.name)
    return sessions


def newest_session(logs_root: Path) -> Optional[Path]:
    sessions = list_log_sessions(logs_root)
    if not sessions:
        return None
    return sessions[-1]


def classify_line(source: str, line: str) -> Optional[TelemetryEvent]:
    text = line.rstrip("\r\n")
    if not text.strip():
        return None

    m = RE_SCRIPT_LEVEL.search(text)
    if m:
        level = m.group(1)
        path = m.group(2)
        line_no = int(m.group(3))
        msg = m.group(4).strip()
        kind = "script_error"
        if level == "W":
            kind = "script_warning"
        elif level == "F":
            kind = "script_fatal"
        rss = bool(RE_RSS_PATH.search(path))
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind=kind,
            summary=f"{path}:{line_no} {msg}",
            raw=text,
            level=level,
            path=path,
            line=line_no,
            rss=rss,
        )

    if RE_ENGINE_CRASH.search(text) or "Application crashed" in text:
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind="engine_crash",
            summary=text.strip(),
            raw=text,
            level="F",
        )

    m = RE_MODULE_LOADED.search(text)
    if m:
        module = m.group(1)
        files = m.group(2)
        classes = m.group(3)
        mem_k = m.group(4)
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind="module_loaded",
            summary=f"{module}: {files} files, {classes} classes, {mem_k}K",
            raw=text,
        )

    if RE_COMPILING.search(text):
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind="compiling",
            summary=text.strip(),
            raw=text,
        )

    m = RE_SEH.search(text)
    if m:
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind="seh_code",
            summary=m.group(1),
            raw=text,
        )

    if "Too many errors" in text:
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind="too_many_errors",
            summary=text.strip(),
            raw=text,
            level="E",
        )

    if "Multiple declaration" in text:
        return TelemetryEvent(
            ts_wall=wall_now(),
            source=source,
            kind="multiple_declaration",
            summary=text.strip(),
            raw=text,
            level="E",
        )

    return None


class LogTail:
    def __init__(self, path: Path, source: str):
        self.path = path
        self.source = source
        self._fh: Optional[IO[str]] = None
        self._pos = 0
        self._inode = None

    def close(self) -> None:
        if self._fh is not None:
            self._fh.close()
            self._fh = None

    def _open(self) -> bool:
        if not self.path.is_file():
            return False
        try:
            st = self.path.stat()
            inode = (st.st_ino, st.st_dev, st.st_mtime_ns)
            if self._fh is None or self._inode != inode:
                self.close()
                self._fh = open(self.path, "r", encoding="utf-8", errors="replace")
                self._inode = inode
                # First open of an existing file: start at beginning for session
                # context, but if file is huge and we reconnect mid-run, seek end.
                if self._pos > 0:
                    self._fh.seek(self._pos)
            return True
        except OSError:
            return False

    def read_new_lines(self) -> list[str]:
        if not self._open() or self._fh is None:
            return []
        lines = self._fh.readlines()
        self._pos = self._fh.tell()
        return lines


def apply_event(state: SessionState, event: TelemetryEvent) -> None:
    state.events.append(event)
    if event.kind == "script_error":
        state.script_errors += 1
        if event.rss:
            state.rss_errors += 1
    elif event.kind == "script_warning":
        state.script_warnings += 1
    elif event.kind == "engine_crash":
        state.crash_seen = True
    elif event.kind == "seh_code":
        state.seh_code = event.summary
        state.crash_seen = True
    elif event.kind == "module_loaded":
        state.modules_loaded.append(event.summary)
        if event.summary.startswith("Game:"):
            state.game_module_ok = True


def print_event(event: TelemetryEvent) -> None:
    tag = event.kind
    if event.rss:
        tag = f"{tag}/RSS"
    print(f"[{event.ts_wall}] {event.source:8} {tag:22} {event.summary}")


def write_report(out_dir: Path, state: SessionState) -> tuple[Path, Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = utc_stamp()
    session_name = Path(state.log_dir).name if state.log_dir else "session"
    safe_session = re.sub(r"[^\w.\-]+", "_", session_name)
    json_path = out_dir / f"wb_telemetry_{safe_session}_{stamp}.json"
    md_path = out_dir / f"wb_telemetry_{safe_session}_{stamp}.md"

    payload = {
        "generated_at": wall_now(),
        "session": {
            "log_dir": state.log_dir,
            "started_wall": state.started_wall,
            "process_name": state.process_name,
            "process_pid": state.process_pid,
            "script_errors": state.script_errors,
            "script_warnings": state.script_warnings,
            "rss_errors": state.rss_errors,
            "game_module_ok": state.game_module_ok,
            "crash_seen": state.crash_seen,
            "seh_code": state.seh_code,
            "modules_loaded": state.modules_loaded,
        },
        "events": [asdict(e) for e in state.events],
        "samples": [asdict(s) for s in state.samples],
    }
    json_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    lines = []
    lines.append("# Workbench Compile Telemetry Report")
    lines.append("")
    lines.append(f"- Generated: `{wall_now()}`")
    lines.append(f"- Log dir: `{state.log_dir}`")
    lines.append(f"- Process: `{state.process_name}` pid={state.process_pid}")
    lines.append(f"- Script errors: **{state.script_errors}** (RSS: **{state.rss_errors}**)")
    lines.append(f"- Script warnings: {state.script_warnings}")
    lines.append(f"- Game module loaded: **{state.game_module_ok}**")
    lines.append(f"- Crash seen: **{state.crash_seen}**")
    if state.seh_code:
        lines.append(f"- SEH: `{state.seh_code}`")
    lines.append("")
    lines.append("## Modules")
    if state.modules_loaded:
        for m in state.modules_loaded:
            lines.append(f"- {m}")
    else:
        lines.append("- (none — compile likely aborted before Module: Game)")
    lines.append("")
    lines.append("## Timeline (key events)")
    for e in state.events:
        mark = ""
        if e.rss:
            mark = " **[RSS]**"
        if e.kind in ("engine_crash", "seh_code", "script_error"):
            mark = mark + " **<<**"
        lines.append(f"- `{e.ts_wall}` `{e.kind}`{mark}: {e.summary}")
    lines.append("")
    lines.append("## Last 15 events before end")
    for e in state.events[-15:]:
        lines.append(f"- `{e.ts_wall}` [{e.source}] {e.kind}: {e.summary}")
    lines.append("")
    lines.append("## Process samples")
    if state.samples:
        first = state.samples[0]
        last = state.samples[-1]
        lines.append(
            f"- First WS: {first.working_set_mb:.1f} MB @ {first.ts_wall}"
        )
        lines.append(
            f"- Last WS: {last.working_set_mb:.1f} MB @ {last.ts_wall}"
        )
        peak = max(state.samples, key=lambda s: s.working_set_mb)
        lines.append(
            f"- Peak WS: {peak.working_set_mb:.1f} MB @ {peak.ts_wall}"
        )
    else:
        lines.append("- (no samples — install `psutil` for memory tracking)")
    lines.append("")
    lines.append("## Hypothesis hints")
    if state.crash_seen and not state.game_module_ok and state.rss_errors == 0:
        lines.append(
            "- Crash with **0 RSS errors** and **no Game module load** → "
            "likely Workbench ICE / heap corruption during late Game compile."
        )
    if state.rss_errors > 0:
        lines.append(
            "- RSS script errors present → fix those first; crash may be secondary."
        )
    if state.seh_code.upper() == "0xC0000374":
        lines.append(
            "- `0xC0000374` = `STATUS_HEAP_CORRUPTION` (Windows heap)."
        )
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return json_path, md_path


def wait_for_process(psutil_mod, timeout_sec: float) -> Optional[object]:
    print("Waiting for Workbench process...")
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        procs = find_workbench_processes(psutil_mod)
        if procs:
            return procs[0]
        time.sleep(0.5)
    return None


def wait_for_new_or_latest_session(
    logs_root: Path, baseline: Optional[Path], timeout_sec: float
) -> Optional[Path]:
    print(f"Watching log sessions under: {logs_root}")
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        newest = newest_session(logs_root)
        if newest is None:
            time.sleep(0.5)
            continue
        if baseline is None:
            return newest
        if newest.name != baseline.name:
            return newest
        # Same session may still be active (recompile inside open WB)
        script_log = newest / "script.log"
        if script_log.is_file():
            return newest
        time.sleep(0.5)
    return newest_session(logs_root)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Telemetry monitor for Arma Reforger Workbench script compile crashes."
    )
    parser.add_argument(
        "--logs-root",
        type=Path,
        default=DEFAULT_LOGS_ROOT,
        help="Workbench logs root directory",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Report output directory (default: <logs-root>/telemetry)",
    )
    parser.add_argument(
        "--poll-ms",
        type=int,
        default=200,
        help="Log poll interval in milliseconds",
    )
    parser.add_argument(
        "--sample-ms",
        type=int,
        default=500,
        help="Process sample interval in milliseconds",
    )
    parser.add_argument(
        "--wait-process",
        action="store_true",
        help="Wait until Workbench process appears",
    )
    parser.add_argument(
        "--wait-new-session",
        action="store_true",
        help="Wait for a new logs_* folder (fresh Workbench start)",
    )
    parser.add_argument(
        "--timeout-sec",
        type=float,
        default=600.0,
        help="Max seconds to wait for process/session",
    )
    parser.add_argument(
        "--follow-sec",
        type=float,
        default=0.0,
        help="Stop after N seconds even without crash (0 = until crash/Ctrl+C)",
    )
    parser.add_argument(
        "--session",
        type=Path,
        default=None,
        help="Attach to an existing logs_* directory instead of auto-detect",
    )
    parser.add_argument(
        "--offline",
        action="store_true",
        help="One-shot parse of --session (or newest session); no live follow",
    )
    return parser.parse_args(argv)


def run_offline(session_dir: Path, out_dir: Path) -> int:
    print(f"Offline parse: {session_dir}")
    state = SessionState(log_dir=str(session_dir), started_wall=wall_now())
    files = (
        ("script", session_dir / "script.log"),
        ("error", session_dir / "error.log"),
        ("console", session_dir / "console.log"),
        ("crash", session_dir / "crash.log"),
    )
    for source, path in files:
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print(f"WARN: cannot read {path}: {exc}")
            continue
        for line in text.splitlines():
            event = classify_line(source, line)
            if event is None:
                continue
            # Prefer log-embedded timestamps already in raw line for offline
            apply_event(state, event)
    json_path, md_path = write_report(out_dir, state)
    print(f"JSON report: {json_path}")
    print(f"Markdown:    {md_path}")
    print(
        f"Summary: errors={state.script_errors} rss_errors={state.rss_errors} "
        f"game_ok={state.game_module_ok} crash={state.crash_seen} "
        f"seh={state.seh_code or '-'}"
    )
    if state.crash_seen:
        return 1
    return 0


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    logs_root = args.logs_root
    out_dir = args.out_dir
    if out_dir is None:
        out_dir = logs_root / "telemetry"

    if args.offline:
        if args.session is not None:
            session_dir = args.session
        else:
            session_dir = newest_session(logs_root)
        if session_dir is None or not session_dir.is_dir():
            print(f"ERROR: no log session under {logs_root}")
            return 2
        return run_offline(session_dir, out_dir)

    psutil_mod = try_import_psutil()
    if psutil_mod is None:
        print(
            "NOTE: psutil not installed — process memory sampling disabled. "
            "Run: pip install psutil"
        )

    proc = None
    if args.wait_process:
        proc = wait_for_process(psutil_mod, args.timeout_sec)
        if proc is None:
            print("ERROR: Workbench process not found before timeout.")
            return 2
        print(f"Attached process pid={proc.pid} name={proc.name()}")
    else:
        procs = find_workbench_processes(psutil_mod)
        if procs:
            proc = procs[0]
            print(f"Found process pid={proc.pid} name={proc.name()}")
        else:
            print("Workbench not running yet — will still watch logs.")

    baseline = newest_session(logs_root)
    if args.session is not None:
        session_dir = args.session
    elif args.wait_new_session:
        session_dir = wait_for_new_or_latest_session(
            logs_root, baseline, args.timeout_sec
        )
    else:
        session_dir = wait_for_new_or_latest_session(
            logs_root, None, min(args.timeout_sec, 30.0)
        )

    if session_dir is None or not session_dir.is_dir():
        print(f"ERROR: no log session under {logs_root}")
        return 2

    print(f"Monitoring session: {session_dir}")
    state = SessionState(
        log_dir=str(session_dir),
        started_wall=wall_now(),
    )
    if proc is not None:
        try:
            state.process_pid = proc.pid
            state.process_name = proc.name()
        except Exception:
            pass

    tails = [
        LogTail(session_dir / "script.log", "script"),
        LogTail(session_dir / "error.log", "error"),
        LogTail(session_dir / "console.log", "console"),
        LogTail(session_dir / "crash.log", "crash"),
    ]

    poll_s = max(args.poll_ms, 50) / 1000.0
    sample_s = max(args.sample_ms, 100) / 1000.0
    next_sample = 0.0
    follow_deadline = None
    if args.follow_sec > 0:
        follow_deadline = time.time() + args.follow_sec

    print("Streaming events (Ctrl+C to stop and write report)...")
    try:
        while True:
            for tail in tails:
                for line in tail.read_new_lines():
                    event = classify_line(tail.source, line)
                    if event is None:
                        continue
                    apply_event(state, event)
                    print_event(event)

            now = time.time()
            if psutil_mod is not None and now >= next_sample:
                if proc is None:
                    procs = find_workbench_processes(psutil_mod)
                    if procs:
                        proc = procs[0]
                        state.process_pid = proc.pid
                        state.process_name = proc.name()
                if proc is not None:
                    if not proc.is_running():
                        print(f"[{wall_now()}] process exited pid={state.process_pid}")
                        state.crash_seen = True
                        break
                    sample = sample_process(psutil_mod, proc)
                    if sample is not None:
                        state.samples.append(sample)
                next_sample = now + sample_s

            if state.crash_seen:
                print("Crash signal detected — finalizing report.")
                # Give logs a moment to flush
                time.sleep(0.4)
                for tail in tails:
                    for line in tail.read_new_lines():
                        event = classify_line(tail.source, line)
                        if event is None:
                            continue
                        apply_event(state, event)
                        print_event(event)
                break

            if follow_deadline is not None and now >= follow_deadline:
                print("Follow timeout reached.")
                break

            time.sleep(poll_s)
    except KeyboardInterrupt:
        print("\nInterrupted — writing report...")
    finally:
        for tail in tails:
            tail.close()

    json_path, md_path = write_report(out_dir, state)
    print("")
    print(f"JSON report: {json_path}")
    print(f"Markdown:    {md_path}")
    print(
        f"Summary: errors={state.script_errors} rss_errors={state.rss_errors} "
        f"game_ok={state.game_module_ok} crash={state.crash_seen} seh={state.seh_code or '-'}"
    )
    if state.crash_seen:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
