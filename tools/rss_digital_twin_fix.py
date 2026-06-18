#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Digital Twin - 从 C 源文件完全重写
严格对照 SCR_RealisticStaminaSystem.c、SCR_StaminaConstants.c、SCR_StaminaConsumption.c、
SCR_StaminaRecovery.c、SCR_StaminaUpdateCoordinator.c、SCR_EncumbranceCache.c 实现。

时间步长：消耗/恢复率按 STAMINA_TICK_SEC=0.2s 定义；玩家 tick 默认 17ms（与 RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS 一致）。
simulate_mission 使用 PlayerBase 同序逻辑：先 GetVelocity 测速再算消耗，冲刺/跑步消耗用引擎原速
(GetDynamicOriginalEngineMaxSpeed: Sprint=5.5, Run=3.8)，与 RSS SetSpeedLimit 理论限速解耦。
"""

import json
import numpy as np
import math
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict, Tuple, Optional
import random

# Tobler 徒步函数：与 C 端 SCR_RealisticStaminaSystem.CalculateSlopeAdjustedTargetSpeed 完全一致
# W = 6 * exp(-3.5 * |S + 0.05|), TOBLER_W_AT_FLAT_KMH = 5.039
TOBLER_W_AT_FLAT_KMH = 5.039

# 体力净变化率按该秒长定义（与 _calculate_drain_rate_c_aligned 注释「每 0.2s」一致）
STAMINA_TICK_SEC = 0.2
# 玩家体力 tick 间隔（SCR_StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS = 17）
RSS_PLAYER_TICK_SEC = 0.017
VELOCITY_HORIZ_CAP_MS = 7.0


def tobler_speed_multiplier(angle_deg: float,
                            uphill_boost: float = 1.15,
                            downhill_boost: float = 1.15,
                            downhill_max: float = 1.25,
                            dampening: float = 0.7,
                            min_mult: float = 0.15) -> float:
    """与 C 端 CalculateSlopeAdjustedTargetSpeed 完全一致。
    返回速度乘数 [0.15, downhill_max]。下坡可大于 1.0。
    """
    s = math.tan(math.radians(angle_deg))
    s = max(-1.0, min(1.0, s))
    w_kmh = 6.0 * math.exp(-3.5 * abs(s + 0.05))
    mult = w_kmh / TOBLER_W_AT_FLAT_KMH
    mult = max(mult, min_mult)
    if angle_deg > 0.0:
        mult = mult * uphill_boost
    elif angle_deg < 0.0:
        mult = mult * downhill_boost
        mult = min(mult, downhill_max)
    mult = 1.0 + dampening * (mult - 1.0)
    return max(min_mult, min(mult, downhill_max))


# =============================================================================
# RSSConstants - 与 SCR_StaminaConstants.c 完全同步
# =============================================================================
@dataclass
class RSSConstants:
    """RSS 常量，与 C 端 SCR_StaminaConstants.c 一一对应"""

    GAME_MAX_SPEED = 5.5
    CHARACTER_WEIGHT = 90.0
    REFERENCE_WEIGHT = 90.0
    BASE_WEIGHT = 1.36

    # Pandolf 1977 论文系数 [HARD]
    PANDOLF_BASE_COEFF = 2.7
    PANDOLF_VELOCITY_COEFF = 3.2
    PANDOLF_VELOCITY_OFFSET = 0.7
    PANDOLF_GRADE_BASE_COEFF = 0.23
    PANDOLF_GRADE_VELOCITY_COEFF = 1.34
    PANDOLF_STATIC_COEFF_1 = 1.2
    PANDOLF_STATIC_COEFF_2 = 1.6

    # [HARD] Pandolf 下坡修正（与 C 端 3.15.12 一致）
    GENTLE_DOWNHILL_GRADE_MAX = 12.0
    GENTLE_DOWNHILL_SAVINGS_MULTIPLIER = 1.25
    STEEP_DOWNHILL_GRADE_THRESHOLD = 15.0
    STEEP_DOWNHILL_PENALTY_MAX_FRACTION = 0.5

    # 预计算人物属性 [HARD] (age=22, fitness=1.0)
    FIXED_FITNESS_EFFICIENCY_FACTOR = 0.70
    FIXED_FITNESS_RECOVERY_MULTIPLIER = 1.25
    FIXED_AGE_RECOVERY_MULTIPLIER = 1.053
    FIXED_PANDOLF_FITNESS_BONUS = 0.80

    # 代谢阈值 [HARD]
    AEROBIC_THRESHOLD = 0.6
    ANAEROBIC_THRESHOLD = 0.8
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2

    # 恢复系统
    BASE_RECOVERY_RATE = 0.00010  # Hardcore：原0.00015
    RECOVERY_NONLINEAR_COEFF = 0.5  # Hardcore：原0.792
    FAST_RECOVERY_DURATION_MINUTES = 0.4
    FAST_RECOVERY_MULTIPLIER = 1.6  # Hardcore：原2.395
    MEDIUM_RECOVERY_START_MINUTES = 0.4
    MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
    MEDIUM_RECOVERY_MULTIPLIER = 1.0  # Hardcore：原1.137
    SLOW_RECOVERY_START_MINUTES = 10.0
    SLOW_RECOVERY_MULTIPLIER = 0.35  # Hardcore：原0.476

    # 姿态恢复倍数
    STANDING_RECOVERY_MULTIPLIER = 0.85  # Hardcore：原1.103
    CROUCHING_RECOVERY_MULTIPLIER = 1.6  # Hardcore：原1.4
    PRONE_RECOVERY_MULTIPLIER = 1.9  # Hardcore：原2.344

    # 姿态消耗倍数（Hardcore 提升：蹲姿+67%，趴姿+40%）
    POSTURE_CROUCH_MULTIPLIER = 3.0  # Hardcore：原1.8
    POSTURE_PRONE_MULTIPLIER = 3.5  # Hardcore：原2.5

    # 恢复/消耗相关
    REST_RECOVERY_RATE = 0.250  # pts/s
    REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2  # = 0.0005

    # 负重
    BODY_TOLERANCE_BASE = 90.0
    ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.28  # Hardcore：原0.20
    ENCUMBRANCE_SPEED_PENALTY_EXPONENT = 1.5
    ENCUMBRANCE_SPEED_PENALTY_MAX = 0.75
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.8  # Hardcore：原1.5
    LOAD_RECOVERY_PENALTY_COEFF = 0.0002  # Hardcore：原0.0001
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0  # 与 C 静态常量一致

    SPRINT_VELOCITY_THRESHOLD = 5.5
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.5  # 与 SCR_RSS_Settings Init*Defaults / 游戏一致
    TARGET_RUN_SPEED = 3.8
    TARGET_RUN_SPEED_MULTIPLIER = 3.8 / 5.5
    SMOOTH_TRANSITION_START = 0.35  # Hardcore：原0.25
    SMOOTH_TRANSITION_END = 0.05
    MIN_LIMP_SPEED_MULTIPLIER = 1.0 / 5.5
    EXHAUSTION_THRESHOLD = 0.0
    SPRINT_ENABLE_THRESHOLD = 0.25  # Hardcore：原0.18
    SPRINT_SPEED_BOOST = 0.22  # Hardcore：原0.30
    WILLPOWER_THRESHOLD = 0.35  # Hardcore：新增可配置参数（原0.25硬编码）

    TACTICAL_SPRINT_BURST_DURATION = 8.0
    TACTICAL_SPRINT_BURST_BUFFER_DURATION = 5.0
    TACTICAL_SPRINT_BURST_ENCUMBRANCE_FACTOR = 0.2
    TACTICAL_SPRINT_COOLDOWN = 15.0

    FATIGUE_START_TIME_MINUTES = 5.0
    FATIGUE_ACCUMULATION_COEFF = 0.025  # Hardcore：原0.015
    FATIGUE_MAX_FACTOR = 2.5  # Hardcore：原2.0
    FATIGUE_RECOVERY_PENALTY = 0.05
    FATIGUE_RECOVERY_DURATION_MINUTES = 20.0

    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 5.0  # Hardcore：原3.0

    EPOC_DELAY_SECONDS = 2.0
    EPOC_DRAIN_RATE = 0.001

    ENERGY_TO_STAMINA_COEFF = 9.5e-07  # Hardcore：原7.17e-07

    # 动作消耗物理模型
    JUMP_EFFICIENCY = 0.22
    JUMP_HEIGHT_GUESS = 0.5
    JUMP_HORIZONTAL_SPEED_GUESS = 0.0
    CLIMB_ISO_EFFICIENCY = 0.12
    JUMP_GRAVITY = 9.81
    JUMP_STAMINA_TO_JOULES = 3.14e5
    JUMP_VAULT_MAX_DRAIN_CLAMP = 0.15
    VAULT_VERT_LIFT_GUESS = 0.5
    VAULT_LIMB_FORCE_RATIO = 0.5
    VAULT_BASE_METABOLISM_WATTS = 50.0

    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    ENV_TEMPERATURE_COLD_STATIC_PENALTY = 0.03
    ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.5
    ENV_HEAT_STRESS_INDOOR_REDUCTION = 0.5
    ENV_SURFACE_WETNESS_PENALTY_MAX = 0.15
    ENV_WIND_RESISTANCE_COEFF = 0.05
    ENV_WIND_TAILWIND_BONUS = 0.02
    ENV_WIND_TAILWIND_MAX = 0.15

    # Tobler 坡度速度常量（SCR_StaminaConstants.c:466-468）
    UPHILL_SPEED_BOOST = 1.15
    DOWNHILL_SPEED_BOOST = 1.15
    DOWNHILL_SPEED_MAX_MULTIPLIER = 1.25
    TOBLER_DAMPENING = 0.7

    # T1 负重代谢阻尼：训练有素操作员只承受负重额外代谢的 70%
    LOAD_METABOLIC_DAMPENING = 0.70
    # 恢复速率上限（每 tick），防止过快回血
    MAX_RECOVERY_PER_TICK = 0.0004

    # [DEPRECATED] C 端 CalculateSlopeStaminaDrainMultiplier 未被调用，坡度由 Pandolf grade 承担
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03

    def __init__(self, **kwargs):
        # 构建 lowercase -> uppercase 映射（优化器传 lowercase，类属性是 uppercase）
        upper_map = {}
        for attr_name in dir(self.__class__):
            if not attr_name.startswith('_') and attr_name.isupper():
                upper_map[attr_name.lower()] = attr_name

        for key, value in kwargs.items():
            # 先尝试原始 key
            if hasattr(self, key):
                setattr(self, key, value)
            # 再尝试 uppercase 映射
            elif key.lower() in upper_map:
                setattr(self, upper_map[key.lower()], value)


def encumbrance_speed_penalty_base(constants, current_weight: float) -> float:
    """
    与 RSSDigitalTwin._encumbrance_speed_penalty_base 一致，基于常量与总重计算负重速度惩罚基数。
    用于在无孪生实例时计算「当前负重下的 run 速度」。
    """
    base_w = getattr(constants, 'BASE_WEIGHT', 1.36)
    bw = getattr(constants, 'CHARACTER_WEIGHT', 90.0)
    coeff = getattr(constants, 'ENCUMBRANCE_SPEED_PENALTY_COEFF', 0.20)
    max_pen = getattr(constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
    effective_load = max(0.0, current_weight - bw - base_w)
    ratio = effective_load / bw if bw > 0.0 else 0.0
    ratio = float(np.clip(ratio, 0.0, 2.0))
    raw = 0.0
    if ratio <= 0.3:
        raw = 0.15 * ratio
    elif ratio <= 0.6:
        seg = ratio - 0.3
        raw = 0.045 + 0.35 * (seg ** 1.5)
    else:
        seg = ratio - 0.6
        raw = 0.25 + 0.65 * (seg * seg)
    raw = raw * (coeff / 0.20)
    return float(np.clip(raw, 0.0, max_pen))


def run_speed_at_weight(constants, current_weight: float) -> float:
    """
    返回在给定总重（body+equipment）下、满体力时的 RUN 速度 (m/s)。
    与 C 端一致：Run 时 enc_penalty = base_pen * (1 + currentSpeed/GAME_MAX_SPEED)，再 clamp 到 max_pen；
    不动点 v = target_run * (1 - enc_penalty(v)) 解得
    v = target_run*(1-base_pen)/(1 + target_run*base_pen/game_max)，若 enc>max_pen 则 v = target_run*(1-max_pen)。
    """
    game_max = getattr(constants, 'GAME_MAX_SPEED', 5.5)
    target_run = getattr(constants, 'TARGET_RUN_SPEED', 3.8)
    max_pen = getattr(constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
    base_pen = encumbrance_speed_penalty_base(constants, current_weight)
    denom = 1.0 + target_run * base_pen / game_max
    v = target_run * (1.0 - base_pen) / denom
    enc_at_v = base_pen * (1.0 + v / game_max)
    if enc_at_v > max_pen:
        v = target_run * (1.0 - max_pen)
    return float(np.clip(v, 0.15 * game_max, target_run))


def walk_speed_at_weight(constants, current_weight: float) -> float:
    """
    返回在给定总重下、满体力时的 WALK 速度 (m/s)。
    与 C 端 WALK 逻辑一致：mult = (walk_base*0.8)*(1 - enc_penalty)，speed_ratio 取 0.8。
    """
    game_max = getattr(constants, 'GAME_MAX_SPEED', 5.5)
    max_pen = getattr(constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
    base_pen = encumbrance_speed_penalty_base(constants, current_weight)
    speed_ratio = 0.8
    enc_penalty = base_pen * (1.0 + speed_ratio)
    enc_penalty = float(np.clip(enc_penalty, 0.0, max_pen))
    return float(game_max * 0.8 * (1.0 - enc_penalty))


def sprint_speed_at_weight(constants, current_weight: float) -> float:
    """
    返回在给定总重下、满体力时的 SPRINT 速度 (m/s)。
    与 C 端 SPRINT 逻辑一致：
    - 先计算该负重下的 Run 速度（已包含 encumbrance 减速）
    - 再在 Run 基础上乘以 (1 + sprint_boost)
    - 然后应用 Sprint 额外的 encumbrance 惩罚（base_pen * 1.5），并 clamp 到 max_pen
    - 最终速度 clamp 到 GAME_MAX_SPEED（引擎最大 5.5 m/s）
    """
    game_max = getattr(constants, 'GAME_MAX_SPEED', 5.5)
    sprint_boost = getattr(constants, 'SPRINT_SPEED_BOOST', 0.30)
    max_pen = getattr(constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
    base_pen = encumbrance_speed_penalty_base(constants, current_weight)

    # 先得到该负重下的 Run 速度（已包含基础 encumbrance 惩罚）
    run_speed = run_speed_at_weight(constants, current_weight)

    # Sprint 额外 encumbrance 惩罚（C 端：Run 基础 enc_penalty 再乘 1.5）
    enc_penalty = float(np.clip(base_pen * 1.5, 0.0, max_pen))

    # 在 Run 速度上应用 Sprint 提升与额外惩罚，并限制到引擎最大速度
    sprint_speed = run_speed * (1.0 + sprint_boost) * (1.0 - enc_penalty)
    sprint_speed = float(np.clip(sprint_speed, 0.0, game_max))
    return sprint_speed


class EnvironmentFactor:
    def __init__(self, constants):
        self.constants = constants
        self.heat_stress = 0.0
        self.cold_stress = 0.0
        self.cold_static_penalty = 0.0
        self.surface_wetness = 0.0
        self.temperature = 20.0
        self.wind_speed = 0.0

    def adjust_energy_for_temperature(self, base_drain: float) -> float:
        """与 C 端 AdjustEnergyForTemperature (SCR_EnvironmentFactor.c:2298-2330) 一致。
        在消耗端加入温度热调节额外功耗。
        """
        t_eff = self.temperature - 1.35 * math.sqrt(max(0.0, self.wind_speed))
        extra_watts = 0.0
        t_low = 18.0
        t_high = 27.0
        if t_eff < t_low:
            dt_cold = t_low - t_eff
            extra_watts = 0.15 * (dt_cold * dt_cold)
        elif t_eff > t_high:
            dt_hot = t_eff - t_high
            extra_watts = 2.0 * (dt_hot * dt_hot)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 9e-7)
        extra_per_tick = extra_watts * coeff * 0.2
        return base_drain + extra_per_tick

    def get_heat_stress_multiplier(self, indoor: bool = False) -> float:
        """与 SCR_EnvironmentFactor.GetHeatStressMultiplier 一致（26°C 起算，每 +1°C +2%）。"""
        threshold = 26.0
        temp = self.temperature
        if temp < threshold:
            mult = 1.0
        else:
            mult = 1.0 + (temp - threshold) * 0.02
        if indoor:
            indoor_reduction = getattr(self.constants, 'ENV_HEAT_STRESS_INDOOR_REDUCTION', 0.5)
            mult = mult * (1.0 - indoor_reduction)
        max_mult = getattr(self.constants, 'ENV_HEAT_STRESS_MAX_MULTIPLIER', 1.5)
        return float(np.clip(mult, 1.0, max_mult))

    def get_heat_stress_penalty(self) -> float:
        """与 GetHeatStressPenalty 一致：恢复率惩罚 = multiplier - 1（上限 0.5）。"""
        mult = self.get_heat_stress_multiplier(indoor=False)
        return float(min(max(mult - 1.0, 0.0), 0.5))

    def get_quick_environment_multiplier(self, mud_factor: float = 0.0, wind_drag: float = 0.0) -> float:
        """与 C GetQuickEnvironmentMultiplier 近似（孪生默认无泥地缓存）。"""
        mud_penalty_max = getattr(self.constants, 'ENV_MUD_PENALTY_MAX', 0.35)
        mult = 1.0 + mud_factor * mud_penalty_max
        if wind_drag > 0.0:
            mult = mult * (1.0 + wind_drag)
        heat_penalty = self.get_heat_stress_penalty()
        if heat_penalty > 0.05:
            mult = mult * (1.0 + heat_penalty * 0.5)
        return float(mult)


# 与游戏 Init*Defaults 一致、但不在 v4 Optuna SEARCH_SPACE 中的字段（创建孪生时合并）
GAME_ALIGNED_FIXED_PARAMS = {
    'sprint_stamina_drain_multiplier': 3.5,
    'load_metabolic_dampening': 0.70,
    'aerobic_efficiency_factor': 0.9,
    'anaerobic_efficiency_factor': 1.2,
    'fatigue_accumulation_coeff': 0.015,
    'fatigue_max_factor': 2.0,
    'encumbrance_speed_penalty_exponent': 1.5,
    'encumbrance_speed_penalty_max': 0.75,
    'load_recovery_penalty_exponent': 2.0,
    'marginal_decay_threshold': 0.8,
    'marginal_decay_coeff': 1.1,
    'min_recovery_stamina_threshold': 0.2,
    'min_recovery_rest_time_seconds': 3.0,
    'sprint_velocity_threshold': 5.5,
}


def merge_game_aligned_params(optuna_params: Dict) -> Dict:
    """Optuna 采样参数 + 游戏固定项，供 RSSDigitalTwin 与 C 预设对齐。"""
    merged = dict(GAME_ALIGNED_FIXED_PARAMS)
    merged.update(optuna_params)
    return merged


class MovementType:
    IDLE = 0
    WALK = 1
    RUN = 2
    SPRINT = 3


class Stance:
    STAND = 0
    CROUCH = 1
    PRONE = 2


# =============================================================================
# RSSDigitalTwin - 与 C 端消耗/恢复逻辑完全同步
# =============================================================================
class RSSDigitalTwin:
    def __init__(self, constants):
        self.constants = constants
        self.environment_factor = EnvironmentFactor(constants)
        self.reset()

    def reset(self):
        self.stamina = 1.0
        self.stamina_history = []
        self.speed_history = []
        self.time_history = []
        self.recovery_rate_history = []
        self.drain_rate_history = []
        self.current_time = 0.0
        self.last_speed = 0.0
        self.epoc_delay_start_time = -1.0
        self.is_in_epoc_delay = False
        self.speed_before_stop = 0.0
        self.rest_duration_minutes = 0.0
        self.exercise_duration_minutes = 0.0
        self.last_movement_time = 0.0
        self.max_history_length = 20000
        self._scenario_wind_drag = 0.0
        self._sprint_start_time = -1.0
        self._sprint_cooldown_until = -1.0
        self._measured_velocity_ms = 0.0
        self._applied_speed_limit_mult = 1.0
        self._applied_speed_limit_ms = -1.0
        self.environment_factor.heat_stress = 0.0
        self.environment_factor.cold_stress = 0.0
        self.environment_factor.cold_static_penalty = 0.0
        self.fatigue = TwinFatigueSystem()
        self.fatigue.initialize(0.0)
        self.v6_cp_state = V6CriticalPowerState(
            cp0=self._critical_power_watts(),
            sprint_power_cap=getattr(self.constants, 'SPRINT_POWER_CAP_WATTS',
                                     V6_SPRINT_POWER_CAP_WATTS_DEFAULT))

    def _critical_power_watts(self) -> float:
        cp0 = getattr(self.constants, 'CRITICAL_POWER_WATTS', 0.0)
        if cp0 > 1.0:
            return float(cp0)
        cp0 = getattr(self.constants, 'V6_CRITICAL_POWER_WATTS_DEFAULT', V6_CRITICAL_POWER_WATTS_DEFAULT)
        return float(cp0)

    def _apply_stamina_cap_clamp(self, stamina_before: float, new_stamina: float) -> float:
        max_cap = self.fatigue.get_max_stamina_cap()
        new_stamina = float(np.clip(new_stamina, 0.0, max_cap))
        if stamina_before > max_cap:
            return max_cap
        return new_stamina
    def _static_standing_cost(self, body_weight: float, load_weight: float) -> float:
        """与 C CalculateStaticStandingCost 完全一致。返回 %/s，负数表示恢复。"""
        body_weight = max(0.0, body_weight)
        load_weight = max(0.0, load_weight)
        if load_weight < 5.0:
            return -0.0025
        if load_weight < 15.0:
            return -0.001
        c1 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_1', 1.2)
        c2 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_2', 1.6)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 0.000015)
        base = c1 * body_weight
        load_ratio = load_weight / body_weight if body_weight > 0 else 0.0
        load_term = c2 * (body_weight + load_weight) * (load_ratio ** 2) if load_weight > 0 else 0.0
        rate = (base + load_term) * coeff
        return float(max(0.0, rate))

    # -------------------------------------------------------------------------
    # CalculatePandolfEnergyExpenditure - SCR_RealisticStaminaSystem.c 904-976
    # 坡度项与 C 端 3.15.12 一致：缓下坡放大省能，陡下坡叠加刹车惩罚（已替代 Santee）
    # -------------------------------------------------------------------------
    def _pandolf_expenditure(self, velocity: float, current_weight: float,
                            grade_percent: float, terrain_factor: float) -> float:
        """与 C 完全一致。返回 %/s。低速回退到静态负重站立消耗（不返回“恢复”）。"""
        velocity = max(0.0, velocity)
        current_weight = max(0.0, current_weight)
        if velocity < 0.1:
            bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
            load_weight = max(0.0, current_weight - bw)
            static_rate = self._static_standing_cost(bw, load_weight)
            return float(max(static_rate, 0.0))

        vb = getattr(self.constants, 'PANDOLF_BASE_COEFF', 2.7)
        vc = getattr(self.constants, 'PANDOLF_VELOCITY_COEFF', 3.2)
        v0 = getattr(self.constants, 'PANDOLF_VELOCITY_OFFSET', 0.7)
        gb = getattr(self.constants, 'PANDOLF_GRADE_BASE_COEFF', 0.23)
        gv = getattr(self.constants, 'PANDOLF_GRADE_VELOCITY_COEFF', 1.34)
        ref = getattr(self.constants, 'REFERENCE_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 0.000015)
        fitness_bonus = getattr(self.constants, 'FIXED_PANDOLF_FITNESS_BONUS', 0.80)

        vt = velocity - v0
        base_term = (vb * fitness_bonus) + (vc * vt * vt)
        g_dec = grade_percent * 0.01
        vsq = velocity * velocity
        grade_term = g_dec * (gb + gv * vsq)

        max_grade_term = base_term * 3.0
        grade_term = min(grade_term, max_grade_term)

        # 生理学修正（与 C 端 3.15.12 一致）：缓下坡更省能、陡下坡刹车耗能
        gentle_max = getattr(self.constants, 'GENTLE_DOWNHILL_GRADE_MAX', 12.0)
        gentle_mult = getattr(self.constants, 'GENTLE_DOWNHILL_SAVINGS_MULTIPLIER', 1.25)
        steep_thr = getattr(self.constants, 'STEEP_DOWNHILL_GRADE_THRESHOLD', 15.0)
        steep_frac = getattr(self.constants, 'STEEP_DOWNHILL_PENALTY_MAX_FRACTION', 0.5)
        if grade_percent < 0.0 and grade_percent > -gentle_max:
            grade_term = grade_term * gentle_mult
        steep_downhill_penalty = 0.0
        if grade_percent < -steep_thr:
            abs_grade = abs(grade_percent)
            ramp = (abs_grade - steep_thr) / 15.0
            if ramp > 1.0:
                ramp = 1.0
            steep_downhill_penalty = base_term * ramp * steep_frac

        terrain_factor = np.clip(terrain_factor, 0.5, 3.0)
        w_mult = max(current_weight / ref, 0.1)
        energy_expenditure = w_mult * (base_term + grade_term + steep_downhill_penalty) * terrain_factor * ref
        stamina_drain_rate = energy_expenditure * coeff
        return float(max(stamina_drain_rate, 0.0))

    # -------------------------------------------------------------------------
    # EncumbranceCache.GetStaminaDrainMultiplier - SCR_EncumbranceCache.c 100-104
    # effectiveWeight = currentWeight - BASE_WEIGHT; bodyMassPercent = effectiveWeight/CHARACTER_WEIGHT
    # mult = 1 + coeff * bodyMassPercent, clamp(1,3)
    # 注：digital twin 的 current_weight = body + equipment，故 effectiveWeight = equipment - BASE_WEIGHT
    # -------------------------------------------------------------------------
    def _encumbrance_stamina_drain_multiplier(self, current_weight: float) -> float:
        base = getattr(self.constants, 'BASE_WEIGHT', 1.36)
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENCUMBRANCE_STAMINA_DRAIN_COEFF', 2.0)
        eff = max(0.0, current_weight - bw - base)
        body_mass_percent = eff / bw if bw > 0 else 0.0
        mult = 1.0 + coeff * body_mass_percent
        return float(np.clip(mult, 1.0, 3.0))

    # -------------------------------------------------------------------------
    # EncumbranceCache (speed penalty) - SCR_EncumbranceCache.c (segmented nonlinear)
    # effectiveLoad = max(equipmentWeight - BASE_WEIGHT, 0)
    # ratio = effectiveLoad / CHARACTER_WEIGHT
    # penalty = segmented(ratio) * (coeff / 0.20), clamp(0, max_pen)
    # -------------------------------------------------------------------------
    def _encumbrance_speed_penalty_base(self, current_weight: float) -> float:
        base_w = getattr(self.constants, 'BASE_WEIGHT', 1.36)
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_COEFF', 0.20)
        max_pen = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)

        effective_load = max(0.0, current_weight - bw - base_w)
        ratio = 0.0
        if bw > 0.0:
            ratio = effective_load / bw
        ratio = float(np.clip(ratio, 0.0, 2.0))

        raw = 0.0
        if ratio <= 0.3:
            raw = 0.15 * ratio
        elif ratio <= 0.6:
            seg = ratio - 0.3
            raw = 0.045 + 0.35 * (seg ** 1.5)
        else:
            seg = ratio - 0.6
            raw = 0.25 + 0.65 * (seg * seg)

        raw = raw * (coeff / 0.20)
        return float(np.clip(raw, 0.0, max_pen))

    # -------------------------------------------------------------------------
    # StaminaConsumptionCalculator.CalculatePostureMultiplier
    # -------------------------------------------------------------------------
    def _posture_consumption_multiplier(self, speed: float, stance: int) -> float:
        if speed <= 0.05:
            return 1.0
        if stance == Stance.CROUCH:
            return getattr(self.constants, 'POSTURE_CROUCH_MULTIPLIER', 1.8)
        if stance == Stance.PRONE:
            return getattr(self.constants, 'POSTURE_PRONE_MULTIPLIER', 3.0)
        return 1.0

    # -------------------------------------------------------------------------
    # 代谢效率因子
    # -------------------------------------------------------------------------
    def _fitness_efficiency_factor(self) -> float:
        return getattr(self.constants, 'FIXED_FITNESS_EFFICIENCY_FACTOR', 0.70)

    def _metabolic_efficiency_factor(self, speed_ratio: float) -> float:
        aero = getattr(self.constants, 'AEROBIC_THRESHOLD', 0.6)
        anao = getattr(self.constants, 'ANAEROBIC_THRESHOLD', 0.8)
        fa = getattr(self.constants, 'AEROBIC_EFFICIENCY_FACTOR', 0.9)
        fn = getattr(self.constants, 'ANAEROBIC_EFFICIENCY_FACTOR', 1.2)
        if speed_ratio < aero:
            return fa
        if speed_ratio < anao:
            t = (speed_ratio - aero) / (anao - aero)
            return fa + t * (fn - fa)
        return fn

    def _fatigue_factor(self) -> float:
        start = getattr(self.constants, 'FATIGUE_START_TIME_MINUTES', 5.0)
        coeff = getattr(self.constants, 'FATIGUE_ACCUMULATION_COEFF', 0.015)
        mx = getattr(self.constants, 'FATIGUE_MAX_FACTOR', 2.0)
        eff = max(0.0, self.exercise_duration_minutes - start)
        return float(np.clip(1.0 + coeff * eff, 1.0, mx))

    def _movement_phase_from_type(self, movement_type: int) -> int:
        if movement_type == MovementType.SPRINT:
            return 3
        if movement_type == MovementType.WALK:
            return 1
        return 2

    def _v6_land_drain_per_second(
        self,
        speed: float,
        current_weight: float,
        grade_percent: float,
        terrain_factor: float,
        movement_type: int,
        wind_drag: float = 0.0,
        effective_cp_watts: float = -1.0,
    ) -> float:
        """v6 陆地代谢消耗 %/s（MetabolismModel，无 enc_mult；STA 仅扣 min(P, CP)）。"""
        phase = self._movement_phase_from_type(movement_type)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 7.17e-7)
        cal = getattr(self.constants, 'V6_STAMINA_DRAIN_CALIBRATION', V6_STAMINA_DRAIN_CALIBRATION)
        power_w = metabolism_power_watts(
            speed, current_weight, grade_percent, terrain_factor, phase)
        aerobic_w = power_w
        if effective_cp_watts > 1.0 and power_w > effective_cp_watts:
            aerobic_w = effective_cp_watts
        per_sec = max(aerobic_w * coeff * cal, 0.0)
        return per_sec * (1.0 + wind_drag)

    # -------------------------------------------------------------------------
    # StaminaUpdateCoordinator.CalculateLandBaseDrainRate + StaminaConsumptionCalculator
    # v6 陆地：MetabolismModel + posture/efficiency/fatigue；legacy Pandolf+enc_mult 已移除
    # -------------------------------------------------------------------------
    def _calculate_drain_rate_c_aligned(self, speed: float, current_weight: float,
                                        grade_percent: float, terrain_factor: float,
                                        stance: int, movement_type: int,
                                        wind_drag: float = 0.0,
                                        environment_factor=None) -> Tuple[float, float]:
        """
        返回 (base_for_recovery, total_drain)，单位均为每 0.2s。
        base_for_recovery 用于恢复计算中减去静态消耗。
        """
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        game_max = getattr(self.constants, 'GAME_MAX_SPEED', 5.5)
        rest_per_tick = getattr(self.constants, 'REST_RECOVERY_PER_TICK', 0.0005)
        idle_threshold = 0.1

        cp0 = getattr(self.constants, 'V6_CRITICAL_POWER_WATTS_DEFAULT', V6_CRITICAL_POWER_WATTS_DEFAULT)
        load_kg = max(0.0, current_weight - bw)
        effective_cp = compute_cp_watts(cp0, load_kg, grade_percent, 1.0, 0.0)

        wind_mult = 1.0 + wind_drag

        # 1. 基础消耗率 (C: StaminaUpdateCoordinator.CalculateLandBaseDrainRate)
        load_dampening = getattr(self.constants, 'LOAD_METABOLIC_DAMPENING', 0.70)

        if movement_type == MovementType.IDLE:
            if speed < idle_threshold:
                raw = -rest_per_tick
            else:
                pandolf_per_s = self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor)
                if current_weight > bw and load_dampening < 1.0:
                    unloaded_per_s = self._pandolf_expenditure(speed, bw, grade_percent, terrain_factor)
                    load_extra = pandolf_per_s - unloaded_per_s
                    pandolf_per_s = unloaded_per_s + load_extra * load_dampening
                raw = pandolf_per_s * wind_mult * STAMINA_TICK_SEC
        elif speed < idle_threshold:
            load_weight = max(0.0, current_weight - bw)
            static_per_s = self._static_standing_cost(bw, load_weight)
            raw = static_per_s * STAMINA_TICK_SEC
        else:
            # v6 陆地：MetabolismModel（与 CalculateLandBaseDrainRate 同形）
            pandolf_per_s = self._v6_land_drain_per_second(
                speed, current_weight, grade_percent, terrain_factor, movement_type, wind_drag,
                effective_cp_watts=effective_cp)
            raw = pandolf_per_s * STAMINA_TICK_SEC

        base_for_recovery = max(0.0, raw) if raw > 0 else 0.0

        # 2. 负数为恢复，传入 recovery_rate 计算而非作为负 drain 双重计入
        if raw <= 0.0:
            return (raw, 0.0)

        # 2.5 温度热调节补偿 (C: SCR_StaminaConsumption.c:128-132, 在 posture 之前)
        if environment_factor is not None and raw > 0.0:
            raw = environment_factor.adjust_energy_for_temperature(raw)
        base_for_recovery = max(0.0, raw)

        # 3. v6 陆地：仅姿态（与 C 快路径一致；efficiency/fatigue 陆地为 1.0）
        posture = self._posture_consumption_multiplier(speed, stance)
        total_drain = raw * posture

        # 4. 轻量环境乘数（与 C GetQuickEnvironmentMultiplier 单层对齐；勿在 step 再叠 heat_mult）
        if environment_factor is not None and total_drain > 0.0:
            total_drain = total_drain * environment_factor.get_quick_environment_multiplier(
                wind_drag=wind_drag)

        return (base_for_recovery, float(max(0.0, total_drain)))

    def _calculate_drain_rate_c_aligned_legacy_pandolf(self, speed: float, current_weight: float,
                                        grade_percent: float, terrain_factor: float,
                                        stance: int, movement_type: int,
                                        wind_drag: float = 0.0,
                                        environment_factor=None) -> Tuple[float, float]:
        """Legacy Pandolf+enc_mult 路径（仅测试对照，主循环已不用）。"""
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        game_max = getattr(self.constants, 'GAME_MAX_SPEED', 5.5)
        wind_mult = 1.0 + wind_drag
        load_dampening = getattr(self.constants, 'LOAD_METABOLIC_DAMPENING', 0.70)
        if speed < 0.1:
            return (0.0, 0.0)
        pandolf_per_s = self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor)
        if current_weight > bw and load_dampening < 1.0:
            unloaded_per_s = self._pandolf_expenditure(speed, bw, grade_percent, terrain_factor)
            load_extra = pandolf_per_s - unloaded_per_s
            pandolf_per_s = unloaded_per_s + load_extra * load_dampening
        raw = pandolf_per_s * wind_mult * 0.2
        posture = self._posture_consumption_multiplier(speed, stance)
        speed_ratio = np.clip(speed / game_max, 0.0, 1.0)
        total_eff = self._fitness_efficiency_factor() * self._metabolic_efficiency_factor(speed_ratio)
        fatigue = self._fatigue_factor()
        enc_mult = self._encumbrance_stamina_drain_multiplier(current_weight)
        total_drain = raw * posture * total_eff * fatigue * enc_mult
        return (raw, float(max(0.0, total_drain)))

    # -------------------------------------------------------------------------
    # CalculateMultiDimensionalRecoveryRate + StaminaRecoveryCalculator 逻辑
    # -------------------------------------------------------------------------
    def _calculate_recovery_rate(self, stamina_percent: float, rest_duration_minutes: float,
                                 exercise_duration_minutes: float, current_weight: float,
                                 base_drain_rate: float, disable_positive_recovery: bool,
                                 stance: int, environment_factor, current_speed: float,
                                 movement_type: int = MovementType.IDLE) -> float:
        c = self.constants
        stamina_percent_clamped = np.clip(stamina_percent, 0.0, 1.0)

        # 最低体力阈值限制 (C 456-458)
        rest_sec = rest_duration_minutes * 60.0
        if (stamina_percent_clamped < c.MIN_RECOVERY_STAMINA_THRESHOLD and
                rest_sec < c.MIN_RECOVERY_REST_TIME_SECONDS):
            return 0.0

        # 基础恢复率
        rec_nl = getattr(c, 'RECOVERY_NONLINEAR_COEFF', 0.5)
        stamina_mult = 1.0 + rec_nl * (1.0 - stamina_percent_clamped)
        recovery_rate = c.BASE_RECOVERY_RATE * stamina_mult
        recovery_rate *= c.FIXED_FITNESS_RECOVERY_MULTIPLIER * c.FIXED_AGE_RECOVERY_MULTIPLIER

        # 休息时间倍数（仅 rest>0 时应用阶段加成，避免运动中 rest=0 误触发快速恢复）
        if rest_duration_minutes > 0.0:
            if rest_duration_minutes <= c.FAST_RECOVERY_DURATION_MINUTES:
                recovery_rate *= c.FAST_RECOVERY_MULTIPLIER
            elif rest_duration_minutes <= c.FAST_RECOVERY_DURATION_MINUTES + c.MEDIUM_RECOVERY_DURATION_MINUTES:
                recovery_rate *= c.MEDIUM_RECOVERY_MULTIPLIER
            elif rest_duration_minutes >= c.SLOW_RECOVERY_START_MINUTES:
                trans_progress = min((rest_duration_minutes - c.SLOW_RECOVERY_START_MINUTES) / 10.0, 1.0)
                slow_mult = 1.0 - trans_progress * (1.0 - c.SLOW_RECOVERY_MULTIPLIER)
                recovery_rate *= slow_mult

        # 疲劳恢复
        fatigue_penalty = c.FATIGUE_RECOVERY_PENALTY * min(
            exercise_duration_minutes / c.FATIGUE_RECOVERY_DURATION_MINUTES, 1.0)
        fatigue_mult = np.clip(1.0 - fatigue_penalty, 0.7, 1.0)
        recovery_rate *= fatigue_mult

        # 姿态
        if stance == Stance.STAND:
            recovery_rate *= c.STANDING_RECOVERY_MULTIPLIER
        elif stance == Stance.CROUCH:
            recovery_rate *= c.CROUCHING_RECOVERY_MULTIPLIER
        elif stance == Stance.PRONE:
            recovery_rate *= c.PRONE_RECOVERY_MULTIPLIER

        # 边际效应衰减 (C: productBeforeLoad 包含 marginalDecayMultiplier)
        if stamina_percent_clamped > c.MARGINAL_DECAY_THRESHOLD:
            marginal_mult = np.clip(c.MARGINAL_DECAY_COEFF - stamina_percent_clamped, 0.2, 1.0)
            recovery_rate *= marginal_mult

        # 负重剥夺 (C: loadFactor = max(0, 1 - penalty/productBeforeLoad), total = product * loadFactor)
        weight_for_rec = c.CHARACTER_WEIGHT if stance == Stance.PRONE else current_weight
        if weight_for_rec > 0:
            load_ratio = np.clip(weight_for_rec / c.BODY_TOLERANCE_BASE, 0.0, 2.0)
            penalty = (load_ratio ** c.LOAD_RECOVERY_PENALTY_EXPONENT) * c.LOAD_RECOVERY_PENALTY_COEFF
            if recovery_rate > 0.0:
                load_factor = max(0.2, 1.0 - penalty / recovery_rate)  # 最低保留20%恢复率
                recovery_rate *= load_factor
            else:
                recovery_rate = 0.00005  # 确保恢复率不会完全归零

        # 热应激：恢复率惩罚 (C: recoveryRate *= (1.0 - heatStressPenalty))
        if environment_factor is not None:
            if hasattr(environment_factor, 'get_heat_stress_penalty'):
                heat_penalty = environment_factor.get_heat_stress_penalty()
            else:
                heat_penalty = getattr(environment_factor, 'heat_stress', 0.0)
            if heat_penalty > 0.0:
                recovery_rate *= (1.0 - heat_penalty)

        # 冷应激：恢复率惩罚 (C: ENV_TEMPERATURE_COLD_THRESHOLD 0°C 以下)
        if environment_factor is not None:
            cold = getattr(environment_factor, 'cold_stress', 0.0)
            recovery_rate *= max(0.0, 1.0 - cold)

        # 地表湿度惩罚：趴姿时 (C: SCR_StaminaRecovery.c:124-128)
        if stance == Stance.PRONE and environment_factor is not None:
            wetness = getattr(environment_factor, 'surface_wetness', 0.0)
            if wetness > 0.0:
                recovery_rate *= (1.0 - wetness)

        recovery_rate = max(recovery_rate, 0.0)

        # v6：陆地移动时不计入恢复（与 SCR_RSS_RecoveryCalculator 对齐）
        if current_speed >= 0.1:
            recovery_rate = 0.0

        if stamina_percent < 0.02:
            recovery_rate = max(recovery_rate, 0.0001)

        if disable_positive_recovery:
            return -max(base_drain_rate, 0.0)

        # 静态消耗处理 (C 198-215)
        if base_drain_rate > 0.0 and current_speed < 0.1:
            adjusted = recovery_rate - base_drain_rate
            recovery_rate = max(adjusted, 0.00005) if adjusted >= 0.00005 else 0.00005
        elif base_drain_rate < 0.0:
            recovery_rate += abs(base_drain_rate)

        if current_speed < 0.1 and current_weight < 40.0 and recovery_rate < 0.00005:
            recovery_rate = 0.0001

        max_cap = getattr(c, 'MAX_RECOVERY_PER_TICK', 0.0004)
        if max_cap > 0.0:
            recovery_rate = min(recovery_rate, max_cap)

        return float(max(recovery_rate, 0.0))

    # -------------------------------------------------------------------------
    # step - 单步仿真
    # -------------------------------------------------------------------------
    def step(self, speed: float, current_weight: float, grade_percent: float, terrain_factor: float,
             stance: int, movement_type: int, current_time: float, enable_randomness: bool = True,
             wind_drag: float = 0.0, time_delta_override: float = None):
        speed = max(0.0, float(speed))
        current_weight = max(0.0, float(current_weight))
        grade_percent = np.clip(float(grade_percent), -85.0, 85.0)
        terrain_factor = np.clip(float(terrain_factor), 0.5, 3.0)
        current_time = max(0.0, float(current_time))

        if time_delta_override is not None:
            time_delta = max(float(time_delta_override), 0.01)
        else:
            time_delta = current_time - self.current_time
            if time_delta <= 0.0:
                time_delta = STAMINA_TICK_SEC
            time_delta = max(time_delta, 0.01)
        self.current_time = current_time

        if enable_randomness:
            speed = max(speed * random.uniform(0.98, 1.02), 0.0)

        was_moving = self.last_speed > 0.05
        is_now_stopped = speed <= 0.05

        if was_moving and is_now_stopped and not self.is_in_epoc_delay:
            self.epoc_delay_start_time = current_time
            self.is_in_epoc_delay = True
            self.speed_before_stop = max(self.last_speed, 0.0)

        if self.is_in_epoc_delay:
            if current_time - self.epoc_delay_start_time >= self.constants.EPOC_DELAY_SECONDS:
                self.is_in_epoc_delay = False
                self.epoc_delay_start_time = -1.0

        if speed > 0.05:
            self.is_in_epoc_delay = False
            self.epoc_delay_start_time = -1.0

        # 战术冲刺爆发期追踪 (C: PlayerBase.c:984-991)
        is_sprinting = (movement_type == MovementType.SPRINT)
        if is_sprinting and self._sprint_start_time < 0.0:
            if self._sprint_cooldown_until < 0.0 or current_time >= self._sprint_cooldown_until:
                self._sprint_start_time = current_time
        elif not is_sprinting and self._sprint_start_time >= 0.0:
            burst_dur = getattr(self.constants, 'TACTICAL_SPRINT_BURST_DURATION', 8.0)
            buffer_dur = getattr(self.constants, 'TACTICAL_SPRINT_BURST_BUFFER_DURATION', 5.0)
            cooldown = getattr(self.constants, 'TACTICAL_SPRINT_COOLDOWN', 15.0)
            elapsed = current_time - self._sprint_start_time
            if elapsed >= burst_dur + buffer_dur:
                self._sprint_cooldown_until = current_time + cooldown
            self._sprint_start_time = -1.0

        self.last_speed = speed
        if len(self.speed_history) < self.max_history_length:
            self.speed_history.append(speed)

        if speed >= 0.1:
            self.exercise_duration_minutes += time_delta / 60.0
            self.rest_duration_minutes = 0.0
        else:
            self.rest_duration_minutes += time_delta / 60.0

        # 预置默认值，防止异常分支引用未初始化变量
        recovery_rate = 0.0
        total_drain = 0.0
        try:
            base_for_rec, total_drain = self._calculate_drain_rate_c_aligned(
                speed, current_weight, grade_percent, terrain_factor,
                stance, movement_type, wind_drag, self.environment_factor)

            total_drain = float(total_drain)
            if total_drain >= 0.0:
                total_drain = min(total_drain, 0.1)

            if self.is_in_epoc_delay:
                epoc_rate = self.constants.EPOC_DRAIN_RATE * (
                    1.0 + np.clip(self.speed_before_stop / 5.5, 0.0, 1.0) * 0.5)
                total_drain = total_drain + epoc_rate

            recovery_rate = self._calculate_recovery_rate(
                self.stamina, self.rest_duration_minutes, self.exercise_duration_minutes,
                current_weight, base_for_rec, False, stance, self.environment_factor, speed,
                movement_type=movement_type)

            recovery_rate = min(max(float(recovery_rate), 0.0), 0.01)

            # ── v6 CP–W′ Tick ──
            bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
            load_kg = max(current_weight - bw, 0.0)
            phase = movement_type
            if phase == MovementType.IDLE:
                phase = MovementType.WALK
            self.v6_cp_state.set_runtime_context(
                load_kg, grade_percent, 1.0,
                self.fatigue.get_fatigue_integral_norm())
            self.v6_cp_state.set_fatigue_cp_multiplier(
                self.fatigue.get_cp_fatigue_multiplier())
            metabolic_power = metabolism_power_watts(
                speed, current_weight, grade_percent, terrain_factor, phase)
            self.v6_cp_state.tick(
                metabolic_power, is_sprinting, current_time, time_delta, speed)
            # ──

            net_change = recovery_rate - total_drain
            net_change = np.clip(float(net_change), -0.1, 0.01)

            tick_scale = time_delta / STAMINA_TICK_SEC
            stamina_delta = float(net_change) * tick_scale
            stamina_before = self.stamina
            self.stamina = self._apply_stamina_cap_clamp(
                stamina_before, stamina_before + stamina_delta)

            self.base_drain_rate_by_velocity = base_for_rec
            self.final_drain_rate = total_drain
            self.recovery_rate = recovery_rate
            self.net_change = stamina_delta

            if len(self.stamina_history) < self.max_history_length:
                self.stamina_history.append(self.stamina)
                self.time_history.append(current_time)
                self.recovery_rate_history.append(recovery_rate)
                self.drain_rate_history.append(total_drain)
        except Exception as e:
            print(f"Warning: step exception: {e}")
            self.base_drain_rate_by_velocity = 0.0
            self.final_drain_rate = 0.01
            self.recovery_rate = 0.001
            fallback_drain = 0.001 * (time_delta / STAMINA_TICK_SEC)
            self.net_change = -fallback_drain
            stamina_before = self.stamina
            self.stamina = self._apply_stamina_cap_clamp(
                stamina_before, stamina_before - fallback_drain)
            if len(self.stamina_history) < self.max_history_length:
                self.stamina_history.append(self.stamina)
                self.time_history.append(current_time)
                self.recovery_rate_history.append(float(recovery_rate))
                self.drain_rate_history.append(float(total_drain))

    # -------------------------------------------------------------------------
    # 闭环仿真：体力→速度→消耗，与 C 端 UpdateSpeedBasedOnStamina 一致
    # -------------------------------------------------------------------------

    def calculate_speed_multiplier_by_stamina(self, stamina_percent: float) -> float:
        """与 C CalculateSpeedMultiplierByStamina 一致（平台期 = willpower_threshold）。"""
        stamina_percent = np.clip(float(stamina_percent), 0.0, 1.0)
        start = getattr(self.constants, 'WILLPOWER_THRESHOLD', 0.35)
        start = float(np.clip(start, 0.15, 0.5))
        end = getattr(self.constants, 'SMOOTH_TRANSITION_END', 0.05)
        target_mult = getattr(self.constants, 'TARGET_RUN_SPEED_MULTIPLIER', 3.8 / 5.5)
        min_limp = getattr(self.constants, 'MIN_LIMP_SPEED_MULTIPLIER', 1.0 / 5.5)
        if stamina_percent >= start:
            return target_mult
        if stamina_percent >= end:
            t = (stamina_percent - end) / (start - end)
            t = np.clip(t, 0.0, 1.0)
            smooth_t = t * t * (3.0 - 2.0 * t)
            return min_limp + (target_mult - min_limp) * smooth_t
        collapse = stamina_percent / end
        base = min_limp * collapse
        return max(base, min_limp * 0.8)

    def calculate_actual_speed(
        self,
        stamina_percent: float,
        current_weight: float,
        movement_phase: int,
        last_speed: float,
        grade_percent: float = 0.0,
        current_time: float = -1.0,
    ) -> float:
        """与 C CalculateFinalSpeedMultiplier + CalculateSlopeAdjustedTargetSpeed 一致，返回 m/s。"""
        c = self.constants
        game_max = getattr(c, 'GAME_MAX_SPEED', 5.5)
        max_pen = getattr(c, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
        sprint_boost = getattr(c, 'SPRINT_SPEED_BOOST', 0.30)
        exhausted = stamina_percent <= getattr(c, 'EXHAUSTION_THRESHOLD', 0.0)
        can_sprint = stamina_percent >= getattr(c, 'SPRINT_ENABLE_THRESHOLD', 0.15)

        phase = movement_phase
        if exhausted or not can_sprint:
            if phase == MovementType.SPRINT:
                phase = MovementType.RUN

        run_base = self.calculate_speed_multiplier_by_stamina(stamina_percent)
        angle_deg = math.degrees(math.atan(grade_percent / 100.0)) if abs(grade_percent) > 0.01 else 0.0
        slope_mult = tobler_speed_multiplier(
            angle_deg,
            uphill_boost=getattr(c, 'UPHILL_SPEED_BOOST', 1.15),
            downhill_boost=getattr(c, 'DOWNHILL_SPEED_BOOST', 1.15),
            downhill_max=getattr(c, 'DOWNHILL_SPEED_MAX_MULTIPLIER', 1.25),
            dampening=getattr(c, 'TOBLER_DAMPENING', 0.7),
        )
        scaled_run = run_base * slope_mult

        base_penalty = self._encumbrance_speed_penalty_base(current_weight)
        speed_ratio = np.clip(last_speed / game_max, 0.0, 1.0)
        enc_penalty = base_penalty * (1.0 + speed_ratio)
        if phase == MovementType.SPRINT:
            enc_penalty *= 1.5
        enc_penalty = np.clip(enc_penalty, 0.0, max_pen)

        # 战术冲刺爆发期：前 N 秒减轻负重惩罚，之后线性过渡 (C: SCR_SpeedCalculation.c:98-116)
        if phase == MovementType.SPRINT and self._sprint_start_time >= 0.0 and current_time >= 0.0:
            burst_dur = getattr(c, 'TACTICAL_SPRINT_BURST_DURATION', 8.0)
            buffer_dur = getattr(c, 'TACTICAL_SPRINT_BURST_BUFFER_DURATION', 5.0)
            burst_factor = getattr(c, 'TACTICAL_SPRINT_BURST_ENCUMBRANCE_FACTOR', 0.2)
            elapsed = current_time - self._sprint_start_time
            if burst_dur > 0.0 and elapsed <= burst_dur:
                enc_penalty = enc_penalty * burst_factor
            elif buffer_dur > 0.0 and elapsed > burst_dur and elapsed <= burst_dur + buffer_dur:
                t = float(np.clip((elapsed - burst_dur) / buffer_dur, 0.0, 1.0))
                blend = burst_factor + (1.0 - burst_factor) * t
                enc_penalty = enc_penalty * blend

        if phase == MovementType.SPRINT:
            mult = (scaled_run * (1.0 + sprint_boost)) * (1.0 - enc_penalty)
            mult = np.clip(mult, 0.15, 1.0)
        elif phase == MovementType.RUN:
            mult = scaled_run * (1.0 - enc_penalty)
            mult = np.clip(mult, 0.15, 1.0)
        elif phase == MovementType.WALK:
            walk_base = self.calculate_speed_multiplier_by_stamina(stamina_percent)
            mult = (walk_base * 0.8) * (1.0 - enc_penalty)
            mult = np.clip(mult, 0.2, 0.9)
        else:
            return 0.0

        if phase in (MovementType.WALK, MovementType.RUN, MovementType.SPRINT) and last_speed < 0.5:
            mult = max(mult, 0.5)

        return float(game_max * mult)

    @staticmethod
    def estimate_engine_original_max_speed(movement_phase: int, constants) -> float:
        """与 PlayerBase.GetDynamicOriginalEngineMaxSpeed 无 AnimComponent 回退一致。"""
        game_max = getattr(constants, 'GAME_MAX_SPEED', 5.5)
        run_mult = getattr(constants, 'TARGET_RUN_SPEED_MULTIPLIER', 3.8 / 5.5)
        if movement_phase == MovementType.SPRINT:
            return float(game_max)
        return float(game_max * run_mult)

    def get_drain_velocity_ms(
        self,
        movement_phase: int,
        is_sprinting: bool,
        can_sprint: bool,
        engine_original_ms: float,
        theoretical_limited_ms: float,
    ) -> float:
        """v6：消耗测速 = min(v_meas, v_limit)；与 CalculateLandBaseDrainRate 一致。"""
        if movement_phase == MovementType.IDLE:
            return 0.0

        measured = max(0.0, self._measured_velocity_ms)
        if measured < 0.05:
            measured = max(0.0, theoretical_limited_ms)

        cap_ms = self._applied_speed_limit_ms
        if cap_ms <= 0.05:
            cap_ms = theoretical_limited_ms
        if cap_ms <= 0.05:
            cap_ms = engine_original_ms

        return get_drain_velocity_ms(measured, cap_ms)

    def get_metabolic_accounting_velocity_ms(
        self,
        movement_phase: int,
        is_sprinting: bool,
        can_sprint: bool,
        engine_original_ms: float,
        theoretical_limited_ms: float,
    ) -> float:
        """超限速时按 v_meas 记账；否则同 get_drain_velocity_ms。"""
        if movement_phase == MovementType.IDLE:
            return 0.0

        measured = max(0.0, self._measured_velocity_ms)
        if measured < 0.05:
            measured = max(0.0, theoretical_limited_ms)

        cap_ms = self._applied_speed_limit_ms
        if cap_ms <= 0.05:
            cap_ms = theoretical_limited_ms
        if cap_ms <= 0.05:
            cap_ms = engine_original_ms

        return get_metabolic_accounting_velocity_ms(measured, cap_ms)

    def _resolve_current_speed_ms(self, theoretical_ms: float) -> float:
        measured = max(0.0, self._measured_velocity_ms)
        if measured < 0.05:
            return max(0.0, theoretical_ms)
        return measured

    def _apply_metabolic_speed_limit(
        self,
        speed_limit_mult: float,
        current_speed_ms: float,
        movement_phase: int,
        current_weight: float,
        grade_percent: float,
        terrain_factor: float,
        engine_base_ms: float,
    ) -> float:
        load_kg = max(current_weight - getattr(self.constants, 'CHARACTER_WEIGHT', 90.0), 0.0)
        # 更新 v6 CP 状态上下文
        self.v6_cp_state.set_runtime_context(
            load_kg, grade_percent, 1.0,
            self.fatigue.get_fatigue_integral_norm())
        self.v6_cp_state.set_fatigue_cp_multiplier(
            self.fatigue.get_cp_fatigue_multiplier())
        effective_cp = self.v6_cp_state.get_effective_critical_power_watts()

        exhausted = self.stamina <= getattr(self.constants, 'EXHAUSTION_THRESHOLD', 0.0)
        metab_phase = movement_phase
        if metab_phase < MovementType.WALK:
            metab_phase = MovementType.RUN

        return get_metabolic_corrected_speed_multiplier(
            speed_limit_mult,
            current_speed_ms,
            metab_phase,
            current_weight,
            grade_percent,
            terrain_factor,
            exhausted,
            engine_base_ms,
            effective_cp,
        )

    def compute_speed_limit_multiplier(
        self, theoretical_target_ms: float, engine_original_ms: float
    ) -> float:
        """与 StaminaUpdateCoordinator.UpdateSpeed：limit = theoretical / engineOriginal。"""
        c = self.constants
        game_max = getattr(c, 'GAME_MAX_SPEED', 5.5)
        if engine_original_ms > 0.1:
            mult = theoretical_target_ms / engine_original_ms
        else:
            mult = theoretical_target_ms / game_max
        return float(np.clip(mult, 0.01, 3.0))

    def resolve_movement_state(
        self,
        intent_phase: int,
        stamina_percent: float,
    ):
        """解析冲刺意图、有效阶段与 can_sprint（与 UpdateSpeed / PlayerBase 一致）。"""
        c = self.constants
        exhausted = stamina_percent <= getattr(c, 'EXHAUSTION_THRESHOLD', 0.0)
        can_sprint = stamina_percent >= getattr(c, 'SPRINT_ENABLE_THRESHOLD', 0.25)
        is_sprinting = intent_phase == MovementType.SPRINT
        effective_phase = intent_phase
        if exhausted or not can_sprint:
            if is_sprinting or effective_phase == MovementType.SPRINT:
                effective_phase = MovementType.RUN
                is_sprinting = False
        return is_sprinting, effective_phase, can_sprint, exhausted

    def game_player_tick(
        self,
        intent_phase: int,
        current_weight: float,
        grade_percent: float,
        terrain_factor: float,
        stance: int,
        current_time: float,
        time_delta: float,
        wind_drag: float = 0.0,
        enable_randomness: bool = False,
    ) -> float:
        """
        单帧 PlayerBase 同序仿真：
        1) GetVelocity -> drain_speed
        2) UpdateSpeed -> 理论限速
        3) 消耗/恢复（step 内核）
        4) 更新 _measured_velocity_ms 供下一帧
        返回本帧用于消耗的测速 (m/s)。
        """
        is_sprinting, effective_phase, can_sprint, _ = self.resolve_movement_state(
            intent_phase, self.stamina
        )

        if intent_phase == MovementType.SPRINT and self._sprint_start_time < 0.0:
            if self._sprint_cooldown_until < 0.0 or current_time >= self._sprint_cooldown_until:
                self._sprint_start_time = current_time

        theoretical_ms = self.calculate_actual_speed(
            self.stamina,
            current_weight,
            effective_phase,
            self._measured_velocity_ms,
            grade_percent=grade_percent,
            current_time=current_time,
        )
        if effective_phase == MovementType.SPRINT:
            engine_phase = MovementType.SPRINT
        elif effective_phase == MovementType.RUN:
            engine_phase = MovementType.RUN
        else:
            engine_phase = effective_phase
        engine_original_ms = self.estimate_engine_original_max_speed(
            engine_phase, self.constants
        )
        speed_limit_mult = self.compute_speed_limit_multiplier(
            theoretical_ms, engine_original_ms
        )

        current_speed_ms = self._resolve_current_speed_ms(theoretical_ms)
        speed_limit_mult = self._apply_metabolic_speed_limit(
            speed_limit_mult,
            current_speed_ms,
            effective_phase,
            current_weight,
            grade_percent,
            terrain_factor,
            engine_original_ms,
        )

        applied_limit_ms = speed_limit_mult * engine_original_ms
        if applied_limit_ms <= 0.05:
            self._applied_speed_limit_ms = -1.0
        else:
            self._applied_speed_limit_ms = applied_limit_ms

        limit_for_power_ms = self._applied_speed_limit_ms
        if limit_for_power_ms <= 0.05:
            limit_for_power_ms = applied_limit_ms

        if intent_phase == MovementType.IDLE or theoretical_ms < 0.1:
            drain_speed = 0.0
            engine_phase = MovementType.IDLE
        else:
            measured_for_drain = max(0.0, self._measured_velocity_ms)
            if measured_for_drain < 0.05:
                measured_for_drain = max(0.0, theoretical_ms)
            cap_ms = limit_for_power_ms
            if cap_ms <= 0.05:
                cap_ms = theoretical_ms
            if cap_ms <= 0.05:
                cap_ms = engine_original_ms
            drain_speed = get_drain_velocity_ms(measured_for_drain, cap_ms)

        engine_movement_phase = self._compute_engine_movement_phase(
            effective_phase,
            drain_speed,
            self._measured_velocity_ms,
            current_weight,
            enable_randomness,
        )

        self.fatigue.process_decay(current_time, current_speed_ms)
        if current_speed_ms >= RSS_IDLE_SPEED_THRESHOLD_MPS:
            load_kg = max(current_weight - getattr(self.constants, 'CHARACTER_WEIGHT', 90.0), 0.0)
            mov_phase = self._movement_phase_from_type(engine_movement_phase)
            fatigue_v = get_drain_velocity_ms(current_speed_ms, limit_for_power_ms)
            power_fat = metabolism_power_watts(
                fatigue_v,
                current_weight,
                grade_percent,
                terrain_factor,
                mov_phase,
            )
            self.fatigue.process_integral(
                power_fat,
                load_kg,
                grade_percent,
                terrain_factor,
                time_delta,
                current_speed_ms,
                self._critical_power_watts(),
            )

        self.step(
            drain_speed,
            current_weight,
            grade_percent,
            terrain_factor,
            stance,
            engine_movement_phase,
            current_time,
            enable_randomness=enable_randomness,
            wind_drag=wind_drag,
            time_delta_override=time_delta,
        )

        if drain_speed >= 0.1:
            new_meas = min(
                VELOCITY_HORIZ_CAP_MS, engine_original_ms * speed_limit_mult
            )
            # 闭环孪生：测速跟随 RSS 限速（避免 limit 抖动造成伪超速）
            if self._applied_speed_limit_ms > 0.05:
                new_meas = min(new_meas, self._applied_speed_limit_ms)
            self._measured_velocity_ms = new_meas
        else:
            self._measured_velocity_ms = 0.0
        self._applied_speed_limit_mult = speed_limit_mult
        return drain_speed

    def _compute_engine_movement_phase(
        self,
        intent_phase: int,
        actual_speed: float,
        last_speed: float,
        current_weight: float,
        enable_randomness: bool,
    ) -> int:
        """近似引擎判定的移动类型（可能与输入意图不一致）。"""
        phase = int(intent_phase)
        if phase == MovementType.IDLE:
            return MovementType.IDLE

        # 可选：低速/重载时偶发误判为 idle（用于对齐线上观察到的状态抖动）。
        if enable_randomness:
            bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
            base_w = getattr(self.constants, 'BASE_WEIGHT', 1.36)
            effective_load = max(0.0, current_weight - bw - base_w)
            ratio = 0.0
            if bw > 0.0:
                ratio = effective_load / bw

            if actual_speed < 0.25 and last_speed < 0.25 and ratio > 0.30:
                # ratio>30% 且速度很低时，给一个小概率误判。
                # 概率随负重增加而上升，上限 20%。
                p = (ratio - 0.30) / 0.70
                p = float(np.clip(p, 0.0, 0.20))
                if random.random() < p:
                    return MovementType.IDLE

        return phase

    def simulate_closed_loop(
        self,
        duration_s: float,
        load_kg: float,
        movement_intent: int,
        grade_percent: float = 0.0,
        terrain_factor: float = 1.0,
        enable_randomness: bool = False,
        player_tick: bool = True,
    ) -> Dict:
        """闭环仿真：默认使用 PlayerBase 同序 game_player_tick。"""
        self.reset()
        current_weight = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0) + load_kg
        stance = Stance.STAND
        dt = RSS_PLAYER_TICK_SEC if player_tick else STAMINA_TICK_SEC
        current_time = 0.0

        steps = int(duration_s / dt)
        for _ in range(steps):
            if player_tick:
                self.game_player_tick(
                    int(movement_intent),
                    current_weight,
                    grade_percent,
                    terrain_factor,
                    stance,
                    current_time,
                    dt,
                    enable_randomness=enable_randomness,
                )
            else:
                intent_phase = int(movement_intent)
                actual_speed = self.calculate_actual_speed(
                    self.stamina, current_weight, intent_phase, self.last_speed,
                    grade_percent=grade_percent,
                    current_time=current_time,
                )
                _, effective_phase, can_sprint, exhausted = self.resolve_movement_state(
                    intent_phase, self.stamina
                )
                if (exhausted or not can_sprint) and intent_phase == MovementType.SPRINT:
                    effective_phase = MovementType.RUN
                engine_phase = self._compute_engine_movement_phase(
                    effective_phase,
                    actual_speed,
                    self.last_speed,
                    current_weight,
                    enable_randomness,
                )
                self.step(
                    actual_speed,
                    current_weight,
                    grade_percent,
                    terrain_factor,
                    stance,
                    engine_phase,
                    current_time,
                    enable_randomness,
                )
            current_time += dt

        return {
            'stamina_history': list(self.stamina_history),
            'time_history': list(self.time_history),
            'speed_history': list(self.speed_history),
        }

    def simulate_scenario(self, speed_profile: List, current_weight: float,
                          grade_percent: float, terrain_factor: float, stance: int,
                          movement_type: int, enable_randomness: bool = True,
                          temperature_celsius: float = None, wind_speed_mps: float = None,
                          fast_mode: bool = False) -> Dict:
        self.reset()
        c = self.constants
        heat_threshold = 30.0
        cold_threshold = 5.0  # 与 C ENV_TEMPERATURE_COLD_THRESHOLD=5.0 一致（北约STANAG）
        if temperature_celsius is not None:
            self.environment_factor.temperature = float(temperature_celsius)
            if temperature_celsius > heat_threshold:
                coeff = getattr(c, 'ENV_TEMPERATURE_HEAT_PENALTY_COEFF', 0.02)
                max_add = getattr(c, 'ENV_HEAT_STRESS_MAX_MULTIPLIER', 1.5) - 1.0
                self.environment_factor.heat_stress = min(
                    (temperature_celsius - heat_threshold) * coeff, max_add)
            else:
                self.environment_factor.heat_stress = 0.0
            if temperature_celsius < cold_threshold:
                cold_coeff = getattr(c, 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF', 0.05)
                self.environment_factor.cold_stress = (
                    cold_threshold - temperature_celsius) * cold_coeff
                cold_static = getattr(c, 'ENV_TEMPERATURE_COLD_STATIC_PENALTY', 0.03)
                self.environment_factor.cold_static_penalty = (
                    cold_threshold - temperature_celsius) * cold_static
            else:
                self.environment_factor.cold_stress = 0.0
                self.environment_factor.cold_static_penalty = 0.0
        if wind_speed_mps is not None:
            self.environment_factor.wind_speed = float(wind_speed_mps)
            if wind_speed_mps >= 1.0:
                wind_coeff = getattr(c, 'ENV_WIND_RESISTANCE_COEFF', 0.05)
                self._scenario_wind_drag = min(1.0, wind_speed_mps * wind_coeff)
            else:
                self._scenario_wind_drag = 0.0
        else:
            self._scenario_wind_drag = 0.0

        current_time = 0.0
        total_distance = 0.0
        nominal_distance = 0.0
        game_max = getattr(self.constants, 'GAME_MAX_SPEED', 5.5)
        max_pen = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)

        # 加速模式：使用更大的时间步长（0.4s vs 0.2s），速度提升约2倍
        dt = 0.4 if fast_mode else 0.2

        # 检查 speed_profile 格式
        if speed_profile and len(speed_profile[0]) > 2:
            # 战斗周期场景格式：(duration, speed, movement_type, stance, name, grade_percent)
            for phase in speed_profile:
                duration, speed, phase_movement_type, phase_stance, _, phase_grade = phase
                steps = int(duration / dt)
                posture_speed_mult = {Stance.STAND: 1.0, Stance.CROUCH: 0.7, Stance.PRONE: 0.3}.get(phase_stance, 1.0)

                for _ in range(steps):
                    wind_drag = getattr(self, '_scenario_wind_drag', 0.0)
                    self.step(speed, current_weight, phase_grade, terrain_factor,
                             phase_stance, phase_movement_type, current_time, enable_randomness, wind_drag)
                    current_time += dt
                    nominal_distance += speed * dt

                    base_pen = self._encumbrance_speed_penalty_base(current_weight)
                    speed_ratio = float(np.clip(speed / game_max, 0.0, 1.0))
                    speed_penalty = base_pen * (1.0 + speed_ratio)
                    if phase_movement_type == MovementType.SPRINT:
                        speed_penalty = speed_penalty * 1.5
                    speed_penalty = float(np.clip(speed_penalty, 0.0, max_pen))
                    effective_speed = speed * posture_speed_mult * (1.0 - speed_penalty)
                    total_distance += effective_speed * dt
        else:
            # 标准格式：(time, speed)
            time_points = [t for t, _ in speed_profile]
            speeds = [v for _, v in speed_profile]
            posture_speed_mult = {Stance.STAND: 1.0, Stance.CROUCH: 0.7, Stance.PRONE: 0.3}.get(stance, 1.0)

            for i in range(len(time_points) - 1):
                start_time = time_points[i]
                end_time = time_points[i + 1]
                speed = speeds[i]
                duration = end_time - start_time
                steps = int(duration / dt)

                for _ in range(steps):
                    wind_drag = getattr(self, '_scenario_wind_drag', 0.0)
                    self.step(speed, current_weight, grade_percent, terrain_factor,
                             stance, movement_type, current_time, enable_randomness, wind_drag)
                    current_time += dt
                    nominal_distance += speed * dt

                    base_pen = self._encumbrance_speed_penalty_base(current_weight)
                    speed_ratio = float(np.clip(speed / game_max, 0.0, 1.0))
                    speed_penalty = base_pen * (1.0 + speed_ratio)
                    if movement_type == MovementType.SPRINT:
                        speed_penalty = speed_penalty * 1.5
                    speed_penalty = float(np.clip(speed_penalty, 0.0, max_pen))
                    effective_speed = speed * posture_speed_mult * (1.0 - speed_penalty)
                    total_distance += effective_speed * dt

        min_stamina = min(self.stamina_history) if self.stamina_history else 1.0
        ratio = nominal_distance / max(total_distance, 1e-6) if nominal_distance > 0 else 1.0
        ratio = np.clip(ratio, 1.0, 5.0)
        total_time_with_penalty = current_time * ratio

        return {
            'total_distance': total_distance,
            'min_stamina': min_stamina,
            'total_time_with_penalty': total_time_with_penalty,
            'stamina_history': self.stamina_history,
            'speed_history': self.speed_history,
            'time_history': self.time_history,
            'recovery_rate_history': self.recovery_rate_history,
            'drain_rate_history': self.drain_rate_history
        }


# =============================================================================
# v5 双池 stub（与 SCR_RSS_DrainCalculator / SCR_RSS_AnaerobicBurst C 端契约）
# =============================================================================

def get_drain_velocity_ms(measured_ms: float, theoretical_max_ms: float) -> float:
    """与 SCR_RSS_DrainCalculator.GetDrainVelocityMs 一致。"""
    measured_ms = max(0.0, measured_ms)
    theoretical_max_ms = max(0.05, theoretical_max_ms)
    if measured_ms > theoretical_max_ms:
        return theoretical_max_ms
    return measured_ms


V6_OVERSPEED_ACCOUNTING_EPS_MPS = 0.12
V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT = 0.2


def is_wprime_pool_available_for_overspeed(
    w_prime_pool01: float,
    threshold: float = V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT,
) -> bool:
    return w_prime_pool01 > threshold


def get_metabolic_accounting_velocity_ms(
    measured_ms: float,
    applied_limit_ms: float,
    w_prime_pool01: float = 1.0,
) -> float:
    """与 SCR_RSS_DrainCalculator.GetMetabolicAccountingVelocityMs 一致。"""
    measured_ms = max(0.0, measured_ms)
    if applied_limit_ms > 0.05 and measured_ms > applied_limit_ms + V6_OVERSPEED_ACCOUNTING_EPS_MPS:
        if not is_wprime_pool_available_for_overspeed(w_prime_pool01):
            return get_drain_velocity_ms(measured_ms, applied_limit_ms)
        return measured_ms
    return get_drain_velocity_ms(measured_ms, applied_limit_ms)


def get_metabolic_accounting_power_watts(
    measured_ms: float,
    applied_limit_ms: float,
    total_weight_kg: float,
    grade_percent: float,
    terrain_factor: float,
    movement_phase: int,
    w_prime_pool01: float = 1.0,
) -> float:
    v_acct = get_metabolic_accounting_velocity_ms(measured_ms, applied_limit_ms, w_prime_pool01)
    return metabolism_power_watts(
        v_acct, total_weight_kg, grade_percent, terrain_factor, movement_phase
    )


def get_client_overspeed_excess_drain_per_second(
    measured_ms: float,
    applied_limit_ms: float,
    w_prime_pool01: float,
    total_weight_kg: float,
    grade_percent: float,
    terrain_factor: float,
    movement_phase: int,
    effective_critical_power_watts: float = -1.0,
) -> float:
    if not is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms):
        return 0.0
    if is_wprime_pool_available_for_overspeed(w_prime_pool01):
        return 0.0
    p_meas = metabolism_power_watts(
        measured_ms, total_weight_kg, grade_percent, terrain_factor, movement_phase
    )
    v_drain = get_drain_velocity_ms(measured_ms, applied_limit_ms)
    p_drain = metabolism_power_watts(
        v_drain, total_weight_kg, grade_percent, terrain_factor, movement_phase
    )
    excess_w = p_meas - p_drain
    if excess_w <= 1.0:
        return 0.0
    return stamina_drain_rate_per_second_from_power_watts(excess_w, -1.0)


def stamina_drain_rate_per_second_from_power_watts(
    power_watts: float,
    critical_power_cap_watts: float = -1.0,
    energy_to_stamina_coeff: float = 1.55e-07,
) -> float:
    aerobic_w = power_watts
    if critical_power_cap_watts > 1.0 and power_watts > critical_power_cap_watts:
        aerobic_w = critical_power_cap_watts
    coeff = max(0.0, min(0.1, energy_to_stamina_coeff)) * V6_STAMINA_DRAIN_CALIBRATION
    return max(aerobic_w * coeff, 0.0)


def is_metabolic_overspeed_accounting(measured_ms: float, applied_limit_ms: float) -> bool:
    if applied_limit_ms <= 0.05:
        return False
    return measured_ms > applied_limit_ms + V6_OVERSPEED_ACCOUNTING_EPS_MPS


def get_metabolic_overspeed_factor(
    pandolf_watts: float,
    sustainable_watts: float = 400.0,
    min_factor: float = 0.35,
) -> float:
    """与 SCR_RSS_DrainCalculator.GetMetabolicOverspeedFactor 一致（v5 过渡）。"""
    if pandolf_watts <= sustainable_watts:
        return 1.0
    ratio = sustainable_watts / pandolf_watts
    return max(min_factor, ratio)


def get_metabolic_speed_cap_ms(
    current_speed_ms: float,
    movement_phase: int,
    total_weight_kg: float,
    grade_percent: float,
    terrain_factor: float,
    is_exhausted: bool,
    effective_cp_watts: float,
) -> float:
    """与 SCR_RSS_DrainCalculator.GetMetabolicSpeedCapMs 同形（孪生不含 W' 瞬时加成）。"""
    if is_exhausted:
        return -1.0
    if movement_phase < 1:
        return -1.0

    power_w = metabolism_power_watts(
        max(0.0, current_speed_ms),
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    )
    available_p = effective_cp_watts
    if power_w <= available_p + 1.0:
        return -1.0

    target_p = effective_cp_watts
    if power_w > effective_cp_watts and movement_phase != MovementType.SPRINT:
        target_p = effective_cp_watts

    return invert_speed_for_power_watts(
        target_p,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        movement_phase,
    )


