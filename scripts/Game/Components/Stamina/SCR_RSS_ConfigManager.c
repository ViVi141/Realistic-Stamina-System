// RSS配置管理器 - 使用Reforger官方标准
// 负责从服务器Profile目录读取或生成JSON配置文件
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c
// 版本: v3.5.0
class SCR_RSS_ConfigManager
{
    protected static const string CONFIG_PATH = "$profile:RealisticStaminaSystem.json";
    protected static const string CURRENT_VERSION = "3.11.1";  // 当前模组版本
    protected static ref SCR_RSS_Settings m_Settings;
    protected static bool m_bIsLoaded = false;
    protected static float m_fLastLoadTime = 0.0;
    protected static const float RELOAD_COOLDOWN = 5.0;  // 重载冷却（秒）
    
    // 默认值与合理范围常量（便于维护）
    protected static const int DEFAULT_UPDATE_INTERVAL_MS = 5000;    // 检测/日志更新间隔
    protected static const int MAX_UPDATE_INTERVAL_MS = 60000;       // 最大间隔 60 秒
    protected static const float DEFAULT_HINT_DURATION = 2.0;        // Hint 显示时长（秒）
    protected static const float STAMINA_MULT_MIN = 0.1;             // 体力倍率下限
    protected static const float STAMINA_MULT_MAX = 5.0;             // 体力倍率上限
    protected static const float SPRINT_SPEED_MAX = 2.0;             // Sprint 速度倍率上限
    protected static const float SPRINT_DRAIN_MAX = 10.0;            // Sprint 消耗倍率上限
    
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
        // 工作台模式：强制使用嵌入的预设值，避免 profile 覆盖导致消耗为 0
        #ifdef WORKBENCH
        m_Settings = new SCR_RSS_Settings();
        m_Settings.m_sSelectedPreset = "EliteStandard";
        m_Settings.InitPresets(true);
        m_bIsLoaded = true;
        m_fLastLoadTime = 0.0;
        EnsureDefaultValues();
        Print("[RSS_ConfigManager] Workbench: Using embedded preset values (profile bypassed)");
        return;
        #endif
        
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
            
            // --- 核心修复逻辑开始 ---
            
            // 检查玩家当前选中的预设（大小写不敏感，避免 JSON 手写 "custom" 被误判）
            string selected = m_Settings.m_sSelectedPreset;
            bool isCustom = false;
            if (selected)
            {
                string selectedLower = selected;
                selectedLower.ToLower();
                isCustom = (selectedLower == "custom");
            }

            if (!isCustom)
            {
                // 如果玩家用的是系统预设，强制用代码里的最新Optuna值覆盖内存
                // 这样即使 JSON 里是旧值，也会被更新
                m_Settings.InitPresets(true);
                
                // 既然内存更新了，我们需要立即保存到 JSON，确保文件同步
                Save();
                Print("[RSS_ConfigManager] Non-Custom preset detected. JSON values synchronized with latest mod defaults.");
            }
            else
            {
                // 如果是 Custom 模式，仅执行常规初始化（补全可能缺失的字段），不覆盖已有数值
                m_Settings.InitPresets(false);
                Print("[RSS_ConfigManager] Custom preset active. Preserving user-defined JSON values.");
            }
            
            // --- 核心修复逻辑结束 ---
            
            // 检查版本号并执行迁移
            string configVersion = m_Settings.m_sConfigVersion;
            if (!configVersion || configVersion == "")
                configVersion = "0.0.0";
            
            if (configVersion != CURRENT_VERSION)
            {
                Print("[RSS_ConfigManager] Config version mismatch: JSON=" + configVersion + ", Mod=" + CURRENT_VERSION);
                MigrateConfig(configVersion);
            }
            
