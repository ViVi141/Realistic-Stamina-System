//! v6 CP–W′ 临界功率模型（Morin–Petit + Skiba 再填充）
//! W′ 以焦耳存储；UI 仍可用 0–1 归一化

class SCR_RSS_CriticalPowerModel
{
    protected float m_fWPrimeJoules;
    protected float m_fWPrimeMaxJoules;
    protected float m_fCooldownUntilSec;
    protected float m_fSprintStartSec;
    protected bool m_bWasSprinting;
    protected float m_fLastShortBurstReleaseSec;
    protected bool m_bDepletionCooldownApplied;
    protected float m_fFatigueCpMultiplier;

    protected float m_fContextLoadKg;
    protected float m_fContextGradePercent;
    protected float m_fContextEnvCpMult;
    protected float m_fContextFatigueNorm;

    void SCR_RSS_CriticalPowerModel()
    {
        ResetToFull();
        m_fFatigueCpMultiplier = 1.0;
        m_fContextLoadKg = 0.0;
        m_fContextGradePercent = 0.0;
        m_fContextEnvCpMult = 1.0;
        m_fContextFatigueNorm = 0.0;
    }

    void ResetToFull()
    {
        m_fWPrimeMaxJoules = SCR_RSS_ConfigBridge.GetWPrimeMaxJoules();
        m_fWPrimeJoules = m_fWPrimeMaxJoules;
        m_fCooldownUntilSec = -1.0;
        m_fSprintStartSec = -1.0;
        m_bWasSprinting = false;
        m_fLastShortBurstReleaseSec = -1.0;
        m_bDepletionCooldownApplied = false;
    }

    void SetRuntimeContext(float loadKg, float gradePercent, float envCpMult, float fatigueNorm)
    {
        m_fContextLoadKg = Math.Max(loadKg, 0.0);
        m_fContextGradePercent = gradePercent;
        m_fContextEnvCpMult = Math.Clamp(envCpMult, SCR_RSS_Constants.V6_CP_ENV_FLOOR, 1.0);
        m_fContextFatigueNorm = Math.Clamp(fatigueNorm, 0.0, 1.0);
    }

    void SetFatigueCpMultiplier(float mult)
    {
        m_fFatigueCpMultiplier = Math.Clamp(mult, 0.75, 1.0);
    }

    float GetWPrimeJoules()
    {
        return m_fWPrimeJoules;
    }

    float GetWPrimeMaxJoules()
    {
        return m_fWPrimeMaxJoules;
    }

    float GetPool01()
    {
        if (m_fWPrimeMaxJoules <= 1.0)
            return 0.0;
        return Math.Clamp(m_fWPrimeJoules / m_fWPrimeMaxJoules, 0.0, 1.0);
    }

    float GetCooldownUntilSec()
    {
        return m_fCooldownUntilSec;
    }

    //! 动态 CP：load / slope / env / fatigue（见 v6 plan 核心方程 §2）
    float ComputeCpBaseWatts()
    {
        float cp0 = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
        if (cp0 <= 1.0)
            cp0 = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;

        float excessLoad = m_fContextLoadKg - SCR_RSS_Constants.V6_CP_LOAD_REF_KG;
        if (excessLoad < 0.0)
            excessLoad = 0.0;
        float cpLoad = cp0 * (1.0 - SCR_RSS_Constants.V6_CP_LOAD_DECAY_PER_KG * excessLoad);
        if (cpLoad < cp0 * 0.5)
            cpLoad = cp0 * 0.5;

        float g = m_fContextGradePercent * 0.01;
        if (g > 0.0)
        {
            float slopeMult = 1.0 - SCR_RSS_Constants.V6_CP_SLOPE_K_UP * g * g;
            if (slopeMult < 0.65)
                slopeMult = 0.65;
            cpLoad = cpLoad * slopeMult;
        }

        cpLoad = cpLoad * m_fContextEnvCpMult;

        float fatMult = 1.0 - SCR_RSS_Constants.V6_CP_FATIGUE_K * m_fContextFatigueNorm;
        if (fatMult < 0.75)
            fatMult = 0.75;
        cpLoad = cpLoad * fatMult;

        return cpLoad;
    }

    float GetEffectiveCriticalPowerWatts()
    {
        return ComputeCpBaseWatts() * m_fFatigueCpMultiplier;
    }

    protected bool UsesSkibaRecovery()
    {
        float cp0 = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
        if (cp0 <= 1.0)
            cp0 = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        if (cp0 <= SCR_RSS_Constants.V6_SKIBA_ELITE_CP_THRESHOLD_W)
            return true;
        return false;
    }

