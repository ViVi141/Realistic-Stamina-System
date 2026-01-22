#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Realistic Stamina System (RSS) - 多维度趋势分析脚本
多维度趋势分析脚本
从多个维度观测体力-速度系统现象，以30KG战斗负重为基准
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


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0, movement_type='run'):
    """
    根据体力百分比、负重和移动类型计算速度倍数（使用 RSSDigitalTwin）
    
    Args:
        stamina_percent: 体力百分比 (0.0-1.0)
        encumbrance_percent: 负重百分比（基于最大负重）
        movement_type: 移动类型
    
    Returns:
        速度倍数
    """
    twin = create_digital_twin()
    
    # 计算当前负重（kg）
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


def plot_2mile_by_load():
    """生成不同负重下的2英里测试对比图"""
    fig, ax1 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    weights_kg = [0.0, 15.0, 30.0, 40.5]
    encumbrance_levels = [w / twin.constants.MAX_ENCUMBRANCE_WEIGHT for w in weights_kg]
    colors = ['blue', 'green', 'orange', 'red']
    labels = ['0kg', '15kg', '30kg(战斗)', '40.5kg(最大)']
    
    for enc, color, label in zip(encumbrance_levels, colors, labels):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(enc, 'run', 0.0)
        line_width = 3 if enc == combat_enc_percent else 2
        ax1.plot(time_e / 60, stamina_e * 100, color=color, linewidth=line_width, label=label)
    
    ax1.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=1.5, alpha=0.5, label='目标时间')
    ax1.set_xlabel('时间（分钟） / Time (min)', fontsize=12)
    ax1.set_ylabel('体力（%） / Stamina (%)', fontsize=12)
    ax1.set_title(
        '不同负重下的2英里测试体力消耗 / 2-mile stamina by load\n'
        '（30KG为战斗负重基准 / 30kg baseline）',
        fontsize=14,
        fontweight='bold'
    )
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'multi_2mile_by_load.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_speed_by_movement_type():
    """生成30KG战斗负重下不同移动类型的速度对比图"""
    fig, ax2 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    stamina_range = np.linspace(0.0, 1.0, 100)
    movement_types = ['walk', 'run', 'sprint']
    movement_labels = ['Walk', 'Run', 'Sprint']
    colors_movement = ['green', 'blue', 'red']
    
    for mt, label, color in zip(movement_types, movement_labels, colors_movement):
        speeds = [twin.constants.GAME_MAX_SPEED * calculate_speed_multiplier_by_stamina(s, combat_enc_percent, mt) for s in stamina_range]
        ax2.plot(stamina_range * 100, speeds, color=color, linewidth=2, label=label)
    
    ax2.axhline(y=twin.constants.GAME_MAX_SPEED * twin.constants.SPRINT_MAX_SPEED_MULTIPLIER, color='orange', linestyle='--', linewidth=1.5, alpha=0.7, label='Sprint最高速度')
    ax2.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax2.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax2.set_title('30KG战斗负重：不同移动类型速度对比 / 30kg: speed by movement type', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'multi_speed_by_movement_type.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_drain_by_slope():
    """生成30KG战斗负重下不同坡度的体力消耗对比图"""
    fig, ax3 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    slopes = [-10.0, -5.0, 0.0, 5.0, 10.0]
    colors_slope = ['cyan', 'lightblue', 'gray', 'orange', 'red']
    labels_slope = ['下坡10°', '下坡5°', '平地', '上坡5°', '上坡10°']
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    
    for slope, color, label in zip(slopes, colors_slope, labels_slope):
        drains = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, 'run')
            speed = twin.constants.GAME_MAX_SPEED * speed_mult
            
            # 使用 RSSDigitalTwin 计算消耗率
            current_weight_kg = combat_enc_percent * twin.constants.MAX_ENCUMBRANCE_WEIGHT
            total_weight = twin.constants.CHARACTER_WEIGHT + current_weight_kg
            
            import math
            grade_percent = math.tan(slope * math.pi / 180.0) * 100.0 if slope != 0.0 else 0.0
            
            drain_rate, _ = twin.calculate_stamina_consumption(
                speed, total_weight, grade_percent, 1.0, 1.0, 1.0, 1.0, 1.0
            )
            drains.append(drain_rate / twin.constants.UPDATE_INTERVAL * 100)
        
        ax3.plot(stamina_test * 100, drains, color=color, linewidth=2, label=label)
    
    ax3.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax3.set_ylabel('体力消耗率（%/秒） / Drain rate (%/s)', fontsize=12)
    ax3.set_title('30KG战斗负重：不同坡度体力消耗 / 30kg: drain by slope', fontsize=14, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(fontsize=10, ncol=2, framealpha=0.85)
    
    output_file = 'multi_drain_by_slope.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_speed_types_slopes():
    """生成30KG基准下不同移动类型&坡度速度对比图"""
    fig, ax4 = plt.subplots(figsize=(12, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    scenarios = [
        ('run', 0.0, 'Run-平地', 'blue', '-'),
        ('run', 5.0, 'Run-上坡5°', 'orange', '-'),
        ('run', -5.0, 'Run-下坡5°', 'cyan', '-'),
        ('sprint', 0.0, 'Sprint-平地', 'red', '--'),
        ('sprint', 5.0, 'Sprint-上坡5°', 'purple', '--'),
        ('sprint', -5.0, 'Sprint-下坡5°', 'pink', '--'),
    ]
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    
    for movement_type, slope, label, color, linestyle in scenarios:
        speeds = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, movement_type)
            speed = twin.constants.GAME_MAX_SPEED * speed_mult
            speeds.append(speed)
        ax4.plot(stamina_test * 100, speeds, color=color, linewidth=2, linestyle=linestyle, label=label)
    
    ax4.axhline(y=twin.constants.TARGET_RUN_SPEED, color='gray', linestyle=':', linewidth=1, alpha=0.5, label='目标Run速度 (3.7 m/s)')
    ax4.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax4.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12)
    ax4.set_title('30KG基准：不同移动类型&坡度速度对比 / 30kg: speed (types & slopes)', fontsize=14, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(fontsize=10, loc='upper right', ncol=3, framealpha=0.85)
    
    output_file = 'multi_speed_types_slopes.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_drain_types_slopes():
    """生成30KG基准下不同移动类型&坡度消耗对比图"""
    fig, ax5 = plt.subplots(figsize=(12, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    scenarios = [
        ('run', 0.0, 'Run-平地', 'blue', '-'),
        ('run', 5.0, 'Run-上坡5°', 'orange', '-'),
        ('run', -5.0, 'Run-下坡5°', 'cyan', '-'),
        ('sprint', 0.0, 'Sprint-平地', 'red', '--'),
        ('sprint', 5.0, 'Sprint-上坡5°', 'purple', '--'),
        ('sprint', -5.0, 'Sprint-下坡5°', 'pink', '--'),
    ]
    
    stamina_test = np.linspace(1.0, 0.2, 50)
    
    for movement_type, slope, label, color, linestyle in scenarios:
        drains = []
        for st in stamina_test:
            speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, movement_type)
            speed = twin.constants.GAME_MAX_SPEED * speed_mult
            
            # 使用 RSSDigitalTwin 计算消耗率
            current_weight_kg = combat_enc_percent * twin.constants.MAX_ENCUMBRANCE_WEIGHT
            total_weight = twin.constants.CHARACTER_WEIGHT + current_weight_kg
            
            import math
            grade_percent = math.tan(slope * math.pi / 180.0) * 100.0 if slope != 0.0 else 0.0
            
            drain_rate, _ = twin.calculate_stamina_consumption(
                speed, total_weight, grade_percent, 1.0, 1.0, 1.0, 1.0, 1.0
            )
            drains.append(drain_rate / twin.constants.UPDATE_INTERVAL * 100)
        
        ax5.plot(stamina_test * 100, drains, color=color, linewidth=2, linestyle=linestyle, label=label)
    
    ax5.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax5.set_ylabel('体力消耗率（%/秒） / Drain rate (%/s)', fontsize=12)
    ax5.set_title('30KG基准：不同移动类型&坡度消耗对比 / 30kg: drain (types & slopes)', fontsize=14, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    ax5.legend(fontsize=10, loc='upper left', ncol=3, framealpha=0.85)
    
    output_file = 'multi_drain_types_slopes.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_2mile_by_movement_type():
    """生成30KG战斗负重下不同移动类型的2英里测试完成时间图"""
    fig, ax6 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    movement_types_test = ['walk', 'run', 'sprint']
    movement_labels_test = ['Walk', 'Run', 'Sprint']
    colors_test = ['green', 'blue', 'red']
    completion_times = []
    
    for mt, label, color in zip(movement_types_test, movement_labels_test, colors_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, mt, 0.0)
        if final_dist >= DISTANCE_METERS:
            completion_times.append(final_time / 60)
        else:
            completion_times.append(None)
    
    valid_indices = [i for i, t in enumerate(completion_times) if t is not None]
    valid_times = [completion_times[i] for i in valid_indices]
    valid_labels = [movement_labels_test[i] for i in valid_indices]
    valid_colors = [colors_test[i] for i in valid_indices]
    
    if valid_times:
        ax6.bar(valid_labels, valid_times, color=valid_colors, alpha=0.7, width=0.6)
    else:
        ax6.text(0.5, 0.5, '30KG负重下\n无法完成2英里测试', 
                ha='center', va='center', fontsize=14, color='red', 
                transform=ax6.transAxes, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    ax6.axhline(y=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    ax6.set_ylabel('完成时间（分钟） / Finish time (min)', fontsize=12)
    ax6.set_title('30KG战斗负重：不同移动类型2英里测试 / 30kg: 2-mile by movement type', fontsize=14, fontweight='bold')
    ax6.grid(True, alpha=0.3, axis='y')
    ax6.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'multi_2mile_by_movement_type.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_2mile_by_slope():
    """生成30KG战斗负重下不同坡度的2英里测试完成时间图"""
    fig, ax7 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    slopes_test = [-5.0, 0.0, 5.0]
    labels_slope_test = ['下坡5°', '平地', '上坡5°']
    colors_slope_test = ['cyan', 'gray', 'orange']
    completion_times_slope = []
    
    for slope, label, color in zip(slopes_test, labels_slope_test, colors_slope_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, 'run', slope)
        if final_dist >= DISTANCE_METERS:
            completion_times_slope.append(final_time / 60)
        else:
            completion_times_slope.append(None)
    
    valid_indices_slope = [i for i, t in enumerate(completion_times_slope) if t is not None]
    valid_times_slope = [completion_times_slope[i] for i in valid_indices_slope]
    valid_labels_slope = [labels_slope_test[i] for i in valid_indices_slope]
    valid_colors_slope = [colors_slope_test[i] for i in valid_indices_slope]
    
    if valid_times_slope:
        ax7.bar(valid_labels_slope, valid_times_slope, color=valid_colors_slope, alpha=0.7, width=0.6)
    else:
        ax7.text(0.5, 0.5, '30KG负重下\n无法完成2英里测试', 
                ha='center', va='center', fontsize=14, color='red', 
                transform=ax7.transAxes, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    ax7.axhline(y=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    ax7.set_ylabel('完成时间（分钟） / Finish time (min)', fontsize=12)
    ax7.set_title('30KG战斗负重：不同坡度2英里测试 / 30kg: 2-mile by slope', fontsize=14, fontweight='bold')
    ax7.grid(True, alpha=0.3, axis='y')
    ax7.legend(fontsize=10, framealpha=0.85)
    
    output_file = 'multi_2mile_by_slope.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_speed_vs_drain():
    """生成30KG战斗负重基准下的速度-体力-消耗率关系图"""
    fig, ax8 = plt.subplots(figsize=(10, 6), constrained_layout=True)
    
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    stamina_range_3d = np.linspace(0.2, 1.0, 50)
    
    speeds_3d = []
    drains_3d = []
    for st in stamina_range_3d:
        speed_mult = calculate_speed_multiplier_by_stamina(st, combat_enc_percent, 'run')
        speed = twin.constants.GAME_MAX_SPEED * speed_mult
        speeds_3d.append(speed)
        
        # 使用 RSSDigitalTwin 计算消耗率
        current_weight_kg = combat_enc_percent * twin.constants.MAX_ENCUMBRANCE_WEIGHT
        total_weight = twin.constants.CHARACTER_WEIGHT + current_weight_kg
        
        drain_rate, _ = twin.calculate_stamina_consumption(
            speed, total_weight, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0
        )
        drains_3d.append(drain_rate / twin.constants.UPDATE_INTERVAL * 100)
    
    ax8_twin = ax8.twinx()
    line1 = ax8.plot(stamina_range_3d * 100, speeds_3d, 'b-', linewidth=2, label='速度')
    line2 = ax8_twin.plot(stamina_range_3d * 100, drains_3d, 'r-', linewidth=2, label='消耗率')
    
    ax8.set_xlabel('体力（%） / Stamina (%)', fontsize=12)
    ax8.set_ylabel('速度（m/s） / Speed (m/s)', fontsize=12, color='b')
    ax8_twin.set_ylabel('体力消耗率（%/秒） / Drain rate (%/s)', fontsize=12, color='r')
    ax8.set_title('30KG战斗负重：速度与消耗率关系 / 30kg: speed vs drain rate', fontsize=14, fontweight='bold')
    ax8.grid(True, alpha=0.3)
    
    lines = line1 + line2
    labels = [line.get_label() for line in lines]
    ax8.legend(lines, labels, loc='upper right', fontsize=10, framealpha=0.85)
    
    output_file = 'multi_speed_vs_drain.png'
    plt.savefig(output_file, dpi=200)
    plt.close()
    print(f"已保存: {output_file}")


def plot_multi_dimensional_analysis():
    """生成所有多维度趋势分析图（拆分为多个独立图表）"""
    print("\n" + "="*70)
    print("生成多维度趋势分析图（拆分为多个独立图表）")
    print("="*70)
    
    # 生成所有图表
    plot_2mile_by_load()
    plot_speed_by_movement_type()
    plot_drain_by_slope()
    plot_speed_types_slopes()
    plot_drain_types_slopes()
    plot_2mile_by_movement_type()
    plot_2mile_by_slope()
    plot_speed_vs_drain()
    
    print("\n所有多维度趋势分析图已生成完成！")
    
    # 30KG战斗负重百分比
    twin = create_digital_twin()
    combat_enc_percent = twin.constants.COMBAT_ENCUMBRANCE_WEIGHT / twin.constants.MAX_ENCUMBRANCE_WEIGHT
    movement_types_test = ['walk', 'run', 'sprint']
    movement_labels_test = ['Walk', 'Run', 'Sprint']
    slopes_test = [-5.0, 0.0, 5.0]
    labels_slope_test = ['下坡5°', '平地', '上坡5°']
    
    # 打印统计信息
    print("\n" + "="*70)
    print("多维度分析统计（以30KG战斗负重为基准）:")
    print("="*70)
    print(f"战斗负重基准: {twin.constants.COMBAT_ENCUMBRANCE_WEIGHT} kg ({combat_enc_percent*100:.1f}%)")
    print(f"最大负重: {twin.constants.MAX_ENCUMBRANCE_WEIGHT} kg")
    print()
    
    # 30KG战斗负重下不同移动类型的2英里测试
    print("30KG战斗负重下不同移动类型的2英里测试:")
    print("-"*70)
    for mt, label in zip(movement_types_test, movement_labels_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, mt, 0.0)
        if final_dist >= DISTANCE_METERS:
            print(f"{label}: 完成时间 {final_time:.1f}秒 ({final_time/60:.2f}分钟), 最终体力 {stamina_e[-1]*100:.1f}%, 平均速度 {np.mean(speed_e):.2f} m/s")
        else:
            print(f"{label}: 无法完成（距离 {final_dist:.1f}米/{DISTANCE_METERS}米）")
    print()
    
    # 30KG战斗负重下不同坡度的2英里测试
    print("30KG战斗负重下不同坡度的2英里测试（Run模式）:")
    print("-"*70)
    for slope, label in zip(slopes_test, labels_slope_test):
        time_e, stamina_e, speed_e, dist_e, final_time, final_dist = simulate_2miles(combat_enc_percent, 'run', slope)
        if final_dist >= DISTANCE_METERS:
            print(f"{label}: 完成时间 {final_time:.1f}秒 ({final_time/60:.2f}分钟), 最终体力 {stamina_e[-1]*100:.1f}%, 平均速度 {np.mean(speed_e):.2f} m/s")
        else:
            print(f"{label}: 无法完成（距离 {final_dist:.1f}米/{DISTANCE_METERS}米）")
    print("="*70)
    
    # 显示图表
    if SHOW_PLOTS:
        plt.show()


if __name__ == "__main__":
    plot_multi_dimensional_analysis()
