// 体力恢复计算模块
// 负责计算体力恢复率（多维度恢复模型、EPOC延迟等）
// 模块化拆分：从 PlayerBase.c 提取的体力恢复计算逻辑

class StaminaRecoveryCalculator
{
    // ==================== EPOC延迟管理 ====================
    
    // 更新EPOC延迟状态
    // @param epocState EPOC状态对象（用于修改状态）
    // @param currentSpeed 当前速度 (m/s)
    // @param currentWorldTime 当前世界时间
    // @return 是否处于EPOC延迟期间
    static bool UpdateEpocDelay(
        EpocState epocState,
        float currentSpeed,
        float currentWorldTime)
    {
        if (!epocState)
            return false;
        
        float lastSpeedForEpoc = epocState.GetLastSpeedForEpoc();
        bool wasMoving = (lastSpeedForEpoc > 0.05);
        bool isNowStopped = (currentSpeed <= 0.05);
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
            if (epocDelayDuration >= StaminaConstants.EPOC_DELAY_SECONDS)
            {
                epocState.SetIsInEpocDelay(false);
                epocState.SetEpocDelayStartTime(-1.0);
            }
        }
        
        // 如果重新开始运动，取消EPOC延迟
        if (currentSpeed > 0.05)
        {
            epocState.SetIsInEpocDelay(false);
            epocState.SetEpocDelayStartTime(-1.0);
        }
        
        // 更新上一帧的速度
        epocState.SetLastSpeedForEpoc(currentSpeed);
        
        return epocState.IsInEpocDelay();
    }
    
    // 计算EPOC延迟期间的消耗
    // @param speedBeforeStop 停止前的速度 (m/s)
    // @return EPOC消耗率（每0.2秒）
    static float CalculateEpocDrainRate(float speedBeforeStop)
    {
        float epocDrainRate = StaminaConstants.EPOC_DRAIN_RATE;
        float speedRatioForEpoc = Math.Clamp(speedBeforeStop / 5.2, 0.0, 1.0);
        epocDrainRate = epocDrainRate * (1.0 + speedRatioForEpoc * 0.5); // 最多增加50%
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
    // @return 恢复率（每0.2秒）
    static float CalculateRecoveryRate(
        float staminaPercent,
        float restDurationMinutes,
        float exerciseDurationMinutes,
        float currentWeightForRecovery,
        float baseDrainRateByVelocity,
        bool disablePositiveRecovery)
    {
        float recoveryRate = RealisticStaminaSpeedSystem.CalculateMultiDimensionalRecoveryRate(
            staminaPercent, 
            restDurationMinutes, 
            exerciseDurationMinutes,
            currentWeightForRecovery
        );
        
        // 关键兜底（仅在明确禁止恢复的场景启用，例如水中）：
        // 禁止任何正向恢复，避免“静止踩水回血”等不合理情况。
        if (disablePositiveRecovery)
            return -Math.Max(baseDrainRateByVelocity, 0.0);
        
        // 陆地静止：从恢复率中减去静态消耗（如果存在），允许净恢复。
        if (baseDrainRateByVelocity > 0.0)
            recoveryRate = Math.Max(recoveryRate - baseDrainRateByVelocity, -0.01);
        
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
            currentWeightForRecovery = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
        }
        
        return currentWeightForRecovery;
    }
}
