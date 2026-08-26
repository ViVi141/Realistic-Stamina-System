//! 专服 / 进退服窗口：GetGame、World、Callqueue 可能已空，解引会 Access violation。
class SCR_RSS_RuntimeGuard
{
    //! World 为 sealed 原生类型，不能作 out 参数（编译器报 Class/World unrelated）。
    static World GetWorldOrNull()
    {
        if (!GetGame())
            return null;
        return GetGame().GetWorld();
    }

    static bool TryGetWorldTimeMs(out float timeMs)
    {
        timeMs = 0.0;
        World world = GetWorldOrNull();
        if (!world)
            return false;
        timeMs = world.GetWorldTime();
        return true;
    }

    static bool TryGetWorldTimeSec(out float timeSec)
    {
        timeSec = 0.0;
        float timeMs = 0.0;
        if (!TryGetWorldTimeMs(timeMs))
            return false;
        timeSec = timeMs / 1000.0;
        return true;
    }

    static ScriptCallQueue GetCallqueueOrNull()
    {
        if (!GetGame())
            return null;
        return GetGame().GetCallqueue();
    }
}
