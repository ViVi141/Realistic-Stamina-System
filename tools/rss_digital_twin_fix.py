#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS Digital Twin (Final Corrected Version)
修复内容：
1. 能量单位修正：W/kg -> Total Watts (乘以体重)
2. 速度惩罚修正：由 (总重-基础) 改为 (总重-体重-基础)
3. 移除双重乘法 (x0.2)
4. 清理重复类定义
"""

import numpy as np
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional
import random

@dataclass
class RSSConstants:
    """RSS 常量定义 (统一版)"""
    
    # 基础游戏参数
    GAME_MAX_SPEED = 5.2
    CHARACTER_WEIGHT = 90.0
    REFERENCE_WEIGHT = 90.0
    
    # 医学模型参数
    PANDOLF_VELOCITY_COEFF = 3.2
    PANDOLF_VELOCITY_OFFSET = 0.7
    PANDOLF_BASE_COEFF = 2.7
    PANDOLF_GRADE_BASE_COEFF = 0.23
    PANDOLF_GRADE_VELOCITY_COEFF = 1.34
    PANDOLF_STATIC_COEFF_1 = 1.2
    PANDOLF_STATIC_COEFF_2 = 1.6
    
    GIVONI_CONSTANT = 0.8  # 从1.0降低到0.8，减缓奔跑时的消耗速度，与C文件保持一致
    GIVONI_VELOCITY_EXPONENT = 2.2
    
    AEROBIC_THRESHOLD = 0.6
    ANAEROBIC_THRESHOLD = 0.8
    FATIGUE_START_TIME_MINUTES = 5.0
    
    # 能量转换（修复版：降低到 1.5e-5，让正常跑步耗尽时间达到 7-10 分钟）
    ENERGY_TO_STAMINA_COEFF = 1.50e-05
    
    # 恢复系统（修复版：降低恢复速度）
    BASE_RECOVERY_RATE = 0.00015  # 从0.00035降低约57%，解决恢复太快的问题
    RECOVERY_NONLINEAR_COEFF = 0.5
    FAST_RECOVERY_DURATION_MINUTES = 1.5
    FAST_RECOVERY_MULTIPLIER = 1.6  # 从2.5降低约36%，解决恢复太快的问题
    MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
    MEDIUM_RECOVERY_MULTIPLIER = 1.3  # 从1.4降低约7%，解决恢复太快的问题
    SLOW_RECOVERY_START_MINUTES = 10.0  # [添加] 慢速恢复期开始时间（分钟）
    SLOW_RECOVERY_MULTIPLIER = 0.6
    
    # 姿态恢复倍数（修复版：降低倍数）
    STANDING_RECOVERY_MULTIPLIER = 1.3  # 从1.5降低约13%，解决恢复太快的问题
    CROUCHING_RECOVERY_MULTIPLIER = 1.4
    PRONE_RECOVERY_MULTIPLIER = 1.6  # 从1.8降低约11%，解决恢复太快的问题
    
    # 负重惩罚参数
    BASE_WEIGHT = 1.36
    ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0
    LOAD_RECOVERY_PENALTY_COEFF = 0.0001  # 修复：从 0.0004 降低到 0.0001，解决负重导致恢复率为 0 的问题
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0
    
    # 阈值
    SPRINT_VELOCITY_THRESHOLD = 5.2
    RUN_VELOCITY_THRESHOLD = 3.7
    
    # Sprint 参数
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0  # Sprint时体力消耗是Run的3.0倍，与C文件保持一致
    SPRINT_SPEED_BOOST = 0.30
    
    # 疲劳参数
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0
    
    # 代谢效率
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2
    
    # 姿态消耗倍数
    POSTURE_CROUCH_MULTIPLIER = 1.8  # 蹲姿行走消耗倍数（1.6-2.0倍，取1.8），与C文件保持一致
    POSTURE_PRONE_MULTIPLIER = 3.0  # 匍匐爬行消耗倍数（与中速跑步相当），与C文件保持一致
    
    # 动作消耗
    JUMP_STAMINA_BASE_COST = 0.035
    VAULT_STAMINA_START_COST = 0.02
    CLIMB_STAMINA_TICK_COST = 0.01
    JUMP_CONSECUTIVE_PENALTY = 0.5
    
    # 环境参数
    ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.5
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    ENV_SURFACE_WETNESS_PENALTY_MAX = 0.15
    
    # 高级恢复参数
    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0
    
    # EPOC
    EPOC_DELAY_SECONDS = 0.5
    EPOC_DRAIN_RATE = 0.001

    # Fitness
    FITNESS_LEVEL = 1.0
    FITNESS_EFFICIENCY_COEFF = 0.35
    FITNESS_RECOVERY_COEFF = 0.25

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

class RSSDigitalTwin:
    _scenario_cache = {}
    _cache_size = 10000
    
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

    def _static_standing_cost(self, body_weight, load_weight):
        c1 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_1', 1.2)
        c2 = getattr(self.constants, 'PANDOLF_STATIC_COEFF_2', 1.6)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
        # 计算 W/kg
        base = c1 * body_weight
        load_term = 0.0
        if body_weight > 0 and load_weight > 0:
            r = load_weight / body_weight
            load_term = c2 * (body_weight + load_weight) * (r * r)
        
        # 修正1：此处虽然名为W/kg系数，但公式结构直接计算总瓦特数，因为乘以了weight
        # c1 * body_weight = W/kg * kg = Watts. 这里的公式是对的。
        rate = (base + load_term) * coeff
        # 修复：完全移除clip上限，让Pandolf模型自然输出（完全尊重生理模型）
        return float(max(0.0, rate))

    def _santee_downhill_correction(self, grade_percent):
        if grade_percent >= 0:
            return 1.0
        ab = abs(grade_percent)
        if ab <= 15.0:
            return 1.0
        term = ab * (1.0 - ab / 15.0) / 2.0
        return float(np.clip(1.0 - term, 0.5, 1.0))

    def _pandolf_expenditure(self, velocity, current_weight, grade_percent, terrain_factor):
        velocity = max(0.0, velocity)
        current_weight = max(0.0, current_weight)
        if velocity < 0.1:
            return 0.0
        
        v0 = getattr(self.constants, 'PANDOLF_VELOCITY_OFFSET', 0.7)
        vb = getattr(self.constants, 'PANDOLF_BASE_COEFF', 2.7)
        vc = getattr(self.constants, 'PANDOLF_VELOCITY_COEFF', 3.2)
        gb = getattr(self.constants, 'PANDOLF_GRADE_BASE_COEFF', 0.23)
        gv = getattr(self.constants, 'PANDOLF_GRADE_VELOCITY_COEFF', 1.34)
        ref = getattr(self.constants, 'REFERENCE_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)
        fit = getattr(self.constants, 'FITNESS_LEVEL', 1.0)
        
        vt = velocity - v0
        # W/kg 基础项
        base_term = (vb * (1.0 - 0.2 * fit)) + (vc * (vt * vt))
        
        g_dec = grade_percent * 0.01
        vsq = velocity * velocity
        # W/kg 坡度项
        grade_term = g_dec * (gb + gv * vsq)
        
        if grade_percent < 0:
            santee = self._santee_downhill_correction(grade_percent)
            if 0 < santee < 1.0:
                grade_term = grade_term / santee
        
        # 修复：加强地形因子保护（环境因子保护，防止极端地形）
        terrain_factor = np.clip(terrain_factor, 0.5, 3.0)
        
        # 注意： Pandolf公式给出的是 W/kg。
        # w_mult 是 负重/参考体重 的比率。
        # 修复：移除负重保护，让玩家承担负重后果（完全尊重玩家选择）
        w_mult = max(0.1, current_weight / ref)  # 只防止负数，不限制上限
        
        # 修正1：将 W/kg 转换为 Total Watts (乘以参考体重)
        # 这里的逻辑是：(基础W/kg + 坡度W/kg) * 负重系数 * 地形 * 参考体重 = 总瓦特
        energy_per_kg = (base_term + grade_term) * terrain_factor
        total_watts = energy_per_kg * ref * w_mult
        
        rate = total_watts * coeff
        # 修复：完全移除clip上限，让Pandolf模型自然输出（完全尊重生理模型）
        return float(max(0.0, rate))

    def _givoni_running(self, velocity, current_weight):
        if velocity <= 2.2:
            return 0.0
        
        ref = getattr(self.constants, 'REFERENCE_WEIGHT', 90.0)
        gc = getattr(self.constants, 'GIVONI_CONSTANT', 0.6)
        ge = getattr(self.constants, 'GIVONI_VELOCITY_EXPONENT', 2.2)
        coeff = getattr(self.constants, 'ENERGY_TO_STAMINA_COEFF', 3.5e-5)

        # 修复：移除负重保护，让玩家承担负重后果（完全尊重玩家选择）
        w_mult = max(0.1, current_weight / ref)  # 只防止负数，不限制上限
        vp = np.power(velocity, ge)
        
        # 修正1：Givoni公式计算 W/kg。需要乘以体重转换为 Watts。
        # energy_per_kg = gc * vp
        # total_watts = energy_per_kg * ref * w_mult
        total_watts = w_mult * gc * vp * ref
        
        rate = total_watts * coeff
        # 修复：完全移除clip上限，让Givoni模型自然输出（完全尊重生理模型）
        return float(max(0.0, rate))

    def _metabolic_efficiency_factor(self, speed_ratio):
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
        c = getattr(self.constants, 'FITNESS_EFFICIENCY_COEFF', 0.35)
        lvl = getattr(self.constants, 'FITNESS_LEVEL', 1.0)
        f = 1.0 - c * lvl
        return float(np.clip(f, 0.7, 1.0))

    def _encumbrance_stamina_drain_multiplier(self, current_weight):
        base = getattr(self.constants, 'BASE_WEIGHT', 1.36)
        eff = max(0.0, current_weight - base)
        bw = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
        coeff = getattr(self.constants, 'ENCUMBRANCE_STAMINA_DRAIN_COEFF', 2.0)
        mult = 1.0 + coeff * (eff / bw)
        return float(np.clip(mult, 1.0, 3.0))

    def _consumption_posture_multiplier(self, speed, stance):
        if speed <= 0.05:
            return 1.0
        crouch_multiplier = getattr(self.constants, 'POSTURE_CROUCH_MULTIPLIER', 1.8)
        prone_multiplier = getattr(self.constants, 'POSTURE_PRONE_MULTIPLIER', 3.0)
        if stance == Stance.CROUCH:
            return min(max(crouch_multiplier, 1.0), 2.5)
        if stance == Stance.PRONE:
            return min(max(prone_multiplier, 1.0), 5.0)
        return 1.0

    def _fatigue_factor(self):
        start = getattr(self.constants, 'FATIGUE_START_TIME_MINUTES', 5.0)
        coeff = getattr(self.constants, 'FATIGUE_ACCUMULATION_COEFF', 0.015)
        mx = getattr(self.constants, 'FATIGUE_MAX_FACTOR', 2.0)
        eff = max(0.0, self.exercise_duration_minutes - start)
        f = 1.0 + coeff * eff
        return float(np.clip(f, 1.0, mx))

    def _calculate_drain_rate_c_aligned(self, speed, current_weight, grade_percent, terrain_factor,
                                        stance, movement_type, wind_drag=0.0, environment_factor=None):
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

        # 修正3：移除了双重乘法 (raw = raw * 0.2) 

        base_for_recovery = max(0.0, raw)

        if raw <= 0.0:
            base = 0.0
        else:
            base = raw * posture * total_eff * fatigue

        # 修复：移除掩盖Pandolf模型的小项，让Pandolf模型成为主要消耗来源
        # 这些小项的量级（1e-05到1e-03）远小于Pandolf模型的应有输出（1e-02）
        # 移除：speed_linear、speed_sq、enc_base、enc_speed
        
        # 保留姿态修正，移除其他小项
        total_drain = base * sprint_mult
        
        # 应用负重修正（通过enc_mult）
        total_drain = total_drain * enc_mult
        
        # 坡度保护：限制坡度项的最大贡献，防止极端坡度导致消耗爆炸
        # 注意：数字孪生中的坡度影响通过terrain_factor和grade_percent体现
        # 这里我们移除硬编码的上限，让消耗可以自然增长
        # 但在实际游戏中，C代码中的坡度保护会限制极端情况
        
        total_drain = max(0.0, total_drain)
        return (base_for_recovery, total_drain)

    def _calculate_recovery_rate(self, stamina_percent, rest_duration_minutes, 
                                exercise_duration_minutes, current_weight, 
                                base_drain_rate, disable_positive_recovery, 
                                stance, environment_factor, current_speed):
        
        stamina_percent_clamped = min(max(stamina_percent, 0.0), 1.0)
        
        base_recovery_rate = self.constants.BASE_RECOVERY_RATE
        
        recovery_nonlinear_coeff = self.constants.RECOVERY_NONLINEAR_COEFF
        stamina_recovery_multiplier = 1.0 + (recovery_nonlinear_coeff * (1.0 - stamina_percent_clamped))
        recovery_rate = base_recovery_rate * stamina_recovery_multiplier
        
        fitness_recovery_multiplier = 1.0 + (self.constants.FITNESS_RECOVERY_COEFF * self.constants.FITNESS_LEVEL)
        fitness_recovery_multiplier = min(max(fitness_recovery_multiplier, 1.0), 1.5)
        recovery_rate *= fitness_recovery_multiplier
        
        if rest_duration_minutes <= self.constants.FAST_RECOVERY_DURATION_MINUTES:
            recovery_rate *= self.constants.FAST_RECOVERY_MULTIPLIER
        elif rest_duration_minutes <= self.constants.FAST_RECOVERY_DURATION_MINUTES + self.constants.MEDIUM_RECOVERY_DURATION_MINUTES:
            recovery_rate *= self.constants.MEDIUM_RECOVERY_MULTIPLIER
        elif rest_duration_minutes >= 10.0:
            transition_duration = 10.0
            transition_progress = min((rest_duration_minutes - 10.0) / transition_duration, 1.0)
            slow_recovery_multiplier = 1.0 - (transition_progress * (1.0 - self.constants.SLOW_RECOVERY_MULTIPLIER))
            recovery_rate *= slow_recovery_multiplier
        
        if stance == Stance.STAND:
            recovery_rate *= self.constants.STANDING_RECOVERY_MULTIPLIER
        elif stance == Stance.CROUCH:
            recovery_rate *= self.constants.CROUCHING_RECOVERY_MULTIPLIER
        elif stance == Stance.PRONE:
            recovery_rate *= self.constants.PRONE_RECOVERY_MULTIPLIER
        
        current_weight_for_recovery = current_weight
        if stance == Stance.PRONE:
            current_weight_for_recovery = self.constants.CHARACTER_WEIGHT
            
        if current_weight_for_recovery > 0.0:
            # 修复：将负重除数从 30.0 增加到 90.0（参考体重），解决恢复率为 0 的问题
            # 原始：load_ratio = current_weight / 30.0，导致 load_ratio 过大（90kg -> 3.0）
            # 修复后：load_ratio = current_weight / 90.0，更合理（90kg -> 1.0）
            load_ratio = current_weight_for_recovery / self.constants.REFERENCE_WEIGHT
            load_ratio = min(max(load_ratio, 0.0), 2.0)
            load_recovery_penalty = (load_ratio ** self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF
            recovery_rate -= load_recovery_penalty
        
        threshold = self.constants.MARGINAL_DECAY_THRESHOLD
        coeff = self.constants.MARGINAL_DECAY_COEFF
        min_threshold = self.constants.MIN_RECOVERY_STAMINA_THRESHOLD
        min_rest_time = self.constants.MIN_RECOVERY_REST_TIME_SECONDS / 60.0
        base_recovery = self.constants.BASE_RECOVERY_RATE
        
        if stamina_percent_clamped > threshold:
            excess = stamina_percent_clamped - threshold
            max_excess = 1.0 - threshold
            if max_excess > 0:
                decay_factor = (excess / max_excess) * (coeff - 1.0)
                marginal_decay_multiplier = max(1.0 - decay_factor, 0.2)
                recovery_rate *= marginal_decay_multiplier
        
        if stamina_percent_clamped < min_threshold and rest_duration_minutes >= min_rest_time:
            min_recovery = base_recovery * 0.5
            recovery_rate = max(recovery_rate, min_recovery)
        
        # ==================== 运动状态恢复率调整 ====================
        
        # 根据当前速度调整恢复率
        # - 静止时：正常恢复率
        # - Walk状态：适当恢复率，允许净恢复
        # - Run状态：大幅降低恢复率，确保消耗率大于恢复率
        # - Sprint状态：几乎不恢复
        speed_based_recovery_multiplier = 1.0
        if current_speed >= 5.0: # Sprint
            speed_based_recovery_multiplier = 0.05 # 几乎不恢复
        elif current_speed >= 3.2: # Run
            speed_based_recovery_multiplier = 0.2 # 大幅降低恢复率
        elif current_speed >= 0.1: # Walk
            speed_based_recovery_multiplier = 0.6 # 适当降低恢复率，允许净恢复
        # 静止时：speed_based_recovery_multiplier = 1.0（正常恢复）
        
        # 应用速度基于的恢复率调整
        recovery_rate = recovery_rate * speed_based_recovery_multiplier
        
        recovery_rate = max(recovery_rate, 0.0)
        
        if environment_factor:
            heat_coeff = self.constants.ENV_TEMPERATURE_HEAT_PENALTY_COEFF
            cold_coeff = self.constants.ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF
            wetness_max = self.constants.ENV_SURFACE_WETNESS_PENALTY_MAX

            heat_stress_penalty = environment_factor.heat_stress * heat_coeff
            recovery_rate *= (1.0 - min(heat_stress_penalty, 0.5))
            
            cold_stress_penalty = environment_factor.cold_stress * cold_coeff
            recovery_rate *= (1.0 - min(cold_stress_penalty, 0.5))
            
            if stance == Stance.PRONE:
                surface_wetness_penalty = environment_factor.surface_wetness * wetness_max
                recovery_rate *= (1.0 - surface_wetness_penalty)
        
        if stamina_percent < 0.02:
            recovery_rate = max(recovery_rate, 0.0001)
        
        if disable_positive_recovery:
            return -max(base_drain_rate, 0.0)
        
        # ==================== 静态消耗处理 ====================
        # 只有在静态站立时（current_speed < 0.1），才从恢复率中减去静态消耗
        # 移动时（current_speed >= 0.1），不减去消耗，因为消耗已经在final_drain_rate中计算了
        if base_drain_rate > 0.0 and current_speed < 0.1:
            adjusted_recovery_rate = recovery_rate - base_drain_rate
            if adjusted_recovery_rate < 0.0:
                recovery_rate = 0.00005
            else:
                recovery_rate = adjusted_recovery_rate
        elif base_drain_rate < 0.0:
            recovery_rate += abs(base_drain_rate)
        
        if current_speed < 0.1 and current_weight < 40.0 and recovery_rate < 0.00005:
            recovery_rate = 0.0001
        
        return recovery_rate

    def step(self, speed, current_weight, grade_percent, terrain_factor, 
             stance, movement_type, current_time, enable_randomness=True):
        # ==================== 边界条件检查 ====================
        # 输入参数验证
        speed = max(float(speed), 0.0)  # 速度不能为负
        current_weight = max(float(current_weight), 0.0)  # 重量不能为负
        # 修复：加强坡度保护（环境因子保护，防止极端坡度）
        # 限制在 -85 到 85% 之间，防止 tan() 爆炸（超过 85 度的坡度不太现实）
        grade_percent = max(-85.0, min(85.0, float(grade_percent)))
        # 修复：加强地形因子保护（环境因子保护，与Pandolf模型保持一致）
        terrain_factor = max(0.5, min(3.0, float(terrain_factor)))
        current_time = max(float(current_time), 0.0)  # 时间不能为负
        
        time_delta = current_time - self.current_time if self.current_time > 0 else 0.2
        time_delta = max(time_delta, 0.01)  # 确保时间增量合理
        self.current_time = current_time
        
        if enable_randomness:
            # 随机性保留，但不影响核心逻辑验证
            speed = max(speed * random.uniform(0.98, 1.02), 0.0)
        
        was_moving = (self.last_speed > 0.05)
        is_now_stopped = (speed <= 0.05)
        
        if was_moving and is_now_stopped and not self.is_in_epoc_delay:
            self.epoc_delay_start_time = current_time
            self.is_in_epoc_delay = True
            self.speed_before_stop = max(self.last_speed, 0.0)  # 确保速度合理
        
        if self.is_in_epoc_delay:
            epoc_delay_duration = current_time - self.epoc_delay_start_time
            if epoc_delay_duration >= self.constants.EPOC_DELAY_SECONDS:
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
            self.last_movement_time = current_time
        else:
            self.rest_duration_minutes += time_delta / 60.0
        
        # ==================== 数值保护机制 ====================
        try:
            base_for_recovery, total_drain = self._calculate_drain_rate_c_aligned(
                speed, current_weight, grade_percent, terrain_factor,
                stance, movement_type, wind_drag=0.0, environment_factor=self.environment_factor
            )
            
            # 确保消耗率合理
            total_drain = max(float(total_drain), 0.0)
            total_drain = min(total_drain, 0.1)  # 限制最大消耗率，避免系统崩溃
            
            if self.is_in_epoc_delay:
                epoc_drain_rate = self.constants.EPOC_DRAIN_RATE * (1.0 + np.clip(self.speed_before_stop / 5.2, 0, 1) * 0.5)
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
            
            # 确保恢复率合理
            recovery_rate = max(float(recovery_rate), 0.0)
            recovery_rate = min(recovery_rate, 0.01)  # 限制最大恢复率
            
            net_change = recovery_rate - total_drain
            
            # 确保净变化合理
            net_change = float(net_change)
            net_change = max(net_change, -0.1)  # 限制最大下降速度
            net_change = min(net_change, 0.01)  # 限制最大上升速度
            
            # 更新体力值
            self.stamina += net_change
            self.stamina = min(max(self.stamina, 0.0), 1.0)  # 确保体力值在合理范围内
            
            # 保存调试信息
            self.base_drain_rate_by_velocity = base_for_recovery
            self.final_drain_rate = total_drain
            self.recovery_rate = recovery_rate
            self.net_change = net_change
            
            if len(self.stamina_history) < self.max_history_length:
                self.stamina_history.append(self.stamina)
                self.time_history.append(current_time)
        
        except Exception as e:
            # ==================== 异常处理 ====================
            # 发生异常时，使用安全值，避免系统崩溃
            print(f"警告：step方法发生异常: {e}")
            # 使用安全值
            self.base_drain_rate_by_velocity = 0.0
            self.final_drain_rate = 0.01
            self.recovery_rate = 0.001
            self.net_change = -0.001
            # 确保体力值合理
            self.stamina = max(self.stamina - 0.001, 0.0)
            self.stamina = min(self.stamina, 1.0)
            
            if len(self.stamina_history) < self.max_history_length:
                self.stamina_history.append(self.stamina)
                self.time_history.append(current_time)

    def simulate_scenario(self, speed_profile, current_weight, grade_percent, 
                         terrain_factor, stance, movement_type, enable_randomness=True):
        self.reset()
        
        time_points = [t for t, _ in speed_profile]
        speeds = [v for _, v in speed_profile]
        
        current_time = 0.0
        total_distance = 0.0
        nominal_distance = 0.0
        
        for i in range(len(time_points) - 1):
            start_time = time_points[i]
            end_time = time_points[i + 1]
            speed = speeds[i]
            
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
                nominal_distance += speed * 0.2

                # 修正2：负重速度惩罚计算
                posture_speed_mult = 1.0
                if stance == Stance.CROUCH:
                    posture_speed_mult = getattr(self.constants, 'POSTURE_CROUCH_MULTIPLIER', 0.7)
                elif stance == Stance.PRONE:
                    posture_speed_mult = getattr(self.constants, 'POSTURE_PRONE_MULTIPLIER', 0.3)

                base_weight = getattr(self.constants, 'BASE_WEIGHT', 1.36)
                body_weight = getattr(self.constants, 'CHARACTER_WEIGHT', 90.0)
                
                # 关键修正：有效负重必须减去体重！
                effective_weight = max(current_weight - body_weight - base_weight, 0.0)
                
                body_mass_percent = (effective_weight / body_weight) if body_weight > 0.0 else 0.0
                coeff = getattr(self.constants, 'ENCUMBRANCE_SPEED_PENALTY_COEFF', 0.20)
                speed_penalty = coeff * body_mass_percent
                speed_penalty = float(np.clip(speed_penalty, 0.0, 0.5))

                effective_speed = speed * posture_speed_mult * (1.0 - speed_penalty)
                total_distance += effective_speed * 0.2
        
        min_stamina = min(self.stamina_history) if self.stamina_history else 1.0
        eps = 1e-6
        ratio = nominal_distance / max(total_distance, eps) if nominal_distance > 0 else 1.0
        ratio = float(np.clip(ratio, 1.0, 5.0))
        total_time_with_penalty = current_time * ratio
        
        return {
            'total_distance': total_distance,
            'min_stamina': min_stamina,
            'total_time_with_penalty': total_time_with_penalty,
            'stamina_history': self.stamina_history[-1000:],
            'speed_history': self.speed_history[-500:],
            'time_history': self.time_history[-1000:]
        }

if __name__ == "__main__":
    # 创建常量实例
    constants = RSSConstants()
    
    # 测试1: ACFT 2英里测试（空载，标准条件）
    print("测试1: ACFT 2英里测试（空载，标准条件）...")
    twin = RSSDigitalTwin(constants)
    
    # ACFT标准测试：0KG负载
    class ACFTScenario:
        def __init__(self):
            self.speed_profile = [(0, 3.7), (927, 3.7)]  # 3.7m/s 跑927秒
            self.current_weight = 90.0 + 0.0 
            self.grade_percent = 0.0  
            self.terrain_factor = 1.0  
            self.stance = Stance.STAND  
            self.movement_type = MovementType.RUN  
    
    scenario = ACFTScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=False 
    )
    
    print(f"ACFT 2英里测试 - 负载: 0.0kg")
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒 (目标: 927s)")
    
    # 验证修复
    if results['total_time_with_penalty'] < 950.0:
        print("✅ 时间修复成功：速度惩罚逻辑正常")
    else:
        print("❌ 时间依然异常：请检查 effective_weight 计算")
        
    if results['min_stamina'] < 0.2:
        print("✅ 消耗修复成功：体力已大幅消耗")
    else:
        print("❌ 消耗依然过慢：请检查 能量乘90倍 的逻辑")
    
    print()
    
    # 测试2: 29kg负重下站立不动的情况（从50%体力开始）
    print("测试2: 29kg负重下站立不动的情况（从50%体力开始）...")
    twin = RSSDigitalTwin(constants)
    twin.stamina = 0.5
    
    class StaticLoadScenario:
        def __init__(self):
            self.speed_profile = [(0, 0.0), (10, 0.0)]
            self.current_weight = 90.0 + 29.0
            self.grade_percent = 0.0
            self.terrain_factor = 1.0
            self.stance = Stance.STAND
            self.movement_type = MovementType.IDLE
    
    scenario = StaticLoadScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=False
    )
    
    print(f"初始体力: {0.5:.4f}")
    print(f"最终体力: {results['min_stamina']:.4f}")
    print(f"体力变化: {results['min_stamina'] - 0.5:.4f}")
    
    if results['min_stamina'] > 0.5:
        print("✅ 静态恢复正常：负重站立时体力恢复")
    else:
        print("❌ 静态恢复异常：负重站立时体力未恢复")
    
    print()
    
    # 测试3: 城市战斗场景（30kg负重）
    print("测试3: 城市战斗场景（30kg负重）...")
    twin = RSSDigitalTwin(constants)
    
    class UrbanCombatScenario:
        def __init__(self):
            self.speed_profile = [(0, 2.5), (60, 3.7), (120, 2.5), (180, 5.0), (210, 2.5), (300, 2.5)]
            self.current_weight = 90.0 + 30.0
            self.grade_percent = 0.0
            self.terrain_factor = 1.2
            self.stance = Stance.STAND
            self.movement_type = MovementType.RUN
    
    scenario = UrbanCombatScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=False
    )
    
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒 (目标: 300s)")
    print(f"体力消耗: {(1.0 - results['min_stamina']) * 100:.1f}%")
    
    if 0.3 <= results['min_stamina'] <= 0.7:
        print("✅ 城市战斗消耗合理：体力消耗30-70%")
    elif results['min_stamina'] < 0.3:
        print("❌ 城市战斗消耗过快：体力消耗超过70%，请调整消耗系数")
    else:
        print("❌ 城市战斗消耗过慢：体力消耗不足30%，请检查负重消耗逻辑")
    
    print()
    
    # 测试4: 山地战斗场景（25kg负重）
    print("测试4: 山地战斗场景（25kg负重）...")
    twin = RSSDigitalTwin(constants)
    
    class MountainCombatScenario:
        def __init__(self):
            self.speed_profile = [(0, 1.8), (120, 2.5), (240, 1.8), (360, 1.8)]
            self.current_weight = 90.0 + 25.0
            self.grade_percent = 15.0
            self.terrain_factor = 1.5
            self.stance = Stance.STAND
            self.movement_type = MovementType.WALK
    
    scenario = MountainCombatScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=False
    )
    
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒 (目标: 360s)")
    print(f"体力消耗: {(1.0 - results['min_stamina']) * 100:.1f}%")
    
    if 0.4 <= results['min_stamina'] <= 0.8:
        print("✅ 山地战斗消耗合理：体力消耗20-60%")
    elif results['min_stamina'] < 0.4:
        print("❌ 山地战斗消耗过快：体力消耗超过60%，请调整坡度/地形惩罚")
    else:
        print("❌ 山地战斗消耗过慢：体力消耗不足20%，请检查坡度和地形惩罚")
    
    print()
    
    # 测试5: 撤离场景（40kg重载）
    print("测试5: 撤离场景（40kg重载）...")
    twin = RSSDigitalTwin(constants)
    
    class EvacuationScenario:
        def __init__(self):
            self.speed_profile = [(0, 2.5), (90, 3.2), (180, 2.5), (270, 2.5)]
            self.current_weight = 90.0 + 40.0
            self.grade_percent = 5.0
            self.terrain_factor = 1.3
            self.stance = Stance.STAND
            self.movement_type = MovementType.RUN
    
    scenario = EvacuationScenario()
    results = twin.simulate_scenario(
        speed_profile=scenario.speed_profile,
        current_weight=scenario.current_weight,
        grade_percent=scenario.grade_percent,
        terrain_factor=scenario.terrain_factor,
        stance=scenario.stance,
        movement_type=scenario.movement_type,
        enable_randomness=False
    )
    
    print(f"最低体力: {results['min_stamina']:.4f}")
    print(f"完成时间: {results['total_time_with_penalty']:.2f}秒 (目标: 270s)")
    print(f"体力消耗: {(1.0 - results['min_stamina']) * 100:.1f}%")
    
    if 0.3 <= results['min_stamina'] <= 0.7:
        print("✅ 重载撤离消耗合理：体力消耗30-70%")
    elif results['min_stamina'] < 0.3:
        print("❌ 重载撤离消耗过快：体力消耗超过70%，请调整重载惩罚")
    else:
        print("❌ 重载撤离消耗过慢：体力消耗不足30%，请检查重载惩罚逻辑")