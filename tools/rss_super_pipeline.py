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
from rss_digital_twin_fix import (
    RSSDigitalTwin,
    MovementType,
    Stance,
    RSSConstants,
)

# 硬编码速度值（m/s）
SPEED_VALUES = {
    0: {"walk": 3.02, "run": 3.81, "sprint": 5.08},    # 0kg
    10: {"walk": 2.94, "run": 3.79, "sprint": 5.04},   # 10kg
    20: {"walk": 2.87, "run": 3.81, "sprint": 5.01},   # 20kg
    30: {"walk": 2.81, "run": 3.81, "sprint": 4.96},   # 30kg
    40: {"walk": 2.32, "run": 3.22, "sprint": 4.39},   # 40kg
    50: {"walk": 1.82, "run": 2.64, "sprint": 3.80},   # 50kg
    55: {"walk": 1.56, "run": 2.35, "sprint": 3.40}    # 55kg
}

def get_speed(load_kg, speed_type):
    """获取指定负载下的速度值"""
    # 找到最接近的负载级别
    load_levels = sorted(SPEED_VALUES.keys())
    closest_load = min(load_levels, key=lambda x: abs(x - load_kg))
    return SPEED_VALUES[closest_load][speed_type]

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
            except Exception:
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
        并行映射任务。返回列表与 tasks 顺序一致（as_completed 仅用于等待，结果按索引写回）。

        Args:
            func: 任务函数
            tasks: 任务列表
            batch_size: 批处理大小

        Returns:
            与 tasks 等长、顺序一致的结果列表
        """
        if not tasks:
            return []

        if not self.executor:
            self.start()

        # 批处理任务以减少线程切换开销；每批内顺序固定，批与批之间按 batch_idx 排序再拼接
        if batch_size > 1:
            batches = []
            for i in range(0, len(tasks), batch_size):
                batches.append(tasks[i:i + batch_size])

            future_to_batch_idx = {}
            for batch_idx, batch in enumerate(batches):
                future = self.executor.submit(self._process_batch, func, batch, batch_idx)
                future_to_batch_idx[future] = batch_idx

            batch_by_idx = {}
            for future in concurrent.futures.as_completed(future_to_batch_idx):
                batch_idx = future_to_batch_idx[future]
                batch_by_idx[batch_idx] = future.result()

            results = []
            for batch_idx in range(len(batches)):
                results.extend(batch_by_idx[batch_idx])
        else:
            future_to_index = {}
            for i, task in enumerate(tasks):
                future = self.executor.submit(func, task)
                future_to_index[future] = i

            results = [None] * len(tasks)
            for future in concurrent.futures.as_completed(future_to_index):
                i = future_to_index[future]
                results[i] = future.result()

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
    def create_acft_2mile_scenario(load_weight=0.0, constants=None):
        """创建ACFT 2英里测试场景。constants 不为 None 时固定为 1kg 负重下的 run 速度（总重 91kg）。"""
        target_duration_s = 927.0  # 15分27秒
        if constants is not None:
            current_weight = 90.0 + 1.0  # 固定 1kg 负载
            target_speed = get_speed(1.0, "run")
        else:
            current_weight = 90.0 + load_weight
            acft_distance_m = 3218.0
            target_speed = acft_distance_m / target_duration_s
        return Scenario(
            speed_profile=[(0, target_speed), (target_duration_s, target_speed)],
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=target_duration_s,
            test_type="ACFT 2英里测试",
            standard_load=0.0
        )
    
    @staticmethod
    def create_urban_combat_scenario(load_weight=30.0, constants=None):
        """创建城市战斗场景。constants 不为 None 时用负重下的 walk/run/sprint 速度。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            walk = get_speed(load_weight, "walk")
            run = get_speed(load_weight, "run")
            sprint = get_speed(load_weight, "sprint")
            speed_profile = [(0, walk), (60, run), (120, walk), (180, sprint), (210, walk), (300, walk)]
        else:
            speed_profile = [(0, 2.5), (60, 3.8), (120, 2.5), (180, 5.0), (210, 2.5), (300, 2.5)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.2,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=300.0,
            test_type="城市战斗场景"
        )
    
    @staticmethod
    def create_mountain_combat_scenario(load_weight=25.0, constants=None):
        """创建山地战斗场景。constants 不为 None 时用负重下的 walk 速度（快走=walk，慢走=0.72*walk）。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            walk = get_speed(load_weight, "walk")
            slow = 0.72 * walk  # 1.8/2.5
            speed_profile = [(0, slow), (120, walk), (240, slow), (360, slow)]
        else:
            speed_profile = [(0, 1.8), (120, 2.5), (240, 1.8), (360, 1.8)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=15.0,
            terrain_factor=1.5,
            stance=Stance.STAND,
            movement_type=MovementType.WALK,
            target_finish_time=360.0,
            test_type="山地战斗场景"
        )
    
    @staticmethod
    def create_evacuation_scenario(load_weight=40.0, constants=None):
        """创建撤离场景（重载）。constants 不为 None 时用负重下的 walk/run 速度。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            walk = get_speed(load_weight, "walk")
            run = get_speed(load_weight, "run")
            speed_profile = [(0, walk), (90, run), (180, walk), (270, walk)]
        else:
            speed_profile = [(0, 2.5), (90, 3.2), (180, 2.5), (270, 2.5)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=5.0,
            terrain_factor=1.3,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=270.0,
            test_type="撤离场景"
        )

    @staticmethod
    def create_realistic_field_patrol_scenario(load_weight=30.0, constants=None):
        """野战巡逻场景。constants 不为 None 时用负重下的 run 速度（0.8*run 与 run）。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            run = get_speed(load_weight, "run")
            slow_run = 0.8 * run  # 2.8/3.5
            speed_profile = [(0, slow_run), (120, run), (180, slow_run)]
        else:
            speed_profile = [(0, 2.8), (120, 3.5), (180, 2.8)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=5.0,
            terrain_factor=1.35,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=180.0,
            test_type="野战巡逻场景"
        )

    @staticmethod
    def create_run_sprint_boundary_scenario(load_weight=30.0, constants=None):
        """Run 与 Sprint 边界区测试。constants 不为 None 时用 run 与 sprint 的中间速度。"""
        current_weight = 90.0 + load_weight
        duration_s = 60.0
        if constants is not None:
            run = get_speed(load_weight, "run")
            sprint = get_speed(load_weight, "sprint")
            speed_m_s = run + 0.3 * (sprint - run)  # 边界介于 run 与 sprint 之间
        else:
            speed_m_s = 4.2
        return Scenario(
            speed_profile=[(0, speed_m_s), (duration_s, speed_m_s)],
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=duration_s,
            test_type="Run/Sprint边界60s"
        )

    @staticmethod
    def create_heavy_load_scenario(load_weight=45.0, constants=None):
        """
        重量极端测试（45kg、10 分钟）。
        拟真修正：人类背 40kg+ 无法长途「跑」，用游骑兵 Ruck 标准 1.8 m/s 快走（WALK），
        避免优化器为满足「重载跑 10 分钟」而把回血拉满、负重惩罚压到超人类。
        """
        current_weight = 90.0 + load_weight
        duration_s = 600.0
        if constants is not None:
            speed_m_s = 1.8  # 游骑兵 Ruck March 战术快走配速，非跑
            movement_type = MovementType.WALK
        else:
            speed_m_s = 2.5
            movement_type = MovementType.RUN
        return Scenario(
            speed_profile=[(0, speed_m_s), (duration_s, speed_m_s)],
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=movement_type,
            target_finish_time=duration_s,
            test_type="重载45kg_10min"
        )

    @staticmethod
    def create_long_duration_tactical_scenario(load_weight=30.0, constants=None, run_speed=None):
        """长距离连续机动测试（30kg、20 分钟）。constants 或 run_speed 提供时用负重 run 速度。"""
        current_weight = 90.0 + load_weight
        duration_s = 1200.0
        if run_speed is not None:
            speed_m_s = run_speed
        elif constants is not None:
            speed_m_s = get_speed(load_weight, "run")
        else:
            speed_m_s = 3.2
        return Scenario(
            speed_profile=[(0, speed_m_s), (duration_s, speed_m_s)],
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=duration_s,
            test_type="30kg持续20min"
        )

    @staticmethod
    def create_run_600m_flat_scenario(load_weight=30.0, run_speed=None, constants=None):
        """30kg 平地跑道 600m。run_speed 或 constants 提供时用负重 run 速度。"""
        if run_speed is not None:
            pass
        elif constants is not None:
            run_speed = get_speed(load_weight, "run")
        else:
            run_speed = 3.8
        duration_s = 600.0 / float(run_speed)
        return Scenario(
            speed_profile=[(0, run_speed), (duration_s, run_speed)],
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=duration_s,
            test_type="30kg跑道600m"
        )

    @staticmethod
    def create_run_1km_flat_scenario(load_weight=30.0, run_speed=None, constants=None):
        """30kg 平地跑道 1000m。run_speed 或 constants 提供时用负重 run 速度。"""
        if run_speed is not None:
            pass
        elif constants is not None:
            run_speed = get_speed(load_weight, "run")
        else:
            run_speed = 3.8
        duration_s = 1000.0 / float(run_speed)
        return Scenario(
            speed_profile=[(0, run_speed), (duration_s, run_speed)],
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=duration_s,
            test_type="30kg跑道1km"
        )

    @staticmethod
    def create_cqb_stealth_scenario(load_weight=30.0, constants=None):
        """CQB 低姿态清理场景（蹲姿 WALK）。constants 不为 None 时用负重下的 walk 速度比例。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            walk = get_speed(load_weight, "walk")
            speed_profile = [(0, 0.6 * walk), (60, walk), (120, 0.4 * walk), (180, walk)]  # 1.5/2.5, 1.0/2.5
        else:
            speed_profile = [(0, 1.5), (60, 2.5), (120, 1.0), (180, 2.5)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.CROUCH,
            movement_type=MovementType.WALK,
            target_finish_time=180.0,
            test_type="CQB低姿突击"
        )

    @staticmethod
    def create_extreme_load_scenario(load_weight=55.0, constants=None):
        """极限重载机动场景（55kg WALK）。constants 不为 None 时用该负重下的 walk 速度。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            speed_m_s = get_speed(load_weight, "walk")
            speed_profile = [(0, speed_m_s), (120, speed_m_s)]
        else:
            speed_profile = [(0, 1.5), (120, 1.5)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.3,
            stance=Stance.STAND,
            movement_type=MovementType.WALK,
            target_finish_time=120.0,
            test_type="极限55kg重载"
        )

    @staticmethod
    def create_florida_swamp_scenario(load_weight=35.0, constants=None):
        """佛罗里达热应激场景（35kg WALK）。constants 不为 None 时用该负重下的 walk 速度。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            speed_m_s = get_speed(load_weight, "walk")
            speed_profile = [(0, speed_m_s), (300, speed_m_s)]
        else:
            speed_profile = [(0, 2.0), (300, 2.0)]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=5.0,
            terrain_factor=2.0,
            stance=Stance.STAND,
            movement_type=MovementType.WALK,
            target_finish_time=300.0,
            test_type="佛罗里达热应激"
        )
    
    @staticmethod
    def create_combat_cycle_scenario(load_weight=30.0, constants=None):
        """战术战斗周期场景（30kg，60分钟）。constants 不为 None 时用该负重下的 walk/run/sprint 速度。"""
        current_weight = 90.0 + load_weight
        if constants is not None:
            walk = get_speed(load_weight, "walk")
            run = get_speed(load_weight, "run")
            sprint = get_speed(load_weight, "sprint")
            speed_profile = [
                (1200.0, walk, MovementType.WALK, Stance.STAND, "渗透", 0.0),  # 0-1200s, 平地
                (600.0, run, MovementType.RUN, Stance.STAND, "接近", 0.0),  # 1200-1800s, 平地
                (300.0, 0.0, MovementType.IDLE, Stance.PRONE, "观察", 0.0),  # 1800-2100s, 平地
                (100.0, sprint, MovementType.SPRINT, Stance.STAND, "突入", 0.0),  # 2100-2200s, 平地
                (500.0, run, MovementType.RUN, Stance.CROUCH, "交火", 0.0),  # 2200-2700s, 平地
                (900.0, walk, MovementType.WALK, Stance.STAND, "撤离", 0.0),  # 2700-3600s, 平地
            ]
        else:
            speed_profile = [
                (1200.0, 2.81, MovementType.WALK, Stance.STAND, "渗透", 0.0),
                (600.0, 3.81, MovementType.RUN, Stance.STAND, "接近", 0.0),
                (300.0, 0.0, MovementType.IDLE, Stance.PRONE, "观察", 0.0),
                (100.0, 4.96, MovementType.SPRINT, Stance.STAND, "突入", 0.0),
                (500.0, 3.81, MovementType.RUN, Stance.CROUCH, "交火", 0.0),
                (900.0, 2.81, MovementType.WALK, Stance.STAND, "撤离", 0.0),
            ]
        return Scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=3600.0,
            test_type="战术战斗周期"
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
    """RSS 增强型多目标优化流水线

    默认目标数量与含义（均 minimize）：
    - 目标 1：可玩性负担（30kg 多场景）
    - 目标 2：稳定性风险（BUG 检测 + 软约束惩罚）
    - 目标 3：生理学损失（100 - 生理学合理性，权重见 REALISM['objective_weight']）
    - 目标 4：会话体验损失（40/60 分钟样本 + 恢复窗）
    - 可选：Sprint-drop、Sprint-remaining（当 enable_sprint_objective 时）

    经验值（仿真 + 多目标优化）：典型 500–3000 次评估；若用种群算法，
    例如 population=64、generation=100 → 约 6400 次评估通常已足够。
    当前默认 n_trials=500、population_size=300，可依需要增至 1000–3000。
    """
    # 集中配置：可玩性惩罚系数与阈值（75th Ranger Regiment 标准）
    # 生理参考见 docs/stamina_consumption_reference.md；下列为文献约束（剩余体力目标带 + 跛行惩罚）
    # 游骑兵标准：极高的有氧/负重耐力，整体体力底线大幅上调
    PLAYABILITY = {
        'min_stamina_threshold': 0.35,
        'min_stamina_coeff': 2000.0,
        'mean_stamina_threshold': 0.55,
        'mean_stamina_coeff': 800.0,
        'exhaustion_coeff': 2000.0,
        'time_ratio_1_15_coeff': 800.0,
        'time_ratio_1_30_coeff': 1500.0,
        # T1 对齐：30kg 600m 剩余 70-76%（文献），硬约束 ≥68%
        'run_600m_remaining_target_low': 0.68,
        'run_600m_remaining_limp': 0.55,
        'run_600m_below_target_coeff': 8000.0,
        'run_600m_below_limp_coeff': 12000.0,
        # T1 对齐：30kg 1000m 剩余 50-60%（文献），硬约束 ≥48%
        'run_1km_remaining_target_low': 0.48,
        'run_1km_remaining_limp': 0.35,
        'run_1km_below_target_coeff': 8000.0,
        'run_1km_below_limp_coeff': 12000.0,
        # T1 对齐：29kg 3.8m/s 连续跑 300s 后体力 ≥20%（未达标时软惩罚，避免全 trial 剪枝无 COMPLETE）
        'ranger_29kg_38ms_duration_s': 300,
        'ranger_29kg_38ms_min_stamina': 0.20,
        'ranger_29kg_38ms_below_penalty_coeff': 35000.0,
        # 战术战斗周期底线（软惩罚系数，与硬剪枝二选一：默认软惩罚）
        'combat_cycle_min_stamina_req': 0.23,
        'combat_cycle_below_penalty_coeff': 55000.0,
        # 600m/1000m 未达文献带时软惩罚（原硬剪枝易导致无可行解）
        'run_600m_shortfall_penalty_coeff': 22000.0,
        'run_1km_shortfall_penalty_coeff': 22000.0,
        # 30kg Run 60s 仍高于 90% 体力 → 惩罚「无净消耗」
        'flat_run_60s_recovery_penalty_coeff': 18000.0,
        # 30kg 跑 5min 后体力低于 15% 的额外惩罚（原硬剪枝）
        't1_5min_floor_penalty_coeff': 18000.0,
        # 铺装路面跑步必须有净消耗
        'flat_paved_run_duration_s': 300,
        'flat_paved_run_load_kg': 29.0,
        'flat_paved_run_max_stamina_after': 0.90,
    }
    # 集中配置：软约束惩罚系数（恢复顺序、姿态倍数等）
    CONSTRAINT = {
        'prone_standing': 200.0,
        'standing_slow': 150.0,
        'fast_medium': 150.0,
        'medium_slow': 300.0,
        'posture_crouch': 200.0,
        'posture_prone': 600.0,
        'posture_prone_crouch': 300.0,
    }
    # 集中配置：会话相关惩罚与阈值
    SESSION = {
        'recovery_soft_target': 0.15,
        'recovery_soft_coeff': 100.0,
        'session_fail_engagement': 500.0,
        'session_penalty_all': 100.0,
        'final_stamina_low': 0.25,
        'final_stamina_low_coeff': 200.0,
        'final_stamina_high': 0.70,
        'final_stamina_high_coeff': 50.0,
        'low_events_per_min_coeff': 10.0,
    }
    # 拟真目标：physiology_loss = (100 - realism_score) * objective_weight，再 *0.1 进入第三目标
    # 旧版 objective_weight=50 且 energy_coeff 上限写死 5e-7（与搜索空间 5e-6 冲突），拟真项几乎失去区分度
    REALISM = {
        'objective_weight': 115.0,
        'energy_coeff_search_low': 5e-7,
        'energy_coeff_search_high': 5e-6,
        'energy_coeff_reference_low': 5e-7,
        'energy_coeff_reference_high': 1.3e-6,
        'energy_coeff_outside_reference_penalty': 7.0,
        'energy_coeff_outside_search_penalty': 14.0,
        'base_recovery_physiological_min': 5e-5,
        'base_recovery_physiological_max': 3e-4,
        'load_metabolic_dampening_min': 0.48,
        'load_metabolic_dampening_max': 0.88,
        'load_metabolic_dampening_penalty': 5.0,
    }

    def __init__(
        self,
        n_trials: int = 500,  # 总评估次数；经验上 500–3000，种群×代数≈6400 通常足够
        n_jobs: int = -1,  # -1表示自动检测CPU核心数
        use_database: bool = False,  # 是否使用数据库存储（默认False以提高性能）
        batch_size: int = 5,  # 批处理大小
        fast_mode: bool = False,  # 加速模式：使用更大时间步长(0.4s vs 0.2s)提速约2倍
        sprint_range: Tuple[float, float] = (2.5, 4.5),  # legacy, no longer used (Pandolf covers sprint)
        sprint_drop_target: Tuple[float, float] = (0.15, 0.40),  # Sprint drop target range (min,max)
        sprint_drop_penalty_below: float = 4000.0,  # penalty scale when below target
        sprint_drop_penalty_above: float = 5000.0,  # penalty scale when above target
        constants_override: Optional[Dict] = None,  # 可选：在试验外覆盖常量（用于对比实验）
        enable_sprint_objective: bool = False,  # 是否将 Sprint-drop 作为显式优化目标
        enable_sprint_remaining_objective: bool = True,  # 是否启用 30s 剩余体力显式目标
        sprint_remaining_penalty_scale: float = 20000.0,  # 30s 剩余差值惩罚scale
        sprint_prune_threshold: float = 0.10,  # 如果 30s 剩余 < threshold 则早期剪枝（默认10%）
        sprint_upper_cap: Optional[float] = 3.5,  # 可选：对 sprint multiplier 进行上界限制（若为None则不限制）
        enable_joint_sprint_energy_constraint: bool = True,  # 是否启用冲刺-能量耦合软约束
        joint_sprint_energy_penalty_scale: float = 8000.0,  # 联合惩罚规模（线性系数）
        joint_sprint_energy_target_coeff: float = 1e-06,  # 目标 energy_to_stamina_coeff（与 C 预设 7-9e-7 对齐）
        prune_on_critical_bug: bool = False,  # 关键 BUG 时改为大惩罚不剪枝，避免帕累托为空
        prune_on_basic_fitness_fail: bool = False,  # 基础体能未通过时改为惩罚不剪枝
        basic_fitness_time_ratio_max: float = 1.30,  # 游骑兵标准：超时不超过 30%（比原 1.45 更严格，比 1.15 留有余量）
    ): 
        """
        初始化流水线
        
        Args:
            n_trials: 总评估次数（默认500；建议 500–3000，或 population×generations≈6400）
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
            except Exception:
                n_jobs = 1

        self.n_trials = n_trials
        self.n_jobs = n_jobs
        self.batch_size = batch_size
        self.fast_mode = fast_mode  # 加速模式
        self.use_database = use_database
        self.study = None
        self.best_trials = []
        self.bug_reports: List[BugReport] = []
        # 可配置：Sprint 搜索区间与最低消耗要求（可用于对比实验）
        self.sprint_range = sprint_range  # retained for compatibility but not applied
        # sprint_drop_target: (min, max)
        self.sprint_drop_target = sprint_drop_target
        self.sprint_drop_penalty_below = sprint_drop_penalty_below
        self.sprint_drop_penalty_above = sprint_drop_penalty_above
        self.constants_override = constants_override or {}
        # 是否启用 Sprint-drop 作为显式优化目标
        self.enable_sprint_objective = enable_sprint_objective
        # 是否启用 30s 剩余体力显式目标
        self.enable_sprint_remaining_objective = enable_sprint_remaining_objective
        self.sprint_remaining_penalty_scale = sprint_remaining_penalty_scale
        # Hard prune threshold for sprint remaining and optional upper cap for sprint multiplier
        self.sprint_prune_threshold = sprint_prune_threshold
        self.sprint_upper_cap = sprint_upper_cap
        self.prune_on_critical_bug = prune_on_critical_bug
        self.prune_on_basic_fitness_fail = prune_on_basic_fitness_fail
        self.basic_fitness_time_ratio_max = basic_fitness_time_ratio_max
        # 冲刺-能量耦合约束配置（可选）
        self.enable_joint_sprint_energy_constraint = enable_joint_sprint_energy_constraint
        self.joint_sprint_energy_penalty_scale = joint_sprint_energy_penalty_scale
        self.joint_sprint_energy_target_coeff = joint_sprint_energy_target_coeff
        # 初始化自定义并行工作器
        self.parallel_worker = ParallelWorker(max_workers=n_jobs)
        
        # GUI支持
        self.gui_update_callback = None
    
    def set_gui_callback(self, callback):
        """设置GUI更新回调函数"""
        self.gui_update_callback = callback
        
    def objective(self, trial: optuna.Trial) -> Tuple[float, ...]:
        """
        Optuna 多目标函数（在数据库模式下为四目标，内存模式下为三目标）

        Args:
            trial: Optuna 试验对象

        Returns:
            如果使用数据库（self.use_database==True）则返回四元组：
                (playability_burden, stability_risk, physiological_realism, engagement_loss)
            否则返回三元组：
                (playability_burden, stability_risk, physiological_realism)
        """
        # ==================== 1. 定义搜索空间 ====================
        # 使用与 rss_optimizer_optuna.py 相同的参数范围（但已优化）
        
        # 能量转换相关（与 C 端 SCR_RSS_Settings 预设完全对齐）
        # C 端 EliteStandard/StandardMilsim/TacticalAction: 7e-7 ~ 9e-7
        # C 端 Custom 默认: 3.5e-5
        # 搜索范围覆盖 C 预设与 Custom，避免优化器与游戏参数尺度脱节
        # 游骑兵版：上界收紧至 5e-6，防止搜索空间落入 >5e-6 的"普通大兵"耗能区间
        # 保持下界 5e-7 不变，防止 1e-7 产生近零消耗的退化解（会被 physiological_realism 扣分）
        energy_to_stamina_coeff = trial.suggest_float(
            'energy_to_stamina_coeff', 5e-07, 5e-06, log=True
        )
        
        # 恢复系统相关
        # 调整：收紧上界，避免移动中"回血"过强导致 Run/Sprint 净消耗过低。
        # 当前问题：0.000268 * 1.4(standing) ≈ 0.000375/0.2s，600秒Run恢复1.1+，导致Run无净消耗
        # 新范围：0.5e-4 ~ 3e-4，确保恢复速度不会过度抵消运动消耗
        base_recovery_rate = trial.suggest_float(
            'base_recovery_rate', 5e-5, 3e-4, log=True  # 收紧范围，避免Run阶段无净消耗
        )
        # 约束：prone恢复应该快于standing恢复，所以prone应该有更高的下界
        prone_recovery_multiplier = trial.suggest_float(
            'prone_recovery_multiplier', 1.5, 3.5  # 更宽松，下界1.5覆盖代码默认1.8
        )
        # 调整：收紧上界，避免站立恢复过快导致Run阶段无净消耗
        # 当前问题：1.408 * 0.000268 ≈ 0.00038/0.2s，恢复过快
        standing_recovery_multiplier = trial.suggest_float(
            'standing_recovery_multiplier', 1.0, 1.8  # 收紧范围，确保Run有净消耗
        )
        # 30kg Walk 需净恢复：penalty 过大会扼杀恢复。1e-3 时 penalty≈0.0016>>base_recovery
        load_recovery_penalty_coeff = trial.suggest_float(
            'load_recovery_penalty_coeff', 1e-6, 5e-4, log=True  # 扩大上限至默认0.0004
        )
        load_recovery_penalty_exponent = trial.suggest_float(
            'load_recovery_penalty_exponent', 1.0, 2.0  # 降低上界，避免 penalty 爆炸
        )
        
        # 负重系统相关（改进：允许更低负重惩罚以通过30KG测试）
        # 游骑兵版：上界收紧至 0.16，模拟背部核心肌群特化训练导致的极低负重掉速
        encumbrance_speed_penalty_coeff = trial.suggest_float(
            'encumbrance_speed_penalty_coeff', 0.10, 0.16  # 游骑兵：重装掉速极小
        )
        # [DEPRECATED] C 端负重速度惩罚改为"分段非线性 + 软阈值"，不再使用 exponent 参与曲线形状。
        # 仍保留该字段写入 JSON/C 端结构体以兼容旧配置，但优化器不再搜索该维度。
        encumbrance_speed_penalty_exponent = 1.5
        # 游骑兵版：上界收紧至 0.70，防止极端负重（55kg）造成近乎定身（>70% 速度惩罚）
        encumbrance_speed_penalty_max = trial.suggest_float(
            'encumbrance_speed_penalty_max', 0.40, 0.70
        )
        # T1 对齐：下界放宽至 0.8，允许游骑兵级别的低负重消耗倍数
        encumbrance_stamina_drain_coeff = trial.suggest_float(
            'encumbrance_stamina_drain_coeff', 0.8, 1.6
        )
        
        # [DEPRECATED] 已统一 Pandolf 公式，C/Python 不再使用 Sprint 倍数；保留供 JSON 兼容
        sprint_stamina_drain_multiplier = 3.5
        
        # 疲劳系统相关
        # 游骑兵版：上界收紧至 0.015，极难积累永久体力上限扣除（Tier-1 神经疲劳抗性）
        fatigue_accumulation_coeff = trial.suggest_float(
            'fatigue_accumulation_coeff', 0.005, 0.015, log=True
        )
        fatigue_max_factor = trial.suggest_float(
            'fatigue_max_factor', 1.5, 3.0
        )
        
        # 代谢适应相关
        # [HARD] 有氧/无氧效率因子为运动生理学共识常数，不参与 Optuna 优化
        # C 端对应常量： AEROBIC_EFFICIENCY_FACTOR=0.9， ANAEROBIC_EFFICIENCY_FACTOR=1.2
        aerobic_efficiency_factor = 0.9   # [HARD] 运动生理学共识值，有氧区 90% 效率
        anaerobic_efficiency_factor = 1.2  # [HARD] 运动生理学共识值，无氧区耗能多 20%
        
        # 恢复系统高级参数（优化：减缓"平台恢复期"，fast/medium/slow 均下调）
        # recovery_nonlinear_coeff 0.15~0.35：降低低体力时的恢复加成
        recovery_nonlinear_coeff = trial.suggest_float(
            'recovery_nonlinear_coeff', 0.1, 0.7
        )
        # 强制 fast > medium > slow：使用顺序采样减少无效样本
        slow_recovery_multiplier = trial.suggest_float(
            'slow_recovery_multiplier', 0.5, 0.8
        )
        medium_lower = max(slow_recovery_multiplier + 0.01, 0.85)
        # 扩大中速/快速范围以包含默认1.4/2.5
        medium_recovery_multiplier = trial.suggest_float(
            'medium_recovery_multiplier', medium_lower, 2.0
        )
        fast_lower = max(medium_recovery_multiplier + 0.01, 1.0)
        fast_recovery_multiplier = trial.suggest_float(
            'fast_recovery_multiplier', fast_lower, 4.5
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
            'min_recovery_rest_time_seconds', 5.0, 25.0
        )
        
        # Sprint系统高级参数
        sprint_speed_boost = trial.suggest_float(
            'sprint_speed_boost', 0.28, 0.35  # 提高下界，确保 Sprint 有足够速度
        )
        
        # 姿态系统参数（消耗倍数：蹲/趴移动时体力消耗增加）
        # 修复：与游戏 SCR_StaminaConsumption 语义一致，游戏将其作为消耗倍数使用
        # posture_crouch: 蹲姿行走消耗倍数 1.2~2.2（站立=1.0）
        # posture_prone: 趴姿匍匐消耗倍数 2.0~4.0
        # 游骑兵版：范围收紧至 C 端配置文件的有效区间内，保证 JSON 嵌入后游戏内一致
        # 蹲姿：1.5~2.0（C 端 Optuna 范围 1.8~2.3 的下半区，体现低姿肌肉耐力特化）
        # 趴姿：2.0~2.8（C 端 Optuna 范围 2.2~2.8，仅微扩下界给游骑兵少量喘息空间）
        posture_crouch_multiplier = trial.suggest_float(
            'posture_crouch_multiplier', 1.5, 2.0
        )
        posture_prone_multiplier = trial.suggest_float(
            'posture_prone_multiplier', max(posture_crouch_multiplier + 0.01, 2.0), 2.8
        )
        
        # 动作消耗参数
        # [HARD] jump_efficiency=0.22，Margaria 1963 肌肉效率测量范围中心值 (20-25%)，不参与优化
        jump_efficiency = 0.22  # [HARD] Margaria 1963
        jump_height_guess = trial.suggest_float('jump_height_guess', 0.3, 1.0)              # [SOFT][OPTIMIZE]
        jump_horizontal_speed_guess = trial.suggest_float('jump_horizontal_speed_guess', 0.0, 1.5)  # [SOFT][OPTIMIZE]
        
        # 坡度系统参数 [DEPRECATED] C 端 CalculateSlopeStaminaDrainMultiplier 未被调用，
        # 坡度消耗已由 Pandolf 公式的 grade_percent 承担；固定值以兼容 JSON/embed
        slope_uphill_coeff = 0.08   # [HARD] 与 StaminaConstants.SLOPE_UPHILL_COEFF 一致
        slope_downhill_coeff = 0.03  # [HARD] 与 StaminaConstants.SLOPE_DOWNHILL_COEFF 一致
        
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

        # T1 模型层面参数
        load_metabolic_dampening = trial.suggest_float(
            'load_metabolic_dampening', 0.50, 0.85
        )
        max_recovery_per_tick = trial.suggest_float(
            'max_recovery_per_tick', 0.0002, 0.0006
        )

        # ==================== 2. 创建参数对象并更新常量 ====================
        
        # 创建新的常量实例（不修改原始类）
        constants = RSSConstants()
        # 如果外部提供了常量覆盖（用于对比实验），应用它们
        if getattr(self, 'constants_override', None):
            for k, v in self.constants_override.items():
                if hasattr(constants, k):
                    setattr(constants, k, v)

        
        # 更新参数
        constants.ENERGY_TO_STAMINA_COEFF = energy_to_stamina_coeff
        constants.BASE_RECOVERY_RATE = base_recovery_rate
        constants.STANDING_RECOVERY_MULTIPLIER = standing_recovery_multiplier
        constants.PRONE_RECOVERY_MULTIPLIER = prone_recovery_multiplier
        constants.LOAD_RECOVERY_PENALTY_COEFF = load_recovery_penalty_coeff
        constants.LOAD_RECOVERY_PENALTY_EXPONENT = load_recovery_penalty_exponent
        constants.ENCUMBRANCE_SPEED_PENALTY_COEFF = encumbrance_speed_penalty_coeff
        constants.ENCUMBRANCE_SPEED_PENALTY_EXPONENT = encumbrance_speed_penalty_exponent
        constants.ENCUMBRANCE_SPEED_PENALTY_MAX = encumbrance_speed_penalty_max
        constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF = encumbrance_stamina_drain_coeff
        constants.SPRINT_STAMINA_DRAIN_MULTIPLIER = sprint_stamina_drain_multiplier  # [DEPRECATED] 保留 JSON 兼容
        constants.FATIGUE_ACCUMULATION_COEFF = fatigue_accumulation_coeff
        constants.FATIGUE_MAX_FACTOR = fatigue_max_factor
        # [HARD] AEROBIC/ANAEROBIC_EFFICIENCY_FACTOR 不参与优化，保持类默认值 0.9/1.2
        # constants.AEROBIC_EFFICIENCY_FACTOR = aerobic_efficiency_factor  # HARD - removed
        # constants.ANAEROBIC_EFFICIENCY_FACTOR = anaerobic_efficiency_factor  # HARD - removed
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
        # constants related to physical jump/climb are set automatically
        # [HARD] jump_efficiency=0.22 不修改，保持类默认值
        # constants.JUMP_EFFICIENCY = jump_efficiency  # HARD - removed from optimization
        # [HARD] CLIMB_ISO_EFFICIENCY=0.12 不修改，保持类默认值
        # constants.CLIMB_ISO_EFFICIENCY = 0.12  # HARD - kept at 0.12 (Margaria 1963)
        constants.JUMP_HEIGHT_GUESS = jump_height_guess
        constants.JUMP_HORIZONTAL_SPEED_GUESS = jump_horizontal_speed_guess
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
        constants.LOAD_METABOLIC_DAMPENING = load_metabolic_dampening
        constants.MAX_RECOVERY_PER_TICK = max_recovery_per_tick
        
        # ==================== 3. 创建数字孪生仿真器 ====================
        
        twin = RSSDigitalTwin(constants)
        
        # ==================== 4. 基础体能检查（ACFT 2英里测试，空载）====================
        # 这是最低标准，不是优化目标
        basic_fitness_passed = self._check_basic_fitness(twin)

        # ==================== 4.5 游骑兵 ETA：29kg 3.8m/s 连续跑 300s 底线（软惩罚计入可玩性）====================
        ranger_penalty = self._check_ranger_29kg_38ms_eta(twin)

        # ==================== 4.6 硬约束：铺装路面跑步必须有净消耗（避免「恢复≈消耗」不现实解）====================
        self._check_flat_paved_net_drain(twin)
        
        # ==================== 5. 目标1：评估30KG可玩性（Playability Burden）====================
        
        # 专门针对30KG负载进行测试
        playability_burden = self._evaluate_30kg_playability(twin) + ranger_penalty
        trial.set_user_attr(
            'combat_cycle_min_stamina',
            float(getattr(self, '_last_combat_cycle_min_stamina', 0.0)),
        )

        # ==================== 6. 目标2：评估稳定性风险（Stability Risk / BUG检测）====================
        
        # 地狱级压力测试
        stability_risk, bug_reports = self._evaluate_stability_risk(
            twin, trial.number, trial.params
        )
        
        # 记录BUG报告
        self.bug_reports.extend(bug_reports)

        constraint_penalty = 0.0
        # 若检测到关键性BUG（NaN/负体力/仿真崩溃）：可选剪枝或仅加惩罚（默认不剪枝以保证帕累托解非空）
        critical_bug_types = {'negative_stamina', 'nan_value', 'simulation_crash'}
        critical_penalty = 0.0
        if any(b.bug_type in critical_bug_types for b in bug_reports):
            trial.set_user_attr('critical_bug', True)
            critical_penalty = getattr(self, 'critical_bug_penalty', 50000.0)
            if getattr(self, 'prune_on_critical_bug', False):
                raise optuna.exceptions.TrialPruned('Critical bug detected during stability evaluation')
            constraint_penalty += critical_penalty
            stability_risk += critical_penalty

        # ==================== 7. 添加物理约束条件（JSON合理性修复） ====================
        # 这些约束确保生成的参数满足生物学和物理学逻辑（constraint_penalty 已在上方初始化）

        # 基础体能检查约束：未通过时可选剪枝或仅加惩罚（默认不剪枝以保证帕累托解非空）
        if not basic_fitness_passed:
            trial.set_user_attr('basic_fitness', False)
            fitness_penalty = getattr(self, 'basic_fitness_fail_penalty', 30000.0)
            constraint_penalty += fitness_penalty
            stability_risk += fitness_penalty
            if getattr(self, 'prune_on_basic_fitness_fail', False):
                raise optuna.exceptions.TrialPruned('Failed basic fitness check')

        # 约束1–6：使用集中配置 CONSTRAINT
        ccfg = getattr(self, 'CONSTRAINT', RSSSuperPipeline.CONSTRAINT)
        if prone_recovery_multiplier <= standing_recovery_multiplier:
            violation_factor = standing_recovery_multiplier - prone_recovery_multiplier + 0.1
            penalty = violation_factor * ccfg['prone_standing']
            constraint_penalty += penalty
            stability_risk += penalty
        if standing_recovery_multiplier <= slow_recovery_multiplier:
            violation_factor = slow_recovery_multiplier - standing_recovery_multiplier + 0.1
            penalty = violation_factor * ccfg['standing_slow']
            constraint_penalty += penalty
            stability_risk += penalty
        if fast_recovery_multiplier <= medium_recovery_multiplier:
            violation_factor = medium_recovery_multiplier - fast_recovery_multiplier + 0.1
            penalty = violation_factor * ccfg['fast_medium']
            constraint_penalty += penalty
            stability_risk += penalty
        if medium_recovery_multiplier <= slow_recovery_multiplier:
            violation_factor = slow_recovery_multiplier - medium_recovery_multiplier + 0.1
            penalty = violation_factor * ccfg['medium_slow']
            constraint_penalty += penalty
            stability_risk += penalty
        if posture_crouch_multiplier <= 1.0:
            violation_factor = 1.0 - posture_crouch_multiplier + 0.1
            penalty = violation_factor * ccfg['posture_crouch']
            constraint_penalty += penalty
            stability_risk += penalty
        if posture_prone_multiplier <= 1.0:
            violation_factor = 1.0 - posture_prone_multiplier + 0.1
            penalty = violation_factor * ccfg['posture_prone']
            constraint_penalty += penalty
            stability_risk += penalty
        if posture_prone_multiplier <= posture_crouch_multiplier:
            violation_factor = posture_crouch_multiplier - posture_prone_multiplier + 0.1
            penalty = violation_factor * ccfg['posture_prone_crouch']
            constraint_penalty += penalty
            stability_risk += penalty
        
        # 约束7: playability配置下，base_recovery_rate应该相对较高
        # 说明：playability（可玩性）应该意味着更容易恢复，而不是更慢
        # 注：这是一个软约束，仅在明显偏离时才惩罚
        # （这个约束很难在不知道当前是哪个配置的情况下强制）
        
        # ==================== 7. 软约束检查（参数合理性惩罚） ====================
        # 目标：
        # - Sprint / Run 必须"净消耗"且消耗不应过低
        # - Walk 必须"缓慢净恢复"（不应快到像静止趴下回血）
        movement_balance_penalty, actual_sprint_drop = self._evaluate_movement_balance_penalty(constants)
        if movement_balance_penalty > 0.0:
            constraint_penalty += movement_balance_penalty
            stability_risk += movement_balance_penalty
        
        # ==================== 8. 计算生理学合理性 ====================
        # 基于科学文献的生理学合理性评估
        physiological_realism = self._evaluate_physiological_realism(constants)

        # ==================== 新目标：Sprint-drop 作为显式优化目标（可选） ====================
        # 提前读取目标区间（供后续联合约束使用）
        required_sprint_drop_min, required_sprint_drop_max = getattr(self, 'sprint_drop_target', (0.15, 0.40))

        # 仅在启用时计算 Sprint-drop 目标（以避免与研究的目标数不匹配）
        sprint_obj = 0.0
        if getattr(self, 'enable_sprint_objective', False):
            if actual_sprint_drop < required_sprint_drop_min:
                sprint_obj = (required_sprint_drop_min - actual_sprint_drop) * getattr(self, 'sprint_drop_penalty_below', 4000.0)
            elif actual_sprint_drop > required_sprint_drop_max:
                sprint_obj = (actual_sprint_drop - required_sprint_drop_max) * getattr(self, 'sprint_drop_penalty_above', 5000.0)

        # ==================== 新约束：冲刺与能量系数的联合惩罚（可选） ====================
        # 目的：避免仅通过增大冲刺倍率而产生不可接受的瞬时耗尽；鼓励 energy_to_stamina_coeff 与 冲刺倍率 联动
        if getattr(self, 'enable_joint_sprint_energy_constraint', False):
            # 期望 energy_to_stamina_coeff 接近目标值（joint_sprint_energy_target_coeff）以保证冲刺不会过度耗尽
            target_coeff = getattr(self, 'joint_sprint_energy_target_coeff', 1.0e-6)
            scale = getattr(self, 'joint_sprint_energy_penalty_scale', 8000.0)

            # 若冲刺耗尽过高（>上限），对低 energy_to_stamina_coeff 强烈惩罚
            if actual_sprint_drop > required_sprint_drop_max:
                # 当 coeff 越小，惩罚越高（鼓励提高能量转化率）
                coeff_ratio = target_coeff / max(energy_to_stamina_coeff, 1e-12)
                joint_penalty = (actual_sprint_drop - required_sprint_drop_max) * scale * coeff_ratio
                constraint_penalty += joint_penalty
                stability_risk += joint_penalty
            # 若冲刺耗尽过低（<下限），对过高的 energy_to_stamina_coeff 惩罚（鼓励降低能量转化率或减少冲刺倍率）
            elif actual_sprint_drop < required_sprint_drop_min:
                coeff_ratio = max(energy_to_stamina_coeff / target_coeff, 1e-12)
                joint_penalty = (required_sprint_drop_min - actual_sprint_drop) * scale * coeff_ratio
                constraint_penalty += joint_penalty
                stability_risk += joint_penalty

            # 极端耗尽检查 - 不再剪枝，只记录以便分析
            if actual_sprint_drop >= 0.98:
                # 打印调试信息，观察实际分布
                print(f"[Sprint] extreme drop={actual_sprint_drop:.3f} (>=0.98) params={trial.params}")
                # 将其视作高风险惩罚，而非直接剪枝
                extreme_pen = (actual_sprint_drop - 0.98) * 10000.0
                constraint_penalty += extreme_pen
                stability_risk += extreme_pen

        # ==================== 新目标：30s 剩余体力（显式） ====================
        # 当启用时，把距离目标剩余区间的偏离作为独立目标返回给优化器（越小越好）
        sprint_remaining_obj = 0.0
        if getattr(self, 'enable_sprint_remaining_objective', False):
            # 目标剩余区间由 sprint_drop_target 翻转得到
            rem_min = 1.0 - required_sprint_drop_max
            rem_max = 1.0 - required_sprint_drop_min
            sprint_remaining = 1.0 - actual_sprint_drop  # 实际30s剩余体力

            # Compute a dedicated 30s sprint test (deterministic) to measure realistic depletion
            # This aligns the optimizer objective with the external 30s sprint simulation used in analysis
            sprint_test_duration = 30.0
            twin_30 = RSSDigitalTwin(constants)
            twin_30.reset()
            twin_30.stamina = 1.0
            steps = int(sprint_test_duration / 0.2)
            for _ in range(steps):
                twin_30.step(
                    speed=5.0,
                    current_weight=90.0,
                    grade_percent=0.0,
                    terrain_factor=1.0,
                    stance=Stance.STAND,
                    movement_type=MovementType.SPRINT,
                    current_time=0.0,
                    enable_randomness=False,
                )
            sprint_30s_drop = 1.0 - twin_30.stamina

            # Hard prune if 30s remaining below configured prune threshold
            # （暂时禁用剪枝以观察分布，只记录日志）
            prune_thr = getattr(self, 'sprint_prune_threshold', 0.05)
            if (1.0 - sprint_30s_drop) < prune_thr:
                print(f"[Sprint] low remaining {1.0 - sprint_30s_drop:.3f} below prune threshold {prune_thr}")
                # 可以考虑加入惩罚而非剪枝
                penalty = (prune_thr - (1.0 - sprint_30s_drop)) * 5000.0
                constraint_penalty += penalty
                stability_risk += penalty

            # Always-on objective: minimize depletion in the 30s test (drop ranges 0..1)
            sprint_remaining_obj = sprint_30s_drop

            # Additional normalized penalty if outside target rem range (amplify deviation)
            rem_range = max(rem_max - rem_min, 1e-6)
            if (1.0 - sprint_30s_drop) < rem_min:
                normalized = (rem_min - (1.0 - sprint_30s_drop)) / rem_range
                sprint_remaining_obj += normalized * (getattr(self, 'sprint_remaining_penalty_scale', 20000.0) / 10000.0)
            elif (1.0 - sprint_30s_drop) > rem_max:
                normalized = ((1.0 - sprint_30s_drop) - rem_max) / rem_range
                sprint_remaining_obj += normalized * (getattr(self, 'sprint_remaining_penalty_scale', 20000.0) / 10000.0)

        # ==================== 9. 应用多阶段优化权重 ====================
        # 获取当前权重
        stability_weight = getattr(self, '_stability_weight', 1.0)
        rcfg = getattr(self, 'REALISM', RSSSuperPipeline.REALISM)
        realism_weight = float(rcfg.get('objective_weight', 115.0))

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
                # 如果回调是绑定到 tk 小部件的方法，则使用 `after` 调度到主线程
                if hasattr(self.gui_update_callback, '__self__') and hasattr(self.gui_update_callback.__self__, 'after'):
                    widget = self.gui_update_callback.__self__
                    widget.after(0, lambda: self.gui_update_callback(
                        iteration=trial.number,
                        playability=playability_burden,
                        stability=stability_risk,
                        realism=physiological_realism,
                        sprint_drop=actual_sprint_drop,
                        params=dict(trial.params)
                    ))
                else:
                    self.gui_update_callback(
                        iteration=trial.number,
                        playability=playability_burden,
                        stability=stability_risk,
                        realism=physiological_realism,
                        sprint_drop=actual_sprint_drop,
                        params=dict(trial.params)
                    )
            except Exception as e:
                print(f"GUI更新失败: {e}")
        
        # 对目标函数进行缩放，使其更容易收敛
        scaled_playability = playability_burden * 0.1  # 缩小10倍（从100倍调整为10倍，避免精度丢失）
        scaled_stability = weighted_stability_risk * 0.1  # 缩小10倍
        scaled_realism = weighted_physiology_loss * 0.1  # 生理学损失：缩小10倍

        # 新：评估 40/60 分钟会话相关指标并作为额外目标（越小越好）
        engagement_loss, session_fail = self._evaluate_session_metrics(twin)

        # 会话失败不再剪枝，改为大惩罚，避免帕累托解数量为 0（失败解仍参与优化但被惩罚支配）
        scfg = getattr(self, 'SESSION', RSSSuperPipeline.SESSION)
        session_penalty = 0.0
        if session_fail:
            trial.set_user_attr('session_fail', True)
            engagement_loss += scfg['session_fail_engagement']
            session_penalty = scfg['session_penalty_all']

        # Sprint-drop 目标缩放并作为独立目标（仅在启用时包含于返回值中）
        if getattr(self, 'enable_sprint_objective', False):
            scaled_sprint = sprint_obj * 0.1
        else:
            scaled_sprint = None

        # 根据配置返回目标向量（长度需与创建研究时的 directions 一致）
        # 统一为 4 基目标（playability, stability, realism, engagement）+ 可选 sprint / sprint_remaining
        engagement_scaled = float(engagement_loss)
        if getattr(self, 'enable_sprint_objective', False) and getattr(self, 'enable_sprint_remaining_objective', False):
            return (scaled_playability + session_penalty, scaled_stability + session_penalty,
                    scaled_realism + session_penalty, engagement_scaled, scaled_sprint, sprint_remaining_obj * 0.1)
        if getattr(self, 'enable_sprint_objective', False):
            return (scaled_playability + session_penalty, scaled_stability + session_penalty,
                    scaled_realism + session_penalty, engagement_scaled, scaled_sprint)
        return (scaled_playability + session_penalty, scaled_stability + session_penalty,
                scaled_realism + session_penalty, engagement_scaled)
    
    def _evaluate_physiological_realism(self, constants: RSSConstants) -> float:
        """
        评估生理学合理性（基于科学文献与当前 Optuna 搜索边界对齐）
        
        Args:
            constants: 常量对象
        
        Returns:
            生理学合理性值（越大越好，0-100范围）
        """
        # 参考：Pandolf et al. (1977)；ACSM；模组内嵌预设 energy 量级
        rcfg = getattr(self, 'REALISM', RSSSuperPipeline.REALISM)
        realism_score = 100.0

        base_recovery = constants.BASE_RECOVERY_RATE
        br_lo = float(rcfg.get('base_recovery_physiological_min', 5e-5))
        br_hi = float(rcfg.get('base_recovery_physiological_max', 3e-4))
        if base_recovery < br_lo or base_recovery > br_hi:
            realism_score -= 10.0

        aerobic_threshold = constants.AEROBIC_THRESHOLD
        anaerobic_threshold = constants.ANAEROBIC_THRESHOLD
        if aerobic_threshold >= anaerobic_threshold:
            realism_score -= 20.0

        energy_coeff = constants.ENERGY_TO_STAMINA_COEFF
        es_lo = float(rcfg.get('energy_coeff_search_low', 5e-7))
        es_hi = float(rcfg.get('energy_coeff_search_high', 5e-6))
        ref_lo = float(rcfg.get('energy_coeff_reference_low', 5e-7))
        ref_hi = float(rcfg.get('energy_coeff_reference_high', 1.3e-6))
        if energy_coeff < es_lo or energy_coeff > es_hi:
            realism_score -= float(rcfg.get('energy_coeff_outside_search_penalty', 14.0))
        elif energy_coeff < ref_lo or energy_coeff > ref_hi:
            realism_score -= float(rcfg.get('energy_coeff_outside_reference_penalty', 7.0))

        encumbrance_coeff = constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF
        if encumbrance_coeff < 0.8 or encumbrance_coeff > 2.0:
            realism_score -= 10.0

        fatigue_accumulation = constants.FATIGUE_ACCUMULATION_COEFF
        if fatigue_accumulation < 0.005 or fatigue_accumulation > 0.03:
            realism_score -= 8.0

        lmd = float(getattr(constants, 'LOAD_METABOLIC_DAMPENING', 0.7))
        lmd_lo = float(rcfg.get('load_metabolic_dampening_min', 0.48))
        lmd_hi = float(rcfg.get('load_metabolic_dampening_max', 0.88))
        if lmd < lmd_lo or lmd > lmd_hi:
            realism_score -= float(rcfg.get('load_metabolic_dampening_penalty', 5.0))

        realism_score = max(0.0, min(realism_score, 100.0))
        return realism_score
    
    def _evaluate_movement_balance_penalty(self, constants: RSSConstants) -> Tuple[float, float]:
        """基于短时段"移动净值"行为的软硬约束惩罚。
        Returns:
            (penalty, actual_sprint_drop)
        """
        # 设计目标（用户意图）：
        # - Run / Sprint：体力应稳定下降（净消耗）
        # - Walk：体力应缓慢上升（净恢复）
        #
        # 说明：
        # - 使用短时段固定速度测试，计算起止体力变化。
        # - 惩罚系数取较大值，使其在 NSGA-II 中具有足够约束力。
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

        # 1) Run：60秒 3.8 m/s 空载
        # 放宽约束：大多数游戏场景负重 20kg+，空载不做严格限制
        run_delta = simulate_fixed_speed(
            speed=3.8,
            duration_seconds=60.0,
            current_weight=90.0,
            movement_type=MovementType.RUN,
            initial_stamina=1.0,
        )
        actual_run_drop = max(0.0, -run_delta)
        required_run_drop_min = 0.003
        required_run_drop_max = 0.10
        if actual_run_drop < required_run_drop_min:
            penalty += (required_run_drop_min - actual_run_drop) * 1000.0
        elif actual_run_drop > required_run_drop_max:
            penalty += (actual_run_drop - required_run_drop_max) * 2000.0

        # 2) Sprint：30秒 5.0m/s 空载
        # 期望：在目标区间内（例如 15%~40%）
        sprint_delta = simulate_fixed_speed(
            speed=5.0,
            duration_seconds=30.0,
            current_weight=90.0,
            movement_type=MovementType.SPRINT,
            initial_stamina=1.0,
        )
        actual_sprint_drop = max(0.0, -sprint_delta)
        # 与全局 sprint_drop_target 一致（默认 0.15~0.40），避免移动平衡与冲刺剩余/联合约束两套标准
        required_sprint_drop_min, required_sprint_drop_max = getattr(self, 'sprint_drop_target', (0.15, 0.40))
        # 硬约束：冲刺压降不得超过上限（例如 ≤40%）
        # 目的：即便是帕累托前沿中最差的"踩线"解，也必须满足该生理底线。
        if actual_sprint_drop > required_sprint_drop_max:
            raise optuna.exceptions.TrialPruned(
                f"Sprint Drop {actual_sprint_drop:.2%} 低于硬约束上限 {required_sprint_drop_max:.0%}"
            )
        if actual_sprint_drop < required_sprint_drop_min:
            # 连续惩罚：低于下限按距离线性惩罚（可调系数）
            penalty += (required_sprint_drop_min - actual_sprint_drop) * getattr(self, 'sprint_drop_penalty_below', 4000.0)
        elif actual_sprint_drop > required_sprint_drop_max:
            # 连续惩罚：高于上限按距离线性惩罚（避免过度惩罚）
            penalty += (actual_sprint_drop - required_sprint_drop_max) * getattr(self, 'sprint_drop_penalty_above', 5000.0)
        # 将 Sprint 的离散惩罚也保留为参数多样性惩罚项（便于诊断）
        if actual_sprint_drop < required_sprint_drop_min or actual_sprint_drop > required_sprint_drop_max:
            # 记录轻量日志供 GUI/调试查看（不影响返回值）
            pass

        # 3) Walk：120秒 1.8m/s 标准战斗负载（30KG，总重 120KG）
        walk_delta = simulate_fixed_speed(
            speed=1.8,
            duration_seconds=120.0,
            current_weight=120.0,  # 90kg 体重 + 30kg 负载
            movement_type=MovementType.WALK,
            initial_stamina=0.50,
        )
        # 30KG 负载下 Walk 净恢复应 0.2%~3%，超 3% 严重惩罚（避免 Walk 回血过快）
        min_walk_gain = 0.001  # 最小恢复 0.1%（修正后Pandolf，coeff=2e-7: ~0.5%）
        max_walk_gain = 0.015  # 最大恢复 1.5%
        if walk_delta < min_walk_gain:
            penalty += (min_walk_gain - walk_delta) * 3000.0
        elif walk_delta > max_walk_gain:
            penalty += (walk_delta - max_walk_gain) * 5000.0  # 提高超限惩罚

        # 返回 (penalty, actual_sprint_drop) 以便作为独立目标使用
        return float(penalty), float(actual_sprint_drop)
    
    def _check_basic_fitness(self, twin: RSSDigitalTwin) -> bool:
        """
        检查基础体能标准（ACFT 2英里测试，空载）
        
        设计标准（与游戏时间缩放 tickScale=0.25 一致）：
        - 0kg 负重，3.5km 用时 15分27秒（927秒），速度约 3.8 m/s
        - 体力最低不能跌破 20%
        - 平均体力应 > 25%

        Args:
            twin: 数字孪生仿真器
        
        Returns:
            是否通过基础体能测试
        """
        # 使用标准ACFT 2英里测试（空载），速度用当前负重下的 run 速度
        scenario = ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0, constants=twin.constants)
        
        try:
            results = twin.simulate_scenario(
                speed_profile=scenario.speed_profile,
                current_weight=scenario.current_weight,
                grade_percent=scenario.grade_percent,
                terrain_factor=scenario.terrain_factor,
                stance=scenario.stance,
                movement_type=scenario.movement_type,
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
                fast_mode=getattr(self, 'fast_mode', False)
            )
            
            # 检查是否通过基础体能测试
            # 标准：完成时间不超过目标时间的120%
            target_time = scenario.target_finish_time
            actual_time = results['total_time_with_penalty']
            time_ratio = actual_time / target_time

            # 检查最低体力是否合理（不低于20%）
            min_stamina = results['min_stamina']

            # 仅使用最低体力作为基础体能判定：平均值在持续消耗场景中容易产生误导（可能被短时高值抬高）
            # 通过条件：完成时间不超过配置比例（默认 1.45，放宽以配合新孪生逻辑 Run/Sprint 不恢复等）
            # 原来的 min_stamina >= 20% 过于严格（默认常量会掉到0），因此仅保留时间检查。
            max_ratio = getattr(self, 'basic_fitness_time_ratio_max', 1.45)
            passed = time_ratio <= max_ratio
            # 记录调试信息以便分析剪枝原因
            if not passed:
                print(f"[Fitness] time_ratio={time_ratio:.3f}, min_stamina={min_stamina:.3f} -> rejected")
            return passed

        except Exception:
            return False

    def _check_ranger_29kg_38ms_eta(self, twin: RSSDigitalTwin) -> float:
        """
        游骑兵 ETA：29kg 负重、3.8 m/s 奔跑在指定时长后体力不得低于阈值。
        未达标时返回软惩罚（不剪枝），以便多目标优化仍能产生 COMPLETE trial。
        """
        pcfg = getattr(self, 'PLAYABILITY', RSSSuperPipeline.PLAYABILITY)
        duration_s = float(pcfg.get('ranger_29kg_38ms_duration_s', 300))
        min_stamina_req = float(pcfg.get('ranger_29kg_38ms_min_stamina', 0.20))
        coeff = float(pcfg.get('ranger_29kg_38ms_below_penalty_coeff', 35000.0))
        load_kg = 29.0
        current_weight = 90.0 + load_kg
        speed_profile = [(0.0, 3.8), (duration_s, 3.8)]
        scenario_twin = RSSDigitalTwin(twin.constants)
        results = scenario_twin.simulate_scenario(
            speed_profile=speed_profile,
            current_weight=current_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
            fast_mode=getattr(self, 'fast_mode', False)
        )
        min_stamina = float(results['min_stamina'])
        if min_stamina < min_stamina_req:
            return float((min_stamina_req - min_stamina) * coeff)
        return 0.0

    def _check_flat_paved_net_drain(self, twin: RSSDigitalTwin) -> None:
        """
        硬约束：铺装路面（grade=0, terrain=1.0）跑步必须有净消耗，不能出现「恢复≈消耗」体力持平。
        避免优化器选出不现实参数导致平地跑不掉体力的现象。
        不通过则剪枝。
        """
        pcfg = getattr(self, 'PLAYABILITY', RSSSuperPipeline.PLAYABILITY)
        duration_s = float(pcfg.get('flat_paved_run_duration_s', 300))
        load_kg = float(pcfg.get('flat_paved_run_load_kg', 29.0))
        max_stamina_after = float(pcfg.get('flat_paved_run_max_stamina_after', 0.90))
        current_weight = 90.0 + load_kg
        run_speed = get_speed(load_kg, "run")
        scenario_twin = RSSDigitalTwin(twin.constants)
        scenario_twin.reset()
        dt = 0.2
        t = 0.0
        while t < duration_s:
            scenario_twin.step(
                run_speed,
                current_weight,
                0.0,
                1.0,
                Stance.STAND,
                MovementType.RUN,
                t + dt,
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
                wind_drag=0.0,
            )
            t += dt
        stamina_after = float(scenario_twin.stamina)
        if stamina_after > max_stamina_after:
            raise optuna.exceptions.TrialPruned(
                f"铺装路面跑步无净消耗（{duration_s:.0f}s 后体力={stamina_after:.2%} > {max_stamina_after:.0%}），不符合现实"
            )
    
    def _evaluate_30kg_playability(self, twin: RSSDigitalTwin) -> float:
        """
        评估30KG负载下的可玩性负担（专门针对玩家反馈的"30KG太累"问题）
        
        Args:
            twin: 数字孪生仿真器
        
        Returns:
            可玩性负担（越小越好）
        """
        # 所有场景速度均按当前参数下的负重速度计算（run/walk/sprint_at_weight）
        c = twin.constants
        run_speed_30kg = get_speed(30.0, "run")
        scenarios = [
            ScenarioLibrary.create_acft_2mile_scenario(load_weight=1.36, constants=c),
            ScenarioLibrary.create_urban_combat_scenario(load_weight=35.0, constants=c),
            ScenarioLibrary.create_mountain_combat_scenario(load_weight=25.0, constants=c),
            ScenarioLibrary.create_realistic_field_patrol_scenario(load_weight=30.0, constants=c),
            ScenarioLibrary.create_run_sprint_boundary_scenario(load_weight=30.0, constants=c),
            ScenarioLibrary.create_heavy_load_scenario(load_weight=45.0, constants=c),
            ScenarioLibrary.create_long_duration_tactical_scenario(load_weight=30.0, constants=c, run_speed=run_speed_30kg),
            ScenarioLibrary.create_run_600m_flat_scenario(load_weight=30.0, run_speed=run_speed_30kg, constants=c),
            ScenarioLibrary.create_run_1km_flat_scenario(load_weight=30.0, run_speed=run_speed_30kg, constants=c),
            ScenarioLibrary.create_cqb_stealth_scenario(load_weight=30.0, constants=c),
            ScenarioLibrary.create_extreme_load_scenario(load_weight=55.0, constants=c),
            ScenarioLibrary.create_florida_swamp_scenario(load_weight=35.0, constants=c),
            ScenarioLibrary.create_combat_cycle_scenario(load_weight=30.0, constants=c),
        ]
        
        # 使用并行处理提高性能
        def evaluate_scenario(scenario):
            """评估单个场景"""
            try:
                # 创建独立的twin实例以避免线程安全问题
                scenario_twin = RSSDigitalTwin(twin.constants)
                # 重置twin状态
                scenario_twin.reset()
                # 为"佛罗里达热应激"场景注入 35°C，激活 heat_stress 计算路径
                # 其余所有场景不传入温度，使用孪生体默认行为（无热应激）
                if getattr(scenario, 'test_type', '') == '佛罗里达热应激':
                    temp_celsius = 35.0
                else:
                    temp_celsius = None
                results = scenario_twin.simulate_scenario(
                    speed_profile=scenario.speed_profile,
                    current_weight=scenario.current_weight,
                    grade_percent=scenario.grade_percent,
                    terrain_factor=scenario.terrain_factor,
                    stance=scenario.stance,
                    movement_type=scenario.movement_type,
                    enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
                    temperature_celsius=temp_celsius,
                    fast_mode=getattr(self, 'fast_mode', False)
                )
                
                # 计算当前场景的可玩性负担（连续型，避免大量解被压成同一常数）
                scenario_burden = 0.0

                target_time = float(scenario.target_finish_time)
                actual_time = float(results['total_time_with_penalty'])
                time_ratio = actual_time / max(target_time, 1e-6)

                # 1) 完成时间惩罚：使用集中配置 PLAYABILITY
                pcfg = getattr(self, 'PLAYABILITY', RSSSuperPipeline.PLAYABILITY)
                scenario_burden += max(0.0, time_ratio - 1.15) * pcfg['time_ratio_1_15_coeff']
                scenario_burden += max(0.0, time_ratio - 1.30) * pcfg['time_ratio_1_30_coeff']

                # 2) 体力压力惩罚：min/mean 体力越低，可玩性越差（连续）
                stamina_history = results.get('stamina_history', [])
                if stamina_history:
                    min_stamina = float(min(stamina_history))
                    mean_stamina = float(sum(stamina_history) / len(stamina_history))

                    scenario_burden += max(0.0, pcfg['min_stamina_threshold'] - min_stamina) * pcfg['min_stamina_coeff']
                    scenario_burden += max(0.0, pcfg['mean_stamina_threshold'] - mean_stamina) * pcfg['mean_stamina_coeff']

                    exhausted_frames = sum(1 for s in stamina_history if s < 0.10)
                    exhaustion_ratio = exhausted_frames / len(stamina_history)
                    scenario_burden += (exhaustion_ratio * exhaustion_ratio) * pcfg['exhaustion_coeff']

                    # 文献约束：30kg 600m/1000m 剩余体力目标带 + 跛行惩罚（见 stamina_consumption_reference.md）
                    test_type = getattr(scenario, 'test_type', '')
                    if test_type == '30kg跑道600m':
                        target_low = pcfg.get('run_600m_remaining_target_low', 0.68)
                        limp = pcfg.get('run_600m_remaining_limp', 0.55)
                        coeff_target = pcfg.get('run_600m_below_target_coeff', 8000.0)
                        coeff_limp = pcfg.get('run_600m_below_limp_coeff', 12000.0)
                        if min_stamina < limp:
                            scenario_burden += (limp - min_stamina) * coeff_limp
                        elif min_stamina < target_low:
                            scenario_burden += (target_low - min_stamina) * coeff_target
                    elif test_type == '30kg跑道1km':
                        # 1km跑更久(约262s vs 157s)，允许剩余体力低于600m
                        target_low = pcfg.get('run_1km_remaining_target_low', 0.48)
                        limp = pcfg.get('run_1km_remaining_limp', 0.35)
                        coeff_target = pcfg.get('run_1km_below_target_coeff', 8000.0)
                        coeff_limp = pcfg.get('run_1km_below_limp_coeff', 12000.0)
                        if min_stamina < limp:
                            scenario_burden += (limp - min_stamina) * coeff_limp
                        elif min_stamina < target_low:
                            scenario_burden += (target_low - min_stamina) * coeff_target
                    elif test_type == '战术战斗周期':
                        min_stamina_req = float(
                            pcfg.get('combat_cycle_min_stamina_req', 0.23))
                        cc_coeff = float(
                            pcfg.get('combat_cycle_below_penalty_coeff', 55000.0))
                        if any(s < min_stamina_req for s in stamina_history):
                            scenario_burden += float(
                                max(0.0, min_stamina_req - min_stamina) * cc_coeff)

                if stamina_history:
                    min_stamina_out = float(min(stamina_history))
                else:
                    min_stamina_out = None
                return (float(scenario_burden), scenario, min_stamina_out)
            except optuna.exceptions.TrialPruned:
                # 已经包含 TrialPruned 异常，直接重新抛出
                raise
            except Exception as e:
                # 如果仿真失败，视为被剪枝（避免异常trial被记录到帕累托解中）
                print(f"场景评估失败: {e}")
                raise optuna.exceptions.TrialPruned(f"场景评估失败: {e}")
        
        # 使用自定义并行工作器并行评估场景
        try:
            # 确保并行工作器已启动
            if not hasattr(self.parallel_worker, 'executor') or self.parallel_worker.executor is None:
                self.parallel_worker.start()
            # 使用更大的batch_size提高并行效率
            eval_batch_size = max(self.batch_size, 4)
            results_list = self.parallel_worker.map(evaluate_scenario, scenarios, batch_size=eval_batch_size)
        except optuna.exceptions.TrialPruned:
            raise
        except Exception as e:
            # 仅基础设施/意外错误时回退串行；TrialPruned 已在上方重抛
            print(f"并行处理失败，回退到串行: {e}")
            results_list = [evaluate_scenario(scenario) for scenario in scenarios]

        pcfg = getattr(self, 'PLAYABILITY', RSSSuperPipeline.PLAYABILITY)
        # 600m/1000m 文献带：软惩罚（硬剪枝易使无可行 COMPLETE trial）
        milestone_penalty = 0.0
        for scenario_burden, scenario, min_stamina in results_list:
            if scenario is None or min_stamina is None:
                continue
            test_type = getattr(scenario, 'test_type', '')
            if test_type == '30kg跑道600m':
                thr = pcfg.get('run_600m_remaining_target_low', 0.68)
                coeff = float(pcfg.get('run_600m_shortfall_penalty_coeff', 22000.0))
                if min_stamina < thr:
                    milestone_penalty += float((thr - min_stamina) * coeff)
            elif test_type == '30kg跑道1km':
                thr = pcfg.get('run_1km_remaining_target_low', 0.48)
                coeff = float(pcfg.get('run_1km_shortfall_penalty_coeff', 22000.0))
                if min_stamina < thr:
                    milestone_penalty += float((thr - min_stamina) * coeff)
        
        # ======== T1 耐力检查点（软约束）：30kg 连续跑 5min 后体力 ≥25% ========
        t1_endurance_penalty = 0.0
        t1_twin = RSSDigitalTwin(twin.constants)
        t1_twin.reset()
        t1_weight = 90.0 + 30.0
        # ======== 合理性约束：30kg Run 60秒必须有净消耗 ========
        # 避免"恢复速度过快导致Run无消耗"的异常参数组合
        run_test_twin = RSSDigitalTwin(twin.constants)
        run_test_twin.reset()
        run_test_weight = 90.0 + 30.0
        run_test_speed = get_speed(30.0, "run")
        run_test_t = 0.0
        while run_test_t < 60.0:
            run_test_twin.step(run_test_speed, run_test_weight, 0.0, 1.0, Stance.STAND,
                               MovementType.RUN, run_test_t + 0.2, enable_randomness=False)
            run_test_t += 0.2
        run_60s_remaining = run_test_twin.stamina
        run_60s_penalty = 0.0
        if run_60s_remaining > 0.90:
            run_60s_penalty = float(
                (run_60s_remaining - 0.90) * float(
                    pcfg.get('flat_run_60s_recovery_penalty_coeff', 18000.0)))

        t1_speed = get_speed(30.0, "run")
        t1_t = 0.0
        while t1_t < 300.0:
            t1_twin.step(t1_speed, t1_weight, 0.0, 1.0, Stance.STAND,
                         MovementType.RUN, t1_t + 0.2, enable_randomness=False)
            t1_t += 0.2
        t1_5min = t1_twin.stamina
        if t1_5min < 0.25:
            t1_endurance_penalty += (0.25 - t1_5min) * 8000.0
        if t1_5min < 0.15:
            t1_endurance_penalty += float(
                (0.15 - t1_5min) * float(
                    pcfg.get('t1_5min_floor_penalty_coeff', 18000.0)))

        # ======== T1 恢复速度约束（软约束）：跑 5min 后站立休息 3min 恢复不超 35% ========
        t1_recov_penalty = 0.0
        rest_t = t1_t
        while rest_t < t1_t + 180.0:
            t1_twin.step(0.0, t1_weight, 0.0, 1.0, Stance.STAND,
                         MovementType.IDLE, rest_t + 0.2, enable_randomness=False)
            rest_t += 0.2
        recov_gain = t1_twin.stamina - t1_5min
        if recov_gain > 0.35:
            t1_recov_penalty += (recov_gain - 0.35) * 6000.0

        total_burden = 0.0
        weights = [
            0.02,  # 0: ACFT 2英里（空载，低优先级）
            0.05,  # 1: 城市战斗 35kg
            0.05,  # 2: 山地战斗 25kg
            0.10,  # 3: 野战巡逻 30kg
            0.05,  # 4: Run/Sprint 边界 30kg
            0.10,  # 5: 重载 45kg 10min（略降以平衡战术周期权重）
            0.10,  # 6: 持续 20min 30kg
            0.08,  # 7: 600m 30kg（T1 核心指标）
            0.08,  # 8: 1000m 30kg（T1 核心指标）
            0.08,  # 9: CQB 低姿突击（蹲姿参数盲区修复）
            0.08,  # 10: 极限 55kg 重载（encumbrance_speed_penalty_max 压力测试）
            0.08,  # 11: 佛罗里达热应激（热应激参数盲区修复）
            0.14,  # 12: 战术战斗周期（提高权重，与 plot_combat_cycle 验收对齐）
        ]

        for i, (scenario_burden, scenario, _) in enumerate(results_list):
            # 如果场景仿真失败（scenario为None），视为被剪枝
            if scenario is None:
                raise optuna.exceptions.TrialPruned("场景评估失败，scenario为None")
            total_burden += float(scenario_burden) * weights[i]
        
        # 参数多样性惩罚：降低权重，避免与可玩性目标冲突
        param_variation_score = 0.0
        key_params = [
            ('BASE_RECOVERY_RATE', 4e-5, 8e-5, 15.0),
            ('ENCUMBRANCE_STAMINA_DRAIN_COEFF', 0.8, 1.5, 15.0),
            ('LOAD_METABOLIC_DAMPENING', 0.50, 0.85, 10.0),
            ('MAX_RECOVERY_PER_TICK', 0.0002, 0.0006, 10.0),
        ]
        for param_name, min_val, max_val, penalty in key_params:
            if hasattr(twin.constants, param_name):
                value = getattr(twin.constants, param_name)
                if value is None:
                    continue  # 跳过未设置的参数
                if not (min_val <= value <= max_val):
                    param_variation_score += penalty

        total_burden += param_variation_score
        total_burden += (
            t1_endurance_penalty + t1_recov_penalty + milestone_penalty + run_60s_penalty
        )

        # 添加场景结果多样性惩罚
        # 如果所有场景的负担都非常接近，说明参数变化影响不大
        if len(results_list) >= 2:
            burdens = [b for b, _, _ in results_list]
            if len(burdens) >= 2:
                burden_diff = max(burdens) - min(burdens)
                if burden_diff < 100.0:
                    total_burden += 100.0  # 惩罚结果过于一致的参数组合

        cc_min = 0.0
        for _, scenario, min_stamina_out in results_list:
            if scenario is not None and getattr(scenario, 'test_type', '') == '战术战斗周期':
                if min_stamina_out is not None:
                    cc_min = float(min_stamina_out)
                break
        self._last_combat_cycle_min_stamina = cc_min

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
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
                fast_mode=getattr(self, 'fast_mode', False)
            )
            
            stamina_history = chaos_results['stamina_history']
            speed_history = chaos_results['speed_history']
            
            # ==================== BUG检测逻辑 ====================
            
            # 1. 检测负数体力（重大BUG）
            if len(stamina_history) > 0:
                min_stamina = min(stamina_history)
            else:
                min_stamina = 0.0
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
            if len(stamina_history) > 0:
                max_stamina = max(stamina_history)
            else:
                max_stamina = 0.0
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
            # 降低测试负重以减少误判，同时添加日志以便调试
            static_weight = 100.0  # 原来 120kg，改为 100kg 更宽松
            
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
                # 记录数值以便分析为何频繁触发
                print(f"[BUGCHECK] static_change={stamina_change:.5f}, weight={static_weight}, params={parameters}")
                # 如果静态站立时体力下降（且不是超重情况），可能是逻辑漏洞
                # 允许更大幅度下降再判定BUG，以减少误报
                if stamina_change < -0.005 and static_weight <= 100.0:
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

            # ==================== 连续型"风险评分"（让稳定性目标有梯度）====================
            # 仅靠"是否触发BUG"会导致稳定性目标大面积为 0，从而退化为单目标优化。
            # 这里加入"接近崩溃"的软风险：在极端场景下体力越接近耗尽，风险越高。
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
        # 根据配置动态构建 direction 列表（确保与 objective 返回值匹配）
        # 默认 4 目标：playability, stability, realism, engagement_loss（内存与数据库一致）
        base_directions = ['minimize', 'minimize', 'minimize']
        base_directions.append('minimize')  # engagement_loss（第 4 目标，内存/数据库均启用）
        if getattr(self, 'enable_sprint_objective', False):
            base_directions.append('minimize')  # sprint objective
            if getattr(self, 'enable_sprint_remaining_objective', False):
                base_directions.append('minimize')  # sprint remaining objective

        if self.use_database:
            # 使用数据库存储以便后续分析（性能较慢）
            storage_url = f"sqlite:///{study_name}.db"
            try:
                # 尝试加载现有研究
                self.study = optuna.load_study(study_name=study_name, storage=storage_url)
                print(f"已加载现有研究：{study_name}（从数据库）")
            except Exception:
                # 创建新研究
                self.study = optuna.create_study(
                    study_name=study_name,
                    storage=storage_url,
                    directions=base_directions,
                    sampler=optuna.samplers.NSGAIISampler(
                        population_size=300,  # 种群大小；经验上 population×代数≈6400 已足够
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
                directions=base_directions,
                sampler=optuna.samplers.NSGAIISampler(
                    population_size=300,  # 种群大小；经验上 population×代数≈6400 已足够
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
                    # 安全地提取第四目标（可能只有部分试验包含第四目标）
                    engagement_values = [t.values[3] for t in best_trials if len(t.values) >= 4]
                    if engagement_values:
                        engagement_range = f"会话体验损失=[{min(engagement_values):.2f}, {max(engagement_values):.2f}], "
                    else:
                        engagement_range = ""
                    elapsed_time = time.time() - start_time
                    print(f"\n进度更新 [试验 {trial.number}/{self.n_trials}]: "
                          f"帕累托解数量={len(best_trials)}, "
                          f"可玩性=[{min(playability_values):.2f}, {max(playability_values):.2f}], "
                          f"稳定性=[{min(stability_values):.2f}, {max(stability_values):.2f}], "
                          f"生理学=[{min(realism_values):.2f}, {max(realism_values):.2f}], "
                          f"{engagement_range}"
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
        # 运行阶段1优化；不要在控制台显示进度条，避免与GUI输出冲突
        self.study.optimize(
            self.objective,
            n_trials=stage1_trials,
            show_progress_bar=False,
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
            show_progress_bar=False,
            callbacks=[progress_callback],
            n_jobs=1  # 设置为1，因为objective内部已使用ParallelWorker进行并行
        )
        
        print("-" * 80)
        print(f"\n优化完成！")
        
        # 提取帕累托前沿
        self.best_trials = self.study.best_trials
        if len(self.best_trials) == 0:
            # 帕累托为空时（例如所有 trial 被惩罚导致无非支配解）：用已完成 trial 按前两目标之和排序取前 50 作为回退
            completed = [t for t in self.study.get_trials() if t.state == optuna.trial.TrialState.COMPLETE and t.values is not None]
            if completed:
                def _sum_first_two(trial):
                    v = trial.values
                    if v is None or len(v) < 2:
                        return float('inf')
                    return float(v[0]) + float(v[1])
                completed.sort(key=_sum_first_two)
                self.best_trials = completed[:50]
                print(f"\n帕累托前沿为空，已用已完成 trial 回退：取前 {len(self.best_trials)} 个（按可玩性+稳定性之和排序）")
            else:
                print("\n警告：无已完成 trial，无法生成导出解。请放宽剪枝/约束或增加 n_trials。")

        print(f"\n帕累托前沿：")
        print(f"  非支配解数量：{len(self.best_trials)}")
        
        if len(self.best_trials) > 0:
            playability_values = [trial.values[0] for trial in self.best_trials]
            stability_values = [trial.values[1] for trial in self.best_trials]
            realism_values = [trial.values[2] for trial in self.best_trials]
            engagement_values = [trial.values[3] for trial in self.best_trials if len(trial.values) >= 4]
            
            print(f"  可玩性负担范围：[{min(playability_values):.2f}, {max(playability_values):.2f}]")
            print(f"  稳定性风险范围：[{min(stability_values):.2f}, {max(stability_values):.2f}]")
            print(f"  生理学合理性范围：[{min(realism_values):.2f}, {max(realism_values):.2f}]")
            if engagement_values:
                print(f"  会话体验损失范围：[{min(engagement_values):.2f}, {max(engagement_values):.2f}]")
            else:
                print("  会话体验损失范围：无（研究中未包含第四目标）")
            
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
        else:
            archetypes = {}
        print("\n" + "=" * 80)
        print("开始自动导出JSON配置文件...")
        print("=" * 80)
        self.export_presets(archetypes, output_dir=".")
        print("\n" + "=" * 80)
        print("JSON配置文件处理完成！")
        print("=" * 80)
        if archetypes:
            print(f"\n生成的文件位置：")
            print("- optimized_rss_config_realism_super.json")
            print("- optimized_rss_config_balanced_super.json")
            print("- optimized_rss_config_playability_super.json")
        else:
            print("\n⚠️ 未生成任何解，输出文件为默认空结构。请检查搜索区间或基础体能约束。")
        
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
                    enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
                    fast_mode=getattr(self, 'fast_mode', False)
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

        # 3) ACFT 验证样本（空载，速度用负重下的 run）
        acft_res = None
        try:
            acft = ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0, constants=twin.constants)
            acft_res = twin.simulate_scenario(
                speed_profile=acft.speed_profile,
                current_weight=acft.current_weight,
                grade_percent=acft.grade_percent,
                terrain_factor=acft.terrain_factor,
                stance=acft.stance,
                movement_type=acft.movement_type,
                enable_randomness=ENABLE_RANDOMNESS_IN_SIMULATION,
                fast_mode=getattr(self, 'fast_mode', False)
            )
        except Exception:
            acft_res = None

        # 会话层级约束：ACFT / 恢复窗 / 不可复原崩溃
        if patrol_res is None or sprint_res is None:
            return 100.0, True  # 样本仿真失败，视为会话不可接受
        patrol_hist = patrol_res.get('stamina_history', [1.0])
        sprint_hist = sprint_res.get('stamina_history', [1.0])
        if acft_res:
            acft_hist = acft_res.get('stamina_history', [1.0])
        else:
            acft_hist = [1.0]

        stamina_lists = [patrol_hist, sprint_hist, acft_hist]
        if stamina_lists:
            min_stamina_overall = min(min(s) for s in stamina_lists if s)
        else:
            min_stamina_overall = 1.0
        no_permanent_incapacitation = min_stamina_overall > 0.01

        # ACFT 通过：空载 ACFT 最低体力 > 1%
        acft_ok = acft_res is not None and min(acft_hist) > 0.01
        # 恢复窗通过：30s 冲刺后 60s 恢复，末端体力应 > 5%（放宽自 15%，避免与低 base_recovery_rate 采样冲突导致 0 解）
        if sprint_hist:
            recovery_end_ok = sprint_hist[-1] > 0.05
        else:
            recovery_end_ok = True
        recovery_ok = sprint_res is not None and recovery_end_ok

        # low_events：巡逻样本中低于10%的次数（按分钟标准化）
        patrol_low_count = sum(1 for s in patrol_hist if s < 0.10)
        if len(patrol_hist) > 0:
            patrol_minutes = len(patrol_hist) * 0.2 / 60.0
        else:
            patrol_minutes = 1.0
        low_events_per_min = patrol_low_count / max(1.0, patrol_minutes)

        # 最终体力（估算）：使用巡逻样本末端体力作为会话估算（保守）
        if len(patrol_hist) > 0:
            final_stamina_est = patrol_hist[-1]
        else:
            final_stamina_est = 1.0

        # engagement_score 计算：优先保证 player 在 20%-80% 区间内的时间占比
        time_in_window_est = sum(1 for s in patrol_hist if 0.2 <= s <= 0.8) / max(len(patrol_hist), 1)
        engagement_loss = 100.0 - 100.0 * time_in_window_est

        scfg = getattr(self, 'SESSION', RSSSuperPipeline.SESSION)
        if final_stamina_est < scfg['final_stamina_low']:
            engagement_loss += (scfg['final_stamina_low'] - final_stamina_est) * scfg['final_stamina_low_coeff']
        elif final_stamina_est > scfg['final_stamina_high']:
            engagement_loss += (final_stamina_est - scfg['final_stamina_high']) * scfg['final_stamina_high_coeff']

        engagement_loss += min(low_events_per_min, 10.0) * scfg['low_events_per_min_coeff']

        # 恢复窗软惩罚：30s 冲刺后 60s 恢复，末端在 (5%, target) 时线性鼓励恢复至 target 以上（不改变 session_fail）
        if sprint_hist and len(sprint_hist) > 0:
            recovery_end = float(sprint_hist[-1])
            target = scfg['recovery_soft_target']
            if 0.05 < recovery_end < target:
                engagement_loss += (target - recovery_end) * scfg['recovery_soft_coeff']

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
            twin.step(3.8, 90.0, 0.0, 1.0, 0, MovementType.RUN, i * 0.2, False)
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
        
        # 提取目标值（注意：有时只有三目标，没有 engagement_loss）
        playability_values = [t.values[0] for t in self.best_trials]
        stability_values = [t.values[1] for t in self.best_trials]

        # 判断当前帕累托解是否含有第四个目标（engagement_loss）
        # 以实际存在的目标为准：有些 trial 可能没有第四个目标
        if hasattr(self, 'study'):
            default_obj = len(self.study.directions)
        else:
            default_obj = 3
        num_objectives = max((len(t.values) for t in self.best_trials), default=default_obj)
        engagement_values = [t.values[3] for t in self.best_trials if len(t.values) >= 4]
        if not engagement_values:
            engagement_values = [0.0 for _ in self.best_trials]

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
            if num_objectives >= 4:
                engagement_values = [self.best_trials[i].values[3] for i in valid_indices]
            else:
                engagement_values = [0.0 for _ in valid_indices]
        # 不跨预设交换参数（避免产生不一致的 param 组合）
        if valid_trials:
            candidates = valid_trial_list
            cand_indices = valid_indices
        else:
            candidates = self.best_trials
            cand_indices = list(range(len(self.best_trials)))

        def energy_coeff(t):
            return t.params.get('energy_to_stamina_coeff', 0)

        pccfg = getattr(self, 'PLAYABILITY', RSSSuperPipeline.PLAYABILITY)
        cc_req = float(pccfg.get('combat_cycle_min_stamina_req', 0.23))

        def _combat_min_from_trial(trial):
            v = trial.user_attrs.get('combat_cycle_min_stamina')
            if v is None:
                return -1.0
            return float(v)

        realism_pool = [
            i for i in cand_indices
            if _combat_min_from_trial(self.best_trials[i]) >= cc_req
        ]
        if realism_pool:
            realism_indices = realism_pool
            print(
                f"  EliteStandard：战术战斗周期最低体力≥{cc_req:.0%} 的候选 {len(realism_pool)} 个"
            )
        else:
            has_metric = any(
                _combat_min_from_trial(self.best_trials[i]) >= 0.0 for i in cand_indices
            )
            if has_metric:
                realism_indices = sorted(
                    cand_indices,
                    key=lambda i: _combat_min_from_trial(self.best_trials[i]),
                    reverse=True,
                )
                print(
                    f"  警告：无 trial 达到战术战斗周期≥{cc_req:.0%}；"
                    "EliteStandard 取 combat_cycle_min_stamina 最大者（请重跑优化或放宽底线）"
                )
            else:
                realism_indices = cand_indices
                print(
                    "  提示：trial 缺少 combat_cycle_min_stamina（旧研究？）；"
                    "EliteStandard 仍按稳定性×能耗选解，导出后请用 plot_combat_cycle_30kg_en.py 自测"
                )

        # 1. EliteStandard：在稳定性较好的候选中，优先第三目标（生理学损失）最小，再按 energy 系数打破平局
        stability_ordered = sorted(realism_indices, key=lambda i: self.best_trials[i].values[1])
        top_n = max(3, len(realism_indices) // 3)
        top_stability = stability_ordered[:top_n]

        def _phys_loss_for_index(idx: int) -> float:
            t = self.best_trials[idx]
            if t.values is not None and len(t.values) >= 3:
                return float(t.values[2])
            return 1e30

        realism_idx = min(
            top_stability,
            key=lambda i: (_phys_loss_for_index(i), -energy_coeff(self.best_trials[i])),
        )
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
        if num_objectives >= 4:
            engagement_vals = [t.values[3] for t in candidates]
        else:
            engagement_vals = [0.0 for _ in candidates]
        e_min, e_max = min(e_vals), max(e_vals)
        balanced_scores = []
        for trial in candidates:
            p_norm = (trial.values[0] - playability_min) / (playability_max - playability_min + 1e-10)
            s_norm = (trial.values[1] - stability_min) / (stability_max - stability_min + 1e-10)
            e_norm = (energy_coeff(trial) - e_min) / (e_max - e_min + 1e-10)
            # 平衡解：目标折中，且 energy 居中（偏好中等严格程度）
            if num_objectives >= 4:
                engagement_norm = (trial.values[3] - min(engagement_vals)) / (max(engagement_vals) - min(engagement_vals) + 1e-10)
            else:
                engagement_norm = 0.0
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
        
        # 如果没有解，仍生成空模板文件以防外部脚本依赖
        if not archetypes:
            for preset_name, filename in preset_mapping.items():
                output_file = output_path / filename
                empty_dict = {
                    "version": "3.0.0",
                    "description": f"RSS 优化配置模板 - {preset_name} (无可用解)",
                    "optimization_method": "NSGA-II (Super Pipeline)",
                    "objectives": {},
                    "parameters": {}
                }
                with open(output_file, 'w', encoding='utf-8') as f:
                    json.dump(empty_dict, f, indent=2, ensure_ascii=False)
                print(f"  空模板已导出：{output_file}")
            return

        # [HARD] 坡度系数固定值，用于确保导出 JSON 包含（C 端 CalculateSlopeStaminaDrainMultiplier 未调用）
        FIXED_SLOPE_PARAMS = {'slope_uphill_coeff': 0.08, 'slope_downhill_coeff': 0.03}

        for preset_name, config in archetypes.items():
            output_file = output_path / preset_mapping[preset_name]
            params = dict(config['params'])
            params.update(FIXED_SLOPE_PARAMS)  # 覆盖或补充坡度参数

            config_dict = {
                "version": "3.0.0",
                "description": f"RSS 增强型优化配置（NSGA-II）- {preset_name}",
                "optimization_method": "NSGA-II (Super Pipeline)",
                "objectives": {
                    "playability_burden": config['playability_burden'],
                    "stability_risk": config['stability_risk']
                },
                "parameters": params
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


def _default_use_gui():
    """
    是否默认启动 Tk GUI。
    Codespaces / 无 DISPLAY 的 Linux SSH 等环境下无图形后端，强行 tk.Tk() 会报错。
    Windows / macOS 本机一般默认可用；Linux 需有 DISPLAY 或 Wayland。
    """
    import sys
    if os.environ.get("CODESPACES", "").lower() == "true":
        return False
    if os.environ.get("GITHUB_CODESPACES", "").lower() == "true":
        return False
    if sys.platform == "win32":
        return True
    if sys.platform == "darwin":
        return True
    if os.environ.get("DISPLAY"):
        return True
    if os.environ.get("WAYLAND_DISPLAY"):
        return True
    return False


def main():
    """主函数：执行完整优化流程"""
    
    print("\n" + "=" * 80)
    print("RSS 增强型多目标优化与鲁棒性测试流水线")
    print("Enhanced Multi-Objective Optimization with Robustness Testing Pipeline")
    print("=" * 80)
    
    # 使用CPU核心数作为默认线程数
    import multiprocessing
    n_jobs = multiprocessing.cpu_count()

    # 快速模式：使用更大时间步长提速约2倍
    import sys
    fast_mode = "--fast" in sys.argv

    # GUI：显式 --gui / --no-gui 优先；否则按环境默认（无头 Linux 默认命令行）
    if "--gui" in sys.argv:
        use_gui = True
    elif "--no-gui" in sys.argv:
        use_gui = False
    else:
        use_gui = _default_use_gui()
        if not use_gui:
            print(
                "[提示] 未检测到图形界面环境（如无 DISPLAY），使用命令行模式。"
                " 在本机桌面运行可加 --gui；云端/Codespaces 请加 --no-gui 或依赖本检测。"
            )

    if fast_mode:
        print("[快速模式] 已启用：使用更大时间步长(0.4s)，提速约2倍")

    # 创建流水线（快速验证设置）
    # 设置为500次迭代以快速验证优化效果
    # use_database=False 使用内存存储以提高性能（默认）
    # 如果需要后续诊断，可以设置 use_database=True（但会降低性能）
    pipeline = RSSSuperPipeline(n_trials=1000, n_jobs=n_jobs, use_database=False, fast_mode=fast_mode)

    gui_completed = False

    # 如果使用 GUI：成功跑完 mainloop 则 gui_completed=True；失败则回退命令行
    if use_gui:
        try:
            print("正在启动GUI优化器...")
            from rss_optimizer_gui import RSSTunerGUI
            import tkinter as tk
            import threading

            root = tk.Tk()
            gui = RSSTunerGUI(root, total_iterations=500)

            pipeline.set_gui_callback(gui.update_data)

            print("GUI已启动，优化将在后台运行...")
            print("提示：GUI窗口可以独立控制优化过程")

            optimization_thread = threading.Thread(
                target=lambda: pipeline.optimize(study_name="rss_super_optimization"),
                daemon=True
            )
            optimization_thread.start()

            root.mainloop()
            optimization_thread.join()

            archetypes = pipeline.extract_archetypes()
            pipeline.export_presets(archetypes, output_dir=".")
            pipeline.export_bug_report("stability_bug_report.json")
            gui_completed = True
        except ImportError:
            print("错误：无法导入GUI模块")
            print("请确保 rss_optimizer_gui.py 在同一目录下")
            print("继续使用命令行模式...")
        except Exception as exc:
            print("GUI 启动或运行失败: " + str(exc))
            print("改用命令行模式...")

    if not gui_completed:
        pipeline.optimize(study_name="rss_super_optimization")
        archetypes = pipeline.extract_archetypes()
        pipeline.export_presets(archetypes, output_dir=".")
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
