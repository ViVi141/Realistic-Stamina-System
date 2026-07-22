use serde::{Deserialize, Serialize};
use std::collections::HashMap;

pub const TOBLER_W_AT_FLAT_KMH: f64 = 5.039;
pub const STAMINA_TICK_SEC: f64 = 0.2;
pub const RSS_PLAYER_TICK_SEC: f64 = 0.017;
pub const VELOCITY_HORIZ_CAP_MS: f64 = 7.0;

pub const V6_OVERSPEED_ACCOUNTING_EPS_MPS: f64 = 0.12;
pub const V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT: f64 = 0.20;
pub const V6_ACSM_REST_W: f64 = 50.0;
pub const V6_ACSM_LINEAR_W_PER_MS: f64 = 200.0;
pub const V6_ACSM_QUAD_W_PER_MS2: f64 = 80.0;
pub const V6_ACSM_BLEND_START_MS: f64 = 2.0;
pub const V6_ACSM_BLEND_END_MS: f64 = 2.4;
pub const V6_INVERT_SPEED_MAX_MS: f64 = 6.0;
pub const V6_CRITICAL_POWER_WATTS_DEFAULT: f64 = 400.0;
pub const V6_W_PRIME_MAX_JOULES_DEFAULT: f64 = 20_000.0;
pub const V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT: f64 = 12.0;
pub const V6_SPRINT_POWER_CAP_WATTS_DEFAULT: f64 = 1200.0;
pub const V6_STAMINA_DRAIN_CALIBRATION: f64 = 0.72;
pub const V6_CP_LOAD_REF_KG: f64 = 10.0;
pub const V6_CP_LOAD_DECAY_PER_KG: f64 = 0.002;
pub const V6_CP_SLOPE_K_UP: f64 = 0.015;
pub const V6_CP_FATIGUE_K: f64 = 0.18;
pub const V6_SKIBA_ELITE_CP_THRESHOLD_W: f64 = 410.0;
pub const V6_W_PRIME_K_FAST: f64 = 0.15;
pub const V6_W_PRIME_K_SLOW: f64 = 0.008;
pub const V6_W_PRIME_LIM_RATIO: f64 = 0.5;
pub const V6_FATIGUE_I_MAX: f64 = 1.0;
pub const V6_FATIGUE_K_RECOVERY: f64 = 0.0008;
pub const V6_FATIGUE_K_LOAD: f64 = 0.15;
pub const V6_FATIGUE_K_SLOPE: f64 = 8.0;
pub const V6_FATIGUE_K_TERRAIN: f64 = 0.25;
pub const V6_MAX_FATIGUE_PENALTY: f64 = 0.3;
pub const RSS_IDLE_SPEED_THRESHOLD_MPS: f64 = 0.1;
pub const V6_CP_ENV_FLOOR: f64 = 0.55;
pub const V6_STANDING_REST_WATTS: f64 = 100.0;
pub const V5_TACTICAL_SHORT_BURST_SEC: f64 = 3.0;
pub const V5_BURST_EARLY_RELEASE_BONUS: f64 = 0.45;
pub const V5_BURST_COOLDOWN_FULL_DEFAULT: f64 = 180.0;
pub const V5_BURST_COOLDOWN_SHORT_DEFAULT: f64 = 75.0;

pub const WALK_VELOCITY_THRESHOLD: f64 = 3.2;
pub const RUN_VELOCITY_THRESHOLD: f64 = 3.8;
pub const EXHAUSTION_LIMP_SPEED: f64 = 1.0;
pub const MIN_SPEED_MULTIPLIER: f64 = 0.15;
pub const V5_WALK_SPEED_MS_DEFAULT: f64 = 1.4;
pub const V5_RUN_SPEED_MS_DEFAULT: f64 = 2.8;
pub const V5_SPRINT_SPEED_MS_DEFAULT: f64 = 4.0;

pub const MOVEMENT_IDLE: i32 = 0;
pub const MOVEMENT_WALK: i32 = 1;
pub const MOVEMENT_RUN: i32 = 2;
pub const MOVEMENT_SPRINT: i32 = 3;

