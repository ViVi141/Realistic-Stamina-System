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
    if get_metabolic_accounting_velocity_ms(3.55, 1.15) != 3.55:
        return False
    if get_metabolic_accounting_velocity_ms(1.0, 1.15) != 1.0:
        return False
    if get_metabolic_accounting_velocity_ms(3.55, 1.15, w_prime_pool01=0.1) != 1.15:
        return False
    if not is_metabolic_overspeed_accounting(3.55, 1.15):
        return False
    if is_metabolic_overspeed_accounting(1.0, 1.15):
        return False
    p_drain = metabolism_power_watts(get_drain_velocity_ms(3.55, 1.15), 125.0, 9.1, 2.24, 2)
    p_acct = get_metabolic_accounting_power_watts(3.55, 1.15, 125.0, 9.1, 2.24, 2)
    p_acct_empty = get_metabolic_accounting_power_watts(
        3.55, 1.15, 125.0, 9.1, 2.24, 2, w_prime_pool01=0.1
    )
    if abs(p_acct_empty - p_drain) > 1.0:
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


SCENARIOS = [
    ("drain_applied_limit", lambda: get_drain_velocity_ms(5.5, 4.0) == 4.0),
    ("overspeed_accounting", lambda: _overspeed_accounting_ok()),
    ("overspeed_excess_drain", lambda: _overspeed_excess_drain_ok()),
    ("metabolism_power_positive", lambda: metabolism_power_watts(1.4, 125.0) > 100.0),
    ("invert_speed_monotonic", lambda: invert_speed_for_power_watts(500.0, 125.0, movement_phase=2) > 0.8),
    ("cp_load_penalty", lambda: compute_cp_watts(400.0, 35.0, 0.0) < 400.0),
    ("wprime_discharge", lambda: _wprime_discharge_ok()),
    ("wprime_discharge_run", lambda: _wprime_discharge_run_ok()),
    ("elite_sprint_duration", lambda: simulate_v6_sprint_seconds(35.0, 400.0) <= 15.0),
    ("fatigue_cap_clamp", lambda: _fatigue_cap_clamp_ok()),
    ("sustain_run_observed", lambda: _sustain_run_observed_ok()),
    ("march_4h_aerobic_end", lambda: _march_4h_ok()),
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
