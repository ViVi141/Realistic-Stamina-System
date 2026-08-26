//! 全局环境信号读取子域：从 SCR_RSS_EnvironmentFactor 拆出。
//! 静态 GameSignalsManager 内存读取（perf），回退到 TimeAndWeatherManagerEntity C++ 桥接。

enum ERSS_EnvSignal
{
    RAIN_INTENSITY,
    WIND_SPEED,
    TIME_OF_DAY,
    WETNESS,
    COUNT
}

class SCR_RSS_EnvSignalReader
{
    // 枚举 ERSS_EnvSignal 提供编译期名称检查，避免信号名拼写错误
    protected static ref GameSignalsManager s_pGlobalSignals;
    protected static int s_iSignalRainIntensity = -1; // ERSS_EnvSignal.RAIN_INTENSITY
    protected static int s_iSignalWindSpeed     = -1; // ERSS_EnvSignal.WIND_SPEED
    protected static int s_iSignalTOD           = -1; // ERSS_EnvSignal.TIME_OF_DAY
    protected static int s_iSignalWetness       = -1; // ERSS_EnvSignal.WETNESS

    //! perf: 全局信号索引（静态，所有实例共享，仅首次注册）
    static void EnsureSignalsRegistered()
    {
        if (s_pGlobalSignals)
            return;

        if (!GetGame())
            return;

        s_pGlobalSignals = GetGame().GetSignalsManager();
        if (!s_pGlobalSignals)
            return;

        s_iSignalRainIntensity = s_pGlobalSignals.AddOrFindSignal("RainIntensity");
        s_iSignalWindSpeed     = s_pGlobalSignals.AddOrFindSignal("WindSpeed");
        s_iSignalTOD           = s_pGlobalSignals.AddOrFindSignal("TimeOfDay");
        s_iSignalWetness       = s_pGlobalSignals.AddOrFindSignal("Wetness");
    }

    static bool CanReadSignals()
    {
        if (!s_pGlobalSignals)
            return false;
        if (!GetGame())
            return false;
        if (!GetGame().GetWorld())
            return false;
        return true;
    }

    static float ReadRainIntensity(TimeAndWeatherManagerEntity weatherManager)
    {
        if (CanReadSignals() && s_iSignalRainIntensity >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalRainIntensity);
        if (weatherManager)
            return weatherManager.GetRainIntensity();
        return 0.0;
    }

    static float ReadWindSpeed(TimeAndWeatherManagerEntity weatherManager)
    {
        if (CanReadSignals() && s_iSignalWindSpeed >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalWindSpeed);
        if (weatherManager)
            return weatherManager.GetWindSpeed();
        return 0.0;
    }

    static float ReadTimeOfDay(TimeAndWeatherManagerEntity weatherManager)
    {
        if (CanReadSignals() && s_iSignalTOD >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalTOD);
        if (weatherManager)
            return weatherManager.GetTimeOfTheDay();
        return 12.0;
    }

    static float ReadWetness(TimeAndWeatherManagerEntity weatherManager)
    {
        if (CanReadSignals() && s_iSignalWetness >= 0)
            return s_pGlobalSignals.GetSignalValue(s_iSignalWetness);
        if (weatherManager)
            return weatherManager.GetCurrentWetness();
        return 0.0;
    }

    //! 新 GameMode / 世界开始时调用（经 SCR_RSS_EnvironmentFactor.ResetGlobalSignalsCache 转发）。
    //! Workbench「重载脚本 + 重载世界」后，静态 s_pGlobalSignals 可能仍指向已销毁的管理器，
    //! 继续 GetSignalValue 会 Access violation。清空后由下一次 EnsureSignalsRegistered 重新绑定当前 GetGame。
    static void ResetGlobalSignalsCache()
    {
        s_pGlobalSignals = null;
        s_iSignalRainIntensity = -1;
        s_iSignalWindSpeed = -1;
        s_iSignalTOD = -1;
        s_iSignalWetness = -1;
    }
}
