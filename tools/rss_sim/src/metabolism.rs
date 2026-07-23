use crate::constants::{
    LCDA_GRADE_BASE_A, LCDA_GRADE_BASE_B, LCDA_GRADE_COEFF, LCDA_GRADE_OFFSET, LCDA_LOAD_COEFF,
    LCDA_LOAD_EXP, LCDA_MAX_SPEED_MS, LCDA_REST_W_PER_KG, LCDA_SPEED_FRAC_COEFF,
    LCDA_SPEED_FRAC_EXP, LCDA_SPEED_QUARTIC_COEFF, LCDA_STAND_NET_W_PER_KG, TOBLER_W_AT_FLAT_KMH,
    V6_ACSM_BLEND_END_MS, V6_ACSM_BLEND_START_MS, V6_ACSM_LINEAR_W_PER_MS, V6_ACSM_QUAD_W_PER_MS2,
    V6_ACSM_REST_W, V6_CP_FATIGUE_K, V6_CP_LOAD_DECAY_PER_KG, V6_CP_LOAD_REF_KG, V6_CP_SLOPE_K_UP,
    V6_INVERT_SPEED_MAX_MS,
};
use crate::math::clip_f64;

pub fn tobler_speed_multiplier(
    angle_deg: f64,
    uphill_boost: f64,
    downhill_boost: f64,
    downhill_max: f64,
    dampening: f64,
    min_mult: f64,
) -> f64 {
    let mut s = angle_deg.to_radians().tan();
    s = clip_f64(s, -1.0, 1.0);
    let w_kmh = 6.0 * (-3.5 * (s + 0.05).abs()).exp();
    let mut mult = w_kmh / TOBLER_W_AT_FLAT_KMH;
    mult = mult.max(min_mult);
    if angle_deg > 0.0 {
        mult *= uphill_boost;
    } else if angle_deg < 0.0 {
        mult *= downhill_boost;
        mult = mult.min(downhill_max);
    }
    mult = 1.0 + dampening * (mult - 1.0);
    clip_f64(mult, min_mult, downhill_max)
}

pub fn calculate_slope_adjusted_target_speed(
    base_target_speed_ms: f64,
    slope_angle_degrees: f64,
    uphill_boost: f64,
    downhill_boost: f64,
    downhill_max: f64,
) -> f64 {
    let mut s = slope_angle_degrees.to_radians().tan();
    s = clip_f64(s, -1.0, 1.0);
    let w_kmh = 6.0 * (-3.5 * (s + 0.05).abs()).exp();
    let mut tobler_mult = w_kmh / TOBLER_W_AT_FLAT_KMH;
    tobler_mult = tobler_mult.max(0.15);
    if slope_angle_degrees > 0.0 {
        tobler_mult *= uphill_boost;
    } else if slope_angle_degrees < 0.0 {
        tobler_mult *= downhill_boost;
        tobler_mult = tobler_mult.min(downhill_max);
    }
    tobler_mult = 1.0 + 0.7 * (tobler_mult - 1.0);
    tobler_mult = clip_f64(tobler_mult, 0.15, downhill_max);
    base_target_speed_ms * tobler_mult
}

pub fn calculate_lcda_backpack_power_watts(
    velocity_ms: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
) -> f64 {
    let body_kg = 90.0_f64.max(1.0);
    let w = total_weight_kg.max(0.0);
    let terrain_factor = clip_f64(terrain_factor, 0.5, 3.0);
    let load_kg = (w - body_kg).max(0.0);
    let load_ratio = load_kg / body_kg;
    let load_mult = 1.0 + LCDA_LOAD_COEFF * load_ratio.powf(LCDA_LOAD_EXP);

    let speed_ms = velocity_ms.max(0.0);
    let mut walk_net = 0.0;
    if speed_ms >= 0.1 {
        let frac_term = LCDA_SPEED_FRAC_COEFF * speed_ms.powf(LCDA_SPEED_FRAC_EXP);
        let quartic_term = LCDA_SPEED_QUARTIC_COEFF * speed_ms.powi(4);
        walk_net = terrain_factor * (frac_term + quartic_term);
    }

    let grade_decimal = grade_percent * 0.01;
    let mut grade_term = 0.0;
    if speed_ms >= 0.1 && grade_decimal.abs() > 0.0001 {
        let grade_exp = 100.0 * grade_decimal + LCDA_GRADE_OFFSET;
        let inner = LCDA_GRADE_BASE_B.powf(grade_exp);
        let outer = LCDA_GRADE_BASE_A.powf(1.0 - inner);
        grade_term = LCDA_GRADE_COEFF * speed_ms * grade_decimal * (1.0 - outer);
    }

    let m_per_kg =
        LCDA_REST_W_PER_KG + (LCDA_STAND_NET_W_PER_KG + walk_net + grade_term) * load_mult;
    (m_per_kg * body_kg).max(0.0)
}

