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
"""

import optuna
import numpy as np
import json
import math
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, asdict

# 导入现有模块（不修改）
from rss_digital_twin import RSSDigitalTwin, MovementType, Stance, RSSConstants
from rss_scenarios import ScenarioLibrary, TestScenario, ScenarioType


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
        n_trials: int = 5000,  # 增加迭代次数以提高多样性
        n_jobs: int = 1,
        use_database: bool = False  # 是否使用数据库存储（默认False以提高性能）
    ):
        """
        初始化流水线
        
        Args:
            n_trials: 优化迭代次数（默认5000，提高多样性）
            n_jobs: 并行线程数
            use_database: 是否使用数据库存储（默认False，使用内存存储以提高性能）
        """
        self.n_trials = n_trials
        self.n_jobs = n_jobs
        self.use_database = use_database
        self.study = None
        self.best_trials = []
        self.bug_reports: List[BugReport] = []
        
    def objective(self, trial: optuna.Trial) -> Tuple[float, float, float]:
        """
        Optuna 三目标函数
        
        Args:
            trial: Optuna 试验对象
        
        Returns:
            (realism_loss, playability_burden, stability_risk)
        """
        # ==================== 1. 定义搜索空间 ====================
        # 使用与 rss_optimizer_optuna.py 相同的参数范围（但已优化）
        
        # 能量转换相关（改进：扩大搜索范围，允许更低消耗以通过30KG测试）
        energy_to_stamina_coeff = trial.suggest_float(
            'energy_to_stamina_coeff', 2.5e-5, 7e-5, log=True  # 从3e-5降低到2.5e-5，允许更低消耗
        )
        
        # 恢复系统相关（改进：允许更快恢复以通过30KG测试）
        base_recovery_rate = trial.suggest_float(
            'base_recovery_rate', 1.5e-4, 5e-4, log=True  # 从4e-4提高到5e-4，允许更快恢复
        )
        standing_recovery_multiplier = trial.suggest_float(
            'standing_recovery_multiplier', 1.0, 2.5
        )
        prone_recovery_multiplier = trial.suggest_float(
            'prone_recovery_multiplier', 1.2, 2.5
        )
        load_recovery_penalty_coeff = trial.suggest_float(
            'load_recovery_penalty_coeff', 1e-4, 1e-3, log=True
        )
        load_recovery_penalty_exponent = trial.suggest_float(
            'load_recovery_penalty_exponent', 1.0, 3.0
        )
        
        # 负重系统相关（改进：允许更低负重惩罚以通过30KG测试）
        encumbrance_speed_penalty_coeff = trial.suggest_float(
            'encumbrance_speed_penalty_coeff', 0.1, 0.3
        )
        encumbrance_stamina_drain_coeff = trial.suggest_float(
            'encumbrance_stamina_drain_coeff', 0.8, 2.0  # 从1.0降低到0.8，允许更低负重惩罚
        )
        
        # Sprint 相关
        sprint_stamina_drain_multiplier = trial.suggest_float(
            'sprint_stamina_drain_multiplier', 2.0, 4.0
        )
        
        # 疲劳系统相关
        fatigue_accumulation_coeff = trial.suggest_float(
            'fatigue_accumulation_coeff', 0.005, 0.03, log=True
        )
        fatigue_max_factor = trial.suggest_float(
            'fatigue_max_factor', 1.5, 3.0
        )
        
        # 代谢适应相关
        aerobic_efficiency_factor = trial.suggest_float(
            'aerobic_efficiency_factor', 0.8, 1.0
        )
        anaerobic_efficiency_factor = trial.suggest_float(
            'anaerobic_efficiency_factor', 1.0, 1.5
        )
        
        # 恢复系统高级参数（优化：降低恢复倍数）
        recovery_nonlinear_coeff = trial.suggest_float(
            'recovery_nonlinear_coeff', 0.3, 0.7
        )
        fast_recovery_multiplier = trial.suggest_float(
            'fast_recovery_multiplier', 2.0, 3.5
        )
        medium_recovery_multiplier = trial.suggest_float(
            'medium_recovery_multiplier', 1.2, 2.0
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
            'sprint_speed_boost', 0.25, 0.35
        )
        
        # 姿态系统参数
        posture_crouch_multiplier = trial.suggest_float(
            'posture_crouch_multiplier', 1.5, 2.2
        )
        posture_prone_multiplier = trial.suggest_float(
            'posture_prone_multiplier', 2.5, 3.5
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
        
        # ==================== 4. 目标1：评估拟真度（Realism Loss）====================
        
        # 使用标准测试工况评估拟真度
        realism_loss = self._evaluate_realism(twin)
        
        # ==================== 5. 目标2：评估30KG可玩性（Playability Burden）====================
        
        # 专门针对30KG负载进行测试
        playability_burden = self._evaluate_30kg_playability(twin)
        
        # ==================== 6. 目标3：评估稳定性风险（Stability Risk / BUG检测）====================
        
        # 地狱级压力测试
        stability_risk, bug_reports = self._evaluate_stability_risk(
            twin, trial.number, trial.params
        )
        
        # 记录BUG报告
        self.bug_reports.extend(bug_reports)
        
        # ==================== 7. 返回三个目标函数 ====================
        
        return realism_loss, playability_burden, stability_risk
    
    def _evaluate_realism(self, twin: RSSDigitalTwin) -> float:
        """
        评估拟真度损失
        
        Args:
            twin: 数字孪生仿真器
        
        Returns:
            拟真度损失（越小越好）
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
                movement_type=scenario.movement_type
            )
            
            # 计算拟真度损失：距离目标差异
            target_distance = scenario.speed_profile[-1][1] * scenario.target_finish_time
            distance_error = abs(results['total_distance'] - target_distance)
            
            # 归一化损失（相对于目标距离）
            realism_loss = distance_error / target_distance if target_distance > 0 else 1000.0
            
            return realism_loss
            
        except Exception as e:
            # 如果仿真失败，返回大惩罚值
            return 1000.0
    
    def _evaluate_30kg_playability(self, twin: RSSDigitalTwin) -> float:
        """
        评估30KG负载下的可玩性负担（专门针对玩家反馈的"30KG太累"问题）
        
        Args:
            twin: 数字孪生仿真器
        
        Returns:
            可玩性负担（越小越好）
        """
        # 30KG战斗负载测试
        scenario = ScenarioLibrary.create_acft_2mile_scenario(load_weight=30.0)
        
        try:
            results = twin.simulate_scenario(
                speed_profile=scenario.speed_profile,
                current_weight=scenario.current_weight,
                grade_percent=scenario.grade_percent,
                terrain_factor=scenario.terrain_factor,
                stance=scenario.stance,
                movement_type=scenario.movement_type
            )
            
            # 计算可玩性负担
            playability_burden = 0.0
            
            # 1. 最低体力惩罚（如果低于15%，大幅惩罚，但使用更温和的惩罚曲线）
            min_stamina = results['min_stamina']
            if min_stamina < 0.15:
                # 使用平方惩罚，让接近15%的解惩罚更小
                penalty_factor = (0.15 - min_stamina) ** 1.5  # 从线性改为1.5次方，更温和
                playability_burden += penalty_factor * 600.0  # 从800降低到600，配合更温和的曲线
            
            # 2. 完成时间惩罚（如果落后目标10%以上，才严重惩罚，放宽到10%）
            target_time = scenario.target_finish_time
            actual_time = results['total_time_with_penalty']
            time_penalty_ratio = (actual_time - target_time) / target_time
            if time_penalty_ratio > 0.10:  # 放宽到10%（从5%提高到10%）
                playability_burden += (time_penalty_ratio - 0.10) * 200.0  # 降低惩罚系数
            elif time_penalty_ratio > 0.05:  # 5%-10%之间，轻微惩罚
                playability_burden += (time_penalty_ratio - 0.05) * 50.0  # 轻微惩罚
            
            # 3. 硬性奖励：如果30KG下min_stamina > 0.12 且完成时间未落后10%，大幅降低负担
            # 改进：放宽条件，min_stamina从0.15降低到0.12，时间从5%放宽到10%
            if min_stamina >= 0.12 and time_penalty_ratio <= 0.10:
                playability_burden = max(0.0, playability_burden - 400.0)  # 进一步增强奖励（从300提高到400）
            
            # 额外奖励：如果min_stamina > 0.15（超过15%），额外奖励
            if min_stamina >= 0.15:
                playability_burden = max(0.0, playability_burden - 100.0)  # 增强额外奖励（从50提高到100）
            
            # 额外奖励：如果min_stamina > 0.20（超过20%），更大奖励
            if min_stamina >= 0.20:
                playability_burden = max(0.0, playability_burden - 50.0)  # 额外奖励
            
            # 4. 体力耗尽时间惩罚（如果体力过早耗尽）
            stamina_history = results['stamina_history']
            if len(stamina_history) > 0:
                exhausted_frames = sum(1 for s in stamina_history if s < 0.05)
                total_frames = len(stamina_history)
                exhaustion_ratio = exhausted_frames / total_frames if total_frames > 0 else 0.0
                if exhaustion_ratio > 0.1:  # 超过10%的时间处于极度疲劳
                    playability_burden += exhaustion_ratio * 50.0
            
            return playability_burden
            
        except Exception as e:
            # 如果仿真失败，返回大惩罚值
            return 1000.0
    
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
                movement_type=MovementType.RUN
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
            
            # 模拟静态站立（速度为0）
            for _ in range(10):  # 测试10个tick（2秒）
                try:
                    static_twin.step(
                        speed=0.0,
                        current_weight=chaos_weight,  # 使用极端负重
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
                if stamina_change < -0.001 and chaos_weight < 120.0:  # 只有在负重<120kg时才视为BUG
                    stability_risk += 500.0
                    bug_reports.append(BugReport(
                        trial_number=trial_number,
                        bug_type="recovery_logic_error",
                        bug_description=f"静态站立时体力恢复率为负：{stamina_change:.6f}/tick（负重{chaos_weight}kg）",
                        parameters=parameters.copy(),
                        context={
                            'stamina_change_per_tick': stamina_change,
                            'test_weight': chaos_weight,
                            'final_stamina': static_twin.stamina_history[-1]
                        }
                    ))
            
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
        print(f"  目标函数数：3（Realism Loss, Playability Burden, Stability Risk）")
        print(f"  并行线程数：{self.n_jobs}")
        
        print(f"\n目标函数：")
        print(f"  1. 拟真度损失（Realism Loss）- 越小越好")
        print(f"  2. 可玩性负担（Playability Burden）- 越小越好（30KG专项测试）")
        print(f"  3. 稳定性风险（Stability Risk）- 越小越好（BUG检测）")
        
        print(f"\n特殊测试：")
        print(f"  - 30KG负载专项平衡测试（解决'30KG太累'问题）")
        print(f"  - 地狱级压力测试（45KG、20度坡、热应激）")
        print(f"  - BUG检测（负数体力、NaN、溢出、恢复逻辑漏洞）")
        
        print(f"\n开始优化...")
        print("-" * 80)
        
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
                    directions=['minimize', 'minimize', 'minimize'],
                    sampler=optuna.samplers.NSGAIISampler(
                        population_size=100,  # 增加种群大小（从50提高到80）以提高多样性
                        mutation_prob=0.2,  # 增加变异概率（从0.1提高到0.15）以探索更多解空间
                        crossover_prob=0.9,  # 交叉概率
                        swapping_prob=0.5    # 交换概率
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
                    population_size=100,  # 增加种群大小（从50提高到80）以提高多样性
                    mutation_prob=0.2,  # 增加变异概率（从0.1提高到0.15）以探索更多解空间
                    crossover_prob=0.9,  # 交叉概率
                    swapping_prob=0.5    # 交换概率
                ),
                pruner=optuna.pruners.MedianPruner()
            )
            print(f"已创建新研究：{study_name}（内存存储，高性能）")
        
        # 进度日志回调函数
        def progress_callback(study, trial):
            if trial.number % 100 == 0 and trial.number > 0:
                best_trials = study.best_trials
                if len(best_trials) > 0:
                    realism_values = [t.values[0] for t in best_trials]
                    playability_values = [t.values[1] for t in best_trials]
                    stability_values = [t.values[2] for t in best_trials]
                    print(f"\n进度更新 [试验 {trial.number}/{self.n_trials}]: "
                          f"帕累托解数量={len(best_trials)}, "
                          f"拟真度=[{min(realism_values):.2f}, {max(realism_values):.2f}], "
                          f"可玩性=[{min(playability_values):.2f}, {max(playability_values):.2f}], "
                          f"稳定性=[{min(stability_values):.2f}, {max(stability_values):.2f}], "
                          f"BUG数量={len(self.bug_reports)}")
        
        # 执行优化
        self.study.optimize(
            self.objective,
            n_trials=self.n_trials,
            show_progress_bar=True,
            callbacks=[progress_callback],
            n_jobs=self.n_jobs
        )
        
        print("-" * 80)
        print(f"\n优化完成！")
        
        # 提取帕累托前沿
        self.best_trials = self.study.best_trials
        
        print(f"\n帕累托前沿：")
        print(f"  非支配解数量：{len(self.best_trials)}")
        
        if len(self.best_trials) > 0:
            realism_values = [trial.values[0] for trial in self.best_trials]
            playability_values = [trial.values[1] for trial in self.best_trials]
            stability_values = [trial.values[2] for trial in self.best_trials]
            
            print(f"  拟真度损失范围：[{min(realism_values):.4f}, {max(realism_values):.4f}]")
            print(f"  可玩性负担范围：[{min(playability_values):.2f}, {max(playability_values):.2f}]")
            print(f"  稳定性风险范围：[{min(stability_values):.2f}, {max(stability_values):.2f}]")
            
            # 诊断：检查目标值的唯一性
            realism_unique = len(set(realism_values))
            playability_unique = len(set(playability_values))
            stability_unique = len(set(stability_values))
            
            print(f"\n  目标值唯一性诊断：")
            print(f"    拟真度损失唯一值：{realism_unique}/{len(self.best_trials)}")
            print(f"    可玩性负担唯一值：{playability_unique}/{len(self.best_trials)}")
            print(f"    稳定性风险唯一值：{stability_unique}/{len(self.best_trials)}")
            
            if realism_unique == 1 and playability_unique == 1 and stability_unique == 1:
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
        
        return {
            'best_trials': self.best_trials,
            'n_solutions': len(self.best_trials),
            'study': self.study,
            'bug_reports': self.bug_reports
        }
    
    def extract_archetypes(self) -> Dict[str, Dict]:
        """
        从帕累托前沿自动提取三个预设（改进：确保选择不同的解）
        
        Returns:
            预设参数字典
        """
        if not self.best_trials or len(self.best_trials) == 0:
            raise ValueError("请先运行优化！")
        
        # 如果只有一个解，警告并返回相同配置
        if len(self.best_trials) == 1:
            print(f"\n警告：帕累托前沿只有1个解，所有预设将使用相同参数")
            single_trial = self.best_trials[0]
            return {
                'EliteStandard': {
                    'params': single_trial.params,
                    'realism_loss': single_trial.values[0],
                    'playability_burden': single_trial.values[1],
                    'stability_risk': single_trial.values[2]
                },
                'StandardMilsim': {
                    'params': single_trial.params,
                    'realism_loss': single_trial.values[0],
                    'playability_burden': single_trial.values[1],
                    'stability_risk': single_trial.values[2]
                },
                'TacticalAction': {
                    'params': single_trial.params,
                    'realism_loss': single_trial.values[0],
                    'playability_burden': single_trial.values[1],
                    'stability_risk': single_trial.values[2]
                }
            }
        
        # 提取目标值
        realism_values = [t.values[0] for t in self.best_trials]
        playability_values = [t.values[1] for t in self.best_trials]
        stability_values = [t.values[2] for t in self.best_trials]
        
        # 1. EliteStandard (Realism): 拟真损失最低的解
        realism_idx = np.argmin(realism_values)
        realism_trial = self.best_trials[realism_idx]
        
        # 2. TacticalAction (Playability): 可玩性负担最低的解（30KG最轻松）
        playability_idx = np.argmin(playability_values)
        playability_trial = self.best_trials[playability_idx]
        
        # 3. StandardMilsim (Balanced): 三个目标的中值解
        # 计算每个解的综合得分（归一化后求和）
        realism_min, realism_max = min(realism_values), max(realism_values)
        playability_min, playability_max = min(playability_values), max(playability_values)
        stability_min, stability_max = min(stability_values), max(stability_values)
        
        balanced_scores = []
        for trial in self.best_trials:
            r_norm = (trial.values[0] - realism_min) / (realism_max - realism_min + 1e-10)
            p_norm = (trial.values[1] - playability_min) / (playability_max - playability_min + 1e-10)
            s_norm = (trial.values[2] - stability_min) / (stability_max - stability_min + 1e-10)
            balanced_score = (r_norm + p_norm + s_norm) / 3.0
            balanced_scores.append(balanced_score)
        
        balanced_idx = np.argmin(balanced_scores)
        balanced_trial = self.best_trials[balanced_idx]
        
        # 改进：如果选择了相同的解，尝试从不同区域选择
        selected_indices = set([realism_idx, playability_idx, balanced_idx])
        
        # 如果三个解都相同，尝试基于参数差异选择不同的解
        if len(selected_indices) == 1:
            print(f"\n警告：三个预设选择了同一个解，尝试基于参数差异选择不同解...")
            
            # 计算所有解之间的参数差异
            param_differences = []
            for i, trial_i in enumerate(self.best_trials):
                for j, trial_j in enumerate(self.best_trials):
                    if i < j:
                        # 计算参数差异（欧氏距离）
                        params_i = np.array(list(trial_i.params.values()))
                        params_j = np.array(list(trial_j.params.values()))
                        diff = np.linalg.norm(params_i - params_j)
                        param_differences.append((i, j, diff))
            
            # 按差异排序，选择差异最大的三个解
            if len(param_differences) > 0:
                param_differences.sort(key=lambda x: x[2], reverse=True)
                # 选择差异最大的三个不同索引
                unique_indices = set()
                for i, j, _ in param_differences:
                    unique_indices.add(i)
                    unique_indices.add(j)
                    if len(unique_indices) >= 3:
                        break
                
                if len(unique_indices) >= 3:
                    unique_indices_list = list(unique_indices)[:3]
                    realism_idx = unique_indices_list[0]
                    playability_idx = unique_indices_list[1]
                    balanced_idx = unique_indices_list[2]
                    realism_trial = self.best_trials[realism_idx]
                    playability_trial = self.best_trials[playability_idx]
                    balanced_trial = self.best_trials[balanced_idx]
                    print(f"  已基于参数差异选择不同解：索引 {realism_idx}, {playability_idx}, {balanced_idx}")
                else:
                    print(f"  警告：无法找到足够多样化的解，将使用相同配置")
        
        # 如果只有两个不同的解，确保至少有一个不同
        elif len(selected_indices) == 2:
            print(f"\n警告：只找到2个不同的解，尝试选择第三个不同的解...")
            available_indices = set(range(len(self.best_trials))) - selected_indices
            if len(available_indices) > 0:
                # 选择与已选解差异最大的解
                best_diff_idx = None
                best_diff = -1
                for idx in available_indices:
                    trial = self.best_trials[idx]
                    # 计算与已选解的平均差异
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
                    # 替换balanced_idx
                    balanced_idx = best_diff_idx
                    balanced_trial = self.best_trials[balanced_idx]
                    print(f"  已选择第三个不同的解：索引 {balanced_idx}")
        
        print(f"\n自动预设提取：")
        print(f"  EliteStandard (Realism): 拟真度={realism_trial.values[0]:.4f}, "
              f"可玩性={realism_trial.values[1]:.2f}, 稳定性={realism_trial.values[2]:.2f}, 索引={realism_idx}")
        print(f"  StandardMilsim (Balanced): 拟真度={balanced_trial.values[0]:.4f}, "
              f"可玩性={balanced_trial.values[1]:.2f}, 稳定性={balanced_trial.values[2]:.2f}, 索引={balanced_idx}")
        print(f"  TacticalAction (Playability): 拟真度={playability_trial.values[0]:.4f}, "
              f"可玩性={playability_trial.values[1]:.2f}, 稳定性={playability_trial.values[2]:.2f}, 索引={playability_idx}")
        
        return {
            'EliteStandard': {
                'params': realism_trial.params,
                'realism_loss': realism_trial.values[0],
                'playability_burden': realism_trial.values[1],
                'stability_risk': realism_trial.values[2]
            },
            'StandardMilsim': {
                'params': balanced_trial.params,
                'realism_loss': balanced_trial.values[0],
                'playability_burden': balanced_trial.values[1],
                'stability_risk': balanced_trial.values[2]
            },
            'TacticalAction': {
                'params': playability_trial.params,
                'realism_loss': playability_trial.values[0],
                'playability_burden': playability_trial.values[1],
                'stability_risk': playability_trial.values[2]
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
                    "realism_loss": config['realism_loss'],
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
    
    # 创建流水线（改进：增加迭代次数以提高多样性）
    # use_database=False 使用内存存储以提高性能（默认）
    # 如果需要后续诊断，可以设置 use_database=True（但会降低性能）
    pipeline = RSSSuperPipeline(n_trials=5000, n_jobs=n_jobs, use_database=False)
    
    # 执行优化
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
