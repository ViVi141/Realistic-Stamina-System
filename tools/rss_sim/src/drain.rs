use crate::constants::{
    MOVEMENT_RUN, MOVEMENT_SPRINT, MOVEMENT_WALK, V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
    V6_AEROBIC_CRUISE_MAX_MS, V6_OVERSPEED_ACCOUNTING_EPS_MPS, V6_RUN_GAIT_FLOOR_MS,
    V6_STAMINA_DRAIN_CALIBRATION,
};
use crate::math::clip_f64;
use crate::metabolism::{
    invert_speed_for_power_watts, metabolism_power_watts,
};

pub fn get_drain_velocity_ms(measured_ms: f64, _applied_limit_ms: f64) -> f64 {
    measured_ms.max(0.0)
}

pub fn refresh_wprime_overspeed_armed(
    pool01: f64,
    armed: bool,
    threshold: f64,
) -> bool {
    let disable_at = threshold + crate::constants::V6_WPRIME_OVERSPEED_HYSTERESIS;
    let mut rearm_at = threshold + crate::constants::V6_WPRIME_OVERSPEED_REARM;
    if rearm_at <= disable_at + 0.01 {
        rearm_at = disable_at + 0.15;
    }
    if armed {
        if pool01 <= disable_at {
            return false;
        }
        return true;
    }
    if pool01 > rearm_at {
        return true;
    }
    false
}

pub fn is_wprime_pool_available_for_overspeed(
    w_prime_pool01: f64,
    threshold: f64,
) -> bool {
    // Stateless approx: match game float overload (rearm band, not disarm+hysteresis).
    w_prime_pool01 > threshold + crate::constants::V6_WPRIME_OVERSPEED_REARM
}

pub fn get_epoc_sample_velocity_ms(measured_ms: f64, applied_limit_ms: f64) -> f64 {
    let mut v = measured_ms.max(0.0);
    if applied_limit_ms > 0.05 && v > applied_limit_ms {
        v = applied_limit_ms;
    }
    v
}

pub fn get_metabolic_accounting_velocity_ms(
    measured_ms: f64,
    applied_limit_ms: f64,
    _w_prime_pool01: f64,
    _is_sprinting: bool,
) -> f64 {
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
    _measured_ms: f64,
    _applied_limit_ms: f64,
    _w_prime_pool01: f64,
    _total_weight_kg: f64,
    _grade_percent: f64,
    _terrain_factor: f64,
    _movement_phase: i32,
) -> f64 {
    // 记账已与 v_meas 对齐，双速差惩罚废弃
    0.0
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
    available_power_watts: f64,
    speed_for_power_eval_ms: f64,
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
    let mut eval_speed = current_speed_ms;
    if speed_for_power_eval_ms >= 0.0 {
        eval_speed = speed_for_power_eval_ms;
    }
    let power_w = metabolism_power_watts(
        eval_speed.max(0.0),
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    );
    let mut available_p = effective_cp_watts;
    if available_power_watts >= 0.0 {
        available_p = available_power_watts;
    }
    if power_w <= available_p + 1.0 {
        return -1.0;
    }
    let mut target_p = available_p;
    if power_w > effective_cp_watts && !is_sprint {
        target_p = effective_cp_watts;
    }
    let mut invert_phase = movement_phase;
    if !is_sprint
        && !is_wprime_pool_available_for_overspeed(
            w_prime_pool01,
            V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
        )
    {
        invert_phase = MOVEMENT_RUN;
        if movement_phase == MOVEMENT_WALK {
            invert_phase = MOVEMENT_WALK;
        }
    }
    let mut cap_ms = invert_speed_for_power_watts(
        target_p,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        invert_phase,
    );
    if !is_sprint && invert_phase != MOVEMENT_WALK {
        if grade_percent >= 0.0 && cap_ms > V6_AEROBIC_CRUISE_MAX_MS {
            cap_ms = V6_AEROBIC_CRUISE_MAX_MS;
        }
        if cap_ms > 0.05 && cap_ms < V6_RUN_GAIT_FLOOR_MS {
            cap_ms = V6_RUN_GAIT_FLOOR_MS;
        }
    }
    cap_ms
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
    available_power_watts: f64,
    applied_speed_limit_ms: f64,
) -> f64 {
    let mut engine_base_ms = engine_base_ms;
    if engine_base_ms <= 0.05 {
        engine_base_ms = 5.5;
    }
    // 功率判定用意图/本帧限速，禁止用 v_meas 追着压
    let mut speed_for_eval = applied_speed_multiplier * engine_base_ms;
    if applied_speed_limit_ms > 0.05 {
        speed_for_eval = applied_speed_limit_ms;
    }
    let cap_ms = get_metabolic_speed_cap_ms(
        current_speed_ms,
        movement_phase,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        is_exhausted,
        effective_cp_watts,
        w_prime_pool01,
        available_power_watts,
        speed_for_eval,
    );
    if cap_ms < 0.0 {
        return applied_speed_multiplier;
    }
    let applied_ms = applied_speed_multiplier * engine_base_ms;
    if applied_ms <= cap_ms + 0.01 {
        return applied_speed_multiplier;
    }
    clip_f64(cap_ms / engine_base_ms, 0.01, 3.0)
}
