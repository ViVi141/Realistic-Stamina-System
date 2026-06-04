//! v6 无氧爆发：CP–W′ 焦耳池（对外保留 AnaerobicBurst 类名以减小 Integration  diff）

class SCR_RSS_AnaerobicBurst
{
    protected ref SCR_RSS_CriticalPowerModel m_pCpModel;

    void SCR_RSS_AnaerobicBurst()
    {
        m_pCpModel = new SCR_RSS_CriticalPowerModel();
    }

    SCR_RSS_CriticalPowerModel GetCpModel()
    {
        return m_pCpModel;
    }

    float GetPool()
    {
        if (!m_pCpModel)
            return 1.0;
        return m_pCpModel.GetPool01();
    }

    float GetWPrimeJoules()
    {
        if (!m_pCpModel)
            return SCR_RSS_Constants.V6_W_PRIME_MAX_JOULES_DEFAULT;
        return m_pCpModel.GetWPrimeJoules();
    }

    float GetCooldownUntilSec()
    {
        if (!m_pCpModel)
            return -1.0;
        return m_pCpModel.GetCooldownUntilSec();
    }

    void ApplyReplication(float pool, float cooldownUntilSec)
    {
        if (!m_pCpModel)
            return;
        float maxJ = SCR_RSS_ConfigBridge.GetWPrimeMaxJoules();
        m_pCpModel.ApplyReplication(pool, cooldownUntilSec, maxJ);
    }

    void ApplyReplicationJoules(float wPrimeJoules, float cooldownUntilSec, float wPrimeMaxJoules)
    {
        if (!m_pCpModel)
            return;
        if (wPrimeMaxJoules > 1.0)
            m_pCpModel.ApplyReplication(wPrimeJoules / wPrimeMaxJoules, cooldownUntilSec, wPrimeMaxJoules);
    }

    float GetCooldownRemainingSec(float worldTimeSec)
    {
        if (!m_pCpModel)
            return 0.0;
        return m_pCpModel.GetCooldownRemainingSec(worldTimeSec);
    }

    bool IsOnCooldown(float worldTimeSec)
    {
        if (!m_pCpModel)
            return false;
        return m_pCpModel.IsOnCooldown(worldTimeSec);
    }

    //! @param powerWatts 实测代谢功率（W）；v6 不再使用固定 drain/sec
    void TickPower(float powerWatts, bool isSprinting, float worldTimeSec, float timeDeltaSec)
    {
        if (!m_pCpModel)
            return;
        m_pCpModel.Tick(powerWatts, isSprinting, worldTimeSec, timeDeltaSec);
    }

    //! 兼容旧调用：由功率代换
    void Tick(bool isSprinting, float worldTimeSec, float timeDeltaSec, float drainPerSec)
    {
        if (!m_pCpModel)
            return;
        float cp = m_pCpModel.GetEffectiveCriticalPowerWatts();
        float powerW = cp;
        if (isSprinting)
            powerW = cp + drainPerSec * SCR_RSS_ConfigBridge.GetWPrimeMaxJoules() * 0.01;
        m_pCpModel.Tick(powerW, isSprinting, worldTimeSec, timeDeltaSec);
    }

    void SetFatigueCpMultiplier(float mult)
    {
        if (m_pCpModel)
            m_pCpModel.SetFatigueCpMultiplier(mult);
    }

    float GetAvailablePowerWatts(bool sprintIntent, float timeDeltaSec)
    {
        if (!m_pCpModel)
            return SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        return m_pCpModel.GetAvailablePowerWatts(sprintIntent, timeDeltaSec, 0.0);
    }
}
