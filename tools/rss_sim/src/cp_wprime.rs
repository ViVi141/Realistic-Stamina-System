use crate::constants::{
    V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V5_BURST_COOLDOWN_FULL_DEFAULT,
    V5_BURST_COOLDOWN_SHORT_DEFAULT, V5_BURST_EARLY_RELEASE_BONUS, V5_TACTICAL_SHORT_BURST_SEC,
    V6_CP_ENV_FLOOR, V6_CRITICAL_POWER_WATTS_DEFAULT, V6_SKIBA_ELITE_CP_THRESHOLD_W,
    V6_SPRINT_POWER_CAP_WATTS_DEFAULT, V6_W_PRIME_K_FAST, V6_W_PRIME_K_SLOW,
    V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT, V6_W_PRIME_LIM_RATIO,
};
use crate::drain::{
    get_drain_velocity_ms, is_metabolic_overspeed_accounting, is_wprime_pool_available_for_overspeed,
};
use crate::math::clip_f64;
use crate::metabolism::{compute_cp_watts, invert_speed_for_power_watts};

#[derive(Clone, Debug)]
pub struct V5AnaerobicState {
    pub pool: f64,
    pub cooldown_until_sec: f64,
}

impl Default for V5AnaerobicState {
    fn default() -> Self {
        Self {
            pool: 1.0,
            cooldown_until_sec: -1.0,
        }
    }
}

impl V5AnaerobicState {
    pub fn tick_sprint(&mut self, dt_sec: f64, drain_per_sec: f64) {
        self.pool = (self.pool - drain_per_sec * dt_sec).max(0.0);
    }

    pub fn cooldown_remaining(&self, world_time_sec: f64) -> f64 {
        if self.cooldown_until_sec < 0.0 {
            return 0.0;
        }
        (self.cooldown_until_sec - world_time_sec).max(0.0)
    }
}

#[derive(Clone, Debug)]
pub struct V6CriticalPowerState {
    pub cp0: f64,
    pub w_prime_max_joules: f64,
    pub sprint_power_cap_watts: f64,

    pub w_prime_joules: f64,
    pub cooldown_until_sec: f64,
    pub sprint_start_sec: f64,
    pub was_sprinting: bool,
    pub last_short_burst_release_sec: f64,
    pub depletion_cooldown_applied: bool,

    pub load_kg: f64,
    pub grade_percent: f64,
    pub env_cp_mult: f64,
    pub fatigue_norm: f64,
    pub fatigue_cp_multiplier: f64,
}

impl V6CriticalPowerState {
    pub fn new(cp0: f64, w_prime_max: f64, sprint_power_cap: f64) -> Self {
        let mut s = Self {
            cp0,
            w_prime_max_joules: w_prime_max,
            sprint_power_cap_watts: sprint_power_cap,
            w_prime_joules: 0.0,
            cooldown_until_sec: -1.0,
            sprint_start_sec: -1.0,
            was_sprinting: false,
            last_short_burst_release_sec: -1.0,
            depletion_cooldown_applied: false,
            load_kg: 0.0,
            grade_percent: 0.0,
            env_cp_mult: 1.0,
            fatigue_norm: 0.0,
            fatigue_cp_multiplier: 1.0,
        };
        s.reset_to_full();
        s
    }

    pub fn reset_to_full(&mut self) {
        self.w_prime_joules = self.w_prime_max_joules;
        self.cooldown_until_sec = -1.0;
        self.sprint_start_sec = -1.0;
        self.was_sprinting = false;
        self.last_short_burst_release_sec = -1.0;
        self.depletion_cooldown_applied = false;
    }

    pub fn set_runtime_context(
        &mut self,
        load_kg: f64,
        grade_percent: f64,
        env_cp_mult: f64,
        fatigue_norm: f64,
    ) {
        self.load_kg = load_kg.max(0.0);
        self.grade_percent = grade_percent;
        self.env_cp_mult = clip_f64(env_cp_mult, V6_CP_ENV_FLOOR, 1.0);
        self.fatigue_norm = clip_f64(fatigue_norm, 0.0, 1.0);
    }

    pub fn set_fatigue_cp_multiplier(&mut self, mult: f64) {
        self.fatigue_cp_multiplier = clip_f64(mult, 0.75, 1.0);
    }

