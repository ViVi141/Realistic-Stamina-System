use crate::constants::{
    MOVEMENT_SPRINT, V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V6_OVERSPEED_ACCOUNTING_EPS_MPS,
    V6_STAMINA_DRAIN_CALIBRATION,
};
use crate::math::clip_f64;
use crate::metabolism::{
    invert_speed_for_power_watts, metabolism_power_watts,
};

pub fn get_drain_velocity_ms(measured_ms: f64, theoretical_max_ms: f64) -> f64 {
    let measured_ms = measured_ms.max(0.0);
    let theoretical_max_ms = theoretical_max_ms.max(0.05);
    if measured_ms > theoretical_max_ms {
        theoretical_max_ms
    } else {
        measured_ms
    }
}

pub fn is_wprime_pool_available_for_overspeed(
    w_prime_pool01: f64,
    threshold: f64,
) -> bool {
    w_prime_pool01 > threshold + crate::constants::V6_WPRIME_OVERSPEED_HYSTERESIS
}

pub fn get_metabolic_accounting_velocity_ms(
    measured_ms: f64,
    applied_limit_ms: f64,
    w_prime_pool01: f64,
    is_sprinting: bool,
) -> f64 {
    let measured_ms = measured_ms.max(0.0);
    if applied_limit_ms > 0.05 && measured_ms > applied_limit_ms + V6_OVERSPEED_ACCOUNTING_EPS_MPS {
        if !is_sprinting {
            return get_drain_velocity_ms(measured_ms, applied_limit_ms);
        }
        if !is_wprime_pool_available_for_overspeed(
            w_prime_pool01,
            V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
        ) {
            return get_drain_velocity_ms(measured_ms, applied_limit_ms);
        }
        return measured_ms;
    }
    get_drain_velocity_ms(measured_ms, applied_limit_ms)
}

pub fn get_metabolic_accounting_power_watts(
    measured_ms: f64,
    applied_limit_ms: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
    w_prime_pool01: f64,
    is_sprinting: bool,
) -> f64 {
    let v_acct = get_metabolic_accounting_velocity_ms(
        measured_ms,
        applied_limit_ms,
        w_prime_pool01,
        is_sprinting,
    );
    metabolism_power_watts(v_acct, total_weight_kg, grade_percent, terrain_factor, movement_phase)
}

pub fn stamina_drain_rate_per_second_from_power_watts(
    power_watts: f64,
    critical_power_cap_watts: f64,
    energy_to_stamina_coeff: f64,
) -> f64 {
    let mut aerobic_w = power_watts;
    if critical_power_cap_watts > 1.0 && power_watts > critical_power_cap_watts {
        aerobic_w = critical_power_cap_watts;
    }
    let coeff = clip_f64(energy_to_stamina_coeff, 0.0, 0.1) * V6_STAMINA_DRAIN_CALIBRATION;
    (aerobic_w * coeff).max(0.0)
}

pub fn is_metabolic_overspeed_accounting(measured_ms: f64, applied_limit_ms: f64) -> bool {
    if applied_limit_ms <= 0.05 {
        return false;
    }
    measured_ms > applied_limit_ms + V6_OVERSPEED_ACCOUNTING_EPS_MPS
}

pub fn get_client_overspeed_excess_drain_per_second(
    measured_ms: f64,
    applied_limit_ms: f64,
    w_prime_pool01: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
) -> f64 {
    if !is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms) {
        return 0.0;
    }
    if is_wprime_pool_available_for_overspeed(w_prime_pool01, V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT) {
        return 0.0;
    }
    let p_meas = metabolism_power_watts(
        measured_ms,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    );
    let v_drain = get_drain_velocity_ms(measured_ms, applied_limit_ms);
    let p_drain = metabolism_power_watts(
        v_drain,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    );
    let excess_w = p_meas - p_drain;
    if excess_w <= 1.0 {
        return 0.0;
    }
    stamina_drain_rate_per_second_from_power_watts(excess_w, -1.0, 1.55e-07)
}

pub fn get_metabolic_overspeed_factor(
    pandolf_watts: f64,
    sustainable_watts: f64,
    min_factor: f64,
) -> f64 {
    if pandolf_watts <= sustainable_watts {
        return 1.0;
    }
    let ratio = sustainable_watts / pandolf_watts;
    ratio.max(min_factor)
}

pub fn get_metabolic_speed_cap_ms(
    current_speed_ms: f64,
    movement_phase: i32,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    is_exhausted: bool,
    effective_cp_watts: f64,
    w_prime_pool01: f64,
) -> f64 {
    if is_exhausted {
        return -1.0;
    }
    if movement_phase < 1 {
        return -1.0;
    }
    let is_sprint = movement_phase == MOVEMENT_SPRINT;
    if !is_sprint
        && is_wprime_pool_available_for_overspeed(
            w_prime_pool01,
            V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
        )
    {
        return -1.0;
    }
    let power_w = metabolism_power_watts(
        current_speed_ms.max(0.0),
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    );
    let available_p = effective_cp_watts;
    if power_w <= available_p + 1.0 {
        return -1.0;
    }
    let mut target_p = effective_cp_watts;
    if power_w > effective_cp_watts && !is_sprint {
        target_p = effective_cp_watts;
    }
    invert_speed_for_power_watts(
        target_p,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    )
}

pub fn get_metabolic_corrected_speed_multiplier(
    applied_speed_multiplier: f64,
    current_speed_ms: f64,
    movement_phase: i32,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    is_exhausted: bool,
    engine_base_ms: f64,
    effective_cp_watts: f64,
    w_prime_pool01: f64,
) -> f64 {
    let cap_ms = get_metabolic_speed_cap_ms(
        current_speed_ms,
        movement_phase,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        is_exhausted,
        effective_cp_watts,
        w_prime_pool01,
    );
    if cap_ms < 0.0 {
        return applied_speed_multiplier;
    }
    let mut engine_base_ms = engine_base_ms;
    if engine_base_ms <= 0.05 {
        engine_base_ms = 5.5;
    }
    let applied_ms = applied_speed_multiplier * engine_base_ms;
    if applied_ms <= cap_ms + 0.01 {
        return applied_speed_multiplier;
    }
    clip_f64(cap_ms / engine_base_ms, 0.01, 3.0)
}
