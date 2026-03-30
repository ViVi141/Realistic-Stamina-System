// 坡度速度过渡模块
// 负责坡度变化时速度的 5 秒平滑过渡
// 避免从平地冲上陡坡时速度瞬间从 3 m/s 骤降到 1 m/s 的"被胶水粘住"感

class SlopeSpeedTransition
{
    // ==================== 状态变量 ====================
    protected float m_fCurrentSmoothedScale = 1.0;     // 当前平滑后的坡度速度倍率
    protected float m_fTransitionStartValue = 1.0;     // 过渡起始值
    protected float m_fTransitionTargetValue = 1.0;    // 过渡目标值
    protected float m_fTransitionStartTime = -1.0;     // 过渡开始时间（-1 表示无过渡）
    protected bool m_bLastSuppressSlope = false;        // 上一帧是否处于坡度抑制（室内）状态

    // ==================== 常量 ====================
    protected const float TRANSITION_DURATION = 5.0;   // 过渡持续时间（秒）
    protected const float CHANGE_THRESHOLD = 0.02;     // 变化阈值，低于此值不触发过渡
    // 立即提速阈值：仅当目标比当前平滑值高出至少此量时才取消平滑、立即提速，避免上坡中细微缓坡造成频繁加速
    protected const float SNAP_UP_THRESHOLD = 0.08;

    // ==================== 公共方法 ====================

    // 初始化
    void Initialize()
    {
        m_fCurrentSmoothedScale = 1.0;
        m_fTransitionStartValue = 1.0;
        m_fTransitionTargetValue = 1.0;
        m_fTransitionStartTime = -1.0;
        m_bLastSuppressSlope = false;
    }

    // 通知过渡器当前帧是否处于坡度抑制（室内）状态。
    // 室内/室外任意方向切换时均立即重置过渡状态为 1.0：
    //   进入室内：防止室外上坡的减速状态冻结在过渡器中，导致楼梯内或出楼梯后速度异常偏低。
    //   离开室内：防止室内冻结的旧值在出楼梯后带来意外减速过渡。
    // @param suppress true=室内（坡度被抑制），false=室外（坡度正常计算）
    void NotifySuppressSlope(bool suppress)
    {
        if (suppress != m_bLastSuppressSlope)
        {
            // 任意方向切换：将过渡器重置为 1.0，以干净状态重新开始
            m_fCurrentSmoothedScale = 1.0;
            m_fTransitionStartValue = 1.0;
            m_fTransitionTargetValue = 1.0;
            m_fTransitionStartTime = -1.0;
        }
        m_bLastSuppressSlope = suppress;
    }

    // 更新并获取平滑后的坡度速度倍率
    // @param currentTime 当前世界时间（秒）
    // @param targetScaleFactor 目标坡度倍率（来自 Tobler，约 0.15~1.0）
    // @return 平滑后的坡度倍率
    float UpdateAndGet(float currentTime, float targetScaleFactor)
    {
        targetScaleFactor = Math.Clamp(targetScaleFactor, 0.15, 1.0);

        // 仅当目标速度比当前平滑值「明显」更大（如从陡坡到平地，差值 ≥ SNAP_UP_THRESHOLD）时才取消平滑、立即提速，避免上坡中细微缓坡造成频繁加速
        float gain = targetScaleFactor - m_fCurrentSmoothedScale;
        if (gain >= SNAP_UP_THRESHOLD)
        {
            m_fCurrentSmoothedScale = targetScaleFactor;
            m_fTransitionTargetValue = targetScaleFactor;
            m_fTransitionStartTime = -1.0;
            return m_fCurrentSmoothedScale;
        }

        // 目标与当前过渡目标差异较大时，重新开始过渡
        bool targetChangedSignificantly = Math.AbsFloat(targetScaleFactor - m_fTransitionTargetValue) >= CHANGE_THRESHOLD;
        bool needNewTransition = Math.AbsFloat(targetScaleFactor - m_fCurrentSmoothedScale) >= CHANGE_THRESHOLD;

        if (needNewTransition && (targetChangedSignificantly || m_fTransitionStartTime < 0.0))
        {
            // 启动或重启 5 秒过渡
            m_fTransitionStartValue = m_fCurrentSmoothedScale;
            m_fTransitionTargetValue = targetScaleFactor;
            m_fTransitionStartTime = currentTime;
        }

        if (m_fTransitionStartTime >= 0.0)
        {
            float elapsed = currentTime - m_fTransitionStartTime;
            float progress = elapsed / TRANSITION_DURATION;
            progress = Math.Clamp(progress, 0.0, 1.0);

            // 使用 smoothstep 实现平滑过渡：t²(3-2t)
            float smoothProgress = progress * progress * (3.0 - 2.0 * progress);
            m_fCurrentSmoothedScale = m_fTransitionStartValue + (m_fTransitionTargetValue - m_fTransitionStartValue) * smoothProgress;

            if (progress >= 1.0)
                m_fTransitionStartTime = -1.0;
        }
        else
        {
            m_fCurrentSmoothedScale = targetScaleFactor;
        }

        return m_fCurrentSmoothedScale;
    }

    // 是否处于过渡中
    bool IsInTransition()
    {
        return m_fTransitionStartTime >= 0.0;
    }
}
