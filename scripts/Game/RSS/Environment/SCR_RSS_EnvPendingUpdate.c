//! Pending force-update + weather snapshot sync (split from SCR_RSS_EnvironmentFactor.c)

class SCR_RSS_EnvPendingUpdate
{
    protected static float s_fNextGlobalEnvLogTime = 0.0;

    static void SyncWeatherCacheFromSnapshot(
        RSS_WeatherSnapshot syncSnap,
        inout float lastKnownTOD,
        inout int lastKnownYear,
        inout int lastKnownMonth,
        inout int lastKnownDay,
        inout float lastKnownRainIntensity,
        inout float lastKnownWindSpeed,
        inout bool lastKnownOverrideTemperature,
        inout float lastKnownSunriseHour,
        inout float lastKnownSunsetHour)
    {
        if (!syncSnap)
            return;

        lastKnownTOD = syncSnap.tod;
        lastKnownYear = syncSnap.year;
        lastKnownMonth = syncSnap.month;
        lastKnownDay = syncSnap.day;
        lastKnownRainIntensity = syncSnap.rainIntensity;
        lastKnownWindSpeed = syncSnap.windSpeed;
        lastKnownOverrideTemperature = syncSnap.overrideTemp;
        if (syncSnap.hasSunrise)
            lastKnownSunriseHour = syncSnap.sunriseHour;
        if (syncSnap.hasSunset)
            lastKnownSunsetHour = syncSnap.sunsetHour;
    }

    static void ApplyPendingForceTemperature(
        TimeAndWeatherManagerEntity weatherManager,
        float latitude,
        float fogDensity,
        float tod,
        float cloud,
        float rain,
        float altM,
        inout bool pendingForceUpdate,
        inout float cachedSurfaceTemperature,
        inout float cachedTemperature,
        inout float nextForceUpdateLogTime)
    {
        if (!pendingForceUpdate)
            return;

        if (weatherManager)
        {
            int year, month, day;
            weatherManager.GetDate(year, month, day);
            int n = SCR_RSS_AstronomyMath.DayOfYear(year, month, day);
            float T = SCR_RSS_AstronomyMath.CalculateUniversalTemperature(
                latitude, n, tod, altM, cloud, rain, fogDensity);
            cachedSurfaceTemperature = T;
            cachedTemperature = cachedSurfaceTemperature;
        }
        pendingForceUpdate = false;

        float tmpLogTime2 = nextForceUpdateLogTime;
        if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLogTime2))
        {
            nextForceUpdateLogTime = tmpLogTime2;
            PrintFormat("[RSS] ForceUpdate: Applied pending recompute: %1°C",
                Math.Round(cachedSurfaceTemperature * 10.0) / 10.0);
        }
    }

    static void LogEnvironmentFactorsThrottle(
        inout float nextEnvLogTime,
        float cachedTemperature,
        float heatStressMultiplier,
        float rainWeight,
        float totalWetWeight,
        float windSpeed)
    {
        // 使用全局节流：多 AI 各带一份 nextEnvLogTime 仍会刷屏。
        if (!SCR_RSS_DebugBatchManager.ShouldLog(s_fNextGlobalEnvLogTime))
            return;
        nextEnvLogTime = s_fNextGlobalEnvLogTime;

        PrintFormat("[RSS] 环境因子 / Environment Factors: 虚拟气温=%1°C | 热应激=%2x | 降雨湿重=%3kg | 总湿重=%4kg | 风速=%5m/s | Simulated Temp=%1°C | Heat Stress=%2x | Rain Weight=%3kg | Total Wet Weight=%4kg | Wind Speed=%5m/s",
            Math.Round(cachedTemperature * 10.0) / 10.0,
            Math.Round(heatStressMultiplier * 100.0) / 100.0,
            Math.Round(rainWeight * 10.0) / 10.0,
            Math.Round(totalWetWeight * 10.0) / 10.0,
            Math.Round(windSpeed * 10.0) / 10.0);
    }
}
