//! Location estimate bootstrap (split from SCR_RSS_EnvironmentFactor.c for compile ICE relief)

class SCR_RSS_EnvLocationBootstrap
{
    //! Server+debug: log engine coords or estimate lat/lon from sunrise/sunset.
    static void TryEstimateOnInit(
        TimeAndWeatherManagerEntity weatherManager,
        float timeZoneOffsetHours,
        float cachedRainIntensity,
        float cachedSurfaceWetness,
        inout float latitude,
        inout float longitude,
        inout float nextLocationEstimateLogTime)
    {
        if (!weatherManager)
            return;
        if (!Replication.IsServer())
            return;
        if (!SCR_RSS_ConfigBridge.IsDebugEnabled())
            return;

        bool skipEstimate = false;
        float engLat = weatherManager.GetCurrentLatitude();
        float engLon = weatherManager.GetCurrentLongitude();
        if (engLat != 0.0 || engLon != 0.0)
        {
            skipEstimate = true;
            float tmpLocLog1 = nextLocationEstimateLogTime;
            if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLocLog1))
            {
                nextLocationEstimateLogTime = tmpLocLog1;
                PrintFormat("[RSS][LocationEstimate] using engine coords lat=%1 lon=%2", engLat, engLon);
            }
        }

        if (skipEstimate)
            return;

        float estLat = 0.0;
        float estLon = 0.0;
        float estConf = SCR_RSS_AstronomyMath.EstimateLatLongFromSunriseSunset(
            weatherManager, timeZoneOffsetHours,
            estLat, estLon,
            cachedRainIntensity, cachedSurfaceWetness);
        if (estConf <= 0.0)
            return;

        latitude = estLat;
        longitude = estLon;

        float tmpLocLog2 = nextLocationEstimateLogTime;
        if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLocLog2))
        {
            nextLocationEstimateLogTime = tmpLocLog2;
            PrintFormat("[RSS][LocationEstimate] Estimated Lat=%1 Lon=%2 Conf=%3 (initial)",
                Math.Round(estLat * 10.0) / 10.0,
                Math.Round(estLon * 10.0) / 10.0,
                Math.Round(estConf * 100.0) / 100.0);
        }

        if (estConf >= 0.9)
            return;

        float refinedLat = 0.0;
        float refinedLon = 0.0;
        float refinedConf = SCR_RSS_AstronomyMath.EstimateLatLongFromAstronomicalSearch(
            weatherManager, timeZoneOffsetHours,
            refinedLat, refinedLon,
            cachedRainIntensity, cachedSurfaceWetness);
        if (refinedConf <= estConf)
            return;

        latitude = refinedLat;
        longitude = refinedLon;

        float tmpLocLog3 = nextLocationEstimateLogTime;
        if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLocLog3))
        {
            nextLocationEstimateLogTime = tmpLocLog3;
            PrintFormat("[RSS][LocationEstimate] Refined Lat=%1 Lon=%2 Conf=%3 (improved)",
                Math.Round(refinedLat * 10.0) / 10.0,
                Math.Round(refinedLon * 10.0) / 10.0,
                Math.Round(refinedConf * 100.0) / 100.0);
        }
    }

    static float ApplySunriseSunsetEstimate(
        TimeAndWeatherManagerEntity weatherManager,
        float timeZoneOffsetHours,
        float cachedRainIntensity,
        float cachedSurfaceWetness,
        inout float latitude,
        inout float longitude,
        inout float nextLocationEstimateLogTime,
        out float outLatDeg,
        out float outLonDeg)
    {
        float conf = SCR_RSS_AstronomyMath.EstimateLatLongFromSunriseSunset(
            weatherManager, timeZoneOffsetHours,
            outLatDeg, outLonDeg,
            cachedRainIntensity, cachedSurfaceWetness);

        if (conf > 0.0)
        {
            latitude = outLatDeg;
            longitude = outLonDeg;
            float tmpLog = nextLocationEstimateLogTime;
            if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLog))
            {
                nextLocationEstimateLogTime = tmpLog;
                PrintFormat("[RSS] EstimateLatLong: lat=%1 lon=%2 conf=%3",
                    Math.Round(outLatDeg * 10.0) / 10.0,
                    Math.Round(outLonDeg * 10.0) / 10.0,
                    Math.Round(conf * 100.0) / 100.0);
            }
        }
        return conf;
    }

    static float ApplyAstronomicalSearchEstimate(
        TimeAndWeatherManagerEntity weatherManager,
        float timeZoneOffsetHours,
        float cachedRainIntensity,
        float cachedSurfaceWetness,
        inout float latitude,
        inout float longitude,
        inout float nextLocationEstimateLogTime,
        out float outLatDeg,
        out float outLonDeg)
    {
        float conf = SCR_RSS_AstronomyMath.EstimateLatLongFromAstronomicalSearch(
            weatherManager, timeZoneOffsetHours,
            outLatDeg, outLonDeg,
            cachedRainIntensity, cachedSurfaceWetness);

        if (conf > 0.0)
        {
            latitude = outLatDeg;
            longitude = outLonDeg;
            float tmpLog = nextLocationEstimateLogTime;
            if (SCR_RSS_DebugBatchManager.ShouldLog(tmpLog))
            {
                nextLocationEstimateLogTime = tmpLog;
                PrintFormat("[RSS] EstimateLatLongAstronomy: lat=%1 lon=%2 conf=%3",
                    Math.Round(outLatDeg * 10.0) / 10.0,
                    Math.Round(outLonDeg * 10.0) / 10.0,
                    Math.Round(conf * 100.0) / 100.0);
            }
        }
        return conf;
    }
}
