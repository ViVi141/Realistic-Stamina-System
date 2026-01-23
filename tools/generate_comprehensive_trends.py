#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Realistic Stamina System (RSS) - 综合趋势图生成器
综合趋势图生成器
包含2英里测试、不同负重情况对比、恢复速度分析等
使用 RSSDigitalTwin 进行仿真，确保可视化与优化器使用相同的逻辑
"""

import numpy as np
import sys
import os
import json

# Headless-friendly backend (do not require GUI)
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 添加当前目录到路径，以便导入常量模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rss_digital_twin import RSSDigitalTwin, RSSConstants, MovementType, Stance

# 设置中文字体（用于图表）
rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False
rcParams['figure.titlesize'] = 15
rcParams['axes.titlesize'] = 10
rcParams['axes.labelsize'] = 9
rcParams['legend.fontsize'] = 8
rcParams['xtick.labelsize'] = 8
rcParams['ytick.labelsize'] = 8

# Whether to show figures interactively (default off; scripts are for PNG generation)
SHOW_PLOTS = False

# ==================== 2英里测试参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒


def load_optimized_config(config_path='optimized_rss_config_balanced_optuna.json'):
    """
    加载优化后的配置文件
    
    Args:
        config_path: 配置文件路径
    
    Returns:
        RSSConstants 对象（如果文件存在），否则返回默认常量
    """
    full_path = os.path.join(os.path.dirname(__file__), config_path)
    
    if os.path.exists(full_path):
        try:
            with open(full_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
            
            # 创建常量对象并更新配置
            constants = RSSConstants()
            
            # 更新可配置的参数
            for key, value in config.items():
                if hasattr(constants, key):
                    setattr(constants, key, value)
            
            print(f"已加载优化配置: {config_path}")
            return constants
        except Exception as e:
            print(f"加载配置文件失败: {e}，使用默认配置")
            return RSSConstants()
    else:
        print(f"配置文件不存在: {config_path}，使用默认配置")
        return RSSConstants()


def create_digital_twin(constants=None):
    """
    创建数字孪生仿真器
    
    Args:
        constants: 常量配置（如果为 None，加载优化配置或使用默认值）
    
    Returns:
        RSSDigitalTwin 对象
    """
    if constants is None:
        constants = load_optimized_config()
    
    return RSSDigitalTwin(constants)


def simulate_2miles(encumbrance_percent=0.0, movement_type='run', slope_angle_degrees=0.0, constants=None):
    """
    模拟跑2英里（使用 RSSDigitalTwin）
    
    Args:
        encumbrance_percent: 负重百分比（0.0-1.0）
        movement_type: 移动类型 ('idle', 'walk', 'run', 'sprint')
        slope_angle_degrees: 坡度角度（度）
        constants: 常量配置
    
    Returns:
        (time_points, stamina_values, speed_values, distance_values, final_time, final_distance)
    """
    twin = create_digital_twin(constants)
    
    # 计算当前负重（kg）
    current_weight_kg = encumbrance_percent * twin.constants.MAX_ENCUMBRANCE_WEIGHT
    total_weight = twin.constants.CHARACTER_WEIGHT + current_weight_kg
    
    # 计算目标速度
    target_speed = DISTANCE_METERS / TARGET_TIME_SECONDS
    
    # 将坡度角度转换为坡度百分比
    import math
    grade_percent = math.tan(slope_angle_degrees * math.pi / 180.0) * 100.0 if slope_angle_degrees != 0.0 else 0.0
    
    # 确定移动类型
    movement_type_map = {
        'idle': MovementType.IDLE,
        'walk': MovementType.WALK,
        'run': MovementType.RUN,
        'sprint': MovementType.SPRINT
    }
    mt = movement_type_map.get(movement_type, MovementType.RUN)
    
    # 使用 simulate_scenario 进行仿真
    speed_profile = [(0, target_speed), (TARGET_TIME_SECONDS, target_speed)]
    result = twin.simulate_scenario(
        speed_profile=speed_profile,
        current_weight=total_weight,
        grade_percent=grade_percent,
        terrain_factor=1.0,
        stance=Stance.STAND,
        movement_type=mt
    )
    
    return (
        np.array(result['time_history']),
        np.array(result['stamina_history']),
        np.array(result['speed_history']),
        np.array([np.trapz(result['speed_history'][:i+1], dx=twin.constants.UPDATE_INTERVAL) 
                  for i in range(len(result['speed_history']))]),
        result['total_time_with_penalty'],
        result['total_distance']
    )


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0, movement_type='run', current_weight_kg=0.0):
    """
    根据体力百分比、负重和移动类型计算速度倍数（使用 RSSDigitalTwin）
    
    Args:
        stamina_percent: 体力百分比 (0.0-1.0)
        encumbrance_percent: 负重百分比（基于最大负重）
        movement_type: 移动类型
        current_weight_kg: 当前负重（kg）
    
    Returns:
        速度倍数
    """
    twin = create_digital_twin()
    
    # 计算当前负重（kg）
    if current_weight_kg <= 0.0:
        current_weight_kg = encumbrance_percent * twin.constants.MAX_ENCUMBRANCE_WEIGHT
    total_weight = twin.constants.CHARACTER_WEIGHT + current_weight_kg
    
    # 确定移动类型
    movement_type_map = {
        'idle': MovementType.IDLE,
        'walk': MovementType.WALK,
        'run': MovementType.RUN,
        'sprint': MovementType.SPRINT
    }
    mt = movement_type_map.get(movement_type, MovementType.RUN)
    
    # 使用 RSSDigitalTwin 的方法计算速度倍数
    return twin.calculate_speed_multiplier_by_stamina(stamina_percent, total_weight, mt)


def plot_2mile_test():
    """生成2英里测试图"""
    fig, ax1 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist = simulate_2miles(0.0, 'run')
    
    ax1_twin = ax1.twinx()
    line1 = ax1.plot(time_2m / 60, stamina_2m * 100, 'b-', linewidth=2, label='体力')
    line2 = ax1_twin.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='速度')
    line3 = ax1_twin.plot(time_2m / 60, dist_2m / 1000, 'g--', linewidth=1.5, label='累计距离(km)')
    
    ax1.axhline(y=50, color='b', linestyle=':', alpha=0.5)
    ax1.axhline(y=25, color='b', linestyle=':', alpha=0.5)
    ax1.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    
    ax1.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax1.set_ylabel('体力（%） / Stamina (%)', fontsize=12, color='b')
    ax1_twin.set_ylabel('速度（m/s） / Speed (m/s) | 距离（km） / Distance (km)', fontsize=12)
    ax1.set_title(
        f'2英里测试 / 2-mile test\n'
        f'完成时间 / Finish: {final_time:.1f}s ({final_time/60:.2f}min)',
        fontsize=14,
        fontweight='bold'
    )
    ax1.grid(True, alpha=0.3)
    
    lines = line1 + line2 + line3
    labels = [line.get_label() for line in lines]
    ax1.legend(lines, labels, loc='upper right', fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_2mile_test.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")
    return time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist


def plot_load_comparison():
    """生成不同负重对比图"""
    fig, ax2 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    weights_kg = [0.0, 15.0, 30.0, 40.5]
    encumbrance_levels = [w / twin.constants.MAX_ENCUMBRANCE_WEIGHT for w in weights_kg]
    colors = ['blue', 'green', 'orange', 'red']
    labels = ['0kg', '15kg', '30kg(战斗)', '40.5kg(最大)']
    
    for enc, color, label in zip(encumbrance_levels, colors, labels):
        time_e, stamina_e, speed_e, dist_e, _, _ = simulate_2miles(enc, 'run', 0.0)
        line_width = 3 if enc == twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT else 2
        ax2.plot(time_e / 60, stamina_e * 100, color=color, linewidth=line_width, label=label)
    
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    ax2.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=1.5, alpha=0.5, label='目标时间')
    
    ax2.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax2.set_ylabel('体力（%） / Stamina (%)', fontsize=12)
    ax2.set_title(
        '不同负重下的体力消耗对比 / Stamina drain by load\n'
        '（30KG为战斗负重基准 / 30kg baseline）',
        fontsize=14,
        fontweight='bold'
    )
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_load_comparison.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_movement_type_speed():
    """生成不同移动类型速度对比图"""
    fig, ax3 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    stamina_range = np.linspace(0.0, 1.0, 100)
    speed_run = [twin.constants.GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, 0.0, 'run') for s in stamina_range]
    speed_sprint = [twin.constants.GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, 0.0, 'sprint') for s in stamina_range]
    speed_walk = [twin.constants.GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, 0.0, 'walk') for s in stamina_range]
    
    ax3.plot(stamina_range * 100, speed_run, 'b-', linewidth=2, label='Run（跑步）')
    ax3.plot(stamina_range * 100, speed_sprint, 'r-', linewidth=2, label='Sprint（冲刺）')
    ax3.plot(stamina_range * 100, speed_walk, 'g-', linewidth=2, label='Walk（行走）')
    ax3.axhline(y=twin.constants.GAME_MAX_SPEED * twin.constants.SPRINT_MAX_SPEED_MULTIPLIER, color='orange', 
                linestyle='--', linewidth=1.5, label=f'Sprint最高速度 ({twin.constants.GAME_MAX_SPEED * twin.constants.SPRINT_MAX_SPEED_MULTIPLIER:.2f} m/s)')
    ax3.axhline(y=twin.constants.TARGET_RUN_SPEED, color='blue', 
                linestyle=':', linewidth=1.5, alpha=0.7, label=f'Run目标速度 ({twin.constants.TARGET_RUN_SPEED:.2f} m/s)')
    
    ax3.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax3.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax3.set_title('不同移动类型的速度对比 / Speed by movement type', fontsize=14, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_movement_type_speed.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_recovery_analysis():
    """生成体力恢复速度分析图"""
    fig, ax4 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    start_levels = [0.1, 0.2, 0.3, 0.5, 0.7]
    colors_recovery = ['red', 'orange', 'yellow', 'lightblue', 'lightgreen']
    
    for start, color in zip(start_levels, colors_recovery):
        # 使用 RSSDigitalTwin 的恢复逻辑
        twin.reset()
        twin.stamina = start
        
        time_points = []
        stamina_values = []
        rest_duration_minutes = 0.0
        exercise_duration_minutes = 0.0
        
        time = 0.0
        while twin.stamina < 1.0 and time < 1200:
            time_points.append(time)
            stamina_values.append(twin.stamina)
            
            # 计算恢复率
            recovery_rate = twin.calculate_multi_dimensional_recovery_rate(
                twin.stamina, rest_duration_minutes, exercise_duration_minutes,
                twin.constants.CHARACTER_WEIGHT, Stance.STAND
            )
            
            twin.stamina = min(twin.stamina + recovery_rate, 1.0)
            time += twin.constants.UPDATE_INTERVAL
            rest_duration_minutes += twin.constants.UPDATE_INTERVAL / 60.0
        
        ax4.plot(np.array(time_points) / 60, np.array(stamina_values) * 100, color=color, linewidth=2, 
                label=f'从 {start*100:.0f}% 恢复')
    
    ax4.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax4.set_ylabel('体力（%） / Stamina (%)', fontsize=12)
    ax4.set_title('体力恢复速度分析 / Recovery analysis', fontsize=14, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(fontsize=10, framealpha=0.85)
    ax4.set_ylim([0, 105])
    
    output_file = 'comprehensive_recovery_analysis.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_2mile_speed(time_2m, speed_2m):
    """生成2英里测试速度变化图"""
    fig, ax5 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    ax5.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='实际速度')
    ax5.axhline(y=twin.constants.TARGET_RUN_SPEED, color='orange', 
                linestyle='--', linewidth=1.5, label=f'目标Run速度 ({twin.constants.TARGET_RUN_SPEED:.2f} m/s)')
    ax5.axhline(y=DISTANCE_METERS/TARGET_TIME_SECONDS, 
                color='green', linestyle=':', linewidth=1.5, label=f'所需平均速度 ({DISTANCE_METERS/TARGET_TIME_SECONDS:.2f} m/s)')
    
    ax5.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax5.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax5.set_title('2英里测试速度变化 / Speed during 2-mile test', fontsize=14, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    ax5.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_2mile_speed.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_load_speed_penalty():
    """生成负重对速度的影响图"""
    fig, ax6 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    enc_range = np.linspace(0.0, 1.0, 50)
    speed_penalty = [enc * twin.constants.ENCUMBRANCE_SPEED_PENALTY_COEFF * 100 for enc in enc_range]
    
    ax6.plot(enc_range * 100, speed_penalty, 'r-', linewidth=2, label='速度惩罚')
    ax6.fill_between(enc_range * 100, 0, speed_penalty, alpha=0.3, color='red')
    
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    combat_speed_penalty = combat_enc_percent * twin.constants.ENCUMBRANCE_SPEED_PENALTY_COEFF * 100
    ax6.axvline(x=combat_enc_percent * 100, color='orange', linestyle='--', linewidth=2, label='30KG战斗负重基准')
    ax6.plot(combat_enc_percent * 100, combat_speed_penalty, 'o', color='orange', markersize=10, label=f'30KG惩罚: {combat_speed_penalty:.1f}%')
    
    ax6.set_xlabel('负重百分比（%） / Load (%)', fontsize=12)
    ax6.set_ylabel('速度惩罚（%） / Speed penalty (%)', fontsize=12)
    ax6.set_title(
        '负重对速度的影响 / Load vs speed penalty\n'
        '（30KG为战斗负重基准 / 30kg baseline）',
        fontsize=14,
        fontweight='bold'
    )
    ax6.grid(True, alpha=0.3)
    ax6.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'comprehensive_load_speed_penalty.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_multi_factor_comparison():
    """生成多维度对比图"""
    fig, ax7 = plt.subplots(figsize=(12, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    scenarios = [
        ('run', 0.0, 'Run-平地', 'blue', '-'),
        ('run', 5.0, 'Run-上坡5°', 'orange', '-'),
        ('run', -5.0, 'Run-下坡5°', 'cyan', '-'),
        ('sprint', 0.0, 'Sprint-平地', 'red', '--'),
        ('sprint', 5.0, 'Sprint-上坡5°', 'purple', '--'),
    ]
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    
    for movement_type, slope, label, color, linestyle in scenarios:
        speeds = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, movement_type, twin.constants.COMBAT_ENCUMBRANCE_WEIGHT)
            speed = twin.constants.GAME_MAX_SPEED * speed_mult
            speeds.append(speed)
        ax7.plot(stamina_test * 100, speeds, color=color, linewidth=2, linestyle=linestyle, label=label)
    
    ax7.axhline(y=twin.constants.TARGET_RUN_SPEED, color='gray', linestyle=':', linewidth=1, alpha=0.5, label='目标Run速度 (3.7 m/s)')
    
    ax7.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax7.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax7.set_title(
        '多维度对比 / Multi-factor comparison (30kg baseline)\n'
        '（不同移动类型与坡度 / movement types & slopes）',
        fontsize=14,
        fontweight='bold'
    )
    ax7.grid(True, alpha=0.3)
    ax7.legend(fontsize=10, loc='upper right', ncol=2, framealpha=0.85)
    
    output_file = 'comprehensive_multi_factor_comparison.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_comprehensive_trends():
    """生成所有综合趋势图（拆分为多个独立图表）"""
    print("\n" + "="*70)
    print("生成综合趋势图（拆分为多个独立图表）")
    print("="*70)
    
    # 生成所有图表
    time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist = plot_2mile_test()
    plot_load_comparison()
    plot_movement_type_speed()
    plot_recovery_analysis()
    plot_2mile_speed(time_2m, speed_2m)
    plot_load_speed_penalty()
    plot_multi_factor_comparison()
    
    print("\n所有综合趋势图已生成完成！")
    
    # 打印统计信息
    twin = create_digital_twin()
    print("\n" + "="*70)
    print("2英里测试统计（无负重，使用 RSSDigitalTwin）:")
    print("="*70)
    print("模型参数：")
    print(f"  体力影响指数 (α): {twin.constants.STAMINA_EXPONENT}")
    print(f"  负重影响指数 (γ): {twin.constants.ENCUMBRANCE_SPEED_EXPONENT}")
    print("  使用 RSSDigitalTwin 进行仿真")
    print()
    print("Sprint参数：")
    print(f"  Sprint速度加成: {twin.constants.SPRINT_SPEED_BOOST*100:.0f}%")
    print(f"  Sprint最高速度倍数: {twin.constants.SPRINT_MAX_SPEED_MULTIPLIER*100:.0f}% ({twin.constants.GAME_MAX_SPEED * twin.constants.SPRINT_MAX_SPEED_MULTIPLIER:.2f} m/s)")
    print(f"  Sprint体力消耗倍数: {twin.constants.SPRINT_STAMINA_DRAIN_MULTIPLIER}x")
    print()
    print("负重配置：")
    print(f"  最大负重: {twin.constants.MAX_ENCUMBRANCE_WEIGHT} kg")
    print(f"  战斗负重基准: {twin.constants.COMBAT_ENCUMBRANCE_WEIGHT} kg ({twin.constants.COMBAT_ENCUMBRANCE_WEIGHT/twin.constants.MAX_ENCUMBRANCE_WEIGHT*100:.1f}%)")
    print()
    print("完成时间: {:.1f}秒 ({:.2f}分钟)".format(final_time, final_time/60))
    print(f"目标时间: {TARGET_TIME_SECONDS}秒 ({TARGET_TIME_SECONDS/60:.2f}分钟)")
    print(f"时间差异: {final_time - TARGET_TIME_SECONDS:.1f}秒 ({(final_time - TARGET_TIME_SECONDS)/60:.2f}分钟)")
    print(f"完成距离: {final_dist:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"平均速度: {np.mean(speed_2m):.2f} m/s")
    print(f"最终体力: {stamina_2m[-1]*100:.2f}%")
    print(f"最终速度: {speed_2m[-1]:.2f} m/s ({speed_2m[-1]/twin.constants.GAME_MAX_SPEED*100:.1f}%最大速度)")
    print()
    
    # 30KG战斗负重基准测试
    print("="*70)
    print("30KG战斗负重基准测试:")
    print("="*70)
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    time_combat, stamina_combat, speed_combat, dist_combat, final_time_combat, final_dist_combat = simulate_2miles(combat_enc_percent, 'run', 0.0)
    print(f"完成时间: {final_time_combat:.1f}秒 ({final_time_combat/60:.2f}分钟)")
    print(f"完成距离: {final_dist_combat:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"平均速度: {np.mean(speed_combat):.2f} m/s")
    print(f"最终体力: {stamina_combat[-1]*100:.2f}%")
    print(f"最终速度: {speed_combat[-1]:.2f} m/s ({speed_combat[-1]/twin.constants.GAME_MAX_SPEED*100:.1f}%最大速度)")
    print("="*70)
    
    # 显示图表
    if SHOW_PLOTS:
        plt.show()


if __name__ == "__main__":
    plot_comprehensive_trends()
