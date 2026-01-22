"""
Realistic Stamina System (RSS) - Digital Twin Simulator
数字孪生仿真器：完全复刻 EnforceScript 的体力系统逻辑

核心功能：
1. 迁移所有体力消耗和恢复的数学公式
2. 支持参数化配置（用于优化）
3. 提供标准测试工况接口
"""

import numpy as np
from dataclasses import dataclass
from typing import Optional, Tuple, List
from enum import Enum


class MovementType(Enum):
    """移动类型枚举"""
    IDLE = 0
    WALK = 1
    RUN = 2
    SPRINT = 3


class Stance(Enum):
    """姿态枚举"""
    STAND = 0
    CROUCH = 1
    PRONE = 2


@dataclass
class RSSConstants:
    """体力系统常量（从 EnforceScript 迁移）"""
    
    # ==================== 游戏配置常量 ====================
    GAME_MAX_SPEED = 5.2  # m/s
    
    # ==================== 速度阈值 ====================
    SPRINT_VELOCITY_THRESHOLD = 5.2  # m/s
    RUN_VELOCITY_THRESHOLD = 3.7  # m/s
    WALK_VELOCITY_THRESHOLD = 3.2  # m/s
    RECOVERY_THRESHOLD_NO_LOAD = 2.5  # m/s
    DRAIN_THRESHOLD_COMBAT_LOAD = 1.5  # m/s
    COMBAT_LOAD_WEIGHT = 30.0  # kg
    MOVING_SPEED_THRESHOLD = 0.1  # m/s - Threshold for determining if moving
    WALKING_RUNNING_THRESHOLD = 2.2  # m/s - Threshold between walking and running
    EPOC_SPEED_THRESHOLD = 0.05  # m/s - Threshold for EPOC delay detection
    
    # ==================== 基础消耗率（pts/s）====================
    SPRINT_BASE_DRAIN_RATE = 0.480  # pts/s
    RUN_BASE_DRAIN_RATE = 0.075  # pts/s
    WALK_BASE_DRAIN_RATE = 0.060  # pts/s
    REST_RECOVERY_RATE = 0.250  # pts/s
    
    # ==================== 转换为每0.2秒的消耗率 ====================
    SPRINT_DRAIN_PER_TICK = SPRINT_BASE_DRAIN_RATE / 100.0 * 0.2
    RUN_DRAIN_PER_TICK = RUN_BASE_DRAIN_RATE / 100.0 * 0.2
    WALK_DRAIN_PER_TICK = WALK_BASE_DRAIN_RATE / 100.0 * 0.2
    REST_RECOVERY_PER_TICK = REST_RECOVERY_RATE / 100.0 * 0.2
    
    # ==================== 精疲力尽阈值 ====================
    EXHAUSTION_THRESHOLD = 0.0
    EXHAUSTION_LIMP_SPEED = 1.0  # m/s
    SPRINT_ENABLE_THRESHOLD = 0.20
    FORCED_REST_STAMINA_THRESHOLD = 0.05  # 5% - Trigger forced rest
    FORCED_REST_RELEASE_THRESHOLD = 0.20  # 20% - Release forced rest
    
    # ==================== 目标速度 ====================
    TARGET_RUN_SPEED = 3.7  # m/s
    TARGET_RUN_SPEED_MULTIPLIER = TARGET_RUN_SPEED / GAME_MAX_SPEED  # 0.7115
    
    # ==================== 双稳态-应激性能模型 ====================
    WILLPOWER_THRESHOLD = 0.25  # 25%
    SMOOTH_TRANSITION_START = 0.25  # 25%
    SMOOTH_TRANSITION_END = 0.05  # 5%
    MIN_LIMP_SPEED_MULTIPLIER = 1.0 / GAME_MAX_SPEED  # 0.1923
    
    # ==================== 医学模型参数 ====================
    STAMINA_EXPONENT = 0.6
    ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.20
    ENCUMBRANCE_SPEED_EXPONENT = 1.0
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0  # Increased from 1.5 to 2.0 (Amplify weight effect)
    MIN_SPEED_MULTIPLIER = 0.15
    MAX_SPEED_MULTIPLIER = 1.0
    
    # ==================== 角色特征 ====================
    CHARACTER_WEIGHT = 90.0  # kg
    CHARACTER_AGE = 22.0  # 岁
    FITNESS_LEVEL = 1.0
    FITNESS_EFFICIENCY_COEFF = 0.35
    FITNESS_RECOVERY_COEFF = 0.25
    
    # ==================== 多维度恢复模型 ====================
    BASE_RECOVERY_RATE = 5.00e-04  # Decreased from 5.00e-03 to 5.00e-04 (10x slower recovery)
    RECOVERY_NONLINEAR_COEFF = 0.5
    FAST_RECOVERY_DURATION_MINUTES = 1.5
    FAST_RECOVERY_MULTIPLIER = 3.5
    MEDIUM_RECOVERY_START_MINUTES = 1.5
    MEDIUM_RECOVERY_DURATION_MINUTES = 8.5
    MEDIUM_RECOVERY_MULTIPLIER = 1.8
    SLOW_RECOVERY_START_MINUTES = 10.0
    SLOW_RECOVERY_MULTIPLIER = 0.8
    AGE_RECOVERY_COEFF = 0.2
    AGE_REFERENCE = 30.0
    FATIGUE_RECOVERY_PENALTY = 0.05
    FATIGUE_RECOVERY_DURATION_MINUTES = 20.0
    
    # ==================== 姿态恢复加成 ====================
    STANDING_RECOVERY_MULTIPLIER = 2.0
    CROUCHING_RECOVERY_MULTIPLIER = 1.5
    PRONE_RECOVERY_MULTIPLIER = 2.2
    
    # ==================== 负重对恢复的惩罚 ====================
    LOAD_RECOVERY_PENALTY_COEFF = 0.0004
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0
    BODY_TOLERANCE_BASE = 30.0
    
    # ==================== 边际效应衰减 ====================
    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1
    
    # ==================== 最低体力阈值 ====================
    # [修复 v3.6.1] 将极度疲劳惩罚时间从 10秒 缩短为 3秒
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0
    
    # ==================== 累积疲劳 ====================
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_START_TIME_MINUTES = 5.0
    FATIGUE_MAX_FACTOR = 2.0
    
    # ==================== 代谢适应 ====================
    AEROBIC_THRESHOLD = 0.6
    ANAEROBIC_THRESHOLD = 0.8
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    MIXED_EFFICIENCY_FACTOR = 1.0
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2
    
    # ==================== 负重配置 ====================
    BASE_WEIGHT = 1.36  # kg
    MAX_ENCUMBRANCE_WEIGHT = 40.5  # kg
    COMBAT_ENCUMBRANCE_WEIGHT = 30.0  # kg
    MAX_LOAD_FOR_RECOVERY = 40.0  # kg - Maximum load weight for recovery
    
    # ==================== Sprint 相关 ====================
    SPRINT_SPEED_BOOST = 0.30
    SPRINT_MAX_SPEED_MULTIPLIER = 1.0
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0  # Increased from 2.0 to 3.0 (Make sprinting more draining)
    
    # ==================== 动作消耗参数 ====================
    JUMP_STAMINA_BASE_COST = 0.035
    VAULT_STAMINA_START_COST = 0.02
    CLIMB_STAMINA_TICK_COST = 0.01
    JUMP_CONSECUTIVE_PENALTY = 0.5
    
    # ==================== 坡度系统参数 ====================
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03
    
    # ==================== 游泳系统参数 ====================
    SWIMMING_BASE_POWER = 25.0
    SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0
    SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0
    SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0
    SWIMMING_ENERGY_TO_STAMINA_COEFF = 0.00005
    
    # ==================== 环境因子参数 ====================
    ENV_HEAT_STRESS_MAX_MULTIPLIER = 1.3
    ENV_RAIN_WEIGHT_MAX = 8.0
    ENV_WIND_RESISTANCE_COEFF = 0.05
    ENV_MUD_PENALTY_MAX = 0.4
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    
    # ==================== Pandolf 模型 ====================
    PANDOLF_BASE_COEFF = 1.5  # Restored physics baseline
    PANDOLF_VELOCITY_COEFF = 1.5  # Restored from 2.0 to 1.5 to reduce quadratic penalty of speed
    PANDOLF_VELOCITY_OFFSET = 0.7
    PANDOLF_GRADE_BASE_COEFF = 0.23
    PANDOLF_GRADE_VELOCITY_COEFF = 1.34
    PANDOLF_STATIC_COEFF_1 = 1.2
    PANDOLF_STATIC_COEFF_2 = 1.6
    ENERGY_TO_STAMINA_COEFF = 3.50e-05  # Increased from 1.25e-05 to 3.50e-05 (Make scenario harder)
    REFERENCE_WEIGHT = 90.0
    
    # ==================== Givoni-Goldman 模型 ====================
    GIVONI_CONSTANT = 0.15  # Increased from 0.1 to 0.15 (Make running costly again)
    GIVONI_VELOCITY_EXPONENT = 2.2
    
    # ==================== 地形系数 ====================
    TERRAIN_FACTOR_PAVED = 1.0
    TERRAIN_FACTOR_DIRT = 1.1
    TERRAIN_FACTOR_GRASS = 1.2
    TERRAIN_FACTOR_BRUSH = 1.5
    TERRAIN_FACTOR_SAND = 1.8
    
    # ==================== 姿态修正 ====================
    POSTURE_STAND_MULTIPLIER = 1.0
    POSTURE_CROUCH_MULTIPLIER = 1.8
    POSTURE_PRONE_MULTIPLIER = 3.0
    
    # ==================== 移动类型速度倍数 ====================
    WALK_SPEED_MULTIPLIER = 0.7
    WALK_MIN_SPEED_MULTIPLIER = 0.2
    WALK_MAX_SPEED_MULTIPLIER = 0.8
    SPRINT_MIN_SPEED_MULTIPLIER = 0.15
    
    # ==================== 恢复启动延迟 ====================
    RECOVERY_STARTUP_DELAY_SECONDS = 5.0
    EPOC_DELAY_SECONDS = 2.0
    EPOC_DRAIN_RATE = 0.001
    
    # ==================== 更新间隔 ====================
    UPDATE_INTERVAL = 0.2  # 秒
    
    # ==================== 最大消耗率限制 ====================
    # 移除硬编码上限，允许优化器调整参数
    MAX_DRAIN_RATE_PER_TICK = 1.0  # 设置为足够高的值以防止截断


