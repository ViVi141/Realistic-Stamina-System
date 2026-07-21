# RSS Sim — PyO3 仿真核（方案 A / Phase-B）

Rust 原生数字孪生，通过 PyO3 暴露给 Python 优化管线。Python 侧保留 Optuna NSGA-II 与场景定义；每次 trial 的 `run_mission_suite` 在可用时走 Rust 热路径。

## 构建

```bash
pip install maturin
cd tools/rss_sim
maturin develop --release
```

## Python API

```python
import rss_sim

rss_sim.is_available()  # True when extension loaded
rss_sim.get_drain_velocity_ms(5.5, 4.0)
rss_sim.simulate_mission(params_dict, mission_json, fast_mode=False, summary_only=True)
rss_sim.run_mission_suite(params_dict, missions_json, fast_mode=False, summary_only=True, parallel=True)
rss_sim.simulate_ideal_march_aerobic_end(params_json, hours=4.0, encumbrance_kg=35.0, dt_sec=2.0)
rss_sim.evaluate_hard_constraints(params_json=None)
```

`params_dict` 为 Optuna 采样参数字典（snake_case）；`mission_json` 为 `Mission` 对象或数组的 JSON。

## 模块结构

| 文件 | 职责 |
|------|------|
| `twin.rs` | `RSSDigitalTwin` + `game_player_tick` |
| `mission.rs` | `simulate_mission` / `MissionResult` |
| `constraints.rs` | 生理硬约束门禁 |
| `metabolism.rs` / `drain.rs` / `cp_wprime.rs` / `fatigue.rs` | 子系统 |

## 测试

```bash
python tools/test_rss_sim_parity.py   # Rust vs Python 数值对齐
python tools/test_v6_smoke.py         # 契约冒烟（仍走 Python 参考实现）
```
