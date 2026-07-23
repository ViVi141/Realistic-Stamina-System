//! 绝对速度空间的限速过渡（Sprint→Run、冲刺中代谢压速、禁 Sprint 等）
//! 在 m/s 上 smoothstep 下行，避免引擎 Sprint/Run 分母切换或目标骤降导致的速度突变

class SCR_RSS_SprintBlockSpeedTransition
{
    protected float m_fCurrentSmoothedAbsMs = 0.0;
    protected float m_fTransitionStartAbsMs = 0.0;
    protected float m_fTransitionTargetAbsMs = 0.0;
    protected float m_fTransitionStartTime = -1.0;
    protected bool m_bWasSprintAllowed = true;
    protected bool m_bSnapUpActive = false;

    //! 下行过渡略短于坡度 5s：冲刺被切时要能感到减速，但不能瞬切
    protected const float TRANSITION_DURATION = 3.2;
    protected const float CHANGE_THRESHOLD_MS = 0.08;
    //! 仍允许 Sprint 时，目标突降超过此值也走缓降（代谢/W′/负重压速）
    protected const float DROP_SMOOTH_THRESHOLD_MS = 0.22;
    //! 提速瞬切阈值：须大于有氧顶(2.4)↔March Run(2.8) 差值，避免 W′ 武装抖跳
    protected const float SNAP_UP_THRESHOLD_MS = 0.55;
    protected const float SNAP_UP_HYST_MS = 0.20;

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

        // 禁 Sprint 边沿：从上一帧绝对速度起步缓降到 Run 目标
        if (m_bWasSprintAllowed && !sprintAllowed)
        {
            float startAbsMs = lastAppliedMultiplier * lastEngineBaseMs;
            if (startAbsMs < m_fCurrentSmoothedAbsMs)
                startAbsMs = m_fCurrentSmoothedAbsMs;
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

        float dropMs = m_fCurrentSmoothedAbsMs - targetAbsoluteSpeedMs;
        float gainMs = targetAbsoluteSpeedMs - m_fCurrentSmoothedAbsMs;

        // 提速：保持即时响应（带滞回）
        if (gainMs >= SNAP_UP_THRESHOLD_MS)
        {
            if (m_bSnapUpActive)
                m_bSnapUpActive = (gainMs >= SNAP_UP_HYST_MS);
            if (!m_bSnapUpActive)
            {
                m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;
                m_fTransitionTargetAbsMs = targetAbsoluteSpeedMs;
                m_fTransitionStartTime = -1.0;
                m_bSnapUpActive = true;
                return m_fCurrentSmoothedAbsMs / currentEngineBaseMs;
            }
        }
        else
        {
            if (m_bSnapUpActive && gainMs < SNAP_UP_HYST_MS)
                m_bSnapUpActive = false;
        }

        // 显著掉速或中等提速（含冲刺中代谢限速、W′ 巡航帽 2.4↔2.8）：绝对速度空间缓变
        // 大提速仍走上方 SNAP_UP；此处覆盖「不够瞬切、但瞬时跳会抖」的区间
        bool significantDrop = false;
        if (dropMs >= DROP_SMOOTH_THRESHOLD_MS)
            significantDrop = true;
        if (!sprintAllowed && dropMs >= CHANGE_THRESHOLD_MS)
            significantDrop = true;

        bool significantGainSmooth = false;
        if (gainMs >= DROP_SMOOTH_THRESHOLD_MS && gainMs < SNAP_UP_THRESHOLD_MS)
            significantGainSmooth = true;

        bool targetChanged = Math.AbsFloat(targetAbsoluteSpeedMs - m_fTransitionTargetAbsMs) >= CHANGE_THRESHOLD_MS;
        if ((significantDrop || significantGainSmooth) && (targetChanged || m_fTransitionStartTime < 0.0))
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
            m_fCurrentSmoothedAbsMs = m_fTransitionStartAbsMs
                + (m_fTransitionTargetAbsMs - m_fTransitionStartAbsMs) * smoothProgress;
            if (progress >= 1.0)
                m_fTransitionStartTime = -1.0;
        }
        else
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
