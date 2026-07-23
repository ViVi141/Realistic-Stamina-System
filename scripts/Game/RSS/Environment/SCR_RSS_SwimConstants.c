//! 游泳/湿重常量（从 SCR_RSS_Constants.c 拆分）
class SCR_RSS_SwimConstants
{
    
    // 生理学依据：游泳的能量消耗远高于陆地移动（约3-5倍）
    // 物理模型：阻力与速度平方成正比 F_d = 0.5 * ρ * v² * C_d * A
    //       Journal of Applied Physiology, 37(5), 762-765.
    
    // [HARD] 游泳物理模型参数（流体力学常数，不可变）
    static const float SWIMMING_DRAG_COEFFICIENT = 0.5;    // [HARD] 阻力系数 C_d（标准近似值）
    static const float SWIMMING_WATER_DENSITY    = 1000.0; // [HARD] 水密度 ρ（kg/m³，物理定律）
    static const float SWIMMING_FRONTAL_AREA     = 0.5;    // [HARD] 正面受力面积（m²，上身投影标准近似）
    static const float SWIMMING_BASE_POWER       = 20.0;   // [SOFT fallback] 基础游泳功率（W），可通过 swimming_base_power 覆盖
    
    // 负重阈值（负浮力效应）
    static const float SWIMMING_ENCUMBRANCE_THRESHOLD = 25.0; // kg，超过此重量时静态消耗大幅增加（从20kg提高到25kg）
    static const float SWIMMING_STATIC_DRAIN_MULTIPLIER = 3.0; // 超过阈值时的静态消耗倍数（模拟踩水）
    static const float SWIMMING_FULL_PENALTY_WEIGHT = 40.0; // kg，达到此重量时应用满额惩罚
    static const float SWIMMING_LOW_INTENSITY_DISCOUNT = 0.7; // 低强度踩水时的消耗折扣（30%折扣）
    static const float SWIMMING_LOW_INTENSITY_VELOCITY = 0.2; // m/s，低于此速度视为低强度踩水
    
    // 游泳专用转换系数（游泳的代谢效率比陆地低，所以系数应略高于陆地）
    // 注意：不要乘以100，直接使用小数系数（0.0-1.0范围）
    static const float SWIMMING_ENERGY_TO_STAMINA_COEFF = 0.00005; // 游泳功率到体力消耗的转换系数
    static const float SWIMMING_DYNAMIC_POWER_EFFICIENCY = 2.0; // 动态功率效率因子（人体游泳效率约0.05，这里取折中值）
    
    // 3D 游泳模型参数（垂直方向比水平方向更“费力”）
    static const float SWIMMING_VERTICAL_DRAG_COEFFICIENT = 1.2; // 垂直方向阻力系数（非流线型）
    static const float SWIMMING_VERTICAL_FRONTAL_AREA = 0.8; // 垂直方向受力面积（m²，简化）
    static const float SWIMMING_VERTICAL_SPEED_THRESHOLD = 0.05; // m/s，垂直速度阈值
    static const float SWIMMING_EFFECTIVE_GRAVITY_COEFF = 0.15; // 水中有效重力系数（上浮时对抗负浮力/装备）
    static const float SWIMMING_BUOYANCY_FORCE_COEFF = 0.10; // 浮力对抗系数（下潜时对抗浮力）
    static const float SWIMMING_VERTICAL_UP_MULTIPLIER = 2.5; // 上浮效率惩罚倍数
    static const float SWIMMING_VERTICAL_DOWN_MULTIPLIER = 1.5; // 下潜效率惩罚倍数
    static const float SWIMMING_MAX_DRAIN_RATE = 0.15; // 每秒最大消耗（3D模型允许更高峰值）
    
    // 负重对垂直方向的影响（非线性/更贴近体感）
    // - 上浮：负重越大越困难
    // - 下潜：负重越大越容易（对抗浮力需求下降）
    static const float SWIMMING_VERTICAL_UP_BASE_BODY_FORCE_COEFF = 0.02; // 即使不背负也存在的上浮“基础费力”
    static const float SWIMMING_VERTICAL_DOWN_LOAD_RELIEF_COEFF = 0.50; // 负重能抵消浮力的比例（0-1）
    
    // 生存压力常数：确保水中静止也不会被陆地恢复覆盖（单位：W）
    static const float SWIMMING_SURVIVAL_STRESS_POWER = 15.0;
    
    // 生理功率上限：防止异常速度/模组冲突导致 totalPower 溢出（单位：W）
    static const float SWIMMING_MAX_TOTAL_POWER = 2000.0;
    
    // 湿重效应参数
    static const float WET_WEIGHT_DURATION = 30.0; // 秒，上岸后湿重持续时间
    static const float WET_WEIGHT_MIN = 5.0; // kg，最小湿重（衣服吸水）
    static const float WET_WEIGHT_MAX = 10.0; // kg，最大湿重（完全湿透）
    
    // 游泳检测参数
    static const float SWIMMING_MIN_SPEED = 0.1; // m/s，游泳最小速度阈值
    static const float SWIMMING_VERTICAL_VELOCITY_THRESHOLD = -0.5; // m/s，垂直速度阈值（检测是否在水中）
}