            // 验证配置：只修正无效字段，不重置整个配置（避免丢失用户自定义）
            if (!ValidateSettings(m_Settings))
            {
                Print("[RSS_ConfigManager] Warning: Invalid settings detected, correcting out-of-range values");
                FixInvalidSettings();
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
            m_Settings.m_bHintDisplayEnabled = false;
            m_Settings.m_iHintUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            m_Settings.m_fHintDuration = DEFAULT_HINT_DURATION;
            m_Settings.m_bDebugLogEnabled = false;
            m_Settings.m_iDebugUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            m_Settings.m_iTerrainUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            m_Settings.m_iEnvironmentUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
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
            m_Settings.m_iHintUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
        if (m_Settings.m_fHintDuration <= 0.0)
            m_Settings.m_fHintDuration = DEFAULT_HINT_DURATION;
        // m_bHintDisplayEnabled：新版本默认关闭 HUD
        // 从旧版本升级时，保持 false（不再强制开启）
        
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
        
        // 版本号与预设
        if (!m_Settings.m_sConfigVersion || m_Settings.m_sConfigVersion == "")
        {
            m_Settings.m_sConfigVersion = CURRENT_VERSION;
            needsSave = true;
        }
        if (!m_Settings.m_sSelectedPreset || m_Settings.m_sSelectedPreset == "")
        {
            m_Settings.m_sSelectedPreset = "StandardMilsim";
            needsSave = true;
        }
        
        // HUD 显示设置
        // 间隔类字段：≤0 视为无效，使用默认值
        if (m_Settings.m_iHintUpdateInterval <= 0)
        {
            m_Settings.m_iHintUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        if (m_Settings.m_fHintDuration <= 0.0)
        {
            m_Settings.m_fHintDuration = DEFAULT_HINT_DURATION;
            needsSave = true;
        }
        if (m_Settings.m_iDebugUpdateInterval <= 0)
        {
            m_Settings.m_iDebugUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        if (m_Settings.m_iTerrainUpdateInterval <= 0)
        {
            m_Settings.m_iTerrainUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        if (m_Settings.m_iEnvironmentUpdateInterval <= 0)
        {
            m_Settings.m_iEnvironmentUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        
        // 倍率类字段：≤0 视为无效
        if (m_Settings.m_fStaminaDrainMultiplier <= 0.0)
        {
            m_Settings.m_fStaminaDrainMultiplier = 1.0;
            needsSave = true;
        }
        if (m_Settings.m_fStaminaRecoveryMultiplier <= 0.0)
        {
            m_Settings.m_fStaminaRecoveryMultiplier = 1.0;
            needsSave = true;
        }
        if (m_Settings.m_fSprintSpeedMultiplier <= 0.0)
        {
            m_Settings.m_fSprintSpeedMultiplier = 1.3;
            needsSave = true;
        }
        if (m_Settings.m_fSprintStaminaDrainMultiplier <= 0.0)
        {
            m_Settings.m_fSprintStaminaDrainMultiplier = 3.0;
            needsSave = true;
        }
        
        // 注意：m_bHintDisplayEnabled / m_bDebugLogEnabled 不覆盖，保留用户设置
        // 用户通过 JSON 修改的 UI 设置（hint、debug）必须被保留
        // 迁移逻辑 MigrateConfig 已处理首次从 pre-3.4.0 升级时的默认值
        
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
        m_Settings.m_bHintDisplayEnabled = false;
        m_Settings.m_iHintUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
        m_Settings.m_fHintDuration = DEFAULT_HINT_DURATION;
        m_Settings.m_bDebugLogEnabled = false;
        m_Settings.m_iDebugUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
        m_Settings.m_iTerrainUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
        m_Settings.m_iEnvironmentUpdateInterval = DEFAULT_UPDATE_INTERVAL_MS;
        m_Settings.m_fStaminaDrainMultiplier = 1.0;
        m_Settings.m_fStaminaRecoveryMultiplier = 1.0;
        m_Settings.m_fSprintSpeedMultiplier = 1.3;
        m_Settings.m_fSprintStaminaDrainMultiplier = 3.0;
        
        Save();
    }
    
    // 修正无效配置值（仅 clamp 到合法范围，不重置其他字段）
    // 用于替代 ResetToDefaults，避免因单个字段越界而丢失全部用户配置
    protected static void FixInvalidSettings()
    {
        if (!m_Settings)
            return;
        
        bool needsSave = false;
        
        // 倍率类：越界则 clamp 到合理范围
        if (m_Settings.m_fStaminaDrainMultiplier > 0 && m_Settings.m_fStaminaDrainMultiplier > STAMINA_MULT_MAX)
        {
            m_Settings.m_fStaminaDrainMultiplier = STAMINA_MULT_MAX;
            needsSave = true;
        }
        if (m_Settings.m_fStaminaRecoveryMultiplier > 0 && m_Settings.m_fStaminaRecoveryMultiplier > STAMINA_MULT_MAX)
        {
            m_Settings.m_fStaminaRecoveryMultiplier = STAMINA_MULT_MAX;
            needsSave = true;
        }
        if (m_Settings.m_fSprintSpeedMultiplier > 0 && m_Settings.m_fSprintSpeedMultiplier > SPRINT_SPEED_MAX)
        {
            m_Settings.m_fSprintSpeedMultiplier = SPRINT_SPEED_MAX;
            needsSave = true;
        }
        if (m_Settings.m_fSprintStaminaDrainMultiplier > 0 && m_Settings.m_fSprintStaminaDrainMultiplier > SPRINT_DRAIN_MAX)
        {
            m_Settings.m_fSprintStaminaDrainMultiplier = SPRINT_DRAIN_MAX;
            needsSave = true;
        }
        // 间隔类：过大会影响体验
        if (m_Settings.m_iDebugUpdateInterval > 0 && m_Settings.m_iDebugUpdateInterval > MAX_UPDATE_INTERVAL_MS)
        {
            m_Settings.m_iDebugUpdateInterval = MAX_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        if (m_Settings.m_iTerrainUpdateInterval > 0 && m_Settings.m_iTerrainUpdateInterval > MAX_UPDATE_INTERVAL_MS)
        {
            m_Settings.m_iTerrainUpdateInterval = MAX_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        if (m_Settings.m_iEnvironmentUpdateInterval > 0 && m_Settings.m_iEnvironmentUpdateInterval > MAX_UPDATE_INTERVAL_MS)
        {
            m_Settings.m_iEnvironmentUpdateInterval = MAX_UPDATE_INTERVAL_MS;
            needsSave = true;
        }
        
        if (needsSave)
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
        
        // 倍率范围（>0 时才校验）
        if (settings.m_fStaminaDrainMultiplier > 0 && settings.m_fStaminaDrainMultiplier > STAMINA_MULT_MAX)
            isValid = false;
        if (settings.m_fStaminaRecoveryMultiplier > 0 && settings.m_fStaminaRecoveryMultiplier > STAMINA_MULT_MAX)
            isValid = false;
        if (settings.m_fSprintSpeedMultiplier > 0 && settings.m_fSprintSpeedMultiplier > SPRINT_SPEED_MAX)
            isValid = false;
        if (settings.m_fSprintStaminaDrainMultiplier > 0 && settings.m_fSprintStaminaDrainMultiplier > SPRINT_DRAIN_MAX)
            isValid = false;
        // 间隔范围
        if (settings.m_iDebugUpdateInterval > 0 && settings.m_iDebugUpdateInterval > MAX_UPDATE_INTERVAL_MS)
            isValid = false;
        if (settings.m_iTerrainUpdateInterval > 0 && settings.m_iTerrainUpdateInterval > MAX_UPDATE_INTERVAL_MS)
            isValid = false;
        if (settings.m_iEnvironmentUpdateInterval > 0 && settings.m_iEnvironmentUpdateInterval > MAX_UPDATE_INTERVAL_MS)
            isValid = false;
        
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
