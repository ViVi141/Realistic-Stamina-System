use pyo3::exceptions::PyValueError;
use pyo3::prelude::*;
use pyo3::types::{PyAny, PyDict, PyList};
use rayon::prelude::*;
use std::collections::HashMap;

pub mod constants;
pub mod constraints;
pub mod cp_wprime;
pub mod drain;
pub mod environment;
pub mod fatigue;
pub mod math;
pub mod metabolism;
pub mod mission;
pub mod twin;

use constants::{merge_game_aligned_params, RssConstants};
use constraints as constraints_mod;
use mission::{simulate_mission as simulate_mission_impl, Mission, MissionResult};
use twin::RSSDigitalTwin;

fn py_dict_to_hashmap(params: &Bound<'_, PyDict>) -> PyResult<HashMap<String, f64>> {
    let mut out = HashMap::new();
    for (k, v) in params.iter() {
        let key = k.extract::<String>()?;
        let value = v.extract::<f64>()?;
        out.insert(key, value);
    }
    Ok(out)
}

fn mission_result_to_pydict<'py>(
    py: Python<'py>,
    result: &MissionResult,
    summary_only: bool,
) -> PyResult<Bound<'py, PyDict>> {
    let d = PyDict::new_bound(py);
    d.set_item("mission_name", &result.mission_name)?;
    d.set_item("total_duration_s", result.total_duration_s)?;
    if summary_only {
        d.set_item("stamina_trace", PyList::empty_bound(py))?;
        d.set_item("speed_trace", PyList::empty_bound(py))?;
    } else {
        d.set_item("stamina_trace", result.stamina_trace.clone())?;
        d.set_item("speed_trace", result.speed_trace.clone())?;
    }
    d.set_item("min_stamina", result.min_stamina)?;
    d.set_item("mean_stamina_active", result.mean_stamina_active)?;
    d.set_item("recovery_gain", result.recovery_gain)?;
    d.set_item("idle_duration_s", result.idle_duration_s)?;
    d.set_item("exhaustion_duration_s", result.exhaustion_duration_s)?;
    d.set_item("completion_possible", result.completion_possible)?;
    d.set_item(
        "observed_depletion_pct_per_s",
        result.observed_depletion_pct_per_s,
    )?;
    Ok(d)
}

fn parse_missions_json(missions_json: &str) -> PyResult<Vec<Mission>> {
    if let Ok(v) = serde_json::from_str::<Vec<Mission>>(missions_json) {
        return Ok(v);
    }
    #[derive(serde::Deserialize)]
    struct Wrapper {
        missions: Vec<Mission>,
    }
    let wrapper = serde_json::from_str::<Wrapper>(missions_json).map_err(|e| {
        PyValueError::new_err(format!(
            "missions_json must be a JSON array of missions or {{\"missions\": [...]}}: {}",
            e
        ))
    })?;
    Ok(wrapper.missions)
}

fn parse_params_json(params_json: &str) -> PyResult<HashMap<String, f64>> {
    if params_json.trim().is_empty() {
        return Ok(HashMap::new());
    }
    serde_json::from_str::<HashMap<String, f64>>(params_json)
        .map_err(|e| PyValueError::new_err(format!("invalid params_json: {}", e)))
}

fn run_mission_suite_native(
    constants: RssConstants,
    missions: Vec<Mission>,
    fast_mode: bool,
    summary_only: bool,
    parallel: bool,
) -> Vec<MissionResult> {
    if parallel && missions.len() > 1 {
        return missions
            .par_iter()
            .map(|m| {
                let mut twin = RSSDigitalTwin::new(constants.clone());
                simulate_mission_impl(&mut twin, m, fast_mode, summary_only)
            })
            .collect();
    }

    let mut results = Vec::with_capacity(missions.len());
    for m in missions {
        let mut twin = RSSDigitalTwin::new(constants.clone());
        results.push(simulate_mission_impl(
            &mut twin,
            &m,
            fast_mode,
            summary_only,
        ));
    }
    results
}

#[pyfunction]
fn is_available() -> bool {
    true
}

#[pyfunction]
fn get_drain_velocity_ms(measured: f64, theoretical: f64) -> f64 {
    drain::get_drain_velocity_ms(measured, theoretical)
}

