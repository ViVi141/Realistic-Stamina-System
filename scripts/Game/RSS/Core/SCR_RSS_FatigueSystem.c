// 疲劳积累系统模块
// v6：积分疲劳 I(t) + 传统 excess-drain 路径

class SCR_RSS_FatigueSystem
{
    protected float m_fFatigueAccumulation = 0.0;
    protected float m_fFatigueIntegral = 0.0;
    protected const float FATIGUE_DECAY_RATE = 0.0005;
    protected const float FATIGUE_DECAY_MIN_REST_TIME = 15.0;
    protected float m_fLastFatigueDecayTime = 0.0;
    protected float m_fLastRestStartTime = -1.0;
    protected const float MAX_FATIGUE_PENALTY = 0.2;
    protected const float FATIGUE_CONVERSION_COEFF = 0.05;

    void Initialize(float currentTime)
    {
        m_fFatigueAccumulation = 0.0;
        m_fFatigueIntegral = 0.0;
        m_fLastFatigueDecayTime = currentTime;
        m_fLastRestStartTime = -1.0;
    }

    void ProcessFatigueAccumulation(float excessDrainRate)
    {
        if (excessDrainRate > 0.0)
        {
            float fatigueGain = excessDrainRate * FATIGUE_CONVERSION_COEFF;
            m_fFatigueAccumulation = Math.Clamp(m_fFatigueAccumulation + fatigueGain, 0.0, MAX_FATIGUE_PENALTY);
        }
    }

    //! v6 积分疲劳：仅对超过有效 CP 的功率积分；上坡才加坡度权（下坡 g² 不再灌满 If）
    void ProcessFatigueIntegral(
        float powerWatts,
        float loadKg,
        float gradePercent,
        float terrainFactor,
        float timeDeltaSec,
        float currentSpeedMs,
        float effectiveCpWatts = -1.0)
    {
        if (timeDeltaSec <= 0.0)
            return;

        float bodyW = SCR_RSS_Constants.CHARACTER_WEIGHT;
        float loadRatio = 0.0;
        if (bodyW > 0.0)
            loadRatio = loadKg / bodyW;

        float g = gradePercent * 0.01;
        float slopeTerm = 0.0;
        if (g > 0.0)
            slopeTerm = SCR_RSS_Constants.V6_FATIGUE_K_SLOPE * g * g;

        float w = 1.0
            + SCR_RSS_Constants.V6_FATIGUE_K_LOAD * loadRatio
            + slopeTerm
            + SCR_RSS_Constants.V6_FATIGUE_K_TERRAIN * (terrainFactor - 1.0);
        if (w < 0.5)
            w = 0.5;

        float cpRef = effectiveCpWatts;
        if (cpRef <= 1.0)
            cpRef = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
        if (cpRef <= 1.0)
            cpRef = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;

        float driveP = powerWatts - cpRef;
        if (driveP < 0.0)
            driveP = 0.0;

        float iNorm = GetFatigueIntegralNorm();
        float r = 0.0;
        if (currentSpeedMs < 0.05 || powerWatts < cpRef * 0.5)
        {
            float oneMinus = 1.0 - iNorm;
            r = SCR_RSS_Constants.V6_FATIGUE_K_RECOVERY * oneMinus * oneMinus * powerWatts;
        }

        float dI = (w * driveP - r) * timeDeltaSec * SCR_RSS_Constants.V6_FATIGUE_INTEGRAL_SCALE;
        m_fFatigueIntegral = Math.Clamp(m_fFatigueIntegral + dI, 0.0, SCR_RSS_Constants.V6_FATIGUE_I_MAX);

        float legacyFromI = m_fFatigueIntegral * MAX_FATIGUE_PENALTY;
        if (legacyFromI > m_fFatigueAccumulation)
            m_fFatigueAccumulation = legacyFromI;
        if (m_fFatigueAccumulation > MAX_FATIGUE_PENALTY)
            m_fFatigueAccumulation = MAX_FATIGUE_PENALTY;
    }

