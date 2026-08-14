//! Preset default bake / init (split from SCR_RSS_Settings.c for compile ICE relief)
class SCR_RSS_SettingsPresetBake
{
    // ==================== 初始化预设默认值 ====================
    // 三个系统预设 + Custom；由 v4 管线（8 场景 NSGA-II）写出 JSON 后嵌入本文件
    // EliteStandard：低 combat_reserve + 低 recovery_pace（最硬核，无单独 Hardcore 档）
    // StandardMilsim：三目标折中
    // TacticalAction：combat 最小（最宽容）
    // Custom：手动调参备份默认值
    
    static void InitPresets(SCR_RSS_Settings s, bool forceRefreshSystemPresets = false)
    {
        // 1. 确保所有对象都已实例化
        bool initElite = !s.m_EliteStandard;
        bool initStandard = !s.m_StandardMilsim;
        bool initTactical = !s.m_TacticalAction;

        if (initElite)
        {
            s.m_EliteStandard = new SCR_RSS_Params();
        }
        if (initStandard)
        {
            s.m_StandardMilsim = new SCR_RSS_Params();
        }
        if (initTactical)
        {
            s.m_TacticalAction = new SCR_RSS_Params();
        }
        
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
            Print("[RSS_Settings] System presets refreshed to latest mod defaults.");
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
    
    // 初始化 EliteStandard 预设默认值
    static void InitEliteStandardDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// EliteStandard — v6 optimizer merge
	// 低 combat_ease + 低 recovery_ease → 最拟真/最硬核
	// metrics: ease=0.7956 recovery=0.000941 realism=2.5194
	s.m_EliteStandard.energy_to_stamina_coeff = 1.7007074390325853e-07;
	s.m_EliteStandard.base_recovery_rate = 9.2048888822953643e-05;
	s.m_EliteStandard.standing_recovery_multiplier = 0.7036252376302605;
	s.m_EliteStandard.prone_recovery_multiplier = 1.6323164696562544;
	s.m_EliteStandard.load_recovery_penalty_coeff = 1.9923493936336870e-04;
	s.m_EliteStandard.load_recovery_penalty_exponent = 2.0;
	s.m_EliteStandard.encumbrance_speed_penalty_coeff = 0.32914617595982326;
	s.m_EliteStandard.encumbrance_speed_penalty_exponent = 1.5;
	s.m_EliteStandard.encumbrance_speed_penalty_max = 0.75;
	s.m_EliteStandard.encumbrance_stamina_drain_coeff = 2.6017248000726663;
	s.m_EliteStandard.load_metabolic_dampening = 0.70;
	s.m_EliteStandard.max_recovery_per_tick = 2.3150469479475108e-04;
	s.m_EliteStandard.sprint_stamina_drain_multiplier = 3.5;
	s.m_EliteStandard.fatigue_accumulation_coeff = 0.015;
	s.m_EliteStandard.fatigue_max_factor = 2.0;
	s.m_EliteStandard.aerobic_efficiency_factor = 0.9;
	s.m_EliteStandard.anaerobic_efficiency_factor = 1.2;
	s.m_EliteStandard.recovery_nonlinear_coeff = 0.32345781637146453;
	s.m_EliteStandard.fast_recovery_multiplier = 1.5840666229329088;
	s.m_EliteStandard.medium_recovery_multiplier = 0.9172495737205478;
	s.m_EliteStandard.slow_recovery_multiplier = 0.35370534372094853;
	s.m_EliteStandard.marginal_decay_threshold = 0.8;
	s.m_EliteStandard.marginal_decay_coeff = 1.1;
	s.m_EliteStandard.min_recovery_stamina_threshold = 0.2;
	s.m_EliteStandard.min_recovery_rest_time_seconds = 3.0;
	s.m_EliteStandard.sprint_speed_boost = 0.22105729089449438;
	s.m_EliteStandard.sprint_velocity_threshold = 5.5;
	s.m_EliteStandard.posture_crouch_multiplier = 2.954245615946638;
	s.m_EliteStandard.posture_prone_multiplier = 3.08595425773382;
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
	s.m_EliteStandard.crouching_recovery_multiplier = 1.3922029498811115;
	s.m_EliteStandard.willpower_threshold = 0.3038205335360764;
	s.m_EliteStandard.sprint_enable_threshold = 0.20691304690460754;
	s.m_EliteStandard.w_prime_recovery_mode = 0.0000000000000000e+00;
	ApplyV6TierCpDefaults(s.m_EliteStandard, 0);
	s.m_EliteStandard.critical_power_watts = 907.7632740817373;
	s.m_EliteStandard.sprint_power_cap_watts = 2730.1644178188026;
	s.m_EliteStandard.v5_run_speed_ms = 3.05;
	s.m_EliteStandard.v5_sprint_speed_ms = 4.5;
	s.m_EliteStandard.v5_walk_speed_ms = 1.4;
	s.m_EliteStandard.w_prime_max_joules = 2.2897951197083501e+04;
	s.m_EliteStandard.w_prime_recovery_w_per_s = 11.142296222078262;
}








    
            // 初始化 StandardMilsim 预设默认值
    static void InitStandardMilsimDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// StandardMilsim — v6 optimizer merge
	// 战斗/恢复折中 → 拟真与可玩性平衡
	// metrics: ease=0.7934 recovery=0.001236 realism=3.5105
	s.m_StandardMilsim.energy_to_stamina_coeff = 1.2020815272044153e-07;
	s.m_StandardMilsim.base_recovery_rate = 1.0648818614147173e-04;
	s.m_StandardMilsim.standing_recovery_multiplier = 0.8028442393119766;
	s.m_StandardMilsim.prone_recovery_multiplier = 1.7543096929090567;
	s.m_StandardMilsim.load_recovery_penalty_coeff = 1.2275957596568957e-04;
	s.m_StandardMilsim.load_recovery_penalty_exponent = 2.0;
	s.m_StandardMilsim.encumbrance_speed_penalty_coeff = 0.2314990834591352;
	s.m_StandardMilsim.encumbrance_speed_penalty_exponent = 1.5;
	s.m_StandardMilsim.encumbrance_speed_penalty_max = 0.75;
	s.m_StandardMilsim.encumbrance_stamina_drain_coeff = 2.574976444921473;
	s.m_StandardMilsim.load_metabolic_dampening = 0.70;
	s.m_StandardMilsim.max_recovery_per_tick = 3.8418197485776010e-04;
	s.m_StandardMilsim.sprint_stamina_drain_multiplier = 3.5;
	s.m_StandardMilsim.fatigue_accumulation_coeff = 0.015;
	s.m_StandardMilsim.fatigue_max_factor = 2.0;
	s.m_StandardMilsim.aerobic_efficiency_factor = 0.9;
	s.m_StandardMilsim.anaerobic_efficiency_factor = 1.2;
	s.m_StandardMilsim.recovery_nonlinear_coeff = 0.5411254316579374;
	s.m_StandardMilsim.fast_recovery_multiplier = 1.6544610064331133;
	s.m_StandardMilsim.medium_recovery_multiplier = 1.0049846211569977;
	s.m_StandardMilsim.slow_recovery_multiplier = 0.33038804605773786;
	s.m_StandardMilsim.marginal_decay_threshold = 0.8;
	s.m_StandardMilsim.marginal_decay_coeff = 1.1;
	s.m_StandardMilsim.min_recovery_stamina_threshold = 0.2;
	s.m_StandardMilsim.min_recovery_rest_time_seconds = 3.0;
	s.m_StandardMilsim.sprint_speed_boost = 0.20259772471376725;
	s.m_StandardMilsim.sprint_velocity_threshold = 5.5;
	s.m_StandardMilsim.posture_crouch_multiplier = 2.505195675044107;
	s.m_StandardMilsim.posture_prone_multiplier = 4.179512239327796;
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
	s.m_StandardMilsim.crouching_recovery_multiplier = 1.448386751595432;
	s.m_StandardMilsim.willpower_threshold = 0.3968922007178048;
	s.m_StandardMilsim.sprint_enable_threshold = 0.1863367010530013;
	s.m_StandardMilsim.w_prime_recovery_mode = 1.0;
	ApplyV6TierCpDefaults(s.m_StandardMilsim, 1);
	s.m_StandardMilsim.critical_power_watts = 1031.0269402827182;
	s.m_StandardMilsim.sprint_power_cap_watts = 2804.5755541954586;
	s.m_StandardMilsim.v5_run_speed_ms = 3.2;
	s.m_StandardMilsim.v5_sprint_speed_ms = 4.5;
	s.m_StandardMilsim.v5_walk_speed_ms = 1.4;
	s.m_StandardMilsim.w_prime_max_joules = 3.0694588204147236e+04;
	s.m_StandardMilsim.w_prime_recovery_w_per_s = 12.56888534461861;
}








    
            // 初始化 TacticalAction 预设默认值
    static void InitTacticalActionDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// TacticalAction — v6 optimizer merge
	// 高 combat_ease + 高 recovery_ease → 战斗最宽容
	// metrics: ease=0.7949 recovery=0.001275 realism=4.0103
	s.m_TacticalAction.energy_to_stamina_coeff = 1.0430233936563329e-07;
	s.m_TacticalAction.base_recovery_rate = 1.1990859520060127e-04;
	s.m_TacticalAction.standing_recovery_multiplier = 0.8836240704260153;
	s.m_TacticalAction.prone_recovery_multiplier = 2.002116594805323;
	s.m_TacticalAction.load_recovery_penalty_coeff = 1.0228602818846379e-04;
	s.m_TacticalAction.load_recovery_penalty_exponent = 2.0;
	s.m_TacticalAction.encumbrance_speed_penalty_coeff = 0.21711050931835466;
	s.m_TacticalAction.encumbrance_speed_penalty_exponent = 1.5;
	s.m_TacticalAction.encumbrance_speed_penalty_max = 0.75;
	s.m_TacticalAction.encumbrance_stamina_drain_coeff = 2.507952749010908;
	s.m_TacticalAction.load_metabolic_dampening = 0.70;
	s.m_TacticalAction.max_recovery_per_tick = 4.1993135026285460e-04;
	s.m_TacticalAction.sprint_stamina_drain_multiplier = 3.5;
	s.m_TacticalAction.fatigue_accumulation_coeff = 0.015;
	s.m_TacticalAction.fatigue_max_factor = 2.0;
	s.m_TacticalAction.aerobic_efficiency_factor = 0.9;
	s.m_TacticalAction.anaerobic_efficiency_factor = 1.2;
	s.m_TacticalAction.recovery_nonlinear_coeff = 0.36243363229323305;
	s.m_TacticalAction.fast_recovery_multiplier = 1.8490315942158921;
	s.m_TacticalAction.medium_recovery_multiplier = 1.0458997891646118;
	s.m_TacticalAction.slow_recovery_multiplier = 0.3456477398318387;
	s.m_TacticalAction.marginal_decay_threshold = 0.8;
	s.m_TacticalAction.marginal_decay_coeff = 1.1;
	s.m_TacticalAction.min_recovery_stamina_threshold = 0.2;
	s.m_TacticalAction.min_recovery_rest_time_seconds = 3.0;
	s.m_TacticalAction.sprint_speed_boost = 0.21089948340831;
	s.m_TacticalAction.sprint_velocity_threshold = 5.5;
	s.m_TacticalAction.posture_crouch_multiplier = 3.3382056894573235;
	s.m_TacticalAction.posture_prone_multiplier = 4.148454793616982;
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
	s.m_TacticalAction.crouching_recovery_multiplier = 1.5289464271704793;
	s.m_TacticalAction.willpower_threshold = 0.3907947102871067;
	s.m_TacticalAction.sprint_enable_threshold = 0.28692893122679247;
	s.m_TacticalAction.w_prime_recovery_mode = 1.0;
	ApplyV6TierCpDefaults(s.m_TacticalAction, 2);
	s.m_TacticalAction.critical_power_watts = 1080.5772859061578;
	s.m_TacticalAction.sprint_power_cap_watts = 2904.58612356082;
	s.m_TacticalAction.v5_run_speed_ms = 3.4;
	s.m_TacticalAction.v5_sprint_speed_ms = 4.5;
	s.m_TacticalAction.v5_walk_speed_ms = 1.4;
	s.m_TacticalAction.w_prime_max_joules = 3.1461312101281204e+04;
	s.m_TacticalAction.w_prime_recovery_w_per_s = 12.873066473065165;
}


    
    // 初始化 Custom 预设默认值
    static void InitCustomDefaults(SCR_RSS_Settings s, bool shouldInit)
    {
        if (!shouldInit)
            return;
        
        // Custom 预设：自定义模式（合理的默认备份值）
        // 管理员可以手动调整所有参数
        s.m_Custom.energy_to_stamina_coeff = 1.117e-07; // 对齐 Standard，消除 300x 漂移（原 v4 值 3.5e-05）
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
        ApplyV6TierCpDefaults(s.m_Custom, 1);
    }

    //! v5 双池参数字段默认值（三档预设 + Custom 共用）
    static void ApplyV5ParamsDefaults(SCR_RSS_Params p)
    {
        ApplyMetabolicAnchorDefaults(p);
    }

    //! 行军档 / 双池代谢锚点默认值（canonical）
    static void ApplyMetabolicAnchorDefaults(SCR_RSS_Params p)
    {
        if (!p)
            return;
        p.sustainable_watts = SCR_RSS_Constants.V5_SUSTAINABLE_WATTS_DEFAULT;
        p.v5_walk_speed_ms = 1.4;
        p.v5_run_speed_ms = 2.8;
        p.v5_sprint_speed_ms = 4.5;
        p.anaerobic_sprint_enable_threshold = SCR_RSS_Constants.V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT;
        p.burst_cooldown_full_seconds = SCR_RSS_Constants.V5_BURST_COOLDOWN_FULL_DEFAULT;
        p.burst_cooldown_short_seconds = SCR_RSS_Constants.V5_BURST_COOLDOWN_SHORT_DEFAULT;
        p.anaerobic_drain_per_sec = 0.12;
        p.anaerobic_recovery_per_sec = SCR_RSS_Constants.V5_ANAEROBIC_RECOVERY_PER_SEC_DEFAULT;
        p.critical_power_watts = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        p.w_prime_max_joules = SCR_RSS_Constants.V6_W_PRIME_MAX_JOULES_DEFAULT;
        p.w_prime_recovery_w_per_s = SCR_RSS_Constants.V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT;
        p.sprint_power_cap_watts = SCR_RSS_Constants.V6_SPRINT_POWER_CAP_WATTS_DEFAULT;
        p.w_prime_recovery_mode = 0.0;  // 默认 Skiba（Elite）
    }

    //! v6 三档 CP-W' 分化（Elite=0 Standard=1 Tactical=2）
    static void ApplyV6TierCpDefaults(SCR_RSS_Params p, int tier)
    {
        if (!p)
            return;
        ApplyV5ParamsDefaults(p);
        if (tier == 0)
        {
            p.critical_power_watts = 907.7632740817373;
            p.w_prime_max_joules = 2.2897951197083501e+04;
            p.w_prime_recovery_w_per_s = 11.142296222078262;
            p.sprint_power_cap_watts = 2730.1644178188026;
            p.w_prime_recovery_mode = 0.0;  // Skiba
            p.v5_walk_speed_ms = 1.4;
            p.v5_run_speed_ms = 3.05;
            p.v5_sprint_speed_ms = 4.5;
            p.burst_cooldown_full_seconds = 180.0;
            p.burst_cooldown_short_seconds = 75.0;
        }
        else if (tier == 1)
        {
            p.critical_power_watts = 1031.0269402827182;
            p.w_prime_max_joules = 3.0694588204147236e+04;
            p.w_prime_recovery_w_per_s = 12.56888534461861;
            p.sprint_power_cap_watts = 2804.5755541954586;
            p.w_prime_recovery_mode = 1.0;  // 线性
            p.v5_walk_speed_ms = 1.4;
            p.v5_run_speed_ms = 3.2;
            p.v5_sprint_speed_ms = 4.5;
            p.burst_cooldown_full_seconds = 120.0;
            p.burst_cooldown_short_seconds = 60.0;
        }
        else
        {
            p.critical_power_watts = 1080.5772859061578;
            p.w_prime_max_joules = 3.1461312101281204e+04;
            p.w_prime_recovery_w_per_s = 12.873066473065165;
            p.sprint_power_cap_watts = 2904.58612356082;
            p.w_prime_recovery_mode = 1.0;  // 线性
            p.v5_walk_speed_ms = 1.4;
            p.v5_run_speed_ms = 3.4;
            p.v5_sprint_speed_ms = 4.5;
            p.burst_cooldown_full_seconds = 90.0;
            p.burst_cooldown_short_seconds = 45.0;
        }
        p.sustainable_watts = p.critical_power_watts;
    }

}
