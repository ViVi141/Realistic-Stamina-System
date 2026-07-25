//! Multi-core grid / random scenario battery (Rust + rayon).
//!
//! cargo run --manifest-path tools/rss_sim/Cargo.toml --bin sim_grid_random --no-default-features --release -- --grid -n 100000 -j 0
//!
//! Design space: 3×99×45×14×100×10×3 = 561,330,000 cells.

use rayon::prelude::*;
use rss_sim::constants::{
    merge_game_aligned_params, RssConstants, MOVEMENT_RUN, MOVEMENT_SPRINT, MOVEMENT_WALK,
    V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V6_CP_CRUISE_OVERSPEED_EPS_MPS,
    V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP, V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS,
    V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_SKIP_GRADE, V6_CP_CRUISE_PHYS_CLAMP_GRADE_ABS_MAX,
    V6_METABOLIC_GRADE_ABS_MAX_PCT,
};
use rss_sim::drain::refresh_wprime_overspeed_armed;
use rss_sim::metabolism::{invert_speed_for_power_watts, metabolism_power_watts};
use rss_sim::twin::RSSDigitalTwin;
use serde_json::Value;
use std::collections::HashMap;
use std::env;
use std::fs;
use std::path::PathBuf;
use std::hint::black_box;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Instant;

const BODY_KG: f64 = 90.0;
const PRESETS: [&str; 3] = ["EliteStandard", "StandardMilsim", "TacticalAction"];

const GRID_SHAPE: [usize; 7] = [3, 99, 45, 14, 100, 10, 3];
const GRID_TOTAL: u64 = 561_330_000;

fn grid_grade(i: usize) -> f64 {
    -98.0 + (i as f64) * (196.0 / 98.0)
}
fn grid_load(i: usize) -> f64 {
    (i as f64) * (44.0 / 44.0)
}
fn grid_terrain(i: usize) -> f64 {
    0.8 + (i as f64) * ((2.2 - 0.8) / 13.0)
}
fn grid_stamina(i: usize) -> f64 {
    (i as f64) * (0.99 / 99.0)
}
fn grid_wprime(i: usize) -> f64 {
    (i as f64) * 0.1
}
fn grid_phase(i: usize) -> i32 {
    match i {
        0 => MOVEMENT_WALK,
        1 => MOVEMENT_RUN,
        _ => MOVEMENT_SPRINT,
    }
}

fn unravel(index: u64) -> [usize; 7] {
    let mut idx = index % GRID_TOTAL;
    let mut out = [0usize; 7];
    for i in (0..7).rev() {
        let d = GRID_SHAPE[i] as u64;
        out[i] = (idx % d) as usize;
        idx /= d;
    }
    out
}

fn clamp_grade_metabolic(g: f64) -> f64 {
    let lim = V6_METABOLIC_GRADE_ABS_MAX_PCT;
    if g > lim {
        return lim;
    }
    if g < -lim {
        return -lim;
    }
    g
}

fn should_enforce_phys_clamp(grade: f64, measured: f64, limit: f64) -> bool {
    if !V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP {
        return false;
    }
    if grade.abs() > V6_CP_CRUISE_PHYS_CLAMP_GRADE_ABS_MAX {
        return false;
    }
    if limit < 0.1 {
        return false;
    }
    let mut trigger = V6_CP_CRUISE_OVERSPEED_EPS_MPS;
    if grade < V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_SKIP_GRADE {
        trigger = V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS;
    }
    if measured <= limit + trigger {
        return false;
    }
    true
}

fn apply_phys_cap(measured: f64, limit: f64, dt: f64, grade: f64) -> f64 {
    if !should_enforce_phys_clamp(grade, measured, limit) {
        return measured;
    }
    let mut d = dt;
    if d < 0.01 {
        d = 0.01;
    }
    if d > 0.5 {
        d = 0.5;
    }
    if measured > limit + 0.35 {
        return limit;
    }
    let mut v = measured - 9.0 * d;
    if v < limit {
        v = limit;
    }
    v
}