    void ProcessFatigueDecay(float currentTime, float currentSpeed)
    {
        bool isCurrentlyMoving = (currentSpeed > 0.05);

        if (!isCurrentlyMoving)
        {
            if (m_fLastRestStartTime < 0.0)
                m_fLastRestStartTime = currentTime;

            if (m_fFatigueAccumulation > 0.0 || m_fFatigueIntegral > 0.0)
            {
                float restDuration = currentTime - m_fLastRestStartTime;

                if (restDuration >= FATIGUE_DECAY_MIN_REST_TIME)
                {
                    float fatigueTimeDelta = currentTime - m_fLastFatigueDecayTime;
                    if (fatigueTimeDelta > 0.0)
                    {
                        m_fFatigueAccumulation = Math.Max(
                            m_fFatigueAccumulation - (FATIGUE_DECAY_RATE * (fatigueTimeDelta / 0.2)), 0.0);
                        m_fFatigueIntegral = Math.Max(
                            m_fFatigueIntegral - (FATIGUE_DECAY_RATE * 0.5 * (fatigueTimeDelta / 0.2)), 0.0);
                        m_fLastFatigueDecayTime = currentTime;
                    }
                }
            }

            m_fLastFatigueDecayTime = currentTime;
        }
        else
        {
            m_fLastRestStartTime = -1.0;
        }
    }

    float GetFatigueAccumulation()
    {
        return m_fFatigueAccumulation;
    }

    float GetFatigueIntegralNorm()
    {
        if (SCR_RSS_Constants.V6_FATIGUE_I_MAX <= 0.0)
            return 0.0;
        return Math.Clamp(m_fFatigueIntegral / SCR_RSS_Constants.V6_FATIGUE_I_MAX, 0.0, 1.0);
    }

    float GetCpFatigueMultiplier()
    {
        float norm = GetFatigueIntegralNorm();
        float mult = 1.0 - SCR_RSS_Constants.V6_CP_FATIGUE_K * norm;
        if (mult < 0.82)
            mult = 0.82;
        return mult;
    }

    float GetMaxStaminaCap()
    {
        return 1.0 - m_fFatigueAccumulation;
    }

    float GetMaxFatiguePenalty()
    {
        return MAX_FATIGUE_PENALTY;
    }

    //! 估算疲劳上限收缩速率（/秒，正值表示 cap 下降），与 ProcessFatigueIntegral 同形、只读。
    float EstimateCapShrinkPerSecond(
        float powerWatts,
        float loadKg,
        float gradePercent,
        float terrainFactor,
        float currentSpeedMs,
        float effectiveCpWatts = -1.0)
    {
        if (m_fFatigueAccumulation >= MAX_FATIGUE_PENALTY - 0.000001)
            return 0.0;

        if (SCR_RSS_Constants.V6_FATIGUE_I_MAX <= 0.0)
            return 0.0;
        if (m_fFatigueIntegral >= SCR_RSS_Constants.V6_FATIGUE_I_MAX - 0.000001)
            return 0.0;

        float bodyW = SCR_RSS_Constants.CHARACTER_WEIGHT;
        float loadRatio = 0.0;
        if (bodyW > 0.0)
            loadRatio = loadKg / bodyW;

        float g = gradePercent * 0.01;
        float slopeTerm = 0.0;
        if (g > 0.0)
            slopeTerm = SCR_RSS_Constants.V6_FATIGUE_K_SLOPE * g * g;

        float w = 1.0
            + SCR_RSS_Constants.V6_FATIGUE_K_LOAD * loadRatio
            + slopeTerm
            + SCR_RSS_Constants.V6_FATIGUE_K_TERRAIN * (terrainFactor - 1.0);
        if (w < 0.5)
            w = 0.5;

        float cpRef = effectiveCpWatts;
        if (cpRef <= 1.0)
            cpRef = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
        if (cpRef <= 1.0)
            cpRef = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;

        float driveP = powerWatts - cpRef;
        if (driveP < 0.0)
            driveP = 0.0;

        float iNorm = GetFatigueIntegralNorm();
        float r = 0.0;
        if (currentSpeedMs < 0.05 || powerWatts < cpRef * 0.5)
        {
            float oneMinus = 1.0 - iNorm;
            r = SCR_RSS_Constants.V6_FATIGUE_K_RECOVERY * oneMinus * oneMinus * powerWatts;
        }

        float dIPerSec = (w * driveP - r) * SCR_RSS_Constants.V6_FATIGUE_INTEGRAL_SCALE;
        if (dIPerSec <= 0.0)
            return 0.0;

        float legacyFromI = m_fFatigueIntegral * MAX_FATIGUE_PENALTY;
        float legacyRate = dIPerSec * MAX_FATIGUE_PENALTY;
        if (legacyFromI + legacyRate <= m_fFatigueAccumulation + 0.000001)
            return 0.0;

        float headroom = MAX_FATIGUE_PENALTY - m_fFatigueAccumulation;
        if (legacyRate > headroom)
            return headroom;
        return legacyRate;
    }
}
