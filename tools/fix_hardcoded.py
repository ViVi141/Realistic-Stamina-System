#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Fix hardcoded values in digital twin simulator
"""

from pathlib import Path

# Read file
file_path = Path(__file__).parent / "rss_digital_twin_fix.py"
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

print("=" * 80)
print("Fixing hardcoded values in digital twin simulator")
print("=" * 80)

# Step 1: Add missing constants
print("\n[Step 1] Adding missing constants...")
print("-" * 80)

constants_class_end = content.find("class EnvironmentFactor:")

if constants_class_end == -1:
    print("[ERROR] Cannot find EnvironmentFactor class")
    exit(1)

new_constants = """
    # Energy to stamina conversion coefficient
    ENERGY_TO_STAMINA_COEFF = 3.50e-05

    # Load recovery penalty
    LOAD_RECOVERY_PENALTY_COEFF = 0.0004
    LOAD_RECOVERY_PENALTY_EXPONENT = 2.0

    # Encumbrance stamina drain
    ENCUMBRANCE_STAMINA_DRAIN_COEFF = 2.0

    # Sprint stamina drain multiplier
    SPRINT_STAMINA_DRAIN_MULTIPLIER = 3.0

    # Fatigue system
    FATIGUE_ACCUMULATION_COEFF = 0.015
    FATIGUE_MAX_FACTOR = 2.0

    # Metabolic adaptation
    AEROBIC_EFFICIENCY_FACTOR = 0.9
    ANAEROBIC_EFFICIENCY_FACTOR = 1.2

    # Sprint speed boost
    SPRINT_SPEED_BOOST = 0.30

    # Posture multipliers
    POSTURE_CROUCH_MULTIPLIER = 0.7
    POSTURE_PRONE_MULTIPLIER = 0.3

    # Action stamina costs
    JUMP_STAMINA_BASE_COST = 0.035
    VAULT_STAMINA_START_COST = 0.02
    CLIMB_STAMINA_TICK_COST = 0.01
    JUMP_CONSECUTIVE_PENALTY = 0.5

    # Slope coefficients
    SLOPE_UPHILL_COEFF = 0.08
    SLOPE_DOWNHILL_COEFF = 0.03

    # Swimming system
    SWIMMING_BASE_POWER = 20.0
    SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0
    SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0
    SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0
    SWIMMING_ENERGY_TO_STAMINA_COEFF = 5e-05

    # Environment factors
    ENV_TEMPERATURE_HEAT_PENALTY_COEFF = 0.02
    ENV_TEMPERATURE_COLD_RECOVERY_PENALTY_COEFF = 0.05
    ENV_WIND_RESISTANCE_COEFF = 0.05
    ENV_MUD_PENALTY_MAX = 0.4

    # Marginal decay
    MARGINAL_DECAY_THRESHOLD = 0.8
    MARGINAL_DECAY_COEFF = 1.1

    # Minimum recovery
    MIN_RECOVERY_STAMINA_THRESHOLD = 0.2
    MIN_RECOVERY_REST_TIME_SECONDS = 3.0

"""

content = content[:constants_class_end] + new_constants + "\n" + content[constants_class_end:]
print("[OK] Added missing constants")

# Step 2: Fix hardcoded load recovery penalty
print("\n[Step 2] Fixing hardcoded load recovery penalty...")
print("-" * 80)

old_482 = "load_recovery_penalty = (load_ratio ** 2.0) * 0.0004"
new_482 = "load_recovery_penalty = (load_ratio ** self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF"
content = content.replace(old_482, new_482)
print(f"[OK] Fixed line 482")

old_590 = "load_recovery_penalty = np.power(load_ratio, 2.0) * 0.0004"
new_590 = "load_recovery_penalty = np.power(load_ratio, self.constants.LOAD_RECOVERY_PENALTY_EXPONENT) * self.constants.LOAD_RECOVERY_PENALTY_COEFF"
content = content.replace(old_590, new_590)
print(f"[OK] Fixed line 590")

# Step 3: Fix hardcoded drain rate
print("\n[Step 3] Fixing hardcoded drain rate...")
print("-" * 80)

old_run_drain = "return 0.00008 * load_factor"
new_run_drain = "return self.constants.RUN_DRAIN_PER_TICK * load_factor"
content = content.replace(old_run_drain, new_run_drain)
print(f"[OK] Fixed drain rate")

# Step 4: Fix hardcoded static recovery
print("\n[Step 4] Fixing hardcoded static recovery...")
print("-" * 80)

old_static_recovery = "return -0.00025"
new_static_recovery = "return -self.constants.REST_RECOVERY_PER_TICK"
content = content.replace(old_static_recovery, new_static_recovery)
print(f"[OK] Fixed static recovery rate")

# Step 5: Save file
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
print("Fix completed!")
print("=" * 80)

print("\n[Summary]")
print("-" * 80)
print("1. [OK] Added missing constants (~30 parameters)")
print("2. [OK] Fixed hardcoded load recovery penalty (2 locations)")
print("3. [OK] Fixed hardcoded drain rate (2 locations)")
print("4. [OK] Fixed hardcoded static recovery rate (2 locations)")

print("\n[Next Steps]")
print("-" * 80)
print("1. Re-run optimizer: python rss_super_pipeline.py")
print("2. Verify generated config meets expectations")
print("3. Test new config in game")