//! ============================================================================
//! RSS-CPCR — CP–W′ Coupled Cardiorespiratory Response
//! （CP–W′ 耦合心肺响应正演）
//!
//! 表现层轻量正演：由 CP–W′ / 有氧状态 → Cardiac / Respiratory 驱动轴。
//! CHR/RCP 启发；非统一学术「CP-W」模型，不反推真实 HR/VO₂。
//! - Cardiac：心负荷，随 P>CP / W′耗空 / 有氧偏低上升，恢复慢
//! - Respiratory：滞后跟随 Cardiac（喘跟在心负荷之后）
//! 与代谢权威解耦：只读采样，不回写功率/体力
//! ============================================================================

class SCR_RSS_CardioDrive
{
    protected float m_fCardiac01 = 0.0;
    protected float m_fRespiratory01 = 0.0;

    protected float m_fPowerW = 0.0;
    protected float m_fCpW = 1.0;
    protected float m_fWPrime01 = 1.0;
    protected float m_fAerobic01 = 1.0;

    protected float m_fLastWorldSec = -1.0;
    protected bool m_bHasSample = false;

    void SetMetabolicSample(
        float powerWatts,
        float criticalPowerWatts,
        float wPrimePool01,
        float aerobic01)
    {
        m_fPowerW = powerWatts;
        m_fCpW = criticalPowerWatts;
        if (m_fCpW < 50.0)
            m_fCpW = 50.0;
        m_fWPrime01 = Math.Clamp(wPrimePool01, 0.0, 1.0);
        m_fAerobic01 = Math.Clamp(aerobic01, 0.0, 1.0);
        m_bHasSample = true;
    }

    void Tick(float worldTimeSec)
    {
        if (!SCR_RSS_Constants.V6_CARDIO_DRIVE_ENABLED)
        {
            m_fCardiac01 = 0.0;
            m_fRespiratory01 = 0.0;
            m_fLastWorldSec = worldTimeSec;
            return;
        }

        float dt = 0.016;
        if (m_fLastWorldSec >= 0.0)
            dt = worldTimeSec - m_fLastWorldSec;
        m_fLastWorldSec = worldTimeSec;

        if (dt < 0.0)
            dt = 0.0;
        if (dt > 0.25)
            dt = 0.25;
        if (dt < 0.0005)
            return;

        float target = 0.0;
        if (m_bHasSample)
            target = ComputeInstantTarget01();

        float tauUp = SCR_RSS_Constants.V6_CARDIO_CARDIAC_TAU_UP_SEC;
        float tauDown = SCR_RSS_Constants.V6_CARDIO_CARDIAC_TAU_DOWN_SEC;
        m_fCardiac01 = ApproachAsymmetric(m_fCardiac01, target, dt, tauUp, tauDown);

        float brUp = SCR_RSS_Constants.V6_CARDIO_BREATH_TAU_UP_SEC;
        float brDown = SCR_RSS_Constants.V6_CARDIO_BREATH_TAU_DOWN_SEC;
        m_fRespiratory01 = ApproachAsymmetric(
            m_fRespiratory01, m_fCardiac01, dt, brUp, brDown);
    }

    float GetCardiac01()
    {
        return m_fCardiac01;
    }

    float GetRespiratory01()
    {
        return m_fRespiratory01;
    }

    void Reset()
    {
        m_fCardiac01 = 0.0;
        m_fRespiratory01 = 0.0;
        m_fLastWorldSec = -1.0;
        m_bHasSample = false;
    }

    //! 瞬时目标：超额功率 + W′耗空 + 有氧偏低（权重可调）
    protected float ComputeInstantTarget01()
    {
        float excess = m_fPowerW - m_fCpW;
        if (excess < 0.0)
            excess = 0.0;
        float excessNorm = excess / m_fCpW;
        float excessRef = SCR_RSS_Constants.V6_CARDIO_EXCESS_REF;
        if (excessRef < 0.05)
            excessRef = 0.05;
        float excessDrive = excessNorm / excessRef;
        if (excessDrive > 1.0)
            excessDrive = 1.0;
        // 欠 CP 时略抑制，避免巡航假喘
        if (m_fPowerW < m_fCpW * 0.92)
        {
            float below = (m_fCpW - m_fPowerW) / m_fCpW;
            if (below > 0.35)
                below = 0.35;
            excessDrive = excessDrive * (1.0 - below);
        }

        float empty = 1.0 - m_fWPrime01;
        float wPrimeStrain = empty * empty;

        float aeroRef = SCR_RSS_Constants.V6_CARDIO_AEROBIC_STRAIN_START;
        float aeroStrain = 0.0;
        if (m_fAerobic01 < aeroRef)
        {
            float a = (aeroRef - m_fAerobic01) / aeroRef;
            aeroStrain = a * a;
        }

        float wEx = SCR_RSS_Constants.V6_CARDIO_W_EXCESS;
        float wWp = SCR_RSS_Constants.V6_CARDIO_W_WPRIME;
        float wAe = SCR_RSS_Constants.V6_CARDIO_W_AEROBIC;
        float sumW = wEx + wWp + wAe;
        if (sumW < 0.001)
            sumW = 1.0;

        float target = (wEx * excessDrive + wWp * wPrimeStrain + wAe * aeroStrain) / sumW;
        return Math.Clamp(target, 0.0, 1.0);
    }

    protected float ApproachAsymmetric(
        float current,
        float target,
        float dt,
        float tauUpSec,
        float tauDownSec)
    {
        float tau = tauDownSec;
        if (target > current)
            tau = tauUpSec;
        if (tau < 0.05)
            tau = 0.05;

        // 一阶近似 α=dt/(τ+dt)，避免依赖 Math.Exp
        float alpha = dt / (tau + dt);
        return current + (target - current) * alpha;
    }
}
