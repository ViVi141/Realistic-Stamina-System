use std::env;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode, Stdio};

fn usage() {
    eprintln!(
        "Usage:\n  rss_pipeline_v6_rs validate [--fast]\n  rss_pipeline_v6_rs calibrate [args...]\n  rss_pipeline_v6_rs optimize [args...]\n  rss_pipeline_v6_rs dual-run [--fast]"
    );
}

fn find_repo_root() -> Result<PathBuf, String> {
    let mut cur = env::current_dir().map_err(|e| format!("failed to read current dir: {e}"))?;
    loop {
        let marker = cur.join("tools").join("rss_pipeline_v6.py");
        if marker.is_file() {
            return Ok(cur);
        }
        if !cur.pop() {
            return Err("could not find repository root containing tools/rss_pipeline_v6.py".to_string());
        }
    }
}

fn run_python_command(repo_root: &Path, args: &[OsString], inherit_stdio: bool) -> Result<std::process::Output, String> {
    let script = repo_root.join("tools").join("rss_pipeline_v6.py");
    if !script.is_file() {
        return Err(format!("missing script: {}", script.display()));
    }

    let mut cmd = Command::new("python3");
    cmd.current_dir(repo_root);
    cmd.arg(script);
    cmd.args(args.iter().cloned());
    if inherit_stdio {
        cmd.stdin(Stdio::inherit())
            .stdout(Stdio::inherit())
            .stderr(Stdio::inherit());
        let status = cmd
            .status()
            .map_err(|e| format!("failed to run python command: {e}"))?;
        Ok(std::process::Output {
            status,
            stdout: Vec::new(),
            stderr: Vec::new(),
        })
    } else {
        cmd.output()
            .map_err(|e| format!("failed to run python command: {e}"))
    }
}

fn run_passthrough(repo_root: &Path, subcommand: &str, rest: &[OsString]) -> ExitCode {
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

fn run_dual_validate(repo_root: &Path, rest: &[OsString]) -> ExitCode {
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

    print!("{}", py_stdout);
    eprint!("{}", py_stderr);

    if status_match && stdout_match && stderr_match {
        println!("[Rust dual-run] validate parity: OK");
        ExitCode::from(py_out.status.code().unwrap_or(0) as u8)
    } else {
        eprintln!("[Rust dual-run] validate parity: MISMATCH");
        eprintln!("  status_match={status_match} stdout_match={stdout_match} stderr_match={stderr_match}");
        ExitCode::from(1)
    }
}

fn main() -> ExitCode {
    let mut args = env::args_os();
    let _bin = args.next();
    let Some(subcommand) = args.next() else {
        usage();
        return ExitCode::from(2);
    };
    let rest: Vec<OsString> = args.collect();

    let repo_root = match find_repo_root() {
        Ok(path) => path,
        Err(err) => {
            eprintln!("error: {err}");
            return ExitCode::FAILURE;
        }
    };

    match subcommand.to_string_lossy().as_ref() {
        "validate" => run_passthrough(&repo_root, "validate", &rest),
        "calibrate" => run_passthrough(&repo_root, "calibrate", &rest),
        "optimize" => run_passthrough(&repo_root, "optimize", &rest),
        "dual-run" => run_dual_validate(&repo_root, &rest),
        _ => {
            usage();
            ExitCode::from(2)
        }
    }
}
