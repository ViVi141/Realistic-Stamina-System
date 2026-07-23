// RSS Parameter Model - serializable configuration parameters
// Attribute-free to reduce Workbench compile ICE (Attribute AST pressure).
// Defaults live in field initializers + SCR_RSS_SettingsPresetBake.
// Disk/network persistence: float arrays via SCR_RSS_SettingsSync.
//
[BaseContainerProps()]
class SCR_RSS_Params
{
    // 能量到体力转换系数
    // 将 Pandolf 能量消耗模型（W/kg）转换为游戏体力消耗率（%/s）
    // Optuna 优化范围：0.000015 - 0.000050
    // EliteStandard: 0.000025（精英拟真）
    // TacticalAction: 0.000020（战术动作）
    // 说明：值越小，体力消耗越慢，玩家可以运动更长时间
    // --- 消耗与恢复基础 ---
    float energy_to_stamina_coeff = 0.000035;

    // 基础恢复率（每0.2秒）
    // 决定玩家在静止时的体力恢复速度
    // Optuna 优化范围：0.0003 - 0.0007
    // EliteStandard: 0.000499（精英拟真）
    // TacticalAction: 0.000440（战术动作）
    // 说明：值越大，体力恢复越快，玩家可以更快恢复体力
    float base_recovery_rate = 0.0003;

    // 站姿恢复缩放因子（相对 base_recovery_rate）
    // Optuna 拟真档常为 0.70–0.95（<1.0 表示比蹲/趴姿恢复更慢，属设计意图而非错误）
    // EliteStandard: ~0.80 | TacticalAction: ~0.76
    float standing_recovery_multiplier = 2.0;

    // 蹲姿恢复倍数
    // 蹲下时的体力恢复加成（介于站姿和趴姿之间）
    // Optuna 优化范围：1.2 - 3.0
    // 说明：值越大，蹲下时体力恢复越快
    float crouching_recovery_multiplier = 1.5;

    // 趴姿恢复倍数
    // 趴下时的体力恢复加成（相对于基础恢复率）
    // Optuna 优化范围：2.0 - 3.5
    // EliteStandard: 2.756（精英拟真）
    // TacticalAction: 2.785（战术动作）
    // 说明：值越大，趴下时体力恢复越快
    float prone_recovery_multiplier = 2.2;

    // 负重恢复惩罚系数
    // 负重对体力恢复的惩罚系数（基于 Pandolf 模型）
    // 公式：penalty = (load_ratio)^exponent * coeff
    // Optuna 优化范围：0.0001 - 0.0005
    // EliteStandard: 0.000302（精英拟真）
    // TacticalAction: 0.000161（战术动作）
    // 说明：值越大，负重时体力恢复越慢
    float load_recovery_penalty_coeff = 0.0004;

    // 负重恢复惩罚指数
    // 负重对体力恢复的惩罚指数（控制惩罚曲线的陡峭程度）
    // Optuna 优化范围：1.5 - 3.0
    // EliteStandard: 2.401（精英拟真）
    // TacticalAction: 2.848（战术动作）
    // 说明：值越大，负重惩罚曲线越陡峭，重装时恢复越慢
    float load_recovery_penalty_exponent = 2.0;

    // 负重速度惩罚系数
    // 负重对移动速度的惩罚系数（基于体重百分比）
    // 公式：speed_penalty = coeff * (load_weight / body_weight)
    // Optuna 优化范围：0.15 - 0.35
    // EliteStandard: 0.272（精英拟真）
    // TacticalAction: 0.217（战术动作）
    // 说明：值越大，负重时移动速度越慢
    // --- 负重 ---
    float encumbrance_speed_penalty_coeff = 0.20;

    // 负重速度惩罚指数（幂次）
    // 控制较高负重时速度惩罚的陡峭程度
    float encumbrance_speed_penalty_exponent = 1.5;

    // 负重速度惩罚上限（0-1）
    // 防止惩罚过大导致完全无法移动
    float encumbrance_speed_penalty_max = 0.75;

    // 负重体力消耗系数
    // 负重对体力消耗的加成系数（基于体重百分比）
    // 公式：drain_multiplier = 1 + coeff * (load_weight / body_weight)
    // Optuna 优化范围：1.0 - 2.5
    // EliteStandard: 1.939（精英拟真）
    // TacticalAction: 1.636（战术动作）
    // 说明：值越大，负重时体力消耗越快
    float encumbrance_stamina_drain_coeff = 1.5;

    // T1 负重代谢阻尼：训练有素操作员只承受负重额外代谢的该比例（0~1）
    // 与 Python 数字孪生/优化器同步，避免测试脚本与游戏表现差异
    float load_metabolic_dampening = 0.70;

    // 恢复速率上限（每 0.2s tick），防止过快回血；与 Python 数字孪生同步
    float max_recovery_per_tick = 0.0004;

