use crate::constants::{MOVEMENT_IDLE, RSS_PLAYER_TICK_SEC, STANCE_STAND};
use crate::twin::RSSDigitalTwin;
use serde::{Deserialize, Serialize};

fn default_grade_pct() -> f64 {
    0.0
}
fn default_terrain() -> f64 {
    1.0
}
fn default_label() -> String {
    String::new()
}
fn default_description() -> String {
    String::new()
}
fn default_temperature() -> f64 {
    20.0
}
fn default_wind_speed() -> f64 {
    0.0
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Phase {
    pub duration_s: f64,
    pub speed_ms: f64,
    pub movement: i32,
    pub stance: i32,
    #[serde(default = "default_grade_pct")]
    pub grade_pct: f64,
    #[serde(default = "default_terrain")]
    pub terrain: f64,
    #[serde(default = "default_label")]
    pub label: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Mission {
    pub name: String,
    pub load_kg: f64,
    pub phases: Vec<Phase>,
    #[serde(default = "default_description")]
    pub description: String,
    #[serde(default = "default_temperature")]
    pub temperature: f64,
    #[serde(default = "default_wind_speed")]
    pub wind_speed: f64,
}

impl Mission {
    pub fn total_duration_s(&self) -> f64 {
        self.phases.iter().map(|p| p.duration_s).sum()
    }

    pub fn current_weight(&self) -> f64 {
        90.0 + self.load_kg
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MissionResult {
    pub mission_name: String,
    pub total_duration_s: f64,
    pub stamina_trace: Vec<f64>,
    pub speed_trace: Vec<f64>,
    pub min_stamina: f64,
    pub mean_stamina_active: f64,
    pub recovery_gain: f64,
    pub idle_duration_s: f64,
    pub exhaustion_duration_s: f64,
    pub completion_possible: bool,
    pub observed_depletion_pct_per_s: f64,
}

fn configure_twin_for_mission(twin: &mut RSSDigitalTwin, mission: &Mission) {
    twin.reset();
    twin.scenario_wind_drag = 0.0;
    let mut cold_stress = 0.0;
    let mut cold_static_penalty = 0.0;
    if mission.temperature < 5.0 {
        let c = &twin.constants;
        cold_stress = (5.0 - mission.temperature) * c.env_temperature_cold_recovery_penalty_coeff;
        cold_static_penalty = (5.0 - mission.temperature) * c.env_temperature_cold_static_penalty;
    }
    twin.environment_factor.temperature = mission.temperature;
    twin.environment_factor.wind_speed = mission.wind_speed;
    twin.environment_factor.cold_stress = cold_stress;
    twin.environment_factor.cold_static_penalty = cold_static_penalty;

    if mission.wind_speed >= 1.0 {
        let wind_drag = (mission.wind_speed * twin.constants.env_wind_resistance_coeff).min(1.0);
        twin.scenario_wind_drag = wind_drag;
    }
}

pub fn simulate_mission(
    twin: &mut RSSDigitalTwin,
    mission: &Mission,
    fast_mode: bool,
    summary_only: bool,
) -> MissionResult {
    configure_twin_for_mission(twin, mission);

    let dt = if fast_mode { 0.05 } else { RSS_PLAYER_TICK_SEC };

    let mut current_time = 0.0;
    let mut stamina_trace: Vec<f64> = Vec::new();
    let mut speed_trace: Vec<f64> = Vec::new();
    let mut total_recovery_gain = 0.0;
    let mut idle_duration_s = 0.0;
    let mut exhaustion_duration_s = 0.0;
    let current_weight = mission.current_weight();
    let wind_drag = twin.scenario_wind_drag;

    let mut min_stamina = f64::INFINITY;
    let mut active_sum = 0.0;
    let mut active_count = 0usize;
    let mut depletion_sum = 0.0;
    let mut depletion_count = 0usize;
    let mut prev_stamina_step = 1.0;

    for phase in &mission.phases {
        let steps = (phase.duration_s / dt) as usize;
        let intent_phase = phase.movement;
        let is_idle_phase = intent_phase == MOVEMENT_IDLE;
        if is_idle_phase {
            idle_duration_s += phase.duration_s;
        }

        for _ in 0..steps {
            let prev_stamina = twin.stamina;
            let drain_speed = if is_idle_phase {
                twin.game_player_tick(
                    MOVEMENT_IDLE,
                    current_weight,
                    phase.grade_pct,
                    phase.terrain,
                    phase.stance,
                    current_time,
                    dt,
                    wind_drag,
                    false,
                )
            } else {
                twin.game_player_tick(
                    intent_phase,
                    current_weight,
                    phase.grade_pct,
                    phase.terrain,
                    phase.stance,
                    current_time,
                    dt,
                    wind_drag,
                    false,
                )
            };

            current_time += dt;
            let now_stamina = twin.stamina;

            if summary_only {
                if now_stamina < min_stamina {
                    min_stamina = now_stamina;
                }
                if !is_idle_phase {
                    active_sum += now_stamina;
                    active_count += 1;
                }
                if now_stamina < prev_stamina_step - 1e-9 && prev_stamina_step > 0.82 {
                    depletion_sum += (prev_stamina_step - now_stamina) / dt * 100.0;
                    depletion_count += 1;
                }
                prev_stamina_step = now_stamina;
            } else {
                stamina_trace.push(now_stamina);
                speed_trace.push(drain_speed);
            }

            if now_stamina > prev_stamina {
                total_recovery_gain += now_stamina - prev_stamina;
            }
            if now_stamina < 0.15 {
                exhaustion_duration_s += dt;
            }
        }
    }

    let (min_stamina, mean_active, observed_depletion_pct_per_s) = if summary_only {
        let min_val = if min_stamina.is_finite() { min_stamina } else { 0.0 };
        let mean = if active_count > 0 {
            active_sum / active_count as f64
        } else {
            min_val
        };
        let obs = if depletion_count > 0 {
            depletion_sum / depletion_count as f64
        } else {
            0.0
        };
        (min_val, mean, obs)
    } else {
        let mut active_stamina: Vec<f64> = Vec::new();
        let mut t = 0.0;
        for phase in &mission.phases {
            let n_steps = (phase.duration_s / dt) as usize;
            for j in 0..n_steps {
                let idx = (t / dt) as usize + j;
                if idx < stamina_trace.len() && phase.movement != MOVEMENT_IDLE {
                    active_stamina.push(stamina_trace[idx]);
                }
            }
            t += phase.duration_s;
        }

        let min_val = if stamina_trace.is_empty() {
            0.0
        } else {
            stamina_trace
                .iter()
                .fold(f64::INFINITY, |acc, v| if *v < acc { *v } else { acc })
        };
        let mean = if active_stamina.is_empty() {
            min_val
        } else {
            active_stamina.iter().sum::<f64>() / active_stamina.len() as f64
        };

        let mut depletion_samples: Vec<f64> = Vec::new();
        for k in 1..stamina_trace.len() {
            let prev_s = stamina_trace[k - 1];
            let cur_s = stamina_trace[k];
            if cur_s < prev_s - 1e-9 && prev_s > 0.82 {
                depletion_samples.push((prev_s - cur_s) / dt * 100.0);
            }
        }
        let obs = if depletion_samples.is_empty() {
            0.0
        } else {
            depletion_samples.iter().sum::<f64>() / depletion_samples.len() as f64
        };
        (min_val, mean, obs)
    };

    MissionResult {
        mission_name: mission.name.clone(),
        total_duration_s: mission.total_duration_s(),
        stamina_trace,
        speed_trace,
        min_stamina,
        mean_stamina_active: mean_active,
        recovery_gain: total_recovery_gain,
        idle_duration_s,
        exhaustion_duration_s,
        completion_possible: min_stamina > 0.0,
        observed_depletion_pct_per_s,
    }
}

pub fn ideal_march_mission(hours: f64) -> Mission {
    Mission {
        name: "ideal_march".to_string(),
        load_kg: 35.0,
        phases: vec![Phase {
            duration_s: hours * 3600.0,
            speed_ms: 1.39,
            movement: 1,
            stance: STANCE_STAND,
            grade_pct: 0.0,
            terrain: 1.0,
            label: "march".to_string(),
        }],
        description: String::new(),
        temperature: 20.0,
        wind_speed: 0.0,
    }
}
