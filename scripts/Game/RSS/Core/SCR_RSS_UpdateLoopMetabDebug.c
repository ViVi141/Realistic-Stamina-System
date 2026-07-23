//! Metabolism / debug snapshot helpers for PlayerBase UpdateLoop (ICE relief)
//! EnforceScript: max 16 method arguments — keep helpers under that limit.

class SCR_RSS_UpdateLoopMetabDebug
{
    static void ComputeMetabDebugPowers(
        bool useSwimmingModel,
        float currentSpeed,
        float appliedSpeedLimitMs,
        float totalWeightWithWetAndBody,
        float gradePercent,
        float terrainFactor,
        int effectiveMovementPhase,
        float encumbranceSpeedPenalty,
        float drainEffectiveCpW,
        out float metabPowerDbg,
        out float metabPowerMetDbg,
        out float metabPowerRawDbg,
        out float metabCpDbg,
        out float metabAerobicDbg)
    {
        metabPowerDbg = -1.0;
        metabPowerMetDbg = -1.0;
        metabPowerRawDbg = -1.0;
        metabCpDbg = drainEffectiveCpW;
        metabAerobicDbg = -1.0;

        if (useSwimmingModel)
            return;

        float drainVelDbg = SCR_RSS_DrainCalculator.GetDrainVelocityMs(
            currentSpeed, appliedSpeedLimitMs);
        if (drainVelDbg < SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS
            && currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
        {
            float capFallback = SCR_RSS_DrainCalculator.GetTheoreticalMaxSpeedMs(
                effectiveMovementPhase, encumbranceSpeedPenalty);
            drainVelDbg = SCR_RSS_DrainCalculator.GetDrainVelocityMs(currentSpeed, capFallback);
        }
        if (drainVelDbg < SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            return;

        metabPowerMetDbg = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            drainVelDbg,
            totalWeightWithWetAndBody,
            gradePercent,
            terrainFactor,
            true,
            effectiveMovementPhase);
        if (metabCpDbg <= 1.0)
        {
            metabCpDbg = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
            if (metabCpDbg <= 1.0)
                metabCpDbg = SCR_RSS_Constants.V6_CRITICAL_POWER_WATTS_DEFAULT;
        }
        metabAerobicDbg = SCR_RSS_MetabolismModel.AerobicPowerWattsForStaminaDrain(
            metabPowerMetDbg, metabCpDbg);
        metabPowerDbg = metabAerobicDbg;
        if (currentSpeed > drainVelDbg + 0.05)
        {
            metabPowerRawDbg = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                currentSpeed,
                totalWeightWithWetAndBody,
                gradePercent,
                terrainFactor,
                true,
                effectiveMovementPhase);
        }
    }

    //! Assign core movement/drain fields (≤16 args including debugTick).
    static void FillDebugTickCore(
        RSS_StaminaDebugOutputParams debugTick,
        float staminaPercent,
        bool useSwimmingModel,
        float currentSpeed,
        float totalDrainRate,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        float baseSpeedMultiplier,
        float encumbranceSpeedPenalty,
        float finalSpeedMultiplier,
        float gradePercent,
        float slopeAngleDegrees,
        bool isSwimming,
        bool isSprinting,
        bool isSprintActive)
    {
        if (!debugTick)
            return;

        debugTick.staminaPercent = staminaPercent;
        debugTick.useSwimmingModel = useSwimmingModel;
        debugTick.currentSpeed = currentSpeed;
        debugTick.totalDrainRate = totalDrainRate;
        debugTick.baseDrainRateByVelocity = baseDrainRateByVelocity;
        debugTick.baseDrainRateByVelocityForModule = baseDrainRateByVelocityForModule;
        debugTick.heatStressMultiplier = heatStressMultiplier;
        debugTick.baseSpeedMultiplier = baseSpeedMultiplier;
        debugTick.encumbranceSpeedPenalty = encumbranceSpeedPenalty;
        debugTick.finalSpeedMultiplier = finalSpeedMultiplier;
        debugTick.gradePercent = gradePercent;
        debugTick.slopeAngleDegrees = slopeAngleDegrees;
        debugTick.isSwimming = isSwimming;
        debugTick.isSprinting = isSprinting;
        debugTick.isSprintActive = isSprintActive;
    }

    //! Assign phase/weight/metab fields (≤16 args including debugTick).
    static void FillDebugTickMetab(
        RSS_StaminaDebugOutputParams debugTick,
        int currentMovementPhase,
        int effectiveMovementPhase,
        float rainWeight,
        float maxStaCapDbg,
        float fatigueNormDbg,
        float metabPowerDbg,
        float metabPowerMetDbg,
        float metabPowerRawDbg,
        float metabCpDbg,
        float metabAerobicDbg,
        float finalDrainDbg,
        float metabolicNetDbg,
        float capRatchetPerTick,
        float netStaminaTickDbg,
        float terrainFactor)
    {
        if (!debugTick)
            return;

        debugTick.currentMovementPhase = currentMovementPhase;
        debugTick.effectiveMovementPhase = effectiveMovementPhase;
        debugTick.rainWeight = rainWeight;
        debugTick.maxStaminaCap = maxStaCapDbg;
        debugTick.fatigueIntegralNorm = fatigueNormDbg;
        debugTick.metabolismPowerW = metabPowerDbg;
        debugTick.metabolismPowerMetW = metabPowerMetDbg;
        debugTick.metabolismPowerRawW = metabPowerRawDbg;
        debugTick.effectiveCpW = metabCpDbg;
        debugTick.aerobicPowerW = metabAerobicDbg;
        debugTick.finalDrainRate = finalDrainDbg;
        debugTick.metabolicNetPerTick = metabolicNetDbg;
        debugTick.capRatchetPerTick = capRatchetPerTick;
        debugTick.netStaminaPerTick = netStaminaTickDbg;
        debugTick.terrainFactor = terrainFactor;
    }

    //! Assign speed/cap/extra fields (≤16 args including debugTick).
    static void FillDebugTickExtras(
        RSS_StaminaDebugOutputParams debugTick,
        float appliedSpeedLimitMs,
        float effectiveCriticalPowerWattsDbg,
        float timeDeltaSec,
        float totalWeightWithWetAndBody,
        float powerWattsDbg,
        float wPrimePool01Dbg,
        float landPositionDeltaSpeedMs,
        float overspeedExtraPerSec,
        float environmentMultDbg,
        float targetStaminaCap,
        float capShrinkPerSec,
        bool epocActive)
    {
        if (!debugTick)
            return;

        debugTick.appliedSpeedLimitMs = appliedSpeedLimitMs;
        debugTick.effectiveCriticalPowerWatts = effectiveCriticalPowerWattsDbg;
        debugTick.timeDeltaSec = timeDeltaSec;
        debugTick.totalWeightWithWetAndBody = totalWeightWithWetAndBody;
        debugTick.powerWatts = powerWattsDbg;
        debugTick.wPrimePool01 = wPrimePool01Dbg;
        debugTick.landPositionDeltaSpeedMs = landPositionDeltaSpeedMs;
        debugTick.overspeedExtraDrainPerSec = overspeedExtraPerSec;
        debugTick.environmentMult = environmentMultDbg;
        debugTick.targetStaminaCap = targetStaminaCap;
        debugTick.capShrinkPerSec = capShrinkPerSec;
        debugTick.epocActive = epocActive;
    }
}
