// SCR_StaminaConstants_MudSlip — split from SCR_StaminaConstants.c

class StaminaMudSlipConstants
{
    // =====================================================================
    // ## DESIGN NOTE: Mud slip camera & trigger thresholds
    // Current values (9-12Hz, Roll 6°, Pitch 3.4°, Yaw 2.9°) were tuned to
    // produce noticeable "instability" feedback before a ragdoll event.  The
    // 8-12Hz band overlaps with the human vestibular resonance range, which
    // may cause motion sickness during prolonged (>30s) muddy traversal.
    //
    // Tuning recommendations for a future pass:
    //   - Shift frequency to 6-8Hz (below resonance band)
    //   - Add adaptive decay: reduce amplitude by 50% after 10s continuous slip stress
    //   - Raise GAP_EPSILON_DICE to 0.012 to reduce false-positive ragdolls on gravel
    // =====================================================================
    // 泥泞滑倒游戏效果（短 Ragdoll + 体力惩罚），由 MudSlipEffects + PlayerBase 消费
    static const float ENV_MUD_SLIP_GLOBAL_SCALE = 1.5; // 总强度倍率（Poisson λ）；由 2.5×0.6 下调以压低 1s 复合触发率
    static const float ENV_MUD_SLIP_MIN_SPEED_MS = 1.6; // 低于此水平速度不判滑倒（m/s）；AI 预警限速与此对齐
    // AI 预计区：镜头应力≥此值时（仅安全 AI）OverrideMaxSpeed 压速作预警；防滑倒靠脚本层不调用 TryRoll，非本阈值
    static const float ENV_MUD_SLIP_AI_WARN_STRESS_MIN = 0.015;
    // 曾用于 AI 泥泞「危险上下文」生命值判定；当前桥接逻辑已取消，常量保留供其它模块或将来使用
    static const float ENV_MUD_SLIP_AI_UNSAFE_HEALTH_SCALED = 0.92;
    // EvaluatePriorityLevel()≥此视为危险上下文：不限速且允许泥泞 Ragdoll（对照 MOVE=30、SUPPRESS≈63）
    static const float ENV_MUD_SLIP_AI_UNSAFE_PRIORITY_MIN = 50.0;
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

