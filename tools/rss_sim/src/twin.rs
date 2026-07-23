use crate::constants::{
    RssConstants, EPOC_MAX_POWER_EXCESS_RATIO, EXHAUSTION_LIMP_SPEED,
    LOADED_RUN_DRAIN_MAX_MULT, LOADED_RUN_DRAIN_REF_KG, LOADED_RUN_DRAIN_START_KG,
    MIN_SPEED_MULTIPLIER, MOVEMENT_IDLE, MOVEMENT_RUN, MOVEMENT_SPRINT, MOVEMENT_WALK,
    RSS_IDLE_SPEED_THRESHOLD_MPS, RSS_PLAYER_TICK_SEC, RUN_VELOCITY_THRESHOLD,
    SPRINT_ENCUMBRANCE_PENALTY_MULT, SPRINT_GAIT_MIN_OVER_RUN_RATIO, STAMINA_TICK_SEC,
    V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V6_AEROBIC_CRUISE_MAX_MS,
    V6_CRITICAL_POWER_WATTS_DEFAULT, V6_STAMINA_DRAIN_CALIBRATION,
    V6_SPRINT_POWER_CAP_WATTS_DEFAULT, VELOCITY_HORIZ_CAP_MS, WALK_VELOCITY_THRESHOLD,
};
use crate::cp_wprime::V6CriticalPowerState;
use crate::drain::{
    get_drain_velocity_ms, get_metabolic_corrected_speed_multiplier,
};
use crate::environment::EnvironmentFactor;
use crate::fatigue::TwinFatigueSystem;
use crate::math::clip_f64;
use crate::metabolism::{
    calculate_slope_adjusted_target_speed, compute_cp_watts, invert_speed_for_power_watts,
    metabolism_power_watts_damped,
};

pub struct RSSDigitalTwin {
    pub constants: RssConstants,
    pub environment_factor: EnvironmentFactor,
    pub stamina: f64,
    pub stamina_history: Vec<f64>,
    pub speed_history: Vec<f64>,
    pub time_history: Vec<f64>,
    pub recovery_rate_history: Vec<f64>,
    pub drain_rate_history: Vec<f64>,
    pub current_time: f64,
    pub last_speed: f64,
    pub epoc_delay_start_time: f64,
    pub is_in_epoc_delay: bool,
    pub speed_before_stop: f64,
    pub rest_duration_minutes: f64,
    pub exercise_duration_minutes: f64,
    pub last_movement_time: f64,
    pub max_history_length: usize,
    pub scenario_wind_drag: f64,
    pub sprint_start_time: f64,
    pub sprint_cooldown_until: f64,
    pub measured_velocity_ms: f64,
    pub applied_speed_limit_mult: f64,
    pub applied_speed_limit_ms: f64,
    pub fatigue: TwinFatigueSystem,
    pub v6_cp_state: V6CriticalPowerState,
    pub base_drain_rate_by_velocity: f64,
    pub final_drain_rate: f64,
    pub recovery_rate: f64,
    pub net_change: f64,
}

impl RSSDigitalTwin {
    pub fn new(constants: RssConstants) -> Self {
        let environment_factor = EnvironmentFactor::new(&constants);
        let cp0 = if constants.critical_power_watts > 1.0 {
            constants.critical_power_watts
        } else {
            V6_CRITICAL_POWER_WATTS_DEFAULT
        };
        let sprint_cap = if constants.sprint_power_cap_watts > 1.0 {
            constants.sprint_power_cap_watts
        } else {
            V6_SPRINT_POWER_CAP_WATTS_DEFAULT
        };
        let v6_cp_state = V6CriticalPowerState::new(cp0, constants.w_prime_max_joules, sprint_cap);

        let mut out = Self {
            constants,
            environment_factor,
            stamina: 1.0,
            stamina_history: Vec::new(),
            speed_history: Vec::new(),
            time_history: Vec::new(),
            recovery_rate_history: Vec::new(),
            drain_rate_history: Vec::new(),
            current_time: 0.0,
            last_speed: 0.0,
            epoc_delay_start_time: -1.0,
            is_in_epoc_delay: false,
            speed_before_stop: 0.0,
            rest_duration_minutes: 0.0,
            exercise_duration_minutes: 0.0,
            last_movement_time: 0.0,
            max_history_length: 20_000,
            scenario_wind_drag: 0.0,
            sprint_start_time: -1.0,
            sprint_cooldown_until: -1.0,
            measured_velocity_ms: 0.0,
            applied_speed_limit_mult: 1.0,
            applied_speed_limit_ms: -1.0,
            fatigue: TwinFatigueSystem::default(),
            v6_cp_state,
            base_drain_rate_by_velocity: 0.0,
            final_drain_rate: 0.0,
            recovery_rate: 0.0,
            net_change: 0.0,
        };
        out.reset();
        out
    }

    pub fn reset(&mut self) {
        self.stamina = 1.0;
        self.stamina_history.clear();
        self.speed_history.clear();
        self.time_history.clear();
        self.recovery_rate_history.clear();
        self.drain_rate_history.clear();
        self.current_time = 0.0;
        self.last_speed = 0.0;
        self.epoc_delay_start_time = -1.0;
        self.is_in_epoc_delay = false;
        self.speed_before_stop = 0.0;
        self.rest_duration_minutes = 0.0;
        self.exercise_duration_minutes = 0.0;
        self.last_movement_time = 0.0;
        self.max_history_length = 20_000;
        self.scenario_wind_drag = 0.0;
        self.sprint_start_time = -1.0;
        self.sprint_cooldown_until = -1.0;
        self.measured_velocity_ms = 0.0;
        self.applied_speed_limit_mult = 1.0;
        self.applied_speed_limit_ms = -1.0;
        self.environment_factor.heat_stress = 0.0;
        self.environment_factor.cold_stress = 0.0;
        self.environment_factor.cold_static_penalty = 0.0;
        self.fatigue = TwinFatigueSystem::default();
        self.fatigue.initialize(0.0);
        self.v6_cp_state = V6CriticalPowerState::new(
            self._critical_power_watts(),
            self.constants.w_prime_max_joules,
            self.constants.sprint_power_cap_watts,
        );
        self.base_drain_rate_by_velocity = 0.0;
        self.final_drain_rate = 0.0;
        self.recovery_rate = 0.0;
        self.net_change = 0.0;
    }

