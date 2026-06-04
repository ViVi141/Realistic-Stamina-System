//! 环境惩罚计算工具（从 SCR_EnvironmentFactor.c 拆分）
class SCR_EnvironmentPenaltyMath
{
    static float CalculateRainBreathingPenalty(float rainIntensity)
    {
        if (rainIntensity < StaminaEnvironmentAdvConstants.ENV_RAIN_INTENSITY_HEAVY_THRESHOLD)
            return 0.0;

        return StaminaEnvironmentAdvConstants.ENV_RAIN_INTENSITY_BREATHING_PENALTY
            * (rainIntensity - StaminaEnvironmentAdvConstants.ENV_RAIN_INTENSITY_HEAVY_THRESHOLD);
    }

    static float CalculateMudTerrainFactor(float terrainFactor, float mudFactor)
    {
        if (terrainFactor <= 1.0)
            return 0.0;

        return mudFactor * StaminaEnvironmentAdvConstants.ENV_MUD_PENALTY_MAX;
    }

    static float CalculateMudSprintPenalty(float mudFactor)
    {
        if (mudFactor < StaminaEnvironmentAdvConstants.ENV_MUD_SLIPPERY_THRESHOLD)
            return 0.0;

        return StaminaEnvironmentAdvConstants.ENV_MUD_SPRINT_PENALTY * mudFactor;
    }

    static float CalculateSlipRisk(float mudFactor)
    {
        if (!StaminaConfigBridge.IsMudSlipMechanismEnabled())
            return 0.0;
        if (mudFactor < StaminaEnvironmentAdvConstants.ENV_MUD_SLIPPERY_THRESHOLD)
            return 0.0;

        return StaminaEnvironmentAdvConstants.ENV_MUD_SLIP_RISK_BASE * mudFactor;
    }

    static float CalculateHeatStressPenalty(float temperature)
    {
        if (temperature <= StaminaConstants.ENV_TEMPERATURE_HEAT_THRESHOLD)
            return 0.0;

        float heatPenaltyCoeff = StaminaConfigBridge.GetEnvTemperatureHeatPenaltyCoeff();
        return (temperature - StaminaConstants.ENV_TEMPERATURE_HEAT_THRESHOLD) * heatPenaltyCoeff;
    }

    static void CalculateColdStressPenalty(float temperature, out float coldStressPenalty, out float coldStaticPenalty)
    {
        if (temperature >= StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD)
        {
            coldStressPenalty = 0.0;
            coldStaticPenalty = 0.0;
            return;
        }

        float coldRecoveryPenaltyCoeff = StaminaConfigBridge.GetEnvTemperatureColdRecoveryPenaltyCoeff();
        coldStressPenalty = (StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD - temperature) * coldRecoveryPenaltyCoeff;
        coldStaticPenalty = (StaminaConstants.ENV_TEMPERATURE_COLD_THRESHOLD - temperature) * StaminaConstants.ENV_TEMPERATURE_COLD_STATIC_PENALTY;
    }

    static float CalculateSurfaceWetnessPenalty(float surfaceWetness, int stance)
    {
        if (stance != 2)
            return 0.0;
        if (surfaceWetness < StaminaEnvironmentAdvConstants.ENV_SURFACE_WETNESS_THRESHOLD)
            return 0.0;

        float surfaceWetnessPenaltyMax = StaminaConfigBridge.GetEnvSurfaceWetnessPenaltyMax();
        return surfaceWetnessPenaltyMax * surfaceWetness;
    }

    static float AdjustEnergyForTemperature(float basePower, float temperature, float windSpeed)
    {
        float tEff = temperature - 1.35 * Math.Sqrt(windSpeed);
        float extraWatts = 0.0;
        const float T_LOW = 18.0;
        const float T_HIGH = 27.0;

        if (tEff < T_LOW)
        {
            float dt = T_LOW - tEff;
            extraWatts = 0.15 * (dt * dt);
        }
        else if (tEff > T_HIGH)
        {
            float dtHot = tEff - T_HIGH;
            extraWatts = 2.0 * (dtHot * dtHot);
        }

        float coeff = StaminaConfigBridge.GetEnergyToStaminaCoeff();
        float extraPerTick = extraWatts * coeff * 0.2;
        return basePower + extraPerTick;
    }
}
