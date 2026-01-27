// RSS配置类 - 使用Reforger官方标准
// 利用 [BaseContainerProps] 和 [Attribute] 属性实现自动序列化
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_Settings.c

// ==================== 预设参数包类 ====================
// 这个类包含所有由 Optuna 贝叶斯优化器优化的核心参数
// 每个预设（EliteStandard, StandardMilsim, TacticalAction, Custom）都有自己的参数包
// 管理员可以通过修改 JSON 文件中的这些参数来调整游戏体验
[BaseContainerProps()]
class SCR_RSS_Params
{
    // 能量到体力转换系数
    // 将 Pandolf 能量消耗模型（W/kg）转换为游戏体力消耗率（%/s）
    // Optuna 优化范围：0.000015 - 0.000050
    // EliteStandard: 0.000025（精英拟真）
    // TacticalAction: 0.000020（战术动作）
    // 说明：值越小，体力消耗越慢，玩家可以运动更长时间
    [Attribute(defvalue: "0.000035", desc: "Energy to stamina conversion coefficient.\nConverts Pandolf energy expenditure (W/kg) to game stamina drain rate (%/s).\nOptimized range: 0.000015 - 0.000050.\nLower value = slower stamina drain, longer endurance.\n能量到体力转换系数。\n将 Pandolf 能量消耗模型（W/kg）转换为游戏体力消耗率（%/s）。\nOptuna 优化范围：0.000015 - 0.000050。\n值越小，体力消耗越慢，耐力越持久。")]
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
    [Attribute(defvalue: "0.20", desc: "Encumbrance speed penalty coefficient.\nSpeed penalty coefficient for load (based on body mass percentage).\nFormula: speed_penalty = coeff * (load_weight / body_weight).\nOptimized range: 0.15 - 0.35.\nHigher value = slower movement under load.\n负重速度惩罚系数。\n负重对移动速度的惩罚系数（基于体重百分比）。\n公式：speed_penalty = coeff * (load_weight / body_weight)。\nOptuna 优化范围：0.15 - 0.35。\n值越大，负重时移动速度越慢。")]
    float encumbrance_speed_penalty_coeff;

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
    [Attribute(defvalue: "3.0", desc: "Sprint stamina drain multiplier.\nStamina drain multiplier when sprinting (relative to running).\nOptimized range: 2.0 - 4.0.\nHigher value = faster stamina drain when sprinting.\nSprint体力消耗倍数。\nSprint 时的体力消耗倍数（相对于 Run）。\nOptuna 优化范围：2.0 - 4.0。\n值越大，Sprint 时体力消耗越快。")]
    float sprint_stamina_drain_multiplier;

    // 疲劳累积系数
    // 长时间运动时的疲劳累积速度（基于运动持续时间）
    // 公式：fatigue_factor = 1.0 + coeff * max(0, exercise_duration - 5.0)
    // Optuna 优化范围：0.010 - 0.030
    // EliteStandard: 0.02（精英拟真）
    // TacticalAction: 0.023（战术动作）
    // 说明：值越大，疲劳累积越快，长期运动后恢复越慢
    [Attribute(defvalue: "0.015", desc: "Fatigue accumulation coefficient.\nFatigue accumulation speed during prolonged exercise (based on exercise duration).\nFormula: fatigue_factor = 1.0 + coeff * max(0, exercise_duration - 5.0).\nOptimized range: 0.010 - 0.030.\nHigher value = faster fatigue accumulation, slower recovery after prolonged exercise.\n疲劳累积系数。\n长时间运动时的疲劳累积速度（基于运动持续时间）。\n公式：fatigue_factor = 1.0 + coeff * max(0, exercise_duration - 5.0)。\nOptuna 优化范围：0.010 - 0.030。\n值越大，疲劳累积越快，长期运动后恢复越慢。")]
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

