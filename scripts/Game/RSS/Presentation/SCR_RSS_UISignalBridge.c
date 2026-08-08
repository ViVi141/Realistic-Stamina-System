// UI信号桥接模块
// 对接官方 Exhaustion 信号（模糊/呼吸等）；有氧权威不改，可叠加 W′→引擎表现映射

class SCR_RSS_UISignalBridge
{
    protected IEntity m_pOwner;
    protected SignalsManagerComponent m_pSignalsManager;
    protected int m_iExhaustionSignal = -1;

    protected static float s_fSmoothedExhaustion = 0.0;
    protected static const float EXHAUSTION_SIGNAL_SMOOTH_ALPHA = 0.35;

    bool Init(IEntity owner)
    {
        m_pOwner = owner;
        return TryResolveSignals();
    }

    bool IsInitialized()
    {
        if (m_pSignalsManager == null || m_iExhaustionSignal == -1)
            TryResolveSignals();
        return (m_pSignalsManager != null && m_iExhaustionSignal != -1);
    }

    SignalsManagerComponent GetSignalsManager()
    {
        return m_pSignalsManager;
    }

    int GetExhaustionSignalID()
    {
        return m_iExhaustionSignal;
    }

    //! Exhaustion：高=更累。与引擎 GetStamina() 表现条同源（min有氧, W′映射）
    //! onset≥0.999：Exhaustion = 1 - presentation（原生路径：条&lt;~0.55 → 模糊）
    void UpdateUISignal(
        float staminaPercent,
        bool isExhausted,
        float currentSpeed,
        float totalDrainRate,
        bool forceOverdoseEffect,
        float wPrimePool01)
    {
        if (!IsInitialized())
            return;

        if (forceOverdoseEffect)
        {
            m_pSignalsManager.SetSignalValue(
                m_iExhaustionSignal, SCR_CombatStimConstants.OD_EXHAUSTION_SIGNAL_VALUE);
            return;
        }

        float presentation = SCR_RSS_SprintGate.ComputeEnginePresentationDisplay(
            staminaPercent,
            wPrimePool01,
            true,
            false);

        float exhaustion = MapPresentationToExhaustion(presentation);
        if (isExhausted)
        {
            if (exhaustion < 0.85)
                exhaustion = 0.85;
        }

        exhaustion = Math.Clamp(exhaustion, 0.0, 1.0);
        s_fSmoothedExhaustion = s_fSmoothedExhaustion
            + (exhaustion - s_fSmoothedExhaustion) * EXHAUSTION_SIGNAL_SMOOTH_ALPHA;
        m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, s_fSmoothedExhaustion);
    }

    //! onset≥0.999 → 直接 1-presentation；否则带死区缓入模糊阈
    protected float MapPresentationToExhaustion(float presentation)
    {
        presentation = Math.Clamp(presentation, 0.0, 1.0);
        float onset = SCR_RSS_Constants.V6_EXHAUSTION_FX_ONSET;
        if (onset >= 0.999)
            return 1.0 - presentation;

        float blurThresh = SCR_RSS_Constants.V6_EXHAUSTION_NATIVE_BLUR_THRESHOLD;
        if (onset < 0.05)
            onset = 0.05;
        if (blurThresh < 0.0)
            blurThresh = 0.0;
        if (blurThresh > 0.95)
            blurThresh = 0.95;

        if (presentation >= onset)
        {
            float span = 1.0 - onset;
            if (span < 0.001)
                return 0.0;
            return blurThresh * (1.0 - presentation) / span;
        }

        float t = (onset - presentation) / onset;
        return blurThresh + (1.0 - blurThresh) * t;
    }

    void SetExhaustionSignalOverride(float value)
    {
        if (!IsInitialized())
            return;
        m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, value);
    }

    void Cleanup()
    {
        m_pOwner = null;
        m_pSignalsManager = null;
        m_iExhaustionSignal = -1;
    }

    protected bool TryResolveSignals()
    {
        if (!m_pOwner)
            return false;

        m_pSignalsManager = SignalsManagerComponent.Cast(
            m_pOwner.FindComponent(SignalsManagerComponent));
        if (!m_pSignalsManager)
            return false;

        m_iExhaustionSignal = m_pSignalsManager.FindSignal("Exhaustion");
        return (m_iExhaustionSignal != -1);
    }
}