def get_metabolic_corrected_speed_multiplier(
    applied_speed_multiplier: float,
    current_speed_ms: float,
    movement_phase: int,
    total_weight_kg: float,
    grade_percent: float,
    terrain_factor: float,
    is_exhausted: bool,
    engine_base_ms: float,
    effective_cp_watts: float,
) -> float:
    """与 SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier 同形。"""
    cap_ms = get_metabolic_speed_cap_ms(
        current_speed_ms,
        movement_phase,
        total_weight_kg,
        grade_percent,
        terrain_factor,
        is_exhausted,
        effective_cp_watts,
    )
    if cap_ms < 0.0:
        return applied_speed_multiplier
    if engine_base_ms <= 0.05:
        engine_base_ms = 5.5

    applied_ms = applied_speed_multiplier * engine_base_ms
    if applied_ms <= cap_ms + 0.01:
        return applied_speed_multiplier
    return float(np.clip(cap_ms / engine_base_ms, 0.01, 3.0))


@dataclass
class V5AnaerobicState:
    """v5 无氧池简化孪生（不写入有氧 stamina 条）。"""

    pool: float = 1.0
    cooldown_until_sec: float = -1.0

    def tick_sprint(self, dt_sec: float, drain_per_sec: float) -> None:
        self.pool = max(0.0, self.pool - drain_per_sec * dt_sec)

    def cooldown_remaining(self, world_time_sec: float) -> float:
        if self.cooldown_until_sec < 0.0:
            return 0.0
        return max(0.0, self.cooldown_until_sec - world_time_sec)


