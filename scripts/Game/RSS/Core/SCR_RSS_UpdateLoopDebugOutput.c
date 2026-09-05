//! Player/AI stamina debug output (split from PlayerBase_UpdateLoop.c for ICE relief)

//! AI 限速诊断快照（避免 Enforce 单函数参数上限 16）
class RSS_AiSpeedDiagSnap
{
    string pathTag;
    float currentSpeedMs;
    float targetMs;
    float appliedFrac;
    int outPhase;
    float encPenalty;
    float gradePercent;
    float wPrime01;
    bool cruiseLatched;
    float distM;
}

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

        bool needDebugOutput = SCR_RSS_DebugBatchManager.IsDebugBatchActive();
        bool needHintOutput = SCR_RSS_StaminaHUDComponent.IsHudWanted();
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

            RSS_StaminaEtaResult eta = SCR_RSS_StaminaNetRate.ComputeStaminaEta(
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
                tick.capShrinkPerSec,
                tick.overspeedExtraDrainPerSec);
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

        RSS_DebugInfoParams debugParams = new RSS_DebugInfoParams();
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

    //! AI 限速诊断（计算层+应用层）。默认开（RSS_AI_SPEED_DIAG_ENABLED）。
    //! 仅记录距玩家 ≤80m 的 AI，每实体约 2s；APPLY 细节读 AIMovementApply 上次结果。
    protected static float s_fNextAiSpeedDiagGateMs = 0.0;
    protected static bool s_bAiSpeedDiagBannerPrinted = false;

    static void LogAiSpeedDiag(
        IEntity owner,
        SCR_CharacterControllerComponent ctrl,
        SCR_RSS_AIManager aiManager,
        RSS_AiSpeedDiagSnap snap)
    {
        if (!owner || !ctrl || !snap)
            return;
        if (!SCR_RSS_AIConstants.RSS_AI_SPEED_DIAG_ENABLED)
            return;
        if (ctrl.IsPlayerControlled())
            return;

        float distM = snap.distM;
        if (distM < 0.0)
            distM = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(owner);
        if (distM < 0.0 || distM > SCR_RSS_AIConstants.RSS_AI_SPEED_DIAG_MAX_DIST_M)
            return;

        float nowMs = 0.0;
        if (!SCR_RSS_RuntimeGuard.TryGetWorldTimeMs(nowMs))
            return;

        if (nowMs < s_fNextAiSpeedDiagGateMs)
            return;

        float aiLast = -1.0;
        if (aiManager)
            aiLast = aiManager.GetDebugLastPrintTime();
        if (aiLast >= 0.0 && (nowMs - aiLast) < SCR_RSS_AIConstants.RSS_AI_SPEED_DIAG_INTERVAL_MS)
            return;

        s_fNextAiSpeedDiagGateMs = nowMs + 250.0;
        if (aiManager)
            aiManager.SetDebugLastPrintTime(nowMs);

        if (!s_bAiSpeedDiagBannerPrinted)
        {
            Print("[RSS][AI-SPD] diag ON (near<=80m, ~2s). CALC=v/tgt/W'/latch APPLY=frac/gait/setOK CFG=drainOff");
            s_bAiSpeedDiagBannerPrinted = true;
        }

        int enginePhase = ctrl.GetCurrentMovementPhase();
        bool sprinting = ctrl.IsSprinting();
        bool drainOff = SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled();
        bool allOff = SCR_RSS_ConfigBridge.IsAiAllCalcDisabled();
        bool combatOn = SCR_RSS_ConfigBridge.IsAIStaminaCombatEffectsEnabled();

        string name = owner.GetName();
        if (name == "")
            name = "AI";

        EMovementType maxGait = SCR_RSS_AIMovementApply.GetLastMaxGait();
        EMovementType wantedAfter = SCR_RSS_AIMovementApply.GetLastWanted();
        bool settingsOk = SCR_RSS_AIMovementApply.GetLastSettingsOk();
        bool aiMoveOk = SCR_RSS_AIMovementApply.GetLastAiMoveOk();
        float phaseTopMs = SCR_RSS_AIMovementApply.GetLastPhaseTopMs();

        string gaitStr = typename.EnumToString(EMovementType, maxGait);
        string wantedStr = typename.EnumToString(EMovementType, wantedAfter);

        PrintFormat(
            "[RSS][AI-SPD] %1 path=%2 dist=%3m CALC v=%4 tgt=%5 ph=%6 enc=%7 grade=%8",
            name,
            snap.pathTag,
            Math.Round(distM).ToString(),
            (Math.Round(snap.currentSpeedMs * 100.0) / 100.0).ToString(),
            (Math.Round(snap.targetMs * 100.0) / 100.0).ToString(),
            snap.outPhase.ToString(),
            (Math.Round(snap.encPenalty * 1000.0) / 1000.0).ToString(),
            (Math.Round(snap.gradePercent * 10.0) / 10.0).ToString());

        PrintFormat(
            "[RSS][AI-SPD] %1 CALC W'=%2 latch=%3",
            name,
            (Math.Round(snap.wPrime01 * 100.0) / 100.0).ToString(),
            snap.cruiseLatched.ToString());

        PrintFormat(
            "[RSS][AI-SPD] %1 APPLY frac=%2 top=%3 maxGait=%4 wanted=%5 setOK=%6 moveOK=%7 engPh=%8 sprint=%9",
            name,
            (Math.Round(snap.appliedFrac * 1000.0) / 1000.0).ToString(),
            (Math.Round(phaseTopMs * 100.0) / 100.0).ToString(),
            gaitStr,
            wantedStr,
            settingsOk.ToString(),
            aiMoveOk.ToString(),
            enginePhase.ToString(),
            sprinting.ToString());

        PrintFormat(
            "[RSS][AI-SPD] %1 CFG drainOff=%2 allOff=%3 combat=%4",
            name,
            drainOff.ToString(),
            allOff.ToString(),
            combatOn.ToString());
    }

    //! AI tick debug (kept out of modded PlayerBase fragments for link reliability)
    //! 默认关闭：波次刷兵时每名 AI 一条会刷爆日志并拖帧；仅 Verbose 时按全局节流输出。
    protected static float s_fNextGlobalAiDebugMs = 0.0;

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
        if (!SCR_RSS_ConfigBridge.IsVerboseLoggingEnabled())
            return;
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            return;

        float nowMs = 0.0;
        if (!SCR_RSS_RuntimeGuard.TryGetWorldTimeMs(nowMs))
            return;
        if (nowMs < s_fNextGlobalAiDebugMs)
            return;
        s_fNextGlobalAiDebugMs = nowMs + 30000.0;

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
