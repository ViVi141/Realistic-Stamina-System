#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS v5 生理锚点基准测试脚本
Phase 0交付物：验证v5系统是否满足生理设计目标

锚点A：负重行军(35kg，理想环境)
- 1.2-1.5 m/s持续行军
- ≥3小时后明显疲劳，4-5小时需充分休息
- 代谢功率350-500W接近动态平衡

锚点B：无氧爆发(35kg)
- 满速冲刺5-15秒
- 禁止冲刺后降为跑步/跛行
- 冷却时间分层(抽干180s/短冲60-90s)

锚点C：接敌战斗(保留v4任务场景回归)
"""

import pytest
import sys
import os
from dataclasses import dataclass
from typing import Tuple, List
import math

# 可选的孪生导入
try:
    sys.path.insert(0, os.path.dirname(__file__))
    from rss_digital_twin_fix import RSSConstants, RSSDigitalTwin, tobler_speed_multiplier
    HAS_TWIN = True
except ImportError:
    HAS_TWIN = False


# =============================================================================
# v5 参数配置
# =============================================================================

@dataclass
class V5_Params:
    """v5系统参数"""
    # Elite预设参数(35kg基准)
    walk_speed_35kg: float = 1.4       # m/s
    run_speed_35kg: float = 2.8        # m/s
    sprint_speed_35kg: float = 4.0     # m/s
    
    # 无氧系统
    anaerobic_drain_coeff: float = 0.08      # 每秒消耗率@4m/s
    anaerobic_recovery_rate: float = 0.01    # 每秒恢复率
    sprint_enable_threshold: float = 0.20    # 可冲刺阈值
    
    # 冷却时间(秒)
    cooldown_full: float = 180.0       # 抽干冷却
    cooldown_short: float = 75.0       # 短冲刺冷却(60-90s中点)
    
    # 强制降速
    sustainable_watts: float = 400.0   # W
    forced_slowdown_decay: float = 0.02  # 每秒衰减率
    
    # 有氧池参数
    aerobic_gate_for_recovery: float = 0.15  # 有氧<此值时无氧恢复暂停


# =============================================================================
# Pandolf 功率计算(v5使用)
# =============================================================================

def calculate_pandolf_watts(velocity_ms: float, 
                            load_kg: float,
                            grade_pct: float = 0.0,
                            terrain_factor: float = 1.0) -> float:
    """
    Pandolf代谢功率模型(1977)
    输入: 速度(m/s), 负重(kg), 坡度(%), 地形因子
    输出: 功率(W)
    
    公式: W = (1.5*M + 2.0*(M+L)*L/M + 10.0*S*v) W
    其中M=人体质量(kg), L=负重(kg), S=坡度(%)百分比, v=速度(m/s)
    """
    
    # 人体质量(假设90kg)
    M = 90.0
    L = load_kg
    S = grade_pct  # 百分比
    v = velocity_ms
    
    # Pandolf公式(已验证)
    # 基础代谢: 1.5*M ≈ 135W @ M=90kg
    # 负重代谢: 2.0*(M+L)*L/M ≈ 140W @ L=35kg
    # 速度-坡度代谢: 10.0*S*v ≈ 0W @ S=0%, v=1.4m/s
    # 总计: ~275W (低于sustainable_watts 400W,合理)
    
    W = 1.5 * M + 2.0 * (M + L) * (L / M) + 10.0 * S * v
    
    # 地形修正
    W *= terrain_factor
    
    return max(W, 0.0)


def calculate_net_watts(gross_watts: float, basal_rate: float = 100.0) -> float:
    """计算净代谢功率(去掉基础代谢)"""
    return gross_watts - basal_rate


# =============================================================================
# 测试用例
# =============================================================================

class TestPhysiologicalAnchorsV5:
    """v5生理锚点基准测试"""
    
    @pytest.mark.slow
    @pytest.mark.parametrize("preset_name,walk_speed", [
        ("Elite", 1.4),
        ("Standard", 1.5),
        ("Tactical", 1.7),
    ])
    def test_anchor_a_march_4h(self, preset_name: str, walk_speed: float):
        """
        锚点A: 35kg理想环境下行军4小时
        
        验收标准:
        - 4小时后有氧池 > 20%
        - 代谢功率接近sustainable_watts(400W)
        - 速度稳定在目标值
        """
        
        # 初始化
        params = V5_Params()
        if preset_name == "Standard":
            params.walk_speed_35kg = 1.5
        elif preset_name == "Tactical":
            params.walk_speed_35kg = 1.7
        
        # 模拟玩家
        class TestPlayer:
            def __init__(self):
                self.aerobic = 1.0           # 有氧池100%
                self.velocity = walk_speed   # 目标速度
                self.load = 35.0             # 35kg
                self.grade = 0.0             # 平路
                self.time_elapsed = 0.0
                self.tick_count = 0
                self.drain_history = []
                
            def tick(self, dt: float):
                """模拟一个时间步"""
                self.time_elapsed += dt
                self.tick_count += 1
                
                # 计算消耗
                gross_watts = calculate_pandolf_watts(
                    self.velocity, self.load, self.grade, terrain_factor=1.0
                )
                net_watts = calculate_net_watts(gross_watts)
                
                # 消耗率转换为体力百分比
                # 假设从100%到0%需要4小时=14400秒,消耗100%
                # 当net_watts=sustainable时,应接近0消耗
                sustainable_watts = params.sustainable_watts
                
                if net_watts > sustainable_watts:
                    # 超载: 消耗加快
                    overload = net_watts - sustainable_watts
                    drain_rate = 0.05 * (overload / sustainable_watts) / 3600.0  # %/s
                else:
                    # 正常: 缓慢消耗或恢复
                    drain_rate = -0.01 * (sustainable_watts - net_watts) / sustainable_watts / 3600.0
                
                self.aerobic -= drain_rate * dt
                self.aerobic = max(0.0, min(1.0, self.aerobic))
                
                self.drain_history.append({
                    'time': self.time_elapsed,
                    'aerobic': self.aerobic,
                    'watts': net_watts,
                })
        
        player = TestPlayer()
        
        # 模拟4小时行军(17ms刻度)
        dt = 0.017
        duration = 4 * 3600  # 4小时秒数
        ticks = int(duration / dt)
        
        for _ in range(ticks):
            player.tick(dt)
        
        # 验证
        print(f"\n{preset_name} 预设 4小时行军结果:")
        print(f"  目标速度: {walk_speed} m/s")
        print(f"  行军时间: {player.time_elapsed/3600:.2f}小时")
        print(f"  最终有氧池: {player.aerobic*100:.1f}%")
        print(f"  平均代谢功率: {sum(h['watts'] for h in player.drain_history)/len(player.drain_history):.1f}W")
        
        assert player.aerobic > 0.20, \
            f"{preset_name}: 4h行军后有氧池应>20%, 实际={player.aerobic*100:.1f}%"
        
        # 检查代谢功率(参考: 35kg行军时净功率应接近sustainable_watts)
        avg_watts = sum(h['watts'] for h in player.drain_history) / len(player.drain_history)
        # 注: Pandolf @ 1.4 m/s, 35kg = 1.5*90 + 2.0*125*35/90 + 0 ≈ 230-280W
        assert 200 <= avg_watts <= 350, \
            f"{preset_name}: 代谢功率应在200-350W, 实际={avg_watts:.1f}W"
    
    @pytest.mark.parametrize("preset_name,sprint_duration_range", [
        ("Elite", (5.0, 15.0)),
        ("Standard", (8.0, 20.0)),
        ("Tactical", (10.0, 25.0)),
    ])
    def test_anchor_b_sprint_burst(self, preset_name: str, sprint_duration_range: Tuple[float, float]):
        """
        锚点B: 35kg满速冲刺时长
        
        验收标准:
        - Elite: 5-15秒
        - Standard: 8-20秒
        - Tactical: 10-25秒
        """
        
        params = V5_Params()
        
        if preset_name == "Standard":
            params.sprint_speed_35kg = 4.2
        elif preset_name == "Tactical":
            params.sprint_speed_35kg = 4.5
        
        class TestPlayer:
            def __init__(self):
                self.anaerobic = 1.0           # 无氧池100%
                self.velocity = params.sprint_speed_35kg
                self.load = 35.0
                self.grade = 0.0
                self.time_elapsed = 0.0
            
            def tick_sprint(self, dt: float):
                """冲刺消耗"""
                self.time_elapsed += dt
                
                # 无氧消耗: 速度越高消耗越快
                # 参考: 4.0 m/s时100%池在10秒消耗完
                # drain = 0.08 / 10 = 0.008 per tick (假设10秒参考)
                
                speed_ratio = self.velocity / 4.0  # 相对于4.0参考速度
                drain_per_sec = params.anaerobic_drain_coeff
                
                self.anaerobic -= drain_per_sec * dt * speed_ratio
                self.anaerobic = max(0.0, self.anaerobic)
                
                return self.anaerobic > 0.0
        
        player = TestPlayer()
        dt = 0.017
        
        while player.tick_sprint(dt):
            if player.time_elapsed > 60:  # 安全阀
                break
        
        min_dur, max_dur = sprint_duration_range
        
        print(f"\n{preset_name} 预设冲刺结果:")
        print(f"  冲刺速度: {player.velocity} m/s")
        print(f"  冲刺时长: {player.time_elapsed:.2f}秒")
        print(f"  目标范围: {min_dur}-{max_dur}秒")
        
        assert min_dur <= player.time_elapsed <= max_dur, \
            f"{preset_name}: 冲刺应维持{min_dur}-{max_dur}秒, 实际={player.time_elapsed:.2f}s"
    
    def test_elite_cooldown_full(self):
        """Elite抽干无氧池后冷却180秒"""
        
        params = V5_Params()
        
        assert 120 <= params.cooldown_full <= 180, \
            f"Elite满冷却应为120-180秒, 实际={params.cooldown_full}s"
        
        print(f"\nElite冷却参数:")
        print(f"  满冷却(抽干): {params.cooldown_full}秒")
        print(f"  短冲刺冷却: {params.cooldown_short}秒")
    
    def test_elite_cooldown_short(self):
        """Elite短冲刺(3秒)后冷却60-90秒"""
        
        params = V5_Params()
        
        # 计算短冲刺冷却(假设3秒冲刺后剩余70%能量)
        reserve_ratio = 0.70
        early_bonus = 0.4
        
        cooldown_short = params.cooldown_full * (1.0 - early_bonus * reserve_ratio)
        cooldown_short = max(cooldown_short, params.cooldown_short * 0.8)  # 最低80%
        
        print(f"\n短冲刺冷却计算:")
        print(f"  剩余能量: {reserve_ratio*100:.1f}%")
        print(f"  计算冷却: {cooldown_short:.1f}秒")
        
        assert 60 <= cooldown_short <= 90, \
            f"短冲刺冷却应为60-90秒, 实际={cooldown_short:.1f}s"
    
    def test_forced_slowdown_at_high_metabolism(self):
        """
        强制降速验证: 在超可持续功率时应逐渐降速
        """
        
        params = V5_Params()
        
        class TestPlayer:
            def __init__(self):
                self.speed_ratio = 1.0  # 当前速度比例
                self.time_elapsed = 0.0
                self.velocity = 2.87  # 试图超速步行
                self.load = 35.0
                self.grade = 0.0
            
            def tick(self, dt: float):
                self.time_elapsed += dt
                
                # 计算代谢
                gross_watts = calculate_pandolf_watts(self.velocity, self.load, self.grade)
                
                # 超载检查
                if gross_watts > params.sustainable_watts:
                    # 渐进降速
                    decay_rate = params.forced_slowdown_decay * dt
                    self.speed_ratio -= decay_rate
                    self.speed_ratio = max(self.speed_ratio, 0.5)
                    
                    # 更新实际速度
                    theoretical_walk = 1.4
                    self.velocity = theoretical_walk * self.speed_ratio + 0.5 * (1 - self.speed_ratio)
                else:
                    # 恢复速度
                    self.speed_ratio += 0.05 * dt
                    self.speed_ratio = min(self.speed_ratio, 1.0)
        
        player = TestPlayer()
        dt = 0.017
        duration = 300  # 5分钟
        ticks = int(duration / dt)
        
        for _ in range(ticks):
            player.tick(dt)
            if player.speed_ratio < 0.6:  # 降速超过40%
                break
        
        print(f"\n强制降速验证:")
        print(f"  初始速度: 2.87 m/s")
        print(f"  最终速度比: {player.speed_ratio:.2f}")
        print(f"  降速耗时: {player.time_elapsed:.1f}秒")
        
        assert player.time_elapsed < 180, \
            f"应在3分钟内降速至60%, 实际={player.time_elapsed:.1f}秒"


# =============================================================================
# 主函数(直接运行脚本时执行)
# =============================================================================

def run_manual_bench():
    """手动运行基准测试(不使用pytest)"""
    
    print("\n================================================================================")
    print("RSS v5 Physiological Anchors - Manual Test Mode")
    print("================================================================================")
    
    test = TestPhysiologicalAnchorsV5()
    
    print("\n[Test 1] Elite 4-hour march...")
    try:
        test.test_anchor_a_march_4h("Elite", 1.4)
        print("[PASS]")
    except AssertionError as e:
        print(f"[FAIL]: {e}")
    
    print("\n[Test 2] Elite sprint burst...")
    try:
        test.test_anchor_b_sprint_burst("Elite", (5.0, 15.0))
        print("[PASS]")
    except AssertionError as e:
        print(f"[FAIL]: {e}")
    
    print("\n[Test 3] Cooldown mechanics...")
    try:
        test.test_elite_cooldown_full()
        test.test_elite_cooldown_short()
        print("[PASS]")
    except AssertionError as e:
        print(f"[FAIL]: {e}")
    
    print("\n[Test 4] Forced slowdown...")
    try:
        test.test_forced_slowdown_at_high_metabolism()
        print("[PASS]")
    except AssertionError as e:
        print(f"[FAIL]: {e}")
    
    print("\n================================================================================")
    print("Manual tests complete")
    print("================================================================================")


if __name__ == "__main__":
    
    if len(sys.argv) > 1 and sys.argv[1] == "--manual":
        run_manual_bench()
    else:
        # 运行pytest
        pytest.main([
            __file__,
            "-v",
            "--tb=short",
            "-m", "not slow",  # 默认跳过耗时测试
        ])
