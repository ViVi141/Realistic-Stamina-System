// RSS配置类 - 使用Reforger官方标准
// 利用 [BaseContainerProps] 和 [Attribute] 属性实现自动序列化
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_Settings.c

// ==================== 预设参数包（SCR_RSS_Params）====================
// 包含 Optuna 优化核心参数，每预设一份。Custom 预设下可直接改 JSON 调参。
// 分组：恢复 | 负重 | 疲劳 | 姿态 | 跳跃/攀爬 | 坡度 | 游泳 | 环境
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
    [Attribute(defvalue: "0.000035", desc: "Energy→stamina coeff. Lower=slower drain. Range ~1.5e-5~5e-5. | 能量转体力系数，值越小消耗越慢")]
    float energy_to_stamina_coeff;

    // 基础恢复率（每0.2秒）
    // 决定玩家在静止时的体力恢复速度
    // Optuna 优化范围：0.0003 - 0.0007
    // EliteStandard: 0.000499（精英拟真）
    // TacticalAction: 0.000440（战术动作）
    // 说明：值越大，体力恢复越快，玩家可以更快恢复体力
    [Attribute(defvalue: "0.0003", desc: "Base recovery rate per tick (0.2 seconds).\nDetermines stamina recovery speed when stationary.\nOptimized range: 0.0003 - 0.0007.\nHigher value = faster stamina recovery.\n基础恢复率（每0.2秒）。\n决定玩家在静止时的体力恢复速度。\nOptuna 优化范围：0.0003 - 0.0007。\n值越大，体力恢复越快。")]
    float base_recovery_rate;

    // 站姿恢复倍数
    // 站立时的体力恢复加成（相对于基础恢复率）
    // Optuna 优化范围：1.0 - 2.5
    // EliteStandard: 1.765（精英拟真）
    // TacticalAction: 1.382（战术动作）
    // 说明：值越大，站立时体力恢复越快
    [Attribute(defvalue: "2.0", desc: "Standing posture recovery multiplier.\nStamina recovery bonus when standing (relative to base recovery rate).\nOptimized range: 1.0 - 2.5.\nHigher value = faster recovery while standing.\n站姿恢复倍数。\n站立时的体力恢复加成（相对于基础恢复率）。\nOptuna 优化范围：1.0 - 2.5。\n值越大，站立时体力恢复越快。")]
    float standing_recovery_multiplier;

    // 趴姿恢复倍数
    // 趴下时的体力恢复加成（相对于基础恢复率）
    // Optuna 优化范围：2.0 - 3.5
    // EliteStandard: 2.756（精英拟真）
    // TacticalAction: 2.785（战术动作）
    // 说明：值越大，趴下时体力恢复越快
    [Attribute(defvalue: "2.2", desc: "Prone posture recovery multiplier.\nStamina recovery bonus when prone (relative to base recovery rate).\nOptimized range: 2.0 - 3.5.\nHigher value = faster recovery while prone.\n趴姿恢复倍数。\n趴下时的体力恢复加成（相对于基础恢复率）。\nOptuna 优化范围：2.0 - 3.5。\n值越大，趴下时体力恢复越快。")]
    float prone_recovery_multiplier;

    // 负重恢复惩罚系数
    // 负重对体力恢复的惩罚系数（基于 Pandolf 模型）
    // 公式：penalty = (load_ratio)^exponent * coeff
    // Optuna 优化范围：0.0001 - 0.0005
    // EliteStandard: 0.000302（精英拟真）
    // TacticalAction: 0.000161（战术动作）
    // 说明：值越大，负重时体力恢复越慢
    [Attribute(defvalue: "0.0004", desc: "Load recovery penalty coefficient.\nPenalty coefficient for stamina recovery under load (based on Pandolf model).\nFormula: penalty = (load_ratio)^exponent * coeff.\nOptimized range: 0.0001 - 0.0005.\nHigher value = slower recovery under load.\n负重恢复惩罚系数。\n负重对体力恢复的惩罚系数（基于 Pandolf 模型）。\n公式：penalty = (load_ratio)^exponent * coeff。\nOptuna 优化范围：0.0001 - 0.0005。\n值越大，负重时体力恢复越慢。")]
    float load_recovery_penalty_coeff;

    // 负重恢复惩罚指数
    // 负重对体力恢复的惩罚指数（控制惩罚曲线的陡峭程度）
    // Optuna 优化范围：1.5 - 3.0
    // EliteStandard: 2.401（精英拟真）
    // TacticalAction: 2.848（战术动作）
    // 说明：值越大，负重惩罚曲线越陡峭，重装时恢复越慢
    [Attribute(defvalue: "2.0", desc: "Load recovery penalty exponent.\nPenalty exponent for stamina recovery under load (controls penalty curve steepness).\nOptimized range: 1.5 - 3.0.\nHigher value = steeper penalty curve, slower recovery for heavy loads.\n负重恢复惩罚指数。\n负重对体力恢复的惩罚指数（控制惩罚曲线的陡峭程度）。\nOptuna 优化范围：1.5 - 3.0。\n值越大，负重惩罚曲线越陡峭，重装时恢复越慢。")]
    float load_recovery_penalty_exponent;

    // 负重速度惩罚系数
    // 负重对移动速度的惩罚系数（基于体重百分比）
    // 公式：speed_penalty = coeff * (load_weight / body_weight)
    // Optuna 优化范围：0.15 - 0.35
    // EliteStandard: 0.272（精英拟真）
    // TacticalAction: 0.217（战术动作）
    // 说明：值越大，负重时移动速度越慢
    // --- 负重 ---
    [Attribute(defvalue: "0.20", desc: "Encumbrance speed penalty. Higher=slower when loaded. | 负重速度惩罚")]
    float encumbrance_speed_penalty_coeff;

    // 负重速度惩罚指数（幂次）
    // 控制较高负重时速度惩罚的陡峭程度
    [Attribute(defvalue: "1.5", desc: "Encumbrance speed penalty exponent.\nPower applied to body mass percent when computing speed penalty. Higher=steeper penalty.\nOptimized range: 1.0 - 3.0. | 负重速度惩罚指数。")]
    float encumbrance_speed_penalty_exponent;

    // 负重速度惩罚上限（0-1）
    // 防止惩罚过大导致完全无法移动
    [Attribute(defvalue: "0.75", desc: "Encumbrance speed penalty max.\nUpper cap for speed penalty (0.0-1.0). Default 0.75 (max 75% slow down). Optimized range: 0.4 - 0.95. | 负重速度惩罚上限。")]
    float encumbrance_speed_penalty_max;

    // 负重体力消耗系数
    // 负重对体力消耗的加成系数（基于体重百分比）
    // 公式：drain_multiplier = 1 + coeff * (load_weight / body_weight)
    // Optuna 优化范围：1.0 - 2.5
    // EliteStandard: 1.939（精英拟真）
    // TacticalAction: 1.636（战术动作）
    // 说明：值越大，负重时体力消耗越快
    [Attribute(defvalue: "1.5", desc: "Encumbrance stamina drain coefficient.\nStamina drain bonus coefficient for load (based on body mass percentage).\nFormula: drain_multiplier = 1 + coeff * (load_weight / body_weight).\nOptimized range: 1.0 - 2.5.\nHigher value = faster stamina drain under load.\n负重体力消耗系数。\n负重对体力消耗的加成系数（基于体重百分比）。\n公式：drain_multiplier = 1 + coeff * (load_weight / body_weight)。\nOptuna 优化范围：1.0 - 2.5。\n值越大，负重时体力消耗越快。")]
    float encumbrance_stamina_drain_coeff;

    // Sprint体力消耗倍数
    // Sprint 时的体力消耗倍数（相对于 Run）
    // Optuna 优化范围：2.0 - 4.0
    // EliteStandard: 3.01（精英拟真）
    // TacticalAction: 2.45（战术动作）
    // 说明：值越大，Sprint 时体力消耗越快
    [Attribute(defvalue: "3.5", desc: "Sprint stamina drain multiplier.\nStamina drain multiplier when sprinting (relative to running).\nOptimized range: 2.0 - 4.0.\nHigher value = faster stamina drain when sprinting.\nSprint体力消耗倍数。\nSprint 时的体力消耗倍数（相对于 Run）。\nOptuna 优化范围：2.0 - 4.0。\n值越大，Sprint 时体力消耗越快.")]
    float sprint_stamina_drain_multiplier;

    // 疲劳累积系数
    // 长时间运动时的疲劳累积速度（基于运动持续时间）
    // 公式：fatigue_factor = 1.0 + coeff * max(0, exercise_duration - 5.0)
    // Optuna 优化范围：0.010 - 0.030
    // EliteStandard: 0.02（精英拟真）
    // TacticalAction: 0.023（战术动作）
    // 说明：值越大，疲劳累积越快，长期运动后恢复越慢
    // --- 疲劳 ---
    [Attribute(defvalue: "0.015", desc: "Fatigue accumulation. Higher=more fatigue over time. | 疲劳累积系数")]
    float fatigue_accumulation_coeff;

    // 最大疲劳因子
    // 疲劳因子的最大值（限制在 1.0 - 3.0 之间）
    // Optuna 优化范围：1.5 - 3.0
    // EliteStandard: 2.70（精英拟真）
    // TacticalAction: 2.50（战术动作）
    // 说明：值越大，最大疲劳惩罚越严重，长期运动后恢复越慢
    [Attribute(defvalue: "2.0", desc: "Maximum fatigue factor.\nMaximum fatigue factor (clamped between 1.0 - 3.0).\nOptimized range: 1.5 - 3.0.\nHigher value = more severe fatigue penalty, slower recovery after prolonged exercise.\n最大疲劳因子。\n疲劳因子的最大值（限制在 1.0 - 3.0 之间）。\nOptuna 优化范围：1.5 - 3.0。\n值越大，最大疲劳惩罚越严重，长期运动后恢复越慢。")]
    float fatigue_max_factor;

    // 有氧效率因子
    // 有氧运动时的能量利用效率
    // Optuna 优化范围：0.8 - 1.2
    // 说明：值越大，有氧运动效率越高，体力消耗越慢
    [Attribute(defvalue: "1.0", desc: "Aerobic efficiency factor.\nEnergy utilization efficiency during aerobic exercise.\nOptimized range: 0.8 - 1.2.\nHigher value = higher aerobic efficiency, slower stamina drain.\n有氧效率因子。\n有氧运动时的能量利用效率。\nOptuna 优化范围：0.8 - 1.2。\n值越大，有氧运动效率越高，体力消耗越慢。")]
    float aerobic_efficiency_factor;

    // 无氧效率因子
    // 无氧运动时的能量利用效率
    // Optuna 优化范围：1.0 - 1.8
    // 说明：值越大，无氧运动效率越高，体力消耗越慢
    [Attribute(defvalue: "1.3", desc: "Anaerobic efficiency factor.\nEnergy utilization efficiency during anaerobic exercise.\nOptimized range: 1.0 - 1.8.\nHigher value = higher anaerobic efficiency, slower stamina drain.\n无氧效率因子。\n无氧运动时的能量利用效率。\nOptuna 优化范围：1.0 - 1.8。\n值越大，无氧运动效率越高，体力消耗越慢。")]
    float anaerobic_efficiency_factor;

    // 恢复非线性系数
    // 控制恢复速度的非线性衰减
    // Optuna 优化范围：0.3 - 0.7
    // 说明：值越大，恢复速度随体力水平变化越明显
    [Attribute(defvalue: "0.5", desc: "Recovery nonlinear coefficient.\nControls nonlinear decay of recovery speed.\nOptimized range: 0.3 - 0.7.\nHigher value = more pronounced recovery speed variation with stamina level.\n恢复非线性系数。\n控制恢复速度的非线性衰减。\nOptuna 优化范围：0.3 - 0.7。\n值越大，恢复速度随体力水平变化越明显。")]
    float recovery_nonlinear_coeff;

    // 快速恢复倍数
    // 体力水平高时的恢复加成
    // Optuna 优化范围：2.5 - 4.5
    // 说明：值越大，高体力时恢复越快
    [Attribute(defvalue: "3.0", desc: "Fast recovery multiplier.\nRecovery bonus when stamina level is high.\nOptimized range: 2.5 - 4.5.\nHigher value = faster recovery when stamina is high.\n快速恢复倍数。\n体力水平高时的恢复加成。\nOptuna 优化范围：2.5 - 4.5。\n值越大，高体力时恢复越快。")]
    float fast_recovery_multiplier;

    // 中速恢复倍数
    // 体力水平中等时的恢复加成
    // Optuna 优化范围：1.5 - 2.5
    // 说明：值越大，中等体力时恢复越快
    [Attribute(defvalue: "1.8", desc: "Medium recovery multiplier.\nRecovery bonus when stamina level is medium.\nOptimized range: 1.5 - 2.5.\nHigher value = faster recovery when stamina is medium.\n中速恢复倍数。\n体力水平中等时的恢复加成。\nOptuna 优化范围：1.5 - 2.5。\n值越大，中等体力时恢复越快。")]
    float medium_recovery_multiplier;

    // 慢速恢复倍数
    // 体力水平低时的恢复惩罚
    // Optuna 优化范围：0.5 - 0.8
    // 说明：值越小，低体力时恢复越慢
    [Attribute(defvalue: "0.6", desc: "Slow recovery multiplier.\nRecovery penalty when stamina level is low.\nOptimized range: 0.5 - 0.8.\nLower value = slower recovery when stamina is low.\n慢速恢复倍数。\n体力水平低时的恢复惩罚。\nOptuna 优化范围：0.5 - 0.8。\n值越小，低体力时恢复越慢。")]
    float slow_recovery_multiplier;

    // 边际衰减阈值
    // 恢复速度开始衰减的体力阈值
    // Optuna 优化范围：0.7 - 0.95
    // 说明：值越大，恢复速度在更高体力水平才开始衰减
    [Attribute(defvalue: "0.85", desc: "Marginal decay threshold.\nStamina threshold where recovery speed starts to decay.\nOptimized range: 0.7 - 0.95.\nHigher value = recovery decay starts at higher stamina level.\n边际衰减阈值。\n恢复速度开始衰减的体力阈值。\nOptuna 优化范围：0.7 - 0.95。\n值越大，恢复速度在更高体力水平才开始衰减。")]
    float marginal_decay_threshold;

    // 边际衰减系数
    // 恢复速度衰减的强度
    // Optuna 优化范围：0.8 - 1.3
    // 说明：值越大，恢复速度衰减越快
    [Attribute(defvalue: "1.0", desc: "Marginal decay coefficient.\nIntensity of recovery speed decay.\nOptimized range: 0.8 - 1.3.\nHigher value = faster recovery speed decay.\n边际衰减系数。\n恢复速度衰减的强度。\nOptuna 优化范围：0.8 - 1.3。\n值越大，恢复速度衰减越快。")]
    float marginal_decay_coeff;

    // 最小恢复体力阈值
    // 开始恢复所需的最小体力水平
    // Optuna 优化范围：0.1 - 0.3
    // 说明：值越大，需要更高体力才能开始恢复
    [Attribute(defvalue: "0.15", desc: "Minimum recovery stamina threshold.\nMinimum stamina level required to start recovery.\nOptimized range: 0.1 - 0.3.\nHigher value = higher stamina needed to start recovery.\n最小恢复体力阈值。\n开始恢复所需的最小体力水平。\nOptuna 优化范围：0.1 - 0.3。\n值越大，需要更高体力才能开始恢复。")]
    float min_recovery_stamina_threshold;

    // 最小恢复休息时间（秒）
    // 开始快速恢复所需的休息时间
    // Optuna 优化范围：10 - 20
    // 说明：值越大，需要休息更长时间才能开始快速恢复
    [Attribute(defvalue: "15.0", desc: "Minimum recovery rest time in seconds.\nRest time required to start fast recovery.\nOptimized range: 10 - 20.\nHigher value = longer rest needed to start fast recovery.\n最小恢复休息时间（秒）。\n开始快速恢复所需的休息时间。\nOptuna 优化范围：10 - 20。\n值越大，需要休息更长时间才能开始快速恢复。")]
    float min_recovery_rest_time_seconds;

    // 冲刺速度提升
    // Sprint 相对于 Run 的速度提升比例
    // Optuna 优化范围：0.2 - 0.4
    // 说明：值越大，Sprint 速度提升越明显
    [Attribute(defvalue: "0.3", desc: "Sprint speed boost.\nSpeed boost ratio of sprint relative to run.\nOptimized range: 0.2 - 0.4.\nHigher value = more significant sprint speed boost.\n冲刺速度提升。\nSprint 相对于 Run 的速度提升比例。\nOptuna 优化范围：0.2 - 0.4。\n值越大，Sprint 速度提升越明显。")]
    float sprint_speed_boost;

    // Sprint速度阈值
    // 触发 Sprint 的最低速度（m/s）
    // Optuna 优化范围：4.8 - 5.6
    // 说明：值越大，需要更快速度才能触发 Sprint
    [Attribute(defvalue: "5.2", desc: "Sprint velocity threshold.\nMinimum speed (m/s) to trigger sprint mode.\nOptimized range: 4.8 - 5.6.\nHigher value = faster speed required to trigger sprint.\nSprint速度阈值。\n触发 Sprint 的最低速度（m/s）。\nOptuna 优化范围：4.8 - 5.6。\n值越大，需要更快速度才能触发 Sprint。")]
    float sprint_velocity_threshold;

    // 蹲姿消耗倍数
    // 蹲下时的体力消耗倍数（相对于站姿）
    // Optuna 优化范围：1.8 - 2.3
    // 说明：值越大，蹲下时体力消耗越快
    // --- 姿态 ---
    [Attribute(defvalue: "2.0", desc: "Crouch stamina multiplier. | 蹲姿消耗倍数")]
    float posture_crouch_multiplier;

    // 趴姿消耗倍数
    // 趴下时的体力消耗倍数（相对于站姿）
    // Optuna 优化范围：2.2 - 2.8
    // 说明：值越大，趴下时体力消耗越快
    [Attribute(defvalue: "2.5", desc: "Posture prone multiplier.\nStamina drain multiplier when prone (relative to standing).\nOptimized range: 2.2 - 2.8.\nHigher value = faster stamina drain when prone.\n趴姿消耗倍数。\n趴下时的体力消耗倍数（相对于站姿）。\nOptuna 优化范围：2.2 - 2.8。\n值越大，趴下时体力消耗越快。")]
    float posture_prone_multiplier;

    // 跳跃基础体力消耗
    // 单次跳跃的基础体力消耗
    // Optuna 优化范围：0.025 - 0.05
    // 说明：值越大，跳跃消耗体力越多
    [Attribute(defvalue: "0.035", desc: "Jump stamina base cost.\nBase stamina cost for a single jump.\nOptimized range: 0.025 - 0.05.\nHigher value = more stamina consumed per jump.\n跳跃基础体力消耗。\n单次跳跃的基础体力消耗。\nOptuna 优化范围：0.025 - 0.05。\n值越大，跳跃消耗体力越多。")]
    float jump_stamina_base_cost;

    // 跃过起始体力消耗
    // 跃过障碍物时的起始体力消耗
    // Optuna 优化范围：0.01 - 0.025
    // 说明：值越大，跃过起始消耗体力越多
    [Attribute(defvalue: "0.02", desc: "Vault stamina start cost.\nInitial stamina cost for vaulting over obstacles.\nOptimized range: 0.01 - 0.025.\nHigher value = more stamina consumed at vault start.\n跃过起始体力消耗。\n跃过障碍物时的起始体力消耗。\nOptuna 优化范围：0.01 - 0.025。\n值越大，跃过起始消耗体力越多。")]
    float vault_stamina_start_cost;

    // 攀爬体力消耗（每tick）
    // 攀爬时的体力消耗（每0.2秒）
    // Optuna 优化范围：0.008 - 0.015
    // 说明：值越大，攀爬消耗体力越快
    [Attribute(defvalue: "0.01", desc: "Climb stamina tick cost.\nStamina cost per tick (0.2s) when climbing.\nOptimized range: 0.008 - 0.015.\nHigher value = faster stamina drain when climbing.\n攀爬体力消耗（每tick）。\n攀爬时的体力消耗（每0.2秒）。\nOptuna 优化范围：0.008 - 0.015。\n值越大，攀爬消耗体力越快。")]
    float climb_stamina_tick_cost;

    // 连续跳跃惩罚
    // 连续跳跃时的体力消耗惩罚
    // Optuna 优化范围：0.4 - 0.7
    // 说明：值越大，连续跳跃惩罚越严重
    [Attribute(defvalue: "0.5", desc: "Jump consecutive penalty.\nStamina drain penalty for consecutive jumps.\nOptimized range: 0.4 - 0.7.\nHigher value = more severe penalty for consecutive jumps.\n连续跳跃惩罚。\n连续跳跃时的体力消耗惩罚。\nOptuna 优化范围：0.4 - 0.7。\n值越大，连续跳跃惩罚越严重。")]
    float jump_consecutive_penalty;

    // 上坡系数
    // 上坡时的体力消耗系数
    // Optuna 优化范围：0.05 - 0.1
    // 说明：值越大，上坡消耗体力越快
    // --- 坡度 ---
    [Attribute(defvalue: "0.08", desc: "Uphill stamina coeff. | 上坡消耗系数")]
    float slope_uphill_coeff;

    // 下坡系数
    // 下坡时的体力消耗系数
    // Optuna 优化范围：0.02 - 0.05
    // 说明：值越大，下坡消耗体力越快
    [Attribute(defvalue: "0.03", desc: "Slope downhill coefficient.\nStamina drain coefficient when going downhill.\nOptimized range: 0.02 - 0.05.\nHigher value = faster stamina drain when going downhill.\n下坡系数。\n下坡时的体力消耗系数。\nOptuna 优化范围：0.02 - 0.05。\n值越大，下坡消耗体力越快。")]
    float slope_downhill_coeff;

    // 游泳基础功率
    // 游泳时的基础功率消耗（W）
    // Optuna 优化范围：15 - 30
    // 说明：值越大，游泳基础消耗越大
    // --- 游泳 ---
    [Attribute(defvalue: "25.0", desc: "Swimming base power (W). | 游泳基础功率")]
    float swimming_base_power;

    // 游泳负重阈值
    // 游泳时开始影响速度的负重阈值（kg）
    // Optuna 优化范围：15 - 30
    // 说明：值越大，需要更重才会影响游泳速度
    [Attribute(defvalue: "25.0", desc: "Swimming encumbrance threshold.\nLoad threshold (kg) that starts affecting swimming speed.\nOptimized range: 15 - 30.\nHigher value = heavier load needed to affect swimming speed.\n游泳负重阈值。\n游泳时开始影响速度的负重阈值（kg）。\nOptuna 优化范围：15 - 30。\n值越大，需要更重才会影响游泳速度。")]
    float swimming_encumbrance_threshold;

    // 游泳静态消耗倍数
    // 静止游泳时的体力消耗倍数
    // Optuna 优化范围：2.5 - 4.0
    // 说明：值越大，静止游泳消耗体力越快
    [Attribute(defvalue: "3.5", desc: "Swimming static drain multiplier.\nStamina drain multiplier when swimming statically.\nOptimized range: 2.5 - 4.0.\nHigher value = faster stamina drain when swimming statically.\n游泳静态消耗倍数。\n静止游泳时的体力消耗倍数。\nOptuna 优化范围：2.5 - 4.0。\n值越大，静止游泳消耗体力越快。")]
    float swimming_static_drain_multiplier;

    // 游泳动态功率效率
    // 动态游泳时的功率效率
    // Optuna 优化范围：1.8 - 2.6
    // 说明：值越大，动态游泳效率越高，消耗越慢
    [Attribute(defvalue: "2.2", desc: "Swimming dynamic power efficiency.\nPower efficiency when swimming dynamically.\nOptimized range: 1.8 - 2.6.\nHigher value = higher dynamic swimming efficiency, slower drain.\n游泳动态功率效率。\n动态游泳时的功率效率。\nOptuna 优化范围：1.8 - 2.6。\n值越大，动态游泳效率越高，消耗越慢。")]
    float swimming_dynamic_power_efficiency;

    // 游泳能量到体力转换系数
    // 游泳时的能量到体力转换系数
    // Optuna 优化范围：3e-05 - 8e-05
    // 说明：值越大，游泳时体力消耗越快
    [Attribute(defvalue: "5e-05", desc: "Swimming energy to stamina coeff.\nEnergy to stamina conversion coefficient when swimming.\nOptimized range: 3e-05 - 8e-05.\nHigher value = faster stamina drain when swimming.\n游泳能量到体力转换系数。\n游泳时的能量到体力转换系数。\nOptuna 优化范围：3e-05 - 8e-05。\n值越大，游泳时体力消耗越快。")]
    float swimming_energy_to_stamina_coeff;

    // 环境热应激最大倍数
    // 高温环境下的最大体力消耗倍数
    // Optuna 优化范围：1.1 - 1.5
    // 说明：值越大，高温环境下体力消耗越快
    // --- 环境 ---
    [Attribute(defvalue: "1.3", desc: "Heat stress max multiplier. | 热应激最大倍数")]
    float env_heat_stress_max_multiplier;

    // 环境雨量最大值
    // 雨天对体力消耗的最大影响
    // Optuna 优化范围：5 - 10
    // 说明：值越大，雨天对体力消耗影响越大
    [Attribute(defvalue: "7.0", desc: "Environment rain weight max.\nMaximum impact of rain on stamina drain.\nOptimized range: 5 - 10.\nHigher value = greater rain impact on stamina drain.\n环境雨量最大值。\n雨天对体力消耗的最大影响。\nOptuna 优化范围：5 - 10。\n值越大，雨天对体力消耗影响越大。")]
    float env_rain_weight_max;

    // 环境风阻系数
    // 风对体力消耗的影响系数
    // Optuna 优化范围：0.03 - 0.08
    // 说明：值越大，风对体力消耗影响越大
    [Attribute(defvalue: "0.05", desc: "Environment wind resistance coeff.\nWind impact coefficient on stamina drain.\nOptimized range: 0.03 - 0.08.\nHigher value = greater wind impact on stamina drain.\n环境风阻系数。\n风对体力消耗的影响系数。\nOptuna 优化范围：0.03 - 0.08。\n值越大，风对体力消耗影响越大。")]
    float env_wind_resistance_coeff;

    // 环境泥泞最大惩罚
    // 泥泞地面对体力消耗的最大惩罚
    // Optuna 优化范围：0.3 - 0.6
    // 说明：值越大，泥泞地面消耗体力越快
    [Attribute(defvalue: "0.45", desc: "Environment mud penalty max.\nMaximum stamina drain penalty in muddy terrain.\nOptimized range: 0.3 - 0.6.\nHigher value = faster stamina drain in muddy terrain.\n环境泥泞最大惩罚。\n泥泞地面对体力消耗的最大惩罚。\nOptuna 优化范围：0.3 - 0.6。\n值越大，泥泞地面消耗体力越快。")]
    float env_mud_penalty_max;

    // 环境温度热惩罚系数
    // 高温对体力消耗的惩罚系数
    // Optuna 优化范围：0.01 - 0.03
    // 说明：值越大，高温对体力消耗影响越大
    [Attribute(defvalue: "0.02", desc: "Environment temperature heat penalty coeff.\nHeat penalty coefficient on stamina drain.\nOptimized range: 0.01 - 0.03.\nHigher value = greater heat impact on stamina drain.\n环境温度热惩罚系数。\n高温对体力消耗的惩罚系数。\nOptuna 优化范围：0.01 - 0.03。\n值越大，高温对体力消耗影响越大。")]
    float env_temperature_heat_penalty_coeff;

    // 环境温度冷恢复惩罚系数
    // 低温对体力恢复的惩罚系数
    // Optuna 优化范围：0.03 - 0.08
    // 说明：值越大，低温对体力恢复影响越大
    [Attribute(defvalue: "0.05", desc: "Environment temperature cold recovery penalty coeff.\nCold penalty coefficient on stamina recovery.\nOptimized range: 0.03 - 0.08.\nHigher value = greater cold impact on stamina recovery.\n环境温度冷恢复惩罚系数。\n低温对体力恢复的惩罚系数。\nOptuna 优化范围：0.03 - 0.08。\n值越大，低温对体力恢复影响越大。")]
    float env_temperature_cold_recovery_penalty_coeff;

    // 环境地表湿度趴下惩罚
    // 湿地趴下时的恢复惩罚
    // Optuna 优化范围：0.1 - 0.2
    // 说明：值越大，湿地趴下时恢复越慢
    [Attribute(defvalue: "0.15", desc: "Environment surface wetness prone penalty.\nRecovery penalty when prone on wet surface.\nOptimized range: 0.1 - 0.2.\nHigher value = slower recovery when prone on wet surface.\n环境地表湿度趴下惩罚。\n湿地趴下时的恢复惩罚。\nOptuna 优化范围：0.1 - 0.2。\n值越大，湿地趴下时恢复越慢。")]
    float env_surface_wetness_prone_penalty;
}

