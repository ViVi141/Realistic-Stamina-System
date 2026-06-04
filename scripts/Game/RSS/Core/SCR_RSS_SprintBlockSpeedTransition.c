//! 无氧池/冷却禁 Sprint 时的速度降低阻尼（Sprint → Run 平滑过渡）
//! 在绝对速度 (m/s) 空间 smoothstep，避免引擎 Sprint→Run 切换时分母变化导致阻尼失效

class SCR_RSS_SprintBlockSpeedTransition
{
    protected float m_fCurrentSmoothedAbsMs = 0.0;
    protected float m_fTransitionStartAbsMs = 0.0;
    protected float m_fTransitionTargetAbsMs = 0.0;
    protected float m_fTransitionStartTime = -1.0;
    protected bool m_bWasSprintAllowed = true;
    protected bool m_bSnapUpActive = false;

    protected const float TRANSITION_DURATION = 5.0;
    protected const float CHANGE_THRESHOLD_MS = 0.08;
    protected const float SNAP_UP_THRESHOLD_MS = 0.35;
    protected const float SNAP_UP_HYST_MS = 0.12;

    void Initialize()
    {
        m_fCurrentSmoothedAbsMs = 0.0;
        m_fTransitionStartAbsMs = 0.0;
        m_fTransitionTargetAbsMs = 0.0;
        m_fTransitionStartTime = -1.0;
        m_bWasSprintAllowed = true;
        m_bSnapUpActive = false;
    }

    //! @param targetAbsoluteSpeedMs UpdateSpeed 算出的目标绝对速度 (m/s)
    //! @param currentEngineBaseMs 当前引擎限速基准 (Run 或 Sprint 原始最大速度)
    //! @param sprintAllowed GetRssSprintAllowed()
    //! @param lastAppliedMultiplier 上一帧已应用的 RSS 限速倍率
    //! @param lastEngineBaseMs 上一帧限速倍率对应的引擎基准
    float UpdateAndGet(
        float currentTime,
        float targetAbsoluteSpeedMs,
        float currentEngineBaseMs,
        bool sprintAllowed,
        float lastAppliedMultiplier,
        float lastEngineBaseMs)
    {
        targetAbsoluteSpeedMs = Math.Max(targetAbsoluteSpeedMs, 0.01);
        currentEngineBaseMs = Math.Max(currentEngineBaseMs, 0.1);
        lastEngineBaseMs = Math.Max(lastEngineBaseMs, 0.1);

        if (m_fCurrentSmoothedAbsMs <= 0.01)
            m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;

        if (m_bWasSprintAllowed && !sprintAllowed)
        {
            float startAbsMs = lastAppliedMultiplier * lastEngineBaseMs;
            if (startAbsMs < m_fCurrentSmoothedAbsMs)
                startAbsMs = m_fCurrentSmoothedAbsMs;
            if (startAbsMs < targetAbsoluteSpeedMs)
                startAbsMs = targetAbsoluteSpeedMs;
            m_fTransitionStartAbsMs = startAbsMs;
            m_fTransitionTargetAbsMs = targetAbsoluteSpeedMs;
            m_fTransitionStartTime = currentTime;
            m_fCurrentSmoothedAbsMs = startAbsMs;
            m_bSnapUpActive = false;
        }

        if (!m_bWasSprintAllowed && sprintAllowed)
        {
            m_fTransitionStartTime = -1.0;
            m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;
            m_bSnapUpActive = false;
        }

        m_bWasSprintAllowed = sprintAllowed;

        if (sprintAllowed)
        {
            float gainMs = targetAbsoluteSpeedMs - m_fCurrentSmoothedAbsMs;
            if (m_bSnapUpActive)
                m_bSnapUpActive = (gainMs >= SNAP_UP_HYST_MS);
            if (!m_bSnapUpActive && gainMs >= SNAP_UP_THRESHOLD_MS)
            {
                m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;
                m_fTransitionTargetAbsMs = targetAbsoluteSpeedMs;
                m_fTransitionStartTime = -1.0;
                m_bSnapUpActive = true;
                return m_fCurrentSmoothedAbsMs / currentEngineBaseMs;
            }
            if (m_fTransitionStartTime < 0.0)
            {
                m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;
                return m_fCurrentSmoothedAbsMs / currentEngineBaseMs;
            }
        }

        bool targetChanged = Math.AbsFloat(targetAbsoluteSpeedMs - m_fTransitionTargetAbsMs) >= CHANGE_THRESHOLD_MS;
        bool needTransition = Math.AbsFloat(targetAbsoluteSpeedMs - m_fCurrentSmoothedAbsMs) >= CHANGE_THRESHOLD_MS;

        if (!sprintAllowed && needTransition && (targetChanged || m_fTransitionStartTime < 0.0))
        {
            m_fTransitionStartAbsMs = m_fCurrentSmoothedAbsMs;
            m_fTransitionTargetAbsMs = targetAbsoluteSpeedMs;
            m_fTransitionStartTime = currentTime;
        }

        if (m_fTransitionStartTime >= 0.0)
        {
            float elapsed = currentTime - m_fTransitionStartTime;
            float progress = elapsed / TRANSITION_DURATION;
            progress = Math.Clamp(progress, 0.0, 1.0);
            float smoothProgress = progress * progress * (3.0 - 2.0 * progress);
            m_fCurrentSmoothedAbsMs = m_fTransitionStartAbsMs + (m_fTransitionTargetAbsMs - m_fTransitionStartAbsMs) * smoothProgress;
            if (progress >= 1.0)
                m_fTransitionStartTime = -1.0;
        }
        else if (!sprintAllowed)
        {
            m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;
        }

        return m_fCurrentSmoothedAbsMs / currentEngineBaseMs;
    }

    bool IsInTransition()
    {
        return m_fTransitionStartTime >= 0.0;
    }
}