#[derive(Clone)]
struct Scenario {
    preset_i: usize,
    grade: f64,
    load_kg: f64,
    terrain: f64,
    stamina: f64,
    w_pool: f64,
    phase: i32,
    v_meas: f64,
    v_limit: f64,
    dt: f64,
    coast: f64,
    grid_index: u64,
}

fn scenario_from_grid_index(index: u64) -> Scenario {
    let u = unravel(index);
    let h = index
        .wrapping_mul(1_103_515_245)
        .wrapping_add(12345)
        & 0x7FFF_FFFF;
    // Independent mix so coast excess is not coupled to grade/unravel bits.
    let h2 = index
        .wrapping_mul(1_664_525)
        .wrapping_add(101_390_4223)
        & 0x7FFF_FFFF;
    let v_limit = 0.35 + ((h % 3400) as f64) / 1000.0;
    let v_meas = (((h / 3400) % 5800) as f64) / 1000.0;
    // Excess over limit: ~0.16 … ~2.95 m/s (covers soft/hard clamp bands).
    let excess = 0.16 + (((h2 % 2800) as f64) / 1000.0);
    let mut coast = v_limit + excess;
    if coast > 5.8 {
        coast = 5.8;
    }
    let dts = [0.017, 0.02, 0.05, 0.1];
    Scenario {
        preset_i: u[0],
        grade: grid_grade(u[1]),
        load_kg: grid_load(u[2]),
        terrain: grid_terrain(u[3]),
        stamina: grid_stamina(u[4]),
        w_pool: grid_wprime(u[5]),
        phase: grid_phase(u[6]),
        v_meas,
        v_limit,
        dt: dts[(h % 4) as usize],
        coast,
        grid_index: index,
    }
}

fn load_preset_constants(config_dir: &PathBuf, name: &str) -> RssConstants {
    let slug = name.to_lowercase();
    let mut map: HashMap<String, f64> = HashMap::new();
    for fname in [
        format!("optimized_rss_config_{slug}_v4.json"),
        format!("optimized_rss_config_{slug}_v6.json"),
    ] {
        let path = config_dir.join(&fname);
        if !path.is_file() {
            continue;
        }
        if let Ok(text) = fs::read_to_string(&path) {
            if let Ok(Value::Object(obj)) = serde_json::from_str::<Value>(&text) {
                for (k, v) in obj {
                    if k.starts_with('_') {
                        continue;
                    }
                    if let Some(n) = v.as_f64() {
                        map.insert(k, n);
                    } else if let Some(i) = v.as_i64() {
                        map.insert(k, i as f64);
                    }
                }
            }
        }
    }
    let merged = merge_game_aligned_params(&map);
    RssConstants::from_params(&merged)
}

fn make_twin(constants: &RssConstants, sc: &Scenario) -> RSSDigitalTwin {
    let mut twin = RSSDigitalTwin::new(constants.clone());
    twin.reset();
    twin.stamina = sc.stamina.clamp(0.0, 1.0);
    let max_j = twin.v6_cp_state.w_prime_max_joules;
    twin.v6_cp_state.w_prime_joules = max_j * sc.w_pool.clamp(0.0, 1.0);
    twin.v6_cp_state.overspeed_armed = sc.w_pool > 0.60;
    twin.v6_cp_state.refresh_and_get_overspeed_armed();
    let fatigue = (1.0 - sc.stamina).clamp(0.0, 1.0);
    twin.v6_cp_state
        .set_runtime_context(sc.load_kg, 0.0, 1.0, fatigue);
    twin
}

#[derive(Default)]
struct WeirdHit {
    walk_near_run: bool,
    metabolic_clamped: bool,
    mid_wprime_disarmed: bool,
    steep_phys_skip: bool,
    downhill_coast_over: bool,
    downhill_excess: f64,
    crawl_v: bool,
    high_power: bool,
    v_phase: f64,
    v_walk: f64,
    v_run: f64,
    power: f64,
}