    fn _critical_power_watts(&self) -> f64 {
        if self.constants.critical_power_watts > 1.0 {
            self.constants.critical_power_watts
        } else {
            V6_CRITICAL_POWER_WATTS_DEFAULT
        }
    }

    fn _apply_stamina_cap_clamp(&self, stamina_before: f64, new_stamina: f64) -> f64 {
        let max_cap = self.fatigue.get_max_stamina_cap();
        let clamped = clip_f64(new_stamina, 0.0, max_cap);
        if stamina_before > max_cap {
            max_cap
        } else {
            clamped
        }
    }

    fn _static_standing_cost(&self, body_weight: f64, load_weight: f64) -> f64 {
        let body_weight = body_weight.max(0.0);
        let load_weight = load_weight.max(0.0);
        if load_weight < 5.0 {
            return -0.0025;
        }
        if load_weight < 15.0 {
            return -0.001;
        }
        let base = self.constants.pandolf_static_coeff_1 * body_weight;
        let load_ratio = if body_weight > 0.0 {
            load_weight / body_weight
        } else {
            0.0
        };
        let load_term = if load_weight > 0.0 {
            self.constants.pandolf_static_coeff_2
                * (body_weight + load_weight)
                * (load_ratio * load_ratio)
        } else {
            0.0
        };
        (base + load_term).max(0.0) * self.constants.energy_to_stamina_coeff
    }

    fn _pandolf_expenditure(
        &self,
        velocity: f64,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
    ) -> f64 {
        let velocity = velocity.max(0.0);
        let current_weight = current_weight.max(0.0);
        if velocity < 0.1 {
            let bw = self.constants.character_weight;
            let load_weight = (current_weight - bw).max(0.0);
            return self._static_standing_cost(bw, load_weight).max(0.0);
        }

        let vt = velocity - self.constants.pandolf_velocity_offset;
        let base_term = (self.constants.pandolf_base_coeff * self.constants.fixed_pandolf_fitness_bonus)
            + (self.constants.pandolf_velocity_coeff * vt * vt);
        let g_dec = grade_percent * 0.01;
        let vsq = velocity * velocity;
        let mut grade_term =
            g_dec * (self.constants.pandolf_grade_base_coeff + self.constants.pandolf_grade_velocity_coeff * vsq);
        grade_term = grade_term.min(base_term * 3.0);

        if grade_percent < 0.0 && grade_percent > -self.constants.gentle_downhill_grade_max {
            grade_term *= self.constants.gentle_downhill_savings_multiplier;
        }
        let mut steep_downhill_penalty = 0.0;
        if grade_percent < -self.constants.steep_downhill_grade_threshold {
            let abs_grade = grade_percent.abs();
            let mut ramp = (abs_grade - self.constants.steep_downhill_grade_threshold) / 15.0;
            if ramp > 1.0 {
                ramp = 1.0;
            }
            steep_downhill_penalty = base_term * ramp * self.constants.steep_downhill_penalty_max_fraction;
        }

        let terrain_factor = clip_f64(terrain_factor, 0.5, 3.0);
        let w_mult = (current_weight / self.constants.reference_weight).max(0.1);
        let energy = w_mult
            * (base_term + grade_term + steep_downhill_penalty)
            * terrain_factor
            * self.constants.reference_weight;
        (energy * self.constants.energy_to_stamina_coeff).max(0.0)
    }

    fn _encumbrance_stamina_drain_multiplier(&self, current_weight: f64) -> f64 {
        let eff = (current_weight - self.constants.character_weight - self.constants.base_weight).max(0.0);
        let body_mass_percent = if self.constants.character_weight > 0.0 {
            eff / self.constants.character_weight
        } else {
            0.0
        };
        clip_f64(
            1.0 + self.constants.encumbrance_stamina_drain_coeff * body_mass_percent,
            1.0,
            3.0,
        )
    }

    fn _encumbrance_speed_penalty_base(&self, current_weight: f64) -> f64 {
        let effective_load = (current_weight - self.constants.character_weight - self.constants.base_weight).max(0.0);
        let mut ratio = 0.0;
        if self.constants.character_weight > 0.0 {
            ratio = effective_load / self.constants.character_weight;
        }
        ratio = clip_f64(ratio, 0.0, 2.0);

        let mut raw = if ratio <= 0.3 {
            0.15 * ratio
        } else if ratio <= 0.6 {
            let seg = ratio - 0.3;
            0.045 + 0.35 * seg.powf(1.5)
        } else {
            let seg = ratio - 0.6;
            0.25 + 0.65 * (seg * seg)
        };
        raw *= self.constants.encumbrance_speed_penalty_coeff / 0.20;
        clip_f64(raw, 0.0, self.constants.encumbrance_speed_penalty_max)
    }

    fn _posture_consumption_multiplier(&self, speed: f64, stance: i32) -> f64 {
        if speed <= 0.05 {
            return 1.0;
        }
        if stance == crate::constants::STANCE_CROUCH {
            return self.constants.posture_crouch_multiplier;
        }
        if stance == crate::constants::STANCE_PRONE {
            return self.constants.posture_prone_multiplier;
        }
        1.0
    }

    fn _fitness_efficiency_factor(&self) -> f64 {
        self.constants.fixed_fitness_efficiency_factor
    }

