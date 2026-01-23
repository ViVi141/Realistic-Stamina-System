"""
Realistic Stamina System (RSS) - Standard Test Scenarios
标准测试工况定义

定义一组标准测试工况，用于参数优化和性能评估
"""

import numpy as np
from dataclasses import dataclass
from typing import List, Tuple
from enum import Enum

from rss_digital_twin import RSSDigitalTwin, MovementType, Stance, RSSConstants


class ScenarioType(Enum):
    """测试工况类型"""
    ACFT_2MILE = "ACFT 2英里测试"
    EVERON_RUCK_MARCH = "Everon 拉练"
    FIRE_ASSAULT = "火力突击"


@dataclass
class TestScenario:
    """测试工况配置"""
    name: str
    description: str
    scenario_type: ScenarioType
    speed_profile: List[Tuple[float, float]]  # [(time, speed), ...]
    current_weight: float  # kg
    grade_percent: float
    terrain_factor: float
    stance: Stance
    movement_type: MovementType
    
    # 目标指标（用于评估）
    target_finish_time: float  # 秒
    target_min_stamina: float  # 最低体力（0.0-1.0）
    target_recovery_time: float  # 秒（从最低体力恢复到80%所需时间）
    
    # 可玩性约束
    max_recovery_time: float  # 秒（最大允许恢复时间）
    max_rest_ratio: float  # 休息时间占比（0.0-1.0）


