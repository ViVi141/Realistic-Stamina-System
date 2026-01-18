// 运动持续时间跟踪模块
// 负责跟踪连续运动时间和休息时间，用于计算累积疲劳因子
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class ExerciseTracker
{
    // ==================== 状态变量 ====================
    protected float m_fExerciseDurationMinutes = 0.0; // 连续运动时间（分钟）
    protected float m_fRestDurationMinutes = 0.0; // 连续休息时间（分钟），从停止运动开始计算
    protected float m_fLastUpdateTime = 0.0; // 上次更新时间（秒）
    protected bool m_bWasMoving = false; // 上次更新时是否在移动
    
    // ==================== 公共方法 ====================
    
    // 初始化运动持续时间跟踪变量
    // @param currentTime 当前世界时间（秒）
    void Initialize(float currentTime)
    {
        m_fExerciseDurationMinutes = 0.0;
        m_fRestDurationMinutes = 0.0;
        m_fLastUpdateTime = currentTime;
        m_bWasMoving = false;
    }
    
    // 更新运动/休息持续时间跟踪
    // @param currentTime 当前世界时间（秒）
    // @param isCurrentlyMoving 当前是否在移动（速度 > 0.05 m/s）
    void Update(float currentTime, bool isCurrentlyMoving)
    {
        if (isCurrentlyMoving)
        {
            if (m_bWasMoving)
            {
                // 继续运动：累积运动时间
                float exerciseTimeDelta = currentTime - m_fLastUpdateTime;
                if (exerciseTimeDelta > 0.0 && exerciseTimeDelta < 1.0) // 防止时间跳跃异常
                {
                    m_fExerciseDurationMinutes += exerciseTimeDelta / 60.0; // 转换为分钟
                }
            }
            else
            {
                // 从静止转为运动：重置运动时间和休息时间
                m_fExerciseDurationMinutes = 0.0;
                m_fRestDurationMinutes = 0.0;
            }
            m_bWasMoving = true;
        }
        else
        {
            // 静止：累积休息时间，并逐渐减少运动时间（疲劳恢复）
            float restTimeDelta = currentTime - m_fLastUpdateTime;
            if (restTimeDelta > 0.0 && restTimeDelta < 1.0)
            {
                // 累积休息时间（用于多维度恢复模型）
                if (m_bWasMoving)
                {
                    // 从运动转为静止：重置休息时间
                    m_fRestDurationMinutes = 0.0;
                }
                else
                {
                    // 继续休息：累积休息时间
                    m_fRestDurationMinutes += restTimeDelta / 60.0; // 转换为分钟
                }
                
                // 静止时，疲劳恢复速度是累积速度的2倍（快速恢复）
                if (m_fExerciseDurationMinutes > 0.0)
                {
                    m_fExerciseDurationMinutes = Math.Max(m_fExerciseDurationMinutes - (restTimeDelta / 60.0 * 2.0), 0.0);
                }
            }
            m_bWasMoving = false;
        }
        m_fLastUpdateTime = currentTime;
    }
    
    // 计算累积疲劳因子（基于运动持续时间）
    // 公式：fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × max(0, exercise_duration - FATIGUE_START_TIME)
    // @return 疲劳因子（1.0-2.0之间）
    float CalculateFatigueFactor()
    {
        // 计算有效运动持续时间（减去启动时间）
        float effectiveExerciseDuration = Math.Max(m_fExerciseDurationMinutes - RealisticStaminaSpeedSystem.FATIGUE_START_TIME_MINUTES, 0.0);
        float fatigueFactor = 1.0 + (RealisticStaminaSpeedSystem.FATIGUE_ACCUMULATION_COEFF * effectiveExerciseDuration);
        fatigueFactor = Math.Clamp(fatigueFactor, 1.0, RealisticStaminaSpeedSystem.FATIGUE_MAX_FACTOR); // 限制在1.0-2.0之间
        return fatigueFactor;
    }
    
    // 获取当前运动持续时间（分钟）
    // @return 运动持续时间（分钟）
    float GetExerciseDurationMinutes()
    {
        return m_fExerciseDurationMinutes;
    }
    
    // 获取当前休息持续时间（分钟）
    // @return 休息持续时间（分钟）
    float GetRestDurationMinutes()
    {
        return m_fRestDurationMinutes;
    }
    
    // 重置运动持续时间（用于调试或特殊场景）
    void ResetExerciseDuration()
    {
        m_fExerciseDurationMinutes = 0.0;
    }
    
    // 重置休息持续时间（用于调试或特殊场景）
    void ResetRestDuration()
    {
        m_fRestDurationMinutes = 0.0;
    }
}
