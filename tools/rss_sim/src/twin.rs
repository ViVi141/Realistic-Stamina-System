use crate::constants::RssConstants;
use pyo3::prelude::*;
use pyo3::types::{PyDict, PyList, PyModule};
use std::collections::HashMap;

pub fn import_rss_digital_twin_module<'py>(py: Python<'py>) -> PyResult<Bound<'py, PyModule>> {
    if let Ok(module) = PyModule::import_bound(py, "rss_digital_twin_fix") {
        return Ok(module);
    }

    let sys = PyModule::import_bound(py, "sys")?;
    let path_any = sys.getattr("path")?;
    let path = path_any.downcast::<PyList>()?;
    let candidates = ["/workspace/tools", "/workspace"];
    for p in candidates {
        let mut exists = false;
        for item in path.iter() {
            if let Ok(s) = item.extract::<String>() {
                if s == p {
                    exists = true;
                    break;
                }
            }
        }
        if !exists {
            path.append(p)?;
        }
    }
    PyModule::import_bound(py, "rss_digital_twin_fix")
}

fn map_to_pydict<'py>(py: Python<'py>, map: &HashMap<String, f64>) -> PyResult<Bound<'py, PyDict>> {
    let out = PyDict::new_bound(py);
    for (k, v) in map {
        out.set_item(k, *v)?;
    }
    Ok(out)
}

pub struct RSSDigitalTwin {
    inner: Py<PyAny>,
    pub constants: RssConstants,
}

impl RSSDigitalTwin {
    pub fn new(py: Python<'_>, constants: RssConstants) -> PyResult<Self> {
        let module = import_rss_digital_twin_module(py)?;
        let constants_cls = module.getattr("RSSConstants")?;
        let twin_cls = module.getattr("RSSDigitalTwin")?;
        let kwargs = map_to_pydict(py, &constants.to_params_map())?;
        let constants_obj = constants_cls.call((), Some(&kwargs))?;
        let twin_obj = twin_cls.call1((constants_obj,))?;
        Ok(Self {
            inner: twin_obj.unbind(),
            constants,
        })
    }

    pub fn reset(&mut self, py: Python<'_>) -> PyResult<()> {
        self.inner.bind(py).call_method0("reset")?;
        Ok(())
    }

    pub fn step(
        &mut self,
        py: Python<'_>,
        speed: f64,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
        stance: i32,
        movement_type: i32,
        current_time: f64,
        enable_randomness: bool,
        wind_drag: f64,
        time_delta_override: Option<f64>,
    ) -> PyResult<()> {
        let kwargs = PyDict::new_bound(py);
        kwargs.set_item("enable_randomness", enable_randomness)?;
        kwargs.set_item("wind_drag", wind_drag)?;
        if let Some(dt) = time_delta_override {
            kwargs.set_item("time_delta_override", dt)?;
        }
        self.inner.bind(py).call_method(
            "step",
            (
                speed,
                current_weight,
                grade_percent,
                terrain_factor,
                stance,
                movement_type,
                current_time,
            ),
            Some(&kwargs),
        )?;
        Ok(())
    }

    pub fn game_player_tick(
        &mut self,
        py: Python<'_>,
        intent_phase: i32,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
        stance: i32,
        current_time: f64,
        time_delta: f64,
        wind_drag: f64,
        enable_randomness: bool,
    ) -> PyResult<f64> {
        let kwargs = PyDict::new_bound(py);
        kwargs.set_item("wind_drag", wind_drag)?;
        kwargs.set_item("enable_randomness", enable_randomness)?;
        self.inner
            .bind(py)
            .call_method(
                "game_player_tick",
                (
                    intent_phase,
                    current_weight,
                    grade_percent,
                    terrain_factor,
                    stance,
                    current_time,
                    time_delta,
                ),
                Some(&kwargs),
            )?
            .extract::<f64>()
    }

    pub fn set_dt(&mut self, py: Python<'_>, dt: f64) -> PyResult<()> {
        self.inner.bind(py).setattr("_dt", dt)?;
        Ok(())
    }

    pub fn stamina(&self, py: Python<'_>) -> PyResult<f64> {
        self.inner.bind(py).getattr("stamina")?.extract::<f64>()
    }

    pub fn set_scenario_wind_drag(&mut self, py: Python<'_>, drag: f64) -> PyResult<()> {
        self.inner.bind(py).setattr("_scenario_wind_drag", drag)?;
        Ok(())
    }

    pub fn set_environment(
        &mut self,
        py: Python<'_>,
        temperature: f64,
        wind_speed: f64,
        cold_stress: f64,
        cold_static_penalty: f64,
    ) -> PyResult<()> {
        let env = self.inner.bind(py).getattr("environment_factor")?;
        env.setattr("temperature", temperature)?;
        env.setattr("wind_speed", wind_speed)?;
        env.setattr("cold_stress", cold_stress)?;
        env.setattr("cold_static_penalty", cold_static_penalty)?;
        Ok(())
    }

    pub fn get_scenario_wind_drag(&self, py: Python<'_>) -> PyResult<f64> {
        self.inner
            .bind(py)
            .getattr("_scenario_wind_drag")?
            .extract::<f64>()
    }
}