class ScenarioLibrary:
    """标准测试工况库"""
    
    @staticmethod
    def create_acft_2mile_scenario(load_weight: float = 0.0) -> TestScenario:
        """
        创建 ACFT 2英里测试工况
        
        参考：美国陆军战斗体能测试（ACFT）22-26岁男性2英里测试
        100分用时：15分27秒（927秒）
        距离：2英里（3218.7米）
        目标速度：3.47 m/s
        
        Args:
            load_weight: 负重（kg）
        
        Returns:
            测试工况配置
        """
        distance = 3218.7  # 米
        target_time = 927  # 秒（15分27秒）
        target_speed = distance / target_time  # 3.47 m/s
        
        return TestScenario(
            name=f"ACFT 2英里测试（{load_weight}kg 负重）",
            description=f"美国陆军战斗体能测试标准：2英里在15分27秒内完成，负重{load_weight}kg",
            scenario_type=ScenarioType.ACFT_2MILE,
            speed_profile=[(0, target_speed), (target_time, target_speed)],
            current_weight=90.0 + load_weight,  # 身体重量 + 负重
            grade_percent=0.0,
            terrain_factor=1.0,  # 铺装路面
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=target_time,
            target_min_stamina=0.0,  # 允许完全耗尽
            target_recovery_time=300,  # 5分钟恢复到80%
            max_recovery_time=180,  # 放宽到3分钟
            max_rest_ratio=0.05  # 放宽到5%休息时间
        )
    
    @staticmethod
    def create_everon_ruck_march_scenario(
        load_weight: float = 20.0,
        slope_degrees: float = 10.0,
        distance: float = 500.0
    ) -> TestScenario:
        """
        创建 Everon 拉练工况
        
        模拟：20kg 负重，10度斜坡向上，急行军 500 米
        目标：测试负重和坡度对体力系统的影响
        
        Args:
            load_weight: 负重（kg）
            slope_degrees: 坡度角度（度）
            distance: 距离（米）
        
        Returns:
            测试工况配置
        """
        # 坡度百分比转换：tan(angle) * 100
        grade_percent = np.tan(np.radians(slope_degrees)) * 100
        
        # 坡度自适应目标速度：每1度降低2.5%速度
        adaptation_factor = max(0.6, 1.0 - slope_degrees * 0.025)
        target_speed = 3.7 * adaptation_factor  # 基础Run速度
        
        # 计算完成时间
        target_time = distance / target_speed
        
        return TestScenario(
            name=f"Everon 拉练（{load_weight}kg 负重，{slope_degrees}°上坡）",
            description=f"模拟山地拉练：{distance}米，{slope_degrees}°上坡，负重{load_weight}kg",
            scenario_type=ScenarioType.EVERON_RUCK_MARCH,
            speed_profile=[(0, target_speed), (target_time, target_speed)],
            current_weight=90.0 + load_weight,
            grade_percent=grade_percent,
            terrain_factor=1.2,  # 碎石路
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=target_time,
            target_min_stamina=0.2,  # 最低20%体力
            target_recovery_time=180,  # 3分钟恢复到80%
            max_recovery_time=120,  # 放宽到2分钟
            max_rest_ratio=0.1  # 放宽到10%休息时间
        )
    
    @staticmethod
    def create_fire_assault_scenario(
        load_weight: float = 30.0,
        sprint_duration: float = 5.0,
        rest_duration: float = 10.0,
        cycles: int = 6
    ) -> TestScenario:
        """
        创建火力突击工况
        
        模拟：30kg 负重，交替冲刺（Sprint）与掩护（休息）
        模式：Sprint 5秒 -> 休息 10秒，重复6次
        目标：测试 Sprint 机制和恢复系统的平衡
        
        Args:
            load_weight: 负重（kg）
            sprint_duration: Sprint持续时间（秒）
            rest_duration: 休息持续时间（秒）
            cycles: 循环次数
        
        Returns:
            测试工况配置
        """
        # 构建速度配置
        speed_profile = []
        current_time = 0.0
        
        for i in range(cycles):
            # Sprint 阶段
            sprint_speed = 5.2  # 最大Sprint速度
            speed_profile.append((current_time, sprint_speed))
            current_time += sprint_duration
            speed_profile.append((current_time, sprint_speed))
            
            # 休息阶段
            speed_profile.append((current_time, 0.0))
            current_time += rest_duration
            speed_profile.append((current_time, 0.0))
        
        total_time = current_time
        
        return TestScenario(
            name=f"火力突击（{load_weight}kg 负重，{cycles}次循环）",
            description=f"模拟火力突击：Sprint {sprint_duration}秒 -> 休息 {rest_duration}秒，重复{cycles}次",
            scenario_type=ScenarioType.FIRE_ASSAULT,
            speed_profile=speed_profile,
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.SPRINT,
            target_finish_time=total_time,
            target_min_stamina=0.15,  # 最低15%体力
            target_recovery_time=60,  # 1分钟恢复到80%
            max_recovery_time=90,  # 最大90秒恢复
            max_rest_ratio=0.5  # 最多50%休息时间
        )
    
    @staticmethod
    def create_prone_recovery_scenario(
        load_weight: float = 20.0,
        recovery_duration: float = 60.0
    ) -> TestScenario:
        """
        创建趴姿恢复测试工况
        
        模拟：20kg 负重，趴姿休息 60 秒，测试恢复速度
        目标：测试趴姿恢复倍数的效果
        
        Args:
            load_weight: 负重（kg）
            recovery_duration: 恢复持续时间（秒）
        
        Returns:
            测试工况配置
        """
        # 趴姿休息速度配置
        speed_profile = [(0, 0.0), (recovery_duration, 0.0)]
        
        return TestScenario(
            name=f"趴姿恢复测试（{load_weight}kg 负重）",
            description=f"模拟趴姿恢复：{recovery_duration}秒趴姿休息，负重{load_weight}kg",
            scenario_type=ScenarioType.EVERON_RUCK_MARCH,
            speed_profile=speed_profile,
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.PRONE,
            movement_type=MovementType.IDLE,
            target_finish_time=recovery_duration,
            target_min_stamina=0.8,  # 恢复到80%
            target_recovery_time=recovery_duration,
            max_recovery_time=recovery_duration * 1.5,  # 允许1.5倍时间
            max_rest_ratio=1.0  # 100%休息时间
        )
    
    @staticmethod
    def create_swimming_scenario(
        load_weight: float = 10.0,
        swimming_duration: float = 300.0,
        swimming_speed: float = 1.0
    ) -> TestScenario:
        """
        创建游泳体力测试工况
        
        模拟：10kg 负重，游泳 5 分钟（300秒），速度 1.0 m/s
        目标：测试游泳时的体力消耗
        
        Args:
            load_weight: 负重（kg）
            swimming_duration: 游泳持续时间（秒）
            swimming_speed: 游泳速度（m/s）
        
        Returns:
            测试工况配置
        """
        # 游泳速度配置
        speed_profile = [(0, swimming_speed), (swimming_duration, swimming_speed)]
        
        return TestScenario(
            name=f"游泳体力测试（{load_weight}kg 负重）",
            description=f"模拟游泳：{swimming_duration}秒游泳，速度{swimming_speed}m/s，负重{load_weight}kg",
            scenario_type=ScenarioType.EVERON_RUCK_MARCH,
            speed_profile=speed_profile,
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=swimming_duration,
            target_min_stamina=0.3,  # 最低30%体力
            target_recovery_time=180,  # 3分钟恢复到80%
            max_recovery_time=300,  # 最大5分钟恢复
            max_rest_ratio=0.0  # 不允许休息
        )
    
    @staticmethod
    def create_environmental_heat_stress_scenario(
        load_weight: float = 20.0,
        temperature_celsius: float = 35.0,
        march_duration: float = 600.0,
        march_speed: float = 2.5
    ) -> TestScenario:
        """
        创建环境因子测试工况（热应激）
        
        模拟：20kg 负重，35°C 高温，急行军 10 分钟（600秒），速度 2.5 m/s
        目标：测试热应激对体力系统的影响
        
        Args:
            load_weight: 负重（kg）
            temperature_celsius: 温度（摄氏度）
            march_duration: 急行军持续时间（秒）
            march_speed: 急行军速度（m/s）
        
        Returns:
            测试工况配置
        """
        # 急行军速度配置
        speed_profile = [(0, march_speed), (march_duration, march_speed)]
        
        # 热应激修正：温度越高，地形系数越大
        terrain_factor = 1.0 + (temperature_celsius - 25.0) * 0.05
        
        return TestScenario(
            name=f"环境因子测试（热应激 {temperature_celsius}°C）",
            description=f"模拟热应激：{temperature_celsius}°C高温，{march_duration}秒急行军，速度{march_speed}m/s，负重{load_weight}kg",
            scenario_type=ScenarioType.EVERON_RUCK_MARCH,
            speed_profile=speed_profile,
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=terrain_factor,
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=march_duration,
            target_min_stamina=0.2,  # 最低20%体力
            target_recovery_time=240,  # 4分钟恢复到80%
            max_recovery_time=360,  # 最大6分钟恢复
            max_rest_ratio=0.0  # 不允许休息
        )
    
    @staticmethod
    def create_long_endurance_scenario(
        load_weight: float = 15.0,
        distance: float = 10000.0,
        march_speed: float = 2.0
    ) -> TestScenario:
        """
        创建长时间耐力测试工况
        
        模拟：15kg 负重，10 公里（10000米）拉练，速度 2.0 m/s
        目标：测试长时间耐力对体力系统的影响
        
        Args:
            load_weight: 负重（kg）
            distance: 距离（米）
            march_speed: 拉练速度（m/s）
        
        Returns:
            测试工况配置
        """
        # 计算完成时间
        target_time = distance / march_speed
        
        # 拉练速度配置
        speed_profile = [(0, march_speed), (target_time, march_speed)]
        
        return TestScenario(
            name=f"长时间耐力测试（{distance/1000:.1f}公里）",
            description=f"模拟长时间耐力：{distance/1000:.1f}公里拉练，速度{march_speed}m/s，负重{load_weight}kg",
            scenario_type=ScenarioType.EVERON_RUCK_MARCH,
            speed_profile=speed_profile,
            current_weight=90.0 + load_weight,
            grade_percent=0.0,
            terrain_factor=1.2,  # 碎石路
            stance=Stance.STAND,
            movement_type=MovementType.RUN,
            target_finish_time=target_time,
            target_min_stamina=0.1,  # 最低10%体力
            target_recovery_time=600,  # 10分钟恢复到80%
            max_recovery_time=900,  # 最大15分钟恢复
            max_rest_ratio=0.05  # 最多5%休息时间
        )
    
    @staticmethod
    def create_standard_test_suite() -> List[TestScenario]:
        """
        创建标准测试套件
        
        Returns:
            标准测试工况列表
        """
        scenarios = []
        
        # ACFT 2英里测试（不同负重）
        scenarios.append(ScenarioLibrary.create_acft_2mile_scenario(load_weight=0.0))
        scenarios.append(ScenarioLibrary.create_acft_2mile_scenario(load_weight=15.0))
        scenarios.append(ScenarioLibrary.create_acft_2mile_scenario(load_weight=30.0))
        
        # Everon 拉练（不同坡度）
        scenarios.append(ScenarioLibrary.create_everon_ruck_march_scenario(
            load_weight=20.0, slope_degrees=5.0, distance=500.0
        ))
        scenarios.append(ScenarioLibrary.create_everon_ruck_march_scenario(
            load_weight=20.0, slope_degrees=10.0, distance=500.0
        ))
        scenarios.append(ScenarioLibrary.create_everon_ruck_march_scenario(
            load_weight=20.0, slope_degrees=15.0, distance=500.0
        ))
        
        # 火力突击（不同配置）
        scenarios.append(ScenarioLibrary.create_fire_assault_scenario(
            load_weight=30.0, sprint_duration=5.0, rest_duration=10.0, cycles=6
        ))
        scenarios.append(ScenarioLibrary.create_fire_assault_scenario(
            load_weight=20.0, sprint_duration=3.0, rest_duration=7.0, cycles=8
        ))
        
        # 趴姿恢复测试
        scenarios.append(ScenarioLibrary.create_prone_recovery_scenario(
            load_weight=20.0, recovery_duration=60.0
        ))
        
        # 游泳体力测试
        scenarios.append(ScenarioLibrary.create_swimming_scenario(
            load_weight=10.0, swimming_duration=300.0, swimming_speed=1.0
        ))
        
        # 环境因子测试（热应激）
        scenarios.append(ScenarioLibrary.create_environmental_heat_stress_scenario(
            load_weight=20.0, temperature_celsius=35.0, march_duration=600.0, march_speed=2.5
        ))
        
        # 长时间耐力测试
        scenarios.append(ScenarioLibrary.create_long_endurance_scenario(
            load_weight=15.0, distance=10000.0, march_speed=2.0
        ))
        
        return scenarios


