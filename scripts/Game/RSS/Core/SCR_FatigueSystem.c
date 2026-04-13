// 疲劳积累系统模块
// 负责处理疲劳积累和恢复（生理上限溢出处理）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class FatigueSystem
{
    // ==================== 状态变量 ====================
    protected float m_fFatigueAccumulation = 0.0; // 疲劳积累值（0.0-1.0），影响最大体力上限
    protected const float FATIGUE_DECAY_RATE = 0.0001; // 疲劳恢复率（每0.2秒），长时间休息时疲劳逐渐减少
    protected const float FATIGUE_DECAY_MIN_REST_TIME = 60.0; // 疲劳恢复所需的最小休息时间（秒），需要长时间休息
    protected float m_fLastFatigueDecayTime = 0.0; // 上次疲劳恢复检查时间
    protected float m_fLastRestStartTime = -1.0; // 上次开始休息的时间（-1表示未休息）
    protected const float MAX_FATIGUE_PENALTY = 0.3; // 最大疲劳惩罚（30%），即疲劳积累最高时可降低30%最大体力上限
    protected const float FATIGUE_CONVERSION_COEFF = 0.05; // 转换系数（5%），超出消耗转化为疲劳的系数
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    void Initialize(float currentTime)
    {
        m_fFatigueAccumulation = 0.0;
        m_fLastFatigueDecayTime = currentTime;
        m_fLastRestStartTime = -1.0;
    }
    
    // 处理疲劳积累（当体力消耗超过生理上限时）
    // @param excessDrainRate 超出生理上限的消耗
    void ProcessFatigueAccumulation(float excessDrainRate)
    {
        if (excessDrainRate > 0.0)
        {
            // 将超出消耗转化为疲劳积累
            // 疲劳积累速度 = 超出消耗 × 转换系数
            // 例如：超出0.01（50%超负荷）→ 疲劳积累 +0.0005
            float fatigueGain = excessDrainRate * FATIGUE_CONVERSION_COEFF;
            m_fFatigueAccumulation = Math.Clamp(m_fFatigueAccumulation + fatigueGain, 0.0, MAX_FATIGUE_PENALTY);
        }
    }
    
    // 处理疲劳恢复（长时间休息后）
    // @param currentTime 当前世界时间
    // @param currentSpeed 当前速度（用于判断是否静止）
    void ProcessFatigueDecay(float currentTime, float currentSpeed)
    {
        bool isCurrentlyMoving = (currentSpeed > 0.05);
        
        // 跟踪休息时间（用于疲劳恢复）
        if (!isCurrentlyMoving)
        {
            // 静止：记录休息开始时间
            if (m_fLastRestStartTime < 0.0)
                m_fLastRestStartTime = currentTime;
            
            // 检查是否满足疲劳恢复条件（长时间休息）
            if (m_fFatigueAccumulation > 0.0)
            {
                float restDuration = currentTime - m_fLastRestStartTime;
                
                if (restDuration >= FATIGUE_DECAY_MIN_REST_TIME)
                {
                    // 满足长时间休息条件：开始疲劳恢复
                    float fatigueTimeDelta = currentTime - m_fLastFatigueDecayTime;
                    if (fatigueTimeDelta > 0.0 && fatigueTimeDelta < 1.0)
                    {
                        // 疲劳逐渐恢复（每0.2秒恢复 FATIGUE_DECAY_RATE）
                        m_fFatigueAccumulation = Math.Max(m_fFatigueAccumulation - (FATIGUE_DECAY_RATE * (fatigueTimeDelta / 0.2)), 0.0);
                    }
                }
                // 否则：休息时间不足，疲劳不恢复
            }
            
            m_fLastFatigueDecayTime = currentTime;
        }
        else
        {
            // 运动：重置休息时间
            m_fLastRestStartTime = -1.0;
        }
    }
    
    // 获取疲劳积累值
    // @return 疲劳积累值（0.0-1.0）
    float GetFatigueAccumulation()
    {
        return m_fFatigueAccumulation;
    }
    
    // 计算最大体力上限（考虑疲劳惩罚）
    // @return 最大体力上限（0.7-1.0），例如：30%疲劳 → 70%最大体力上限
    float GetMaxStaminaCap()
    {
        return 1.0 - m_fFatigueAccumulation;
    }
    
    // 获取最大疲劳惩罚
    // @return 最大疲劳惩罚（0.3 = 30%）
    float GetMaxFatiguePenalty()
    {
        return MAX_FATIGUE_PENALTY;
    }
}
