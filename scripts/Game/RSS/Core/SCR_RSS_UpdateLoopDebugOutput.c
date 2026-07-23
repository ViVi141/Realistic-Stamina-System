//! Player/AI stamina debug output (split from PlayerBase_UpdateLoop.c for ICE relief)

class SCR_RSS_UpdateLoopDebugOutput
{
    static void OutputPlayerStaminaAndHints(
        SCR_CharacterControllerComponent ctrl,
        IEntity ownerEnt,
        RSS_StaminaDebugOutputParams tick,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_FatigueSystem fatigueSystem,
        SCR_RSS_EpocState epocState,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_RSS_EnvironmentFactor environmentFactor,
        SCR_RSS_TerrainDetector terrainDetector,
        SCR_RSS_StanceTransitionManager stanceTransitionManager,
        float swimmingWetWeight,
        string speedSource,
        float anaerobicPercent,
        float sprintCooldownSec)
    {
        if (!ownerEnt || ownerEnt != SCR_PlayerController.GetLocalControlledEntity())
            return;
        if (!tick || !ctrl)
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        bool needDebugOutput = SCR_RSS_DebugBatchManager.IsDebugBatchActive();
        bool needHintOutput = false;
        if (settings && settings.m_bHintDisplayEnabled)
            needHintOutput = true;
        if (!needDebugOutput && !needHintOutput)
            return;

        float debugCurrentWeight = 0.0;
        float combatEncumbrancePercent = 0.0;
        if (encumbranceCache && encumbranceCache.IsCacheValid())
        {
            debugCurrentWeight = encumbranceCache.GetCurrentWeight();
            combatEncumbrancePercent = SCR_RSS_MetabolismMath.CalculateCombatEncumbrancePercent(ownerEnt);
        }

        float timeToDepleteSec = -1.0;
        float timeToFullSec = -1.0;
        if (needHintOutput)
        {
            float targetStamina = 1.0;
            if (fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
                targetStamina = fatigueSystem.GetMaxStaminaCap();

            StaminaEtaResult eta = SCR_RSS_StaminaNetRate.ComputeStaminaEta(
                tick.staminaPercent,
                targetStamina,
                tick.useSwimmingModel,
                tick.currentSpeed,
                tick.totalDrainRate,
                tick.baseDrainRateByVelocity,
                tick.baseDrainRateByVelocityForModule,
                tick.heatStressMultiplier,
                epocState,
                encumbranceCache,
                exerciseTracker,
                ctrl,
                environmentFactor,
                tick.capShrinkPerSec);
            if (eta)
            {
                timeToDepleteSec = eta.timeToDepleteSec;
                timeToFullSec = eta.timeToFullSec;
            }
        }

        tick.timeToDepleteSec = timeToDepleteSec;
        tick.timeToFullSec = timeToFullSec;

        if (needDebugOutput || needHintOutput)
        {
            SCR_RSS_DebugDisplay.OutputStaminaDrainDiagnostics(
                tick,
                ctrl,
                epocState,
                encumbranceCache,
                exerciseTracker,
                environmentFactor);
        }

        DebugInfoParams debugParams = new DebugInfoParams();
        debugParams.owner = ownerEnt;
        debugParams.movementTypeStr = SCR_RSS_SpeedCalculator.FormatMovementTypeForDisplay(
            tick.isSprinting,
            tick.currentMovementPhase,
            tick.effectiveMovementPhase,
            tick.currentSpeed);
        debugParams.staminaPercent = tick.staminaPercent;
        debugParams.baseSpeedMultiplier = tick.baseSpeedMultiplier;
        debugParams.encumbranceSpeedPenalty = tick.encumbranceSpeedPenalty;
        debugParams.finalSpeedMultiplier = tick.finalSpeedMultiplier;
        debugParams.gradePercent = tick.gradePercent;
        if (tick.isSwimming)
            debugParams.slopeAngleDegrees = 0.0;
        else
            debugParams.slopeAngleDegrees = tick.slopeAngleDegrees;
        debugParams.isSprinting = tick.isSprinting;
        debugParams.currentMovementPhase = tick.currentMovementPhase;
        debugParams.debugCurrentWeight = debugCurrentWeight;
        debugParams.combatEncumbrancePercent = combatEncumbrancePercent;
        debugParams.terrainDetector = terrainDetector;
        debugParams.environmentFactor = environmentFactor;
        debugParams.heatStressMultiplier = tick.heatStressMultiplier;
        debugParams.rainWeight = tick.rainWeight;
        debugParams.swimmingWetWeight = swimmingWetWeight;
        debugParams.currentSpeed = tick.currentSpeed;
        debugParams.isSwimming = tick.isSwimming;
        debugParams.stanceTransitionManager = stanceTransitionManager;
        debugParams.timeToDepleteSec = timeToDepleteSec;
        debugParams.timeToFullSec = timeToFullSec;
        debugParams.speedSource = speedSource;
        debugParams.anaerobicPercent = anaerobicPercent;
        debugParams.sprintCooldownSec = sprintCooldownSec;
        debugParams.burstCooldownFullSec = SCR_RSS_ConfigBridge.GetBurstCooldownFullSeconds();
        debugParams.maxStaminaCap = tick.maxStaminaCap;
        debugParams.fatigueIntegralNorm = tick.fatigueIntegralNorm;
        debugParams.metabolismPowerW = tick.metabolismPowerW;
        debugParams.metabolismPowerMetW = tick.metabolismPowerMetW;
        debugParams.metabolismPowerRawW = tick.metabolismPowerRawW;
        debugParams.effectiveCpW = tick.effectiveCpW;
        debugParams.aerobicPowerW = tick.aerobicPowerW;
        debugParams.totalDrainPerTick = tick.totalDrainRate;
        debugParams.finalDrainPerTick = tick.finalDrainRate;
        debugParams.metabolicNetPerTick = tick.metabolicNetPerTick;
        debugParams.capRatchetPerTick = tick.capRatchetPerTick;
        debugParams.netStaminaPerTick = tick.netStaminaPerTick;
        if (needDebugOutput)
            SCR_RSS_DebugDisplay.OutputDebugInfo(debugParams);
        if (needHintOutput)
            SCR_RSS_DebugDisplay.OutputHintInfo(debugParams);
    }

    //! AI tick debug (kept out of modded PlayerBase fragments for link reliability)
    static void LogAiStaminaTick(
        IEntity owner,
        float staminaPercent,
        float currentWeight,
        float finalSpeedMultiplier,
        float currentSpeed,
        bool isSprinting,
        int currentMovementPhase,
        SCR_RSS_AIManager aiManager,
        SCR_RSS_FatigueSystem fatigueSystem,
        string speedSource)
    {
        if (!owner || !SCR_PlayerBaseConfigHelper.IsRssDebugEnabled())
            return;
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            return;

        float nowMs = GetGame().GetWorld().GetWorldTime();
        float aiDebugLastPrint = -1.0;
        if (aiManager)
            aiDebugLastPrint = aiManager.GetDebugLastPrintTime();
        if (aiDebugLastPrint >= 0.0 && (nowMs - aiDebugLastPrint) < 30000.0)
            return;

        if (aiManager)
            aiManager.SetDebugLastPrintTime(nowMs);

        string movementStr = SCR_RSS_DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
        ERSS_AIStaminaState aiState = ERSS_AIStaminaState.FRESH;
        if (aiManager)
            aiState = aiManager.GetStaminaState();
        string stateStr = SCR_RSS_AIStaminaState.StateToString(aiState);
        float fatigueVal = 0.0;
        if (fatigueSystem)
            fatigueVal = fatigueSystem.GetFatigueAccumulation();

        PrintFormat("[RSS] AI: %1 | 状态=%2 体力=%3%% 疲劳=%4% 负重=%5kg 速度倍=%6 速度=%7m/s %8 | %9",
            owner.GetName(),
            stateStr,
            Math.Round(staminaPercent * 100.0).ToString(),
            Math.Round(fatigueVal * 100.0).ToString(),
            Math.Round(currentWeight * 10.0) / 10.0,
            Math.Round(finalSpeedMultiplier * 100.0) / 100.0,
            Math.Round(currentSpeed * 10.0) / 10.0,
            movementStr,
            speedSource);

        if (!Replication.IsServer())
            return;

        AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiCtrl)
            return;
        AIAgent agent = aiCtrl.GetAIAgent();
        if (!agent)
            return;
        AIGroup parentGroup = agent.GetParentGroup();
        if (!parentGroup)
            return;
        SCR_AIGroup scrGrp = SCR_AIGroup.Cast(parentGroup);
        if (!scrGrp)
            return;
        float spread = SCR_RSS_AIUpdateInterval.CalcAiGroupSpreadM(scrGrp);
        if (spread <= 0.0)
            return;
        PrintFormat("[RSS] Group: id=%1 分散=%2m 成员=%3",
            scrGrp.GetGroupID().ToString(),
            Math.Round(spread * 10.0) / 10.0,
            SCR_RSS_AIUpdateInterval.GetAliveMemberCount(scrGrp).ToString());
    }
}
