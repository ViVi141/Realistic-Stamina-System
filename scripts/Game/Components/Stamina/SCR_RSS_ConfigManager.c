// RSS配置管理器 - 使用Reforger官方标准
// 负责从服务器Profile目录读取或生成JSON配置文件
// 建议路径: scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c
// 版本: v3.5.0

class SCR_RSS_ConfigManager
{
    protected static const string CONFIG_PATH = "$profile:RealisticStaminaSystem.json";
    protected static const string CONFIG_BACKUP_PATH = "$profile:RealisticStaminaSystem.bak.json";  // 配置备份路径
    protected static const int MAX_BACKUP_COUNT = 3;  // 最大备份文件数量
    protected static const string CURRENT_VERSION = "3.13.1";  // 当前模组版本
    protected static ref SCR_RSS_Settings m_Settings;
    protected static bool m_bIsLoaded = false;
    protected static float m_fLastLoadTime = 0.0;
    protected static const float RELOAD_COOLDOWN = 5.0;  // 重载冷却（秒）
    protected static bool m_bIsServerConfigApplied = false;  // 是否已应用服务器配置
    protected static ref array<IEntity> m_aConfigChangeListeners = new array<IEntity>();  // 配置变更监听器
    protected static string m_sLastSelectedPreset = "";  // 上次选中的预设，用于检测变更
    protected static ref SCR_RSS_Settings m_CachedSettings;  // 配置缓存，用于检测变更
    protected static float m_fLastSyncTime = 0.0;  // 上次同步时间
    protected static const float SYNC_COOLDOWN = 2.0;  // 同步冷却（秒）
    protected static bool m_bIsSyncing = false;  // 是否正在同步
    
    // 默认值与合理范围常量（便于维护）
    protected static const int DEFAULT_UPDATE_INTERVAL_MS = 5000;    // 检测/日志更新间隔
    protected static const int MAX_UPDATE_INTERVAL_MS = 60000;       // 最大间隔 60 秒
    protected static const float DEFAULT_HINT_DURATION = 2.0;        // Hint 显示时长（秒）
    protected static const float STAMINA_MULT_MIN = 0.1;             // 体力倍率下限
    protected static const float STAMINA_MULT_MAX = 5.0;             // 体力倍率上限
    protected static const float SPRINT_SPEED_MAX = 2.0;             // Sprint 速度倍率上限
    protected static const float SPRINT_DRAIN_MAX = 10.0;            // Sprint 消耗倍率上限

    // Encumbrance parameter bounds
    protected static const float ENCUMBRANCE_EXP_MIN = 1.0;          // 负重速度惩罚指数下限
    protected static const float ENCUMBRANCE_EXP_MAX = 3.0;          // 负重速度惩罚指数上限
    protected static const float ENCUMBRANCE_MAX_MIN = 0.4;          // 负重速度惩罚上限下限（至少0.4）
    protected static const float ENCUMBRANCE_MAX_MAX = 0.95;         // 负重速度惩罚上限上限（不超过0.95）
    
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
        // Workbench: force enable debug and HUD for easy verification in the editor
        m_Settings.m_bDebugLogEnabled = true;
        m_Settings.m_bHintDisplayEnabled = true;
        m_bIsLoaded = true;
        m_fLastLoadTime = 0.0;
        EnsureDefaultValues();
        UpdateConfigCache();
        Print("[RSS_ConfigManager] Workbench: Using embedded preset values (profile bypassed). Debug/HUD forced ON.");
        return;
        #endif

        // 客户端不读取本地 JSON，仅使用内存默认值等待服务器同步
        if (!Replication.IsServer())
        {
            m_Settings = new SCR_RSS_Settings();
            m_Settings.InitPresets();
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
            m_Settings.m_fSprintStaminaDrainMultiplier = 3.5;

            m_bIsLoaded = true;
            m_fLastLoadTime = 0.0;
            EnsureDefaultValues();
            UpdateConfigCache();
            Print("[RSS_ConfigManager] Client: Using in-memory defaults (JSON read skipped).");
            return;
        }
        
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
            
            // 检查是否已应用服务器配置
            if (!m_bIsServerConfigApplied)
            {
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
                    if (CanWriteConfig())
                    {
                        Save();
                        Print("[RSS_ConfigManager] Non-Custom preset detected. JSON values synchronized with latest mod defaults.");
                    }
                }
                else
                {
                    // 如果是 Custom 模式，仅执行常规初始化（补全可能缺失的字段），不覆盖已有数值
                    m_Settings.InitPresets(false);
                    Print("[RSS_ConfigManager] Custom preset active. Preserving user-defined JSON values.");
                }
                
