// ============================================================
// SCR_RecoveryModel.c
// 职责: 深度生理压制恢复模型——多维恢复计算
// 所属域: Core
// ============================================================
// 提取自: SCR_RealisticStaminaSystem.c (CalculateMultiDimensionalRecoveryRate)
// ============================================================

class RecoveryModel
{
    // 计算多维恢复率：整合姿态、休息时间、疲劳、年龄、负重压制
    // @param staminaPercent 当前体力百分比 (0.0-1.0)
    // @param restDurationMinutes 休息持续分钟数
    // @param exerciseDurationMinutes 运动持续分钟数
    // @param currentWeight 当前负重 (kg)，默认0
    // @param stance 当前姿态 (0=站立, 1=蹲姿, 2=趴姿)，默认0
    // @return 恢复率（每0.2秒）
    static float CalculateRecoveryRate(
        float staminaPercent, float restDurationMinutes,
        float exerciseDurationMinutes, float currentWeight = 0.0, int stance = 0)
    {
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        restDurationMinutes = Math.Max(restDurationMinutes, 0.0);
        exerciseDurationMinutes = Math.Max(exerciseDurationMinutes, 0.0);

        // 最低体力阈值限制
        float restDurationSeconds = restDurationMinutes * 60.0;
        float minStaminaThreshold = StaminaConfigBridge.GetMinRecoveryStaminaThreshold();
        float minRestSeconds = StaminaConfigBridge.GetMinRecoveryRestTimeSeconds();
        if (staminaPercent < minStaminaThreshold && restDurationSeconds < minRestSeconds)
            return 0.0;

        // 1. 基础恢复率（非线性）：体力越低恢复越快
        float recoveryNonlinearCoeff = StaminaConfigBridge.GetRecoveryNonlinearCoeff();
        float staminaRecoveryMultiplier = 1.0 + (recoveryNonlinearCoeff * (1.0 - staminaPercent));
        float baseRecoveryRate = StaminaConfigBridge.GetBaseRecoveryRate() * staminaRecoveryMultiplier;

        // 2. 健康状态/训练水平影响（固定值，防止不平等）
        float fitnessRecoveryMultiplier = StaminaConstants.FIXED_FITNESS_RECOVERY_MULTIPLIER;

        // 3. 休息时间影响
        float restTimeMultiplier = 1.0;
        float fastRecoveryMultiplier = StaminaConfigBridge.GetFastRecoveryMultiplier();
        float mediumRecoveryMultiplier = StaminaConfigBridge.GetMediumRecoveryMultiplier();
        float slowRecoveryMultiplier = StaminaConfigBridge.GetSlowRecoveryMultiplier();

        if (restDurationMinutes <= StaminaConstants.FAST_RECOVERY_DURATION_MINUTES)
        {
            restTimeMultiplier = fastRecoveryMultiplier;
        }
        else if (restDurationMinutes <= StaminaConstants.MEDIUM_RECOVERY_START_MINUTES
            + StaminaConstants.MEDIUM_RECOVERY_DURATION_MINUTES)
        {
            restTimeMultiplier = mediumRecoveryMultiplier;
        }
        else if (restDurationMinutes >= StaminaConstants.SLOW_RECOVERY_START_MINUTES)
        {
            const float transitionDuration = 10.0;
            float transitionProgress = Math.Min(
                (restDurationMinutes - StaminaConstants.SLOW_RECOVERY_START_MINUTES) / transitionDuration, 1.0);
            restTimeMultiplier = 1.0 - (transitionProgress * (1.0 - slowRecoveryMultiplier));
        }

        // 4. 年龄影响（固定值，防止不平等）
        float ageRecoveryMultiplier = StaminaConstants.FIXED_AGE_RECOVERY_MULTIPLIER;

        // 5. 累积疲劳恢复影响
        float fatigueRecoveryPenalty = StaminaConstants.FATIGUE_RECOVERY_PENALTY
            * Math.Min(exerciseDurationMinutes / StaminaConstants.FATIGUE_RECOVERY_DURATION_MINUTES, 1.0);
        float fatigueRecoveryMultiplier = Math.Clamp(1.0 - fatigueRecoveryPenalty, 0.7, 1.0);

        // 6. 姿态恢复加成
        float stanceRecoveryMultiplier = 1.0;
        switch (stance)
        {
            case 0: stanceRecoveryMultiplier = StaminaConfigBridge.GetStandingRecoveryMultiplier(); break;
            case 1: stanceRecoveryMultiplier = StaminaConfigBridge.GetCrouchingRecoveryMultiplier(); break;
            case 2: stanceRecoveryMultiplier = StaminaConfigBridge.GetProneRecoveryMultiplier(); break;
            default: stanceRecoveryMultiplier = StaminaConfigBridge.GetStandingRecoveryMultiplier(); break;
        }

        // 7. 负重恢复惩罚
        float loadRecoveryPenalty = 0.0;
        if (currentWeight > 0.0)
        {
            float loadRatio = Math.Clamp(currentWeight / StaminaConstants.BODY_TOLERANCE_BASE, 0.0, 2.0);
            float loadRecoveryPenaltyCoeff = StaminaConfigBridge.GetLoadRecoveryPenaltyCoeff();
            float loadRecoveryPenaltyExponent = StaminaConfigBridge.GetLoadRecoveryPenaltyExponent();
            loadRecoveryPenalty = Math.Pow(loadRatio, loadRecoveryPenaltyExponent) * loadRecoveryPenaltyCoeff;
        }

        // 8. 边际效应衰减（最后20%恢复极慢）
        float marginalDecayMultiplier = 1.0;
        float marginalDecayThreshold = StaminaConfigBridge.GetMarginalDecayThreshold();
        float marginalDecayCoeff = StaminaConfigBridge.GetMarginalDecayCoeff();
        if (staminaPercent > marginalDecayThreshold)
        {
            marginalDecayMultiplier = marginalDecayCoeff - staminaPercent;
            marginalDecayMultiplier = Math.Clamp(marginalDecayMultiplier, 0.2, 1.0);
        }

        // 综合：乘数链 + 负载压制
        float productBeforeLoad = baseRecoveryRate * fitnessRecoveryMultiplier
            * restTimeMultiplier * ageRecoveryMultiplier
            * fatigueRecoveryMultiplier * stanceRecoveryMultiplier
            * marginalDecayMultiplier;

        float loadFactor = 1.0;
        if (productBeforeLoad > 0.0)
        {
            float penaltyRatio = loadRecoveryPenalty / productBeforeLoad;
            loadFactor = Math.Max(0.2, 1.0 - penaltyRatio);
        }
        else
        {
            loadFactor = 0.00005;
        }

        return productBeforeLoad * loadFactor;
    }
}
