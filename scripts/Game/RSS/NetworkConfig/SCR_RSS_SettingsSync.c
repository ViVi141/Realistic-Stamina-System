//! Settings/Params array sync (split from SCR_RSS_Settings.c for 64KB / ICE relief)
class SCR_RSS_SettingsSync
{
    // ==================== Full sync helpers ====================
    static void WriteParamsToArray(SCR_RSS_Params p, array<float> outArr)
    {
        if (!outArr || !p)
            return;
        outArr.Clear();

        outArr.Insert(p.energy_to_stamina_coeff);
        outArr.Insert(p.base_recovery_rate);
        outArr.Insert(p.standing_recovery_multiplier);
        outArr.Insert(p.crouching_recovery_multiplier);
        outArr.Insert(p.prone_recovery_multiplier);
        outArr.Insert(p.load_recovery_penalty_coeff);
        outArr.Insert(p.load_recovery_penalty_exponent);
        outArr.Insert(p.encumbrance_speed_penalty_coeff);
        outArr.Insert(p.encumbrance_speed_penalty_exponent);
        outArr.Insert(p.encumbrance_speed_penalty_max);
        outArr.Insert(p.encumbrance_stamina_drain_coeff);
        outArr.Insert(p.load_metabolic_dampening);
        outArr.Insert(p.max_recovery_per_tick);
        outArr.Insert(p.sprint_stamina_drain_multiplier);
        outArr.Insert(p.fatigue_accumulation_coeff);
        outArr.Insert(p.fatigue_max_factor);
        outArr.Insert(p.aerobic_efficiency_factor);
        outArr.Insert(p.anaerobic_efficiency_factor);
        outArr.Insert(p.recovery_nonlinear_coeff);
        outArr.Insert(p.fast_recovery_multiplier);
        outArr.Insert(p.medium_recovery_multiplier);
        outArr.Insert(p.slow_recovery_multiplier);
        outArr.Insert(p.marginal_decay_threshold);
        outArr.Insert(p.marginal_decay_coeff);
        outArr.Insert(p.min_recovery_stamina_threshold);
        outArr.Insert(p.min_recovery_rest_time_seconds);
        outArr.Insert(p.sprint_speed_boost);
        outArr.Insert(p.sprint_velocity_threshold);
        outArr.Insert(p.willpower_threshold);
        outArr.Insert(p.sprint_enable_threshold);
        outArr.Insert(p.posture_crouch_multiplier);
        outArr.Insert(p.posture_prone_multiplier);
        outArr.Insert(p.jump_efficiency);
        outArr.Insert(p.jump_height_guess);
        outArr.Insert(p.jump_horizontal_speed_guess);
        outArr.Insert(p.climb_iso_efficiency);
        outArr.Insert(p.slope_uphill_coeff);
        outArr.Insert(p.slope_downhill_coeff);
        outArr.Insert(p.swimming_base_power);
        outArr.Insert(p.swimming_encumbrance_threshold);
        outArr.Insert(p.swimming_static_drain_multiplier);
        outArr.Insert(p.swimming_dynamic_power_efficiency);
        outArr.Insert(p.swimming_energy_to_stamina_coeff);
        outArr.Insert(p.env_heat_stress_max_multiplier);
        outArr.Insert(p.env_rain_weight_max);
        outArr.Insert(p.env_wind_resistance_coeff);
        outArr.Insert(p.env_mud_penalty_max);
        outArr.Insert(p.env_temperature_heat_penalty_coeff);
        outArr.Insert(p.env_temperature_cold_recovery_penalty_coeff);
        outArr.Insert(p.env_surface_wetness_prone_penalty);
        outArr.Insert(p.sustainable_watts);
        outArr.Insert(p.v5_walk_speed_ms);
        outArr.Insert(p.v5_run_speed_ms);
        outArr.Insert(p.v5_sprint_speed_ms);
        outArr.Insert(p.anaerobic_sprint_enable_threshold);
        outArr.Insert(p.burst_cooldown_full_seconds);
        outArr.Insert(p.burst_cooldown_short_seconds);
        outArr.Insert(p.anaerobic_drain_per_sec);
        outArr.Insert(p.anaerobic_recovery_per_sec);
        outArr.Insert(p.critical_power_watts);
        outArr.Insert(p.w_prime_max_joules);
        outArr.Insert(p.w_prime_recovery_w_per_s);
        outArr.Insert(p.sprint_power_cap_watts);
    }

