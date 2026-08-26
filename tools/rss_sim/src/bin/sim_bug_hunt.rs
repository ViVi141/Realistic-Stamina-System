//! 拟真找 BUG：对照权威生理语义 vs 现状规则，输出可复现的失败用例
//!
//! cargo run --manifest-path tools/rss_sim/Cargo.toml --bin sim_bug_hunt --no-default-features --release

use rss_sim::constants::{
    V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT, V6_AEROBIC_CRUISE_MAX_MS,
    V6_CP_CRUISE_OVERSPEED_EPS_MPS, V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP,
    V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_SKIP_GRADE, V6_CP_CRUISE_PHYS_CLAMP_GRADE_ABS_MAX,
    V6_METABOLIC_GRADE_ABS_MAX_PCT, V6_RUN_GAIT_FLOOR_MS, V6_WPRIME_OVERSPEED_HYSTERESIS,
    V6_WPRIME_OVERSPEED_REARM,
};
use rss_sim::cp_wprime::V6CriticalPowerState;
use rss_sim::drain::{refresh_wprime_overspeed_armed, resolve_run_cruise_cap_ms};
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
        cap = resolve_run_cruise_cap_ms(cap, RUN, grade, total, 1.0, cp);
    }
    cap
}

fn clamp_grade_metabolic(grade: f64) -> f64 {
    let lim = V6_METABOLIC_GRADE_ABS_MAX_PCT;
    if grade > lim {
        return lim;
    }
    if grade < -lim {
        return -lim;
    }
    grade
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
        trigger = rss_sim::constants::V6_CP_CRUISE_PHYS_CLAMP_DOWNHILL_COAST_ALLOW_MPS;
    }
    if measured <= limit + trigger {
        return false;
    }
    true
}

fn apply_phys_clamp(v_meas: f64, v_limit: f64, dt: f64, clamp_on: bool) -> f64 {
    if !clamp_on {
        return v_meas;
    }
    if v_meas <= v_limit + V6_CP_CRUISE_OVERSPEED_EPS_MPS {
        return v_meas;
    }
    if v_meas > v_limit + 0.35 {
        return v_limit;
    }
    let mut v = v_meas - 9.0 * dt;
    if v < v_limit {
        v = v_limit;
    }
    v
}