// ==================== 主配置类 ====================
// 这个类是 RSS 系统的主配置类，包含所有可配置的参数
// 使用 [BaseContainerProps(configRoot: true)] 属性实现自动序列化到 JSON 文件
// 管理员可以通过修改 JSON 文件来调整游戏体验
[BaseContainerProps(configRoot: true)]
class SCR_RSS_Settings
{
    // ==================== 基础配置 ====================
    [Attribute("3.4.0", desc: "Config version for migration. Do not edit. | 配置版本号，用于迁移，请勿修改")]
    string m_sConfigVersion;

    [Attribute("StandardMilsim", UIWidgets.ComboBox, "Preset: EliteStandard(最拟真) | StandardMilsim(平衡) | TacticalAction(流畅) | Custom(自定义)", "EliteStandard StandardMilsim TacticalAction Custom")]
    string m_sSelectedPreset;

    // ==================== 预设参数包 ====================
    // 每个预设都有自己的参数包，包含 40 个由 Optuna 优化的参数
    // 管理员可以直接修改这些参数，或者选择使用预设
    
    // EliteStandard 预设参数包（精英拟真）
    // 适用于硬核拟真服务器，追求最真实的体力系统
    [Attribute(desc: "EliteStandard preset parameters.\nMost realistic configuration optimized by Optuna for hardcore players.\nEliteStandard 预设参数包。\n最拟真的配置，由 Optuna 为硬核玩家优化。")]
    ref SCR_RSS_Params m_EliteStandard;