# =============================================================================
# v6 CP–W′ 孪生（与 SCR_RSS_MetabolismModel / CriticalPowerModel 同形）
# =============================================================================

V6_ACSM_REST_W = 50.0
V6_ACSM_LINEAR_W_PER_MS = 200.0
V6_ACSM_QUAD_W_PER_MS2 = 80.0
V6_ACSM_BLEND_START_MS = 2.0
V6_ACSM_BLEND_END_MS = 2.4
V6_INVERT_SPEED_MAX_MS = 6.0
V6_CRITICAL_POWER_WATTS_DEFAULT = 400.0
V6_W_PRIME_MAX_JOULES_DEFAULT = 20000.0
V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT = 12.0
V6_SPRINT_POWER_CAP_WATTS_DEFAULT = 1200.0
V6_STAMINA_DRAIN_CALIBRATION = 0.72
V6_CP_LOAD_REF_KG = 10.0
V6_CP_LOAD_DECAY_PER_KG = 0.002
V6_CP_SLOPE_K_UP = 0.015
V6_CP_FATIGUE_K = 0.18
V6_SKIBA_ELITE_CP_THRESHOLD_W = 410.0
V6_W_PRIME_K_FAST = 0.15
V6_W_PRIME_K_SLOW = 0.008
V6_W_PRIME_LIM_RATIO = 0.5