    static void ApplyParamsFromArray(SCR_RSS_Params p, array<float> values)
    {
        if (!p || !values)
            return;

        // 旧版 Custom 同步包（49 floats）：在站姿与趴姿之间插入蹲姿恢复默认值
        if (values.Count() == SCR_RSS_Settings.PARAMS_ARRAY_SIZE_LEGACY)
        {
            array<float> migrated = new array<float>();
            int k;
            for (k = 0; k < 3; k++)
                migrated.Insert(values[k]);
            float crouchDefault = 1.5;
            if (values[2] > 0.0 && values[3] > 0.0)
                crouchDefault = (values[2] + values[3]) * 0.5;
            migrated.Insert(crouchDefault);
            for (k = 3; k < SCR_RSS_Settings.PARAMS_ARRAY_SIZE_LEGACY; k++)
                migrated.Insert(values[k]);
            ApplyParamsFromArray(p, migrated);
            return;
        }

        if (values.Count() == 50)
        {
            array<float> migratedV5 = new array<float>();
            int j;
            for (j = 0; j < 50; j++)
                migratedV5.Insert(values[j]);
            migratedV5.Insert(SCR_RSS_Constants.V5_SUSTAINABLE_WATTS_DEFAULT);
            migratedV5.Insert(SCR_RSS_Constants.V5_WALK_SPEED_MS_DEFAULT);
            migratedV5.Insert(SCR_RSS_Constants.V5_RUN_SPEED_MS_DEFAULT);
            migratedV5.Insert(SCR_RSS_Constants.V5_SPRINT_SPEED_MS_DEFAULT);
            migratedV5.Insert(SCR_RSS_Constants.V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT);
            migratedV5.Insert(SCR_RSS_Constants.V5_BURST_COOLDOWN_FULL_DEFAULT);
            migratedV5.Insert(SCR_RSS_Constants.V5_BURST_COOLDOWN_SHORT_DEFAULT);
            migratedV5.Insert(0.12);
            migratedV5.Insert(SCR_RSS_Constants.V5_ANAEROBIC_RECOVERY_PER_SEC_DEFAULT);
            ApplyParamsFromArray(p, migratedV5);
            return;
        }

        if (values.Count() == SCR_RSS_Settings.PARAMS_ARRAY_SIZE_V5)
        {
            array<float> migratedV6 = new array<float>();
            int m;
            for (m = 0; m < SCR_RSS_Settings.PARAMS_ARRAY_SIZE_V5; m++)
                migratedV6.Insert(values[m]);
            migratedV6.Insert(SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT);
            migratedV6.Insert(SCR_RSS_Constants.V6_W_PRIME_MAX_JOULES_DEFAULT);
            migratedV6.Insert(SCR_RSS_Constants.V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT);
            migratedV6.Insert(SCR_RSS_Constants.V6_SPRINT_POWER_CAP_WATTS_DEFAULT);
            ApplyParamsFromArray(p, migratedV6);
            return;
        }

        if (values.Count() < SCR_RSS_Settings.PARAMS_ARRAY_SIZE)
            return;

        int i = 0;
        p.energy_to_stamina_coeff = values[i++];
        p.base_recovery_rate = values[i++];
        p.standing_recovery_multiplier = values[i++];
        p.crouching_recovery_multiplier = values[i++];
        p.prone_recovery_multiplier = values[i++];
        p.load_recovery_penalty_coeff = values[i++];
        p.load_recovery_penalty_exponent = values[i++];
        p.encumbrance_speed_penalty_coeff = values[i++];
        p.encumbrance_speed_penalty_exponent = values[i++];
        p.encumbrance_speed_penalty_max = values[i++];
        p.encumbrance_stamina_drain_coeff = values[i++];
        p.load_metabolic_dampening = values[i++];
        p.max_recovery_per_tick = values[i++];
        p.sprint_stamina_drain_multiplier = values[i++];
        p.fatigue_accumulation_coeff = values[i++];
        p.fatigue_max_factor = values[i++];
        p.aerobic_efficiency_factor = values[i++];
        p.anaerobic_efficiency_factor = values[i++];
        p.recovery_nonlinear_coeff = values[i++];
        p.fast_recovery_multiplier = values[i++];
        p.medium_recovery_multiplier = values[i++];
        p.slow_recovery_multiplier = values[i++];
        p.marginal_decay_threshold = values[i++];
        p.marginal_decay_coeff = values[i++];
        p.min_recovery_stamina_threshold = values[i++];
        p.min_recovery_rest_time_seconds = values[i++];
        p.sprint_speed_boost = values[i++];
        p.sprint_velocity_threshold = values[i++];
        p.willpower_threshold = values[i++];
        p.sprint_enable_threshold = values[i++];
        p.posture_crouch_multiplier = values[i++];
        p.posture_prone_multiplier = values[i++];
        p.jump_efficiency = values[i++];
        p.jump_height_guess = values[i++];
        p.jump_horizontal_speed_guess = values[i++];
        p.climb_iso_efficiency = values[i++];
        p.slope_uphill_coeff = values[i++];
        p.slope_downhill_coeff = values[i++];
        p.swimming_base_power = values[i++];
        p.swimming_encumbrance_threshold = values[i++];
        p.swimming_static_drain_multiplier = values[i++];
        p.swimming_dynamic_power_efficiency = values[i++];
        p.swimming_energy_to_stamina_coeff = values[i++];
        p.env_heat_stress_max_multiplier = values[i++];
        p.env_rain_weight_max = values[i++];
        p.env_wind_resistance_coeff = values[i++];
        p.env_mud_penalty_max = values[i++];
        p.env_temperature_heat_penalty_coeff = values[i++];
        p.env_temperature_cold_recovery_penalty_coeff = values[i++];
        p.env_surface_wetness_prone_penalty = values[i++];
        p.sustainable_watts = values[i++];
        p.v5_walk_speed_ms = values[i++];
        p.v5_run_speed_ms = values[i++];
        p.v5_sprint_speed_ms = values[i++];
        p.anaerobic_sprint_enable_threshold = values[i++];
        p.burst_cooldown_full_seconds = values[i++];
        p.burst_cooldown_short_seconds = values[i++];
        p.anaerobic_drain_per_sec = values[i++];
        p.anaerobic_recovery_per_sec = values[i++];
        if (values.Count() >= SCR_RSS_Settings.PARAMS_ARRAY_SIZE)
        {
            p.critical_power_watts = values[i++];
            p.w_prime_max_joules = values[i++];
            p.w_prime_recovery_w_per_s = values[i++];
            p.sprint_power_cap_watts = values[i++];
        }
    }

