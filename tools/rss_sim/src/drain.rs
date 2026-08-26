use crate::constants::{
    ENGINE_WALK_TOP_MS, LOADED_RUN_DRAIN_MAX_MULT, LOADED_RUN_DRAIN_REF_KG,
    LOADED_RUN_DRAIN_START_KG, MOVEMENT_RUN,
    MOVEMENT_SPRINT, MOVEMENT_WALK, V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V5_WALK_SPEED_MS_DEFAULT,
    V6_AEROBIC_CRUISE_MAX_MS, V6_APPLY_HORIZONTAL_SPEED_CLAMP, V6_CP_HIKE_FLOOR_MS,
    V6_CP_INVERT_GRADE_ABS_MAX_PCT, V6_CP_INVERT_TERRAIN_MAX, V6_OVERSPEED_ACCOUNTING_EPS_MPS,
    V6_GAIT_EXCESS_STA_TAX_MAX_PER_SEC, V6_GAIT_EXCESS_STA_TAX_MULT, V6_GAIT_SPEED_LIMIT_MIN_FRAC,
    V6_OVERSPEED_STA_TAX_MULT, V6_RUN_GAIT_DEMOTE_TO_WALK, V6_RUN_GAIT_FLOOR_MS,
    V6_RUN_SOFT_BAND_BELOW_FLOOR_MS, V6_STAMINA_DRAIN_CALIBRATION,
    V6_WALK_OVERRIDE_IN_BAND_SLACK_MS,
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

pub fn is_walk_override_in_band_cruise(cp_walk_override_active: bool, measured_ms: f64) -> bool {
    if !cp_walk_override_active {
        return false;
    }
    let ceiling = ENGINE_WALK_TOP_MS + V6_WALK_OVERRIDE_IN_BAND_SLACK_MS;
    if measured_ms <= ceiling {
        return true;
    }
    false
}

pub fn is_phys_overspeed_for_anaerobic_tick(
    measured_ms: f64,
    applied_limit_ms: f64,
    cp_walk_override_active: bool,
) -> bool {
    if is_walk_override_in_band_cruise(cp_walk_override_active, measured_ms) {
        return false;
    }
    is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms)
}

fn loaded_gait_stamina_drain_multiplier(load_weight_kg: f64, movement_phase: i32) -> f64 {
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
    let t = clip_f64(
        (load_weight_kg - LOADED_RUN_DRAIN_START_KG) / span,
        0.0,
        1.0,
    );
    1.0 + (LOADED_RUN_DRAIN_MAX_MULT - 1.0) * t
}

pub fn invert_cruise_cap_ms(
    critical_power_watts: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
) -> f64 {
    if critical_power_watts <= 1.0 {
        return 0.0;
    }
    let grade_lim = V6_CP_INVERT_GRADE_ABS_MAX_PCT;
    let mut grade_for_invert = grade_percent;
    if grade_for_invert > grade_lim {
        grade_for_invert = grade_lim;
    }
    if grade_for_invert < -grade_lim {
        grade_for_invert = -grade_lim;
    }
    let mut terrain_for_invert = terrain_factor;
    if terrain_for_invert > V6_CP_INVERT_TERRAIN_MAX {
        terrain_for_invert = V6_CP_INVERT_TERRAIN_MAX;
    }
    if terrain_for_invert < 0.5 {
        terrain_for_invert = 0.5;
    }
    let mut cap_ms = invert_speed_for_power_watts(
        critical_power_watts,
        total_weight_kg,
        grade_for_invert,
        terrain_for_invert,
        movement_phase,
    );
    if cap_ms < V6_CP_HIKE_FLOOR_MS {
        cap_ms = V6_CP_HIKE_FLOOR_MS;
    }
    cap_ms
}