V6_FATIGUE_I_MAX = 1.0
V6_FATIGUE_K_RECOVERY = 0.0008
V6_FATIGUE_K_LOAD = 0.15
V6_FATIGUE_K_SLOPE = 8.0
V6_FATIGUE_K_TERRAIN = 0.25
V6_MAX_FATIGUE_PENALTY = 0.3
RSS_IDLE_SPEED_THRESHOLD_MPS = 0.1
V6_CP_ENV_FLOOR = 0.55
V6_STANDING_REST_WATTS = 100.0
V5_TACTICAL_SHORT_BURST_SEC = 3.0
V5_BURST_EARLY_RELEASE_BONUS = 0.45
V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT = 0.20
V5_BURST_COOLDOWN_FULL_DEFAULT = 180.0
V5_BURST_COOLDOWN_SHORT_DEFAULT = 75.0


class TwinFatigueSystem:
    """与 SCR_RSS_FatigueSystem 同形（v6 积分疲劳 + 上限 cap）。"""

    def __init__(self):
        self.fatigue_accumulation = 0.0
        self.fatigue_integral = 0.0
        self.last_fatigue_decay_time = 0.0
        self.last_rest_start_time = -1.0
        self.fatigue_decay_rate = 0.0005
        self.fatigue_decay_min_rest_time = 15.0

    def initialize(self, current_time: float) -> None:
        self.fatigue_accumulation = 0.0
        self.fatigue_integral = 0.0
        self.last_fatigue_decay_time = current_time
        self.last_rest_start_time = -1.0

    def get_fatigue_integral_norm(self) -> float:
        if V6_FATIGUE_I_MAX <= 0.0:
            return 0.0
        return float(np.clip(self.fatigue_integral / V6_FATIGUE_I_MAX, 0.0, 1.0))

    def get_max_stamina_cap(self) -> float:
        return 1.0 - self.fatigue_accumulation

    def get_cp_fatigue_multiplier(self) -> float:
        """与 SCR_RSS_FatigueSystem.GetCpFatigueMultiplier 同形。"""
        norm = self.get_fatigue_integral_norm()
        mult = 1.0 - V6_CP_FATIGUE_K * norm
        return max(mult, 0.82)

    def process_integral(
        self,
        power_watts: float,
        load_kg: float,
        grade_percent: float,
        terrain_factor: float,
        time_delta_sec: float,
        current_speed_ms: float,
        critical_power_watts: float,
    ) -> None:
        if time_delta_sec <= 0.0:
            return

        body_w = 90.0
        load_ratio = 0.0
        if body_w > 0.0:
            load_ratio = load_kg / body_w

        g = grade_percent * 0.01
        w = (
            1.0
            + V6_FATIGUE_K_LOAD * load_ratio
            + V6_FATIGUE_K_SLOPE * g * g
            + V6_FATIGUE_K_TERRAIN * (terrain_factor - 1.0)
        )
        if w < 0.5:
            w = 0.5

        i_norm = self.get_fatigue_integral_norm()
        r = 0.0
        if current_speed_ms < 0.05 or power_watts < critical_power_watts * 0.5:
            one_minus = 1.0 - i_norm
            r = V6_FATIGUE_K_RECOVERY * one_minus * one_minus * power_watts

        d_i = (w * power_watts - r) * time_delta_sec * 0.0001
        self.fatigue_integral = float(np.clip(
            self.fatigue_integral + d_i, 0.0, V6_FATIGUE_I_MAX))

        legacy_from_i = self.fatigue_integral * V6_MAX_FATIGUE_PENALTY
        if legacy_from_i > self.fatigue_accumulation:
            self.fatigue_accumulation = legacy_from_i
        if self.fatigue_accumulation > V6_MAX_FATIGUE_PENALTY:
            self.fatigue_accumulation = V6_MAX_FATIGUE_PENALTY

    def process_decay(self, current_time: float, current_speed: float) -> None:
        is_moving = current_speed >= RSS_IDLE_SPEED_THRESHOLD_MPS

        if not is_moving:
            if self.last_rest_start_time < 0.0:
                self.last_rest_start_time = current_time

            if self.fatigue_accumulation > 0.0 or self.fatigue_integral > 0.0:
                rest_duration = current_time - self.last_rest_start_time
                if rest_duration >= self.fatigue_decay_min_rest_time:
                    fatigue_time_delta = current_time - self.last_fatigue_decay_time
                    if fatigue_time_delta > 0.0:
                        self.fatigue_accumulation = max(
                            self.fatigue_accumulation
                            - (self.fatigue_decay_rate * (fatigue_time_delta / 0.2)),
                            0.0,
                        )
                        self.fatigue_integral = max(
                            self.fatigue_integral
                            - (self.fatigue_decay_rate * 0.5 * (fatigue_time_delta / 0.2)),
                            0.0,
                        )
                        self.last_fatigue_decay_time = current_time

            self.last_fatigue_decay_time = current_time
        else:
            self.last_rest_start_time = -1.0


