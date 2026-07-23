// RSS配置类 - 使用Reforger官方标准
// 利用 [BaseContainerProps] 和 [Attribute] 属性实现自动序列化
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_Settings.c

// ==================== 预设参数包（SCR_RSS_Params）====================
// 包含 Optuna 优化核心参数，每预设一份。Custom 预设下可直接改 JSON 调参。
// 分组：恢复 | 负重 | 疲劳 | 姿态 | 跳跃/攀爬 | 坡度 | 游泳 | 环境
// ==================== 主配置类 ====================
// 这个类是 RSS 系统的主配置类，包含所有可配置的参数
// 使用 [BaseContainerProps(configRoot: true)] 属性实现自动序列化到 JSON 文件
// 管理员可以通过修改 JSON 文件来调整游戏体验
[BaseContainerProps(configRoot: true)]
class SCR_RSS_Settings
{
    // ==================== Sync helpers ====================
    static const int PARAMS_ARRAY_SIZE = 63;  // v6: +CP/W'/sprint cap
    static const int PARAMS_ARRAY_SIZE_V5 = 59;
    static const int PARAMS_ARRAY_SIZE_LEGACY = 49;  // v3.22.x 网络包无蹲姿恢复槽
    static const int SETTINGS_FLOATS_SIZE = 17;
    static const int SETTINGS_INTS_SIZE = 5;
    //! 与 WriteSettingsToArrays / ApplySettingsFromArrays 末尾布尔数量一致；旧版见 LEGACY
    static const int SETTINGS_BOOLS_SIZE = 19;
    static const int SETTINGS_BOOLS_SIZE_LEGACY_17 = 17;
    static const int SETTINGS_BOOLS_SIZE_LEGACY_16 = 16;
    static const int SETTINGS_BOOLS_SIZE_LEGACY = 15;

    // ==================== 基础配置 ====================
    [Attribute("6.0.0", desc: "Config version for migration. Do not edit. | 配置版本号，用于迁移，请勿修改")]
    string m_sConfigVersion;

    [Attribute("StandardMilsim", UIWidgets.ComboBox, "Preset: EliteStandard | StandardMilsim | TacticalAction | Custom", "EliteStandard StandardMilsim TacticalAction Custom")]
    string m_sSelectedPreset;

    // ==================== 预设参数包（运行时对象，无 Attribute，避免 ICE）====================
    // JSON 持久化见下方 m_aParams* 平面数组；数值由 Bake / ApplyParamsFromArray 填充。
    ref SCR_RSS_Params m_EliteStandard;
    ref SCR_RSS_Params m_StandardMilsim;
    ref SCR_RSS_Params m_TacticalAction;
    ref SCR_RSS_Params m_Custom;

    // 平面数组：Attribute 仅 4 个，替代 Params 内 63×Attribute 的编译压力
    [Attribute(desc: "EliteStandard params flat (PARAMS_ARRAY_SIZE). Auto-managed. | Elite 预设平面数组，自动维护")]
    ref array<float> m_aParamsElite;

    [Attribute(desc: "StandardMilsim params flat. Auto-managed. | Standard 预设平面数组")]
    ref array<float> m_aParamsStandard;

    [Attribute(desc: "TacticalAction params flat. Auto-managed. | Tactical 预设平面数组")]
    ref array<float> m_aParamsTactical;

    [Attribute(desc: "Custom params flat (edit this array or use tools). | Custom 预设平面数组，可手改")]
    ref array<float> m_aParamsCustom;

    // ==================== 初始化预设默认值（实现见 SCR_RSS_SettingsPresetBake）====================
    void InitPresets(bool forceRefreshSystemPresets = false)
    {
        SCR_RSS_SettingsPresetBake.InitPresets(this, forceRefreshSystemPresets);
    }

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

    // ==================== Full sync helpers (impl in SCR_RSS_SettingsSync) ====================
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

    void PackParamsToFlatArrays()
    {
        SCR_RSS_SettingsSync.PackAllParamsToFlatArrays(this);
    }

    bool ApplyFlatArraysToParams()
    {
        return SCR_RSS_SettingsSync.ApplyFlatArraysToAllParams(this);
    }
}