//! 绝对速度空间的限速过渡（Sprint→Run、W′ 解除武装断崖、禁 Sprint 等）
//! 在 m/s 上 smoothstep 下行，避免引擎分母切换或巡航帽骤降导致的速度突变 / phys 互殴

class SCR_RSS_SprintBlockSpeedTransition
{
    protected float m_fCurrentSmoothedAbsMs = 0.0;
    protected float m_fTransitionStartAbsMs = 0.0;
    protected float m_fTransitionTargetAbsMs = 0.0;
    protected float m_fTransitionStartTime = -1.0;
    protected float m_fActiveTransitionDuration = 3.2;
    protected bool m_bWasSprintAllowed = true;
    protected bool m_bWasOverspeedArmed = true;
    protected bool m_bSnapUpActive = false;

    //! 普通掉速缓降
    protected const float TRANSITION_DURATION = 3.2;
    //! W′ 武装→解除：3.3→1.8 类断崖，需要更长缓降，避免 phys 与限速互殴
    protected const float DISARM_TRANSITION_DURATION = 5.2;
    protected const float LARGE_DROP_TRANSITION_DURATION = 4.6;
    protected const float HUGE_DROP_TRANSITION_DURATION = 5.5;
    protected const float CHANGE_THRESHOLD_MS = 0.08;
    //! 仍允许 Sprint 时，目标突降超过此值也走缓降（代谢/W′/负重压速）
    protected const float DROP_SMOOTH_THRESHOLD_MS = 0.22;
    protected const float LARGE_DROP_MS = 1.00;
    protected const float HUGE_DROP_MS = 1.40;
    //! 提速瞬切阈值：须大于有氧顶(2.4)↔March Run(2.8) 差值，避免 W′ 武装抖跳
    protected const float SNAP_UP_THRESHOLD_MS = 0.55;
    protected const float SNAP_UP_HYST_MS = 0.20;

    void Initialize()
    {
        m_fCurrentSmoothedAbsMs = 0.0;
        m_fTransitionStartAbsMs = 0.0;
        m_fTransitionTargetAbsMs = 0.0;
        m_fTransitionStartTime = -1.0;
        m_fActiveTransitionDuration = TRANSITION_DURATION;
        m_bWasSprintAllowed = true;
        m_bWasOverspeedArmed = true;
        m_bSnapUpActive = false;
    }

    protected void BeginAbsTransition(float currentTime, float startAbsMs, float targetAbsMs, float durationSec)
    {
        m_fTransitionStartAbsMs = startAbsMs;
        m_fTransitionTargetAbsMs = targetAbsMs;
        m_fTransitionStartTime = currentTime;
        m_fCurrentSmoothedAbsMs = startAbsMs;
        m_fActiveTransitionDuration = durationSec;
        if (m_fActiveTransitionDuration < 0.5)
            m_fActiveTransitionDuration = 0.5;
        m_bSnapUpActive = false;
    }

    protected float DurationForDropMs(float dropMs)
    {
        if (dropMs >= HUGE_DROP_MS)
            return HUGE_DROP_TRANSITION_DURATION;
        if (dropMs >= LARGE_DROP_MS)
            return LARGE_DROP_TRANSITION_DURATION;
        return TRANSITION_DURATION;
    }