def calculate_pandolf_power_watts(
    velocity_ms: float,
    total_weight_kg: float,
    grade_percent: float = 0.0,
    terrain_factor: float = 1.0,
) -> float:
    """与 SCR_RSS_MetabolismModel.CalculatePandolfPowerWatts 同形。"""
    v = max(0.0, velocity_ms)
    w = max(total_weight_kg, 0.0)
    terrain_factor = float(np.clip(terrain_factor, 0.5, 3.0))
    ref_w = 90.0

    if v < 0.1:
        body_w = ref_w
        load_w = max(w - body_w, 0.0)
        if load_w < 5.0:
            return 100.0 * (w / ref_w)
        base_static = 1.2 * body_w
        load_ratio = load_w / body_w if body_w > 0.0 else 0.0
        load_static = 1.6 * (body_w + load_w) * (load_ratio * load_ratio)
        return max(base_static + load_static, 0.0)

    vt = v - 0.7
    base_term = (2.7 * 0.80) + (3.2 * vt * vt)
    g = grade_percent * 0.01
    grade_term = g * (0.23 + 1.34 * v * v)
    grade_term = min(grade_term, base_term * 3.0)
    if grade_percent < 0.0 and grade_percent > -12.0:
        grade_term = grade_term * 1.25
    steep_penalty = 0.0
    if grade_percent < -15.0:
        abs_g = abs(grade_percent)
        ramp = min((abs_g - 15.0) / 15.0, 1.0)
        steep_penalty = base_term * ramp * 0.5
    w_mult = max(w / ref_w, 0.1)
    return max(w_mult * (base_term + grade_term + steep_penalty) * terrain_factor * ref_w, 0.0)