struct WeirdStats {
    walk_near_run: AtomicU64,
    metabolic_clamped: AtomicU64,
    mid_wprime_disarmed: AtomicU64,
    steep_phys_skip: AtomicU64,
    downhill_coast_over: AtomicU64,
    over_bin_le_035: AtomicU64,
    over_bin_le_070: AtomicU64,
    over_bin_le_150: AtomicU64,
    over_bin_gt_150: AtomicU64,
    crawl_v: AtomicU64,
    high_power: AtomicU64,
    max_downhill_excess_milli: AtomicU64,
    samples: Mutex<Vec<(String, String)>>,
}

impl WeirdStats {
    fn new() -> Self {
        Self {
            walk_near_run: AtomicU64::new(0),
            metabolic_clamped: AtomicU64::new(0),
            mid_wprime_disarmed: AtomicU64::new(0),
            steep_phys_skip: AtomicU64::new(0),
            downhill_coast_over: AtomicU64::new(0),
            over_bin_le_035: AtomicU64::new(0),
            over_bin_le_070: AtomicU64::new(0),
            over_bin_le_150: AtomicU64::new(0),
            over_bin_gt_150: AtomicU64::new(0),
            crawl_v: AtomicU64::new(0),
            high_power: AtomicU64::new(0),
            max_downhill_excess_milli: AtomicU64::new(0),
            samples: Mutex::new(Vec::new()),
        }
    }

    fn push_sample(&self, cat: &str, note: String) {
        if let Ok(mut guard) = self.samples.lock() {
            guard.push((cat.to_string(), note));
        }
    }

    fn bump_sample(counter: &AtomicU64, stats: &WeirdStats, cat: &str, note: String) {
        let prev = counter.fetch_add(1, Ordering::Relaxed);
        if prev < 3 {
            stats.push_sample(cat, note);
        }
    }

    fn record(&self, sc: &Scenario, hit: &WeirdHit) {
        if hit.metabolic_clamped {
            Self::bump_sample(
                &self.metabolic_clamped,
                self,
                "metabolic_clamped",
                format!(
                    "gi={} grade={:.1} → meta±{:.0}",
                    sc.grid_index, sc.grade, V6_METABOLIC_GRADE_ABS_MAX_PCT
                ),
            );
        }
        if hit.steep_phys_skip {
            Self::bump_sample(
                &self.steep_phys_skip,
                self,
                "steep_phys_skip",
                format!("gi={} |grade|={:.1}", sc.grid_index, sc.grade.abs()),
            );
        }
        if hit.mid_wprime_disarmed {
            Self::bump_sample(
                &self.mid_wprime_disarmed,
                self,
                "mid_wprime_disarmed",
                format!(
                    "gi={} W′={:.2} (rearm>~0.60) stamina={:.2}",
                    sc.grid_index, sc.w_pool, sc.stamina
                ),
            );
        }
        if hit.walk_near_run {
            Self::bump_sample(
                &self.walk_near_run,
                self,
                "walk_near_run",
                format!(
                    "gi={} walk={:.3} run={:.3} grade={:.1} load={:.1}",
                    sc.grid_index, hit.v_walk, hit.v_run, sc.grade, sc.load_kg
                ),
            );
        }
        if hit.crawl_v {
            Self::bump_sample(
                &self.crawl_v,
                self,
                "crawl_v",
                format!(
                    "gi={} v={:.3} phase={} grade={:.1} load={:.1} sta={:.2}",
                    sc.grid_index, hit.v_phase, sc.phase, sc.grade, sc.load_kg, sc.stamina
                ),
            );
        }
        if hit.high_power {
            Self::bump_sample(
                &self.high_power,
                self,
                "high_power",
                format!(
                    "gi={} P={:.0}W v={:.3} grade={:.1} load={:.1}",
                    sc.grid_index, hit.power, hit.v_phase, sc.grade, sc.load_kg
                ),
            );
        }
        if hit.downhill_coast_over {
            let prev = self.downhill_coast_over.fetch_add(1, Ordering::Relaxed);
            let ex = hit.downhill_excess;
            if ex <= 0.35 {
                self.over_bin_le_035.fetch_add(1, Ordering::Relaxed);
            } else if ex <= 0.70 {
                self.over_bin_le_070.fetch_add(1, Ordering::Relaxed);
            } else if ex <= 1.50 {
                self.over_bin_le_150.fetch_add(1, Ordering::Relaxed);
            } else {
                self.over_bin_gt_150.fetch_add(1, Ordering::Relaxed);
            }
            let milli = (ex * 1000.0).round() as u64;
            let mut cur = self.max_downhill_excess_milli.load(Ordering::Relaxed);
            while milli > cur {
                match self.max_downhill_excess_milli.compare_exchange_weak(
                    cur,
                    milli,
                    Ordering::Relaxed,
                    Ordering::Relaxed,
                ) {
                    Ok(_) => break,
                    Err(v) => cur = v,
                }
            }
            if prev < 3 {
                self.push_sample(
                    "downhill_coast_over",
                    format!(
                        "gi={} grade={:.1} coast={:.2} lim={:.2} excess={:.2}",
                        sc.grid_index, sc.grade, sc.coast, sc.v_limit, ex
                    ),
                );
            }
        }
    }