    //! @param targetAbsoluteSpeedMs UpdateSpeed 算出的目标绝对速度 (m/s)
    //! @param currentEngineBaseMs 当前引擎限速基准 (Run 或 Sprint 原始最大速度)
    //! @param sprintAllowed GetRssSprintAllowed()
    //! @param lastAppliedMultiplier 上一帧已应用的 RSS 限速倍率
    //! @param lastEngineBaseMs 上一帧限速倍率对应的引擎基准
    //! @param overspeedArmed W′ 施密特武装态（边沿触发更长缓降）
    float UpdateAndGet(
        float currentTime,
        float targetAbsoluteSpeedMs,
        float currentEngineBaseMs,
        bool sprintAllowed,
        float lastAppliedMultiplier,
        float lastEngineBaseMs,
        bool overspeedArmed)
    {
        targetAbsoluteSpeedMs = Math.Max(targetAbsoluteSpeedMs, 0.01);
        currentEngineBaseMs = Math.Max(currentEngineBaseMs, 0.1);
        lastEngineBaseMs = Math.Max(lastEngineBaseMs, 0.1);

        // Idle 曾把目标托成 0.01 m/s；若当前已有可用限速，保持之，避免站着缓降到爬行。
        if (targetAbsoluteSpeedMs <= 0.02 && m_fCurrentSmoothedAbsMs > 0.5)
        {
            return Math.Clamp(m_fCurrentSmoothedAbsMs / currentEngineBaseMs, 0.01, 3.0);
        }

        if (m_fCurrentSmoothedAbsMs <= 0.01)
            m_fCurrentSmoothedAbsMs = targetAbsoluteSpeedMs;

        // W′ 武装→解除：强制从当前绝对速缓降到巡航顶（日志 3.3→1.8）
        if (m_bWasOverspeedArmed && !overspeedArmed)
        {
            float startAbsMs = lastAppliedMultiplier * lastEngineBaseMs;
            if (startAbsMs < m_fCurrentSmoothedAbsMs)
                startAbsMs = m_fCurrentSmoothedAbsMs;
            float dropOnDisarm = startAbsMs - targetAbsoluteSpeedMs;
            float dur = DISARM_TRANSITION_DURATION;
            float dropDur = DurationForDropMs(dropOnDisarm);
            if (dropDur > dur)
                dur = dropDur;
            BeginAbsTransition(currentTime, startAbsMs, targetAbsoluteSpeedMs, dur);
        }
        m_bWasOverspeedArmed = overspeedArmed;

        // 禁 Sprint 边沿：从上一帧绝对速度起步缓降到 Run 目标
        if (m_bWasSprintAllowed && !sprintAllowed)
        {
            float startAbsMs = lastAppliedMultiplier * lastEngineBaseMs;
            if (startAbsMs < m_fCurrentSmoothedAbsMs)
                startAbsMs = m_fCurrentSmoothedAbsMs;
            float dropSp = startAbsMs - targetAbsoluteSpeedMs;
            BeginAbsTransition(
                currentTime, startAbsMs, targetAbsoluteSpeedMs, DurationForDropMs(dropSp));
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
            // 已在下行缓降中：只改终点，不重置计时（避免巡航顶微抖把 5s 缓降永远拉长）
            if (significantDrop && m_fTransitionStartTime >= 0.0
                && m_fCurrentSmoothedAbsMs > targetAbsoluteSpeedMs)
            {
                m_fTransitionTargetAbsMs = targetAbsoluteSpeedMs;
                float needDur = DurationForDropMs(dropMs);
                if (needDur > m_fActiveTransitionDuration)
                    m_fActiveTransitionDuration = needDur;
            }
            else
            {
                BeginAbsTransition(
                    currentTime,
                    m_fCurrentSmoothedAbsMs,
                    targetAbsoluteSpeedMs,
                    DurationForDropMs(dropMs));
            }
        }

        if (m_fTransitionStartTime >= 0.0)
        {
            float elapsed = currentTime - m_fTransitionStartTime;
            float dur = m_fActiveTransitionDuration;
            if (dur < 0.5)
                dur = TRANSITION_DURATION;
            float progress = elapsed / dur;
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

        // 分母切到更矮的相位顶（Run→Walk）时，禁止用「高绝对速/矮顶」推出 >1 倍率
        float absForFrac = m_fCurrentSmoothedAbsMs;
        if (absForFrac > currentEngineBaseMs)
            absForFrac = currentEngineBaseMs;
        return absForFrac / currentEngineBaseMs;
    }

    bool IsInTransition()
    {
        return m_fTransitionStartTime >= 0.0;
    }

    //! TickPower 同帧解除武装时调用：UpdateAndGet 在 tick 前尚未见到边沿
    void EnsureDisarmTransition(float currentTime, float startAbsMs, float targetAbsMs)
    {
        if (!m_bWasOverspeedArmed)
            return;

        if (startAbsMs < 0.05)
            startAbsMs = m_fCurrentSmoothedAbsMs;
        if (startAbsMs < 0.05)
            startAbsMs = targetAbsMs;
        if (m_fCurrentSmoothedAbsMs > startAbsMs)
            startAbsMs = m_fCurrentSmoothedAbsMs;

        float dropMs = startAbsMs - targetAbsMs;
        float dur = DISARM_TRANSITION_DURATION;
        float dropDur = DurationForDropMs(dropMs);
        if (dropDur > dur)
            dur = dropDur;

        BeginAbsTransition(currentTime, startAbsMs, targetAbsMs, dur);
        m_bWasOverspeedArmed = false;
    }
}
