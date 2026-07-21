use std::env;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::{Command, Output, Stdio};

pub fn find_repo_root() -> Result<PathBuf, String> {
    let mut cur = env::current_dir().map_err(|e| format!("failed to read current dir: {e}"))?;
    loop {
        let marker = cur.join("tools").join("rss_pipeline_v6.py");
        if marker.is_file() {
            return Ok(cur);
        }
        if !cur.pop() {
            return Err(
                "could not find repository root containing tools/rss_pipeline_v6.py".to_string(),
            );
        }
    }
}

pub fn run_python_command(
    repo_root: &Path,
    args: &[OsString],
    inherit_stdio: bool,
) -> Result<Output, String> {
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
        Ok(Output {
            status,
            stdout: Vec::new(),
            stderr: Vec::new(),
        })
    } else {
        cmd.output()
            .map_err(|e| format!("failed to run python command: {e}"))
    }
}

pub fn run_python_file(
    repo_root: &Path,
    file_under_tools: &str,
    args: &[OsString],
) -> Result<Output, String> {
    let script = repo_root.join("tools").join(file_under_tools);
    if !script.is_file() {
        return Err(format!("missing script: {}", script.display()));
    }

    let mut cmd = Command::new("python3");
    cmd.current_dir(repo_root);
    cmd.arg(script);
    cmd.args(args.iter().cloned());
    cmd.output()
        .map_err(|e| format!("failed to run python file: {e}"))
}
