use std::env;
use std::ffi::OsString;
use std::process::ExitCode;

mod baseline;
mod constraints;
mod digital_twin;
mod io;
mod mission;
mod pipeline;
mod python_backend;

fn usage() {
    eprintln!(
        "Usage:
  rss_pipeline_v6_rs validate [--fast]
  rss_pipeline_v6_rs calibrate [args...]
  rss_pipeline_v6_rs optimize [args...]
  rss_pipeline_v6_rs dual-run [--fast]
  rss_pipeline_v6_rs freeze-baseline"
    );
}

fn main() -> ExitCode {
    let mut args = env::args_os();
    let _bin = args.next();
    let Some(subcommand) = args.next() else {
        usage();
        return ExitCode::from(2);
    };
    let rest: Vec<OsString> = args.collect();
    let repo_root = match python_backend::find_repo_root() {
        Ok(path) => path,
        Err(err) => {
            eprintln!("error: {err}");
            return ExitCode::FAILURE;
        }
    };

    match subcommand.to_string_lossy().as_ref() {
        "validate" => pipeline::run_passthrough(&repo_root, "validate", &rest),
        "calibrate" => pipeline::run_passthrough(&repo_root, "calibrate", &rest),
        "optimize" => pipeline::run_passthrough(&repo_root, "optimize", &rest),
        "dual-run" => pipeline::run_dual_validate(&repo_root, &rest),
        "freeze-baseline" => baseline::freeze_baseline(&repo_root),
        _ => {
            usage();
            ExitCode::from(2)
        }
    }
}
