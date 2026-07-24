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
	// metrics: ease=0.8195 recovery=0.000994 realism=2.9506
	s.m_EliteStandard.energy_to_stamina_coeff = 1.6762486580255938e-07;
	s.m_EliteStandard.base_recovery_rate = 8.4523221021625403e-05;
	s.m_EliteStandard.standing_recovery_multiplier = 0.8099529270981558;
	s.m_EliteStandard.prone_recovery_multiplier = 1.7949480598703875;
	s.m_EliteStandard.load_recovery_penalty_coeff = 2.8563630829716636e-04;
	s.m_EliteStandard.load_recovery_penalty_exponent = 2.0;
	s.m_EliteStandard.encumbrance_speed_penalty_coeff = 0.30921194998729323;
	s.m_EliteStandard.encumbrance_speed_penalty_exponent = 1.5;
	s.m_EliteStandard.encumbrance_speed_penalty_max = 0.75;
	s.m_EliteStandard.encumbrance_stamina_drain_coeff = 2.3165524060538982;
	s.m_EliteStandard.load_metabolic_dampening = 0.70;
	s.m_EliteStandard.max_recovery_per_tick = 4.0992910421115437e-04;
	s.m_EliteStandard.sprint_stamina_drain_multiplier = 3.5;
	s.m_EliteStandard.fatigue_accumulation_coeff = 0.015;
	s.m_EliteStandard.fatigue_max_factor = 2.0;
	s.m_EliteStandard.aerobic_efficiency_factor = 0.9;
	s.m_EliteStandard.anaerobic_efficiency_factor = 1.2;
	s.m_EliteStandard.recovery_nonlinear_coeff = 0.3416094715094934;
	s.m_EliteStandard.fast_recovery_multiplier = 1.3173894973514577;
	s.m_EliteStandard.medium_recovery_multiplier = 0.9929312135762477;
	s.m_EliteStandard.slow_recovery_multiplier = 0.3726098785949407;
	s.m_EliteStandard.marginal_decay_threshold = 0.8;
	s.m_EliteStandard.marginal_decay_coeff = 1.1;
	s.m_EliteStandard.min_recovery_stamina_threshold = 0.2;
	s.m_EliteStandard.min_recovery_rest_time_seconds = 3.0;
	s.m_EliteStandard.sprint_speed_boost = 0.20580506236017265;
	s.m_EliteStandard.sprint_velocity_threshold = 5.5;
	s.m_EliteStandard.posture_crouch_multiplier = 3.3381462669763993;
	s.m_EliteStandard.posture_prone_multiplier = 3.4004363459928775;
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
	s.m_EliteStandard.crouching_recovery_multiplier = 1.4944690733801813;
	s.m_EliteStandard.willpower_threshold = 0.338359114866622;
	s.m_EliteStandard.sprint_enable_threshold = 0.23501672497827122;
	ApplyV6TierCpDefaults(s.m_EliteStandard, 0);
	s.m_EliteStandard.critical_power_watts = 985.3636094279169;
	s.m_EliteStandard.sprint_power_cap_watts = 2643.8900304912536;
	s.m_EliteStandard.v5_run_speed_ms = 2.8;
	s.m_EliteStandard.v5_sprint_speed_ms = 4.5;
	s.m_EliteStandard.v5_walk_speed_ms = 1.4;
	s.m_EliteStandard.w_prime_max_joules = 2.9395260758997149e+04;
	s.m_EliteStandard.w_prime_recovery_w_per_s = 10.051652902339022;
}








    
            // 初始化 StandardMilsim 预设默认值
    static void InitStandardMilsimDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// StandardMilsim — v6 optimizer merge
	// 战斗/恢复折中 → 拟真与可玩性平衡
	// metrics: ease=0.8169 recovery=0.000996 realism=3.0170
	s.m_StandardMilsim.energy_to_stamina_coeff = 1.5479440662173946e-07;
	s.m_StandardMilsim.base_recovery_rate = 1.0054772525050367e-04;
	s.m_StandardMilsim.standing_recovery_multiplier = 0.8239524573632725;
	s.m_StandardMilsim.prone_recovery_multiplier = 1.8879376621142272;
	s.m_StandardMilsim.load_recovery_penalty_coeff = 1.7910061985321080e-04;
	s.m_StandardMilsim.load_recovery_penalty_exponent = 2.0;
	s.m_StandardMilsim.encumbrance_speed_penalty_coeff = 0.2228270374661727;
	s.m_StandardMilsim.encumbrance_speed_penalty_exponent = 1.5;
	s.m_StandardMilsim.encumbrance_speed_penalty_max = 0.75;
	s.m_StandardMilsim.encumbrance_stamina_drain_coeff = 2.285127288188333;
	s.m_StandardMilsim.load_metabolic_dampening = 0.70;
	s.m_StandardMilsim.max_recovery_per_tick = 4.1975414820828781e-04;
	s.m_StandardMilsim.sprint_stamina_drain_multiplier = 3.5;
	s.m_StandardMilsim.fatigue_accumulation_coeff = 0.015;
	s.m_StandardMilsim.fatigue_max_factor = 2.0;
	s.m_StandardMilsim.aerobic_efficiency_factor = 0.9;
	s.m_StandardMilsim.anaerobic_efficiency_factor = 1.2;
	s.m_StandardMilsim.recovery_nonlinear_coeff = 0.29162448043819417;
	s.m_StandardMilsim.fast_recovery_multiplier = 1.486906644372012;
	s.m_StandardMilsim.medium_recovery_multiplier = 1.0300074279296594;
	s.m_StandardMilsim.slow_recovery_multiplier = 0.41354553549279977;
	s.m_StandardMilsim.marginal_decay_threshold = 0.8;
	s.m_StandardMilsim.marginal_decay_coeff = 1.1;
	s.m_StandardMilsim.min_recovery_stamina_threshold = 0.2;
	s.m_StandardMilsim.min_recovery_rest_time_seconds = 3.0;
	s.m_StandardMilsim.sprint_speed_boost = 0.21157806314979427;
	s.m_StandardMilsim.sprint_velocity_threshold = 5.5;
	s.m_StandardMilsim.posture_crouch_multiplier = 3.4519279354233094;
	s.m_StandardMilsim.posture_prone_multiplier = 3.8526744031461146;
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
	s.m_StandardMilsim.crouching_recovery_multiplier = 1.557250550485081;
	s.m_StandardMilsim.willpower_threshold = 0.39409453118282983;
	s.m_StandardMilsim.sprint_enable_threshold = 0.18872669767304578;
	ApplyV6TierCpDefaults(s.m_StandardMilsim, 1);
	s.m_StandardMilsim.critical_power_watts = 1009.6933509755557;
	s.m_StandardMilsim.sprint_power_cap_watts = 2712.811847516955;
	s.m_StandardMilsim.v5_run_speed_ms = 2.8;
	s.m_StandardMilsim.v5_sprint_speed_ms = 4.5;
	s.m_StandardMilsim.v5_walk_speed_ms = 1.4;
	s.m_StandardMilsim.w_prime_max_joules = 3.0377131963467975e+04;
	s.m_StandardMilsim.w_prime_recovery_w_per_s = 11.737827623603499;
}








    
            // 初始化 TacticalAction 预设默认值
    static void InitTacticalActionDefaults(SCR_RSS_Settings s, bool shouldInit)
{
	if (!shouldInit)
		return;

	// TacticalAction — v6 optimizer merge
	// 高 combat_ease + 高 recovery_ease → 战斗最宽容
	// metrics: ease=0.8288 recovery=0.000928 realism=3.8256
	s.m_TacticalAction.energy_to_stamina_coeff = 1.2298324574507038e-07;
	s.m_TacticalAction.base_recovery_rate = 1.2871493108319398e-04;
	s.m_TacticalAction.standing_recovery_multiplier = 0.8558548865024461;
	s.m_TacticalAction.prone_recovery_multiplier = 1.988538381752295;
	s.m_TacticalAction.load_recovery_penalty_coeff = 1.6356524148979294e-04;
	s.m_TacticalAction.load_recovery_penalty_exponent = 2.0;
	s.m_TacticalAction.encumbrance_speed_penalty_coeff = 0.2079617068218318;
	s.m_TacticalAction.encumbrance_speed_penalty_exponent = 1.5;
	s.m_TacticalAction.encumbrance_speed_penalty_max = 0.75;
	s.m_TacticalAction.encumbrance_stamina_drain_coeff = 2.2395866648217284;
	s.m_TacticalAction.load_metabolic_dampening = 0.70;
	s.m_TacticalAction.max_recovery_per_tick = 4.1989329194295799e-04;
	s.m_TacticalAction.sprint_stamina_drain_multiplier = 3.5;
	s.m_TacticalAction.fatigue_accumulation_coeff = 0.015;
	s.m_TacticalAction.fatigue_max_factor = 2.0;
	s.m_TacticalAction.aerobic_efficiency_factor = 0.9;
	s.m_TacticalAction.anaerobic_efficiency_factor = 1.2;
	s.m_TacticalAction.recovery_nonlinear_coeff = 0.3652436020339985;
	s.m_TacticalAction.fast_recovery_multiplier = 1.6263255547285405;
	s.m_TacticalAction.medium_recovery_multiplier = 1.0797717943237837;
	s.m_TacticalAction.slow_recovery_multiplier = 0.4709071192510163;
	s.m_TacticalAction.marginal_decay_threshold = 0.8;
	s.m_TacticalAction.marginal_decay_coeff = 1.1;
	s.m_TacticalAction.min_recovery_stamina_threshold = 0.2;
	s.m_TacticalAction.min_recovery_rest_time_seconds = 3.0;
	s.m_TacticalAction.sprint_speed_boost = 0.2525709513148368;
	s.m_TacticalAction.sprint_velocity_threshold = 5.5;
	s.m_TacticalAction.posture_crouch_multiplier = 3.409604426119208;
	s.m_TacticalAction.posture_prone_multiplier = 3.275644185868978;
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
	s.m_TacticalAction.crouching_recovery_multiplier = 1.7539536920495202;
	s.m_TacticalAction.willpower_threshold = 0.37267064126314375;
	s.m_TacticalAction.sprint_enable_threshold = 0.23609051897357758;
	ApplyV6TierCpDefaults(s.m_TacticalAction, 2);
	s.m_TacticalAction.critical_power_watts = 1016.7126119312;
	s.m_TacticalAction.sprint_power_cap_watts = 2683.421046819574;
	s.m_TacticalAction.v5_run_speed_ms = 2.8;
	s.m_TacticalAction.v5_sprint_speed_ms = 4.5;
	s.m_TacticalAction.v5_walk_speed_ms = 1.4;
	s.m_TacticalAction.w_prime_max_joules = 3.0543645791597119e+04;
	s.m_TacticalAction.w_prime_recovery_w_per_s = 13.355441773088375;
}


    
    // 初始化 Custom 预设默认值
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
    }

    //! v6 三档 CP-W' 分化（Elite=0 Standard=1 Tactical=2）
    static void ApplyV6TierCpDefaults(SCR_RSS_Params p, int tier)
    {
        if (!p)
            return;
        ApplyV5ParamsDefaults(p);
        if (tier == 0)
        {
            p.critical_power_watts = 985.3636094279169;
            p.w_prime_max_joules = 2.9395260758997149e+04;
            p.w_prime_recovery_w_per_s = 10.051652902339022;
            p.sprint_power_cap_watts = 2643.8900304912536;
            p.v5_walk_speed_ms = 1.4;
            p.v5_run_speed_ms = 2.8;
            p.v5_sprint_speed_ms = 4.5;
            p.burst_cooldown_full_seconds = 180.0;
            p.burst_cooldown_short_seconds = 75.0;
        }
        else if (tier == 1)
        {
            p.critical_power_watts = 1009.6933509755557;
            p.w_prime_max_joules = 3.0377131963467975e+04;
            p.w_prime_recovery_w_per_s = 11.737827623603499;
            p.sprint_power_cap_watts = 2712.811847516955;
            p.v5_walk_speed_ms = 1.4;
            p.v5_run_speed_ms = 2.8;
            p.v5_sprint_speed_ms = 4.5;
            p.burst_cooldown_full_seconds = 120.0;
            p.burst_cooldown_short_seconds = 60.0;
        }
        else
        {
            p.critical_power_watts = 1016.7126119312;
            p.w_prime_max_joules = 3.0543645791597119e+04;
            p.w_prime_recovery_w_per_s = 13.355441773088375;
            p.sprint_power_cap_watts = 2683.421046819574;
            p.v5_walk_speed_ms = 1.4;
            p.v5_run_speed_ms = 2.8;
            p.v5_sprint_speed_ms = 4.5;
            p.burst_cooldown_full_seconds = 90.0;
            p.burst_cooldown_short_seconds = 45.0;
        }
        p.sustainable_watts = p.critical_power_watts;
    }

}
