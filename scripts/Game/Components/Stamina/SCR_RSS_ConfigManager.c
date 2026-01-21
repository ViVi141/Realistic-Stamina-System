// RSS配置管理器 - 使用Reforger官方标准
// 负责从服务器Profile目录读取或生成JSON配置文件
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c
// 版本: v3.5.0
class SCR_RSS_ConfigManager
{
    protected static const string CONFIG_PATH = "$profile:RealisticStaminaSystem.json";
    protected static const string CURRENT_VERSION = "3.6.0";  // 当前模组版本
    protected static ref SCR_RSS_Settings m_Settings;
    protected static bool m_bIsLoaded = false;
    protected static float m_fLastLoadTime = 0.0;
    protected static const float RELOAD_COOLDOWN = 5.0; // 重载冷却时间（秒）
    
    // 获取配置实例（单例模式）
    static SCR_RSS_Settings GetSettings()
    {
        if (!m_Settings)
            Load();
        return m_Settings;
    }
    
    // 加载配置文件
    static void Load()
    {
        // 防止频繁重载
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
        if (m_bIsLoaded && (currentTime - m_fLastLoadTime) < RELOAD_COOLDOWN)
        {
            return;
        }
        
        m_Settings = new SCR_RSS_Settings();
        
        // 尝试从磁盘读取
        // 使用官方的JsonLoadContext
        SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
        if (loadContext.LoadFromFile(CONFIG_PATH))
        {
            loadContext.ReadValue("", m_Settings);
            Print("[RSS_ConfigManager] Settings loaded from " + CONFIG_PATH);
            
            // 打印读取的预设状态（调试用）
            string presetStatus = "Presets status: ";
            if (m_Settings.m_EliteStandard)
                presetStatus += "Elite=OK ";
            else
                presetStatus += "Elite=NULL ";
            if (m_Settings.m_StandardMilsim)
                presetStatus += "Standard=OK ";
            else
                presetStatus += "Standard=NULL ";
            if (m_Settings.m_TacticalAction)
                presetStatus += "Tactical=OK ";
            else
                presetStatus += "Tactical=NULL ";
            if (m_Settings.m_Custom)
                presetStatus += "Custom=OK";
            else
                presetStatus += "Custom=NULL";
            Print("[RSS_ConfigManager] " + presetStatus);
            
            // 初始化预设默认值（只有当预设对象为空时才会初始化）
            m_Settings.InitPresets();
            
            // 检查版本号并执行迁移
            string configVersion = m_Settings.m_sConfigVersion;
            if (!configVersion || configVersion == "")
                configVersion = "0.0.0";
            
            if (configVersion != CURRENT_VERSION)
            {
                Print("[RSS_ConfigManager] Config version mismatch: JSON=" + configVersion + ", Mod=" + CURRENT_VERSION);
                MigrateConfig(configVersion);
            }
            
            // 验证配置
            if (!ValidateSettings(m_Settings))
            {
                Print("[RSS_ConfigManager] Warning: Invalid settings detected, using default values");
                ResetToDefaults();
            }
        }
        else
        {
            // 文件不存在，初始化预设并保存默认值
            Print("[RSS_ConfigManager] Config file not found, creating new config with defaults");
            m_Settings.InitPresets();
            
            // 设置所有必要的默认值
            m_Settings.m_sConfigVersion = CURRENT_VERSION;
            m_Settings.m_sSelectedPreset = "StandardMilsim";
            m_Settings.m_bHintDisplayEnabled = true;
            m_Settings.m_iHintUpdateInterval = 5000;
            m_Settings.m_fHintDuration = 2.0;
            m_Settings.m_bDebugLogEnabled = false;
            m_Settings.m_iDebugUpdateInterval = 5000;
            m_Settings.m_iTerrainUpdateInterval = 5000;
            m_Settings.m_iEnvironmentUpdateInterval = 5000;
            m_Settings.m_fStaminaDrainMultiplier = 1.0;
            m_Settings.m_fStaminaRecoveryMultiplier = 1.0;
            m_Settings.m_fSprintSpeedMultiplier = 1.3;
            m_Settings.m_fSprintStaminaDrainMultiplier = 3.0;
            
            // 工作台模式：默认使用 EliteStandard
            #ifdef WORKBENCH
                m_Settings.m_sSelectedPreset = "EliteStandard";
                m_Settings.m_bDebugLogEnabled = true;
                Print("[RSS_ConfigManager] Workbench detected - Forcing EliteStandard model for verification.");
            #endif
            
            Save();
            Print("[RSS_ConfigManager] Default settings created at " + CONFIG_PATH);
        }
        
        m_bIsLoaded = true;
        m_fLastLoadTime = currentTime;
        
        // 工作台模式：强制开启调试和 Hint 显示
        #ifdef WORKBENCH
            m_Settings.m_bDebugLogEnabled = true;
            m_Settings.m_bHintDisplayEnabled = true;
        #endif
        
        // 确保所有字段有合理的默认值（兼容旧版本配置文件或空值）
        EnsureDefaultValues();
        
        // 打印启动提示（让服主确认模组已正常加载）
        string debugStatus;
        if (m_Settings.m_bDebugLogEnabled)
            debugStatus = "ON";
        else
            debugStatus = "OFF";
        
        string hintStatus;
        if (m_Settings.m_bHintDisplayEnabled)
            hintStatus = "ON";
        else
            hintStatus = "OFF";
        
        string presetName = m_Settings.m_sSelectedPreset;
        if (!presetName)
            presetName = "EliteStandard";
        
        Print("[RSS] Realistic Stamina System v" + CURRENT_VERSION + " initialized (Debug: " + debugStatus + ", Hint: " + hintStatus + ", Preset: " + presetName + ")");
        
        // 打印当前预设的关键参数值（调试用）
        SCR_RSS_Params activeParams = m_Settings.GetActiveParams();
        if (activeParams)
        {
            Print("[RSS_ConfigManager] Active preset params: energy_coeff=" + activeParams.energy_to_stamina_coeff.ToString() + 
                  ", base_recovery=" + activeParams.base_recovery_rate.ToString() +
                  ", sprint_drain=" + activeParams.sprint_stamina_drain_multiplier.ToString());
        }
    }
    
