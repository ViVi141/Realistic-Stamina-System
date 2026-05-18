//! RSS AI Manager — 统一 AI 子系统管理器
//!
//! 集中管理所有 AI 体力相关模块的调用频率、状态生命周期和模块编排。
//! PlayerBase 仅需持有一个 ref SCR_RSS_AIManager，每 tick 调用 .Tick() 一次。
//!
//! 职责范围：
//!   - 行为层节流（500ms）
//!   - 体力状态机 Tick + SpeedCap / IntentFilter / CombatDecay 模块链

// ============================================================================
// Tick 返回结果
// ============================================================================
enum ERSS_AIManagerTickResult
{
    AI_TICK_NORMAL,            // 正常执行了 AI tick
    AI_TICK_THROTTLED          // 行为层被 500ms 节流跳过
}

// ============================================================================
// AI Manager
// ============================================================================
class SCR_RSS_AIManager
{
    // ── 行为层节流（500ms） ──
    protected float m_fLastBehaviorTickTime;
    protected static const float BEHAVIOR_TICK_INTERVAL_SEC = 0.5;

    // ── AI 个体状态 ──
    protected ERSS_AIStaminaState m_eStaminaState;
    protected float m_fTimeStationarySec;
    protected float m_fLastDebugPrintTime;

    // ========================================================================
    //  构造
    // ========================================================================
    void SCR_RSS_AIManager()
    {
        m_eStaminaState = ERSS_AIStaminaState.FRESH;
        m_fTimeStationarySec = 0.0;
        m_fLastDebugPrintTime = -1.0;
        m_fLastBehaviorTickTime = -1.0;
    }

    // ========================================================================
    //  主入口：每 tick 由 PlayerBase 调用一次
    // ========================================================================
    ERSS_AIManagerTickResult Tick(
        IEntity owner,
        float currentTime,
        float timeDeltaSec,
        float staminaPercent,
        float fatigueVal,
        float currentSpeed,
        bool isPlayer)
    {
        if (!owner)
            return ERSS_AIManagerTickResult.AI_TICK_NORMAL;
        if (!Replication.IsServer())
            return ERSS_AIManagerTickResult.AI_TICK_NORMAL;
        if (isPlayer)
            return ERSS_AIManagerTickResult.AI_TICK_NORMAL;

        // ── 行为层节流（500ms） ──
        if (currentTime - m_fLastBehaviorTickTime < BEHAVIOR_TICK_INTERVAL_SEC)
            return ERSS_AIManagerTickResult.AI_TICK_THROTTLED;

        m_fLastBehaviorTickTime = currentTime;

        // ── 静止时间累计 ──
        if (currentSpeed < 0.05)
            m_fTimeStationarySec = m_fTimeStationarySec + timeDeltaSec;
        else
            m_fTimeStationarySec = 0.0;

        // ── 体力状态机 ──
        ERSS_AIStaminaState prevState = m_eStaminaState;
        ERSS_AIStaminaState aiState = SCR_RSS_AIStaminaState.Tick(
            staminaPercent,
            fatigueVal,
            currentSpeed < 0.05,
            m_fTimeStationarySec,
            m_eStaminaState);

        // ── 模块链按序 Apply ──
        bool isThreatened = false;  // 群组功能已移除，默认非威胁状态
        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            owner.FindComponent(SCR_CharacterControllerComponent));
        if (ctrl)
        {
            SCR_RSS_AISpeedCap.Apply(ctrl, owner, aiState, staminaPercent, isThreatened);
        }
        SCR_RSS_AIIntentFilter.Apply(owner, aiState, prevState, isThreatened);
        SCR_RSS_AICombatDecay.Apply(owner, aiState);

        return ERSS_AIManagerTickResult.AI_TICK_NORMAL;
    }

    // ========================================================================
    //  公开查询方法
    // ========================================================================

    ERSS_AIStaminaState GetStaminaState()
    {
        return m_eStaminaState;
    }

    float GetDebugLastPrintTime()
    {
        return m_fLastDebugPrintTime;
    }

    void SetDebugLastPrintTime(float t)
    {
        m_fLastDebugPrintTime = t;
    }

    // ========================================================================
    //  生命周期管理
    // ========================================================================

    void OnEntityDeleted()
    {
        m_fLastBehaviorTickTime = -1.0;
    }
}
