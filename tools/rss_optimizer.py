"""
Realistic Stamina System (RSS) - Multi-Objective Optimization
多目标优化器：使用 NSGA-II 算法优化体力系统参数

核心功能：
1. 定义可优化的参数空间
2. 实现多目标函数（拟真度 vs 可玩性）
3. 使用 NSGA-II 算法寻找帕累托最优前沿
4. 生成优化结果和敏感度分析
"""

import numpy as np
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional
import json
from pathlib import Path

from pymoo.core.problem import ElementwiseProblem
from pymoo.algorithms.moo.nsga2 import NSGA2
from pymoo.optimize import minimize
from pymoo.termination import get_termination
from pymoo.visualization.scatter import Scatter

from rss_digital_twin import RSSDigitalTwin, RSSConstants, MovementType, Stance
from rss_scenarios import ScenarioLibrary, ScenarioEvaluator, TestScenario


@dataclass
class OptimizationParameters:
    """可优化的参数"""
    
    # ==================== 能量转换相关 ====================
    energy_to_stamina_coeff: float = 0.000035  # 能量到体力转换系数
    
    # ==================== 恢复系统相关 ====================
    base_recovery_rate: float = 0.0003  # 基础恢复率（每0.2秒）
    standing_recovery_multiplier: float = 2.0  # 站姿恢复倍数
    prone_recovery_multiplier: float = 2.2  # 趴姿恢复倍数
    load_recovery_penalty_coeff: float = 0.0004  # 负重恢复惩罚系数
    load_recovery_penalty_exponent: float = 2.0  # 负重恢复惩罚指数
    
    # ==================== 负重系统相关 ====================
    encumbrance_speed_penalty_coeff: float = 0.20  # 负重速度惩罚系数
    encumbrance_stamina_drain_coeff: float = 1.5  # 负重体力消耗系数
    
    # ==================== Sprint 相关 ====================
    sprint_stamina_drain_multiplier: float = 3.0  # Sprint体力消耗倍数
    
    # ==================== 疲劳系统相关 ====================
    fatigue_accumulation_coeff: float = 0.015  # 疲劳累积系数
    fatigue_max_factor: float = 2.0  # 最大疲劳因子
    
    # ==================== 代谢适应相关 ====================
    aerobic_efficiency_factor: float = 0.9  # 有氧区效率因子
    anaerobic_efficiency_factor: float = 1.2  # 无氧区效率因子
    
    def to_constants(self) -> RSSConstants:
        """
        转换为 RSSConstants 对象
        
        Returns:
            RSSConstants 对象
        """
        constants = RSSConstants()
        
        # 更新能量转换相关
        constants.ENERGY_TO_STAMINA_COEFF = self.energy_to_stamina_coeff
        
        # 更新恢复系统相关
        constants.BASE_RECOVERY_RATE = self.base_recovery_rate
        constants.STANDING_RECOVERY_MULTIPLIER = self.standing_recovery_multiplier
        constants.PRONE_RECOVERY_MULTIPLIER = self.prone_recovery_multiplier
        constants.LOAD_RECOVERY_PENALTY_COEFF = self.load_recovery_penalty_coeff
        constants.LOAD_RECOVERY_PENALTY_EXPONENT = self.load_recovery_penalty_exponent
        
        # 更新负重系统相关
        constants.ENCUMBRANCE_SPEED_PENALTY_COEFF = self.encumbrance_speed_penalty_coeff
        constants.ENCUMBRANCE_STAMINA_DRAIN_COEFF = self.encumbrance_stamina_drain_coeff
        
        # 更新 Sprint 相关
        constants.SPRINT_STAMINA_DRAIN_MULTIPLIER = self.sprint_stamina_drain_multiplier
        
        # 更新疲劳系统相关
        constants.FATIGUE_ACCUMULATION_COEFF = self.fatigue_accumulation_coeff
        constants.FATIGUE_MAX_FACTOR = self.fatigue_max_factor
        
        # 更新代谢适应相关
        constants.AEROBIC_EFFICIENCY_FACTOR = self.aerobic_efficiency_factor
        constants.ANAEROBIC_EFFICIENCY_FACTOR = self.anaerobic_efficiency_factor
        
        return constants


