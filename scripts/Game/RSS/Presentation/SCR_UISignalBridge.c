// UI信号桥接模块
// v4: 对接官方UI特效和音效系统，让自定义体力系统状态能够触发视觉模糊和呼吸声
// v5: 通过 ScriptInvoker 事件委托连接无氧/代谢逻辑到 HUD 系统
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class UISignalBridge
{
    // ==================== 静态单例（供 v5 事件触发方使用）====================
    protected static UISignalBridge s_Instance;

    static UISignalBridge GetInstance()
    {
        return s_Instance;
    }

    // ==================== 状态变量 ====================
    protected IEntity m_pOwner;
    protected SignalsManagerComponent m_pSignalsManager; // 信号管理器组件引用
    protected int m_iExhaustionSignal = -1; // "Exhaustion" 信号的ID（-1表示未找到）

    // 体力显示平滑：与 HUD 一致，避免 Exhaustion 信号驱动效果抽动
    protected static float s_fSmoothedStaminaForSignal = 1.0;
    protected static const float STAMINA_SIGNAL_SMOOTH_ALPHA = 0.4;

    // ==================== v5 事件数据 ====================
    protected float m_fLastCooldownDuration;
    protected bool m_bLastIsFullDepletion;
    protected int m_iLastSeverityLevel;

    protected ref ScriptInvokerVoid m_OnSprintCooldownStarted;
    protected ref ScriptInvokerVoid m_OnMetabolicOverload;
    protected ref ScriptInvokerVoid m_OnMetabolicNormal;

    // ==================== 公共方法 ====================
    
    // 初始化UI信号桥接系统
    // @param owner 角色实体
    // @return true表示初始化成功，false表示失败
    bool Init(IEntity owner)
    {
        m_pOwner = owner;
        s_Instance = this;
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

    //! 实体删除时清理所有引用，防止悬空指针访问。
    void Cleanup()
    {
        if (s_Instance == this)
            s_Instance = null;
        m_pOwner = null;
        m_pSignalsManager = null;
        m_iExhaustionSignal = -1;
        m_OnSprintCooldownStarted = null;
        m_OnMetabolicOverload = null;
        m_OnMetabolicNormal = null;
    }

    // ==================== v5 ScriptInvoker 访问器 ====================

    ScriptInvokerVoid GetOnSprintCooldownStarted()
    {
        if (!m_OnSprintCooldownStarted)
            m_OnSprintCooldownStarted = new ScriptInvokerVoid();
        return m_OnSprintCooldownStarted;
    }

    ScriptInvokerVoid GetOnMetabolicOverload()
    {
        if (!m_OnMetabolicOverload)
            m_OnMetabolicOverload = new ScriptInvokerVoid();
        return m_OnMetabolicOverload;
    }

    ScriptInvokerVoid GetOnMetabolicNormal()
    {
        if (!m_OnMetabolicNormal)
            m_OnMetabolicNormal = new ScriptInvokerVoid();
        return m_OnMetabolicNormal;
    }

    float GetLastCooldownDuration() { return m_fLastCooldownDuration; }
    bool GetLastIsFullDepletion() { return m_bLastIsFullDepletion; }
    int GetLastSeverityLevel() { return m_iLastSeverityLevel; }

    // ==================== v5 事件触发接口 ====================

    void OnSprintCooldownStarted(float duration, bool isFullDepletion)
    {
        m_fLastCooldownDuration = duration;
        m_bLastIsFullDepletion = isFullDepletion;
        if (m_OnSprintCooldownStarted)
            m_OnSprintCooldownStarted.Invoke();
    }

    void OnMetabolicOverload(bool severe)
    {
        if (severe)
            m_iLastSeverityLevel = 1;
        else
            m_iLastSeverityLevel = 0;
        if (m_OnMetabolicOverload)
            m_OnMetabolicOverload.Invoke();
    }

    void OnMetabolicNormal()
    {
        if (m_OnMetabolicNormal)
            m_OnMetabolicNormal.Invoke();
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