    // StandardMilsim 预设参数包（标准军事模拟）
    // 适用于标准军事模拟服务器，追求平衡的游戏体验
    [Attribute(desc: "StandardMilsim preset parameters.\nStandard military simulation, balanced experience.\nStandardMilsim 预设参数包。\n标准军事模拟，平衡体验。")]
    ref SCR_RSS_Params m_StandardMilsim;

    // TacticalAction 预设参数包（战术动作）
    // 适用于动作导向的服务器，追求更流畅的游戏体验
    [Attribute(desc: "TacticalAction preset parameters.\nMore fluid gameplay configuration optimized for action-oriented servers.\nTacticalAction 预设参数包。\n更流畅的游戏体验配置，为动作导向的服务器优化。")]
    ref SCR_RSS_Params m_TacticalAction;

    // Custom 预设参数包（自定义）
    // 管理员可以手动调整所有参数
    [Attribute(desc: "Custom preset parameters.\nManually adjust all parameters to your liking.\nCustom 预设参数包。\n手动调整所有参数以适应您的需求。")]
    ref SCR_RSS_Params m_Custom;

    // ==================== 初始化预设默认值 ====================
    // 这个方法初始化四个预设的默认值
    // 这些值是由 Optuna 贝叶斯优化器在 13 个最严苛工况下跑出的最优解
    // EliteStandard：精英拟真模式（最严格）
    // StandardMilsim：标准军事模拟模式（平衡）
    // TacticalAction：战术动作模式（更流畅）
    // Custom：自定义模式（合理的默认备份值）
    
