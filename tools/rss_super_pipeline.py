#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS 增强型多目标优化与鲁棒性测试流水线
RSS Super Pipeline - Enhanced Multi-Objective Optimization with Robustness Testing

核心功能：
1. 使用 NSGA-II 采样器进行多目标优化
2. 三个目标函数：Realism_Loss, Playability_Burden, Stability_Risk
3. 30KG 负载专项平衡测试
4. 地狱级压力测试（45KG、20度坡、热应激）
5. BUG检测逻辑（数值崩溃、逻辑漏洞）
6. 自动预设提取（StandardMilsim, EliteStandard, TacticalAction）
7. 生成稳定性报告

设计原则：零侵入性 - 不修改现有文件，仅通过导入使用

时间步长同步说明（与游戏 tickScale 修复一致）：
- 数字孪生使用 dt=0.2s 步长，恢复/消耗率为「每0.2秒」
- 游戏内 UpdateSpeedBasedOnStamina 每 50ms 调用，应用 tickScale=0.05/0.2 缩放
- 孪生体预测与游戏实际行为一致
"""

import optuna
import numpy as np
import json
import math
import os
import multiprocessing
import concurrent.futures
import time
from pathlib import Path
from typing import List, Dict, Tuple, Optional, Callable
from dataclasses import dataclass, asdict

# 导入修复版数字孪生仿真器
from rss_digital_twin_fix import RSSDigitalTwin, MovementType, Stance, RSSConstants

# -----------------------------------------------------------------------------
# Performance / determinism switches
# -----------------------------------------------------------------------------
# Optimization phase should be deterministic for speed + convergence quality.
# If you want robustness testing against noise, set this to True.
ENABLE_RANDOMNESS_IN_SIMULATION = False

# 自定义多进程并行框架
class ParallelWorker:
    """
    自定义多进程并行工作器
    绕过Optuna的n_jobs限制，实现更高效的并行计算
    """
    
    def __init__(self, max_workers: int = -1):
        """
        初始化并行工作器
        
        Args:
            max_workers: 最大工作进程数，-1表示自动检测
        """
        if max_workers == -1:
            try:
                max_workers = multiprocessing.cpu_count()
                if max_workers > 1:
                    max_workers -= 1  # 预留一个核心给系统
            except:
                max_workers = 1
        
        self.max_workers = max_workers
        self.executor = None
        self.start_time = 0
        self.use_threads = (os.name == 'nt')  # Windows 上使用线程池以避免 spawn/pickle 问题
    
    def start(self):
        """启动工作池"""
        self.start_time = time.time()
        # 在 Windows 上，ProcessPoolExecutor 在启动时可能无法序列化局部函数，使用线程池替代
        if self.use_threads:
            print("[ParallelWorker] Windows detected: using ThreadPoolExecutor to avoid spawn/pickle issues")
            self.executor = concurrent.futures.ThreadPoolExecutor(
                max_workers=self.max_workers
            )
        else:
            self.executor = concurrent.futures.ProcessPoolExecutor(
                max_workers=self.max_workers
            )
    
    def shutdown(self):
        """关闭工作池"""
        if self.executor:
            self.executor.shutdown(wait=True)
    
    def map(self, func: Callable, tasks: List, batch_size: int = 1):
        """
        并行映射任务
        
        Args:
            func: 任务函数
            tasks: 任务列表
            batch_size: 批处理大小
        
        Returns:
            任务结果列表
        """
        if not self.executor:
            self.start()
        
        # 批处理任务以减少进程间通信开销
        if batch_size > 1:
            batches = []
            for i in range(0, len(tasks), batch_size):
                batches.append(tasks[i:i + batch_size])
            
            future_to_batch = {}
            for i, batch in enumerate(batches):
                future = self.executor.submit(self._process_batch, func, batch, i)
                future_to_batch[future] = i
            
            results = []
            for future in concurrent.futures.as_completed(future_to_batch):
                batch_results = future.result()
                results.extend(batch_results)
        else:
            # 单个任务处理
            future_to_task = {}
            for i, task in enumerate(tasks):
                future = self.executor.submit(func, task)
                future_to_task[future] = i
            
            results = []
            for future in concurrent.futures.as_completed(future_to_task):
                result = future.result()
                results.append(result)
        
        return results
    
    def _process_batch(self, func: Callable, batch: List, batch_idx: int):
        """
        处理一批任务
        
        Args:
            func: 任务函数
            batch: 任务批
            batch_idx: 批索引
        
        Returns:
            批处理结果
        """
        results = []
        for task in batch:
            result = func(task)
            results.append(result)
        return results
    
    def get_elapsed_time(self):
        """
        获取已运行时间
        
        Returns:
            运行时间（秒）
        """
        return time.time() - self.start_time

# 场景数据类
@dataclass
class Scenario:
    """场景数据类"""
    speed_profile: List[Tuple[float, float]]  # 时间-速度曲线
    current_weight: float  # 当前重量（kg）
    grade_percent: float  # 坡度百分比
    terrain_factor: float  # 地形因子
    stance: int  # 姿态
    movement_type: int  # 移动类型
    target_finish_time: float  # 目标完成时间（秒）
    test_type: str = ""  # 测试类型
    standard_load: float = 0.0  # 标准负载


# 场景库
class ScenarioLibrary:
    @staticmethod
    def create_acft_2mile_scenario(load_weight=0.0):
        """创建ACFT 2英里测试场景
        
        Args:
            load_weight: 负载重量（kg），ACFT标准测试应为0.0
        """
        # 校准目标：0kg Run 3.5km/15:27 → 最低体力 20%
        # 3500m / 927s ≈ 3.776 m/s
        target_speed = 3500.0 / 927.0
        return Scenario(
            speed_profile=[(0, target_speed), (927, target_speed)],
            current_weight=90.0 + load_weight,  # 体重90kg + 负载
            grade_percent=0.0,  # 平地
            terrain_factor=1.0,  # 普通地形
            stance=Stance.STAND,  # 站立
            movement_type=MovementType.RUN,  # 跑步
            target_finish_time=927.0,  # 目标完成时间15分27秒
            test_type="ACFT 2英里测试",
            standard_load=0.0  # ACFT标准测试应为0KG
        )
    
    @staticmethod
    def create_urban_combat_scenario(load_weight=30.0):
        """创建城市战斗场景"""
        # 城市战斗速度曲线：快走->跑步->快走->冲刺->快走
        return Scenario(
            speed_profile=[(0, 2.5), (60, 3.7), (120, 2.5), (180, 5.0), (210, 2.5), (300, 2.5)],
            current_weight=90.0 + load_weight,  # 体重90kg + 负载
            grade_percent=0.0,  # 平地
            terrain_factor=1.2,  # 城市地形（有障碍物）
            stance=Stance.STAND,  # 站立
            movement_type=MovementType.RUN,  # 跑步
            target_finish_time=300.0,  # 5分钟场景
            test_type="城市战斗场景"
        )
    
    @staticmethod
    def create_mountain_combat_scenario(load_weight=25.0):
        """创建山地战斗场景"""
        # 山地战斗速度曲线：慢走->快走->慢走
        return Scenario(
            speed_profile=[(0, 1.8), (120, 2.5), (240, 1.8), (360, 1.8)],
            current_weight=90.0 + load_weight,  # 体重90kg + 负载
            grade_percent=15.0,  # 15%坡度（山地）
            terrain_factor=1.5,  # 山地地形
            stance=Stance.STAND,  # 站立
            movement_type=MovementType.WALK,  # 行走
            target_finish_time=360.0,  # 6分钟场景
            test_type="山地战斗场景"
        )
    
    @staticmethod
    def create_evacuation_scenario(load_weight=40.0):
        """创建撤离场景（重载）"""
        # 撤离速度曲线：快走->跑步->快走
        return Scenario(
            speed_profile=[(0, 2.5), (90, 3.2), (180, 2.5), (270, 2.5)],
            current_weight=90.0 + load_weight,  # 体重90kg + 重载
            grade_percent=5.0,  # 5%坡度
            terrain_factor=1.3,  # 复杂地形
            stance=Stance.STAND,  # 站立
            movement_type=MovementType.RUN,  # 跑步
            target_finish_time=270.0,  # 4.5分钟场景
            test_type="撤离场景"
        )

    @staticmethod
    def create_realistic_field_patrol_scenario(load_weight=30.0):
        """创建 realistic 野战巡逻场景（模拟游戏内典型环境压力）

        目的：使优化器在更接近真实游戏条件下调参，避免理想条件优化导致游戏内消耗偏高。
        条件：草地+缓坡+中等地形，模拟 Everon 野外常见情况。
        """
        return Scenario(
            speed_profile=[(0, 2.8), (120, 3.5), (180, 2.8)],  # 巡逻速度曲线
            current_weight=90.0 + load_weight,
            grade_percent=5.0,   # 5%缓坡（游戏中常见）
            terrain_factor=1.35,  # 草地+轻度越野
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=180.0,  # 3分钟
            test_type="野战巡逻场景"
        )


@dataclass
class BugReport:
    """BUG报告数据类"""
    trial_number: int
    bug_type: str  # "negative_stamina", "nan_value", "recovery_logic_error", "overflow"
    bug_description: str
    parameters: Dict
    context: Dict  # 额外的上下文信息（如当前速度、负重、体力值等）


class RSSSuperPipeline:
    """RSS 增强型多目标优化流水线"""
    
    def __init__(
        self,
        n_trials: int = 500,  # 默认500次迭代，平衡性能和多样性
        n_jobs: int = -1,  # -1表示自动检测CPU核心数
        use_database: bool = False,  # 是否使用数据库存储（默认False以提高性能）
        batch_size: int = 5  # 批处理大小
    ):
        """
        初始化流水线
        
        Args:
            n_trials: 优化迭代次数（默认5000，提高多样性）
            n_jobs: 并行线程数（-1表示自动检测CPU核心数）
            use_database: 是否使用数据库存储（默认False，使用内存存储以提高性能）
            batch_size: 批处理大小
        """
        # 自动检测CPU核心数
        if n_jobs == -1:
            try:
                n_jobs = multiprocessing.cpu_count()
                # 预留一个核心给系统
                if n_jobs > 1:
                    n_jobs -= 1
            except:
                n_jobs = 1
        
        self.n_trials = n_trials
        self.n_jobs = n_jobs
        self.batch_size = batch_size
        self.use_database = use_database
        self.study = None
        self.best_trials = []
        self.bug_reports: List[BugReport] = []
        # 初始化自定义并行工作器
        self.parallel_worker = ParallelWorker(max_workers=n_jobs)
        
        # GUI支持
        self.gui_update_callback = None
    
    def set_gui_callback(self, callback):
        """设置GUI更新回调函数"""
        self.gui_update_callback = callback
        
    def objective(self, trial: optuna.Trial) -> Tuple[float, float, float]:
        """
        Optuna 三目标函数
        
        Args:
            trial: Optuna 试验对象
        
        Returns:
            (playability_burden, stability_risk, physiological_realism)
        """
        # ==================== 1. 定义搜索空间 ====================
        # 使用与 rss_optimizer_optuna.py 相同的参数范围（但已优化）
        
        # 能量转换相关
        # 校准目标：0kg Run 3.5km/15:27 → 最低体力 20%
        # 8e-7 时 ACFT 约 3%，需更低以达 20%。范围已下调以匹配生理学检查：5e-7~2.5e-6
        energy_to_stamina_coeff = trial.suggest_float(
            'energy_to_stamina_coeff', 5e-7, 2.5e-6, log=True
        )
        
        # 恢复系统相关
        # 调整：适当收紧上界，避免移动中“回血”过强导致 Run/Sprint 净消耗过低。
        base_recovery_rate = trial.suggest_float(
            'base_recovery_rate', 2e-5, 1.2e-4, log=True  # 兼顾静止慢恢复与 Walk 30kg 净恢复
        )
        # 约束：prone恢复应该快于standing恢复，所以prone应该有更高的下界
        prone_recovery_multiplier = trial.suggest_float(
            'prone_recovery_multiplier', 2.0, 2.8  # 提高下界，确保>fast
        )
        standing_recovery_multiplier = trial.suggest_float(
            'standing_recovery_multiplier', 1.3, 1.8  # 提高下界，确保>medium
        )
        # 30kg Walk 需净恢复：penalty 过大会扼杀恢复。1e-3 时 penalty≈0.0016>>base_recovery
        load_recovery_penalty_coeff = trial.suggest_float(
            'load_recovery_penalty_coeff', 5e-6, 5e-5, log=True
        )
        load_recovery_penalty_exponent = trial.suggest_float(
            'load_recovery_penalty_exponent', 1.0, 2.0  # 降低上界，避免 penalty 爆炸
        )
        
        # 负重系统相关（改进：允许更低负重惩罚以通过30KG测试）
        encumbrance_speed_penalty_coeff = trial.suggest_float(
            'encumbrance_speed_penalty_coeff', 0.1, 0.22  # 降低上界，减轻速度限制
        )
        encumbrance_stamina_drain_coeff = trial.suggest_float(
            'encumbrance_stamina_drain_coeff', 0.8, 2.0  # 从1.0降低到0.8，允许更低负重惩罚
        )
        
        # Sprint 相关
        # Sprint 消耗 = Run × 3x
        # 范围 2.0~3.5：允许 3x 冲刺消耗
        sprint_stamina_drain_multiplier = trial.suggest_float(
            'sprint_stamina_drain_multiplier', 2.0, 3.5
        )
        
        # 疲劳系统相关
        fatigue_accumulation_coeff = trial.suggest_float(
            'fatigue_accumulation_coeff', 0.005, 0.03, log=True
        )
        fatigue_max_factor = trial.suggest_float(
            'fatigue_max_factor', 1.5, 3.0
        )
        
        # 代谢适应相关
        # 调整：放宽范围，允许更高效率（更低消耗），改善可玩性
        aerobic_efficiency_factor = trial.suggest_float(
            'aerobic_efficiency_factor', 0.85, 1.05
        )
        anaerobic_efficiency_factor = trial.suggest_float(
            'anaerobic_efficiency_factor', 1.1, 1.5
        )
        
        # 恢复系统高级参数（优化：减缓"平台恢复期"，fast/medium/slow 均下调）
        # recovery_nonlinear_coeff 0.15~0.35：降低低体力时的恢复加成
        recovery_nonlinear_coeff = trial.suggest_float(
            'recovery_nonlinear_coeff', 0.15, 0.35
        )
        # 约束：fast > medium > slow，但整体降低以减缓平台期
        # fast 1.0~1.3（原 1.6~2.4 导致恢复过快）
        fast_recovery_multiplier = trial.suggest_float(
            'fast_recovery_multiplier', 1.0, 1.3
        )
        medium_recovery_multiplier = trial.suggest_float(
            'medium_recovery_multiplier', 0.85, 1.1  # 必须 < fast
        )
        slow_recovery_multiplier = trial.suggest_float(
            'slow_recovery_multiplier', 0.5, 0.8
        )
        marginal_decay_threshold = trial.suggest_float(
            'marginal_decay_threshold', 0.7, 0.9
        )
        marginal_decay_coeff = trial.suggest_float(
            'marginal_decay_coeff', 1.05, 1.15
        )
        min_recovery_stamina_threshold = trial.suggest_float(
            'min_recovery_stamina_threshold', 0.15, 0.25
        )
        min_recovery_rest_time_seconds = trial.suggest_float(
            'min_recovery_rest_time_seconds', 1.0, 5.0
        )
        
        # Sprint系统高级参数
        sprint_speed_boost = trial.suggest_float(
            'sprint_speed_boost', 0.28, 0.35  # 提高下界，确保 Sprint 有足够速度
        )
        
        # 姿态系统参数（消耗倍数：蹲/趴移动时体力消耗增加）
        # 修复：与游戏 SCR_StaminaConsumption 语义一致，游戏将其作为消耗倍数使用
        # posture_crouch: 蹲姿行走消耗倍数 1.2~2.2（站立=1.0）
        # posture_prone: 趴姿匍匐消耗倍数 2.0~4.0
        posture_crouch_multiplier = trial.suggest_float(
            'posture_crouch_multiplier', 1.2, 2.2
        )
        posture_prone_multiplier = trial.suggest_float(
            'posture_prone_multiplier', 2.3, 4.0  # 确保 > crouch_max(2.2)
        )
        
        # 动作消耗参数
        jump_stamina_base_cost = trial.suggest_float(
            'jump_stamina_base_cost', 0.025, 0.045
        )
        vault_stamina_start_cost = trial.suggest_float(
            'vault_stamina_start_cost', 0.015, 0.025
        )
        climb_stamina_tick_cost = trial.suggest_float(
            'climb_stamina_tick_cost', 0.008, 0.012
        )
        jump_consecutive_penalty = trial.suggest_float(
            'jump_consecutive_penalty', 0.4, 0.6
        )
        
        # 坡度系统参数
        slope_uphill_coeff = trial.suggest_float(
            'slope_uphill_coeff', 0.06, 0.10
        )
        slope_downhill_coeff = trial.suggest_float(
            'slope_downhill_coeff', 0.02, 0.04
        )
        
        # 游泳系统参数
        swimming_base_power = trial.suggest_float(
            'swimming_base_power', 15.0, 25.0
        )
        swimming_encumbrance_threshold = trial.suggest_float(
            'swimming_encumbrance_threshold', 20.0, 30.0
        )
        swimming_static_drain_multiplier = trial.suggest_float(
            'swimming_static_drain_multiplier', 2.5, 3.5
        )
        swimming_dynamic_power_efficiency = trial.suggest_float(
            'swimming_dynamic_power_efficiency', 1.5, 2.5
        )
        swimming_energy_to_stamina_coeff = trial.suggest_float(
            'swimming_energy_to_stamina_coeff', 0.00004, 0.00006
        )
        
        # 环境因子参数
        env_heat_stress_max_multiplier = trial.suggest_float(
            'env_heat_stress_max_multiplier', 1.2, 1.6
        )
        env_rain_weight_max = trial.suggest_float(
            'env_rain_weight_max', 6.0, 10.0
        )
        env_wind_resistance_coeff = trial.suggest_float(
            'env_wind_resistance_coeff', 0.03, 0.07
        )
        env_mud_penalty_max = trial.suggest_float(
            'env_mud_penalty_max', 0.3, 0.5
        )
        env_temperature_heat_penalty_coeff = trial.suggest_float(
            'env_temperature_heat_penalty_coeff', 0.015, 0.025
        )
        env_temperature_cold_recovery_penalty_coeff = trial.suggest_float(
            'env_temperature_cold_recovery_penalty_coeff', 0.04, 0.06
        )
        
        # ==================== 2. 创建参数对象并更新常量 ====================
        
        # 创建新的常量实例（不修改原始类）
        constants = RSSConstants()
        
        # 更新参数
        constants.ENERGY_TO_STAMINA_COEFF = energy_to_stamina_coeff
        constants.BASE_RECOVERY_RATE = base_recovery_rate
        constants.STANDING_RECOVERY_MULTIPLIER = standing_recovery_multiplier
        constants.PRONE_RECOVERY_MULTIPLIER = prone_recovery_multiplier
        constants.LOAD_RECOVERY_PENALTY_COEFF = load_recovery_penalty_coeff
        constants.LOAD_RECOVERY_PENALTY_EXPONENT = load_recovery_penalty_exponent
        constants.ENCUMBRANCE_SPEED_PENALTY_COEFF = encumbrance_speed_penalty_coeff
        constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF = encumbrance_stamina_drain_coeff
        constants.SPRINT_STAMINA_DRAIN_MULTIPLIER = sprint_stamina_drain_multiplier
        constants.FATIGUE_ACCUMULATION_COEFF = fatigue_accumulation_coeff
        constants.FATIGUE_MAX_FACTOR = fatigue_max_factor
        constants.AEROBIC_EFFICIENCY_FACTOR = aerobic_efficiency_factor
        constants.ANAEROBIC_EFFICIENCY_FACTOR = anaerobic_efficiency_factor
        constants.RECOVERY_NONLINEAR_COEFF = recovery_nonlinear_coeff
        constants.FAST_RECOVERY_MULTIPLIER = fast_recovery_multiplier
        constants.MEDIUM_RECOVERY_MULTIPLIER = medium_recovery_multiplier
        constants.SLOW_RECOVERY_MULTIPLIER = slow_recovery_multiplier
        constants.MARGINAL_DECAY_THRESHOLD = marginal_decay_threshold
        constants.MARGINAL_DECAY_COEFF = marginal_decay_coeff
        constants.MIN_RECOVERY_STAMINA_THRESHOLD = min_recovery_stamina_threshold
        constants.MIN_RECOVERY_REST_TIME_SECONDS = min_recovery_rest_time_seconds
        constants.SPRINT_SPEED_BOOST = sprint_speed_boost
        constants.POSTURE_CROUCH_MULTIPLIER = posture_crouch_multiplier
        constants.POSTURE_PRONE_MULTIPLIER = posture_prone_multiplier
        constants.JUMP_STAMINA_BASE_COST = jump_stamina_base_cost
        constants.VAULT_STAMINA_START_COST = vault_stamina_start_cost
        constants.CLIMB_STAMINA_TICK_COST = climb_stamina_tick_cost
        constants.JUMP_CONSECUTIVE_PENALTY = jump_consecutive_penalty
        constants.SLOPE_UPHILL_COEFF = slope_uphill_coeff
        constants.SLOPE_DOWNHILL_COEFF = slope_downhill_coeff
        constants.SWIMMING_BASE_POWER = swimming_base_power
        constants.SWIMMING_ENCUMBRANCE_THRESHOLD = swimming_encumbrance_threshold
        constants.SWIMMING_STATIC_DRAIN_MULTIPLIER = swimming_static_drain_multiplier
        constants.SWIMMING_DYNAMIC_POWER_EFFICIENCY = swimming_dynamic_power_efficiency
        constants.SWIMMING_ENERGY_TO_STAMINA_COEFF = swimming_energy_to_stamina_coeff
        constants.ENV_HEAT_STRESS_MAX_MULTIPLIER = env_heat_stress_max_multiplier
        constants.ENV_RAIN_WEIGHT_MAX = env_rain_weight_max
        constants.ENV_WIND_RESISTANCE_COEFF = env_wind_resistance_coeff
        constants.ENV_MUD_PENALTY_MAX = env_mud_penalty_max
        constants.ENV_TEMPERATURE_HEAT_PENALTY_COEFF = env_temperature_heat_penalty_coeff
        constants.ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = env_temperature_cold_recovery_penalty_coeff
        
        # ==================== 3. 创建数字孪生仿真器 ====================
        
        twin = RSSDigitalTwin(constants)
        
        # ==================== 4. 基础体能检查（ACFT 2英里测试，空载）====================
        # 这是最低标准，不是优化目标
        basic_fitness_passed = self._check_basic_fitness(twin)
        
        # ==================== 5. 目标1：评估30KG可玩性（Playability Burden）====================
        
        # 专门针对30KG负载进行测试
        playability_burden = self._evaluate_30kg_playability(twin)
        
        # ==================== 6. 目标2：评估稳定性风险（Stability Risk / BUG检测）====================
        
        # 地狱级压力测试
        stability_risk, bug_reports = self._evaluate_stability_risk(
            twin, trial.number, trial.params
        )
        
        # 记录BUG报告
        self.bug_reports.extend(bug_reports)
        
        # ==================== 7. 添加物理约束条件（JSON合理性修复） ====================
        # 这些约束确保生成的参数满足生物学和物理学逻辑
        
        constraint_penalty = 0.0
        
        # 基础体能检查约束：未通过基础体能测试的参数组合将被严重惩罚
        if not basic_fitness_passed:
            penalty = 1000.0  # 严重惩罚未通过基础体能测试的参数
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束1: prone_recovery > standing_recovery（生理逻辑）
        # 说明：趴下应该比站立更容易恢复体力
        if prone_recovery_multiplier <= standing_recovery_multiplier:
            violation_factor = standing_recovery_multiplier - prone_recovery_multiplier + 0.1
            penalty = violation_factor * 200.0  # 从500降到200
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束2: standing_recovery > slow_recovery（恢复倍数递减）
        # 说明：站立恢复应该快于慢速恢复
        if standing_recovery_multiplier <= slow_recovery_multiplier:
            violation_factor = slow_recovery_multiplier - standing_recovery_multiplier + 0.1
            penalty = violation_factor * 150.0  # 从300降到150
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束3: fast_recovery > medium_recovery > slow_recovery（恢复速度递减）
        # 说明：快速恢复 > 中速恢复 > 慢速恢复
        if fast_recovery_multiplier <= medium_recovery_multiplier:
            violation_factor = medium_recovery_multiplier - fast_recovery_multiplier + 0.1
            penalty = violation_factor * 150.0  # 从400降到150
            constraint_penalty += penalty
            stability_risk += penalty
        
        if medium_recovery_multiplier <= slow_recovery_multiplier:
            violation_factor = slow_recovery_multiplier - medium_recovery_multiplier + 0.1
            penalty = violation_factor * 300.0
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束4: posture_crouch_multiplier > 1.0（蹲下应该增加消耗）
        # 说明：蹲下时体力消耗应该大于正常站立，multiplier应该>1.0
        if posture_crouch_multiplier <= 1.0:
            violation_factor = 1.0 - posture_crouch_multiplier + 0.1
            penalty = violation_factor * 200.0
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束5: posture_prone_multiplier > 1.0（趴下应该增加消耗）
        # 说明：趴下时体力消耗应该大于正常站立，multiplier应该>1.0
        if posture_prone_multiplier <= 1.0:
            violation_factor = 1.0 - posture_prone_multiplier + 0.1
            penalty = violation_factor * 600.0
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束6: posture_prone_multiplier > posture_crouch_multiplier（趴下比蹲下消耗更大）
        # 说明：趴下时体力消耗应该大于蹲下
        if posture_prone_multiplier <= posture_crouch_multiplier:
            violation_factor = posture_crouch_multiplier - posture_prone_multiplier + 0.1
            penalty = violation_factor * 300.0
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束7: playability配置下，base_recovery_rate应该相对较高
        # 说明：playability（可玩性）应该意味着更容易恢复，而不是更慢
        # 注：这是一个软约束，仅在明显偏离时才惩罚
        # （这个约束很难在不知道当前是哪个配置的情况下强制）
        
        # ==================== 7. 软约束检查（参数合理性惩罚） ====================
        # 目标：
        # - Sprint / Run 必须“净消耗”且消耗不应过低
        # - Walk 必须“缓慢净恢复”（不应快到像静止趴下回血）
        movement_balance_penalty = self._evaluate_movement_balance_penalty(constants)
        if movement_balance_penalty > 0.0:
            constraint_penalty += movement_balance_penalty
            stability_risk += movement_balance_penalty
        
        # ==================== 8. 计算生理学合理性 ====================
        # 基于科学文献的生理学合理性评估
        physiological_realism = self._evaluate_physiological_realism(constants)
        
        # ==================== 9. 应用多阶段优化权重 ====================
        # 获取当前权重
        stability_weight = getattr(self, '_stability_weight', 1.0)
        realism_weight = 50.0  # 进一步降低到50，优先优化可玩性和稳定性
        
        # 应用权重到稳定性风险
        weighted_stability_risk = stability_risk * stability_weight
        
        # 生理学合理度是越大越好（0-100），但优化器使用最小化方向。
        # 因此这里将其转换为损失：physiology_loss = (100 - realism_score)，越小越好。
        physiology_loss = max(0.0, 100.0 - physiological_realism)
        weighted_physiology_loss = physiology_loss * realism_weight
        
        # ==================== 9. 返回三个目标函数（包含约束惩罚） ====================
        
        # 更新GUI（如果存在）
        # 注意：优化在后台线程运行，回调由 GUI 通过 root.after(0, ...) 调度到主线程
        if self.gui_update_callback:
            try:
                self.gui_update_callback(
                    iteration=trial.number,
                    playability=playability_burden,
                    stability=stability_risk,
                    realism=physiological_realism,
                    params=dict(trial.params)
                )
            except Exception as e:
                print(f"GUI更新失败: {e}")
        
        # 对目标函数进行缩放，使其更容易收敛
        scaled_playability = playability_burden * 0.1  # 缩小10倍（从100倍调整为10倍，避免精度丢失）
        scaled_stability = weighted_stability_risk * 0.1  # 缩小10倍
        scaled_realism = weighted_physiology_loss * 0.1  # 生理学损失：缩小10倍

        # 新：评估 40/60 分钟会话相关指标并作为第四个目标（越小越好）
        engagement_loss, session_fail = self._evaluate_session_metrics(twin)

        # 如果会话硬约束失败，给出极大惩罚，优先淘汰此解
        if session_fail:
            severe_penalty = 1e6
            scaled_stability += severe_penalty
            scaled_playability += severe_penalty
            scaled_realism += severe_penalty
            engagement_loss = severe_penalty

        # 返回四目标：playability, stability, realism_loss, engagement_loss
        return scaled_playability, scaled_stability, scaled_realism, float(engagement_loss)
    
    def _evaluate_physiological_realism(self, constants: RSSConstants) -> float:
        """
        评估生理学合理性（基于科学文献）
        
        Args:
            constants: 常量对象
        
        Returns:
            生理学合理性值（越大越好，0-100范围）
        """
        # 基于科学文献的生理学合理性评估
        # 参考：
        # - Pandolf et al. (1977): 能量消耗模型
        # - Givoni & Goldman (1971): 跑步模型
        # - ACSM (2018): 生理学阈值
        
        realism_score = 100.0
        
        # 1. 恢复模型合理性
        # 检查恢复率是否在生理学合理范围内
        base_recovery = constants.BASE_RECOVERY_RATE
        if base_recovery < 1.0e-4 or base_recovery > 5.0e-4:
            realism_score -= 10.0  # 基础恢复率偏离生理学范围
        # 增加恢复率的精细评估
        elif 1.8e-4 <= base_recovery <= 2.2e-4:
            realism_score -= 5.0  # 惩罚过于接近默认值的参数
        
        # 2. 恢复倍数合理性
        # 检查恢复倍数是否合理
        prone_recovery = constants.PRONE_RECOVERY_MULTIPLIER
        standing_recovery = constants.STANDING_RECOVERY_MULTIPLIER
        fast_recovery = constants.FAST_RECOVERY_MULTIPLIER
        medium_recovery = constants.MEDIUM_RECOVERY_MULTIPLIER
        slow_recovery = constants.SLOW_RECOVERY_MULTIPLIER
        
        # 恢复倍数合理性检查（分离姿态和恢复阶段）
        # 姿态约束：趴下应该比站立恢复快
        if not (prone_recovery > standing_recovery):
            realism_score -= 15.0  # 趴下应该比站立恢复快
        elif abs(prone_recovery - standing_recovery) < 0.5:
            realism_score -= 8.0  # 惩罚差异过小的参数

        # 恢复阶段约束：快速 > 中等 > 慢速
        if not (fast_recovery > medium_recovery > slow_recovery):
            realism_score -= 15.0  # 恢复阶段应该递减
        elif abs(fast_recovery - slow_recovery) < 1.0:
            realism_score -= 8.0  # 惩罚差异过小的参数
        
        # 3. 代谢阈值合理性
        # 检查代谢阈值是否在合理范围内
        aerobic_threshold = constants.AEROBIC_THRESHOLD
        anaerobic_threshold = constants.ANAEROBIC_THRESHOLD
        if aerobic_threshold >= anaerobic_threshold:
            realism_score -= 20.0  # 有氧阈值应该低于无氧阈值
        
        # 4. 能量转换合理性
        # 校准目标：0kg Run 3.5km/15:27 → 20%，约 1.5e-06
        # 允许 5e-7~5e-5 范围
        energy_coeff = constants.ENERGY_TO_STAMINA_COEFF
        if energy_coeff < 5e-7 or energy_coeff > 5e-5:
            realism_score -= 10.0  # 能量转换系数偏离生理学范围
        
        # 5. 负重影响合理性
        # 检查负重影响是否合理
        encumbrance_coeff = constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF
        if encumbrance_coeff < 0.8 or encumbrance_coeff > 2.0:
            realism_score -= 10.0  # 负重系数偏离生理学范围
        
        # 6. 运动消耗合理性
        # Sprint 消耗倍数应 > 1.0（Sprint 比 Run 更耗体）
        # 搜索空间 1.2~2.5，游戏测试 2.0x 体验良好
        sprint_multiplier = constants.SPRINT_STAMINA_DRAIN_MULTIPLIER
        if sprint_multiplier <= 1.0:
            realism_score -= 10.0  # 冲刺应比跑步更耗体力
        
        # 7. 疲劳系统合理性
        # 检查疲劳积累系数是否合理
        fatigue_accumulation = constants.FATIGUE_ACCUMULATION_COEFF
        if fatigue_accumulation < 0.005 or fatigue_accumulation > 0.03:
            realism_score -= 8.0  # 疲劳积累系数偏离合理范围
        
        # 8. 姿态系统合理性
        # 检查姿态消耗倍数是否合理
        crouch_multiplier = constants.POSTURE_CROUCH_MULTIPLIER
        prone_multiplier = constants.POSTURE_PRONE_MULTIPLIER
        if crouch_multiplier <= 1.0 or prone_multiplier <= 1.0:
            realism_score -= 10.0  # 姿态倍数应该大于1.0
        if prone_multiplier <= crouch_multiplier:
            realism_score -= 8.0  # 趴下应该比蹲下消耗更大
        
        # 确保分数在0-100范围内
        realism_score = max(0.0, min(realism_score, 100.0))
        
        return realism_score
    
    def _evaluate_movement_balance_penalty(self, constants: RSSConstants) -> float:
        """基于短时段“移动净值”行为的软硬约束惩罚。
        
        设计目标（用户意图）：
        - Run / Sprint：体力应稳定下降（净消耗）
        - Walk：体力应缓慢上升（净恢复）
        
        说明：
        - 使用短时段固定速度测试，计算起止体力变化。
        - 惩罚系数取较大值，使其在 NSGA-II 中具有足够约束力。
        """
        # 使用独立 twin，避免污染主仿真器状态
        twin = RSSDigitalTwin(constants)

        def simulate_fixed_speed(
            speed: float,
            duration_seconds: float,
            current_weight: float,
            movement_type: int,
            initial_stamina: float,
        ) -> float:
            """返回 (end_stamina - start_stamina)。"""
            twin.reset()
            twin.stamina = float(np.clip(initial_stamina, 0.0, 1.0))
            current_time = 0.0
            steps = int(duration_seconds / 0.2)
            for _ in range(steps):
                twin.step(
                    speed=speed,
                    current_weight=current_weight,
                    grade_percent=0.0,
                    terrain_factor=1.0,
                    stance=Stance.STAND,
                    movement_type=movement_type,
                    current_time=current_time,
                    enable_randomness=False,
                )
                current_time += 0.2
            return twin.stamina - initial_stamina

        penalty = 0.0

        # 1) Run：60秒 3.7m/s 空载
        # 期望：5% ≤ 消耗 ≤ 15%（避免消耗过大导致可玩性差）
        run_delta = simulate_fixed_speed(
            speed=3.7,
            duration_seconds=60.0,
            current_weight=90.0,
            movement_type=MovementType.RUN,
            initial_stamina=1.0,
        )
        actual_run_drop = max(0.0, -run_delta)
        required_run_drop_min = 0.05
        required_run_drop_max = 0.15  # 消耗上限，避免"消耗非常大"
        if actual_run_drop < required_run_drop_min:
            penalty += (required_run_drop_min - actual_run_drop) * 3000.0
        elif actual_run_drop > required_run_drop_max:
            penalty += (actual_run_drop - required_run_drop_max) * 4000.0

        # 2) Sprint：30秒 5.0m/s 空载
        # 期望：10% ≤ 消耗 ≤ 45%（sprint_mult 3.0 时约 35-40%）
        sprint_delta = simulate_fixed_speed(
            speed=5.0,
            duration_seconds=30.0,
            current_weight=90.0,
            movement_type=MovementType.SPRINT,
            initial_stamina=1.0,
        )
        actual_sprint_drop = max(0.0, -sprint_delta)
        required_sprint_drop_min = 0.10
        required_sprint_drop_max = 0.45
        if actual_sprint_drop < required_sprint_drop_min:
            penalty += (required_sprint_drop_min - actual_sprint_drop) * 4000.0
        elif actual_sprint_drop > required_sprint_drop_max:
            penalty += (actual_sprint_drop - required_sprint_drop_max) * 5000.0

        # 3) Walk：120秒 1.8m/s 标准战斗负载（30KG，总重 120KG）
        walk_delta = simulate_fixed_speed(
            speed=1.8,
            duration_seconds=120.0,
            current_weight=120.0,  # 90kg 体重 + 30kg 负载
            movement_type=MovementType.WALK,
            initial_stamina=0.50,
        )
        # 30KG 负载下 Walk 净恢复应 0.2%~3%，超 3% 严重惩罚（避免 Walk 回血过快）
        min_walk_gain = 0.002  # 最小恢复 0.2%
        max_walk_gain = 0.03   # 最大恢复 3%
        if walk_delta < min_walk_gain:
            penalty += (min_walk_gain - walk_delta) * 3000.0
        elif walk_delta > max_walk_gain:
            penalty += (walk_delta - max_walk_gain) * 5000.0  # 提高超限惩罚

        return float(penalty)
    
    def _check_basic_fitness(self, twin: RSSDigitalTwin) -> bool:
        """
        检查基础体能标准（ACFT 2英里测试，空载）
        
        设计标准（与游戏时间缩放 tickScale=0.25 一致）：
        - 0kg 负重，3.5km 用时 15分27秒（927秒），速度 3.776 m/s
        - 体力最低不能跌破 20%
        - 平均体力应 > 25%

        Args:
            twin: 数字孪生仿真器
        
        Returns:
            是否通过基础体能测试
        """
        # 使用标准ACFT 2英里测试（空载）
        scenario = ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0)
        
        try:
            results = twin.simulate_scenario(
                speed_profile=scenario.speed_profile,
                current_weight=scenario.current_weight,
                grade_percent=scenario.grade_percent,
                terrain_factor=scenario.terrain_factor,
                stance=scenario.stance,
                movement_type=scenario.movement_type,
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION
            )
            
            # 检查是否通过基础体能测试
            # 标准：完成时间不超过目标时间的120%
            target_time = scenario.target_finish_time
            actual_time = results['total_time_with_penalty']
            time_ratio = actual_time / target_time

            # 检查最低体力是否合理（不低于10%）
            min_stamina = results['min_stamina']
            
            # 检查平均体力是否合理（不能长期处于10%，应该 > 15%）
            stamina_history = results.get('stamina_history', [])
            if stamina_history:
                avg_stamina = sum(stamina_history) / len(stamina_history)
            else:
                avg_stamina = 1.0  # 如果没有历史数据，假设为100%

            # 通过条件：时间不超过120%且体力不低于20%且平均体力 > 25%
            return time_ratio <= 1.2 and min_stamina >= 0.20 and avg_stamina > 0.25

        except Exception:
            return False
    
    def _evaluate_30kg_playability(self, twin: RSSDigitalTwin) -> float:
        """
        评估30KG负载下的可玩性负担（专门针对玩家反馈的"30KG太累"问题）
        
        Args:
            twin: 数字孪生仿真器
        
        Returns:
            可玩性负担（越小越好）
        """
        # 30KG战斗负载测试 - 结合多个场景（含环境压力场景）
        scenarios = [
            ScenarioLibrary.create_acft_2mile_scenario(load_weight=30.0),
            ScenarioLibrary.create_urban_combat_scenario(load_weight=30.0),
            ScenarioLibrary.create_mountain_combat_scenario(load_weight=25.0),
            ScenarioLibrary.create_realistic_field_patrol_scenario(load_weight=30.0),
        ]
        
        # 使用并行处理提高性能
        def evaluate_scenario(scenario):
            """评估单个场景"""
            try:
                # 创建独立的twin实例以避免线程安全问题
                scenario_twin = RSSDigitalTwin(twin.constants)
                # 重置twin状态
                scenario_twin.reset()
                results = scenario_twin.simulate_scenario(
                    speed_profile=scenario.speed_profile,
                    current_weight=scenario.current_weight,
                    grade_percent=scenario.grade_percent,
                    terrain_factor=scenario.terrain_factor,
                    stance=scenario.stance,
                    movement_type=scenario.movement_type,
                    enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION
                )
                
                # 计算当前场景的可玩性负担（连续型，避免大量解被压成同一常数）
                scenario_burden = 0.0

                target_time = float(scenario.target_finish_time)
                actual_time = float(results['total_time_with_penalty'])
                time_ratio = actual_time / max(target_time, 1e-6)

                # 1) 完成时间惩罚：允许轻微超时，但超时越多惩罚越大（连续而非阶梯）
                # 目标：30KG 下不要“过分慢”，但也不强迫极限跑。
                scenario_burden += max(0.0, time_ratio - 1.15) * 250.0  # 增加惩罚系数
                scenario_burden += max(0.0, time_ratio - 1.30) * 600.0  # 增加惩罚系数

                # 2) 体力压力惩罚：min/mean 体力越低，可玩性越差（连续）
                stamina_history = results.get('stamina_history', [])
                if stamina_history:
                    min_stamina = float(min(stamina_history))
                    mean_stamina = float(sum(stamina_history) / len(stamina_history))

                    # 偏好：最低体力至少 15%（从20%放宽到15%）
                    scenario_burden += max(0.0, 0.15 - min_stamina) * 1200.0  # 增加惩罚系数和阈值

                    # 偏好：平均体力不要太低（避免全程“红条”）
                    scenario_burden += max(0.0, 0.30 - mean_stamina) * 400.0  # 增加惩罚系数和阈值

                    # 3) “耗尽占比”惩罚：低体力时长占比越高，惩罚越大（平方增强区分度）
                    exhausted_frames = sum(1 for s in stamina_history if s < 0.10)  # 增加耗尽阈值
                    exhaustion_ratio = exhausted_frames / len(stamina_history)
                    scenario_burden += (exhaustion_ratio * exhaustion_ratio) * 1000.0  # 增加惩罚系数

                return float(scenario_burden), scenario
            except Exception as e:
                # 如果仿真失败，返回大惩罚值
                print(f"场景评估失败: {e}")
                return 2000.0, None
        
        # 使用自定义并行工作器并行评估场景
        try:
            # 确保并行工作器已启动
            if not hasattr(self.parallel_worker, 'executor') or self.parallel_worker.executor is None:
                self.parallel_worker.start()
            results_list = self.parallel_worker.map(evaluate_scenario, scenarios, batch_size=1)
        except Exception as e:
            # 如果并行处理失败，回退到串行处理
            print(f"并行处理失败，回退到串行: {e}")
            results_list = [evaluate_scenario(scenario) for scenario in scenarios]
        
        total_burden = 0.0
        weights = [0.35, 0.25, 0.2, 0.2]  # 2英里、城市、山地、野战巡逻
        
        for i, (scenario_burden, scenario) in enumerate(results_list):
            # 如果场景仿真失败，返回极大惩罚值（但不要“夹紧”成常数，否则会让目标函数失去区分度）
            if scenario is None:
                return 2_000_000.0
            total_burden += float(scenario_burden) * weights[i]
        
        # 参数多样性惩罚：降低权重，避免与可玩性目标冲突
        # sprint_mult 校准 2.0，不惩罚 1.8~2.2 区间
        param_variation_score = 0.0
        key_params = [
            ('SPRINT_STAMINA_DRAIN_MULTIPLIER', 2.8, 3.2, 15.0),  # Sprint=3x Run
            ('BASE_RECOVERY_RATE', 4e-5, 8e-5, 15.0),  # Walk 可净恢复
            ('ENCUMBRANCE_STAMINA_DRAIN_COEFF', 0.9, 1.5, 15.0),
        ]
        for param_name, min_val, max_val, penalty in key_params:
            if hasattr(twin.constants, param_name):
                value = getattr(twin.constants, param_name)
                if not (min_val <= value <= max_val):
                    param_variation_score += penalty

        total_burden += param_variation_score
        
        # 添加场景结果多样性惩罚
        # 如果所有场景的负担都非常接近，说明参数变化影响不大
        if len(results_list) >= 2:
            burdens = [scenario_burden for scenario_burden, _ in results_list]
            if len(burdens) >= 2:
                burden_diff = max(burdens) - min(burdens)
                if burden_diff < 100.0:
                    total_burden += 100.0  # 惩罚结果过于一致的参数组合
        
        return float(total_burden)
    
    def _evaluate_stability_risk(
        self,
        twin: RSSDigitalTwin,
        trial_number: int,
        parameters: Dict
    ) -> Tuple[float, List[BugReport]]:
        """
        评估稳定性风险（BUG检测）
        
        Args:
            twin: 数字孪生仿真器
            trial_number: 试验编号
            parameters: 参数字典
        
        Returns:
            (稳定性风险值, BUG报告列表)
        """
        stability_risk = 0.0
        bug_reports = []
        
        # ==================== 地狱级压力测试：Chaos Scenario ====================
        # 45KG负载、20度陡坡、环境热应激最大化
        
        # 20度坡 = tan(20°) * 100 ≈ 36.4%
        chaos_grade_percent = math.tan(math.radians(20.0)) * 100.0
        chaos_weight = 90.0 + 45.0  # 135kg总重
        chaos_heat_multiplier = 1.5  # 最大热应激
        
        # 创建地狱级场景：短距离但极端条件
        chaos_speed = 2.5  # m/s，降低速度以应对极端条件
        chaos_duration = 120.0  # 2分钟
        chaos_speed_profile = [(0, chaos_speed), (chaos_duration, chaos_speed)]
        
        try:
            # 运行地狱级测试
            chaos_results = twin.simulate_scenario(
                speed_profile=chaos_speed_profile,
                current_weight=chaos_weight,
                grade_percent=chaos_grade_percent,
                terrain_factor=1.5,  # 困难地形
                stance=Stance.STAND,
                movement_type=MovementType.RUN,
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION
            )
            
            stamina_history = chaos_results['stamina_history']
            speed_history = chaos_results['speed_history']
            
            # ==================== BUG检测逻辑 ====================
            
            # 1. 检测负数体力（重大BUG）
            min_stamina = min(stamina_history) if len(stamina_history) > 0 else 0.0
            if min_stamina < -0.01:  # 允许小的数值误差，但负数明显是BUG
                stability_risk += 1000.0
                bug_reports.append(BugReport(
                    trial_number=trial_number,
                    bug_type="negative_stamina",
                    bug_description=f"体力值变为负数：{min_stamina:.6f}",
                    parameters=parameters.copy(),
                    context={
                        'min_stamina': min_stamina,
                        'chaos_weight': chaos_weight,
                        'chaos_grade': chaos_grade_percent
                    }
                ))
            
            # 2. 检测NaN值
            for i, stamina in enumerate(stamina_history):
                if math.isnan(stamina) or math.isinf(stamina):
                    stability_risk += 1000.0
                    bug_reports.append(BugReport(
                        trial_number=trial_number,
                        bug_type="nan_value",
                        bug_description=f"体力值变为NaN或Inf：索引{i}, 值={stamina}",
                        parameters=parameters.copy(),
                        context={
                            'index': i,
                            'stamina_value': stamina,
                            'chaos_weight': chaos_weight
                        }
                    ))
                    break
            
            # 3. 检测数值溢出（体力值超过合理范围）
            max_stamina = max(stamina_history) if len(stamina_history) > 0 else 0.0
            if max_stamina > 1.5:  # 体力不应该超过150%
                stability_risk += 500.0
                bug_reports.append(BugReport(
                    trial_number=trial_number,
                    bug_type="overflow",
                    bug_description=f"体力值溢出：{max_stamina:.6f}（超过150%）",
                    parameters=parameters.copy(),
                    context={
                        'max_stamina': max_stamina,
                        'chaos_weight': chaos_weight
                    }
                ))
            
            # 4. 检测恢复逻辑漏洞：静态站立时恢复率为负（除非超重严重）
            # 测试静态站立恢复
            static_twin = RSSDigitalTwin(twin.constants)
            static_twin.reset()
            static_twin.stamina = 0.5  # 从50%体力开始
            static_weight = 120.0
            
            # 模拟静态站立（速度为0）
            for _ in range(10):  # 测试10个tick（2秒）
                try:
                    static_twin.step(
                        speed=0.0,
                        current_weight=static_weight,
                        grade_percent=0.0,
                        terrain_factor=1.0,
                        stance=Stance.STAND,
                        movement_type=MovementType.IDLE,
                        current_time=0.0
                    )
                except Exception:
                    pass
            
            # 检查恢复逻辑
            if len(static_twin.stamina_history) >= 2:
                stamina_change = static_twin.stamina_history[-1] - static_twin.stamina_history[-2]
                # 如果静态站立时体力下降（且不是极端超重情况），可能是逻辑漏洞
                # 注意：135kg（45KG装备+90KG体重）已经是极端情况，应该允许恢复为负
                # 只有在负重小于120kg（30KG装备）时，静态站立恢复为负才可能是BUG
                if stamina_change < -0.001 and static_weight <= 120.0:  # 只有在负重<=120kg时才视为BUG
                    stability_risk += 500.0
                    bug_reports.append(BugReport(
                        trial_number=trial_number,
                        bug_type="recovery_logic_error",
                        bug_description=f"静态站立时体力恢复率为负：{stamina_change:.6f}/tick（负重{static_weight}kg）",
                        parameters=parameters.copy(),
                        context={
                            'stamina_change_per_tick': stamina_change,
                            'test_weight': static_weight,
                            'final_stamina': static_twin.stamina_history[-1]
                        }
                    ))

            # ==================== 连续型“风险评分”（让稳定性目标有梯度）====================
            # 仅靠“是否触发BUG”会导致稳定性目标大面积为 0，从而退化为单目标优化。
            # 这里加入“接近崩溃”的软风险：在极端场景下体力越接近耗尽，风险越高。
            if stamina_history:
                min_stamina = float(min(stamina_history))
                final_stamina = float(stamina_history[-1])

                # 极端场景下希望仍保留一定体力余量（例如 >=10%）。
                stability_risk += max(0.0, 0.10 - min_stamina) * 4000.0

                # 如果最终体力也很低，说明长期稳定性差（附加软惩罚）。
                stability_risk += max(0.0, 0.05 - final_stamina) * 2000.0

                # 耗尽占比：越多 tick 处于接近耗尽，风险越大。
                near_zero_frames = sum(1 for s in stamina_history if s <= 0.01)
                near_zero_ratio = near_zero_frames / len(stamina_history)
                stability_risk += (near_zero_ratio * near_zero_ratio) * 1500.0
            
        except Exception as e:
            # 如果仿真崩溃，记录为严重BUG
            stability_risk += 2000.0
            bug_reports.append(BugReport(
                trial_number=trial_number,
                bug_type="simulation_crash",
                bug_description=f"仿真崩溃：{str(e)}",
                parameters=parameters.copy(),
                context={
                    'error': str(e),
                    'chaos_weight': chaos_weight,
                    'chaos_grade': chaos_grade_percent
                }
            ))
        
        return stability_risk, bug_reports
    
    def optimize(self, study_name: str = "rss_super_optimization") -> Dict:
        """
        执行优化
        
        Args:
            study_name: 研究名称
        
        Returns:
            优化结果字典
        """
        print("=" * 80)
        print("RSS 增强型多目标优化流水线 - NSGA-II")
        print("=" * 80)
        
        print(f"优化配置：")
        print(f"  采样次数：{self.n_trials}")
        print(f"  优化变量数：40")
        print(f"  目标函数数：4（Playability Burden, Stability Risk, Physiological Realism, Session Engagement Loss）")
        print(f"  并行线程数：{self.n_jobs}（自动检测CPU核心数）")
        print(f"  批处理大小：{self.batch_size}")
        
        print(f"\n目标函数：")
        print(f"  1. 可玩性负担（Playability Burden）- 越小越好（30KG专项测试）")
        print(f"  2. 稳定性风险（Stability Risk）- 越小越好（BUG检测）")
        print(f"  3. 生理学合理性（Physiological Realism）- 越小越好（内部加权）")
        print(f"\n基础体能标准：")
        print(f"  - ACFT 2英里测试（空载）作为最低标准，不是优化目标")
        print(f"  - 必须通过基础体能测试才能被视为有效解")
        
        print(f"\n特殊测试：")
        print(f"  - 30KG负载专项平衡测试（解决'30KG太累'问题）")
        print(f"  - 地狱级压力测试（45KG、20度坡、热应激）")
        print(f"  - BUG检测（负数体力、NaN、溢出、恢复逻辑漏洞）")
        
        print(f"\n开始优化...")
        print("-" * 80)
        
        # 启动时间记录
        start_time = time.time()
        
        # 创建研究（使用NSGA-II采样器，改进参数以增加多样性）
        if self.use_database:
            # 使用数据库存储以便后续分析（性能较慢）
            storage_url = f"sqlite:///{study_name}.db"
            try:
                # 尝试加载现有研究
                self.study = optuna.load_study(study_name=study_name, storage=storage_url)
                print(f"已加载现有研究：{study_name}（从数据库）")
            except:
                # 创建新研究
                self.study = optuna.create_study(
                    study_name=study_name,
                    storage=storage_url,
                    directions=['minimize', 'minimize', 'minimize', 'minimize'],
                    sampler=optuna.samplers.NSGAIISampler(
                        population_size=300,  # 增加种群大小以提高多样性（从200增加到300）
                        mutation_prob=0.5,  # 增加变异概率以提高探索能力（从0.4增加到0.5）
                        crossover_prob=0.9,  # 增加交叉概率以提高收敛速度
                        swapping_prob=0.5,    # 增加交换概率以提高多样性
                        seed=42  # 固定随机种子，提高可重复性
                    ),
                    pruner=optuna.pruners.MedianPruner(),
                    load_if_exists=True
                )
                print(f"已创建新研究：{study_name}（保存到数据库，性能较慢）")
        else:
            # 使用内存存储（默认，性能更快）
            self.study = optuna.create_study(
                study_name=study_name,
                directions=['minimize', 'minimize', 'minimize'],
                sampler=optuna.samplers.NSGAIISampler(
                    population_size=300,  # 增加种群大小以提高多样性（从200增加到300）
                    mutation_prob=0.5,  # 增加变异概率以提高探索能力（从0.4增加到0.5）
                    crossover_prob=0.9,  # 增加交叉概率以提高收敛速度
                    swapping_prob=0.5,    # 增加交换概率以提高多样性
                    seed=42  # 固定随机种子，提高可重复性
                ),
                pruner=optuna.pruners.MedianPruner()
            )
            print(f"已创建新研究：{study_name}（内存存储，高性能）")
        
        # 进度日志回调函数
        def progress_callback(study, trial):
            if trial.number % 100 == 0 and trial.number > 0:
                best_trials = study.best_trials
                if len(best_trials) > 0:
                    playability_values = [t.values[0] for t in best_trials]
                    stability_values = [t.values[1] for t in best_trials]
                    realism_values = [t.values[2] for t in best_trials]
                    engagement_values = [t.values[3] for t in best_trials]
                    elapsed_time = time.time() - start_time
                    print(f"\n进度更新 [试验 {trial.number}/{self.n_trials}]: "
                          f"帕累托解数量={len(best_trials)}, "
                          f"可玩性=[{min(playability_values):.2f}, {max(playability_values):.2f}], "
                          f"稳定性=[{min(stability_values):.2f}, {max(stability_values):.2f}], "
                          f"生理学=[{min(realism_values):.2f}, {max(realism_values):.2f}], "
                          f"会话体验损失=[{min(engagement_values):.2f}, {max(engagement_values):.2f}], "
                          f"BUG数量={len(self.bug_reports)}, "
                          f"耗时={elapsed_time:.2f}秒")

        
        # ==================== 多阶段优化 ====================
        # 阶段1：优先优化稳定性（50%迭代）
        print("\n【阶段1】：优先优化稳定性")
        print("目标：降低稳定性风险，确保系统不崩溃")
        print("策略：增加稳定性风险的惩罚权重")
        
        # 保存原始权重
        original_weight = getattr(self, '_stability_weight', 1.0)
        self._stability_weight = 5.0  # 增加稳定性权重
        
        # 执行阶段1优化（50%迭代）
        stage1_trials = int(self.n_trials * 0.5)
        self.study.optimize(
            self.objective,
            n_trials=stage1_trials,
            show_progress_bar=True,
            callbacks=[progress_callback],
            n_jobs=1  # 设置为1，因为objective内部已使用ParallelWorker进行并行
        )
        
        # 阶段2：平衡优化（剩余50%迭代）
        print("\n【阶段2】：平衡优化稳定性和生理学合理性")
        print("目标：在稳定基础上，优化生理学合理性")
        print("策略：恢复正常权重，平衡各项指标")
        
        # 恢复正常权重
        self._stability_weight = 1.0
        
        # 执行阶段2优化（剩余50%迭代）
        stage2_trials = self.n_trials - stage1_trials
        self.study.optimize(
            self.objective,
            n_trials=stage2_trials,
            show_progress_bar=True,
            callbacks=[progress_callback],
            n_jobs=1  # 设置为1，因为objective内部已使用ParallelWorker进行并行
        )
        
        print("-" * 80)
        print(f"\n优化完成！")
        
        # 提取帕累托前沿
        self.best_trials = self.study.best_trials
        
        print(f"\n帕累托前沿：")
        print(f"  非支配解数量：{len(self.best_trials)}")
        
        if len(self.best_trials) > 0:
            playability_values = [trial.values[0] for trial in self.best_trials]
            stability_values = [trial.values[1] for trial in self.best_trials]
            realism_values = [trial.values[2] for trial in self.best_trials]
            engagement_values = [trial.values[3] for trial in self.best_trials]
            
            print(f"  可玩性负担范围：[{min(playability_values):.2f}, {max(playability_values):.2f}]")
            print(f"  稳定性风险范围：[{min(stability_values):.2f}, {max(stability_values):.2f}]")
            print(f"  生理学合理性范围：[{min(realism_values):.2f}, {max(realism_values):.2f}]")
            print(f"  会话体验损失范围：[{min(engagement_values):.2f}, {max(engagement_values):.2f}]")
            
            # 诊断：检查目标值的唯一性
            playability_unique = len(set(playability_values))
            stability_unique = len(set(stability_values))
            realism_unique = len(set(realism_values))
            engagement_unique = len(set(engagement_values))
            
            print(f"\n  目标值唯一性诊断：")
            print(f"    可玩性负担唯一值：{playability_unique}/{len(self.best_trials)}")
            print(f"    稳定性风险唯一值：{stability_unique}/{len(self.best_trials)}")
            print(f"    生理学合理性唯一值：{realism_unique}/{len(self.best_trials)}")
            print(f"    会话体验损失唯一值：{engagement_unique}/{len(self.best_trials)}")
            
            if playability_unique == 1 and stability_unique == 1 and realism_unique == 1:
                print(f"\n  ⚠️ 警告：所有帕累托解的目标值完全相同！")
                print(f"     这可能表示：")
                print(f"     1. 优化器收敛到了单一最优解")
                print(f"     2. 三个目标函数过于一致，导致所有解相同")
                print(f"     3. 需要增加优化迭代次数或调整NSGA-II参数")
            
            # 诊断：检查参数多样性
            if len(self.best_trials) > 1:
                param_names = list(self.best_trials[0].params.keys())
                identical_params = []
                diverse_params = []
                
                for param_name in param_names:
                    values = [t.params[param_name] for t in self.best_trials]
                    unique_values = len(set(values))
                    if unique_values == 1:
                        identical_params.append(param_name)
                    else:
                        diverse_params.append((param_name, unique_values))
                
                print(f"\n  参数多样性诊断：")
                print(f"    完全相同参数数：{len(identical_params)}/{len(param_names)}")
                print(f"    有差异参数数：{len(diverse_params)}/{len(param_names)}")
                
                if len(diverse_params) == 0:
                    print(f"\n  ⚠️ 警告：所有参数在所有解中都完全相同！")
                    print(f"     这说明帕累托前沿可能真的只有一个解")
                elif len(diverse_params) < 5:
                    print(f"\n  ⚠️ 警告：参数多样性不足（只有{len(diverse_params)}个参数有差异）")
                    print(f"     建议增加优化迭代次数或调整NSGA-II参数")
                
                # 显示有差异的参数（前10个）
                if len(diverse_params) > 0:
                    diverse_params.sort(key=lambda x: x[1], reverse=True)
                    print(f"\n    有差异的参数（前10个）：")
                    for param_name, unique_count in diverse_params[:10]:
                        print(f"      {param_name}: {unique_count} 个唯一值")
        
        print(f"\nBUG检测统计：")
        print(f"  总BUG数量：{len(self.bug_reports)}")
        bug_types = {}
        for bug in self.bug_reports:
            bug_types[bug.bug_type] = bug_types.get(bug.bug_type, 0) + 1
        for bug_type, count in bug_types.items():
            print(f"    {bug_type}: {count}")
        
        # 自动导出JSON配置文件
        if len(self.best_trials) > 0:
            archetypes = self.extract_archetypes()
            print("\n" + "=" * 80)
            print("开始自动导出JSON配置文件...")
            print("=" * 80)
            self.export_presets(archetypes, output_dir=".")
            print("\n" + "=" * 80)
            print("JSON配置文件已自动生成！")
            print("=" * 80)
            print(f"\n生成的文件位置：")
            print("- optimized_rss_config_realism_super.json")
            print("- optimized_rss_config_balanced_super.json")
            print("- optimized_rss_config_playability_super.json")
        
        return {
            'best_trials': self.best_trials,
            'n_solutions': len(self.best_trials),
            'study': self.study,
            'bug_reports': self.bug_reports
        }

    def _evaluate_session_metrics(self, twin: RSSDigitalTwin, durations_minutes: List[int] = [40, 60]) -> Tuple[float, bool]:
        """
        评估会话体验相关指标并检查硬约束（针对40/60分钟会话）

        Returns:
            (engagement_loss, session_fail_flag)
        """
        # 使用若干代表性短时段组合来估计40/60分钟会话行为，避免每试验完全模拟整段40-60分钟（性能考虑）
        # 核心思路：运行若干关键事件样本（巡逻段、短冲刺、恢复片段、ACFT 验证），然后按权重合成会话指标

        def run_sample(duration_seconds: float, speed_profile, current_weight):
            try:
                twin_sample = RSSDigitalTwin(twin.constants)
                twin_sample.reset()
                results = twin_sample.simulate_scenario(
                    speed_profile=speed_profile,
                    current_weight=current_weight,
                    grade_percent=0.0,
                    terrain_factor=1.0,
                    stance=Stance.STAND,
                    movement_type=MovementType.RUN,
                    enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION
                )
                return results
            except Exception as e:
                print(f"会话样本仿真失败: {e}")
                return None

        # 1) 巡逻样本（10分钟）：混合走/跑/冲刺片段
        patrol_profile = [(0, 2.8), (120, 3.5), (180, 2.8), (300, 3.0), (600, 2.5)]  # 10分钟示例
        patrol_res = run_sample(600.0, patrol_profile, 90.0 + 30.0)

        # 2) 短冲刺样本（30秒） + 60s 恢复，检测恢复窗
        sprint_profile = [(0, 5.0), (30, 0.0), (90, 0.0)]
        sprint_res = run_sample(90.0, sprint_profile, 90.0)

        # 3) ACFT 验证样本（空载）
        acft_res = None
        try:
            acft = ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0)
            acft_res = twin.simulate_scenario(
                speed_profile=acft.speed_profile,
                current_weight=acft.current_weight,
                grade_percent=acft.grade_percent,
                terrain_factor=acft.terrain_factor,
                stance=acft.stance,
                movement_type=acft.movement_type,
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION
            )
        except Exception:
            acft_res = None

        # 如果任何样本失败，则标记为会话失败（严谨处理）
        if patrol_res is None or sprint_res is None or acft_res is None:
            return 1e6, True

        # 汇总巡逻样本的时间窗口占比（体力在20%-80%之间）
        patrol_hist = patrol_res.get('stamina_history', [])
        if patrol_hist:
            patrol_time_in_window = sum(1 for s in patrol_hist if 0.20 <= s <= 0.80) / len(patrol_hist)
        else:
            patrol_time_in_window = 0.0

        # 估算在40/60分钟会话中 time_in_window 的加权值（以巡逻样本为主）
        # 这里使用巡逻样本占70%，其余30%由其他事件影响
        time_in_window_est = patrol_time_in_window * 0.7

        # sprint 恢复片段：测量30s冲刺后60s恢复的体力恢复量
        sprint_hist = sprint_res.get('stamina_history', [])
        if len(sprint_hist) >= 3:
            # sprint_hist: 短冲刺结束时体力在某值，接下来为静止恢复区段
            sprint_end = sprint_hist[ int(30.0 / 0.2) - 1 ] if int(30.0 / 0.2) - 1 < len(sprint_hist) else sprint_hist[-1]
            recovery_after_60s = sprint_hist[-1] - sprint_end
        else:
            recovery_after_60s = 0.0

        # 检查恢复窗硬约束：60s 内恢复 >= 2%（0.02）——保守放宽，避免过多合理解被淘汰
        recovery_ok = recovery_after_60s >= 0.02

        # ACFT 最低体力硬约束 >= 25%（从30%放宽到25%以匹配实际平衡预设）
        acft_min = acft_res.get('min_stamina', 0.0) if acft_res else 0.0
        acft_ok = acft_min >= 0.25

        # 不可复原崩溃：任何样本的 min_stamina <= 0.01 视为失败
        min_stamina_overall = min(
            min(patrol_res.get('stamina_history', [1.0])),
            min(sprint_res.get('stamina_history', [1.0])),
            min(acft_res.get('stamina_history', [1.0]))
        )
        no_permanent_incapacitation = min_stamina_overall > 0.01

        # low_events：巡逻样本中低于10%的次数（按分钟标准化）
        patrol_low_count = sum(1 for s in patrol_hist if s < 0.10)
        patrol_minutes = len(patrol_hist) * 0.2 / 60.0 if len(patrol_hist) > 0 else 1.0
        low_events_per_min = patrol_low_count / max(1.0, patrol_minutes)

        # 最终体力（估算）：使用巡逻样本末端体力作为会话估算（保守）
        final_stamina_est = patrol_hist[-1] if len(patrol_hist) > 0 else 1.0

        # engagement_score 计算：优先保证 player 在 20%-80% 区间内的时间占比
        # engagement_loss = 100 - 100*(time_in_window_est)
        engagement_loss = 100.0 - 100.0 * time_in_window_est

        # 如果 final_stamina_est 不在 [0.25, 0.70] 区间，增加额外惩罚
        if final_stamina_est < 0.25:
            engagement_loss += (0.25 - final_stamina_est) * 200.0  # 越低惩罚越大
        elif final_stamina_est > 0.70:
            engagement_loss += (final_stamina_est - 0.70) * 50.0  # 过高也略微惩罚（过轻松）

        # 增加 low_events 惩罚：每分钟低于10% 的次数越多，体验越差
        engagement_loss += min(low_events_per_min, 10.0) * 10.0

        # 会话失败判定（硬约束）：ACFT / 恢复窗 / 不可复原崩溃
        session_fail = not (acft_ok and recovery_ok and no_permanent_incapacitation)

        return float(engagement_loss), bool(session_fail)
    
    def shutdown(self):
        """关闭并行工作器"""
        if self.parallel_worker:
            self.parallel_worker.shutdown()
    
    def _trial_has_minimum_consumption(self, trial) -> bool:
        """验证 trial 是否满足最低消耗要求（Run 60s 至少 3%，Sprint 30s 至少 4%）"""
        param_to_attr = {
            'energy_to_stamina_coeff': 'ENERGY_TO_STAMINA_COEFF',
            'sprint_stamina_drain_multiplier': 'SPRINT_STAMINA_DRAIN_MULTIPLIER',
            'base_recovery_rate': 'BASE_RECOVERY_RATE',
            'aerobic_efficiency_factor': 'AEROBIC_EFFICIENCY_FACTOR',
            'anaerobic_efficiency_factor': 'ANAEROBIC_EFFICIENCY_FACTOR',
        }
        constants = RSSConstants()
        for pk, attr in param_to_attr.items():
            if pk in trial.params and hasattr(constants, attr):
                setattr(constants, attr, trial.params[pk])
        twin = RSSDigitalTwin(constants)
        twin.reset()
        twin.stamina = 1.0
        for i in range(300):
            twin.step(3.7, 90.0, 0.0, 1.0, 0, MovementType.RUN, i * 0.2, False)
        run_drop = 1.0 - twin.stamina
        twin.reset()
        twin.stamina = 1.0
        for i in range(150):
            twin.step(5.0, 90.0, 0.0, 1.0, 0, MovementType.SPRINT, i * 0.2, False)
        sprint_drop = 1.0 - twin.stamina
        return run_drop >= 0.03 and sprint_drop >= 0.04

    def extract_archetypes(self) -> Dict[str, Dict]:
        """
        从帕累托前沿自动提取三个预设（改进：确保选择不同的解，且满足最低消耗）
        
        Returns:
            预设参数字典
        """
        if not self.best_trials or len(self.best_trials) == 0:
            raise ValueError("请先运行优化！")
        
        # 如果只有一个解，警告并返回相同配置
        if len(self.best_trials) == 1:
            single_trial = self.best_trials[0]
            return {
                'EliteStandard': {
                    'params': single_trial.params,
                    'playability_burden': single_trial.values[0],
                    'stability_risk': single_trial.values[1]
                },
                'StandardMilsim': {
                    'params': single_trial.params,
                    'playability_burden': single_trial.values[0],
                    'stability_risk': single_trial.values[1]
                },
                'TacticalAction': {
                    'params': single_trial.params,
                    'playability_burden': single_trial.values[0],
                    'stability_risk': single_trial.values[1]
                }
            }
        
        # 提取目标值
        playability_values = [t.values[0] for t in self.best_trials]
        stability_values = [t.values[1] for t in self.best_trials]
        engagement_values = [t.values[3] for t in self.best_trials]
        
        # 过滤：只保留满足最低消耗的 trial（Run 60s>=3%, Sprint 30s>=4%）
        valid_trials = [(i, t) for i, t in enumerate(self.best_trials) if self._trial_has_minimum_consumption(t)]
        if not valid_trials:
            print("警告：帕累托前沿中无满足最低消耗的解，使用全部 trial（可能消耗为 0）")
            valid_indices = list(range(len(self.best_trials)))
            valid_trial_list = self.best_trials
        else:
            valid_indices = [v[0] for v in valid_trials]
            valid_trial_list = [v[1] for v in valid_trials]
            playability_values = [self.best_trials[i].values[0] for i in valid_indices]
            stability_values = [self.best_trials[i].values[1] for i in valid_indices]
            engagement_values = [self.best_trials[i].values[3] for i in valid_indices]
        # 不跨预设交换参数（避免产生不一致的 param 组合）
        candidates = valid_trial_list if valid_trials else self.best_trials
        cand_indices = valid_indices if valid_trials else list(range(len(self.best_trials)))

        def energy_coeff(t):
            return t.params.get('energy_to_stamina_coeff', 0)

        # 1. Realism (EliteStandard): 稳定性好的 trial 中选 energy_coeff 最高的
        stability_ordered = sorted(cand_indices, key=lambda i: self.best_trials[i].values[1])
        top_stability = stability_ordered[: max(3, len(cand_indices) // 3)]
        realism_idx = max(top_stability, key=lambda i: energy_coeff(self.best_trials[i]))
        realism_trial = self.best_trials[realism_idx]

        # 2. Playability (TacticalAction): 可玩性好的 trial 中选 energy_coeff 最低的
        playability_ordered = sorted(cand_indices, key=lambda i: self.best_trials[i].values[0])
        top_playability = playability_ordered[: max(3, len(cand_indices) // 3)]
        playability_idx = min(top_playability, key=lambda i: energy_coeff(self.best_trials[i]))
        playability_trial = self.best_trials[playability_idx]

        # 3. Balanced: 综合目标平衡且 energy_coeff 居中
        p_vals = [t.values[0] for t in candidates]
        s_vals = [t.values[1] for t in candidates]
        playability_min, playability_max = min(p_vals), max(p_vals)
        stability_min, stability_max = min(s_vals), max(s_vals)
        e_vals = [energy_coeff(t) for t in candidates]
        # 某些情况下 engagement 会影响平衡选择：优先选择 engagement 较低的解
        engagement_vals = [t.values[3] for t in candidates]
        e_min, e_max = min(e_vals), max(e_vals)
        balanced_scores = []
        for trial in candidates:
            p_norm = (trial.values[0] - playability_min) / (playability_max - playability_min + 1e-10)
            s_norm = (trial.values[1] - stability_min) / (stability_max - stability_min + 1e-10)
            e_norm = (energy_coeff(trial) - e_min) / (e_max - e_min + 1e-10)
            # 平衡解：目标折中，且 energy 居中（偏好中等严格程度）
            engagement_norm = (trial.values[3] - min(engagement_vals)) / (max(engagement_vals) - min(engagement_vals) + 1e-10)
            # 平衡解：目标折中，偏好 engagement 小（体验损失小）
            balanced_scores.append((p_norm + s_norm) / 2.0 - 0.2 * (0.5 - abs(e_norm - 0.5)) + 0.3 * engagement_norm)
        balanced_in_candidates = np.argmin(balanced_scores)
        balanced_trial = candidates[balanced_in_candidates]
        balanced_idx = cand_indices[balanced_in_candidates]
        
        # 改进：如果选择了相同的解，尝试从不同区域选择
        selected_indices = set([realism_idx, playability_idx, balanced_idx])
        
        # 如果三个解都相同，尝试基于参数差异选择不同的解
        if len(selected_indices) == 1:
            # 计算所有解之间的参数差异
            param_differences = []
            for i, trial_i in enumerate(self.best_trials):
                for j, trial_j in enumerate(self.best_trials):
                    if i < j:
                        params_i = np.array(list(trial_i.params.values()))
                        params_j = np.array(list(trial_j.params.values()))
                        diff = np.linalg.norm(params_i - params_j)
                        param_differences.append((i, j, diff))
            
            # 按差异排序，选择差异最大的三个解
            if len(param_differences) > 0:
                param_differences.sort(key=lambda x: x[2], reverse=True)
                unique_indices = set()
                for i, j, _ in param_differences:
                    unique_indices.add(i)
                    unique_indices.add(j)
                    if len(unique_indices) >= 3:
                        break
                
                if len(unique_indices) >= 3:
                    unique_indices_list = list(unique_indices)[:3]
                    stability_idx = unique_indices_list[0]
                    playability_idx = unique_indices_list[1]
                    balanced_idx = unique_indices_list[2]
                    realism_trial = self.best_trials[stability_idx]
                    playability_trial = self.best_trials[playability_idx]
                    balanced_trial = self.best_trials[balanced_idx]
        
        # 如果只有两个不同的解，确保至少有一个不同
        elif len(selected_indices) == 2:
            available_indices = set(range(len(self.best_trials))) - selected_indices
            if len(available_indices) > 0:
                # 选择与已选解差异最大的解
                best_diff_idx = None
                best_diff = -1
                for idx in available_indices:
                    trial = self.best_trials[idx]
                    avg_diff = 0.0
                    for selected_idx in selected_indices:
                        selected_trial = self.best_trials[selected_idx]
                        params = np.array(list(trial.params.values()))
                        selected_params = np.array(list(selected_trial.params.values()))
                        diff = np.linalg.norm(params - selected_params)
                        avg_diff += diff
                    avg_diff /= len(selected_indices)
                    if avg_diff > best_diff:
                        best_diff = avg_diff
                        best_diff_idx = idx
                
                if best_diff_idx is not None:
                    balanced_idx = best_diff_idx
                    balanced_trial = self.best_trials[balanced_idx]
        
        return {
            'EliteStandard': {
                'params': realism_trial.params,
                'playability_burden': realism_trial.values[0],
                'stability_risk': realism_trial.values[1]
            },
            'StandardMilsim': {
                'params': balanced_trial.params,
                'playability_burden': balanced_trial.values[0],
                'stability_risk': balanced_trial.values[1]
            },
            'TacticalAction': {
                'params': playability_trial.params,
                'playability_burden': playability_trial.values[0],
                'stability_risk': playability_trial.values[1]
            }
        }
    
    def export_presets(self, archetypes: Dict[str, Dict], output_dir: str = "."):
        """
        导出预设配置到JSON文件
        
        Args:
            archetypes: 预设参数字典
            output_dir: 输出目录
        """
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        preset_mapping = {
            'EliteStandard': 'optimized_rss_config_realism_super.json',
            'StandardMilsim': 'optimized_rss_config_balanced_super.json',
            'TacticalAction': 'optimized_rss_config_playability_super.json'
        }
        
        for preset_name, config in archetypes.items():
            output_file = output_path / preset_mapping[preset_name]
            
            config_dict = {
                "version": "3.0.0",
                "description": f"RSS 增强型优化配置（NSGA-II）- {preset_name}",
                "optimization_method": "NSGA-II (Super Pipeline)",
                "objectives": {
                    "playability_burden": config['playability_burden'],
                    "stability_risk": config['stability_risk']
                },
                "parameters": config['params']
            }
            
            with open(output_file, 'w', encoding='utf-8') as f:
                json.dump(config_dict, f, indent=2, ensure_ascii=False)
            
            print(f"  预设已导出：{output_file}")
    
    def export_bug_report(self, output_path: str = "stability_bug_report.json"):
        """
        导出BUG报告
        
        Args:
            output_path: 输出文件路径
        """
        # 转换为可序列化的字典
        bug_reports_dict = []
        for bug in self.bug_reports:
            bug_dict = asdict(bug)
            bug_reports_dict.append(bug_dict)
        
        report = {
            "version": "1.0.0",
            "description": "RSS 稳定性BUG检测报告",
            "total_bugs": len(self.bug_reports),
            "bug_summary": {
                bug_type: sum(1 for b in self.bug_reports if b.bug_type == bug_type)
                for bug_type in set(b.bug_type for b in self.bug_reports)
            },
            "bug_reports": bug_reports_dict
        }
        
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        
        print(f"\nBUG报告已导出：{output_path}")
        print(f"  总BUG数量：{len(self.bug_reports)}")
        for bug_type, count in report['bug_summary'].items():
            print(f"    {bug_type}: {count}")


def main():
    """主函数：执行完整优化流程"""
    
    print("\n" + "=" * 80)
    print("RSS 增强型多目标优化与鲁棒性测试流水线")
    print("Enhanced Multi-Objective Optimization with Robustness Testing Pipeline")
    print("=" * 80)
    
    # 使用CPU核心数作为默认线程数
    import multiprocessing
    n_jobs = multiprocessing.cpu_count()
    
    # 询问是否启动GUI
    import sys
    use_gui = "--gui" in sys.argv or "--no-gui" not in sys.argv
    
    if use_gui:
        print("正在启动GUI优化器...")
        
        # 导入GUI模块
        try:
            from rss_optimizer_gui import RSSTunerGUI
        except ImportError:
            print("错误：无法导入GUI模块")
            print("请确保 rss_optimizer_gui.py 在同一目录下")
            use_gui = False
    
    # 创建流水线（快速验证设置）
    # 设置为500次迭代以快速验证优化效果
    # use_database=False 使用内存存储以提高性能（默认）
    # 如果需要后续诊断，可以设置 use_database=True（但会降低性能）
    pipeline = RSSSuperPipeline(n_trials=500, n_jobs=n_jobs, use_database=False)
    
    # 如果使用GUI，设置回调函数
    if use_gui:
        # 导入GUI模块并启动
        try:
            from rss_optimizer_gui import RSSTunerGUI
            import tkinter as tk
            import threading
            
            # 创建GUI实例，传递总迭代次数
            root = tk.Tk()
            gui = RSSTunerGUI(root, total_iterations=500)
            
            # 设置GUI更新回调
            pipeline.set_gui_callback(gui.update_data)
            
            print("GUI已启动，优化将在后台运行...")
            print("提示：GUI窗口可以独立控制优化过程")
            
            # 在单独线程中运行优化
            optimization_thread = threading.Thread(
                target=lambda: pipeline.optimize(study_name="rss_super_optimization"),
                daemon=True
            )
            optimization_thread.start()
            
            # 在主线程中运行GUI
            root.mainloop()
            
            # 等待优化线程完成
            optimization_thread.join()
            
            # 自动提取预设
            archetypes = pipeline.extract_archetypes()
            
            # 导出预设配置
            pipeline.export_presets(archetypes, output_dir=".")
            
            # 导出BUG报告
            pipeline.export_bug_report("stability_bug_report.json")
            
            print("\n" + "=" * 80)
            print("优化流程完成！")
            print("=" * 80)
            print("\n生成的文件：")
            print("  1. optimized_rss_config_realism_super.json - EliteStandard 预设")
            print("  2. optimized_rss_config_balanced_super.json - StandardMilsim 预设")
            print("  3. optimized_rss_config_playability_super.json - TacticalAction 预设")
            print("  4. stability_bug_report.json - 稳定性BUG检测报告")
            print("\n建议：")
            print("  - 硬核 Milsim 服务器：使用 EliteStandard 预设")
            print("  - 公共服务器：使用 StandardMilsim 预设")
            print("  - 休闲服务器：使用 TacticalAction 预设（30KG最轻松）")
            print("\nBUG报告说明：")
            print("  - 查看 stability_bug_report.json 了解所有导致数值崩溃的参数组合")
            print("  - 这些信息可用于排查公式漏洞和边界条件问题")
        except ImportError:
            print("错误：无法导入GUI模块")
            print("请确保 rss_optimizer_gui.py 在同一目录下")
            print("继续使用命令行模式...")
            use_gui = False
    else:
        # 执行优化（不使用GUI）
        results = pipeline.optimize(study_name="rss_super_optimization")
        
        # 自动提取预设
        archetypes = pipeline.extract_archetypes()
        
        # 导出预设配置
        pipeline.export_presets(archetypes, output_dir=".")
        
        # 导出BUG报告
        pipeline.export_bug_report("stability_bug_report.json")
        
        print("\n" + "=" * 80)
        print("优化流程完成！")
        print("=" * 80)
        print("\n生成的文件：")
        print("  1. optimized_rss_config_realism_super.json - EliteStandard 预设")
        print("  2. optimized_rss_config_balanced_super.json - StandardMilsim 预设")
        print("  3. optimized_rss_config_playability_super.json - TacticalAction 预设")
        print("  4. stability_bug_report.json - 稳定性BUG检测报告")
        print("\n建议：")
        print("  - 硬核 Milsim 服务器：使用 EliteStandard 预设")
        print("  - 公共服务器：使用 StandardMilsim 预设")
        print("  - 休闲服务器：使用 TacticalAction 预设（30KG最轻松）")
        print("\nBUG报告说明：")
        print("  - 查看 stability_bug_report.json 了解所有导致数值崩溃的参数组合")
        print("  - 这些信息可用于排查公式漏洞和边界条件问题")


if __name__ == '__main__':
    main()
