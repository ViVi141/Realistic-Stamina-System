# Encumbrance Speed Penalty

概述：
本项目使用修正后的负重速度惩罚模型，旨在使高负重对速度、尤其是 Sprint 的影响更明显，从而提高游戏平衡性与真实感。

实现（数字孪生侧）：
- 参数位于 `RSSConstants`：
  - `ENCUMBRANCE_SPEED_PENALTY_COEFF`（默认 0.20）：基准系数
  - `ENCUMBRANCE_SPEED_PENALTY_EXPONENT`（默认 1.5）：幂次，用于放大较大负重的影响
  - `ENCUMBRANCE_SPEED_PENALTY_MAX`（默认 0.75）：速度惩罚上限（最多减速 75%）

计算公式（伪代码）：
- effective_weight = max(current_weight - body_weight - base_weight, 0.0)
- body_mass_percent = effective_weight / body_weight
- speed_ratio = speed / GAME_MAX_SPEED
- penalty = coeff * (body_mass_percent ** exp) * (1 + speed_ratio)
- 如果 movement_type 为 SPRINT，则 penalty *= 1.5
- penalty = clip(penalty, 0.0, max_penalty)
- effective_speed = speed * posture_speed_mult * (1 - penalty)

建议：
- 默认值（coeff=0.20, exp=1.5, max=0.75）既能在 29~30kg 下对冲刺产生显著影响，又不会对 Walk/Run 造成过分惩罚。
- 若你想进一步强化沉重负载的影响：可提高 `exp`（例如 1.8~2.0），或提高 `coeff`。配合 `sustainability_report.py` 的热力图可以快速评估玩家体验。

测试与验证：
- 已添加 `tools/encumbrance_tuning.py` 用于参数网格搜索并生成 CSV 报告。
- 已添加 `tools/test_encumbrance_penalty.py` 单元测试以确保惩罚单调性与对 Sprint 的影响。

下一步：
- 在确定最终值后，请在工作台编译并在游戏内做实机验证以确认玩家手感。