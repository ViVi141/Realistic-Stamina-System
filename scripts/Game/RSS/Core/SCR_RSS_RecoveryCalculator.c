// 体力恢复计算模块
// 负责计算体力恢复率（多维度恢复模型、EPOC延迟等）
// 模块化拆分：从 PlayerBase.c 提取的体力恢复计算逻辑

class SCR_RSS_RecoveryCalculator
{
    // ==================== EPOC延迟管理 ====================
    
    // 更新EPOC延迟状态
    // @param epocState EPOC状态对象（用于修改状态）
    // @param currentSpeed 当前速度 (m/s)
    // @param currentWorldTime 当前世界时间
    // @return 是否处于EPOC延迟期间
    static bool UpdateEpocDelay(
        SCR_RSS_EpocState epocState,
        float currentSpeed,
        float currentWorldTime)
    {
        if (!epocState)
            return false;
        
        float lastSpeedForEpoc = epocState.GetLastSpeedForEpoc();
        float idleThreshold = SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS;
        bool wasMoving = (lastSpeedForEpoc >= idleThreshold);
        bool isNowStopped = (currentSpeed < idleThreshold);
        bool isInEpocDelay = epocState.IsInEpocDelay();
        
        // 如果从运动状态变为静止状态，启动EPOC延迟
        if (wasMoving && isNowStopped && !isInEpocDelay)
        {
            epocState.SetEpocDelayStartTime(currentWorldTime);
            epocState.SetIsInEpocDelay(true);
            epocState.SetSpeedBeforeStop(lastSpeedForEpoc);
        }
        
        // 检查EPOC延迟是否已结束
        if (isInEpocDelay)
        {
            float epocDelayStartTime = epocState.GetEpocDelayStartTime();
            float epocDelayDuration = currentWorldTime - epocDelayStartTime;
            if (epocDelayDuration >= SCR_RSS_Constants.EPOC_DELAY_SECONDS)
            {
                epocState.SetIsInEpocDelay(false);
                epocState.SetEpocDelayStartTime(-1.0);
                epocState.ResetPeakPowerForNewRest();
            }
        }
        
        // 重新移动才取消 EPOC（与 RSS_IDLE_SPEED_THRESHOLD_MPS 对齐，避免微动闪烁）
        if (currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
        {
            epocState.SetIsInEpocDelay(false);
            epocState.SetEpocDelayStartTime(-1.0);
        }
        
        // 更新上一帧的速度
        epocState.SetLastSpeedForEpoc(currentSpeed);
        
        return epocState.IsInEpocDelay();
    }
    
    // 计算EPOC延迟期间的消耗（v6：与峰值「限速内」代谢功率成正比，并封顶）
    static float CalculateEpocDrainRate(float speedBeforeStop, float peakPowerWatts = -1.0)
    {
        float epocDrainRate = SCR_RSS_Constants.EPOC_DRAIN_PER_TICK;
        if (peakPowerWatts > 1.0)
        {
            float cp = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
            if (cp <= 1.0)
                cp = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
            float excess = Math.Max(peakPowerWatts - cp, 0.0);
            float ratio = excess / cp;
            if (ratio > SCR_RSS_Constants.EPOC_MAX_POWER_EXCESS_RATIO)
                ratio = SCR_RSS_Constants.EPOC_MAX_POWER_EXCESS_RATIO;
            epocDrainRate = epocDrainRate * (1.0 + ratio);
        }
        else
        {
            float speedRatioForEpoc = Math.Clamp(speedBeforeStop / SCR_RSS_MetabolismMath.GAME_MAX_SPEED, 0.0, 1.0);
            epocDrainRate = epocDrainRate * (1.0 + speedRatioForEpoc * 0.5);
        }
        return epocDrainRate;
    }
    
    // ==================== 恢复率计算 ====================
    
    // 计算多维度恢复率
    // @param staminaPercent 当前体力百分比
    // @param restDurationMinutes 休息持续时间（分钟）
    // @param exerciseDurationMinutes 运动持续时间（分钟）
    // @param currentWeightForRecovery 恢复计算用的当前重量（考虑姿态优化）
    // @param baseDrainRateByVelocity 基础消耗率（用于静态站立消耗）
    // @param disablePositiveRecovery 是否禁止正向恢复（例如：水中/踩水等场景）
    // @param stance 当前姿态（0=站立，1=蹲姿，2=趴姿）
    // @param environmentFactor 环境因子模块引用（v2.14.0新增）
    // @param currentSpeed 当前速度 (m/s) - 新增参数用于静态保护逻辑
    // @param rssCtrlForCaffeine 若处于苯甲酸钠咖啡因药效期，则跳过天气类恢复惩罚
    // @return 恢复率（每0.2秒）
    static float CalculateRecoveryRate(
        float staminaPercent,
        float restDurationMinutes,
        float exerciseDurationMinutes,
        float currentWeightForRecovery,
        float baseDrainRateByVelocity,
        bool disablePositiveRecovery,
        int stance = 0,
        SCR_RSS_EnvironmentFactor environmentFactor = null,
        float currentSpeed = 0.0,
        int movementPhase = 0,
        SCR_CharacterControllerComponent rssCtrlForCaffeine = null)
    {
        float recoveryRate = SCR_RSS_MetabolismMath.CalculateMultiDimensionalRecoveryRate(
            staminaPercent, 
            restDurationMinutes, 
            exerciseDurationMinutes,
            currentWeightForRecovery,
            stance
        );
        
        // ==================== v2.14.0 环境因子修正 ====================
        
        bool skipWeatherRecoveryPenalties = false;
        if (rssCtrlForCaffeine && rssCtrlForCaffeine.RSS_IsCaffeineSodiumBenzoateActive())
            skipWeatherRecoveryPenalties = true;

        // 获取高级环境因子（如果环境因子模块存在）
        float heatStressPenalty = 0.0;
        float coldStressPenalty = 0.0;
        float surfaceWetnessPenalty = 0.0;
        
        if (!skipWeatherRecoveryPenalties && environmentFactor)
        {
            heatStressPenalty = environmentFactor.GetHeatStressPenalty();
            coldStressPenalty = environmentFactor.GetColdStressPenalty();
            surfaceWetnessPenalty = environmentFactor.GetSurfaceWetnessPenalty();
        }
        
        // 应用热应激惩罚（降低恢复率）
        recoveryRate = recoveryRate * (1.0 - heatStressPenalty);
        
        // 应用冷应激惩罚（降低恢复率）
        recoveryRate = recoveryRate * (1.0 - coldStressPenalty);
        
        // 应用地表湿度惩罚（趴下时的恢复惩罚）
        if (stance == 2) // 趴姿
        {
            recoveryRate = recoveryRate * (1.0 - surfaceWetnessPenalty);
        }
        
        // ==================== 运动状态恢复率调整 ====================
        // v6：陆地移动时不计入有氧恢复（net = -movementDrain）；仅静止/EPOC 后恢复。
        // 与 ResolveMovementDrainForNet 阈值对齐；绝境呼吸(<2%) 仍于下方兜底。
        float speedBasedRecoveryMultiplier = 1.0;
        if (currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            speedBasedRecoveryMultiplier = 0.0;

        recoveryRate = recoveryRate * speedBasedRecoveryMultiplier;
        
        // 关键兜底（仅在明确禁止恢复的场景启用，例如水中）：
        // 禁止任何正向恢复，避免"静止踩水回血"等不合理情况。
        if (disablePositiveRecovery)
            return -Math.Max(baseDrainRateByVelocity, 0.0);
        
        // ==================== [修复 v3.6.1 增强版] 保护机制（按优先级顺序执行）====================
        // 保护机制说明：以下两个保护逻辑按顺序执行，确保合理的恢复行为
        //
        // 1. 绝境呼吸保护（高优先级）：
        //    - 触发条件：体力 < 2%
        //    - 作用：强制恢复率至少为0.0001，打破"0体力死锁"
        //    - 生理学依据：极度疲劳时，人体进入求生本能，强制吸氧恢复
        //
        // 2. 静态保护（低优先级）：
        //    - 触发条件：静止 + 重量<40kg + 恢复率<0.00005
        //    - 作用：强制恢复率为0.0001，确保静止时缓慢恢复
        //    - 设计意图：防止"静止不回血"的糟糕体验
        //
        // 执行顺序的重要性：
        // - 如果体力<2%，绝境呼吸保护会先执行，设置recoveryRate >= 0.0001
        // - 然后静态保护检查recoveryRate < 0.00005，但此时recoveryRate已经是0.0001，不会触发
        // - 这个顺序确保了绝境呼吸保护的优先级更高，且两个逻辑不会冲突
        //
        // ==================== 绝境呼吸保护机制 (Desperation Wind) ====================
        // 当体力极低 (<2%) 且非水下时，人体进入求生本能强制吸氧
        // 此时忽略背包重量的静态消耗，保证哪怕一丝微弱的体力恢复，打破死锁
        if (staminaPercent < 0.02)
        {
            // 求生保底：当体力低于2%时，忽略所有负重和环境导致的静态消耗
            // 强制返回至少为0.0001的正向恢复值，确保0体力不会死锁
            recoveryRate = Math.Max(recoveryRate, 0.0001);

            // 双重保护：如果上述Math.Max没有生效，强制设置为0.0001
            if (recoveryRate < 0.0001)
                recoveryRate = 0.0001;
        }
        // 正常陆地静止：处理静态消耗或恢复
        // 注意：baseDrainRateByVelocity 为负数表示恢复，正数表示消耗
        // [修复 fix2.md #1] 移除了减去消耗的逻辑，只返回总恢复率（Gross Recovery）
        // 让协调器（SCR_StaminaUpdateCoordinator）去计算净值：netChange = recoveryRate - finalDrainRate
        // 避免了消耗被双重扣除的问题
        if (baseDrainRateByVelocity < 0.0)
        {
            // v6：负基线不再叠加到 recovery（静止恢复仅由多维恢复模型 + ResolveMovementDrainForNet 处理）
        }
        else if (baseDrainRateByVelocity > 0.0 && currentSpeed < 0.1)
        {
            // ==================== 静态消耗处理 ====================
            // 只有在静态站立时（currentSpeed < 0.1），才从恢复率中减去静态消耗
            // 移动时（currentSpeed >= 0.1），不减去消耗，因为消耗已经在finalDrainRate中计算了
            // 参考：rss_digital_twin_fix.py 第421-428行
            float adjustedRecoveryRate = recoveryRate - baseDrainRateByVelocity;
            if (adjustedRecoveryRate < 0.00005)
            {
                recoveryRate = 0.00005;  // 强制设置最小值（与优化器一致）
            }
            else
            {
                recoveryRate = adjustedRecoveryRate;
            }
        }

        // ==================== 静态保护逻辑 ====================
        // 确保除非玩家严重超载(>40kg)，否则静止时总是缓慢恢复体力
        // 注意：此逻辑在绝境呼吸保护之后执行，因此不会覆盖绝境保护的效果
        if (currentSpeed < 0.1 && // 静止不动
            currentWeightForRecovery < 40.0 && // 重量在合理范围内
            recoveryRate < 0.00005) // 只有当恢复率过低时才触发（绝境保护后recoveryRate>=0.0001，不会触发）
        {
            recoveryRate = 0.0001;
        }

        float maxRecoveryPerTick = SCR_RSS_ConfigBridge.GetMaxRecoveryPerTick();
        if (maxRecoveryPerTick > 0.0 && recoveryRate > maxRecoveryPerTick)
        {
            recoveryRate = maxRecoveryPerTick;
        }
        
        return recoveryRate;
    }
    
    // 计算恢复用的重量（考虑姿态优化）
    // @param currentWeight 当前重量 (kg)
    // @param controller 角色控制器组件（用于获取姿态）
    // @return 恢复计算用的重量
    static float CalculateRecoveryWeight(float currentWeight, SCR_CharacterControllerComponent controller)
    {
        float currentWeightForRecovery = currentWeight;
        
        // 趴下休息时的负重优化
        ECharacterStance currentStance = controller.GetStance();
        if (currentStance == ECharacterStance.PRONE)
        {
            // 如果角色趴下，将负重视为基准重量，去除额外负重的影响
            currentWeightForRecovery = SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        }
        
        return currentWeightForRecovery;
    }
}
