// "撞墙"阻尼过渡模块
// 负责处理体力降至25%临界点时的5秒阻尼过渡逻辑
// 解决"撞墙"瞬间的陡峭度问题，让玩家感觉到角色是"腿越来越重"，而不是"引擎突然断油"
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class CollapseTransition
{
    // ==================== 状态变量 ====================
    protected bool m_bInCollapseTransition = false; // 是否处于"撞墙"过渡状态
    protected float m_fCollapseTransitionStartTime = 0.0; // "撞墙"过渡开始时间（秒）
    protected float m_fLastStaminaPercent = 1.0; // 上次体力百分比（用于检测临界点）
    
    // ==================== 常量 ====================
    protected const float COLLAPSE_TRANSITION_DURATION = 5.0; // "撞墙"过渡持续时间（秒）
    protected const float COLLAPSE_THRESHOLD = 0.25; // "撞墙"临界点（25%体力）
    
    // ==================== 公共方法 ====================
    
    // 初始化"撞墙"阻尼过渡变量
    void Initialize()
    {
        m_bInCollapseTransition = false;
        m_fCollapseTransitionStartTime = 0.0;
        m_fLastStaminaPercent = 1.0;
    }
    
    // 更新"撞墙"临界点检测和过渡状态
    // @param currentTime 当前世界时间（秒）
    // @param currentStaminaPercent 当前体力百分比（0.0-1.0）
    void Update(float currentTime, float currentStaminaPercent)
    {
        // 检测体力是否刚从25%以上降下来
        if (m_fLastStaminaPercent >= COLLAPSE_THRESHOLD && currentStaminaPercent < COLLAPSE_THRESHOLD)
        {
            // 体力刚从25%以上降下来，启动5秒阻尼过渡
            m_bInCollapseTransition = true;
            m_fCollapseTransitionStartTime = currentTime;
        }
        
        // 检查5秒阻尼过渡是否已过期
        if (m_bInCollapseTransition)
        {
            float elapsedTime = currentTime - m_fCollapseTransitionStartTime;
            if (elapsedTime >= COLLAPSE_TRANSITION_DURATION)
            {
                // 5秒阻尼过渡结束，恢复正常平滑过渡逻辑
                m_bInCollapseTransition = false;
            }
        }
        
        // 如果体力恢复到25%以上，取消阻尼过渡
        if (currentStaminaPercent >= COLLAPSE_THRESHOLD)
        {
            m_bInCollapseTransition = false;
        }
        
        // 更新上次体力百分比
        m_fLastStaminaPercent = currentStaminaPercent;
    }
    
    // 计算阻尼过渡期间的速度倍数
    // @param currentTime 当前世界时间（秒）
    // @param baseSpeedMultiplier 基础速度倍数（正常计算的速度）
    // @return 经过阻尼过渡调整后的速度倍数
    float CalculateTransitionSpeedMultiplier(float currentTime, float baseSpeedMultiplier)
    {
        // 如果不在阻尼过渡期间，直接返回基础速度倍数
        if (!m_bInCollapseTransition)
            return baseSpeedMultiplier;
        
        // 计算阻尼过渡进度（0.0-1.0）
        float elapsedTime = currentTime - m_fCollapseTransitionStartTime;
        float transitionProgress = elapsedTime / COLLAPSE_TRANSITION_DURATION;
        transitionProgress = Math.Clamp(transitionProgress, 0.0, 1.0);
        
        // 使用平滑的S型曲线（ease-in-out）来插值速度
        // smoothstep函数：t²(3-2t)
        float smoothProgress = transitionProgress * transitionProgress * (3.0 - 2.0 * transitionProgress);
        
        // 计算目标速度（阻尼过渡开始时的速度）和结束速度（5%体力时的速度）
        // 开始速度：25%体力时的速度（TARGET_RUN_SPEED_MULTIPLIER = 3.7 m/s对应的倍数）
        float startSpeedMultiplier = RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        
        // 结束速度：5%体力时的速度（大约80%过渡位置）
        // 使用 MIN_LIMP_SPEED_MULTIPLIER 作为下限，计算一个中间值
        float minSpeedMultiplier = RealisticStaminaSpeedSystem.MIN_LIMP_SPEED_MULTIPLIER;
        float endSpeedMultiplier = minSpeedMultiplier + (startSpeedMultiplier - minSpeedMultiplier) * 0.8;
        
        // 在5秒内平滑插值速度（从3.7 m/s逐渐降到约2.2 m/s）
        float transitionSpeedMultiplier = startSpeedMultiplier + (endSpeedMultiplier - startSpeedMultiplier) * smoothProgress;
        
        return transitionSpeedMultiplier;
    }
    
    // 检查是否处于阻尼过渡状态
    // @return 是否正在过渡中
    bool IsInTransition()
    {
        return m_bInCollapseTransition;
    }
    
    // 获取过渡进度（0.0-1.0）
    // @param currentTime 当前世界时间（秒）
    // @return 过渡进度，0.0=刚开始，1.0=已完成
    float GetTransitionProgress(float currentTime)
    {
        if (!m_bInCollapseTransition)
            return 0.0;
        
        float elapsedTime = currentTime - m_fCollapseTransitionStartTime;
        float progress = elapsedTime / COLLAPSE_TRANSITION_DURATION;
        return Math.Clamp(progress, 0.0, 1.0);
    }
    
    // 强制结束过渡（用于外部控制）
    void EndTransition()
    {
        m_bInCollapseTransition = false;
    }
    
    // 获取临界点阈值
    // @return 临界点体力百分比（25%）
    static float GetCollapseThreshold()
    {
        return 0.25; // COLLAPSE_THRESHOLD
    }
    
    // 获取过渡持续时间
    // @return 过渡持续时间（秒）
    static float GetTransitionDuration()
    {
        return 5.0; // COLLAPSE_TRANSITION_DURATION
    }
}
