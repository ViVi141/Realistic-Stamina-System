// 网络同步管理模块
// 负责管理网络同步的状态变量和速度插值平滑处理
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块
// 注意：RPC方法保留在PlayerBase.c中（必须在modded class中）
//
// v3.20.0 性能优化：分层同步策略
//   BASE  (20Hz) — 速度倍数、体力百分比等常规字段
//   CRITICAL (60Hz) — 精疲力尽、战斗状态等突发关键变化
//   STABLE (5Hz)  — 配置哈希等极少变化的稳定字段
// 相比原始全字段 60Hz，可减少约 30-40% 的 RPC 数量。

class NetworkSyncManager
{
    // ==================== 状态变量 ====================
    protected float m_fServerValidatedSpeedMultiplier = 1.0; // 服务器端验证的速度倍数
    protected bool m_bHasReceivedServerValidation = false;   // 是否已收到过服务器验证值（含 1.0）
    protected float m_fLastReportedStaminaPercent = 1.0; // 上次客户端报告的体力百分比
    protected float m_fLastReportedWeight = 0.0; // 上次客户端报告的重量
    protected const float VALIDATION_TOLERANCE = 0.1; // 验证容差（10%差异视为正常）

    // ── 分层同步频率常量 ──────────────────────────────────────────────────
    protected const float BASE_SYNC_HZ = 20.0;      // 基础字段：速度倍数、体力% (20Hz)
    protected const float CRITICAL_SYNC_HZ = 60.0;  // 关键状态变化：精疲力尽、战斗 (60Hz)
    protected const float STABLE_SYNC_HZ = 5.0;     // 稳定字段：配置哈希 (5Hz)

    // 预计算同步间隔（秒），避免 ShouldSync / AcceptClientReport 每次调用都做浮点除法
    protected const float BASE_SYNC_INTERVAL = 1.0 / BASE_SYNC_HZ;         // 0.05s
    protected const float CRITICAL_SYNC_INTERVAL = 1.0 / CRITICAL_SYNC_HZ; // ~0.01667s
    protected const float STABLE_SYNC_INTERVAL = 1.0 / STABLE_SYNC_HZ;     // 0.2s

    // 各层独立时间戳（避免不同层互相干扰）
    protected float m_fLastBaseSyncTime = 0.0;       // 上次基础同步时间
    protected float m_fLastCriticalSyncTime = 0.0;   // 上次关键同步时间
    protected float m_fLastStableSyncTime = 0.0;     // 上次稳定同步时间

    // 兼容旧调用：保留 m_fLastNetworkSyncTime（指向基础层时间戳）
    protected float m_fLastNetworkSyncTime = 0.0;

    // 客户端上报速率限制（防滥用）
    protected float m_fLastClientReportTime = 0.0;   // 上次接受客户端上报的服务器时间（秒）
    protected float m_fLastCriticalReportTime = 0.0; // 关键数据上次接受时间（独立限流）

    // 网络同步容差优化：连续偏差累计触发
    protected float m_fDeviationStartTime = -1.0; // 偏差开始时间（-1表示无偏差）
    protected const float DEVIATION_TRIGGER_DURATION = 0.0; // 偏差触发持续时间（秒），0=立即下发

    // 速度插值平滑：0.15s 过渡窗口（约9帧@60fps），确保在 50ms 更新间隔下插值实际生效。
    // 原值 0.05s 导致 lerpSpeed = timeDelta/0.05 ≥ 1.0，平滑形同虚设。
    protected const float SMOOTH_TRANSITION_DURATION = 0.15;
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
        m_fLastBaseSyncTime = 0.0;
        m_fLastCriticalSyncTime = 0.0;
        m_fLastStableSyncTime = 0.0;
        m_fLastNetworkSyncTime = 0.0;
        m_fDeviationStartTime = -1.0;
        m_fTargetSpeedMultiplier = 1.0;
        m_fSmoothedSpeedMultiplier = 1.0;
        m_fLastSmoothUpdateTime = 0.0;
        m_fLastClientReportTime = 0.0;
        m_fLastCriticalReportTime = 0.0;
    }

    // 检查是否需要发送网络同步（分层策略）
    // @param currentTime 当前世界时间
    // @param syncType    同步类型：0=基础(20Hz) 1=关键(60Hz) 2=稳定(5Hz)
    // @return true 表示需要同步，false 表示不需要
    bool ShouldSync(float currentTime, int syncType = 0)
    {
        // 使用预计算间隔常量，避免每次调用做浮点除法
        if (syncType == 1)
        {
            if (currentTime - m_fLastCriticalSyncTime >= CRITICAL_SYNC_INTERVAL)
            {
                m_fLastCriticalSyncTime = currentTime;
                return true;
            }
            return false;
        }
        else if (syncType == 2)
        {
            if (currentTime - m_fLastStableSyncTime >= STABLE_SYNC_INTERVAL)
            {
                m_fLastStableSyncTime = currentTime;
                return true;
            }
            return false;
        }
        else
        {
            if (currentTime - m_fLastBaseSyncTime >= BASE_SYNC_INTERVAL)
            {
                m_fLastBaseSyncTime = currentTime;
                m_fLastNetworkSyncTime = currentTime; // 保持兼容
                return true;
            }
            return false;
        }
    }

    // 验证并接受客户端报告（速率限制）
    // @param currentTime 当前服务器时间
    // @param isCriticalData 是否为关键数据（关键数据使用独立的 60Hz 限流，而非无限制）
    // @return true表示接受报告，false表示拒绝
    bool AcceptClientReport(float currentTime, bool isCriticalData = false)
    {
        if (isCriticalData)
        {
            // 关键数据使用 60Hz 独立限流，使用预计算间隔常量
            if (currentTime - m_fLastCriticalReportTime >= CRITICAL_SYNC_INTERVAL)
            {
                m_fLastCriticalReportTime = currentTime;
                return true;
            }
            return false;
        }

        if (currentTime - m_fLastClientReportTime >= BASE_SYNC_INTERVAL)
        {
            m_fLastClientReportTime = currentTime;
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
        // 使用线性插值，过渡时间为 SMOOTH_TRANSITION_DURATION（0.15s）
        // 在 50ms 更新间隔下 lerpSpeed = 0.05/0.15 ≈ 0.33，插值实际生效
        if (timeDelta > 0.0 && timeDelta < 1.0 && Math.AbsFloat(m_fSmoothedSpeedMultiplier - targetSpeedMultiplier) > 0.001)
        {
            float lerpSpeed = timeDelta / SMOOTH_TRANSITION_DURATION;
            lerpSpeed = Math.Clamp(lerpSpeed, 0.0, 1.0);
            
            m_fSmoothedSpeedMultiplier = Math.Lerp(m_fSmoothedSpeedMultiplier, targetSpeedMultiplier, lerpSpeed);
        }
        else
        {
            m_fSmoothedSpeedMultiplier = targetSpeedMultiplier;
        }
        
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