    // 配置迁移：将旧版本配置升级到新版本
    // 保留用户已有的配置值，只添加新字段的默认值
    protected static void MigrateConfig(string oldVersion)
    {
        Print("[RSS_ConfigManager] Migrating config from v" + oldVersion + " to v" + CURRENT_VERSION);
        
        // 创建新的默认配置用于获取新字段的默认值
        SCR_RSS_Settings defaultSettings = new SCR_RSS_Settings();
        defaultSettings.InitPresets();
        
        // ==================== v3.4.0 新增字段 ====================
        // HUD 显示系统
        if (m_Settings.m_iHintUpdateInterval <= 0)
        {
            m_Settings.m_iHintUpdateInterval = 5000;
            Print("[RSS_ConfigManager] Migration: Added m_iHintUpdateInterval = 5000");
        }
        if (m_Settings.m_fHintDuration <= 0.0)
        {
            m_Settings.m_fHintDuration = 2.0;
            Print("[RSS_ConfigManager] Migration: Added m_fHintDuration = 2.0");
        }
        // m_bHintDisplayEnabled 默认为 false，需要显式设置为 true
        // 从旧版本升级时，默认开启 HUD
        if (CompareVersions(oldVersion, "3.4.0") < 0)
        {
            // 从 3.4.0 之前的版本升级，默认开启 HUD
            m_Settings.m_bHintDisplayEnabled = true;
            Print("[RSS_ConfigManager] Migration: Set m_bHintDisplayEnabled = true (upgrading from pre-3.4.0)");
        }
        
        // 确保预设选择有效
        if (!m_Settings.m_sSelectedPreset || m_Settings.m_sSelectedPreset == "")
        {
            m_Settings.m_sSelectedPreset = "StandardMilsim";
            Print("[RSS_ConfigManager] Migration: Set m_sSelectedPreset = StandardMilsim");
        }
        
        // ==================== 更新版本号并保存 ====================
        m_Settings.m_sConfigVersion = CURRENT_VERSION;
        Save();
        Print("[RSS_ConfigManager] Migration completed. Config saved with version " + CURRENT_VERSION);
    }
    
    // 比较版本号
    // 返回: -1 如果 v1 < v2, 0 如果 v1 == v2, 1 如果 v1 > v2
    protected static int CompareVersions(string v1, string v2)
    {
        // 简单的版本比较：将版本号转换为数字进行比较
        // 例如 "3.4.0" -> 3*10000 + 4*100 + 0 = 30400
        int num1 = VersionToNumber(v1);
        int num2 = VersionToNumber(v2);
        
        if (num1 < num2)
            return -1;
        else if (num1 > num2)
            return 1;
        else
            return 0;
    }
    
    // 将版本号字符串转换为数字
    // "3.4.0" -> 30400
    protected static int VersionToNumber(string version)
    {
        if (!version || version == "")
            return 0;
        
        // 分割版本号
        array<string> parts = new array<string>();
        version.Split(".", parts, false);
        
        int result = 0;
        int multiplier = 10000;
        
        for (int i = 0; i < parts.Count() && i < 3; i++)
        {
            result += parts[i].ToInt() * multiplier;
            multiplier = multiplier / 100;
        }
        
        return result;
    }
    
