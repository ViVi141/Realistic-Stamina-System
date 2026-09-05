//! AI / 性能 LOD 相关常量（从 Constants 拆分时误入 EnvConstants，现独立归位）
class SCR_RSS_AIConstants
{
    // RSS 主循环速度/体力 tick（毫秒）：玩家≈60Hz；AI 仅在服务器上降频
    static const int RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS = 17;
    static const int RSS_AI_SPEED_UPDATE_INTERVAL_MS = 100;
    //! 服端单兵：按距玩家距离分档刷新间隔
    static const bool RSS_PERF_AI_DISTANCE_LOD_ENABLED = true;
    static const float RSS_PERF_AI_LOD_NEAR_M = 400.0;
    static const float RSS_PERF_AI_LOD_FAR_M = 1200.0;
    //! 全服玩家原点缓存 TTL（秒）；所有 AI LOD 共用，避免每 tick 分配 GetPlayers
    static const float RSS_PERF_AI_PLAYER_POS_CACHE_TTL_SEC = 0.25;
    //! 全量体力路径 LOD（DisableAIStaminaCalc=false）；再拉稀以削 Speed 尖刺
    static const int RSS_PERF_AI_LOD_NEAR_INTERVAL_MS = 600;
    static const int RSS_PERF_AI_LOD_MID_INTERVAL_MS = 1000;
    static const int RSS_PERF_AI_LOD_FAR_INTERVAL_MS = 2500;
    //! 轻量路径（DisableAIStaminaCalc=true）：仅负重/相位限速
    static const int RSS_PERF_AI_LIGHT_NEAR_INTERVAL_MS = 800;
    static const int RSS_PERF_AI_LIGHT_MID_INTERVAL_MS = 1500;
    static const int RSS_PERF_AI_LIGHT_FAR_INTERVAL_MS = 3000;
    //! CallLater 错峰上限（ms），避免百人 AI 同帧齐刷
    static const int RSS_PERF_AI_TICK_STAGGER_MS = 180;
    //! AI 专用链路：地形射线采样间隔（秒）；坡度用 Y 差分，不依赖射线
    static const float RSS_AI_TERRAIN_SAMPLE_NEAR_SEC = 2.0;
    static const float RSS_AI_TERRAIN_SAMPLE_MID_SEC = 5.0;
    //! AI 战斗行为层节流（秒）
    static const float RSS_PERF_AI_BEHAVIOR_NEAR_SEC = 0.75;
    static const float RSS_PERF_AI_BEHAVIOR_MID_SEC = 1.5;
    static const float RSS_PERF_AI_BEHAVIOR_FAR_SEC = 3.0;
    // [SOFT] AI 体力状态机 — 转移阈值（可在预设中调整）
    static const float RSS_AI_STATE_FRESH_DOWN = 0.80;           // FRESH → WINDED
    static const float RSS_AI_STATE_WINDED_DOWN = 0.50;          // WINDED → FATIGUED
    static const float RSS_AI_STATE_WINDED_UP = 0.85;            // WINDED → FRESH（滞回）
    static const float RSS_AI_STATE_FATIGUED_DOWN = 0.25;        // FATIGUED → EXHAUSTED
    static const float RSS_AI_STATE_FATIGUED_UP = 0.55;          // FATIGUED → WINDED（滞回）
    static const float RSS_AI_STATE_EXHAUSTED_DOWN = 0.10;       // EXHAUSTED → COLLAPSED
    static const float RSS_AI_STATE_EXHAUSTED_UP = 0.30;         // EXHAUSTED → FATIGUED（滞回）
    static const float RSS_AI_STATE_COLLAPSE_THRESHOLD = 0.05;   // 强制恢复触发线
    static const float RSS_AI_STATE_FORCE_RECOVER_DELAY_SEC = 5.0; // 强制恢复前静止时长
    static const float RSS_AI_STATE_RECOVER_TO_EXHAUSTED = 0.15; // RECOVERING → EXHAUSTED
    static const float RSS_AI_STATE_RECOVER_TO_FATIGUED = 0.30;  // RECOVERING → FATIGUED

    // [SOFT] AI 移动限速 — 按体力状态的 OverrideMaxSpeed 上限
    static const float RSS_AI_SPEED_FATIGUED_LIMIT = 0.65;       // FATIGUED 时限速 65%
    static const float RSS_AI_SPEED_EXHAUSTED_LIMIT = 0.40;      // EXHAUSTED 时限速 40%
    static const float RSS_AI_SPEED_RECOVERING_MIN = 0.30;       // RECOVERING 时最低速度倍率
    static const float RSS_AI_SPEED_CONTINUOUS_MIN = 0.30;       // 连续衰减曲线下限

    // [SOFT] AI 战斗衰减矩阵 — 感知 / 射速 / 技能（6 状态）
    static const float RSS_AI_COMBAT_PERCEPTION_WINDED     = 0.95;
    static const float RSS_AI_COMBAT_FIRE_RATE_WINDED      = 1.00;
    static const float RSS_AI_COMBAT_SKILL_WINDED          = 0.95;
    static const float RSS_AI_COMBAT_PERCEPTION_FATIGUED   = 0.80;
    static const float RSS_AI_COMBAT_FIRE_RATE_FATIGUED    = 0.85;
    static const float RSS_AI_COMBAT_SKILL_FATIGUED        = 0.80;
    static const float RSS_AI_COMBAT_PERCEPTION_EXHAUSTED  = 0.60;
    static const float RSS_AI_COMBAT_FIRE_RATE_EXHAUSTED   = 0.60;
    static const float RSS_AI_COMBAT_SKILL_EXHAUSTED       = 0.55;
    static const float RSS_AI_COMBAT_PERCEPTION_COLLAPSED  = 0.40;
    static const float RSS_AI_COMBAT_FIRE_RATE_COLLAPSED   = 0.30;
    static const float RSS_AI_COMBAT_SKILL_COLLAPSED       = 0.30;
    static const float RSS_AI_COMBAT_PERCEPTION_RECOVERING = 0.80;
    static const float RSS_AI_COMBAT_FIRE_RATE_RECOVERING  = 0.90;
    static const float RSS_AI_COMBAT_SKILL_RECOVERING      = 0.80;

    // [SOFT] AI 伤害-体力联动
    static const float RSS_AI_INJURY_DRAIN_MILD = 1.15;
    static const float RSS_AI_INJURY_RECOVERY_MILD = 0.85;
    static const float RSS_AI_INJURY_DRAIN_MODERATE = 1.4;
    static const float RSS_AI_INJURY_RECOVERY_MODERATE = 0.6;
    static const float RSS_AI_INJURY_DRAIN_SEVERE = 1.8;
    static const float RSS_AI_INJURY_RECOVERY_SEVERE = 0.3;
    static const float RSS_AI_INJURY_DRAIN_CRITICAL = 2.5;
    static const float RSS_AI_INJURY_RECOVERY_CRITICAL = 0.1;

    // [SOFT] AI 行为过滤 — 常量
    static const float RSS_AI_INTENT_WAIT_PROMOTED_PRIORITY = 100.0;
}
