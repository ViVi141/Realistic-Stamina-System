// SCR_RSS_PresetDefaults.c — 预设参数默认值初始化
// 从 SCR_RSS_Settings.c 拆分

class SCR_RSS_PresetDefaults
{
static void InitPresets(SCR_RSS_Settings s, bool forceRefreshSystemPresets = false)
    {
        if (!s)
            return;
        // 1. 确保所有对象都已实例化
        bool initElite = !s.m_EliteStandard;
        bool initStandard = !s.m_StandardMilsim;
        bool initTactical = !s.m_TacticalAction;

        if (initElite) s.m_EliteStandard = new SCR_RSS_Params();
        if (initStandard) s.m_StandardMilsim = new SCR_RSS_Params();
        if (initTactical) s.m_TacticalAction = new SCR_RSS_Params();
        
        // 2. 处理 Custom 预设：仅在不存在时初始化默认值
        bool initCustom = !s.m_Custom;
        if (initCustom)
        {
            s.m_Custom = new SCR_RSS_Params();
            InitCustomDefaults(s, true);
        }

        // 3. 处理系统预设 (Elite/Standard/Tactical)
        // 如果外部要求强制刷新（通常是因为当前没选 Custom），则覆盖为代码里的最新值
        if (forceRefreshSystemPresets)
        {
            InitEliteStandardDefaults(s, true);
            InitStandardMilsimDefaults(s, true);
            InitTacticalActionDefaults(s, true);
            SCR_RSS_Logger.Info("[RSS_Settings] System presets refreshed to latest mod defaults.");
        }
        else
        {
            // 如果不强制刷新（处于 Custom 模式），则仅初始化缺失的对象
            // 注意：这里传入 false 或根据对象是否为 null 来决定，保持向后兼容
            InitEliteStandardDefaults(s, initElite);
            InitStandardMilsimDefaults(s, initStandard);
            InitTacticalActionDefaults(s, initTactical);
        }
    }

static void InitEliteStandardDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// EliteStandard — v4 optimizer: parameter_realism最小 → 最贴近C参考值
	// 8-mission multi-phase scenarios + environment stress (2026-05)
	s.m_EliteStandard.energy_to_stamina_coeff = 7.173939269261512e-07;
	s.m_EliteStandard.base_recovery_rate = 1.5347845665467625e-04;
	s.m_EliteStandard.standing_recovery_multiplier = 1.1033940520181997;
	s.m_EliteStandard.prone_recovery_multiplier = 2.3436692109309787;
	s.m_EliteStandard.load_recovery_penalty_coeff = 5.401551119196543e-05;
	s.m_EliteStandard.load_recovery_penalty_exponent = 2.0;
	s.m_EliteStandard.encumbrance_speed_penalty_coeff = 0.12557037442961433;
	s.m_EliteStandard.encumbrance_speed_penalty_exponent = 1.5;
	s.m_EliteStandard.encumbrance_speed_penalty_max = 0.75;
	s.m_EliteStandard.encumbrance_stamina_drain_coeff = 1.9633666302334787;
	s.m_EliteStandard.load_metabolic_dampening = 0.70;
	s.m_EliteStandard.max_recovery_per_tick = 5.831263335868464e-04;
	s.m_EliteStandard.sprint_stamina_drain_multiplier = 3.5;
	s.m_EliteStandard.fatigue_accumulation_coeff = 0.015;
	s.m_EliteStandard.fatigue_max_factor = 2.0;
	s.m_EliteStandard.aerobic_efficiency_factor = 0.9;
	s.m_EliteStandard.anaerobic_efficiency_factor = 1.2;
	s.m_EliteStandard.recovery_nonlinear_coeff = 0.7916386681694456;
	s.m_EliteStandard.fast_recovery_multiplier = 2.395424393975942;
	s.m_EliteStandard.medium_recovery_multiplier = 1.1374814244785123;
	s.m_EliteStandard.slow_recovery_multiplier = 0.4756093184784932;
	s.m_EliteStandard.marginal_decay_threshold = 0.8;
	s.m_EliteStandard.marginal_decay_coeff = 1.1;
	s.m_EliteStandard.min_recovery_stamina_threshold = 0.2;
	s.m_EliteStandard.min_recovery_rest_time_seconds = 3.0;
	s.m_EliteStandard.sprint_speed_boost = 0.2561103503743016;
	s.m_EliteStandard.sprint_velocity_threshold = 5.5;
	s.m_EliteStandard.posture_crouch_multiplier = 2.437251840930866;
	s.m_EliteStandard.posture_prone_multiplier = 2.964831628856109;
	s.m_EliteStandard.jump_efficiency = 0.22;
	s.m_EliteStandard.jump_height_guess = 0.5;
	s.m_EliteStandard.jump_horizontal_speed_guess = 0.0;
	s.m_EliteStandard.climb_iso_efficiency = 0.12;
	s.m_EliteStandard.slope_uphill_coeff = 0.08;
	s.m_EliteStandard.slope_downhill_coeff = 0.03;
	s.m_EliteStandard.swimming_base_power = 20.0;
	s.m_EliteStandard.swimming_encumbrance_threshold = 25.0;
	s.m_EliteStandard.swimming_static_drain_multiplier = 3.0;
	s.m_EliteStandard.swimming_dynamic_power_efficiency = 2.0;
	s.m_EliteStandard.swimming_energy_to_stamina_coeff = 5e-05;
	s.m_EliteStandard.env_heat_stress_max_multiplier = 1.5;
	s.m_EliteStandard.env_rain_weight_max = 5.0;
	s.m_EliteStandard.env_wind_resistance_coeff = 0.05;
	s.m_EliteStandard.env_mud_penalty_max = 0.4;
	s.m_EliteStandard.env_temperature_heat_penalty_coeff = 0.02;
	s.m_EliteStandard.env_temperature_cold_recovery_penalty_coeff = 0.05;
	s.m_EliteStandard.env_surface_wetness_prone_penalty = 0.15;
}

