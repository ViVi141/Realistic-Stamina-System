#![allow(dead_code)]

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MovementType {
    Idle = 0,
    Walk = 1,
    Run = 2,
    Sprint = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Stance {
    Stand = 0,
    Crouch = 1,
    Prone = 2,
}

#[derive(Debug, Clone)]
pub struct V6Defaults {
    pub critical_power_watts: f64,
    pub w_prime_max_joules: f64,
    pub w_prime_recovery_w_per_s: f64,
    pub sprint_power_cap_watts: f64,
}

impl Default for V6Defaults {
    fn default() -> Self {
        Self {
            critical_power_watts: 400.0,
            w_prime_max_joules: 20_000.0,
            w_prime_recovery_w_per_s: 12.0,
            sprint_power_cap_watts: 1450.0,
        }
    }
}
