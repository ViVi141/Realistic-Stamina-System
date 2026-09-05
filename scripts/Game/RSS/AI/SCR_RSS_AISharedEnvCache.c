//! AI 共用环境采样（全服每秒至多一次），避免每 AI 调 UpdateEnvironmentFactors。

class SCR_RSS_AISharedEnvCache
{
    protected static float s_fLastUpdateSec = -1000.0;
    protected static float s_fHeatStressMultiplier = 1.0;
    protected static const float UPDATE_INTERVAL_SEC = 1.0;

    static float GetHeatStressMultiplier(float nowSec)
    {
        RefreshIfNeeded(nowSec);
        return s_fHeatStressMultiplier;
    }

    protected static void RefreshIfNeeded(float nowSec)
    {
        if ((nowSec - s_fLastUpdateSec) < UPDATE_INTERVAL_SEC)
            return;
        s_fLastUpdateSec = nowSec;
        s_fHeatStressMultiplier = 1.0;

        if (!GetGame())
            return;
        World world = GetGame().GetWorld();
        if (!world)
            return;
        ChimeraWorld chimera = ChimeraWorld.CastFrom(world);
        if (!chimera)
            return;
        TimeAndWeatherManagerEntity weather = chimera.GetTimeAndWeatherManager();
        if (!weather)
            return;

        // 与玩家热应激同形的轻量近似：10:00–18:00 抛物线，峰值 1.3
        float tod = weather.GetTimeOfTheDay();
        if (tod < 10.0 || tod > 18.0)
        {
            s_fHeatStressMultiplier = 1.0;
            return;
        }

        float mid = 14.0;
        float span = 4.0;
        float t = (tod - mid) / span;
        if (t < 0.0)
            t = -t;
        if (t > 1.0)
            t = 1.0;
        float peak = 1.3;
        s_fHeatStressMultiplier = 1.0 + (peak - 1.0) * (1.0 - t);
    }

    static void Reset()
    {
        s_fLastUpdateSec = -1000.0;
        s_fHeatStressMultiplier = 1.0;
    }
}
