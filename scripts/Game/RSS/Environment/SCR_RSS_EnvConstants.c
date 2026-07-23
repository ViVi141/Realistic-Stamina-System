//! 环境相关常量（从 SCR_RSS_Constants.c 拆分以控制 64KB 上限）
class SCR_RSS_EnvConstants
{
    
    // 时间窗口为 HARD（太阳辐射物理规律）；强度倍数为 SOFT（通过配置管理器读取）
    static const float ENV_HEAT_STRESS_START_HOUR      = 10.0; // [HARD] 热应激开始（日照曲线）
    static const float ENV_HEAT_STRESS_PEAK_HOUR       = 14.0; // [HARD] 峰值（地表温度滞后约2h）
    static const float ENV_HEAT_STRESS_END_HOUR        = 18.0; // [HARD] 热应激结束
    static const float ENV_HEAT_STRESS_MAX_MULTIPLIER  = 1.5;  // [SOFT fallback] 最大倍数，可通过 env_heat_stress_max_multiplier 覆盖
    static const float ENV_HEAT_STRESS_BASE_MULTIPLIER = 1.0;  // [HARD] 基准倍数（无热影响）
    static const float ENV_HEAT_STRESS_INDOOR_REDUCTION = 0.5; // [SOFT fallback] 室内减免比例
    
    // 降雨湿重参数
    static const float ENV_RAIN_WEIGHT_MIN = 2.0; // kg，小雨时的湿重
    static const float ENV_RAIN_WEIGHT_MAX = 8.0; // kg，暴雨时的湿重
    static const float ENV_RAIN_WEIGHT_DURATION = 60.0; // 秒，停止降雨后湿重持续时间
    static const float ENV_RAIN_WEIGHT_DECAY_RATE = 0.0167; // 每秒衰减率（60秒内完全消失）
    
    // 湿重饱和上限（防止游泳湿重+降雨湿重叠加导致数值爆炸）
    static const float ENV_MAX_TOTAL_WET_WEIGHT = 10.0; // kg，总湿重上限（游泳+降雨）
    
    // 环境因子检测频率（性能优化）
    static const float ENV_CHECK_INTERVAL = 10.0; // 秒，环境因子检测间隔（perf: 5→10，天气变化缓慢无需高频）
    
    // 室内检测参数
    static const float ENV_INDOOR_CHECK_HEIGHT = 10.0; // 米，向上检测高度（判断是否有屋顶）
    static const float ENV_SLOPE_SUPPRESS_ROOF_CHECK_HEIGHT = 35.0; // 米
    
    
    // 降雨强度相关常量
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE = 0.5; // kg/秒，基础湿重增加速率
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT = 1.5; // 降雨强度指数（非线性增长）
    static const float ENV_RAIN_INTENSITY_THRESHOLD = 0.01; // 原始阈值（仅用于 API 回退逻辑）
    static const float ENV_RAIN_VISUAL_EFFECT_THRESHOLD = 0.15;
    static const float ENV_RAIN_INTENSITY_HEAVY_THRESHOLD = 0.8; // 暴雨阈值（呼吸阻力触发）
    static const float ENV_RAIN_INTENSITY_BREATHING_PENALTY = 0.05; // 暴雨时的无氧代谢增加比例
    
    // 风阻相关常量
    static const float ENV_WIND_RESISTANCE_COEFF = 0.05; // 风阻系数（体力消耗权重）
    static const float ENV_WIND_SPEED_THRESHOLD = 1.0; // m/s，风速阈值（低于此值忽略）
    static const float ENV_WIND_TAILWIND_BONUS = 0.02; // 顺风时的消耗减少比例
    static const float ENV_WIND_TAILWIND_SPEED_BONUS = 0.01; // 顺风时的速度加成比例
    