#[pyfunction]
#[pyo3(signature = (params, mission_json, fast_mode, summary_only=false))]
fn simulate_mission(
    py: Python<'_>,
    params: &Bound<'_, PyAny>,
    mission_json: &str,
    fast_mode: bool,
    summary_only: bool,
) -> PyResult<Py<PyAny>> {
    let params_dict = params.downcast::<PyDict>()?;
    let params_map = py_dict_to_hashmap(params_dict)?;
    let merged = merge_game_aligned_params(&params_map);
    let constants = RssConstants::from_params(&merged);
    let mission = serde_json::from_str::<Mission>(mission_json).map_err(|e| {
        PyValueError::new_err(format!("mission_json must be a single Mission object: {}", e))
    })?;

    let result = py.allow_threads(|| {
        let mut twin = RSSDigitalTwin::new(constants);
        simulate_mission_impl(&mut twin, &mission, fast_mode, summary_only)
    });
    let d = mission_result_to_pydict(py, &result, summary_only)?;
    Ok(d.into_any().unbind())
}

#[pyfunction]
#[pyo3(signature = (params, missions_json, fast_mode, summary_only=true, parallel=true))]
fn run_mission_suite(
    py: Python<'_>,
    params: &Bound<'_, PyAny>,
    missions_json: &str,
    fast_mode: bool,
    summary_only: bool,
    parallel: bool,
) -> PyResult<Py<PyAny>> {
    let params_dict = params.downcast::<PyDict>()?;
    let params_map = py_dict_to_hashmap(params_dict)?;
    let merged = merge_game_aligned_params(&params_map);
    let constants = RssConstants::from_params(&merged);
    let missions = parse_missions_json(missions_json)?;

    let results = py.allow_threads(|| {
        run_mission_suite_native(constants, missions, fast_mode, summary_only, parallel)
    });

    let out = PyList::empty_bound(py);
    for result in results {
        let d = mission_result_to_pydict(py, &result, summary_only)?;
        out.append(d)?;
    }
    Ok(out.into_any().unbind())
}

#[pyfunction]
fn simulate_ideal_march_aerobic_end(
    _py: Python<'_>,
    params_json: &str,
    hours: f64,
    encumbrance_kg: f64,
    dt_sec: f64,
) -> PyResult<f64> {
    let params = parse_params_json(params_json)?;
    let param_ref = if params.is_empty() {
        None
    } else {
        Some(params)
    };
    Ok(constraints_mod::simulate_ideal_march_aerobic_end(
        hours,
        encumbrance_kg,
        dt_sec,
        param_ref.as_ref(),
    ))
}

#[pyfunction]
#[pyo3(signature = (params_json=None))]
fn evaluate_hard_constraints(
    py: Python<'_>,
    params_json: Option<&str>,
) -> PyResult<Py<PyAny>> {
    let parsed_params = if let Some(s) = params_json {
        let p = parse_params_json(s)?;
        if p.is_empty() {
            None
        } else {
            Some(p)
        }
    } else {
        None
    };

    let report = py.allow_threads(|| {
        constraints_mod::evaluate_hard_constraints(parsed_params.as_ref(), false)
    });
    let d = PyDict::new_bound(py);
    d.set_item("all_hard_passed", report.all_hard_passed)?;
    let checks = PyList::empty_bound(py);
    for c in report.checks {
        let row = PyDict::new_bound(py);
        row.set_item("name", c.name)?;
        row.set_item("passed", c.passed)?;
        row.set_item("detail", c.detail)?;
        row.set_item("hard", c.hard)?;
        checks.append(row)?;
    }
    d.set_item("checks", checks)?;
    Ok(d.into_any().unbind())
}

#[pymodule]
fn rss_sim(_py: Python<'_>, m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(is_available, m)?)?;
    m.add_function(wrap_pyfunction!(get_drain_velocity_ms, m)?)?;
    m.add_function(wrap_pyfunction!(simulate_mission, m)?)?;
    m.add_function(wrap_pyfunction!(run_mission_suite, m)?)?;
    m.add_function(wrap_pyfunction!(simulate_ideal_march_aerobic_end, m)?)?;
    m.add_function(wrap_pyfunction!(evaluate_hard_constraints, m)?)?;
    Ok(())
}