    pub fn pool01(&self) -> f64 {
        if self.w_prime_max_joules <= 1.0 {
            return 0.0;
        }
        clip_f64(self.w_prime_joules / self.w_prime_max_joules, 0.0, 1.0)
    }

    pub fn cooldown_remaining_at(&self, world_time_sec: f64) -> f64 {
        if self.cooldown_until_sec < 0.0 {
            return 0.0;
        }
        (self.cooldown_until_sec - world_time_sec).max(0.0)
    }

    pub fn is_on_cooldown(&self, world_time_sec: f64) -> bool {
        self.cooldown_remaining_at(world_time_sec) > 0.0
    }

    pub fn compute_cp_base_watts(&self) -> f64 {
        compute_cp_watts(self.cp0, self.load_kg, self.grade_percent, self.env_cp_mult, 0.0)
    }

    pub fn get_effective_critical_power_watts(&self) -> f64 {
        self.compute_cp_base_watts() * self.fatigue_cp_multiplier
    }

    fn uses_skiba_recovery(&self) -> bool {
        self.cp0 <= V6_SKIBA_ELITE_CP_THRESHOLD_W
    }

    fn apply_w_prime_recovery(&mut self, _power_watts: f64, _cp: f64, dt: f64) {
        if self.uses_skiba_recovery() {
            let w_lim = self.w_prime_max_joules * V6_W_PRIME_LIM_RATIO;
            let mut k_fast = V6_W_PRIME_K_FAST * (1.0 - 0.3 * self.fatigue_norm);
            let mut k_slow = V6_W_PRIME_K_SLOW * (1.0 - 0.5 * self.fatigue_norm);
            k_fast = k_fast.max(0.01);
            k_slow = k_slow.max(0.0001);
            let mut fast_term = 0.0;
            if self.w_prime_joules < w_lim {
                fast_term = k_fast * (w_lim - self.w_prime_joules);
            }
            let mut slow_term = 0.0;
            if self.w_prime_joules >= w_lim {
                slow_term = k_slow * (self.w_prime_max_joules - self.w_prime_joules);
            }
            self.w_prime_joules += (fast_term + slow_term) * dt;
        } else {
            self.w_prime_joules += V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT * dt;
        }
        self.w_prime_joules = self.w_prime_joules.min(self.w_prime_max_joules);
    }

    pub fn get_available_power_watts(
        &self,
        sprint_intent: bool,
        dt: f64,
        world_time_sec: f64,
    ) -> f64 {
        let cp = self.get_effective_critical_power_watts();
        if !sprint_intent {
            return cp;
        }
        if self.is_on_cooldown(world_time_sec) {
            return cp;
        }
        let mut cap = self.sprint_power_cap_watts;
        if cap <= cp {
            cap = cp + V6_SPRINT_POWER_CAP_WATTS_DEFAULT * 0.5;
        }
        if self.w_prime_joules <= 0.0 {
            return cp;
        }
        let burst_budget = self.w_prime_joules / dt.max(0.01);
        let available = cp + burst_budget;
        available.min(cap)
    }

    pub fn is_sprint_allowed(
        &self,
        aerobic_stamina: f64,
        collapse_state: bool,
        world_time_sec: f64,
    ) -> bool {
        if collapse_state {
            return false;
        }
        if aerobic_stamina < 0.25 {
            return false;
        }
        if self.pool01() <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT {
            return false;
        }
        if self.is_on_cooldown(world_time_sec) {
            return false;
        }
        true
    }

    fn apply_cooldown_on_sprint_end(
        &mut self,
        world_time_sec: f64,
        burst_duration_sec: f64,
        reserve_at_end01: f64,
    ) {
        if reserve_at_end01 <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT {
            self.cooldown_until_sec = world_time_sec + V5_BURST_COOLDOWN_FULL_DEFAULT;
            return;
        }
        if burst_duration_sec <= V5_TACTICAL_SHORT_BURST_SEC {
            self.cooldown_until_sec = world_time_sec + V5_BURST_COOLDOWN_SHORT_DEFAULT;
            self.last_short_burst_release_sec = world_time_sec;
            return;
        }
        let mut scaled =
            V5_BURST_COOLDOWN_FULL_DEFAULT * (1.0 - V5_BURST_EARLY_RELEASE_BONUS * reserve_at_end01);
        scaled = scaled.max(V5_BURST_COOLDOWN_SHORT_DEFAULT);
        self.cooldown_until_sec = world_time_sec + scaled;
    }

