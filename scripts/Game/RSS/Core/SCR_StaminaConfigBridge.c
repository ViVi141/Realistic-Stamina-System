// 体力系统配置桥接模块
// 从 SCR_StaminaConstants.c 拆分出所有通过 SCR_RSS_ConfigManager 动态获取配置的方法
// 保留在 StaminaConstants 中的仅为 static const 常量和简单返回常量的 getter
//
// 拆分原因：SCR_StaminaConstants.c 超过 EnforceScript 65535 字节编译限制
// 日期：2026-05-08

class StaminaConfigBridge
{
    // ==================== 配置系统桥接方法 ====================
    
    // 获取能量到体力转换系数（从配置管理器）
    // [修复 v2.16.0] 降低最小值至 1e-8：优化器产出约 8.9e-7，此前 1e-6 的截断导致游戏实际消耗比
    // 优化器模拟值高 +12%（1e-6 / 8.9e-7 ≈ 1.12）。温度单位 bug 修复后此保护不再必要。
    static const float ENERGY_TO_STAMINA_COEFF_MIN = 0.00000001;  // 1e-08，仅防止零值或负值
    static float GetEnergyToStaminaCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
            {
                float coeff = params.energy_to_stamina_coeff;
                return Math.Max(coeff, ENERGY_TO_STAMINA_COEFF_MIN);
            }
        }
        return 7.173939269261512e-07; // v4 EliteStandard fallback (2026-05)
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
        return 0.000153478; // v4 EliteStandard fallback (2026-05)
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
        return 1.1033940520181997; // v4 EliteStandard fallback (2026-05)
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
        return 2.3436692109309787; // v4 EliteStandard fallback (2026-05)
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
        return 5.401551119196543e-05; // v4 EliteStandard fallback (2026-05)
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
        return 0.12557037442961433; // v4 EliteStandard fallback (2026-05)
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
        return 1.9633666302334787; // v4 EliteStandard fallback (2026-05)
    }

    // 获取负重代谢阻尼（与 Python 优化器同步，避免测试与游戏表现差异）
    static float GetLoadMetabolicDampening()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Clamp(params.load_metabolic_dampening, 0.0, 1.0);
        }
        return 0.70; // fallback
    }

    // 获取每 tick 恢复率上限（与 Python 优化器同步）
    static float GetMaxRecoveryPerTick()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Max(params.max_recovery_per_tick, 0.0);
        }
        return 0.0005831263335868464; // v4 EliteStandard fallback (2026-05)
    }

    // 获取负重速度惩罚指数（从配置管理器）
    static float GetEncumbranceSpeedPenaltyExponent()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_speed_penalty_exponent;
        }
        return 1.5; // 默认值
    }

    // 获取负重速度惩罚上限（从配置管理器）
    static float GetEncumbranceSpeedPenaltyMax()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.encumbrance_speed_penalty_max;
        }
        return 0.75; // 默认值
    }
    
    // 获取Sprint体力消耗倍数（从配置管理器）
    // 确保不低于1.0，避免为0导致体力不消耗
    static float GetSprintStaminaDrainMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
            {
                float v = params.sprint_stamina_drain_multiplier;
                return Math.Max(v, 1.0);
            }
        }
        return 3.5; // [FIX] 默认值与 SPRINT_STAMINA_DRAIN_MULTIPLIER static const 统一为 3.5
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
    
    // 获取蹲姿消耗倍数（从配置管理器）
    static float GetPostureCrouchMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.posture_crouch_multiplier;
        }
        return 2.437251840930866; // v4 EliteStandard fallback (2026-05)
    }

    // 获取趴姿消耗倍数（从配置管理器）
    static float GetPostureProneMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.posture_prone_multiplier;
        }
        return 2.964831628856109; // v4 EliteStandard fallback (2026-05)
    }

    // ==================== 恢复模型参数配置方法 ====================

    // 获取恢复非线性系数（从配置管理器）
    static float GetRecoveryNonlinearCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.recovery_nonlinear_coeff;
        }
        return 0.7916386681694456; // v4 EliteStandard fallback (2026-05)
    }

    // 获取快速恢复倍数（从配置管理器）
    static float GetFastRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.fast_recovery_multiplier;
        }
        return 2.395424393975942; // v4 EliteStandard fallback (2026-05)
    }

    // 获取中等恢复倍数（从配置管理器）
    static float GetMediumRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.medium_recovery_multiplier;
        }
        return 1.1374814244785123; // v4 EliteStandard fallback (2026-05)
    }

    // 获取慢速恢复倍数（从配置管理器）
    static float GetSlowRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.slow_recovery_multiplier;
        }
        return 0.4756093184784932; // v4 EliteStandard fallback (2026-05)
    }

    // 获取最低体力阈值（从配置管理器）
    static float GetMinRecoveryStaminaThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Clamp(params.min_recovery_stamina_threshold, 0.0, 0.5);
        }
        return 0.2; // 回退到硬编码默认值 0.2
    }

    // 获取最低体力时所需静止时间（从配置管理器）
    static float GetMinRecoveryRestTimeSeconds()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Max(params.min_recovery_rest_time_seconds, 0.0);
        }
        return 3.0; // 回退到硬编码默认值 3.0s
    }

    // ==================== Sprint参数配置方法 ====================

    // 获取Sprint速度阈值（从配置管理器）
    static float GetSprintVelocityThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.sprint_velocity_threshold;
        }
        return 5.5; // GAME_MAX_SPEED fallback
    }

    // 获取Sprint速度加成（从配置管理器）
    static float GetSprintSpeedBoost()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.sprint_speed_boost;
        }
        return 0.2561103503743016; // v4 EliteStandard fallback (2026-05)
    }

    // ==================== 边际效应参数配置方法 ====================

    // 获取边际效应衰减阈值（从配置管理器）
    static float GetMarginalDecayThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.marginal_decay_threshold;
        }
        return 0.8; // 默认值（80%体力）
    }

    // 获取边际效应衰减系数（从配置管理器）
    static float GetMarginalDecayCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.marginal_decay_coeff;
        }
        return 1.1; // 默认值
    }

    // ==================== 动作消耗参数配置方法 ====================

    // 跳跃肌肉效率
    static float GetJumpEfficiency()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.jump_efficiency >= 0.15)
                return Math.Clamp(params.jump_efficiency, 0.15, 0.30);
        }
        return 0.22; // [HARD fallback] 0.22 (Margaria 1963)
    }

    // 跳跃重心抬升高度猜测
    static float GetJumpHeightGuess()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.jump_height_guess;
        }
        return 0.5; // default 0.5 m
    }

    // 跳跃水平速度猜测
    static float GetJumpHorizSpeedGuess()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.jump_horizontal_speed_guess;
        }
        return 0.0;
    }

    // 获取攀爬/翻越等长收缩效率（从配置管理器）
    static float GetClimbIsoEfficiency()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return Math.Clamp(params.climb_iso_efficiency, 0.05, 0.25);
        }
        return 0.12; // 硬编码回退值 0.12
    }

    // 获取热应激惩罚系数（从配置管理器）
    static float GetEnvTemperatureHeatPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.env_temperature_heat_penalty_coeff;
        }
        return 0.02;
    }

    // 获取冷应激恢复惩罚系数（从配置管理器）
    static float GetEnvTemperatureColdRecoveryPenaltyCoeff()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.env_temperature_cold_recovery_penalty_coeff;
        }
        return 0.05;
    }

    // 获取地表湿度惩罚最大值（从配置管理器）
    static float GetEnvSurfaceWetnessPenaltyMax()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.env_surface_wetness_prone_penalty;
        }
        return 0.15;
    }

    // 获取降雨单独触发的湿重上限（从配置管理器）
    static float GetEnvRainWeightMax()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.env_rain_weight_max >= 1.0)
                return params.env_rain_weight_max;
        }
        return 5.0; // 硬编码回退值
    }

    // 获取调试状态的快捷静态方法
    static bool IsDebugEnabled()
    {
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
    
    // 获取调试信息刷新频率（默认 1 秒，统一波次输出）
    static int GetDebugUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iDebugUpdateInterval;
        
        return 1000; // 默认 1 秒
    }
    
    // ==================== 以下配置仅在 Custom 预设下生效 ====================
    
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
        
        return 1.0;
    }
    
    // 获取体力恢复倍率（仅 Custom 预设生效）
    static float GetStaminaRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fStaminaRecoveryMultiplier;
        
        return 1.0;
    }
    
    // 获取负重速度惩罚倍率（仅 Custom 预设生效）
    static float GetEncumbranceSpeedPenaltyMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fEncumbranceSpeedPenaltyMultiplier;
        
        return 1.0;
    }
    
    // 获取Sprint速度倍率（仅 Custom 预设生效）
    static float GetSprintSpeedMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_fSprintSpeedMultiplier;
        
        return 1.3;
    }
    
    // 检查是否启用热应激系统
    static bool IsHeatStressEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableHeatStress;
        return true;
    }
    
    // 检查是否启用降雨湿重系统
    static bool IsRainWeightEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableRainWeight;
        return true;
    }
    
    // 检查是否启用风阻系统
    static bool IsWindResistanceEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableWindResistance;
        return true;
    }
    
    // 检查是否启用泥泞惩罚系统
    static bool IsMudPenaltyEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableMudPenalty;
        return true;
    }

    //! 泥泞滑倒机制（布娃娃/镜头失稳/AI 泥泞预警）
    static bool IsMudSlipMechanismEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return false;
        return settings.m_bEnableMudSlipMechanism;
    }

    //! 体力驱动 AI 感知/射速/战斗技能
    static bool IsAIStaminaCombatEffectsEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return false;
        return settings.m_bEnableAIStaminaCombatEffects;
    }

    //! 完全禁用 AI RSS 计算（交还引擎）。勾选时同时关闭 AI combat 效果。
    static bool IsAiAllCalcDisabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return false;
        return settings.m_bDisableAIAllCalc;
    }

    //! 仅禁用 AI 体力消耗/恢复计算，仍保留 RSS 速度倍率
    static bool IsAiStaminaCalcDisabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return false;
        return settings.m_bDisableAIStaminaCalc;
    }
    
    // 检查是否启用疲劳积累系统
    static bool IsFatigueSystemEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableFatigueSystem;
        return true;
    }
    
    // 检查是否启用代谢适应系统
    static bool IsMetabolicAdaptationEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableMetabolicAdaptation;
        return true;
    }
    
    // 检查是否启用室内检测系统
    static bool IsIndoorDetectionEnabled()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_sSelectedPreset == "Custom")
            return settings.m_bEnableIndoorDetection;
        return true;
    }
    
    // 获取地形检测更新间隔
    static int GetTerrainUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iTerrainUpdateInterval;
        return 5000;
    }
    
    // 获取环境因子更新间隔
    static int GetEnvironmentUpdateInterval()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
            return settings.m_iEnvironmentUpdateInterval;
        return 5000;
    }

    // ==========================================================================
    // AI 模块配置桥接
    // ==========================================================================

    //! AI 体力集成总开关（复用现有 combat effects 开关）
    static bool IsAIStaminaIntegrationEnabled()
    {
        return IsAIStaminaCombatEffectsEnabled();
    }

    //! AI 行为过滤开关（与 combat effects 同开关）
    static bool IsAIIntentFilterEnabled()
    {
        return IsAIStaminaCombatEffectsEnabled();
    }

    //! AI 伤害联动开关（与 combat effects 同开关）
    static bool IsAIInjuryLinkEnabled()
    {
        return IsAIStaminaCombatEffectsEnabled();
    }
}
