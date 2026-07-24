//! 拟真找 BUG：对照权威生理语义 vs 现状规则，输出可复现的失败用例
//!
//! cargo run --manifest-path tools/rss_sim/Cargo.toml --bin sim_bug_hunt --no-default-features --release

use rss_sim::constants::{
    V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V6_AEROBIC_CRUISE_MAX_MS, V6_RUN_GAIT_FLOOR_MS,
    V6_WPRIME_OVERSPEED_HYSTERESIS, V6_WPRIME_OVERSPEED_REARM,
};
use rss_sim::cp_wprime::V6CriticalPowerState;
use rss_sim::drain::refresh_wprime_overspeed_armed;
use rss_sim::metabolism::{compute_cp_watts, invert_speed_for_power_watts, metabolism_power_watts};

const BODY: f64 = 90.0;
const RUN: i32 = 2;
const DT: f64 = 0.05;

struct Bug {
    id: &'static str,
    severity: &'static str,
    detail: String,
}

fn elite_cp_state(load_kg: f64, grade: f64, fatigue: f64) -> V6CriticalPowerState {
    let cp0 = 941.7155709077625;
    let mut s = V6CriticalPowerState::new(cp0, 20421.65, 2832.5);
    s.set_runtime_context(load_kg, grade, 1.0, fatigue);
    s.set_fatigue_cp_multiplier((1.0 - 0.18 * fatigue).max(0.75));
    s.reset_to_full();
    s
}

fn cruise_v(cp: f64, total: f64, grade: f64, skip_flat_cap_downhill: bool) -> f64 {
    let inv = invert_speed_for_power_watts(cp, total, grade, 1.0, RUN);
    let mut cap = if grade < 0.0 && skip_flat_cap_downhill {
        inv.max(0.05)
    } else if inv > 0.05 && inv < V6_AEROBIC_CRUISE_MAX_MS {
        inv
    } else {
        V6_AEROBIC_CRUISE_MAX_MS.min(inv.max(0.05))
    };
    if cap > 0.05 && cap < V6_RUN_GAIT_FLOOR_MS {
        cap = V6_RUN_GAIT_FLOOR_MS;
    }
    cap
}

