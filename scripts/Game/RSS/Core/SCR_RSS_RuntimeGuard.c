//! 专服 / 进退服窗口：GetGame、World、Callqueue 可能已空，解引会 Access violation。
class SCR_RSS_RuntimeGuard
{
    static bool TryGetWorld(out World world)
    {
        world = null;
        if (!GetGame())
            return false;
        world = GetGame().GetWorld();
        if (!world)
            return false;
        return true;
    }

    static bool TryGetWorldTimeMs(out float timeMs)
    {
        timeMs = 0.0;
        World world;
        if (!TryGetWorld(world))
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
