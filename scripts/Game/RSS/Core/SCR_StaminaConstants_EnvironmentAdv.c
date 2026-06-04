// SCR_StaminaConstants_EnvironmentAdv.c

class StaminaEnvironmentAdvConstants
{
    // ==================== 高级环境因子常量（v2.15.0）====================
    
    // 降雨强度相关常量
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_BASE_RATE = 0.5; // kg/秒，基础湿重增加速率
    static const float ENV_RAIN_INTENSITY_ACCUMULATION_EXPONENT = 1.5; // 降雨强度指数（非线性增长）
    static const float ENV_RAIN_INTENSITY_THRESHOLD = 0.01; // 原始阈值（仅用于 API 回退逻辑）
    // 引擎降雨特效阈值：GetRainIntensity() 在 0.1 左右时通常不显示下雨粒子，只有 >= 此值才视为"真下雨"
    // 参考：drizzle/light=0.2, rain=0.5, storm=0.9；0.1 多为 overcast/潮湿，无可见雨滴
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
    static const float ENV_MUD_SLIP_RISK_BASE = 0.005; // 基础滑倒风险（每0.2秒，与 EnvironmentFactor.CalculateSlipRisk 一致）
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
