#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
拟真体力-速度系统模拟器
基于医学模型模拟角色在最大速度下，体力和其他指标随时间的变化趋势
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置中文字体（用于图表）
rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False

# ==================== 游戏配置常量 ====================
GAME_MAX_SPEED = 5.2  # m/s，游戏最大速度

# ==================== 医学模型参数 ====================
# 目标速度倍数（基于游戏最大速度5.2 m/s，目标平均速度3.47 m/s）
# 目标：2英里在15分27秒内完成（927秒，平均速度3.47 m/s）
# 经过参数优化后的最佳参数（能在15分27秒内完成2英里）
TARGET_SPEED_MULTIPLIER = 0.920  # 5.2 × 0.920 = 4.78 m/s

# 体力下降对速度的影响指数（α）
# 精确数学模型：S(E) = S_max * E^α
# 基于医学文献（Minetti et al., 2002; Weyand et al., 2010），α = 0.6 更符合实际
STAMINA_EXPONENT = 0.6  # 精确值，不使用近似

# 负重对速度的惩罚系数（β）
# 精确数学模型：速度惩罚 = β * (负重百分比)^γ
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.40  # 负重速度惩罚系数
ENCUMBRANCE_SPEED_EXPONENT = 1.0  # 负重影响指数（1.0 = 线性，可调整为 1.2 以模拟非线性）

# 负重对体力消耗的影响系数（γ）
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5

# 最小速度倍数（防止体力完全耗尽时完全无法移动）
MIN_SPEED_MULTIPLIER = 0.15

# ==================== 体力消耗参数 ====================
# 基础体力消耗率（每0.2秒）
# 经过优化后的最佳参数
BASE_DRAIN_RATE = 0.00004  # 每0.2秒消耗0.004%（每10秒消耗0.2%，极低消耗率以完成长距离跑步）

# 速度相关的体力消耗系数（基于精确 Pandolf 模型）
SPEED_LINEAR_DRAIN_COEFF = 0.0001  # 速度线性项系数
SPEED_SQUARED_DRAIN_COEFF = 0.0001  # 速度平方项系数（优化后降低）

# 负重相关的体力消耗系数（基于精确 Pandolf 模型）
ENCUMBRANCE_BASE_DRAIN_COEFF = 0.001  # 负重基础消耗系数
ENCUMBRANCE_SPEED_DRAIN_COEFF = 0.0002  # 负重速度交互项系数

# 体力恢复率（每0.2秒，静止时）
RECOVERY_RATE = 0.00015  # 每0.2秒恢复0.015%（每10秒恢复0.75%，从86%到100%约需3分钟）

# 更新间隔（秒）
UPDATE_INTERVAL = 0.2  # 每0.2秒更新一次


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0):
    """
    根据体力百分比和负重计算速度倍数（基于精确医学模型）
    精确数学模型：S(E) = S_max * E^α
    
    Args:
        stamina_percent: 当前体力百分比 (0.0-1.0)
        encumbrance_percent: 负重百分比 (0.0-1.0)，可选
    
    Returns:
        速度倍数（相对于游戏最大速度）
    """
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    
    # 应用精确医学模型：S(E) = S_max * E^α
    # 使用精确的幂函数，不使用平方根近似
    stamina_effect = np.power(stamina_percent, STAMINA_EXPONENT)
    
    # 基础速度倍数 = 目标速度倍数 × 体力影响
    base_speed_multiplier = TARGET_SPEED_MULTIPLIER * stamina_effect
    
    # 应用负重惩罚（精确计算）
    if encumbrance_percent > 0.0:
        encumbrance_percent = np.clip(encumbrance_percent, 0.0, 1.0)
        # 精确模型：速度惩罚 = β * (负重百分比)^γ
        speed_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * np.power(encumbrance_percent, ENCUMBRANCE_SPEED_EXPONENT)
        speed_penalty = np.clip(speed_penalty, 0.0, 0.5)  # 最多减少50%速度
        base_speed_multiplier = base_speed_multiplier * (1.0 - speed_penalty)
    
    # 应用最小速度限制
    base_speed_multiplier = max(base_speed_multiplier, MIN_SPEED_MULTIPLIER)
    
    # 应用最大速度限制
    base_speed_multiplier = min(base_speed_multiplier, 1.0)
    
    return base_speed_multiplier