class ScenarioEvaluator:
    """测试工况评估器"""
    
    def __init__(self, twin: RSSDigitalTwin):
        """
        初始化评估器
        
        Args:
            twin: 数字孪生仿真器
        """
        self.twin = twin
    
    def evaluate_scenario(self, scenario: TestScenario) -> dict:
        """
        评估单个测试工况
        
        Args:
            scenario: 测试工况配置
        
        Returns:
            评估结果字典
        """
        # 运行仿真
        result = self.twin.simulate_scenario(
            speed_profile=scenario.speed_profile,
            current_weight=scenario.current_weight,
            grade_percent=scenario.grade_percent,
            terrain_factor=scenario.terrain_factor,
            stance=scenario.stance,
            movement_type=scenario.movement_type
        )
        
        # 计算恢复时间（从最低体力恢复到80%）
        min_stamina = result['min_stamina']
        recovery_time = self._calculate_recovery_time(
            min_stamina, 0.8, scenario.current_weight
        )
        
        # 计算休息时间占比
        rest_ratio = result['rest_duration'] / result['total_time']
        
        # 计算拟真度损失
        realism_loss = self._calculate_realism_loss(
            scenario, result, recovery_time
        )
        
        # 计算可玩性负担
        playability_burden = self._calculate_playability_burden(
            scenario, result, recovery_time, rest_ratio
        )
        
        return {
            'scenario_name': scenario.name,
            'scenario_type': scenario.scenario_type,
            'result': result,
            'recovery_time': recovery_time,
            'rest_ratio': rest_ratio,
            'realism_loss': realism_loss,
            'playability_burden': playability_burden,
            'meets_constraints': (
                recovery_time <= scenario.max_recovery_time and
                rest_ratio <= scenario.max_rest_ratio
            )
        }
    
    def _calculate_recovery_time(
        self,
        from_stamina: float,
        to_stamina: float,
        current_weight: float
    ) -> float:
        """
        计算恢复时间（从某个体力值恢复到另一个体力值）
        
        Args:
            from_stamina: 起始体力（0.0-1.0）
            to_stamina: 目标体力（0.0-1.0）
            current_weight: 当前重量（kg）
        
        Returns:
            恢复时间（秒）
        """
        # 简化计算：使用平均恢复率
        avg_stamina = (from_stamina + to_stamina) / 2.0
        rest_duration_minutes = 1.0  # 假设休息1分钟
        exercise_duration_minutes = 5.0  # 假设运动5分钟
        
        recovery_rate = self.twin.calculate_multi_dimensional_recovery_rate(
            avg_stamina, rest_duration_minutes, exercise_duration_minutes,
            current_weight, Stance.PRONE
        )
        
        # 计算恢复时间
        stamina_delta = to_stamina - from_stamina
        recovery_time = stamina_delta / abs(recovery_rate) * 0.2  # 转换为秒
        
        return recovery_time
    
    def _calculate_realism_loss(
        self,
        scenario: TestScenario,
        result: dict,
        recovery_time: float
    ) -> float:
        """
        计算拟真度损失
        
        Args:
            scenario: 测试工况配置
            result: 仿真结果
            recovery_time: 恢复时间（秒）
        
        Returns:
            拟真度损失（越小越好）
        """
        # 1. 完成时间偏差
        finish_time_loss = abs(result['total_time'] - scenario.target_finish_time)
        
        # 2. 最低体力偏差
        min_stamina_loss = abs(result['min_stamina'] - scenario.target_min_stamina)
        
        # 3. 恢复时间偏差
        recovery_time_loss = abs(recovery_time - scenario.target_recovery_time)
        
        # 综合拟真度损失（加权）
        realism_loss = (finish_time_loss * 1.0 +
                      min_stamina_loss * 100.0 +
                      recovery_time_loss * 0.5)
        
        return realism_loss
    
    def _calculate_playability_burden(
        self,
        scenario: TestScenario,
        result: dict,
        recovery_time: float,
        rest_ratio: float
    ) -> float:
        """
        计算可玩性负担
        
        Args:
            scenario: 测试工况配置
            result: 仿真结果
            recovery_time: 恢复时间（秒）
            rest_ratio: 休息时间占比
        
        Returns:
            可玩性负担（越小越好）
        """
        # 1. 恢复时间惩罚（超过最大允许恢复时间）
        recovery_penalty = max(0, recovery_time - scenario.max_recovery_time)
        
        # 2. 休息时间占比惩罚
        rest_penalty = max(0, rest_ratio - scenario.max_rest_ratio) * 100.0
        
        # 3. 最低体力惩罚（太低会让玩家感到沮丧）
        low_stamina_penalty = max(0, 0.15 - result['min_stamina']) * 100.0
        
        # 综合可玩性负担（加权）
        playability_burden = (recovery_penalty * 1.0 +
                             rest_penalty * 2.0 +
                             low_stamina_penalty * 0.5)
        
        return playability_burden
    
    def evaluate_test_suite(
        self,
        scenarios: List[TestScenario]
    ) -> dict:
        """
        评估整个测试套件
        
        Args:
            scenarios: 测试工况列表
        
        Returns:
            综合评估结果
        """
        results = []
        total_realism_loss = 0.0
        total_playability_burden = 0.0
        constraint_violations = 0
        
        for scenario in scenarios:
            result = self.evaluate_scenario(scenario)
            results.append(result)
            
            total_realism_loss += result['realism_loss']
            total_playability_burden += result['playability_burden']
            
            if not result['meets_constraints']:
                constraint_violations += 1
        
        return {
            'scenario_results': results,
            'avg_realism_loss': total_realism_loss / len(scenarios),
            'avg_playability_burden': total_playability_burden / len(scenarios),
            'constraint_violations': constraint_violations,
            'total_scenarios': len(scenarios)
        }