    // Sprint体力消耗倍数
    // Sprint 时的体力消耗倍数（相对于 Run）
    // Optuna 优化范围：2.0 - 4.0
    // EliteStandard: 3.01（精英拟真）
    // TacticalAction: 2.45（战术动作）
    // 说明：值越大，Sprint 时体力消耗越快
    float sprint_stamina_drain_multiplier = 3.5;

    // 疲劳累积系数
    // 长时间运动时的疲劳累积速度（基于运动持续时间）
    // 公式：fatigue_factor = 1.0 + coeff * max(0, exercise_duration - 5.0)
    // Optuna 优化范围：0.010 - 0.030
    // EliteStandard: 0.02（精英拟真）
    // TacticalAction: 0.023（战术动作）
    // 说明：值越大，疲劳累积越快，长期运动后恢复越慢
    // --- 疲劳 ---
    float fatigue_accumulation_coeff = 0.015;

    // 最大疲劳因子
    // 疲劳因子的最大值（限制在 1.0 - 3.0 之间）
    // Optuna 优化范围：1.5 - 3.0
    // EliteStandard: 2.70（精英拟真）
    // TacticalAction: 2.50（战术动作）
    // 说明：值越大，最大疲劳惩罚越严重，长期运动后恢复越慢
    float fatigue_max_factor = 2.0;

    // 有氧效率因子
    // 有氧运动时的能量利用效率
    // Optuna 优化范围：0.8 - 1.2
    // 说明：值越大，有氧运动效率越高，体力消耗越慢
    float aerobic_efficiency_factor = 1.0;

    // 无氧效率因子
    // 无氧运动时的能量利用效率
    // Optuna 优化范围：1.0 - 1.8
    // 说明：值越大，无氧运动效率越高，体力消耗越慢
    float anaerobic_efficiency_factor = 1.3;

    // 恢复非线性系数
    // 控制恢复速度的非线性衰减
    // Optuna 优化范围：0.3 - 0.7
    // 说明：值越大，恢复速度随体力水平变化越明显
    float recovery_nonlinear_coeff = 0.5;

    // 快速恢复倍数
    // 体力水平高时的恢复加成
    // Optuna 优化范围：2.5 - 4.5
    // 说明：值越大，高体力时恢复越快
    float fast_recovery_multiplier = 3.0;

    // 中速恢复倍数
    // 体力水平中等时的恢复加成
    // Optuna 优化范围：1.5 - 2.5
    // 说明：值越大，中等体力时恢复越快
    float medium_recovery_multiplier = 1.8;

    // 慢速恢复倍数
    // 体力水平低时的恢复惩罚
    // Optuna 优化范围：0.5 - 0.8
    // 说明：值越小，低体力时恢复越慢
    float slow_recovery_multiplier = 0.6;

    // 边际衰减阈值
    // 恢复速度开始衰减的体力阈值
    // Optuna 优化范围：0.7 - 0.95
    // 说明：值越大，恢复速度在更高体力水平才开始衰减
    float marginal_decay_threshold = 0.85;

    // 边际衰减系数
    // 恢复速度衰减的强度
    // Optuna 优化范围：0.8 - 1.3
    // 说明：值越大，恢复速度衰减越快
    float marginal_decay_coeff = 1.0;

    // 最小恢复体力阈值
    // 开始恢复所需的最小体力水平
    // Optuna 优化范围：0.1 - 0.3
    // 说明：值越大，需要更高体力才能开始恢复
    float min_recovery_stamina_threshold = 0.15;

    // 最小恢复休息时间（秒）
    // 开始快速恢复所需的休息时间
    // Optuna 优化范围：10 - 20
    // 说明：值越大，需要休息更长时间才能开始快速恢复
    float min_recovery_rest_time_seconds = 15.0;

    // 冲刺速度提升
    // Sprint 相对于 Run 的速度提升比例
    // Optuna 优化范围：0.2 - 0.4
    // 说明：值越大，Sprint 速度提升越明显
    float sprint_speed_boost = 0.3;

    // Sprint速度阈值
    // 触发 Sprint 的最低速度（m/s）
    // Optuna 优化范围：4.8 - 5.6
    // 说明：值越大，需要更快速度才能触发 Sprint
    float sprint_velocity_threshold = 5.5;

    // 意志力平台期阈值（Hardcore 新增）
    // 体力高于此值时保持恒定目标速度（模拟意志力克服早期疲劳）
    // Hardcore 默认 0.35，原 0.25
    // 说明：值越大，疲劳感越早出现，拟真度越高
    float willpower_threshold = 0.35;

    // 有氧 Sprint 最低体力（0-1）。与 anaerobic_sprint_enable_threshold（W' 池）分工不同。
    float sprint_enable_threshold = 0.25;

    // 蹲姿消耗倍数
    // 蹲下时的体力消耗倍数（相对于站姿）
    // Optuna 优化范围：1.8 - 2.3
    // 说明：值越大，蹲下时体力消耗越快
    // --- 姿态 ---
    float posture_crouch_multiplier = 2.0;

