use crate::constants::{
    merge_game_aligned_params, RssConstants, MOVEMENT_RUN, MOVEMENT_SPRINT, MOVEMENT_WALK,
    RSS_PLAYER_TICK_SEC, STANCE_STAND,
};
use crate::cp_wprime::{simulate_v6_sprint_seconds, V5AnaerobicState};
use crate::drain::{get_drain_velocity_ms, get_metabolic_overspeed_factor};
use crate::mission::{simulate_mission, Mission, Phase};
use crate::twin::RSSDigitalTwin;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

pub const MARCH_4H_AEROBIC_MIN: f64 = 0.20;
pub const MARCH_4H_HARD: bool = true;
pub const SPRINT_BURST_MAX_SEC: f64 = 15.0;
pub const SPRINT_COOLDOWN_MIN_SEC: f64 = 120.0;
pub const DRAIN_VEL_TOLERANCE: f64 = 0.001;
pub const OVERSPEED_EXPECTED: f64 = 0.5;
pub const SUSTAIN_OBS_MIN_PCT_PER_S: f64 = 0.40;
pub const SUSTAIN_OBS_MAX_PCT_PER_S: f64 = 2.60;
pub const SUSTAIN_OBS_HARD: bool = true;

pub const MOBILITY_RUN_0KG_MIN_MS: f64 = 2.65;
pub const MOBILITY_RUN_0KG_MAX_MS: f64 = 2.95;
pub const MOBILITY_RUN_35KG_MIN_MS: f64 = 2.15;
pub const MOBILITY_RUN_35KG_MAX_MS: f64 = 2.85;
pub const MOBILITY_HARD: bool = true;

pub const TWO_MILE_DIST_M: f64 = 2.0 * 1609.344;
pub const TWO_MILE_SCORE_70_SEC: f64 = 18.0 * 60.0;
pub const TWO_MILE_SCORE_85_SEC: f64 = 15.0 * 60.0 + 30.0;
pub const TWO_MILE_SCORE_70: f64 = 0.70;
pub const TWO_MILE_SCORE_85: f64 = 0.85;
pub const TWO_MILE_MAX_SEC: f64 = TWO_MILE_SCORE_70_SEC;
pub const TWO_MILE_TIMEOUT_SEC: f64 = 1800.0;
pub const TWO_MILE_HARD: bool = true;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConstraintCheck {
    pub name: String,
    pub passed: bool,
    pub detail: String,
    pub hard: bool,
    #[serde(default)]
    pub margin: f64,
    #[serde(default)]
    pub hint: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConstraintReport {
    pub checks: Vec<ConstraintCheck>,
    pub all_hard_passed: bool,
}

fn elite_preset_path() -> PathBuf {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let tools_dir = manifest
        .parent()
        .map(|p| p.to_path_buf())
        .unwrap_or(manifest);
    tools_dir.join("optimized_rss_config_elitestandard_v4.json")
}

pub fn load_elite_preset_params() -> HashMap<String, f64> {
    let path = elite_preset_path();
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(_) => return HashMap::new(),
    };
    let parsed = match serde_json::from_str::<serde_json::Value>(&content) {
        Ok(v) => v,
        Err(_) => return HashMap::new(),
    };
    let mut out = HashMap::new();
    if let Some(obj) = parsed.as_object() {
        for (k, v) in obj {
            if k.starts_with('_') {
                continue;
            }
            if let Some(f) = v.as_f64() {
                out.insert(k.clone(), f);
            }
        }
    }
    out
}

pub fn check_drain_velocity_contract() -> ConstraintCheck {
    let v = get_drain_velocity_ms(5.5, 4.0);
    let ok = (v - 5.5).abs() <= DRAIN_VEL_TOLERANCE;
    ConstraintCheck {
        name: "drain_velocity_meas".to_string(),
        passed: ok,
        detail: format!("expected 5.5 m/s (v_meas), got {:.4}", v),
        hard: true,
        margin: 5.5 - (v - 5.5).abs(),
        hint: String::new(),
    }
}