class RSSOptimizationProblem(ElementwiseProblem):
    """RSS 多目标优化问题"""
    
    def __init__(
        self,
        scenarios: List[TestScenario],
        variable_bounds: Optional[Dict[str, Tuple[float, float]]] = None
    ):
        """
        初始化优化问题
        
        Args:
            scenarios: 测试工况列表
            variable_bounds: 变量边界（如果为 None，使用默认值）
        """
        # 定义变量边界
        self.variable_names = [
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
            'anaerobic_efficiency_factor'
        ]
        
        # 默认变量边界
        self.default_bounds = {
            'energy_to_stamina_coeff': (0.00002, 0.00005),
            'base_recovery_rate': (0.0001, 0.0005),
            'standing_recovery_multiplier': (1.0, 3.0),
            'prone_recovery_multiplier': (1.5, 3.0),
            'load_recovery_penalty_coeff': (0.0001, 0.001),
            'load_recovery_penalty_exponent': (1.0, 3.0),
            'encumbrance_speed_penalty_coeff': (0.1, 0.3),
            'encumbrance_stamina_drain_coeff': (1.0, 2.0),
            'sprint_stamina_drain_multiplier': (2.0, 4.0),
            'fatigue_accumulation_coeff': (0.005, 0.03),
            'fatigue_max_factor': (1.5, 3.0),
            'aerobic_efficiency_factor': (0.8, 1.0),
            'anaerobic_efficiency_factor': (1.0, 1.5)
        }
        
        # 使用自定义边界或默认边界
        self.variable_bounds = variable_bounds if variable_bounds else self.default_bounds
        
        # 提取边界数组
        xl = np.array([self.variable_bounds[name][0] for name in self.variable_names])
        xu = np.array([self.variable_bounds[name][1] for name in self.variable_names])
        
        # 初始化父类
        super().__init__(n_var=len(self.variable_names), n_obj=2, xl=xl, xu=xu)
        
        # 保存测试工况
        self.scenarios = scenarios
        
        # 创建评估器（每个个体独立）
        self.evaluator = None
    
    def _evaluate(self, x, out, *args, **kwargs):
        """
        评估单个解
        
        Args:
            x: 决策变量向量
            out: 输出字典
        """
        # 创建参数对象
        params = OptimizationParameters(
            energy_to_stamina_coeff=x[0],
            base_recovery_rate=x[1],
            standing_recovery_multiplier=x[2],
            prone_recovery_multiplier=x[3],
            load_recovery_penalty_coeff=x[4],
            load_recovery_penalty_exponent=x[5],
            encumbrance_speed_penalty_coeff=x[6],
            encumbrance_stamina_drain_coeff=x[7],
            sprint_stamina_drain_multiplier=x[8],
            fatigue_accumulation_coeff=x[9],
            fatigue_max_factor=x[10],
            aerobic_efficiency_factor=x[11],
            anaerobic_efficiency_factor=x[12]
        )
        
        # 转换为常量
        constants = params.to_constants()
        
        # 创建数字孪生仿真器
        twin = RSSDigitalTwin(constants)
        
        # 创建评估器
        if self.evaluator is None:
            self.evaluator = ScenarioEvaluator(twin)
        else:
            self.evaluator.twin = twin
        
        # 评估测试套件
        suite_results = self.evaluator.evaluate_test_suite(self.scenarios)
        
        # 计算目标函数
        # 目标1：拟真度损失（越小越好）
        f1 = suite_results['avg_realism_loss']
        
        # 目标2：可玩性负担（越小越好）
        f2 = suite_results['avg_playability_burden']
        
        # 约束惩罚：违反约束的工况数量
        constraint_penalty = suite_results['constraint_violations']
        
        # 将约束惩罚添加到目标函数中（软约束）
        f1 += constraint_penalty * 10.0  # 降低惩罚权重，从100.0降到10.0
        f2 += constraint_penalty * 5.0   # 降低惩罚权重，从50.0降到5.0
        
        out["F"] = [f1, f2]
    
    def get_parameter_names(self) -> List[str]:
        """
        获取参数名称列表
        
        Returns:
            参数名称列表
        """
        return self.variable_names


