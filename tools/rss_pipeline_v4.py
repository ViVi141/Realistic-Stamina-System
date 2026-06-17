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
    merge_game_aligned_params,
    STAMINA_TICK_SEC,
    RSS_PLAYER_TICK_SEC,
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
    temperature: float = 20.0  # 环境温度 (°C)，默认 20°C 无热/冷应激
    wind_speed: float = 0.0    # 风速 (m/s)，默认 0 无风阻

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

    @staticmethod
    def heavy_evacuation(load_kg: float = 45.0) -> Mission:
        """
        重载撤离 (10min, 45kg)
        节奏: 战术撤退步行 → 穿越开阔地急跑 → 恢复行军 → 穿越公路 → 最后冲刺上车
        覆盖 40kg+ 极端负重场景，确保优化器在重载下仍保持可玩性
        """
        return Mission(
            name="重载撤离",
            load_kg=load_kg,
            description="45kg重载撤离，10分钟内间歇冲刺到达安全点",
            phases=[
                Phase(180, 2.0,  MovementType.WALK,  Stance.STAND,  0,   1.0, "战术撤退"),
                Phase(20,  3.0,  MovementType.RUN,   Stance.STAND,  0,   1.2, "穿越开阔地"),
                Phase(120, 2.0,  MovementType.WALK,  Stance.STAND,  0,   1.0, "恢复行军"),
                Phase(20,  3.0,  MovementType.RUN,   Stance.STAND,  0,   1.2, "穿越公路"),
                Phase(180, 2.0,  MovementType.WALK,  Stance.STAND,  3,   1.3, "最后接近"),
                Phase(15,  3.8,  MovementType.SPRINT, Stance.STAND,  0,   1.0, "冲刺上车"),
                Phase(60,  0.0,  MovementType.IDLE,  Stance.STAND,  0,   1.0, "安全抵达"),
            ],
        )

    @staticmethod
    def amphibious_landing(load_kg: float = 25.0) -> Mission:
        """
        两栖登陆 (8min, 25kg)
        节奏: 游泳上岸 → 湿重冲刺 → 蹲姿交火 → 推进
        测试游泳模型 + 湿重衰减 + 上岸后立即战斗的极限场景
        对应 Arma 抢滩登陆 / 河流渡河玩法
        """
        return Mission(
            name="两栖登陆",
            load_kg=load_kg,
            temperature=20.0,  # 水温常温
            wind_speed=3.0,    # 中等海风
            description="25kg游泳上岸→湿重冲刺→交火，8分钟两栖突击",
            phases=[
                Phase(60,  1.2,  MovementType.WALK,   Stance.STAND,  0,  1.0, "游泳接近"),   # 游泳速度
                Phase(20,  4.5,  MovementType.SPRINT, Stance.STAND,  0,  1.2, "上岸冲刺"),   # 湿重冲刺
                Phase(90,  1.5,  MovementType.WALK,   Stance.CROUCH, 3,  1.0, "滩头交火"),   # 蹲姿战斗
                Phase(120, 2.5,  MovementType.WALK,   Stance.STAND,  5,  1.3, "向内陆推进"),  # 上坡推进
                Phase(90,  1.5,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "巩固阵地"),
                Phase(120, 0.0,  MovementType.IDLE,   Stance.STAND,  0,  1.0, "任务结束"),
            ],
        )

    @staticmethod
    def desert_patrol(load_kg: float = 30.0) -> Mission:
        """
        沙漠巡逻 (12min, 30kg, 35°C)
        节奏: 与巡逻接敌相同，但在高温环境下
        测试热应激对体力消耗和恢复的影响
        对应 Arma 沙漠地图（如 Arid）
        """
        return Mission(
            name="沙漠巡逻",
            load_kg=load_kg,
            temperature=35.0,   # 沙漠高温
            wind_speed=2.0,     # 轻微热风
            description="30kg沙漠巡逻，35°C高温+2m/s热风，12分钟",
            phases=[
                Phase(420, 2.8,  MovementType.WALK,   Stance.STAND,  0,  1.0, "沙漠行军"),
                Phase(30,  4.8,  MovementType.SPRINT, Stance.STAND,  0,  1.3, "冲刺接敌"),
                Phase(90,  3.5,  MovementType.RUN,    Stance.STAND,  0,  1.3, "跑步推进"),
                Phase(120, 1.5,  MovementType.WALK,   Stance.CROUCH, 0,  1.0, "交火"),
                Phase(60,  2.8,  MovementType.WALK,   Stance.STAND,  0,  1.2, "撤离"),
            ],
        )

    @staticmethod
    def hell_desert_breakout(load_kg: float = 30.0) -> Mission:
        """
        地狱沙漠突围 (15min, 30kg, 35°C)
        节奏: 沙漠巡逻强度 ×2，含 60s 趴姿休整窗口
        高温 + 长行军 + 3 冲刺 = 区分恢复参数的标尺场景
        """
        return Mission(
            name="地狱沙漠突围",
            load_kg=load_kg,
            temperature=35.0,   # 与沙漠巡逻相同温度
            wind_speed=2.0,     # 轻微热风
            description="30kg/35°C/15min 地狱突围，默认min≈0.2，差恢复参数会崩",
            phases=[
                Phase(360, 2.8,  MovementType.WALK,   Stance.STAND,  0,  1.2, "沙漠行军"),
                Phase(20,  4.8,  MovementType.SPRINT, Stance.STAND,  0,  1.3, "冲刺突破"),
                Phase(120, 2.8,  MovementType.WALK,   Stance.STAND,  3,  1.2, "上坡行军"),
                Phase(60,  0.0,  MovementType.IDLE,   Stance.PRONE,   0,  1.0, "趴姿休整"),
                Phase(180, 2.5,  MovementType.WALK,   Stance.STAND,  0,  1.3, "持续行军"),
                Phase(20,  4.5,  MovementType.SPRINT, Stance.STAND,  0,  1.3, "冲刺突破"),
                Phase(120, 2.5,  MovementType.WALK,   Stance.STAND,  0,  1.2, "撤离行军"),
                Phase(30,  0.0,  MovementType.IDLE,   Stance.STAND,  0,  1.0, "抵达"),
            ],
        )

    @classmethod
    def all_missions(cls, load_30: float = 30.0, load_25: float = 25.0,
                     load_35: float = 35.0, load_20: float = 20.0,
                     load_45: float = 45.0) -> List[Mission]:
        return [
            cls.patrol_contact(load_30),
            cls.desert_patrol(load_30),         # 环境压力测试
            cls.hell_desert_breakout(load_30),  # 极限高温 + 多冲刺
            cls.amphibious_landing(load_25),    # 游泳+湿重测试
            cls.urban_clearance(load_25),
            cls.mountain_approach(load_35),
            cls.vehicle_dismount(load_20),
            cls.heavy_evacuation(load_45),
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
    idle_duration_s: float         # 静止/恢复窗口总时长（秒）
    exhaustion_duration_s: float   # 体力<15%的时间
    completion_possible: bool      # 是否全程体力>0
    observed_depletion_pct_per_s: float = 0.0  # 运动阶段条上观测掉速（%/s，含 cap clamp）

def simulate_mission(twin: RSSDigitalTwin, mission: Mission) -> MissionResult:
    """运行一个完整 Mission 的数字孪生仿真（PlayerBase 同序 game_player_tick）。"""
    twin.reset()

    twin._scenario_wind_drag = 0.0
    twin.environment_factor.temperature = mission.temperature
    twin.environment_factor.wind_speed = mission.wind_speed
    if mission.temperature < 5.0:
        c = twin.constants
        cold_coeff = getattr(c, 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF', 0.05)
        twin.environment_factor.cold_stress = (5.0 - mission.temperature) * cold_coeff
        cold_static = getattr(c, 'ENV_TEMPERATURE_COLD_STATIC_PENALTY', 0.03)
        twin.environment_factor.cold_static_penalty = (5.0 - mission.temperature) * cold_static
    else:
        twin.environment_factor.cold_stress = 0.0
        twin.environment_factor.cold_static_penalty = 0.0
    if mission.wind_speed >= 1.0:
        wind_coeff = getattr(twin.constants, 'ENV_WIND_RESISTANCE_COEFF', 0.05)
        twin._scenario_wind_drag = min(1.0, mission.wind_speed * wind_coeff)

    dt = getattr(twin, '_dt', RSS_PLAYER_TICK_SEC)
    current_time = 0.0
    stamina_trace = []
    speed_trace = []
    total_recovery_gain = 0.0
    idle_duration_s = 0.0
    exhaustion_duration = 0.0
    current_weight = mission.current_weight
    wind_drag = getattr(twin, '_scenario_wind_drag', 0.0)

    for phase in mission.phases:
        steps = int(phase.duration_s / dt)
        intent_phase = int(phase.movement)
        is_idle_phase = (intent_phase == MovementType.IDLE)
        if is_idle_phase:
            idle_duration_s += phase.duration_s
        for _ in range(steps):
            prev_stamina = twin.stamina

            if is_idle_phase:
                drain_speed = twin.game_player_tick(
                    MovementType.IDLE,
                    current_weight,
                    phase.grade_pct,
                    phase.terrain,
                    phase.stance,
                    current_time,
                    dt,
                    wind_drag=wind_drag,
                    enable_randomness=False,
                )
            else:
                drain_speed = twin.game_player_tick(
                    intent_phase,
                    current_weight,
                    phase.grade_pct,
                    phase.terrain,
                    phase.stance,
                    current_time,
                    dt,
                    wind_drag=wind_drag,
                    enable_randomness=False,
                )

            current_time += dt
            stamina_trace.append(twin.stamina)
            speed_trace.append(drain_speed)

            if twin.stamina > prev_stamina:
                total_recovery_gain += twin.stamina - prev_stamina
            if twin.stamina < 0.15:
                exhaustion_duration += dt

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

    depletion_samples = []
    for k in range(1, len(stamina_trace)):
        prev_s = stamina_trace[k - 1]
        cur_s = stamina_trace[k]
        if cur_s < prev_s - 1e-9 and prev_s > 0.82:
            depletion_samples.append((prev_s - cur_s) / dt * 100.0)
    observed_depletion = 0.0
    if depletion_samples:
        observed_depletion = float(np.mean(depletion_samples))

    return MissionResult(
        mission_name=mission.name,
        total_duration_s=mission.total_duration_s,
        stamina_trace=stamina_trace,
        speed_trace=speed_trace,
        min_stamina=min_stamina,
        mean_stamina_active=float(mean_active),
        recovery_gain=total_recovery_gain,
        idle_duration_s=idle_duration_s,
        exhaustion_duration_s=exhaustion_duration,
        completion_possible=min_stamina > 0.0,
        observed_depletion_pct_per_s=observed_depletion,
    )

# =============================================================================
# 指标计算 — 三目标体系（v4.1 重设计）
# =============================================================================
# 三目标均为 minimize；选解阶段按档位取 Pareto 不同区域：
#   EliteStandard  → 低 combat_reserve + 低 recovery_pace + 低 realism
#   StandardMilsim → 三目标归一化后接近理想折中点
#   TacticalAction → 高 combat_reserve + 高 recovery_pace（最宽容）
# =============================================================================

# Hardcore 生理参考锚点（与 StaminaConstants / ConfigBridge fallback 一致）
HARDCORE_PARAM_REFS = {
    'energy_to_stamina_coeff': 9.5e-7,
    'base_recovery_rate': 1.0e-4,
    'standing_recovery_multiplier': 0.85,
    'crouching_recovery_multiplier': 1.6,
    'prone_recovery_multiplier': 1.9,
    'fast_recovery_multiplier': 1.6,
    'medium_recovery_multiplier': 1.0,
    'slow_recovery_multiplier': 0.35,
    'encumbrance_speed_penalty_coeff': 0.28,
    'encumbrance_stamina_drain_coeff': 2.8,
    'load_recovery_penalty_coeff': 2.0e-4,
    'posture_crouch_multiplier': 3.0,
    'posture_prone_multiplier': 3.5,
    'sprint_speed_boost': 0.22,
    'recovery_nonlinear_coeff': 0.5,
    'max_recovery_per_tick': 4.0e-4,
    'willpower_threshold': 0.35,
    'sprint_enable_threshold': 0.25,
}

# Standard 档理想折中点（归一化空间：中等 combat_ease / recovery_ease / 低 realism）
STANDARD_IDEAL_NORM = np.array([0.48, 0.45, 0.35])


@dataclass
class Metrics:
    """三目标指标（均为 minimize；值越低对 Elite 越硬核）"""
    combat_ease: float         # 运动阶段体力余量 [0,1]；越低 = 战斗越吃紧
    recovery_ease: float       # 静止窗口回血速率；越低 = 恢复越慢
    parameter_realism: float   # 偏离 Hardcore 生理参考 + 可玩性下限惩罚

    def as_tuple(self) -> Tuple[float, float, float]:
        return (self.combat_ease, self.recovery_ease, self.parameter_realism)


def compute_metrics(results: List[MissionResult], params: Dict) -> Metrics:
    """从多个 Mission 结果计算三目标指标"""

    # ── 目标 1: 战斗体力余量（combat_reserve，minimize = 更硬核）──────────
    weights = {
        "地狱沙漠突围": 0.22, "巡逻接敌": 0.18, "沙漠巡逻": 0.16, "重载撤离": 0.14,
        "山地接近": 0.10, "两栖登陆": 0.08, "城镇清扫": 0.07, "载具下车突击": 0.05,
    }
    weighted_mean = 0.0
    total_weight = 0.0
    for r in results:
        w = weights.get(r.mission_name, 0.25)
        weighted_mean += r.mean_stamina_active * w
        total_weight += w
    weighted_mean = weighted_mean / total_weight if total_weight > 0 else 0.5

    collapse_penalty = 0.0
    exhaustion_ease_penalty = 0.0
    for r in results:
        if r.min_stamina < 0.10:
            collapse_penalty += (0.10 - r.min_stamina) * 2.0
        if r.total_duration_s > 0.0:
            exh_ratio = r.exhaustion_duration_s / r.total_duration_s
            exhaustion_ease_penalty += min(exh_ratio, 0.35) * 0.12

    combat_ease = weighted_mean - collapse_penalty - exhaustion_ease_penalty
    combat_ease = max(0.0, min(1.0, combat_ease))

    # ── 目标 2: 恢复宽松度（recovery_ease，minimize = 恢复越慢）────────────
    total_recovery = sum(r.recovery_gain for r in results)
    total_idle = sum(r.idle_duration_s for r in results)
    recovery_ease = total_recovery / max(total_idle, 60.0)

    # ── 目标 3: 参数拟真度（parameter_realism，minimize）────────────────
    realism_score = 0.0
    refs = HARDCORE_PARAM_REFS

    # 可玩性：恢复过慢则抬高 realism（避免 Elite 选出“永动机/僵尸”两端）
    min_playable_recovery = 0.00006
    if recovery_ease < min_playable_recovery:
        realism_score += (min_playable_recovery - recovery_ease) / min_playable_recovery * 4.0

    # 可玩性：战斗体力余量过低则抬高 realism（避免三档全部“归零”）
    min_playable_combat = 0.10
    if combat_ease < min_playable_combat:
        realism_score += (min_playable_combat - combat_ease) / min_playable_combat * 5.0

    ec = params.get('energy_to_stamina_coeff', refs['energy_to_stamina_coeff'])
    ec_ref = refs['energy_to_stamina_coeff']
    realism_score += abs(math.log10(ec / ec_ref)) * 1.5

    br = params.get('base_recovery_rate', refs['base_recovery_rate'])
    br_ref = refs['base_recovery_rate']
    realism_score += abs(br - br_ref) / br_ref * 4.0

    pm = params.get('prone_recovery_multiplier', refs['prone_recovery_multiplier'])
    cm = params.get('crouching_recovery_multiplier', refs['crouching_recovery_multiplier'])
    sm = params.get('standing_recovery_multiplier', refs['standing_recovery_multiplier'])
    if pm <= cm:
        realism_score += (cm - pm + 0.1) * 10.0
    if cm <= sm:
        realism_score += (sm - cm + 0.1) * 10.0
    if sm >= 1.0:
        realism_score += (sm - 0.85) * 8.0

    fm = params.get('fast_recovery_multiplier', refs['fast_recovery_multiplier'])
    mm = params.get('medium_recovery_multiplier', refs['medium_recovery_multiplier'])
    lm = params.get('slow_recovery_multiplier', refs['slow_recovery_multiplier'])
    if fm <= mm:
        realism_score += (mm - fm + 0.1) * 8.0
    if mm <= lm:
        realism_score += (lm - mm + 0.1) * 8.0

    pcm = params.get('posture_crouch_multiplier', refs['posture_crouch_multiplier'])
    if pcm < 1.5:
        realism_score += (1.5 - pcm) * 5.0

    espc = params.get('encumbrance_speed_penalty_coeff', refs['encumbrance_speed_penalty_coeff'])
    if espc < 0.15 or espc > 0.50:
        realism_score += min(abs(espc - 0.15), abs(espc - 0.50)) * 20.0

    wt = params.get('willpower_threshold', refs['willpower_threshold'])
    wt_ref = refs['willpower_threshold']
    realism_score += abs(wt - wt_ref) / wt_ref * 2.0

    se = params.get('sprint_enable_threshold', refs['sprint_enable_threshold'])
    se_ref = refs['sprint_enable_threshold']
    realism_score += abs(se - se_ref) / se_ref * 2.0

    parameter_realism = realism_score

    return Metrics(
        combat_ease=float(combat_ease),
        recovery_ease=float(recovery_ease),
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

    # ── 搜索空间（与 C 端 Hardcore 锚点对齐，站姿恢复上限 ≤1.0）──────────
    SEARCH_SPACE = {
        'energy_to_stamina_coeff':       (6e-7,   1.4e-6, True),   # log-uniform，偏高=更耗体
        'base_recovery_rate':            (6e-5,   1.5e-4, True),   # Hardcore 上限约 1.5e-4
        'standing_recovery_multiplier':  (0.65,   0.98,   False),  # 站姿应 <1.0
        'crouching_recovery_multiplier': (1.2,    2.0,    False),
        'prone_recovery_multiplier':     (1.5,    2.3,    False),
        'fast_recovery_multiplier':      (1.2,    1.9,    False),
        'medium_recovery_multiplier':    (0.75,   1.2,    False),
        'slow_recovery_multiplier':      (0.25,   0.50,   False),
        'encumbrance_speed_penalty_coeff': (0.18, 0.42,   False),
        'encumbrance_stamina_drain_coeff': (2.0,  3.2,    False),
        'load_recovery_penalty_coeff':   (8e-5,   3.5e-4, True),
        'posture_crouch_multiplier':     (2.0,    3.5,    False),
        'posture_prone_multiplier':      (2.5,    4.5,    False),
        'sprint_speed_boost':            (0.18,   0.28,   False),
        'recovery_nonlinear_coeff':      (0.25,   0.60,   False),
        'max_recovery_per_tick':         (2.0e-4, 4.5e-4, False),
        'willpower_threshold':           (0.30,   0.42,   False),
        'sprint_enable_threshold':       (0.20,   0.32,   False),
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
        params = merge_game_aligned_params(self.suggest_params(trial))

        # 2. 创建孪生实例（含游戏固定参数，与 Init*Defaults 一致）
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
        print(f"[V4] 目标: minimize(combat_ease, recovery_ease, parameter_realism)")
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
            print(f"    combat_ease={t.values[0]:.4f}")
            print(f"    recovery_ease={t.values[1]:.6f}")
            print(f"    parameter_realism={t.values[2]:.4f}")
            print(f"    energy_coeff={t.params['energy_to_stamina_coeff']:.2e}")
            print(f"    base_recovery={t.params['base_recovery_rate']:.2e}")

        return study

# =============================================================================
# 预设提取
# =============================================================================
def _normalize_objectives(values: np.ndarray) -> np.ndarray:
    """将三目标矩阵归一化到 [0, 1]。"""
    v_min = values.min(axis=0)
    v_max = values.max(axis=0)
    v_range = v_max - v_min
    v_range[v_range == 0] = 1.0
    return (values - v_min) / v_range


def _pick_unique_index(candidates: np.ndarray, used: set, n: int) -> int:
    """从候选索引中选取第一个未使用的。"""
    for c in candidates:
        idx = int(c)
        if idx not in used:
            return idx
    return int(candidates[0]) if len(candidates) > 0 else 0


def extract_presets(study, output_dir: str = ".") -> Dict:
    """从 Pareto 前沿提取三份游戏预设 JSON

    选解策略（v4.1）：
      - EliteStandard  → 低 combat_ease + 低 recovery_ease + 低 realism（最硬核）
      - StandardMilsim → 归一化空间接近 STANDARD_IDEAL_NORM（折中）
      - TacticalAction → 高 combat_ease + 高 recovery_ease（最宽容）
    """
    if not study or not study.best_trials:
        print("[V4] 无 Pareto 解，跳过预设提取")
        return {}

    trials = study.best_trials
    n = len(trials)
    values = np.array([[t.values[0], t.values[1], t.values[2]] for t in trials])

    # ── EliteStandard: 战斗最吃紧的 20% 池中，再选恢复最慢 ─────────────
    elite_pool_size = max(3, n // 5)
    elite_pool = np.argsort(values[:, 0])[:elite_pool_size]
    elite_sub = values[elite_pool, 1]
    idx_elite = int(elite_pool[int(np.argmin(elite_sub))])

    used = {idx_elite}

    # ── TacticalAction: 战斗最宽松的 20% 池中，再选恢复最快 ───────────
    tac_pool_size = max(3, n // 5)
    tac_sorted = np.argsort(-values[:, 0])
    tac_pool = []
    for c in tac_sorted:
        ci = int(c)
        if ci not in used:
            tac_pool.append(ci)
        if len(tac_pool) >= tac_pool_size:
            break
    if not tac_pool:
        tac_pool = [int(tac_sorted[0])]
    tac_sub = values[tac_pool, 1]
    idx_tactical = int(tac_pool[int(np.argmax(tac_sub))])
    used.add(idx_tactical)

    # ── StandardMilsim: 中间战斗区间，接近 Elite/Tactical 的中点 ───────
    mid_start = max(1, n // 5)
    mid_end = max(mid_start + 1, n - n // 5)
    combat_sorted = np.argsort(values[:, 0])
    balanced_pool = []
    for c in combat_sorted[mid_start:mid_end]:
        ci = int(c)
        if ci not in used:
            balanced_pool.append(ci)
    if not balanced_pool:
        for c in combat_sorted:
            ci = int(c)
            if ci not in used:
                balanced_pool.append(ci)
                break
    target_combat = (values[idx_elite, 0] + values[idx_tactical, 0]) * 0.5
    target_recovery = (values[idx_elite, 1] + values[idx_tactical, 1]) * 0.5
    balanced_scores = []
    for i in balanced_pool:
        score = abs(values[i, 0] - target_combat)
        score += abs(values[i, 1] - target_recovery) * 80.0
        score += values[i, 2] * 0.15
        balanced_scores.append(score)
    idx_balanced = int(balanced_pool[int(np.argmin(balanced_scores))])

    t_elite = trials[idx_elite]
    t_balanced = trials[idx_balanced]
    t_tactical = trials[idx_tactical]

    unique_indices = len({idx_elite, idx_balanced, idx_tactical})
    if unique_indices < 3:
        print(f"[V4] 警告: 预设多样性不足 ({unique_indices}/3 个唯一点)")
        print(f"       combat_ease 范围: [{values[:,0].min():.4f}, {values[:,0].max():.4f}]")
        print(f"       recovery_ease 范围: [{values[:,1].min():.6f}, {values[:,1].max():.6f}]")
        print(f"       parameter_realism 范围: [{values[:,2].min():.4f}, {values[:,2].max():.4f}]")
        print(f"       → 建议: 增加 trial 数")

    selection = [
        (
            "EliteStandard",
            t_elite,
            "低 combat_ease + 低 recovery_ease + 低 realism → 最拟真/最硬核",
        ),
        ("StandardMilsim", t_balanced, "三目标折中 → 拟真与可玩性平衡"),
        ("TacticalAction", t_tactical, "高 combat_ease + 高 recovery_ease → 战斗最宽容"),
    ]

    legacy_hardcore = os.path.join(output_dir, "optimized_rss_config_hardcore_v4.json")
    if os.path.isfile(legacy_hardcore):
        os.remove(legacy_hardcore)
        print(f"[V4] 已删除遗留 hardcore JSON")

    presets = {}
    for label, t, philosophy in selection:
        config = {k: float(v) for k, v in t.params.items()}
        config['_metrics'] = {
            'combat_ease': float(t.values[0]),
            'recovery_ease': float(t.values[1]),
            'parameter_realism': float(t.values[2]),
        }
        config['_philosophy'] = philosophy
        presets[label] = config

        path = os.path.join(output_dir, f"optimized_rss_config_{label.lower()}_v4.json")
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2, ensure_ascii=False)

        print(f"[V4] {label}: {philosophy}")
        print(f"       ease={t.values[0]:.4f}  recovery={t.values[1]:.6f}  realism={t.values[2]:.4f}")
        print(f"       saved → {path}")

    _print_tier_order_check(presets)
    return presets


def _print_tier_order_check(presets: Dict) -> None:
    """诊断三档排序是否符合 Elite < Standard < Tactical。"""
    if not all(k in presets for k in ("EliteStandard", "StandardMilsim", "TacticalAction")):
        return
    elite_e = presets["EliteStandard"]["_metrics"]["combat_ease"]
    std_e = presets["StandardMilsim"]["_metrics"]["combat_ease"]
    tac_e = presets["TacticalAction"]["_metrics"]["combat_ease"]
    elite_r = presets["EliteStandard"]["_metrics"]["recovery_ease"]
    std_r = presets["StandardMilsim"]["_metrics"]["recovery_ease"]
    tac_r = presets["TacticalAction"]["_metrics"]["recovery_ease"]
    ok_ease = elite_e <= std_e <= tac_e
    ok_recovery = elite_r <= std_r <= tac_r
    status = "OK" if (ok_ease and ok_recovery) else "WARN"
    print(f"[V4] 档位排序检查 [{status}]: "
          f"combat_ease {elite_e:.3f} ≤ {std_e:.3f} ≤ {tac_e:.3f} ? {ok_ease}; "
          f"recovery_ease {elite_r:.6f} ≤ {std_r:.6f} ≤ {tac_r:.6f} ? {ok_recovery}")

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