def calculate_stamina_drain(current_speed, encumbrance_percent=0.0):
    """
    计算体力消耗率（基于精确 Pandolf 模型）
    
    精确数学模型（基于 Pandolf et al., 1977）：
    消耗 = a + b·V + c·V² + d·M_enc + e·M_enc·V²
    
    其中：
    - a = 基础消耗
    - b = 速度线性项系数
    - c = 速度平方项系数
    - d = 负重基础消耗系数
    - e = 负重速度交互项系数
    - V = 速度（相对速度）
    - M_enc = 负重影响倍数
    
    Args:
        current_speed: 当前速度（m/s）
        encumbrance_percent: 负重百分比（0.0-1.0）
    
    Returns:
        体力消耗率（每0.2秒）
    """
    # 计算速度比（相对于最大速度）
    speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
    
    # 基础体力消耗率（对应 Pandolf 模型中的常数项 a）
    base_drain = BASE_DRAIN_RATE
    
    # 速度线性项（对应 Pandolf 模型中的 b·V 项）
    speed_linear_drain = SPEED_LINEAR_DRAIN_COEFF * speed_ratio
    
    # 速度平方项（对应 Pandolf 模型中的 c·V² 项）
    speed_squared_drain = SPEED_SQUARED_DRAIN_COEFF * speed_ratio * speed_ratio
    
    # 负重相关的体力消耗（基于精确 Pandolf 模型）
    encumbrance_drain_multiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * encumbrance_percent)
    
    # 负重基础消耗（对应 Pandolf 模型中的 d·M_enc 项）
    encumbrance_base_drain = ENCUMBRANCE_BASE_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0)
    
    # 负重速度交互项（对应 Pandolf 模型中的 e·M_enc·V² 项）
    encumbrance_speed_drain = ENCUMBRANCE_SPEED_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0) * speed_ratio * speed_ratio
    
    # 综合体力消耗率（精确公式）
    total_drain = base_drain + speed_linear_drain + speed_squared_drain + encumbrance_base_drain + encumbrance_speed_drain
    
    return total_drain


def simulate_stamina_system(simulation_time=300, encumbrance_percent=0.0, max_speed_mode=True):
    """
    模拟体力系统
    
    Args:
        simulation_time: 模拟时间（秒）
        encumbrance_percent: 负重百分比（0.0-1.0）
        max_speed_mode: 是否保持最大速度模式（True）或静止模式（False）
    
    Returns:
        time_points: 时间点数组
        stamina_values: 体力值数组
        speed_values: 速度值数组
        speed_multiplier_values: 速度倍数数组
        drain_rate_values: 消耗率数组
    """
    # 初始化
    num_steps = int(simulation_time / UPDATE_INTERVAL)
    time_points = np.linspace(0, simulation_time, num_steps)
    
    stamina_values = np.zeros(num_steps)
    speed_values = np.zeros(num_steps)
    speed_multiplier_values = np.zeros(num_steps)
    drain_rate_values = np.zeros(num_steps)
    
    # 初始值
    stamina_values[0] = 1.0  # 100%体力
    speed_multiplier_values[0] = calculate_speed_multiplier_by_stamina(stamina_values[0])
    speed_values[0] = GAME_MAX_SPEED * speed_multiplier_values[0]
    
    # 模拟循环
    for i in range(1, num_steps):
        # 获取当前体力
        current_stamina = stamina_values[i-1]
        
        # 计算当前速度倍数（包含负重影响）
        current_speed_multiplier = calculate_speed_multiplier_by_stamina(current_stamina, encumbrance_percent)
        speed_multiplier_values[i] = current_speed_multiplier
        
        # 计算当前速度
        if max_speed_mode:
            # 最大速度模式：使用当前体力对应的最大速度
            current_speed = GAME_MAX_SPEED * current_speed_multiplier
        else:
            # 静止模式：速度为0
            current_speed = 0.0
        
        speed_values[i] = current_speed
        
        # 计算体力消耗/恢复
        if max_speed_mode and current_speed > 0.05:
            # 移动时：消耗体力
            drain_rate = calculate_stamina_drain(current_speed, encumbrance_percent)
            drain_rate_values[i] = drain_rate
            new_stamina = current_stamina - drain_rate
        else:
            # 静止时：恢复体力
            drain_rate_values[i] = -RECOVERY_RATE  # 负数表示恢复
            new_stamina = current_stamina + RECOVERY_RATE
        
        # 限制体力值范围
        stamina_values[i] = np.clip(new_stamina, 0.0, 1.0)
    
    return time_points, stamina_values, speed_values, speed_multiplier_values, drain_rate_values