    void InitPresets(bool forceRefreshSystemPresets = false)
    {
        // 1. 确保所有对象都已实例化
        bool initElite = !m_EliteStandard;
        bool initStandard = !m_StandardMilsim;
        bool initTactical = !m_TacticalAction;

        if (initElite) m_EliteStandard = new SCR_RSS_Params();
        if (initStandard) m_StandardMilsim = new SCR_RSS_Params();
        if (initTactical) m_TacticalAction = new SCR_RSS_Params();
        
        // 2. 处理 Custom 预设：仅在不存在时初始化默认值
        bool initCustom = !m_Custom;
        if (initCustom)
        {
            m_Custom = new SCR_RSS_Params();
            InitCustomDefaults(true);
        }

        // 3. 处理系统预设 (Elite/Standard/Tactical)
        // 如果外部要求强制刷新（通常是因为当前没选 Custom），则覆盖为代码里的最新值
        if (forceRefreshSystemPresets)
        {
            InitEliteStandardDefaults(true);
            InitStandardMilsimDefaults(true);
            InitTacticalActionDefaults(true);
            Print("[RSS_Settings] System presets refreshed to latest mod defaults.");
        }
        else
        {
            // 如果不强制刷新（处于 Custom 模式），则仅初始化缺失的对象
            // 注意：这里传入 false 或根据对象是否为 null 来决定，保持向后兼容
            InitEliteStandardDefaults(initElite);
            InitStandardMilsimDefaults(initStandard);
            InitTacticalActionDefaults(initTactical);
        }
    }
    
