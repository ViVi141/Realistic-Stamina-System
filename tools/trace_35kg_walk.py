#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
35KG 理想环境「走一步」体力消耗追踪
配置来源: optimized_rss_config_realism_super.json
"""
import math

# 从 JSON 配置
params = {
    "energy_to_stamina_coeff": 8.643552656801269e-7,
    "base_recovery_rate": 0.00016234616393570565,
    "load_recovery_penalty_coeff": 0.00001029827680626232,
    "load_recovery_penalty_exponent": 1.5990126371146123,
    "encumbrance_stamina_drain_coeff": 1.290612394070082,
    "standing_recovery_multiplier": 1.7983668851471908,
    "recovery_nonlinear_coeff": 0.1455761721789501,
    "fast_recovery_multiplier": 2.1936592991633046,
    "marginal_decay_threshold": 0.7242596482925,
    "marginal_decay_coeff": 1.136465911388989,
}

# 常量
CHARACTER_WEIGHT = 90.0
BASE_WEIGHT = 1.36
REFERENCE_WEIGHT = 90.0
PANDOLF_BASE_COEFF = 2.7
PANDOLF_VELOCITY_COEFF = 3.2
PANDOLF_VELOCITY_OFFSET = 0.7
FIXED_PANDOLF_FITNESS_BONUS = 0.80
FIXED_FITNESS_RECOVERY_MULTIPLIER = 1.25
FIXED_AGE_RECOVERY_MULTIPLIER = 1.053
BODY_TOLERANCE_BASE = 90.0
TICK = 0.2

# 场景: 35KG 负重, Walk 速度 2.8 m/s, 理想环境(坡度0, 地形1.0)
equipment_kg = 35.0
total_weight = CHARACTER_WEIGHT + equipment_kg  # 125 kg
walk_speed = 2.8
grade_percent = 0.0
terrain_factor = 1.0
stamina = 1.0  # 满体力起步

print("=" * 60)
print("35KG 理想环境 Walk 一步 (0.2s) 体力追踪")
print("=" * 60)
print(f"负重: {equipment_kg} kg, 总重: {total_weight} kg, 速度: {walk_speed} m/s")
print()

# ========== 1. 消耗: Pandolf 基础 ==========
vt = walk_speed - PANDOLF_VELOCITY_OFFSET
base_term = (PANDOLF_BASE_COEFF * FIXED_PANDOLF_FITNESS_BONUS) + (PANDOLF_VELOCITY_COEFF * vt * vt)
grade_term = 0.0
w_mult = max(total_weight / REFERENCE_WEIGHT, 0.1)
energy_watts = w_mult * (base_term + grade_term) * terrain_factor * REFERENCE_WEIGHT
pandolf_per_s = energy_watts * params["energy_to_stamina_coeff"]
base_drain_per_tick = pandolf_per_s * TICK

print("[1] Pandolf 基础消耗")
print(f"    base_term={base_term:.4f}, energy={energy_watts:.1f} W")
print(f"    pandolf_per_s={pandolf_per_s:.6f} %/s")
print(f"    base_drain_per_tick={base_drain_per_tick:.6f}")
print()

# ========== 2. 消耗: 姿态、效率、疲劳、负重 ==========
posture_mult = 1.0  # 站立
speed_ratio = walk_speed / 5.2
# AEROBIC 0.6, ANAEROBIC 0.8
if speed_ratio < 0.6:
    metabolic_eff = 0.9
elif speed_ratio < 0.8:
    metabolic_eff = 0.9 + (speed_ratio - 0.6) / 0.2 * (1.2 - 0.9)
else:
    metabolic_eff = 1.2
total_eff = 0.70 * metabolic_eff  # fitness * metabolic
fatigue_mult = 1.0

# Encumbrance: C 端用 equipment - BASE_WEIGHT
effective_weight = max(0, equipment_kg - BASE_WEIGHT)
body_mass_pct = effective_weight / CHARACTER_WEIGHT
enc_mult = 1.0 + params["encumbrance_stamina_drain_coeff"] * body_mass_pct
enc_mult = max(1.0, min(3.0, enc_mult))

total_drain = base_drain_per_tick * posture_mult * total_eff * fatigue_mult * enc_mult

print("[2] 消耗链乘数")
print(f"    posture=1.0, total_eff={total_eff:.3f}, fatigue=1.0")
print(f"    enc: effective_weight={effective_weight:.2f}, body_mass_pct={body_mass_pct:.3f}")
print(f"    enc_mult={enc_mult:.3f}")
print(f"    total_drain_per_tick={total_drain:.6f}")
print()

# ========== 3. 恢复: 基础 ==========
stamina_mult = 1.0 + params["recovery_nonlinear_coeff"] * (1.0 - stamina)
base_rec = params["base_recovery_rate"] * stamina_mult
rest_time_mult = params["fast_recovery_multiplier"]  # rest=0 时用 fast
rec_rate = base_rec * FIXED_FITNESS_RECOVERY_MULTIPLIER * FIXED_AGE_RECOVERY_MULTIPLIER
rec_rate *= rest_time_mult * 1.0 * params["standing_recovery_multiplier"]  # fatigue=1, stance
print("[3] 恢复基础")
print(f"    stamina_mult={stamina_mult:.4f}, base_rec={base_rec:.6f}")
print(f"    rest_time_mult(fast)={rest_time_mult:.3f}, stance_mult={params['standing_recovery_multiplier']:.3f}")
print(f"    rec_before_load_penalty={rec_rate:.6f}")
print()

# ========== 4. 恢复: 负重剥夺 (C 用 equipment 作为 currentWeight) ==========
load_ratio = min(max(equipment_kg / BODY_TOLERANCE_BASE, 0), 2.0)  # C: currentWeight=35
load_penalty = (load_ratio ** params["load_recovery_penalty_exponent"]) * params["load_recovery_penalty_coeff"]
rec_rate -= load_penalty
print("[4] 负重剥夺")
print(f"    load_ratio={load_ratio:.3f} (C 用 equipment/90)")
print(f"    load_penalty={load_penalty:.6f}")
print(f"    rec_after_load={rec_rate:.6f}")
print()

# ========== 5. 恢复: 边际效应衰减 (关键) ==========
if stamina > params["marginal_decay_threshold"]:
    marginal_mult = max(0.2, min(1.0, params["marginal_decay_coeff"] - stamina))
else:
    marginal_mult = 1.0
rec_rate *= marginal_mult
print("[5] 边际效应衰减 (体力>72.4%时大幅压制恢复)")
print(f"    stamina={stamina:.2f}, threshold={params['marginal_decay_threshold']:.3f}")
print(f"    marginal_mult={marginal_mult:.3f}")
print(f"    rec_after_marginal={rec_rate:.6f}")
print()

# ========== 6. 恢复: 速度调整 (Walk 时 0.8) ==========
speed_rec_mult = 0.8  # Walk: 3.2 > speed >= 0.1
rec_rate *= speed_rec_mult
print("[6] 速度恢复倍数 (Walk=0.8)")
print(f"    speed_rec_mult={speed_rec_mult}")
print(f"    final_recovery_per_tick={rec_rate:.6f}")
print()

# ========== 净值 ==========
net = rec_rate - total_drain
print("=" * 60)
print("[结果] 每 tick (0.2s) 净值")
print(f"    恢复: {rec_rate:.6f}")
print(f"    消耗: {total_drain:.6f}")
print(f"    净值: {net:.6f} ({'净消耗' if net < 0 else '净恢复'})")
print()

# 诊断
print("=" * 60)
print("[诊断] 导致「走一步就掉体力」的主要环节")
print("=" * 60)
if marginal_mult < 0.5:
    print(f"  *** 边际效应衰减: 体力>{params['marginal_decay_threshold']*100:.0f}% 时恢复被压至 {marginal_mult*100:.0f}%")
    print(f"      恢复从 {rec_rate/speed_rec_mult/marginal_mult:.6f} 被压到 {rec_rate/speed_rec_mult:.6f}")
if enc_mult > 1.3:
    print(f"  *** 负重消耗倍数: {enc_mult:.2f}x 显著放大消耗")
if net < 0:
    print(f"  *** 净消耗: 恢复({rec_rate:.6f}) < 消耗({total_drain:.6f})")
    print(f"      每步约掉 {abs(net)*100:.4f}% 体力")
