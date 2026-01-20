# RSS Multi-Objective Optimization Report

Parameter optimization results based on Optuna Bayesian optimization

---

## 📊 Optimization Configuration Summary

| Metric | Value |
|------|-----|
| Optimization Method | Optuna (TPE) |
| Number of Trials | 200 |
| Number of Scenarios | 13 |
| Number of Pareto Solutions | 50 |
| Optimization Time | ~47 seconds |

---

## 📈 Pareto Front Results


| Metric | Value |
|------|-----|
| Realism Loss Range | [59.21, 91.73] |
| Playability Burden Range | [24.35, 162.88] |

---

## 🔍 Parameter Sensitivity Analysis

| Parameter Name | Coefficient of Variation | Sensitivity |
|---------|---------|--------|
| load_recovery_penalty_coeff | 0.2848 | Medium |
| fatigue_accumulation_coeff | 0.2692 | Medium |
| standing_recovery_multiplier | 0.2618 | Medium |
| encumbrance_speed_penalty_coeff | 0.2249 | Medium |
| energy_to_stamina_coeff | 0.1837 | Medium |
| encumbrance_stamina_drain_coeff | 0.1809 | Medium |
| load_recovery_penalty_exponent | 0.1650 | Medium |
| sprint_stamina_drain_multiplier | 0.1486 | Medium |
| fatigue_max_factor | 0.1159 | Medium |
| prone_recovery_multiplier | 0.1129 | Medium |
| anaerobic_efficiency_factor | 0.1091 | Medium |
| base_recovery_rate | 0.1088 | Medium |
| aerobic_efficiency_factor | 0.0630 | Low |

---

## ⚖️ Configuration Comparison

| Parameter | 平衡型配置（50% 拟真 + 50% 可玩性） | 拟真优先配置（70% 拟真 + 30% 可玩性） | 可玩性优先配置（30% 拟真 + 70% 可玩性） |
|------|------|------|------|
| energy_to_stamina_coeff | 0.000025 | 0.000025 | 0.000020 |
| base_recovery_rate | 0.000499 | 0.000499 | 0.000440 |
| standing_recovery_multiplier | 1.765886 | 1.765886 | 1.382203 |
| prone_recovery_multiplier | 2.756956 | 2.756956 | 2.785653 |
| load_recovery_penalty_coeff | 0.000302 | 0.000302 | 0.000161 |
| load_recovery_penalty_exponent | 2.401259 | 2.401259 | 2.848409 |
| encumbrance_speed_penalty_coeff | 0.272329 | 0.272329 | 0.217013 |
| encumbrance_stamina_drain_coeff | 1.939689 | 1.939689 | 1.636593 |
| sprint_stamina_drain_multiplier | 3.010818 | 3.010818 | 2.456596 |
| fatigue_accumulation_coeff | 0.020043 | 0.020043 | 0.023566 |
| fatigue_max_factor | 2.707786 | 2.707786 | 2.509931 |
| aerobic_efficiency_factor | 0.815724 | 0.815724 | 0.915464 |
| anaerobic_efficiency_factor | 1.142847 | 1.142847 | 1.466094 |


---

*Generated at: 2026-01-20 22:25:28*
*Version: RSS v3.0.0*
*Author: ViVi141 (747384120@qq.com)*
