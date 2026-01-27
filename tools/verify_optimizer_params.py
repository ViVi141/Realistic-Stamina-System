#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""验证所有优化器参数是否在数字孪生中被正确使用"""

from rss_digital_twin_fix import RSSConstants

# 优化器搜索的41个参数（从rss_super_pipeline.py提取）
OPTIMIZER_PARAMS = [
    'energy_to_stamina_coeff', 'base_recovery_rate', 'prone_recovery_multiplier',
    'standing_recovery_multiplier', 'load_recovery_penalty_coeff', 'load_recovery_penalty_exponent',
    'encumbrance_speed_penalty_coeff', 'encumbrance_stamina_drain_coeff',
    'sprint_stamina_drain_multiplier', 'fatigue_accumulation_coeff', 'fatigue_max_factor',
    'aerobic_efficiency_factor', 'anaerobic_efficiency_factor', 'recovery_nonlinear_coeff',
    'fast_recovery_multiplier', 'medium_recovery_multiplier', 'slow_recovery_multiplier',
    'marginal_decay_threshold', 'marginal_decay_coeff', 'min_recovery_stamina_threshold',
    'min_recovery_rest_time_seconds', 'sprint_speed_boost', 'posture_crouch_multiplier',
    'posture_prone_multiplier', 'jump_stamina_base_cost', 'vault_stamina_start_cost',
    'climb_stamina_tick_cost', 'jump_consecutive_penalty', 'slope_uphill_coeff',
    'slope_downhill_coeff', 'swimming_base_power', 'swimming_encumbrance_threshold',
    'swimming_static_drain_multiplier', 'swimming_dynamic_power_efficiency',
    'swimming_energy_to_stamina_coeff', 'env_heat_stress_max_multiplier', 'env_rain_weight_max',
    'env_wind_resistance_coeff', 'env_mud_penalty_max', 'env_temperature_heat_penalty_coeff',
    'env_temperature_cold_recovery_penalty_coeff'
]

def verify_params():
    """验证所有优化参数是否在RSSConstants中定义"""
    c = RSSConstants()
    missing = []
    present = []
    
    for param in OPTIMIZER_PARAMS:
        const_name = param.upper()
        if hasattr(c, const_name):
            present.append(param)
        else:
            missing.append(param)
    
    print(f"优化参数总数: {len(OPTIMIZER_PARAMS)}")
    print(f"OK 已定义的常量: {len(present)}")
    print(f"FAIL 缺失的常量: {len(missing)}")
    
    if missing:
        print("\n缺失的常量定义:")
        for p in missing:
            print(f"  - {p.upper()}")
    
    return len(missing) == 0

if __name__ == '__main__':
    success = verify_params()
    exit(0 if success else 1)