            // 初始化 EliteStandard 预设默认值
protected void InitEliteStandardDefaults(bool shouldInit)
{
	if (!shouldInit)
		return;

	// EliteStandard preset parameters (optimized by NSGA-II Super Pipeline)
	m_EliteStandard.energy_to_stamina_coeff = 9.0833624163837614e-07;
	m_EliteStandard.base_recovery_rate = 1.0066439495265973e-04;
	m_EliteStandard.standing_recovery_multiplier = 1.792429201381238;
	m_EliteStandard.prone_recovery_multiplier = 2.645354815755791;
	m_EliteStandard.load_recovery_penalty_coeff = 2.8393456440500719e-05;
	m_EliteStandard.load_recovery_penalty_exponent = 1.3931952010279223;
	m_EliteStandard.encumbrance_speed_penalty_coeff = 0.17087654007935654;
	m_EliteStandard.encumbrance_speed_penalty_exponent = 2.3220285084946406;
	m_EliteStandard.encumbrance_speed_penalty_max = 0.44315069897441817;
	m_EliteStandard.encumbrance_stamina_drain_coeff = 1.4533963509246381;
	m_EliteStandard.sprint_stamina_drain_multiplier = 3.2093208284726096;
	m_EliteStandard.fatigue_accumulation_coeff = 0.006747941148235028;
	m_EliteStandard.fatigue_max_factor = 2.6709475761383357;
	m_EliteStandard.aerobic_efficiency_factor = 0.9667545561028223;
	m_EliteStandard.anaerobic_efficiency_factor = 1.480888714279038;
	m_EliteStandard.recovery_nonlinear_coeff = 0.15848445964137833;
	m_EliteStandard.fast_recovery_multiplier = 1.0962756397874924;
	m_EliteStandard.medium_recovery_multiplier = 1.000388469824956;
	m_EliteStandard.slow_recovery_multiplier = 0.57959784126664;
	m_EliteStandard.marginal_decay_threshold = 0.8428848328142484;
	m_EliteStandard.marginal_decay_coeff = 1.125900528252193;
	m_EliteStandard.min_recovery_stamina_threshold = 0.16025159640664743;
	m_EliteStandard.min_recovery_rest_time_seconds = 3.055417856827071;
	m_EliteStandard.sprint_speed_boost = 0.3156223388913739;
	m_EliteStandard.posture_crouch_multiplier = 1.5692807702155411;
	m_EliteStandard.posture_prone_multiplier = 3.8859722663684426;
	m_EliteStandard.jump_stamina_base_cost = 0.04155012639746442;
	m_EliteStandard.vault_stamina_start_cost = 0.02197209381909637;
	m_EliteStandard.climb_stamina_tick_cost = 0.01085730654293999;
	m_EliteStandard.jump_consecutive_penalty = 0.4923432287626238;
	m_EliteStandard.slope_uphill_coeff = 0.09683978084386305;
	m_EliteStandard.slope_downhill_coeff = 0.033891908545065706;
	m_EliteStandard.swimming_base_power = 22.289810616558164;
	m_EliteStandard.swimming_encumbrance_threshold = 28.61690903787345;
	m_EliteStandard.swimming_static_drain_multiplier = 2.7740715893421912;
	m_EliteStandard.swimming_dynamic_power_efficiency = 2.3070709038667028;
	m_EliteStandard.swimming_energy_to_stamina_coeff = 4.3904812059129845e-05;
	m_EliteStandard.env_heat_stress_max_multiplier = 1.3381367808979552;
	m_EliteStandard.env_rain_weight_max = 7.34244181707937;
	m_EliteStandard.env_wind_resistance_coeff = 0.06914101873556563;
	m_EliteStandard.env_mud_penalty_max = 0.47130744474958536;
	m_EliteStandard.env_temperature_heat_penalty_coeff = 0.0220116986159026;
	m_EliteStandard.env_temperature_cold_recovery_penalty_coeff = 0.0545411352323078;
}








    
            // 初始化 StandardMilsim 预设默认值
protected void InitStandardMilsimDefaults(bool shouldInit)
{
	if (!shouldInit)
		return;

	// StandardMilsim preset parameters (optimized by NSGA-II Super Pipeline)
	m_StandardMilsim.energy_to_stamina_coeff = 7.4986127799100011e-07;
	m_StandardMilsim.base_recovery_rate = 1.1598967172256293e-04;
	m_StandardMilsim.standing_recovery_multiplier = 1.6967687011144155;
	m_StandardMilsim.prone_recovery_multiplier = 2.4909663756349976;
	m_StandardMilsim.load_recovery_penalty_coeff = 6.0893306544907759e-06;
	m_StandardMilsim.load_recovery_penalty_exponent = 1.183457884758865;
	m_StandardMilsim.encumbrance_speed_penalty_coeff = 0.1910084407641044;
	m_StandardMilsim.encumbrance_speed_penalty_exponent = 1.0439730440786472;
	m_StandardMilsim.encumbrance_speed_penalty_max = 0.8492828922415805;
	m_StandardMilsim.encumbrance_stamina_drain_coeff = 0.8167388224302938;
	m_StandardMilsim.sprint_stamina_drain_multiplier = 3.1571188390612734;
	m_StandardMilsim.fatigue_accumulation_coeff = 0.009617635083763393;
	m_StandardMilsim.fatigue_max_factor = 2.4172422790787986;
	m_StandardMilsim.aerobic_efficiency_factor = 0.9892062813978227;
	m_StandardMilsim.anaerobic_efficiency_factor = 1.2369900876745217;
	m_StandardMilsim.recovery_nonlinear_coeff = 0.1715924896221671;
	m_StandardMilsim.fast_recovery_multiplier = 1.1596641879951302;
	m_StandardMilsim.medium_recovery_multiplier = 0.997234005640637;
	m_StandardMilsim.slow_recovery_multiplier = 0.5208937603244796;
	m_StandardMilsim.marginal_decay_threshold = 0.8484402018242265;
	m_StandardMilsim.marginal_decay_coeff = 1.1009735510003937;
	m_StandardMilsim.min_recovery_stamina_threshold = 0.19224457900308792;
	m_StandardMilsim.min_recovery_rest_time_seconds = 2.5923487129449234;
	m_StandardMilsim.sprint_speed_boost = 0.2818512831818367;
	m_StandardMilsim.posture_crouch_multiplier = 1.5355572016966572;
	m_StandardMilsim.posture_prone_multiplier = 3.95923426779164;
	m_StandardMilsim.jump_stamina_base_cost = 0.04247111152454178;
	m_StandardMilsim.vault_stamina_start_cost = 0.018293780670342177;
	m_StandardMilsim.climb_stamina_tick_cost = 0.01035118162026549;
	m_StandardMilsim.jump_consecutive_penalty = 0.5694593491070987;
	m_StandardMilsim.slope_uphill_coeff = 0.07765097347000274;
	m_StandardMilsim.slope_downhill_coeff = 0.02035915727500302;
	m_StandardMilsim.swimming_base_power = 15.745104618186401;
	m_StandardMilsim.swimming_encumbrance_threshold = 25.103927213487065;
	m_StandardMilsim.swimming_static_drain_multiplier = 3.4694393756758783;
	m_StandardMilsim.swimming_dynamic_power_efficiency = 2.1932095422188675;
	m_StandardMilsim.swimming_energy_to_stamina_coeff = 4.1418323828771390e-05;
	m_StandardMilsim.env_heat_stress_max_multiplier = 1.2757443503167452;
	m_StandardMilsim.env_rain_weight_max = 9.0852925728674;
	m_StandardMilsim.env_wind_resistance_coeff = 0.0636575903982498;
	m_StandardMilsim.env_mud_penalty_max = 0.4235389172377182;
	m_StandardMilsim.env_temperature_heat_penalty_coeff = 0.021444231696298166;
	m_StandardMilsim.env_temperature_cold_recovery_penalty_coeff = 0.04056674900766769;
}








    
            // 初始化 TacticalAction 预设默认值
protected void InitTacticalActionDefaults(bool shouldInit)
{
	if (!shouldInit)
		return;

	// TacticalAction preset parameters (optimized by NSGA-II Super Pipeline)
	m_TacticalAction.energy_to_stamina_coeff = 7.9414002553712188e-07;
	m_TacticalAction.base_recovery_rate = 6.4876221463169384e-05;
	m_TacticalAction.standing_recovery_multiplier = 1.6018186002796875;
	m_TacticalAction.prone_recovery_multiplier = 2.7748298653599135;
	m_TacticalAction.load_recovery_penalty_coeff = 5.9696582771897269e-06;
	m_TacticalAction.load_recovery_penalty_exponent = 1.075583613854067;
	m_TacticalAction.encumbrance_speed_penalty_coeff = 0.21417078650146792;
	m_TacticalAction.encumbrance_speed_penalty_exponent = 1.5945815806146673;
	m_TacticalAction.encumbrance_speed_penalty_max = 0.4506368423453615;
	m_TacticalAction.encumbrance_stamina_drain_coeff = 1.5188535045804579;
	m_TacticalAction.sprint_stamina_drain_multiplier = 3.123648778290266;
	m_TacticalAction.fatigue_accumulation_coeff = 0.015981032731020462;
	m_TacticalAction.fatigue_max_factor = 1.901103041786312;
	m_TacticalAction.aerobic_efficiency_factor = 0.853022136842895;
	m_TacticalAction.anaerobic_efficiency_factor = 1.4860061477817887;
	m_TacticalAction.recovery_nonlinear_coeff = 0.2001786091382205;
	m_TacticalAction.fast_recovery_multiplier = 1.19729836554567;
	m_TacticalAction.medium_recovery_multiplier = 1.0266574674696773;
	m_TacticalAction.slow_recovery_multiplier = 0.7028078864157643;
	m_TacticalAction.marginal_decay_threshold = 0.7625814788427296;
	m_TacticalAction.marginal_decay_coeff = 1.0771096253764065;
	m_TacticalAction.min_recovery_stamina_threshold = 0.20976682827027032;
	m_TacticalAction.min_recovery_rest_time_seconds = 4.4643825344467665;
	m_TacticalAction.sprint_speed_boost = 0.3462713610246377;
	m_TacticalAction.posture_crouch_multiplier = 1.3059058044037577;
	m_TacticalAction.posture_prone_multiplier = 2.5632086414161237;
	m_TacticalAction.jump_stamina_base_cost = 0.04389472517566159;
	m_TacticalAction.vault_stamina_start_cost = 0.022365352283347927;
	m_TacticalAction.climb_stamina_tick_cost = 0.011531975299319092;
	m_TacticalAction.jump_consecutive_penalty = 0.44052652682706867;
	m_TacticalAction.slope_uphill_coeff = 0.08350343484437608;
	m_TacticalAction.slope_downhill_coeff = 0.034022791888349474;
	m_TacticalAction.swimming_base_power = 21.801118854469728;
	m_TacticalAction.swimming_encumbrance_threshold = 24.08151690866698;
	m_TacticalAction.swimming_static_drain_multiplier = 2.5153948916913085;
	m_TacticalAction.swimming_dynamic_power_efficiency = 2.082926056804263;
	m_TacticalAction.swimming_energy_to_stamina_coeff = 4.5062030783592177e-05;
	m_TacticalAction.env_heat_stress_max_multiplier = 1.3801016984571561;
	m_TacticalAction.env_rain_weight_max = 9.830324060335268;
	m_TacticalAction.env_wind_resistance_coeff = 0.04596141270569739;
	m_TacticalAction.env_mud_penalty_max = 0.46796032133288534;
	m_TacticalAction.env_temperature_heat_penalty_coeff = 0.01688540605749251;
	m_TacticalAction.env_temperature_cold_recovery_penalty_coeff = 0.05344920996050058;
}








    
    // 初始化 Custom 预设默认值
    protected void InitCustomDefaults(bool shouldInit)
    {
        if (!shouldInit)
            return;
        
        // Custom 预设：自定义模式（合理的默认备份值）
        // 管理员可以手动调整所有参数
        m_Custom.energy_to_stamina_coeff = 0.000035;
        m_Custom.base_recovery_rate = 0.0003;
        m_Custom.standing_recovery_multiplier = 2.0;
        m_Custom.prone_recovery_multiplier = 2.2;
        m_Custom.load_recovery_penalty_coeff = 0.0004;
        m_Custom.load_recovery_penalty_exponent = 2.0;
        m_Custom.encumbrance_speed_penalty_coeff = 0.20;
        m_Custom.encumbrance_speed_penalty_exponent = 1.5;
        m_Custom.encumbrance_speed_penalty_max = 0.75;
        m_Custom.encumbrance_stamina_drain_coeff = 1.5;
        m_Custom.sprint_stamina_drain_multiplier = 3.5;
        m_Custom.fatigue_accumulation_coeff = 0.015;
        m_Custom.fatigue_max_factor = 2.0;
        m_Custom.aerobic_efficiency_factor = 1.0;
        m_Custom.anaerobic_efficiency_factor = 1.3;
        m_Custom.recovery_nonlinear_coeff = 0.5;
        m_Custom.fast_recovery_multiplier = 3.0;
        m_Custom.medium_recovery_multiplier = 1.8;
        m_Custom.slow_recovery_multiplier = 0.6;
        m_Custom.marginal_decay_threshold = 0.85;
        m_Custom.marginal_decay_coeff = 1.0;
        m_Custom.min_recovery_stamina_threshold = 0.15;
        m_Custom.min_recovery_rest_time_seconds = 3.0;
        m_Custom.sprint_speed_boost = 0.3;
        m_Custom.posture_crouch_multiplier = 2.0;
        m_Custom.posture_prone_multiplier = 2.5;
        m_Custom.jump_stamina_base_cost = 0.035;
        m_Custom.vault_stamina_start_cost = 0.02;
        m_Custom.climb_stamina_tick_cost = 0.01;
        m_Custom.jump_consecutive_penalty = 0.5;
        m_Custom.slope_uphill_coeff = 0.08;
        m_Custom.slope_downhill_coeff = 0.03;
        m_Custom.swimming_base_power = 25.0;
        m_Custom.swimming_encumbrance_threshold = 25.0;
        m_Custom.swimming_static_drain_multiplier = 3.5;
        m_Custom.swimming_dynamic_power_efficiency = 2.2;
        m_Custom.swimming_energy_to_stamina_coeff = 5e-05;
        m_Custom.env_heat_stress_max_multiplier = 1.3;
        m_Custom.env_rain_weight_max = 7.0;
        m_Custom.env_wind_resistance_coeff = 0.05;
        m_Custom.env_mud_penalty_max = 0.45;
        m_Custom.env_temperature_heat_penalty_coeff = 0.02;
        m_Custom.env_temperature_cold_recovery_penalty_coeff = 0.05;

        // Weather/temperature model defaults (top-level settings)
        m_fTempUpdateInterval = 5.0;
        m_fTemperatureMixingHeight = 1000.0;
        m_fAlbedo = 0.2;
        m_fAerosolOpticalDepth = 0.14;
        m_fSurfaceEmissivity = 0.98;
        m_fCloudBlockingCoeff = 0.7;
        m_fLECoef = 200.0;
        m_bUseEngineTemperature = false;
        m_bUseEngineTimezone = true;
        m_fLongitude = 0.0;
        m_fTimeZoneOffsetHours = 0.0;
    }

