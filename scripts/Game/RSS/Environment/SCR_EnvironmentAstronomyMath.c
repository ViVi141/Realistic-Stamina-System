//! 环境天文/辐射计算工具（从 SCR_EnvironmentFactor.c 拆分）
class SCR_EnvironmentAstronomyMath
{
    static const float NATURAL_E = 2.718281828459045;

    static int DayOfYear(int year, int month, int day)
    {
        int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        bool isLeap = ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
        if (isLeap)
            mdays[1] = 29;

        int n = 0;
        int i = 0;
        while (i < month - 1)
        {
            n += mdays[i];
            i = i + 1;
        }
        n = n + day;
        return n;
    }

    static float SolarDeclination(int n)
    {
        return 23.44 * Math.DEG2RAD * Math.Sin(2.0 * Math.PI * (284.0 + n) / 365.0);
    }

    static float SolarCosZenith(float latDeg, int n, float localHour)
    {
        float latRad = latDeg * Math.DEG2RAD;
        float decl = SolarDeclination(n);
        float hourAngleDeg = 15.0 * (localHour - 12.0);
        float hourAngleRad = hourAngleDeg * Math.DEG2RAD;
        float cosTheta = Math.Sin(latRad) * Math.Sin(decl) + Math.Cos(latRad) * Math.Cos(decl) * Math.Cos(hourAngleRad);
        return cosTheta;
    }

    static float AirMass(float cosTheta)
    {
        if (cosTheta <= 0.0)
            return 9999.0;

        float thetaDeg = Math.Acos(cosTheta) * Math.RAD2DEG;
        float m = 1.0 / (cosTheta + 0.50572 * Math.Pow(96.07995 - thetaDeg, -1.6364));
        return m;
    }

    static float ClearSkyTransmittance(float airMass, float aerosolOpticalDepth)
    {
        float tau = Math.Pow(NATURAL_E, -aerosolOpticalDepth * airMass);
        return Math.Clamp(tau, 0.0, 1.0);
    }

    static float EstimateSeasonalAvgTemp(float lat, int dayOfYear)
    {
        float absLat = Math.AbsFloat(lat);
        float baseTemp = 27.0 - 0.4 * absLat;
        float seasonOffset = -10.0 * Math.Cos(2.0 * Math.PI * (float)(dayOfYear + 10) / 365.0);
        if (absLat > 40.0 && absLat < 60.0)
            baseTemp -= 3.0;
        return baseTemp + seasonOffset;
    }

    // 简单云因子推断，从雨强、湿度与天气状态推测云量因子 [0,1]
    // @param rainIntensity 已缓存的降雨强度 (0.0-1.0)
    // @param surfaceWetness 已缓存的地表湿度 (0.0-1.0)
    // @param weatherManager 天气管理器引用（可 null）
    // @return 云量因子 (0.0-1.0)
    static float InferCloudFactor(float rainIntensity, float surfaceWetness, TimeAndWeatherManagerEntity weatherManager)
    {
        float cloud = 0.0;
        cloud = Math.Max(cloud, rainIntensity);
        cloud = Math.Max(cloud, surfaceWetness * 0.8);

        if (weatherManager)
        {
            BaseWeatherStateTransitionManager transitionManager = weatherManager.GetTransitionManager();
            if (transitionManager)
            {
                WeatherState currentWeatherState = transitionManager.GetCurrentState();
                if (currentWeatherState)
                {
                    string s = currentWeatherState.GetStateName();
                    s.ToLower();
                    if (s.Contains("storm") || s.Contains("heavy"))
                        cloud = Math.Max(cloud, 0.95);
                    else if (s.Contains("rain") || s.Contains("shower"))
                        cloud = Math.Max(cloud, 0.6);
                    else if (s.Contains("cloud") || s.Contains("overcast"))
                        cloud = Math.Max(cloud, 0.6);
                    else if (s.Contains("partly") || s.Contains("few"))
                        cloud = Math.Max(cloud, 0.25);
                }
            }
        }

        return Math.Clamp(cloud, 0.0, 1.0);
    }