/// Align with SCR_RSS_DrainCalculator.ResolveRunCruiseCapMs.
pub fn resolve_run_cruise_cap_ms(
    raw_cap_ms: f64,
    movement_phase: i32,
    grade_percent: f64,
    _total_weight_kg: f64,
    _terrain_factor: f64,
    _critical_power_watts: f64,
) -> f64 {
    if movement_phase == MOVEMENT_WALK {
        return raw_cap_ms;
    }
    if raw_cap_ms <= 0.05 {
        return raw_cap_ms;
    }

    let mut cap_ms = raw_cap_ms;
    let cruise_max = V6_AEROBIC_CRUISE_MAX_MS;
    let floor_ms = V6_RUN_GAIT_FLOOR_MS;
    let demote = V6_RUN_GAIT_DEMOTE_TO_WALK;
    let soft_band = V6_RUN_SOFT_BAND_BELOW_FLOOR_MS;
    let horiz_clamp = V6_APPLY_HORIZONTAL_SPEED_CLAMP;
    let walk_top = V5_WALK_SPEED_MS_DEFAULT;

    if grade_percent >= 0.0 && cap_ms > cruise_max {
        cap_ms = cruise_max;
    }

    if cap_ms >= floor_ms {
        return cap_ms;
    }

    if !demote || horiz_clamp {
        return floor_ms;
    }

    // Near-floor soft Run only; out of band: skip cap (do not press Walk onto Run).
    let mut soft_run_floor = floor_ms - soft_band;
    if soft_run_floor < walk_top {
        soft_run_floor = walk_top;
    }
    if cap_ms >= soft_run_floor {
        return cap_ms;
    }

    -1.0
}