static void InitStandardMilsimDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// StandardMilsim — v4 optimizer: 三目标均衡 → 拟真与可玩性折中
	// 8-mission multi-phase scenarios + environment stress (2026-05)
	s.m_StandardMilsim.energy_to_stamina_coeff = 5.204828024908641e-07;
	s.m_StandardMilsim.base_recovery_rate = 1.9790442492866728e-04;
	s.m_StandardMilsim.standing_recovery_multiplier = 1.7222133471594372;
	s.m_StandardMilsim.prone_recovery_multiplier = 2.4485642362267273;
	s.m_StandardMilsim.load_recovery_penalty_coeff = 5.401551119196543e-05;
	s.m_StandardMilsim.load_recovery_penalty_exponent = 2.0;
	s.m_StandardMilsim.encumbrance_speed_penalty_coeff = 0.18144287220553007;
	s.m_StandardMilsim.encumbrance_speed_penalty_exponent = 1.5;
	s.m_StandardMilsim.encumbrance_speed_penalty_max = 0.75;
	s.m_StandardMilsim.encumbrance_stamina_drain_coeff = 2.061042585044162;
	s.m_StandardMilsim.load_metabolic_dampening = 0.70;
	s.m_StandardMilsim.max_recovery_per_tick = 6.37859531144872e-04;
	s.m_StandardMilsim.sprint_stamina_drain_multiplier = 3.5;
	s.m_StandardMilsim.fatigue_accumulation_coeff = 0.015;
	s.m_StandardMilsim.fatigue_max_factor = 2.0;
	s.m_StandardMilsim.aerobic_efficiency_factor = 0.9;
	s.m_StandardMilsim.anaerobic_efficiency_factor = 1.2;
	s.m_StandardMilsim.recovery_nonlinear_coeff = 0.46571452763728277;
	s.m_StandardMilsim.fast_recovery_multiplier = 2.355594973260361;
	s.m_StandardMilsim.medium_recovery_multiplier = 1.4591476356882789;
	s.m_StandardMilsim.slow_recovery_multiplier = 0.7991960450502016;
	s.m_StandardMilsim.marginal_decay_threshold = 0.8;
	s.m_StandardMilsim.marginal_decay_coeff = 1.1;
	s.m_StandardMilsim.min_recovery_stamina_threshold = 0.2;
	s.m_StandardMilsim.min_recovery_rest_time_seconds = 3.0;
	s.m_StandardMilsim.sprint_speed_boost = 0.35415297756523734;
	s.m_StandardMilsim.sprint_velocity_threshold = 5.5;
	s.m_StandardMilsim.posture_crouch_multiplier = 1.7241996712928864;
	s.m_StandardMilsim.posture_prone_multiplier = 3.9728373312502088;
	s.m_StandardMilsim.jump_efficiency = 0.22;
	s.m_StandardMilsim.jump_height_guess = 0.5;
	s.m_StandardMilsim.jump_horizontal_speed_guess = 0.0;
	s.m_StandardMilsim.climb_iso_efficiency = 0.12;
	s.m_StandardMilsim.slope_uphill_coeff = 0.08;
	s.m_StandardMilsim.slope_downhill_coeff = 0.03;
	s.m_StandardMilsim.swimming_base_power = 20.0;
	s.m_StandardMilsim.swimming_encumbrance_threshold = 25.0;
	s.m_StandardMilsim.swimming_static_drain_multiplier = 3.0;
	s.m_StandardMilsim.swimming_dynamic_power_efficiency = 2.0;
	s.m_StandardMilsim.swimming_energy_to_stamina_coeff = 5e-05;
	s.m_StandardMilsim.env_heat_stress_max_multiplier = 1.5;
	s.m_StandardMilsim.env_rain_weight_max = 5.0;
	s.m_StandardMilsim.env_wind_resistance_coeff = 0.05;
	s.m_StandardMilsim.env_mud_penalty_max = 0.4;
	s.m_StandardMilsim.env_temperature_heat_penalty_coeff = 0.02;
	s.m_StandardMilsim.env_temperature_cold_recovery_penalty_coeff = 0.05;
	s.m_StandardMilsim.env_surface_wetness_prone_penalty = 0.15;
}

