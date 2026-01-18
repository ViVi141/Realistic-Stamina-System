#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
综合趋势图生成器
包含2英里测试、不同负重情况对比、恢复速度分析等
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置中文字体（用于图表）
rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False

# ==================== 游戏配置常量 ====================
GAME_MAX_SPEED = 5.2  # m/s，游戏最大速度
UPDATE_INTERVAL = 0.2  # 秒

# ==================== 医学模型参数（精确数学模型）====================
TARGET_SPEED_MULTIPLIER = 0.920  # 5.2 × 0.920 = 4.78 m/s（优化后）
STAMINA_EXPONENT = 0.6  # 精确值，基于医学文献（Minetti et al., 2002）
ENCUMBRANCE_SPEED_PENALTY_COEFF = 0.40  # 负重速度惩罚系数
ENCUMBRANCE_SPEED_EXPONENT = 1.0  # 负重影响指数（1.0 = 线性）
ENCUMBRANCE_STAMINA_DRAIN_COEFF = 1.5  # 负重体力消耗系数
MIN_SPEED_MULTIPLIER = 0.15

# ==================== 体力消耗参数（基于精确 Pandolf 模型）====================
BASE_DRAIN_RATE = 0.00004  # 每0.2秒，基础消耗
SPEED_LINEAR_DRAIN_COEFF = 0.0001  # 速度线性项系数
SPEED_SQUARED_DRAIN_COEFF = 0.0001  # 速度平方项系数（优化后降低）
ENCUMBRANCE_BASE_DRAIN_COEFF = 0.001  # 负重基础消耗系数
ENCUMBRANCE_SPEED_DRAIN_COEFF = 0.0002  # 负重速度交互项系数
RECOVERY_RATE = 0.00015  # 每0.2秒

# ==================== 2英里测试参数 ====================
DISTANCE_METERS = 3218.7  # 米（2英里）
TARGET_TIME_SECONDS = 15 * 60 + 27  # 927秒


def calculate_speed_multiplier_by_stamina(stamina_percent, encumbrance_percent=0.0):
    """
    根据体力百分比和负重计算速度倍数（精确数学模型）
    
    精确公式：
    - 基础速度：S_base = S_max × E^α，其中 α = 0.6
    - 负重惩罚：P_enc = β × (W/W_max)^γ，其中 β = 0.40, γ = 1.0
    - 最终速度：S_final = S_base × (1 - P_enc)
    """
    stamina_percent = np.clip(stamina_percent, 0.0, 1.0)
    
    # 精确计算：S_base = S_max × E^α（使用精确幂函数）
    stamina_effect = np.power(stamina_percent, STAMINA_EXPONENT)
    base_speed_multiplier = TARGET_SPEED_MULTIPLIER * stamina_effect
    base_speed_multiplier = max(base_speed_multiplier, MIN_SPEED_MULTIPLIER)
    
    # 应用精确负重惩罚：P_enc = β × (W/W_max)^γ
    if encumbrance_percent > 0.0:
        encumbrance_percent = np.clip(encumbrance_percent, 0.0, 1.0)
        encumbrance_penalty = ENCUMBRANCE_SPEED_PENALTY_COEFF * np.power(encumbrance_percent, ENCUMBRANCE_SPEED_EXPONENT)
        encumbrance_penalty = np.clip(encumbrance_penalty, 0.0, 0.5)  # 最多减少50%速度
        final_speed_multiplier = base_speed_multiplier * (1.0 - encumbrance_penalty)
    else:
        final_speed_multiplier = base_speed_multiplier
    
    return np.clip(final_speed_multiplier, 0.2, 1.0)