    // 蹲姿消耗倍数
    // 蹲下时的体力消耗倍数（相对于站姿）
    // Optuna 优化范围：1.8 - 2.3
    // 说明：值越大，蹲下时体力消耗越快
    [Attribute(defvalue: "2.0", desc: "Posture crouch multiplier.\nStamina drain multiplier when crouching (relative to standing).\nOptimized range: 1.8 - 2.3.\nHigher value = faster stamina drain when crouching.\n蹲姿消耗倍数。\n蹲下时的体力消耗倍数（相对于站姿）。\nOptuna 优化范围：1.8 - 2.3。\n值越大，蹲下时体力消耗越快。")]
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
    [Attribute(defvalue: "0.08", desc: "Slope uphill coefficient.\nStamina drain coefficient when going uphill.\nOptimized range: 0.05 - 0.1.\nHigher value = faster stamina drain when going uphill.\n上坡系数。\n上坡时的体力消耗系数。\nOptuna 优化范围：0.05 - 0.1。\n值越大，上坡消耗体力越快。")]
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
    [Attribute(defvalue: "25.0", desc: "Swimming base power.\nBase power consumption when swimming (W).\nOptimized range: 15 - 30.\nHigher value = higher base swimming power consumption.\n游泳基础功率。\n游泳时的基础功率消耗（W）。\nOptuna 优化范围：15 - 30。\n值越大，游泳基础消耗越大。")]
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
    [Attribute(defvalue: "1.3", desc: "Environment heat stress max multiplier.\nMaximum stamina drain multiplier in hot environment.\nOptimized range: 1.1 - 1.5.\nHigher value = faster stamina drain in hot environment.\n环境热应激最大倍数。\n高温环境下的最大体力消耗倍数。\nOptuna 优化范围：1.1 - 1.5。\n值越大，高温环境下体力消耗越快。")]
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
}

// ==================== 主配置类 ====================
// 这个类是 RSS 系统的主配置类，包含所有可配置的参数
// 使用 [BaseContainerProps(configRoot: true)] 属性实现自动序列化到 JSON 文件
// 管理员可以通过修改 JSON 文件来调整游戏体验
[BaseContainerProps(configRoot: true)]
class SCR_RSS_Settings
{
    // ==================== 配置版本号 ====================
    // 用于检测配置文件版本，当模组更新时自动迁移旧配置
    // 格式：主版本.次版本.修订版本（如 "3.4.0"）
    // 当模组版本与配置版本不匹配时，会自动添加新字段的默认值
    [Attribute("3.4.0", desc: "Config file version.\nUsed to detect config version and auto-migrate old configs.\nFormat: major.minor.patch (e.g. '3.4.0').\nWhen mod version differs from config version, new fields will be added with default values.\n配置文件版本号。\n用于检测配置版本并自动迁移旧配置。\n格式：主版本.次版本.修订版本（如 '3.4.0'）。\n当模组版本与配置版本不匹配时，会自动添加新字段的默认值。")]
    string m_sConfigVersion;

    // ==================== 预设选择 ====================
    // 预设选择器：管理员可以选择使用哪个预设配置
    // EliteStandard：精英拟真模式（Optuna 优化出的最严格模式）
    // StandardMilsim：标准军事模拟模式（平衡体验）
    // TacticalAction：战术动作模式（更流畅的游戏体验）
    // Custom：自定义模式（管理员可以手动调整所有参数）
    // 工作台模式：默认使用 StandardMilsim
    [Attribute("StandardMilsim", UIWidgets.ComboBox, "Select preset configuration.\nEliteStandard: Most realistic, optimized by Optuna for hardcore players.\nStandardMilsim: Standard military simulation, balanced experience.\nTacticalAction: More fluid gameplay, optimized for action-oriented servers.\nCustom: Manually adjust all parameters.\n选择预设配置方案。\nEliteStandard：最拟真，由 Optuna 为硬核玩家优化。\nStandardMilsim：标准军事模拟，平衡体验。\nTacticalAction：更流畅的游戏体验，为动作导向的服务器优化。\nCustom：手动调整所有参数。", "EliteStandard StandardMilsim TacticalAction Custom")]
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
        if (!m_EliteStandard) m_EliteStandard = new SCR_RSS_Params();
        if (!m_StandardMilsim) m_StandardMilsim = new SCR_RSS_Params();
        if (!m_TacticalAction) m_TacticalAction = new SCR_RSS_Params();
        
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
            InitEliteStandardDefaults(!m_EliteStandard);
            InitStandardMilsimDefaults(!m_StandardMilsim);
            InitTacticalActionDefaults(!m_TacticalAction);
        }
    }
    
            // 初始化 EliteStandard 预设默认值
