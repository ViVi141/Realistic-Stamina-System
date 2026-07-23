//! 体力净变化率 / ETA / 恢复上下文（从 SCR_RSS_UpdateCoordinator.c 拆分）

class SCR_RSS_StaminaNetRate
{
    static RecoveryContext BuildRecoveryContext(
        bool inVehicle,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EnvironmentFactor environmentFactor,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        float currentSpeed)
    {
        RecoveryContext ctx = new RecoveryContext();

        if (inVehicle)
        {
            ctx.currentWeightForRecovery = 0.0;
            ctx.staticDrainForRecovery = -SCR_RSS_Constants.REST_RECOVERY_PER_TICK;
            ctx.stanceInt = 2;
            ctx.envFactor = null;
            ctx.speedForRecovery = 0.0;
            ctx.heatPenalty = 1.0;
        }
        else
        {
            float currentWeightForRecovery = 0.0;
            if (encumbranceCache && encumbranceCache.IsCacheValid())
                currentWeightForRecovery = encumbranceCache.GetCurrentWeight();
            ctx.currentWeightForRecovery = SCR_RSS_RecoveryCalculator.CalculateRecoveryWeight(
                currentWeightForRecovery, controller);

            ctx.staticDrainForRecovery = baseDrainRateByVelocity;
            if (baseDrainRateByVelocityForModule > 0.0)
                ctx.staticDrainForRecovery = baseDrainRateByVelocityForModule;

            ECharacterStance stanceForRecovery = controller.GetStance();
            if (stanceForRecovery == ECharacterStance.PRONE)
                ctx.staticDrainForRecovery = 0.0;

            if (stanceForRecovery == ECharacterStance.PRONE)
                ctx.stanceInt = 2;
            else if (stanceForRecovery == ECharacterStance.CROUCH)
                ctx.stanceInt = 1;
            else
                ctx.stanceInt = 0;

            ctx.envFactor = environmentFactor;
            ctx.speedForRecovery = currentSpeed;
            ctx.heatPenalty = 1.0;
        }

        ctx.restDurationMinutes = 0.0;
        ctx.exerciseDurationMinutes = 0.0;
        if (exerciseTracker)
        {
            ctx.restDurationMinutes = exerciseTracker.GetRestDurationMinutes();
            ctx.exerciseDurationMinutes = exerciseTracker.GetExerciseDurationMinutes();
        }

        return ctx;
    }

