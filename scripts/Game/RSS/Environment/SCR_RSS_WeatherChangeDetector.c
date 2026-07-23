//! 引擎天气变更检测（从 SCR_RSS_EnvironmentFactor.c 拆分）

class RSS_WeatherSnapshot
{
    float tod;
    int year;
    int month;
    int day;
    float rainIntensity;
    float windSpeed;
    bool overrideTemp;
    float sunriseHour;
    float sunsetHour;
    bool hasSunrise;
    bool hasSunset;
}

class SCR_RSS_WeatherChangeDetector
{
    //! 采样当前引擎天气到快照
    static RSS_WeatherSnapshot Sample(
        TimeAndWeatherManagerEntity weatherManager,
        float tod,
        float rainIntensity,
        float windSpeed)
    {
        RSS_WeatherSnapshot snap = new RSS_WeatherSnapshot();
        snap.tod = tod;
        snap.rainIntensity = rainIntensity;
        snap.windSpeed = windSpeed;
        snap.overrideTemp = false;
        snap.hasSunrise = false;
        snap.hasSunset = false;
        snap.sunriseHour = 0.0;
        snap.sunsetHour = 0.0;
        snap.year = 0;
        snap.month = 0;
        snap.day = 0;

        if (!weatherManager)
            return snap;

        int y, mo, d;
        weatherManager.GetDate(y, mo, d);
        snap.year = y;
        snap.month = mo;
        snap.day = d;
        snap.overrideTemp = weatherManager.GetOverrideTemperature();

        float sr = 0.0;
        float ss = 0.0;
        snap.hasSunrise = weatherManager.GetSunriseHour(sr);
        snap.hasSunset = weatherManager.GetSunsetHour(ss);
        snap.sunriseHour = sr;
        snap.sunsetHour = ss;
        return snap;
    }

    //! 与上次已知值比较，判断是否需要强制更新
    static bool NeedsForceUpdate(
        RSS_WeatherSnapshot curr,
        float lastTod,
        int lastYear,
        int lastMonth,
        int lastDay,
        float lastRain,
        float lastWind,
        bool lastOverrideTemp,
        float lastSunrise,
        float lastSunset)
    {
        if (!curr)
            return false;

        if (lastTod < 0.0 || Math.AbsFloat(curr.tod - lastTod) > 0.1)
            return true;
        if (lastYear != curr.year || lastMonth != curr.month || lastDay != curr.day)
            return true;
        if (Math.AbsFloat(curr.rainIntensity - lastRain) > 0.05)
            return true;
        if (Math.AbsFloat(curr.windSpeed - lastWind) > 0.5)
            return true;
        if (curr.overrideTemp != lastOverrideTemp)
            return true;
        if (curr.hasSunrise && Math.AbsFloat(curr.sunriseHour - lastSunrise) > 0.01)
            return true;
        if (curr.hasSunset && Math.AbsFloat(curr.sunsetHour - lastSunset) > 0.01)
            return true;
        return false;
    }

    static void LogEngineWeatherBatch(RSS_WeatherSnapshot curr)
    {
        if (!curr)
            return;
        if (!Replication.IsServer())
            return;
        if (!SCR_RSS_DebugBatchManager.IsDebugBatchActive())
            return;

        string dateStr = curr.year.ToString() + "/" + curr.month.ToString() + "/" + curr.day.ToString();
        string line = string.Format("[RSS] 引擎天气: TOD=%1 雨=%2 风=%3 ovTemp=%4 日出=%5 日落=%6 日期=%7",
            curr.tod, curr.rainIntensity, curr.windSpeed, curr.overrideTemp,
            curr.sunriseHour, curr.sunsetHour, dateStr);
        SCR_RSS_DebugBatchManager.AddDebugBatchLineOnce("EngineTOD", line);
    }

    static void LogZeroCoordinateThrottle(inout float nextLogTime, float lat, float lon)
    {
        if (lat == 0.0 && lon == 0.0)
        {
            float tmp = nextLogTime;
            if (SCR_RSS_DebugBatchManager.ShouldLog(tmp))
            {
                nextLogTime = tmp;
                Print("[RSS] weather mgr returned 0/0 coordinates, delaying");
            }
            return;
        }

        static bool once = false;
        if (!once)
        {
            float tmp2 = nextLogTime;
            if (SCR_RSS_DebugBatchManager.ShouldLog(tmp2))
            {
                nextLogTime = tmp2;
                PrintFormat("[RSS] weather mgr now has coords lat=%1 lon=%2", lat, lon);
            }
            once = true;
        }
    }
}