pub const STANCE_STAND: i32 = 0;
pub const STANCE_CROUCH: i32 = 1;
pub const STANCE_PRONE: i32 = 2;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RssConstants {
    pub game_max_speed: f64,
    pub character_weight: f64,
    pub reference_weight: f64,
    pub base_weight: f64,

    pub pandolf_base_coeff: f64,
    pub pandolf_velocity_coeff: f64,
    pub pandolf_velocity_offset: f64,
    pub pandolf_grade_base_coeff: f64,
    pub pandolf_grade_velocity_coeff: f64,
    pub pandolf_static_coeff_1: f64,
    pub pandolf_static_coeff_2: f64,

    pub gentle_downhill_grade_max: f64,
    pub gentle_downhill_savings_multiplier: f64,
    pub steep_downhill_grade_threshold: f64,
    pub steep_downhill_penalty_max_fraction: f64,

    pub fixed_fitness_efficiency_factor: f64,
    pub fixed_fitness_recovery_multiplier: f64,
    pub fixed_age_recovery_multiplier: f64,
    pub fixed_pandolf_fitness_bonus: f64,

    pub aerobic_threshold: f64,
    pub anaerobic_threshold: f64,
    pub aerobic_efficiency_factor: f64,
    pub anaerobic_efficiency_factor: f64,

    pub base_recovery_rate: f64,
    pub recovery_nonlinear_coeff: f64,
    pub fast_recovery_duration_minutes: f64,
    pub fast_recovery_multiplier: f64,
    pub medium_recovery_start_minutes: f64,
    pub medium_recovery_duration_minutes: f64,
    pub medium_recovery_multiplier: f64,
    pub slow_recovery_start_minutes: f64,
    pub slow_recovery_multiplier: f64,

    pub standing_recovery_multiplier: f64,
    pub crouching_recovery_multiplier: f64,
    pub prone_recovery_multiplier: f64,

    pub posture_crouch_multiplier: f64,
    pub posture_prone_multiplier: f64,

    pub rest_recovery_rate: f64,
    pub rest_recovery_per_tick: f64,

    pub body_tolerance_base: f64,
    pub encumbrance_speed_penalty_coeff: f64,
    pub encumbrance_speed_penalty_exponent: f64,
    pub encumbrance_speed_penalty_max: f64,
    pub encumbrance_stamina_drain_coeff: f64,
    pub load_recovery_penalty_coeff: f64,
    pub load_recovery_penalty_exponent: f64,

    pub sprint_velocity_threshold: f64,
    pub sprint_stamina_drain_multiplier: f64,
    pub target_run_speed: f64,
    pub target_run_speed_multiplier: f64,
    pub smooth_transition_start: f64,
    pub smooth_transition_end: f64,
    pub min_limp_speed_multiplier: f64,
    pub exhaustion_threshold: f64,
    pub sprint_enable_threshold: f64,
    pub sprint_speed_boost: f64,
    pub willpower_threshold: f64,

    pub tactical_sprint_burst_duration: f64,
    pub tactical_sprint_burst_buffer_duration: f64,
    pub tactical_sprint_burst_encumbrance_factor: f64,
    pub tactical_sprint_cooldown: f64,

    pub fatigue_start_time_minutes: f64,
    pub fatigue_accumulation_coeff: f64,
    pub fatigue_max_factor: f64,
    pub fatigue_recovery_penalty: f64,
    pub fatigue_recovery_duration_minutes: f64,

    pub marginal_decay_threshold: f64,
    pub marginal_decay_coeff: f64,
    pub min_recovery_stamina_threshold: f64,
    pub min_recovery_rest_time_seconds: f64,

    pub epoc_delay_seconds: f64,
    pub epoc_drain_rate: f64,

    pub energy_to_stamina_coeff: f64,

    pub jump_efficiency: f64,
    pub jump_height_guess: f64,
    pub jump_horizontal_speed_guess: f64,
    pub climb_iso_efficiency: f64,
    pub jump_gravity: f64,
    pub jump_stamina_to_joules: f64,
    pub jump_vault_max_drain_clamp: f64,
    pub vault_vert_lift_guess: f64,
    pub vault_limb_force_ratio: f64,
    pub vault_base_metabolism_watts: f64,

    pub env_temperature_heat_penalty_coeff: f64,
    pub env_temperature_cold_recovery_penalty_coeff: f64,
    pub env_temperature_cold_static_penalty: f64,
    pub env_heat_stress_max_multiplier: f64,
    pub env_heat_stress_indoor_reduction: f64,
    pub env_surface_wetness_penalty_max: f64,
    pub env_wind_resistance_coeff: f64,
    pub env_wind_tailwind_bonus: f64,
    pub env_wind_tailwind_max: f64,

    pub uphill_speed_boost: f64,
    pub downhill_speed_boost: f64,
    pub downhill_speed_max_multiplier: f64,
    pub tobler_dampening: f64,

    pub load_metabolic_dampening: f64,
    pub max_recovery_per_tick: f64,
    pub slope_uphill_coeff: f64,
    pub slope_downhill_coeff: f64,

    pub critical_power_watts: f64,
    pub w_prime_max_joules: f64,
    pub w_prime_recovery_w_per_s: f64,
    pub sprint_power_cap_watts: f64,
    pub v5_walk_speed_ms: f64,
    pub v5_run_speed_ms: f64,
    pub v5_sprint_speed_ms: f64,
}

