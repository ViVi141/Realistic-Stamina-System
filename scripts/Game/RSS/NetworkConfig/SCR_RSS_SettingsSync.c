// SCR_RSS_SettingsSync.c — Settings 序列化/反序列化
// 从 SCR_RSS_Settings.c 拆分

class SCR_RSS_SettingsSync
{
    static const int PARAMS_ARRAY_SIZE = 50;
    static const int PARAMS_ARRAY_SIZE_LEGACY = 49;
    static const int SETTINGS_FLOATS_SIZE = 17;
    static const int SETTINGS_INTS_SIZE = 5;
    static const int SETTINGS_BOOLS_SIZE = 19;
    static const int SETTINGS_BOOLS_SIZE_LEGACY_17 = 17;
    static const int SETTINGS_BOOLS_SIZE_LEGACY_16 = 16;
    static const int SETTINGS_BOOLS_SIZE_LEGACY = 15;
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
    }

    static void ApplyParamsFromArray(SCR_RSS_Params p, array<float> values)
    {
        if (!p || !values)
            return;

        // 鏃х増 Custom 鍚屾鍖咃紙49 floats锛夛細鍦ㄧ珯濮夸笌瓒村Э涔嬮棿鎻掑叆韫插Э鎭㈠榛樿鍊?
        if (values.Count() == PARAMS_ARRAY_SIZE_LEGACY)
        {
            array<float> migrated = new array<float>();
            int k;
            for (k = 0; k < 3; k++)
                migrated.Insert(values[k]);
            float crouchDefault = 1.5;
            if (values[2] > 0.0 && values[3] > 0.0)
                crouchDefault = (values[2] + values[3]) * 0.5;
            migrated.Insert(crouchDefault);
            for (k = 3; k < PARAMS_ARRAY_SIZE_LEGACY; k++)
                migrated.Insert(values[k]);
            ApplyParamsFromArray(p, migrated);
            return;
        }

        if (values.Count() < PARAMS_ARRAY_SIZE)
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

        if (floats && floats.Count() >= SETTINGS_FLOATS_SIZE)
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

        if (ints && ints.Count() >= SETTINGS_INTS_SIZE)
        {
            int ii = 0;
            s.m_iDebugUpdateInterval = ints[ii++];
            s.m_iHintUpdateInterval = ints[ii++];
            s.m_iTerrainUpdateInterval = ints[ii++];
            s.m_iEnvironmentUpdateInterval = ints[ii++];
            s.m_iDataExportIntervalMs = ints[ii++];
        }

        if (bools && bools.Count() >= SETTINGS_BOOLS_SIZE)
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
        else if (bools && bools.Count() >= SETTINGS_BOOLS_SIZE_LEGACY_17)
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
        else if (bools && bools.Count() >= SETTINGS_BOOLS_SIZE_LEGACY_16)
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
        else if (bools && bools.Count() >= SETTINGS_BOOLS_SIZE_LEGACY)
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

        // 鏁版嵁瀵煎嚭寮€鍏充粎鏈嶅姟鍣ㄧ敓鏁堬紝瀹㈡埛绔敹鍒伴厤缃悗寮哄埗鍏抽棴锛岄伩鍏嶅鎴风鍐欐枃浠?
        if (!Replication.IsServer())
        {
            SCR_RSS_ConfigManager.SetServerDataExportEnabled(s.m_bDataExportEnabled);  // 鍏堜繚瀛樻湇鍔″櫒鎰忓浘锛屽啀鍏抽棴鏈湴瀵煎嚭
            s.m_bDataExportEnabled = false;
        }
    }
}