    static float ResolveMovementDrainForNet(
        bool useSwimmingModel,
        float currentSpeed,
        float totalDrainRate)
    {
        if (useSwimmingModel)
            return totalDrainRate;
        if (currentSpeed < SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            return 0.0;
        return totalDrainRate;
    }

    static float ComputeRecoveryRatePerTick(
        float staminaPercent,
        float currentSpeed,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        SCR_RSS_EpocState epocState,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EnvironmentFactor environmentFactor,
        bool inVehicle)
    {
        bool isInEpocDelay = false;
        if (epocState)
            isInEpocDelay = epocState.IsInEpocDelay();

        if (isInEpocDelay && !inVehicle)
            return 0.0;

        RecoveryContext ctx = BuildRecoveryContext(
            inVehicle,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            baseDrainRateByVelocity,
            baseDrainRateByVelocityForModule,
            heatStressMultiplier,
            currentSpeed);

        float recoveryRate = SCR_RSS_RecoveryCalculator.CalculateRecoveryRate(
            staminaPercent,
            ctx.restDurationMinutes,
            ctx.exerciseDurationMinutes,
            ctx.currentWeightForRecovery,
            ctx.staticDrainForRecovery,
            false,
            ctx.stanceInt,
            ctx.envFactor,
            ctx.speedForRecovery,
            0,
            controller);

        return recoveryRate * ctx.heatPenalty * SCR_RSS_ConfigBridge.GetCustomStaminaRecoveryMultiplier();
    }

    static float ComputeFinalDrainRatePerTick(
        bool useSwimmingModel,
        float currentSpeed,
        float totalDrainRate,
        SCR_RSS_EpocState epocState,
        bool inVehicle)
    {
        float epocDrainRate = 0.0;
        bool isInEpocDelay = false;
        if (epocState)
            isInEpocDelay = epocState.IsInEpocDelay();

        if (isInEpocDelay && !inVehicle)
        {
            float peakP = -1.0;
            float speedBeforeStop = epocState.GetSpeedBeforeStop();
            peakP = epocState.GetPeakPowerWatts();
            if (peakP <= 1.0)
                peakP = epocState.GetLastPowerWatts();
            epocDrainRate = SCR_RSS_RecoveryCalculator.CalculateEpocDrainRate(speedBeforeStop, peakP);
        }

        float movementDrainRate = ResolveMovementDrainForNet(useSwimmingModel, currentSpeed, totalDrainRate);
        return movementDrainRate + epocDrainRate;
    }

    static StaminaEtaResult ComputeStaminaEta(
        float staminaPercent,
        float targetStaminaCap,
        bool useSwimmingModel,
        float currentSpeed,
        float totalDrainRate,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        SCR_RSS_EpocState epocState,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EnvironmentFactor environmentFactor,
        float capShrinkPerSec)
    {
        StaminaEtaResult result = new StaminaEtaResult();
        result.timeToDepleteSec = -1.0;
        result.timeToFullSec = -1.0;

        float recoveryPerSec = ComputeRecoveryRatePerTick(
            staminaPercent,
            currentSpeed,
            baseDrainRateByVelocity,
            baseDrainRateByVelocityForModule,
            heatStressMultiplier,
            epocState,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            false) * 5.0;

        float drainPerSec = ComputeFinalDrainRatePerTick(
            useSwimmingModel,
            currentSpeed,
            totalDrainRate,
            epocState,
            false) * 5.0;

        if (drainPerSec > recoveryPerSec)
        {
            float netLossPerSec = drainPerSec - recoveryPerSec;
            float effectiveLossPerSec = netLossPerSec;
            if (capShrinkPerSec > 0.0)
            {
                float capGap = staminaPercent - targetStaminaCap;
                if (Math.AbsFloat(capGap) < 0.02 || staminaPercent > targetStaminaCap)
                    effectiveLossPerSec = netLossPerSec + capShrinkPerSec;
            }
            if (effectiveLossPerSec > 0.0)
                result.timeToDepleteSec = Math.Min(staminaPercent / effectiveLossPerSec, 7200.0);
        }
        else if (recoveryPerSec > drainPerSec && staminaPercent < targetStaminaCap - 0.001)
        {
            if (currentSpeed < SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            {
                result.timeToFullSec = EstimateRecoveryTimeToFull(
                    staminaPercent,
                    targetStaminaCap,
                    totalDrainRate,
                    baseDrainRateByVelocity,
                    baseDrainRateByVelocityForModule,
                    heatStressMultiplier,
                    epocState,
                    encumbranceCache,
                    exerciseTracker,
                    controller,
                    environmentFactor,
                    useSwimmingModel);
            }
            else
            {
                float netGainPerSec = recoveryPerSec - drainPerSec;
                if (netGainPerSec > 0.000001)
                {
                    float remaining = targetStaminaCap - staminaPercent;
                    result.timeToFullSec = Math.Min(remaining / netGainPerSec, 7200.0);
                }
            }
        }

        return result;
    }

    static float GetNetStaminaRatePerSecond(
        float staminaPercent,
        bool useSwimmingModel,
        float currentSpeed,
        float totalDrainRate,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        SCR_RSS_EpocState epocState,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EnvironmentFactor environmentFactor = null,
        bool inVehicle = false)
    {
        float recoveryRate = ComputeRecoveryRatePerTick(
            staminaPercent,
            currentSpeed,
            baseDrainRateByVelocity,
            baseDrainRateByVelocityForModule,
            heatStressMultiplier,
            epocState,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            inVehicle);

        float finalDrainRate = ComputeFinalDrainRatePerTick(
            useSwimmingModel,
            currentSpeed,
            totalDrainRate,
            epocState,
            inVehicle);

        float netRatePerTick = recoveryRate - finalDrainRate;
        return netRatePerTick * 5.0;
    }

    static float EstimateRecoveryTimeToFull(
        float staminaPercent,
        float targetStamina,
        float totalDrainRate,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        SCR_RSS_EpocState epocState,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EnvironmentFactor environmentFactor = null,
        bool useSwimmingModel = false)
    {
        if (targetStamina <= staminaPercent + 0.001)
            return 0.0;

        const int SEGMENTS = 10;
        float range = targetStamina - staminaPercent;
        float step = range / SEGMENTS;

        bool isInEpocDelay = false;
        if (epocState)
            isInEpocDelay = epocState.IsInEpocDelay();

        RecoveryContext ctx = BuildRecoveryContext(
            false,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            baseDrainRateByVelocity,
            baseDrainRateByVelocityForModule,
            heatStressMultiplier,
            0.0);

        float totalTime = 0.0;

        float fixedFinalDrain = ResolveMovementDrainForNet(useSwimmingModel, 0.0, totalDrainRate);
        if (isInEpocDelay)
        {
            float peakP = -1.0;
            if (epocState)
            {
                peakP = epocState.GetPeakPowerWatts();
                if (peakP <= 1.0)
                    peakP = epocState.GetLastPowerWatts();
            }
            fixedFinalDrain = fixedFinalDrain + SCR_RSS_RecoveryCalculator.CalculateEpocDrainRate(
                epocState.GetSpeedBeforeStop(), peakP);
        }

        for (int i = 0; i < SEGMENTS; i++)
        {
            float segStart = staminaPercent + i * step;
            float segMid = segStart + step * 0.5;

            float segRecovery = 0.0;
            if (!isInEpocDelay)
            {
                segRecovery = SCR_RSS_RecoveryCalculator.CalculateRecoveryRate(
                    segMid,
                    ctx.restDurationMinutes,
                    ctx.exerciseDurationMinutes,
                    ctx.currentWeightForRecovery,
                    ctx.staticDrainForRecovery,
                    false,
                    ctx.stanceInt,
                    ctx.envFactor,
                    ctx.speedForRecovery,
                    0,
                    controller);

                segRecovery = segRecovery * ctx.heatPenalty;
            }

            float segNet = segRecovery - fixedFinalDrain;

            if (segNet > 0.00001)
                totalTime += (step / segNet) * SCR_RSS_Constants.RSS_STAMINA_TICK_SEC;
            else
                totalTime += 300.0;
        }

        return Math.Min(totalTime, 7200.0);
    }
}