protected void InitEliteStandardDefaults(bool shouldInit)
{
	if (!shouldInit)
		return;

	// EliteStandard preset parameters (optimized by NSGA-II Super Pipeline)
	m_EliteStandard.energy_to_stamina_coeff = 5.9328396632567927e-05;
	m_EliteStandard.base_recovery_rate = 3.9478268773972123e-04;
	m_EliteStandard.standing_recovery_multiplier = 1.4423580201308843;
	m_EliteStandard.prone_recovery_multiplier = 2.3652773496127115;
	m_EliteStandard.load_recovery_penalty_coeff = 1.2130327472246061e-04;
	m_EliteStandard.load_recovery_penalty_exponent = 1.3190326481643582;
	m_EliteStandard.encumbrance_speed_penalty_coeff = 0.10980867601716877;
	m_EliteStandard.encumbrance_stamina_drain_coeff = 1.240045012706199;
	m_EliteStandard.sprint_stamina_drain_multiplier = 3.227905740884214;
	m_EliteStandard.fatigue_accumulation_coeff = 0.016787416372858823;
	m_EliteStandard.fatigue_max_factor = 2.0313492744469612;
	m_EliteStandard.aerobic_efficiency_factor = 0.8938134956909851;
	m_EliteStandard.anaerobic_efficiency_factor = 1.4503157994406068;
	m_EliteStandard.recovery_nonlinear_coeff = 0.5113298438447526;
	m_EliteStandard.fast_recovery_multiplier = 3.262607369968264;
	m_EliteStandard.medium_recovery_multiplier = 1.9249258381165226;
	m_EliteStandard.slow_recovery_multiplier = 0.5170831501893385;
	m_EliteStandard.marginal_decay_threshold = 0.8744835806982311;
	m_EliteStandard.marginal_decay_coeff = 1.1290443522034068;
	m_EliteStandard.min_recovery_stamina_threshold = 0.19404491538547047;
	m_EliteStandard.min_recovery_rest_time_seconds = 1.0151265917420047;
	m_EliteStandard.sprint_speed_boost = 0.33893549337669693;
	m_EliteStandard.posture_crouch_multiplier = 0.7628748214716501;
	m_EliteStandard.posture_prone_multiplier = 0.6200321792044691;
	m_EliteStandard.jump_stamina_base_cost = 0.027644037700059132;
	m_EliteStandard.vault_stamina_start_cost = 0.024809469126089414;
	m_EliteStandard.climb_stamina_tick_cost = 0.01175086924431621;
	m_EliteStandard.jump_consecutive_penalty = 0.5807833133205341;
	m_EliteStandard.slope_uphill_coeff = 0.0664774364427466;
	m_EliteStandard.slope_downhill_coeff = 0.023732078325808908;
	m_EliteStandard.swimming_base_power = 24.99330755358171;
	m_EliteStandard.swimming_encumbrance_threshold = 20.28932515154274;
	m_EliteStandard.swimming_static_drain_multiplier = 2.9063637536701212;
	m_EliteStandard.swimming_dynamic_power_efficiency = 1.7387941275313652;
	m_EliteStandard.swimming_energy_to_stamina_coeff = 4.1856366583109239e-05;
	m_EliteStandard.env_heat_stress_max_multiplier = 1.2857712708757252;
	m_EliteStandard.env_rain_weight_max = 9.980330136249743;
	m_EliteStandard.env_wind_resistance_coeff = 0.061276660111670135;
	m_EliteStandard.env_mud_penalty_max = 0.464589268884568;
	m_EliteStandard.env_temperature_heat_penalty_coeff = 0.024089107048359214;
	m_EliteStandard.env_temperature_cold_recovery_penalty_coeff = 0.05796554565761408;
}








    
            // 初始化 StandardMilsim 预设默认值
