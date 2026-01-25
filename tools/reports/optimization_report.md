# RSS Multi-Objective Optimization Report

Parameter optimization results based on Optuna Bayesian optimization

---

## 📊 Optimization Configuration Summary

| Metric | Value |
|------|-----|
| Optimization Method | Optuna (TPE) |
| Number of Trials | 3000 |
| Number of Scenarios | 13 |
| Number of Pareto Solutions | 2 |
| Optimization Time | ~47 seconds |

---

## 📈 Pareto Front Results


| Metric | Value |
|------|-----|
| Realism Loss Range | [17.61, 17.61] |
| Playability Burden Range | [0.00, 4.71] |

---

## 🔍 Parameter Sensitivity Analysis

| Parameter Name | Coefficient of Variation | Sensitivity |
|---------|---------|--------|
| load_recovery_penalty_coeff | 0.6929 | High |
| recovery_nonlinear_coeff | 0.1954 | Medium |
| energy_to_stamina_coeff | 0.1920 | Medium |
| prone_recovery_multiplier | 0.1888 | Medium |
| swimming_energy_to_stamina_coeff | 0.1715 | Medium |
| swimming_encumbrance_threshold | 0.1417 | Medium |
| fatigue_max_factor | 0.1231 | Medium |
| posture_crouch_multiplier | 0.1054 | Medium |
| jump_consecutive_penalty | 0.0932 | Low |
| load_recovery_penalty_exponent | 0.0893 | Low |
| env_rain_weight_max | 0.0707 | Low |
| aerobic_efficiency_factor | 0.0611 | Low |
| marginal_decay_threshold | 0.0401 | Low |
| sprint_stamina_drain_multiplier | 0.0221 | Low |
| base_recovery_rate | 0.0213 | Low |
| min_recovery_rest_time_seconds | 0.0200 | Low |
| env_temperature_heat_penalty_coeff | 0.0194 | Low |
| fatigue_accumulation_coeff | 0.0192 | Low |
| slow_recovery_multiplier | 0.0186 | Low |
| sprint_speed_boost | 0.0152 | Low |
| medium_recovery_multiplier | 0.0128 | Low |
| jump_stamina_base_cost | 0.0127 | Low |
| vault_stamina_start_cost | 0.0122 | Low |
| swimming_base_power | 0.0112 | Low |
| env_mud_penalty_max | 0.0112 | Low |
| encumbrance_stamina_drain_coeff | 0.0109 | Low |
| climb_stamina_tick_cost | 0.0108 | Low |
| env_temperature_cold_recovery_penalty_coeff | 0.0092 | Low |
| posture_prone_multiplier | 0.0072 | Low |
| slope_downhill_coeff | 0.0069 | Low |
| standing_recovery_multiplier | 0.0065 | Low |
| swimming_dynamic_power_efficiency | 0.0061 | Low |
| slope_uphill_coeff | 0.0056 | Low |
| swimming_static_drain_multiplier | 0.0049 | Low |
| fast_recovery_multiplier | 0.0036 | Low |
| min_recovery_stamina_threshold | 0.0026 | Low |
| marginal_decay_coeff | 0.0017 | Low |
| anaerobic_efficiency_factor | 0.0015 | Low |
| env_wind_resistance_coeff | 0.0006 | Low |
| env_heat_stress_max_multiplier | 0.0001 | Low |
| encumbrance_speed_penalty_coeff | 0.0000 | Low |

---

## ⚖️ Configuration Comparison

| Parameter | 平衡型配置（50% 拟真 + 50% 可玩性） | 拟真优先配置（70% 拟真 + 30% 可玩性） | 可玩性优先配置（30% 拟真 + 70% 可玩性） |
|------|------|------|------|
| energy_to_stamina_coeff | 0.000050 | 0.000034 | 0.000050 |
| base_recovery_rate | 0.000293 | 0.000305 | 0.000293 |
| standing_recovery_multiplier | 2.400363 | 2.431582 | 2.400363 |
| prone_recovery_multiplier | 1.385188 | 2.030094 | 1.385188 |
| load_recovery_penalty_coeff | 0.000120 | 0.000661 | 0.000120 |
| load_recovery_penalty_exponent | 1.639862 | 1.961528 | 1.639862 |
| encumbrance_speed_penalty_coeff | 0.100000 | 0.100000 | 0.100000 |
| encumbrance_stamina_drain_coeff | 1.040571 | 1.018203 | 1.040571 |
| sprint_stamina_drain_multiplier | 3.092328 | 3.232287 | 3.092328 |
| fatigue_accumulation_coeff | 0.005369 | 0.005579 | 0.005369 |
| fatigue_max_factor | 1.631360 | 2.089187 | 1.631360 |
| aerobic_efficiency_factor | 0.843946 | 0.953799 | 0.843946 |
| anaerobic_efficiency_factor | 1.128123 | 1.124639 | 1.128123 |
| recovery_nonlinear_coeff | 0.545565 | 0.367186 | 0.545565 |
| fast_recovery_multiplier | 3.057596 | 3.079555 | 3.057596 |
| medium_recovery_multiplier | 1.804266 | 1.758705 | 1.804266 |
| slow_recovery_multiplier | 0.538806 | 0.519178 | 0.538806 |
| marginal_decay_threshold | 0.712658 | 0.772185 | 0.712658 |
| marginal_decay_coeff | 1.068100 | 1.064375 | 1.068100 |
| min_recovery_stamina_threshold | 0.163629 | 0.164473 | 0.163629 |
| min_recovery_rest_time_seconds | 4.915402 | 4.722310 | 4.915402 |
| sprint_speed_boost | 0.304471 | 0.313856 | 0.304471 |
| posture_crouch_multiplier | 1.881910 | 1.522962 | 1.881910 |
| posture_prone_multiplier | 3.064246 | 3.108832 | 3.064246 |
| jump_stamina_base_cost | 0.038862 | 0.037886 | 0.038862 |
| vault_stamina_start_cost | 0.019405 | 0.019884 | 0.019405 |
| climb_stamina_tick_cost | 0.008197 | 0.008376 | 0.008197 |
| jump_consecutive_penalty | 0.595740 | 0.494112 | 0.595740 |
| slope_uphill_coeff | 0.072918 | 0.073745 | 0.072918 |
| slope_downhill_coeff | 0.031025 | 0.031454 | 0.031025 |
| swimming_base_power | 20.772111 | 20.311027 | 20.772111 |
| swimming_encumbrance_threshold | 20.286466 | 26.982214 | 20.286466 |
| swimming_static_drain_multiplier | 2.833324 | 2.861130 | 2.833324 |
| swimming_dynamic_power_efficiency | 2.457312 | 2.427591 | 2.457312 |
| swimming_energy_to_stamina_coeff | 0.000059 | 0.000042 | 0.000059 |
| env_heat_stress_max_multiplier | 1.264941 | 1.265070 | 1.264941 |
| env_rain_weight_max | 7.748856 | 8.927085 | 7.748856 |
| env_wind_resistance_coeff | 0.064680 | 0.064751 | 0.064680 |
| env_mud_penalty_max | 0.364388 | 0.372643 | 0.364388 |
| env_temperature_heat_penalty_coeff | 0.015740 | 0.015143 | 0.015740 |
| env_temperature_cold_recovery_penalty_coeff | 0.054461 | 0.053464 | 0.054461 |


---

*Generated at: 2026-01-25 18:57:39*
*Version: RSS v3.2.0*
*Author: ViVi141 (747384120@qq.com)*