    fn _metabolic_efficiency_factor(&self, speed_ratio: f64) -> f64 {
        if speed_ratio < self.constants.aerobic_threshold {
            return self.constants.aerobic_efficiency_factor;
        }
        if speed_ratio < self.constants.anaerobic_threshold {
            let t = (speed_ratio - self.constants.aerobic_threshold)
                / (self.constants.anaerobic_threshold - self.constants.aerobic_threshold);
            return self.constants.aerobic_efficiency_factor
                + t * (self.constants.anaerobic_efficiency_factor - self.constants.aerobic_efficiency_factor);
        }
        self.constants.anaerobic_efficiency_factor
    }

    fn _fatigue_factor(&self) -> f64 {
        let eff = (self.exercise_duration_minutes - self.constants.fatigue_start_time_minutes).max(0.0);
        clip_f64(
            1.0 + self.constants.fatigue_accumulation_coeff * eff,
            1.0,
            self.constants.fatigue_max_factor,
        )
    }

    fn _movement_phase_from_type(&self, movement_type: i32) -> i32 {
        if movement_type == MOVEMENT_SPRINT {
            3
        } else if movement_type == MOVEMENT_WALK {
            1
        } else {
            2
        }
    }

    fn _loaded_gait_stamina_drain_multiplier(load_weight_kg: f64, movement_phase: i32) -> f64 {
        if movement_phase < 2 {
            return 1.0;
        }
        if load_weight_kg <= LOADED_RUN_DRAIN_START_KG {
            return 1.0;
        }
        let mut span = LOADED_RUN_DRAIN_REF_KG - LOADED_RUN_DRAIN_START_KG;
        if span < 0.1 {
            span = 0.1;
        }
        let mut t = (load_weight_kg - LOADED_RUN_DRAIN_START_KG) / span;
        if t < 0.0 {
            t = 0.0;
        } else if t > 1.0 {
            t = 1.0;
        }
        1.0 + (LOADED_RUN_DRAIN_MAX_MULT - 1.0) * t
    }

    fn _v6_land_drain_per_second(
        &self,
        speed: f64,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
        movement_type: i32,
        wind_drag: f64,
        effective_cp_watts: f64,
    ) -> f64 {
        let phase = self._movement_phase_from_type(movement_type);
        let power_w = metabolism_power_watts_damped(
            speed,
            current_weight,
            grade_percent,
            terrain_factor,
            phase,
            self.constants.load_metabolic_dampening,
        );
        let mut aerobic_w = power_w;
        if effective_cp_watts > 1.0 && power_w > effective_cp_watts {
            aerobic_w = effective_cp_watts;
        }
        let mut per_sec =
            (aerobic_w * self.constants.energy_to_stamina_coeff * V6_STAMINA_DRAIN_CALIBRATION)
                .max(0.0);
        let load_kg = (current_weight - self.constants.character_weight).max(0.0);
        per_sec *= Self::_loaded_gait_stamina_drain_multiplier(load_kg, phase);
        per_sec * (1.0 + wind_drag)
    }

    fn _calculate_drain_rate_c_aligned(
        &self,
        speed: f64,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
        stance: i32,
        movement_type: i32,
        wind_drag: f64,
    ) -> (f64, f64) {
        let bw = self.constants.character_weight;
        let idle_threshold = 0.1;
        let mut cp0 = self.constants.critical_power_watts;
        if cp0 <= 1.0 {
            cp0 = V6_CRITICAL_POWER_WATTS_DEFAULT;
        }
        let load_kg = (current_weight - bw).max(0.0);
        let effective_cp = compute_cp_watts(cp0, load_kg, grade_percent, 1.0, 0.0);
        let wind_mult = 1.0 + wind_drag;
        let load_dampening = self.constants.load_metabolic_dampening;

        let raw = if movement_type == MOVEMENT_IDLE {
            if speed < idle_threshold {
                -self.constants.rest_recovery_per_tick
            } else {
                let mut pandolf_per_s =
                    self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor);
                if current_weight > bw && load_dampening < 1.0 {
                    let unloaded_per_s = self._pandolf_expenditure(speed, bw, grade_percent, terrain_factor);
                    let load_extra = pandolf_per_s - unloaded_per_s;
                    pandolf_per_s = unloaded_per_s + load_extra * load_dampening;
                }
                pandolf_per_s * wind_mult * STAMINA_TICK_SEC
            }
        } else if speed < idle_threshold {
            let load_weight = (current_weight - bw).max(0.0);
            self._static_standing_cost(bw, load_weight) * STAMINA_TICK_SEC
        } else {
            let pandolf_per_s = self._v6_land_drain_per_second(
                speed,
                current_weight,
                grade_percent,
                terrain_factor,
                movement_type,
                wind_drag,
                effective_cp,
            );
            pandolf_per_s * STAMINA_TICK_SEC
        };

        if raw <= 0.0 {
            return (raw, 0.0);
        }

