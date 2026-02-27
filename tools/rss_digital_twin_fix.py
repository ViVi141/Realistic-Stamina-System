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

# Tobler 徒步函数：与 C 端 SCR_RealisticStaminaSystem.CalculateSlopeAdjustedTargetSpeed 一致
# 公式形状保留（上坡/陡下坡减速），归一化以平地=1.0，使 0 kg 平地下达引擎最大 5.2 m/s


def tobler_speed_multiplier(angle_deg: float, w_max_kmh: float = 6.0,
                            exp_coeff: float = 3.5, s_offset: float = 0.05,
                            min_mult: float = 0.15) -> float:
    """托布勒徒步函数：根据坡度角（度）返回速度乘数 [0.15, 1.0]。以平地=1.0 归一化，与 C 端一致。"""
    s = math.tan(math.radians(angle_deg))
    w_kmh = w_max_kmh * math.exp(-exp_coeff * abs(s + s_offset))
    flat = w_max_kmh * math.exp(-exp_coeff * s_offset)
    mult = w_kmh / flat if flat > 0 else min_mult
    return max(min_mult, min(1.0, mult))


# =============================================================================
# RSSConstants - 与 SCR_StaminaConstants.c 完全同步
# =============================================================================
@dataclass
class RSSConstants:
    """RSS 常量，与 C 端 SCR_StaminaConstants.c 一一对应"""

    GAME_MAX_SPEED = 5.2
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
    BASE_RECOVERY_RATE = 0.00015
    RECOVERY_NONLINEAR_COEFF = 0.5
    FAST_RECOVERY_DURATION_MINUTES = 0.4
    FAST_RECOVERY_MULTIPLIER = 1.6
    MEDIUM_RECOVERY_START_MINUTES = 0.4
    MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
    MEDIUM_RECOVERY_MULTIPLIER = 1.3
    SLOW_RECOVERY_START_MINUTES = 10.0
    SLOW_RECOVERY_MULTIPLIER = 0.6

    # 姿态恢复倍数
    STANDING_RECOVERY_MULTIPLIER = 1.3
    CROUCHING_RECOVERY_MULTIPLIER = 1.4
    PRONE_RECOVERY_MULTIPLIER = 1.6

    # 姿态消耗倍数（SCR_StaminaConstants POSTURE_CROUCH_MULTIPLIER=1.8, POSTURE_PRONE_MULTIPLIER=3.0）
    POSTURE_CROUCH_MULTIPLIER = 1.8
    POSTURE_PRONE_MULTIPLIER = 3.0

    # 恢复/消耗相关
    REST_RECOVERY_RATE = 0.250  # pts/s
    REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2  # = 0.0005

    # 负重
    BODY_TOLERANCE_BASE = 90.0
    ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20
    ENCUMBRANCE_SPEED_PENALTY_EXPONENT = 1.5
    ENCUMBRANCE_SPEED_PENALTY_MAX = 0.75
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0
    LOAD_RECOVERY_PENALTY_COEFF = 0.0001
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0

    SPRINT_VELOCITY_THRESHOLD = 5.2
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.5  # [DEPRECATED] C 端已统一公式，Python twin 不应用此倍数
    TARGET_RUN_SPEED = 3.7
    TARGET_RUN_SPEED_MULTIPLIER = 3.7 / 5.2  # 0.7115
    SMOOTH_TRANSITION_START = 0.25
    SMOOTH_TRANSITION_END = 0.05
    MIN_LIMP_SPEED_MULTIPLIER = 1.0 / 5.2  # 1.0 m/s
    EXHAUSTION_THRESHOLD = 0.0
    SPRINT_ENABLE_THRESHOLD = 0.20
    SPRINT_SPEED_BOOST = 0.30  # 从配置获取，默认 0.30

    FATIGUE_START_TIME_MINUTES = 5.0
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0
    FATIGUE_RECOVERY_PENALTY = 0.05
    FATIGUE_RECOVERY_DURATION_MINUTES = 20.0

    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0

    EPOC_DELAY_SECONDS = 0.5
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
    ENV_SURFACE_WETNESS_PENALTY_MAX = 0.15

    # [DEPRECATED] C 端 CalculateSlopeStaminaDrainMultiplier 未被调用，坡度由 Pandolf grade 承担
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03

    def __init__(self, **kwargs):
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)


