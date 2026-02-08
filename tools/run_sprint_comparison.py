#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Run a short comparison between baseline and modified sprint configurations.
"""
from rss_super_pipeline import RSSSuperPipeline
from rss_digital_twin_fix import RSSDigitalTwin, RSSConstants, MovementType, Stance
import optuna
import statistics


def simulate_sprint_drop(sprint_multiplier, duration_seconds=30.0):
    const = RSSConstants(SPRINT_STAMINA_DRAIN_MULTIPLIER=sprint_multiplier)
    twin = RSSDigitalTwin(const)
    twin.reset()
    twin.stamina = 1.0
    steps = int(duration_seconds / 0.2)
    for _ in range(steps):
        twin.step(
            speed=5.0,
            current_weight=90.0,
            grade_percent=0.0,
            terrain_factor=1.0,
            stance=Stance.STAND,
            movement_type=MovementType.SPRINT,
            current_time=0.0,
            enable_randomness=False,
        )
    return 1.0 - twin.stamina


def summarize_run(result):
    study = result['study']
    trials = study.trials
    pruned = sum(1 for t in trials if t.state == optuna.trial.TrialState.PRUNED)
    completed = sum(1 for t in trials if t.state == optuna.trial.TrialState.COMPLETE)
    best_trials = result['best_trials']

    sprint_values = []
    for t in best_trials:
        val = t.params.get('sprint_stamina_drain_multiplier')
        if val is not None:
            sprint_values.append(val)

    # Extract sprint objective (if present) from Pareto values
    sprint_obj_values = [t.values[3] for t in best_trials if len(t.values) >= 4]

    print("--- RUN SUMMARY ---")
    print(f"Total trials: {len(trials)} | Pruned: {pruned} | Complete: {completed}")
    print(f"Pareto solutions: {len(best_trials)}")
    if sprint_values:
        print(f"Sprint multiplier (best trials) - mean: {statistics.mean(sprint_values):.3f}, median: {statistics.median(sprint_values):.3f}")
        # simulate sprint drop for each Pareto trial using its full parameter set
        drops = []
        for t in best_trials:
            # build constants from trial params by mapping param names to constant names (uppercased)
            params = {k.upper(): v for k, v in t.params.items()}
            const = RSSConstants(**params)
            twin = RSSDigitalTwin(const)
            twin.reset()
            twin.stamina = 1.0
            steps = int(30.0 / 0.2)
            for _ in range(steps):
                twin.step(
                    speed=5.0,
                    current_weight=90.0,
                    grade_percent=0.0,
                    terrain_factor=1.0,
                    stance=Stance.STAND,
                    movement_type=MovementType.SPRINT,
                    current_time=0.0,
                    enable_randomness=False,
                )
            drops.append(1.0 - twin.stamina)
        print(f"Simulated 30s sprint drop across Pareto trials - mean: {statistics.mean(drops)*100:.1f}%, median: {statistics.median(drops)*100:.1f}%")
    else:
        print("No sprint parameter found in Pareto trials (maybe pruned or not used).")

    if sprint_obj_values:
        print(f"Sprint objective (best trials) - mean: {statistics.mean(sprint_obj_values):.3f}, median: {statistics.median(sprint_obj_values):.3f}")
    else:
        print("Sprint objective not present in Pareto trials.")


if __name__ == '__main__':
    # Baseline: old behavior
    print("Running baseline experiment (sprint_range=2.0-3.5, sprint_drop_min=0.10, constants_override sprint=3.0)")
    baseline = RSSSuperPipeline(n_trials=200, n_jobs=2, sprint_range=(2.0, 3.5), sprint_drop_target=(0.10,0.45), constants_override={'SPRINT_STAMINA_DRAIN_MULTIPLIER': 3.0})
    res_base = baseline.optimize(study_name='sprint_baseline')
    summarize_run(res_base)

    # Modified: new defaults
    print("\nRunning modified experiment (sprint_range=2.5-4.5, sprint_drop_target=(0.15,0.40), default sprint=3.5)")
    modified = RSSSuperPipeline(n_trials=200, n_jobs=2, sprint_range=(2.5, 4.5), sprint_drop_target=(0.15,0.40), enable_sprint_objective=True, enable_sprint_remaining_objective=True, sprint_remaining_penalty_scale=20000.0, enable_joint_sprint_energy_constraint=True, joint_sprint_energy_penalty_scale=20000.0)
    res_mod = modified.optimize(study_name='sprint_modified')
    summarize_run(res_mod)

    # Modified + Sprint-drop target penalty enforced
    print("\nRunning modified experiment WITH explicit sprint-drop penalty (0.15-0.40)")
    penalized = RSSSuperPipeline(n_trials=200, n_jobs=2, sprint_range=(2.5,4.5), sprint_drop_target=(0.15,0.40), sprint_drop_penalty_below=6000.0, sprint_drop_penalty_above=6000.0, enable_sprint_objective=True, enable_sprint_remaining_objective=True, sprint_remaining_penalty_scale=20000.0, enable_joint_sprint_energy_constraint=True, joint_sprint_energy_penalty_scale=20000.0)
    res_pen = penalized.optimize(study_name='sprint_penalized')
    summarize_run(res_pen)