    // ==================== 调试配置 ====================
    [Attribute("false", UIWidgets.CheckBox, "Debug: Enable detailed RSS logs in console. Workbench auto-on. | 调试：控制台输出详细日志，工作台模式自动开启")]
    bool m_bDebugLogEnabled;
    
    [Attribute("5000", UIWidgets.EditBox, "Debug log interval (ms). Range: 1000-60000. | 调试日志间隔（毫秒），范围 1-60 秒")]
    int m_iDebugUpdateInterval;
    
    [Attribute("false", UIWidgets.CheckBox, "Verbose: Log all calculation details. | 详细模式：输出完整计算过程")]
    bool m_bVerboseLogging;
    
    [Attribute("false", UIWidgets.CheckBox, "Log to file (RSS_Log.txt). | 将日志写入文件")]
    bool m_bLogToFile;
    
    // ==================== HUD 显示配置 ====================
    [Attribute("false", UIWidgets.CheckBox, "HUD: Show stamina/speed/weight in top-right. Default OFF. | 屏幕右上角显示体力/速度/负重等，默认关闭")]
    bool m_bHintDisplayEnabled;
    
    [Attribute("5000", UIWidgets.EditBox, "[Deprecated] HUD is real-time now. Kept for compatibility. | [已弃用] HUD 已实时更新，保留兼容")]
    int m_iHintUpdateInterval;
    
