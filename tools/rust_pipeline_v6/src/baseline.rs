use std::collections::{BTreeMap, BTreeSet};
use std::ffi::OsString;
use std::fs;
use std::path::Path;
use std::process::ExitCode;

use serde_json::json;

use crate::io::{merge_key_types, read_json_object, snapshot_value_types};
use crate::python_backend::{run_python_command, run_python_file};

fn run_pipeline_and_capture(
    repo_root: &Path,
    args: &[OsString],
    out_file: &Path,
) -> Result<(), String> {
    let output = run_python_command(repo_root, args, false)?;
    if !output.status.success() {
        return Err(format!(
            "python command failed ({:?}): {}",
            args,
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    fs::write(out_file, String::from_utf8_lossy(&output.stdout).as_bytes())
        .map_err(|e| format!("failed to write {}: {e}", out_file.display()))
}

fn run_python_file_and_capture(
    repo_root: &Path,
    file_under_tools: &str,
    args: &[OsString],
    out_file: &Path,
) -> Result<(), String> {
    let output = run_python_file(repo_root, file_under_tools, args)?;
    if !output.status.success() {
        return Err(format!(
            "python file failed ({}): {}",
            file_under_tools,
            String::from_utf8_lossy(&output.stderr)
        ));
    }
    fs::write(out_file, String::from_utf8_lossy(&output.stdout).as_bytes())
        .map_err(|e| format!("failed to write {}: {e}", out_file.display()))
}

pub fn freeze_baseline(repo_root: &Path) -> ExitCode {
    let baseline_dir = repo_root
        .join("tools")
        .join("rust_pipeline_v6")
        .join("baseline");
    if let Err(err) = fs::create_dir_all(&baseline_dir) {
        eprintln!("error: failed to create baseline dir: {err}");
        return ExitCode::FAILURE;
    }

    let validate_out = baseline_dir.join("validate_fast.stdout.txt");
    let smoke_out = baseline_dir.join("test_v6_smoke.stdout.txt");
    let calibrate_out = baseline_dir.join("calibrate_elite.stdout.txt");

    if let Err(err) = run_pipeline_and_capture(
        repo_root,
        &[OsString::from("validate"), OsString::from("--fast")],
        &validate_out,
    ) {
        eprintln!("error: {err}");
        return ExitCode::FAILURE;
    }
    let no_args: Vec<OsString> = Vec::new();
    if let Err(err) =
        run_python_file_and_capture(repo_root, "test_v6_smoke.py", &no_args, &smoke_out)
    {
        eprintln!("error: {err}");
        return ExitCode::FAILURE;
    }
    if let Err(err) = run_pipeline_and_capture(
        repo_root,
        &[
            OsString::from("calibrate"),
            OsString::from("--preset"),
            OsString::from("EliteStandard"),
            OsString::from("--hours"),
            OsString::from("4"),
            OsString::from("--target"),
            OsString::from("0.2"),
        ],
        &calibrate_out,
    ) {
        eprintln!("error: {err}");
        return ExitCode::FAILURE;
    }

    let preset_files = [
        "optimized_rss_config_elitestandard_v6.json",
        "optimized_rss_config_standardmilsim_v6.json",
        "optimized_rss_config_tacticalaction_v6.json",
    ];
    let mut merged_types: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    for fname in preset_files {
        let path = repo_root.join("tools").join(fname);
        let obj = match read_json_object(&path) {
            Ok(v) => v,
            Err(err) => {
                eprintln!("error: {err}");
                return ExitCode::FAILURE;
            }
        };
        let kinds = snapshot_value_types(&obj);
        merge_key_types(&mut merged_types, &kinds);
    }
    let schema_out = baseline_dir.join("preset_v6_schema_snapshot.json");
    let schema = json!({
        "files": preset_files,
        "schema": merged_types.into_iter().map(|(k, v)| (k, v.into_iter().collect::<Vec<_>>())).collect::<BTreeMap<_, _>>(),
    });
    if let Err(err) = fs::write(
        &schema_out,
        serde_json::to_string_pretty(&schema).unwrap_or_else(|_| "{}".to_string()),
    ) {
        eprintln!("error: failed to write schema snapshot: {err}");
        return ExitCode::FAILURE;
    }

    println!("[Rust baseline] wrote {}", baseline_dir.display());
    ExitCode::SUCCESS
}
