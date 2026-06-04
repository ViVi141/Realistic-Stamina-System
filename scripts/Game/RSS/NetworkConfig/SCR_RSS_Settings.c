// SCR_RSS_Settings.c — RSS 主配置类
// Params / V5 / Sync / PresetDefaults 已拆至独立文件

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
    
    // EliteStandard 预设参数包（精英拟真 / 最硬核）
    // v4 管线：Pareto 前沿上 combat_endurance + recovery_efficiency 联合最小的解（无独立 Hardcore 预设）
    [Attribute(desc: "EliteStandard preset parameters.\nV4 optimizer: minimize combat+recovery on Pareto front (most realistic).\nEliteStandard 预设。\nv4：三目标优化中 combat 与 recovery 联合最小，最拟真。")]
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
    // 三个系统预设 + Custom；由 v4 管线（8 场景 NSGA-II）写出 JSON 后嵌入本文件
    // EliteStandard：低 combat_reserve + 低 recovery_pace（最硬核，无单独 Hardcore 档）
    // StandardMilsim：三目标折中
    // TacticalAction：combat 最小（最宽容）
    // Custom：手动调参备份默认值

    // ==================== 调试配置 ====================
    [Attribute("false", UIWidgets.CheckBox, "Debug: Enable detailed RSS logs in console. Workbench auto-on. | 调试：控制台输出详细日志，工作台模式自动开启")]
    bool m_bDebugLogEnabled;
    
    [Attribute("1000", UIWidgets.EditBox, "Debug batch interval (ms). Unified output per second. | 调试批次间隔（毫秒），统一波次每秒")]
    int m_iDebugUpdateInterval;
    
    [Attribute("false", UIWidgets.CheckBox, "Verbose: Log all calculation details. | 详细模式：输出完整计算过程")]
    bool m_bVerboseLogging;
    
    [Attribute("false", UIWidgets.CheckBox, "Log to file (RSS_Log.txt). | 将日志写入文件")]
    bool m_bLogToFile;
    
    // ==================== HUD 显示配置 ====================
    [Attribute("false", UIWidgets.CheckBox, "HUD: Show stamina/speed/weight in top-right. Default OFF.")]
    bool m_bHintDisplayEnabled;

    [Attribute("false", UIWidgets.CheckBox, "Data Export: Write player stamina/env to JSON for external apps. File: RSS_PlayerData.json in profile. | 数据导出：将玩家体力/环境写入 JSON 供外部应用读取")]
    bool m_bDataExportEnabled;

    [Attribute("false", UIWidgets.CheckBox, "Mud slip mechanic: ragdoll + camera stress on slippery wet terrain. Server chooses via JSON; default OFF. | 泥泞滑倒机制（湿滑地形布娃娃/镜头失稳），服主在 JSON 中开关，默认关闭")]
    bool m_bEnableMudSlipMechanism;

    [Attribute("true", UIWidgets.CheckBox, "[Experimental] AI stamina combat: state machine, speed cap, intent filter, combat decay, group sync. Workbench default ON; dedicated server JSON default OFF. | 【实验性】AI 体力战斗效果：状态机、限速、意图过滤、战斗衰减、群组协同等；工作台默认开启，专用服 JSON 默认关闭")]
    bool m_bEnableAIStaminaCombatEffects;
    
    [Attribute("false", UIWidgets.CheckBox, "AI: disable ALL RSS calculations for AI. Engine handles stamina completely. Also disables AI combat effects. | 完全禁用 AI 的 RSS 计算，体力交还引擎处理。同时关闭 AI 战斗效果")]
    bool m_bDisableAIAllCalc;
    
    [Attribute("false", UIWidgets.CheckBox, "AI: disable stamina calc only. Keep speed multiplier from RSS, skip drain/recovery. | 仅禁用 AI 体力计算，仍保留 RSS 速度倍率")]
    bool m_bDisableAIStaminaCalc;

    [Attribute("1000", UIWidgets.EditBox, "Data export interval (ms). 1000 = 1s. | 数据导出间隔（毫秒），1000=1秒")]
    int m_iDataExportIntervalMs;
    
    [Attribute("5000", UIWidgets.EditBox, "[Deprecated] HUD is real-time now. Kept for compatibility.")]
    int m_iHintUpdateInterval;
    
    [Attribute("2.0", UIWidgets.EditBox, "[Deprecated] HUD is always visible. Kept for compatibility.")]
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

    [Attribute("0.0", UIWidgets.EditBox, "Universal temp: altitude (m). 0 = sea level. Used for lapse rate (-6.5°C/1000m). | 通用气温：海拔（米），0=海平面")]
    float m_fAltitudeMeters;

    [Attribute("false", UIWidgets.CheckBox, "Universal temp: map is ocean/island (reserved for ClimateType). | 通用气温：海洋/岛屿（预留）")]
    bool m_bMapOverWater;

    [Attribute("0.0", UIWidgets.EditBox, "Universal temp: fog/humidity (0~1). Dampens diurnal range. | 通用气温：雾/湿度(0~1)，压缩昼夜温差")]
    float m_fFogDensity;

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

    void InitPresets(bool forceRefreshSystemPresets = false)
    {
        SCR_RSS_PresetDefaults.InitPresets(this, forceRefreshSystemPresets);
    }

    static const int PARAMS_ARRAY_SIZE = SCR_RSS_SettingsSync.PARAMS_ARRAY_SIZE;
    static const int PARAMS_ARRAY_SIZE_LEGACY = SCR_RSS_SettingsSync.PARAMS_ARRAY_SIZE_LEGACY;
    static const int SETTINGS_FLOATS_SIZE = SCR_RSS_SettingsSync.SETTINGS_FLOATS_SIZE;
    static const int SETTINGS_INTS_SIZE = SCR_RSS_SettingsSync.SETTINGS_INTS_SIZE;
    static const int SETTINGS_BOOLS_SIZE = SCR_RSS_SettingsSync.SETTINGS_BOOLS_SIZE;
    static const int SETTINGS_BOOLS_SIZE_LEGACY_17 = SCR_RSS_SettingsSync.SETTINGS_BOOLS_SIZE_LEGACY_17;
    static const int SETTINGS_BOOLS_SIZE_LEGACY_16 = SCR_RSS_SettingsSync.SETTINGS_BOOLS_SIZE_LEGACY_16;
    static const int SETTINGS_BOOLS_SIZE_LEGACY = SCR_RSS_SettingsSync.SETTINGS_BOOLS_SIZE_LEGACY;

    static void WriteParamsToArray(SCR_RSS_Params p, array<float> outArr)
    {
        SCR_RSS_SettingsSync.WriteParamsToArray(p, outArr);
    }

    static void ApplyParamsFromArray(SCR_RSS_Params p, array<float> values)
    {
        SCR_RSS_SettingsSync.ApplyParamsFromArray(p, values);
    }

    static void WriteSettingsToArrays(SCR_RSS_Settings s, array<float> outFloats, array<int> outInts, array<bool> outBools)
    {
        SCR_RSS_SettingsSync.WriteSettingsToArrays(s, outFloats, outInts, outBools);
    }

    static void ApplySettingsFromArrays(SCR_RSS_Settings s, array<float> floats, array<int> ints, array<bool> bools)
    {
        SCR_RSS_SettingsSync.ApplySettingsFromArrays(s, floats, ints, bools);
    }
}
