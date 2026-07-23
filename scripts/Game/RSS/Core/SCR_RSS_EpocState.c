// EPOC 延迟状态管理（v6：跟踪峰值代谢功率供 EPOC drain 标定）

class SCR_RSS_EpocState
{
    float m_fEpocDelayStartTime = -1.0;
    bool m_bIsInEpocDelay = false;
    float m_fLastSpeedForEpoc = 0.0;
    float m_fSpeedBeforeStop = 0.0;
    float m_fPeakPowerWatts = 0.0;
    float m_fLastPowerWatts = 0.0;

    float GetEpocDelayStartTime()
    {
        return m_fEpocDelayStartTime;
    }

    void SetEpocDelayStartTime(float time)
    {
        m_fEpocDelayStartTime = time;
    }

    bool IsInEpocDelay()
    {
        return m_bIsInEpocDelay;
    }

    void SetIsInEpocDelay(bool value)
    {
        m_bIsInEpocDelay = value;
    }

    float GetLastSpeedForEpoc()
    {
        return m_fLastSpeedForEpoc;
    }

    void SetLastSpeedForEpoc(float speed)
    {
        m_fLastSpeedForEpoc = speed;
    }

    float GetSpeedBeforeStop()
    {
        return m_fSpeedBeforeStop;
    }

    void SetSpeedBeforeStop(float speed)
    {
        m_fSpeedBeforeStop = speed;
    }

    float GetPeakPowerWatts()
    {
        return m_fPeakPowerWatts;
    }

    float GetLastPowerWatts()
    {
        return m_fLastPowerWatts;
    }

    //! 运动 tick：跟踪近期峰值（供 EPOC ∝ P_peak）
    //! 调用方须传 GetEpocSamplePowerWatts（限速内意图功率）；峰值只在运动中衰减。
    void UpdateExercisePowerSample(float powerWatts, float currentSpeedMs, float timeDeltaSec = 0.0)
    {
        m_fLastPowerWatts = Math.Max(powerWatts, 0.0);
        if (currentSpeedMs <= 0.05)
            return;

        if (powerWatts > m_fPeakPowerWatts)
        {
            m_fPeakPowerWatts = powerWatts;
            return;
        }

        if (timeDeltaSec <= 0.0)
            return;

        float decayRate = SCR_RSS_Constants.EPOC_PEAK_DECAY_WATTS_PER_SEC;
        float fastFloor = m_fPeakPowerWatts * SCR_RSS_Constants.EPOC_PEAK_FAST_DECAY_RATIO;
        if (powerWatts < fastFloor)
            decayRate = SCR_RSS_Constants.EPOC_PEAK_DECAY_FAST_WATTS_PER_SEC;

        float decay = decayRate * timeDeltaSec;
        float decayed = m_fPeakPowerWatts - decay;
        if (decayed < powerWatts)
            decayed = powerWatts;
        if (decayed < 0.0)
            decayed = 0.0;
        m_fPeakPowerWatts = decayed;
    }

    void ResetPeakPowerForNewRest()
    {
        m_fPeakPowerWatts = 0.0;
    }

    void Reset()
    {
        m_fEpocDelayStartTime = -1.0;
        m_bIsInEpocDelay = false;
        m_fSpeedBeforeStop = 0.0;
        m_fPeakPowerWatts = 0.0;
        m_fLastPowerWatts = 0.0;
    }
}