    fn print_report(&self, total: u64) {
        let pct = |c: u64| -> f64 {
            if total == 0 {
                return 0.0;
            }
            (c as f64) * 100.0 / (total as f64)
        };
        let wnr = self.walk_near_run.load(Ordering::Relaxed);
        let mc = self.metabolic_clamped.load(Ordering::Relaxed);
        let mw = self.mid_wprime_disarmed.load(Ordering::Relaxed);
        let sp = self.steep_phys_skip.load(Ordering::Relaxed);
        let dco = self.downhill_coast_over.load(Ordering::Relaxed);
        let b035 = self.over_bin_le_035.load(Ordering::Relaxed);
        let b070 = self.over_bin_le_070.load(Ordering::Relaxed);
        let b150 = self.over_bin_le_150.load(Ordering::Relaxed);
        let bgt = self.over_bin_gt_150.load(Ordering::Relaxed);
        let cv = self.crawl_v.load(Ordering::Relaxed);
        let hp = self.high_power.load(Ordering::Relaxed);
        let max_ex = (self.max_downhill_excess_milli.load(Ordering::Relaxed) as f64) / 1000.0;
        println!("--- weird-but-legal (pass, counterintuitive zones) ---");
        println!(
            "  metabolic_clamped(|grade|>45): {mc} ({:.2}%)",
            pct(mc)
        );
        println!("  steep_phys_skip(|grade|>35): {sp} ({:.2}%)", pct(sp));
        println!(
            "  mid_wprime_disarmed(0.25<W′≤0.60): {mw} ({:.2}%)",
            pct(mw)
        );
        println!(
            "  walk_near_run(demoted |Δ|≤0.05): {wnr} ({:.2}%)",
            pct(wnr)
        );
        println!("  crawl_v(phase v<0.55): {cv} ({:.2}%)", pct(cv));
        println!("  high_power(P>3000W): {hp} ({:.2}%)", pct(hp));
        println!(
            "  downhill_coast_over(grade<-2, coast>lim+0.15): {dco} ({:.2}%) max_excess={max_ex:.3} m/s",
            pct(dco)
        );
        println!(
            "    excess bins: ≤0.35={b035}  ≤0.70={b070}  ≤1.50={b150}  >1.50={bgt}"
        );
        if let Ok(guard) = self.samples.lock() {
            if !guard.is_empty() {
                println!("  samples:");
                for (cat, note) in guard.iter() {
                    println!("    [{cat}] {note}");
                }
            }
        }
    }
}