static void InitTacticalActionDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// TacticalAction — v4 optimizer: combat_endurance最小 → 最宽容
	// 8-mission multi-phase scenarios + environment stress (2026-05)
	s.m_TacticalAction.energy_to_stamina_coeff = 5.204828024908641e-07;
	s.m_TacticalAction.base_recovery_rate = 2.9653365623360926e-04;
	s.m_TacticalAction.standing_recovery_multiplier = 1.7222133471594372;
	s.m_TacticalAction.prone_recovery_multiplier = 1.9999844240601297;
	s.m_TacticalAction.load_recovery_penalty_coeff = 5.401551119196543e-05;
	s.m_TacticalAction.load_recovery_penalty_exponent = 2.0;
	s.m_TacticalAction.encumbrance_speed_penalty_coeff = 0.33862835378323297;
	s.m_TacticalAction.encumbrance_speed_penalty_exponent = 1.5;
	s.m_TacticalAction.encumbrance_speed_penalty_max = 0.75;
	s.m_TacticalAction.encumbrance_stamina_drain_coeff = 1.0135352700725277;
	s.m_TacticalAction.load_metabolic_dampening = 0.70;
	s.m_TacticalAction.max_recovery_per_tick = 6.37859531144872e-04;
	s.m_TacticalAction.sprint_stamina_drain_multiplier = 3.5;
	s.m_TacticalAction.fatigue_accumulation_coeff = 0.015;
	s.m_TacticalAction.fatigue_max_factor = 2.0;
	s.m_TacticalAction.aerobic_efficiency_factor = 0.9;
	s.m_TacticalAction.anaerobic_efficiency_factor = 1.2;
	s.m_TacticalAction.recovery_nonlinear_coeff = 0.7045964149758532;
	s.m_TacticalAction.fast_recovery_multiplier = 2.355594973260361;
	s.m_TacticalAction.medium_recovery_multiplier = 1.4900823876746438;
	s.m_TacticalAction.slow_recovery_multiplier = 0.7991960450502016;
	s.m_TacticalAction.marginal_decay_threshold = 0.8;
	s.m_TacticalAction.marginal_decay_coeff = 1.1;
	s.m_TacticalAction.min_recovery_stamina_threshold = 0.2;
	s.m_TacticalAction.min_recovery_rest_time_seconds = 3.0;
	s.m_TacticalAction.sprint_speed_boost = 0.3970126187219183;
	s.m_TacticalAction.sprint_velocity_threshold = 5.5;
	s.m_TacticalAction.posture_crouch_multiplier = 1.6230497282109586;
	s.m_TacticalAction.posture_prone_multiplier = 3.6968270696462717;
	s.m_TacticalAction.jump_efficiency = 0.22;
	s.m_TacticalAction.jump_height_guess = 0.5;
	s.m_TacticalAction.jump_horizontal_speed_guess = 0.0;
	s.m_TacticalAction.climb_iso_efficiency = 0.12;
	s.m_TacticalAction.slope_uphill_coeff = 0.08;
	s.m_TacticalAction.slope_downhill_coeff = 0.03;
	s.m_TacticalAction.swimming_base_power = 20.0;
	s.m_TacticalAction.swimming_encumbrance_threshold = 25.0;
	s.m_TacticalAction.swimming_static_drain_multiplier = 3.0;
	s.m_TacticalAction.swimming_dynamic_power_efficiency = 2.0;
	s.m_TacticalAction.swimming_energy_to_stamina_coeff = 5e-05;
	s.m_TacticalAction.env_heat_stress_max_multiplier = 1.5;
	s.m_TacticalAction.env_rain_weight_max = 5.0;
	s.m_TacticalAction.env_wind_resistance_coeff = 0.05;
	s.m_TacticalAction.env_mud_penalty_max = 0.4;
	s.m_TacticalAction.env_temperature_heat_penalty_coeff = 0.02;
	s.m_TacticalAction.env_temperature_cold_recovery_penalty_coeff = 0.05;
	s.m_TacticalAction.env_surface_wetness_prone_penalty = 0.15;
}

