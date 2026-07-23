//! 降雨湿重累积/衰减（从 SCR_RSS_EnvironmentFactor.c 拆分）

class SCR_RSS_RainWetWeight
{
    //! 更新降雨湿重；通过 inout 回写状态字段
    static void Update(
        float currentTime,
        float lastUpdateTime,
        float rainIntensity,
        bool isIndoor,
        inout float rainWeight,
        inout float rainStopTime,
        inout float rainPeakWeight,
        inout float lastRainIntensity)
    {
        if (!SCR_RSS_ConfigBridge.IsRainWeightEnabled())
        {
            rainWeight = 0.0;
            rainStopTime = -1.0;
            rainPeakWeight = 0.0;
            return;
        }

        float deltaTime = currentTime - lastUpdateTime;
        if (deltaTime <= 0)
            return;

        bool isOutdoorAndRaining = !isIndoor
            && rainIntensity >= SCR_RSS_EnvConstants.ENV_RAIN_VISUAL_EFFECT_THRESHOLD;

        if (isOutdoorAndRaining)
        {
            rainStopTime = -1.0;
            rainPeakWeight = 0.0;

            float accumulationRate = SCR_RSS_EnvConstants.ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE
                * Math.Pow(rainIntensity, SCR_RSS_EnvConstants.ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT);

            rainWeight = Math.Clamp(
                rainWeight + accumulationRate * deltaTime,
                0.0,
                SCR_RSS_ConfigBridge.GetEnvRainWeightMax());
        }
        else
        {
            if (rainWeight > 0.0)
            {
                if (rainStopTime < 0.0)
                {
                    rainStopTime = currentTime;
                    rainPeakWeight = rainWeight;
                }

                float elapsed = currentTime - rainStopTime;
                float duration = SCR_RSS_EnvConstants.ENV_RAIN_WEIGHT_DURATION;
                if (elapsed >= duration)
                {
                    rainWeight = 0.0;
                    rainStopTime = -1.0;
                    rainPeakWeight = 0.0;
                    lastRainIntensity = 0.0;
                }
                else
                {
                    rainWeight = rainPeakWeight * (1.0 - elapsed / duration);
                }
            }
        }
    }
}