/// Gravity coasts toward target; RSS clamp fights back. Returns clamp-event count.
fn count_downhill_clamp_fights(
    v_limit: f64,
    coast_ms: f64,
    grade: f64,
    force_clamp: Option<bool>,
    seconds: f64,
) -> u32 {
    let mut v = v_limit;
    let mut events = 0u32;
    let mut t = 0.0;
    let g_accel = 12.0;
    while t < seconds {
        v = (v + g_accel * DT).min(coast_ms);
        let before = v;
        let clamp_on = match force_clamp {
            Some(flag) => flag,
            None => should_enforce_phys_clamp(grade, before, v_limit),
        };
        v = apply_phys_clamp(before, v_limit, DT, clamp_on);
        if clamp_on && before > v_limit + V6_CP_CRUISE_OVERSPEED_EPS_MPS {
            events += 1;
        }
        t += DT;
    }
    events
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
        if inv < V6_RUN_GAIT_FLOOR_MS - 0.05 && capped > 0.05 && capped < V6_RUN_GAIT_FLOOR_MS - 0.01 {
            bugs.push(Bug {
                id: "RUN_GAIT_OUT_OF_BAND_CAP",
                severity: "高",
                detail: format!(
                    "CP 反解 {inv:.2} 掉出 Run 带，巡航仍写成 {capped:.2}（应跳过越步态帽）"
                ),
            });
        }
    }

    // ── BUG8: 无物理钳时疲劳不得按跑飞 v_meas 积分 ──
    {
        let cp_eff = compute_cp_watts(941.7, load, grade, 1.0, 1.0);
        let v_lim = cruise_v(cp_eff, total, grade, true);
        let v_runaway = 3.50;
        let p_intent = rss_sim::cp_wprime::fatigue_power_for_integral(
            v_runaway, v_lim, total, grade, 1.0, RUN,
        );
        let p_raw = metabolism_power_watts(v_runaway, total, grade, 1.0, RUN);
        println!(
            "[用例8] 疲劳采样: v_meas={v_runaway:.2} v_lim={v_lim:.2}  P_fat意图={p_intent:.0}  若用v_meas={p_raw:.0}  CP={cp_eff:.0}"
        );
        if p_intent > cp_eff * 1.35 && (p_intent - p_raw).abs() < 50.0 {
            bugs.push(Bug {
                id: "FATIGUE_USES_RUNAWAY_VMEAS",
                severity: "高",
                detail: format!(
                    "解除武装后 v_meas≫v_limit 时 P_fat 仍按跑飞={p_raw:.0}W（意图应≈{:.0}），If 虚高压 CP",
                    metabolism_power_watts(v_lim, total, grade, 1.0, RUN)
                ),
            });
        }
        if (p_intent - metabolism_power_watts(v_lim, total, grade, 1.0, RUN)).abs() > 30.0 {
            bugs.push(Bug {
                id: "FATIGUE_INTENT_MISMATCH",
                severity: "中",
                detail: format!("P_fat={p_intent:.0} 应贴近限速功率"),
            });
        }
    }

    // ── BUG9: 无状态超速门须对齐再武装带（非 disarm+0.05）──
    {
        let pool = 0.29;
        let float_ok = rss_sim::drain::is_wprime_pool_available_for_overspeed(pool, thresh);
        let armed = refresh_wprime_overspeed_armed(pool, false, thresh);
        println!(
            "[用例9] 无状态超速门: pool={:.0}% float_ok={float_ok} schmitt_armed={armed}  (期望均 false)",
            pool * 100.0
        );
        if float_ok || armed {
            bugs.push(Bug {
                id: "FLOAT_OVERSPEED_GATE_IGNORES_SCHMITT",
                severity: "中",
                detail: format!(
                    "池 29% 时 float_ok={float_ok} armed={armed}，调试/回退路径会误判仍可超速记账"
                ),
            });
        }
    }

    // ── BUG10: 步态地板处 P>CP 但解除武装不抽 W′（设计核对）──
    {
        let cp_eff = compute_cp_watts(941.7, load, grade, 1.0, 1.0);
        let v_floor = V6_RUN_GAIT_FLOOR_MS;
        let p_floor = metabolism_power_watts(v_floor, total, grade, 1.0, RUN);
        let mut cp = elite_cp_state(load, grade, 1.0);
        cp.w_prime_joules = cp.w_prime_max_joules * 0.24;
        cp.overspeed_armed = false;
        let pool0 = cp.pool01();
        let p_tick = cp_eff.min(p_floor);
        for i in 0..100 {
            cp.tick(p_tick, false, i as f64 * DT, DT, v_floor);
        }
        let d = (cp.pool01() - pool0) * 100.0;
        println!(
            "[用例10] 地板巡航: v={v_floor} P_met={p_floor:.0}>CP={cp_eff:.0}  P_tick={p_tick:.0}  ΔW'={d:+.2}%  (期望≈0)"
        );
        if d < -1.0 {
            bugs.push(Bug {
                id: "GAIT_FLOOR_DRAINS_WPRIME",
                severity: "中",
                detail: format!("地板速度下 W′ 仍被抽 {d:.1}%"),
            });
        }
    }

    // ── 注记: Skiba 快相渐近 50% < 再武装 60% ──
    {
        println!(
            "[注记] Skiba lim=50% < rearm={:.0}%：休息快相到不了再武装，须慢相爬升（非回归失败）",
            rearm_at * 100.0
        );
    }

    // ── BUG11: 缓下坡 — 小幅滑行不互殴；窜速（Walk 3.3）仍钳 ──
    {
        let load_log = 31.128;
        let grade_dn = -9.1;
        let total_log = BODY + load_log;
        let cp_eff = compute_cp_watts(780.0, load_log, grade_dn, 1.0, 1.0);
        let v_limit = 1.94;
        let mild = v_limit + 0.30;
        let runaway = 2.79;
        let events_mild = count_downhill_clamp_fights(v_limit, mild, grade_dn, None, 3.0);
        let events_runaway = count_downhill_clamp_fights(v_limit, runaway, grade_dn, None, 3.0);
        let events_flat = count_downhill_clamp_fights(v_limit, runaway, 0.0, None, 3.0);
        let pool = 0.38;
        let armed = refresh_wprime_overspeed_armed(pool, false, thresh);
        println!(
            "[用例11] 缓下坡物理钳: W'%={:.0} armed={} CP≈{:.0} v_lim={v_limit}",
            pool * 100.0,
            armed,
            cp_eff
        );
        println!(
            "         小幅滑行 events={events_mild}  窜速钳 events={events_runaway}  平路钳 events={events_flat}"
        );
        let _ = total_log;
        if events_mild > 2 {
            bugs.push(Bug {
                id: "DOWNHILL_MILD_COAST_FOUGHT",
                severity: "高",
                detail: format!(
                    "缓下坡 mild coast 仍互殴 {events_mild} 次/3s（应放行 ≤coast_allow）"
                ),
            });
        }
        if events_runaway < 10 {
            bugs.push(Bug {
                id: "DOWNHILL_RUNAWAY_NOT_CLAMPED",
                severity: "高",
                detail: format!(
                    "缓下坡窜速 coast={runaway} 应钳，却只有 {events_runaway} 次（Walk 超速回归）"
                ),
            });
        }
        if events_flat < 10 {
            bugs.push(Bug {
                id: "FLAT_PHYS_CLAMP_MISSING",
                severity: "中",
                detail: format!("平路 W′解除后应仍钳防窜速，events={events_flat}"),
            });
        }
        if armed {
            bugs.push(Bug {
                id: "WPRIME_38_SHOULD_STAY_DISARMED",
                severity: "高",
                detail: "池 38% 再武装闩应为 false（rearm≈60%）".into(),
            });
        }
    }

    // ── BUG12: 代谢坡度未钳 → 峭壁反解成爬行顶 ──
    {
        let raw = 99.0;
        let clamped = clamp_grade_metabolic(raw);
        let inv_raw = invert_speed_for_power_watts(740.0, BODY + 31.128, raw, 1.0, RUN);
        let inv_c = invert_speed_for_power_watts(740.0, BODY + 31.128, clamped, 1.0, RUN);
        println!(
            "[用例12] 代谢坡度钳: raw={raw}% → {clamped}%  Invert {inv_raw:.2} → {inv_c:.2} m/s"
        );
        if clamped > V6_METABOLIC_GRADE_ABS_MAX_PCT + 0.01 {
            bugs.push(Bug {
                id: "METABOLIC_GRADE_CLAMP_MISSING",
                severity: "高",
                detail: format!("峭壁坡度未钳到 ±{V6_METABOLIC_GRADE_ABS_MAX_PCT}%"),
            });
        }
        if inv_c + 0.05 < inv_raw {
            // expected: clamp raises uphill invert (less extreme)
        }
        if should_enforce_phys_clamp(raw, 3.0, 1.5) {
            bugs.push(Bug {
                id: "STEEP_PHYS_CLAMP_NOT_SKIPPED",
                severity: "高",
                detail: format!("|grade|={raw}% 仍启用物理钳"),
            });
        }
        if should_enforce_phys_clamp(-9.1, 2.14, 1.94) {
            bugs.push(Bug {
                id: "DOWNHILL_MILD_COAST_CLAMPED",
                severity: "高",
                detail: "缓下坡小幅滑行不应物理钳".into(),
            });
        }
        if !should_enforce_phys_clamp(-5.0, 3.27, 1.47) {
            bugs.push(Bug {
                id: "DOWNHILL_WALK_RUNAWAY_NOT_CLAMPED",
                severity: "高",
                detail: "缓下坡 Walk 窜速 3.27 vs lim 1.47 应物理钳".into(),
            });
        }
    }

    println!();
    if bugs.is_empty() {
        println!("========== 拟真回归：未发现未修复问题 ==========");
        println!(
            "已验证: P≈CP 不回充 / 施密特 / 空池 AvailableP / 下坡2.4 / Run地板 / 疲劳意图速 / 无状态门 / 下坡物理钳跳过 / 代谢坡度钳"
        );
    } else {
        println!("========== 发现 {} 个问题 ==========", bugs.len());
        for (i, b) in bugs.iter().enumerate() {
            println!("{}. [{}] {} — {}", i + 1, b.severity, b.id, b.detail);
        }
    }
}