static void InitCustomDefaults(SCR_RSS_Settings s, bool shouldInit)
    {
        if (!shouldInit)
            return;
        
        // Custom 预设：自定义模式（合理的默认备份值）
        // 管理员可以手动调整所有参数
        s.m_Custom.energy_to_stamina_coeff = 0.000035;
        s.m_Custom.base_recovery_rate = 0.0003;
        s.m_Custom.standing_recovery_multiplier = 2.0;
        s.m_Custom.crouching_recovery_multiplier = 1.5;
        s.m_Custom.prone_recovery_multiplier = 2.2;
        s.m_Custom.load_recovery_penalty_coeff = 0.0004;
        s.m_Custom.load_recovery_penalty_exponent = 2.0;
        s.m_Custom.encumbrance_speed_penalty_coeff = 0.20;
        s.m_Custom.encumbrance_speed_penalty_exponent = 1.5;
        s.m_Custom.encumbrance_speed_penalty_max = 0.75;
        s.m_Custom.encumbrance_stamina_drain_coeff = 1.5;
        s.m_Custom.load_metabolic_dampening = 0.70;
        s.m_Custom.max_recovery_per_tick = 0.0004;
        s.m_Custom.sprint_stamina_drain_multiplier = 3.5;
        s.m_Custom.fatigue_accumulation_coeff = 0.015;
        s.m_Custom.fatigue_max_factor = 2.0;
        s.m_Custom.aerobic_efficiency_factor = 1.0;
        s.m_Custom.anaerobic_efficiency_factor = 1.3;
        s.m_Custom.recovery_nonlinear_coeff = 0.5;
        s.m_Custom.fast_recovery_multiplier = 3.0;
        s.m_Custom.medium_recovery_multiplier = 1.8;
        s.m_Custom.slow_recovery_multiplier = 0.6;
        s.m_Custom.marginal_decay_threshold = 0.85;
        s.m_Custom.marginal_decay_coeff = 1.0;
        s.m_Custom.min_recovery_stamina_threshold = 0.15;
        s.m_Custom.min_recovery_rest_time_seconds = 3.0;
        s.m_Custom.sprint_speed_boost = 0.3;
        s.m_Custom.sprint_velocity_threshold = 5.5;
        s.m_Custom.willpower_threshold = 0.35;
        s.m_Custom.sprint_enable_threshold = 0.25;
        s.m_Custom.posture_crouch_multiplier = 2.0;
        s.m_Custom.posture_prone_multiplier = 2.5;
        s.m_Custom.jump_efficiency = 0.22; // [HARD] Margaria 1963
        s.m_Custom.jump_height_guess = 0.5;
        s.m_Custom.jump_horizontal_speed_guess = 0.0;
        s.m_Custom.climb_iso_efficiency = 0.12; // [HARD] Margaria 1963
        s.m_Custom.slope_uphill_coeff = 0.08;
        s.m_Custom.slope_downhill_coeff = 0.03;
        s.m_Custom.swimming_base_power = 25.0;
        s.m_Custom.swimming_encumbrance_threshold = 25.0;
        s.m_Custom.swimming_static_drain_multiplier = 3.5;
        s.m_Custom.swimming_dynamic_power_efficiency = 2.2;
        s.m_Custom.swimming_energy_to_stamina_coeff = 5e-05;
        s.m_Custom.env_heat_stress_max_multiplier = 1.3;
        s.m_Custom.env_rain_weight_max = 5.0; // [v2.17.1] 降雨单独上限：自定义预设默认值
        s.m_Custom.env_wind_resistance_coeff = 0.05;
        s.m_Custom.env_mud_penalty_max = 0.45;
        s.m_Custom.env_temperature_heat_penalty_coeff = 0.02;
        s.m_Custom.env_temperature_cold_recovery_penalty_coeff = 0.05;
        s.m_Custom.env_surface_wetness_prone_penalty = 0.15;

        // Weather/temperature model defaults (top-level settings)
        s.m_fTempUpdateInterval = 5.0;
        s.m_fTemperatureMixingHeight = 1000.0;
        s.m_fAlbedo = 0.2;
        s.m_fAerosolOpticalDepth = 0.14;
        s.m_fSurfaceEmissivity = 0.98;
        s.m_fCloudBlockingCoeff = 0.7;
        s.m_fLECoef = 200.0;
        s.m_bUseEngineTemperature = false;
        s.m_bUseEngineTimezone = true;
        s.m_fLongitude = 0.0;
        s.m_fTimeZoneOffsetHours = 0.0;
        s.m_fAltitudeMeters = 0.0;
        s.m_bMapOverWater = false;
        s.m_fFogDensity = 0.0;
    }
}