def calculate_stamina_drain(current_speed, encumbrance_percent=0.0):
    """
    计算体力消耗率（基于精确 Pandolf 模型）
    
    精确公式（Pandolf et al., 1977）：
    消耗 = a + b·V + c·V² + d·M_enc + e·M_enc·V²
    """
    speed_ratio = np.clip(current_speed / GAME_MAX_SPEED, 0.0, 1.0)
    
    # 基础消耗（a）
    base_drain = BASE_DRAIN_RATE
    
    # 速度线性项（b·V）
    speed_linear_drain = SPEED_LINEAR_DRAIN_COEFF * speed_ratio
    
    # 速度平方项（c·V²）
    speed_squared_drain = SPEED_SQUARED_DRAIN_COEFF * speed_ratio * speed_ratio
    
    # 负重相关消耗
    encumbrance_drain_multiplier = 1.0 + (ENCUMBRANCE_STAMINA_DRAIN_COEFF * encumbrance_percent)
    
    # 负重基础消耗（d·M_enc）
    encumbrance_base_drain = ENCUMBRANCE_BASE_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0)
    
    # 负重速度交互项（e·M_enc·V²）
    encumbrance_speed_drain = ENCUMBRANCE_SPEED_DRAIN_COEFF * (encumbrance_drain_multiplier - 1.0) * speed_ratio * speed_ratio
    
    # 综合消耗（精确公式）
    return base_drain + speed_linear_drain + speed_squared_drain + encumbrance_base_drain + encumbrance_speed_drain


def simulate_2miles(encumbrance_percent=0.0):
    """模拟跑2英里"""
    stamina = 1.0
    distance = 0.0
    time = 0.0
    time_points = []
    stamina_values = []
    speed_values = []
    distance_values = []
    
    max_time = TARGET_TIME_SECONDS * 2
    
    while distance < DISTANCE_METERS and time < max_time:
        # 计算当前速度倍数
        speed_mult = calculate_speed_multiplier_by_stamina(stamina, encumbrance_percent)
        current_speed = GAME_MAX_SPEED * speed_mult
        
        # 记录数据
        time_points.append(time)
        stamina_values.append(stamina)
        speed_values.append(current_speed)
        distance_values.append(distance)
        
        # 更新距离
        distance += current_speed * UPDATE_INTERVAL
        
        # 如果已跑完距离，停止
        if distance >= DISTANCE_METERS:
            break
        
        # 更新体力
        drain_rate = calculate_stamina_drain(current_speed, encumbrance_percent)
        stamina = max(stamina - drain_rate, 0.0)
        
        time += UPDATE_INTERVAL
    
    return np.array(time_points), np.array(stamina_values), np.array(speed_values), np.array(distance_values), time, distance


def simulate_recovery(start_stamina=0.2, target_stamina=1.0):
    """模拟体力恢复过程"""
    stamina = start_stamina
    time = 0.0
    time_points = []
    stamina_values = []
    
    while stamina < target_stamina:
        time_points.append(time)
        stamina_values.append(stamina)
        
        stamina = min(stamina + RECOVERY_RATE, target_stamina)
        time += UPDATE_INTERVAL
        
        if time > 600:  # 最多模拟10分钟
            break
    
    return np.array(time_points), np.array(stamina_values)


