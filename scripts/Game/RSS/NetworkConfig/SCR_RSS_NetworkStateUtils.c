//! 网络状态判定工具（从 PlayerBase.c 拆分）
class SCR_RSS_NetworkStateUtils
{
    static bool IsHeartbeatActive(bool hasValidPlayerId, float lastHeartbeatTime, float currentTime, float heartbeatTimeoutSec = 10.0)
    {
        if (!hasValidPlayerId)
            return false;
        if (lastHeartbeatTime <= 0.0)
            return false;

        return (currentTime - lastHeartbeatTime) < heartbeatTimeoutSec;
    }

    static bool IsConnected(bool hasValidPlayerId, float lastHeartbeatTime, bool isHeartbeatActive)
    {
        if (!hasValidPlayerId)
            return false;
        if (lastHeartbeatTime == 0.0)
            return true;

        return isHeartbeatActive;
    }

    static bool HasPlayerIdChanged(int cachedPlayerId, int currentPlayerId)
    {
        if (cachedPlayerId == 0)
            return false;
        if (currentPlayerId == 0)
            return false;

        return cachedPlayerId != currentPlayerId;
    }
}