impl Default for RssConstants {
    fn default() -> Self {
        Self {
            game_max_speed: 5.5,
            character_weight: 90.0,
            reference_weight: 90.0,
            base_weight: 1.36,
            pandolf_base_coeff: 2.7,
            pandolf_velocity_coeff: 3.2,
            pandolf_velocity_offset: 0.7,
            pandolf_grade_base_coeff: 0.23,
            pandolf_grade_velocity_coeff: 1.34,
            pandolf_static_coeff_1: 1.2,
            pandolf_static_coeff_2: 1.6,
            gentle_downhill_grade_max: 12.0,
            gentle_downhill_savings_multiplier: 1.25,
            steep_downhill_grade_threshold: 15.0,
            steep_downhill_penalty_max_fraction: 0.5,
            fixed_fitness_efficiency_factor: 0.70,
            fixed_fitness_recovery_multiplier: 1.25,
            fixed_age_recovery_multiplier: 1.053,
            fixed_pandolf_fitness_bonus: 0.80,
            aerobic_threshold: 0.6,
            anaerobic_threshold: 0.8,
            aerobic_efficiency_factor: 0.9,
            anaerobic_efficiency_factor: 1.2,
            base_recovery_rate: 0.00010,
            recovery_nonlinear_coeff: 0.5,
            fast_recovery_duration_minutes: 0.4,
            fast_recovery_multiplier: 1.6,
            medium_recovery_start_minutes: 0.4,
            medium_recovery_duration_minutes: 5.0,
            medium_recovery_multiplier: 1.0,
            slow_recovery_start_minutes: 10.0,
            slow_recovery_multiplier: 0.35,
            standing_recovery_multiplier: 0.85,
            crouching_recovery_multiplier: 1.6,
            prone_recovery_multiplier: 1.9,
            posture_crouch_multiplier: 3.0,
            posture_prone_multiplier: 3.5,
            rest_recovery_rate: 0.250,
            rest_recovery_per_tick: 0.250 / 100.0 * 0.2,
            body_tolerance_base: 90.0,
            encumbrance_speed_penalty_coeff: 0.28,
            encumbrance_speed_penalty_exponent: 1.5,
            encumbrance_speed_penalty_max: 0.75,
            encumbrance_stamina_drain_coeff: 2.8,
            load_recovery_penalty_coeff: 0.0002,
            load_recovery_penalty_exponent: 2.0,
            sprint_velocity_threshold: 5.5,
            sprint_stamina_drain_multiplier: 3.5,
            target_run_speed: 3.8,
            target_run_speed_multiplier: 3.8 / 5.5,
            smooth_transition_start: 0.35,
            smooth_transition_end: 0.05,
            min_limp_speed_multiplier: 1.0 / 5.5,
            exhaustion_threshold: 0.0,
            sprint_enable_threshold: 0.25,
            sprint_speed_boost: 0.22,
            willpower_threshold: 0.35,
            tactical_sprint_burst_duration: 8.0,
            tactical_sprint_burst_buffer_duration: 5.0,
            tactical_sprint_burst_encumbrance_factor: 0.2,
            tactical_sprint_cooldown: 15.0,
            fatigue_start_time_minutes: 5.0,
            fatigue_accumulation_coeff: 0.025,
            fatigue_max_factor: 2.5,
            fatigue_recovery_penalty: 0.05,
            fatigue_recovery_duration_minutes: 20.0,
            marginal_decay_threshold: 0.8,
            marginal_decay_coeff: 1.1,
            min_recovery_stamina_threshold: 0.2,
            min_recovery_rest_time_seconds: 5.0,
            epoc_delay_seconds: 2.0,
            epoc_drain_rate: 0.001,
            energy_to_stamina_coeff: 9.5e-07,
            jump_efficiency: 0.22,
            jump_height_guess: 0.5,
            jump_horizontal_speed_guess: 0.0,
            climb_iso_efficiency: 0.12,
            jump_gravity: 9.81,
            jump_stamina_to_joules: 3.14e5,
            jump_vault_max_drain_clamp: 0.15,
            vault_vert_lift_guess: 0.5,
            vault_limb_force_ratio: 0.5,
            vault_base_metabolism_watts: 50.0,
            env_temperature_heat_penalty_coeff: 0.02,
            env_temperature_cold_recovery_penalty_coeff: 0.05,
            env_temperature_cold_static_penalty: 0.03,
            env_heat_stress_max_multiplier: 1.5,
            env_heat_stress_indoor_reduction: 0.5,
            env_surface_wetness_penalty_max: 0.15,
            env_wind_resistance_coeff: 0.05,
            env_wind_tailwind_bonus: 0.02,
            env_wind_tailwind_max: 0.15,
            uphill_speed_boost: 1.15,
            downhill_speed_boost: 1.15,
            downhill_speed_max_multiplier: 1.25,
            tobler_dampening: 0.7,
            load_metabolic_dampening: 0.70,
            max_recovery_per_tick: 0.0004,
            slope_uphill_coeff: 0.08,
            slope_downhill_coeff: 0.03,
            critical_power_watts: V6_CRITICAL_POWER_WATTS_DEFAULT,
            w_prime_max_joules: V6_W_PRIME_MAX_JOULES_DEFAULT,
            w_prime_recovery_w_per_s: V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT,
            sprint_power_cap_watts: V6_SPRINT_POWER_CAP_WATTS_DEFAULT,
            v5_walk_speed_ms: V5_WALK_SPEED_MS_DEFAULT,
            v5_run_speed_ms: V5_RUN_SPEED_MS_DEFAULT,
            v5_sprint_speed_ms: V5_SPRINT_SPEED_MS_DEFAULT,
        }
    }
}