/// Returns (error, checksum, weird-hit). Checksum forces real twin work.
fn check_scenario(constants: &[RssConstants; 3], sc: &Scenario) -> (Option<String>, f64, WeirdHit) {
    let mut hit = WeirdHit::default();
    let g = sc.grade;
    // Steep: full skip even for runaway. Other grades: path may enforce.
    if g.abs() > 35.0 {
        if should_enforce_phys_clamp(g, sc.coast, sc.v_limit) {
            return (
                Some(format!("steep |grade|={} still enforces", g.abs())),
                0.0,
                hit,
            );
        }
    }

    let c = clamp_grade_metabolic(g);
    if c.abs() > 45.0 + 1e-9 {
        return (Some(format!("metabolic clamp leaked {c}")), 0.0, hit);
    }
    hit.metabolic_clamped = g.abs() > 45.0;
    hit.steep_phys_skip = g.abs() > 35.0;
    hit.mid_wprime_disarmed = sc.w_pool > 0.25 && sc.w_pool <= 0.60;

    let out = apply_phys_cap(sc.coast, sc.v_limit, sc.dt, g);
    if !out.is_finite() {
        return (Some("phys cap non-finite".into()), 0.0, hit);
    }
    let enforce = should_enforce_phys_clamp(g, sc.coast, sc.v_limit);
    if g.abs() > 35.0 {
        if (out - sc.coast).abs() > 1e-6 {
            return (Some(format!("steep skip mutated {out}")), 0.0, hit);
        }
    } else if !enforce {
        if (out - sc.coast).abs() > 1e-6 {
            return (Some(format!("mild coast mutated {out}")), 0.0, hit);
        }
    } else if sc.coast > sc.v_limit + 0.35 {
        if out > sc.v_limit + 1e-6 {
            return (Some(format!("hard clamp failed out={out}")), 0.0, hit);
        }
    } else if sc.coast > sc.v_limit + 1e-9 {
        let mut expected_max = sc.coast - 9.0 * sc.dt.clamp(0.01, 0.5);
        if expected_max < sc.v_limit {
            expected_max = sc.v_limit;
        }
        if out > expected_max + 1e-4 {
            return (
                Some(format!("soft clamp no decelerate out={out}")),
                0.0,
                hit,
            );
        }
    }

    if g < -2.0 && sc.coast > sc.v_limit + 0.15 {
        hit.downhill_coast_over = true;
        hit.downhill_excess = sc.coast - sc.v_limit;
    }

    let thresh = V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT;
    if sc.w_pool <= 0.60 && refresh_wprime_overspeed_armed(sc.w_pool, false, thresh) {
        return (
            Some(format!("pool={} rearmed from disarmed", sc.w_pool)),
            0.0,
            hit,
        );
    }
    if sc.w_pool > 0.25 && !refresh_wprime_overspeed_armed(sc.w_pool, true, thresh) {
        return (
            Some(format!("pool={} disarmed unexpectedly", sc.w_pool)),
            0.0,
            hit,
        );
    }
    if sc.w_pool <= 0.25 && refresh_wprime_overspeed_armed(sc.w_pool, true, thresh) {
        return (
            Some(format!("pool={} should disarm", sc.w_pool)),
            0.0,
            hit,
        );
    }

    let mut twin = make_twin(&constants[sc.preset_i], sc);
    let total = BODY_KG + sc.load_kg;
    let v = twin.calculate_actual_speed(
        sc.stamina,
        total,
        sc.phase,
        2.0,
        sc.grade,
        12.0,
        sc.terrain,
    );
    if !v.is_finite() || v < 0.0 || v > 8.0 {
        return (Some(format!("calculate_actual_speed bad {v}")), 0.0, hit);
    }
    hit.v_phase = v;
    hit.crawl_v = v < 0.55;
    let g_meta = clamp_grade_metabolic(sc.grade);
    let p = metabolism_power_watts(v.max(0.1), total, g_meta, sc.terrain, sc.phase);
    if !p.is_finite() || p < 0.0 || p > 20000.0 {
        return (Some(format!("metabolism weird {p}")), 0.0, hit);
    }
    hit.power = p;
    hit.high_power = p > 3000.0;
    let cp = twin.v6_cp_state.get_effective_critical_power_watts().max(200.0);
    let inv = invert_speed_for_power_watts(cp, total, g_meta, sc.terrain, MOVEMENT_RUN);
    if !inv.is_finite() || inv < 0.0 || inv > 8.0 {
        return (Some(format!("invert weird {inv}")), 0.0, hit);
    }

    twin.v6_cp_state.w_prime_joules = 0.0;
    twin.v6_cp_state.overspeed_armed = false;
    twin.v6_cp_state.refresh_and_get_overspeed_armed();
    let sta = sc.stamina.max(0.15);
    let v_run = twin.calculate_actual_speed(sta, total, MOVEMENT_RUN, 2.0, sc.grade, 20.0, sc.terrain);
    let v_walk =
        twin.calculate_actual_speed(sta, total, MOVEMENT_WALK, 1.0, sc.grade, 20.0, sc.terrain);
    if !v_run.is_finite() || !v_walk.is_finite() {
        return (
            Some(format!("walk/run non-finite {v_run}/{v_walk}")),
            0.0,
            hit,
        );
    }
    if v_walk > v_run + 0.03 {
        return (
            Some(format!(
                "Walk faster than Run walk={v_walk:.3} run={v_run:.3} grade={:.1}",
                sc.grade
            )),
            0.0,
            hit,
        );
    }
    hit.v_walk = v_walk;
    hit.v_run = v_run;
    let gap = v_run - v_walk;
    if gap >= 0.0 && gap <= 0.05 {
        hit.walk_near_run = true;
    }

    // downhill: mild coast must not fight; runaway must clamp
    if g < -2.0 && g >= -35.0 {
        let lim = sc.v_limit;
        let mild = lim + V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS * 0.75;
        let mut vv = lim;
        let dt = 0.02;
        for _ in 0..40 {
            vv = (vv + 10.0 * dt).min(mild);
            let before = vv;
            vv = apply_phys_cap(vv, lim, dt, g);
            if (vv - before).abs() > 1e-9 {
                return (Some(format!("mild downhill coast fought grade={g:.2}")), 0.0, hit);
            }
        }
        let runaway = lim + V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS + 0.50;
        let after = apply_phys_cap(runaway, lim, dt, g);
        if after > lim + 1e-3 {
            return (
                Some(format!("downhill runaway not clamped grade={g:.2}")),
                0.0,
                hit,
            );
        }
    }

    let checksum = v + v_run + v_walk + p * 1e-4 + inv + out;
    (None, checksum, hit)
}