    pub fn tick(
        &mut self,
        power_watts: f64,
        sprint_intent: bool,
        world_time_sec: f64,
        dt: f64,
        current_speed_ms: f64,
    ) {
        let cp = self.get_effective_critical_power_watts();

        if power_watts > cp {
            let mut allow_discharge = true;
            if current_speed_ms < 0.05 && !sprint_intent {
                allow_discharge = false;
            }
            if allow_discharge {
                let drain_j = (power_watts - cp) * dt;
                self.w_prime_joules = (self.w_prime_joules - drain_j).max(0.0);
            }
        }

        if sprint_intent {
            if !self.was_sprinting {
                self.sprint_start_sec = world_time_sec;
            }
            if self.pool01() <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT && !self.depletion_cooldown_applied {
                let burst_dur = (world_time_sec - self.sprint_start_sec).max(0.0);
                self.apply_cooldown_on_sprint_end(world_time_sec, burst_dur, self.pool01());
                self.depletion_cooldown_applied = true;
            }
        } else {
            self.depletion_cooldown_applied = false;
            if self.was_sprinting {
                let burst_dur = (world_time_sec - self.sprint_start_sec).max(0.0);
                self.apply_cooldown_on_sprint_end(world_time_sec, burst_dur, self.pool01());
                self.sprint_start_sec = -1.0;
            }
            if !self.is_on_cooldown(world_time_sec) && power_watts <= cp + 5.0 {
                self.apply_w_prime_recovery(power_watts, cp, dt);
            }
        }
        self.was_sprinting = sprint_intent;
    }

    pub fn get_w_prime_exhausted_overspeed_cap_ms(
        &self,
        measured_speed_ms: f64,
        applied_speed_limit_ms: f64,
        movement_phase: i32,
        total_weight_kg: f64,
        grade_percent: f64,
        terrain_factor: f64,
    ) -> f64 {
        if !is_metabolic_overspeed_accounting(measured_speed_ms, applied_speed_limit_ms) {
            return -1.0;
        }
        if is_wprime_pool_available_for_overspeed(self.pool01(), V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT) {
            return -1.0;
        }
        let mut cp = self.get_effective_critical_power_watts();
        if cp <= 1.0 {
            cp = V6_CRITICAL_POWER_WATTS_DEFAULT;
        }
        invert_speed_for_power_watts(cp, total_weight_kg, grade_percent, terrain_factor, movement_phase)
    }
}

pub fn get_w_prime_exhausted_overspeed_cap_ms(
    measured_ms: f64,
    applied_limit_ms: f64,
    w_prime_pool01: f64,
    movement_phase: i32,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    cp_model: Option<&V6CriticalPowerState>,
) -> f64 {
    if !is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms) {
        return -1.0;
    }
    if is_wprime_pool_available_for_overspeed(
        w_prime_pool01,
        V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
    ) {
        return -1.0;
    }
    let cp = if let Some(m) = cp_model {
        m.get_effective_critical_power_watts()
    } else {
        V6_CRITICAL_POWER_WATTS_DEFAULT
    };
    if cp <= 1.0 {
        return -1.0;
    }
    invert_speed_for_power_watts(cp, total_weight_kg, grade_percent, terrain_factor, movement_phase)
}

pub fn simulate_v6_sprint_seconds(
    load_kg: f64,
    cp0: f64,
    dt: f64,
    sprint_cap_w: f64,
    w_prime_max: f64,
) -> f64 {
    let mut state = V6CriticalPowerState::new(cp0, w_prime_max, sprint_cap_w);
    state.set_runtime_context(load_kg, 0.0, 1.0, 0.0);
    let mut t = 0.0;
    while state.pool01() > 0.20 && t < 30.0 {
        let available_p = state.get_available_power_watts(true, dt, t);
        state.tick(available_p, true, t, dt, 0.0);
        t += dt;
    }
    t
}

pub fn fatigue_power_for_integral(
    current_speed_ms: f64,
    limit_for_power_ms: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
) -> f64 {
    let fatigue_v = get_drain_velocity_ms(current_speed_ms, limit_for_power_ms);
    crate::metabolism::metabolism_power_watts(
        fatigue_v,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    )
}
