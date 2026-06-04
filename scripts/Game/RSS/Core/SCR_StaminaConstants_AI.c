// SCR_StaminaConstants_AI — split from SCR_StaminaConstants.c

class StaminaAIConstants
{
    // RSS 主循环速度/体力 tick（毫秒）：玩家≈60Hz；AI 仅在服务器上降频
    static const int RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS = 17;
    static const int RSS_AI_SPEED_UPDATE_INTERVAL_MS = 100;
    //! 服端：非交战群组距玩家较远时仅队长全算，队员同步；见 SCR_RSS_AIGroupStaminaProxy.c
    //! v3.20.0: 激活距离从 1500m 缩短至 800m，提高代理覆盖率（约 +50%），
    //!          在中等交战密度场景下减少约 20% 的 AI 全量计算开销。
    static const bool RSS_PERF_AI_GROUP_PROXY_ENABLED = true;
    static const float RSS_PERF_AI_GROUP_PROXY_DISTANCE_M = 800.0;
    static const int RSS_PERF_AI_GROUP_PROXY_INTERVAL_MS = 5000;
    //! 服端单兵：未命中群组代理时按距玩家距离分档刷新间隔
    static const bool RSS_PERF_AI_DISTANCE_LOD_ENABLED = true;
    static const float RSS_PERF_AI_LOD_NEAR_M = 400.0;
    static const float RSS_PERF_AI_LOD_FAR_M = 1200.0;
    static const int RSS_PERF_AI_LOD_NEAR_INTERVAL_MS = 200; // perf: 100→200，高密度 AI 减负。体力变化是秒级，200ms 精度足够
    static const int RSS_PERF_AI_LOD_MID_INTERVAL_MS = 300;
    static const int RSS_PERF_AI_LOD_FAR_INTERVAL_MS = 1500;
    //! 群组「是否存在战场危险」判定缓存秒数；见 SCR_RSS_AIGroupStaminaProxy.c
    //! v3.20.0: 缓存时间从 0.5s 延长至 1.0s，减少高密度场景下的全组扫描频率（约 -50% 扫描次数）。
    static const float RSS_PERF_AI_GROUP_BATTLE_CACHE_SEC = 1.0;
    // ==========================================================================
    // [SOFT] AI 体力状态机 — 转移阈值（可在预设中调整）
    // ==========================================================================
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

    // ==========================================================================
    // [SOFT] AI 移动限速 — 按体力状态的 OverrideMaxSpeed 上限
    // ==========================================================================
    static const float RSS_AI_SPEED_FATIGUED_LIMIT = 0.65;       // FATIGUED 时限速 65%
    static const float RSS_AI_SPEED_EXHAUSTED_LIMIT = 0.40;      // EXHAUSTED 时限速 40%
    static const float RSS_AI_SPEED_RECOVERING_MIN = 0.30;       // RECOVERING 时最低速度倍率
    static const float RSS_AI_SPEED_CONTINUOUS_MIN = 0.30;       // 连续衰减曲线下限

    // ==========================================================================
    // [SOFT] AI 战斗衰减矩阵 — 感知 / 射速 / 技能（6 状态）
    // ==========================================================================
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

    // ==========================================================================
    // [SOFT] AI 群组协同 — 预扫描 + 步速 + 休息
    // ==========================================================================
    static const float RSS_AI_GROUP_SYNC_FIT_THRESHOLD = 0.70;
    static const float RSS_AI_GROUP_SYNC_TIRING_THRESHOLD = 0.40;
    static const float RSS_AI_GROUP_SYNC_SPENT_THRESHOLD = 0.20;
    static const float RSS_AI_GROUP_SYNC_TIRING_INSERT_DIST_M = 300.0;
    static const float RSS_AI_GROUP_SYNC_COOLDOWN_SEC = 90.0;
    static const float RSS_AI_GROUP_SYNC_PACE_MIN = 0.15;
    //! 自适应群组步速缓存刷新间隔（秒）；与 PlayerBase AI 桥接节流同量级
    static const float RSS_AI_GROUP_SYNC_PACE_REFRESH_SEC = 0.5;
    //! 编队几何：成员距几何中心 ≤ 软半径时不受额外减速
    static const float RSS_AI_GROUP_COHESION_SOFT_RADIUS_M = 8.0;
    //! 编队几何：成员距几何中心 ≥ 硬半径时降至成员最低倍率
    static const float RSS_AI_GROUP_COHESION_HARD_RADIUS_M = 22.0;
    static const float RSS_AI_GROUP_COHESION_MEMBER_MIN_MUL = 0.25;
    //! 目标：全队相对几何中心的最大散布（超过则全组略减速）
    static const float RSS_AI_GROUP_COHESION_TARGET_SPREAD_M = 16.0;
    //! 几何中心与队长分离上限（超过则队长减速，把中心拉回可控范围）
    static const float RSS_AI_GROUP_COHESION_MAX_CENTER_LEADER_M = 12.0;
    static const float RSS_AI_GROUP_COHESION_GLOBAL_MIN_MUL = 0.45;
    static const float RSS_AI_GROUP_COHESION_LEADER_MIN_MUL = 0.35;
    //! 编队 OverrideMaxSpeed 向目标倍率收敛的时间常数（秒），抑制边缘 AI 顿挫
    static const float RSS_AI_GROUP_COHESION_SPEED_SMOOTH_TAU_SEC = 1.25;
    //! 几何减速滞回：进入外圈 / 退出外圈半径（米），避免在软边界来回抖
    static const float RSS_AI_GROUP_COHESION_OUTER_ENTER_M = 10.0;
    static const float RSS_AI_GROUP_COHESION_OUTER_EXIT_M = 6.5;
    static const ResourceName RSS_AI_GROUP_SYNC_WAIT_WAYPOINT_PREFAB = "{73A8E1C2D5F14906}Prefabs/AI/Waypoints/AIWaypoint_Wait.et";
    static const ResourceName RSS_AI_GROUP_SYNC_DEFEND_WAYPOINT_PREFAB = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";

    // ==========================================================================
    // [SOFT] AI 伤害-体力联动
    // ==========================================================================
    static const float RSS_AI_INJURY_DRAIN_MILD = 1.15;
    static const float RSS_AI_INJURY_RECOVERY_MILD = 0.85;
    static const float RSS_AI_INJURY_DRAIN_MODERATE = 1.4;
    static const float RSS_AI_INJURY_RECOVERY_MODERATE = 0.6;
    static const float RSS_AI_INJURY_DRAIN_SEVERE = 1.8;
    static const float RSS_AI_INJURY_RECOVERY_SEVERE = 0.3;
    static const float RSS_AI_INJURY_DRAIN_CRITICAL = 2.5;
    static const float RSS_AI_INJURY_RECOVERY_CRITICAL = 0.1;
}
