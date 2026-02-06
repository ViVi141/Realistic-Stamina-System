// 网络同步管理模块
// 负责管理网络同步的状态变量和速度插值平滑处理
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块
// 注意：RPC方法保留在PlayerBase.c中（必须在modded class中）

class NetworkSyncManager
{
    // ==================== 状态变量 ====================
    protected float m_fServerValidatedSpeedMultiplier = 1.0; // 服务器端验证的速度倍数
    protected float m_fLastReportedStaminaPercent = 1.0; // 上次客户端报告的体力百分比
    protected float m_fLastReportedWeight = 0.0; // 上次客户端报告的重量
    protected const float VALIDATION_TOLERANCE = 0.1; // 验证容差（10%差异视为正常）
    protected const float NETWORK_SYNC_INTERVAL = 1.0; // 网络同步间隔（秒）
    protected float m_fLastNetworkSyncTime = 0.0; // 上次网络同步时间
    
    // 网络同步容差优化：连续偏差累计触发
    protected float m_fDeviationStartTime = -1.0; // 偏差开始时间（-1表示无偏差）
    protected const float DEVIATION_TRIGGER_DURATION = 2.0; // 偏差触发持续时间（秒），连续超过此时间才触发同步
    protected const float SMOOTH_TRANSITION_DURATION = 0.1; // 速度插值平滑过渡时间（秒）
    protected float m_fTargetSpeedMultiplier = 1.0; // 目标速度倍数（用于插值）
    protected float m_fSmoothedSpeedMultiplier = 1.0; // 平滑后的速度倍数
    protected float m_fLastSmoothUpdateTime = 0.0; // 上次平滑更新时间（用于内部时间管理）
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    void Initialize()
    {
        m_fServerValidatedSpeedMultiplier = 1.0;
        m_fLastReportedStaminaPercent = 1.0;
        m_fLastReportedWeight = 0.0;
        m_fLastNetworkSyncTime = 0.0;
        m_fDeviationStartTime = -1.0;
        m_fTargetSpeedMultiplier = 1.0;
        m_fSmoothedSpeedMultiplier = 1.0;
        m_fLastSmoothUpdateTime = 0.0;
    }
    
    // 检查是否需要发送网络同步（每1秒一次）
    // @param currentTime 当前世界时间
    // @return true表示需要同步，false表示不需要
    bool ShouldSync(float currentTime)
    {
        if (currentTime - m_fLastNetworkSyncTime >= NETWORK_SYNC_INTERVAL)
        {
            m_fLastNetworkSyncTime = currentTime;
            return true;
        }
        return false;
    }
    
    // 处理服务器端验证的速度倍数
    // @param finalSpeedMultiplier 客户端计算的速度倍数
    // @return 目标速度倍数（可能是服务器端验证的值）
    float GetTargetSpeedMultiplier(float finalSpeedMultiplier)
    {
        float targetSpeedMultiplier = finalSpeedMultiplier;
        
        // 如果服务器端验证的速度倍数与客户端计算的不同，使用插值平滑过渡
        if (m_fServerValidatedSpeedMultiplier > 0.0 && m_fServerValidatedSpeedMultiplier != finalSpeedMultiplier)
        {
            // 检查是否需要使用服务器端的值（差异较大时）
            float serverDifference = Math.AbsFloat(finalSpeedMultiplier - m_fServerValidatedSpeedMultiplier);
            if (serverDifference > VALIDATION_TOLERANCE * 2.0) // 双倍容差时才使用服务器端值
            {
                // 使用服务器端的值作为目标，但使用插值平滑过渡
                targetSpeedMultiplier = m_fServerValidatedSpeedMultiplier;
            }
        }
        
        m_fTargetSpeedMultiplier = targetSpeedMultiplier;
        return targetSpeedMultiplier;
    }
    
