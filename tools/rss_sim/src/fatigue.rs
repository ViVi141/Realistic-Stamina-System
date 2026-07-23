use crate::constants::{
    RSS_IDLE_SPEED_THRESHOLD_MPS, V6_CP_FATIGUE_K, V6_FATIGUE_I_MAX, V6_FATIGUE_INTEGRAL_SCALE,
    V6_FATIGUE_K_LOAD, V6_FATIGUE_K_RECOVERY, V6_FATIGUE_K_SLOPE, V6_FATIGUE_K_TERRAIN,
    V6_MAX_FATIGUE_PENALTY,
};
use crate::math::clip_f64;

#[derive(Clone, Debug)]
pub struct TwinFatigueSystem {
    pub fatigue_accumulation: f64,
    pub fatigue_integral: f64,
    pub last_fatigue_decay_time: f64,
    pub last_rest_start_time: f64,
    pub fatigue_decay_rate: f64,
    pub fatigue_decay_min_rest_time: f64,
}

impl Default for TwinFatigueSystem {
    fn default() -> Self {
        Self {
            fatigue_accumulation: 0.0,
            fatigue_integral: 0.0,
            last_fatigue_decay_time: 0.0,
            last_rest_start_time: -1.0,
            fatigue_decay_rate: 0.0005,
            fatigue_decay_min_rest_time: 15.0,
        }
    }
}

impl TwinFatigueSystem {
    pub fn initialize(&mut self, current_time: f64) {
        self.fatigue_accumulation = 0.0;
        self.fatigue_integral = 0.0;
        self.last_fatigue_decay_time = current_time;
        self.last_rest_start_time = -1.0;
    }

    pub fn get_fatigue_integral_norm(&self) -> f64 {
        if V6_FATIGUE_I_MAX <= 0.0 {
            return 0.0;
        }
        clip_f64(self.fatigue_integral / V6_FATIGUE_I_MAX, 0.0, 1.0)
    }

    pub fn get_max_stamina_cap(&self) -> f64 {
        1.0 - self.fatigue_accumulation
    }

    pub fn get_cp_fatigue_multiplier(&self) -> f64 {
        let norm = self.get_fatigue_integral_norm();
        let mult = 1.0 - V6_CP_FATIGUE_K * norm;
        mult.max(0.82)
    }

    pub fn process_integral(
        &mut self,
        power_watts: f64,
        load_kg: f64,
        grade_percent: f64,
        terrain_factor: f64,
        time_delta_sec: f64,
        current_speed_ms: f64,
        critical_power_watts: f64,
    ) {
        if time_delta_sec <= 0.0 {
            return;
        }
        let body_w = 90.0;
        let load_ratio = if body_w > 0.0 { load_kg / body_w } else { 0.0 };
        let g = grade_percent * 0.01;
        let mut slope_term = 0.0;
        if g > 0.0 {
            slope_term = V6_FATIGUE_K_SLOPE * g * g;
        }
        let mut w = 1.0
            + V6_FATIGUE_K_LOAD * load_ratio
            + slope_term
            + V6_FATIGUE_K_TERRAIN * (terrain_factor - 1.0);
        if w < 0.5 {
            w = 0.5;
        }
        let mut cp_ref = critical_power_watts;
        if cp_ref <= 1.0 {
            cp_ref = 780.0;
        }
        let mut drive_p = power_watts - cp_ref;
        if drive_p < 0.0 {
            drive_p = 0.0;
        }
        let i_norm = self.get_fatigue_integral_norm();
        let mut r = 0.0;
        if current_speed_ms < 0.05 || power_watts < cp_ref * 0.5 {
            let one_minus = 1.0 - i_norm;
            r = V6_FATIGUE_K_RECOVERY * one_minus * one_minus * power_watts;
        }
        let d_i = (w * drive_p - r) * time_delta_sec * V6_FATIGUE_INTEGRAL_SCALE;
        self.fatigue_integral = clip_f64(self.fatigue_integral + d_i, 0.0, V6_FATIGUE_I_MAX);

        let legacy_from_i = self.fatigue_integral * V6_MAX_FATIGUE_PENALTY;
        if legacy_from_i > self.fatigue_accumulation {
            self.fatigue_accumulation = legacy_from_i;
        }
        if self.fatigue_accumulation > V6_MAX_FATIGUE_PENALTY {
            self.fatigue_accumulation = V6_MAX_FATIGUE_PENALTY;
        }
    }

    pub fn process_decay(&mut self, current_time: f64, current_speed: f64) {
        let is_moving = current_speed >= RSS_IDLE_SPEED_THRESHOLD_MPS;
        if !is_moving {
            if self.last_rest_start_time < 0.0 {
                self.last_rest_start_time = current_time;
            }
            if self.fatigue_accumulation > 0.0 || self.fatigue_integral > 0.0 {
                let rest_duration = current_time - self.last_rest_start_time;
                if rest_duration >= self.fatigue_decay_min_rest_time {
                    let fatigue_time_delta = current_time - self.last_fatigue_decay_time;
                    if fatigue_time_delta > 0.0 {
                        self.fatigue_accumulation = (self.fatigue_accumulation
                            - (self.fatigue_decay_rate * (fatigue_time_delta / 0.2)))
                            .max(0.0);
                        self.fatigue_integral = (self.fatigue_integral
                            - (self.fatigue_decay_rate * 0.5 * (fatigue_time_delta / 0.2)))
                            .max(0.0);
                        self.last_fatigue_decay_time = current_time;
                    }
                }
            }
            self.last_fatigue_decay_time = current_time;
        } else {
            self.last_rest_start_time = -1.0;
        }
    }
}