    // 估算经纬度（基于引擎提供的日出/日落时间，返回置信度 0.0-1.0）
    // @param weatherManager 天气管理器引用
    // @param timeZoneOffsetHours 时区偏移（小时）
    // @param outLatDeg [out] 估算纬度
    // @param outLonDeg [out] 估算经度
    // @param rainIntensity 已缓存的降雨强度（用于 InferCloudFactor）
    // @param surfaceWetness 已缓存的地表湿度（用于 InferCloudFactor）
    // @return 置信度 (0.0-1.0)
    static float EstimateLatLongFromSunriseSunset(
        TimeAndWeatherManagerEntity weatherManager, float timeZoneOffsetHours,
        out float outLatDeg, out float outLonDeg,
        float rainIntensity, float surfaceWetness)
    {
        outLatDeg = 0.0;
        outLonDeg = 0.0;
        if (!weatherManager) return 0.0;

        float sr = 0.0, ss = 0.0;
        if (!weatherManager.GetSunriseHour(sr) || !weatherManager.GetSunsetHour(ss))
            return 0.0;

        if (ss < sr) ss += 24.0;
        float L = ss - sr;
        if (L <= 0.0 || L >= 24.0) return 0.0;

        int year, month, day;
        weatherManager.GetDate(year, month, day);
        int n = DayOfYear(year, month, day);

        float omega0_deg = 7.5 * L;
        float omega0_rad = omega0_deg * Math.DEG2RAD;
        float decl = SolarDeclination(n);
        float tanDecl = Math.Tan(decl);
        if (Math.AbsFloat(tanDecl) < 1e-6) return 0.0;

        float tanPhi = -Math.Cos(omega0_rad) / tanDecl;
        float denom = Math.Sqrt(1.0 + tanPhi * tanPhi);
        float latRad = Math.Asin(tanPhi / denom);
        float latDeg = latRad * Math.RAD2DEG;

        float t_noon_local = (sr + ss) * 0.5;
        while (t_noon_local >= 24.0) t_noon_local -= 24.0;
        float solarNoonUTC = t_noon_local - timeZoneOffsetHours;
        float lonDeg = 15.0 * (12.0 - solarNoonUTC);
        while (lonDeg > 180.0) lonDeg -= 360.0;
        while (lonDeg < -180.0) lonDeg += 360.0;

        float cloud = InferCloudFactor(rainIntensity, surfaceWetness, weatherManager);
        float conf = 1.0;
        conf -= Math.Clamp(cloud * 0.5, 0.0, 0.5);
        conf -= Math.Clamp(1.0 - Math.AbsFloat(tanDecl) * 1000.0, 0.0, 0.3);
        if (L < 2.0 || L > 22.0) conf -= 0.3;
        conf = Math.Clamp(conf, 0.0, 1.0);

        outLatDeg = latDeg;
        outLonDeg = lonDeg;
        return conf;
    }

