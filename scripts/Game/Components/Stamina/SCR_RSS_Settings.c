// RSS配置类 - 使用Reforger官方标准
// 利用 [BaseContainerProps] 和 [Attribute] 属性实现自动序列化
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_Settings.c

// ==================== 预设参数包类 ====================
// 这个类包含所有由 Optuna 贝叶斯优化器优化的核心参数
// 每个预设（EliteStandard, TacticalAction, Custom）都有自己的参数包
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
}

// ==================== 主配置类 ====================
// 这个类是 RSS 系统的主配置类，包含所有可配置的参数
// 使用 [BaseContainerProps(configRoot: true)] 属性实现自动序列化到 JSON 文件
// 管理员可以通过修改 JSON 文件来调整游戏体验
[BaseContainerProps(configRoot: true)]
class SCR_RSS_Settings
{
    // ==================== 预设选择 ====================
    // 预设选择器：管理员可以选择使用哪个预设配置
    // EliteStandard：精英拟真模式（Optuna 优化出的最严格模式）
    // TacticalAction：战术动作模式（更流畅的游戏体验）
    // Custom：自定义模式（管理员可以手动调整所有参数）
    // 工作台模式：默认使用 EliteStandard
    [Attribute("EliteStandard", UIWidgets.ComboBox, "Select preset configuration.\nEliteStandard: Most realistic, optimized by Optuna for hardcore players.\nTacticalAction: More fluid gameplay, optimized for action-oriented servers.\nCustom: Manually adjust all parameters.\n选择预设配置方案。\nEliteStandard：最拟真，由 Optuna 为硬核玩家优化。\nTacticalAction：更流畅的游戏体验，为动作导向的服务器优化。\nCustom：手动调整所有参数。", "EliteStandard TacticalAction Custom")]
    string m_sSelectedPreset;

    // ==================== 预设参数包 ====================
    // 每个预设都有自己的参数包，包含 11 个由 Optuna 优化的核心参数
    // 管理员可以直接修改这些参数，或者选择使用预设
    
    // EliteStandard 预设参数包（精英拟真）
    // 适用于硬核拟真服务器，追求最真实的体力系统
    [Attribute(desc: "EliteStandard preset parameters.\nMost realistic configuration optimized by Optuna for hardcore players.\nEliteStandard 预设参数包。\n最拟真的配置，由 Optuna 为硬核玩家优化。")]
    ref SCR_RSS_Params m_EliteStandard;

    // TacticalAction 预设参数包（战术动作）
    // 适用于动作导向的服务器，追求更流畅的游戏体验
    [Attribute(desc: "TacticalAction preset parameters.\nMore fluid gameplay configuration optimized for action-oriented servers.\nTacticalAction 预设参数包。\n更流畅的游戏体验配置，为动作导向的服务器优化。")]
    ref SCR_RSS_Params m_TacticalAction;

    // Custom 预设参数包（自定义）
    // 管理员可以手动调整所有参数
    [Attribute(desc: "Custom preset parameters.\nManually adjust all parameters to your liking.\nCustom 预设参数包。\n手动调整所有参数以适应您的需求。")]
    ref SCR_RSS_Params m_Custom;

    // ==================== 初始化预设默认值 ====================
    // 这个方法初始化三个预设的默认值
    // 这些值是由 Optuna 贝叶斯优化器在 13 个最严苛工况下跑出的最优解
    // EliteStandard：精英拟真模式（最严格）
    // TacticalAction：战术动作模式（更流畅）
    // Custom：自定义模式（合理的默认备份值）
    