fn parse_args() -> (String, u64, u64, usize, PathBuf) {
    let mut mode = "grid".to_string();
    let mut n: u64 = 100_000;
    let mut seed: u64 = 42;
    let mut jobs: usize = 0;
    let mut config_dir = env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let args: Vec<String> = env::args().skip(1).collect();
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--grid" => mode = "grid".into(),
            "--grid-full" => {
                mode = "grid-full".into();
                n = GRID_TOTAL;
            }
            "--quick" => {
                mode = "quick".into();
                n = 128;
            }
            "-n" | "--n" => {
                i += 1;
                n = args.get(i).and_then(|s| s.parse().ok()).unwrap_or(n);
            }
            "--seed" => {
                i += 1;
                seed = args.get(i).and_then(|s| s.parse().ok()).unwrap_or(seed);
            }
            "-j" | "--jobs" => {
                i += 1;
                jobs = args.get(i).and_then(|s| s.parse().ok()).unwrap_or(jobs);
            }
            "--config-dir" => {
                i += 1;
                if let Some(p) = args.get(i) {
                    config_dir = PathBuf::from(p);
                }
            }
            "--show-grid" => {
                println!(
                    "design grid: {}×{}×{}×{}×{}×{}×{} = {} cells",
                    GRID_SHAPE[0],
                    GRID_SHAPE[1],
                    GRID_SHAPE[2],
                    GRID_SHAPE[3],
                    GRID_SHAPE[4],
                    GRID_SHAPE[5],
                    GRID_SHAPE[6],
                    GRID_TOTAL
                );
                std::process::exit(0);
            }
            _ => {}
        }
        i += 1;
    }
    if jobs == 0 {
        jobs = std::thread::available_parallelism()
            .map(|n| n.get().saturating_sub(1).max(1))
            .unwrap_or(1);
    }
    (mode, n, seed, jobs, config_dir)
}

