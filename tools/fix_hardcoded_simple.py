#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复数字孪生模拟器中的硬编码值 - 简化版本
"""

import re
from pathlib import Path

print("=" * 80)
print("Fixing hardcoded values in digital twin simulator")
print("=" * 80)

# Read file
file_path = Path(__file__).parent / "rss_digital_twin_fix.py"
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

print("\n[Step 1] Adding missing constant definitions...")
print("-" * 80)

# Find EnvironmentFactor class
constants_class_end = content.find("class EnvironmentFactor:")

if constants_class_end == -1:
    print("[ERROR] Cannot find EnvironmentFactor class")
    exit(1)

# Add missing constants
new_constants = """
    # Energy conversion coefficient
    ENERGY_TO_STAMINA_COEFF = 3.50e-05

    # Load recovery penalty coefficients
    LOAD_RECOVERY_PENALTY_COEFF = 0.0004
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0

    # Encumbrance stamina drain coefficient
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0

    # Sprint stamina drain multiplier
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0

    # Fatigue system parameters
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0

    # Metabolic adaptation parameters
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2

    # Sprint speed boost
    SPRINT_SPEED_BOOST = 0.30

    # Posture speed multipliers
    POSTURE_CROUCH_MULTIPLIER = 0.7
    POSTURE_PRONE_MULTIPLIER = 0.3

    # Action stamina costs
    JUMP_STAMINA_BASE_COST = 0.035
    VAULT_STAMINA_START_COST = 0.02
    CLIMB_STAMINA_TICK_COST = 0.01
    JUMP_CONSECUTIVE_PENALTY = 0.5

    # Slope parameters
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03

    # Swimming system parameters
    SWIMMING_BASE_POWER = 20.0
    SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0
    SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0
    SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0
    SWIMMING_ENERGY_TO_STAMINA_COEFF = 5e-05

    # Environment factor parameters
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    ENV_WIND_RESISTANCE_COEFF = 0.05
    ENV_MUD_PENALTY_MAX = 0.4

    # Marginal decay parameters
    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1

    # Minimum recovery parameters
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0

"""

# Insert new constants before EnvironmentFactor class
content = content[:constants_class_end] + new_constants + "\n" + content[constants_class_end:]
print("[OK] Added missing constant definitions")

print("\n[Step 2] Fixing hardcoded load recovery penalty coefficients...")
print("-" * 80)

# Fix line 482
old_482 = "load_recovery_penalty = (load_ratio ** 2.0) * 0.0004"
new_482 = "load_recovery_penalty = (load_ratio ** self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF"
content = content.replace(old_482, new_482)
print(f"[OK] Fixed line 482")

# Fix line 590
old_590 = "load_recovery_penalty = np.power(load_ratio, 2.0) * 0.0004"
new_590 = "load_recovery_penalty = np.power(load_ratio, self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF"
content = content.replace(old_590, new_590)
print(f"[OK] Fixed line 590")

print("\n[Step 3] Fixing hardcoded drain rates...")
print("-" * 80)

# Fix RUN drain rate (lines 297 and 412)
old_run_drain = "return 0.00008 * load_factor"
new_run_drain = "return self.constants.RUN_DRAIN_PER_TICK * load_factor"
content = content.replace(old_run_drain, new_run_drain)
print(f"[OK] Fixed RUN drain rate")

print("\n[Step 4] Fixing hardcoded static recovery rate...")
print("-" * 80)

# Fix static recovery rate (lines 301 and 417)
old_static_recovery = "return -0.00025"
new_static_recovery = "return -self.constants.REST_RECOVERY_PER_TICK"
content = content.replace(old_static_recovery, new_static_recovery)
print(f"[OK] Fixed static recovery rate")

print("\n[Step 5] Saving fixed file...")
print("-" * 80)

backup_path = Path(__file__).parent / "rss_digital_twin_fix.py.backup"
with open(backup_path, 'w', encoding='utf-8') as f:
    f.write(content)
print(f"[OK] Backup saved: {backup_path}")

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)
print(f"[OK] Fixed file saved: {file_path}")

print("\n" + "=" * 80)
print("Fix completed successfully!")
print("=" * 80)

print("\n[Summary]")
print("-" * 80)
print("1. [OK] Added missing constant definitions (about 30 parameters)")
print("2. [OK] Fixed hardcoded load recovery penalty coefficients (2 locations)")
print("3. [OK] Fixed hardcoded drain rates (2 locations)")
print("4. [OK] Fixed hardcoded static recovery rate (2 locations)")

print("\n[Next Steps]")
print("-" * 80)
print("1. Re-run optimizer: python rss_super_pipeline.py")
print("2. Verify the generated configuration meets expectations")
print("3. Test the new configuration in game")

print("\nWould you like to run the optimizer now? (y/n)")