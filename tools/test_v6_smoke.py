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
    return st.w_prime_joules < j0


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
    # Run 超速不得按 v_meas 记账
    if get_metabolic_accounting_velocity_ms(3.55, 1.15) != 1.15:
        return False
    if get_metabolic_accounting_velocity_ms(1.0, 1.15) != 1.0:
        return False
    # Sprint + W′ 可用才按 v_meas
    if get_metabolic_accounting_velocity_ms(3.55, 1.15, is_sprinting=True) != 3.55:
        return False
    if get_metabolic_accounting_velocity_ms(3.55, 1.15, w_prime_pool01=0.1, is_sprinting=True) != 1.15:
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
    p_acct_empty = get_metabolic_accounting_power_watts(
        3.55, 1.15, 125.0, 9.1, 2.24, 2, w_prime_pool01=0.1, is_sprinting=True
    )
    if abs(p_acct_empty - p_drain) > 1.0:
        return False
    if abs(p_acct_run - p_drain) > 1.0:
        return False
    return p_acct > p_drain * 1.5


def _overspeed_excess_drain_ok() -> bool:
    if get_client_overspeed_excess_drain_per_second(3.55, 1.15, 1.0, 125.0, 9.1, 2.24, 2) != 0.0:
        return False
    extra = get_client_overspeed_excess_drain_per_second(
        3.55, 1.15, 0.1, 125.0, 9.1, 2.24, 2, 380.0
    )
    if extra <= 0.00001:
        return False
    base = get_client_overspeed_excess_drain_per_second(1.0, 1.15, 0.1, 125.0, 9.1, 2.24, 2)
    return base == 0.0


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


SCENARIOS = [
    ("drain_applied_limit", lambda: get_drain_velocity_ms(5.5, 4.0) == 4.0),
    ("overspeed_accounting", lambda: _overspeed_accounting_ok()),
    ("overspeed_excess_drain", lambda: _overspeed_excess_drain_ok()),
    ("metabolism_power_positive", lambda: metabolism_power_watts(1.4, 125.0) > 100.0),
    ("lcda_walk_30kg_level", lambda: _lcda_walk_30kg_ok()),
    ("anchors_batch_feasibility", lambda: _anchors_and_batch_ok()),
    ("invert_speed_monotonic", lambda: invert_speed_for_power_watts(500.0, 125.0, movement_phase=2) > 0.8),
    ("cp_load_penalty", lambda: compute_cp_watts(400.0, 35.0, 0.0) < 400.0),
    ("wprime_discharge", lambda: _wprime_discharge_ok()),
    ("wprime_discharge_run", lambda: _wprime_discharge_run_ok()),
    ("elite_sprint_duration", lambda: simulate_v6_sprint_seconds(35.0, 400.0) <= 15.0),
    ("fatigue_cap_clamp", lambda: _fatigue_cap_clamp_ok()),
    ("sustain_run_observed", lambda: _sustain_run_observed_ok()),
    ("mobility_run_speed", lambda: _mobility_ok()),
    ("sprint_load_30m", lambda: _sprint_load_30m_ok()),
    ("march_4h_aerobic_end", lambda: _march_4h_ok()),
    ("tier_scalar_gradients", lambda: _tier_scalar_gradients_ok()),
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
