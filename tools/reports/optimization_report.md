# RSS Multi-Objective Optimization Report

Parameter optimization results based on Optuna Bayesian optimization

---

## 📊 Optimization Configuration Summary

| Metric | Value |
|------|-----|
| Optimization Method | Optuna (TPE) |
| Number of Trials | 200 |
| Number of Scenarios | 13 |
| Number of Pareto Solutions | 36 |
| Optimization Time | ~47 seconds |

---

## 📈 Pareto Front Results


| Metric | Value |
|------|-----|
| Realism Loss Range | [61.21, 90.21] |
| Playability Burden Range | [24.35, 147.10] |

---

## 🔍 Parameter Sensitivity Analysis

| Parameter Name | Coefficient of Variation | Sensitivity |
|---------|---------|--------|
| base_recovery_rate | 0.5876 | High |
| fatigue_accumulation_coeff | 0.3480 | High |
| energy_to_stamina_coeff | 0.3203 | High |
| min_recovery_rest_time_seconds | 0.2556 | Medium |
| load_recovery_penalty_exponent | 0.2552 | Medium |
| standing_recovery_multiplier | 0.2512 | Medium |
| encumbrance_stamina_drain_coeff | 0.2461 | Medium |
| jump_stamina_base_cost | 0.1850 | Medium |
| slope_downhill_coeff | 0.1677 | Medium |
| recovery_nonlinear_coeff | 0.1615 | Medium |
| encumbrance_speed_penalty_coeff | 0.1595 | Medium |
| env_temperature_heat_penalty_coeff | 0.1506 | Medium |
| fatigue_max_factor | 0.1498 | Medium |
| env_wind_resistance_coeff | 0.1483 | Medium |
| slow_recovery_multiplier | 0.1471 | Medium |
| slope_uphill_coeff | 0.1373 | Medium |
| env_rain_weight_max | 0.1317 | Medium |
| fast_recovery_multiplier | 0.1300 | Medium |
| sprint_stamina_drain_multiplier | 0.1282 | Medium |
| load_recovery_penalty_coeff | 0.1272 | Medium |
| swimming_base_power | 0.1134 | Medium |
| swimming_encumbrance_threshold | 0.1122 | Medium |
| min_recovery_stamina_threshold | 0.1092 | Medium |
| env_temperature_cold_recovery_penalty_coeff | 0.1055 | Medium |
| prone_recovery_multiplier | 0.1032 | Medium |
| swimming_energy_to_stamina_coeff | 0.1005 | Medium |
| vault_stamina_start_cost | 0.0980 | Low |
| medium_recovery_multiplier | 0.0927 | Low |
| swimming_dynamic_power_efficiency | 0.0918 | Low |
| sprint_speed_boost | 0.0859 | Low |
| swimming_static_drain_multiplier | 0.0833 | Low |
| jump_consecutive_penalty | 0.0758 | Low |
| env_mud_penalty_max | 0.0722 | Low |
| posture_prone_multiplier | 0.0697 | Low |
| marginal_decay_threshold | 0.0625 | Low |
| posture_crouch_multiplier | 0.0588 | Low |
| climb_stamina_tick_cost | 0.0506 | Low |
| aerobic_efficiency_factor | 0.0491 | Low |
| env_heat_stress_max_multiplier | 0.0402 | Low |
| anaerobic_efficiency_factor | 0.0369 | Low |
| marginal_decay_coeff | 0.0174 | Low |

---

## ⚖️ Configuration Comparison

| Parameter | 平衡型配置（50% 拟真 + 50% 可玩性） | 拟真优先配置（70% 拟真 + 30% 可玩性） | 可玩性优先配置（30% 拟真 + 70% 可玩性） |
|------|------|------|------|
| energy_to_stamina_coeff | 0.000029 | 0.000029 | 0.000021 |
| base_recovery_rate | 0.000435 | 0.000435 | 0.000156 |
| standing_recovery_multiplier | 1.627813 | 1.627813 | 1.383328 |
| prone_recovery_multiplier | 2.996284 | 2.996284 | 2.122086 |
| load_recovery_penalty_coeff | 0.000276 | 0.000276 | 0.000291 |
| load_recovery_penalty_exponent | 2.215715 | 2.215715 | 1.171550 |
| encumbrance_speed_penalty_coeff | 0.247094 | 0.247094 | 0.269459 |
| encumbrance_stamina_drain_coeff | 1.885040 | 1.885040 | 1.106133 |
| sprint_stamina_drain_multiplier | 2.864645 | 2.864645 | 3.950201 |
| fatigue_accumulation_coeff | 0.016410 | 0.016410 | 0.011541 |
| fatigue_max_factor | 2.161953 | 2.161953 | 1.696312 |
| aerobic_efficiency_factor | 0.957964 | 0.957964 | 0.945722 |
| anaerobic_efficiency_factor | 1.389996 | 1.389996 | 1.437468 |
| recovery_nonlinear_coeff | 0.571668 | 0.571668 | 0.576102 |
| fast_recovery_multiplier | 3.650166 | 3.650166 | 3.762541 |
| medium_recovery_multiplier | 1.843309 | 1.843309 | 1.591475 |
| slow_recovery_multiplier | 0.638701 | 0.638701 | 0.727726 |
| marginal_decay_threshold | 0.868022 | 0.868022 | 0.893842 |
| marginal_decay_coeff | 1.096043 | 1.096043 | 1.120426 |
| min_recovery_stamina_threshold | 0.170367 | 0.170367 | 0.176328 |
| min_recovery_rest_time_seconds | 13.670584 | 13.670584 | 14.974878 |
| sprint_speed_boost | 0.287941 | 0.287941 | 0.294876 |
| posture_crouch_multiplier | 2.030973 | 2.030973 | 1.927608 |
| posture_prone_multiplier | 2.500270 | 2.500270 | 2.632080 |
| jump_stamina_base_cost | 0.041871 | 0.041871 | 0.026853 |
| vault_stamina_start_cost | 0.016469 | 0.016469 | 0.019709 |
| climb_stamina_tick_cost | 0.011587 | 0.011587 | 0.011182 |
| jump_consecutive_penalty | 0.549338 | 0.549338 | 0.557846 |
| slope_uphill_coeff | 0.073105 | 0.073105 | 0.070983 |
| slope_downhill_coeff | 0.038085 | 0.038085 | 0.035410 |
| swimming_base_power | 22.162963 | 22.162963 | 20.570954 |
| swimming_encumbrance_threshold | 23.974364 | 23.974364 | 23.001678 |
| swimming_static_drain_multiplier | 3.486432 | 3.486432 | 3.450024 |
| swimming_dynamic_power_efficiency | 2.209226 | 2.209226 | 2.170739 |
| swimming_energy_to_stamina_coeff | 0.000055 | 0.000055 | 0.000056 |
| env_heat_stress_max_multiplier | 1.263299 | 1.263299 | 1.213359 |
| env_rain_weight_max | 7.058414 | 7.058414 | 7.703978 |
| env_wind_resistance_coeff | 0.052385 | 0.052385 | 0.061370 |
| env_mud_penalty_max | 0.462532 | 0.462532 | 0.464245 |
| env_temperature_heat_penalty_coeff | 0.020949 | 0.020949 | 0.015319 |
| env_temperature_cold_recovery_penalty_coeff | 0.050549 | 0.050549 | 0.042980 |


---

*Generated at: 2026-01-21 01:55:33*
*Version: RSS v3.2.0*
*Author: ViVi141 (747384120@qq.com)*