fn main() {
    let (mode, n, seed, jobs, config_dir) = parse_args();
    println!(
        "design grid: {}×{}×{}×{}×{}×{}×{} = {} cells",
        GRID_SHAPE[0],
        GRID_SHAPE[1],
        GRID_SHAPE[2],
        GRID_SHAPE[3],
        GRID_SHAPE[4],
        GRID_SHAPE[5],
        GRID_SHAPE[6],
        GRID_TOTAL
    );
    println!(
        "=== RSS Rust grid battery ({mode}) n={n} seed={seed} jobs={jobs} ==="
    );

    rayon::ThreadPoolBuilder::new()
        .num_threads(jobs)
        .build_global()
        .ok();

    let constants = [
        load_preset_constants(&config_dir, PRESETS[0]),
        load_preset_constants(&config_dir, PRESETS[1]),
        load_preset_constants(&config_dir, PRESETS[2]),
    ];

    let t0 = Instant::now();
    let fail_count = AtomicU64::new(0);
    let checksum_bits = AtomicU64::new(0);
    let first_fail: Arc<Mutex<Option<String>>> = Arc::new(Mutex::new(None));
    let weird = Arc::new(WeirdStats::new());

    let total_cases: u64;
    if mode == "grid-full" {
        // Exhaustive: stream indices — do not allocate Vec of GRID_TOTAL.
        total_cases = GRID_TOTAL;
        let ff = Arc::clone(&first_fail);
        let ws = Arc::clone(&weird);
        (0..GRID_TOTAL).into_par_iter().for_each(|gi| {
            let sc = scenario_from_grid_index(gi);
            let (err, sum, hit) = black_box(check_scenario(&constants, &sc));
            checksum_bits.fetch_xor(sum.to_bits(), Ordering::Relaxed);
            if let Some(msg) = err {
                fail_count.fetch_add(1, Ordering::Relaxed);
                if let Ok(mut guard) = ff.lock() {
                    if guard.is_none() {
                        *guard = Some(format!("[gi={gi}] {msg}"));
                    }
                }
            } else {
                ws.record(&sc, &hit);
            }
        });
    } else {
        let indices: Vec<u64> = if mode == "quick" {
            vec![
                0,
                GRID_TOTAL / 2,
                GRID_TOTAL - 1,
                1_234_567 % GRID_TOTAL,
            ]
            .into_iter()
            .chain((0..128u64).map(|i| {
                seed.wrapping_mul(1_000_003)
                    .wrapping_add(i.wrapping_mul(9_876_541))
                    % GRID_TOTAL
            }))
            .collect()
        } else {
            // Unique-ish sample without allocating a giant HashSet for huge n
            (0..n)
                .map(|i| {
                    seed.wrapping_mul(1_000_003)
                        .wrapping_add(i.wrapping_mul(9_876_541))
                        % GRID_TOTAL
                })
                .collect()
        };

        total_cases = indices.len() as u64;
        let ff = Arc::clone(&first_fail);
        let ws = Arc::clone(&weird);
        indices.par_iter().enumerate().for_each(|(case_id, &gi)| {
            let sc = scenario_from_grid_index(gi);
            let (err, sum, hit) = black_box(check_scenario(&constants, &sc));
            checksum_bits.fetch_xor(sum.to_bits(), Ordering::Relaxed);
            if let Some(msg) = err {
                fail_count.fetch_add(1, Ordering::Relaxed);
                if let Ok(mut guard) = ff.lock() {
                    if guard.is_none() {
                        *guard = Some(format!("[{case_id}] gi={gi} {msg}"));
                    }
                }
            } else {
                ws.record(&sc, &hit);
            }
        });
    }

    let elapsed = t0.elapsed().as_secs_f64();
    let fails = fail_count.load(Ordering::Relaxed);
    let rate = (total_cases as f64) / elapsed.max(1e-9);
    let csum = f64::from_bits(checksum_bits.load(Ordering::Relaxed));
    println!(
        "cases={total_cases} fails={fails} elapsed={elapsed:.3}s ({rate:.0}/s) jobs={jobs} checksum={csum}"
    );
    weird.print_report(total_cases);
    if fails == 0 {
        println!("PASS: {total_cases}/{total_cases} scenarios");
    } else {
        if let Ok(guard) = first_fail.lock() {
            if let Some(msg) = guard.as_ref() {
                println!("sample fail: {msg}");
            }
        }
        println!("FAIL: {fails}/{total_cases} scenarios");
        std::process::exit(1);
    }
}
