// RSS配置类 - 使用Reforger官方标准
// 利用 [BaseContainerProps] 和 [Attribute] 属性实现自动序列化
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_Settings.c

[BaseContainerProps(configRoot: true)]
class SCR_RSS_Settings
{
    // ==================== 调试配置 ====================
    
    [Attribute("false", UIWidgets.CheckBox, "Enable detailed RSS debug logs in console.\n启用详细RSS调试日志输出到控制台。")]
    bool m_bDebugLogEnabled;
    
    [Attribute("5000", UIWidgets.EditBox, "Debug log interval in milliseconds.\n调试日志刷新间隔（毫秒）。")]
    int m_iDebugUpdateInterval;
    
    [Attribute("false", UIWidgets.CheckBox, "Enable verbose logging (includes all calculation details).\n启用详细日志（包含所有计算细节）。")]
    bool m_bVerboseLogging;
    
    [Attribute("false", UIWidgets.CheckBox, "Enable logging to file (RSS_Log.txt).\n启用日志输出到文件（RSS_Log.txt）。")]
    bool m_bLogToFile;
    
    // ==================== 体力系统配置 ====================
    
    [Attribute("1.0", UIWidgets.EditBox, "Global stamina consumption multiplier (0.1-5.0).\n全局体力消耗倍率（0.1-5.0）。")]
    float m_fStaminaDrainMultiplier;
    
    [Attribute("1.0", UIWidgets.EditBox, "Global stamina recovery multiplier (0.1-5.0).\n全局体力恢复倍率（0.1-5.0）。")]
    float m_fStaminaRecoveryMultiplier;
    
    [Attribute("1.0", UIWidgets.EditBox, "Encumbrance speed penalty multiplier (0.1-5.0).\n负重速度惩罚倍率（0.1-5.0）。")]
    float m_fEncumbranceSpeedPenaltyMultiplier;
    
    // ==================== 移动系统配置 ====================
    
    [Attribute("1.3", UIWidgets.EditBox, "Sprint speed multiplier (1.0-2.0).\nSprint速度倍率（1.0-2.0）。")]
    float m_fSprintSpeedMultiplier;
    
    [Attribute("3.0", UIWidgets.EditBox, "Sprint stamina drain multiplier (1.0-10.0).\nSprint体力消耗倍率（1.0-10.0）。")]
    float m_fSprintStaminaDrainMultiplier;
    
    // ==================== 环境因子配置 ====================
    
    [Attribute("true", UIWidgets.CheckBox, "Enable heat stress system (10:00-18:00).\n启用热应激系统（10:00-18:00）。")]
    bool m_bEnableHeatStress;
    
    [Attribute("true", UIWidgets.CheckBox, "Enable rain weight system (clothes get wet).\n启用降雨湿重系统（衣服湿重）。")]
    bool m_bEnableRainWeight;
    
    [Attribute("true", UIWidgets.CheckBox, "Enable wind resistance system.\n启用风阻系统。")]
    bool m_bEnableWindResistance;
    
    [Attribute("true", UIWidgets.CheckBox, "Enable mud penalty system (wet terrain).\n启用泥泞惩罚系统（湿地形）。")]
    bool m_bEnableMudPenalty;
    
    // ==================== 高级配置 ====================
    
    [Attribute("true", UIWidgets.CheckBox, "Enable fatigue accumulation system.\n启用疲劳积累系统。")]
    bool m_bEnableFatigueSystem;
    
    [Attribute("true", UIWidgets.CheckBox, "Enable metabolic adaptation system.\n启用代谢适应系统。")]
    bool m_bEnableMetabolicAdaptation;
    
    [Attribute("true", UIWidgets.CheckBox, "Enable indoor detection system.\n启用室内检测系统。")]
    bool m_bEnableIndoorDetection;
    
    // ==================== 性能配置 ====================
    
    [Attribute("5000", UIWidgets.EditBox, "Terrain detection update interval in milliseconds.\n地形检测更新间隔（毫秒）。")]
    int m_iTerrainUpdateInterval;
    
    [Attribute("5000", UIWidgets.EditBox, "Environment factor update interval in milliseconds.\n环境因子更新间隔（毫秒）。")]
    int m_iEnvironmentUpdateInterval;
}