def calculate_acsm_power_watts(velocity_ms: float, total_weight_kg: float) -> float:
    """与 SCR_RSS_MetabolismModel.CalculateAcsmPowerWatts 同形。"""
    v = max(0.0, velocity_ms)
    w = max(total_weight_kg, 45.0)
    mass_scale = w / 90.0
    pref = V6_ACSM_REST_W + V6_ACSM_LINEAR_W_PER_MS * v + V6_ACSM_QUAD_W_PER_MS2 * v * v
    return max(pref * mass_scale, 0.0)


def get_acsm_blend_weight(velocity_ms: float) -> float:
    if velocity_ms <= V6_ACSM_BLEND_START_MS:
        return 0.0
    if velocity_ms >= V6_ACSM_BLEND_END_MS:
        return 1.0
    return (velocity_ms - V6_ACSM_BLEND_START_MS) / (V6_ACSM_BLEND_END_MS - V6_ACSM_BLEND_START_MS)


def metabolism_power_watts(
    velocity_ms: float,
    total_weight_kg: float,
    grade_percent: float = 0.0,
    terrain_factor: float = 1.0,
    movement_phase: int = 2,
) -> float:
    """Pandolf + ACSM 混合（与 SCR_RSS_MetabolismModel.MetabolismPowerWatts 同形）。"""
    pandolf_w = calculate_pandolf_power_watts(
        velocity_ms, total_weight_kg, grade_percent, terrain_factor)
    prefer_acsm = movement_phase in (2, 3) or velocity_ms >= V6_ACSM_BLEND_END_MS
    if not prefer_acsm and velocity_ms < V6_ACSM_BLEND_START_MS:
        return pandolf_w
    acsm_w = calculate_acsm_power_watts(velocity_ms, total_weight_kg)
    blend = get_acsm_blend_weight(velocity_ms)
    if movement_phase == 3:
        blend = max(blend, 0.85)
    return pandolf_w * (1.0 - blend) + acsm_w * blend