    static void WriteSettingsToArrays(SCR_RSS_Settings s, array<float> outFloats, array<int> outInts, array<bool> outBools)
    {
        if (!outFloats || !outInts || !outBools || !s)
            return;
        outFloats.Clear();
        outInts.Clear();
        outBools.Clear();

        outFloats.Insert(s.m_fHintDuration);
        outFloats.Insert(s.m_fStaminaDrainMultiplier);
        outFloats.Insert(s.m_fStaminaRecoveryMultiplier);
        outFloats.Insert(s.m_fEncumbranceSpeedPenaltyMultiplier);
        outFloats.Insert(s.m_fSprintSpeedMultiplier);
        outFloats.Insert(s.m_fSprintStaminaDrainMultiplier);
        outFloats.Insert(s.m_fTempUpdateInterval);
        outFloats.Insert(s.m_fTemperatureMixingHeight);
        outFloats.Insert(s.m_fAlbedo);
        outFloats.Insert(s.m_fAerosolOpticalDepth);
        outFloats.Insert(s.m_fSurfaceEmissivity);
        outFloats.Insert(s.m_fCloudBlockingCoeff);
        outFloats.Insert(s.m_fLECoef);
        outFloats.Insert(s.m_fLongitude);
        outFloats.Insert(s.m_fTimeZoneOffsetHours);
        outFloats.Insert(s.m_fAltitudeMeters);
        outFloats.Insert(s.m_fFogDensity);

        outInts.Insert(s.m_iDebugUpdateInterval);
        outInts.Insert(s.m_iHintUpdateInterval);
        outInts.Insert(s.m_iTerrainUpdateInterval);
        outInts.Insert(s.m_iEnvironmentUpdateInterval);
        outInts.Insert(s.m_iDataExportIntervalMs);

        outBools.Insert(s.m_bDebugLogEnabled);
        outBools.Insert(s.m_bVerboseLogging);
        outBools.Insert(s.m_bLogToFile);
        outBools.Insert(s.m_bHintDisplayEnabled);
        outBools.Insert(s.m_bEnableHeatStress);
        outBools.Insert(s.m_bEnableRainWeight);
        outBools.Insert(s.m_bEnableWindResistance);
        outBools.Insert(s.m_bEnableMudPenalty);
        outBools.Insert(s.m_bUseEngineTemperature);
        outBools.Insert(s.m_bUseEngineTimezone);
        outBools.Insert(s.m_bMapOverWater);
        outBools.Insert(s.m_bEnableFatigueSystem);
        outBools.Insert(s.m_bEnableMetabolicAdaptation);
        outBools.Insert(s.m_bEnableIndoorDetection);
        outBools.Insert(s.m_bDataExportEnabled);
        outBools.Insert(s.m_bEnableMudSlipMechanism);
        outBools.Insert(s.m_bEnableAIStaminaCombatEffects);
        outBools.Insert(s.m_bDisableAIAllCalc);
        outBools.Insert(s.m_bDisableAIStaminaCalc);
    }