    protected void ApplyWPrimeRecovery(float powerWatts, float cp, float timeDeltaSec)
    {
        if (UsesSkibaRecovery())
        {
            float wLim = m_fWPrimeMaxJoules * SCR_RSS_Constants.V6_W_PRIME_LIM_RATIO;
            float kFast = SCR_RSS_Constants.V6_W_PRIME_K_FAST * (1.0 - 0.3 * m_fContextFatigueNorm);
            float kSlow = SCR_RSS_Constants.V6_W_PRIME_K_SLOW * (1.0 - 0.5 * m_fContextFatigueNorm);
            if (kFast < 0.01)
                kFast = 0.01;
            if (kSlow < 0.0001)
                kSlow = 0.0001;

            float fastTerm = 0.0;
            if (m_fWPrimeJoules < wLim)
                fastTerm = kFast * (wLim - m_fWPrimeJoules);
            float slowTerm = kSlow * (m_fWPrimeMaxJoules - wLim);
            m_fWPrimeJoules = m_fWPrimeJoules + (fastTerm + slowTerm) * timeDeltaSec;
        }
        else
        {
            float recoveryW = SCR_RSS_ConfigBridge.GetWPrimeRecoveryWPerSec();
            m_fWPrimeJoules = m_fWPrimeJoules + recoveryW * timeDeltaSec;
        }

        if (m_fWPrimeJoules > m_fWPrimeMaxJoules)
            m_fWPrimeJoules = m_fWPrimeMaxJoules;
    }

    void ApplyReplication(float pool01, float cooldownUntilSec, float wPrimeMaxJoules)
    {
        if (wPrimeMaxJoules > 1.0)
            m_fWPrimeMaxJoules = wPrimeMaxJoules;
        m_fWPrimeJoules = Math.Clamp(pool01, 0.0, 1.0) * m_fWPrimeMaxJoules;
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

    float GetAvailablePowerWatts(bool sprintIntent, float timeDeltaSec, float worldTimeSec)
    {
        float cp = GetEffectiveCriticalPowerWatts();
        if (!sprintIntent)
            return cp;

        if (IsOnCooldown(worldTimeSec))
            return cp;

        float cap = SCR_RSS_ConfigBridge.GetSprintPowerCapWatts();
        if (cap <= cp)
            cap = cp + SCR_RSS_Constants.V6_SPRINT_POWER_CAP_WATTS_DEFAULT * 0.5;

        if (m_fWPrimeJoules <= 0.0)
            return cp;

        float burstBudget = m_fWPrimeJoules / Math.Max(timeDeltaSec, 0.01);
        float available = cp + burstBudget;
        if (available > cap)
            available = cap;
        return available;
    }

    bool IsSprintAllowed(float aerobicStamina, bool collapseState, float worldTimeSec)
    {
        if (collapseState)
            return false;
        if (aerobicStamina < SCR_RSS_Constants.SPRINT_ENABLE_THRESHOLD)
            return false;

        float threshold = SCR_RSS_ConfigBridge.GetAnaerobicSprintEnableThreshold();
        if (GetPool01() <= threshold)
            return false;

        if (IsOnCooldown(worldTimeSec))
            return false;

        return true;
    }

    void Tick(float powerWatts, bool sprintIntent, float worldTimeSec, float timeDeltaSec)
    {
        float cp = GetEffectiveCriticalPowerWatts();

        if (sprintIntent)
        {
            if (!m_bWasSprinting)
                m_fSprintStartSec = worldTimeSec;

            if (powerWatts > cp)
            {
                float drainJ = (powerWatts - cp) * timeDeltaSec;
                m_fWPrimeJoules = m_fWPrimeJoules - drainJ;
                if (m_fWPrimeJoules < 0.0)
                    m_fWPrimeJoules = 0.0;
            }

            float threshold = SCR_RSS_ConfigBridge.GetAnaerobicSprintEnableThreshold();
            if (GetPool01() <= threshold)
            {
                if (!m_bDepletionCooldownApplied)
                {
                    float burstDuration = worldTimeSec - m_fSprintStartSec;
                    if (burstDuration < 0.0)
                        burstDuration = 0.0;
                    ApplyCooldownOnSprintEnd(worldTimeSec, burstDuration, GetPool01());
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
                ApplyCooldownOnSprintEnd(worldTimeSec, burstDuration, GetPool01());
                m_fSprintStartSec = -1.0;
            }

            if (!IsOnCooldown(worldTimeSec) && powerWatts <= cp + 5.0)
                ApplyWPrimeRecovery(powerWatts, cp, timeDeltaSec);
        }

        m_bWasSprinting = sprintIntent;
    }

    protected void ApplyCooldownOnSprintEnd(float worldTimeSec, float burstDurationSec, float reserveAtEnd01)
    {
        float fullCd = SCR_RSS_ConfigBridge.GetBurstCooldownFullSeconds();
        float shortCd = SCR_RSS_ConfigBridge.GetBurstCooldownShortSeconds();

        if (reserveAtEnd01 <= SCR_RSS_ConfigBridge.GetAnaerobicSprintEnableThreshold())
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

        float scaled = fullCd * (1.0 - SCR_RSS_Constants.V5_BURST_EARLY_RELEASE_BONUS * reserveAtEnd01);
        if (scaled < shortCd)
            scaled = shortCd;
        m_fCooldownUntilSec = worldTimeSec + scaled;
    }
}
