// 网络同步管理模块
// 负责管理网络同步的状态变量和速度插值平滑处理
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块
// 注意：RPC方法保留在PlayerBase.c中（必须在modded class中）

class NetworkSyncManager
{
    // ==================== 状态变量 ====================
    protected float m_fServerValidatedSpeedMultiplier = 1.0; // 服务器端验证的速度倍数
    protected bool m_bHasReceivedServerValidation = false;   // 是否已收到过服务器验证值（含 1.0）
    protected float m_fLastReportedStaminaPercent = 1.0; // 上次客户端报告的体力百分比
    protected float m_fLastReportedWeight = 0.0; // 上次客户端报告的重量
    protected const float VALIDATION_TOLERANCE = 0.1; // 验证容差（10%差异视为正常）
    protected const float SYNC_HZ = 60.0; // 同步频率（Hz）
    protected const float NETWORK_SYNC_INTERVAL = 1.0 / SYNC_HZ; // 网络同步间隔（60Hz ≈ 16.67ms）
    protected float m_fLastNetworkSyncTime = 0.0; // 上次网络同步时间

    // 客户端上报速率限制（防滥用）
    protected float m_fLastClientReportTime = 0.0; // 记录上次接受客户端上报的服务器时间（秒）
    protected const float MIN_CLIENT_REPORT_INTERVAL = 1.0 / SYNC_HZ; // 最小允许的客户端上报间隔（60Hz）

    // 网络同步容差优化：连续偏差累计触发
    protected float m_fDeviationStartTime = -1.0; // 偏差开始时间（-1表示无偏差）
    protected const float DEVIATION_TRIGGER_DURATION = 0.0; // 偏差触发持续时间（秒），0=立即下发
    protected const float SMOOTH_TRANSITION_DURATION = 1.0 / SYNC_HZ; // 速度插值平滑过渡时间（60Hz）
    protected float m_fTargetSpeedMultiplier = 1.0; // 目标速度倍数（用于插值）
    protected float m_fSmoothedSpeedMultiplier = 1.0; // 平滑后的速度倍数
    protected float m_fLastSmoothUpdateTime = 0.0; // 上次平滑更新时间（用于内部时间管理）
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    void Initialize()
    {
        m_fServerValidatedSpeedMultiplier = 1.0;
        m_bHasReceivedServerValidation = false;
        m_fLastReportedStaminaPercent = 1.0;
        m_fLastReportedWeight = 0.0;
        m_fLastNetworkSyncTime = 0.0;
        m_fDeviationStartTime = -1.0;
        m_fTargetSpeedMultiplier = 1.0;
        m_fSmoothedSpeedMultiplier = 1.0;
        m_fLastSmoothUpdateTime = 0.0;
        m_fLastClientReportTime = 0.0;
    }
    
    // 检查是否需要发送网络同步（60Hz）
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

    // 验证并接受客户端报告（速率限制）
    // @param currentTime 当前服务器时间
    // @param isCriticalData 是否为关键数据（关键数据允许即时上报）
    // @return true表示接受报告，false表示拒绝
    bool AcceptClientReport(float currentTime, bool isCriticalData = false)
    {
        // 关键数据允许即时上报（体力耗尽、突发状态等）
        if (isCriticalData)
        {
            m_fLastClientReportTime = currentTime;
            return true;
        }

        if (currentTime - m_fLastClientReportTime >= MIN_CLIENT_REPORT_INTERVAL)
        {
            m_fLastClientReportTime = currentTime;
            return true;
        }
        // 报告过于频繁，拒绝
        return false;
    }
    
    // 处理服务器端验证的速度倍数
    // @param finalSpeedMultiplier 客户端计算的速度倍数
    // @return 目标速度倍数（可能是服务器端验证的值）
    float GetTargetSpeedMultiplier(float finalSpeedMultiplier)
    {
        float targetSpeedMultiplier = finalSpeedMultiplier;
        
        // 服务器权威：已收到验证值时，始终以服务器值为目标（插值平滑过渡）
        if (m_bHasReceivedServerValidation && m_fServerValidatedSpeedMultiplier > 0.0)
        {
            targetSpeedMultiplier = m_fServerValidatedSpeedMultiplier;
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
            if (m_fDeviationStartTime < 0.0)
            {
                m_fDeviationStartTime = currentTime;
            }
            float deviationDuration = currentTime - m_fDeviationStartTime;
            if (deviationDuration >= DEVIATION_TRIGGER_DURATION)
            {
                m_fDeviationStartTime = -1.0;
                return true;
            }
            return false;
        }
        else
        {
            m_fDeviationStartTime = -1.0;
            return false;
        }
    }
    
    // 设置服务器端验证的速度倍数
    // @param speedMultiplier 服务器端验证的速度倍数
    void SetServerValidatedSpeedMultiplier(float speedMultiplier)
    {
        m_fServerValidatedSpeedMultiplier = speedMultiplier;
        m_bHasReceivedServerValidation = true;
    }
    
    // 获取服务器端验证的速度倍数
    // @return 服务器端验证的速度倍数
    float GetServerValidatedSpeedMultiplier()
    {
        return m_fServerValidatedSpeedMultiplier;
    }

    // 判断服务器是否已设置验证值（用于客户端决定是否优先使用服务器值）
    // 修复：此前用「值≠1.0」判断，导致满速时误判为无验证；现用「是否收到过」判断
    bool HasServerValidation()
    {
        return m_bHasReceivedServerValidation;
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
