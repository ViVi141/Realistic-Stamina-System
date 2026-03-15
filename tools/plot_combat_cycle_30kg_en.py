#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
30KG Combat Cycle - Tactical Simulation
Simulates realistic combat scenarios: Engagement -> Withdrawal -> Recovery -> Re-engagement
Config: optimized_rss_config_realism_super.json
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
    tobler_speed_multiplier,
)

# 硬编码速度值（m/s）
SPEED_VALUES = {
    0: {"walk": 3.02, "run": 3.81, "sprint": 5.08},    # 0kg
    10: {"walk": 2.94, "run": 3.79, "sprint": 5.04},   # 10kg
    20: {"walk": 2.87, "run": 3.81, "sprint": 5.01},   # 20kg
    30: {"walk": 2.81, "run": 3.81, "sprint": 4.96},   # 30kg
    40: {"walk": 2.32, "run": 3.22, "sprint": 4.39},   # 40kg
    50: {"walk": 1.82, "run": 2.64, "sprint": 3.80},   # 50kg
    55: {"walk": 1.56, "run": 2.35, "sprint": 3.40}    # 55kg
}

def get_speed(load_kg, speed_type):
    """获取指定负载下的速度值"""
    if load_kg in SPEED_VALUES:
        return SPEED_VALUES[load_kg][speed_type]

    load_levels = sorted(SPEED_VALUES.keys())
    if load_kg <= load_levels[0]:
        return SPEED_VALUES[load_levels[0]][speed_type]
    if load_kg >= load_levels[-1]:
        return SPEED_VALUES[load_levels[-1]][speed_type]

    lower = max(x for x in load_levels if x < load_kg)
    upper = min(x for x in load_levels if x > load_kg)
    low_v = SPEED_VALUES[lower][speed_type]
    high_v = SPEED_VALUES[upper][speed_type]
    t = (float(load_kg) - float(lower)) / float(upper - lower)
    return low_v + (high_v - low_v) * t

LOAD_KG = 30
CHARACTER_WEIGHT = 90.0
DT = 0.2
TOTAL_DURATION_S = 3600.0  # 60 minutes


def load_constants_from_json(json_path: Path) -> RSSConstants:
    """Load RSSConstants from JSON config"""
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
    """Setup environment factors"""
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


def simulate_combat_cycle(twin, speed_profile, current_weight, terrain_factor=1.0, wind_drag=0.0):
    """
    Simulate combat cycle
    speed_profile: [(duration_s, speed, movement_type, stance, note, grade_percent), ...]
    """
    # Convert speed_profile to time-based format for simulate_scenario
    time_based_profile = []
    current_time = 0.0
    for duration_s, speed, movement_type, stance, note, grade_percent in speed_profile:
        time_based_profile.append((current_time, speed, movement_type, stance, note, grade_percent))
        current_time += duration_s
    
    # Use simulate_scenario to run the combat cycle
    results = twin.simulate_scenario(
        speed_profile=speed_profile,
        current_weight=current_weight,
        grade_percent=0.0,
        terrain_factor=terrain_factor,
        stance=0,
        movement_type=0,
        enable_randomness=False
    )
    
    # Extract data from results
    time_list = results['time_history']
    stamina_list = results['stamina_history']
    speed_list = results['speed_history']
    recovery_rate_list = results.get('recovery_rate_history', [0.0] * len(time_list))
    drain_rate_list = results.get('drain_rate_history', [0.0] * len(time_list))
    distance_list = [0.0] * len(time_list)
    
    # Calculate distance
    total_distance = 0.0
    for i in range(1, len(time_list)):
        if i < len(speed_list):
            distance = speed_list[i] * DT
            total_distance += distance
            distance_list[i] = total_distance
    
    # Create phase labels
    phase_labels = []
    current_time = 0.0
    for duration_s, speed, movement_type, stance, note, grade_percent in speed_profile:
        phase_labels.append({
            'start_time': current_time,
            'end_time': current_time + duration_s,
            'note': note,
            'movement': ['Idle', 'Walk', 'Run', 'Sprint'][movement_type] if movement_type <= 3 else 'Run',
            'stance': ['Stand', 'Crouch', 'Prone'][stance] if stance <= 2 else 'Stand',
            'grade': grade_percent
        })
        current_time += duration_s
    
    # Generate grade_list
    grade_list = [0.0] * len(time_list)
    
    return time_list, stamina_list, recovery_rate_list, drain_rate_list, speed_list, grade_list, distance_list, phase_labels


