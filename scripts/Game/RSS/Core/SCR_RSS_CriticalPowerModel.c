//! v6 CP–W′ 临界功率模型（Morin–Petit + Skiba 再填充）
//! W′ 以焦耳存储；UI 仍可用 0–1 归一化

class SCR_RSS_CriticalPowerModel
{
    protected float m_fWPrimeJoules;
    protected float m_fWPrimeMaxJoules;
    //! 网络兼容字段；v6 不上时间 CD，恒为 -1
    protected float m_fCooldownUntilSec;
    protected float m_fFatigueCpMultiplier;
    //! Run/Sprint 超速武装（施密特）：耗尽后钉在 CP，直到 W′ 明显回升
    protected bool m_bOverspeedArmed;
    //! 有氧巡航闩：W′ 真正见底后锁巡航，再武装才解开。解除武装但池未空时仍可跑、仍烧 W′。
    protected bool m_bAerobicCruiseLatched;

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
        m_bOverspeedArmed = true;
        m_bAerobicCruiseLatched = false;
    }

    //! 施密特更新并返回是否允许超过 CP 的步态速度
    bool RefreshAndGetOverspeedArmed()
    {
        float pool = GetPool01();
        float threshold = SCR_RSS_ConfigBridge.GetWPrimeSprintEnableThreshold();
        float disableAt = threshold + SCR_RSS_Constants.V6_WPRIME_OVERSPEED_HYSTERESIS;
        float rearmAt = threshold + SCR_RSS_Constants.V6_WPRIME_OVERSPEED_REARM;
        if (rearmAt <= disableAt + 0.01)
            rearmAt = disableAt + 0.15;

        if (m_bOverspeedArmed)
        {
            if (pool <= disableAt)
                m_bOverspeedArmed = false;
        }
        else
        {
            if (pool > rearmAt)
                m_bOverspeedArmed = true;
        }
        return m_bOverspeedArmed;
    }

    bool IsOverspeedArmed()
    {
        return m_bOverspeedArmed;
    }

    //! 空仓锁巡航：池 ≤ 空地板则闩上；施密特再武装则解开。滞回避免 0↔10% 在满 Run 与 2.4 之间抽。
    bool RefreshAndGetAerobicCruiseLatched()
    {
        RefreshAndGetOverspeedArmed();
        if (m_bOverspeedArmed)
        {
            m_bAerobicCruiseLatched = false;
            return false;
        }

        float emptyFloorJ = SCR_RSS_Constants.V6_WPRIME_EMPTY_FLOOR_JOULES;
        if (m_fWPrimeJoules <= emptyFloorJ)
            m_bAerobicCruiseLatched = true;
        return m_bAerobicCruiseLatched;
    }

    bool IsAerobicCruiseLatched()
    {
        return m_bAerobicCruiseLatched;
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

    //! @deprecated 时间 CD 已停用；恒 -1（复制协议仍带该槽）
    float GetCooldownUntilSec()
    {
        return -1.0;
    }

    //! 动态 CP：load / slope / env（疲劳经 GetEffectiveCriticalPowerWatts × m_fFatigueCpMultiplier）
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

        return cpLoad;
    }

    float GetEffectiveCriticalPowerWatts()
    {
        return ComputeCpBaseWatts() * m_fFatigueCpMultiplier;
    }

    protected bool UsesSkibaRecovery()
    {
        // 按档位显式分派（不再用 CP 阈值近似）：Elite=Skiba，Standard/Tactical=线性
        return SCR_RSS_ConfigBridge.GetWPrimeRecoveryMode() == 0;
    }

    //! 欠 CP 越深回充越快：P→0 为满速率，P→(CP−margin) 收敛到 floor。
    protected float ComputeUnderCpRecoveryDepth01(float powerWatts, float cp)
    {
        float recoveryCeil = cp - SCR_RSS_Constants.V6_W_PRIME_RECOVERY_POWER_MARGIN_W;
        if (recoveryCeil < 1.0)
            recoveryCeil = 1.0;
        if (powerWatts >= recoveryCeil)
            return 0.0;

        float slack = recoveryCeil - powerWatts;
        float depth = slack / recoveryCeil;
        if (depth > 1.0)
            depth = 1.0;
        if (depth < 0.0)
            depth = 0.0;

        float depthFloor = SCR_RSS_Constants.V6_W_PRIME_RECOVERY_DEPTH_FLOOR;
        if (depthFloor < 0.0)
            depthFloor = 0.0;
        if (depthFloor > 1.0)
            depthFloor = 1.0;
        return depthFloor + (1.0 - depthFloor) * depth;
    }

    protected void ApplyWPrimeRecovery(float powerWatts, float cp, float timeDeltaSec)
    {
        float depth = ComputeUnderCpRecoveryDepth01(powerWatts, cp);
        if (depth <= 0.0)
            return;

        if (UsesSkibaRecovery())
        {
            float wLim = m_fWPrimeMaxJoules * SCR_RSS_Constants.V6_W_PRIME_LIM_RATIO;
            float phaseGate = wLim - m_fWPrimeMaxJoules * SCR_RSS_Constants.V6_W_PRIME_SKIBA_PHASE_EPS_RATIO;
            if (phaseGate < 0.0)
                phaseGate = 0.0;

            float kFast = SCR_RSS_Constants.V6_W_PRIME_K_FAST * (1.0 - 0.3 * m_fContextFatigueNorm);
            float kSlow = SCR_RSS_Constants.V6_W_PRIME_K_SLOW * (1.0 - 0.5 * m_fContextFatigueNorm);
            if (kFast < 0.01)
                kFast = 0.01;
            if (kSlow < 0.0001)
                kSlow = 0.0001;

            float dWdt = 0.0;
            if (m_fWPrimeJoules < phaseGate)
                dWdt = kFast * (wLim - m_fWPrimeJoules);
            else
                dWdt = kSlow * (m_fWPrimeMaxJoules - m_fWPrimeJoules);

            m_fWPrimeJoules = m_fWPrimeJoules + dWdt * depth * timeDeltaSec;
        }
        else
        {
            float recoveryW = SCR_RSS_ConfigBridge.GetWPrimeRecoveryWPerSec();
            m_fWPrimeJoules = m_fWPrimeJoules + recoveryW * depth * timeDeltaSec;
        }

        if (m_fWPrimeJoules > m_fWPrimeMaxJoules)
            m_fWPrimeJoules = m_fWPrimeMaxJoules;
    }

    void ApplyReplication(float pool01, float cooldownUntilSec, float wPrimeMaxJoules)
    {
        if (wPrimeMaxJoules > 1.0)
            m_fWPrimeMaxJoules = wPrimeMaxJoules;
        m_fWPrimeJoules = Math.Clamp(pool01, 0.0, 1.0) * m_fWPrimeMaxJoules;
        RefreshAndGetOverspeedArmed();
        // 忽略远端时间 CD；门禁只看 W′ / 有氧
        m_fCooldownUntilSec = -1.0;
    }

    //! @deprecated 时间 CD 已停用
    float GetCooldownRemainingSec(float worldTimeSec)
    {
        return 0.0;
    }

    //! @deprecated 时间 CD 已停用
    bool IsOnCooldown(float worldTimeSec)
    {
        return false;
    }

    float GetAvailablePowerWatts(bool sprintIntent, float timeDeltaSec, float worldTimeSec)
    {
        float cp = GetEffectiveCriticalPowerWatts();
        if (!sprintIntent)
            return cp;

        // 解除武装后禁止再用 W′/Δt 虚高功率（否则 W′≈0 时 0↔ε 会在 CP↔冲刺顶之间跳）
        if (!RefreshAndGetOverspeedArmed())
            return cp;

        float cap = SCR_RSS_ConfigBridge.GetSprintPowerCapWatts();
        if (cap <= cp)
            cap = cp + SCR_RSS_Constants.V6_SPRINT_POWER_CAP_WATTS_DEFAULT * 0.5;

        // 残余焦耳按空池处理，避免 ε/Δt 突然顶满 sprint_power_cap
        float emptyFloorJ = SCR_RSS_Constants.V6_WPRIME_EMPTY_FLOOR_JOULES;
        if (m_fWPrimeJoules <= emptyFloorJ)
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
        if (aerobicStamina < SCR_RSS_ConfigBridge.GetSprintEnableThreshold())
            return false;

        // 与超速武装共用施密特：耗尽带关闭后须回到 rearm，避免 W′≈20–25% 按住冲刺时快慢震荡
        if (!RefreshAndGetOverspeedArmed())
            return false;

        return true;
    }

    void Tick(float powerWatts, bool sprintIntent, float worldTimeSec, float timeDeltaSec, float currentSpeedMs = 0.0)
    {
        float cp = GetEffectiveCriticalPowerWatts();

        // Morin–Petit：P > CP 消耗 W′；静止且非 Sprint 意图时不扣无氧池
        if (powerWatts > cp)
        {
            bool allowDischarge = true;
            if (currentSpeedMs < 0.05 && !sprintIntent)
                allowDischarge = false;
            if (allowDischarge)
            {
                float drainJ = (powerWatts - cp) * timeDeltaSec;
                m_fWPrimeJoules = m_fWPrimeJoules - drainJ;
                if (m_fWPrimeJoules < SCR_RSS_Constants.V6_WPRIME_EMPTY_FLOOR_JOULES)
                    m_fWPrimeJoules = 0.0;
            }
        }

        if (!sprintIntent)
        {
            // Morin–Petit / Skiba：仅 P 明显低于 CP 才再填充（禁止 CP 巡航回充 → 再武装）
            float recoveryCeil = cp - SCR_RSS_Constants.V6_W_PRIME_RECOVERY_POWER_MARGIN_W;
            if (powerWatts < recoveryCeil)
                ApplyWPrimeRecovery(powerWatts, cp, timeDeltaSec);
        }
    }
}
