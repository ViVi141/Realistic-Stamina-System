// UI信号桥接模块
// 负责对接官方UI特效和音效系统，让自定义体力系统状态能够触发视觉模糊和呼吸声
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class UISignalBridge
{
    // ==================== 状态变量 ====================
    protected IEntity m_pOwner;
    protected SignalsManagerComponent m_pSignalsManager; // 信号管理器组件引用
    protected int m_iExhaustionSignal = -1; // "Exhaustion" 信号的ID（-1表示未找到）

    // 体力显示平滑：与 HUD 一致，避免 Exhaustion 信号驱动效果抽动
    protected static float s_fSmoothedStaminaForSignal = 1.0;
    protected static const float STAMINA_SIGNAL_SMOOTH_ALPHA = 0.4;
    
    // ==================== 公共方法 ====================
    
    // 初始化UI信号桥接系统
    // @param owner 角色实体
    // @return true表示初始化成功，false表示失败
    bool Init(IEntity owner)
    {
        m_pOwner = owner;
        return TryResolveSignals();
    }
    
    // 检查是否已初始化
    // @return true表示已初始化，false表示未初始化
    bool IsInitialized()
    {
        if (m_pSignalsManager == null || m_iExhaustionSignal == -1)
            TryResolveSignals();
        return (m_pSignalsManager != null && m_iExhaustionSignal != -1);
    }
    
    // 获取信号管理器组件引用（用于其他模块）
    // @return SignalsManagerComponent 引用，如果未初始化则返回 null
    SignalsManagerComponent GetSignalsManager()
    {
        return m_pSignalsManager;
    }
    
    // 获取 Exhaustion 信号ID（用于其他模块）
    // @return 信号ID，如果未初始化则返回 -1
    int GetExhaustionSignalID()
    {
        return m_iExhaustionSignal;
    }
    
    // 更新UI信号值（根据体力状态、消耗功率、速度等计算）
    // 官方UI特效阈值：STAMINA_EFFECT_THRESHOLD = 0.45
    // 拟真模型崩溃点：EXHAUSTION_THRESHOLD = 0.25
    // 
    // 映射逻辑：
    // 1. 基础模糊（由体力剩余量决定）
    // 2. 瞬时模糊（由运动强度/消耗功率决定）
    // 3. 崩溃期强化（体力 < 25% 时模糊感剧增）
    // 
    // @param staminaPercent 当前体力百分比（0.0-1.0）
    // @param isExhausted 是否精疲力尽
    // @param currentSpeed 当前移动速度（米/秒）
    // @param totalDrainRate 总消耗率（每0.2秒的消耗）
    void UpdateUISignal(float staminaPercent, bool isExhausted, float currentSpeed, float totalDrainRate, bool forceOverdoseEffect = false)
    {
        // 检查是否已初始化
        if (!IsInitialized())
            return;

        if (forceOverdoseEffect)
        {
            m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, SCR_CombatStimConstants.OD_EXHAUSTION_SIGNAL_VALUE);
            return;
        }

        // 已移除体力驱动的画面效果（原 40% 以下模糊、呼吸声等）
        m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, 0.0);
    }

    // 强制设置 Exhaustion 信号（用于 OD 中毒画面等高优先级效果）
    void SetExhaustionSignalOverride(float value)
    {
        if (!IsInitialized())
            return;
        m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, value);
    }

    protected bool TryResolveSignals()
    {
        if (!m_pOwner)
            return false;

        m_pSignalsManager = SignalsManagerComponent.Cast(m_pOwner.FindComponent(SignalsManagerComponent));
        if (!m_pSignalsManager)
            return false;

        m_iExhaustionSignal = m_pSignalsManager.FindSignal("Exhaustion");
        return (m_iExhaustionSignal != -1);
    }
}
