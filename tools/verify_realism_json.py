#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
用数字孪生验证 tools/optimized_rss_config_realism_super.json 的 30kg 600m/1000m 剩余体力。
条件：移动速度 3.8 m/s，气温 32°C，风 NE 5.7 m/s（逆风按风阻计入）。
硬约束：600m 剩余 >= 50%，1000m 剩余 >= 40%。若孪生通过而游戏不通过，说明 C 与孪生需对齐。
"""

import json
import sys
from pathlib import Path

# 保证可导入同目录模块
sys.path.insert(0, str(Path(__file__).resolve().parent))
from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin, Stance, MovementType


def load_json_constants(json_path: str) -> RSSConstants:
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    params = data.get('parameters', {})
    constants = RSSConstants()
    for key, value in params.items():
        attr = key.upper()
        if hasattr(constants, attr):
            setattr(constants, attr, value)
    return constants


def main():
    json_path = Path(__file__).parent / 'optimized_rss_config_realism_super.json'
    if not json_path.exists():
        print(f"未找到: {json_path}")
        return
    constants = load_json_constants(str(json_path))
    twin = RSSDigitalTwin(constants)
    run_speed = 3.8
    load_kg = 28.87
    weight_total = 90.0 + load_kg
    temperature_celsius = 32.0
    wind_speed_mps = 5.7

    # 28.87kg 600m（32°C，NE 5.7 m/s，速度 3.8 m/s）
    duration_600 = 600.0 / run_speed
    res_600 = twin.simulate_scenario(
        speed_profile=[(0, run_speed), (duration_600, run_speed)],
        current_weight=weight_total,
        grade_percent=0.0,
        terrain_factor=1.0,
        stance=Stance.STAND,
        movement_type=MovementType.RUN,
        enable_randomness=False,
        temperature_celsius=temperature_celsius,
        wind_speed_mps=wind_speed_mps,
    )
    min_600 = res_600['min_stamina']
    ok_600 = min_600 >= 0.50

    # 28.87kg 1000m（同上环境）
    duration_1000 = 1000.0 / run_speed
    res_1000 = twin.simulate_scenario(
        speed_profile=[(0, run_speed), (duration_1000, run_speed)],
        current_weight=weight_total,
        grade_percent=0.0,
        terrain_factor=1.0,
        stance=Stance.STAND,
        movement_type=MovementType.RUN,
        enable_randomness=False,
        temperature_celsius=temperature_celsius,
        wind_speed_mps=wind_speed_mps,
    )
    min_1000 = res_1000['min_stamina']
    ok_1000 = min_1000 >= 0.40

    print(f"JSON: {json_path.name}")
    print(f"  条件: 负重 {load_kg} kg, 速度 {run_speed} m/s, {temperature_celsius}°C, 风 NE {wind_speed_mps} m/s")
    print(f"  {load_kg}kg 600m  剩余体力: {min_600:.2%}  硬约束 >= 50%: {'通过' if ok_600 else '未通过'}")
    print(f"  {load_kg}kg 1000m 剩余体力: {min_1000:.2%}  硬约束 >= 40%: {'通过' if ok_1000 else '未通过'}")
    if not ok_600 or not ok_1000:
        print("  若游戏内剩余更低，请排查 C 端与孪生公式是否一致。")


if __name__ == '__main__':
    main()
