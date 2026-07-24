//! 模拟「目前现状」完整链路（对齐 2026-07-24 修复后游戏侧行为）
//!
//! 现状规则摘要：
//! 1. W′ 施密特：≤25% 解除武装，须回 >60% 再武装
//! 2. 解除武装后 Run 套 CP∩2.4 巡航限速（下坡不套 2.4）
//! 3. 解除武装且非冲刺：W′ 放电功率钳到 CP（重力/物理超速不再抽 W′）
//! 4. 物理：v_meas > v_limit+0.2 时软/硬钳（V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP）
//!
//! 运行：
//!   cargo run --manifest-path tools/rss_sim/Cargo.toml --bin sim_cp_cruise --no-default-features --release

use rss_sim::constants::{
    V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V6_AEROBIC_CRUISE_MAX_MS,
    V6_WPRIME_OVERSPEED_HYSTERESIS, V6_WPRIME_OVERSPEED_REARM,
};
use rss_sim::cp_wprime::V6CriticalPowerState;
use rss_sim::drain::refresh_wprime_overspeed_armed;
use rss_sim::metabolism::{compute_cp_watts, invert_speed_for_power_watts, metabolism_power_watts};

const BODY_KG: f64 = 90.0;
const MOVEMENT_RUN: i32 = 2;
const DT: f64 = 0.05;
const PHYS_CLAMP_EPS: f64 = 0.20;
const PHYS_SOFT_DECEL: f64 = 9.0;

#[derive(Clone, Copy)]
struct Scenario {
    name: &'static str,
    /// 意图步态顶速（武装时想跑多快）
    intent_run_ms: f64,
    /// 解除武装后是否做物理超速钳（现状=true）
    physics_clamp: bool,
    /// 解除武装后 W′ 放电是否钳到 CP（现状=true）
    wprime_cp_clamp: bool,
    /// 下坡是否跳过 2.4 平路帽（现状=true）
    downhill_skip_cruise_max: bool,
}

fn cruise_cap_ms(
    cp_eff: f64,
    total_kg: f64,
    grade_pct: f64,
    terrain: f64,
    downhill_skip: bool,
) -> f64 {
    let cp_cap = invert_speed_for_power_watts(cp_eff, total_kg, grade_pct, terrain, MOVEMENT_RUN);
    if grade_pct < 0.0 && downhill_skip {
        return cp_cap.max(0.05);
    }
    let mut cap = V6_AEROBIC_CRUISE_MAX_MS;
    if cp_cap > 0.05 && cp_cap < cap {
        cap = cp_cap;
    }
    cap
}

fn apply_physics(v_meas: f64, v_limit: f64, dt: f64, clamp_on: bool) -> f64 {
    if !clamp_on {
        // 修前：SetSpeedLimit 压不住 → 向意图速缓慢爬升（简化模型）
        return v_meas;
    }
    if v_meas <= v_limit + PHYS_CLAMP_EPS {
        return v_meas.min(v_limit + 0.05);
    }
    // SoftClamp：超额大则硬钳
    if v_meas > v_limit + 0.35 {
        return v_limit;
    }
    let mut v = v_meas - PHYS_SOFT_DECEL * dt;
    if v < v_limit {
        v = v_limit;
    }
    v
}

