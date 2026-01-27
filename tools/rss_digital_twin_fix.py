#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Digital Twin 修复版本
修复与C代码算法不一致的问题
"""

import numpy as np
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional


@dataclass
class RSSConstants:
    """RSS 常量定义"""
    # 游戏常量
    GAME_MAX_SPEED = 5.2  # m/s
    CHARACTER_WEIGHT = 90.0  # kg
    TARGET_RUN_SPEED = 3.7  # m/s
    
    # 医学模型参数（与 C 端 SCR_RealisticStaminaSystem / SCR_StaminaConstants 对齐）
    PANDOLF_VELOCITY_COEFF = 3.2
    PANDOLF_VELOCITY_OFFSET = 0.7
    PANDOLF_BASE_COEFF = 2.7
    PANDOLF_GRADE_BASE_COEFF = 0.23
    PANDOLF_GRADE_VELOCITY_COEFF = 1.34
    PANDOLF_STATIC_COEFF_1 = 1.2
    PANDOLF_STATIC_COEFF_2 = 1.6
    REFERENCE_WEIGHT = 90.0
    GIVONI_CONSTANT = 0.3
    GIVONI_VELOCITY_EXPONENT = 2.2
    AEROBIC_THRESHOLD = 0.6
    ANAEROBIC_THRESHOLD = 0.8
    FATIGUE_START_TIME_MINUTES = 5.0
    # 消耗用姿态倍数（与 C 端 Consumption 一致，仅移动时应用）
    CONSUMPTION_POSTURE_STAND = 1.0
    CONSUMPTION_POSTURE_CROUCH = 1.8
    CONSUMPTION_POSTURE_PRONE = 3.0
    
    # 恢复模型参数
    BASE_RECOVERY_RATE = 0.0004  # 每0.2秒恢复0.04%
    RECOVERY_NONLINEAR_COEFF = 0.5
    FAST_RECOVERY_DURATION_MINUTES = 1.5
    FAST_RECOVERY_MULTIPLIER = 3.5
    MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
    MEDIUM_RECOVERY_MULTIPLIER = 1.5
    SLOW_RECOVERY_MULTIPLIER = 0.8
    
    # 姿态恢复加成
    STANDING_RECOVERY_MULTIPLIER = 2.0
    CROUCHING_RECOVERY_MULTIPLIER = 1.5
    PRONE_RECOVERY_MULTIPLIER = 2.2
    
    # 负重参数
    BASE_WEIGHT = 1.36  # kg
    MAX_ENCUMBRANCE_WEIGHT = 40.5  # kg
    COMBAT_ENCUMBRANCE_WEIGHT = 30.0  # kg
    
    # 速度阈值（与 C 端一致）
    SPRINT_VELOCITY_THRESHOLD = 5.2  # m/s
    RUN_VELOCITY_THRESHOLD = 3.7    # m/s
    WALK_VELOCITY_THRESHOLD = 3.2   # m/s
    
    # 动态阈值
    RECOVERY_THRESHOLD_NO_LOAD = 2.5  # m/s
    DRAIN_THRESHOLD_COMBAT_LOAD = 1.5  # m/s
    
    # 消耗率
    SPRINT_BASE_DRAIN_RATE = 0.480  # pts/s
    RUN_BASE_DRAIN_RATE = 0.075  # pts/s
    WALK_BASE_DRAIN_RATE = 0.045  # pts/s
    REST_RECOVERY_RATE = 0.250  # pts/s
    
    # 转换为每0.2秒的消耗率
    SPRINT_DRAIN_PER_TICK = SPRINT_BASE_DRAIN_RATE / 100 * 0.2
    RUN_DRAIN_PER_TICK = RUN_BASE_DRAIN_RATE / 100 * 0.2
    WALK_DRAIN_PER_TICK = WALK_BASE_DRAIN_RATE / 100 * 0.2
    REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100 * 0.2
    
    # 其他参数
    EPOC_DELAY_SECONDS = 0.5
    EPOC_DRAIN_RATE = 0.001  # 每0.2秒
    FITNESS_LEVEL = 1.0
    FITNESS_EFFICIENCY_COEFF = 0.35
    FITNESS_RECOVERY_COEFF = 0.25
    
    # 环境因子
    ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.5
    ENV_RAIN_WEIGHT_MAX = 8.0  # kg
    ENV_HEAT_STRESS_PENALTY_MAX = 0.3  # 最大热应激惩罚
    ENV_COLD_STRESS_PENALTY_MAX = 0.2  # 最大冷应激惩罚
    ENV_SURFACE_WETNESS_PENALTY_MAX = 0.15  # 最大地表湿度惩罚
    
    # 初始化时可覆盖的参数
    def __init__(self, **kwargs):
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)



    # Energy to stamina conversion coefficient
    ENERGY_TO_STAMINA_COEFF = 3.50e-05

    # Load recovery penalty
    LOAD_RECOVERY_PENALTY_COEFF = 0.0004
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0

    # Encumbrance stamina drain
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0

    # Sprint stamina drain multiplier
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0

    # Fatigue system
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0

    # Metabolic adaptation
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2

    # Sprint speed boost
    SPRINT_SPEED_BOOST = 0.30

    # Posture multipliers
    POSTURE_CROUCH_MULTIPLIER = 0.7
    POSTURE_PRONE_MULTIPLIER = 0.3

    # Action stamina costs
    JUMP_STAMINA_BASE_COST = 0.035
    VAULT_STAMINA_START_COST = 0.02
    CLIMB_STAMINA_TICK_COST = 0.01
    JUMP_CONSECUTIVE_PENALTY = 0.5

    # Slope coefficients
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03

    # Swimming system
    SWIMMING_BASE_POWER = 20.0
    SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0
    SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0
    SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0
    SWIMMING_ENERGY_TO_STAMINA_COEFF = 5e-05

    # Environment factors
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    ENV_WIND_RESISTANCE_COEFF = 0.05
    ENV_MUD_PENALTY_MAX = 0.4

    # Marginal decay
    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1

    # Minimum recovery
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0



    # Energy conversion coefficient
    ENERGY_TO_STAMINA_COEFF = 3.50e-05

    # Load recovery penalty coefficients
    LOAD_RECOVERY_PENALTY_COEFF = 0.0004
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0

    # Encumbrance stamina drain coefficient
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0

    # Encumbrance speed penalty coefficient (align with C: SCR_StaminaConstants)
    # speed_penalty = coeff * (effective_weight / body_weight), clamped to [0.0, 0.5]
    ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20

    # Sprint stamina drain multiplier
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0

    # Fatigue system parameters
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0

    # Metabolic adaptation parameters
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2

    # Sprint speed boost
    SPRINT_SPEED_BOOST = 0.30

    # Posture speed multipliers
    POSTURE_CROUCH_MULTIPLIER = 0.7
    POSTURE_PRONE_MULTIPLIER = 0.3

    # Action stamina costs
    JUMP_STAMINA_BASE_COST = 0.035
    VAULT_STAMINA_START_COST = 0.02
    CLIMB_STAMINA_TICK_COST = 0.01
    JUMP_CONSECUTIVE_PENALTY = 0.5

    # Slope parameters
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03

    # Swimming system parameters
    SWIMMING_BASE_POWER = 20.0
    SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0
    SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0
    SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0
    SWIMMING_ENERGY_TO_STAMINA_COEFF = 5e-05

    # Environment factor parameters
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    ENV_WIND_RESISTANCE_COEFF = 0.05
    ENV_MUD_PENALTY_MAX = 0.4

    # Marginal decay parameters
    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1

    # Minimum recovery parameters
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0


class EnvironmentFactor:
    """环境因子模块"""
    
    def __init__(self, constants):
        """初始化环境因子模块"""
        self.constants = constants
        self.heat_stress = 0.0
        self.cold_stress = 0.0
        self.surface_wetness = 0.0
    
    def set_heat_stress(self, value):
        """设置热应激水平"""
        self.heat_stress = np.clip(value, 0.0, 1.0)
    
    def set_cold_stress(self, value):
        """设置冷应激水平"""
        self.cold_stress = np.clip(value, 0.0, 1.0)
    
    def set_surface_wetness(self, value):
        """设置地表湿度水平"""
        self.surface_wetness = np.clip(value, 0.0, 1.0)
    
    def get_heat_stress_penalty(self):
        """获取热应激惩罚"""
        return self.heat_stress * self.constants.ENV_HEAT_STRESS_PENALTY_MAX
    
    def get_cold_stress_penalty(self):
        """获取冷应激惩罚"""
        return self.cold_stress * self.constants.ENV_COLD_STRESS_PENALTY_MAX
    
    def get_surface_wetness_penalty(self):
        """获取地表湿度惩罚"""
        return self.surface_wetness * self.constants.ENV_SURFACE_WETNESS_PENALTY_MAX


class MovementType:
    """移动类型"""
    IDLE = 0
    WALK = 1
    RUN = 2
    SPRINT = 3


class Stance:
    """姿态"""
    STAND = 0
    CROUCH = 1
    PRONE = 2


class RSSDigitalTwin:
    """RSS 数字孪生仿真器"""
    
    def __init__(self, constants):
        """初始化仿真器"""
        self.constants = constants
        self.environment_factor = EnvironmentFactor(constants)
        self.reset()
    
    def reset(self):
        """重置仿真器状态"""
        self.stamina = 1.0  # 初始体力100%
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
        # 重置环境因子
        self.environment_factor.set_heat_stress(0.0)
        self.environment_factor.set_cold_stress(0.0)
        self.environment_factor.set_surface_wetness(0.0)
        # 性能优化：预分配空间（Python列表动态扩容，这里使用空列表初始化）
        # Python列表没有reserve方法，使用空列表初始化
    
    def _calculate_epoc_drain_rate(self, speed_before_stop):
        """计算EPOC延迟期间的消耗"""
        epoc_drain_rate = self.constants.EPOC_DRAIN_RATE
        speed_ratio_for_epoc = np.clip(speed_before_stop / self.constants.GAME_MAX_SPEED, 0.0, 1.0)
        epoc_drain_rate = epoc_drain_rate * (1.0 + speed_ratio_for_epoc * 0.5)  # 最多增加50%
        return epoc_drain_rate

    # ---------- C 端 Consumption 对齐：Pandolf / Givoni / 静态 + 全局 ×0.2 + 修正 ----------

    def _static_standing_cost(self, body_weight, load_weight):
        """静态站立消耗 %/s（与 C CalculateStaticStandingCost 一致）"""
        c1 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_1', 1.2)
        c2 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_2', 1.6)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
        base = c1 * body_weight
        load_term = 0.0
        if body_weight > 0 and load_weight > 0:
            r = load_weight / body_weight
            load_term = c2 * (body_weight + load_weight) * (r * r)
        rate = (base + load_term) * coeff
        return float(np.clip(rate, 0.0, 0.05))

    def _santee_downhill_correction(self, grade_percent):
        """Santee 下坡修正系数（与 C 一致）"""
        if grade_percent >= 0:
            return 1.0
        ab = abs(grade_percent)
        if ab <= 15.0:
            return 1.0
        term = ab * (1.0 - ab / 15.0) / 2.0
        return float(np.clip(1.0 - term, 0.5, 1.0))

    def _pandolf_expenditure(self, velocity, current_weight, grade_percent, terrain_factor):
        """Pandolf 能耗 %/s（与 C CalculatePandolfEnergyExpenditure 一致）"""
        velocity = max(0.0, velocity)
        current_weight = max(0.0, current_weight)
        if velocity < 0.1:
            return 0.0  # 静态走 Static 分支，这里不返回负
        v0 = getattr(self.constants, 'PANDOLF_VELOCITY_OFFSET', 0.7)
        vb = getattr(self.constants, 'PANDOLF_BASE_COEFF', 2.7)
        vc = getattr(self.constants, 'PANDOLF_VELOCITY_COEFF', 3.2)
        gb = getattr(self.constants, 'PANDOLF_GRADE_BASE_COEFF', 0.23)
        gv = getattr(self.constants, 'PANDOLF_GRADE_VELOCITY_COEFF', 1.34)
        ref = getattr(self.constants, 'REFERENCE_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
        fit = getattr(self.constants, 'FITNESS_LEVEL', 1.0)
        vt = velocity - v0
        base_term = (vb * (1.0 - 0.2 * fit)) + (vc * (vt * vt))
        g_dec = grade_percent * 0.01
        vsq = velocity * velocity
        grade_term = g_dec * (gb + gv * vsq)
        if grade_percent < 0:
            santee = self._santee_downhill_correction(grade_percent)
            if 0 < santee < 1.0:
                grade_term = grade_term / santee
        terrain_factor = np.clip(terrain_factor, 0.5, 3.0)
        w_mult = np.clip(current_weight / ref, 0.5, 2.0)
        energy = w_mult * (base_term + grade_term) * terrain_factor
        rate = energy * coeff
        return float(np.clip(rate, 0.0, 0.05))

    def _givoni_running(self, velocity, current_weight):
        """Givoni-Goldman 跑步 %/s（与 C CalculateGivoniGoldmanRunning 一致）"""
        if velocity <= 2.2:
            return 0.0
        ref = getattr(self.constants, 'REFERENCE_WEIGHT', 90.0)
        gc = getattr(self.constants, 'GIVONI_CONSTANT', 0.3)
        ge = getattr(self.constants, 'GIVONI_VELOCITY_EXPONENT', 2.2)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
        w_mult = np.clip(current_weight / ref, 0.5, 2.0)
        vp = np.power(velocity, ge)
        energy = w_mult * gc * vp
        rate = energy * coeff
        return float(np.clip(rate, 0.0, 0.05))

    def _metabolic_efficiency_factor(self, speed_ratio):
        """代谢适应效率因子（与 C CalculateMetabolicEfficiencyFactor 一致）"""
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

    def _fitness_efficiency_factor(self):
        """健康状态效率因子（与 C CalculateFitnessEfficiencyFactor 一致）"""
        c = getattr(self.constants, 'FITNESS_EFFICIENCY_COEFF', 0.35)
        lvl = getattr(self.constants, 'FITNESS_LEVEL', 1.0)
        f = 1.0 - c * lvl
        return float(np.clip(f, 0.7, 1.0))

    def _encumbrance_stamina_drain_multiplier(self, current_weight):
        """负重体力消耗倍数（与 C 一致，effective = current - BASE_WEIGHT）"""
        base = getattr(self.constants, 'BASE_WEIGHT', 1.36)
        eff = max(0.0, current_weight - base)
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENCUMBRANCE_STAMINA_DRAIN_COEFF', 1.5)
        mult = 1.0 + coeff * (eff / bw)
        return float(np.clip(mult, 1.0, 3.0))

    def _consumption_posture_multiplier(self, speed, stance):
        """
        消耗用姿态倍数（仅移动时应用，与 C 一致）。
        注意：C端消耗倍数与速度倍数是不同的参数。
        C端：POSTURE_CROUCH_MULTIPLIER=1.8（消耗），POSTURE_PRONE_MULTIPLIER=3.0（消耗）。
        优化器搜索的是速度倍数（0.5-1.0和0.2-0.8），但这里需要消耗倍数。
        为简化，我们假设消耗倍数 = 1.0 / 速度倍数（蹲下/趴下时速度慢但消耗高）。
        """
        if speed <= 0.05:
            return 1.0
        # 从速度倍数推导消耗倍数：速度越慢，消耗越高（因为效率低）
        speed_crouch = getattr(self.constants, 'POSTURE_CROUCH_MULTIPLIER', 0.7)
        speed_prone = getattr(self.constants, 'POSTURE_PRONE_MULTIPLIER', 0.3)
        if stance == Stance.CROUCH:
            # 消耗倍数 = 1.0 / 速度倍数，但限制在合理范围 [1.0, 2.5]
            return min(max(1.0 / max(speed_crouch, 0.4), 1.0), 2.5)
        if stance == Stance.PRONE:
            return min(max(1.0 / max(speed_prone, 0.2), 1.0), 5.0)
        return 1.0

    def _fatigue_factor(self):
        """累积疲劳因子（与 C ExerciseTracker 一致）"""
        start = getattr(self.constants, 'FATIGUE_START_TIME_MINUTES', 5.0)
        coeff = getattr(self.constants, 'FATIGUE_ACCUMULATION_COEFF', 0.015)
        mx = getattr(self.constants, 'FATIGUE_MAX_FACTOR', 2.0)
        eff = max(0.0, self.exercise_duration_minutes - start)
        f = 1.0 + coeff * eff
        return float(np.clip(f, 1.0, mx))

    def _calculate_drain_rate_c_aligned(self, speed, current_weight, grade_percent, terrain_factor,
                                        stance, movement_type, wind_drag=0.0, environment_factor=None):
        """
        与 C 端 SCR_StaminaConsumption 对齐的消耗率（每 0.2s）。
        返回 (base_for_recovery, total_drain)：
        - base_for_recovery：原始基础消耗（全局 ×0.2 后、姿态等修正前），供恢复计算用。
        - total_drain：总消耗（含姿态、效率、疲劳、速度项、负重、Sprint），用于 stamina -= total_drain。
        """
        body_weight = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        game_max = getattr(self.constants, 'GAME_MAX_SPEED', 5.2)
        enc_mult = self._encumbrance_stamina_drain_multiplier(current_weight)
        posture = self._consumption_posture_multiplier(speed, stance)
        speed_ratio = np.clip(speed / game_max, 0.0, 1.0)
        total_eff = self._fitness_efficiency_factor() * self._metabolic_efficiency_factor(speed_ratio)
        fatigue = self._fatigue_factor()
        is_sprint = (speed >= getattr(self.constants, 'SPRINT_VELOCITY_THRESHOLD', 5.2) or
                     movement_type == MovementType.SPRINT)
        sprint_mult = getattr(self.constants, 'SPRINT_STAMINA_DRAIN_MULTIPLIER', 3.0) if is_sprint else 1.0

        # 静态 / 跑步 / 步行
        if speed < 0.1:
            load_weight = max(0.0, current_weight - body_weight)
            static_per_s = self._static_standing_cost(body_weight, load_weight)
            raw = static_per_s * 0.2
        elif speed > 2.2:
            run_per_s = self._givoni_running(speed, current_weight) * terrain_factor * (1.0 + wind_drag)
            raw = run_per_s * 0.2
        else:
            pandolf_per_s = self._pandolf_expenditure(speed, current_weight, grade_percent, terrain_factor) * (1.0 + wind_drag)
            raw = pandolf_per_s * 0.2

        raw = raw * 0.2   # 全局 ×0.2（与 C 一致）
        base_for_recovery = max(0.0, raw)

        if raw <= 0.0:
            base = 0.0
        else:
            base = raw * posture * total_eff * fatigue

        speed_linear = 0.00005 * speed_ratio * total_eff * fatigue
        speed_sq = 0.00005 * speed_ratio * speed_ratio * total_eff * fatigue
        enc_base = 0.001 * (enc_mult - 1.0)
        enc_speed = 0.0002 * (enc_mult - 1.0) * speed_ratio * speed_ratio
        components = base + speed_linear + speed_sq + enc_base + enc_speed
        total_drain = components * sprint_mult
        total_drain = min(total_drain, 0.02)
        total_drain = max(0.0, total_drain)
        return (base_for_recovery, total_drain)

    def _update_epoc_delay(self, speed, current_time):
        """更新EPOC延迟状态"""
        was_moving = (self.last_speed > 0.05)
        is_now_stopped = (speed <= 0.05)
        
        # 如果从运动状态变为静止状态，启动EPOC延迟
        if was_moving and is_now_stopped and not self.is_in_epoc_delay:
            self.epoc_delay_start_time = current_time
            self.is_in_epoc_delay = True
            self.speed_before_stop = self.last_speed
        
        # 检查EPOC延迟是否已结束
        if self.is_in_epoc_delay:
            epoc_delay_duration = current_time - self.epoc_delay_start_time
            if epoc_delay_duration >= self.constants.EPOC_DELAY_SECONDS:
                self.is_in_epoc_delay = False
                self.epoc_delay_start_time = -1.0
        
        # 如果重新开始运动，取消EPOC延迟
        if speed > 0.05:
            self.is_in_epoc_delay = False
            self.epoc_delay_start_time = -1.0
        
        # 更新上一帧的速度
        self.last_speed = speed
        
        return self.is_in_epoc_delay
    
    def step(self, speed, current_weight, grade_percent, terrain_factor, 
             stance, movement_type, current_time, enable_randomness=True):
        """单步更新体力状态（性能优化版）"""
        # 更新时间
        time_delta = current_time - self.current_time if self.current_time > 0 else 0.2
        self.current_time = current_time
        
        # 添加随机扰动（减少numpy调用）
        if enable_randomness:
            import random
            # 速度扰动：±5%
            speed_noise = speed * random.uniform(-0.05, 0.05)
            speed += speed_noise
            speed = max(speed, 0.0)
            
            # 负重扰动：±2%
            weight_noise = current_weight * random.uniform(-0.02, 0.02)
            current_weight += weight_noise
            current_weight = max(current_weight, 0.0)
            
            # 坡度扰动：±1%
            grade_noise = grade_percent * random.uniform(-0.01, 0.01)
            grade_percent += grade_noise
            
            # 地形因子扰动：±5%
            terrain_noise = terrain_factor * random.uniform(-0.05, 0.05)
            terrain_factor += terrain_noise
            terrain_factor = max(terrain_factor, 0.5)
        
        # 内联EPOC延迟更新，减少函数调用
        was_moving = (self.last_speed > 0.05)
        is_now_stopped = (speed <= 0.05)
        is_in_epoc_delay = self.is_in_epoc_delay
        
        # 如果从运动状态变为静止状态，启动EPOC延迟
        if was_moving and is_now_stopped and not is_in_epoc_delay:
            self.epoc_delay_start_time = current_time
            self.is_in_epoc_delay = True
            self.speed_before_stop = self.last_speed
            is_in_epoc_delay = True
        
        # 检查EPOC延迟是否已结束
        if is_in_epoc_delay:
            epoc_delay_duration = current_time - self.epoc_delay_start_time
            if epoc_delay_duration >= self.constants.EPOC_DELAY_SECONDS:
                self.is_in_epoc_delay = False
                self.epoc_delay_start_time = -1.0
                is_in_epoc_delay = False
        
        # 如果重新开始运动，取消EPOC延迟
        if speed > 0.05:
            self.is_in_epoc_delay = False
            self.epoc_delay_start_time = -1.0
            is_in_epoc_delay = False
        
        # 更新上一帧的速度
        self.last_speed = speed
        
        # 记录速度（减少内存操作，只在需要时记录）
        if len(self.speed_history) < 10000:  # 限制历史记录长度
            self.speed_history.append(speed)
        
        # 更新运动/休息时间（避免重复计算）
        if speed > 0.05:
            self.exercise_duration_minutes += time_delta / 60.0
            self.rest_duration_minutes = 0.0
            self.last_movement_time = current_time
        else:
            self.rest_duration_minutes += time_delta / 60.0
        
        # 与 C 端对齐：base 供恢复用，total 单独扣除；代谢净值 = recovery - total_drain
        base_for_recovery, total_drain = self._calculate_drain_rate_c_aligned(
            speed, current_weight, grade_percent, terrain_factor,
            stance, movement_type, wind_drag=0.0, environment_factor=self.environment_factor
        )
        if is_in_epoc_delay:
            epoc_drain_rate = self._calculate_epoc_drain_rate(self.speed_before_stop)
            total_drain = max(total_drain, epoc_drain_rate)

        recovery_rate = self._calculate_recovery_rate(
            self.stamina,
            self.rest_duration_minutes,
            self.exercise_duration_minutes,
            current_weight,
            base_for_recovery,
            False,
            stance,
            self.environment_factor,
            speed
        )
        if enable_randomness:
            recovery_noise = recovery_rate * random.uniform(-0.1, 0.1)
            recovery_rate += recovery_noise

        net_change = recovery_rate - total_drain
        self.stamina += net_change
        self.stamina = min(max(self.stamina, 0.0), 1.0)
        
        # 记录体力（限制历史记录长度）
        if len(self.stamina_history) < 10000:
            self.stamina_history.append(self.stamina)
            self.time_history.append(current_time)
    
    def simulate_scenario(self, speed_profile, current_weight, grade_percent, 
                         terrain_factor, stance, movement_type, enable_randomness=True):
        """模拟完整场景（线程安全）"""
        # 每次模拟前重置状态，确保线程安全
        self.reset()
        
        # 解析速度曲线
        time_points = [t for t, _ in speed_profile]
        speeds = [v for _, v in speed_profile]
        
        # 模拟场景
        current_time = 0.0
        total_distance = 0.0
        nominal_distance = 0.0
        
        for i in range(len(time_points) - 1):
            start_time = time_points[i]
            end_time = time_points[i + 1]
            speed = speeds[i]
            
            # 计算时间步长
            duration = end_time - start_time
            steps = int(duration / 0.2)
            
            for _ in range(steps):
                self.step(
                    speed=speed,
                    current_weight=current_weight,
                    grade_percent=grade_percent,
                    terrain_factor=terrain_factor,
                    stance=stance,
                    movement_type=movement_type,
                    current_time=current_time,
                    enable_randomness=enable_randomness
                )
                current_time += 0.2
                # Nominal distance: scenario-defined speed profile.
                nominal_distance += speed * 0.2

                # Effective speed: posture speed multiplier + encumbrance speed penalty (align with C).
                posture_speed_mult = 1.0
                if stance == Stance.CROUCH:
                    posture_speed_mult = getattr(self.constants, 'POSTURE_CROUCH_MULTIPLIER', 0.7)
                elif stance == Stance.PRONE:
                    posture_speed_mult = getattr(self.constants, 'POSTURE_PRONE_MULTIPLIER', 0.3)

                base_weight = getattr(self.constants, 'BASE_WEIGHT', 1.36)
                body_weight = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
                effective_weight = max(current_weight - base_weight, 0.0)
                body_mass_percent = (effective_weight / body_weight) if body_weight > 0.0 else 0.0
                coeff = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_COEFF', 0.20)
                speed_penalty = coeff * body_mass_percent
                speed_penalty = float(np.clip(speed_penalty, 0.0, 0.5))

                effective_speed = speed * posture_speed_mult * (1.0 - speed_penalty)
                total_distance += effective_speed * 0.2
        
        # 计算结果
        min_stamina = min(self.stamina_history) if self.stamina_history else 1.0

        # Derive a "time with penalty": if effective distance < nominal, scale time up.
        # This makes optimizer objectives sensitive to speed-related parameters.
        eps = 1e-6
        if nominal_distance > 0.0:
            ratio = nominal_distance / max(total_distance, eps)
        else:
            ratio = 1.0
        ratio = float(np.clip(ratio, 1.0, 5.0))
        total_time_with_penalty = current_time * ratio
        
        return {
            'total_distance': total_distance,
            'min_stamina': min_stamina,
            'stamina_history': self.stamina_history.copy(),  # 返回副本，避免线程间共享
            'speed_history': self.speed_history.copy(),      # 返回副本，避免线程间共享
            'time_history': self.time_history.copy(),        # 返回副本，避免线程间共享
            'total_time_with_penalty': total_time_with_penalty
        }
    
    def _calculate_base_drain_rate(self, speed, current_weight):
        """计算基础消耗率。委托给 C 端对齐的 _calculate_drain_rate_c_aligned（默认平地、站姿、按速度推断移动类型）。返回 total_drain。"""
        mt = MovementType.RUN
        if speed >= getattr(self.constants, 'SPRINT_VELOCITY_THRESHOLD', 5.2):
            mt = MovementType.SPRINT
        elif speed < getattr(self.constants, 'RUN_VELOCITY_THRESHOLD', 3.7):
            mt = MovementType.WALK if speed > 0.05 else MovementType.IDLE
        _, total = self._calculate_drain_rate_c_aligned(
            speed, current_weight, 0.0, 1.0, Stance.STAND, mt, wind_drag=0.0
        )
        return total
    
    def _calculate_recovery_weight(self, current_weight, stance):
        """计算恢复用的重量（考虑姿态优化）"""
        current_weight_for_recovery = current_weight
        
        # 趴下休息时的负重优化
        if stance == Stance.PRONE:
            # 如果角色趴下，将负重视为基准重量，去除额外负重的影响
            current_weight_for_recovery = self.constants.CHARACTER_WEIGHT
        
        return current_weight_for_recovery
    
    def _calculate_recovery_rate(self, stamina_percent, rest_duration_minutes, 
                                exercise_duration_minutes, current_weight, 
                                base_drain_rate, disable_positive_recovery, 
                                stance, environment_factor, current_speed):
        """计算恢复率（与C代码一致，性能优化版）"""
        # 内联恢复用重量计算，减少函数调用
        current_weight_for_recovery = current_weight
        if stance == Stance.PRONE:
            current_weight_for_recovery = self.constants.CHARACTER_WEIGHT
        
        # 内联多维度恢复率计算，减少函数调用开销
        # 限制输入参数
        stamina_percent_clamped = min(max(stamina_percent, 0.0), 1.0)
        rest_duration_minutes = max(rest_duration_minutes, 0.0)
        exercise_duration_minutes = max(exercise_duration_minutes, 0.0)
        
        # 基础恢复率
        base_recovery_rate = self.constants.BASE_RECOVERY_RATE
        
        # 非线性恢复系数（避免重复计算）
        recovery_nonlinear_coeff = self.constants.RECOVERY_NONLINEAR_COEFF
        stamina_recovery_multiplier = 1.0 + (recovery_nonlinear_coeff * (1.0 - stamina_percent_clamped))
        recovery_rate = base_recovery_rate * stamina_recovery_multiplier
        
        # 健康状态影响
        fitness_recovery_multiplier = 1.0 + (self.constants.FITNESS_RECOVERY_COEFF * self.constants.FITNESS_LEVEL)
        fitness_recovery_multiplier = min(max(fitness_recovery_multiplier, 1.0), 1.5)
        recovery_rate *= fitness_recovery_multiplier
        
        # 休息时间影响
        if rest_duration_minutes <= self.constants.FAST_RECOVERY_DURATION_MINUTES:
            recovery_rate *= self.constants.FAST_RECOVERY_MULTIPLIER
        elif rest_duration_minutes <= self.constants.FAST_RECOVERY_DURATION_MINUTES + self.constants.MEDIUM_RECOVERY_DURATION_MINUTES:
            recovery_rate *= self.constants.MEDIUM_RECOVERY_MULTIPLIER
        elif rest_duration_minutes >= 10.0:
            transition_duration = 10.0
            transition_progress = min((rest_duration_minutes - 10.0) / transition_duration, 1.0)
            slow_recovery_multiplier = 1.0 - (transition_progress * (1.0 - self.constants.SLOW_RECOVERY_MULTIPLIER))
            recovery_rate *= slow_recovery_multiplier
        
        # 姿态影响
        if stance == Stance.STAND:
            recovery_rate *= self.constants.STANDING_RECOVERY_MULTIPLIER
        elif stance == Stance.CROUCH:
            recovery_rate *= self.constants.CROUCHING_RECOVERY_MULTIPLIER
        elif stance == Stance.PRONE:
            recovery_rate *= self.constants.PRONE_RECOVERY_MULTIPLIER
        
        # 负重影响
        if current_weight_for_recovery > 0.0:
            load_ratio = current_weight_for_recovery / 30.0  # 30kg为身体耐受基准
            load_ratio = min(max(load_ratio, 0.0), 2.0)
            load_recovery_penalty = (load_ratio ** self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF
            recovery_rate -= load_recovery_penalty
        
        # 边际效应衰减（使用优化参数）
        threshold = getattr(self.constants, 'MARGINAL_DECAY_THRESHOLD', 0.8)
        coeff = getattr(self.constants, 'MARGINAL_DECAY_COEFF', 1.1)
        if stamina_percent_clamped > threshold:
            # 公式：multiplier = 1.0 - (stamina - threshold) * (coeff - 1.0) / (1.0 - threshold)
            excess = stamina_percent_clamped - threshold
            max_excess = 1.0 - threshold
            if max_excess > 0:
                decay_factor = (excess / max_excess) * (coeff - 1.0)
                marginal_decay_multiplier = max(1.0 - decay_factor, 0.2)
                recovery_rate *= marginal_decay_multiplier
        
        # 最小恢复阈值检查（使用优化参数）
        min_threshold = getattr(self.constants, 'MIN_RECOVERY_STAMINA_THRESHOLD', 0.2)
        min_rest_time = getattr(self.constants, 'MIN_RECOVERY_REST_TIME_SECONDS', 3.0) / 60.0
        if stamina_percent_clamped < min_threshold and rest_duration_minutes >= min_rest_time:
            # 体力低于阈值且休息时间足够时，确保最小恢复率
            min_recovery = getattr(self.constants, 'BASE_RECOVERY_RATE', 4e-4) * 0.5
            recovery_rate = max(recovery_rate, min_recovery)
        
        # 确保恢复率不为负
        recovery_rate = max(recovery_rate, 0.0)
        
        # ==================== 环境因子处理（使用优化参数）====================
        if environment_factor:
            # 热应激惩罚（使用优化参数）
            heat_coeff = getattr(self.constants, 'ENV_TEMPERATURE_HEAT_PENALTY_COEFF', 0.02)
            heat_stress_penalty = environment_factor.heat_stress * heat_coeff
            recovery_rate *= (1.0 - min(heat_stress_penalty, 0.5))
            
            # 冷应激惩罚（使用优化参数）
            cold_coeff = getattr(self.constants, 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF', 0.05)
            cold_stress_penalty = environment_factor.cold_stress * cold_coeff
            recovery_rate *= (1.0 - min(cold_stress_penalty, 0.5))
            
            # 地表湿度惩罚（趴下时的恢复惩罚）
            if stance == Stance.PRONE:
                wetness_max = getattr(self.constants, 'ENV_SURFACE_WETNESS_PENALTY_MAX', 0.15)
                surface_wetness_penalty = environment_factor.surface_wetness * wetness_max
                recovery_rate *= (1.0 - surface_wetness_penalty)
        
        # ==================== 绝境呼吸保护机制 ====================
        # 当体力极低 (<2%) 且非水下时，人体进入求生本能强制吸氧
        if stamina_percent < 0.02:
            recovery_rate = max(recovery_rate, 0.0001)
        
        # 关键兜底（仅在明确禁止恢复的场景启用，例如水中）：
        if disable_positive_recovery:
            return -max(base_drain_rate, 0.0)
        
        # 处理静态消耗或恢复
        if base_drain_rate > 0.0:
            # 正数表示消耗，从恢复率中减去
            adjusted_recovery_rate = recovery_rate - base_drain_rate
            
            # 如果消耗大于恢复，屏蔽消耗，只按最低值恢复
            if adjusted_recovery_rate < 0.0:
                recovery_rate = 0.00005
            else:
                recovery_rate = adjusted_recovery_rate
        elif base_drain_rate < 0.0:
            # 负数表示恢复，加到恢复率中
            recovery_rate += abs(base_drain_rate)
        
        # 静态保护逻辑
        if current_speed < 0.1 and current_weight < 40.0 and recovery_rate < 0.00005:
            recovery_rate = 0.0001
        
        return recovery_rate
    
    def _calculate_multi_dimensional_recovery_rate(self, stamina_percent, 
                                                 rest_duration_minutes, 
                                                 exercise_duration_minutes, 
                                                 current_weight, 
                                                 stance):
        """计算多维度恢复率（与C代码一致）"""
        # 限制输入参数
        stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
        rest_duration_minutes = max(rest_duration_minutes, 0.0)
        exercise_duration_minutes = max(exercise_duration_minutes, 0.0)
        
        # 基础恢复率
        base_recovery_rate = self.constants.BASE_RECOVERY_RATE
        
        # 非线性恢复系数
        recovery_nonlinear_coeff = self.constants.RECOVERY_NONLINEAR_COEFF
        stamina_recovery_multiplier = 1.0 + (recovery_nonlinear_coeff * (1.0 - stamina_percent))
        recovery_rate = base_recovery_rate * stamina_recovery_multiplier
        
        # 健康状态影响
        fitness_recovery_multiplier = 1.0 + (self.constants.FITNESS_RECOVERY_COEFF * self.constants.FITNESS_LEVEL)
        fitness_recovery_multiplier = np.clip(fitness_recovery_multiplier, 1.0, 1.5)
        recovery_rate *= fitness_recovery_multiplier
        
        # 休息时间影响
        if rest_duration_minutes <= self.constants.FAST_RECOVERY_DURATION_MINUTES:
            # 快速恢复期
            recovery_rate *= self.constants.FAST_RECOVERY_MULTIPLIER
        elif rest_duration_minutes <= self.constants.FAST_RECOVERY_DURATION_MINUTES + self.constants.MEDIUM_RECOVERY_DURATION_MINUTES:
            # 中等恢复期
            recovery_rate *= self.constants.MEDIUM_RECOVERY_MULTIPLIER
        elif rest_duration_minutes >= 10.0:
            # 慢速恢复期
            transition_duration = 10.0
            transition_progress = min((rest_duration_minutes - 10.0) / transition_duration, 1.0)
            slow_recovery_multiplier = 1.0 - (transition_progress * (1.0 - self.constants.SLOW_RECOVERY_MULTIPLIER))
            recovery_rate *= slow_recovery_multiplier
        
        # 姿态影响
        if stance == Stance.STAND:
            recovery_rate *= self.constants.STANDING_RECOVERY_MULTIPLIER
        elif stance == Stance.CROUCH:
            recovery_rate *= self.constants.CROUCHING_RECOVERY_MULTIPLIER
        elif stance == Stance.PRONE:
            recovery_rate *= self.constants.PRONE_RECOVERY_MULTIPLIER
        
        # 负重影响
        if current_weight > 0.0:
            load_ratio = current_weight / 30.0  # 30kg为身体耐受基准
            load_ratio = np.clip(load_ratio, 0.0, 2.0)
            load_recovery_penalty = np.power(load_ratio, self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF
            recovery_rate -= load_recovery_penalty
        
        # 边际效应衰减
        if stamina_percent > 0.8:
            marginal_decay_multiplier = 1.1 - stamina_percent
            marginal_decay_multiplier = np.clip(marginal_decay_multiplier, 0.2, 1.0)
            recovery_rate *= marginal_decay_multiplier
        
        # 确保恢复率不为负
        recovery_rate = max(recovery_rate, 0.0)
        
        return recovery_rate


# 测试代码
if __name__ == "__main__":
    # 创建常量实例
    constants = RSSConstants()
    
    # 测试1: ACFT 2英里测试（空载，标准条件）
    print("测试1: ACFT 2英里测试（空载，标准条件）...")
    twin = RSSDigitalTwin(constants)
    
    # ACFT标准测试：0KG负载，15分27秒完成2英里
    class ACFTScenario:
        def __init__(self):
            self.speed_profile = [(0, 3.7), (927, 3.7)]  # 2英里测试，目标速度3.7m/s
            self.current_weight = 90.0 + 0.0  # 体重90kg + 0kg负载
            self.grade_percent = 0.0  # 平地
            self.terrain_factor = 1.0  # 普通地形
            self.stance = Stance.STAND  # 站立
            self.movement_type = MovementType.RUN  # 跑步
    
    scenario = ACFTScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=True  # 启用随机扰动
    )
    
    print(f"ACFT 2英里测试 - 负载: 0.0kg")
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒")
    print(f"是否达到标准: {results['total_time_with_penalty'] <= 927.0}")
    
    # 测试2: 29kg负重下站立不动的情况（从50%体力开始）
    print("\n测试2: 29kg负重下站立不动的情况（从50%体力开始）...")
    twin = RSSDigitalTwin(constants)
    twin.stamina = 0.5  # 从50%体力开始
    
    # 模拟站立不动10秒
    for i in range(50):  # 50 * 0.2秒 = 10秒
        twin.step(
            speed=0.0,  # 站立不动
            current_weight=29.0,  # 29kg负重
            grade_percent=0.0,  # 平地
            terrain_factor=1.0,  # 普通地形
            stance=Stance.STAND,  # 站立
            movement_type=MovementType.IDLE,  # 静止
            current_time=i * 0.2,
            enable_randomness=True  # 启用随机扰动
        )
    
    # 打印结果
    print(f"初始体力: {twin.stamina_history[0]:.4f}")
    print(f"最终体力: {twin.stamina_history[-1]:.4f}")
    print(f"体力变化: {twin.stamina_history[-1] - twin.stamina_history[0]:.4f}")
    print(f"是否恢复体力: {twin.stamina_history[-1] > twin.stamina_history[0]}")
    
    # 测试3: 城市战斗场景（30kg负重）
    print("\n测试3: 城市战斗场景（30kg负重）...")
    twin = RSSDigitalTwin(constants)
    
    # 创建城市战斗场景
    class UrbanScenario:
        def __init__(self):
            self.speed_profile = [(0, 2.5), (60, 3.7), (120, 2.5), (180, 5.0), (210, 2.5), (300, 2.5)]
            self.current_weight = 90.0 + 30.0  # 体重90kg + 30kg负载
            self.grade_percent = 0.0  # 平地
            self.terrain_factor = 1.2  # 城市地形
            self.stance = Stance.STAND  # 站立
            self.movement_type = MovementType.RUN  # 跑步
    
    scenario = UrbanScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=True  # 启用随机扰动
    )
    
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒")
    print(f"是否完成场景: {results['total_time_with_penalty'] <= 300.0}")
    
    # 测试4: 山地战斗场景（25kg负重）
    print("\n测试4: 山地战斗场景（25kg负重）...")
    twin = RSSDigitalTwin(constants)
    
    # 创建山地战斗场景
    class MountainScenario:
        def __init__(self):
            self.speed_profile = [(0, 1.8), (120, 2.5), (240, 1.8), (360, 1.8)]
            self.current_weight = 90.0 + 25.0  # 体重90kg + 25kg负载
            self.grade_percent = 15.0  # 15%坡度
            self.terrain_factor = 1.5  # 山地地形
            self.stance = Stance.STAND  # 站立
            self.movement_type = MovementType.WALK  # 行走
    
    scenario = MountainScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=True  # 启用随机扰动
    )
    
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒")
    print(f"是否完成场景: {results['total_time_with_penalty'] <= 360.0}")
    
    # 测试5: 撤离场景（40kg重载）
    print("\n测试5: 撤离场景（40kg重载）...")
    twin = RSSDigitalTwin(constants)
    
    # 创建撤离场景
    class EvacuationScenario:
        def __init__(self):
            self.speed_profile = [(0, 2.5), (90, 3.2), (180, 2.5), (270, 2.5)]
            self.current_weight = 90.0 + 40.0  # 体重90kg + 40kg负载
            self.grade_percent = 5.0  # 5%坡度
            self.terrain_factor = 1.3  # 复杂地形
            self.stance = Stance.STAND  # 站立
            self.movement_type = MovementType.RUN  # 跑步
    
    scenario = EvacuationScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=True  # 启用随机扰动
    )
    
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒")
    print(f"是否完成场景: {results['total_time_with_penalty'] <= 270.0}")
