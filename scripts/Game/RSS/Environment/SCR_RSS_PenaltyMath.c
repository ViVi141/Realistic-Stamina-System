//! 环境惩罚计算工具（从 SCR_EnvironmentFactor.c 拆分）
class SCR_RSS_PenaltyMath
{
    static float CalculateRainBreathingPenalty(float rainIntensity)
    {
        if (rainIntensity < SCR_RSS_EnvConstants.ENV_RAIN_INTENSITY_HEAVY_THRESHOLD)
            return 0.0;

        return SCR_RSS_EnvConstants.ENV_RAIN_INTENSITY_BREATHING_PENALTY
            * (rainIntensity - SCR_RSS_EnvConstants.ENV_RAIN_INTENSITY_HEAVY_THRESHOLD);
    }

    static float CalculateMudTerrainFactor(float terrainFactor, float mudFactor)
    {
        if (terrainFactor <= 1.0)
            return 0.0;

        return mudFactor * SCR_RSS_EnvConstants.ENV_MUD_PENALTY_MAX;
    }

    static float CalculateMudSprintPenalty(float mudFactor)
    {
        if (mudFactor < SCR_RSS_EnvConstants.ENV_MUD_SLIPPERY_THRESHOLD)
            return 0.0;

        return SCR_RSS_EnvConstants.ENV_MUD_SPRINT_PENALTY * mudFactor;
    }

    static float CalculateSlipRisk(float mudFactor)
    {
        if (!SCR_RSS_ConfigBridge.IsMudSlipMechanismEnabled())
            return 0.0;
        if (mudFactor < SCR_RSS_EnvConstants.ENV_MUD_SLIPPERY_THRESHOLD)
            return 0.0;

        return SCR_RSS_EnvConstants.ENV_MUD_SLIP_RISK_BASE * mudFactor;
    }

    static float CalculateHeatStressPenalty(float temperature)
    {
        if (temperature <= SCR_RSS_EnvConstants.ENV_THERMONEUTRAL_HIGH)
            return 0.0;

        float heatPenaltyCoeff = SCR_RSS_ConfigBridge.GetEnvTemperatureHeatPenaltyCoeff();
        return (temperature - SCR_RSS_EnvConstants.ENV_THERMONEUTRAL_HIGH) * heatPenaltyCoeff;
    }

    static void CalculateColdStressPenalty(float temperature, out float coldStressPenalty, out float coldStaticPenalty)
    {
        if (temperature >= SCR_RSS_EnvConstants.ENV_TEMPERATURE_COLD_THRESHOLD)
        {
            coldStressPenalty = 0.0;
            coldStaticPenalty = 0.0;
            return;
        }

        float coldRecoveryPenaltyCoeff = SCR_RSS_ConfigBridge.GetEnvTemperatureColdRecoveryPenaltyCoeff();
        coldStressPenalty = (SCR_RSS_EnvConstants.ENV_TEMPERATURE_COLD_THRESHOLD - temperature) * coldRecoveryPenaltyCoeff;
        coldStaticPenalty = (SCR_RSS_EnvConstants.ENV_TEMPERATURE_COLD_THRESHOLD - temperature) * SCR_RSS_EnvConstants.ENV_TEMPERATURE_COLD_STATIC_PENALTY;
    }

    static float CalculateSurfaceWetnessPenalty(float surfaceWetness, int stance)
    {
        if (stance != 2)
            return 0.0;
        if (surfaceWetness < SCR_RSS_EnvConstants.ENV_SURFACE_WETNESS_THRESHOLD)
            return 0.0;

        float surfaceWetnessPenaltyMax = SCR_RSS_ConfigBridge.GetEnvSurfaceWetnessPenaltyMax();
        return surfaceWetnessPenaltyMax * surfaceWetness;
    }

    static float AdjustEnergyForTemperature(float basePower, float temperature, float windSpeed)
    {
        float tEff = temperature - 1.35 * Math.Sqrt(Math.Max(windSpeed, 0.0));
        float extraWatts = 0.0;
        float tLow = SCR_RSS_EnvConstants.ENV_THERMONEUTRAL_LOW;
        float tHigh = SCR_RSS_EnvConstants.ENV_THERMONEUTRAL_HIGH;

        if (tEff < tLow)
        {
            float dt = tLow - tEff;
            extraWatts = SCR_RSS_EnvConstants.ENV_COLD_STRESS_K * (dt * dt);
        }
        else if (tEff > tHigh)
        {
            float dtHot = tEff - tHigh;
            extraWatts = SCR_RSS_EnvConstants.ENV_HEAT_STRESS_K * (dtHot * dtHot);
        }

        float coeff = SCR_RSS_ConfigBridge.GetEnergyToStaminaCoeff();
        float extraPerTick = extraWatts * coeff * 0.2;
        return basePower + extraPerTick;
    }

    //! 热应激倍数（基于气温阈值 + 室内减免）
    static float CalculateHeatStressMultiplier(float currentTemp, bool isIndoor)
    {
        float heatStressThreshold = SCR_RSS_EnvConstants.ENV_THERMONEUTRAL_HIGH;
        float multiplier = 1.0;

        if (currentTemp >= heatStressThreshold)
        {
            float tempExcess = currentTemp - heatStressThreshold;
            multiplier = 1.0 + tempExcess * 0.02;
        }

        if (isIndoor)
            multiplier = multiplier * (1.0 - SCR_RSS_EnvConstants.ENV_HEAT_STRESS_INDOOR_REDUCTION);

        return Math.Clamp(multiplier, 1.0, SCR_RSS_EnvConstants.ENV_HEAT_STRESS_MAX_MULTIPLIER);
    }
}