protected void InitStandardMilsimDefaults(bool shouldInit)
{
	if (!shouldInit)
		return;

	// StandardMilsim preset parameters (optimized by NSGA-II Super Pipeline)
	m_StandardMilsim.energy_to_stamina_coeff = 3.0642147332118343e-05;
	m_StandardMilsim.base_recovery_rate = 1.9936373374823975e-04;
	m_StandardMilsim.standing_recovery_multiplier = 1.2724034116269574;
	m_StandardMilsim.prone_recovery_multiplier = 2.391326412175987;
	m_StandardMilsim.load_recovery_penalty_coeff = 3.7821186090925278e-04;
	m_StandardMilsim.load_recovery_penalty_exponent = 2.5276814094458167;
	m_StandardMilsim.encumbrance_speed_penalty_coeff = 0.10088955403798212;
	m_StandardMilsim.encumbrance_stamina_drain_coeff = 1.6898716414300166;
	m_StandardMilsim.sprint_stamina_drain_multiplier = 2.7253183447857894;
	m_StandardMilsim.fatigue_accumulation_coeff = 0.017708247520076315;
	m_StandardMilsim.fatigue_max_factor = 1.7728257697975012;
	m_StandardMilsim.aerobic_efficiency_factor = 0.9580226448169157;
	m_StandardMilsim.anaerobic_efficiency_factor = 1.1451038332485566;
	m_StandardMilsim.recovery_nonlinear_coeff = 0.4617781375319723;
	m_StandardMilsim.fast_recovery_multiplier = 3.3405797224154985;
	m_StandardMilsim.medium_recovery_multiplier = 2.359916891565974;
	m_StandardMilsim.slow_recovery_multiplier = 0.5497795112222168;
	m_StandardMilsim.marginal_decay_threshold = 0.838317550836646;
	m_StandardMilsim.marginal_decay_coeff = 1.141746779387695;
	m_StandardMilsim.min_recovery_stamina_threshold = 0.18828089202756704;
	m_StandardMilsim.min_recovery_rest_time_seconds = 4.99613436832593;
	m_StandardMilsim.sprint_speed_boost = 0.26717181653170036;
	m_StandardMilsim.posture_crouch_multiplier = 0.8636620302884634;
	m_StandardMilsim.posture_prone_multiplier = 0.5035801490873693;
	m_StandardMilsim.jump_stamina_base_cost = 0.026409506811332646;
	m_StandardMilsim.vault_stamina_start_cost = 0.020111229109444982;
	m_StandardMilsim.climb_stamina_tick_cost = 0.011974645957191547;
	m_StandardMilsim.jump_consecutive_penalty = 0.5042599856965883;
	m_StandardMilsim.slope_uphill_coeff = 0.08265449219468612;
	m_StandardMilsim.slope_downhill_coeff = 0.03737210335384906;
	m_StandardMilsim.swimming_base_power = 15.026116794068825;
	m_StandardMilsim.swimming_encumbrance_threshold = 29.715974469928774;
	m_StandardMilsim.swimming_static_drain_multiplier = 2.8644689712130877;
	m_StandardMilsim.swimming_dynamic_power_efficiency = 1.8437541817387046;
	m_StandardMilsim.swimming_energy_to_stamina_coeff = 4.7149865804272418e-05;
	m_StandardMilsim.env_heat_stress_max_multiplier = 1.5849771106962125;
	m_StandardMilsim.env_rain_weight_max = 9.82371721487525;
	m_StandardMilsim.env_wind_resistance_coeff = 0.062338533128189544;
	m_StandardMilsim.env_mud_penalty_max = 0.48572534033970255;
	m_StandardMilsim.env_temperature_heat_penalty_coeff = 0.015775151991930546;
	m_StandardMilsim.env_temperature_cold_recovery_penalty_coeff = 0.04113140365869699;
}








    
            // 初始化 TacticalAction 预设默认值