    [Attribute("2.0", UIWidgets.EditBox, "[Deprecated] HUD is always visible. Kept for compatibility. | [已弃用] HUD 常驻显示，保留兼容")]
    float m_fHintDuration;
    
    // ==================== Custom 预设：体力/移动 ====================
    // 以下配置仅在选择 Custom 预设时生效
    
    [Attribute("1.0", UIWidgets.EditBox, "[Custom] Stamina drain multiplier. Range 0.1-5.0. Higher = harder. | 体力消耗倍率，值越大越难")]
    float m_fStaminaDrainMultiplier;
    
    [Attribute("1.0", UIWidgets.EditBox, "[Custom] Stamina recovery multiplier. Range 0.1-5.0. Higher = easier. | 体力恢复倍率，值越大越易")]
    float m_fStaminaRecoveryMultiplier;
    
    [Attribute("1.0", UIWidgets.EditBox, "[Custom] Encumbrance speed penalty. Range 0.1-5.0. | 负重速度惩罚倍率")]
    float m_fEncumbranceSpeedPenaltyMultiplier;
    
    [Attribute("1.3", UIWidgets.EditBox, "[Custom] Sprint speed multiplier. Range 1.0-2.0. | Sprint 速度倍率")]
    float m_fSprintSpeedMultiplier;
    