    void InitPresets()
    {
        if (!m_EliteStandard)
            m_EliteStandard = new SCR_RSS_Params();
        
        if (!m_TacticalAction)
            m_TacticalAction = new SCR_RSS_Params();
        
        if (!m_Custom)
            m_Custom = new SCR_RSS_Params();
        
        // EliteStandard 预设：精英拟真模式（最严格）
        // 适用于硬核拟真服务器，追求最真实的体力系统
        m_EliteStandard.energy_to_stamina_coeff = 0.000025;
        m_EliteStandard.base_recovery_rate = 0.000499;
        m_EliteStandard.standing_recovery_multiplier = 1.765;
        m_EliteStandard.prone_recovery_multiplier = 2.756;
        m_EliteStandard.load_recovery_penalty_coeff = 0.000302;
        m_EliteStandard.load_recovery_penalty_exponent = 2.401;
        m_EliteStandard.encumbrance_speed_penalty_coeff = 0.272;
        m_EliteStandard.encumbrance_stamina_drain_coeff = 1.939;
        m_EliteStandard.sprint_stamina_drain_multiplier = 3.01;
        m_EliteStandard.fatigue_accumulation_coeff = 0.02;
        m_EliteStandard.fatigue_max_factor = 2.70;
        
        // TacticalAction 预设：战术动作模式（更流畅）
        // 适用于动作导向的服务器，追求更流畅的游戏体验
        m_TacticalAction.energy_to_stamina_coeff = 0.000020;
        m_TacticalAction.base_recovery_rate = 0.000440;
        m_TacticalAction.standing_recovery_multiplier = 1.382;
        m_TacticalAction.prone_recovery_multiplier = 2.785;
        m_TacticalAction.load_recovery_penalty_coeff = 0.000161;
        m_TacticalAction.load_recovery_penalty_exponent = 2.848;
        m_TacticalAction.encumbrance_speed_penalty_coeff = 0.217;
        m_TacticalAction.encumbrance_stamina_drain_coeff = 1.636;
        m_TacticalAction.sprint_stamina_drain_multiplier = 2.45;
        m_TacticalAction.fatigue_accumulation_coeff = 0.023;
        m_TacticalAction.fatigue_max_factor = 2.50;
        
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
    
    // ==================== 体力系统配置 ====================
    // 全局体力系统配置：控制体力的消耗和恢复
    // 这些倍率可以全局调整体力系统的难度
    
    // 全局体力消耗倍率（0.1-5.0）
    // 值越大，体力消耗越快，游戏难度越高
    [Attribute("1.0", UIWidgets.EditBox, "Global stamina consumption multiplier (0.1-5.0).\nHigher value = faster stamina drain, higher difficulty.\n全局体力消耗倍率（0.1-5.0）。\n值越大，体力消耗越快，游戏难度越高。")]
    float m_fStaminaDrainMultiplier;
    
    // 全局体力恢复倍率（0.1-5.0）
    // 值越大，体力恢复越快，游戏难度越低
    [Attribute("1.0", UIWidgets.EditBox, "Global stamina recovery multiplier (0.1-5.0).\nHigher value = faster stamina recovery, lower difficulty.\n全局体力恢复倍率（0.1-5.0）。\n值越大，体力恢复越快，游戏难度越低。")]
    float m_fStaminaRecoveryMultiplier;
    
    // 负重速度惩罚倍率（0.1-5.0）
    // 值越大，负重时移动速度越慢
    [Attribute("1.0", UIWidgets.EditBox, "Encumbrance speed penalty multiplier (0.1-5.0).\nHigher value = slower movement under load.\n负重速度惩罚倍率（0.1-5.0）。\n值越大，负重时移动速度越慢。")]
    float m_fEncumbranceSpeedPenaltyMultiplier;
    
    // ==================== 移动系统配置 ====================
    // 移动系统配置：控制 Sprint 和 Run 的速度与体力消耗
    
    // Sprint 速度倍率（1.0-2.0）
    // 值越大，Sprint 时移动速度越快
    [Attribute("1.3", UIWidgets.EditBox, "Sprint speed multiplier (1.0-2.0).\nHigher value = faster sprint speed.\nSprint速度倍率（1.0-2.0）。\n值越大，Sprint 时移动速度越快。")]
    float m_fSprintSpeedMultiplier;
    
    // Sprint 体力消耗倍率（1.0-10.0）
    // 值越大，Sprint 时体力消耗越快
    [Attribute("3.0", UIWidgets.EditBox, "Sprint stamina drain multiplier (1.0-10.0).\nHigher value = faster stamina drain when sprinting.\nSprint体力消耗倍率（1.0-10.0）。\n值越大，Sprint 时体力消耗越快。")]
    float m_fSprintStaminaDrainMultiplier;
    
    // ==================== 环境因子配置 ====================
    // 环境因子配置：控制各种环境因素对体力系统的影响
    // 这些系统可以单独启用或禁用
    
    // 启用热应激系统（10:00-18:00）
    // 模拟高温环境对体力系统的影响
    [Attribute("true", UIWidgets.CheckBox, "Enable heat stress system (10:00-18:00).\nSimulates the impact of high temperature on stamina system.\n启用热应激系统（10:00-18:00）。\n模拟高温环境对体力系统的影响。")]
    bool m_bEnableHeatStress;
    
    // 启用降雨湿重系统（衣服湿重）
    // 模拟降雨导致衣服湿重增加对体力系统的影响
    [Attribute("true", UIWidgets.CheckBox, "Enable rain weight system (clothes get wet).\nSimulates the impact of rain-soaked clothes on stamina system.\n启用降雨湿重系统（衣服湿重）。\n模拟降雨导致衣服湿重增加对体力系统的影响。")]
    bool m_bEnableRainWeight;
    
    // 启用风阻系统
    // 模拟风速对移动速度和体力消耗的影响
    [Attribute("true", UIWidgets.CheckBox, "Enable wind resistance system.\nSimulates the impact of wind speed on movement speed and stamina drain.\n启用风阻系统。\n模拟风速对移动速度和体力消耗的影响。")]
    bool m_bEnableWindResistance;
    
    // 启用泥泞惩罚系统（湿地形）
    // 模拟湿地形对移动速度和体力消耗的影响
    [Attribute("true", UIWidgets.CheckBox, "Enable mud penalty system (wet terrain).\nSimulates the impact of wet terrain on movement speed and stamina drain.\n启用泥泞惩罚系统（湿地形）。\n模拟湿地形对移动速度和体力消耗的影响。")]
    bool m_bEnableMudPenalty;
    
    // ==================== 高级配置 ====================
    // 高级配置：控制高级系统功能的启用或禁用
    // 这些系统可以单独启用或禁用
    
    // 启用疲劳积累系统
    // 模拟长时间运动后的疲劳累积效应
    [Attribute("true", UIWidgets.CheckBox, "Enable fatigue accumulation system.\nSimulates fatigue accumulation effect after prolonged exercise.\n启用疲劳积累系统。\n模拟长时间运动后的疲劳累积效应。")]
    bool m_bEnableFatigueSystem;
    
    // 启用代谢适应系统
    // 模拟长期训练后的代谢适应效应
    [Attribute("true", UIWidgets.CheckBox, "Enable metabolic adaptation system.\nSimulates metabolic adaptation effect after long-term training.\n启用代谢适应系统。\n模拟长期训练后的代谢适应效应。")]
    bool m_bEnableMetabolicAdaptation;
    
    // 启用室内检测系统
    // 检测玩家是否在室内，调整环境因子影响
    [Attribute("true", UIWidgets.CheckBox, "Enable indoor detection system.\nDetects if player is indoors, adjusts environmental factor impact.\n启用室内检测系统。\n检测玩家是否在室内，调整环境因子影响。")]
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
            m_sSelectedPreset = "EliteStandard";
        
        if (m_sSelectedPreset == "EliteStandard")
        {
            if (!m_EliteStandard)
                m_EliteStandard = new SCR_RSS_Params();
            return m_EliteStandard;
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