def invert_speed_for_power_watts(
    target_power_watts: float,
    total_weight_kg: float,
    grade_percent: float = 0.0,
    terrain_factor: float = 1.0,
    movement_phase: int = 2,
) -> float:
    if target_power_watts <= 1.0:
        return 0.0
    lo, hi = 0.0, V6_INVERT_SPEED_MAX_MS
    for _ in range(24):
        mid = (lo + hi) * 0.5
        p = metabolism_power_watts(mid, total_weight_kg, grade_percent, terrain_factor, movement_phase)
        if p > target_power_watts:
            hi = mid
        else:
            lo = mid
    return (lo + hi) * 0.5


def compute_cp_watts(
    cp0: float,
    load_kg: float,
    grade_percent: float,
    env_mult: float = 1.0,
    fatigue_norm: float = 0.0,
) -> float:
    excess = max(0.0, load_kg - V6_CP_LOAD_REF_KG)
    cp = cp0 * (1.0 - V6_CP_LOAD_DECAY_PER_KG * excess)
    g = grade_percent * 0.01
    if g > 0.0:
        cp *= max(0.65, 1.0 - V6_CP_SLOPE_K_UP * g * g)
    cp *= max(0.55, min(1.0, env_mult))
    cp *= max(0.75, 1.0 - V6_CP_FATIGUE_K * fatigue_norm)
    return cp