if __name__ == '__main__':
    import numpy as np
    
    print("Realistic Stamina System - Standard Test Scenarios")
    print("=" * 60)
    
    # 创建数字孪生仿真器
    twin = RSSDigitalTwin()
    evaluator = ScenarioEvaluator(twin)
    
    # 创建标准测试套件
    scenarios = ScenarioLibrary.create_standard_test_suite()
    
    print(f"\n标准测试套件包含 {len(scenarios)} 个工况：")
    for i, scenario in enumerate(scenarios, 1):
        print(f"{i}. {scenario.name}")
        print(f"   {scenario.description}")
    
    # 评估测试套件
    print("\n开始评估测试套件...")
    suite_results = evaluator.evaluate_test_suite(scenarios)
    
    print(f"\n评估结果：")
    print(f"平均拟真度损失：{suite_results['avg_realism_loss']:.2f}")
    print(f"平均可玩性负担：{suite_results['avg_playability_burden']:.2f}")
    print(f"约束违反次数：{suite_results['constraint_violations']}/{suite_results['total_scenarios']}")
    
    print("\n详细结果：")
    for result in suite_results['scenario_results']:
        print(f"\n{result['scenario_name']}:")
        print(f"  拟真度损失：{result['realism_loss']:.2f}")
        print(f"  可玩性负担：{result['playability_burden']:.2f}")
        print(f"  恢复时间：{result['recovery_time']:.1f} 秒")
        print(f"  休息占比：{result['rest_ratio']*100:.1f}%")
        print(f"  满足约束：{'是' if result['meets_constraints'] else '否'}")
    
    print("\n标准测试工况库测试完成！")
