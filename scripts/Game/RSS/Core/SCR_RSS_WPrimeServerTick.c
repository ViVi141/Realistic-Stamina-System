//! 专服 W′ 补 tick（从 PlayerBase.c 拆分）

class SCR_RSS_WPrimeServerTick
{
    //! 玩家主循环不在服务端跑时补 tick 无氧池；返回是否执行了 tick
    static bool MaybeTick(
        bool isServer,
        bool isPlayerControlled,
        bool shouldProcessStaminaUpdate,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        SCR_RSS_StaminaState staminaState,
        bool isSprintActive,
        float aerobicPercent,
        float intervalSec,
        inout float lastStaminaUpdateTime,
        out float replPool,
        out float replCooldownUntil)
    {
        replPool = 1.0;
        replCooldownUntil = -1.0;

        if (!isServer || !isPlayerControlled)
            return false;
        if (!anaerobicBurst)
            return false;
        if (shouldProcessStaminaUpdate)
            return false;
        if (!GetGame() || !GetGame().GetWorld())
            return false;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
        float useInterval = intervalSec;
        if (lastStaminaUpdateTime >= 0.0)
        {
            float elapsed = currentTime - lastStaminaUpdateTime;
            if (elapsed < useInterval * 0.5)
                return false;
            useInterval = Math.Clamp(elapsed, 0.01, 0.5);
        }

        float anaDrain = SCR_RSS_ConfigBridge.GetWPrimeDrainPerSec();
        float powerW = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        if (isSprintActive)
            powerW = powerW + anaDrain * SCR_RSS_ConfigBridge.GetWPrimeMaxJoules() * 0.01;
        anaerobicBurst.TickPower(powerW, isSprintActive, currentTime, useInterval);
        if (staminaState)
        {
            staminaState.SetWPrimePool01(anaerobicBurst.GetPool());
            staminaState.SetAerobic(aerobicPercent);
        }
        SCR_RSS_NetworkSyncManager.ReadAnaerobicForReplication(
            anaerobicBurst, replPool, replCooldownUntil);
        lastStaminaUpdateTime = currentTime;
        return true;
    }
}