class V6CriticalPowerState:
    """v6 CP–W′ 临界功率模型 — 与 SCR_RSS_CriticalPowerModel.c 完全同形。

    核心逻辑：
    - Morin–Petit: P > CP → W′ 放电（焦耳）；静止且非 Sprint 时不放电
    - Elite Skiba 双指数再填充 or 标准线性恢复
    - Sprint 冷却（耗尽→满冷却 / 提前释放→短冷却）
    - 动态 CP：load/slope/env/fatigue 四因子调制
    """

    def __init__(self, cp0: float = V6_CRITICAL_POWER_WATTS_DEFAULT,
                 w_prime_max: float = V6_W_PRIME_MAX_JOULES_DEFAULT,
                 sprint_power_cap: float = V6_SPRINT_POWER_CAP_WATTS_DEFAULT):
        self.cp0 = cp0
        self.w_prime_max_joules = w_prime_max
        self.sprint_power_cap_watts = sprint_power_cap
        self.reset_to_full()

        # 上下文（SetRuntimeContext）
        self.load_kg = 0.0
        self.grade_percent = 0.0
        self.env_cp_mult = 1.0
        self.fatigue_norm = 0.0
        self.fatigue_cp_multiplier = 1.0

    # ── 状态管理 ──
    def reset_to_full(self):
        self.w_prime_joules = self.w_prime_max_joules
        self.cooldown_until_sec = -1.0
        self.sprint_start_sec = -1.0
        self.was_sprinting = False
        self.last_short_burst_release_sec = -1.0
        self.depletion_cooldown_applied = False

    def set_runtime_context(self, load_kg: float, grade_percent: float,
                            env_cp_mult: float, fatigue_norm: float):
        self.load_kg = max(load_kg, 0.0)
        self.grade_percent = grade_percent
        self.env_cp_mult = max(V6_CP_ENV_FLOOR, min(env_cp_mult, 1.0))
        self.fatigue_norm = max(0.0, min(fatigue_norm, 1.0))

    def set_fatigue_cp_multiplier(self, mult: float):
        self.fatigue_cp_multiplier = max(0.75, min(mult, 1.0))

    # ── 属性 ──
    @property
    def pool01(self) -> float:
        if self.w_prime_max_joules <= 1.0:
            return 0.0
        return max(0.0, min(1.0, self.w_prime_joules / self.w_prime_max_joules))

    @property
    def cooldown_remaining(self) -> float:
        if self.cooldown_until_sec < 0.0:
            return 0.0
        return max(0.0, self.cooldown_until_sec)  # caller subtracts world_time

    def cooldown_remaining_at(self, world_time_sec: float) -> float:
        if self.cooldown_until_sec < 0.0:
            return 0.0
        return max(0.0, self.cooldown_until_sec - world_time_sec)

    def is_on_cooldown(self, world_time_sec: float) -> bool:
        return self.cooldown_remaining_at(world_time_sec) > 0.0

    # ── 动态 CP 计算 ──
    def compute_cp_base_watts(self) -> float:
        """与 C ComputeCpBaseWatts 完全一致：load/slope/env 三因子调制。"""
        return compute_cp_watts(self.cp0, self.load_kg, self.grade_percent,
                                self.env_cp_mult, 0.0)  # fatigue 经 GetEffectiveCriticalPowerWatts

    def get_effective_critical_power_watts(self) -> float:
        """含疲劳乘数：effective = cp_base * fatigue_mult。"""
        return self.compute_cp_base_watts() * self.fatigue_cp_multiplier

    # ── Skiba vs 线性恢复判断 ──
    def _uses_skiba_recovery(self) -> bool:
        return self.cp0 <= V6_SKIBA_ELITE_CP_THRESHOLD_W

    # ── W′ 恢复（Skiba 双指数 or 线性）─与 C ApplyWPrimeRecovery 同形──
    def _apply_w_prime_recovery(self, power_watts: float, cp: float, dt: float):
        if self._uses_skiba_recovery():
            w_lim = self.w_prime_max_joules * V6_W_PRIME_LIM_RATIO
            k_fast = V6_W_PRIME_K_FAST * (1.0 - 0.3 * self.fatigue_norm)
            k_slow = V6_W_PRIME_K_SLOW * (1.0 - 0.5 * self.fatigue_norm)
            k_fast = max(k_fast, 0.01)
            k_slow = max(k_slow, 0.0001)

            fast_term = 0.0
            if self.w_prime_joules < w_lim:
                fast_term = k_fast * (w_lim - self.w_prime_joules)
            slow_term = 0.0
            if self.w_prime_joules >= w_lim:
                slow_term = k_slow * (self.w_prime_max_joules - self.w_prime_joules)
            self.w_prime_joules += (fast_term + slow_term) * dt
        else:
            recovery_w = V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT  # simplified; C uses ConfigBridge
            self.w_prime_joules += recovery_w * dt

        self.w_prime_joules = min(self.w_prime_joules, self.w_prime_max_joules)

    # ── 可用功率（含 W′ 爆发预算）──
    def get_available_power_watts(self, sprint_intent: bool, dt: float, world_time_sec: float) -> float:
        cp = self.get_effective_critical_power_watts()
        if not sprint_intent:
            return cp
        if self.is_on_cooldown(world_time_sec):
            return cp

        cap = self.sprint_power_cap_watts
        if cap <= cp:
            cap = cp + V6_SPRINT_POWER_CAP_WATTS_DEFAULT * 0.5

        if self.w_prime_joules <= 0.0:
            return cp

        burst_budget = self.w_prime_joules / max(dt, 0.01)
        available = cp + burst_budget
        return min(available, cap)

    # ── Sprint 准入判断 ──
    def is_sprint_allowed(self, aerobic_stamina: float, collapse_state: bool, world_time_sec: float) -> bool:
        if collapse_state:
            return False
        if aerobic_stamina < 0.25:  # SPRINT_ENABLE_THRESHOLD
            return False
        if self.pool01 <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT:
            return False
        if self.is_on_cooldown(world_time_sec):
            return False
        return True

    # ── Sprint 结束冷却 ──
    def _apply_cooldown_on_sprint_end(self, world_time_sec: float, burst_duration_sec: float, reserve_at_end01: float):
        full_cd = V5_BURST_COOLDOWN_FULL_DEFAULT
        short_cd = V5_BURST_COOLDOWN_SHORT_DEFAULT

        if reserve_at_end01 <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT:
            # 耗尽 → 满冷却
            self.cooldown_until_sec = world_time_sec + full_cd
            return

        if burst_duration_sec <= V5_TACTICAL_SHORT_BURST_SEC:
            # 短爆发 → 短冷却
            self.cooldown_until_sec = world_time_sec + short_cd
            self.last_short_burst_release_sec = world_time_sec
            return

        # 中等爆发 → 按剩余池缩放冷却
        scaled = full_cd * (1.0 - V5_BURST_EARLY_RELEASE_BONUS * reserve_at_end01)
        scaled = max(scaled, short_cd)
        self.cooldown_until_sec = world_time_sec + scaled

    # ── 主 Tick ──
    def tick(self, power_watts: float, sprint_intent: bool, world_time_sec: float,
             dt: float, current_speed_ms: float = 0.0):
        cp = self.get_effective_critical_power_watts()

        # Morin–Petit: P > CP 消耗 W′；静止且非 Sprint 意图时不放电
        if power_watts > cp:
            allow_discharge = True
            if current_speed_ms < 0.05 and not sprint_intent:
                allow_discharge = False
            if allow_discharge:
                drain_j = (power_watts - cp) * dt
                self.w_prime_joules = max(0.0, self.w_prime_joules - drain_j)

        if sprint_intent:
            # 首次 sprint 记录开始时间
            if not self.was_sprinting:
                self.sprint_start_sec = world_time_sec

            # 池耗尽时标记冷却（持续 sprint 中仅触发一次）
            if self.pool01 <= V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT:
                if not self.depletion_cooldown_applied:
                    burst_dur = world_time_sec - self.sprint_start_sec
                    burst_dur = max(burst_dur, 0.0)
                    self._apply_cooldown_on_sprint_end(world_time_sec, burst_dur, self.pool01)
                    self.depletion_cooldown_applied = True
        else:
            self.depletion_cooldown_applied = False
            # sprint→非sprint 转换
            if self.was_sprinting:
                burst_dur = world_time_sec - self.sprint_start_sec
                burst_dur = max(burst_dur, 0.0)
                self._apply_cooldown_on_sprint_end(world_time_sec, burst_dur, self.pool01)
                self.sprint_start_sec = -1.0

            # 非冷却 + P <= CP+5 → W′ 恢复
            if not self.is_on_cooldown(world_time_sec) and power_watts <= cp + 5.0:
                self._apply_w_prime_recovery(power_watts, cp, dt)

        self.was_sprinting = sprint_intent

    # ── W′ 耗尽超速钳位 ──
    def get_w_prime_exhausted_overspeed_cap_ms(
        self,
        measured_speed_ms: float,
        applied_speed_limit_ms: float,
        movement_phase: int,
        total_weight_kg: float,
        grade_percent: float,
        terrain_factor: float,
    ) -> float:
        """与 SCR_RSS_DrainCalculator.GetWPrimeExhaustedOverspeedCapMs 同形。
        返回 W′ 耗尽时应强制应用的绝对速度上限（m/s），否则返回 -1。
        """
        if not is_metabolic_overspeed_accounting(measured_speed_ms, applied_speed_limit_ms):
            return -1.0
        if is_wprime_pool_available_for_overspeed(self.pool01, V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT):
            return -1.0

        cp = self.get_effective_critical_power_watts()
        if cp <= 1.0:
            cp = V6_CRITICAL_POWER_WATTS_DEFAULT

        # 用 CP 反解可持续速度
        return invert_speed_for_power_watts(
            cp, total_weight_kg, grade_percent, terrain_factor, movement_phase)


def get_w_prime_exhausted_overspeed_cap_ms(
    measured_ms: float,
    applied_limit_ms: float,
    w_prime_pool01: float,
    movement_phase: int,
    total_weight_kg: float,
    grade_percent: float,
    terrain_factor: float,
    cp_model=None,
) -> float:
    """与 SCR_RSS_DrainCalculator.GetWPrimeExhaustedOverspeedCapMs 同形。
    返回 W′ 耗尽时应强制应用的绝对速度上限（m/s）；否则 -1。
    """
    if not is_metabolic_overspeed_accounting(measured_ms, applied_limit_ms):
        return -1.0
    if is_wprime_pool_available_for_overspeed(w_prime_pool01):
        return -1.0
    if cp_model is not None:
        cp = cp_model.get_effective_critical_power_watts()
    else:
        cp = V6_CRITICAL_POWER_WATTS_DEFAULT
    if cp <= 1.0:
        return -1.0
    return invert_speed_for_power_watts(cp, total_weight_kg, grade_percent, terrain_factor, movement_phase)


def simulate_v6_sprint_seconds(
    load_kg: float = 35.0,
    cp0: float = 400.0,
    dt: float = 0.017,
    sprint_cap_w: float = 1450.0,
    w_prime_max: float = V6_W_PRIME_MAX_JOULES_DEFAULT,
) -> float:
    """v6 CP–W′ sprint 爆发时长（W′ 池从满降至 20%）。

    接受完整 v6 参数，用于优化管线硬约束门禁。
    """
    total_w = 90.0 + load_kg
    state = V6CriticalPowerState(cp0=cp0, w_prime_max=w_prime_max,
                                  sprint_power_cap=sprint_cap_w)
    state.set_runtime_context(load_kg, 0.0, 1.0, 0.0)
    t = 0.0
    while state.pool01 > 0.20 and t < 30.0:
        available_p = state.get_available_power_watts(True, dt, t)
        state.tick(available_p, True, t, dt)
        t += dt
    return t


def _load_elite_preset_params_from_json() -> Dict:
    """EliteStandard v4 JSON（标定 coeff 源）。"""
    path = Path(__file__).resolve().parent / "optimized_rss_config_elitestandard_v4.json"
    if not path.is_file():
        return {}
    with path.open(encoding="utf-8") as f:
        data = json.load(f)
    return {k: float(v) for k, v in data.items() if not str(k).startswith("_")}


def simulate_ideal_march_aerobic_end(
    hours: float = 4.0,
    encumbrance_kg: float = 35.0,
    speed_ms: float = 1.39,
    dt_sec: float = 2.0,
    params: Optional[Dict] = None,
    constants: Optional["RSSConstants"] = None,
) -> float:
    """理想平地行军：返回结束时有氧池比例（0..1）。

    与 bench_physio_anchors 4h/35kg 锚点契约一致；使用 game_player_tick + 疲劳 cap。
    默认 Elite preset（含标定 energy_to_stamina_coeff）。
    """
    if constants is None:
        trial = params if params is not None else _load_elite_preset_params_from_json()
        if trial:
            merged = merge_game_aligned_params(
                {k: float(v) for k, v in trial.items() if not str(k).startswith("_")}
            )
            constants = RSSConstants(**merged)
        else:
            constants = RSSConstants()
    twin = RSSDigitalTwin(constants)
    total_weight = twin.constants.CHARACTER_WEIGHT + encumbrance_kg
    t = 0.0
    end_sec = hours * 3600.0
    while t < end_sec:
        twin.game_player_tick(
            MovementType.WALK,
            total_weight,
            0.0,
            1.0,
            Stance.STAND,
            t,
            dt_sec,
            enable_randomness=False,
        )
        t += dt_sec
    return float(twin.stamina)


# =============================================================================
# 测试入口
# =============================================================================
if __name__ == '__main__':
    constants = RSSConstants()
    twin = RSSDigitalTwin(constants)

    # ACFT 2英里
    target_speed = 3500.0 / 927.0
    results = twin.simulate_scenario(
        [(0, target_speed), (927, target_speed)], 90.0, 0.0, 1.0,
        Stance.STAND, MovementType.RUN, enable_randomness=False)
    print(f"ACFT: min_stamina={results['min_stamina']:.4f}, time={results['total_time_with_penalty']:.1f}s")

    # 30kg 城市战斗
    twin.reset()
    results = twin.simulate_scenario(
        [(0, 2.5), (60, 3.8), (120, 2.5), (180, 5.0), (210, 2.5), (300, 2.5)],
        120.0, 0.0, 1.2, Stance.STAND, MovementType.RUN, enable_randomness=False)
    print(f"Urban 30kg: min_stamina={results['min_stamina']:.4f}")
