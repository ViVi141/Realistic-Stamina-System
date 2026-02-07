import pytest
from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin, MovementType, Stance


def sprint_duration_for(const):
    twin = RSSDigitalTwin(const)
    twin.reset()
    twin.stamina = 1.0
    body = getattr(const, 'CHARACTER_WEIGHT', 90.0)
    current_weight = body + 29.0
    t = 0.0
    dt = 0.2
    for _ in range(int(300/ dt)):
        twin.step(speed=5.0, current_weight=current_weight, grade_percent=0.0, terrain_factor=1.0, stance=Stance.STAND, movement_type=MovementType.SPRINT, current_time=t, enable_randomness=False)
        t += dt
        if twin.stamina < 0.25:
            return t
    return float('inf')


def test_higher_coeff_decreases_duration():
    c1 = RSSConstants()
    c2 = RSSConstants()
    c1.ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.2
    c2.ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.4
    d1 = sprint_duration_for(c1)
    d2 = sprint_duration_for(c2)
    assert d2 <= d1


def test_higher_exp_decreases_duration():
    e1 = RSSConstants()
    e2 = RSSConstants()
    e1.ENCUMBRANCE_SPEED_PENALTY_EXPONENT = 1.2
    e2.ENCUMBRANCE_SPEED_PENALTY_EXPONENT = 2.0
    d1 = sprint_duration_for(e1)
    d2 = sprint_duration_for(e2)
    assert d2 <= d1


def test_max_penalty_cap_effect():
    m1 = RSSConstants()
    m2 = RSSConstants()
    m1.ENCUMBRANCE_SPEED_PENALTY_MAX = 0.6
    m2.ENCUMBRANCE_SPEED_PENALTY_MAX = 0.9
    m1.ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.4
    m2.ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.4
    d1 = sprint_duration_for(m1)
    d2 = sprint_duration_for(m2)
    # higher cap should not increase duration
    assert d2 <= d1