    [Attribute("3.5", UIWidgets.EditBox, "[Custom] Sprint stamina drain multiplier. Range 1.0-10.0. | Sprint 消耗倍率")]
    float m_fSprintStaminaDrainMultiplier;
    
    // ==================== Custom 预设：环境因子 ====================
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Heat stress (10:00-18:00). | 热应激系统")]
    bool m_bEnableHeatStress;
    
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Rain weight (wet clothes). | 降雨湿重")]
    bool m_bEnableRainWeight;
    
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Wind resistance. | 风阻系统")]
    bool m_bEnableWindResistance;
    
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Mud penalty (wet terrain). | 泥泞惩罚")]
    bool m_bEnableMudPenalty;

    // ==================== Weather / Temperature Model Settings ====================
    [Attribute("5.0", UIWidgets.EditBox, "Temperature update interval (s). How often the physics model steps. | 温度步进间隔（秒），物理模型步进频率")]
    float m_fTempUpdateInterval;

    [Attribute("1000.0", UIWidgets.EditBox, "Temperature mixing height (m). Effective air mixing depth for near-surface layer. | 温度混合层高度（米），影响温度响应速率")]
    float m_fTemperatureMixingHeight;

    [Attribute("0.2", UIWidgets.EditBox, "Surface albedo (0-1). Surface reflectance used in SW absorption. | 地表反照率（0-1），短波反射率")]
    float m_fAlbedo;

    [Attribute("0.14", UIWidgets.EditBox, "Aerosol optical depth (AOD). Used for clear-sky transmittance. | 气溶胶光学厚度，用于透过率估计")]
    float m_fAerosolOpticalDepth;

    [Attribute("0.98", UIWidgets.EditBox, "Surface emissivity (0-1). Used for LW_up. | 地表发射率（0-1），用于长波发射计算")]
    float m_fSurfaceEmissivity;

    [Attribute("0.7", UIWidgets.EditBox, "Cloud blocking coefficient. Multiplies inferred cloud factor to estimate SW blocking. | 云遮挡系数，乘以 cloud factor 估计短波遮挡")]
    float m_fCloudBlockingCoeff;

    [Attribute("200.0", UIWidgets.EditBox, "Latent heat coefficient (W/m2 per wetness). Multiplied by surface wetness to approximate LE. | 潜热系数（W/m2 每单位地表湿度）")]
    float m_fLECoef;

    [Attribute("false", UIWidgets.CheckBox, "Use engine-provided air temperature (min/max) as model boundary. If false, compute temperature purely from module physics. | 是否使用引擎提供的气温作为模型边界")]
    bool m_bUseEngineTemperature;

    [Attribute("true", UIWidgets.CheckBox, "Use engine time zone info if available. If false, use longitude/timezone overrides below. | 优先使用引擎时区信息（若存在），否则使用下方经度/时区覆盖")]
    bool m_bUseEngineTimezone;

    [Attribute("0.0", UIWidgets.EditBox, "Longitude override (deg). Used for solar time correction if engine longitude is not available. | 经度覆盖（度），用于太阳时校正")]
    float m_fLongitude;

    [Attribute("0.0", UIWidgets.EditBox, "Time zone offset override (hours). Example: +8 for CST. Used when not using engine timezone. | 时区偏移覆盖（小时），例如 +8 表示东八区")]
    float m_fTimeZoneOffsetHours;
    
    // ==================== Custom 预设：高级系统 ====================
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Fatigue accumulation. | 疲劳积累")]
    bool m_bEnableFatigueSystem;
    
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Metabolic adaptation. | 代谢适应")]
    bool m_bEnableMetabolicAdaptation;
    
    [Attribute("true", UIWidgets.CheckBox, "[Custom] Indoor detection. | 室内检测")]
    bool m_bEnableIndoorDetection;
    
    // ==================== 性能配置 ====================
    [Attribute("5000", UIWidgets.EditBox, "Terrain update interval (ms). Lower = more accurate, higher CPU. | 地形检测间隔")]
    int m_iTerrainUpdateInterval;
    
    [Attribute("5000", UIWidgets.EditBox, "Environment update interval (ms). | 环境因子检测间隔")]
    int m_iEnvironmentUpdateInterval;

    // ==================== 获取激活的预设参数 ====================
    
    SCR_RSS_Params GetActiveParams()
    {
        if (!m_sSelectedPreset)
            m_sSelectedPreset = "StandardMilsim";
        
        if (m_sSelectedPreset == "EliteStandard")
        {
            if (!m_EliteStandard)
                m_EliteStandard = new SCR_RSS_Params();
            return m_EliteStandard;
        }
        else if (m_sSelectedPreset == "StandardMilsim")
        {
            if (!m_StandardMilsim)
                m_StandardMilsim = new SCR_RSS_Params();
            return m_StandardMilsim;
        }
        else if (m_sSelectedPreset == "TacticalAction")
        {
            if (!m_TacticalAction)
                m_TacticalAction = new SCR_RSS_Params();
            return m_TacticalAction;
        }
        else
        {
            if (!m_Custom)
                m_Custom = new SCR_RSS_Params();
            return m_Custom;
        }
    }
}