fn run_scenario(sc: Scenario) {
    let cp0 = 941.7155709077625;
    let w_prime_max = 20421.65248363737;
    let sprint_cap = 2832.5017222379406;
    let load_kg = 37.64;
    let grade_pct = 4.2;
    let terrain = 1.0;
    let fatigue_norm = 1.0;
    let total_kg = BODY_KG + load_kg;
    let threshold = V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT;

    let mut cp = V6CriticalPowerState::new(cp0, w_prime_max, sprint_cap);
    cp.set_runtime_context(load_kg, grade_pct, 1.0, fatigue_norm);
    cp.fatigue_cp_multiplier = (1.0 - 0.18 * fatigue_norm).max(0.75);
    // 从满池开始，模拟「冲一阵后掉到解除武装」
    cp.reset_to_full();
    cp.overspeed_armed = true;

    let cp_eff = cp.get_effective_critical_power_watts();
    let v_cruise = cruise_cap_ms(
        cp_eff,
        total_kg,
        grade_pct,
        terrain,
        sc.downhill_skip_cruise_max,
    );

    println!("────────────────────────────────────────");
    println!("场景: {}", sc.name);
    println!(
        "  CP_eff={:.0}W  巡航 v_limit={:.2} m/s  意图速={:.2}  physics_clamp={}  W′钳CP={}",
        cp_eff, v_cruise, sc.intent_run_ms, sc.physics_clamp, sc.wprime_cp_clamp
    );
    println!("  t(s) | W'% | armed | v_limit | v_meas | P_met | P_tick | 备注");

    let mut t = 0.0;
    let mut v_meas = sc.intent_run_ms;
    let mut disarmed_at = -1.0_f64;
    let mut rows = 0;
    let duration = 45.0;

    while t < duration {
        let pool = cp.pool01();
        cp.overspeed_armed =
            refresh_wprime_overspeed_armed(pool, cp.overspeed_armed, threshold);
        let armed = cp.overspeed_armed;

        let v_limit = if armed {
            sc.intent_run_ms
        } else {
            v_cruise
        };

        if !armed && disarmed_at < 0.0 {
            disarmed_at = t;
        }

        // 物理速度：武装时跟意图；解除后若无钳会往意图爬（跑飞），有钳则贴限速
        if armed {
            v_meas = sc.intent_run_ms;
        } else {
            // 解除瞬间先带着惯性超速一点（模拟 SetSpeedLimit 渐变）
            if v_meas < v_limit {
                v_meas = v_limit;
            } else if !sc.physics_clamp {
                // 修前：限速无效，保持/回到意图速
                let blend = 0.15;
                v_meas = v_meas * (1.0 - blend) + sc.intent_run_ms * blend;
            } else {
                // 现状：先可能冲一下，再被物理钳拉回
                if (t - disarmed_at) < 0.15 && v_meas < sc.intent_run_ms * 0.85 {
                    v_meas = (v_limit + 0.45).min(sc.intent_run_ms);
                }
                v_meas = apply_physics(v_meas, v_limit, DT, true);
            }
        }

        let p_met = metabolism_power_watts(v_meas, total_kg, grade_pct, terrain, MOVEMENT_RUN);
        let mut p_tick = p_met;
        if !armed && sc.wprime_cp_clamp {
            // 现状：解除武装非冲刺 → W′ 放电钳到 CP
            if p_tick > cp_eff {
                p_tick = cp_eff;
            }
        }

        let note = if !armed && (v_meas - v_limit).abs() <= 0.08 {
            "贴限速"
        } else if !armed && v_meas > v_limit + 0.2 {
            "跑飞"
        } else if armed {
            "武装Run"
        } else {
            ""
        };

        // 每 1s 打一行，解除武装前后多打几行
        let print = rows % 20 == 0
            || (disarmed_at >= 0.0 && (t - disarmed_at) < 1.0 && rows % 4 == 0)
            || (pool <= 0.28 && pool >= 0.22 && rows % 4 == 0);
        if print {
            println!(
                "{:5.1} | {:3.0}% | {:5} | {:7.2} | {:6.2} | {:5.0} | {:5.0} | {}",
                t,
                pool * 100.0,
                if armed { "yes" } else { "no" },
                v_limit,
                v_meas,
                p_met,
                p_tick,
                note
            );
        }

        cp.tick(p_tick, false, t, DT, v_meas);
        // 解除武装后若 P_tick=CP，tick 内不放电也不恢复（需 P<=CP+5 才恢复；相等边界）
        t += DT;
        rows += 1;

        if !armed && t - disarmed_at > 12.0 {
            break;
        }
    }

    let final_pool = cp.pool01() * 100.0;
    println!(
        "  结束: t={:.1}s  W'={:.0}%  armed={}  v_meas={:.2}  v_limit≈{:.2}",
        t,
        final_pool,
        cp.overspeed_armed,
        v_meas,
        if cp.overspeed_armed {
            sc.intent_run_ms
        } else {
            v_cruise
        }
    );
}

fn main() {
    let cp0 = 941.7155709077625;
    let load_kg = 37.64;
    let grade_pct = 4.2;
    let fatigue_norm = 1.0;
    let cp_eff = compute_cp_watts(cp0, load_kg, grade_pct, 1.0, fatigue_norm);
    let disable_at = V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT + V6_WPRIME_OVERSPEED_HYSTERESIS;
    let rearm_at = V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT + V6_WPRIME_OVERSPEED_REARM;

    println!("=== RSS 现状时序仿真（Rust）===");
    println!("条件: Elite CP0={cp0:.0}W  load={load_kg}kg  grade={grade_pct}%  If={fatigue_norm}");
    println!("      CP_eff={cp_eff:.0}W  施密特 disable≤{disable_at:.2} rearm>{rearm_at:.2}");
    println!("意图: 持续 Run（非 Sprint），从 W′=100% 跑到解除武装后观察限速/物理");
    println!();

    run_scenario(Scenario {
        name: "修前（无物理钳、超速仍抽 W′）",
        intent_run_ms: 3.50,
        physics_clamp: false,
        wprime_cp_clamp: false,
        downhill_skip_cruise_max: true,
    });
    println!();

    run_scenario(Scenario {
        name: "现状（物理钳 + W′放电钳CP）← 当前游戏",
        intent_run_ms: 3.50,
        physics_clamp: true,
        wprime_cp_clamp: true,
        downhill_skip_cruise_max: true,
    });

    println!();
    println!("解读:");
    println!("  • 武装阶段 v≈3.5，P≫CP → W′ 下降");
    println!("  • 穿过 ≤25% 解除武装 → v_limit 落到 Invert(CP)≈1.8");
    println!("  • 现状: v_meas 被拉回≈1.8，P_tick=CP → W′ 不再被跑飞抽干");
    println!("  • 修前: v_meas 仍≈3.5，P_tick≫CP → W′ 继续掉到 0，且速度抖");
}