pub fn get_client_overspeed_excess_drain_per_second(
    measured_ms: f64,
    applied_limit_ms: f64,
    w_prime_pool01: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
    effective_critical_power_watts: f64,
    w_prime_overspeed_armed: bool,
    energy_to_stamina_coeff: f64,
    character_weight_kg: f64,
    cp_walk_override_active: bool,
) -> f64 {
    use crate::constants::V6_APPLY_CP_METABOLIC_SPEED_CAP;

    if is_walk_override_in_band_cruise(cp_walk_override_active, measured_ms) {
        return 0.0;
    }

    let p_meas = metabolism_power_watts(
        measured_ms,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    );

    let mut armed = w_prime_overspeed_armed;
    if !armed {
        armed = is_wprime_pool_available_for_overspeed(
            w_prime_pool01,
            V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
        );
    }

    let mut use_gait_excess_tax = false;
    let unpaid_w;
    if !V6_APPLY_CP_METABOLIC_SPEED_CAP {
        if armed {
            return 0.0;
        }
        if effective_critical_power_watts <= 1.0 {
            return 0.0;
        }
        unpaid_w = p_meas - effective_critical_power_watts;
        use_gait_excess_tax = true;
    } else if armed {
        if !is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms) {
            return 0.0;
        }
        if effective_critical_power_watts > 1.0
            && p_meas > effective_critical_power_watts + 1.0
        {
            return 0.0;
        }
        let p_limit = metabolism_power_watts(
            applied_limit_ms,
            total_weight_kg,
            grade_percent,
            terrain_factor,
            movement_phase,
        );
        unpaid_w = p_meas - p_limit;
    } else if is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms)
        && movement_phase < 2
    {
        let p_limit = metabolism_power_watts(
            applied_limit_ms,
            total_weight_kg,
            grade_percent,
            terrain_factor,
            movement_phase,
        );
        unpaid_w = p_meas - p_limit;
    } else {
        if effective_critical_power_watts <= 1.0 {
            return 0.0;
        }
        unpaid_w = p_meas - effective_critical_power_watts;
        use_gait_excess_tax = true;
    }
    if unpaid_w <= 1.0 {
        return 0.0;
    }

    let mut per_sec = stamina_drain_rate_per_second_from_power_watts(
        unpaid_w,
        -1.0,
        energy_to_stamina_coeff,
    );
    let load_kg = (total_weight_kg - character_weight_kg).max(0.0);
    per_sec *= loaded_gait_stamina_drain_multiplier(load_kg, movement_phase);
    let mut tax_mult = V6_OVERSPEED_STA_TAX_MULT;
    if use_gait_excess_tax && movement_phase >= 2 {
        tax_mult = V6_GAIT_EXCESS_STA_TAX_MULT;
    } else if !V6_APPLY_CP_METABOLIC_SPEED_CAP && movement_phase < 2 {
        tax_mult = crate::constants::V6_CP_EXCESS_STA_TAX_MULT;
    }
    per_sec *= tax_mult;
    if use_gait_excess_tax && movement_phase >= 2 {
        let tax_max = V6_GAIT_EXCESS_STA_TAX_MAX_PER_SEC;
        if per_sec > tax_max {
            per_sec = tax_max;
        }
    }
    per_sec
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
    use crate::constants::V6_APPLY_CP_METABOLIC_SPEED_CAP;
    if !V6_APPLY_CP_METABOLIC_SPEED_CAP {
        return -1.0;
    }
    if is_exhausted {
        return -1.0;
    }
    if movement_phase < 1 {
        return -1.0;
    }

    let mut is_sprint_phase = movement_phase == MOVEMENT_SPRINT;
    let armed = is_wprime_pool_available_for_overspeed(
        w_prime_pool01,
        V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
    );
    // W′ 武装纯 Run：勿再压回 2.0~2.4
    if armed && movement_phase == MOVEMENT_RUN {
        return -1.0;
    }
    if !armed {
        is_sprint_phase = false;
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
    if is_sprint_phase && available_power_watts >= 0.0 {
        available_p = available_power_watts;
    }

    if power_w <= available_p + 1.0 {
        if !is_sprint_phase && movement_phase != MOVEMENT_WALK && grade_percent >= 0.0 {
            let cruise_only = resolve_run_cruise_cap_ms(
                V6_AEROBIC_CRUISE_MAX_MS,
                MOVEMENT_RUN,
                grade_percent,
                total_weight_kg,
                terrain_factor,
                effective_cp_watts,
            );
            if cruise_only > 0.05 && eval_speed > cruise_only + 0.05 {
                return cruise_only;
            }
        }
        return -1.0;
    }

    let mut target_p = available_p;
    if power_w > effective_cp_watts && !is_sprint_phase {
        target_p = effective_cp_watts;
    }

    let mut invert_phase = movement_phase;
    if !is_sprint_phase {
        invert_phase = MOVEMENT_RUN;
        if movement_phase == MOVEMENT_WALK {
            invert_phase = MOVEMENT_WALK;
        }
    }

    let mut cap_ms = invert_cruise_cap_ms(
        target_p,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        invert_phase,
    );
    if !is_sprint_phase && invert_phase != MOVEMENT_WALK {
        cap_ms = resolve_run_cruise_cap_ms(
            cap_ms,
            invert_phase,
            grade_percent,
            total_weight_kg,
            terrain_factor,
            effective_cp_watts,
        );
    }
    cap_ms
}

pub fn clamp_speed_limit_fraction_to_gait_band(frac: f64, _is_exhausted: bool) -> f64 {
    let mut min_frac = V6_GAIT_SPEED_LIMIT_MIN_FRAC;
    if min_frac < 0.2 {
        min_frac = 0.2;
    }
    if min_frac > 0.9 {
        min_frac = 0.9;
    }
    if frac < min_frac {
        return min_frac;
    }
    frac
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
        return clamp_speed_limit_fraction_to_gait_band(applied_speed_multiplier, is_exhausted);
    }
    let applied_ms = applied_speed_multiplier * engine_base_ms;
    let mut next_frac = applied_speed_multiplier;
    if applied_ms > cap_ms + 0.01 {
        next_frac = clip_f64(cap_ms / engine_base_ms, 0.01, 3.0);
    }
    clamp_speed_limit_fraction_to_gait_band(next_frac, is_exhausted)
}