                // --- 核心修复逻辑结束 ---
            }
            else
            {
                // 已应用服务器配置，不覆盖预设值
                Print("[RSS_ConfigManager] Server config already applied. Preserving server preset values.");
                // 只补全可能缺失的字段，不覆盖已有数值
                m_Settings.InitPresets(false);
            }
            
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
            m_Settings.m_fSprintStaminaDrainMultiplier = 3.5;
            
            // 工作台模式：默认使用 EliteStandard
            #ifdef WORKBENCH
                m_Settings.m_sSelectedPreset = "EliteStandard";
                m_Settings.m_bDebugLogEnabled = true;
                Print("[RSS_ConfigManager] Workbench detected - Forcing EliteStandard model for verification.");
            #endif

            if (CanWriteConfig())
            {
                Save();
                Print("[RSS_ConfigManager] Default settings created at " + CONFIG_PATH);
            }
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
        
        // 更新配置缓存
        UpdateConfigCache();
        
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
            m_Settings.m_fSprintStaminaDrainMultiplier = 3.5;
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

    // 仅允许服务器写入配置文件，防止客户端覆盖服务器 JSON
    protected static bool CanWriteConfig()
    {
        #ifdef WORKBENCH
        return true;
        #endif

        return Replication.IsServer();
    }
    
    // 保存配置文件
    static void Save()
    {
        if (!m_Settings) m_Settings = new SCR_RSS_Settings();

        if (!CanWriteConfig())
        {
            // 客户端保持内存配置，不写入磁盘
            UpdateConfigCache();
            return;
        }
        
        // 创建配置备份
        CreateConfigBackup();
        
        // 使用官方的JsonSaveContext
        SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
        saveContext.WriteValue("", m_Settings);
        saveContext.SaveToFile(CONFIG_PATH);
        Print("[RSS_ConfigManager] Settings saved to " + CONFIG_PATH);
        
        // 更新配置缓存
        UpdateConfigCache();
        
        // 检测配置变更并通知
        if (Replication.IsServer())
        {
            DetectConfigChanges();
            NotifyConfigChanges();
        }
    }
    
    // 创建配置备份
    protected static void CreateConfigBackup()
    {
        // 检查主配置文件是否存在
        if (!FileIO.FileExists(CONFIG_PATH))
            return;
        
        // 管理备份文件
        ManageBackupFiles();
        
        // 创建当前备份
        FileIO.CopyFile(CONFIG_PATH, CONFIG_BACKUP_PATH);
        Print("[RSS_ConfigManager] Config backup created at " + CONFIG_BACKUP_PATH);
    }
    
    // 管理备份文件
    protected static void ManageBackupFiles()
    {
        // 仅保留单个 .bak.json，不保留编号历史备份
        for (int i = 1; i <= MAX_BACKUP_COUNT; i++)
        {
            string oldBackupPath = CONFIG_BACKUP_PATH + "." + i.ToString();
            if (FileIO.FileExists(oldBackupPath))
            {
                FileIO.DeleteFile(oldBackupPath);
            }
        }
    }
    
    // 从备份恢复配置
    static bool RestoreFromBackup()
    {
        // 尝试从最新的备份恢复
        for (int i = 1; i <= MAX_BACKUP_COUNT; i++)
        {
            string backupPath = CONFIG_BACKUP_PATH + "." + i.ToString();
            if (FileIO.FileExists(backupPath))
            {
                // 复制备份文件到主配置文件
                if (FileIO.CopyFile(backupPath, CONFIG_PATH))
                {
                    Print("[RSS_ConfigManager] Config restored from backup: " + backupPath);
                    // 重新加载配置
                    m_bIsLoaded = false;
                    Load();
                    return true;
                }
            }
        }
        
        // 尝试从主备份文件恢复
        if (FileIO.FileExists(CONFIG_BACKUP_PATH))
        {
            if (FileIO.CopyFile(CONFIG_BACKUP_PATH, CONFIG_PATH))
            {
                Print("[RSS_ConfigManager] Config restored from main backup");
                // 重新加载配置
                m_bIsLoaded = false;
                Load();
                return true;
            }
        }
        
        Print("[RSS_ConfigManager] No backup files found for restoration");
        return false;
    }
    
    // 更新配置缓存
    protected static void UpdateConfigCache()
    {
        if (!m_Settings) return;
        
        // 创建配置副本作为缓存
        m_CachedSettings = new SCR_RSS_Settings();
        
        // 复制基本配置
        m_CachedSettings.m_sConfigVersion = m_Settings.m_sConfigVersion;
        m_CachedSettings.m_sSelectedPreset = m_Settings.m_sSelectedPreset;
        
        // 复制预设参数
        if (m_Settings.m_EliteStandard) {
            m_CachedSettings.m_EliteStandard = new SCR_RSS_Params();
            m_CachedSettings.m_EliteStandard = m_Settings.m_EliteStandard;
        }
        if (m_Settings.m_StandardMilsim) {
            m_CachedSettings.m_StandardMilsim = new SCR_RSS_Params();
            m_CachedSettings.m_StandardMilsim = m_Settings.m_StandardMilsim;
        }
        if (m_Settings.m_TacticalAction) {
            m_CachedSettings.m_TacticalAction = new SCR_RSS_Params();
            m_CachedSettings.m_TacticalAction = m_Settings.m_TacticalAction;
        }
        if (m_Settings.m_Custom) {
            m_CachedSettings.m_Custom = new SCR_RSS_Params();
            m_CachedSettings.m_Custom = m_Settings.m_Custom;
        }
        
        // 复制其他配置
        m_CachedSettings.m_bDebugLogEnabled = m_Settings.m_bDebugLogEnabled;
        m_CachedSettings.m_iDebugUpdateInterval = m_Settings.m_iDebugUpdateInterval;
        m_CachedSettings.m_bVerboseLogging = m_Settings.m_bVerboseLogging;
        m_CachedSettings.m_bLogToFile = m_Settings.m_bLogToFile;
        m_CachedSettings.m_bHintDisplayEnabled = m_Settings.m_bHintDisplayEnabled;
        m_CachedSettings.m_iHintUpdateInterval = m_Settings.m_iHintUpdateInterval;
        m_CachedSettings.m_fHintDuration = m_Settings.m_fHintDuration;
        m_CachedSettings.m_fStaminaDrainMultiplier = m_Settings.m_fStaminaDrainMultiplier;
        m_CachedSettings.m_fStaminaRecoveryMultiplier = m_Settings.m_fStaminaRecoveryMultiplier;
        m_CachedSettings.m_fEncumbranceSpeedPenaltyMultiplier = m_Settings.m_fEncumbranceSpeedPenaltyMultiplier;
        m_CachedSettings.m_fSprintSpeedMultiplier = m_Settings.m_fSprintSpeedMultiplier;
        m_CachedSettings.m_fSprintStaminaDrainMultiplier = m_Settings.m_fSprintStaminaDrainMultiplier;
        m_CachedSettings.m_bEnableHeatStress = m_Settings.m_bEnableHeatStress;
        m_CachedSettings.m_bEnableRainWeight = m_Settings.m_bEnableRainWeight;
        m_CachedSettings.m_bEnableWindResistance = m_Settings.m_bEnableWindResistance;
        m_CachedSettings.m_bEnableMudPenalty = m_Settings.m_bEnableMudPenalty;
        m_CachedSettings.m_bEnableFatigueSystem = m_Settings.m_bEnableFatigueSystem;
        m_CachedSettings.m_bEnableMetabolicAdaptation = m_Settings.m_bEnableMetabolicAdaptation;
        m_CachedSettings.m_bEnableIndoorDetection = m_Settings.m_bEnableIndoorDetection;
        m_CachedSettings.m_iTerrainUpdateInterval = m_Settings.m_iTerrainUpdateInterval;
        m_CachedSettings.m_iEnvironmentUpdateInterval = m_Settings.m_iEnvironmentUpdateInterval;
        
        Print("[RSS_ConfigManager] Config cache updated");
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
        m_Settings.m_fSprintStaminaDrainMultiplier = 3.5;
        
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

        // Clamp encumbrance-related parameters across all presets to avoid invalid user edits
        array<ref SCR_RSS_Params> presets = { m_Settings.m_EliteStandard, m_Settings.m_StandardMilsim, m_Settings.m_TacticalAction, m_Settings.m_Custom };
        foreach (SCR_RSS_Params p : presets)
        {
            if (!p) continue;

            if (p.encumbrance_speed_penalty_exponent < ENCUMBRANCE_EXP_MIN)
            {
                p.encumbrance_speed_penalty_exponent = ENCUMBRANCE_EXP_MIN;
                needsSave = true;
            }
            else if (p.encumbrance_speed_penalty_exponent > ENCUMBRANCE_EXP_MAX)
            {
                p.encumbrance_speed_penalty_exponent = ENCUMBRANCE_EXP_MAX;
                needsSave = true;
            }

            if (p.encumbrance_speed_penalty_max < ENCUMBRANCE_MAX_MIN)
            {
                p.encumbrance_speed_penalty_max = ENCUMBRANCE_MAX_MIN;
                needsSave = true;
            }
            else if (p.encumbrance_speed_penalty_max > ENCUMBRANCE_MAX_MAX)
            {
                p.encumbrance_speed_penalty_max = ENCUMBRANCE_MAX_MAX;
                needsSave = true;
            }
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
    
    // 设置服务器配置已应用标志
    static void SetServerConfigApplied(bool applied)
    {
        m_bIsServerConfigApplied = applied;
    }
    
    // 检查服务器配置是否已应用
    static bool IsServerConfigApplied()
    {
        return m_bIsServerConfigApplied;
    }
    
    // 注册配置变更监听器
    static void RegisterConfigChangeListener(IEntity listener)
    {
        if (listener && !m_aConfigChangeListeners.Contains(listener))
        {
            m_aConfigChangeListeners.Insert(listener);
            Print("[RSS_ConfigManager] Registered config change listener: " + listener.GetName());
        }
    }
    
    // 移除配置变更监听器
    static void UnregisterConfigChangeListener(IEntity listener)
    {
        if (listener && m_aConfigChangeListeners.Contains(listener))
        {
            m_aConfigChangeListeners.RemoveItem(listener);
            Print("[RSS_ConfigManager] Unregistered config change listener: " + listener.GetName());
        }
    }
    
    // 检测配置变更
    static bool DetectConfigChanges()
    {
        if (!m_Settings || !m_CachedSettings)
            return false;
        
        bool hasChanged = false;
        
        // 检测预设变更
        string currentPreset = m_Settings.m_sSelectedPreset;
        if (currentPreset != m_sLastSelectedPreset)
        {
            hasChanged = true;
            m_sLastSelectedPreset = currentPreset;
            Print("[RSS_ConfigManager] Config changed: Preset changed to " + currentPreset);
        }
        
        // 检测关键配置变更
        if (m_Settings.m_bDebugLogEnabled != m_CachedSettings.m_bDebugLogEnabled) {
            hasChanged = true;
            string debugStatus = "disabled";
            if (m_Settings.m_bDebugLogEnabled) {
                debugStatus = "enabled";
            }
            Print("[RSS_ConfigManager] Config changed: Debug log " + debugStatus);
        }
        
        if (m_Settings.m_bHintDisplayEnabled != m_CachedSettings.m_bHintDisplayEnabled) {
            hasChanged = true;
            string hintStatus = "disabled";
            if (m_Settings.m_bHintDisplayEnabled) {
                hintStatus = "enabled";
            }
            Print("[RSS_ConfigManager] Config changed: Hint display " + hintStatus);
        }
        
        if (m_Settings.m_fStaminaDrainMultiplier != m_CachedSettings.m_fStaminaDrainMultiplier) {
            hasChanged = true;
            Print("[RSS_ConfigManager] Config changed: Stamina drain multiplier changed to " + m_Settings.m_fStaminaDrainMultiplier.ToString());
        }
        
        if (m_Settings.m_fStaminaRecoveryMultiplier != m_CachedSettings.m_fStaminaRecoveryMultiplier) {
            hasChanged = true;
            Print("[RSS_ConfigManager] Config changed: Stamina recovery multiplier changed to " + m_Settings.m_fStaminaRecoveryMultiplier.ToString());
        }
        
        // 检测预设参数变更
        if (currentPreset == "EliteStandard" && m_Settings.m_EliteStandard && m_CachedSettings.m_EliteStandard) {
            if (m_Settings.m_EliteStandard.energy_to_stamina_coeff != m_CachedSettings.m_EliteStandard.energy_to_stamina_coeff) {
                hasChanged = true;
                Print("[RSS_ConfigManager] Config changed: EliteStandard energy coefficient updated");
            }
        }
        
        if (currentPreset == "StandardMilsim" && m_Settings.m_StandardMilsim && m_CachedSettings.m_StandardMilsim) {
            if (m_Settings.m_StandardMilsim.energy_to_stamina_coeff != m_CachedSettings.m_StandardMilsim.energy_to_stamina_coeff) {
                hasChanged = true;
                Print("[RSS_ConfigManager] Config changed: StandardMilsim energy coefficient updated");
            }
        }
        
        if (currentPreset == "TacticalAction" && m_Settings.m_TacticalAction && m_CachedSettings.m_TacticalAction) {
            if (m_Settings.m_TacticalAction.energy_to_stamina_coeff != m_CachedSettings.m_TacticalAction.energy_to_stamina_coeff) {
                hasChanged = true;
                Print("[RSS_ConfigManager] Config changed: TacticalAction energy coefficient updated");
            }
        }
        
        if (currentPreset == "Custom" && m_Settings.m_Custom && m_CachedSettings.m_Custom) {
            if (m_Settings.m_Custom.energy_to_stamina_coeff != m_CachedSettings.m_Custom.energy_to_stamina_coeff) {
                hasChanged = true;
                Print("[RSS_ConfigManager] Config changed: Custom energy coefficient updated");
            }
        }
        
        return hasChanged;
    }
    
    // 通知配置变更
    static void NotifyConfigChanges()
    {
        for (int i = 0; i < m_aConfigChangeListeners.Count(); i++)
        {
            IEntity listener = m_aConfigChangeListeners[i];
            if (listener)
            {
                SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(listener.FindComponent(SCR_CharacterControllerComponent));
                if (controller)
                {
                    // 通知监听器配置已变更
                    controller.OnConfigChanged();
                }
            }
        }
    }
    
    // 保存配置并检测变更
    static void SaveWithChangeDetection()
    {
        if (!m_Settings)
            return;
        
        // 保存前检测变更
        bool hasChanged = DetectConfigChanges();
        
        // 保存配置
        Save();
        
        // 如果有变更，通知监听器
        if (hasChanged && Replication.IsServer())
        {
            NotifyConfigChanges();
        }
    }
    
    // 验证配置文件完整性
    static bool ValidateConfigFile()
    {
        // 检查配置文件是否存在
        if (!FileIO.FileExists(CONFIG_PATH))
        {
            Print("[RSS_ConfigManager] Config file not found: " + CONFIG_PATH);
            return false;
        }
        
        // 尝试加载配置文件
        SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
        if (!loadContext.LoadFromFile(CONFIG_PATH))
        {
            Print("[RSS_ConfigManager] Config file is corrupted: " + CONFIG_PATH);
            return false;
        }
        
        // 尝试读取配置
        SCR_RSS_Settings testSettings = new SCR_RSS_Settings();
        if (!loadContext.ReadValue("", testSettings))
        {
            Print("[RSS_ConfigManager] Failed to parse config file: " + CONFIG_PATH);
            return false;
        }
        
        return true;
    }
    
    // 修复损坏的配置文件
    static bool FixCorruptedConfig()
    {
        // 验证配置文件
        if (ValidateConfigFile())
            return true;
        
        // 尝试从备份恢复
        if (RestoreFromBackup())
            return true;
        
        // 创建新的默认配置
        m_Settings = new SCR_RSS_Settings();
        m_Settings.InitPresets();
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
        m_Settings.m_fSprintStaminaDrainMultiplier = 3.5;
        
        Save();
        Print("[RSS_ConfigManager] Created new default config due to corruption");
        return true;
    }
    
    // 获取配置状态信息
    static string GetConfigStatus()
    {
        string status = "[RSS Config Status]\n";
        
        // 基本状态
        status += "Loaded: " + m_bIsLoaded.ToString() + "\n";
        status += "Server Config Applied: " + m_bIsServerConfigApplied.ToString() + "\n";
        
        // 配置文件状态
        status += "Config File Exists: " + FileIO.FileExists(CONFIG_PATH).ToString() + "\n";
        status += "Backup File Exists: " + FileIO.FileExists(CONFIG_BACKUP_PATH).ToString() + "\n";
        
        // 配置内容
        if (m_Settings)
        {
            status += "Config Version: " + m_Settings.m_sConfigVersion + "\n";
            status += "Selected Preset: " + m_Settings.m_sSelectedPreset + "\n";
            
            // 活动参数
            SCR_RSS_Params activeParams = m_Settings.GetActiveParams();
            if (activeParams)
            {
                status += "Active Params: energy_coeff=" + activeParams.energy_to_stamina_coeff.ToString() + ", base_recovery=" + activeParams.base_recovery_rate.ToString() + "\n";
            }
        }
        
        return status;
    }
    
    // 显示配置状态
    static void ShowConfigStatus()
    {
        string status = GetConfigStatus();
        Print(status);
        
        // 如果启用了HUD显示，也可以在游戏内显示
        if (m_Settings && m_Settings.m_bHintDisplayEnabled)
        {
            // 这里可以添加游戏内HUD显示逻辑
        }
    }
    
    // 强制同步配置到所有客户端
    static void ForceSyncToClients()
    {
        if (!Replication.IsServer() || !m_Settings)
            return;
        
        // 通知所有监听器
        NotifyConfigChanges();
        Print("[RSS_ConfigManager] Forced config sync to all clients");
    }
}