def create_tactical_cycle(load_kg, flat_run_speed, flat_walk_speed, sprint_speed):
    """
    Create tactical cycle based on user-provided phases:
    P1: Infiltration 0-1200 Walk Stand Low Maintain stamina above 80%, find cover
    P2: Approach 1200-1800 Run Stand Medium Quickly cross non-combat risk areas
    P3: Observation 1800-2100 Idle/Walk Prone Very Low Core recovery: forced prone reconnaissance, stamina recharge
    P4: Assault 2100-2200 Sprint Stand Burst Tactical冲击, cross exposed area
    P5: Engagement 2200-2700 Run/Walk Stand/Crouch High Sustained movement and shooting, stamina drain
    P6: Withdrawal 2700-3600 Walk Stand Medium Withdraw from combat, maintain mobility with remaining stamina
    """
    return [
        (1200.0, flat_walk_speed, MovementType.WALK, Stance.STAND, "Infiltration", 0.0),  # 0-1200s, flat
        (600.0, flat_run_speed, MovementType.RUN, Stance.STAND, "Approach", 0.0),  # 1200-1800s, flat
        (300.0, 0.0, MovementType.IDLE, Stance.PRONE, "Observation", 0.0),  # 1800-2100s, flat
        (100.0, sprint_speed, MovementType.SPRINT, Stance.STAND, "Assault", 0.0),  # 2100-2200s, flat
        (500.0, flat_run_speed, MovementType.RUN, Stance.CROUCH, "Engagement", 0.0),  # 2200-2700s, flat
        (900.0, flat_walk_speed, MovementType.WALK, Stance.STAND, "Withdrawal", 0.0),  # 2700-3600s, flat
    ]