protected void InitTacticalActionDefaults(bool shouldInit)
{
	if (!shouldInit)
		return;

	// TacticalAction preset parameters (optimized by NSGA-II Super Pipeline)
	m_TacticalAction.energy_to_stamina_coeff = 3.7545777796634012e-05;
	m_TacticalAction.base_recovery_rate = 3.8969847685976398e-04;
	m_TacticalAction.standing_recovery_multiplier = 1.567322475731821;
	m_TacticalAction.prone_recovery_multiplier = 2.412022336818396;
	m_TacticalAction.load_recovery_penalty_coeff = 1.1697759827567251e-04;
	m_TacticalAction.load_recovery_penalty_exponent = 2.0216604130171225;
	m_TacticalAction.encumbrance_speed_penalty_coeff = 0.15968408569280323;
	m_TacticalAction.encumbrance_stamina_drain_coeff = 1.3316460210670358;
	m_TacticalAction.sprint_stamina_drain_multiplier = 2.831768381799883;
	m_TacticalAction.fatigue_accumulation_coeff = 0.009942016918192005;
	m_TacticalAction.fatigue_max_factor = 1.6142501509061506;
	m_TacticalAction.aerobic_efficiency_factor = 0.9580226448169157;
	m_TacticalAction.anaerobic_efficiency_factor = 1.4593371015297572;
	m_TacticalAction.recovery_nonlinear_coeff = 0.36355256527405394;
	m_TacticalAction.fast_recovery_multiplier = 3.0594946001546113;
	m_TacticalAction.medium_recovery_multiplier = 1.5226049992553392;
	m_TacticalAction.slow_recovery_multiplier = 0.5831779498548113;
	m_TacticalAction.marginal_decay_threshold = 0.838317550836646;
	m_TacticalAction.marginal_decay_coeff = 1.141746779387695;
	m_TacticalAction.min_recovery_stamina_threshold = 0.1748678182691837;
	m_TacticalAction.min_recovery_rest_time_seconds = 4.99613436832593;
	m_TacticalAction.sprint_speed_boost = 0.27634069583982906;
	m_TacticalAction.posture_crouch_multiplier = 0.5563193054244363;
	m_TacticalAction.posture_prone_multiplier = 0.48741175614703;
	m_TacticalAction.jump_stamina_base_cost = 0.03160757382837484;
	m_TacticalAction.vault_stamina_start_cost = 0.020111229109444982;
	m_TacticalAction.climb_stamina_tick_cost = 0.010234346687491186;
	m_TacticalAction.jump_consecutive_penalty = 0.5042599856965883;
	m_TacticalAction.slope_uphill_coeff = 0.08265449219468612;
	m_TacticalAction.slope_downhill_coeff = 0.03737210335384906;
	m_TacticalAction.swimming_base_power = 15.026116794068825;
	m_TacticalAction.swimming_encumbrance_threshold = 29.715974469928774;
	m_TacticalAction.swimming_static_drain_multiplier = 3.4823201396805556;
	m_TacticalAction.swimming_dynamic_power_efficiency = 1.8437541817387046;
	m_TacticalAction.swimming_energy_to_stamina_coeff = 4.3154759173432568e-05;
	m_TacticalAction.env_heat_stress_max_multiplier = 1.5849771106962125;
	m_TacticalAction.env_rain_weight_max = 6.170606331120339;
	m_TacticalAction.env_wind_resistance_coeff = 0.058521808242183226;
	m_TacticalAction.env_mud_penalty_max = 0.34467401528876807;
	m_TacticalAction.env_temperature_heat_penalty_coeff = 0.022692369507509164;
	m_TacticalAction.env_temperature_cold_recovery_penalty_coeff = 0.04113140365869699;
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
        m_Custom.encumbrance_stamina_drain_coeff = 1.5;
        m_Custom.sprint_stamina_drain_multiplier = 3.0;
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
    }

    // ==================== 调试配置 ====================
    // 调试日志配置：控制调试信息的输出
    // 适用于开发和测试阶段，帮助管理员了解系统运行状态
    
    // 启用详细 RSS 调试日志
    // 工作台模式：自动启用
    [Attribute("false", UIWidgets.CheckBox, "Enable detailed RSS debug logs in console.\nAutomatically enabled in Workbench mode.\n启用详细RSS调试日志输出到控制台。\n工作台模式下自动启用。")]
    bool m_bDebugLogEnabled;
    
    // 调试日志刷新间隔（毫秒）
    // 控制调试日志的输出频率
    [Attribute("5000", UIWidgets.EditBox, "Debug log interval in milliseconds.\nControls the frequency of debug log output.\n调试日志刷新间隔（毫秒）。\n控制调试日志的输出频率。")]
    int m_iDebugUpdateInterval;
    
    // 启用详细日志（包含所有计算细节）
    // 输出所有计算过程的详细信息，适用于深度调试
    [Attribute("false", UIWidgets.CheckBox, "Enable verbose logging (includes all calculation details).\nOutputs detailed information of all calculation processes.\n启用详细日志（包含所有计算细节）。\n输出所有计算过程的详细信息。")]
    bool m_bVerboseLogging;
    
    // 启用日志输出到文件（RSS_Log.txt）
    // 将日志输出到文件，便于长期保存和分析
    [Attribute("false", UIWidgets.CheckBox, "Enable logging to file (RSS_Log.txt).\nOutputs logs to file for long-term storage and analysis.\n启用日志输出到文件（RSS_Log.txt）。\n将日志输出到文件，便于长期保存和分析。")]
    bool m_bLogToFile;
    
    // ==================== HUD 显示配置 ====================
    // 在屏幕右上角显示实时体力状态信息（类似游戏原生的 FPS/Ping 显示）
    // 让玩家无需查看控制台也能了解当前体力状态
    
    // 启用屏幕 HUD 显示
    // 在屏幕右上角显示体力、速度、负重、温度、风向等实时状态
    [Attribute("true", UIWidgets.CheckBox, "Enable on-screen HUD display.\nShows real-time stamina status in top-right corner (stamina, speed, weight, temperature, wind, etc.).\n启用屏幕 HUD 显示。\n在屏幕右上角显示实时体力状态（体力、速度、负重、温度、风向等）。")]
    bool m_bHintDisplayEnabled;
    
    // [已弃用] Hint 显示间隔（毫秒）
    // 注意：此配置项已弃用，HUD 现在是实时更新的（每 0.2 秒）
    [Attribute("5000", UIWidgets.EditBox, "[DEPRECATED] Hint display interval in milliseconds.\nNote: This setting is deprecated. HUD now updates in real-time (every 0.2 seconds).\n[已弃用] Hint 显示间隔（毫秒）。\n注意：此配置项已弃用，HUD 现在是实时更新的（每 0.2 秒）。")]
    int m_iHintUpdateInterval;
    
    // [已弃用] Hint 显示时长（秒）
    // 注意：此配置项已弃用，HUD 是常驻显示的
    [Attribute("2.0", UIWidgets.EditBox, "[DEPRECATED] Hint display duration in seconds.\nNote: This setting is deprecated. HUD is now always visible.\n[已弃用] Hint 显示时长（秒）。\n注意：此配置项已弃用，HUD 是常驻显示的。")]
    float m_fHintDuration;
    
    // ==================== 体力系统配置（仅 Custom 预设生效） ====================
    // 全局体力系统配置：控制体力的消耗和恢复
    // 注意：这些配置仅在选择 Custom 预设时生效！
    // 使用 EliteStandard/StandardMilsim/TacticalAction 预设时，这些配置将被忽略
    
    // 全局体力消耗倍率（0.1-5.0）
    // 值越大，体力消耗越快，游戏难度越高
    // 仅 Custom 预设生效
    [Attribute("1.0", UIWidgets.EditBox, "[Custom preset only] Global stamina consumption multiplier (0.1-5.0).\nHigher value = faster stamina drain, higher difficulty.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 全局体力消耗倍率（0.1-5.0）。\n值越大，体力消耗越快，游戏难度越高。\n仅在选择 Custom 预设时生效。")]
    float m_fStaminaDrainMultiplier;
    
    // 全局体力恢复倍率（0.1-5.0）
    // 值越大，体力恢复越快，游戏难度越低
    // 仅 Custom 预设生效
    [Attribute("1.0", UIWidgets.EditBox, "[Custom preset only] Global stamina recovery multiplier (0.1-5.0).\nHigher value = faster stamina recovery, lower difficulty.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 全局体力恢复倍率（0.1-5.0）。\n值越大，体力恢复越快，游戏难度越低。\n仅在选择 Custom 预设时生效。")]
    float m_fStaminaRecoveryMultiplier;
    
    // 负重速度惩罚倍率（0.1-5.0）
    // 值越大，负重时移动速度越慢
    // 仅 Custom 预设生效
    [Attribute("1.0", UIWidgets.EditBox, "[Custom preset only] Encumbrance speed penalty multiplier (0.1-5.0).\nHigher value = slower movement under load.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 负重速度惩罚倍率（0.1-5.0）。\n值越大，负重时移动速度越慢。\n仅在选择 Custom 预设时生效。")]
    float m_fEncumbranceSpeedPenaltyMultiplier;
    
    // ==================== 移动系统配置（仅 Custom 预设生效） ====================
    // 移动系统配置：控制 Sprint 和 Run 的速度与体力消耗
    // 注意：这些配置仅在选择 Custom 预设时生效！
    
    // Sprint 速度倍率（1.0-2.0）
    // 值越大，Sprint 时移动速度越快
    // 仅 Custom 预设生效
    [Attribute("1.3", UIWidgets.EditBox, "[Custom preset only] Sprint speed multiplier (1.0-2.0).\nHigher value = faster sprint speed.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] Sprint速度倍率（1.0-2.0）。\n值越大，Sprint 时移动速度越快。\n仅在选择 Custom 预设时生效。")]
    float m_fSprintSpeedMultiplier;
    
    // Sprint 体力消耗倍率（1.0-10.0）
    // 值越大，Sprint 时体力消耗越快
    // 仅 Custom 预设生效
    [Attribute("3.0", UIWidgets.EditBox, "[Custom preset only] Sprint stamina drain multiplier (1.0-10.0).\nHigher value = faster stamina drain when sprinting.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] Sprint体力消耗倍率（1.0-10.0）。\n值越大，Sprint 时体力消耗越快。\n仅在选择 Custom 预设时生效。")]
    float m_fSprintStaminaDrainMultiplier;
    
    // ==================== 环境因子配置（仅 Custom 预设生效） ====================
    // 环境因子配置：控制各种环境因素对体力系统的影响
    // 注意：这些配置仅在选择 Custom 预设时生效！
    // 使用其他预设时，所有环境因子系统默认启用
    
    // 启用热应激系统（10:00-18:00）
    // 模拟高温环境对体力系统的影响
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable heat stress system (10:00-18:00).\nSimulates the impact of high temperature on stamina system.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用热应激系统（10:00-18:00）。\n模拟高温环境对体力系统的影响。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableHeatStress;
    
    // 启用降雨湿重系统（衣服湿重）
    // 模拟降雨导致衣服湿重增加对体力系统的影响
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable rain weight system (clothes get wet).\nSimulates the impact of rain-soaked clothes on stamina system.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用降雨湿重系统（衣服湿重）。\n模拟降雨导致衣服湿重增加对体力系统的影响。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableRainWeight;
    
    // 启用风阻系统
    // 模拟风速对移动速度和体力消耗的影响
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable wind resistance system.\nSimulates the impact of wind speed on movement speed and stamina drain.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用风阻系统。\n模拟风速对移动速度和体力消耗的影响。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableWindResistance;
    
    // 启用泥泞惩罚系统（湿地形）
    // 模拟湿地形对移动速度和体力消耗的影响
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable mud penalty system (wet terrain).\nSimulates the impact of wet terrain on movement speed and stamina drain.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用泥泞惩罚系统（湿地形）。\n模拟湿地形对移动速度和体力消耗的影响。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableMudPenalty;
    
    // ==================== 高级配置（仅 Custom 预设生效） ====================
    // 高级配置：控制高级系统功能的启用或禁用
    // 注意：这些配置仅在选择 Custom 预设时生效！
    // 使用其他预设时，所有高级系统默认启用
    
    // 启用疲劳积累系统
    // 模拟长时间运动后的疲劳累积效应
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable fatigue accumulation system.\nSimulates fatigue accumulation effect after prolonged exercise.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用疲劳积累系统。\n模拟长时间运动后的疲劳累积效应。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableFatigueSystem;
    
    // 启用代谢适应系统
    // 模拟长期训练后的代谢适应效应
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable metabolic adaptation system.\nSimulates metabolic adaptation effect after long-term training.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用代谢适应系统。\n模拟长期训练后的代谢适应效应。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableMetabolicAdaptation;
    
    // 启用室内检测系统
    // 检测玩家是否在室内，调整环境因子影响
    // 仅 Custom 预设生效
    [Attribute("true", UIWidgets.CheckBox, "[Custom preset only] Enable indoor detection system.\nDetects if player is indoors, adjusts environmental factor impact.\nOnly effective when Custom preset is selected.\n[仅 Custom 预设生效] 启用室内检测系统。\n检测玩家是否在室内，调整环境因子影响。\n仅在选择 Custom 预设时生效。")]
    bool m_bEnableIndoorDetection;
    
    // ==================== 性能配置 ====================
    // 性能配置：控制各种检测系统的更新间隔
    // 较低的值可以提高精度，但会增加 CPU 负载
    
    // 地形检测更新间隔（毫秒）
    // 控制地形检测系统的更新频率
    [Attribute("5000", UIWidgets.EditBox, "Terrain detection update interval in milliseconds.\nLower value = higher accuracy, higher CPU load.\n地形检测更新间隔（毫秒）。\n值越低，精度越高，CPU 负载越高。")]
    int m_iTerrainUpdateInterval;
    
    // 环境因子更新间隔（毫秒）
    // 控制环境因子检测系统的更新频率
    [Attribute("5000", UIWidgets.EditBox, "Environment factor update interval in milliseconds.\nLower value = higher accuracy, higher CPU load.\n环境因子更新间隔（毫秒）。\n值越低，精度越高，CPU 负载越高。")]
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
