//! RSS AI Stamina State — 体力状态机
//!
//! 每个 AI 实体拥有一个体力状态标签（非简单 float），由体力值和疲劳值联合决定。
//! 所有下游模块（行为过滤、移动限速、战斗衰减、群组协同）只读此状态做决策。
//!
//! 状态转移包含滞回（hysteresis），避免在边界反复横跳。
//!
//! 调用方：PlayerBase.c 每帧 Tick

enum ERSS_AIStaminaState
{
    FRESH,       // 充沛: 体力 ≥ 80%, 疲劳 < 0.3
    WINDED,      // 微喘: 体力 50%~80%
    FATIGUED,    // 疲劳: 体力 25%~50%
    EXHAUSTED,   // 力竭: 体力 10%~25%
    COLLAPSED,   // 崩溃: 体力 < 10%
    RECOVERING   // 恢复中: 体力在回升但尚未到 FRESH
}

class SCR_RSS_AIStaminaState
{
    //------------------------------------------------------------------------------------------------
    //! 主要入口：每帧以当前体力+疲劳计算并返回状态。
    //! 调用方自行维护 prevState（作为传入引用）。
    //!
    //! \param staminaPercent01  当前体力百分比 [0~1]
    //! \param fatiguePercent01  当前疲劳百分比 [0~1]
    //! \param isStationary      当前是否静止 (speed < 0.05 m/s)
    //! \param timeStationarySec 连续静止时长（秒）
    //! \param prevState         上一个状态（传入传出引用）
    //! \return                  当前状态
    static ERSS_AIStaminaState Tick(
        float staminaPercent01,
        float fatiguePercent01,
        bool isStationary,
        float timeStationarySec,
        inout ERSS_AIStaminaState prevState)
    {
        float s = staminaPercent01;
        float f = fatiguePercent01;

        // — 强制恢复：体力 < 5% 且持续 5s —
        if (s < StaminaConstants.RSS_AI_STATE_COLLAPSE_THRESHOLD &&
            isStationary &&
            timeStationarySec >= StaminaConstants.RSS_AI_STATE_FORCE_RECOVER_DELAY_SEC)
        {
            prevState = ERSS_AIStaminaState.RECOVERING;
            return ERSS_AIStaminaState.RECOVERING;
        }

        ERSS_AIStaminaState state = prevState;

        switch (state)
        {
        case ERSS_AIStaminaState.FRESH:
            if (s < StaminaConstants.RSS_AI_STATE_FRESH_DOWN)
                state = ERSS_AIStaminaState.WINDED;
            break;

        case ERSS_AIStaminaState.WINDED:
            if (s < StaminaConstants.RSS_AI_STATE_WINDED_DOWN)
                state = ERSS_AIStaminaState.FATIGUED;
            else if (s >= StaminaConstants.RSS_AI_STATE_WINDED_UP)
                state = ERSS_AIStaminaState.FRESH;
            break;

        case ERSS_AIStaminaState.FATIGUED:
            if (s < StaminaConstants.RSS_AI_STATE_FATIGUED_DOWN)
                state = ERSS_AIStaminaState.EXHAUSTED;
            else if (s >= StaminaConstants.RSS_AI_STATE_FATIGUED_UP)
                state = ERSS_AIStaminaState.WINDED;
            break;

        case ERSS_AIStaminaState.EXHAUSTED:
            if (s < StaminaConstants.RSS_AI_STATE_EXHAUSTED_DOWN)
                state = ERSS_AIStaminaState.COLLAPSED;
            else if (s >= StaminaConstants.RSS_AI_STATE_EXHAUSTED_UP)
                state = ERSS_AIStaminaState.FATIGUED;
            break;

        case ERSS_AIStaminaState.COLLAPSED:
            if (isStationary && timeStationarySec >= 3.0)
                state = ERSS_AIStaminaState.RECOVERING;
            break;

        case ERSS_AIStaminaState.RECOVERING:
            if (s >= StaminaConstants.RSS_AI_STATE_RECOVER_TO_EXHAUSTED)
            {
                if (s >= StaminaConstants.RSS_AI_STATE_RECOVER_TO_FATIGUED)
                    state = ERSS_AIStaminaState.FATIGUED;
                else
                    state = ERSS_AIStaminaState.EXHAUSTED;
            }
            break;
        }

        prevState = state;
        return state;
    }

    //------------------------------------------------------------------------------------------------
    //! 状态严重度（越大越差），用于群组「最弱成员决定全队机动」
    static int GetSeverity(ERSS_AIStaminaState state)
    {
        switch (state)
        {
        case ERSS_AIStaminaState.FRESH:      return 1;
        case ERSS_AIStaminaState.WINDED:     return 2;
        case ERSS_AIStaminaState.FATIGUED:   return 3;
        case ERSS_AIStaminaState.EXHAUSTED:  return 4;
        case ERSS_AIStaminaState.RECOVERING: return 5;
        case ERSS_AIStaminaState.COLLAPSED:  return 6;
        }
        return 0;
    }

    //------------------------------------------------------------------------------------------------
    static ERSS_AIStaminaState PickWorse(ERSS_AIStaminaState a, ERSS_AIStaminaState b)
    {
        if (SCR_RSS_AIStaminaState.GetSeverity(a) >= SCR_RSS_AIStaminaState.GetSeverity(b))
            return a;
        return b;
    }

    //------------------------------------------------------------------------------------------------
    //! 供调试日志使用
    static string StateToString(ERSS_AIStaminaState state)
    {
        switch (state)
        {
        case ERSS_AIStaminaState.FRESH:      return "FRESH";
        case ERSS_AIStaminaState.WINDED:     return "WINDED";
        case ERSS_AIStaminaState.FATIGUED:   return "FATIGUED";
        case ERSS_AIStaminaState.EXHAUSTED:  return "EXHAUSTED";
        case ERSS_AIStaminaState.COLLAPSED:  return "COLLAPSED";
        case ERSS_AIStaminaState.RECOVERING: return "RECOVERING";
        }
        return "UNKNOWN";
    }
}