class RSSOptimizer:
    """RSS 多目标优化器"""
    
    def __init__(
        self,
        scenarios: Optional[List[TestScenario]] = None,
        variable_bounds: Optional[Dict[str, Tuple[float, float]]] = None
    ):
        """
        初始化优化器
        
        Args:
            scenarios: 测试工况列表（如果为 None，使用标准测试套件）
            variable_bounds: 变量边界（如果为 None，使用默认值）
        """
        # 创建标准测试套件
        if scenarios is None:
            scenarios = ScenarioLibrary.create_standard_test_suite()
        
        # 创建优化问题
        self.problem = RSSOptimizationProblem(scenarios, variable_bounds)
        
        # 优化结果
        self.results = None
        self.pareto_front = None
        self.pareto_set = None
    
    def optimize(
        self,
        pop_size: int = 100,
        n_gen: int = 200,
        seed: int = 1,
        verbose: bool = True
    ) -> dict:
        """
        执行优化
        
        Args:
            pop_size: 种群大小
            n_gen: 进化代数
            seed: 随机种子
            verbose: 是否显示详细输出
        
        Returns:
            优化结果字典
        """
        print("=" * 80)
        print("RSS 多目标优化器 - NSGA-II 算法")
        print("=" * 80)
        
        print(f"\n优化配置：")
        print(f"  种群大小：{pop_size}")
        print(f"  进化代数：{n_gen}")
        print(f"  随机种子：{seed}")
        print(f"  测试工况数：{len(self.problem.scenarios)}")
        print(f"  优化变量数：{self.problem.n_var}")
        print(f"  目标函数数：{self.problem.n_obj}")
        
        print(f"\n优化变量：")
        for i, name in enumerate(self.problem.get_parameter_names(), 1):
            bounds = self.problem.variable_bounds[name]
            print(f"  {i}. {name}: [{bounds[0]:.6f}, {bounds[1]:.6f}]")
        
        print(f"\n目标函数：")
        print(f"  1. 拟真度损失（Realism Loss）- 越小越好")
        print(f"  2. 可玩性负担（Playability Burden）- 越小越好")
        
        print(f"\n开始优化...")
        print("-" * 80)
        
        # 创建 NSGA-II 算法
        algorithm = NSGA2(
            pop_size=pop_size,
            eliminate_duplicates=True
        )
        
        # 执行优化
        termination = get_termination("n_gen", n_gen)
        self.results = minimize(
            self.problem,
            algorithm,
            termination,
            seed=seed,
            verbose=verbose
        )
        
        print("-" * 80)
        print(f"\n优化完成！")
        
        # 提取帕累托前沿
        self.pareto_front = self.results.F
        self.pareto_set = self.results.X
        
        print(f"\n帕累托前沿：")
        print(f"  非支配解数量：{len(self.pareto_front)}")
        print(f"  拟真度损失范围：[{self.pareto_front[:, 0].min():.2f}, {self.pareto_front[:, 0].max():.2f}]")
        print(f"  可玩性负担范围：[{self.pareto_front[:, 1].min():.2f}, {self.pareto_front[:, 1].max():.2f}]")
        
        return {
            'pareto_front': self.pareto_front,
            'pareto_set': self.pareto_set,
            'n_solutions': len(self.pareto_front),
            'problem': self.problem
        }
    
    def analyze_sensitivity(self) -> Dict[str, float]:
        """
        分析参数敏感度
        
        Returns:
            参数敏感度字典
        """
        if self.pareto_set is None:
            raise ValueError("请先运行优化！")
        
        # 计算每个参数的变异系数
        sensitivity = {}
        
        for i, name in enumerate(self.problem.get_parameter_names()):
            values = self.pareto_set[:, i]
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
    ) -> OptimizationParameters:
        """
        从帕累托前沿中选择一个解
        
        Args:
            realism_weight: 拟真度权重（0.0-1.0）
            playability_weight: 可玩性权重（0.0-1.0）
        
        Returns:
            优化参数对象
        """
        if self.pareto_front is None or self.pareto_set is None:
            raise ValueError("请先运行优化！")
        
        # 归一化目标函数
        f1_normalized = (self.pareto_front[:, 0] - self.pareto_front[:, 0].min()) / \
                       (self.pareto_front[:, 0].max() - self.pareto_front[:, 0].min() + 1e-10)
        f2_normalized = (self.pareto_front[:, 1] - self.pareto_front[:, 1].min()) / \
                       (self.pareto_front[:, 1].max() - self.pareto_front[:, 1].min() + 1e-10)
        
        # 计算加权得分
        scores = realism_weight * f1_normalized + playability_weight * f2_normalized
        
        # 选择得分最低的解
        best_idx = np.argmin(scores)
        best_x = self.pareto_set[best_idx]
        
        print(f"\n选择最优解：")
        print(f"  拟真度权重：{realism_weight:.2f}")
        print(f"  可玩性权重：{playability_weight:.2f}")
        print(f"  拟真度损失：{self.pareto_front[best_idx, 0]:.2f}")
        print(f"  可玩性负担：{self.pareto_front[best_idx, 1]:.2f}")
        print(f"  综合得分：{scores[best_idx]:.4f}")
        
        # 创建参数对象
        params = OptimizationParameters(
            energy_to_stamina_coeff=best_x[0],
            base_recovery_rate=best_x[1],
            standing_recovery_multiplier=best_x[2],
            prone_recovery_multiplier=best_x[3],
            load_recovery_penalty_coeff=best_x[4],
            load_recovery_penalty_exponent=best_x[5],
            encumbrance_speed_penalty_coeff=best_x[6],
            encumbrance_stamina_drain_coeff=best_x[7],
            sprint_stamina_drain_multiplier=best_x[8],
            fatigue_accumulation_coeff=best_x[9],
            fatigue_max_factor=best_x[10],
            aerobic_efficiency_factor=best_x[11],
            anaerobic_efficiency_factor=best_x[12]
        )
        
        return params
    
    def export_to_json(
        self,
        params: OptimizationParameters,
        output_path: str = "optimized_rss_config.json"
    ):
        """
        导出优化参数到 JSON 文件
        
        Args:
            params: 优化参数对象
            output_path: 输出文件路径
        """
        # 转换为字典
        config = {
            "version": "3.0.0",
            "description": "RSS 多目标优化配置（NSGA-II）",
            "optimization_method": "NSGA-II",
            "parameters": {
                "energy_to_stamina_coeff": params.energy_to_stamina_coeff,
                "base_recovery_rate": params.base_recovery_rate,
                "standing_recovery_multiplier": params.standing_recovery_multiplier,
                "prone_recovery_multiplier": params.prone_recovery_multiplier,
                "load_recovery_penalty_coeff": params.load_recovery_penalty_coeff,
                "load_recovery_penalty_exponent": params.load_recovery_penalty_exponent,
                "encumbrance_speed_penalty_coeff": params.encumbrance_speed_penalty_coeff,
                "encumbrance_stamina_drain_coeff": params.encumbrance_stamina_drain_coeff,
                "sprint_stamina_drain_multiplier": params.sprint_stamina_drain_multiplier,
                "fatigue_accumulation_coeff": params.fatigue_accumulation_coeff,
                "fatigue_max_factor": params.fatigue_max_factor,
                "aerobic_efficiency_factor": params.aerobic_efficiency_factor,
                "anaerobic_efficiency_factor": params.anaerobic_efficiency_factor
            }
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
    print("基于仿真的多目标优化（NSGA-II）")
    print("=" * 80)
    
    # 创建优化器
    optimizer = RSSOptimizer()
    
    # 执行优化
    results = optimizer.optimize(
        pop_size=50,  # 种群大小（降低以提高速度）
        n_gen=100,  # 进化代数（降低以提高速度）
        seed=1,
        verbose=True
    )
    
    # 分析敏感度
    sensitivity = optimizer.analyze_sensitivity()
    
    # 选择最优解（平衡型）
    balanced_params = optimizer.select_solution(
        realism_weight=0.5,
        playability_weight=0.5
    )
    
    # 导出配置
    optimizer.export_to_json(
        balanced_params,
        "optimized_rss_config_balanced.json"
    )
    
    # 选择最优解（拟真优先）
    realism_params = optimizer.select_solution(
        realism_weight=0.7,
        playability_weight=0.3
    )
    
    # 导出配置
    optimizer.export_to_json(
        realism_params,
        "optimized_rss_config_realism.json"
    )
    
    # 选择最优解（可玩性优先）
    playability_params = optimizer.select_solution(
        realism_weight=0.3,
        playability_weight=0.7
    )
    
    # 导出配置
    optimizer.export_to_json(
        playability_params,
        "optimized_rss_config_playability.json"
    )
    
    print("\n" + "=" * 80)
    print("优化流程完成！")
    print("=" * 80)
    print("\n生成的配置文件：")
    print("  1. optimized_rss_config_balanced.json - 平衡型配置")
    print("  2. optimized_rss_config_realism.json - 拟真优先配置")
    print("  3. optimized_rss_config_playability.json - 可玩性优先配置")
    print("\n建议：")
    print("  - 硬核 Milsim 服务器：使用拟真优先配置")
    print("  - 公共服务器：使用平衡型配置")
    print("  - 休闲服务器：使用可玩性优先配置")


if __name__ == '__main__':
    main()