    // ==========================================================================
    // [SOFT] AI 行为过滤 — 常量
    // ==========================================================================
    static const float RSS_AI_INTENT_WAIT_PROMOTED_PRIORITY = 100.0;
    // 以下为旧版乘法模型遗留，滑倒判定已改为 ACOF/RCOF（见 MudSlipEffects）
    static const float ENV_MUD_SLIP_SPEED_COEFF = 0.14; // 未使用
    static const float ENV_MUD_SLIP_SPRINT_MULT = 2.3; // 未使用
    static const float ENV_MUD_SLIP_WEIGHT_COEFF = 0.014; // 未使用
    // 摩擦阈值模型（无量纲，与 μ 同量级）：RCOF > ACOF 时才有可能滑倒
    static const float ENV_SLIP_ACOF_DRY_BASE = 0.52; // 干燥铺装近似可用摩擦系数上限
    static const float ENV_SLIP_ACOF_OFFROAD_DROP = 0.07; // 地形系数高于 1 时干摩擦上界降低（松软地面）
    static const float ENV_SLIP_LUBRICATION_MAX = 0.62; // 泥泞润滑使 ACOF 最多降到约 (1-max)*μ_dry
    static const float ENV_SLIP_ACOF_MIN = 0.07;
    static const float ENV_SLIP_ACOF_MAX = 0.58;
    static const float ENV_SLIP_RCOF_BASE = 0.20; // 低速行走所需摩擦（原0.13→0.20：让泥地冲刺即可产生非零 gap）
    static const float ENV_SLIP_RCOF_VSQ = 0.0080; // 速度平方项（原0.0055→0.0080：提高高速段 RCOF 灵敏度）
    static const float ENV_SLIP_RCOF_SPRINT = 0.058; // 冲刺固定加项（原0.0406→0.058：恢复至原始值，让冲刺有更清晰的风险信号）
    static const float ENV_SLIP_RCOF_WEIGHT = 0.065; // 满负重时额外需求（0~55kg）
    static const float ENV_SLIP_RCOF_CROUCH_MULT = 0.86; // 蹲姿步幅小，降低 RCOF
    // 坡度：先按 |α| 得 slopeAdd，再乘以下坡/上坡系数（GetSlopeAngle：正=上坡，负=下坡）
    static const float ENV_SLIP_RCOF_SLOPE_PER_DEG = 0.0026;
    static const float ENV_SLIP_RCOF_SLOPE_MAX = 0.095;
    static const float ENV_SLIP_RCOF_SLOPE_DOWNHILL_MULT = 1.14; // 下坡略增需求（重心前移）
    static const float ENV_SLIP_RCOF_SLOPE_UPHILL_MULT = 0.98; // 上坡略低于同倾角下坡
    // 急转弯：ω(rad/s) 与水平速 v(m/s) 耦合 turnAdd = min(ω·v·k, T_max)，与 F∝mvω 量级一致（高速急转更危险）
    static const float ENV_SLIP_TURN_MIN_HORIZ_MS = 0.2; // 本帧与上一帧水平速均高于此才计 ω
    static const float ENV_SLIP_RCOF_TURN_OMEGA_V_COEFF = 0.0030; // 原仅 ω·k_ω 已弃用，见 git 历史
    static const float ENV_SLIP_RCOF_TURN_MAX = 0.07;
    static const float ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC = 12.0; // 防止单帧数值爆炸
    // 低体力：平衡能力下降 → RCOF 基线上升 + 小幅随机抖动 σ
    static const float ENV_SLIP_RCOF_BALANCE_STAMINA = 0.085; // 与 (1−stamina) 成正比
    static const float ENV_SLIP_RCOF_BALANCE_JITTER = 0.045; // RandomFloat01()×此项×(1−stamina)
    // 落地/坠地瞬间：上一帧明显下落、本帧已接近接地 → 重心不稳
    static const float ENV_SLIP_RCOF_LANDING = 0.11;
    static const float ENV_SLIP_LANDING_VY_PREV = -1.15;
    static const float ENV_SLIP_LANDING_VY_CUR = -0.42;
    // 起跳蹬地瞬间（垂直上速较大）
    static const float ENV_SLIP_RCOF_JUMP_UP_VY = 1.65;
    static const float ENV_SLIP_RCOF_JUMP_UP = 0.042;
    // 摩擦缺口 gap=RCOF−ACOF（无量纲）：镜头用较小 ε，掷骰用较大 ε，使「先晃后摔」有缓冲带
    static const float ENV_SLIP_GAP_EPSILON_CAMERA = 0.004; // 镜头应力：gap>此值才开始有预警（再经疲劳缩小 ε_eff）
    static const float ENV_SLIP_GAP_EPSILON_DICE = 0.012;   // 掷骰：gap≤此值不进入 Poisson（仅镜头仍可预警）；原0.008→0.012 减少碎石路误触
    // λ∝gap（线性标度）；单步 P 经约 20 次/秒复合后体感见 docs/泥泞滑倒判定模型.md §6.1
    static const float ENV_MUD_SLIP_PHYS_SCALE = 2.2; // 与 ENV_MUD_SLIP_GLOBAL_SCALE 联调（原 gap² 时代为 20）
    static const float ENV_MUD_SLIP_COOLDOWN_SEC = 9.0; // 两次滑倒之间的最短间隔（秒），从「趴→蹲/站」锚定时刻起算（见 RSS_MudSlipRunner）
    // 滑倒后若从未趴下：布娃娃已结束且姿态为蹲/站、且自滑倒事件起已满此秒数，则锚定 9s 冷却起点（避免永不趴地时无法进入冷却）
    static const float ENV_MUD_SLIP_COOLDOWN_ANCHOR_FALLBACK_SEC = 3.0;
    static const float ENV_MUD_SLIP_STAMINA_FRAC = 0.065; // 触发时额外扣除体力比例（0~1）
    // Ragdoll() 后必须 RefreshRagdoll(>0)，否则骨骼很快静止会立即混合结束，几乎看不到布娃娃（见引擎 CharacterControllerComponent 文档）
    static const float ENV_MUD_SLIP_RAGDOLL_WARMUP_SEC = 2.2; // 首次 Refresh：保持布娃娃至少若干秒内不因“静止”被关掉
    static const int ENV_MUD_SLIP_RAGDOLL_BLEND_DELAY_MS = 1200; // 此后延迟再 Refresh(0) 开始混合回动画（毫秒）
    // 泥泞失稳：第一人称镜头角抖动预警（无踉跄动画时的主要反馈）；与 gap 映射，与滑倒 Poisson 独立
    static const float ENV_MUD_SLIP_CAM_SHAKE_GAP_SPAN = 0.14; // gap 从 ε 拉到 1.0 应力的跨度（无量纲）
    // 低体力：镜头应力映射用更小有效 ε（仅 ComputeSlipCameraStress01），更易「提前发虚」；与掷骰 ε 无关
    static const float ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_STAMINA_THRESHOLD = 0.2; // 低于此体力开始收紧 ε_eff
    static const float ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_EPS_SCALE = 0.35; // stamina→0 时 ε_eff = ε·scale；→threshold 时回到 ε
    static const float ENV_MUD_SLIP_CAM_SHAKE_ROLL_DEG = 6.0;   // 应力=1 时横滚峰值（度）；Roll 对「失稳」暗示强且较不易晕
    static const float ENV_MUD_SLIP_CAM_SHAKE_PITCH_DEG = 3.4;  // 俯仰峰值（度）
    static const float ENV_MUD_SLIP_CAM_SHAKE_YAW_DEG = 2.9;    // 偏航峰值（度）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FREQ_BASE = 6.0;  // 基础角频率 Hz 量级（原9.0→6.0：移出人前庭共振带 8-12Hz）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FREQ_STRESS = 6.0; // 应力越高频率越快（原12.0→6.0：典型应力0.2-0.5时频率7-9Hz<12Hz）
    static const float ENV_MUD_SLIP_CAM_SHAKE_SMOOTH_RATE = 16.0; // 应力平滑（越大越快跟上目标）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FOV_JITTER_DEG = 0.35; // 极力克制：FOV 高频抖易诱发晕动症，优先靠 Roll
}
