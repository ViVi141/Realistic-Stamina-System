#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证 JSON 参数在数字孪生中的预期表现
用于对比游戏内实际表现，排查优化器/嵌入流程问题
包含 ACFT 2英里、Walk 恢复、Run/Sprint 消耗等测试
"""

import json
import sys
from pathlib import Path

# 添加当前目录以导入数字孪生
sys.path.insert(0, str(Path(__file__).parent))
from rss_digital_twin_fix import RSSDigitalTwin, MovementType, RSSConstants, Stance


def load_json_params(json_path: str) -> dict:
    """从 JSON 加载 parameters"""
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    return data.get('parameters', {})


def apply_params_to_constants(constants: RSSConstants, params: dict) -> None:
    """将 JSON 参数应用到 RSSConstants（与 pipeline 一致）"""
    mapping = {
        'energy_to_stamina_coeff': 'ENERGY_TO_STAMINA_COEFF',
        'base_recovery_rate': 'BASE_RECOVERY_RATE',
        'standing_recovery_multiplier': 'STANDING_RECOVERY_MULTIPLIER',
        'prone_recovery_multiplier': 'PRONE_RECOVERY_MULTIPLIER',
        'load_recovery_penalty_coeff': 'LOAD_RECOVERY_PENALTY_COEFF',
        'load_recovery_penalty_exponent': 'LOAD_RECOVERY_PENALTY_EXPONENT',
        'encumbrance_speed_penalty_coeff': 'ENCUMBRANCE_SPEED_PENALTY_COEFF',
        'encumbrance_stamina_drain_coeff': 'ENCUMBRANCE_STAMINA_DRAIN_COEFF',
        'sprint_stamina_drain_multiplier': 'SPRINT_STAMINA_DRAIN_MULTIPLIER',
        'fatigue_accumulation_coeff': 'FATIGUE_ACCUMULATION_COEFF',
        'fatigue_max_factor': 'FATIGUE_MAX_FACTOR',
        'aerobic_efficiency_factor': 'AEROBIC_EFFICIENCY_FACTOR',
        'anaerobic_efficiency_factor': 'ANAEROBIC_EFFICIENCY_FACTOR',
        'recovery_nonlinear_coeff': 'RECOVERY_NONLINEAR_COEFF',
        'fast_recovery_multiplier': 'FAST_RECOVERY_MULTIPLIER',
        'medium_recovery_multiplier': 'MEDIUM_RECOVERY_MULTIPLIER',
        'slow_recovery_multiplier': 'SLOW_RECOVERY_MULTIPLIER',
        'sprint_speed_boost': 'SPRINT_SPEED_BOOST',
        'posture_crouch_multiplier': 'POSTURE_CROUCH_MULTIPLIER',
        'posture_prone_multiplier': 'POSTURE_PRONE_MULTIPLIER',
        'min_recovery_stamina_threshold': 'MIN_RECOVERY_STAMINA_THRESHOLD',
    }
    for json_key, const_key in mapping.items():
        if json_key in params and hasattr(constants, const_key):
            setattr(constants, const_key, params[json_key])


def run_acft_test(twin: RSSDigitalTwin) -> dict:
    """ACFT 3.5km/15:27 测试（0kg 空载）"""
    target_speed = 3500.0 / 927.0
    twin.reset()
    twin.stamina = 1.0
    results = twin.simulate_scenario(
        speed_profile=[(0, target_speed), (927, target_speed)],
        current_weight=90.0,
        grade_percent=0.0,
        terrain_factor=1.0,
        stance=Stance.STAND,
        movement_type=MovementType.RUN,
        enable_randomness=False,
    )
    return results


def run_walk_recovery_test(twin: RSSDigitalTwin) -> float:
    """Walk 120s 1.8m/s 30kg 负载，从50%体力开始，返回净恢复量"""
    twin.reset()
    twin.stamina = 0.5
    for i in range(600):  # 120s / 0.2
        twin.step(
            speed=1.8,
            current_weight=120.0,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.WALK,
            current_time=i * 0.2,
            enable_randomness=False,
        )
    return twin.stamina - 0.5


def verify_json(json_path: str) -> None:
    """验证 JSON 在数字孪生中的预期消耗与恢复"""
    params = load_json_params(json_path)
    constants = RSSConstants()
    apply_params_to_constants(constants, params)

    twin = RSSDigitalTwin(constants)

    # Run 60s 空载
    twin.reset()
    twin.stamina = 1.0
    for i in range(300):  # 60s / 0.2
        twin.step(speed=3.7, current_weight=90.0, grade_percent=0.0,
                  terrain_factor=1.0, stance=Stance.STAND, movement_type=MovementType.RUN,
                  current_time=i * 0.2, enable_randomness=False)
    run_drop = 1.0 - twin.stamina

    # Sprint 30s 空载
    twin.reset()
    twin.stamina = 1.0
    for i in range(150):
        twin.step(speed=5.0, current_weight=90.0, grade_percent=0.0,
                  terrain_factor=1.0, stance=Stance.STAND, movement_type=MovementType.SPRINT,
                  current_time=i * 0.2, enable_randomness=False)
    sprint_drop = 1.0 - twin.stamina

    # ACFT 3.5km/15:27 测试
    acft = run_acft_test(twin)
    acft_ok = acft['min_stamina'] >= 0.20

    # Walk 120s 30kg 恢复测试
    walk_delta = run_walk_recovery_test(twin)
    walk_ok = 0.002 <= walk_delta <= 0.03

    print(f"\n=== JSON 验证: {Path(json_path).name} ===")
    print(f"energy_to_stamina_coeff: {params.get('energy_to_stamina_coeff', 'N/A')}")
    print(f"sprint_stamina_drain_multiplier: {params.get('sprint_stamina_drain_multiplier', 'N/A')}")
    print(f"encumbrance_speed_penalty_coeff: {params.get('encumbrance_speed_penalty_coeff', 'N/A')}")
    print(f"\n数字孪生预期:")
    print(f"  Run 60s 空载 消耗: {run_drop*100:.2f}%")
    print(f"  Sprint 30s 空载 消耗: {sprint_drop*100:.2f}%")
    print(f"  ACFT 3.5km/15:27 最低体力: {acft['min_stamina']*100:.2f}% {'[OK]' if acft_ok else '[FAIL] (>=20%)'}")
    print(f"  Walk 120s 30kg 净恢复: {walk_delta*100:.2f}% {'[OK]' if walk_ok else '[FAIL] (0.2%~3%)'}")
    print(f"\n若游戏内消耗为 0，请检查:")
    print(f"  1. 是否已运行 embed_json_to_c.py 并重新编译")
    print(f"  2. 工作台是否使用 WORKBENCH 编译（会绕过 profile）")
    print(f"  3. 删除 $profile:RealisticStaminaSystem.json 后重试")
    print()


if __name__ == '__main__':
    json_dir = Path(__file__).parent
    for name in ['optimized_rss_config_balanced_super.json',
                 'optimized_rss_config_realism_super.json',
                 'optimized_rss_config_playability_super.json']:
        p = json_dir / name
        if p.exists():
            verify_json(str(p))
