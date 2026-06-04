//! v5 无氧爆发池：秒级消耗 + 分层冷却（不写入引擎 stamina 条）

class SCR_RSS_AnaerobicBurst
{
    protected float m_fPool;
    protected float m_fCooldownUntilSec;
    protected float m_fSprintStartSec;
    protected bool m_bWasSprinting;
    protected float m_fLastShortBurstReleaseSec;
    protected bool m_bDepletionCooldownApplied;

    void SCR_RSS_AnaerobicBurst()
    {
        m_fPool = 1.0;
        m_fCooldownUntilSec = -1.0;
        m_fSprintStartSec = -1.0;
        m_bWasSprinting = false;
        m_fLastShortBurstReleaseSec = -1.0;
        m_bDepletionCooldownApplied = false;
    }

    float GetPool()
    {
        return m_fPool;
    }

    float GetCooldownUntilSec()
    {
        return m_fCooldownUntilSec;
    }

    //! 客户端 RplProp 回调写入（不触发 tick）
    void ApplyReplication(float pool, float cooldownUntilSec)
    {
        m_fPool = Math.Clamp(pool, 0.0, 1.0);
        m_fCooldownUntilSec = cooldownUntilSec;
    }

    float GetCooldownRemainingSec(float worldTimeSec)
    {
        if (m_fCooldownUntilSec < 0.0)
            return 0.0;
        float rem = m_fCooldownUntilSec - worldTimeSec;
        if (rem < 0.0)
            return 0.0;
        return rem;
    }

    bool IsOnCooldown(float worldTimeSec)
    {
        return GetCooldownRemainingSec(worldTimeSec) > 0.0;
    }

    //! @param isSprinting 当前是否在 Sprint
    //! @param worldTimeSec 世界时间（秒）
    //! @param timeDeltaSec tick 间隔
    //! @param drainPerSec 无氧消耗率（0-1 / s）
    void Tick(bool isSprinting, float worldTimeSec, float timeDeltaSec, float drainPerSec)
    {
        if (isSprinting)
        {
            if (!m_bWasSprinting)
                m_fSprintStartSec = worldTimeSec;

            m_fPool = m_fPool - drainPerSec * timeDeltaSec;
            if (m_fPool < 0.0)
                m_fPool = 0.0;

            if (m_fPool <= SCR_RSS_ConfigBridge.GetAnaerobicSprintEnableThreshold())
            {
                if (!m_bDepletionCooldownApplied)
                {
                    float burstDuration = worldTimeSec - m_fSprintStartSec;
                    if (burstDuration < 0.0)
                        burstDuration = 0.0;
                    ApplyCooldownOnSprintEnd(worldTimeSec, burstDuration, m_fPool);
                    m_bDepletionCooldownApplied = true;
                }
            }
        }
        else
        {
            m_bDepletionCooldownApplied = false;
            if (m_bWasSprinting)
            {
                float burstDuration = worldTimeSec - m_fSprintStartSec;
                if (burstDuration < 0.0)
                    burstDuration = 0.0;
                ApplyCooldownOnSprintEnd(worldTimeSec, burstDuration, m_fPool);
                m_fSprintStartSec = -1.0;
            }

            if (!IsOnCooldown(worldTimeSec))
            {
                float recoveryPerSec = SCR_RSS_ConfigBridge.GetAnaerobicRecoveryPerSec();
                m_fPool = m_fPool + recoveryPerSec * timeDeltaSec;
                if (m_fPool > 1.0)
                    m_fPool = 1.0;
            }
        }

        m_bWasSprinting = isSprinting;
    }

    protected void ApplyCooldownOnSprintEnd(float worldTimeSec, float burstDurationSec, float reserveAtEnd)
    {
        float fullCd = SCR_RSS_ConfigBridge.GetBurstCooldownFullSeconds();
        float shortCd = SCR_RSS_ConfigBridge.GetBurstCooldownShortSeconds();

        if (m_fPool <= SCR_RSS_ConfigBridge.GetAnaerobicSprintEnableThreshold())
        {
            m_fCooldownUntilSec = worldTimeSec + fullCd;
            return;
        }

        if (burstDurationSec <= SCR_RSS_Constants.V5_TACTICAL_SHORT_BURST_SEC)
        {
            m_fCooldownUntilSec = worldTimeSec + shortCd;
            m_fLastShortBurstReleaseSec = worldTimeSec;
            return;
        }

        float reserveRatio = reserveAtEnd;
        float scaled = fullCd * (1.0 - SCR_RSS_Constants.V5_BURST_EARLY_RELEASE_BONUS * reserveRatio);
        if (scaled < shortCd)
            scaled = shortCd;
        m_fCooldownUntilSec = worldTimeSec + scaled;
    }
}