    // 平滑插值速度倍数
    // @param currentTime 当前世界时间
    // @return 平滑后的速度倍数
    float GetSmoothedSpeedMultiplier(float currentTime)
    {
        float targetSpeedMultiplier = m_fTargetSpeedMultiplier;
        
        // 首次调用时初始化时间
        if (m_fLastSmoothUpdateTime <= 0.0)
            m_fLastSmoothUpdateTime = currentTime;
        
        float timeDelta = currentTime - m_fLastSmoothUpdateTime;
        
        // 平滑插值：从当前速度倍数过渡到目标速度倍数
        // 使用线性插值，过渡时间为 SMOOTH_TRANSITION_DURATION
        if (timeDelta > 0.0 && timeDelta < 1.0 && Math.AbsFloat(m_fSmoothedSpeedMultiplier - targetSpeedMultiplier) > 0.001)
        {
            // 计算插值系数（每秒过渡速度）
            float lerpSpeed = timeDelta / SMOOTH_TRANSITION_DURATION;
            lerpSpeed = Math.Clamp(lerpSpeed, 0.0, 1.0); // 限制在0-1之间
            
            // 线性插值
            m_fSmoothedSpeedMultiplier = Math.Lerp(m_fSmoothedSpeedMultiplier, targetSpeedMultiplier, lerpSpeed);
        }
        else
        {
            // 无需插值：直接使用目标值
            m_fSmoothedSpeedMultiplier = targetSpeedMultiplier;
        }
        
        // 更新内部时间
        m_fLastSmoothUpdateTime = currentTime;
        
        return m_fSmoothedSpeedMultiplier;
    }
    
    // 处理服务器端验证（在RPC_UpdateStaminaState中使用）
    // @param speedDifference 速度差异
    // @param currentTime 当前时间
    // @return true表示需要触发同步，false表示不需要
    bool ProcessDeviation(float speedDifference, float currentTime)
    {
        if (speedDifference > VALIDATION_TOLERANCE)
        {
            // 偏差超过容差：检查是否为连续偏差
            if (m_fDeviationStartTime < 0.0)
            {
                // 首次检测到偏差：记录开始时间
                m_fDeviationStartTime = currentTime;
                return false;
            }
            else
            {
                // 已有偏差：检查持续时间
                float deviationDuration = currentTime - m_fDeviationStartTime;
                
                if (deviationDuration >= DEVIATION_TRIGGER_DURATION)
                {
                    // 连续偏差持续时间超过阈值：触发同步
                    m_fDeviationStartTime = -1.0;
                    return true;
                }
                // 否则：偏差持续时间不足，不触发同步（容忍小幅度偏差）
                return false;
            }
        }
        else
        {
            // 偏差在容差范围内：验证通过
            // 重置偏差计时器
            m_fDeviationStartTime = -1.0;
            return false;
        }
    }
    
    // 设置服务器端验证的速度倍数
    // @param speedMultiplier 服务器端验证的速度倍数
    void SetServerValidatedSpeedMultiplier(float speedMultiplier)
    {
        m_fServerValidatedSpeedMultiplier = speedMultiplier;
    }
    
    // 获取服务器端验证的速度倍数
    // @return 服务器端验证的速度倍数
    float GetServerValidatedSpeedMultiplier()
    {
        return m_fServerValidatedSpeedMultiplier;
    }

    // 判断服务器是否已设置验证值（用于客户端决定是否优先使用服务器值）
    bool HasServerValidation()
    {
        return Math.AbsFloat(m_fServerValidatedSpeedMultiplier - 1.0) > 0.0001;
    }
    
    // 更新报告的状态值
    // @param staminaPercent 体力百分比
    // @param weight 重量
    void UpdateReportedState(float staminaPercent, float weight)
    {
        m_fLastReportedStaminaPercent = staminaPercent;
        m_fLastReportedWeight = weight;
    }
    
    // 获取上次报告的体力百分比
    // @return 上次报告的体力百分比
    float GetLastReportedStaminaPercent()
    {
        return m_fLastReportedStaminaPercent;
    }
    
    // 获取上次报告的重量
    // @return 上次报告的重量
    float GetLastReportedWeight()
    {
        return m_fLastReportedWeight;
    }
    
    // 获取验证容差
    // @return 验证容差
    float GetValidationTolerance()
    {
        return VALIDATION_TOLERANCE;
    }
}