    // 路面泥泞度相关常量
    static const float ENV_MUD_PENALTY_MAX = 0.4; // 最大泥泞惩罚（40%地形阻力增加）
    static const float ENV_MUD_SLIPPERY_THRESHOLD = 0.3; // 积水阈值（高于此值触发滑倒风险）
    static const float ENV_MUD_SPRINT_PENALTY = 0.1; // 泥泞时Sprint速度惩罚
    static const float ENV_MUD_SLIP_RISK_BASE = 0.005; // 基础滑倒风险（每0.2秒，与 SCR_RSS_EnvironmentFactor.CalculateSlipRisk 一致）
    // ## DESIGN NOTE: Mud slip camera & trigger thresholds
    //
    // Tuning recommendations for a future pass:
    //   - Shift frequency to 6-8Hz (below resonance band)
    static const float ENV_MUD_SLIP_MIN_SPEED_MS = 1.6; // 低于此水平速度不判滑倒（m/s）
    static const float ENV_MUD_SLIP_GLOBAL_SCALE = 1.5; // 总强度倍率（Poisson λ）；由 2.5×0.6 下调以压低 1s 复合触发率
    // AI / LOD tick 常量见 SCR_RSS_AIConstants
    // 以下为旧版乘法模型遗留，滑倒判定已改为 ACOF/RCOF（见 SCR_RSS_MudSlipEffects）
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
    static const float ENV_MUD_SLIP_COOLDOWN_ANCHOR_FALLBACK_SEC = 3.0;
    static const float ENV_MUD_SLIP_STAMINA_FRAC = 0.065; // 触发时额外扣除体力比例（0~1）
    static const float ENV_MUD_SLIP_RAGDOLL_WARMUP_SEC = 2.2; // 首次 Refresh：保持布娃娃至少若干秒内不因“静止”被关掉
    static const int ENV_MUD_SLIP_RAGDOLL_BLEND_DELAY_MS = 1200; // 此后延迟再 Refresh(0) 开始混合回动画（毫秒）
    // 泥泞失稳：第一人称镜头角抖动预警（无踉跄动画时的主要反馈）；与 gap 映射，与滑倒 Poisson 独立
    static const float ENV_MUD_SLIP_CAM_SHAKE_GAP_SPAN = 0.14; // gap 从 ε 拉到 1.0 应力的跨度（无量纲）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_STAMINA_THRESHOLD = 0.2; // 低于此体力开始收紧 ε_eff
    static const float ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_EPS_SCALE = 0.35; // stamina→0 时 ε_eff = ε·scale；→threshold 时回到 ε
    static const float ENV_MUD_SLIP_CAM_SHAKE_ROLL_DEG = 6.0;   // 应力=1 时横滚峰值（度）；Roll 对「失稳」暗示强且较不易晕
    static const float ENV_MUD_SLIP_CAM_SHAKE_PITCH_DEG = 3.4;  // 俯仰峰值（度）
    static const float ENV_MUD_SLIP_CAM_SHAKE_YAW_DEG = 2.9;    // 偏航峰值（度）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FREQ_BASE = 6.0;  // 基础角频率 Hz 量级（原9.0→6.0：移出人前庭共振带 8-12Hz）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FREQ_STRESS = 6.0; // 应力越高频率越快（原12.0→6.0：典型应力0.2-0.5时频率7-9Hz<12Hz）
    static const float ENV_MUD_SLIP_CAM_SHAKE_SMOOTH_RATE = 16.0; // 应力平滑（越大越快跟上目标）
    static const float ENV_MUD_SLIP_CAM_SHAKE_FOV_JITTER_DEG = 0.35; // 极力克制：FOV 高频抖易诱发晕动症，优先靠 Roll
    
    // 气温相关常量
    static const float ENV_TEMPERATURE_HEAT_THRESHOLD       = 30.0; // [HARD] °C，医学热应激阈值
    static const float ENV_TEMPERATURE_HEAT_PENALTY_COEFF   = 0.02; // [SOFT fallback] 每高1°C恢复率降低2%
    static const float ENV_TEMPERATURE_COLD_THRESHOLD       =  5.0; // [HARD] °C，北约STANAG冷应激阈值（原0°C几乎不触发，提升至5°C使温带地图生效）
    static const float ENV_TEMPERATURE_COLD_STATIC_PENALTY  = 0.03; // [SOFT fallback] 低温静态消耗增加
    static const float ENV_TEMPERATURE_COLD_RECOVERY_PENALTY = 0.05; // [SOFT fallback] 低温恢复率惩罚
    
    // 地表湿度相关常量
    static const float ENV_SURFACE_WETNESS_SOAK_RATE = 1.0; // kg/秒，趴下时的湿重增加速率
    static const float ENV_SURFACE_WETNESS_THRESHOLD = 0.1; // 积水阈值（高于此值触发湿重增加）
    static const float ENV_SURFACE_WETNESS_MARGINAL_DECAY_ADVANCE = 0.1; // 边际效应衰减提前触发比例
    static const float ENV_SURFACE_WETNESS_PRONE_PENALTY = 0.15;
}
