// 体力系统完全覆盖
// 完全禁用游戏原生的体力恢复和消耗计算，只使用自定义系统
modded class SCR_CharacterStaminaComponent : CharacterStaminaComponent
{
    // 上次的体力值（用于检测非预期的体力变化）
    protected float m_fLastKnownStamina = 1.0;
    
    // 标记：是否允许原生体力系统工作
    // false = 完全禁用原生系统，只使用自定义系统
    protected bool m_bAllowNativeStaminaSystem = false;
    
    // 目标体力值（由我们的自定义系统控制）
    protected float m_fTargetStamina = 1.0;
    
    // 主动监控标志（控制是否继续监控）
    protected bool m_bIsMonitoring = false;
    
    // 标记：是否是我们自己的调用（避免循环）
    protected bool m_bIsOurOwnCall = false;
    
    // 关键发现：
    // 1. OnStaminaDrain 是一个 event，每次体力值改变时都会触发（包括 AddStamina 调用）
    // 2. AddStamina 是 proto external，无法覆盖，但调用它会触发 OnStaminaDrain 事件
    // 3. 因此，我们可以通过覆盖 OnStaminaDrain 来拦截所有体力值变化
    
    // 每帧/每次体力变化时，拦截并重新设置体力值
    // 这确保只有我们的自定义系统可以改变体力值
    // 注意：原生系统调用 AddStamina 时会触发此事件，我们可以在这里拦截
    override void OnStaminaDrain(float pDrain)
    {
        // 如果允许原生系统工作，调用父类方法
        if (m_bAllowNativeStaminaSystem)
        {
            super.OnStaminaDrain(pDrain);
            return;
        }
        
        // 如果是我们自己的调用（通过 SetTargetStamina），允许执行
        if (m_bIsOurOwnCall)
        {
            super.OnStaminaDrain(pDrain);
            return;
        }
        
        // 检测到原生系统试图修改体力值！
        // 记录调试信息（每5次输出一次，避免日志过多）
        static int interceptCounter = 0;
        interceptCounter++;
        if (interceptCounter >= 5)
        {
            float currentStamina = GetStamina();
            PrintFormat("[StaminaOverride] 拦截到原生系统体力修改！pDrain=%1%%, 当前体力=%2%%, 目标=%3%%", 
                Math.Round(pDrain * 100.0).ToString(),
                Math.Round(currentStamina * 100.0).ToString(),
                Math.Round(m_fTargetStamina * 100.0).ToString());
            interceptCounter = 0;
        }
        
        // 完全禁用原生系统：
        // 1. 不调用父类方法（不触发原生体力恢复/消耗）
        // 2. 立即纠正任何非预期的体力变化
        // 注意：原生系统调用 AddStamina 时会触发此事件，我们在这里拦截并纠正
        CorrectStaminaToTarget();
    }
    
    // 纠正体力值到目标值（主动监控机制）
    void CorrectStaminaToTarget()
    {
        float currentStamina = GetStamina();
        
        // 如果体力值 < 0（没有体力组件），不处理
        if (currentStamina < 0.0)
            return;
        
        // 如果发现非预期的体力变化（原生系统试图改变体力），立即恢复到目标值
        if (Math.AbsFloat(currentStamina - m_fTargetStamina) > 0.001)
        {
            // 计算需要调整的体力量，使其回到目标值
            float correction = m_fTargetStamina - currentStamina;
            
            // 标记这是我们自己的调用，允许执行
            m_bIsOurOwnCall = true;
            // 使用父类的 AddStamina 来设置体力值
            super.AddStamina(correction);
            m_bIsOurOwnCall = false;
        }
        
        // 更新记录的体力值（使用实际值，而不是目标值）
        m_fLastKnownStamina = GetStamina();
    }
    
    // 启动主动监控（每帧检查，确保完全覆盖原生系统）
    // 原生系统可能每帧都在恢复体力，所以需要每帧检查
    void StartStaminaMonitor()
    {
        // 如果已经在监控，不重复启动
        if (m_bIsMonitoring)
            return;
        
        // 设置监控标志
        m_bIsMonitoring = true;
        
        // 启动监控循环（高频检查，确保完全覆盖原生系统）
        // 使用合理的间隔（50ms = 每秒检查20次）
        // 这个频率既能及时拦截原生系统，又不会造成性能负担
        // 考虑到我们的主要更新频率是200ms，50ms的监测频率已经足够
        GetGame().GetCallqueue().CallLater(MonitorStamina, 50, false);
    }
    
    // 停止主动监控
    void StopStaminaMonitor()
    {
        // 设置标志为 false，停止监控循环
        m_bIsMonitoring = false;
    }
    
    // 监控体力值（定期调用，确保完全覆盖原生系统）
    // 原生系统可能每帧都在恢复体力，所以需要频繁检查
    void MonitorStamina()
    {
        // 如果允许原生系统工作，或监控已停止，不继续监控
        if (m_bAllowNativeStaminaSystem || !m_bIsMonitoring)
            return;
        
        // 获取当前体力值
        float currentStamina = GetStamina();
        
        // 如果发现非预期的体力变化（原生系统试图改变体力），立即恢复到目标值
        // 降低阈值到0.0005（0.05%），更敏感地检测原生系统干扰
        if (currentStamina >= 0.0 && Math.AbsFloat(currentStamina - m_fTargetStamina) > 0.0005)
        {
            // 检测到原生系统干扰，记录调试信息
            float deviation = currentStamina - m_fTargetStamina;
            
            // 如果偏差超过0.5%，输出警告（降低阈值，更敏感）
            static int warningCounter = 0;
            warningCounter++;
            if (Math.AbsFloat(deviation) > 0.005 && warningCounter >= 10) // 每10次输出一次
            {
                PrintFormat("[StaminaOverride] 检测到原生系统干扰！当前体力=%1%%，目标=%2%%，偏差=%3%%", 
                    Math.Round(currentStamina * 100.0).ToString(),
                    Math.Round(m_fTargetStamina * 100.0).ToString(),
                    Math.Round(deviation * 100.0).ToString());
                warningCounter = 0;
            }
            
            // 立即纠正体力值
            CorrectStaminaToTarget();
        }
        
        // 继续下一次监控（每50ms，每秒检查20次）
        // 这个频率既能及时拦截原生系统，又不会造成性能负担
        GetGame().GetCallqueue().CallLater(MonitorStamina, 50, false);
    }
    
    // 设置目标体力值（由我们的自定义系统调用）
    // 这是唯一允许改变体力的方式
    void SetTargetStamina(float targetStamina)
    {
        m_fTargetStamina = Math.Clamp(targetStamina, 0.0, 1.0);
        
        // 立即应用目标体力值（直接调用父类的 AddStamina）
        float currentStamina = GetStamina();
        if (currentStamina >= 0.0)
        {
            float correction = m_fTargetStamina - currentStamina;
            
            // 标记这是我们自己的调用，允许执行
            // 注意：调用 AddStamina 会触发 OnStaminaDrain 事件
            m_bIsOurOwnCall = true;
            // 直接调用父类方法设置体力值
            super.AddStamina(correction);
            m_bIsOurOwnCall = false;
            
            // 立即再次检查，确保设置成功（原生系统可能在设置后立即恢复）
            float verifyStamina = GetStamina();
            if (Math.AbsFloat(verifyStamina - m_fTargetStamina) > 0.001)
            {
                // 如果设置后立即被改变，再次纠正
                float reCorrection = m_fTargetStamina - verifyStamina;
                m_bIsOurOwnCall = true;
                super.AddStamina(reCorrection);
                m_bIsOurOwnCall = false;
            }
        }
        
        // 更新记录的体力值（使用实际设置后的值）
        float finalStamina = GetStamina();
        if (finalStamina >= 0.0)
            m_fLastKnownStamina = finalStamina;
    }
    
    // 获取目标体力值
    float GetTargetStamina()
    {
        return m_fTargetStamina;
    }
    
    // 允许/禁用原生体力系统
    // false = 完全禁用原生系统（推荐）
    // true = 允许原生系统工作（不推荐）
    void SetAllowNativeStaminaSystem(bool allow)
    {
        m_bAllowNativeStaminaSystem = allow;
        
        // 根据设置启动或停止监控
        if (allow)
        {
            StopStaminaMonitor();
        }
        else
        {
            StartStaminaMonitor();
        }
    }
    
    // 组件初始化时启动监控
    // 注意：使用 OnInit 而不是 OnPostInit（如果基类支持）
    // 如果基类不支持 OnInit，监控将在 SetAllowNativeStaminaSystem(false) 时启动
    
    // 获取是否允许原生体力系统
    bool GetAllowNativeStaminaSystem()
    {
        return m_bAllowNativeStaminaSystem;
    }
}