pub fn check_downhill_same_speed_savings() -> ConstraintCheck {
    use crate::metabolism::{calculate_acsm_power_watts, metabolism_power_watts};
    let total_w = 121.0;
    let v = 3.25;
    let p_flat = metabolism_power_watts(v, total_w, 0.0, 1.0, 1);
    let p_down = metabolism_power_watts(v, total_w, -5.0, 1.0, 1);
    let p_acsm = calculate_acsm_power_watts(v, total_w);
    let ok_save = p_down < p_flat * 0.92;
    let ok_scale = p_down < 1500.0;
    let ok_vs_acsm = p_down < p_acsm * 0.75;
    let ok = ok_save && ok_scale && ok_vs_acsm && p_flat > 200.0;
    make_check(
        "downhill_same_speed_savings",
        ok,
        format!(
            "Walk@3.25: flat={:.0}W down5%={:.0}W (need down<0.92*flat, <1500W, <0.75*ACSM)",
            p_flat, p_down
        ),
        true,
        p_flat * 0.92 - p_down,
        String::new(),
    )
}

fn make_check(
    name: &str,
    passed: bool,
    detail: String,
    hard: bool,
    margin: f64,
    hint: String,
) -> ConstraintCheck {
    ConstraintCheck {
        name: name.to_string(),
        passed,
        detail,
        hard,
        margin,
        hint,
    }
}

pub fn check_metabolic_overspeed_contract() -> ConstraintCheck {
    let f = get_metabolic_overspeed_factor(800.0, 400.0, 0.35);
    let ok = (f - OVERSPEED_EXPECTED).abs() <= DRAIN_VEL_TOLERANCE;
    make_check(
        "metabolic_overspeed_factor",
        ok,
        format!("expected {}, got {:.4} (v5 legacy API)", OVERSPEED_EXPECTED, f),
        false,
        OVERSPEED_EXPECTED - (f - OVERSPEED_EXPECTED).abs(),
        String::new(),
    )
}

pub fn check_v5_sprint_burst_duration() -> ConstraintCheck {
    let mut ana = V5AnaerobicState::default();
    let mut t = 0.0;
    while ana.pool > 0.0 && t < 20.0 {
        ana.tick_sprint(0.2, 0.12);
        t += 0.2;
    }
    let ok = t <= SPRINT_BURST_MAX_SEC;
    make_check(
        "v5_sprint_burst_duration",
        ok,
        format!("empty pool in {:.1}s (max {}s)", t, SPRINT_BURST_MAX_SEC),
        true,
        SPRINT_BURST_MAX_SEC - t,
        String::new(),
    )
}

pub fn check_v5_sprint_cooldown() -> ConstraintCheck {
    let ana = V5AnaerobicState {
        pool: 0.0,
        cooldown_until_sec: 180.0,
    };
    let rem = ana.cooldown_remaining(0.0);
    let ok = rem >= SPRINT_COOLDOWN_MIN_SEC;
    make_check(
        "v5_sprint_cooldown",
        ok,
        format!("cooldown remaining {:.0}s (min {}s)", rem, SPRINT_COOLDOWN_MIN_SEC),
        true,
        rem - SPRINT_COOLDOWN_MIN_SEC,
        String::new(),
    )
}

pub fn check_v6_cp_sprint_burst(
    load_kg: f64,
    cp0: f64,
    w_prime_max: f64,
    sprint_cap_w: f64,
) -> ConstraintCheck {
    let t = simulate_v6_sprint_seconds(load_kg, cp0, 0.017, sprint_cap_w, w_prime_max);
    let ok = t <= SPRINT_BURST_MAX_SEC;
    let mut hint = String::new();
    if !ok {
        hint = format!(
            "lower sprint_power_cap_watts or w_prime_max_joules (burst {:.1}s > {:.0}s)",
            t, SPRINT_BURST_MAX_SEC
        );
    }
    make_check(
        "v6_cp_sprint_burst_35kg",
        ok,
        format!("W' burst {:.2}s (max {}s)", t, SPRINT_BURST_MAX_SEC),
        true,
        SPRINT_BURST_MAX_SEC - t,
        hint,
    )
}

