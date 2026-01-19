// RSS配置管理器 - 使用Reforger官方标准
// 负责从服务器Profile目录读取或生成JSON配置文件
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c

class SCR_RSS_ConfigManager
{
    protected static const string CONFIG_PATH = "$profile:RealisticStaminaSystem.json";
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
        float currentTime = GetGame().GetWorld().GetWorldTime();
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
            
            // 验证配置
            if (!ValidateSettings(m_Settings))
            {
                Print("[RSS_ConfigManager] Warning: Invalid settings detected, using default values");
                ResetToDefaults();
            }
        }
        else
        {
            // 文件不存在，保存默认值
            Save();
            Print("[RSS_ConfigManager] Default settings created at " + CONFIG_PATH);
        }
        
        m_bIsLoaded = true;
        m_fLastLoadTime = currentTime;
        
        // 打印启动提示（让服主确认模组已正常加载）
        string debugStatus;
        if (m_Settings.m_bDebugLogEnabled)
            debugStatus = "ON";
        else
            debugStatus = "OFF";
        Print("[RSS] Realistic Stamina System v2.15.0 initialized (Debug Logs: " + debugStatus + ")");
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
        m_Settings = new SCR_RSS_Settings();
        Save();
    }
    
    // 验证配置值的有效性
    protected static bool ValidateSettings(SCR_RSS_Settings settings)
    {
        if (!settings)
            return false;
        
        bool isValid = true;
        
        // 验证倍率范围
        if (settings.m_fStaminaDrainMultiplier < 0.1 || settings.m_fStaminaDrainMultiplier > 5.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fStaminaDrainMultiplier out of range (0.1-5.0)");
            isValid = false;
        }
        
        if (settings.m_fStaminaRecoveryMultiplier < 0.1 || settings.m_fStaminaRecoveryMultiplier > 5.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fStaminaRecoveryMultiplier out of range (0.1-5.0)");
            isValid = false;
        }
        
        if (settings.m_fSprintSpeedMultiplier < 1.0 || settings.m_fSprintSpeedMultiplier > 2.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fSprintSpeedMultiplier out of range (1.0-2.0)");
            isValid = false;
        }
        
        if (settings.m_fSprintStaminaDrainMultiplier < 1.0 || settings.m_fSprintStaminaDrainMultiplier > 10.0)
        {
            Print("[RSS_ConfigManager] Warning: m_fSprintStaminaDrainMultiplier out of range (1.0-10.0)");
            isValid = false;
        }
        
        // 验证间隔范围
        if (settings.m_iDebugUpdateInterval < 1000 || settings.m_iDebugUpdateInterval > 60000)
        {
            Print("[RSS_ConfigManager] Warning: m_iDebugUpdateInterval out of range (1000-60000)");
            isValid = false;
        }
        
        if (settings.m_iTerrainUpdateInterval < 1000 || settings.m_iTerrainUpdateInterval > 60000)
        {
            Print("[RSS_ConfigManager] Warning: m_iTerrainUpdateInterval out of range (1000-60000)");
            isValid = false;
        }
        
        if (settings.m_iEnvironmentUpdateInterval < 1000 || settings.m_iEnvironmentUpdateInterval > 60000)
        {
            Print("[RSS_ConfigManager] Warning: m_iEnvironmentUpdateInterval out of range (1000-60000)");
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
