#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Pipeline v4 — 重新设计的优化管线
=====================================
v4 变更（相对于 super_pipeline.py）：
1. 场景改用真实 Arma 战斗节奏的多阶段剖面，替代单阶段匀速测试
2. 指标改为基于游戏结局（耐力/恢复/拟真）的三目标体系，替代 20+ 惩罚魔法数字
3. 搜索空间与 C 端 SCR_StaminaConstants.c 的 getter fallback 值对齐

时间步长：dt=0.2s，与游戏 UpdateSpeedBasedOnStamina 一致。
"""

import sys
import numpy as np
import json
import math
import os
import time
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, asdict

# optuna 延迟导入（烟雾测试不需要）
try:
    import optuna
    HAS_OPTUNA = True
except ImportError:
    HAS_OPTUNA = False
    optuna = None

# 导入数字孪生（常量已与 C 对齐）
from rss_digital_twin_fix import (
    RSSDigitalTwin,
    MovementType,
    Stance,
    RSSConstants,
)

# =============================================================================
# Phase — 单个战斗阶段
# =============================================================================
@dataclass
class Phase:
    """战斗阶段：时长 + 移动类型 + 姿态 + 坡度 + 地形 + 描述"""
    duration_s: float          # 持续时间（秒）
    speed_ms: float            # 目标速度（m/s），0 = 静止
    movement: int              # MovementType (IDLE/WALK/RUN/SPRINT)
    stance: int                # Stance (STAND/CROUCH/PRONE)
    grade_pct: float = 0.0     # 坡度百分比
    terrain: float = 1.0       # 地形系数
    label: str = ""            # 阶段标签

# =============================================================================
# Mission — 一个完整任务 = 多个 Phase
# =============================================================================
@dataclass
class Mission:
    """完整任务 = 多个战斗阶段序列"""
    name: str                  # 任务名
    load_kg: float             # 装备负重（不含体重 90kg）
    phases: List[Phase]        # 阶段序列
    description: str = ""      # 任务描述

    @property
    def total_duration_s(self) -> float:
        return sum(p.duration_s for p in self.phases)

    @property
    def current_weight(self) -> float:
        return 90.0 + self.load_kg

# =============================================================================
# Mission 库 — 基于真实 Arma Reforger 战斗节奏
# =============================================================================
class MissionLibrary:
    """4 个核心任务，覆盖 Arma 典型玩法循环"""

    @staticmethod
    def patrol_contact(load_kg: float = 30.0) -> Mission:
        """
        巡逻接敌 (15min, 30kg)
        节奏: 长距离徒步行军 → 突然接敌冲刺 → 跑步找掩体 → 蹲姿交火 → 撤离
        对应 Arma 最频繁的玩法: 巡逻 → 遭遇战 → 撤离
        """
        return Mission(
            name="巡逻接敌",
            load_kg=load_kg,
            description="30kg巡逻→遭遇→交火→撤离，15分钟标准循环",
            phases=[
                Phase(600, 2.8,  MovementType.WALK,   Stance.STAND,  0,   1.0, "徒步行军"),
                Phase(30,  4.8,  MovementType.SPRINT, Stance.STAND,  0,   1.0, "冲刺接敌"),
                Phase(90,  3.5,  MovementType.RUN,    Stance.STAND,  0,   1.2, "跑步找掩体"),
                Phase(120, 1.5,  MovementType.WALK,   Stance.CROUCH, 0,   1.0, "蹲姿交火"),
                Phase(180, 2.8,  MovementType.WALK,   Stance.STAND,  3,   1.3, "撤离行军"),
            ],
        )

    @staticmethod
    def urban_clearance(load_kg: float = 25.0) -> Mission:
        """
        城镇清扫 (12min, 25kg)
        节奏: 蹲姿慢行清房 → 冲刺过街道 → 短暂交火 → 重复 → 冲刺脱离
        对应 CQB / 城镇攻坚玩法
        """
        return Mission(
            name="城镇清扫",
            load_kg=load_kg,
            description="25kg CQB清扫，12分钟内2次清房+街道冲刺",
            phases=[
                Phase(180, 1.8,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "清房A"),
                Phase(20,  4.5,  MovementType.SPRINT, Stance.STAND,  0,  1.2, "过街道"),
                Phase(60,  1.5,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "交火"),
                Phase(120, 1.8,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "清房B"),
                Phase(20,  4.5,  MovementType.SPRINT, Stance.STAND,  0,  1.2, "过街道"),
                Phase(60,  1.5,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "交火"),
                Phase(30,  4.8,  MovementType.SPRINT, Stance.STAND,  0,  1.0, "冲刺脱离"),
                Phase(240, 0.0,  MovementType.IDLE,   Stance.STAND,  0,  1.0, "任务结束"),
            ],
        )

    @staticmethod
    def mountain_approach(load_kg: float = 35.0) -> Mission:
        """
        山地接近 (22min, 35kg)
        节奏: 上坡负重行军 → 趴姿休整 → 继续上坡 → 下坡
        对应 Everon/Arland 山地作战
        """
        return Mission(
            name="山地接近",
            load_kg=load_kg,
            description="35kg山地行军，15%坡→休整→继续→下坡",
            phases=[
                Phase(600, 2.0,  MovementType.WALK,  Stance.STAND,  15,  1.5, "上坡行军"),
                Phase(120, 0.0,  MovementType.IDLE,  Stance.PRONE,   0,  1.0, "趴姿休整"),
                Phase(300, 1.8,  MovementType.WALK,  Stance.STAND,  12,  1.4, "继续上坡"),
                Phase(180, 2.5,  MovementType.WALK,  Stance.STAND,  -8, 1.3, "下坡"),
                Phase(120, 0.0,  MovementType.IDLE,  Stance.STAND,   0, 1.0, "抵达"),
            ],
        )

    @staticmethod
    def vehicle_dismount(load_kg: float = 20.0) -> Mission:
        """
        载具下车突击 (6min, 20kg)
        节奏: 下车冲刺 → 交火 → 回载具 → 载具内恢复
        对应机械化步兵玩法
        """
        return Mission(
            name="载具下车突击",
            load_kg=load_kg,
            description="20kg轻装下车突击，6分钟含载具恢复",
            phases=[
                Phase(30,  5.0,  MovementType.SPRINT, Stance.STAND,  0,  1.0, "冲刺下车"),
                Phase(90,  1.5,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "交火"),
                Phase(30,  5.0,  MovementType.SPRINT, Stance.STAND,  0,  1.0, "冲刺回载具"),
                Phase(210, 0.0,  MovementType.IDLE,   Stance.STAND,  0,  1.0, "载具恢复"),
            ],
        )

    @classmethod
    def all_missions(cls, load_30: float = 30.0, load_25: float = 25.0,
                     load_35: float = 35.0, load_20: float = 20.0) -> List[Mission]:
        return [
            cls.patrol_contact(load_30),
            cls.urban_clearance(load_25),
            cls.mountain_approach(load_35),
            cls.vehicle_dismount(load_20),
        ]

# =============================================================================
# 仿真运行器 — 执行一个 Mission 并返回结构化结果
# =============================================================================
@dataclass
class MissionResult:
    """单个 Mission 的仿真结果"""
    mission_name: str
    total_duration_s: float
    stamina_trace: List[float]     # 体力时间序列
    speed_trace: List[float]       # 速度时间序列
    min_stamina: float             # 全局最低体力
    mean_stamina_active: float     # 运动阶段平均体力
    recovery_gain: float           # 静止阶段恢复的体力总量
    exhaustion_duration_s: float   # 体力<15%的时间
    completion_possible: bool      # 是否全程体力>0

def simulate_mission(twin: RSSDigitalTwin, mission: Mission) -> MissionResult:
    """运行一个完整 Mission 的数字孪生仿真"""
    twin.reset()
    dt = 0.2
    current_time = 0.0
    stamina_trace = []
    speed_trace = []
    total_recovery_gain = 0.0
    exhaustion_duration = 0.0
    last_stamina = 1.0
    current_weight = mission.current_weight

    for phase in mission.phases:
        steps = int(phase.duration_s / dt)
        for _ in range(steps):
            # 记录恢复量（当前帧 vs 上一帧）
            prev_stamina = twin.stamina
            twin.step(
                phase.speed_ms,
                current_weight,
                phase.grade_pct,
                phase.terrain,
                phase.stance,
                phase.movement,
                current_time,
                enable_randomness=False,
            )
            current_time += dt
            stamina_trace.append(twin.stamina)
            speed_trace.append(phase.speed_ms)

            if twin.stamina > prev_stamina:
                total_recovery_gain += twin.stamina - prev_stamina
            if twin.stamina < 0.15:
                exhaustion_duration += dt
            last_stamina = twin.stamina

    # 计算运动阶段平均体力（排除 IDLE 阶段）
    active_stamina = []
    t = 0.0
    for i, phase in enumerate(mission.phases):
        n_steps = int(phase.duration_s / dt)
        for j in range(n_steps):
            idx = int(t / dt) + j
            if idx < len(stamina_trace) and phase.movement != MovementType.IDLE:
                active_stamina.append(stamina_trace[idx])
        t += phase.duration_s

    min_stamina = min(stamina_trace) if stamina_trace else 0.0
    mean_active = np.mean(active_stamina) if active_stamina else min_stamina

    return MissionResult(
        mission_name=mission.name,
        total_duration_s=mission.total_duration_s,
        stamina_trace=stamina_trace,
        speed_trace=speed_trace,
        min_stamina=min_stamina,
        mean_stamina_active=float(mean_active),
        recovery_gain=total_recovery_gain,
        exhaustion_duration_s=exhaustion_duration,
        completion_possible=min_stamina > 0.0,
    )

# =============================================================================
# 指标计算 — 三目标体系
# =============================================================================
@dataclass
class Metrics:
    """三目标指标"""
    combat_endurance: float    # 目标1 (minimize): 1 - 各任务运动阶段平均体力
    recovery_efficiency: float # 目标2 (minimize): 恢复窗口效率惩罚
    parameter_realism: float   # 目标3 (minimize): 参数偏离生理参考值的程度

    def as_tuple(self) -> Tuple[float, float, float]:
        return (self.combat_endurance, self.recovery_efficiency, self.parameter_realism)

def compute_metrics(results: List[MissionResult], params: Dict) -> Metrics:
    """从多个 Mission 结果计算三目标指标"""

    # ── 目标 1: 战斗耐力 ──────────────────────────────────────
    # 策略：取各任务运动阶段平均体力的加权平均，越低越差
    # 权重：巡逻(0.35) > 山地(0.30) > 城镇(0.20) > 突击(0.15)
    weights = {"巡逻接敌": 0.35, "山地接近": 0.30, "城镇清扫": 0.20, "载具下车突击": 0.15}
    weighted_mean = 0.0
    total_weight = 0.0
    for r in results:
        w = weights.get(r.mission_name, 0.25)
        weighted_mean += r.mean_stamina_active * w
        total_weight += w
    weighted_mean = weighted_mean / total_weight if total_weight > 0 else 0.5

    # 惩罚：任何任务全局最低体力 < 10%
    exhaustion_penalty = 0.0
    for r in results:
        if r.min_stamina < 0.10:
            exhaustion_penalty += (0.10 - r.min_stamina) * 5.0

    combat_endurance = (1.0 - weighted_mean) + exhaustion_penalty

    # ── 目标 2: 恢复效率 ──────────────────────────────────────
    # 策略：总量恢复 vs 可用恢复窗口的比例
    # 如果任务设计了休息窗口但恢复太少 → 恢复效率低（penalty 大）
    total_recovery = sum(r.recovery_gain for r in results)
    total_exhaustion = sum(r.exhaustion_duration_s for r in results)

    # 恢复总量过少 = 回血系统不工作（正常应在 0.02~0.15 之间）
    recovery_deficit = max(0.0, 0.05 - total_recovery) * 20.0
    # 力竭时间过长
    exhaustion_penalty_2 = total_exhaustion / 60.0  # 分钟

    recovery_efficiency = recovery_deficit + exhaustion_penalty_2

    # ── 目标 3: 参数拟真度 ──────────────────────────────────────
    # 检查参数与 C 端静态常量的偏差
    realism_score = 0.0

    # 3a. energy_to_stamina_coeff 在生理学合理区间 (5e-7 ~ 3e-6)
    ec = params.get('energy_to_stamina_coeff', 9e-7)
    ec_ref = 9e-7  # C EliteStandard 预设
    if ec < 5e-7 or ec > 3e-6:
        realism_score += min(abs(ec - 5e-7), abs(ec - 3e-6)) / 1e-6 * 2.0

    # 3b. base_recovery_rate 在合理区间 (5e-5 ~ 3e-4), 中心 1.5e-4
    br = params.get('base_recovery_rate', 1.5e-4)
    br_ref = 1.5e-4
    realism_score += abs(br - br_ref) / br_ref * 3.0

    # 3c. 姿态恢复倍数排序: prone > crouch > standing
    pm = params.get('prone_recovery_multiplier', 1.6)
    cm = params.get('crouching_recovery_multiplier', 1.4)
    sm = params.get('standing_recovery_multiplier', 1.3)
    if pm <= cm:
        realism_score += (cm - pm + 0.1) * 10.0
    if cm <= sm:
        realism_score += (sm - cm + 0.1) * 10.0

    # 3d. 快速恢复 > 中速恢复 > 慢速恢复
    fm = params.get('fast_recovery_multiplier', 1.6)
    mm = params.get('medium_recovery_multiplier', 1.3)
    lm = params.get('slow_recovery_multiplier', 0.6)
    if fm <= mm:
        realism_score += (mm - fm + 0.1) * 8.0
    if mm <= lm:
        realism_score += (lm - mm + 0.1) * 8.0

    # 3e. 蹲姿消耗 >= 站姿消耗
    pcm = params.get('posture_crouch_multiplier', 1.8)
    if pcm < 1.0:
        realism_score += (1.0 - pcm) * 5.0

    # 3f. encumbrance 参数合理性
    espc = params.get('encumbrance_speed_penalty_coeff', 0.20)
    if espc < 0.10 or espc > 0.50:
        realism_score += min(abs(espc - 0.10), abs(espc - 0.50)) * 20.0

    parameter_realism = realism_score

    return Metrics(
        combat_endurance=float(combat_endurance),
        recovery_efficiency=float(recovery_efficiency),
        parameter_realism=float(parameter_realism),
    )

# =============================================================================
# Optuna 目标函数
# =============================================================================
class RSSOptimizerV4:
    """v4 优化器 — 三目标 NSGA-II"""

    def __init__(self, n_trials: int = 1000, n_jobs: int = -1, fast_mode: bool = False):
        self.n_trials = n_trials
        self.n_jobs = n_jobs if n_jobs > 0 else max(1, os.cpu_count() - 1)
        self.fast_mode = fast_mode
        self.study = None
        self.missions = MissionLibrary.all_missions()

    # ── 搜索空间（与 C 端对齐）─────────────────────────────────
    SEARCH_SPACE = {
        'energy_to_stamina_coeff':       (5e-7,   3e-6,   True),   # log-uniform
        'base_recovery_rate':            (5e-5,   3e-4,   True),
        'standing_recovery_multiplier':  (1.0,    2.0,    False),
        'crouching_recovery_multiplier': (1.1,    2.2,    False),
        'prone_recovery_multiplier':     (1.3,    3.0,    False),
        'fast_recovery_multiplier':      (1.2,    2.5,    False),
        'medium_recovery_multiplier':    (1.0,    1.8,    False),
        'slow_recovery_multiplier':      (0.3,    0.9,    False),
        'encumbrance_speed_penalty_coeff': (0.10, 0.40,   False),
        'encumbrance_stamina_drain_coeff': (1.0,  3.0,    False),
        'load_recovery_penalty_coeff':   (5e-5,   3e-4,   True),
        'posture_crouch_multiplier':     (1.2,    2.5,    False),
        'posture_prone_multiplier':      (1.8,    4.0,    False),
        'sprint_speed_boost':            (0.20,   0.40,   False),
        'recovery_nonlinear_coeff':      (0.2,    0.8,    False),
        'max_recovery_per_tick':         (2e-4,   8e-4,   False),
    }

    def suggest_params(self, trial: optuna.Trial) -> Dict:
        params = {}
        for name, (low, high, log_scale) in self.SEARCH_SPACE.items():
            if log_scale and low > 0 and high > 0:
                params[name] = trial.suggest_float(name, low, high, log=True)
            else:
                params[name] = trial.suggest_float(name, low, high)
        return params

    def objective(self, trial: optuna.Trial) -> Tuple[float, float, float]:
        # 1. 采样参数
        params = self.suggest_params(trial)

        # 2. 创建孪生实例
        twin = RSSDigitalTwin(RSSConstants(**params))

        # 3. 运行所有 Mission
        results = []
        dt_fast = 0.4 if self.fast_mode else 0.2
        for mission in self.missions:
            # 使用加速模式时调 dt
            twin._dt = dt_fast
            r = simulate_mission(twin, mission)
            results.append(r)

        # 4. 计算三目标指标
        metrics = compute_metrics(results, params)

        # 5. 存储中间结果供回调
        trial.set_user_attr('min_stamina', min(r.min_stamina for r in results))
        trial.set_user_attr('params', json.dumps({k: float(v) for k, v in params.items()}))

        return metrics.as_tuple()

    def run(self, study_name: str = "rss_v4", storage: str = None):
        """执行优化"""
        if not HAS_OPTUNA:
            raise RuntimeError("optuna not installed. Run: pip install optuna")

        if storage:
            study = optuna.create_study(
                study_name=study_name,
                storage=storage,
                directions=["minimize", "minimize", "minimize"],
                sampler=optuna.samplers.NSGAIISampler(population_size=64),
                load_if_exists=True,
            )
        else:
            study = optuna.create_study(
                study_name=study_name,
                directions=["minimize", "minimize", "minimize"],
                sampler=optuna.samplers.NSGAIISampler(population_size=64),
            )

        print(f"[V4] 开始优化: {self.n_trials} trials × {len(self.missions)} missions")
        print(f"[V4] 目标: minimize(combat_endurance, recovery_efficiency, parameter_realism)")
        t0 = time.time()

        study.optimize(
            self.objective,
            n_trials=self.n_trials,
            n_jobs=self.n_jobs,
            show_progress_bar=True,
        )

        elapsed = time.time() - t0
        self.study = study

        print(f"\n[V4] 优化完成 ({elapsed:.0f}s)")
        print(f"[V4] Pareto 前沿解数量: {len(study.best_trials)}")

        # 打印最佳解
        for i, t in enumerate(study.best_trials[:3]):
            print(f"\n  #{i+1}:")
            print(f"    combat_endurance={t.values[0]:.4f}")
            print(f"    recovery_efficiency={t.values[1]:.4f}")
            print(f"    parameter_realism={t.values[2]:.4f}")
            print(f"    energy_coeff={t.params['energy_to_stamina_coeff']:.2e}")
            print(f"    base_recovery={t.params['base_recovery_rate']:.2e}")

        return study

# =============================================================================
# 预设提取
# =============================================================================
def extract_presets(study: optuna.Study, output_dir: str = ".") -> Dict:
    """从 Pareto 前沿提取三个预设配置"""
    if not study or not study.best_trials:
        print("[V4] 无 Pareto 解，跳过预设提取")
        return {}

    # 按目标排序取前三
    trials = sorted(study.best_trials, key=lambda t: t.values[0])  # 按耐力排序

    presets = {}
    labels = ["EliteStandard", "StandardMilsim", "TacticalAction"]
    for i, label in enumerate(labels):
        if i >= len(trials):
            break
        t = trials[i]
        config = {k: float(v) for k, v in t.params.items()}
        config['_metrics'] = {
            'combat_endurance': float(t.values[0]),
            'recovery_efficiency': float(t.values[1]),
            'parameter_realism': float(t.values[2]),
        }
        presets[label] = config

        path = os.path.join(output_dir, f"optimized_rss_config_{label.lower()}_v4.json")
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        print(f"[V4] 预设已保存: {path}")

    return presets

# =============================================================================
# main
# =============================================================================
if __name__ == "__main__":
    if not HAS_OPTUNA:
        print("Error: optuna not installed. Run: pip install optuna")
        sys.exit(1)

    import argparse

    ap = argparse.ArgumentParser(description="RSS Pipeline v4 — 重新设计的优化管线")
    ap.add_argument("--trials", type=int, default=500, help="总 trial 数")
    ap.add_argument("--jobs", type=int, default=-1, help="并行数")
    ap.add_argument("--fast", action="store_true", help="加速模式 (dt=0.4)")
    ap.add_argument("--db", type=str, default=None, help="Optuna 数据库 URL")
    ap.add_argument("--output", type=str, default=".", help="预设输出目录")
    args = ap.parse_args()

    opt = RSSOptimizerV4(n_trials=args.trials, n_jobs=args.jobs, fast_mode=args.fast)
    study = opt.run(storage=args.db)
    extract_presets(study, args.output)