pub fn simulate_ideal_march_aerobic_end(
    hours: f64,
    encumbrance_kg: f64,
    dt_sec: f64,
    params: Option<&HashMap<String, f64>>,
) -> f64 {
    let constants = if let Some(p) = params {
        let merged = merge_game_aligned_params(p);
        RssConstants::from_params(&merged)
    } else {
        let preset = load_elite_preset_params();
        if preset.is_empty() {
            RssConstants::default()
        } else {
            let merged = merge_game_aligned_params(&preset);
            RssConstants::from_params(&merged)
        }
    };

    let mut twin = RSSDigitalTwin::new(constants.clone());
    twin.reset();

    let total_weight = constants.character_weight + encumbrance_kg;
    let end_sec = hours * 3600.0;
    let mut t = 0.0;
    while t < end_sec {
        twin.game_player_tick(
            MOVEMENT_WALK,
            total_weight,
            0.0,
            1.0,
            STANCE_STAND,
            t,
            dt_sec,
            0.0,
            false,
        );
        t += dt_sec;
    }
    twin.stamina
}

pub fn check_sustain_run_observed(
    params: Option<&HashMap<String, f64>>,
    duration_s: f64,
    fast_mode: bool,
) -> ConstraintCheck {
    let mut merged = merge_game_aligned_params(&load_elite_preset_params());
    if let Some(p) = params {
        for (k, v) in p {
            if !k.starts_with('_') {
                merged.insert(k.clone(), *v);
            }
        }
    }
    let constants = RssConstants::from_params(&merged);
    let mut twin = RSSDigitalTwin::new(constants);
    let mission = Mission {
        name: "35kg稳态跑步".to_string(),
        load_kg: 35.0,
        phases: vec![Phase {
            duration_s,
            speed_ms: 0.0,
            movement: MOVEMENT_RUN,
            stance: 0,
            grade_pct: 0.0,
            terrain: 1.0,
            label: "稳态Run".to_string(),
        }],
        description: String::new(),
        temperature: 20.0,
        wind_speed: 0.0,
    };
    let result = simulate_mission(&mut twin, &mission, fast_mode, true);
    let obs = result.observed_depletion_pct_per_s;
    let mid = 0.5 * (SUSTAIN_OBS_MIN_PCT_PER_S + SUSTAIN_OBS_MAX_PCT_PER_S);
    let half = 0.5 * (SUSTAIN_OBS_MAX_PCT_PER_S - SUSTAIN_OBS_MIN_PCT_PER_S);
    let margin = half - (obs - mid).abs();
    let ok = (SUSTAIN_OBS_MIN_PCT_PER_S..=SUSTAIN_OBS_MAX_PCT_PER_S).contains(&obs);
    let mut hint = String::new();
    if !ok {
        if obs < SUSTAIN_OBS_MIN_PCT_PER_S {
            hint = "raise energy_to_stamina_coeff or lower critical_power_watts".to_string();
        } else {
            hint = "lower energy_to_stamina_coeff or raise critical_power_watts".to_string();
        }
    }
    if !SUSTAIN_OBS_HARD && !ok {
        return make_check(
            "sustain_run_observed_35kg",
            true,
            format!(
                "obs={:.3}%/s outside [{}, {}] (soft)",
                obs, SUSTAIN_OBS_MIN_PCT_PER_S, SUSTAIN_OBS_MAX_PCT_PER_S
            ),
            false,
            margin,
            hint,
        );
    }
    make_check(
        "sustain_run_observed_35kg",
        ok,
        format!(
            "obs={:.3}%/s (band {}–{})",
            obs, SUSTAIN_OBS_MIN_PCT_PER_S, SUSTAIN_OBS_MAX_PCT_PER_S
        ),
        SUSTAIN_OBS_HARD,
        margin,
        hint,
    )
}

