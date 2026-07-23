//! 环境因子 verbose 调试输出（从 SCR_RSS_EnvironmentFactor.c 拆分以控制文件体积）

class RSS_EnvironmentDebugSnapshot
{
    float m_fRainIntensity;
    float m_fWindSpeed;
    float m_fWindDirection;
    float m_fWindDrag;
    float m_fMudFactor;
    float m_fTemperature;
    float m_fSurfaceWetness;
    float m_fRainWeight;
    float m_fRainBreathingPenalty;
    float m_fMudTerrainFactor;
    float m_fMudSprintPenalty;
    float m_fSlipRisk;
    float m_fHeatStressPenalty;
    float m_fColdStressPenalty;
    float m_fColdStaticPenalty;
    float m_fSurfaceWetnessPenalty;
}

class SCR_RSS_EnvironmentDebug
{
    //! 高级环境因子 verbose 日志（仅 debug batch verbose 时调用）
    static void LogAdvancedFactors(RSS_EnvironmentDebugSnapshot snap, bool useEngineWeather, TimeAndWeatherManagerEntity weatherManager)
    {
        PrintFormat("[RSS] 高级环境因子 / Advanced Environment Factors:");
        PrintFormat("  降雨强度 / Rain Intensity: %1 (%2%%)",
            Math.Round(snap.m_fRainIntensity * 100.0) / 100.0,
            Math.Round(snap.m_fRainIntensity * 100.0).ToString());
        PrintFormat("  风速 / Wind Speed: %1 m/s", Math.Round(snap.m_fWindSpeed * 10.0) / 10.0);
        PrintFormat("  风向 / Wind Direction: %1°", Math.Round(snap.m_fWindDirection));
        PrintFormat("  风阻系数 / Wind Drag: %1", Math.Round(snap.m_fWindDrag * 100.0) / 100.0);
        PrintFormat("  泥泞度 / Mud Factor: %1 (%2%%)",
            Math.Round(snap.m_fMudFactor * 100.0) / 100.0,
            Math.Round(snap.m_fMudFactor * 100.0).ToString());

        string tempSource = "simulated";
        if (useEngineWeather && weatherManager)
            tempSource = "engine";

        float tempMinDbg = 0.0;
        float tempMaxDbg = 0.0;
        if (weatherManager)
        {
            tempMinDbg = weatherManager.GetTemperatureAirMinOverride();
            tempMaxDbg = weatherManager.GetTemperatureAirMaxOverride();
        }

        PrintFormat("  Temperature: Current=%1°C (source=%2) | Min=%3 | Max=%4",
            Math.Round(snap.m_fTemperature * 10.0) / 10.0,
            tempSource,
            Math.Round(tempMinDbg * 10.0) / 10.0,
            Math.Round(tempMaxDbg * 10.0) / 10.0);

        PrintFormat("  地表湿度 / Surface Wetness: %1 (%2%%)",
            Math.Round(snap.m_fSurfaceWetness * 100.0) / 100.0,
            Math.Round(snap.m_fSurfaceWetness * 100.0).ToString());
        PrintFormat("  降雨湿重 / Rain Weight: %1 kg", Math.Round(snap.m_fRainWeight * 10.0) / 10.0);
        PrintFormat("  暴雨呼吸阻力 / Rain Breathing Penalty: %1", Math.Round(snap.m_fRainBreathingPenalty * 10000.0) / 10000.0);
        PrintFormat("  泥泞地形系数 / Mud Terrain Factor: %1", Math.Round(snap.m_fMudTerrainFactor * 100.0) / 100.0);
        PrintFormat("  泥泞Sprint惩罚 / Mud Sprint Penalty: %1", Math.Round(snap.m_fMudSprintPenalty * 100.0) / 100.0);
        PrintFormat("  滑倒风险 / Slip Risk: %1", Math.Round(snap.m_fSlipRisk * 10000.0) / 10000.0);
        PrintFormat("  热应激惩罚 / Heat Stress Penalty: %1", Math.Round(snap.m_fHeatStressPenalty * 100.0) / 100.0);
        PrintFormat("  冷应激惩罚 / Cold Stress Penalty: %1", Math.Round(snap.m_fColdStressPenalty * 100.0) / 100.0);
        PrintFormat("  冷应激静态惩罚 / Cold Static Penalty: %1", Math.Round(snap.m_fColdStaticPenalty * 100.0) / 100.0);
        PrintFormat("  地表湿度惩罚 / Surface Wetness Penalty: %1", Math.Round(snap.m_fSurfaceWetnessPenalty * 100.0) / 100.0);
    }

    //! Initialize 时的天气管理器调试输出
    static void LogInitWeatherDebug(
        TimeAndWeatherManagerEntity weatherManager,
        bool useEngineTemperature,
        bool useEngineTimezone,
        float longitude,
        float timeZoneOffsetHours)
    {
        if (!weatherManager)
            return;
        if (!Replication.IsServer())
            return;
        if (!SCR_RSS_ConfigBridge.IsDebugEnabled())
            return;

        bool overrideTemp = weatherManager.GetOverrideTemperature();
        float tempMin = weatherManager.GetTemperatureAirMinOverride();
        float tempMax = weatherManager.GetTemperatureAirMaxOverride();
        float wetness = weatherManager.GetCurrentWetness();
        float rain = weatherManager.GetRainIntensity();
        float wind = weatherManager.GetWindSpeed();
        float tod = weatherManager.GetTimeOfTheDay();

        string useEngineTempStr = "false";
        if (useEngineTemperature)
            useEngineTempStr = "true";

        string useEngineTzStr = "false";
        if (useEngineTimezone)
            useEngineTzStr = "true";

        string extras = useEngineTempStr + " | " + useEngineTzStr
            + " | Lon=" + (Math.Round(longitude * 10.0) / 10.0)
            + " | TZOff=" + (Math.Round(timeZoneOffsetHours * 10.0) / 10.0);

        PrintFormat("[RSS][WeatherDebug] OverrideTemp=%1 | TempMin=%2 | TempMax=%3 | Wetness=%4 | Rain=%5 | Wind=%6 | TimeOfDay=%7 | Server=true | Extras=%8",
            overrideTemp,
            Math.Round(tempMin * 10.0) / 10.0,
            Math.Round(tempMax * 10.0) / 10.0,
            Math.Round(wetness * 100.0) / 100.0,
            Math.Round(rain * 100.0) / 100.0,
            Math.Round(wind * 10.0) / 10.0,
            Math.Round(tod * 10.0) / 10.0,
            extras);
    }
}