    static void ApplySettingsFromArrays(SCR_RSS_Settings s, array<float> floats, array<int> ints, array<bool> bools)
    {
        if (!s)
            return;

        if (floats && floats.Count() >= SCR_RSS_Settings.SETTINGS_FLOATS_SIZE)
        {
            int fi = 0;
            s.m_fHintDuration = floats[fi++];
            s.m_fStaminaDrainMultiplier = floats[fi++];
            s.m_fStaminaRecoveryMultiplier = floats[fi++];
            s.m_fEncumbranceSpeedPenaltyMultiplier = floats[fi++];
            s.m_fSprintSpeedMultiplier = floats[fi++];
            s.m_fSprintStaminaDrainMultiplier = floats[fi++];
            s.m_fTempUpdateInterval = floats[fi++];
            s.m_fTemperatureMixingHeight = floats[fi++];
            s.m_fAlbedo = floats[fi++];
            s.m_fAerosolOpticalDepth = floats[fi++];
            s.m_fSurfaceEmissivity = floats[fi++];
            s.m_fCloudBlockingCoeff = floats[fi++];
            s.m_fLECoef = floats[fi++];
            s.m_fLongitude = floats[fi++];
            s.m_fTimeZoneOffsetHours = floats[fi++];
            s.m_fAltitudeMeters = floats[fi++];
            s.m_fFogDensity = floats[fi++];
        }

        if (ints && ints.Count() >= SCR_RSS_Settings.SETTINGS_INTS_SIZE)
        {
            int ii = 0;
            s.m_iDebugUpdateInterval = ints[ii++];
            s.m_iHintUpdateInterval = ints[ii++];
            s.m_iTerrainUpdateInterval = ints[ii++];
            s.m_iEnvironmentUpdateInterval = ints[ii++];
            s.m_iDataExportIntervalMs = ints[ii++];
        }

        if (bools && bools.Count() >= SCR_RSS_Settings.SETTINGS_BOOLS_SIZE)
        {
            int bi = 0;
            s.m_bDebugLogEnabled = bools[bi++];
            s.m_bVerboseLogging = bools[bi++];
            s.m_bLogToFile = bools[bi++];
            s.m_bHintDisplayEnabled = bools[bi++];
            s.m_bEnableHeatStress = bools[bi++];
            s.m_bEnableRainWeight = bools[bi++];
            s.m_bEnableWindResistance = bools[bi++];
            s.m_bEnableMudPenalty = bools[bi++];
            s.m_bUseEngineTemperature = bools[bi++];
            s.m_bUseEngineTimezone = bools[bi++];
            s.m_bMapOverWater = bools[bi++];
            s.m_bEnableFatigueSystem = bools[bi++];
            s.m_bEnableMetabolicAdaptation = bools[bi++];
            s.m_bEnableIndoorDetection = bools[bi++];
            s.m_bDataExportEnabled = bools[bi++];
            s.m_bEnableMudSlipMechanism = bools[bi++];
            s.m_bEnableAIStaminaCombatEffects = bools[bi++];
            s.m_bDisableAIAllCalc = bools[bi++];
            s.m_bDisableAIStaminaCalc = bools[bi++];
        }
        else if (bools && bools.Count() >= SCR_RSS_Settings.SETTINGS_BOOLS_SIZE_LEGACY_17)
        {
            int bi = 0;
            // ... (identical to above 17 bools, omit 2 new ones)
            s.m_bDebugLogEnabled = bools[bi++];
            s.m_bVerboseLogging = bools[bi++];
            s.m_bLogToFile = bools[bi++];
            s.m_bHintDisplayEnabled = bools[bi++];
            s.m_bEnableHeatStress = bools[bi++];
            s.m_bEnableRainWeight = bools[bi++];
            s.m_bEnableWindResistance = bools[bi++];
            s.m_bEnableMudPenalty = bools[bi++];
            s.m_bUseEngineTemperature = bools[bi++];
            s.m_bUseEngineTimezone = bools[bi++];
            s.m_bMapOverWater = bools[bi++];
            s.m_bEnableFatigueSystem = bools[bi++];
            s.m_bEnableMetabolicAdaptation = bools[bi++];
            s.m_bEnableIndoorDetection = bools[bi++];
            s.m_bDataExportEnabled = bools[bi++];
            s.m_bEnableMudSlipMechanism = bools[bi++];
            s.m_bEnableAIStaminaCombatEffects = bools[bi++];
            s.m_bDisableAIAllCalc = false;
            s.m_bDisableAIStaminaCalc = false;
        }
        else if (bools && bools.Count() >= SCR_RSS_Settings.SETTINGS_BOOLS_SIZE_LEGACY_16)
        {
            int bi = 0;
            s.m_bDebugLogEnabled = bools[bi++];
            s.m_bVerboseLogging = bools[bi++];
            s.m_bLogToFile = bools[bi++];
            s.m_bHintDisplayEnabled = bools[bi++];
            s.m_bEnableHeatStress = bools[bi++];
            s.m_bEnableRainWeight = bools[bi++];
            s.m_bEnableWindResistance = bools[bi++];
            s.m_bEnableMudPenalty = bools[bi++];
            s.m_bUseEngineTemperature = bools[bi++];
            s.m_bUseEngineTimezone = bools[bi++];
            s.m_bMapOverWater = bools[bi++];
            s.m_bEnableFatigueSystem = bools[bi++];
            s.m_bEnableMetabolicAdaptation = bools[bi++];
            s.m_bEnableIndoorDetection = bools[bi++];
            s.m_bDataExportEnabled = bools[bi++];
            s.m_bEnableMudSlipMechanism = bools[bi++];
            s.m_bEnableAIStaminaCombatEffects = false;
            s.m_bDisableAIAllCalc = false;
            s.m_bDisableAIStaminaCalc = false;
        }
        else if (bools && bools.Count() >= SCR_RSS_Settings.SETTINGS_BOOLS_SIZE_LEGACY)
        {
            int bi = 0;
            s.m_bDebugLogEnabled = bools[bi++];
            s.m_bVerboseLogging = bools[bi++];
            s.m_bLogToFile = bools[bi++];
            s.m_bHintDisplayEnabled = bools[bi++];
            s.m_bEnableHeatStress = bools[bi++];
            s.m_bEnableRainWeight = bools[bi++];
            s.m_bEnableWindResistance = bools[bi++];
            s.m_bEnableMudPenalty = bools[bi++];
            s.m_bUseEngineTemperature = bools[bi++];
            s.m_bUseEngineTimezone = bools[bi++];
            s.m_bMapOverWater = bools[bi++];
            s.m_bEnableFatigueSystem = bools[bi++];
            s.m_bEnableMetabolicAdaptation = bools[bi++];
            s.m_bEnableIndoorDetection = bools[bi++];
            s.m_bDataExportEnabled = bools[bi++];
            s.m_bEnableMudSlipMechanism = false;
            s.m_bEnableAIStaminaCombatEffects = false;
            s.m_bDisableAIAllCalc = false;
            s.m_bDisableAIStaminaCalc = false;
        }

        // 数据导出开关仅服务器生效，客户端收到配置后强制关闭，避免客户端写文件
        if (!Replication.IsServer())
        {
            SCR_RSS_ConfigManager.SetServerDataExportEnabled(s.m_bDataExportEnabled);  // 先保存服务器意图，再关闭本地导出
            s.m_bDataExportEnabled = false;
        }
    }
}