pub fn check_march_4h_aerobic_end(
    encumbrance_kg: f64,
    hours: f64,
    params: Option<&HashMap<String, f64>>,
) -> ConstraintCheck {
    let aerobic_end = simulate_ideal_march_aerobic_end(hours, encumbrance_kg, 2.0, params);
    let ok = aerobic_end >= MARCH_4H_AEROBIC_MIN;
    let margin = aerobic_end - MARCH_4H_AEROBIC_MIN;
    let mut hint = String::new();
    if !ok {
        hint = "lower energy_to_stamina_coeff".to_string();
    }
    if !MARCH_4H_HARD && !ok {
        return make_check(
            "march_4h_aerobic_end_35kg",
            true,
            format!(
                "aerobic_end={:.3} < {} (soft; coeff calibration pending)",
                aerobic_end, MARCH_4H_AEROBIC_MIN
            ),
            false,
            margin,
            hint,
        );
    }
    make_check(
        "march_4h_aerobic_end_35kg",
        ok,
        format!("aerobic_end={:.3} (min {})", aerobic_end, MARCH_4H_AEROBIC_MIN),
        MARCH_4H_HARD,
        margin,
        hint,
    )
}

pub fn two_mile_score_01(time_s: f64) -> f64 {
    let denom = TWO_MILE_SCORE_85_SEC - TWO_MILE_SCORE_70_SEC;
    if denom.abs() < 1e-9 {
        return TWO_MILE_SCORE_70;
    }
    TWO_MILE_SCORE_70
        + (time_s - TWO_MILE_SCORE_70_SEC) * (TWO_MILE_SCORE_85 - TWO_MILE_SCORE_70) / denom
}

pub fn two_mile_ease_from_time(time_s: f64) -> f64 {
    (TWO_MILE_SCORE_85 - two_mile_score_01(time_s)).max(0.0)
}

pub fn simulate_zero_load_run_time_to_distance(
    distance_m: f64,
    params: Option<&HashMap<String, f64>>,
    timeout_s: f64,
) -> (f64, f64) {
    let mut merged = merge_game_aligned_params(&load_elite_preset_params());
    if let Some(p) = params {
        for (k, v) in p {
            if !k.starts_with('_') {
                merged.insert(k.clone(), *v);
            }
        }
    }
    let constants = RssConstants::from_params(&merged);
    let body_kg = constants.character_weight;
    let mut twin = RSSDigitalTwin::new(constants);
    twin.reset();
    let dt = RSS_PLAYER_TICK_SEC;
    let mut dist = 0.0;
    let mut t = 0.0;
    while t < timeout_s && dist < distance_m {
        twin.game_player_tick(
            MOVEMENT_SPRINT,
            body_kg,
            0.0,
            1.0,
            STANCE_STAND,
            t,
            dt,
            0.0,
            false,
        );
        dist += twin.measured_velocity_ms * dt;
        t += dt;
    }
    (t, dist)
}

pub fn check_zero_load_run_2mile(params: Option<&HashMap<String, f64>>) -> ConstraintCheck {
    let (time_s, dist_m) =
        simulate_zero_load_run_time_to_distance(TWO_MILE_DIST_M, params, TWO_MILE_TIMEOUT_SEC);
    let finished = dist_m + 1e-6 >= TWO_MILE_DIST_M;
    let ok = finished && time_s <= TWO_MILE_MAX_SEC;
    let mut margin = TWO_MILE_MAX_SEC - time_s;
    if !finished {
        margin = TWO_MILE_MAX_SEC - TWO_MILE_TIMEOUT_SEC;
    }
    let score = if finished {
        two_mile_score_01(time_s)
    } else {
        0.0
    };
    let mut hint = String::new();
    if !ok {
        hint = "raise critical_power_watts / w_prime_max_joules or sprint_power_cap_watts so 2mi Sprint finishes by 18:00 (70%)"
            .to_string();
    }
    let minutes = (time_s / 60.0).floor() as i64;
    let seconds = time_s - 60.0 * (minutes as f64);
    let detail = if finished {
        format!(
            "time={}:{:05.2} score={:.1}% (hard≤18:00/70%, soft 15:30/85%, dist={:.1}m)",
            minutes,
            seconds,
            score * 100.0,
            dist_m
        )
    } else {
        format!(
            "timeout at {:.1}m / {:.1}m after {:.0}s",
            dist_m, TWO_MILE_DIST_M, TWO_MILE_TIMEOUT_SEC
        )
    };
    let check_name = "zero_load_2mile_pt_ge70";
    if !TWO_MILE_HARD && !ok {
        return make_check(
            check_name,
            true,
            format!("{} (soft)", detail),
            false,
            margin,
            hint,
        );
    }
    make_check(check_name, ok, detail, TWO_MILE_HARD, margin, hint)
}

