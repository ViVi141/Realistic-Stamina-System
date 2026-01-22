# RSS Multi-Objective Optimization Report

Parameter optimization results based on Optuna Bayesian optimization

---

## 📊 Optimization Configuration Summary

| Metric | Value |
|------|-----|
| Optimization Method | Optuna (TPE) |
| Number of Trials | 3000 |
| Number of Scenarios | 13 |
| Number of Pareto Solutions | 3 |
| Optimization Time | ~47 seconds |

---

## 📈 Pareto Front Results


| Metric | Value |
|------|-----|
| Realism Loss Range | [17.61, 17.61] |
| Playability Burden Range | [0.00, 5.92] |

---

## 🔍 Parameter Sensitivity Analysis

| Parameter Name | Coefficient of Variation | Sensitivity |
|---------|---------|--------|
| load_recovery_penalty_coeff | 0.6951 | High |
| standing_recovery_multiplier | 0.2094 | Medium |
| slope_uphill_coeff | 0.2004 | Medium |
| min_recovery_stamina_threshold | 0.1697 | Medium |
| sprint_stamina_drain_multiplier | 0.1426 | Medium |
| slow_recovery_multiplier | 0.1278 | Medium |
| jump_stamina_base_cost | 0.1139 | Medium |
| env_temperature_heat_penalty_coeff | 0.1109 | Medium |
| min_recovery_rest_time_seconds | 0.0977 | Low |
| vault_stamina_start_cost | 0.0940 | Low |
| swimming_dynamic_power_efficiency | 0.0909 | Low |
| fast_recovery_multiplier | 0.0881 | Low |
| swimming_encumbrance_threshold | 0.0828 | Low |
| base_recovery_rate | 0.0800 | Low |
| env_temperature_cold_recovery_penalty_coeff | 0.0777 | Low |
| load_recovery_penalty_exponent | 0.0724 | Low |
| recovery_nonlinear_coeff | 0.0666 | Low |
| env_mud_penalty_max | 0.0593 | Low |
| swimming_static_drain_multiplier | 0.0586 | Low |
| climb_stamina_tick_cost | 0.0547 | Low |
| jump_consecutive_penalty | 0.0525 | Low |
| marginal_decay_threshold | 0.0494 | Low |
| encumbrance_stamina_drain_coeff | 0.0450 | Low |
| medium_recovery_multiplier | 0.0427 | Low |
| env_rain_weight_max | 0.0414 | Low |
| sprint_speed_boost | 0.0391 | Low |
| env_wind_resistance_coeff | 0.0330 | Low |
| prone_recovery_multiplier | 0.0328 | Low |
| energy_to_stamina_coeff | 0.0253 | Low |
| fatigue_accumulation_coeff | 0.0238 | Low |
| slope_downhill_coeff | 0.0177 | Low |
| aerobic_efficiency_factor | 0.0174 | Low |
| marginal_decay_coeff | 0.0144 | Low |
| fatigue_max_factor | 0.0139 | Low |
| swimming_energy_to_stamina_coeff | 0.0132 | Low |
| posture_crouch_multiplier | 0.0104 | Low |
| swimming_base_power | 0.0071 | Low |
| env_heat_stress_max_multiplier | 0.0038 | Low |
| anaerobic_efficiency_factor | 0.0038 | Low |
| posture_prone_multiplier | 0.0021 | Low |
| encumbrance_speed_penalty_coeff | 0.0000 | Low |

---

## ⚖️ Configuration Comparison

| Parameter | 平衡型配置（50% 拟真 + 50% 可玩性） | 拟真优先配置（70% 拟真 + 30% 可玩性） | 可玩性优先配置（30% 拟真 + 70% 可玩性） |
|------|------|------|------|
| energy_to_stamina_coeff | 0.000022 | 0.000021 | 0.000022 |
| base_recovery_rate | 0.000471 | 0.000399 | 0.000471 |
| standing_recovery_multiplier | 1.983014 | 1.230198 | 1.983014 |
| prone_recovery_multiplier | 2.658985 | 2.708213 | 2.658985 |
| load_recovery_penalty_coeff | 0.000100 | 0.000104 | 0.000100 |
| load_recovery_penalty_exponent | 2.421311 | 2.490376 | 2.421311 |
| encumbrance_speed_penalty_coeff | 0.100001 | 0.100000 | 0.100001 |
| encumbrance_stamina_drain_coeff | 1.348359 | 1.353765 | 1.348359 |
| sprint_stamina_drain_multiplier | 3.352700 | 2.415672 | 3.352700 |
| fatigue_accumulation_coeff | 0.005302 | 0.005009 | 0.005302 |
| fatigue_max_factor | 2.868174 | 2.828088 | 2.868174 |
| aerobic_efficiency_factor | 0.934243 | 0.898975 | 0.934243 |
| anaerobic_efficiency_factor | 1.298692 | 1.292150 | 1.298692 |
| recovery_nonlinear_coeff | 0.606313 | 0.691513 | 0.606313 |
| fast_recovery_multiplier | 2.667607 | 3.233242 | 2.667607 |
| medium_recovery_multiplier | 1.882958 | 1.813867 | 1.882958 |
| slow_recovery_multiplier | 0.894669 | 0.674586 | 0.894669 |
| marginal_decay_threshold | 0.797730 | 0.895847 | 0.797730 |
| marginal_decay_coeff | 1.069843 | 1.093048 | 1.069843 |
| min_recovery_stamina_threshold | 0.160330 | 0.235675 | 0.160330 |
| min_recovery_rest_time_seconds | 3.654945 | 3.810945 | 3.654945 |
| sprint_speed_boost | 0.309586 | 0.307205 | 0.309586 |
| posture_crouch_multiplier | 2.101262 | 2.056003 | 2.101262 |
| posture_prone_multiplier | 3.327731 | 3.343336 | 3.327731 |
| jump_stamina_base_cost | 0.027067 | 0.027361 | 0.027067 |
| vault_stamina_start_cost | 0.017159 | 0.020200 | 0.017159 |
| climb_stamina_tick_cost | 0.009191 | 0.010350 | 0.009191 |
| jump_consecutive_penalty | 0.467961 | 0.452845 | 0.467961 |
| slope_uphill_coeff | 0.098556 | 0.097611 | 0.098556 |
| slope_downhill_coeff | 0.036952 | 0.035383 | 0.036952 |
| swimming_base_power | 24.674404 | 24.796405 | 24.674404 |
| swimming_encumbrance_threshold | 25.242502 | 28.204570 | 25.242502 |
| swimming_static_drain_multiplier | 3.285878 | 3.298764 | 3.285878 |
| swimming_dynamic_power_efficiency | 2.392314 | 2.370038 | 2.392314 |
| swimming_energy_to_stamina_coeff | 0.000057 | 0.000055 | 0.000057 |
| env_heat_stress_max_multiplier | 1.355115 | 1.356144 | 1.355115 |
| env_rain_weight_max | 8.246238 | 8.996719 | 8.246238 |
| env_wind_resistance_coeff | 0.055786 | 0.059587 | 0.055786 |
| env_mud_penalty_max | 0.337353 | 0.381803 | 0.337353 |
| env_temperature_heat_penalty_coeff | 0.018806 | 0.023322 | 0.018806 |
| env_temperature_cold_recovery_penalty_coeff | 0.046652 | 0.048796 | 0.046652 |


---

*Generated at: 2026-01-22 20:45:20*
*Version: RSS v3.2.0*
*Author: ViVi141 (747384120@qq.com)*