impl RssConstants {
    pub fn from_params(params: &HashMap<String, f64>) -> Self {
        let mut c = Self::default();
        c.apply_params(params);
        c
    }

    pub fn apply_params(&mut self, params: &HashMap<String, f64>) {
        for (key, value) in params {
            match key.as_str() {
                "game_max_speed" => self.game_max_speed = *value,
                "character_weight" => self.character_weight = *value,
                "reference_weight" => self.reference_weight = *value,
                "base_weight" => self.base_weight = *value,
                "pandolf_base_coeff" => self.pandolf_base_coeff = *value,
                "pandolf_velocity_coeff" => self.pandolf_velocity_coeff = *value,
                "pandolf_velocity_offset" => self.pandolf_velocity_offset = *value,
                "pandolf_grade_base_coeff" => self.pandolf_grade_base_coeff = *value,
                "pandolf_grade_velocity_coeff" => self.pandolf_grade_velocity_coeff = *value,
                "pandolf_static_coeff_1" => self.pandolf_static_coeff_1 = *value,
                "pandolf_static_coeff_2" => self.pandolf_static_coeff_2 = *value,
                "gentle_downhill_grade_max" => self.gentle_downhill_grade_max = *value,
                "gentle_downhill_savings_multiplier" => {
                    self.gentle_downhill_savings_multiplier = *value
                }
                "steep_downhill_grade_threshold" => self.steep_downhill_grade_threshold = *value,
                "steep_downhill_penalty_max_fraction" => {
                    self.steep_downhill_penalty_max_fraction = *value
                }
                "fixed_fitness_efficiency_factor" => self.fixed_fitness_efficiency_factor = *value,
                "fixed_fitness_recovery_multiplier" => self.fixed_fitness_recovery_multiplier = *value,
                "fixed_age_recovery_multiplier" => self.fixed_age_recovery_multiplier = *value,
                "fixed_pandolf_fitness_bonus" => self.fixed_pandolf_fitness_bonus = *value,
                "aerobic_threshold" => self.aerobic_threshold = *value,
                "anaerobic_threshold" => self.anaerobic_threshold = *value,
                "aerobic_efficiency_factor" => self.aerobic_efficiency_factor = *value,
                "anaerobic_efficiency_factor" => self.anaerobic_efficiency_factor = *value,
                "base_recovery_rate" => self.base_recovery_rate = *value,
                "recovery_nonlinear_coeff" => self.recovery_nonlinear_coeff = *value,
                "fast_recovery_duration_minutes" => self.fast_recovery_duration_minutes = *value,
                "fast_recovery_multiplier" => self.fast_recovery_multiplier = *value,
                "medium_recovery_start_minutes" => self.medium_recovery_start_minutes = *value,
                "medium_recovery_duration_minutes" => self.medium_recovery_duration_minutes = *value,
                "medium_recovery_multiplier" => self.medium_recovery_multiplier = *value,
                "slow_recovery_start_minutes" => self.slow_recovery_start_minutes = *value,
                "slow_recovery_multiplier" => self.slow_recovery_multiplier = *value,
                "standing_recovery_multiplier" => self.standing_recovery_multiplier = *value,
                "crouching_recovery_multiplier" => self.crouching_recovery_multiplier = *value,
                "prone_recovery_multiplier" => self.prone_recovery_multiplier = *value,
                "posture_crouch_multiplier" => self.posture_crouch_multiplier = *value,
                "posture_prone_multiplier" => self.posture_prone_multiplier = *value,
                "rest_recovery_rate" => self.rest_recovery_rate = *value,
                "rest_recovery_per_tick" => self.rest_recovery_per_tick = *value,
                "body_tolerance_base" => self.body_tolerance_base = *value,
                "encumbrance_speed_penalty_coeff" => self.encumbrance_speed_penalty_coeff = *value,
                "encumbrance_speed_penalty_exponent" => {
                    self.encumbrance_speed_penalty_exponent = *value
                }
                "encumbrance_speed_penalty_max" => self.encumbrance_speed_penalty_max = *value,
                "encumbrance_stamina_drain_coeff" => self.encumbrance_stamina_drain_coeff = *value,
                "load_recovery_penalty_coeff" => self.load_recovery_penalty_coeff = *value,
                "load_recovery_penalty_exponent" => self.load_recovery_penalty_exponent = *value,
                "sprint_velocity_threshold" => self.sprint_velocity_threshold = *value,
                "sprint_stamina_drain_multiplier" => self.sprint_stamina_drain_multiplier = *value,
                "target_run_speed" => self.target_run_speed = *value,
                "target_run_speed_multiplier" => self.target_run_speed_multiplier = *value,
                "smooth_transition_start" => self.smooth_transition_start = *value,
                "smooth_transition_end" => self.smooth_transition_end = *value,
                "min_limp_speed_multiplier" => self.min_limp_speed_multiplier = *value,
                "exhaustion_threshold" => self.exhaustion_threshold = *value,
                "sprint_enable_threshold" => self.sprint_enable_threshold = *value,
                "sprint_speed_boost" => self.sprint_speed_boost = *value,
                "willpower_threshold" => self.willpower_threshold = *value,
                "tactical_sprint_burst_duration" => self.tactical_sprint_burst_duration = *value,
                "tactical_sprint_burst_buffer_duration" => {
                    self.tactical_sprint_burst_buffer_duration = *value
                }
                "tactical_sprint_burst_encumbrance_factor" => {
                    self.tactical_sprint_burst_encumbrance_factor = *value
                }
                "tactical_sprint_cooldown" => self.tactical_sprint_cooldown = *value,
                "fatigue_start_time_minutes" => self.fatigue_start_time_minutes = *value,
                "fatigue_accumulation_coeff" => self.fatigue_accumulation_coeff = *value,
                "fatigue_max_factor" => self.fatigue_max_factor = *value,
                "fatigue_recovery_penalty" => self.fatigue_recovery_penalty = *value,
                "fatigue_recovery_duration_minutes" => self.fatigue_recovery_duration_minutes = *value,
                "marginal_decay_threshold" => self.marginal_decay_threshold = *value,
                "marginal_decay_coeff" => self.marginal_decay_coeff = *value,
                "min_recovery_stamina_threshold" => self.min_recovery_stamina_threshold = *value,
                "min_recovery_rest_time_seconds" => self.min_recovery_rest_time_seconds = *value,
                "epoc_delay_seconds" => self.epoc_delay_seconds = *value,
                "epoc_drain_rate" => self.epoc_drain_rate = *value,
                "energy_to_stamina_coeff" => self.energy_to_stamina_coeff = *value,
                "jump_efficiency" => self.jump_efficiency = *value,
                "jump_height_guess" => self.jump_height_guess = *value,
                "jump_horizontal_speed_guess" => self.jump_horizontal_speed_guess = *value,
                "climb_iso_efficiency" => self.climb_iso_efficiency = *value,
                "jump_gravity" => self.jump_gravity = *value,
                "jump_stamina_to_joules" => self.jump_stamina_to_joules = *value,
                "jump_vault_max_drain_clamp" => self.jump_vault_max_drain_clamp = *value,
                "vault_vert_lift_guess" => self.vault_vert_lift_guess = *value,
                "vault_limb_force_ratio" => self.vault_limb_force_ratio = *value,
                "vault_base_metabolism_watts" => self.vault_base_metabolism_watts = *value,
                "env_temperature_heat_penalty_coeff" => self.env_temperature_heat_penalty_coeff = *value,
                "env_temperature_cold_recovery_penalty_coeff" => {
                    self.env_temperature_cold_recovery_penalty_coeff = *value
                }
                "env_temperature_cold_static_penalty" => self.env_temperature_cold_static_penalty = *value,
                "env_heat_stress_max_multiplier" => self.env_heat_stress_max_multiplier = *value,
                "env_heat_stress_indoor_reduction" => self.env_heat_stress_indoor_reduction = *value,
                "env_surface_wetness_penalty_max" => self.env_surface_wetness_penalty_max = *value,
                "env_wind_resistance_coeff" => self.env_wind_resistance_coeff = *value,
                "env_wind_tailwind_bonus" => self.env_wind_tailwind_bonus = *value,
                "env_wind_tailwind_max" => self.env_wind_tailwind_max = *value,
                "uphill_speed_boost" => self.uphill_speed_boost = *value,
                "downhill_speed_boost" => self.downhill_speed_boost = *value,
                "downhill_speed_max_multiplier" => self.downhill_speed_max_multiplier = *value,
                "tobler_dampening" => self.tobler_dampening = *value,
                "load_metabolic_dampening" => self.load_metabolic_dampening = *value,
                "max_recovery_per_tick" => self.max_recovery_per_tick = *value,
                "slope_uphill_coeff" => self.slope_uphill_coeff = *value,
                "slope_downhill_coeff" => self.slope_downhill_coeff = *value,
                "critical_power_watts" => self.critical_power_watts = *value,
                "w_prime_max_joules" => self.w_prime_max_joules = *value,
                "w_prime_recovery_w_per_s" => self.w_prime_recovery_w_per_s = *value,
                "sprint_power_cap_watts" => self.sprint_power_cap_watts = *value,
                "v5_walk_speed_ms" => self.v5_walk_speed_ms = *value,
                "v5_run_speed_ms" => self.v5_run_speed_ms = *value,
                "v5_sprint_speed_ms" => self.v5_sprint_speed_ms = *value,
                _ => {}
            }
        }
    }