pub fn check_mobility_run_speed(
    load_kg: f64,
    min_ms: f64,
    max_ms: f64,
    label: &str,
    params: Option<&HashMap<String, f64>>,
) -> ConstraintCheck {
    let mut merged = merge_game_aligned_params(&load_elite_preset_params());
    if let Some(p) = params {
        for (k, v) in p {
            if !k.starts_with('_') {
                merged.insert(k.clone(), *v);
            }
        }
    }
    let constants = RssConstants::from_params(&merged);
    let mut twin = RSSDigitalTwin::new(constants);
    let total_weight = 90.0 + load_kg;
    let speed_ms = twin.theoretical_speed_at_weight(total_weight, MOVEMENT_RUN, 0.0, 1.0, 1.0);
    let ok = (min_ms..=max_ms).contains(&speed_ms);
    let name = format!("mobility_run_{}", label);
    let mid = 0.5 * (min_ms + max_ms);
    let half = 0.5 * (max_ms - min_ms);
    let margin = half - (speed_ms - mid).abs();
    let mut hint = String::new();
    if !ok {
        if speed_ms < min_ms {
            hint = "lower encumbrance_speed_penalty_coeff".to_string();
        } else {
            hint = "raise encumbrance_speed_penalty_coeff".to_string();
        }
    }
    if !MOBILITY_HARD && !ok {
        return make_check(
            &name,
            true,
            format!(
                "speed={:.3} m/s outside [{}, {}] (soft)",
                speed_ms, min_ms, max_ms
            ),
            false,
            margin,
            hint,
        );
    }
    make_check(
        &name,
        ok,
        format!("speed={:.3} m/s (band {}–{})", speed_ms, min_ms, max_ms),
        MOBILITY_HARD,
        margin,
        hint,
    )
}

pub fn min_cp0_for_march_cruise(
    load_kg: f64,
    speed_ms: f64,
    damp: f64,
    headroom_w: f64,
) -> f64 {
    use crate::metabolism::metabolism_power_watts_damped;

    let total_w = 90.0 + load_kg;
    let p_acct = metabolism_power_watts_damped(
        speed_ms,
        total_w,
        0.0,
        1.0,
        MOVEMENT_WALK,
        damp,
    );
    let excess = (load_kg - crate::constants::V6_CP_LOAD_REF_KG).max(0.0);
    let factor = (1.0 - crate::constants::V6_CP_LOAD_DECAY_PER_KG * excess).max(0.05);
    (p_acct + headroom_w) / factor
}

pub fn check_march_cruise_below_cp(
    load_kg: f64,
    speed_ms: f64,
    params: Option<&HashMap<String, f64>>,
    cp0_default: f64,
) -> ConstraintCheck {
    use crate::metabolism::{compute_cp_watts, metabolism_power_watts_damped};

    let mut cp0 = cp0_default;
    let mut damp = 0.70;
    if let Some(p) = params {
        cp0 = *p.get("critical_power_watts").unwrap_or(&cp0);
        damp = *p.get("load_metabolic_dampening").unwrap_or(&damp);
    }
    let total_w = 90.0 + load_kg;
    let p_acct = metabolism_power_watts_damped(
        speed_ms,
        total_w,
        0.0,
        1.0,
        MOVEMENT_WALK,
        damp,
    );
    let cp_eff = compute_cp_watts(cp0, load_kg, 0.0, 1.0, 0.0);
    let margin = cp_eff - p_acct;
    let ok = p_acct <= cp_eff + 1.0;
    let mut hint = String::new();
    if !ok {
        let need = min_cp0_for_march_cruise(load_kg, speed_ms, damp, 10.0);
        hint = format!(
            "raise critical_power_watts to >= {:.0} (have {:.0})",
            need.ceil(),
            cp0
        );
    }
    make_check(
        "march_cruise_below_cp_38kg_1p7",
        ok,
        format!(
            "P={:.1}W vs CPeff={:.1}W (CP0={:.0})",
            p_acct, cp_eff, cp0
        ),
        true,
        margin,
        hint,
    )
}

