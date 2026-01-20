"""
Realistic Stamina System (RSS) - Multi-Objective Optimization (Optuna Version)
多目标优化器：使用 Optuna 贝叶斯优化替代 NSGA-II

核心改进：
1. 使用 Optuna 的 TPE (Tree-structured Parzen Estimator) 算法
2. 修复约束处理：从硬惩罚改为软偏离贡献
3. 提高采样效率：200 次采样 vs 5000 次评估
"""

import optuna
import numpy as np
import json
from pathlib import Path
from typing import List, Dict, Tuple, Optional

from rss_digital_twin import RSSDigitalTwin, MovementType, Stance, RSSConstants
from rss_scenarios import ScenarioLibrary, ScenarioEvaluator, TestScenario


class RSSOptunaOptimizer:
    """RSS 多目标优化器（Optuna 版本）"""
    
    def __init__(
        self,
        scenarios: Optional[List[TestScenario]] = None,
        n_trials: int = 200
    ):
        """
        初始化优化器
        
        Args:
            scenarios: 测试工况列表（如果为 None，使用标准测试套件）
            n_trials: 采样次数（默认 200）
        """
        # 创建标准测试套件
        if scenarios is None:
            scenarios = ScenarioLibrary.create_standard_test_suite()
        
        # 保存测试工况
        self.scenarios = scenarios
        
        # 采样次数
        self.n_trials = n_trials
        
        # 优化结果
        self.study = None
        self.best_trials = []
    
    def objective(self, trial: optuna.Trial) -> Tuple[float, float]:
        """
        Optuna 目标函数
        
        Args:
            trial: Optuna 试验对象
        
        Returns:
            (realism_loss, playability_burden)
        """
        # ==================== 1. 定义搜索空间 ====================
        
        # 能量转换相关
        energy_to_stamina_coeff = trial.suggest_float(
            'energy_to_stamina_coeff', 2e-5, 5e-5, log=True
        )
        
        # 恢复系统相关
        base_recovery_rate = trial.suggest_float(
            'base_recovery_rate', 1e-4, 5e-4, log=True
        )
        standing_recovery_multiplier = trial.suggest_float(
            'standing_recovery_multiplier', 1.0, 3.0
        )
        prone_recovery_multiplier = trial.suggest_float(
            'prone_recovery_multiplier', 1.5, 3.0
        )
        load_recovery_penalty_coeff = trial.suggest_float(
            'load_recovery_penalty_coeff', 1e-4, 1e-3, log=True
        )
        load_recovery_penalty_exponent = trial.suggest_float(
            'load_recovery_penalty_exponent', 1.0, 3.0
        )
        
        # 负重系统相关
        encumbrance_speed_penalty_coeff = trial.suggest_float(
            'encumbrance_speed_penalty_coeff', 0.1, 0.3
        )
        encumbrance_stamina_drain_coeff = trial.suggest_float(
            'encumbrance_stamina_drain_coeff', 1.0, 2.0
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
        
        # 恢复系统高级参数
        recovery_nonlinear_coeff = trial.suggest_float(
            'recovery_nonlinear_coeff', 0.3, 0.7
        )
        fast_recovery_multiplier = trial.suggest_float(
            'fast_recovery_multiplier', 2.5, 4.5
        )
        medium_recovery_multiplier = trial.suggest_float(
            'medium_recovery_multiplier', 1.5, 2.5
        )
        slow_recovery_multiplier = trial.suggest_float(
            'slow_recovery_multiplier', 0.6, 1.0
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
            'min_recovery_rest_time_seconds', 5.0, 15.0
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
            'swimming_base_power', 20.0, 30.0
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
            'env_heat_stress_max_multiplier', 1.2, 1.4
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
        
        # ==================== 2. 创建参数对象 ====================
        
        class OptimizationParams:
            def __init__(self):
                self.energy_to_stamina_coeff = energy_to_stamina_coeff
                self.base_recovery_rate = base_recovery_rate
                self.standing_recovery_multiplier = standing_recovery_multiplier
                self.prone_recovery_multiplier = prone_recovery_multiplier
                self.load_recovery_penalty_coeff = load_recovery_penalty_coeff
                self.load_recovery_penalty_exponent = load_recovery_penalty_exponent
                self.encumbrance_speed_penalty_coeff = encumbrance_speed_penalty_coeff
                self.encumbrance_stamina_drain_coeff = encumbrance_stamina_drain_coeff
                self.sprint_stamina_drain_multiplier = sprint_stamina_drain_multiplier
                self.fatigue_accumulation_coeff = fatigue_accumulation_coeff
                self.fatigue_max_factor = fatigue_max_factor
                self.aerobic_efficiency_factor = aerobic_efficiency_factor
                self.anaerobic_efficiency_factor = anaerobic_efficiency_factor
                self.recovery_nonlinear_coeff = recovery_nonlinear_coeff
                self.fast_recovery_multiplier = fast_recovery_multiplier
                self.medium_recovery_multiplier = medium_recovery_multiplier
                self.slow_recovery_multiplier = slow_recovery_multiplier
                self.marginal_decay_threshold = marginal_decay_threshold
                self.marginal_decay_coeff = marginal_decay_coeff
                self.min_recovery_stamina_threshold = min_recovery_stamina_threshold
                self.min_recovery_rest_time_seconds = min_recovery_rest_time_seconds
                self.sprint_speed_boost = sprint_speed_boost
                self.posture_crouch_multiplier = posture_crouch_multiplier
                self.posture_prone_multiplier = posture_prone_multiplier
                self.jump_stamina_base_cost = jump_stamina_base_cost
                self.vault_stamina_start_cost = vault_stamina_start_cost
                self.climb_stamina_tick_cost = climb_stamina_tick_cost
                self.jump_consecutive_penalty = jump_consecutive_penalty
                self.slope_uphill_coeff = slope_uphill_coeff
                self.slope_downhill_coeff = slope_downhill_coeff
                self.swimming_base_power = swimming_base_power
                self.swimming_encumbrance_threshold = swimming_encumbrance_threshold
                self.swimming_static_drain_multiplier = swimming_static_drain_multiplier
                self.swimming_dynamic_power_efficiency = swimming_dynamic_power_efficiency
                self.swimming_energy_to_stamina_coeff = swimming_energy_to_stamina_coeff
                self.env_heat_stress_max_multiplier = env_heat_stress_max_multiplier
                self.env_rain_weight_max = env_rain_weight_max
                self.env_wind_resistance_coeff = env_wind_resistance_coeff
                self.env_mud_penalty_max = env_mud_penalty_max
                self.env_temperature_heat_penalty_coeff = env_temperature_heat_penalty_coeff
                self.env_temperature_cold_recovery_penalty_coeff = env_temperature_cold_recovery_penalty_coeff
            
            def to_constants(self) -> RSSConstants:
                constants = RSSConstants()
                constants.ENERGY_TO_STAMINA_COEFF = self.energy_to_stamina_coeff
                constants.BASE_RECOVERY_RATE = self.base_recovery_rate
                constants.STANDING_RECOVERY_MULTIPLIER = self.standing_recovery_multiplier
                constants.PRONE_RECOVERY_MULTIPLIER = self.prone_recovery_multiplier
                constants.LOAD_RECOVERY_PENALTY_COEFF = self.load_recovery_penalty_coeff
                constants.LOAD_RECOVERY_PENALTY_EXPONENT = self.load_recovery_penalty_exponent
                constants.ENCUMBRANCE_SPEED_PENALTY_COEFF = self.encumbrance_speed_penalty_coeff
                constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF = self.encumbrance_stamina_drain_coeff
                constants.SPRINT_STAMINA_DRAIN_MULTIPLIER = self.sprint_stamina_drain_multiplier
                constants.FATIGUE_ACCUMULATION_COEFF = self.fatigue_accumulation_coeff
                constants.FATIGUE_MAX_FACTOR = self.fatigue_max_factor
                constants.AEROBIC_EFFICIENCY_FACTOR = self.aerobic_efficiency_factor
                constants.ANAEROBIC_EFFICIENCY_FACTOR = self.anaerobic_efficiency_factor
                constants.RECOVERY_NONLINEAR_COEFF = self.recovery_nonlinear_coeff
                constants.FAST_RECOVERY_MULTIPLIER = self.fast_recovery_multiplier
                constants.MEDIUM_RECOVERY_MULTIPLIER = self.medium_recovery_multiplier
                constants.SLOW_RECOVERY_MULTIPLIER = self.slow_recovery_multiplier
                constants.MARGINAL_DECAY_THRESHOLD = self.marginal_decay_threshold
                constants.MARGINAL_DECAY_COEFF = self.marginal_decay_coeff
                constants.MIN_RECOVERY_STAMINA_THRESHOLD = self.min_recovery_stamina_threshold
                constants.MIN_RECOVERY_REST_TIME_SECONDS = self.min_recovery_rest_time_seconds
                constants.SPRINT_SPEED_BOOST = self.sprint_speed_boost
                constants.POSTURE_CROUCH_MULTIPLIER = self.posture_crouch_multiplier
                constants.POSTURE_PRONE_MULTIPLIER = self.posture_prone_multiplier
                constants.JUMP_STAMINA_BASE_COST = self.jump_stamina_base_cost
                constants.VAULT_STAMINA_START_COST = self.vault_stamina_start_cost
                constants.CLIMB_STAMINA_TICK_COST = self.climb_stamina_tick_cost
                constants.JUMP_CONSECUTIVE_PENALTY = self.jump_consecutive_penalty
                constants.SLOPE_UPHILL_COEFF = self.slope_uphill_coeff
                constants.SLOPE_DOWNHILL_COEFF = self.slope_downhill_coeff
                constants.SWIMMING_BASE_POWER = self.swimming_base_power
                constants.SWIMMING_ENCUMBRANCE_THRESHOLD = self.swimming_encumbrance_threshold
                constants.SWIMMING_STATIC_DRAIN_MULTIPLIER = self.swimming_static_drain_multiplier
                constants.SWIMMING_DYNAMIC_POWER_EFFICIENCY = self.swimming_dynamic_power_efficiency
                constants.SWIMMING_ENERGY_TO_STAMINA_COEFF = self.swimming_energy_to_stamina_coeff
                constants.ENV_HEAT_STRESS_MAX_MULTIPLIER = self.env_heat_stress_max_multiplier
                constants.ENV_RAIN_WEIGHT_MAX = self.env_rain_weight_max
                constants.ENV_WIND_RESISTANCE_COEFF = self.env_wind_resistance_coeff
                constants.ENV_MUD_PENALTY_MAX = self.env_mud_penalty_max
                constants.ENV_TEMPERATURE_HEAT_PENALTY_COEFF = self.env_temperature_heat_penalty_coeff
                constants.ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = self.env_temperature_cold_recovery_penalty_coeff
                return constants
        
        params = OptimizationParams()
        constants = params.to_constants()
        
        # ==================== 3. 创建数字孪生仿真器 ====================
        
        twin = RSSDigitalTwin(constants)
        evaluator = ScenarioEvaluator(twin)
        
        # ==================== 4. 评估所有测试工况 ====================
        
        total_realism_loss = 0.0
        total_playability_burden = 0.0
        constraint_violation = 0.0
        
        for scenario in self.scenarios:
            result = evaluator.evaluate_scenario(scenario)
            
            # 拟真度评估
            realism_loss = result['realism_loss']
            total_realism_loss += realism_loss
            
            # 可玩性评估（关键修复！）
            recovery_time = result['recovery_time']
            rest_ratio = result['rest_ratio']
            
            # 恢复时间惩罚：超过 90 秒就惩罚
            recovery_penalty = max(0, recovery_time - 90)
            
            # 休息时间占比惩罚：超过 5% 就惩罚
            rest_penalty = max(0, rest_ratio - 0.05) * 100.0
            
            # 最低体力惩罚：低于 15% 就惩罚
            low_stamina_penalty = max(0, 0.15 - result['result']['min_stamina']) * 100.0
            
            # 综合可玩性负担
            playability_burden = recovery_penalty * 1.0 + rest_penalty * 2.0 + low_stamina_penalty * 0.5
            
            total_playability_burden += playability_burden
            
            # 约束违反：恢复时间超过 180 秒
            if recovery_time > 180:
                constraint_violation += 1.0
        
        # ==================== 5. 计算平均目标函数 ====================
        
        avg_realism_loss = total_realism_loss / len(self.scenarios)
        avg_playability_burden = total_playability_burden / len(self.scenarios)
        
        # ==================== 6. 返回两个目标函数 ====================
        
        return avg_realism_loss, avg_playability_burden
    
    def optimize(self, study_name: str = "rss_optimization") -> Dict:
        """
        执行优化
        
        Args:
            study_name: 研究名称
        
        Returns:
            优化结果字典
        """
        print("=" * 80)
        print("RSS 多目标优化器 - Optuna 贝叶斯优化")
        print("=" * 80)
        
        print(f"\n优化配置：")
        print(f"  采样次数：{self.n_trials}")
        print(f"  测试工况数：{len(self.scenarios)}")
        print(f"  优化变量数：40")
        print(f"  目标函数数：2")
        
        print(f"\n目标函数：")
        print(f"  1. 拟真度损失（Realism Loss）- 越小越好")
        print(f"  2. 可玩性负担（Playability Burden）- 越小越好")
        
        print(f"\n开始优化...")
        print("-" * 80)
        
        # 创建研究
        self.study = optuna.create_study(
            study_name=study_name,
            directions=['minimize', 'minimize'],
            sampler=optuna.samplers.TPESampler(),
            pruner=optuna.pruners.MedianPruner()
        )
        
        # 执行优化
        self.study.optimize(
            self.objective,
            n_trials=self.n_trials,
            show_progress_bar=True
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
            
            print(f"  拟真度损失范围：[{min(realism_values):.2f}, {max(realism_values):.2f}]")
            print(f"  可玩性负担范围：[{min(playability_values):.2f}, {max(playability_values):.2f}]")
        
        return {
            'best_trials': self.best_trials,
            'n_solutions': len(self.best_trials),
            'study': self.study
        }
    
    def analyze_sensitivity(self) -> Dict[str, float]:
        """
        分析参数敏感度
        
        Returns:
            参数敏感度字典
        """
        if not self.best_trials or len(self.best_trials) == 0:
            raise ValueError("请先运行优化！")
        
        # 计算每个参数的变异系数
        sensitivity = {}
        
        # 提取所有参数值
        param_names = [
            # 基础参数（13个）
            'energy_to_stamina_coeff',
            'base_recovery_rate',
            'standing_recovery_multiplier',
            'prone_recovery_multiplier',
            'load_recovery_penalty_coeff',
            'load_recovery_penalty_exponent',
            'encumbrance_speed_penalty_coeff',
            'encumbrance_stamina_drain_coeff',
            'sprint_stamina_drain_multiplier',
            'fatigue_accumulation_coeff',
            'fatigue_max_factor',
            'aerobic_efficiency_factor',
            'anaerobic_efficiency_factor',
            # 恢复系统高级参数（8个）
            'recovery_nonlinear_coeff',
            'fast_recovery_multiplier',
            'medium_recovery_multiplier',
            'slow_recovery_multiplier',
            'marginal_decay_threshold',
            'marginal_decay_coeff',
            'min_recovery_stamina_threshold',
            'min_recovery_rest_time_seconds',
            # Sprint系统高级参数（1个）
            'sprint_speed_boost',
            # 姿态系统参数（2个）
            'posture_crouch_multiplier',
            'posture_prone_multiplier',
            # 动作消耗参数（4个）
            'jump_stamina_base_cost',
            'vault_stamina_start_cost',
            'climb_stamina_tick_cost',
            'jump_consecutive_penalty',
            # 坡度系统参数（2个）
            'slope_uphill_coeff',
            'slope_downhill_coeff',
            # 游泳系统参数（5个）
            'swimming_base_power',
            'swimming_encumbrance_threshold',
            'swimming_static_drain_multiplier',
            'swimming_dynamic_power_efficiency',
            'swimming_energy_to_stamina_coeff',
            # 环境因子参数（5个）
            'env_heat_stress_max_multiplier',
            'env_rain_weight_max',
            'env_wind_resistance_coeff',
            'env_mud_penalty_max',
            'env_temperature_heat_penalty_coeff',
            'env_temperature_cold_recovery_penalty_coeff'
        ]
        
        for name in param_names:
            values = [trial.params[name] for trial in self.best_trials]
            mean_val = np.mean(values)
            std_val = np.std(values)
            cv = std_val / mean_val if mean_val != 0 else 0.0
            sensitivity[name] = cv
        
        # 按敏感度排序
        sensitivity = dict(sorted(sensitivity.items(), key=lambda x: x[1], reverse=True))
        
        print(f"\n参数敏感度分析：")
        print(f"{'参数':<40} {'变异系数':>15} {'敏感度':>10}")
        print("-" * 70)
        for name, cv in sensitivity.items():
            sensitivity_level = "高" if cv > 0.3 else "中" if cv > 0.1 else "低"
            print(f"{name:<40} {cv:>15.4f} {sensitivity_level:>10}")
        
        return sensitivity
    
    def select_solution(
        self,
        realism_weight: float = 0.5,
        playability_weight: float = 0.5
    ) -> Dict:
        """
        从帕累托前沿中选择一个解
        
        Args:
            realism_weight: 拟真度权重（0.0-1.0）
            playability_weight: 可玩性权重（0.0-1.0）
        
        Returns:
            优化参数字典
        """
        if not self.best_trials or len(self.best_trials) == 0:
            raise ValueError("请先运行优化！")
        
        # 计算加权得分
        scores = []
        for trial in self.best_trials:
            realism_loss = trial.values[0]
            playability_burden = trial.values[1]
            
            # 归一化
            realism_values = [t.values[0] for t in self.best_trials]
            playability_values = [t.values[1] for t in self.best_trials]
            
            realism_min = min(realism_values)
            realism_max = max(realism_values)
            playability_min = min(playability_values)
            playability_max = max(playability_values)
            
            f1_normalized = (realism_loss - realism_min) / (realism_max - realism_min + 1e-10)
            f2_normalized = (playability_burden - playability_min) / (playability_max - playability_min + 1e-10)
            
            # 计算加权得分
            score = realism_weight * f1_normalized + playability_weight * f2_normalized
            scores.append(score)
        
        # 选择得分最低的解
        best_idx = np.argmin(scores)
        best_trial = self.best_trials[best_idx]
        
        print(f"\n选择最优解：")
        print(f"  拟真度权重：{realism_weight:.2f}")
        print(f"  可玩性权重：{playability_weight:.2f}")
        print(f"  拟真度损失：{best_trial.values[0]:.2f}")
        print(f"  可玩性负担：{best_trial.values[1]:.2f}")
        print(f"  综合得分：{scores[best_idx]:.4f}")
        
        return {
            'params': best_trial.params,
            'realism_loss': best_trial.values[0],
            'playability_burden': best_trial.values[1],
            'score': scores[best_idx]
        }
    
    def export_to_json(
        self,
        params: Dict,
        output_path: str = "optimized_rss_config_optuna.json"
    ):
        """
        导出优化参数到 JSON 文件
        
        Args:
            params: 优化参数字典
            output_path: 输出文件路径
        """
        # 转换为字典
        config = {
            "version": "3.0.0",
            "description": "RSS 多目标优化配置（Optuna 贝叶斯优化）",
            "optimization_method": "Optuna (TPE)",
            "parameters": params
        }
        
        # 写入文件
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        
        print(f"\n配置已导出到：{output_path}")


def main():
    """主函数：执行优化流程"""
    
    print("\n" + "=" * 80)
    print("Realistic Stamina System - Multi-Objective Optimization")
    print("基于仿真的多目标优化（Optuna 贝叶斯优化）")
    print("=" * 80)
    
    # 创建优化器
    optimizer = RSSOptunaOptimizer(n_trials=200)
    
    # 执行优化
    results = optimizer.optimize(study_name="rss_optimization")
    
    # 分析敏感度
    sensitivity = optimizer.analyze_sensitivity()
    
    # 选择最优解（平衡型）
    balanced_solution = optimizer.select_solution(
        realism_weight=0.5,
        playability_weight=0.5
    )
    
    # 导出配置
    optimizer.export_to_json(
        balanced_solution['params'],
        "optimized_rss_config_balanced_optuna.json"
    )
    
    # 选择最优解（拟真优先）
    realism_solution = optimizer.select_solution(
        realism_weight=0.7,
        playability_weight=0.3
    )
    
    # 导出配置
    optimizer.export_to_json(
        realism_solution['params'],
        "optimized_rss_config_realism_optuna.json"
    )
    
    # 选择最优解（可玩性优先）
    playability_solution = optimizer.select_solution(
        realism_weight=0.3,
        playability_weight=0.7
    )
    
    # 导出配置
    optimizer.export_to_json(
        playability_solution['params'],
        "optimized_rss_config_playability_optuna.json"
    )
    
    print("\n" + "=" * 80)
    print("优化流程完成！")
    print("=" * 80)
    print("\n生成的配置文件：")
    print("  1. optimized_rss_config_balanced_optuna.json - 平衡型配置")
    print("  2. optimized_rss_config_realism_optuna.json - 拟真优先配置")
    print("  3. optimized_rss_config_playability_optuna.json - 可玩性优先配置")
    print("\n建议：")
    print("  - 硬核 Milsim 服务器：使用拟真优先配置")
    print("  - 公共服务器：使用平衡型配置")
    print("  - 休闲服务器：使用可玩性优先配置")


if __name__ == '__main__':
    main()