def plot_trends(simulation_time=300, encumbrance_percent=0.0):
    """
    绘制趋势图
    """
    print(f"模拟参数（精确数学模型）：")
    print(f"  模拟时间: {simulation_time}秒 ({simulation_time/60:.1f}分钟)")
    print(f"  负重: {encumbrance_percent*100:.0f}%")
    print(f"  游戏最大速度: {GAME_MAX_SPEED} m/s")
    print(f"  目标速度倍数: {TARGET_SPEED_MULTIPLIER}")
    print(f"  体力影响指数 (α): {STAMINA_EXPONENT}")
    print(f"  负重影响指数 (γ): {ENCUMBRANCE_SPEED_EXPONENT}")
    print()
    
    # 模拟最大速度模式
    print("模拟：保持最大速度模式...")
    time_max, stamina_max, speed_max, speed_mult_max, drain_max = simulate_stamina_system(
        simulation_time, encumbrance_percent, max_speed_mode=True
    )
    
    # 模拟静止模式（对比）
    print("模拟：静止恢复模式...")
    time_rest, stamina_rest, speed_rest, speed_mult_rest, drain_rest = simulate_stamina_system(
        simulation_time, encumbrance_percent, max_speed_mode=False
    )
    
    # 创建图表
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('拟真体力-速度系统趋势图\n（精确数学模型，α=0.6）', fontsize=16, fontweight='bold')
    
    # 子图1：体力随时间变化
    ax1 = axes[0, 0]
    ax1.plot(time_max / 60, stamina_max * 100, 'r-', linewidth=2, label='保持最大速度')
    ax1.plot(time_rest / 60, stamina_rest * 100, 'b-', linewidth=2, label='静止恢复')
    ax1.set_xlabel('时间（分钟）', fontsize=12)
    ax1.set_ylabel('体力（%）', fontsize=12)
    ax1.set_title('体力随时间变化', fontsize=13, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_ylim([0, 105])
    
    # 子图2：速度随时间变化
    ax2 = axes[0, 1]
    ax2.plot(time_max / 60, speed_max, 'r-', linewidth=2, label='实际速度')
    ax2.axhline(y=GAME_MAX_SPEED, color='g', linestyle='--', linewidth=1.5, label=f'游戏最大速度 ({GAME_MAX_SPEED} m/s)')
    ax2.axhline(y=GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER, color='orange', linestyle='--', linewidth=1.5, 
                label=f'目标速度 ({GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER:.2f} m/s)')
    ax2.set_xlabel('时间（分钟）', fontsize=12)
    ax2.set_ylabel('速度（m/s）', fontsize=12)
    ax2.set_title('速度随时间变化（保持最大速度模式）', fontsize=13, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    
    # 子图3：速度倍数随时间变化
    ax3 = axes[1, 0]
    ax3.plot(time_max / 60, speed_mult_max * 100, 'purple', linewidth=2, label='速度倍数')
    ax3.axhline(y=TARGET_SPEED_MULTIPLIER * 100, color='orange', linestyle='--', linewidth=1.5,
                label=f'目标速度倍数 ({TARGET_SPEED_MULTIPLIER*100:.0f}%)')
    ax3.axhline(y=MIN_SPEED_MULTIPLIER * 100, color='red', linestyle='--', linewidth=1.5,
                label=f'最小速度倍数 ({MIN_SPEED_MULTIPLIER*100:.0f}%)')
    ax3.set_xlabel('时间（分钟）', fontsize=12)
    ax3.set_ylabel('速度倍数（%）', fontsize=12)
    ax3.set_title('速度倍数随时间变化', fontsize=13, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend()
    
    # 子图4：体力消耗率随时间变化
    ax4 = axes[1, 1]
    ax4.plot(time_max / 60, drain_max / UPDATE_INTERVAL * 100, 'red', linewidth=2, label='消耗率（每秒）')
    ax4.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax4.set_xlabel('时间（分钟）', fontsize=12)
    ax4.set_ylabel('体力变化率（%/秒）', fontsize=12)
    ax4.set_title('体力消耗率随时间变化', fontsize=13, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend()
    
    plt.tight_layout()
    
    # 保存图表
    output_file = 'stamina_system_trends.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n图表已保存为: {output_file}")
    
    # 显示图表
    plt.show()
    
    # 打印关键指标
    print("\n" + "="*60)
    print("关键指标统计（保持最大速度模式）:")
    print("="*60)
    time_to_50_percent = None
    time_to_25_percent = None
    time_to_exhaustion = None
    
    for i, s in enumerate(stamina_max):
        if s <= 0.5 and time_to_50_percent is None:
            time_to_50_percent = time_max[i]
        if s <= 0.25 and time_to_25_percent is None:
            time_to_25_percent = time_max[i]
        if s <= 0.01 and time_to_exhaustion is None:
            time_to_exhaustion = time_max[i]
    
    print(f"体力降至50%所需时间: {time_to_50_percent:.1f}秒 ({time_to_50_percent/60:.2f}分钟)" if time_to_50_percent else "体力降至50%所需时间: 未达到")
    print(f"体力降至25%所需时间: {time_to_25_percent:.1f}秒 ({time_to_25_percent/60:.2f}分钟)" if time_to_25_percent else "体力降至25%所需时间: 未达到")
    print(f"体力耗尽可能时间: {time_to_exhaustion:.1f}秒 ({time_to_exhaustion/60:.2f}分钟)" if time_to_exhaustion else "体力耗尽可能时间: 未完全耗尽")
    print(f"最终体力值: {stamina_max[-1]*100:.2f}%")
    print(f"最终速度: {speed_max[-1]:.2f} m/s ({speed_max[-1]/GAME_MAX_SPEED*100:.1f}%最大速度)")
    print("="*60)


if __name__ == "__main__":
    # 运行模拟
    # 参数：模拟时间（秒），负重百分比（0.0-1.0）
    # 模拟时间设置为20分钟（1200秒），以观察完整趋势
    plot_trends(simulation_time=1200, encumbrance_percent=0.0)  # 1200秒（20分钟），无负重