    pub fn to_params_map(&self) -> HashMap<String, f64> {
        let mut out = HashMap::new();
        macro_rules! put {
            ($key:literal, $value:expr) => {
                out.insert($key.to_string(), $value);
            };
        }
        put!("game_max_speed", self.game_max_speed);
        put!("character_weight", self.character_weight);
        put!("reference_weight", self.reference_weight);
        put!("base_weight", self.base_weight);
        put!("pandolf_base_coeff", self.pandolf_base_coeff);
        put!("pandolf_velocity_coeff", self.pandolf_velocity_coeff);
        put!("pandolf_velocity_offset", self.pandolf_velocity_offset);
        put!("pandolf_grade_base_coeff", self.pandolf_grade_base_coeff);
        put!("pandolf_grade_velocity_coeff", self.pandolf_grade_velocity_coeff);
        put!("pandolf_static_coeff_1", self.pandolf_static_coeff_1);
        put!("pandolf_static_coeff_2", self.pandolf_static_coeff_2);
        put!("gentle_downhill_grade_max", self.gentle_downhill_grade_max);
        put!(
            "gentle_downhill_savings_multiplier",
            self.gentle_downhill_savings_multiplier
        );
        put!("steep_downhill_grade_threshold", self.steep_downhill_grade_threshold);
        put!(
            "steep_downhill_penalty_max_fraction",
            self.steep_downhill_penalty_max_fraction
        );
        put!(
            "fixed_fitness_efficiency_factor",
            self.fixed_fitness_efficiency_factor
        );
        put!(
            "fixed_fitness_recovery_multiplier",
            self.fixed_fitness_recovery_multiplier
        );
        put!("fixed_age_recovery_multiplier", self.fixed_age_recovery_multiplier);
        put!("fixed_pandolf_fitness_bonus", self.fixed_pandolf_fitness_bonus);
        put!("aerobic_threshold", self.aerobic_threshold);
        put!("anaerobic_threshold", self.anaerobic_threshold);
        put!("aerobic_efficiency_factor", self.aerobic_efficiency_factor);
        put!("anaerobic_efficiency_factor", self.anaerobic_efficiency_factor);
        put!("base_recovery_rate", self.base_recovery_rate);
        put!("recovery_nonlinear_coeff", self.recovery_nonlinear_coeff);
        put!("fast_recovery_duration_minutes", self.fast_recovery_duration_minutes);
        put!("fast_recovery_multiplier", self.fast_recovery_multiplier);
        put!("medium_recovery_start_minutes", self.medium_recovery_start_minutes);
        put!("medium_recovery_duration_minutes", self.medium_recovery_duration_minutes);
        put!("medium_recovery_multiplier", self.medium_recovery_multiplier);
        put!("slow_recovery_start_minutes", self.slow_recovery_start_minutes);
        put!("slow_recovery_multiplier", self.slow_recovery_multiplier);
        put!("standing_recovery_multiplier", self.standing_recovery_multiplier);
        put!("crouching_recovery_multiplier", self.crouching_recovery_multiplier);
        put!("prone_recovery_multiplier", self.prone_recovery_multiplier);
        put!("posture_crouch_multiplier", self.posture_crouch_multiplier);
        put!("posture_prone_multiplier", self.posture_prone_multiplier);
        put!("rest_recovery_rate", self.rest_recovery_rate);
        put!("rest_recovery_per_tick", self.rest_recovery_per_tick);
        put!("body_tolerance_base", self.body_tolerance_base);
        put!(
            "encumbrance_speed_penalty_coeff",
            self.encumbrance_speed_penalty_coeff
        );
        put!(
            "encumbrance_speed_penalty_exponent",
            self.encumbrance_speed_penalty_exponent
        );
        put!(
            "encumbrance_speed_penalty_max",
            self.encumbrance_speed_penalty_max
        );
        put!(
            "encumbrance_stamina_drain_coeff",
            self.encumbrance_stamina_drain_coeff
        );
        put!("load_recovery_penalty_coeff", self.load_recovery_penalty_coeff);
        put!(
            "load_recovery_penalty_exponent",
            self.load_recovery_penalty_exponent
        );
        put!("sprint_velocity_threshold", self.sprint_velocity_threshold);
        put!(
            "sprint_stamina_drain_multiplier",
            self.sprint_stamina_drain_multiplier
        );
        put!("target_run_speed", self.target_run_speed);
        put!("target_run_speed_multiplier", self.target_run_speed_multiplier);
        put!("smooth_transition_start", self.smooth_transition_start);
        put!("smooth_transition_end", self.smooth_transition_end);
        put!("min_limp_speed_multiplier", self.min_limp_speed_multiplier);
        put!("exhaustion_threshold", self.exhaustion_threshold);
        put!("sprint_enable_threshold", self.sprint_enable_threshold);
        put!("sprint_speed_boost", self.sprint_speed_boost);
        put!("willpower_threshold", self.willpower_threshold);
        put!(
            "tactical_sprint_burst_duration",
            self.tactical_sprint_burst_duration
        );
        put!(
            "tactical_sprint_burst_buffer_duration",
            self.tactical_sprint_burst_buffer_duration
        );
        put!(
            "tactical_sprint_burst_encumbrance_factor",
            self.tactical_sprint_burst_encumbrance_factor
        );
        put!("tactical_sprint_cooldown", self.tactical_sprint_cooldown);
        put!("fatigue_start_time_minutes", self.fatigue_start_time_minutes);
        put!("fatigue_accumulation_coeff", self.fatigue_accumulation_coeff);
        put!("fatigue_max_factor", self.fatigue_max_factor);
        put!("fatigue_recovery_penalty", self.fatigue_recovery_penalty);
        put!(
            "fatigue_recovery_duration_minutes",
            self.fatigue_recovery_duration_minutes
        );
        put!("marginal_decay_threshold", self.marginal_decay_threshold);
        put!("marginal_decay_coeff", self.marginal_decay_coeff);
        put!(
            "min_recovery_stamina_threshold",
            self.min_recovery_stamina_threshold
        );
        put!(
            "min_recovery_rest_time_seconds",
            self.min_recovery_rest_time_seconds
        );
        put!("epoc_delay_seconds", self.epoc_delay_seconds);
        put!("epoc_drain_rate", self.epoc_drain_rate);
        put!("energy_to_stamina_coeff", self.energy_to_stamina_coeff);
        put!("jump_efficiency", self.jump_efficiency);
        put!("jump_height_guess", self.jump_height_guess);
        put!("jump_horizontal_speed_guess", self.jump_horizontal_speed_guess);
        put!("climb_iso_efficiency", self.climb_iso_efficiency);
        put!("jump_gravity", self.jump_gravity);
        put!("jump_stamina_to_joules", self.jump_stamina_to_joules);
        put!("jump_vault_max_drain_clamp", self.jump_vault_max_drain_clamp);
        put!("vault_vert_lift_guess", self.vault_vert_lift_guess);
        put!("vault_limb_force_ratio", self.vault_limb_force_ratio);
        put!("vault_base_metabolism_watts", self.vault_base_metabolism_watts);
        put!(
            "env_temperature_heat_penalty_coeff",
            self.env_temperature_heat_penalty_coeff
        );
        put!(
            "env_temperature_cold_recovery_penalty_coeff",
            self.env_temperature_cold_recovery_penalty_coeff
        );
        put!(
            "env_temperature_cold_static_penalty",
            self.env_temperature_cold_static_penalty
        );
        put!(
            "env_heat_stress_max_multiplier",
            self.env_heat_stress_max_multiplier
        );
        put!(
            "env_heat_stress_indoor_reduction",
            self.env_heat_stress_indoor_reduction
        );
        put!(
            "env_surface_wetness_penalty_max",
            self.env_surface_wetness_penalty_max
        );
        put!("env_wind_resistance_coeff", self.env_wind_resistance_coeff);
        put!("env_wind_tailwind_bonus", self.env_wind_tailwind_bonus);
        put!("env_wind_tailwind_max", self.env_wind_tailwind_max);
        put!("uphill_speed_boost", self.uphill_speed_boost);
        put!("downhill_speed_boost", self.downhill_speed_boost);
        put!(
            "downhill_speed_max_multiplier",
            self.downhill_speed_max_multiplier
        );
        put!("tobler_dampening", self.tobler_dampening);
        put!("load_metabolic_dampening", self.load_metabolic_dampening);
        put!("max_recovery_per_tick", self.max_recovery_per_tick);
        put!("slope_uphill_coeff", self.slope_uphill_coeff);
        put!("slope_downhill_coeff", self.slope_downhill_coeff);
        put!("critical_power_watts", self.critical_power_watts);
        put!("w_prime_max_joules", self.w_prime_max_joules);
        put!("w_prime_recovery_w_per_s", self.w_prime_recovery_w_per_s);
        put!("sprint_power_cap_watts", self.sprint_power_cap_watts);
        put!("v5_walk_speed_ms", self.v5_walk_speed_ms);
        put!("v5_run_speed_ms", self.v5_run_speed_ms);
        put!("v5_sprint_speed_ms", self.v5_sprint_speed_ms);
        out
    }
}

