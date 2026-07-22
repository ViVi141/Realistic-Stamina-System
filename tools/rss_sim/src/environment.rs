use crate::constants::RssConstants;
use crate::math::clip_f64;

#[derive(Clone, Debug)]
pub struct EnvironmentFactor {
    pub constants: RssConstants,
    pub heat_stress: f64,
    pub cold_stress: f64,
    pub cold_static_penalty: f64,
    pub surface_wetness: f64,
    pub temperature: f64,
    pub wind_speed: f64,
}

impl EnvironmentFactor {
    pub fn new(constants: &RssConstants) -> Self {
        Self {
            constants: constants.clone(),
            heat_stress: 0.0,
            cold_stress: 0.0,
            cold_static_penalty: 0.0,
            surface_wetness: 0.0,
            temperature: 20.0,
            wind_speed: 0.0,
        }
    }

    pub fn adjust_energy_for_temperature(&self, base_drain: f64) -> f64 {
        let t_eff = self.temperature - 1.35 * self.wind_speed.max(0.0).sqrt();
        let mut extra_watts = 0.0;
        let t_low = 18.0;
        let t_high = 27.0;
        if t_eff < t_low {
            let dt_cold = t_low - t_eff;
            extra_watts = 0.15 * (dt_cold * dt_cold);
        } else if t_eff > t_high {
            let dt_hot = t_eff - t_high;
            extra_watts = 2.0 * (dt_hot * dt_hot);
        }
        let extra_per_tick = extra_watts * self.constants.energy_to_stamina_coeff * 0.2;
        base_drain + extra_per_tick
    }

    pub fn get_heat_stress_multiplier(&self, indoor: bool) -> f64 {
        let threshold = 26.0;
        let mut mult = if self.temperature < threshold {
            1.0
        } else {
            1.0 + (self.temperature - threshold) * 0.02
        };
        if indoor {
            mult *= 1.0 - self.constants.env_heat_stress_indoor_reduction;
        }
        clip_f64(mult, 1.0, self.constants.env_heat_stress_max_multiplier)
    }

    pub fn get_heat_stress_penalty(&self) -> f64 {
        let mult = self.get_heat_stress_multiplier(false);
        (mult - 1.0).max(0.0).min(0.5)
    }

    pub fn get_quick_environment_multiplier(&self, mud_factor: f64, wind_drag: f64) -> f64 {
        let mut mult = 1.0 + mud_factor * 0.35;
        if wind_drag > 0.0 {
            mult *= 1.0 + wind_drag;
        }
        let heat_penalty = self.get_heat_stress_penalty();
        if heat_penalty > 0.05 {
            mult *= 1.0 + heat_penalty * 0.5;
        }
        mult
    }
}
