#![allow(dead_code)]

use crate::digital_twin::{MovementType, Stance};

#[derive(Debug, Clone)]
pub struct Phase {
    pub duration_s: f64,
    pub speed_ms: f64,
    pub movement: MovementType,
    pub stance: Stance,
    pub grade_pct: f64,
    pub terrain: f64,
    pub label: String,
}

#[derive(Debug, Clone)]
pub struct Mission {
    pub name: String,
    pub load_kg: f64,
    pub phases: Vec<Phase>,
    pub description: String,
    pub temperature: f64,
    pub wind_speed: f64,
}

impl Mission {
    pub fn total_duration_s(&self) -> f64 {
        self.phases.iter().map(|p| p.duration_s).sum()
    }
}

#[derive(Debug, Clone, Default)]
pub struct MissionResult {
    pub mission_name: String,
    pub total_duration_s: f64,
    pub min_stamina: f64,
    pub mean_stamina_active: f64,
    pub recovery_gain: f64,
    pub idle_duration_s: f64,
    pub exhaustion_duration_s: f64,
    pub completion_possible: bool,
    pub observed_depletion_pct_per_s: f64,
}
