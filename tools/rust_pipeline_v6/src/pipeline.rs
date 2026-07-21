use std::ffi::OsString;
use std::path::Path;
use std::process::ExitCode;

use crate::python_backend::run_python_command;

pub fn run_passthrough(repo_root: &Path, subcommand: &str, rest: &[OsString]) -> ExitCode {
    let mut args = vec![OsString::from(subcommand)];
    args.extend(rest.iter().cloned());
    match run_python_command(repo_root, &args, true) {
        Ok(output) => ExitCode::from(output.status.code().unwrap_or(1) as u8),
        Err(err) => {
            eprintln!("error: {err}");
            ExitCode::FAILURE
        }
    }
}

pub fn run_dual_validate(repo_root: &Path, rest: &[OsString]) -> ExitCode {
    let mut args = vec![OsString::from("validate")];
    args.extend(rest.iter().cloned());

    let rust_proxy = run_python_command(repo_root, &args, false);
    let direct_python = run_python_command(repo_root, &args, false);

    let (proxy_out, py_out) = match (rust_proxy, direct_python) {
        (Ok(a), Ok(b)) => (a, b),
        (Err(e), _) | (_, Err(e)) => {
            eprintln!("dual-run failed: {e}");
            return ExitCode::FAILURE;
        }
    };

    let proxy_stdout = String::from_utf8_lossy(&proxy_out.stdout);
    let proxy_stderr = String::from_utf8_lossy(&proxy_out.stderr);
    let py_stdout = String::from_utf8_lossy(&py_out.stdout);
    let py_stderr = String::from_utf8_lossy(&py_out.stderr);

    let status_match = proxy_out.status.code() == py_out.status.code();
    let stdout_match = proxy_stdout == py_stdout;
    let stderr_match = proxy_stderr == py_stderr;

    print!("{py_stdout}");
    eprint!("{py_stderr}");

    if status_match && stdout_match && stderr_match {
        println!("[Rust dual-run] validate parity: OK");
        ExitCode::from(py_out.status.code().unwrap_or(0) as u8)
    } else {
        eprintln!("[Rust dual-run] validate parity: MISMATCH");
        eprintln!(
            "  status_match={status_match} stdout_match={stdout_match} stderr_match={stderr_match}"
        );
        ExitCode::from(1)
    }
}