    // 使用引擎天文函数进行网格搜索以细化经纬度估算（返回置信度 0-1）
    // @param weatherManager 天气管理器引用
    // @param timeZoneOffsetHours 时区偏移（小时）
    // @param outLatDeg [out] 估算纬度
    // @param outLonDeg [out] 估算经度
    // @param rainIntensity 已缓存的降雨强度（用于 InferCloudFactor）
    // @param surfaceWetness 已缓存的地表湿度（用于 InferCloudFactor）
    // @return 置信度 (0.0-1.0)
    static float EstimateLatLongFromAstronomicalSearch(
        TimeAndWeatherManagerEntity weatherManager, float timeZoneOffsetHours,
        out float outLatDeg, out float outLonDeg,
        float rainIntensity, float surfaceWetness)
    {
        outLatDeg = 0.0;
        outLonDeg = 0.0;
        if (!weatherManager) return 0.0;

        float obsSR = 0.0, obsSS = 0.0;
        if (!weatherManager.GetSunriseHour(obsSR) || !weatherManager.GetSunsetHour(obsSS))
            return 0.0;

        float obsMoonPhase = weatherManager.GetMoonPhase(weatherManager.GetTimeOfTheDay());
        float tod = weatherManager.GetTimeOfTheDay();
        int year, month, day;
        weatherManager.GetDate(year, month, day);
        float tz = timeZoneOffsetHours;
        float dst = weatherManager.GetDSTOffset();

        if (obsSS < obsSR) obsSS += 24.0;
        float obsL = obsSS - obsSR;
        float obsNoon = (obsSR + obsSS) * 0.5;
        while (obsNoon >= 24.0) obsNoon -= 24.0;

        float bestErr = 1e9, bestLat = 0.0, bestLon = 0.0;
        const float wL = 1.0, wNoon = 0.5, wMoon = 0.3;

        // 粗网格（步长 5°）
        for (float lat = -85.0; lat <= 85.0; lat += 5.0)
        {
            for (float lon = -180.0; lon <= 180.0; lon += 5.0)
            {
                float sr_c = 0.0, ss_c = 0.0;
                bool okSR = weatherManager.GetSunriseHourForDate(year, month, day, lat, lon, tz, dst, sr_c);
                bool okSS = weatherManager.GetSunsetHourForDate(year, month, day, lat, lon, tz, dst, ss_c);
                float penalty = 0.0;
                if (!okSR || !okSS) penalty = 10.0;
                if (ss_c < sr_c) ss_c += 24.0;
                float Lc = ss_c - sr_c;
                float noon_c = (sr_c + ss_c) * 0.5;
                while (noon_c >= 24.0) noon_c -= 24.0;
                float moon_c = weatherManager.GetMoonPhaseForDate(year, month, day, tod, tz, dst);
                float err = wL * Math.AbsFloat(obsL - Lc) + wNoon * Math.AbsFloat(obsNoon - noon_c) + wMoon * Math.AbsFloat(obsMoonPhase - moon_c) + penalty;
                if (err < bestErr) { bestErr = err; bestLat = lat; bestLon = lon; }
            }
        }

        // 细化搜索
        float searchRadius = 5.0, step = 1.0;
        for (int iter = 0; iter < 3; iter++)
        {
            float localBestErr = bestErr, localBestLat = bestLat, localBestLon = bestLon;
            for (float lat = bestLat - searchRadius; lat <= bestLat + searchRadius; lat += step)
            {
                if (lat < -89.9 || lat > 89.9) continue;
                for (float lon = bestLon - searchRadius; lon <= bestLon + searchRadius; lon += step)
                {
                    float sr_c = 0.0, ss_c = 0.0;
                    bool okSR = weatherManager.GetSunriseHourForDate(year, month, day, lat, lon, tz, dst, sr_c);
                    bool okSS = weatherManager.GetSunsetHourForDate(year, month, day, lat, lon, tz, dst, ss_c);
                    float penalty = 0.0;
                    if (!okSR || !okSS) penalty = 10.0;
                    if (ss_c < sr_c) ss_c += 24.0;
                    float Lc = ss_c - sr_c;
                    float noon_c = (sr_c + ss_c) * 0.5;
                    while (noon_c >= 24.0) noon_c -= 24.0;
                    float moon_c = weatherManager.GetMoonPhaseForDate(year, month, day, tod, tz, dst);
                    float err = wL * Math.AbsFloat(obsL - Lc) + wNoon * Math.AbsFloat(obsNoon - noon_c) + wMoon * Math.AbsFloat(obsMoonPhase - moon_c) + penalty;
                    if (err < localBestErr) { localBestErr = err; localBestLat = lat; localBestLon = lon; }
                }
            }
            bestErr = localBestErr; bestLat = localBestLat; bestLon = localBestLon;
            searchRadius = Math.Max(0.5, searchRadius * 0.5);
            step = Math.Max(0.1, step * 0.5);
        }

        const float maxAcceptableErr = 12.0;
        float errScore = Math.Clamp(bestErr / maxAcceptableErr, 0.0, 1.0);
        float conf = 1.0 - errScore;
        float cloud = InferCloudFactor(rainIntensity, surfaceWetness, weatherManager);
        conf -= Math.Clamp(cloud * 0.5, 0.0, 0.5);
        conf = Math.Clamp(conf, 0.0, 1.0);

        outLatDeg = bestLat;
        outLonDeg = bestLon;
        return conf;
    }

    // 正弦波叠加气温模型：纯数学拟合，无辐射求解，极轻量且稳定
    // T = T_纬度基准 + T_季节偏差 + T_昼夜波动 - T_海拔衰减 + T_天气修正
    static float CalculateUniversalTemperature(float latitude, int dayOfYear, float hourOfDay, float altitudeMeters, float overcast, float rainIntensity, float fogDensity)
    {
        float latRad = latitude * Math.PI / 180.0;
        float baseTemp = 27.0 - 42.0 * Math.Pow(Math.Sin(latRad), 2.0);

        float seasonRange = 2.0 + 20.0 * Math.AbsFloat(Math.Sin(latRad));
        float yearPhase = ((float)(dayOfYear) - 15.0) / 365.0 * 2.0 * Math.PI;
        float hemisphere;
        if (latitude >= 0.0)
            hemisphere = -1.0;
        else
            hemisphere = 1.0;
        float seasonTemp = hemisphere * seasonRange * Math.Cos(yearPhase);

        const float dailyRange = 10.0;
        float damping = 1.0 - (overcast * 0.5) - (fogDensity * 0.3);
        float currentRange = dailyRange * Math.Max(0.2, damping);
        float hourPhase = (hourOfDay - 9.5) / 24.0 * 2.0 * Math.PI;
        float dailyTemp = (currentRange / 2.0) * Math.Sin(hourPhase);

        float altTemp = (altitudeMeters / 1000.0) * 6.5;

        float currentBase = baseTemp + seasonTemp + dailyTemp - altTemp;
        float rainCooling = 0.0;
        if (currentBase > 10.0 && rainIntensity > 0.0)
        {
            rainCooling = rainIntensity * 5.0 * ((currentBase - 10.0) / 20.0);
            if (rainCooling > 5.0)
                rainCooling = 5.0;
        }

        return currentBase - rainCooling;
    }
}
