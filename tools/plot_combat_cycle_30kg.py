#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
30KG 负重下正常作战节奏的体力变化
模拟典型战术场景: 冲突 -> 撤退修整 -> 再次冲突
配置来源: optimized_rss_config_realism_super.json
"""

import json
import math
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from rss_digital_twin_fix import (
    RSSDigitalTwin,
    RSSConstants,
    MovementType,
    Stance,
    run_speed_at_weight,
    tobler_speed_multiplier,
)

LOAD_KG = 30
CHARACTER_WEIGHT = 90.0
DT = 0.2
TOTAL_DURATION_S = 1800.0  # 30分钟


def load_constants_from_json(json_path: Path) -> RSSConstants:
    """从 JSON 配置构建 RSSConstants"""
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    params = data.get("parameters", {})
    constants = RSSConstants()
    for k, v in params.items():
        attr = k.upper()
        if hasattr(constants, attr):
            setattr(constants, attr, v)
    return constants


def setup_environment(twin, temperature_celsius=20.0, wind_speed_mps=0.0):
    """设置环境因子"""
    c = twin.constants
    twin.environment_factor.heat_stress = 0.0
    twin.environment_factor.cold_stress = 0.0
    twin.environment_factor.cold_static_penalty = 0.0
    twin.environment_factor.temperature = 20.0
    twin.environment_factor.wind_speed = float(wind_speed_mps)
    twin.environment_factor.surface_wetness = 0.0
    
    if temperature_celsius is not None:
        twin.environment_factor.temperature = float(temperature_celsius)
        heat_threshold = 30.0
        cold_threshold = 0.0
        if temperature_celsius > heat_threshold:
            coeff = getattr(c, 'ENV_TEMPERATURE_HEAT_PENALTY_COEFF', 0.02)
            max_add = getattr(c, 'ENV_HEAT_STRESS_MAX_MULTIPLIER', 1.5) - 1.0
            twin.environment_factor.heat_stress = min(
                (temperature_celsius - heat_threshold) * coeff, max_add)
        if temperature_celsius < cold_threshold:
            cold_coeff = getattr(c, 'ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF', 0.05)
            twin.environment_factor.cold_stress = (
                cold_threshold - temperature_celsius) * cold_coeff
            cold_static = getattr(c, 'ENV_TEMPERATURE_COLD_STATIC_PENALTY', 0.03)
            twin.environment_factor.cold_static_penalty = (
                cold_threshold - temperature_celsius) * cold_static


def simulate_combat_cycle(twin, speed_profile, current_weight, grade_percent=0.0, 
                         terrain_factor=1.0, wind_drag=0.0):
    """
    模拟战斗周期
    speed_profile: [(duration_s, speed, movement_type, stance, note), ...]
    """
    time_list = [0.0]
    stamina_list = [1.0]
    recovery_rate_list = [0.0]
    drain_rate_list = [0.0]
    phase_labels = []
    
    current_time = 0.0
    
    for duration_s, speed, movement_type, stance, note in speed_profile:
        steps = int(duration_s / DT)
        for _ in range(steps):
            twin.step(
                speed,
                current_weight,
                grade_percent,
                terrain_factor,
                stance,
                movement_type,
                current_time + DT,
                enable_randomness=False,
                wind_drag=wind_drag,
            )
            current_time += DT
            time_list.append(current_time)
            stamina_list.append(max(0.0, twin.stamina))
            recovery_rate_list.append(twin.recovery_rate if hasattr(twin, 'recovery_rate') else 0.0)
            drain_rate_list.append(twin.final_drain_rate if hasattr(twin, 'final_drain_rate') else 0.0)
        
        # 记录阶段标签
        phase_labels.append({
            'start_time': current_time - duration_s,
            'end_time': current_time,
            'note': note,
            'movement': ['Idle', 'Walk', 'Run', 'Sprint'][movement_type] if movement_type <= 3 else 'Run',
            'stance': ['Stand', 'Crouch', 'Prone'][stance] if stance <= 2 else 'Stand'
        })
    
    return time_list, stamina_list, recovery_rate_list, drain_rate_list, phase_labels


def create_tactical_cycle(load_kg, flat_run_speed, flat_walk_speed, sprint_speed):
    """
    创建典型战术周期:
    1. 接近敌人: 走 2分钟 (低强度)
    2. 遭遇冲突: 跑 90秒 + 冲刺 30秒 (高强度)
    3. 撤退掩护: 走 2分钟 (低强度)
    4. 修整准备: 站立休息 3分钟 (恢复)
    5. 再次冲突: 跑 60秒 + 冲刺 30秒 (高强度)
    6. 战术撤退: 走 3分钟 (低强度)
    7. 完全修整: 趴下休息 5分钟 (快速恢复)
    8. 准备撤离: 站立 2分钟 (中等强度)
    """
    return [
        (120.0, flat_walk_speed, MovementType.WALK, Stance.STAND, "接近敌人"),
        (90.0, flat_run_speed, MovementType.RUN, Stance.STAND, "遭遇冲突"),
        (30.0, sprint_speed, MovementType.SPRINT, Stance.STAND, "冲刺压制"),
        (120.0, flat_walk_speed, MovementType.WALK, Stance.STAND, "撤退掩护"),
        (180.0, 0.0, MovementType.IDLE, Stance.STAND, "站立修整"),
        (60.0, flat_run_speed, MovementType.RUN, Stance.STAND, "再次冲突"),
        (30.0, sprint_speed, MovementType.SPRINT, Stance.STAND, "突击压制"),
        (180.0, flat_walk_speed, MovementType.WALK, Stance.STAND, "战术撤退"),
        (300.0, 0.0, MovementType.IDLE, Stance.PRONE, "趴下修整"),
        (120.0, 0.0, MovementType.IDLE, Stance.STAND, "准备撤离"),
    ]


def plot_combat_cycle(time_list, stamina_list, recovery_rate_list, drain_rate_list, 
                     phase_labels, out_path, title_suffix):
    """绘制战斗周期图表"""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("需要 matplotlib，请安装: pip install matplotlib")
        return
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
    
    # 上图：体力变化
    ax1.plot(time_list, stamina_list, color='#2E7D32', linewidth=2, label='Stamina')
    ax1.fill_between(time_list, stamina_list, alpha=0.3, color='#4CAF50')
    
    # 标记阶段
    colors_phase = ['#FFCDD2', '#C8E6C9', '#BBDEFB', '#FFF9C4', '#E1BEE7']
    for i, phase in enumerate(phase_labels):
        color = colors_phase[i % len(colors_phase)]
        ax1.axvspan(phase['start_time'], phase['end_time'], 
                   alpha=0.2, color=color)
        ax1.text((phase['start_time'] + phase['end_time']) / 2, 0.5,
                phase['note'] + f"\n{phase['movement']} {phase['stance']}",
                rotation=0, ha='center', va='center',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.8),
                fontsize=8)
    
    ax1.set_xlabel("Time (s)", fontsize=12)
    ax1.set_ylabel("Stamina", fontsize=12)
    ax1.set_title(f"30kg Combat Cycle - Stamina vs Time\n{title_suffix}", fontsize=13)
    ax1.set_ylim(-0.05, 1.05)
    ax1.grid(True, alpha=0.3)
    ax1.axhline(y=0.25, color='orange', linestyle='--', alpha=0.5, label='Sprint threshold')
    ax1.axhline(y=0.15, color='red', linestyle='--', alpha=0.5, label='Exhaustion threshold')
    ax1.legend(loc='best', fontsize=10)
    
    # 标记关键点
    min_stamina = min(stamina_list)
    min_time = time_list[stamina_list.index(min_stamina)]
    ax1.scatter([min_time], [min_stamina], color='red', s=100, zorder=5)
    ax1.annotate(f'Min: {min_stamina:.2%}', xy=(min_time, min_stamina),
                xytext=(10, 10), textcoords='offset points',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
    
    # 下图：恢复 vs 消耗率
    ax2.plot(time_list, recovery_rate_list, color='#1976D2', linewidth=1.5, 
             label='Recovery Rate', alpha=0.7)
    ax2.plot(time_list, drain_rate_list, color='#D32F2F', linewidth=1.5,
             label='Drain Rate', alpha=0.7)
    
    for phase in phase_labels:
        ax2.axvspan(phase['start_time'], phase['end_time'],
                   alpha=0.1, color='gray')
    
    ax2.set_xlabel("Time (s)", fontsize=12)
    ax2.set_ylabel("Rate (per tick)", fontsize=12)
    ax2.set_title("Recovery vs Drain Rate", fontsize=13)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='best', fontsize=10)
    ax2.axhline(y=0, color='black', linestyle='-', alpha=0.5)
    
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    import matplotlib.pyplot as plt
    plt.close(fig)
    print(f"Saved: {out_path}")
    
    # 打印统计信息
    print(f"\n=== 战斗周期统计 ===")
    print(f"起始体力: {stamina_list[0]:.2%}")
    print(f"最低体力: {min_stamina:.2%} (at {min_time:.0f}s)")
    print(f"最终体力: {stamina_list[-1]:.2%}")
    print(f"体力变化: {(stamina_list[-1] - stamina_list[0]):+.2%}")
    
    print(f"\n=== 阶段分析 ===")
    for i, phase in enumerate(phase_labels):
        start_idx = int(phase['start_time'] / DT)
        end_idx = int(phase['end_time'] / DT)
        phase_start_stamina = stamina_list[start_idx]
        phase_end_stamina = stamina_list[end_idx]
        phase_change = phase_end_stamina - phase_start_stamina
        print(f"{i+1}. {phase['note']:12s} ({phase['movement']} {phase['stance']:5s}): "
              f"{phase_start_stamina:.2%} → {phase_end_stamina:.2%} "
              f"({phase_change:+.2%})")


def main():
    json_path = SCRIPT_DIR / "optimized_rss_config_realism_super.json"
    if not json_path.exists():
        print(f"未找到配置: {json_path}")
        return 1
    
    constants = load_constants_from_json(json_path)
    twin = RSSDigitalTwin(constants)
    
    current_weight = CHARACTER_WEIGHT + float(LOAD_KG)
    flat_run_speed = run_speed_at_weight(constants, current_weight)
    
    # 计算 Walk 和 Sprint 速度
    game_max = getattr(constants, 'GAME_MAX_SPEED', 5.5)
    flat_walk_speed = game_max * 0.8 * 0.9  # Walk 基础速度 (假设无负重惩罚)
    sprint_boost = getattr(constants, 'SPRINT_SPEED_BOOST', 0.30)
    flat_sprint_speed = flat_run_speed * (1.0 + sprint_boost)
    
    print(f"30kg 负重速度:")
    print(f"  Walk:  {flat_walk_speed:.2f} m/s")
    print(f"  Run:   {flat_run_speed:.2f} m/s")
    print(f"  Sprint: {flat_sprint_speed:.2f} m/s\n")
    
    # 设置理想环境
    setup_environment(twin, temperature_celsius=20.0, wind_speed_mps=0.0)
    
    # 创建战术周期
    speed_profile = create_tactical_cycle(LOAD_KG, flat_run_speed, flat_walk_speed, flat_sprint_speed)
    
    # 模拟
    time_list, stamina_list, recovery_rate_list, drain_rate_list, phase_labels = \
        simulate_combat_cycle(twin, speed_profile, current_weight)
    
    # 绘图
    out_path = SCRIPT_DIR / "combat_cycle_30kg.png"
    plot_combat_cycle(time_list, stamina_list, recovery_rate_list, drain_rate_list,
                     phase_labels, out_path,
                     f"Load={LOAD_KG}kg, Run={flat_run_speed:.2f}m/s, Sprint={flat_sprint_speed:.2f}m/s")
    
    print("\n图表生成完成!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