    // 趴姿消耗倍数
    // 趴下时的体力消耗倍数（相对于站姿）
    // Optuna 优化范围：2.2 - 2.8
    // 说明：值越大，趴下时体力消耗越快
    float posture_prone_multiplier = 2.5;


    float jump_efficiency = 0.22;

    float jump_height_guess = 0.5;

    float jump_horizontal_speed_guess = 0.0;

    float climb_iso_efficiency = 0.12;

    // 跃过起始体力消耗

    // 上坡系数
    // 上坡时的体力消耗系数
    // Optuna 优化范围：0.05 - 0.1
    // 说明：值越大，上坡消耗体力越快
    // --- 坡度 ---
    float slope_uphill_coeff = 0.08;

    // 下坡系数
    // 下坡时的体力消耗系数
    // Optuna 优化范围：0.02 - 0.05
    // 说明：值越大，下坡消耗体力越快
    float slope_downhill_coeff = 0.03;

    // 游泳基础功率
    // 游泳时的基础功率消耗（W）
    // Optuna 优化范围：15 - 30
    // 说明：值越大，游泳基础消耗越大
    // --- 游泳 ---
    float swimming_base_power = 25.0;

    // 游泳负重阈值
    // 游泳时开始影响速度的负重阈值（kg）
    // Optuna 优化范围：15 - 30
    // 说明：值越大，需要更重才会影响游泳速度
    float swimming_encumbrance_threshold = 25.0;

    // 游泳静态消耗倍数
    // 静止游泳时的体力消耗倍数
    // Optuna 优化范围：2.5 - 4.0
    // 说明：值越大，静止游泳消耗体力越快
    float swimming_static_drain_multiplier = 3.5;

    // 游泳动态功率效率
    // 动态游泳时的功率效率
    // Optuna 优化范围：1.8 - 2.6
    // 说明：值越大，动态游泳效率越高，消耗越慢
    float swimming_dynamic_power_efficiency = 2.2;

    // 游泳能量到体力转换系数
    // 游泳时的能量到体力转换系数
    // Optuna 优化范围：3e-05 - 8e-05
    // 说明：值越大，游泳时体力消耗越快
    float swimming_energy_to_stamina_coeff = 5e-05;

    // 环境热应激最大倍数
    // 高温环境下的最大体力消耗倍数
    // Optuna 优化范围：1.1 - 1.5
    // 说明：值越大，高温环境下体力消耗越快
    // --- 环境 ---
    float env_heat_stress_max_multiplier = 1.3;

    // 环境雨量最大值
    // 雨天对体力消耗的最大影响
    // Optuna 优化范围：5 - 10
    // 说明：值越大，雨天对体力消耗影响越大
    float env_rain_weight_max = 7.0;

    // 环境风阻系数
    // 风对体力消耗的影响系数
    // Optuna 优化范围：0.03 - 0.08
    // 说明：值越大，风对体力消耗影响越大
    float env_wind_resistance_coeff = 0.05;

    // 环境泥泞最大惩罚
    // 泥泞地面对体力消耗的最大惩罚
    // Optuna 优化范围：0.3 - 0.6
    // 说明：值越大，泥泞地面消耗体力越快
    float env_mud_penalty_max = 0.45;

    // 环境温度热惩罚系数
    // 高温对体力消耗的惩罚系数
    // Optuna 优化范围：0.01 - 0.03
    // 说明：值越大，高温对体力消耗影响越大
    float env_temperature_heat_penalty_coeff = 0.02;

    // 环境温度冷恢复惩罚系数
    // 低温对体力恢复的惩罚系数
    // Optuna 优化范围：0.03 - 0.08
    // 说明：值越大，低温对体力恢复影响越大
    float env_temperature_cold_recovery_penalty_coeff = 0.05;

    // 环境地表湿度趴下惩罚
    // 湿地趴下时的恢复惩罚
    // Optuna 优化范围：0.1 - 0.2
    // 说明：值越大，湿地趴下时恢复越慢
    float env_surface_wetness_prone_penalty = 0.15;

    // --- v5 双池 / 代谢锚点 ---
    float sustainable_watts = 780.0;

    float v5_walk_speed_ms = 1.4;

    float v5_run_speed_ms = 2.8;

    float v5_sprint_speed_ms = 4.5;

    float anaerobic_sprint_enable_threshold = 0.20;

    float burst_cooldown_full_seconds = 180.0;

    float burst_cooldown_short_seconds = 75.0;

    float anaerobic_drain_per_sec = 0.12;

    float anaerobic_recovery_per_sec = 0.08;

    // --- v6 CP-W' 学术拟真 ---
    float critical_power_watts = 780.0;

    float w_prime_max_joules = 20000.0;

    float w_prime_recovery_w_per_s = 12.0;

    float sprint_power_cap_watts = 1200.0;
}