    // 确保所有字段有合理的默认值
    // 用于兼容旧版本配置文件或处理空值
    protected static void EnsureDefaultValues()
    {
        if (!m_Settings)
            return;
        
        bool needsSave = false;
        
        // 检查是否是旧版本配置（用于决定 bool 字段的默认值）
        string currentConfigVersion = m_Settings.m_sConfigVersion;
        bool isOldConfig = !currentConfigVersion || currentConfigVersion == "" || CompareVersions(currentConfigVersion, "3.4.0") < 0;
        
        // 版本号
        if (!m_Settings.m_sConfigVersion || m_Settings.m_sConfigVersion == "")
        {
            m_Settings.m_sConfigVersion = CURRENT_VERSION;
            Print("[RSS_ConfigManager] Set default: m_sConfigVersion = " + CURRENT_VERSION);
            needsSave = true;
        }
        
        // 预设选择
        if (!m_Settings.m_sSelectedPreset || m_Settings.m_sSelectedPreset == "")
        {
            m_Settings.m_sSelectedPreset = "StandardMilsim";
            Print("[RSS_ConfigManager] Set default: m_sSelectedPreset = StandardMilsim");
            needsSave = true;
        }
        
        // HUD 显示设置
        if (m_Settings.m_iHintUpdateInterval <= 0)
        {
            m_Settings.m_iHintUpdateInterval = 5000;
            Print("[RSS_ConfigManager] Set default: m_iHintUpdateInterval = 5000");
            needsSave = true;
        }
        if (m_Settings.m_fHintDuration <= 0.0)
        {
            m_Settings.m_fHintDuration = 2.0;
            Print("[RSS_ConfigManager] Set default: m_fHintDuration = 2.0");
            needsSave = true;
        }
        
        // 调试更新间隔
        if (m_Settings.m_iDebugUpdateInterval <= 0)
        {
            m_Settings.m_iDebugUpdateInterval = 5000;
            Print("[RSS_ConfigManager] Set default: m_iDebugUpdateInterval = 5000");
            needsSave = true;
        }
        
        // 地形更新间隔
        if (m_Settings.m_iTerrainUpdateInterval <= 0)
        {
            m_Settings.m_iTerrainUpdateInterval = 5000;
            Print("[RSS_ConfigManager] Set default: m_iTerrainUpdateInterval = 5000");
            needsSave = true;
        }
        
        // 环境更新间隔
        if (m_Settings.m_iEnvironmentUpdateInterval <= 0)
        {
            m_Settings.m_iEnvironmentUpdateInterval = 5000;
            Print("[RSS_ConfigManager] Set default: m_iEnvironmentUpdateInterval = 5000");
            needsSave = true;
        }
        
        // 体力消耗倍数
        if (m_Settings.m_fStaminaDrainMultiplier <= 0.0)
        {
            m_Settings.m_fStaminaDrainMultiplier = 1.0;
            Print("[RSS_ConfigManager] Set default: m_fStaminaDrainMultiplier = 1.0");
            needsSave = true;
        }
        
        // 体力恢复倍数
        if (m_Settings.m_fStaminaRecoveryMultiplier <= 0.0)
        {
            m_Settings.m_fStaminaRecoveryMultiplier = 1.0;
            Print("[RSS_ConfigManager] Set default: m_fStaminaRecoveryMultiplier = 1.0");
            needsSave = true;
        }
        
        // Sprint 速度倍数
        if (m_Settings.m_fSprintSpeedMultiplier <= 0.0)
        {
            m_Settings.m_fSprintSpeedMultiplier = 1.3;
            Print("[RSS_ConfigManager] Set default: m_fSprintSpeedMultiplier = 1.3");
            needsSave = true;
        }
        
        // Sprint 体力消耗倍数
        if (m_Settings.m_fSprintStaminaDrainMultiplier <= 0.0)
        {
            m_Settings.m_fSprintStaminaDrainMultiplier = 3.0;
            Print("[RSS_ConfigManager] Set default: m_fSprintStaminaDrainMultiplier = 3.0");
            needsSave = true;
        }
        
        // HUD 显示开关（特殊处理：旧版本配置默认开启）
        // 由于 bool 默认为 false，无法区分"用户设置为 false"和"字段不存在"
        // 所以只有在旧版本配置时才强制开启
        if (isOldConfig && !m_Settings.m_bHintDisplayEnabled)
        {
            m_Settings.m_bHintDisplayEnabled = true;
            Print("[RSS_ConfigManager] Set default: m_bHintDisplayEnabled = true (old config detected)");
            needsSave = true;
        }
        
        // 如果有任何默认值被设置，保存配置
        if (needsSave)
        {
            Print("[RSS_ConfigManager] Saving config with default values applied");
            Save();
        }
    }
    