pub fn calculate_pandolf_power_watts(
    velocity_ms: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
) -> f64 {
    let v = velocity_ms.max(0.0);
    let w = total_weight_kg.max(0.0);
    let terrain_factor = clip_f64(terrain_factor, 0.5, 3.0);
    let ref_w = 90.0;

    if v < 0.1 {
        let body_w = ref_w;
        let load_w = (w - body_w).max(0.0);
        if load_w < 5.0 {
            return 100.0 * (w / ref_w);
        }
        let base_static = 1.2 * body_w;
        let load_ratio = if body_w > 0.0 { load_w / body_w } else { 0.0 };
        let load_static = 1.6 * (body_w + load_w) * (load_ratio * load_ratio);
        return (base_static + load_static).max(0.0);
    }

    let vt = v - 0.7;
    let base_term = (2.7 * 0.80) + (3.2 * vt * vt);
    let g = grade_percent * 0.01;
    let mut grade_term = g * (0.23 + 1.34 * v * v);
    grade_term = grade_term.min(base_term * 3.0);
    if grade_percent < 0.0 && grade_percent > -12.0 {
        grade_term *= 1.25;
    }
    let mut steep_penalty = 0.0;
    if grade_percent < -15.0 {
        let abs_g = grade_percent.abs();
        let ramp = ((abs_g - 15.0) / 15.0).min(1.0);
        steep_penalty = base_term * ramp * 0.5;
    }
    let w_mult = (w / ref_w).max(0.1);
    (w_mult * (base_term + grade_term + steep_penalty) * terrain_factor * ref_w).max(0.0)
}

pub fn calculate_acsm_power_watts(velocity_ms: f64, total_weight_kg: f64) -> f64 {
    let v = velocity_ms.max(0.0);
    let w = total_weight_kg.max(45.0);
    let mass_scale = w / 90.0;
    let pref = V6_ACSM_REST_W + V6_ACSM_LINEAR_W_PER_MS * v + V6_ACSM_QUAD_W_PER_MS2 * v * v;
    (pref * mass_scale).max(0.0)
}

pub fn get_acsm_blend_weight(velocity_ms: f64) -> f64 {
    if velocity_ms <= V6_ACSM_BLEND_START_MS {
        return 0.0;
    }
    if velocity_ms >= V6_ACSM_BLEND_END_MS {
        return 1.0;
    }
    (velocity_ms - V6_ACSM_BLEND_START_MS) / (V6_ACSM_BLEND_END_MS - V6_ACSM_BLEND_START_MS)
}

pub fn metabolism_power_watts(
    velocity_ms: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
) -> f64 {
    metabolism_power_watts_damped(
        velocity_ms,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
        0.70,
    )
}

pub fn metabolism_power_watts_damped(
    velocity_ms: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
    load_metabolic_dampening: f64,
) -> f64 {
    let prefer_acsm =
        movement_phase == 2 || movement_phase == 3 || velocity_ms >= V6_ACSM_BLEND_END_MS;
    let blended = if !prefer_acsm && velocity_ms <= LCDA_MAX_SPEED_MS {
        calculate_lcda_backpack_power_watts(
            velocity_ms,
            total_weight_kg,
            grade_percent,
            terrain_factor,
        )
    } else {
        let pandolf_w =
            calculate_pandolf_power_watts(velocity_ms, total_weight_kg, grade_percent, terrain_factor);
        if !prefer_acsm && velocity_ms < V6_ACSM_BLEND_START_MS {
            pandolf_w
        } else {
            let acsm_w = calculate_acsm_power_watts(velocity_ms, total_weight_kg);
            let mut blend = get_acsm_blend_weight(velocity_ms);
            if movement_phase == 3 {
                blend = blend.max(0.85);
            }
            pandolf_w * (1.0 - blend) + acsm_w * blend
        }
    };

    let body_weight = 90.0;
    if total_weight_kg <= body_weight + 0.5 || load_metabolic_dampening >= 1.0 {
        return blended;
    }
    let unloaded = metabolism_power_watts_damped(
        velocity_ms,
        body_weight,
        grade_percent,
        terrain_factor,
        movement_phase,
        1.0,
    );
    let load_extra = (blended - unloaded).max(0.0);
    unloaded + load_extra * load_metabolic_dampening
}

pub fn invert_speed_for_power_watts(
    target_power_watts: f64,
    total_weight_kg: f64,
    grade_percent: f64,
    terrain_factor: f64,
    movement_phase: i32,
) -> f64 {
    if target_power_watts <= 1.0 {
        return 0.0;
    }
    let mut lo = 0.0;
    let mut hi = V6_INVERT_SPEED_MAX_MS;
    for _ in 0..24 {
        let mid = (lo + hi) * 0.5;
        let p = metabolism_power_watts(mid, total_weight_kg, grade_percent, terrain_factor, movement_phase);
        if p > target_power_watts {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    (lo + hi) * 0.5
}

pub fn compute_cp_watts(
    cp0: f64,
    load_kg: f64,
    grade_percent: f64,
    env_mult: f64,
    fatigue_norm: f64,
) -> f64 {
    let excess = (load_kg - V6_CP_LOAD_REF_KG).max(0.0);
    let mut cp = cp0 * (1.0 - V6_CP_LOAD_DECAY_PER_KG * excess);
    let g = grade_percent * 0.01;
    if g > 0.0 {
        cp *= (1.0 - V6_CP_SLOPE_K_UP * g * g).max(0.65);
    }
    cp *= clip_f64(env_mult, 0.55, 1.0);
    cp *= (1.0 - V6_CP_FATIGUE_K * fatigue_norm).max(0.75);
    cp
}
