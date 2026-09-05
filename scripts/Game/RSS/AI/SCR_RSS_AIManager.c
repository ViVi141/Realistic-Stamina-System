//! RSS AI Manager — 统一 AI 子系统管理器
//!
//! 集中管理所有 AI 体力相关模块的调用频率、状态生命周期和模块编排。
//! PlayerBase 仅需持有一个 ref SCR_RSS_AIManager，每 tick 调用 .Tick() 一次。
//!
//! 职责范围：
//!   - 行为层节流（距离 LOD）
//!   - 体力状态机 Tick + SpeedCap / IntentFilter / CombatDecay 模块链

enum ERSS_AIManagerTickResult
{
    AI_TICK_NORMAL,
    AI_TICK_THROTTLED,
    AI_TICK_SKIPPED_COMBAT
}

class SCR_RSS_AIManager
{
    protected float m_fLastBehaviorTickTime;
    protected ERSS_AIStaminaState m_eStaminaState;
    protected float m_fTimeStationarySec;
    protected float m_fLastDebugPrintTime;
    protected ERSS_AIStaminaState m_eLastAppliedCombatState;
    protected bool m_bHasAppliedCombatState;

    void SCR_RSS_AIManager()
    {
        m_eStaminaState = ERSS_AIStaminaState.FRESH;
        m_fTimeStationarySec = 0.0;
        m_fLastDebugPrintTime = -1.0;
        m_fLastBehaviorTickTime = -1.0;
        m_eLastAppliedCombatState = ERSS_AIStaminaState.FRESH;
        m_bHasAppliedCombatState = false;
    }

    //! @param ctrl 已持有的角色控制器（避免每 tick FindComponent）
    //! @param distToNearestPlayerM 距最近玩家（m）；<0 表示未知→按近距
    ERSS_AIManagerTickResult Tick(
        IEntity owner,
        SCR_CharacterControllerComponent ctrl,
        float currentTime,
        float timeDeltaSec,
        float staminaPercent,
        float fatigueVal,
        float currentSpeed,
        bool isPlayer,
        float distToNearestPlayerM)
    {
        if (!owner)
            return ERSS_AIManagerTickResult.AI_TICK_NORMAL;
        if (!Replication.IsServer())
            return ERSS_AIManagerTickResult.AI_TICK_NORMAL;
        if (isPlayer)
            return ERSS_AIManagerTickResult.AI_TICK_NORMAL;

        if (!SCR_RSS_ConfigBridge.IsAIStaminaCombatEffectsEnabled())
            return ERSS_AIManagerTickResult.AI_TICK_SKIPPED_COMBAT;

        float behaviorInterval = SCR_RSS_AIConstants.RSS_PERF_AI_BEHAVIOR_NEAR_SEC;
        if (distToNearestPlayerM >= 0.0)
        {
            if (distToNearestPlayerM > SCR_RSS_AIConstants.RSS_PERF_AI_LOD_FAR_M)
                behaviorInterval = SCR_RSS_AIConstants.RSS_PERF_AI_BEHAVIOR_FAR_SEC;
            else if (distToNearestPlayerM > SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_M)
                behaviorInterval = SCR_RSS_AIConstants.RSS_PERF_AI_BEHAVIOR_MID_SEC;
        }

        if (currentTime - m_fLastBehaviorTickTime < behaviorInterval)
            return ERSS_AIManagerTickResult.AI_TICK_THROTTLED;

        m_fLastBehaviorTickTime = currentTime;

        if (currentSpeed < 0.05)
            m_fTimeStationarySec = m_fTimeStationarySec + timeDeltaSec;
        else
            m_fTimeStationarySec = 0.0;

        ERSS_AIStaminaState prevState = m_eStaminaState;
        ERSS_AIStaminaState aiState = SCR_RSS_AIStaminaState.Tick(
            staminaPercent,
            fatigueVal,
            currentSpeed < 0.05,
            m_fTimeStationarySec,
            m_eStaminaState);

        bool isThreatened = false;
        if (ctrl)
            SCR_RSS_AISpeedCap.Apply(ctrl, owner, aiState, staminaPercent, isThreatened);

        // Intent / CombatDecay：状态未变则跳过（SetStateAllActionsOfType 很贵）
        bool stateChanged = true;
        if (m_bHasAppliedCombatState)
        {
            if (aiState == m_eLastAppliedCombatState)
                stateChanged = false;
        }

        if (stateChanged || prevState != aiState)
        {
            SCR_RSS_AIIntentFilter.Apply(owner, aiState, prevState, isThreatened);
            SCR_RSS_AICombatDecay.Apply(owner, aiState);
            m_eLastAppliedCombatState = aiState;
            m_bHasAppliedCombatState = true;
        }

        return ERSS_AIManagerTickResult.AI_TICK_NORMAL;
    }

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

    void OnEntityDeleted()
    {
        m_fLastBehaviorTickTime = -1.0;
        m_bHasAppliedCombatState = false;
    }
}
