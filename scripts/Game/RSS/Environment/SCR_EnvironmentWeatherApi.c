//! 环境天气 API 读取与风阻计算工具（从 SCR_EnvironmentFactor.c 拆分）
class SCR_EnvironmentWeatherApi
{
    static float CalculateRainIntensityFromAPI(TimeAndWeatherManagerEntity weatherManager)
    {
        if (!weatherManager)
            return 0.0;

        float rainIntensity = weatherManager.GetRainIntensity();
        if (rainIntensity > StaminaConstants.ENV_RAIN_INTENSITY_THRESHOLD)
            return rainIntensity;

        return CalculateRainIntensityFromStateName(weatherManager);
    }

    static float CalculateRainIntensityFromStateName(TimeAndWeatherManagerEntity weatherManager)
    {
        if (!weatherManager)
            return 0.0;

        BaseWeatherStateTransitionManager transitionManager = weatherManager.GetTransitionManager();
        if (!transitionManager)
            return 0.0;

        WeatherState currentWeatherState = transitionManager.GetCurrentState();
        if (!currentWeatherState)
            return 0.0;

        string stateName = currentWeatherState.GetStateName();
        stateName.ToLower();

        if (stateName.Contains("storm") || stateName.Contains("heavy"))
            return 0.9;
        if (stateName.Contains("rain") || stateName.Contains("shower"))
            return 0.5;
        if (stateName.Contains("drizzle") || stateName.Contains("light"))
            return 0.2;
        if (stateName.Contains("cloudy") || stateName.Contains("overcast"))
            return 0.05;

        return 0.0;
    }

    static float CalculateWindSpeedFromAPI(TimeAndWeatherManagerEntity weatherManager)
    {
        if (!weatherManager)
            return 0.0;

        return weatherManager.GetWindSpeed();
    }

    static float CalculateWindDirectionFromAPI(TimeAndWeatherManagerEntity weatherManager)
    {
        if (!weatherManager)
            return 0.0;

        return weatherManager.GetWindDirection();
    }

    static float CalculateMudFactorFromAPI(TimeAndWeatherManagerEntity weatherManager)
    {
        if (!weatherManager)
            return 0.0;

        return weatherManager.GetCurrentWaterAccumulationPuddles();
    }

    static float CalculateSurfaceWetnessFromAPI(TimeAndWeatherManagerEntity weatherManager)
    {
        if (!weatherManager)
            return 0.0;

        return weatherManager.GetCurrentWetness();
    }

    static float CalculateWindDrag(float cachedWindSpeed, float cachedWindDirection, vector playerVelocity)
    {
        if (cachedWindSpeed < StaminaConstants.ENV_WIND_SPEED_THRESHOLD)
            return 0.0;

        vector playerVel = playerVelocity;
        playerVel[1] = 0;
        float speed = playerVel.Length();
        if (speed < 0.1)
            return 0.0;

        float windDirRad = cachedWindDirection * Math.DEG2RAD;
        vector windDirVec = Vector(Math.Sin(windDirRad), 0, Math.Cos(windDirRad));
        float windProjection = vector.Dot(playerVel.Normalized(), windDirVec);

        if (windProjection > 0)
        {
            float tailwindBenefit = windProjection * cachedWindSpeed * StaminaConstants.ENV_WIND_TAILWIND_BONUS;
            return -Math.Clamp(tailwindBenefit, 0.0, 0.15);
        }

        float windResistance = Math.AbsFloat(windProjection) * cachedWindSpeed * StaminaConstants.ENV_WIND_RESISTANCE_COEFF;
        return Math.Clamp(windResistance, 0.0, 1.0);
    }
}
