// 体力系统配置桥接模块
// 从 SCR_StaminaConstants.c 拆分出所有通过 SCR_RSS_ConfigManager 动态获取配置的方法
// 保留在 SCR_RSS_Constants 中的仅为 static const 常量和简单返回常量的 getter
//
// 拆分原因：SCR_StaminaConstants.c 超过 EnforceScript 65535 字节编译限制
// 日期：2026-05-08

class SCR_RSS_ConfigBridge
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
        return 9.5e-07; // Hardcore fallback (2026-05，原7.17e-07)
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
        return 0.00010; // Hardcore fallback (2026-05，原0.000153)
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
        return 0.85; // Hardcore fallback (2026-05，原1.103)
    }
    
    // 获取蹲姿恢复倍数（从配置管理器）
    static float GetCrouchingRecoveryMultiplier()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params)
                return params.crouching_recovery_multiplier;
        }
        return 1.6; // Hardcore fallback (2026-05，原1.5)
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
        return 1.9; // Hardcore fallback (2026-05，原2.344)
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
        return 0.0002; // Hardcore fallback (2026-05，原5.4e-05)
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
        return 2.0; // 负重恢复惩罚指数（平方关系，不变）
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
        return 0.28; // Hardcore fallback (2026-05，原0.126)
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
        return 2.8; // Hardcore fallback (2026-05，原1.963)
    }

    // 获取负重代谢阻尼（与 Python/Rust 数字孪生同步：仅超额负重部分的代谢成本）
    static float GetLoadMetabolicDampening()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.load_metabolic_dampening > 0.0)
                return Math.Clamp(params.load_metabolic_dampening, 0.1, 1.0);
        }
        return 0.70;
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
        return 0.0004; // Hardcore fallback (2026-05，原0.000583)
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
        return 0.025; // Hardcore fallback (2026-05，原0.015)
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
        return 2.5; // Hardcore fallback (2026-05，原2.0)
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
        return 3.0; // Hardcore fallback (2026-05，原2.437) — 蹲姿移动消耗更高
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
        return 3.5; // Hardcore fallback (2026-05，原2.965) — 趴姿移动消耗更高
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
        return 0.5; // Hardcore fallback (2026-05，原0.792)
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
        return 1.6; // Hardcore fallback (2026-05，原2.395)
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
        return 1.0; // Hardcore fallback (2026-05，原1.137)
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
        return 0.35; // Hardcore fallback (2026-05，原0.476)
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
        return 5.0; // Hardcore fallback (2026-05，原3.0s)
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
        return SCR_RSS_Constants.SPRINT_SPEED_BOOST;
    }
    
    // ==================== 速度模型阈值配置方法（Hardcore 新增暴露）====================
    
    // 获取意志力平台期阈值（从配置管理器）
    // 体力高于此值时保持恒定目标速度。Hardcore 默认 0.35（原 0.25）
    static float GetWillpowerThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.willpower_threshold > 0.0)
                return Math.Clamp(params.willpower_threshold, 0.15, 0.5);
        }
        return 0.35; // Hardcore fallback（原0.25）
    }
    
    // 获取平滑过渡起点（别名：等同于 GetWillpowerThreshold）
    static float GetSmoothTransitionStart()
    {
        return GetWillpowerThreshold();
    }
    
    // 获取冲刺最小体力阈值（从配置管理器）
    // 体力低于此值时禁止冲刺。Hardcore 默认 0.25（原 0.18）
    static float GetSprintEnableThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.sprint_enable_threshold > 0.0)
                return Math.Clamp(params.sprint_enable_threshold, 0.10, 0.40);
        }
        return 0.25; // Hardcore fallback（原0.18）
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

    // ==========================================================================
    // v5 双池 / 代谢锚点
    // ==========================================================================

    static float GetSustainableWatts()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.sustainable_watts > 0.0)
                return params.sustainable_watts;
        }
        return SCR_RSS_Constants.V5_SUSTAINABLE_WATTS_DEFAULT;
    }

    //! 行军档 Walk 绝对速度（m/s）；Params 字段仍为 v5_walk_speed_ms（网络同步不可改）
    //! V6_USE_MARCH_GAIT_SPEEDS=false 时改回引擎 Walk 顶。
    static float GetMarchWalkSpeedMs()
    {
        if (!SCR_RSS_Constants.V6_USE_MARCH_GAIT_SPEEDS)
            return SCR_RSS_Constants.ENGINE_WALK_TOP_MS;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.v5_walk_speed_ms > 0.0)
                return params.v5_walk_speed_ms;
        }
        return SCR_RSS_Constants.V5_WALK_SPEED_MS_DEFAULT;
    }

    //! 行军档 Run 绝对速度（m/s）；关 March 时用引擎 Run 顶 3.8
    static float GetMarchRunSpeedMs()
    {
        if (!SCR_RSS_Constants.V6_USE_MARCH_GAIT_SPEEDS)
            return SCR_RSS_Constants.TARGET_RUN_SPEED;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.v5_run_speed_ms > 0.0)
                return params.v5_run_speed_ms;
        }
        return SCR_RSS_Constants.V5_RUN_SPEED_MS_DEFAULT;
    }

    //! 行军档 Sprint 绝对速度（m/s）；关 March 时用引擎 Sprint 顶 5.5
    static float GetMarchSprintSpeedMs()
    {
        if (!SCR_RSS_Constants.V6_USE_MARCH_GAIT_SPEEDS)
            return SCR_RSS_Constants.GAME_MAX_SPEED;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.v5_sprint_speed_ms > 0.0)
                return params.v5_sprint_speed_ms;
        }
        return SCR_RSS_Constants.V5_SPRINT_SPEED_MS_DEFAULT;
    }

    //! @deprecated 兼容别名，请用 GetMarchWalkSpeedMs
    static float GetV5WalkSpeedMs()
    {
        return GetMarchWalkSpeedMs();
    }

    //! @deprecated 兼容别名，请用 GetMarchRunSpeedMs
    static float GetV5RunSpeedMs()
    {
        return GetMarchRunSpeedMs();
    }

    //! @deprecated 兼容别名，请用 GetMarchSprintSpeedMs
    static float GetV5SprintSpeedMs()
    {
        return GetMarchSprintSpeedMs();
    }

    //! W′ 池耗尽后禁止 Sprint 的阈值（0–1）
    static float GetWPrimeSprintEnableThreshold()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.anaerobic_sprint_enable_threshold > 0.0)
                return params.anaerobic_sprint_enable_threshold;
        }
        return SCR_RSS_Constants.V5_ANAEROBIC_SPRINT_THRESHOLD_DEFAULT;
    }

    //! @deprecated 兼容别名，请用 GetWPrimeSprintEnableThreshold
    static float GetAnaerobicSprintEnableThreshold()
    {
        return GetWPrimeSprintEnableThreshold();
    }

    static float GetBurstCooldownFullSeconds()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.burst_cooldown_full_seconds > 0.0)
                return params.burst_cooldown_full_seconds;
        }
        return SCR_RSS_Constants.V5_BURST_COOLDOWN_FULL_DEFAULT;
    }

    static float GetBurstCooldownShortSeconds()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.burst_cooldown_short_seconds > 0.0)
                return params.burst_cooldown_short_seconds;
        }
        return SCR_RSS_Constants.V5_BURST_COOLDOWN_SHORT_DEFAULT;
    }

    static float GetAnaerobicRecoveryPerSec()
    {
        return GetWPrimeRecoveryWPerSec();
    }

    //! 专服无氧池补 tick 的功率估算回退（非主路径 W′ 放电）
    static float GetWPrimeDrainPerSec()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.anaerobic_drain_per_sec > 0.0)
                return params.anaerobic_drain_per_sec;
        }
        return 0.12;
    }

    //! @deprecated 兼容别名，请用 GetWPrimeDrainPerSec
    static float GetAnaerobicDrainPerSec()
    {
        return GetWPrimeDrainPerSec();
    }

    static float GetAerobicEfficiencyFactor()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.aerobic_efficiency_factor > 0.0)
                return Math.Clamp(params.aerobic_efficiency_factor, 0.5, 2.0);
        }
        return SCR_RSS_Constants.AEROBIC_EFFICIENCY_FACTOR;
    }

    static float GetWPrimeEfficiencyFactor()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.anaerobic_efficiency_factor > 0.0)
                return Math.Clamp(params.anaerobic_efficiency_factor, 0.5, 2.5);
        }
        return SCR_RSS_Constants.ANAEROBIC_EFFICIENCY_FACTOR;
    }

    //! @deprecated 兼容别名，请用 GetWPrimeEfficiencyFactor
    static float GetAnaerobicEfficiencyFactor()
    {
        return GetWPrimeEfficiencyFactor();
    }

    //! v6：预设锚点 3.5 表示「相对 P(v) 中性」；低于锚点 Sprint 更省力，高于更费力
    static float GetSprintStaminaDrainMultiplierEffective()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        float mult = 3.5;
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.sprint_stamina_drain_multiplier > 0.0)
                mult = params.sprint_stamina_drain_multiplier;
            if (settings.m_sSelectedPreset == "Custom" && settings.m_fSprintStaminaDrainMultiplier > 0.0)
                mult = mult * settings.m_fSprintStaminaDrainMultiplier;
        }
        float anchor = 3.5;
        if (anchor < 0.01)
            return 1.0;
        return Math.Clamp(mult / anchor, 0.25, 4.0);
    }

    static bool IsCustomPresetSelected()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return false;
        return settings.m_sSelectedPreset == "Custom";
    }

    static float GetCustomStaminaDrainMultiplier()
    {
        if (!IsCustomPresetSelected())
            return 1.0;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return 1.0;
        return Math.Clamp(settings.m_fStaminaDrainMultiplier, 0.1, 5.0);
    }

    static float GetCustomStaminaRecoveryMultiplier()
    {
        if (!IsCustomPresetSelected())
            return 1.0;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return 1.0;
        return Math.Clamp(settings.m_fStaminaRecoveryMultiplier, 0.1, 5.0);
    }

    static float GetCustomEncumbranceSpeedPenaltyMultiplier()
    {
        if (!IsCustomPresetSelected())
            return 1.0;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return 1.0;
        return Math.Clamp(settings.m_fEncumbranceSpeedPenaltyMultiplier, 0.1, 5.0);
    }

    static float GetCustomSprintSpeedMultiplier()
    {
        if (!IsCustomPresetSelected())
            return 1.0;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return 1.0;
        return Math.Clamp(settings.m_fSprintSpeedMultiplier, 0.5, 2.0);
    }

    static float GetCriticalPowerWatts()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.critical_power_watts > 1.0)
                return params.critical_power_watts;
            if (params && params.sustainable_watts > 1.0)
                return params.sustainable_watts;
        }
        return SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
    }

    static float GetWPrimeMaxJoules()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.w_prime_max_joules > 1.0)
                return params.w_prime_max_joules;
        }
        return SCR_RSS_Constants.V6_W_PRIME_MAX_JOULES_DEFAULT;
    }

    static float GetWPrimeRecoveryWPerSec()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.w_prime_recovery_w_per_s > 0.0)
                return params.w_prime_recovery_w_per_s;
            if (params && params.anaerobic_recovery_per_sec > 0.0)
                return params.anaerobic_recovery_per_sec;
        }
        return SCR_RSS_Constants.V6_W_PRIME_RECOVERY_W_PER_S_DEFAULT;
    }

    static float GetSprintPowerCapWatts()
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings)
        {
            SCR_RSS_Params params = settings.GetActiveParams();
            if (params && params.sprint_power_cap_watts > 1.0)
                return params.sprint_power_cap_watts;
        }
        return SCR_RSS_Constants.V6_SPRINT_POWER_CAP_WATTS_DEFAULT;
    }

}