class EnvironmentFactor:
    def __init__(self, constants):
        self.constants = constants
        self.heat_stress = 0.0
        self.cold_stress = 0.0
        self.surface_wetness = 0.0


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
        self.current_time = 0.0
        self.last_speed = 0.0
        self.epoc_delay_start_time = -1.0
        self.is_in_epoc_delay = False
        self.speed_before_stop = 0.0
        self.rest_duration_minutes = 0.0
        self.exercise_duration_minutes = 0.0
        self.last_movement_time = 0.0
        self.max_history_length = 5000

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
    # CalculateSanteeDownhillCorrection - SCR_RealisticStaminaSystem.c 1172-1195
    # -------------------------------------------------------------------------
    def _santee_downhill_correction(self, grade_percent: float) -> float:
        if grade_percent >= 0:
            return 1.0
        ab = abs(grade_percent)
        if ab <= 15.0:
            return 1.0
        term = ab * (1.0 - ab / 15.0) / 2.0
        return float(np.clip(1.0 - term, 0.5, 1.0))

    # -------------------------------------------------------------------------
    # CalculatePandolfEnergyExpenditure - SCR_RealisticStaminaSystem.c 904-976
    # -------------------------------------------------------------------------
    def _pandolf_expenditure(self, velocity: float, current_weight: float,
                            grade_percent: float, terrain_factor: float) -> float:
        """与 C 完全一致。返回 %/s，velocity<0.1 时返回 -0.0025（恢复）。"""
        velocity = max(0.0, velocity)
        current_weight = max(0.0, current_weight)
        if velocity < 0.1:
            return -0.0025

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

        if grade_percent < 0:
            santee = self._santee_downhill_correction(grade_percent)
            if 0 < santee < 1.0:
                grade_term = grade_term / santee

        max_grade_term = base_term * 3.0
        grade_term = min(grade_term, max_grade_term)
        terrain_factor = np.clip(terrain_factor, 0.5, 3.0)

        w_mult = max(current_weight / ref, 0.1)
        energy_expenditure = w_mult * (base_term + grade_term) * terrain_factor * ref
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
        game_max = getattr(self.constants, 'GAME_MAX_SPEED', 5.2)
        rest_per_tick = getattr(self.constants, 'REST_RECOVERY_PER_TICK', 0.0005)

        # 1. 基础消耗率 (C: StaminaUpdateCoordinator.CalculateLandBaseDrainRate)
        if movement_type == MovementType.IDLE or speed <= 0.05:
            # Idle: baseDrainRate = -REST_RECOVERY_PER_TICK
            raw = -rest_per_tick
        elif speed < 0.1:
            # 静态负重站立
            load_weight = max(0.0, current_weight - bw)
            static_per_s = self._static_standing_cost(bw, load_weight)
            raw = static_per_s * 0.2
        else:
            pandolf_per_s = self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor)
            raw = pandolf_per_s * (1.0 + wind_drag) * 0.2

        base_for_recovery = max(0.0, raw) if raw > 0 else 0.0

        # 2. 负数为恢复，不应用消耗链
        if raw <= 0.0:
            return (0.0, raw)

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
                                 stance: int, environment_factor, current_speed: float) -> float:
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

        # 负重剥夺
        weight_for_rec = c.CHARACTER_WEIGHT if stance == Stance.PRONE else current_weight
        if weight_for_rec > 0:
            load_ratio = np.clip(weight_for_rec / c.BODY_TOLERANCE_BASE, 0.0, 2.0)
            penalty = (load_ratio ** c.LOAD_RECOVERY_PENALTY_EXPONENT) * c.LOAD_RECOVERY_PENALTY_COEFF
            recovery_rate -= penalty

        # 边际效应衰减
        if stamina_percent_clamped > c.MARGINAL_DECAY_THRESHOLD:
            marginal_mult = np.clip(c.MARGINAL_DECAY_COEFF - stamina_percent_clamped, 0.2, 1.0)
            recovery_rate *= marginal_mult

        recovery_rate = max(recovery_rate, 0.0)

        # 速度调整 (SCR_StaminaRecovery 136-150)
        if current_speed >= 5.0:
            recovery_rate *= 0.1
        elif current_speed >= 3.2:
            recovery_rate *= 0.3
        elif current_speed >= 0.1:
            recovery_rate *= 0.8

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

        return float(max(recovery_rate, 0.0))

    # -------------------------------------------------------------------------
    # step - 单步仿真
    # -------------------------------------------------------------------------
    def step(self, speed: float, current_weight: float, grade_percent: float, terrain_factor: float,
             stance: int, movement_type: int, current_time: float, enable_randomness: bool = True):
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

        self.last_speed = speed
        if len(self.speed_history) < self.max_history_length:
            self.speed_history.append(speed)

        if speed > 0.05:
            self.exercise_duration_minutes += time_delta / 60.0
            self.rest_duration_minutes = 0.0
        else:
            self.rest_duration_minutes += time_delta / 60.0

        try:
            base_for_rec, total_drain = self._calculate_drain_rate_c_aligned(
                speed, current_weight, grade_percent, terrain_factor,
                stance, movement_type, 0.0, self.environment_factor)

            total_drain = float(total_drain)
            if total_drain >= 0.0:
                total_drain = min(total_drain, 0.1)

            if self.is_in_epoc_delay:
                epoc_rate = self.constants.EPOC_DRAIN_RATE * (
                    1.0 + np.clip(self.speed_before_stop / 5.2, 0.0, 1.0) * 0.5)
                total_drain = total_drain + epoc_rate

            recovery_rate = self._calculate_recovery_rate(
                self.stamina, self.rest_duration_minutes, self.exercise_duration_minutes,
                current_weight, base_for_rec, False, stance, self.environment_factor, speed)

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

    # -------------------------------------------------------------------------
    # 闭环仿真：体力→速度→消耗，与 C 端 UpdateSpeedBasedOnStamina 一致
    # -------------------------------------------------------------------------

    def calculate_speed_multiplier_by_stamina(self, stamina_percent: float) -> float:
        """与 C CalculateSpeedMultiplierByStamina 一致。"""
        stamina_percent = np.clip(float(stamina_percent), 0.0, 1.0)
        start = getattr(self.constants, 'SMOOTH_TRANSITION_START', 0.25)
        end = getattr(self.constants, 'SMOOTH_TRANSITION_END', 0.05)
        target_mult = getattr(self.constants, 'TARGET_RUN_SPEED_MULTIPLIER', 3.7 / 5.2)
        min_limp = getattr(self.constants, 'MIN_LIMP_SPEED_MULTIPLIER', 1.0 / 5.2)
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
    ) -> float:
        """与 C CalculateFinalSpeedMultiplier 一致，返回 m/s。平地，无坡度。"""
        c = self.constants
        game_max = getattr(c, 'GAME_MAX_SPEED', 5.2)
        bw = getattr(c, 'CHARACTER_WEIGHT', 90.0)
        base_w = getattr(c, 'BASE_WEIGHT', 1.36)
        coeff = getattr(c, 'ENCUMBRANCE_SPEED_PENALTY_COEFF', 0.20)
        exp = getattr(c, 'ENCUMBRANCE_SPEED_PENALTY_EXPONENT', 1.5)
        max_pen = getattr(c, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
        sprint_boost = getattr(c, 'SPRINT_SPEED_BOOST', 0.30)
        exhausted = stamina_percent <= getattr(c, 'EXHAUSTION_THRESHOLD', 0.0)
        can_sprint = stamina_percent >= getattr(c, 'SPRINT_ENABLE_THRESHOLD', 0.20)

        phase = movement_phase
        if exhausted or not can_sprint:
            if phase == MovementType.SPRINT:
                phase = MovementType.RUN

        run_base = self.calculate_speed_multiplier_by_stamina(stamina_percent)
        slope_mult = 1.0
        scaled_run = run_base * slope_mult

        eff_weight = max(0.0, current_weight - bw - base_w)
        body_mass_pct = eff_weight / bw if bw > 0 else 0.0
        base_penalty = coeff * (body_mass_pct ** exp)
        base_penalty = np.clip(base_penalty, 0.0, max_pen)
        speed_ratio = np.clip(last_speed / game_max, 0.0, 1.0)
        enc_penalty = base_penalty * (1.0 + speed_ratio)
        if phase == MovementType.SPRINT:
            enc_penalty *= 1.5
        enc_penalty = np.clip(enc_penalty, 0.0, max_pen)

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
            actual_speed = self.calculate_actual_speed(
                self.stamina, current_weight, movement_intent, last_speed
            )
            exhausted = self.stamina <= getattr(self.constants, 'EXHAUSTION_THRESHOLD', 0.0)
            can_sprint = self.stamina >= getattr(self.constants, 'SPRINT_ENABLE_THRESHOLD', 0.20)
            effective_phase = movement_intent
            if (exhausted or not can_sprint) and movement_intent == MovementType.SPRINT:
                effective_phase = MovementType.RUN
            self.step(
                actual_speed,
                current_weight,
                grade_percent,
                terrain_factor,
                stance,
                effective_phase,
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

    def simulate_scenario(self, speed_profile: List[Tuple[float, float]], current_weight: float,
                          grade_percent: float, terrain_factor: float, stance: int,
                          movement_type: int, enable_randomness: bool = True) -> Dict:
        self.reset()
        time_points = [t for t, _ in speed_profile]
        speeds = [v for _, v in speed_profile]

        current_time = 0.0
        total_distance = 0.0
        nominal_distance = 0.0
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        base = getattr(self.constants, 'BASE_WEIGHT', 1.36)
        game_max = getattr(self.constants, 'GAME_MAX_SPEED', 5.2)
        coeff = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_COEFF', 0.20)
        exp = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_EXPONENT', 1.5)
        max_pen = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_MAX', 0.75)
        posture_speed_mult = {Stance.STAND: 1.0, Stance.CROUCH: 0.7, Stance.PRONE: 0.3}.get(stance, 1.0)

        for i in range(len(time_points) - 1):
            start_time = time_points[i]
            end_time = time_points[i + 1]
            speed = speeds[i]
            duration = end_time - start_time
            steps = int(duration / 0.2)

            for _ in range(steps):
                self.step(speed, current_weight, grade_percent, terrain_factor,
                         stance, movement_type, current_time, enable_randomness)
                current_time += 0.2
                nominal_distance += speed * 0.2

                eff_weight = max(0.0, current_weight - bw - base)
                body_mass_pct = eff_weight / bw if bw > 0 else 0.0
                speed_penalty = coeff * (body_mass_pct ** exp)
                if movement_type == MovementType.SPRINT:
                    speed_penalty *= 1.5
                speed_penalty = np.clip(speed_penalty, 0.0, max_pen)
                effective_speed = speed * posture_speed_mult * (1.0 - speed_penalty)
                total_distance += effective_speed * 0.2

        min_stamina = min(self.stamina_history) if self.stamina_history else 1.0
        ratio = nominal_distance / max(total_distance, 1e-6) if nominal_distance > 0 else 1.0
        ratio = np.clip(ratio, 1.0, 5.0)
        total_time_with_penalty = current_time * ratio

        return {
            'total_distance': total_distance,
            'min_stamina': min_stamina,
            'total_time_with_penalty': total_time_with_penalty,
            'stamina_history': self.stamina_history[-1000:],
            'speed_history': self.speed_history[-500:],
            'time_history': self.time_history[-1000:]
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
        [(0, 2.5), (60, 3.7), (120, 2.5), (180, 5.0), (210, 2.5), (300, 2.5)],
        120.0, 0.0, 1.2, Stance.STAND, MovementType.RUN, enable_randomness=False)
    print(f"Urban 30kg: min_stamina={results['min_stamina']:.4f}")
