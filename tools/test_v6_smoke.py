#!/usr/bin/env python3
"""v6 smoke tests (CP-W' + metabolism twin)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from rss_digital_twin_fix import (
    V6CriticalPowerState,
    V6_SPRINT_POWER_CAP_WATTS_DEFAULT,
    compute_cp_watts,
    get_drain_velocity_ms,
    get_metabolic_accounting_velocity_ms,
    get_metabolic_accounting_power_watts,
    get_client_overspeed_excess_drain_per_second,
    is_metabolic_overspeed_accounting,
    invert_speed_for_power_watts,
    invert_cruise_cap_ms,
    resolve_run_cruise_cap_ms,
    get_metabolic_speed_cap_ms,
    get_metabolic_corrected_speed_multiplier,
    metabolism_power_watts,
    simulate_v6_sprint_seconds,
    RSSDigitalTwin,
    RSSConstants,
    TwinFatigueSystem,
    MovementType,
    Stance,
)


def _wprime_discharge_ok() -> bool:
    st = V6CriticalPowerState(cp0=400.0)
    st.set_runtime_context(0.0, 0.0, 1.0, 0.0)
    dt = 0.017
    cap = 1450.0
    t = 0.0
    for _ in range(900):
        cp = st.get_effective_critical_power_watts()
        burst = st.w_prime_joules / dt
        p = min(cap, cp + burst)
        st.tick(p, True, t, dt)
        t += dt
    return st.pool01 < 0.25


def _wprime_discharge_run_ok() -> bool:
    st = V6CriticalPowerState(cp0=400.0)
    st.set_runtime_context(35.0, 0.0, 1.0, 0.0)
    dt = 0.017
    cp = st.get_effective_critical_power_watts()
    power_run = cp + 120.0
    j0 = st.w_prime_joules
    st.tick(power_run, False, 0.0, dt, 1.2)
    if st.w_prime_joules >= j0:
        return False
    # 恢复门槛：P < CP-40 才回充；P=CP 不回充
    st2 = V6CriticalPowerState(cp0=400.0)
    st2.set_runtime_context(0.0, 0.0, 1.0, 0.0)
    st2.w_prime_joules = st2.w_prime_max_joules * 0.5
    cp2 = st2.get_effective_critical_power_watts()
    j_mid = st2.w_prime_joules
    st2.tick(cp2, False, 0.0, 1.0, 1.2)
    if st2.w_prime_joules != j_mid:
        return False
    st2.tick(cp2 - 50.0, False, 1.0, 1.0, 1.2)
    return st2.w_prime_joules > j_mid


def _fatigue_cap_clamp_ok() -> bool:
    t = RSSDigitalTwin(RSSConstants())
    t.fatigue = TwinFatigueSystem()
    t.fatigue.fatigue_accumulation = 0.05
    t.fatigue.fatigue_integral = 0.05 / 0.3
    before = 1.0
    t.stamina = before
    after = t._apply_stamina_cap_clamp(before, before - 0.001)
    if after > t.fatigue.get_max_stamina_cap() + 0.001:
        return False
    w = 125.0
    for i in range(30):
        t.game_player_tick(
            MovementType.RUN,
            w,
            0.0,
            1.0,
            Stance.STAND,
            i * 0.017,
            0.017,
            enable_randomness=False,
        )
    if t.stamina >= 0.99:
        return False
    if abs(t.stamina - t.fatigue.get_max_stamina_cap()) > 0.02:
        return False
    return True


def _sustain_run_observed_ok() -> bool:
    from rss_constraints_v6 import check_sustain_run_observed
    return check_sustain_run_observed(None, duration_s=60.0).passed


def _overspeed_accounting_ok() -> bool:
    # 记账一律 v_meas；超速标志仍可用于调试 / CP 压速
    if get_metabolic_accounting_velocity_ms(3.55, 1.15) != 3.55:
        return False
    if get_metabolic_accounting_velocity_ms(1.0, 1.15) != 1.0:
        return False
    if get_metabolic_accounting_velocity_ms(3.55, 1.15, is_sprinting=True) != 3.55:
        return False
    if get_metabolic_accounting_velocity_ms(3.55, 1.15, w_prime_pool01=0.1, is_sprinting=True) != 3.55:
        return False
    if not is_metabolic_overspeed_accounting(3.55, 1.15):
        return False
    if is_metabolic_overspeed_accounting(1.0, 1.15):
        return False
    p_drain = metabolism_power_watts(get_drain_velocity_ms(3.55, 1.15), 125.0, 9.1, 2.24, 2)
    p_acct = get_metabolic_accounting_power_watts(
        3.55, 1.15, 125.0, 9.1, 2.24, 2, is_sprinting=True
    )
    p_acct_run = get_metabolic_accounting_power_watts(3.55, 1.15, 125.0, 9.1, 2.24, 2)
    if abs(p_acct - p_drain) > 1.0:
        return False
    if abs(p_acct_run - p_drain) > 1.0:
        return False
    return True


def _overspeed_excess_drain_ok() -> bool:
    # 武装透支只烧 W′，STA 税为 0（drain-only 与代谢伺服两种模式一致）
    tax_armed = get_client_overspeed_excess_drain_per_second(
        3.55, 1.15, 1.0, 125.0, 9.1, 2.24, 2, 380.0, True
    )
    if tax_armed != 0.0:
        return False
    # 解除武装 + 物理超限速：Run 仍走 P−CP 步态税（上限 0.4%/s），不得因略超 v_limit 变便宜
    tax_disarmed = get_client_overspeed_excess_drain_per_second(
        3.55, 1.15, 0.1, 125.0, 9.1, 2.24, 2, 380.0, False
    )
    if tax_disarmed < 0.0005:
        return False
    if tax_disarmed > 0.004 + 1e-9:
        return False
    # P≪CP 且不超限速：税为 0
    tax_ok = get_client_overspeed_excess_drain_per_second(
        0.8, 3.5, 0.1, 125.0, 0.0, 1.0, 1, 2000.0, False
    )
    return tax_ok == 0.0


def _run_cruise_out_of_band_skips_cap_ok() -> bool:
    # 日志工况：29 kg、27% 坡。Run 反解掉出步态带时不得把 Walk/爬行帽写到 Run。
    raw = invert_cruise_cap_ms(715.0, 118.868, 27.0, 1.35, 2)
    cap = resolve_run_cruise_cap_ms(raw, 2, 27.0, 118.868, 1.35, 715.0)
    if cap >= 0.0:
        return False
    return True


def _gait_excess_sta_tax_ok() -> bool:
    # W′ 空、帽内硬跑（v≈limit）：走 P−CP 步态税，约 0.05–0.4 %/s，不得为 0。
    tax = get_client_overspeed_excess_drain_per_second(
        2.5, 2.5, 0.0, 118.868, 27.0, 1.35, 2, 715.0, False
    )
    if tax < 0.0005:
        return False
    if tax > 0.004 + 1e-9:
        return False
    tax_overspeed = get_client_overspeed_excess_drain_per_second(
        3.1, 2.2, 0.0, 118.868, 27.0, 1.35, 2, 715.0, False
    )
    if tax_overspeed < 0.0005:
        return False
    if tax_overspeed > 0.004 + 1e-9:
        return False
    return True


def _cruise_hike_floor_ok() -> bool:
    # 日志工况：29 kg、27% 坡、η=1.35、CP=715 W。真反解 ≈0.4 m/s，巡航帽应托在徒步地板。
    v = invert_cruise_cap_ms(715.0, 118.868, 27.0, 1.35, 1)
    if v < 0.99:
        return False
    if v > 1.45:
        return False
    raw = invert_speed_for_power_watts(715.0, 118.868, 27.0, 1.35, 1)
    if raw >= v:
        return False
    return True


def _walk_metabolic_cap_hike_floor_ok() -> bool:
    # 实机：W′ 空、Walk、~20% 坡、29 kg、η=1.44、CP=716。裸反解 ~0.5 m/s；
    # 代谢帽必须走 InvertCruiseCapMs（地板 1.0），不得把 v_limit 写成 0.45–0.58。
    cap = get_metabolic_speed_cap_ms(
        1.8,
        1,
        118.868,
        20.4,
        1.44,
        False,
        716.0,
        w_prime_pool01=0.0,
        speed_for_power_eval_ms=1.8,
        overspeed_armed=False,
    )
    if cap < 0.99:
        return False
    if cap > 1.45:
        return False
    walk_top = 1.45
    coord_frac = 1.0 / walk_top
    frac = get_metabolic_corrected_speed_multiplier(
        coord_frac,
        1.8,
        1,
        118.868,
        20.4,
        1.44,
        False,
        walk_top,
        716.0,
        w_prime_pool01=0.0,
        applied_speed_limit_ms=1.0,
        overspeed_armed=False,
    )
    if frac * walk_top < 0.99:
        return False
    low = get_metabolic_corrected_speed_multiplier(
        0.41,
        1.8,
        1,
        118.868,
        20.4,
        1.44,
        False,
        walk_top,
        716.0,
        w_prime_pool01=0.0,
        applied_speed_limit_ms=0.58,
        overspeed_armed=False,
    )
    if low < 0.50 - 1e-9:
        return False
    return True


def _exhausted_run_keeps_gait_floor_ok() -> bool:
    # 条空仍按住 Run：代谢帽跳过，但不得把 0.15 爬行倍率写进 Run 相位。
    frac = get_metabolic_corrected_speed_multiplier(
        0.15,
        3.0,
        2,
        118.868,
        10.0,
        1.35,
        True,
        3.64,
        716.0,
        w_prime_pool01=0.0,
        overspeed_armed=False,
    )
    if frac < 0.50 - 1e-9:
        return False
    twin = RSSDigitalTwin(RSSConstants())
    limp_ms = twin.get_dynamic_limp_speed_ms(0.0743022)
    if limp_ms < 0.99:
        return False
    if limp_ms > 1.45 + 1e-9:
        return False
    wrong_phase_threshold_ms = 3.2 * (1.0 - 0.0743022)
    if limp_ms >= wrong_phase_threshold_ms - 0.1:
        return False
    return True


def _march_4h_ok() -> bool:
    from rss_constraints_v6 import check_march_4h_aerobic_end
    return check_march_4h_aerobic_end().passed


def _mobility_ok() -> bool:
    from rss_constraints_v6 import (
        check_mobility_run_speed,
        MOBILITY_RUN_0KG_MIN_MS,
        MOBILITY_RUN_0KG_MAX_MS,
        MOBILITY_RUN_35KG_MIN_MS,
        MOBILITY_RUN_35KG_MAX_MS,
    )
    c0 = check_mobility_run_speed(0.0, MOBILITY_RUN_0KG_MIN_MS, MOBILITY_RUN_0KG_MAX_MS, "0kg")
    c35 = check_mobility_run_speed(35.0, MOBILITY_RUN_35KG_MIN_MS, MOBILITY_RUN_35KG_MAX_MS, "35kg")
    return c0.passed and c35.passed


def _zero_load_2mile_constants_ok() -> bool:
    from rss_constraints_v6 import (
        TWO_MILE_DIST_M,
        TWO_MILE_HARD,
        TWO_MILE_HARD_MAX_SEC,
        TWO_MILE_MAX_SEC,
        TWO_MILE_SCORE_70_SEC,
        TWO_MILE_SCORE_85_SEC,
        two_mile_score_01,
    )

    if abs(TWO_MILE_DIST_M - 3218.688) > 0.01:
        return False
    if TWO_MILE_MAX_SEC != TWO_MILE_SCORE_70_SEC:
        return False
    if abs(TWO_MILE_SCORE_70_SEC - 1080.0) > 1e-6:
        return False
    if abs(TWO_MILE_SCORE_85_SEC - 930.0) > 1e-6:
        return False
    if abs(TWO_MILE_HARD_MAX_SEC - TWO_MILE_SCORE_70_SEC) > 1e-6:
        return False
    if abs(TWO_MILE_HARD_MAX_SEC - 1080.0) > 1e-6:
        return False
    if abs(two_mile_score_01(1080.0) - 0.70) > 1e-6:
        return False
    if abs(two_mile_score_01(930.0) - 0.85) > 1e-6:
        return False
    return bool(TWO_MILE_HARD)


def _walk_not_faster_than_demoted_run_ok() -> bool:
    """W′ 空时：Walk 不得快过同条件降速 Run（重装/低 CP）。"""
    from rss_pipeline_v6 import load_preset_params
    from rss_digital_twin_fix import merge_game_aligned_params

    params = dict(load_preset_params("StandardMilsim"))
    params["critical_power_watts"] = 400.0
    constants = RSSConstants(**merge_game_aligned_params(params))
    for total_w in (110.0, 125.0, 135.0):
        for grade in (0.0, 5.0, 10.0):
            twin = RSSDigitalTwin(constants)
            twin.reset()
            twin.v6_cp_state.w_prime_joules = 0.0
            v_run = twin.calculate_actual_speed(
                1.0, total_w, MovementType.RUN, 2.0, grade_percent=grade, current_time=10.0
            )
            v_walk = twin.calculate_actual_speed(
                1.0, total_w, MovementType.WALK, 1.0, grade_percent=grade, current_time=10.0
            )
            if v_walk > v_run + 0.02:
                return False
    return True


def _cp_walk_override_latch_ok() -> bool:
    """W′ 解除武装且 Run 反解掉出带：game_player_tick 切 Walk 并锁存。"""
    from rss_pipeline_v6 import load_preset_params
    from rss_digital_twin_fix import (
        ENGINE_WALK_TOP_MS,
        merge_game_aligned_params,
        RSS_PLAYER_TICK_SEC,
    )

    params = dict(load_preset_params("StandardMilsim"))
    params["critical_power_watts"] = 400.0
    constants = RSSConstants(**merge_game_aligned_params(params))
    twin = RSSDigitalTwin(constants)
    twin.reset()
    twin.v6_cp_state.w_prime_joules = 0.0
    twin.v6_cp_state.overspeed_armed = False
    total_w = 90.0 + 28.868
    dt = RSS_PLAYER_TICK_SEC
    t = 0.0
    last_v = 0.0
    i = 0
    while i < 30:
        last_v = twin.game_player_tick(
            MovementType.RUN, total_w, 12.0, 1.35, Stance.STAND, t, dt
        )
        t = t + dt
        i = i + 1
    if not twin._cp_walk_override_active:
        return False
    walk_ceil = ENGINE_WALK_TOP_MS + 0.35
    if last_v > walk_ceil:
        return False
    if twin._measured_velocity_ms > walk_ceil:
        return False
    return True


def _cp_walk_override_no_false_wprime_burn_ok() -> bool:
    """步态覆盖后 Walk 顶内速度不得因徒步地板 1.0 假超速烧 W′；下坡应能回充。"""
    from rss_pipeline_v6 import load_preset_params
    from rss_digital_twin_fix import (
        merge_game_aligned_params,
        RSS_PLAYER_TICK_SEC,
        is_walk_override_in_band_cruise,
        is_phys_overspeed_for_anaerobic_tick,
    )

    if is_phys_overspeed_for_anaerobic_tick(1.47, 1.0, True):
        return False
    if not is_walk_override_in_band_cruise(True, 1.47):
        return False
    if not is_phys_overspeed_for_anaerobic_tick(1.47, 1.0, False):
        return False
    if not is_phys_overspeed_for_anaerobic_tick(2.80, 1.0, True):
        return False

    params = dict(load_preset_params("StandardMilsim"))
    params["critical_power_watts"] = 400.0
    constants = RSSConstants(**merge_game_aligned_params(params))
    twin = RSSDigitalTwin(constants)
    twin.reset()
    twin.v6_cp_state.w_prime_joules = 0.0
    twin.v6_cp_state.overspeed_armed = False
    total_w = 90.0 + 28.868
    dt = RSS_PLAYER_TICK_SEC
    t = 0.0
    i = 0
    while i < 30:
        twin.game_player_tick(
            MovementType.RUN, total_w, 12.0, 1.35, Stance.STAND, t, dt
        )
        t = t + dt
        i = i + 1
    if not twin._cp_walk_override_active:
        return False

    w_max = twin.v6_cp_state.w_prime_max_joules
    if w_max < 100.0:
        return False
    start_j = w_max * 0.23
    twin.v6_cp_state.w_prime_joules = start_j
    twin.v6_cp_state.overspeed_armed = False

    i = 0
    while i < 180:
        twin.game_player_tick(
            MovementType.RUN, total_w, 12.0, 1.35, Stance.STAND, t, dt
        )
        t = t + dt
        i = i + 1
    if not twin._cp_walk_override_active:
        return False
    dumped = start_j - twin.v6_cp_state.w_prime_joules
    if dumped > w_max * 0.02:
        return False

    after_uphill = twin.v6_cp_state.w_prime_joules
    i = 0
    while i < 180:
        twin.game_player_tick(
            MovementType.RUN, total_w, -12.0, 1.35, Stance.STAND, t, dt
        )
        t = t + dt
        i = i + 1
    if twin.v6_cp_state.w_prime_joules <= after_uphill + 1.0:
        return False
    return True


def _wprime_disarm_apply_path_ok() -> bool:
    """控制环时间序列：W′ 解除武装后 applied 不得 SNAP；legacy 策略必须被检出失败。"""
    from rss_apply_path_twin import wprime_disarm_apply_path_regression_ok

    return wprime_disarm_apply_path_regression_ok()


def _downhill_phys_clamp_policy_ok() -> bool:
    """v6.1.7 起默认：物理钳关闭；代谢限速开；代谢坡度±45；施密特滞回仍有效。"""
    from rss_digital_twin_fix import (
        apply_cp_cruise_physics_cap,
        clamp_grade_percent_for_metabolic_speed,
        should_enforce_cp_cruise_physics_cap,
        invert_speed_for_power_watts,
        refresh_wprime_overspeed_armed,
        V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP,
        V6_APPLY_CP_METABOLIC_SPEED_CAP,
    )

    if V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP:
        return False
    if not V6_APPLY_CP_METABOLIC_SPEED_CAP:
        return False

    # 物理钳关闭后任意坡度都不应 enforce
    if should_enforce_cp_cruise_physics_cap(0.0, 3.0, 1.5):
        return False
    if should_enforce_cp_cruise_physics_cap(-5.0, 3.0, 1.5):
        return False
    v_passthrough = apply_cp_cruise_physics_cap(3.22, 1.47, 0.018, 3.5, movement_phase=1)
    if abs(v_passthrough - 3.22) > 1e-6:
        return False

    g = clamp_grade_percent_for_metabolic_speed(99.0)
    if abs(g - 45.0) > 1e-6:
        return False
    g2 = clamp_grade_percent_for_metabolic_speed(-99.0)
    if abs(g2 + 45.0) > 1e-6:
        return False

    if refresh_wprime_overspeed_armed(0.38, False):
        return False

    inv_raw = invert_speed_for_power_watts(740.0, 121.128, 99.0, 1.0, 2)
    inv_c = invert_speed_for_power_watts(740.0, 121.128, g, 1.0, 2)
    if inv_c + 0.05 < inv_raw:
        return False
    return True


def _tier_scalar_gradients_ok() -> bool:
    from rss_pipeline_v6 import V6Metrics, scalarize_tier_metrics, make_mo_sampler

    hard = V6Metrics(0.30, 0.05, 0.690, 0.0010, 1.5)
    easy = V6Metrics(0.30, 0.02, 0.700, 0.0020, 1.5)
    elite_hard = scalarize_tier_metrics(hard, 0.36, "EliteStandard")
    elite_easy = scalarize_tier_metrics(easy, 0.20, "EliteStandard")
    tac_hard = scalarize_tier_metrics(hard, 0.36, "TacticalAction")
    tac_easy = scalarize_tier_metrics(easy, 0.20, "TacticalAction")
    if elite_hard >= elite_easy:
        return False
    if tac_easy >= tac_hard:
        return False
    sampler = make_mo_sampler("nsga3", 92)
    return sampler is not None



def _sprint_load_30m_ok() -> bool:
    """21.6 kg 战斗装 30 m 冲刺：稳态用时接近军事文献 ~8.2 s（空载步态 4.5 m/s）。"""
    from rss_digital_twin_fix import (
        encumbrance_speed_penalty_base,
        RSSConstants,
        merge_game_aligned_params,
        RSSDigitalTwin,
        MovementType,
        SPRINT_ENCUMBRANCE_PENALTY_MULT,
    )
    from rss_constraints_v6 import _load_elite_preset_params

    if abs(SPRINT_ENCUMBRANCE_PENALTY_MULT - 2.2) > 1e-6:
        return False
    c = RSSConstants(**merge_game_aligned_params(_load_elite_preset_params()))
    twin = RSSDigitalTwin(c)
    game_max = 5.5
    v_sprint0 = 4.5
    enc = encumbrance_speed_penalty_base(c, 90.0 + 21.6)
    v = v_sprint0
    pen = 0.0
    for _ in range(40):
        sr = min(1.0, max(0.0, v / game_max))
        pen = enc * (1.0 + sr) * SPRINT_ENCUMBRANCE_PENALTY_MULT
        if pen > 0.75:
            pen = 0.75
        if pen < 0.0:
            pen = 0.0
        v_new = twin.get_v5_absolute_speed_ms(MovementType.SPRINT, True, 1.0, pen, 1.0)
        if abs(v_new - v) < 1e-5:
            v = v_new
            break
        v = 0.5 * v + 0.5 * v_new
    t30 = 30.0 / v
    return 7.8 <= t30 <= 8.6


def _lcda_walk_30kg_ok() -> bool:
    """Walk@1.34 m/s + 30 kg：LCDA 平地功率应明显高于旧 Pandolf，并落在文献带。"""
    p_damped = metabolism_power_watts(1.34, 120.0, 0.0, 1.0, MovementType.WALK, 0.70)
    p_full = metabolism_power_watts(1.34, 120.0, 0.0, 1.0, MovementType.WALK, 1.0)
    if p_full < 480.0 or p_full > 540.0:
        return False
    if p_damped < 430.0 or p_damped > 510.0:
        return False
    p_down = metabolism_power_watts(1.34, 120.0, -10.0, 1.0, MovementType.WALK, 1.0)
    p_up = metabolism_power_watts(1.34, 120.0, 10.0, 1.0, MovementType.WALK, 1.0)
    if p_down >= p_full:
        return False
    if p_up <= p_full:
        return False
    return True


def _anchors_and_batch_ok() -> bool:
    from rss_anchors_v6 import compile_march_cp_anchors, min_cp0_for_march_cruise
    from rss_pipeline_v6 import sample_lhs_params
    from rss_sim_backend import batch_evaluate_hard_constraints

    a = compile_march_cp_anchors()
    if a.min_cp0 < 700.0 or a.min_cp0 > 850.0:
        return False
    if a.elite_cp0 > a.standard_cp0 or a.standard_cp0 > a.tactical_cp0:
        return False
    need = min_cp0_for_march_cruise()
    if abs(need - a.min_cp0) > 1.0:
        return False
    batch = sample_lhs_params(8, seed=7)
    reports = batch_evaluate_hard_constraints(batch, fast_mode=True)
    if len(reports) != 8:
        return False
    return True


def _wprime_engine_fx_map_ok() -> bool:
    """Mirror SCR_RSS_SprintGate.MapWPrimePoolToEngineDisplay / ComputeEnginePresentationDisplay."""
    start = 0.50
    floor_val = 0.12

    def map_w(w: float) -> float:
        if w >= start:
            return 1.0
        t = max(0.0, min(1.0, w)) / start
        return floor_val + (1.0 - floor_val) * t

    def compute(aerobic: float, w: float, sprint_allowed: bool, sprint_intent: bool) -> float:
        display = max(0.0, min(1.0, aerobic))
        wm = map_w(w)
        if wm < display:
            display = wm
        if (not sprint_allowed) and sprint_intent:
            block = 0.20 - 0.01
            if block < 0.0:
                block = 0.0
            if block < display:
                display = block
        return display

    if abs(map_w(1.0) - 1.0) > 1e-6:
        return False
    if abs(map_w(0.50) - 1.0) > 1e-6:
        return False
    if abs(map_w(0.0) - floor_val) > 1e-6:
        return False
    mid = map_w(0.25)
    if mid <= floor_val or mid >= 1.0:
        return False
    # W′ empty + full aerobic → presentation at floor (Exhaustion ≈ 0.88)
    if abs(compute(0.95, 0.0, True, False) - floor_val) > 1e-6:
        return False
    # Sprint block wins when lower
    if abs(compute(0.95, 1.0, False, True) - 0.19) > 1e-6:
        return False
    return True


def _elite_baked_params() -> tuple:
    """烘焙 Elite 档 CP/sprint_cap/W′_max（与 SettingsPresetBake.c 对齐）。"""
    from rss_pipeline_v6 import load_preset_params

    p = load_preset_params("EliteStandard")
    return (
        float(p["critical_power_watts"]),
        float(p["sprint_power_cap_watts"]),
        float(p["w_prime_max_joules"]),
    )


def _elite_sprint_duration_ok() -> bool:
    """Elite 烘焙档 35kg 全冲刺到 ANA 门槛 ≤ 15s（doc §4.4 硬约束）。"""
    cp, cap, w_max = _elite_baked_params()
    return simulate_v6_sprint_seconds(35.0, cp, sprint_cap_w=cap, w_prime_max=w_max) <= 15.0


def _baked_preset_drift_guard_ok() -> bool:
    """三档阶梯叙事守卫：CP/sprint_cap/W′恢复单调递增（Elite≤Standard≤Tactical），W′_max Elite 最小。

    不硬编码具体数值（重新调优后数值会变），只守护「档位不倒置」这一叙事。
    """
    from rss_pipeline_v6 import load_preset_params

    names = ("EliteStandard", "StandardMilsim", "TacticalAction")
    p = {n: load_preset_params(n) for n in names}
    e, s, t = p["EliteStandard"], p["StandardMilsim"], p["TacticalAction"]

    def monotonic(key: str) -> bool:
        return (
            float(e[key]) <= float(s[key]) <= float(t[key])
        )

    if not monotonic("critical_power_watts"):
        return False
    if not monotonic("sprint_power_cap_watts"):
        return False
    if not monotonic("w_prime_recovery_w_per_s"):
        return False
    if not (float(e["w_prime_max_joules"]) <= float(s["w_prime_max_joules"])):
        return False
    return True


def _wprime_recovery_dispatch_ok() -> bool:
    """Skiba/线性按档位显式分派：Elite=Skiba(0)，Standard/Tactical=线性(1)。"""
    from rss_pipeline_v6 import load_preset_params

    expected = {"EliteStandard": 0.0, "StandardMilsim": 1.0, "TacticalAction": 1.0}
    for name, mode in expected.items():
        p = load_preset_params(name)
        if abs(float(p.get("w_prime_recovery_mode", 0.0)) - mode) > 0.01:
            return False
    skiba = V6CriticalPowerState(cp0=889.0, w_prime_recovery_mode=0.0)
    linear = V6CriticalPowerState(cp0=1010.0, w_prime_recovery_mode=1.0)
    if not skiba._uses_skiba_recovery():
        return False
    if linear._uses_skiba_recovery():
        return False
    return True


def _run_wprime_armed_29kg_ok() -> bool:
    """低 CP 应失败；慢跑口径 CP 应在 29 kg 平路 Run 60 s 后仍武装。"""
    from rss_constraints_v6 import check_run_wprime_armed_29kg_60s

    low = check_run_wprime_armed_29kg_60s(
        {
            "critical_power_watts": 800.0,
            "w_prime_max_joules": 25000.0,
            "sprint_power_cap_watts": 3000.0,
            "encumbrance_speed_penalty_coeff": 0.34,
            "v5_run_speed_ms": 3.05,
        }
    )
    high = check_run_wprime_armed_29kg_60s(
        {
            "critical_power_watts": 1600.0,
            "w_prime_max_joules": 25000.0,
            "sprint_power_cap_watts": 3600.0,
            "encumbrance_speed_penalty_coeff": 0.34,
            "v5_run_speed_ms": 3.05,
        }
    )
    if low.passed:
        return False
    if not high.passed:
        return False
    # 档位移动性不得覆盖慢跑情景：Tactical 低负重/高速仍应过门（enc/v5 被钉死）
    tactical_gait = check_run_wprime_armed_29kg_60s(
        {
            "critical_power_watts": 1600.0,
            "w_prime_max_joules": 25000.0,
            "sprint_power_cap_watts": 3600.0,
            "encumbrance_speed_penalty_coeff": 0.18,
            "v5_run_speed_ms": 3.45,
        }
    )
    if not tactical_gait.passed:
        return False
    return True


SCENARIOS = [
    ("drain_applied_limit", lambda: get_drain_velocity_ms(5.5, 4.0) == 5.5),
    ("overspeed_accounting", lambda: _overspeed_accounting_ok()),
    ("overspeed_excess_drain", lambda: _overspeed_excess_drain_ok()),
    ("cruise_hike_floor", lambda: _cruise_hike_floor_ok()),
    ("walk_metabolic_cap_hike_floor", lambda: _walk_metabolic_cap_hike_floor_ok()),
    ("exhausted_run_keeps_gait_floor", lambda: _exhausted_run_keeps_gait_floor_ok()),
    ("run_cruise_out_of_band_skips_cap", lambda: _run_cruise_out_of_band_skips_cap_ok()),
    ("gait_excess_sta_tax", lambda: _gait_excess_sta_tax_ok()),
    ("metabolism_power_positive", lambda: metabolism_power_watts(1.4, 125.0) > 100.0),
    (
        "downhill_same_speed_savings",
        lambda: __import__("rss_constraints_v6", fromlist=["check_downhill_same_speed_savings"])
        .check_downhill_same_speed_savings()
        .passed,
    ),
    ("lcda_walk_30kg_level", lambda: _lcda_walk_30kg_ok()),
    ("anchors_batch_feasibility", lambda: _anchors_and_batch_ok()),
    ("invert_speed_monotonic", lambda: invert_speed_for_power_watts(500.0, 125.0, movement_phase=2) > 0.8),
    ("cp_load_penalty", lambda: compute_cp_watts(400.0, 35.0, 0.0) < 400.0),
    ("wprime_discharge", lambda: _wprime_discharge_ok()),
    ("wprime_discharge_run", lambda: _wprime_discharge_run_ok()),
    ("elite_sprint_duration", lambda: _elite_sprint_duration_ok()),
    ("baked_preset_drift_guard", lambda: _baked_preset_drift_guard_ok()),
    ("wprime_recovery_dispatch", lambda: _wprime_recovery_dispatch_ok()),
    ("run_wprime_armed_29kg_60s", lambda: _run_wprime_armed_29kg_ok()),
    ("fatigue_cap_clamp", lambda: _fatigue_cap_clamp_ok()),
    ("sustain_run_observed", lambda: _sustain_run_observed_ok()),
    ("mobility_run_speed", lambda: _mobility_ok()),
    ("zero_load_run_2mile_constants", lambda: _zero_load_2mile_constants_ok()),
    ("sprint_load_30m", lambda: _sprint_load_30m_ok()),
    ("march_4h_aerobic_end", lambda: _march_4h_ok()),
    ("tier_scalar_gradients", lambda: _tier_scalar_gradients_ok()),
    ("walk_not_faster_than_demoted_run", lambda: _walk_not_faster_than_demoted_run_ok()),
    ("cp_walk_override_latch", lambda: _cp_walk_override_latch_ok()),
    ("cp_walk_override_no_false_wprime_burn", lambda: _cp_walk_override_no_false_wprime_burn_ok()),
    ("downhill_phys_clamp_policy", lambda: _downhill_phys_clamp_policy_ok()),
    ("wprime_disarm_apply_path", lambda: _wprime_disarm_apply_path_ok()),
    ("wprime_engine_fx_map", lambda: _wprime_engine_fx_map_ok()),
    (
        "random_scenarios_quick",
        lambda: __import__(
            "test_rss_random_scenarios", fromlist=["run_quick"]
        ).run_quick(),
    ),
]


def main() -> int:
    failed = 0
    for name, fn in SCENARIOS:
        try:
            ok = fn()
        except Exception as exc:  # noqa: BLE001
            print(f"  [FAIL] {name}: {exc}")
            failed += 1
            continue
        if ok:
            print(f"  [PASS] {name}")
        else:
            print(f"  [FAIL] {name}")
            failed += 1
    if failed:
        print(f"test_v6_smoke: {failed} failure(s)")
        return 1
    print(f"test_v6_smoke: {len(SCENARIOS)}/{len(SCENARIOS)} passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
