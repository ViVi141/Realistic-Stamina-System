import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt

# Defaults mirror current code constants/params.
CHARACTER_WEIGHT_KG = 90.0
BASE_WEIGHT_KG = 1.36
GAME_MAX_SPEED_MS = 5.2
TARGET_RUN_SPEED_MS = 3.7
TARGET_RUN_MULT = TARGET_RUN_SPEED_MS / GAME_MAX_SPEED_MS
DEFAULT_COEFF = 0.20
DEFAULT_EXP = 1.5
DEFAULT_MAX_PEN = 0.75
DEFAULT_SPRINT_BOOST = 0.30


def clamp(value, vmin, vmax):
    return max(vmin, min(value, vmax))


def speed_penalty(total_weight_kg, coeff, exp, max_pen):
    effective = max(total_weight_kg - CHARACTER_WEIGHT_KG - BASE_WEIGHT_KG, 0.0)
    body_mass_percent = effective / CHARACTER_WEIGHT_KG
    penalty = coeff * (body_mass_percent ** exp if body_mass_percent > 0.0 else 0.0)
    return clamp(penalty, 0.0, max_pen)


def final_speed_multiplier_run(penalty):
    return clamp(TARGET_RUN_MULT - penalty * 0.2, 0.15, 1.0)


def final_speed_multiplier_sprint(penalty, sprint_boost):
    sprint_mult = 1.0 + sprint_boost
    return clamp(TARGET_RUN_MULT * sprint_mult - penalty * 0.15, 0.15, 1.0)


def main():
    parser = argparse.ArgumentParser(description="Plot encumbrance vs max speed curve.")
    parser.add_argument("--min-weight", type=float, default=0.0, help="Min total weight (kg).")
    parser.add_argument("--max-weight", type=float, default=80.0, help="Max total weight (kg).")
    parser.add_argument("--step", type=float, default=0.5, help="Step (kg).")
    parser.add_argument("--coeff", type=float, default=DEFAULT_COEFF, help="Encumbrance speed penalty coeff.")
    parser.add_argument("--exp", type=float, default=DEFAULT_EXP, help="Encumbrance speed penalty exponent.")
    parser.add_argument("--max-pen", type=float, default=DEFAULT_MAX_PEN, help="Encumbrance speed penalty max.")
    parser.add_argument("--sprint-boost", type=float, default=DEFAULT_SPRINT_BOOST, help="Sprint speed boost.")
    parser.add_argument("--out", type=str, default="encumbrance_speed_curve.png", help="Output PNG path.")
    args = parser.parse_args()

    weights = []
    run_speeds = []
    sprint_speeds = []

    w = args.min_weight
    while w <= args.max_weight + 1e-6:
        penalty = speed_penalty(w, args.coeff, args.exp, args.max_pen)
        run_mult = final_speed_multiplier_run(penalty)
        sprint_mult = final_speed_multiplier_sprint(penalty, args.sprint_boost)
        weights.append(w)
        run_speeds.append(run_mult * GAME_MAX_SPEED_MS)
        sprint_speeds.append(sprint_mult * GAME_MAX_SPEED_MS)
        w += args.step

    plt.figure(figsize=(9, 5))
    plt.plot(weights, run_speeds, label="Run max speed")
    plt.plot(weights, sprint_speeds, label="Sprint max speed")
    plt.axhline(GAME_MAX_SPEED_MS, color="gray", linewidth=1, linestyle="--", label="Engine max")
    plt.xlabel("Total weight (kg)")
    plt.ylabel("Max speed (m/s)")
    plt.title("Encumbrance vs Max Speed")
    plt.legend()
    plt.grid(True, alpha=0.3)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    print(f"Saved: {out_path}")


if __name__ == "__main__":
    main()
