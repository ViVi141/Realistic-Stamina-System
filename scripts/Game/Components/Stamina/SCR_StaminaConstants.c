// 体力系统常量定义模块
// 存放所有体力系统相关的常量定义和配置管理器桥接方法
// 注意：所有可配置参数现在通过配置管理器获取

class StaminaConstants
{
    // ==================== 基础游戏常量 ====================
    // 这些常量是游戏引擎提供的基础值，不通过配置管理器管理
    static const float GAME_MAX_SPEED = 5.2; // m/s，游戏最大速度
    
    // ==================== 配置系统桥接方法 ====================
    
    // 获取能量到体力转换系数（从配置管理器）
    static float GetEnergyToStaminaCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.energy_to_stamina_coeff;
        }
        return 0.000035; // 默认值
    }
    
    // 获取基础恢复率（从配置管理器）
    static float GetBaseRecoveryRate()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.base_recovery_rate;
        }
        return 0.0003; // 默认值
    }
    
    // 获取站姿恢复倍数（从配置管理器）
    static float GetStandingRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.standing_recovery_multiplier;
        }
        return 2.0; // 默认值
    }
    
    // 获取趴姿恢复倍数（从配置管理器）
    static float GetProneRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.prone_recovery_multiplier;
        }
        return 2.2; // 默认值
    }
    
    // 获取负重恢复惩罚系数（从配置管理器）
    static float GetLoadRecoveryPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.load_recovery_penalty_coeff;
        }
        return 0.0004; // 默认值
    }
    
    // 获取负重恢复惩罚指数（从配置管理器）
    static float GetLoadRecoveryPenaltyExponent()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.load_recovery_penalty_exponent;
        }
        return 2.0; // 默认值
    }
    
    // 获取负重速度惩罚系数（从配置管理器）
    static float GetEncumbranceSpeedPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_speed_penalty_coeff;
        }
        return 0.20; // 默认值
    }
    
    // 获取负重体力消耗系数（从配置管理器）
    static float GetEncumbranceStaminaDrainCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_stamina_drain_coeff;
        }
        return 1.5; // 默认值
    }
    
    // 获取Sprint体力消耗倍数（从配置管理器）
    static float GetSprintStaminaDrainMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.sprint_stamina_drain_multiplier;
        }
        return 3.0; // 默认值
    }
    
    // 获取疲劳累积系数（从配置管理器）
    static float GetFatigueAccumulationCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.fatigue_accumulation_coeff;
        }
        return 0.015; // 默认值
    }
    
    // 获取最大疲劳因子（从配置管理器）
    static float GetFatigueMaxFactor()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.fatigue_max_factor;
        }
        return 2.0; // 默认值
    }
    
    // 获取调试状态的快捷静态方法
    static bool IsDebugEnabled()
    {
        // 工作台模式下强制开启以便开发
        #ifdef WORKBENCH
            return true;
        #endif
        
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_bDebugLogEnabled;
        
        return false;
    }
    
    // 获取详细日志状态
    static bool IsVerboseLoggingEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_bVerboseLogging;
        
        return false;
    }
    
    // 获取调试信息刷新频率
    static int GetDebugUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iDebugUpdateInterval;
        
        return 5000; // 默认5秒
    }
    
    // ==================== 以下配置仅在 Custom 预设下生效 ====================
    // 当使用 EliteStandard/StandardMilsim/TacticalAction 预设时，这些配置返回默认值
    // 只有选择 Custom 预设时，用户的自定义配置才会被读取
    
    // 检查是否为 Custom 预设
    static bool IsCustomPreset()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return true;
        return false;
    }
    
    // 获取体力消耗倍率（仅 Custom 预设生效）
    static float GetStaminaDrainMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fStaminaDrainMultiplier;
        
        return 1.0; // 非 Custom 预设返回默认值
    }
    
    // 获取体力恢复倍率（仅 Custom 预设生效）
    static float GetStaminaRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fStaminaRecoveryMultiplier;
        
        return 1.0; // 非 Custom 预设返回默认值
    }
    
    // 获取负重速度惩罚倍率（仅 Custom 预设生效）
    static float GetEncumbranceSpeedPenaltyMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fEncumbranceSpeedPenaltyMultiplier;
        
        return 1.0; // 非 Custom 预设返回默认值
    }
    
    // 获取Sprint速度倍率（仅 Custom 预设生效）
    static float GetSprintSpeedMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fSprintSpeedMultiplier;
        
        return 1.3; // 非 Custom 预设返回默认值
    }
    
    // 检查是否启用热应激系统（仅 Custom 预设生效）
    static bool IsHeatStressEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableHeatStress;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用降雨湿重系统（仅 Custom 预设生效）
    static bool IsRainWeightEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableRainWeight;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用风阻系统（仅 Custom 预设生效）
    static bool IsWindResistanceEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableWindResistance;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用泥泞惩罚系统（仅 Custom 预设生效）
    static bool IsMudPenaltyEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableMudPenalty;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用疲劳积累系统（仅 Custom 预设生效）
    static bool IsFatigueSystemEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableFatigueSystem;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用代谢适应系统（仅 Custom 预设生效）
    static bool IsMetabolicAdaptationEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableMetabolicAdaptation;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 检查是否启用室内检测系统（仅 Custom 预设生效）
    static bool IsIndoorDetectionEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableIndoorDetection;
        
        return true; // 非 Custom 预设默认启用
    }
    
    // 获取地形检测更新间隔
    static int GetTerrainUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iTerrainUpdateInterval;
        
        return 5000; // 默认5秒
    }
    
    // 获取环境因子更新间隔
    static int GetEnvironmentUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iEnvironmentUpdateInterval;
        
        return 5000; // 默认5秒
    }
}