def plot_combat_cycle(time_list, stamina_list, recovery_rate_list, drain_rate_list, speed_list, grade_list, distance_list,
                     phase_labels, out_path, title_suffix):
    """Plot combat cycle chart"""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib required: pip install matplotlib")
        return
    
    fig, (ax1, ax2, ax3, ax4, ax5) = plt.subplots(5, 1, figsize=(14, 16))
    
    # Top: Stamina change
    ax1.plot(time_list, stamina_list, color='#2E7D32', linewidth=2, label='Stamina')
    ax1.fill_between(time_list, stamina_list, alpha=0.3, color='#4CAF50')
    
    # Mark phases
    colors_phase = ['#FFCDD2', '#C8E6C9', '#BBDEFB', '#FFF9C4', '#E1BEE7']
    for i, phase in enumerate(phase_labels):
        color = colors_phase[i % len(colors_phase)]
        ax1.axvspan(phase['start_time'], phase['end_time'],
                   alpha=0.2, color=color)
        ax1.text((phase['start_time'] + phase['end_time']) / 2, 0.5,
                f"{phase['note']}\n{phase['movement']} {phase['stance']} {phase['grade']}°",
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
    
    # Mark key points
    min_stamina = min(stamina_list)
    min_time = time_list[stamina_list.index(min_stamina)]
    ax1.scatter([min_time], [min_stamina], color='red', s=100, zorder=5)
    ax1.annotate(f'Min: {min_stamina:.2%}', xy=(min_time, min_stamina),
                xytext=(10, 10), textcoords='offset points',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
    
    # Middle 1: Speed
    ax2.plot(time_list, speed_list, color='#FF9800', linewidth=2, label='Current Speed')
    ax2.fill_between(time_list, speed_list, alpha=0.2, color='#FFB74D')
    
    for phase in phase_labels:
        ax2.axvspan(phase['start_time'], phase['end_time'],
                   alpha=0.1, color='gray')
    
    ax2.set_xlabel("Time (s)", fontsize=12)
    ax2.set_ylabel("Speed (m/s)", fontsize=12)
    ax2.set_title("Current Speed", fontsize=13)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='best', fontsize=10)
    ax2.set_ylim(-0.1, 5.5)
    
    # Middle 2: Grade
    ax3.plot(time_list, grade_list, color='#9C27B0', linewidth=2, label='Terrain Grade')
    ax3.fill_between(time_list, grade_list, alpha=0.2, color='#CE93D8')
    
    for phase in phase_labels:
        ax3.axvspan(phase['start_time'], phase['end_time'],
                   alpha=0.1, color='gray')
    
    ax3.set_xlabel("Time (s)", fontsize=12)
    ax3.set_ylabel("Grade (°)", fontsize=12)
    ax3.set_title("Terrain Grade", fontsize=13)
    ax3.grid(True, alpha=0.3)
    ax3.legend(loc='best', fontsize=10)
    ax3.axhline(y=0, color='black', linestyle='-', alpha=0.5)
    
    # Middle 3: Distance
    ax4.plot(time_list, distance_list, color='#2196F3', linewidth=2, label='Cumulative Distance')
    ax4.fill_between(time_list, distance_list, alpha=0.2, color='#64B5F6')
    
    for phase in phase_labels:
        ax4.axvspan(phase['start_time'], phase['end_time'],
                   alpha=0.1, color='gray')
    
    ax4.set_xlabel("Time (s)", fontsize=12)
    ax4.set_ylabel("Distance (m)", fontsize=12)
    ax4.set_title("Cumulative Distance vs Time", fontsize=13)
    ax4.grid(True, alpha=0.3)
    ax4.legend(loc='best', fontsize=10)
    
    # Bottom: Recovery vs Drain rate
    ax5.plot(time_list, recovery_rate_list, color='#1976D2', linewidth=1.5,
             label='Recovery Rate', alpha=0.7)
    ax5.plot(time_list, drain_rate_list, color='#D32F2F', linewidth=1.5,
             label='Drain Rate', alpha=0.7)
    
    for phase in phase_labels:
        ax5.axvspan(phase['start_time'], phase['end_time'],
                   alpha=0.1, color='gray')
    
    ax5.set_xlabel("Time (s)", fontsize=12)
    ax5.set_ylabel("Rate (per tick)", fontsize=12)
    ax5.set_title("Recovery vs Drain Rate", fontsize=13)
    ax5.grid(True, alpha=0.3)
    ax5.legend(loc='best', fontsize=10)
    ax5.axhline(y=0, color='black', linestyle='-', alpha=0.5)
    
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    import matplotlib.pyplot as plt
    plt.close(fig)
    print(f"Saved: {out_path}")
    
    # Print statistics
    print(f"\n=== Combat Cycle Statistics ===")
    if stamina_list:
        print(f"Starting stamina: {stamina_list[0]:.2%}")
        print(f"Minimum stamina: {min_stamina:.2%} (at {min_time:.0f}s)")
        print(f"Final stamina: {stamina_list[-1]:.2%}")
        print(f"Stamina change: {(stamina_list[-1] - stamina_list[0]):+.2%}")
    if distance_list:
        print(f"Total distance: {distance_list[-1]:.1f} meters")
    if recovery_rate_list and drain_rate_list:
        print(f"\n=== Recovery vs Drain Rates ===")
        print(f"Average recovery rate: {sum(recovery_rate_list)/len(recovery_rate_list):.6f}")
        print(f"Average drain rate: {sum(drain_rate_list)/len(drain_rate_list):.6f}")
        print(f"Max recovery rate: {max(recovery_rate_list):.6f}")
        print(f"Max drain rate: {max(drain_rate_list):.6f}")
    
    print(f"\n=== Phase Analysis ===")
    for i, phase in enumerate(phase_labels):
        # Find the closest time in time_list for start and end times
        start_time = phase['start_time']
        end_time = phase['end_time']
        
        # Find the closest indices
        start_idx = min(range(len(time_list)), key=lambda j: abs(time_list[j] - start_time))
        end_idx = min(range(len(time_list)), key=lambda j: abs(time_list[j] - end_time))
        
        if 0 <= start_idx < len(stamina_list) and 0 <= end_idx < len(stamina_list):
            phase_start_stamina = stamina_list[start_idx]
            phase_end_stamina = stamina_list[end_idx]
            phase_change = phase_end_stamina - phase_start_stamina
            print(f"{i+1}. {phase['note']:12s} ({phase['movement']} {phase['stance']:5s} {phase['grade']:3.1f}°): "
                  f"{phase_start_stamina:.2%} -> {phase_end_stamina:.2%} "
                  f"({phase_change:+.2%})")
        else:
            print(f"{i+1}. {phase['note']:12s} ({phase['movement']} {phase['stance']:5s} {phase['grade']:3.1f}°): "
                  f"Data not available")


def main():
    json_path = SCRIPT_DIR / "optimized_rss_config_realism_super.json"
    if not json_path.exists():
        print(f"Config not found: {json_path}")
        return 1
    
    constants = load_constants_from_json(json_path)
    twin = RSSDigitalTwin(constants)
    
    current_weight = CHARACTER_WEIGHT + float(LOAD_KG)
    
    # Get Walk, Run, and Sprint speeds from hardcoded values
    flat_walk_speed = get_speed(LOAD_KG, "walk")
    flat_run_speed = get_speed(LOAD_KG, "run")
    flat_sprint_speed = get_speed(LOAD_KG, "sprint")
    
    print(f"30kg Speeds:")
    print(f"  Walk:  {flat_walk_speed:.2f} m/s")
    print(f"  Run:   {flat_run_speed:.2f} m/s")
    print(f"  Sprint: {flat_sprint_speed:.2f} m/s\n")
    
    # Setup ideal environment
    setup_environment(twin, temperature_celsius=20.0, wind_speed_mps=0.0)
    
    # Create tactical cycle
    speed_profile = create_tactical_cycle(LOAD_KG, flat_run_speed, flat_walk_speed, flat_sprint_speed)
    
    # Simulate
    time_list, stamina_list, recovery_rate_list, drain_rate_list, speed_list, grade_list, distance_list, phase_labels = \
        simulate_combat_cycle(twin, speed_profile, current_weight)

    min_stamina = min(stamina_list)
    if min_stamina < 0.23:
        print("\n[WARNING] Combat cycle minimum stamina {:.1%} is below optimizer constraint (23%). "
              "This config may have been exported before the digital-twin 0.3 drain fix. "
              "Re-run the optimizer (e.g. rss_super_pipeline) with current code to get a preset that satisfies the constraint.\n".format(min_stamina))

    # Plot
    out_path = SCRIPT_DIR / "combat_cycle_30kg_en.png"
    plot_combat_cycle(time_list, stamina_list, recovery_rate_list, drain_rate_list, speed_list, grade_list, distance_list,
                     phase_labels, out_path,
                     f"Load={LOAD_KG}kg, Run={flat_run_speed:.2f}m/s, Sprint={flat_sprint_speed:.2f}m/s")
    
    print("\nChart generation complete!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
