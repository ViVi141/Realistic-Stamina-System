// UI信号桥接模块
// 负责对接官方UI特效和音效系统，让自定义体力系统状态能够触发视觉模糊和呼吸声
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class UISignalBridge
{
    // ==================== 状态变量 ====================
    protected SignalsManagerComponent m_pSignalsManager; // 信号管理器组件引用
    protected int m_iExhaustionSignal = -1; // "Exhaustion" 信号的ID（-1表示未找到）
    
    // ==================== 公共方法 ====================
    
    // 初始化UI信号桥接系统
    // @param owner 角色实体
    // @return true表示初始化成功，false表示失败
    bool Init(IEntity owner)
    {
        if (!owner)
            return false;
        
        // 获取信号管理器组件
        m_pSignalsManager = SignalsManagerComponent.Cast(owner.FindComponent(SignalsManagerComponent));
        if (!m_pSignalsManager)
            return false;
        
        // 查找 "Exhaustion" 信号（官方UI模糊效果和呼吸声基于此信号）
        m_iExhaustionSignal = m_pSignalsManager.FindSignal("Exhaustion");
        
        return (m_iExhaustionSignal != -1);
    }
    
    // 检查是否已初始化
    // @return true表示已初始化，false表示未初始化
    bool IsInitialized()
    {
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
    void UpdateUISignal(float staminaPercent, bool isExhausted, float currentSpeed, float totalDrainRate)
    {
        // 检查是否已初始化
        if (!IsInitialized())
            return;
        
        float uiExhaustion = 0.0;
        
        if (isExhausted)
        {
            // 彻底没气了，满模糊（达到或超过官方阈值 0.45）
            uiExhaustion = 1.0;
        }
        else
        {
            // 综合考虑体力剩余量和瞬时消耗功率来决定UI表现
            // 基础模糊：由体力剩余量决定（体力越低，模糊越强）
            float fatigueBase = 1.0 - staminaPercent;
            
            // 瞬时模糊：由运动强度/消耗功率决定（消耗越大，模糊越强）
            // totalDrainRate 约 0.001-0.02（每0.2秒），乘以系数转换为信号值
            // 例如：totalDrainRate = 0.01 → intensityBoost = 0.5（50%瞬时模糊）
            float intensityBoost = 0.0;
            if (currentSpeed > 0.05)
            {
                // 计算瞬时消耗功率（归一化）
                // totalDrainRate 通常在 0.001-0.02 之间（每0.2秒）
                // 归一化到 0.0-1.0 范围，然后乘以强度系数
                float normalizedDrainRate = Math.Clamp(totalDrainRate / 0.02, 0.0, 1.0); // 假设最大消耗为0.02
                intensityBoost = normalizedDrainRate * 0.5; // 瞬时模糊最高50%
            }
            
            // 崩溃期强化：体力 < 25% 时模糊感剧增
            // 官方阈值是 0.45，拟真模型的崩溃点是 0.25
            // 当体力掉到 0.25 时，信号值应该超过 0.45，立即触发视觉模糊
            if (staminaPercent <= 0.25)
            {
                // 非线性映射：进入崩溃期，模糊感呈指数级加强
                // 0.25 → 0.5, 0.1 → 0.8, 0.0 → 1.0
                float collapseFactor = (0.25 - staminaPercent) / 0.25; // 0.0-1.0
                float collapseBoost = 0.3 + (collapseFactor * 0.5); // 0.3-0.8（额外强化）
                uiExhaustion = fatigueBase + intensityBoost + collapseBoost;
            }
            else
            {
                // 正常期：基础模糊 + 瞬时模糊
                uiExhaustion = fatigueBase + intensityBoost;
            }
        }
        
        // 限制信号值在有效范围内（0.0-1.0）
        uiExhaustion = Math.Clamp(uiExhaustion, 0.0, 1.0);
        
        // 更新信号值（这会让官方UI特效和音效系统响应）
        m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, uiExhaustion);
    }
}
