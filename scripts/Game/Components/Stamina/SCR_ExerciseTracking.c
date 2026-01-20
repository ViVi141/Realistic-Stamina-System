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
    protected float m_fLastMovementTime = 0.0; // 上次移动时间（用于判断 idle 状态）
    protected const float IDLE_THRESHOLD_TIME = 1.0; // 静止判定阈值（秒，超过此时间视为 idle）
    
    // ==================== 公共方法 ====================
    
    // 初始化运动持续时间跟踪变量
    // @param currentTime 当前世界时间（毫秒）
    void Initialize(float currentTime)
    {
        m_fExerciseDurationMinutes = 0.0;
        m_fRestDurationMinutes = 0.0;
        m_fLastUpdateTime = currentTime / 1000.0; // 转换为秒
        m_bWasMoving = false;
        m_fLastMovementTime = currentTime / 1000.0; // 转换为秒
    }
    
    // 更新运动/休息持续时间跟踪
    // @param currentTime 当前世界时间（毫秒）
    // @param isCurrentlyMoving 当前是否在移动（速度 > 0.05 m/s）
    void Update(float currentTime, bool isCurrentlyMoving)
    {
        float currentTimeSeconds = currentTime / 1000.0; // 转换为秒
        
        if (isCurrentlyMoving)
        {
            if (m_bWasMoving)
            {
                // 继续运动：累积运动时间
                float exerciseTimeDelta = currentTimeSeconds - m_fLastUpdateTime;
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
            m_fLastMovementTime = currentTimeSeconds; // 更新最后移动时间
        }
        else
        {
            // 静止：检查是否进入 idle 状态
            float idleDuration = currentTimeSeconds - m_fLastMovementTime;
            bool isIdle = (idleDuration >= IDLE_THRESHOLD_TIME);
            
            // 调试信息：idle 状态检测
            static int idleDebugCounter = 0;
            idleDebugCounter++;
            if (idleDebugCounter >= 25) // 每5秒输出一次
            {
                idleDebugCounter = 0;
                PrintFormat("[ExerciseTracker] 静止检测 / Idle Detection: idleDuration=%1s, isIdle=%2, restDuration=%3min | idleDuration=%1s, isIdle=%2, restDuration=%3min",
                    Math.Round(idleDuration * 10.0) / 10.0,
                    isIdle,
                    Math.Round(m_fRestDurationMinutes * 10.0) / 10.0);
            }
            
            if (isIdle)
            {
                // 进入 idle 状态：累积休息时间
                float restTimeDelta = currentTimeSeconds - m_fLastUpdateTime;
                
                // 调试信息：休息时间累积
                static int restDebugCounter = 0;
                restDebugCounter++;
                if (restDebugCounter >= 25) // 每5秒输出一次
                {
                    restDebugCounter = 0;
                    PrintFormat("[ExerciseTracker] 休息时间累积 / Rest Time Accumulation: restTimeDelta=%1s, wasMoving=%2 | restTimeDelta=%1s, wasMoving=%2",
                        Math.Round(restTimeDelta * 10.0) / 10.0,
                        m_bWasMoving);
                }
                
                if (restTimeDelta > 0.0 && restTimeDelta < 1.0)
                {
                    // 累积休息时间（用于多维度恢复模型）
                    if (m_bWasMoving)
                    {
                        // 从运动转为 idle：重置休息时间
                        m_fRestDurationMinutes = 0.0;
                    }
                    else
                    {
                        // 继续 idle：累积休息时间
                        m_fRestDurationMinutes += restTimeDelta / 60.0; // 转换为分钟
                    }
                    
                    // 静止时，疲劳恢复速度是累积速度的2倍（快速恢复）
                    if (m_fExerciseDurationMinutes > 0.0)
                    {
                        m_fExerciseDurationMinutes = Math.Max(m_fExerciseDurationMinutes - (restTimeDelta / 60.0 * 2.0), 0.0);
                    }
                }
                else if (restTimeDelta >= 1.0)
                {
                    // 时间跳跃异常：重置更新时间，但不累积休息时间
                    PrintFormat("[ExerciseTracker] 时间跳跃异常 / Time Jump: restTimeDelta=%1s, 重置更新时间 | restTimeDelta=%1s, Resetting update time",
                        Math.Round(restTimeDelta * 10.0) / 10.0);
                }
            }
            else
            {
                // 未进入 idle 状态（静止时间 < 1秒）：不累积休息时间
                // 避免"短暂停顿"被误判为休息
            }
            m_bWasMoving = false;
        }
        m_fLastUpdateTime = currentTimeSeconds;
    }
    
    // 计算累积疲劳因子（基于运动持续时间）
    // 公式：fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × max(0, exercise_duration - FATIGUE_START_TIME)
    // @return 疲劳因子（1.0-2.0之间）
    float CalculateFatigueFactor()
    {
        // 计算有效运动持续时间（减去启动时间）
        float fatigueCoeff = StaminaConstants.GetFatigueAccumulationCoeff();
        float fatigueMaxFactor = StaminaConstants.GetFatigueMaxFactor();
        
        // 疲劳启动时间：如果疲劳系数 > 0，使用5.0分钟；否则使用配置值
        float fatigueStartTime = 5.0;
        if (fatigueCoeff <= 0.0)
        {
            fatigueStartTime = 5.0;
        }
        else
        {
            fatigueStartTime = StaminaConstants.FATIGUE_START_TIME_MINUTES;
        }
        
        float effectiveExerciseDuration = Math.Max(m_fExerciseDurationMinutes - fatigueStartTime, 0.0);
        float fatigueFactor = 1.0 + (fatigueCoeff * effectiveExerciseDuration);
        fatigueFactor = Math.Clamp(fatigueFactor, 1.0, fatigueMaxFactor); // 限制在1.0-2.0之间
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