    // 保存配置文件
    static void Save()
    {
        if (!m_Settings)
            m_Settings = new SCR_RSS_Settings();
        
        // 使用官方的JsonSaveContext
        SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
        saveContext.WriteValue("", m_Settings);
        saveContext.SaveToFile(CONFIG_PATH);
        Print("[RSS_ConfigManager] Settings saved to " + CONFIG_PATH);
    }
    
    // 重新加载配置文件（热重载）
    static void Reload()
    {
        Print("[RSS_ConfigManager] Reloading settings...");
        m_bIsLoaded = false;
        Load();
        Print("[RSS_ConfigManager] Settings reloaded successfully");
    }
    
    // 重置为默认值
    static void ResetToDefaults()
    {
        Print("[RSS_ConfigManager] Resetting to defaults");
        m_Settings = new SCR_RSS_Settings();
        m_Settings.InitPresets();
        
        // 设置所有必要的默认值
        m_Settings.m_sConfigVersion = CURRENT_VERSION;
        m_Settings.m_sSelectedPreset = "StandardMilsim";
        m_Settings.m_bHintDisplayEnabled = true;
        m_Settings.m_iHintUpdateInterval = 5000;
        m_Settings.m_fHintDuration = 2.0;
        m_Settings.m_bDebugLogEnabled = false;
        m_Settings.m_iDebugUpdateInterval = 5000;
        m_Settings.m_iTerrainUpdateInterval = 5000;
        m_Settings.m_iEnvironmentUpdateInterval = 5000;
        m_Settings.m_fStaminaDrainMultiplier = 1.0;
        m_Settings.m_fStaminaRecoveryMultiplier = 1.0;
        m_Settings.m_fSprintSpeedMultiplier = 1.3;
        m_Settings.m_fSprintStaminaDrainMultiplier = 3.0;
        
        Save();
    }
    
    // 验证配置值的有效性
    // 注意：值为 0 或空不算无效，会由 EnsureDefaultValues() 处理
    // 这里只检查明显超出合理范围的值
    protected static bool ValidateSettings(SCR_RSS_Settings settings)
    {
        if (!settings)
            return false;
        
        bool isValid = true;
        
        // 验证倍率范围（只有当值 > 0 时才检查范围）
        if (settings.m_fStaminaDrainMultiplier > 0 && settings.m_fStaminaDrainMultiplier > 5.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fStaminaDrainMultiplier too high (max 5.0)");
            isValid = false;
        }
        
        if (settings.m_fStaminaRecoveryMultiplier > 0 && settings.m_fStaminaRecoveryMultiplier > 5.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fStaminaRecoveryMultiplier too high (max 5.0)");
            isValid = false;
        }
        
        if (settings.m_fSprintSpeedMultiplier > 0 && settings.m_fSprintSpeedMultiplier > 2.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fSprintSpeedMultiplier too high (max 2.0)");
            isValid = false;
        }
        
        if (settings.m_fSprintStaminaDrainMultiplier > 0 && settings.m_fSprintStaminaDrainMultiplier > 10.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fSprintStaminaDrainMultiplier too high (max 10.0)");
            isValid = false;
        }
        
        // 验证间隔范围（只有当值 > 0 时才检查范围）
        if (settings.m_iDebugUpdateInterval > 0 && settings.m_iDebugUpdateInterval > 60000)
        {
            Print("[RSS_ConfigManager] Warning: m_iDebugUpdateInterval too high (max 60000)");
            isValid = false;
        }
        
        if (settings.m_iTerrainUpdateInterval > 0 && settings.m_iTerrainUpdateInterval > 60000)
        {
            Print("[RSS_ConfigManager] Warning: m_iTerrainUpdateInterval too high (max 60000)");
            isValid = false;
        }
        
        if (settings.m_iEnvironmentUpdateInterval > 0 && settings.m_iEnvironmentUpdateInterval > 60000)
        {
            Print("[RSS_ConfigManager] Warning: m_iEnvironmentUpdateInterval too high (max 60000)");
            isValid = false;
        }
        
        return isValid;
    }
    
    // 获取配置文件路径
    static string GetConfigPath()
    {
        return CONFIG_PATH;
    }
    
    // 检查配置是否已加载
    static bool IsLoaded()
    {
        return m_bIsLoaded;
    }
}