pub fn game_aligned_fixed_params() -> HashMap<String, f64> {
    let mut fixed = HashMap::new();
    fixed.insert("sprint_stamina_drain_multiplier".to_string(), 3.5);
    fixed.insert("load_metabolic_dampening".to_string(), 0.70);
    fixed.insert("aerobic_efficiency_factor".to_string(), 0.9);
    fixed.insert("anaerobic_efficiency_factor".to_string(), 1.2);
    fixed.insert("fatigue_accumulation_coeff".to_string(), 0.015);
    fixed.insert("fatigue_max_factor".to_string(), 2.0);
    fixed.insert("encumbrance_speed_penalty_exponent".to_string(), 1.5);
    fixed.insert("encumbrance_speed_penalty_max".to_string(), 0.75);
    fixed.insert("load_recovery_penalty_exponent".to_string(), 2.0);
    fixed.insert("marginal_decay_threshold".to_string(), 0.8);
    fixed.insert("marginal_decay_coeff".to_string(), 1.1);
    fixed.insert("min_recovery_stamina_threshold".to_string(), 0.2);
    fixed.insert("min_recovery_rest_time_seconds".to_string(), 3.0);
    fixed.insert("sprint_velocity_threshold".to_string(), 5.5);
    fixed
}

pub fn merge_game_aligned_params(optuna_params: &HashMap<String, f64>) -> HashMap<String, f64> {
    let mut merged = game_aligned_fixed_params();
    for (k, v) in optuna_params {
        merged.insert(k.clone(), *v);
    }
    merged
}