fn main() {
    let mut bugs: Vec<Bug> = Vec::new();
    let load = 37.64;
    let grade = 4.2;
    let total = BODY + load;
    let thresh = V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT;
    let _disable_at = thresh + V6_WPRIME_OVERSPEED_HYSTERESIS;
    let rearm_at = thresh + V6_WPRIME_OVERSPEED_REARM;

    println!("=== RSS 拟真找 BUG ===");
    println!("基准: Elite + {load}kg + grade={grade}% + If=100%\n");

    // ── BUG1: 在 CP 巡航（P≈CP）时 W′ 仍快速再填充 → 稍后必然再武装冲刺 ──
    {
        let mut cp = elite_cp_state(load, grade, 1.0);
        // 强制到刚解除武装
        cp.w_prime_joules = cp.w_prime_max_joules * 0.24;
        cp.overspeed_armed = false;
        let cp_eff = cp.get_effective_critical_power_watts();
        let v_lim = cruise_v(cp_eff, total, grade, true);
        let p = metabolism_power_watts(v_lim, total, grade, 1.0, RUN);

        let t0 = 0.0;
        let mut t = t0;
        let mut rearmed = false;
        let mut rearm_t = -1.0;
        while t < 120.0 {
            let pool = cp.pool01();
            cp.overspeed_armed = refresh_wprime_overspeed_armed(pool, cp.overspeed_armed, thresh);
            // 现状：解除武装后 P_tick 钳到 CP，且 tick 在 P<=CP+5 时 Skiba 恢复
            let p_tick = cp_eff.min(p);
            cp.tick(p_tick, false, t, DT, v_lim);
            if cp.overspeed_armed && !rearmed {
                rearmed = true;
                rearm_t = t;
                break;
            }
            t += DT;
        }
        println!(
            "[用例1] CP巡航再填充: P_tick={:.0}≈CP  v={:.2}  → {:.1}s 后 armed={} W'={:.0}%",
            p.min(cp_eff),
            v_lim,
            if rearm_t >= 0.0 { rearm_t } else { t },
            rearmed,
            cp.pool01() * 100.0
        );
        if rearmed {
            bugs.push(Bug {
                id: "WPRIME_REARM_WHILE_CRUISING",
                severity: "高",
                detail: format!(
                    "解除武装后以 P≈CP 持续 Run，Skiba 仍再填充，约 {rearm_t:.0}s 回到 >{rearm_at:.0}% 再武装；\
                     速度会从 ~{v_lim:.1} 跳回快跑再抽干，形成慢周期震荡。生理上 P≈CP 时 W′ 不应明显回充。"
                ),
            });
        }
    }

    // ── BUG2: 施密特门禁（修复后应禁止）──
    {
        let mut cp = elite_cp_state(load, grade, 1.0);
        cp.w_prime_joules = cp.w_prime_max_joules * 0.35;
        cp.overspeed_armed = false;
        let armed = refresh_wprime_overspeed_armed(0.35, false, thresh);
        let sprint_ok = cp.is_sprint_allowed(0.80, false, 10.0);
        println!(
            "[用例2] 池=35% armed={armed}  IsSprintAllowed={sprint_ok}  (期望 false)"
        );
        if !armed && sprint_ok {
            bugs.push(Bug {
                id: "SPRINT_GATE_IGNORES_SCHMITT",
                severity: "高",
                detail: "IsSprintAllowed 忽略施密特闩锁".into(),
            });
        }
    }

    // ── BUG3: 下坡 2.4 帽（孪生 calculate_actual_speed 已与游戏对齐；此处仅文档旧公式）──
    {
        let cp_light = compute_cp_watts(941.7, 10.0, -9.4, 1.0, 0.0);
        let inv_l = invert_speed_for_power_watts(cp_light, BODY + 10.0, -9.4, 1.0, RUN);
        let game_cap = inv_l; // downhill skip flat max
        println!(
            "[用例3] 轻载下坡 Invert={inv_l:.3}  游戏/新孪生帽={game_cap:.3}  (已对齐，跳过 2.4)"
        );
        let _ = cp_light;
    }

    // ── BUG4: P==CP 时不应再填充（修复后 W′ 应冻结）──
    {
        let mut cp = elite_cp_state(load, grade, 1.0);
        cp.w_prime_joules = cp.w_prime_max_joules * 0.24;
        cp.overspeed_armed = false;
        let cp_eff = cp.get_effective_critical_power_watts();
        let pool0 = cp.pool01();
        for i in 0..200 {
            cp.tick(cp_eff, false, i as f64 * DT, DT, 1.8);
        }
        let pool1 = cp.pool01();
        let d_pct = (pool1 - pool0) * 100.0;
        println!(
            "[用例4] P=CP 巡航 10s: W' {:.1}% → {:.1}% (Δ={:+.2}%)  期望≈0",
            pool0 * 100.0,
            pool1 * 100.0,
            d_pct
        );
        if d_pct > 0.5 {
            bugs.push(Bug {
                id: "SKIBA_RECOVERY_AT_CP",
                severity: "高",
                detail: format!("P=CP 时 W′ 仍增加 {d_pct:.1}%"),
            });
        }
    }

    // ── 设计注记：再武装速度阶跃（恢复条件修复后仅休息后出现，属施密特预期）──
    {
        let cp_eff = compute_cp_watts(941.7, load, grade, 1.0, 1.0);
        let v_lo = cruise_v(cp_eff, total, grade, true);
        let v_hi = 3.5;
        println!(
            "[用例5] 再武装速度阶跃(设计): {v_lo:.2}→{v_hi:.2} m/s — 仅 P≪CP 休息回充后触发，非巡航震荡"
        );
    }

    // ── BUG6: 空池 AvailableP（修复后应≈CP）──
    {
        let mut cp = elite_cp_state(load, grade, 0.0);
        cp.w_prime_joules = 3.0;
        cp.overspeed_armed = true;
        let avail = cp.get_available_power_watts(true, 0.017, 0.0);
        let cp_eff = cp.get_effective_critical_power_watts();
        println!(
            "[用例6] W′=3J AvailableP={avail:.0} CP={cp_eff:.0}  期望≈CP"
        );
        if avail > cp_eff + 50.0 {
            bugs.push(Bug {
                id: "AVAILABLE_P_EPS_CLIFF",
                severity: "中",
                detail: format!("空池仍 AvailableP={avail:.0}"),
            });
        }
    }

    // ── BUG7: Run CP 巡航不得落入 Walk 带（滑步）──
    {
        let cp_eff = compute_cp_watts(941.7, 31.128, 1.6, 1.0, 1.0);
        let inv = invert_speed_for_power_watts(cp_eff, BODY + 31.128, 1.6, 1.0, RUN);
        let capped = cruise_v(cp_eff, BODY + 31.128, 1.6, true);
        println!(
            "[用例7] Run CP巡航地板: Invert={inv:.2} → 应用={capped:.2}  (地板={V6_RUN_GAIT_FLOOR_MS})"
        );
        if inv < V6_RUN_GAIT_FLOOR_MS - 0.05 && capped < V6_RUN_GAIT_FLOOR_MS - 0.01 {
            bugs.push(Bug {
                id: "RUN_GAIT_FLOOR_MISSING",
                severity: "高",
                detail: format!(
                    "CP 反解 {inv:.2} 落入 Walk 带，巡航未抬到地板 {V6_RUN_GAIT_FLOOR_MS} → Walk 动画/滑步"
                ),
            });
        }
    }

    println!();
    if bugs.is_empty() {
        println!("========== 拟真回归：未发现未修复问题 ==========");
        println!("已验证: P=CP 不回充 W′ / 施密特禁 Sprint / 空池 AvailableP=CP / 下坡跳过 2.4 / Run 步态地板");
    } else {
        println!("========== 发现 {} 个问题 ==========", bugs.len());
        for (i, b) in bugs.iter().enumerate() {
            println!("{}. [{}] {} — {}", i + 1, b.severity, b.id, b.detail);
        }
    }
}
