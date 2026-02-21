from rss_super_pipeline import RSSSuperPipeline, ScenarioLibrary, RSSDigitalTwin, RSSConstants

const = RSSConstants()
print('default constants:')
print('  ENERGY_TO_STAMINA_COEFF', const.ENERGY_TO_STAMINA_COEFF)
print('  BASE_RECOVERY_RATE', const.BASE_RECOVERY_RATE)

pipeline = RSSSuperPipeline(n_trials=1)
twin = RSSDigitalTwin(const)
print('basic fitness passed?', pipeline._check_basic_fitness(twin))

scenario = ScenarioLibrary.create_acft_2mile_scenario(0.0)
res = twin.simulate_scenario(
    speed_profile=scenario.speed_profile,
    current_weight=scenario.current_weight,
    grade_percent=scenario.grade_percent,
    terrain_factor=scenario.terrain_factor,
    stance=scenario.stance,
    movement_type=scenario.movement_type
)
print('simulation results:')
print(res)