def plot_comprehensive_trends():
    """生成综合趋势图"""
    fig = plt.figure(figsize=(16, 12))
    gs = fig.add_gridspec(3, 3, hspace=0.3, wspace=0.3)
    
    fig.suptitle('拟真体力-速度系统综合趋势分析\n（基于医学模型）', fontsize=18, fontweight='bold')
    
    # ========== 图1: 2英里测试 ==========
    ax1 = fig.add_subplot(gs[0, 0])
    time_2m, stamina_2m, speed_2m, dist_2m, final_time, final_dist = simulate_2miles(0.0)
    
    ax1_twin = ax1.twinx()
    line1 = ax1.plot(time_2m / 60, stamina_2m * 100, 'b-', linewidth=2, label='体力')
    line2 = ax1_twin.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='速度')
    line3 = ax1_twin.plot(time_2m / 60, dist_2m / 1000, 'g--', linewidth=1.5, label='累计距离(km)')
    
    ax1.axhline(y=50, color='b', linestyle=':', alpha=0.5)
    ax1.axhline(y=25, color='b', linestyle=':', alpha=0.5)
    ax1.axvline(x=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', linewidth=2, label='目标时间')
    
    ax1.set_xlabel('时间（分钟）', fontsize=10)
    ax1.set_ylabel('体力（%）', fontsize=10, color='b')
    ax1_twin.set_ylabel('速度（m/s） / 距离（km）', fontsize=10)
    ax1.set_title(f'2英里测试\n完成时间: {final_time:.1f}秒 ({final_time/60:.2f}分钟)', fontsize=11, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    
    lines = line1 + line2 + line3
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc='upper right', fontsize=8)
    
    # ========== 图2: 不同负重对比 ==========
    ax2 = fig.add_subplot(gs[0, 1])
    encumbrance_levels = [0.0, 0.25, 0.5, 0.75, 1.0]
    colors = ['blue', 'green', 'orange', 'red', 'purple']
    
    for enc, color in zip(encumbrance_levels, colors):
        time_e, stamina_e, speed_e, dist_e, _, _ = simulate_2miles(enc)
        ax2.plot(time_e / 60, stamina_e * 100, color=color, linewidth=2, 
                label=f'负重 {enc*100:.0f}%')
    
    ax2.set_xlabel('时间（分钟）', fontsize=10)
    ax2.set_ylabel('体力（%）', fontsize=10)
    ax2.set_title('不同负重下的体力消耗对比', fontsize=11, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=8)
    
    # ========== 图3: 速度衰减曲线（精确 vs 近似对比）==========
    ax3 = fig.add_subplot(gs[0, 2])
    stamina_range = np.linspace(0.0, 1.0, 100)
    speed_mult_range = [calculate_speed_multiplier_by_stamina(s, 0.0) for s in stamina_range]
    speed_range = [GAME_MAX_SPEED * sm for sm in speed_mult_range]
    
    # 对比：旧模型（sqrt，α=0.5）vs 新模型（精确，α=0.6）
    speed_mult_old = [TARGET_SPEED_MULTIPLIER * np.sqrt(s) for s in stamina_range]
    speed_range_old = [GAME_MAX_SPEED * sm for sm in speed_mult_old]
    
    ax3.plot(stamina_range * 100, speed_range, 'b-', linewidth=2, label=f'精确模型 (α={STAMINA_EXPONENT})')
    ax3.plot(stamina_range * 100, speed_range_old, 'g--', linewidth=1.5, alpha=0.7, label='旧模型 (α=0.5)')
    ax3.axhline(y=GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER, color='orange', 
                linestyle='--', linewidth=1.5, label=f'目标速度 ({GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER:.2f} m/s)')
    ax3.axhline(y=GAME_MAX_SPEED * MIN_SPEED_MULTIPLIER, color='red', 
                linestyle='--', linewidth=1.5, label=f'最小速度 ({GAME_MAX_SPEED * MIN_SPEED_MULTIPLIER:.2f} m/s)')
    
    ax3.set_xlabel('体力（%）', fontsize=10)
    ax3.set_ylabel('速度（m/s）', fontsize=10)
    ax3.set_title('速度-体力关系曲线（精确 vs 近似）', fontsize=11, fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.legend(fontsize=8)
    
    # ========== 图4: 体力恢复速度 ==========
    ax4 = fig.add_subplot(gs[1, 0])
    start_levels = [0.1, 0.2, 0.3, 0.5, 0.7]
    colors_recovery = ['red', 'orange', 'yellow', 'lightblue', 'lightgreen']
    
    for start, color in zip(start_levels, colors_recovery):
        time_r, stamina_r = simulate_recovery(start, 1.0)
        ax4.plot(time_r / 60, stamina_r * 100, color=color, linewidth=2, 
                label=f'从 {start*100:.0f}% 恢复')
    
    ax4.set_xlabel('时间（分钟）', fontsize=10)
    ax4.set_ylabel('体力（%）', fontsize=10)
    ax4.set_title('体力恢复速度分析', fontsize=11, fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.legend(fontsize=8)
    ax4.set_ylim([0, 105])
    
    # ========== 图5: 2英里测试速度曲线 ==========
    ax5 = fig.add_subplot(gs[1, 1])
    ax5.plot(time_2m / 60, speed_2m, 'r-', linewidth=2, label='实际速度')
    ax5.axhline(y=GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER, color='orange', 
                linestyle='--', linewidth=1.5, label=f'目标速度 ({GAME_MAX_SPEED * TARGET_SPEED_MULTIPLIER:.2f} m/s)')
    ax5.axhline(y=TARGET_TIME_SECONDS / DISTANCE_METERS * DISTANCE_METERS / TARGET_TIME_SECONDS, 
                color='green', linestyle=':', linewidth=1.5, label=f'所需平均速度 ({DISTANCE_METERS/TARGET_TIME_SECONDS:.2f} m/s)')
    
    ax5.set_xlabel('时间（分钟）', fontsize=10)
    ax5.set_ylabel('速度（m/s）', fontsize=10)
    ax5.set_title('2英里测试速度变化', fontsize=11, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    ax5.legend(fontsize=8)
    
    # ========== 图6: 负重对速度的影响 ==========
    ax6 = fig.add_subplot(gs[1, 2])
    enc_range = np.linspace(0.0, 1.0, 50)
    speed_penalty = [enc * ENCUMBRANCE_SPEED_PENALTY_COEFF * 100 for enc in enc_range]
    
    ax6.plot(enc_range * 100, speed_penalty, 'r-', linewidth=2)
    ax6.fill_between(enc_range * 100, 0, speed_penalty, alpha=0.3, color='red')
    ax6.set_xlabel('负重百分比（%）', fontsize=10)
    ax6.set_ylabel('速度惩罚（%）', fontsize=10)
    ax6.set_title('负重对速度的影响', fontsize=11, fontweight='bold')
    ax6.grid(True, alpha=0.3)
    
    # ========== 图7: 2英里测试完成时间对比 ==========
    ax7 = fig.add_subplot(gs[2, :])
    enc_test = [0.0, 0.1, 0.2, 0.3, 0.4, 0.5]
    completion_times = []
    final_staminas = []
    
    for enc in enc_test:
        _, _, _, _, time_f, dist_f = simulate_2miles(enc)
        if dist_f >= DISTANCE_METERS:
            completion_times.append(time_f / 60)
            final_staminas.append(_[-1] * 100 if len(_) > 0 else 0)
        else:
            completion_times.append(None)
            final_staminas.append(None)
    
    valid_times = [t for t in completion_times if t is not None]
    valid_enc = [enc_test[i] * 100 for i, t in enumerate(completion_times) if t is not None]
    
    ax7_twin = ax7.twinx()
    bars = ax7.bar(valid_enc, valid_times, width=8, alpha=0.7, color='steelblue', label='完成时间')
    ax7_twin.plot(valid_enc, [final_staminas[i] for i, t in enumerate(completion_times) if t is not None], 
                  'ro-', linewidth=2, markersize=8, label='剩余体力')
    
    ax7.axhline(y=TARGET_TIME_SECONDS / 60, color='orange', linestyle='--', 
                linewidth=2, label=f'目标时间 ({TARGET_TIME_SECONDS/60:.2f}分钟)')
    
    ax7.set_xlabel('负重百分比（%）', fontsize=11)
    ax7.set_ylabel('完成时间（分钟）', fontsize=11, color='steelblue')
    ax7_twin.set_ylabel('剩余体力（%）', fontsize=11, color='red')
    ax7.set_title('不同负重下2英里测试完成时间对比', fontsize=12, fontweight='bold')
    ax7.grid(True, alpha=0.3, axis='y')
    
    lines1, labels1 = ax7.get_legend_handles_labels()
    lines2, labels2 = ax7_twin.get_legend_handles_labels()
    ax7.legend(lines1 + lines2, labels1 + labels2, loc='upper left', fontsize=9)
    
    # 保存图表
    output_file = 'comprehensive_stamina_trends.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n综合趋势图已保存为: {output_file}")
    
    # 打印统计信息
    print("\n" + "="*70)
    print("2英里测试统计（无负重，精确数学模型）:")
    print("="*70)
    print(f"模型参数：")
    print(f"  体力影响指数 (α): {STAMINA_EXPONENT}")
    print(f"  负重影响指数 (γ): {ENCUMBRANCE_SPEED_EXPONENT}")
    print(f"  使用精确幂函数计算（不使用近似）")
    print()
    print(f"完成时间: {final_time:.1f}秒 ({final_time/60:.2f}分钟)")
    print(f"目标时间: {TARGET_TIME_SECONDS}秒 ({TARGET_TIME_SECONDS/60:.2f}分钟)")
    print(f"时间差异: {final_time - TARGET_TIME_SECONDS:.1f}秒 ({(final_time - TARGET_TIME_SECONDS)/60:.2f}分钟)")
    print(f"完成距离: {final_dist:.1f}米 (目标: {DISTANCE_METERS}米)")
    print(f"平均速度: {np.mean(speed_2m):.2f} m/s")
    print(f"最终体力: {stamina_2m[-1]*100:.2f}%")
    print(f"最终速度: {speed_2m[-1]:.2f} m/s ({speed_2m[-1]/GAME_MAX_SPEED*100:.1f}%最大速度)")
    print("="*70)
    
    # 显示图表
    plt.show()


if __name__ == "__main__":
    plot_comprehensive_trends()
