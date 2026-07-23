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

    //! 运动 tick 调用：记录会话峰值功率（供 EPOC ∝ P_peak）
    //! 必须传入「限速内」功率（v_drain / FatiguePower），禁止用超速记账功率，否则停步 EPOC 会被 P_raw 打爆。
    void UpdateExercisePowerSample(float powerWatts, float currentSpeedMs)
    {
        m_fLastPowerWatts = Math.Max(powerWatts, 0.0);
        if (currentSpeedMs > 0.05 && powerWatts > m_fPeakPowerWatts)
            m_fPeakPowerWatts = powerWatts;
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