pub fn evaluate_physio_anchors(
    load_kg: f64,
    cp0: f64,
    params: Option<&HashMap<String, f64>>,
    fast_mode: bool,
) -> ConstraintReport {
    let mut trial_params: Option<HashMap<String, f64>> = None;
    let mut w_prime_max = 20_000.0;
    let mut sprint_cap_w = 2400.0;
    let mut cp = cp0;

    if let Some(p) = params {
        let mut t = p.clone();
        cp = *p.get("critical_power_watts").unwrap_or(&cp);
        w_prime_max = *p.get("w_prime_max_joules").unwrap_or(&w_prime_max);
        sprint_cap_w = *p.get("sprint_power_cap_watts").unwrap_or(&sprint_cap_w);
        trial_params = Some(std::mem::take(&mut t));
    } else if (cp0 - 780.0).abs() > f64::EPSILON {
        let mut t = HashMap::new();
        t.insert("critical_power_watts".to_string(), cp0);
        trial_params = Some(t);
    }

    let trial_ref = trial_params.as_ref();
    let checks = vec![
        check_drain_velocity_contract(),
        check_downhill_same_speed_savings(),
        check_metabolic_overspeed_contract(),
        check_v5_sprint_burst_duration(),
        check_v5_sprint_cooldown(),
        check_v6_cp_sprint_burst(load_kg, cp, w_prime_max, sprint_cap_w),
        check_march_cruise_below_cp(38.0, 1.7, trial_ref, cp),
        check_sustain_run_observed(trial_ref, 90.0, fast_mode),
        check_mobility_run_speed(
            0.0,
            MOBILITY_RUN_0KG_MIN_MS,
            MOBILITY_RUN_0KG_MAX_MS,
            "0kg",
            trial_ref,
        ),
        check_mobility_run_speed(
            35.0,
            MOBILITY_RUN_35KG_MIN_MS,
            MOBILITY_RUN_35KG_MAX_MS,
            "35kg",
            trial_ref,
        ),
        check_zero_load_run_2mile(trial_ref),
        check_march_4h_aerobic_end(load_kg, 4.0, trial_ref),
    ];

    let all_hard_passed = checks.iter().all(|c| !c.hard || c.passed);
    ConstraintReport {
        checks,
        all_hard_passed,
    }
}

pub fn evaluate_hard_constraints(
    params: Option<&HashMap<String, f64>>,
    fast_mode: bool,
) -> ConstraintReport {
    let cp0 = if let Some(p) = params {
        *p.get("critical_power_watts").unwrap_or(&780.0)
    } else {
        780.0
    };
    evaluate_physio_anchors(35.0, cp0, params, fast_mode)
}

/// Parallel batch hard-constraint evaluation (one report per params map).
pub fn batch_evaluate_hard_constraints(
    batch: &[HashMap<String, f64>],
    fast_mode: bool,
) -> Vec<ConstraintReport> {
    use rayon::prelude::*;
    batch
        .par_iter()
        .map(|p| evaluate_hard_constraints(Some(p), fast_mode))
        .collect()
}

pub fn report_violation_score(report: &ConstraintReport) -> f64 {
    report
        .checks
        .iter()
        .filter(|c| c.hard && !c.passed)
        .map(|c| 1.0 + (-c.margin).max(0.0) * 0.01)
        .sum()
}