        let adjusted_raw = self.environment_factor.adjust_energy_for_temperature(raw);
        let base_for_recovery = adjusted_raw.max(0.0);
        let posture = self._posture_consumption_multiplier(speed, stance);
        let mut total_drain = adjusted_raw * posture;
        if total_drain > 0.0 {
            total_drain *= self
                .environment_factor
                .get_quick_environment_multiplier(0.0, wind_drag);
        }
        (base_for_recovery, total_drain.max(0.0))
    }

    fn _calculate_recovery_rate(
        &self,
        stamina_percent: f64,
        rest_duration_minutes: f64,
        exercise_duration_minutes: f64,
        current_weight: f64,
        base_drain_rate: f64,
        disable_positive_recovery: bool,
        stance: i32,
        current_speed: f64,
    ) -> f64 {
        let c = &self.constants;
        let stamina_percent_clamped = clip_f64(stamina_percent, 0.0, 1.0);
        let rest_sec = rest_duration_minutes * 60.0;
        if stamina_percent_clamped < c.min_recovery_stamina_threshold
            && rest_sec < c.min_recovery_rest_time_seconds
        {
            return 0.0;
        }

        let stamina_mult = 1.0 + c.recovery_nonlinear_coeff * (1.0 - stamina_percent_clamped);
        let mut recovery_rate = c.base_recovery_rate * stamina_mult;
        recovery_rate *= c.fixed_fitness_recovery_multiplier * c.fixed_age_recovery_multiplier;

        if rest_duration_minutes > 0.0 {
            if rest_duration_minutes <= c.fast_recovery_duration_minutes {
                recovery_rate *= c.fast_recovery_multiplier;
            } else if rest_duration_minutes
                <= c.fast_recovery_duration_minutes + c.medium_recovery_duration_minutes
            {
                recovery_rate *= c.medium_recovery_multiplier;
            } else if rest_duration_minutes >= c.slow_recovery_start_minutes {
                let trans_progress = ((rest_duration_minutes - c.slow_recovery_start_minutes) / 10.0).min(1.0);
                let slow_mult = 1.0 - trans_progress * (1.0 - c.slow_recovery_multiplier);
                recovery_rate *= slow_mult;
            }
        }

        let fatigue_penalty = c.fatigue_recovery_penalty
            * (exercise_duration_minutes / c.fatigue_recovery_duration_minutes).min(1.0);
        let fatigue_mult = clip_f64(1.0 - fatigue_penalty, 0.7, 1.0);
        recovery_rate *= fatigue_mult;

        if stance == crate::constants::STANCE_STAND {
            recovery_rate *= c.standing_recovery_multiplier;
        } else if stance == crate::constants::STANCE_CROUCH {
            recovery_rate *= c.crouching_recovery_multiplier;
        } else if stance == crate::constants::STANCE_PRONE {
            recovery_rate *= c.prone_recovery_multiplier;
        }

        if stamina_percent_clamped > c.marginal_decay_threshold {
            let marginal_mult = clip_f64(c.marginal_decay_coeff - stamina_percent_clamped, 0.2, 1.0);
            recovery_rate *= marginal_mult;
        }

        let weight_for_rec = if stance == crate::constants::STANCE_PRONE {
            c.character_weight
        } else {
            current_weight
        };
        if weight_for_rec > 0.0 {
            let load_ratio = clip_f64(weight_for_rec / c.body_tolerance_base, 0.0, 2.0);
            let penalty = load_ratio.powf(c.load_recovery_penalty_exponent) * c.load_recovery_penalty_coeff;
            if recovery_rate > 0.0 {
                let load_factor = (1.0 - penalty / recovery_rate).max(0.2);
                recovery_rate *= load_factor;
            } else {
                recovery_rate = 0.00005;
            }
        }

        let heat_penalty = self.environment_factor.get_heat_stress_penalty();
        if heat_penalty > 0.0 {
            recovery_rate *= 1.0 - heat_penalty;
        }
        recovery_rate *= (1.0 - self.environment_factor.cold_stress).max(0.0);
        if stance == crate::constants::STANCE_PRONE {
            if self.environment_factor.surface_wetness > 0.0 {
                recovery_rate *= 1.0 - self.environment_factor.surface_wetness;
            }
        }
        recovery_rate = recovery_rate.max(0.0);

        if current_speed >= 0.1 {
            recovery_rate = 0.0;
        }
        if stamina_percent < 0.02 {
            recovery_rate = recovery_rate.max(0.0001);
        }
        if disable_positive_recovery {
            return -base_drain_rate.max(0.0);
        }

        if base_drain_rate > 0.0 && current_speed < 0.1 {
            let adjusted = recovery_rate - base_drain_rate;
            if adjusted >= 0.00005 {
                recovery_rate = adjusted.max(0.00005);
            } else {
                recovery_rate = 0.00005;
            }
        } else if base_drain_rate < 0.0 {
            recovery_rate += base_drain_rate.abs();
        }
        if current_speed < 0.1 && current_weight < 40.0 && recovery_rate < 0.00005 {
            recovery_rate = 0.0001;
        }
        if c.max_recovery_per_tick > 0.0 {
            recovery_rate = recovery_rate.min(c.max_recovery_per_tick);
        }
        recovery_rate.max(0.0)
    }

    pub fn step(
        &mut self,
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
    ) {
        let mut speed = speed.max(0.0);
        let current_weight = current_weight.max(0.0);
        let grade_percent = clip_f64(grade_percent, -85.0, 85.0);
        let terrain_factor = clip_f64(terrain_factor, 0.5, 3.0);
        let current_time = current_time.max(0.0);

        let mut time_delta = if let Some(t) = time_delta_override {
            t.max(0.01)
        } else {
            let mut t = current_time - self.current_time;
            if t <= 0.0 {
                t = STAMINA_TICK_SEC;
            }
            t.max(0.01)
        };
        if time_delta <= 0.0 {
            time_delta = 0.01;
        }
        self.current_time = current_time;

        if enable_randomness {
            let phase = (current_time * 9876.54321).sin() * 0.5 + 0.5;
            let n = 0.98 + phase * 0.04;
            speed = (speed * n).max(0.0);
        }

        let was_moving = self.last_speed > 0.05;
        let is_now_stopped = speed <= 0.05;
        if was_moving && is_now_stopped && !self.is_in_epoc_delay {
            self.epoc_delay_start_time = current_time;
            self.is_in_epoc_delay = true;
            self.speed_before_stop = self.last_speed.max(0.0);
        }

        if self.is_in_epoc_delay {
            if current_time - self.epoc_delay_start_time >= self.constants.epoc_delay_seconds {
                self.is_in_epoc_delay = false;
                self.epoc_delay_start_time = -1.0;
            }
        }
        if speed > 0.05 {
            self.is_in_epoc_delay = false;
            self.epoc_delay_start_time = -1.0;
        }

        let is_sprinting = movement_type == MOVEMENT_SPRINT;
        if is_sprinting && self.sprint_start_time < 0.0 {
            if self.sprint_cooldown_until < 0.0 || current_time >= self.sprint_cooldown_until {
                self.sprint_start_time = current_time;
            }
        } else if !is_sprinting && self.sprint_start_time >= 0.0 {
            let elapsed = current_time - self.sprint_start_time;
            if elapsed >= self.constants.tactical_sprint_burst_duration + self.constants.tactical_sprint_burst_buffer_duration {
                self.sprint_cooldown_until = current_time + self.constants.tactical_sprint_cooldown;
            }
            self.sprint_start_time = -1.0;
        }

        self.last_speed = speed;
        if self.speed_history.len() < self.max_history_length {
            self.speed_history.push(speed);
        }

        if speed >= 0.1 {
            self.exercise_duration_minutes += time_delta / 60.0;
            self.rest_duration_minutes = 0.0;
        } else {
            self.rest_duration_minutes += time_delta / 60.0;
        }

        let (base_for_rec, mut total_drain) = self._calculate_drain_rate_c_aligned(
            speed,
            current_weight,
            grade_percent,
            terrain_factor,
            stance,
            movement_type,
            wind_drag,
        );
        if total_drain >= 0.0 {
            total_drain = total_drain.min(0.1);
        }
        if self.is_in_epoc_delay {
            let speed_ratio = clip_f64(self.speed_before_stop / 5.5, 0.0, 1.0);
            let mut excess_ratio = speed_ratio * 0.5;
            if excess_ratio > EPOC_MAX_POWER_EXCESS_RATIO {
                excess_ratio = EPOC_MAX_POWER_EXCESS_RATIO;
            }
            let epoc_rate = self.constants.epoc_drain_rate * (1.0 + excess_ratio);
            total_drain += epoc_rate;
        }

        let mut recovery_rate = self._calculate_recovery_rate(
            self.stamina,
            self.rest_duration_minutes,
            self.exercise_duration_minutes,
            current_weight,
            base_for_rec,
            false,
            stance,
            speed,
        );
        recovery_rate = clip_f64(recovery_rate, 0.0, 0.01);

        let load_kg = (current_weight - self.constants.character_weight).max(0.0);
        let phase = if movement_type == MOVEMENT_IDLE {
            MOVEMENT_WALK
        } else {
            movement_type
        };
        self.v6_cp_state.set_runtime_context(
            load_kg,
            grade_percent,
            1.0,
            self.fatigue.get_fatigue_integral_norm(),
        );
        self.v6_cp_state
            .set_fatigue_cp_multiplier(self.fatigue.get_cp_fatigue_multiplier());
        let metabolic_power = metabolism_power_watts_damped(
            speed,
            current_weight,
            grade_percent,
            terrain_factor,
            phase,
            self.constants.load_metabolic_dampening,
        );
        self.v6_cp_state
            .tick(metabolic_power, is_sprinting, current_time, time_delta, speed);

        let net_change = clip_f64(recovery_rate - total_drain, -0.1, 0.01);
        let tick_scale = time_delta / STAMINA_TICK_SEC;
        let stamina_delta = net_change * tick_scale;
        let stamina_before = self.stamina;
        self.stamina = self._apply_stamina_cap_clamp(stamina_before, stamina_before + stamina_delta);

        self.base_drain_rate_by_velocity = base_for_rec;
        self.final_drain_rate = total_drain;
        self.recovery_rate = recovery_rate;
        self.net_change = stamina_delta;

        if self.stamina_history.len() < self.max_history_length {
            self.stamina_history.push(self.stamina);
            self.time_history.push(current_time);
            self.recovery_rate_history.push(recovery_rate);
            self.drain_rate_history.push(total_drain);
        }
    }

    fn get_dynamic_limp_multiplier(&self, encumbrance_penalty: f64) -> f64 {
        let mut max_walk_speed = WALK_VELOCITY_THRESHOLD * (1.0 - encumbrance_penalty);
        max_walk_speed = clip_f64(max_walk_speed, EXHAUSTION_LIMP_SPEED, RUN_VELOCITY_THRESHOLD);
        max_walk_speed / self.constants.game_max_speed
    }

    fn calculate_v6_phase_speed_multiplier(
        &self,
        stamina_percent: f64,
        movement_phase: i32,
        encumbrance_speed_penalty: f64,
    ) -> f64 {
        let stamina_percent = clip_f64(stamina_percent, 0.0, 1.0);
        let mut enc_mult = 1.0 - encumbrance_speed_penalty;
        if enc_mult < 0.5 {
            enc_mult = 0.5;
        }
        let mut target_ms = self.constants.v5_run_speed_ms;
        if movement_phase == MOVEMENT_SPRINT {
            target_ms = self.constants.v5_sprint_speed_ms;
        } else if movement_phase == MOVEMENT_WALK {
            target_ms = self.constants.v5_walk_speed_ms;
        }
        target_ms *= enc_mult;
        let run_mult = target_ms / self.constants.game_max_speed;
        let limp_threshold = self.constants.smooth_transition_end;
        if stamina_percent >= limp_threshold {
            return clip_f64(run_mult, MIN_SPEED_MULTIPLIER, 1.0);
        }
        let t = clip_f64(stamina_percent / limp_threshold, 0.0, 1.0);
        let limp_mult = self.get_dynamic_limp_multiplier(encumbrance_speed_penalty);
        (limp_mult * t).max(MIN_SPEED_MULTIPLIER)
    }

    pub fn calculate_speed_multiplier_by_stamina(&self, stamina_percent: f64) -> f64 {
        self.calculate_v6_phase_speed_multiplier(stamina_percent, MOVEMENT_RUN, 0.0)
    }

    fn stamina_scale_from_run_multiplier(&self, scaled_run_multiplier: f64) -> f64 {
        let run_ref_mult = self.constants.v5_run_speed_ms / self.constants.game_max_speed;
        let run_ref_mult = if run_ref_mult < 0.01 { 0.01 } else { run_ref_mult };
        clip_f64(scaled_run_multiplier / run_ref_mult, 0.15, 1.0)
    }

    fn get_v5_absolute_speed_ms(
        &self,
        movement_phase: i32,
        is_sprinting: bool,
        scaled_run_speed: f64,
        encumbrance_penalty: f64,
        anaerobic_percent: f64,
    ) -> f64 {
        let stamina_scale = self.stamina_scale_from_run_multiplier(scaled_run_speed);
        let mut enc_mult = 1.0 - encumbrance_penalty;
        if enc_mult < 0.5 {
            enc_mult = 0.5;
        }
        let walk_ms = self.constants.v5_walk_speed_ms * enc_mult * stamina_scale;
        let run_ms = self.constants.v5_run_speed_ms * enc_mult * stamina_scale;
        let mut sprint_ms = self.ensured_march_sprint_speed_ms() * enc_mult * stamina_scale;
        if anaerobic_percent < 1.0 {
            let ana_scale = 0.65 + 0.35 * anaerobic_percent;
            sprint_ms *= ana_scale;
            let min_sprint = run_ms * SPRINT_GAIT_MIN_OVER_RUN_RATIO;
            if sprint_ms < min_sprint {
                sprint_ms = min_sprint;
            }
        }
        if is_sprinting || movement_phase == MOVEMENT_SPRINT {
            return clip_f64(sprint_ms, walk_ms, self.ensured_march_sprint_speed_ms());
        }
        if movement_phase == MOVEMENT_RUN {
            return clip_f64(run_ms, walk_ms, self.constants.v5_run_speed_ms);
        }
        if movement_phase == MOVEMENT_WALK {
            return clip_f64(walk_ms, 0.5, self.constants.v5_walk_speed_ms);
        }
        walk_ms
    }

    fn ensured_march_sprint_speed_ms(&self) -> f64 {
        let run_ms = self.constants.v5_run_speed_ms;
        let mut sprint_ms = self.constants.v5_sprint_speed_ms;
        let min_sprint = run_ms * SPRINT_GAIT_MIN_OVER_RUN_RATIO;
        if sprint_ms < min_sprint {
            sprint_ms = min_sprint;
        }
        if sprint_ms > self.constants.game_max_speed {
            sprint_ms = self.constants.game_max_speed;
        }
        sprint_ms
    }

    fn apply_sprint_gait_min_separation(&self, mut sprint_ms: f64, run_ms: f64) -> f64 {
        let mut min_sprint = run_ms * SPRINT_GAIT_MIN_OVER_RUN_RATIO;
        let gait_sprint = self.ensured_march_sprint_speed_ms();
        if min_sprint > gait_sprint {
            min_sprint = gait_sprint;
        }
        if sprint_ms < min_sprint {
            sprint_ms = min_sprint;
        }
        if sprint_ms > gait_sprint {
            sprint_ms = gait_sprint;
        }
        sprint_ms
    }

    fn get_v6_sprint_speed_ms(
        &self,
        encumbrance_penalty: f64,
        total_weight_kg: f64,
        grade_percent: f64,
        terrain_factor: f64,
        time_delta_sec: f64,
        world_time_sec: f64,
    ) -> f64 {
        let mut enc_mult = 1.0 - encumbrance_penalty;
        if enc_mult < 0.5 {
            enc_mult = 0.5;
        }
        let run_ms = self.constants.v5_run_speed_ms * enc_mult;
        let gait_sprint_ms = self.ensured_march_sprint_speed_ms() * enc_mult;
        if self.v6_cp_state.pool01() <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT {
            return run_ms;
        }
        let available_p = self
            .v6_cp_state
            .get_available_power_watts(true, time_delta_sec, world_time_sec);
        let mut power_ms = invert_speed_for_power_watts(
            available_p,
            total_weight_kg,
            grade_percent,
            terrain_factor,
            MOVEMENT_SPRINT,
        );
        power_ms *= enc_mult;
        let mut sprint_ms = gait_sprint_ms;
        if power_ms < sprint_ms {
            sprint_ms = power_ms;
        }
        self.apply_sprint_gait_min_separation(sprint_ms, run_ms)
    }

    fn calculate_actual_speed(
        &mut self,
        stamina_percent: f64,
        current_weight: f64,
        movement_phase: i32,
        last_speed: f64,
        grade_percent: f64,
        current_time: f64,
        terrain_factor: f64,
    ) -> f64 {
        let game_max = self.constants.game_max_speed;
        let max_pen = self.constants.encumbrance_speed_penalty_max;
        let exhausted = stamina_percent <= self.constants.exhaustion_threshold;
        let can_sprint = stamina_percent >= self.constants.sprint_enable_threshold;

        let mut phase = movement_phase;
        let mut is_sprinting = phase == MOVEMENT_SPRINT;
        if (exhausted || !can_sprint) && (is_sprinting || phase == MOVEMENT_SPRINT) {
            phase = MOVEMENT_RUN;
            is_sprinting = false;
        }

        if phase != MOVEMENT_WALK && phase != MOVEMENT_RUN && phase != MOVEMENT_SPRINT && !is_sprinting {
            return 0.0;
        }

        let load_kg = (current_weight - self.constants.character_weight).max(0.0);
        self.v6_cp_state.set_runtime_context(
            load_kg,
            grade_percent,
            1.0,
            self.fatigue.get_fatigue_integral_norm(),
        );
        self.v6_cp_state
            .set_fatigue_cp_multiplier(self.fatigue.get_cp_fatigue_multiplier());

        let base_penalty = self._encumbrance_speed_penalty_base(current_weight);
        let run_base_mult =
            self.calculate_v6_phase_speed_multiplier(stamina_percent, MOVEMENT_RUN, 0.0);

        let angle_deg = if grade_percent.abs() > 0.01 {
            (grade_percent / 100.0).atan().to_degrees()
        } else {
            0.0
        };
        let slope_run_base_ms = self.constants.v5_run_speed_ms;
        let mut slope_run_ref_mult = slope_run_base_ms / game_max;
        if slope_run_ref_mult < 0.01 {
            slope_run_ref_mult = 0.01;
        }
        let slope_adjusted_ms = calculate_slope_adjusted_target_speed(
            slope_run_base_ms,
            angle_deg,
            self.constants.uphill_speed_boost,
            self.constants.downhill_speed_boost,
            self.constants.downhill_speed_max_multiplier,
        );
        let slope_adjusted_mult = slope_adjusted_ms / game_max;
        let speed_scale_factor = slope_adjusted_mult / slope_run_ref_mult;
        let scaled_run = run_base_mult * speed_scale_factor;

        let speed_ratio = clip_f64(last_speed / game_max, 0.0, 1.0);
        let mut enc_penalty = base_penalty * (1.0 + speed_ratio);
        if is_sprinting || phase == MOVEMENT_SPRINT {
            enc_penalty *= SPRINT_ENCUMBRANCE_PENALTY_MULT;
        }
        enc_penalty = clip_f64(enc_penalty, 0.0, max_pen);

        let mut final_abs = self.get_v5_absolute_speed_ms(phase, is_sprinting, scaled_run, enc_penalty, 1.0);
        if last_speed < 0.5 {
            let mut start_min = self.constants.v5_walk_speed_ms * (1.0 - enc_penalty);
            if start_min < 0.8 {
                start_min = 0.8;
            }
            if final_abs < start_min {
                final_abs = start_min;
            }
        }

        let mut theoretical_target = final_abs;

        let mut tf = terrain_factor;
        if tf < 0.5 {
            tf = 0.5;
        }
        if tf > 3.0 {
            tf = 3.0;
        }

        if is_sprinting || phase == MOVEMENT_SPRINT {
            let dt = if current_time >= 0.0 {
                RSS_PLAYER_TICK_SEC
            } else {
                0.017
            };
            theoretical_target = self.get_v6_sprint_speed_ms(
                enc_penalty,
                current_weight,
                grade_percent,
                tf,
                dt,
                current_time,
            );
        } else if phase != MOVEMENT_WALK && !self.v6_cp_state.refresh_and_get_overspeed_armed() {
            // Run only: CP ∩ aerobic cruise max; Walk exempt
            let mut run_phase = phase;
            if run_phase < MOVEMENT_RUN {
                run_phase = MOVEMENT_RUN;
            }
            let mut cruise_cap = V6_AEROBIC_CRUISE_MAX_MS;
            let cp_cap_ms = invert_speed_for_power_watts(
                self.v6_cp_state.get_effective_critical_power_watts(),
                current_weight,
                grade_percent,
                tf,
                run_phase,
            );
            if cp_cap_ms > 0.05 && cp_cap_ms < cruise_cap {
                cruise_cap = cp_cap_ms;
            }
            if theoretical_target > cruise_cap {
                theoretical_target = cruise_cap;
            }
        }

        theoretical_target
    }

    pub fn theoretical_speed_at_weight(
        &mut self,
        current_weight: f64,
        movement_phase: i32,
        grade_percent: f64,
        terrain_factor: f64,
        stamina_percent: f64,
    ) -> f64 {
        let mut last = if movement_phase == MOVEMENT_WALK {
            1.0
        } else if movement_phase == MOVEMENT_SPRINT {
            3.5
        } else {
            2.5
        };
        let mut out = last;
        for _ in 0..24 {
            out = self.calculate_actual_speed(
                stamina_percent,
                current_weight,
                movement_phase,
                last,
                grade_percent,
                10.0,
                terrain_factor,
            );
            if (out - last).abs() < 1e-4 {
                break;
            }
            last = out;
        }
        out
    }

    fn estimate_engine_original_max_speed(movement_phase: i32, constants: &RssConstants) -> f64 {
        if movement_phase == MOVEMENT_SPRINT {
            constants.game_max_speed
        } else {
            constants.game_max_speed * constants.target_run_speed_multiplier
        }
    }

    fn _resolve_current_speed_ms(&self, theoretical_ms: f64) -> f64 {
        if self.measured_velocity_ms.max(0.0) < 0.05 {
            theoretical_ms.max(0.0)
        } else {
            self.measured_velocity_ms.max(0.0)
        }
    }

    fn _apply_metabolic_speed_limit(
        &mut self,
        speed_limit_mult: f64,
        current_speed_ms: f64,
        movement_phase: i32,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
        engine_base_ms: f64,
    ) -> f64 {
        let load_kg = (current_weight - self.constants.character_weight).max(0.0);
        self.v6_cp_state.set_runtime_context(
            load_kg,
            grade_percent,
            1.0,
            self.fatigue.get_fatigue_integral_norm(),
        );
        self.v6_cp_state
            .set_fatigue_cp_multiplier(self.fatigue.get_cp_fatigue_multiplier());
        let effective_cp = self.v6_cp_state.get_effective_critical_power_watts();

        let exhausted = self.stamina <= self.constants.exhaustion_threshold;
        let metab_phase = if movement_phase < MOVEMENT_WALK {
            MOVEMENT_RUN
        } else {
            movement_phase
        };
        get_metabolic_corrected_speed_multiplier(
            speed_limit_mult,
            current_speed_ms,
            metab_phase,
            current_weight,
            grade_percent,
            terrain_factor,
            exhausted,
            engine_base_ms,
            effective_cp,
            self.v6_cp_state.pool01(),
        )
    }

    fn compute_speed_limit_multiplier(&self, theoretical_target_ms: f64, engine_original_ms: f64) -> f64 {
        let mult = if engine_original_ms > 0.1 {
            theoretical_target_ms / engine_original_ms
        } else {
            theoretical_target_ms / self.constants.game_max_speed
        };
        clip_f64(mult, 0.01, 3.0)
    }

    fn resolve_movement_state(&self, intent_phase: i32, stamina_percent: f64) -> (bool, i32, bool, bool) {
        let exhausted = stamina_percent <= self.constants.exhaustion_threshold;
        let can_sprint = stamina_percent >= self.constants.sprint_enable_threshold;
        let mut is_sprinting = intent_phase == MOVEMENT_SPRINT;
        let mut effective_phase = intent_phase;
        if exhausted || !can_sprint {
            if is_sprinting || effective_phase == MOVEMENT_SPRINT {
                effective_phase = MOVEMENT_RUN;
                is_sprinting = false;
            }
        }
        (is_sprinting, effective_phase, can_sprint, exhausted)
    }

    fn _compute_engine_movement_phase(
        &self,
        intent_phase: i32,
        actual_speed: f64,
        last_speed: f64,
        current_weight: f64,
        enable_randomness: bool,
    ) -> i32 {
        let phase = intent_phase;
        if phase == MOVEMENT_IDLE {
            return MOVEMENT_IDLE;
        }
        if enable_randomness {
            let effective_load = (current_weight - self.constants.character_weight - self.constants.base_weight).max(0.0);
            let mut ratio = 0.0;
            if self.constants.character_weight > 0.0 {
                ratio = effective_load / self.constants.character_weight;
            }
            if actual_speed < 0.25 && last_speed < 0.25 && ratio > 0.30 {
                let p = clip_f64((ratio - 0.30) / 0.70, 0.0, 0.20);
                let r = ((self.current_time + current_weight + actual_speed + last_speed) * 12.34567)
                    .sin()
                    .abs()
                    .fract();
                if r < p {
                    return MOVEMENT_IDLE;
                }
            }
        }
        phase
    }

    pub fn game_player_tick(
        &mut self,
        intent_phase: i32,
        current_weight: f64,
        grade_percent: f64,
        terrain_factor: f64,
        stance: i32,
        current_time: f64,
        time_delta: f64,
        wind_drag: f64,
        enable_randomness: bool,
    ) -> f64 {
        let (_, effective_phase, _, _) = self.resolve_movement_state(intent_phase, self.stamina);

        if intent_phase == MOVEMENT_SPRINT && self.sprint_start_time < 0.0 {
            if self.sprint_cooldown_until < 0.0 || current_time >= self.sprint_cooldown_until {
                self.sprint_start_time = current_time;
            }
        }

        let theoretical_ms = self.calculate_actual_speed(
            self.stamina,
            current_weight,
            effective_phase,
            self.measured_velocity_ms,
            grade_percent,
            current_time,
            terrain_factor,
        );
        let engine_phase = if effective_phase == MOVEMENT_SPRINT {
            MOVEMENT_SPRINT
        } else if effective_phase == MOVEMENT_RUN {
            MOVEMENT_RUN
        } else {
            effective_phase
        };
        let engine_original_ms = Self::estimate_engine_original_max_speed(engine_phase, &self.constants);
        let mut speed_limit_mult = self.compute_speed_limit_multiplier(theoretical_ms, engine_original_ms);

        let current_speed_ms = self._resolve_current_speed_ms(theoretical_ms);
        speed_limit_mult = self._apply_metabolic_speed_limit(
            speed_limit_mult,
            current_speed_ms,
            effective_phase,
            current_weight,
            grade_percent,
            terrain_factor,
            engine_original_ms,
        );

        let applied_limit_ms = speed_limit_mult * engine_original_ms;
        if applied_limit_ms <= 0.05 {
            self.applied_speed_limit_ms = -1.0;
        } else {
            self.applied_speed_limit_ms = applied_limit_ms;
        }

        let mut limit_for_power_ms = self.applied_speed_limit_ms;
        if limit_for_power_ms <= 0.05 {
            limit_for_power_ms = applied_limit_ms;
        }

        let drain_speed = if intent_phase == MOVEMENT_IDLE || theoretical_ms < 0.1 {
            0.0
        } else {
            let mut measured_for_drain = self.measured_velocity_ms.max(0.0);
            if measured_for_drain < 0.05 {
                measured_for_drain = theoretical_ms.max(0.0);
            }
            let mut cap_ms = limit_for_power_ms;
            if cap_ms <= 0.05 {
                cap_ms = theoretical_ms;
            }
            if cap_ms <= 0.05 {
                cap_ms = engine_original_ms;
            }
            get_drain_velocity_ms(measured_for_drain, cap_ms)
        };

        let engine_movement_phase = self._compute_engine_movement_phase(
            effective_phase,
            drain_speed,
            self.measured_velocity_ms,
            current_weight,
            enable_randomness,
        );

        self.fatigue.process_decay(current_time, current_speed_ms);
        if current_speed_ms >= RSS_IDLE_SPEED_THRESHOLD_MPS {
            let load_kg = (current_weight - self.constants.character_weight).max(0.0);
            let mov_phase = self._movement_phase_from_type(engine_movement_phase);
            let fatigue_v = get_drain_velocity_ms(current_speed_ms, limit_for_power_ms);
            let power_fat = metabolism_power_watts_damped(
                fatigue_v,
                current_weight,
                grade_percent,
                terrain_factor,
                mov_phase,
                self.constants.load_metabolic_dampening,
            );
            self.fatigue.process_integral(
                power_fat,
                load_kg,
                grade_percent,
                terrain_factor,
                time_delta,
                current_speed_ms,
                self._critical_power_watts(),
            );
        }

        self.step(
            drain_speed,
            current_weight,
            grade_percent,
            terrain_factor,
            stance,
            engine_movement_phase,
            current_time,
            enable_randomness,
            wind_drag,
            Some(time_delta),
        );

        if drain_speed >= 0.1 {
            // 与游戏关水平硬钳一致：测速不强行钳到 applied limit
            let new_meas = (engine_original_ms * speed_limit_mult).min(VELOCITY_HORIZ_CAP_MS);
            self.measured_velocity_ms = new_meas;
        } else {
            self.measured_velocity_ms = 0.0;
        }
        self.applied_speed_limit_mult = speed_limit_mult;
        drain_speed
    }
}