class RSSDigitalTwin:
    """数字孪生仿真器核心类 - 严格面向对象设计"""
    
    def __init__(self, constants: Optional[RSSConstants] = None):
        """
        初始化数字孪生仿真器
        
        Args:
            constants: 常量配置（如果为 None，使用默认值）
        """
        self.constants = constants if constants else RSSConstants()
        
        # 状态变量
        self.stamina = 1.0  # 当前体力（0.0-1.0）
        self.exercise_duration = 0.0  # 运动持续时间（秒）
        self.rest_duration = 0.0  # 休息持续时间（秒）
        self.fatigue_factor = 1.0  # 疲劳因子
        self.is_in_epoc_delay = False  # 是否处于 EPOC 延迟期
        self.epoc_delay_start_time = 0.0  # EPOC 延迟开始时间
        self.last_speed = 0.0  # 上一帧速度
        self.speed_before_stop = 0.0  # 停止前的速度
        
        # 历史记录
        self.stamina_history = []  # 体力历史
        self.speed_history = []  # 速度历史
        self.time_history = []  # 时间历史
    
    def reset(self):
        """重置仿真器状态"""
        self.stamina = 1.0
        self.exercise_duration = 0.0
        self.rest_duration = 0.0
        self.fatigue_factor = 1.0
        self.is_in_epoc_delay = False
        self.epoc_delay_start_time = 0.0
        self.last_speed = 0.0
        self.speed_before_stop = 0.0
        self.stamina_history = []
        self.speed_history = []
        self.time_history = []
    
    def calculate_pandolf_energy_expenditure(
        self,
        speed: float,
        weight: float,
        grade_percent: float,
        terrain_factor: float = 1.0,
        use_santee_correction: bool = True
    ) -> float:
        """
        计算 Pandolf 能量消耗率（W/kg）
        
        公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²)) · η
        
        Args:
            speed: 速度（m/s）
            weight: 总重量（kg）
            grade_percent: 坡度百分比（正数=上坡，负数=下坡）
            terrain_factor: 地形系数
            use_santee_correction: 是否使用 Santee 下坡修正
        
        Returns:
            能量消耗率（W/kg）
        """
        # 基础项
        base_term = self.constants.PANDOLF_BASE_COEFF
        
        # 速度项
        velocity_term = self.constants.PANDOLF_VELOCITY_COEFF * \
                      ((speed - self.constants.PANDOLF_VELOCITY_OFFSET) ** 2)
        
        # 坡度项
        grade_term = grade_percent * (self.constants.PANDOLF_GRADE_BASE_COEFF +
                                   self.constants.PANDOLF_GRADE_VELOCITY_COEFF * (speed ** 2))
        
        # 完整 Pandolf 公式
        energy_per_kg = base_term + velocity_term + grade_term
        
        # 应用地形系数
        energy_per_kg *= terrain_factor
        
        # Santee 下坡修正（仅在下坡时）
        if use_santee_correction and grade_percent < 0 and speed <= 2.2:
            # μ = 1.0 - [G·(1 - G/15)/2]
            correction = 1.0 - (grade_percent * (1.0 - grade_percent / 15.0) / 2.0)
            energy_per_kg *= correction
        
        # 转换为总能量消耗（W）
        energy = energy_per_kg * weight
        
        return energy
    
    def calculate_givoni_goldman_running(
        self,
        speed: float,
        weight: float,
        terrain_factor: float = 1.0
    ) -> float:
        """
        计算 Givoni-Goldman 跑步能量消耗率（W/kg）
        
        公式：E = G × V^2.2 × M × η
        
        Args:
            speed: 速度（m/s）
            weight: 总重量（kg）
            terrain_factor: 地形系数
        
        Returns:
            能量消耗率（W）
        """
        energy = (self.constants.GIVONI_CONSTANT *
                 (speed ** self.constants.GIVONI_VELOCITY_EXPONENT) *
                 weight *
                 terrain_factor)
        return energy
    
    def calculate_static_standing_cost(
        self,
        body_weight: float,
        load_weight: float
    ) -> float:
        """
        计算静态站立消耗（W/kg）
        
        公式：E = M·(1.5 + 2.0·L/M)
        
        Args:
            body_weight: 身体重量（kg）
            load_weight: 负重（kg）
        
        Returns:
            能量消耗率（W）
        """
        total_weight = body_weight + load_weight
        energy = total_weight * (self.constants.PANDOLF_STATIC_COEFF_1 +
                               self.constants.PANDOLF_STATIC_COEFF_2 * load_weight / total_weight)
        return energy
    
    def calculate_stamina_consumption(
        self,
        speed: float,
        current_weight: float,
        grade_percent: float = 0.0,
        terrain_factor: float = 1.0,
        posture_multiplier: float = 1.0,
        total_efficiency_factor: float = 1.0,
        fatigue_factor: float = 1.0,
        sprint_multiplier: float = 1.0,
        encumbrance_stamina_drain_multiplier: float = 1.5
    ) -> Tuple[float, float]:
        """
        计算体力消耗率（每0.2秒）
        
        Args:
            speed: 当前速度（m/s）
            current_weight: 当前重量（kg）
            grade_percent: 坡度百分比
            terrain_factor: 地形系数
            posture_multiplier: 姿态修正倍数
            total_efficiency_factor: 综合效率因子
            fatigue_factor: 疲劳因子
            sprint_multiplier: Sprint消耗倍数
            encumbrance_stamina_drain_multiplier: 负重体力消耗倍数
        
        Returns:
            (total_drain_rate, base_drain_rate_by_velocity)
        """
        # 静态站立消耗
        if speed < self.constants.MOVING_SPEED_THRESHOLD:
            body_weight = self.constants.CHARACTER_WEIGHT
            load_weight = max(current_weight - body_weight, 0.0)
            static_drain_rate = self.calculate_static_standing_cost(body_weight, load_weight)
            base_drain_rate_by_velocity = static_drain_rate * self.constants.UPDATE_INTERVAL  # 转换为每0.2秒
        # 跑步模型（速度 > 2.2 m/s）
        elif speed > self.constants.WALKING_RUNNING_THRESHOLD:
            energy_expenditure = self.calculate_givoni_goldman_running(
                speed, current_weight, terrain_factor
            )
            base_drain_rate_by_velocity = energy_expenditure * self.constants.ENERGY_TO_STAMINA_COEFF * self.constants.UPDATE_INTERVAL
        # 步行模型（速度 <= 2.2 m/s）
        else:
            energy_expenditure = self.calculate_pandolf_energy_expenditure(
                speed, current_weight, grade_percent, terrain_factor, True
            )
            base_drain_rate_by_velocity = energy_expenditure * self.constants.ENERGY_TO_STAMINA_COEFF * self.constants.UPDATE_INTERVAL
        
        # 保存原始基础消耗率
        original_base_drain_rate = base_drain_rate_by_velocity
        
        # 应用姿态修正
        if base_drain_rate_by_velocity > 0.0:
            base_drain_rate_by_velocity *= posture_multiplier
        
        # 应用多维度修正因子
        if base_drain_rate_by_velocity < 0.0:
            base_drain_rate = base_drain_rate_by_velocity
        else:
            base_drain_rate = base_drain_rate_by_velocity * total_efficiency_factor * fatigue_factor
        
        # 速度相关项 - 使用基础消耗率作为参考（不是恢复率）
        speed_ratio = np.clip(speed / self.constants.GAME_MAX_SPEED, 0.0, 1.0)
        base_drain_ref = self.constants.RUN_DRAIN_PER_TICK
        speed_linear_drain_rate = base_drain_ref * speed_ratio * total_efficiency_factor * fatigue_factor
        speed_squared_drain_rate = base_drain_ref * (speed_ratio ** 2) * total_efficiency_factor * fatigue_factor
        
        # 负重相关消耗
        encumbrance_base_drain_rate = base_drain_ref * 10 * (encumbrance_stamina_drain_multiplier - 1.0)
        encumbrance_speed_drain_rate = base_drain_ref * 2 * (encumbrance_stamina_drain_multiplier - 1.0) * (speed_ratio ** 2)
        
        # 综合体力消耗率
        base_drain_components = (base_drain_rate + speed_linear_drain_rate +
                               speed_squared_drain_rate + encumbrance_base_drain_rate +
                               encumbrance_speed_drain_rate)
        
        # 应用 Sprint 倍数
        total_drain_rate = base_drain_components * sprint_multiplier
        
        # 生理上限 - 使用常量而不是硬编码值
        total_drain_rate = min(total_drain_rate, self.constants.MAX_DRAIN_RATE_PER_TICK)
        
        return total_drain_rate, original_base_drain_rate
    
    def calculate_metabolic_efficiency_factor(self, speed_ratio: float) -> float:
        """
        计算代谢适应效率因子
        
        Args:
            speed_ratio: 速度比率（0.0-1.0）
        
        Returns:
            代谢适应效率因子
        """
        if speed_ratio < self.constants.AEROBIC_THRESHOLD:
            return self.constants.AEROBIC_EFFICIENCY_FACTOR
        elif speed_ratio < self.constants.ANAEROBIC_THRESHOLD:
            t = (speed_ratio - self.constants.AEROBIC_THRESHOLD) / \
                  (self.constants.ANAEROBIC_THRESHOLD - self.constants.AEROBIC_THRESHOLD)
            return (self.constants.AEROBIC_EFFICIENCY_FACTOR +
                   t * (self.constants.ANAEROBIC_EFFICIENCY_FACTOR -
                         self.constants.AEROBIC_EFFICIENCY_FACTOR))
        else:
            return self.constants.ANAEROBIC_EFFICIENCY_FACTOR
    
    def calculate_fitness_efficiency_factor(self) -> float:
        """
        计算健康状态效率因子
        
        Returns:
            健康状态效率因子
        """
        fitness_efficiency_factor = 1.0 - (self.constants.FITNESS_EFFICIENCY_COEFF *
                                          self.constants.FITNESS_LEVEL)
        return np.clip(fitness_efficiency_factor, 0.7, 1.0)
    
    def calculate_multi_dimensional_recovery_rate(
        self,
        stamina_percent: float,
        rest_duration_minutes: float,
        exercise_duration_minutes: float,
        current_weight: float,
        stance: Stance = Stance.STAND
    ) -> float:
        """
        计算多维度恢复率（每0.2秒）
        
        Args:
            stamina_percent: 当前体力百分比（0.0-1.0）
            rest_duration_minutes: 休息持续时间（分钟）
            exercise_duration_minutes: 运动持续时间（分钟）
            current_weight: 当前重量（kg）
            stance: 当前姿态
        
        Returns:
            恢复率（每0.2秒）
        """
        # 基础恢复率
        base_recovery_rate = self.constants.BASE_RECOVERY_RATE
        
        # 恢复率非线性系数
        recovery_nonlinear = self.constants.RECOVERY_NONLINEAR_COEFF
        stamina_nonlinear = stamina_percent ** recovery_nonlinear
        recovery_rate = base_recovery_rate * stamina_nonlinear
        
        # 快速恢复期
        if rest_duration_minutes < self.constants.FAST_RECOVERY_DURATION_MINUTES:
            recovery_rate *= self.constants.FAST_RECOVERY_MULTIPLIER
        # 中等恢复期
        elif rest_duration_minutes < (self.constants.MEDIUM_RECOVERY_START_MINUTES +
                                    self.constants.MEDIUM_RECOVERY_DURATION_MINUTES):
            recovery_rate *= self.constants.MEDIUM_RECOVERY_MULTIPLIER
        # 慢速恢复期
        elif rest_duration_minutes >= self.constants.SLOW_RECOVERY_START_MINUTES:
            recovery_rate *= self.constants.SLOW_RECOVERY_MULTIPLIER
        
        # 年龄修正
        age_factor = 1.0 + self.constants.AGE_RECOVERY_COEFF * \
                   (self.constants.CHARACTER_AGE - self.constants.AGE_REFERENCE) / 10.0
        recovery_rate /= age_factor
        
        # 疲劳恢复惩罚
        fatigue_penalty = self.fatigue_factor - 1.0
        recovery_rate *= (1.0 - fatigue_penalty * self.constants.FATIGUE_RECOVERY_PENALTY)
        
        # 姿态恢复加成
        if stance == Stance.STAND:
            recovery_rate *= self.constants.STANDING_RECOVERY_MULTIPLIER
        elif stance == Stance.CROUCH:
            recovery_rate *= self.constants.CROUCHING_RECOVERY_MULTIPLIER
        elif stance == Stance.PRONE:
            recovery_rate *= self.constants.PRONE_RECOVERY_MULTIPLIER
        
        # 负重对恢复的惩罚
        recovery_weight = current_weight
        if stance == Stance.PRONE:
            recovery_weight = self.constants.CHARACTER_WEIGHT
        
        load_penalty = ((recovery_weight / self.constants.BODY_TOLERANCE_BASE) ** 2) * \
                       self.constants.LOAD_RECOVERY_PENALTY_COEFF
        recovery_rate -= load_penalty
        
        # 边际效应衰减
        if stamina_percent > self.constants.MARGINAL_DECAY_THRESHOLD:
            recovery_rate *= (self.constants.MARGINAL_DECAY_COEFF - stamina_percent)
        
        # 最低体力阈值限制
        if stamina_percent < self.constants.MIN_RECOVERY_STAMINA_THRESHOLD:
            if rest_duration_minutes * 60 < self.constants.MIN_RECOVERY_REST_TIME_SECONDS:
                recovery_rate = 0.0
        
        return recovery_rate
    
    def update_epoc_delay(self, current_speed: float, current_time: float) -> bool:
        """
        更新 EPOC 延迟状态
        
        Args:
            current_speed: 当前速度（m/s）
            current_time: 当前时间（秒）
        
        Returns:
            是否处于 EPOC 延迟期
        """
        was_moving = (self.last_speed > self.constants.EPOC_SPEED_THRESHOLD)
        is_now_stopped = (current_speed <= self.constants.EPOC_SPEED_THRESHOLD)
        
        # 从运动状态变为静止状态，启动 EPOC 延迟
        if was_moving and is_now_stopped and not self.is_in_epoc_delay:
            self.epoc_delay_start_time = current_time
            self.is_in_epoc_delay = True
            self.speed_before_stop = self.last_speed
        
        # 检查 EPOC 延迟是否已结束
        if self.is_in_epoc_delay:
            epoc_delay_duration = current_time - self.epoc_delay_start_time
            if epoc_delay_duration >= self.constants.EPOC_DELAY_SECONDS:
                self.is_in_epoc_delay = False
                self.epoc_delay_start_time = -1.0
        
        # 如果重新开始运动，取消 EPOC 延迟
        if current_speed > self.constants.EPOC_SPEED_THRESHOLD:
            self.is_in_epoc_delay = False
            self.epoc_delay_start_time = -1.0
        
        # 更新上一帧的速度
        self.last_speed = current_speed
        
        return self.is_in_epoc_delay
    
    def update_fatigue_factor(self, is_moving: bool):
        """
        更新疲劳因子
        
        Args:
            is_moving: 是否在运动
        """
        if is_moving:
            # 运动时累积疲劳
            exercise_minutes = self.exercise_duration / 60.0
            if exercise_minutes > self.constants.FATIGUE_START_TIME_MINUTES:
                excess_time = exercise_minutes - self.constants.FATIGUE_START_TIME_MINUTES
                self.fatigue_factor = 1.0 + \
                    self.constants.FATIGUE_ACCUMULATION_COEFF * excess_time
                self.fatigue_factor = min(self.fatigue_factor, self.constants.FATIGUE_MAX_FACTOR)
        else:
            # 休息时恢复疲劳
            rest_minutes = self.rest_duration / 60.0
            fatigue_recovery_rate = self.constants.FATIGUE_ACCUMULATION_COEFF * 2.0
            excess_fatigue = self.fatigue_factor - 1.0
            recovered_fatigue = excess_fatigue * fatigue_recovery_rate * rest_minutes
            self.fatigue_factor = max(1.0, self.fatigue_factor - recovered_fatigue)
    
    def calculate_speed_multiplier_by_stamina(
        self,
        stamina_percent: float,
        current_weight: float,
        movement_type: MovementType = MovementType.RUN
    ) -> float:
        """
        根据体力百分比、负重和移动类型计算速度倍数（双稳态-应激性能模型）
        
        速度性能分段（Performance Plateau）：
        1. 平台期（Willpower Zone, Stamina 25% - 100%）：
           只要体力高于25%，士兵可以强行维持设定的目标速度（3.7 m/s）。
           这模拟了士兵在比赛或战斗中通过意志力克服早期疲劳。
        2. 衰减期（Failure Zone, Stamina 0% - 25%）：
           只有当体力掉入最后25%时，生理机能开始真正崩塌，速度平滑下降到跛行。
           使用SmoothStep函数实现25%-5%的平滑过渡缓冲区。
        
        Args:
            stamina_percent: 当前体力百分比 (0.0-1.0)
            current_weight: 当前重量（kg）
            movement_type: 移动类型
        
        Returns:
            速度倍数（相对于游戏最大速度）
        """
        stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
        
        # 计算Run速度（使用双稳态模型，带平滑过渡）
        run_speed_multiplier = 0.0
        if stamina_percent >= self.constants.SMOOTH_TRANSITION_START:
            # 意志力平台期（25%-100%）：保持恒定目标速度（3.7 m/s）
            run_speed_multiplier = self.constants.TARGET_RUN_SPEED_MULTIPLIER
        elif stamina_percent >= self.constants.SMOOTH_TRANSITION_END:
            # 平滑过渡期（5%-25%）：使用SmoothStep建立缓冲区，避免突兀的"撞墙"效果
            t = (stamina_percent - self.constants.SMOOTH_TRANSITION_END) / \
                  (self.constants.SMOOTH_TRANSITION_START - self.constants.SMOOTH_TRANSITION_END)
            t = np.clip(t, 0.0, 1.0)
            smooth_factor = t * t * (3.0 - 2.0 * t)  # smoothstep函数
            
            # 在目标速度和跛行速度之间平滑过渡
            run_speed_multiplier = (self.constants.MIN_LIMP_SPEED_MULTIPLIER + 
                                   (self.constants.TARGET_RUN_SPEED_MULTIPLIER - self.constants.MIN_LIMP_SPEED_MULTIPLIER) * smooth_factor)
        else:
            # 生理崩溃期（0%-5%）：速度快速线性下降到跛行速度
            collapse_factor = stamina_percent / self.constants.SMOOTH_TRANSITION_END
            run_speed_multiplier = self.constants.MIN_LIMP_SPEED_MULTIPLIER * collapse_factor
            # 确保不会低于最小速度
            run_speed_multiplier = max(run_speed_multiplier, self.constants.MIN_LIMP_SPEED_MULTIPLIER * 0.8)
        
        # 计算有效负重（减去基准重量）
        effective_weight = max(current_weight - self.constants.CHARACTER_WEIGHT, 0.0)
        
        # 计算有效负重占体重的百分比（Body Mass Percentage）
        body_mass_percent = effective_weight / self.constants.CHARACTER_WEIGHT
        
        # 应用真实医学模型：速度惩罚 = β * (负重/体重)
        # β = 0.20 表示40%体重负重时，速度下降20%（符合文献）
        encumbrance_penalty = self.constants.ENCUMBRANCE_SPEED_PENALTY_COEFF * body_mass_percent
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)  # 最多减少50%速度
        
        # 应用负重惩罚（直接从速度倍数中减去）
        run_speed_multiplier = run_speed_multiplier * (1.0 - encumbrance_penalty)
        
        # 根据移动类型调整速度
        if movement_type == MovementType.IDLE:
            final_speed_multiplier = 0.0
        elif movement_type == MovementType.WALK:
            final_speed_multiplier = run_speed_multiplier * self.constants.WALK_SPEED_MULTIPLIER
            final_speed_multiplier = np.clip(final_speed_multiplier, self.constants.WALK_MIN_SPEED_MULTIPLIER, self.constants.WALK_MAX_SPEED_MULTIPLIER)
        elif movement_type == MovementType.SPRINT:
            # Sprint速度 = Run速度 × (1 + 30%)，完全继承Run的双稳态-平台期逻辑
            sprint_multiplier = 1.0 + self.constants.SPRINT_SPEED_BOOST  # 1.30
            final_speed_multiplier = run_speed_multiplier * sprint_multiplier
            final_speed_multiplier = np.clip(final_speed_multiplier, self.constants.SPRINT_MIN_SPEED_MULTIPLIER, self.constants.SPRINT_MAX_SPEED_MULTIPLIER)
        else:  # MovementType.RUN
            final_speed_multiplier = run_speed_multiplier
            final_speed_multiplier = np.clip(final_speed_multiplier, 
                                          self.constants.MIN_SPEED_MULTIPLIER, 
                                          self.constants.MAX_SPEED_MULTIPLIER)
        
        return final_speed_multiplier
    
    def step(
        self,
        speed: float,
        current_weight: float,
        grade_percent: float = 0.0,
        terrain_factor: float = 1.0,
        stance: Stance = Stance.STAND,
        movement_type: MovementType = MovementType.RUN,
        current_time: float = 0.0
    ) -> Tuple[float, float]:
        """
        执行一个仿真步（0.2秒）
        
        Args:
            speed: 当前速度（m/s）
            current_weight: 当前重量（kg）
            grade_percent: 坡度百分比
            terrain_factor: 地形系数
            stance: 当前姿态
            movement_type: 移动类型
            current_time: 当前时间（秒）
        
        Returns:
            (new_stamina, drain_rate)
        """
        # 判断是否在运动
        is_moving = (speed > self.constants.MOVING_SPEED_THRESHOLD)
        
        # 更新运动/休息时间
        if is_moving:
            self.exercise_duration += self.constants.UPDATE_INTERVAL
            self.rest_duration = 0.0
        else:
            self.rest_duration += self.constants.UPDATE_INTERVAL
        
        # 更新疲劳因子
        self.update_fatigue_factor(is_moving)
        
        # 更新 EPOC 延迟
        is_in_epoc_delay = self.update_epoc_delay(speed, current_time)
        
        # 计算综合效率因子
        speed_ratio = np.clip(speed / self.constants.GAME_MAX_SPEED, 0.0, 1.0)
        metabolic_efficiency_factor = self.calculate_metabolic_efficiency_factor(speed_ratio)
        fitness_efficiency_factor = self.calculate_fitness_efficiency_factor()
        total_efficiency_factor = metabolic_efficiency_factor * fitness_efficiency_factor
        
        # 计算姿态修正倍数
        if is_moving:
            if stance == Stance.CROUCH:
                posture_multiplier = self.constants.POSTURE_CROUCH_MULTIPLIER
            elif stance == Stance.PRONE:
                posture_multiplier = self.constants.POSTURE_PRONE_MULTIPLIER
            else:
                posture_multiplier = self.constants.POSTURE_STAND_MULTIPLIER
        else:
            posture_multiplier = 1.0
        
        # 计算 Sprint 倍数
        if movement_type == MovementType.SPRINT:
            sprint_multiplier = self.constants.SPRINT_STAMINA_DRAIN_MULTIPLIER
        else:
            sprint_multiplier = 1.0
        
        # 计算负重体力消耗倍数
        encumbrance_stamina_drain_multiplier = self.constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF
        
        # 计算体力消耗率
        drain_rate, base_drain_rate_by_velocity = self.calculate_stamina_consumption(
            speed, current_weight, grade_percent, terrain_factor,
            posture_multiplier, total_efficiency_factor,
            self.fatigue_factor, sprint_multiplier,
            encumbrance_stamina_drain_multiplier
        )
        
        # 如果在 EPOC 延迟期，应用 EPOC 消耗
        if is_in_epoc_delay and not is_moving:
            speed_ratio_for_epoc = np.clip(self.speed_before_stop / self.constants.GAME_MAX_SPEED, 0.0, 1.0)
            epoc_drain_rate = self.constants.EPOC_DRAIN_RATE * (1.0 + speed_ratio_for_epoc * 0.5)
            drain_rate = epoc_drain_rate
        
        # 计算恢复率
        if not is_moving and not is_in_epoc_delay:
            rest_duration_minutes = self.rest_duration / 60.0
            exercise_duration_minutes = self.exercise_duration / 60.0
            recovery_rate = self.calculate_multi_dimensional_recovery_rate(
                self.stamina, rest_duration_minutes, exercise_duration_minutes,
                current_weight, stance
            )
            
            # ==================== 静态保护逻辑 ====================
            # 确保除非玩家严重超载(>40kg负重)，否则静止时总是缓慢恢复体力
            load_weight = max(current_weight - self.constants.CHARACTER_WEIGHT, 0.0)
            if not is_moving and recovery_rate < 0 and load_weight < self.constants.MAX_LOAD_FOR_RECOVERY:
                # 强制设置为一个小的正值（每0.2秒恢复0.005%，每秒恢复0.025%）
                recovery_rate = 0.00005
            
            drain_rate = -recovery_rate
        
        # 更新体力
        self.stamina = np.clip(self.stamina - drain_rate, 0.0, 1.0)
        
        # 记录历史
        self.stamina_history.append(self.stamina)
        self.speed_history.append(speed)
        self.time_history.append(current_time)
        
        return self.stamina, drain_rate
    
    def simulate_scenario(
        self,
        speed_profile: List[Tuple[float, float]],
        current_weight: float,
        grade_percent: float = 0.0,
        terrain_factor: float = 1.0,
        stance: Stance = Stance.STAND,
        movement_type: MovementType = MovementType.RUN
    ) -> dict:
        """
        模拟一个完整的测试工况（Closed-Loop：速度随体力下降而降低）
        
        Args:
            speed_profile: 速度配置列表 [(time, speed), ...]
            current_weight: 当前重量（kg）
            grade_percent: 坡度百分比
            terrain_factor: 地形系数
            stance: 当前姿态
            movement_type: 移动类型
        
        Returns:
            仿真结果字典
        """
        self.reset()
        
        # 生成时间序列
        max_time = max(t for t, _ in speed_profile)
        times = np.arange(0, max_time, self.constants.UPDATE_INTERVAL)
        
        # 插值目标速度（玩家尝试达到的速度）
        speed_times = [t for t, _ in speed_profile]
        speed_values = [s for _, s in speed_profile]
        target_speeds = np.interp(times, speed_times, speed_values)
        
        # 仿真循环（Closed-Loop：速度随体力下降而降低）
        actual_speeds = []
        is_forced_resting = False  # 强制休息状态标志
        for i, (t, target_speed) in enumerate(zip(times, target_speeds)):
            # 1. 检查强制休息触发条件
            if self.stamina < self.constants.FORCED_REST_STAMINA_THRESHOLD:
                is_forced_resting = True
            
            # 2. 检查强制休息释放条件
            if is_forced_resting and self.stamina > self.constants.FORCED_REST_RELEASE_THRESHOLD:
                is_forced_resting = False
            
            # 3. 确定实际速度
            if is_forced_resting:
                # 强制休息：速度设为0.0，模拟玩家停止休息
                actual_speed = 0.0
            else:
                # 正常逻辑：计算基于当前体力的最大允许速度
                speed_multiplier = self.calculate_speed_multiplier_by_stamina(
                    self.stamina, current_weight, movement_type
                )
                max_allowed_speed = self.constants.GAME_MAX_SPEED * speed_multiplier
                
                # 实际速度 = min(目标速度, 最大允许速度)
                # 如果体力充足，可以维持目标速度
                # 如果体力不足，速度会下降到最大允许速度
                actual_speed = min(target_speed, max_allowed_speed)
            
            actual_speeds.append(actual_speed)
            
            # 4. 使用实际速度进行仿真步计算
            self.step(
                actual_speed, current_weight, grade_percent,
                terrain_factor, stance, movement_type, t
            )
        
        # 计算实际覆盖的距离
        total_distance = np.trapz(actual_speeds, dx=self.constants.UPDATE_INTERVAL)
        
        # 如果距离未达到目标距离，计算惩罚时间（以跛行速度完成剩余距离）
        target_distance = target_speeds[-1] * max_time  # 目标距离 = 目标速度 × 时间
        if total_distance < target_distance:
            remaining_distance = target_distance - total_distance
            limp_speed = self.constants.EXHAUSTION_LIMP_SPEED  # 跛行速度 1.0 m/s
            penalty_time = remaining_distance / limp_speed
            total_time_with_penalty = max_time + penalty_time
        else:
            penalty_time = 0.0
            total_time_with_penalty = max_time
        
        # 返回结果
        return {
            'stamina_history': self.stamina_history,
            'speed_history': actual_speeds,  # 使用实际速度历史
            'target_speed_history': target_speeds,  # 添加目标速度历史
            'time_history': self.time_history,
            'final_stamina': self.stamina,
            'total_distance': total_distance,
            'target_distance': target_distance,
            'total_time': max_time,
            'total_time_with_penalty': total_time_with_penalty,
            'penalty_time': penalty_time,
            'average_speed': np.mean(actual_speeds),
            'min_stamina': min(self.stamina_history),
            'exercise_duration': self.exercise_duration,
            'rest_duration': self.rest_duration
        }
    
    def simulate_multi_phase_scenario(
        self,
        phases: List[dict],
        current_weight: float,
        terrain_factor: float = 1.0,
        stance: Stance = Stance.STAND
    ) -> dict:
        """
        模拟多阶段场景（支持每个阶段有不同的速度、坡度、移动类型）
        
        Args:
            phases: 阶段列表，每个阶段包含：
                - name: 阶段名称
                - start_time: 开始时间（秒）
                - end_time: 结束时间（秒）
                - speed: 速度（m/s）
                - grade_percent: 坡度百分比
                - movement_type: 移动类型
            current_weight: 当前重量（kg）
            terrain_factor: 地形系数
            stance: 当前姿态
        
        Returns:
            仿真结果字典
        """
        self.reset()
        
        # 生成完整时间序列
        max_time = max(phase['end_time'] for phase in phases)
        times = np.arange(0, max_time, self.constants.UPDATE_INTERVAL)
        
        # 为每个阶段构建速度、坡度、移动类型配置
        target_speeds = []
        grade_percents = []
        movement_types = []
        
        for phase in phases:
            # 计算该阶段的持续时间（以0.2秒为单位）
            phase_duration = phase['end_time'] - phase['start_time']
            num_steps = int(phase_duration / self.constants.UPDATE_INTERVAL)
            
            # 添加该阶段的配置
            target_speeds.extend([phase['speed']] * num_steps)
            grade_percents.extend([phase['grade_percent']] * num_steps)
            movement_types.extend([phase['movement_type']] * num_steps)
        
        # 插值到完整时间序列
        target_speeds = np.array(target_speeds)
        grade_percents = np.array(grade_percents)
        movement_types = movement_types
        
        # 仿真循环（Closed-Loop：速度随体力下降而降低）
        actual_speeds = []
        is_forced_resting = False  # 强制休息状态标志
        for i, (t, target_speed, grade_percent, movement_type) in enumerate(zip(times, target_speeds, grade_percents, movement_types)):
            # 1. 检查强制休息触发条件
            if self.stamina < self.constants.FORCED_REST_STAMINA_THRESHOLD:
                is_forced_resting = True
            
            # 2. 检查强制休息释放条件
            if is_forced_resting and self.stamina > self.constants.FORCED_REST_RELEASE_THRESHOLD:
                is_forced_resting = False
            
            # 3. 确定实际速度
            if is_forced_resting:
                # 强制休息：速度设为0.0，模拟玩家停止休息
                actual_speed = 0.0
            else:
                # 正常逻辑：计算基于当前体力的最大允许速度
                speed_multiplier = self.calculate_speed_multiplier_by_stamina(
                    self.stamina, current_weight, movement_type
                )
                max_allowed_speed = self.constants.GAME_MAX_SPEED * speed_multiplier
                
                # 实际速度 = min(目标速度, 最大允许速度)
                actual_speed = min(target_speed, max_allowed_speed)
            
            actual_speeds.append(actual_speed)
            
            # 4. 使用实际速度、坡度、移动类型进行仿真步计算
            self.step(
                actual_speed, current_weight, grade_percent,
                terrain_factor, stance, movement_type, t
            )
        
        # 计算实际覆盖的距离
        total_distance = np.trapz(actual_speeds, dx=self.constants.UPDATE_INTERVAL)
        
        # 计算各阶段距离
        phase_distances = []
        phase_start_idx = 0
        for phase in phases:
            phase_end_idx = int(phase['end_time'] / self.constants.UPDATE_INTERVAL)
            phase_distance = np.trapz(
                actual_speeds[phase_start_idx:phase_end_idx], 
                dx=self.constants.UPDATE_INTERVAL
            )
            phase_distances.append(phase_distance)
            phase_start_idx = phase_end_idx
        
        # 计算总目标距离（基于目标速度）
        target_distance = sum(phase['speed'] * (phase['end_time'] - phase['start_time']) for phase in phases)
        
        # 如果距离未达到目标距离，计算惩罚时间（以跛行速度完成剩余距离）
        if total_distance < target_distance:
            remaining_distance = target_distance - total_distance
            limp_speed = self.constants.EXHAUSTION_LIMP_SPEED
            penalty_time = remaining_distance / limp_speed
            total_time_with_penalty = max_time + penalty_time
        else:
            penalty_time = 0.0
            total_time_with_penalty = max_time
        
        # 返回结果
        return {
            'stamina_history': self.stamina_history,
            'speed_history': actual_speeds,
            'target_speed_history': target_speeds,
            'time_history': self.time_history,
            'final_stamina': self.stamina,
            'total_distance': total_distance,
            'target_distance': target_distance,
            'total_time': max_time,
            'total_time_with_penalty': total_time_with_penalty,
            'penalty_time': penalty_time,
            'average_speed': np.mean(actual_speeds),
            'min_stamina': min(self.stamina_history),
            'exercise_duration': self.exercise_duration,
            'rest_duration': self.rest_duration,
            'phase_distances': phase_distances,
            'phases': phases
        }


