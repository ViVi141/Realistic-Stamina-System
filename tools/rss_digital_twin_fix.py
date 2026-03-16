#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Digital Twin - 从 C 源文件完全重写
严格对照 SCR_RealisticStaminaSystem.c、SCR_StaminaConstants.c、SCR_StaminaConsumption.c、
SCR_StaminaRecovery.c、SCR_StaminaUpdateCoordinator.c、SCR_EncumbranceCache.c 实现。

时间步长：dt=0.2s，与游戏 UpdateSpeedBasedOnStamina 每 0.2s 调用一致。
"""

import numpy as np
import math
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional
import random

# Tobler 徒步函数：与 C 端 SCR_RealisticStaminaSystem.CalculateSlopeAdjustedTargetSpeed 完全一致
# W = 6 * exp(-3.5 * |S + 0.05|), TOBLER_W_AT_FLAT_KMH = 5.039
TOBLER_W_AT_FLAT_KMH = 5.039


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
    BASE_RECOVERY_RATE = 0.0003  # 增加基础恢复率
    RECOVERY_NONLINEAR_COEFF = 0.8  # 增加恢复的非线性系数，使低体力时恢复更快
    FAST_RECOVERY_DURATION_MINUTES = 0.4
    FAST_RECOVERY_MULTIPLIER = 1.8  # 增加快速恢复倍数
    MEDIUM_RECOVERY_START_MINUTES = 0.4
    MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
    MEDIUM_RECOVERY_MULTIPLIER = 1.5  # 增加中速恢复倍数
    SLOW_RECOVERY_START_MINUTES = 10.0
    SLOW_RECOVERY_MULTIPLIER = 0.8  # 增加慢速恢复倍数

    # 姿态恢复倍数
    STANDING_RECOVERY_MULTIPLIER = 1.5  # 增加站姿恢复倍数
    CROUCHING_RECOVERY_MULTIPLIER = 1.6  # 增加蹲姿恢复倍数
    PRONE_RECOVERY_MULTIPLIER = 1.8  # 增加趴姿恢复倍数

    # 姿态消耗倍数（SCR_StaminaConstants POSTURE_CROUCH_MULTIPLIER=1.8, POSTURE_PRONE_MULTIPLIER=3.0）
    POSTURE_CROUCH_MULTIPLIER = 1.5  # 减少蹲姿消耗倍数
    POSTURE_PRONE_MULTIPLIER = 2.5  # 减少趴姿消耗倍数

    # 恢复/消耗相关
    REST_RECOVERY_RATE = 0.250  # pts/s
    REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2  # = 0.0005

    # 负重
    BODY_TOLERANCE_BASE = 90.0
    ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20
    ENCUMBRANCE_SPEED_PENALTY_EXPONENT = 1.5
    ENCUMBRANCE_SPEED_PENALTY_MAX = 0.75
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5  # 减少负重对消耗率的影响
    LOAD_RECOVERY_PENALTY_COEFF = 0.00008  # 减少负重对恢复率的惩罚
    LOAD_RECOVERY_PENALTY_EXPONENT = 1.8  # 减少负重对恢复率的惩罚指数

    SPRINT_VELOCITY_THRESHOLD = 5.5
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.5  # [DEPRECATED] C 端已统一公式，Python twin 不应用此倍数
    TARGET_RUN_SPEED = 3.8
    TARGET_RUN_SPEED_MULTIPLIER = 3.8 / 5.5
    SMOOTH_TRANSITION_START = 0.25
    SMOOTH_TRANSITION_END = 0.05
    MIN_LIMP_SPEED_MULTIPLIER = 1.0 / 5.5
    EXHAUSTION_THRESHOLD = 0.0
    SPRINT_ENABLE_THRESHOLD = 0.15
    SPRINT_SPEED_BOOST = 0.30  # 从配置获取，默认 0.30

    TACTICAL_SPRINT_BURST_DURATION = 8.0
    TACTICAL_SPRINT_BURST_BUFFER_DURATION = 5.0
    TACTICAL_SPRINT_BURST_ENCUMBRANCE_FACTOR = 0.2
    TACTICAL_SPRINT_COOLDOWN = 15.0

    FATIGUE_START_TIME_MINUTES = 5.0
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0
    FATIGUE_RECOVERY_PENALTY = 0.05
    FATIGUE_RECOVERY_DURATION_MINUTES = 20.0

    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0

    EPOC_DELAY_SECONDS = 2.0
    EPOC_DRAIN_RATE = 0.001

    ENERGY_TO_STAMINA_COEFF = 9e-07  # 与 C 端 EliteStandard 预设一致

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
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)


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
        self.environment_factor.heat_stress = 0.0
        self.environment_factor.cold_stress = 0.0
        self.environment_factor.cold_static_penalty = 0.0

    # -------------------------------------------------------------------------
    # CalculateStaticStandingCost - SCR_RealisticStaminaSystem.c 1135-1168
    # -------------------------------------------------------------------------
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

    # -------------------------------------------------------------------------
    # StaminaUpdateCoordinator.CalculateLandBaseDrainRate + StaminaConsumptionCalculator
    # 完全复现 C 端消耗流程：Idle 用 REST_RECOVERY，静态用 CalculateStaticStandingCost，
    # 运动用 Pandolf；再乘以 posture、efficiency、fatigue、enc_mult（已统一公式，无 Sprint 倍数）
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

        wind_mult = 1.0 + wind_drag

        # 1. 基础消耗率 (C: StaminaUpdateCoordinator.CalculateLandBaseDrainRate)
        load_dampening = getattr(self.constants, 'LOAD_METABOLIC_DAMPENING', 0.70)

        if movement_type == MovementType.IDLE:
            if speed < 0.1:
                raw = -rest_per_tick
            else:
                pandolf_per_s = self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor)
                if current_weight > bw and load_dampening < 1.0:
                    unloaded_per_s = self._pandolf_expenditure(speed, bw, grade_percent, terrain_factor)
                    load_extra = pandolf_per_s - unloaded_per_s
                    pandolf_per_s = unloaded_per_s + load_extra * load_dampening
                raw = pandolf_per_s * wind_mult * 0.2
        elif speed < 0.1:
            load_weight = max(0.0, current_weight - bw)
            static_per_s = self._static_standing_cost(bw, load_weight)
            raw = static_per_s * 0.2
        else:
            pandolf_per_s = self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor)

            # T1 负重代谢阻尼：只承受负重额外代谢的 dampening 比例
            if current_weight > bw and load_dampening < 1.0:
                unloaded_per_s = self._pandolf_expenditure(speed, bw, grade_percent, terrain_factor)
                load_extra = pandolf_per_s - unloaded_per_s
                pandolf_per_s = unloaded_per_s + load_extra * load_dampening

            # 负重限速努力补偿（与 C 端 SCR_StaminaUpdateCoordinator 一致）
            if movement_type == MovementType.RUN or movement_type == MovementType.SPRINT:
                base_pen = self._encumbrance_speed_penalty_base(current_weight)
                if base_pen > 0.0:
                    speed_ratio = float(np.clip(speed / game_max, 0.0, 1.0))
                    enc_pen = base_pen * (1.0 + speed_ratio)
                    if movement_type == MovementType.SPRINT:
                        enc_pen = enc_pen * 1.5
                    enc_pen = float(np.clip(enc_pen, 0.0, getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)))

                    denom = max(1.0 - enc_pen, 0.15)
                    unenc_speed = min(speed / denom, 6.0)  # 限制无负重速度最大值，避免消耗率过高
                    effort_per_s = self._pandolf_expenditure(unenc_speed, current_weight, grade_percent, terrain_factor)
                    if current_weight > bw and load_dampening < 1.0:
                        unloaded_effort = self._pandolf_expenditure(unenc_speed, bw, grade_percent, terrain_factor)
                        effort_extra = effort_per_s - unloaded_effort
                        effort_per_s = unloaded_effort + effort_extra * load_dampening
                    if effort_per_s > pandolf_per_s:
                        blend = float(np.clip(enc_pen / 0.7, 0.0, 0.8))  # 降低混合因子，减少额外消耗
                        pandolf_per_s = pandolf_per_s + (effort_per_s - pandolf_per_s) * blend

            raw = pandolf_per_s * wind_mult * 0.2

        base_for_recovery = max(0.0, raw) if raw > 0 else 0.0

        # 2. 负数为恢复，传入 recovery_rate 计算而非作为负 drain 双重计入
        if raw <= 0.0:
            return (raw, 0.0)

        # 2.5 温度热调节补偿 (C: SCR_StaminaConsumption.c:128-132, 在 posture 之前)
        if environment_factor is not None and raw > 0.0:
            raw = environment_factor.adjust_energy_for_temperature(raw)
        base_for_recovery = max(0.0, raw)

        # 3. 应用 posture、efficiency、fatigue (SCR_StaminaConsumption)
        posture = self._posture_consumption_multiplier(speed, stance)
        speed_ratio = np.clip(speed / game_max, 0.0, 1.0)
        total_eff = self._fitness_efficiency_factor() * self._metabolic_efficiency_factor(speed_ratio)
        fatigue = self._fatigue_factor()
        base = raw * posture * total_eff * fatigue

        # 4. 与 C 一致：已统一为 Pandolf 公式，不再对 Sprint 额外乘倍数
        # （Pandolf 中速度项自然体现 Sprint 更高消耗）

        # 5. 应用负重消耗倍数 (C: totalDrainRate *= encumbranceStaminaDrainMultiplier)
        enc_mult = self._encumbrance_stamina_drain_multiplier(current_weight)
        total_drain = base * enc_mult

        return (base_for_recovery, float(max(0.0, total_drain)))

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

        # 休息时间倍数
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
            heat = getattr(environment_factor, 'heat_stress', 0.0)
            if heat > 0.0:
                recovery_rate *= (1.0 - heat)

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

        # 移除基于速度的恢复率惩罚，保持正常恢复率
        # 这样更符合现实：即使在运动时，身体也会持续恢复，只是消耗大于恢复
        # 通过消耗率大于恢复率来实现净消耗
        pass



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
             wind_drag: float = 0.0):
        speed = max(0.0, float(speed))
        current_weight = max(0.0, float(current_weight))
        grade_percent = np.clip(float(grade_percent), -85.0, 85.0)
        terrain_factor = np.clip(float(terrain_factor), 0.5, 3.0)
        current_time = max(0.0, float(current_time))

        time_delta = current_time - self.current_time if self.current_time > 0 else 0.2
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

        if speed > 0.05:
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
            net_change = recovery_rate - total_drain
            net_change = np.clip(float(net_change), -0.1, 0.01)

            self.stamina += net_change
            self.stamina = np.clip(self.stamina, 0.0, 1.0)

            self.base_drain_rate_by_velocity = base_for_rec
            self.final_drain_rate = total_drain
            self.recovery_rate = recovery_rate
            self.net_change = net_change

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
            self.net_change = -0.001
            self.stamina = np.clip(self.stamina - 0.001, 0.0, 1.0)
            if len(self.stamina_history) < self.max_history_length:
                self.stamina_history.append(self.stamina)
                self.time_history.append(current_time)
                self.recovery_rate_history.append(float(recovery_rate))
                self.drain_rate_history.append(float(total_drain))

    # -------------------------------------------------------------------------
    # 闭环仿真：体力→速度→消耗，与 C 端 UpdateSpeedBasedOnStamina 一致
    # -------------------------------------------------------------------------

    def calculate_speed_multiplier_by_stamina(self, stamina_percent: float) -> float:
        """与 C CalculateSpeedMultiplierByStamina 一致。"""
        stamina_percent = np.clip(float(stamina_percent), 0.0, 1.0)
        start = getattr(self.constants, 'SMOOTH_TRANSITION_START', 0.25)
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
    ) -> Dict:
        """闭环仿真：每 tick 根据当前体力计算实际速度，再计算消耗。"""
        self.reset()
        current_weight = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0) + load_kg
        stance = Stance.STAND
        dt = 0.2
        current_time = 0.0
        last_speed = 0.0

        steps = int(duration_s / dt)
        for _ in range(steps):
            intent_phase = int(movement_intent)
            actual_speed = self.calculate_actual_speed(
                self.stamina, current_weight, intent_phase, last_speed,
                grade_percent=grade_percent,
                current_time=current_time,
            )
            exhausted = self.stamina <= getattr(self.constants, 'EXHAUSTION_THRESHOLD', 0.0)
            can_sprint = self.stamina >= getattr(self.constants, 'SPRINT_ENABLE_THRESHOLD', 0.15)
            effective_intent = intent_phase
            if (exhausted or not can_sprint) and intent_phase == MovementType.SPRINT:
                effective_phase = MovementType.RUN
            else:
                effective_phase = effective_intent

            engine_phase = self._compute_engine_movement_phase(
                effective_phase,
                actual_speed,
                last_speed,
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
            last_speed = actual_speed
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
        cold_threshold = 0.0
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
