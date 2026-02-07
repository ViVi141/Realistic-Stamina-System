import json
from rss_super_pipeline import RSSSuperPipeline
from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin


def load_params_to_constants(params: dict) -> RSSConstants:
    const = RSSConstants()
    mapping = {
        'energy_to_stamina_coeff': 'ENERGY_TO_STAMINA_COEFF',
        'base_recovery_rate': 'BASE_RECOVERY_RATE',
        'prone_recovery_multiplier': 'PRONE_RECOVERY_MULTIPLIER',
        'standing_recovery_multiplier': 'STANDING_RECOVERY_MULTIPLIER',
        'load_recovery_penalty_coeff': 'LOAD_RECOVERY_PENALTY_COEFF',
        'load_recovery_penalty_exponent': 'LOAD_RECOVERY_PENALTY_EXPONENT',
        'encumbrance_speed_penalty_coeff': 'ENCUMBRANCE_SPEED_PENALTY_COEFF',
        'encumbrance_stamina_drain_coeff': 'ENCUMBRANCE_STAMINA_DRAIN_COEFF',
        'sprint_stamina_drain_multiplier': 'SPRINT_STAMINA_DRAIN_MULTIPLIER',
        'fatigue_accumulation_coeff': 'FATIGUE_ACCUMULATION_COEFF',
        'fatigue_max_factor': 'FATIGUE_MAX_FACTOR',
        'aerobic_efficiency_factor': 'AEROBIC_EFFICIENCY_FACTOR',
        'anaerobic_efficiency_factor': 'ANAEROBIC_EFFICIENCY_FACTOR',
        'recovery_nonlinear_coeff': 'RECOVERY_NONLINEAR_COEFF',
        'fast_recovery_multiplier': 'FAST_RECOVERY_MULTIPLIER',
        'medium_recovery_multiplier': 'MEDIUM_RECOVERY_MULTIPLIER',
        'slow_recovery_multiplier': 'SLOW_RECOVERY_MULTIPLIER',
        'min_recovery_stamina_threshold': 'MIN_RECOVERY_STAMINA_THRESHOLD',
        'min_recovery_rest_time_seconds': 'MIN_RECOVERY_REST_TIME_SECONDS',
        'sprint_speed_boost': 'SPRINT_SPEED_BOOST',
        'posture_crouch_multiplier': 'POSTURE_CROUCH_MULTIPLIER',
        'posture_prone_multiplier': 'POSTURE_PRONE_MULTIPLIER',
        'jump_stamina_base_cost': 'JUMP_STAMINA_BASE_COST',
        'vault_stamina_start_cost': 'VAULT_STAMINA_START_COST',
        'climb_stamina_tick_cost': 'CLIMB_STAMINA_TICK_COST',
        'jump_consecutive_penalty': 'JUMP_CONSECUTIVE_PENALTY',
        'slope_uphill_coeff': 'SLOPE_UPHILL_COEFF',
        'slope_downhill_coeff': 'SLOPE_DOWNHILL_COEFF',
        'swimming_base_power': 'SWIMMING_BASE_POWER',
        'swimming_encumbrance_threshold': 'SWIMMING_ENCUMBRANCE_THRESHOLD',
        'swimming_static_drain_multiplier': 'SWIMMING_STATIC_DRAIN_MULTIPLIER',
        'swimming_dynamic_power_efficiency': 'SWIMMING_DYNAMIC_POWER_EFFICIENCY',
        'swimming_energy_to_stamina_coeff': 'SWIMMING_ENERGY_TO_STAMINA_COEFF',
        'env_heat_stress_max_multiplier': 'ENV_HEAT_STRESS_MAX_MULTIPLIER',
        'env_rain_weight_max': 'ENV_RAIN_WEIGHT_MAX',
        'env_wind_resistance_coeff': 'ENV_WIND_RESISTANCE_COEFF',
        'env_mud_penalty_max': 'ENV_MUD_PENALTY_MAX',
        'env_temperature_heat_penalty_coeff': 'ENV_TEMPERATURE_HEAT_PENALTY_COEFF',
        'env_temperature_cold_recovery_penalty_coeff': 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF'
    }

    for k, v in params.items():
        if k in mapping and hasattr(const, mapping[k]):
            setattr(const, mapping[k], v)
    return const


if __name__ == '__main__':
    # 读取已有平衡预设并验证会话约束
    with open('optimized_rss_config_balanced_super.json', 'r', encoding='utf-8') as f:
        cfg = json.load(f)

    params = cfg.get('parameters', {})
    const = load_params_to_constants(params)
    twin = RSSDigitalTwin(const)

    pipeline = RSSSuperPipeline(n_trials=1)

    engagement_loss, session_fail = pipeline._evaluate_session_metrics(twin)

    print(f"engagement_loss={engagement_loss}, session_fail={session_fail}")

    # Debug: 复现检查项目以便定位问题
    acft = pipeline._check_basic_fitness(twin)
    try:
        acft_scenario = acft_res = twin.simulate_scenario(
            speed_profile=[(0, 3500.0/927.0),(927,3500.0/927.0)],
            current_weight=90.0,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=0,
            movement_type=0,
            enable_randomness=False
        )
        acft_min = acft_scenario.get('min_stamina', None)
    except Exception as e:
        acft_min = None

    print(f"ACFT min_stamina={acft_min}")

    # 复现 sprint 恢复样本
    sprint_profile = [(0,5.0),(30,0.0),(90,0.0)]
    sprint_res = RSSDigitalTwin(twin.constants).simulate_scenario(
        speed_profile=sprint_profile,
        current_weight=90.0,
        grade_percent=0.0,
        terrain_factor=1.0,
        stance=0,
        movement_type=0,
        enable_randomness=False
    )
    sprint_hist = sprint_res.get('stamina_history', [])
    sprint_end = sprint_hist[int(30/0.2)-1] if len(sprint_hist) >= int(30/0.2) else sprint_hist[-1]
    recovery_after_60s = sprint_hist[-1] - sprint_end if sprint_hist else 0.0
    print(f"recovery_after_60s={recovery_after_60s}")

    # 最低体力
    patrol_profile = [(0,2.8),(120,3.5),(180,2.8),(300,3.0),(600,2.5)]
    patrol_res = RSSDigitalTwin(twin.constants).simulate_scenario(
        speed_profile=patrol_profile,
        current_weight=90.0+30.0,
        grade_percent=0.0,
        terrain_factor=1.0,
        stance=0,
        movement_type=0,
        enable_randomness=False
    )
    min_stamina_overall = min(
        min(patrol_res.get('stamina_history', [1.0])),
        min(sprint_res.get('stamina_history', [1.0])),
        min(acft_scenario.get('stamina_history', [1.0]))
    )
    print(f"min_stamina_overall={min_stamina_overall}")

    assert session_fail is False, "平衡预设未通过会话硬约束（ACFT/恢复窗/不可复原崩溃）"
    assert engagement_loss < 1e5, "engagement_loss 非法（可能硬约束触发）"
    print("会话约束测试通过。")