if __name__ == '__main__':
    # 测试数字孪生仿真器
    print("Realistic Stamina System - Digital Twin Simulator")
    print("=" * 60)
    
    # 创建仿真器
    twin = RSSDigitalTwin()
    
    # 测试 ACFT 工况：2英里（3218.7米）在15分27秒（927秒）内完成
    # 目标速度：3.47 m/s
    distance = 3218.7  # 米
    target_time = 927  # 秒
    target_speed = distance / target_time  # 3.47 m/s
    
    print(f"\n测试工况：ACFT 2英里测试")
    print(f"距离：{distance} 米")
    print(f"目标时间：{target_time} 秒（{target_time/60:.2f} 分钟）")
    print(f"目标速度：{target_speed:.2f} m/s")
    
    # 模拟
    speed_profile = [(0, target_speed), (target_time, target_speed)]
    result = twin.simulate_scenario(
        speed_profile=speed_profile,
        current_weight=90.0,  # 空载
        grade_percent=0.0,
        terrain_factor=1.0,
        stance=Stance.STAND,
        movement_type=MovementType.RUN
    )
    
    print(f"\n仿真结果（Closed-Loop）：")
    print(f"最终体力：{result['final_stamina']*100:.1f}%")
    print(f"最低体力：{result['min_stamina']*100:.1f}%")
    print(f"目标距离：{result['target_distance']:.1f} 米")
    print(f"实际距离：{result['total_distance']:.1f} 米")
    print(f"平均速度：{result['average_speed']:.2f} m/s")
    print(f"运动持续时间：{result['exercise_duration']:.1f} 秒")
    print(f"休息持续时间：{result['rest_duration']:.1f} 秒")
    print(f"目标时间：{result['total_time']:.1f} 秒")
    print(f"实际完成时间：{result['total_time_with_penalty']:.1f} 秒")
    if result['penalty_time'] > 0:
        print(f"惩罚时间：{result['penalty_time']:.1f} 秒（以跛行速度{result['penalty_time']/(result['target_distance']-result['total_distance']):.2f} m/s完成剩余{result['target_distance']-result['total_distance']:.1f}米）")
    print(f"时间差异：{result['total_time_with_penalty']-result['total_time']:.1f} 秒")
    
    print("\n数字孪生仿真器测试完成！")
    print("注意：Closed-Loop 模式下，速度会随体力下降而降低，影响完成时间。")
