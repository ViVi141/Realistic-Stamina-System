//! Phase C stamina tick finalize (drain write / cardio / debug flush).
//! 从 PlayerBase.c 拆出；行为不变。EnforceScript 参数上限 16，故用 context DTO。

class RSS_StaminaTickFinalizeContext
{
    SCR_CharacterControllerComponent ctrl;
    SCR_CharacterStaminaComponent staminaComponent;
    SCR_RSS_AnaerobicBurst anaerobicBurst;
    SCR_RSS_StaminaState staminaState;
    SCR_RSS_EnvironmentFactor environmentFactor;
    SCR_RSS_EncumbranceCache encumbranceCache;
    SCR_RSS_FatigueSystem fatigueSystem;
    SCR_RSS_ExerciseTracker exerciseTracker;
    SCR_RSS_EpocState epocState;
    SCR_RSS_CardioDrive cardioDrive;
    SCR_RSS_UISignalBridge uiSignalBridge;
    SCR_RSS_AIManager aiManager;
    SCR_RSS_TerrainDetector terrainDetector;
    SCR_RSS_StanceTransitionManager stanceTransitionManager;
    float appliedSpeedLimitMs;
    float landPositionDeltaSpeedMs;
    float currentWetWeight;
    float lastCapRatchetPerTick;
    string lastSpeedSource;
    bool sprintGateEnginePokeActive;
    bool swimmingVelocityDebugPrinted;
    float lastStaminaUpdateTime;
    float lastStaminaPercent;
    float lastSpeedMultiplier;
}

class SCR_PlayerBaseStaminaTickFinalize
{
    //! @return always true (Phase C has no early-out)
    static bool Run(RSS_StaminaTickLocals loc, RSS_StaminaTickFinalizeContext ctx)
    {
        if (!ctx || !ctx.ctrl || !loc)
            return true;

        loc.drainParams = new RSS_StaminaDrainTickParams();
        loc.drainParams.useSwimmingModel = loc.useSwimmingModel;
        loc.drainParams.currentSpeed = loc.currentSpeed;
        loc.drainParams.gearWeightKg = loc.currentWeight;
        loc.drainParams.encumbranceSpeedPenalty = loc.encumbranceSpeedPenalty;
        loc.drainParams.bodyPlusGearWeightKg = loc.totalWeight;
        loc.drainParams.totalWeightWithWetAndBody = loc.totalWeightWithWetAndBody;
        loc.drainParams.gradePercent = loc.gradePercent;
        loc.drainParams.terrainFactor = loc.terrainFactor;
        loc.drainParams.velocityForDrain = loc.velocityForDrain;
        loc.drainParams.ctx.swimmingVelocityDebugPrinted = ctx.swimmingVelocityDebugPrinted;
        loc.drainParams.owner = loc.owner;
        loc.drainParams.controller = ctx.ctrl;
        loc.drainParams.ctx.environmentFactor = ctx.environmentFactor;
        loc.drainParams.isSprinting = loc.isSprinting;
        loc.drainParams.currentMovementPhase = loc.effectiveMovementPhase;
        loc.drainParams.speedRatio = loc.speedRatio;
        loc.drainParams.heatStressMultiplier = loc.heatStressMultiplier;
        loc.drainParams.isSprintActive = loc.isSprintActive;
        loc.drainParams.staminaPercent = loc.staminaPercent;
        loc.drainParams.combatStimActive = loc.combatStimActive;
        loc.drainParams.ctx.encumbranceCache = ctx.encumbranceCache;
        loc.drainParams.ctx.fatigueSystem = ctx.fatigueSystem;
        loc.drainParams.ctx.exerciseTracker = ctx.exerciseTracker;
        loc.drainParams.ctx.epocState = ctx.epocState;
        loc.drainParams.currentTimeSec = loc.currentTime;
        loc.drainParams.currentTimeForExerciseMs = loc.currentTimeForExerciseMs;
        loc.drainParams.ctx.appliedSpeedLimitMs = ctx.appliedSpeedLimitMs;
        loc.drainParams.effectiveCriticalPowerWatts = -1.0;
        loc.drainParams.wPrimePool01 = 1.0;
        if (ctx.anaerobicBurst)
        {
            SCR_RSS_CriticalPowerModel cpForDrain = ctx.anaerobicBurst.GetCpModel();
            if (cpForDrain)
            {
                loc.drainParams.effectiveCriticalPowerWatts = cpForDrain.GetEffectiveCriticalPowerWatts();
                loc.drainParams.wPrimePool01 = cpForDrain.GetPool01();
            }
        }
        else
        {
            float cpFallback = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
            if (cpFallback > 1.0)
                loc.drainParams.effectiveCriticalPowerWatts = cpFallback;
        }

        loc.drainTick = SCR_RSS_UpdateCoordinator.CalculateTotalDrainRate(loc.drainParams);
        loc.totalDrainRate = loc.drainTick.totalDrainRate;
        loc.baseDrainRateByVelocity = loc.drainTick.baseDrainRateByVelocity;
        loc.baseDrainRateByVelocityForModule = loc.drainTick.baseDrainRateByVelocityForModule;
        ctx.swimmingVelocityDebugPrinted = loc.drainTick.ctx.swimmingVelocityDebugPrinted;

        loc.effectiveCriticalPowerWattsDbg = loc.drainParams.effectiveCriticalPowerWatts;
        loc.environmentMultDbg = 1.0;
        if (ctx.environmentFactor)
            loc.environmentMultDbg = ctx.environmentFactor.GetQuickEnvironmentMultiplier();

        loc.powerWattsDbg = 0.0;
        loc.wPrimePool01Dbg = loc.drainParams.wPrimePool01;
        if (!loc.useSwimmingModel)
        {
            loc.powerWattsDbg = SCR_RSS_DrainCalculator.GetMetabolicAccountingPowerWatts(
                loc.currentSpeed,
                ctx.appliedSpeedLimitMs,
                loc.totalWeightWithWetAndBody,
                loc.gradePercent,
                loc.terrainFactor,
                loc.effectiveMovementPhase,
                loc.wPrimePool01Dbg,
                loc.isSprintActive);
        }

        loc.needLocalDebugBatch = false;
        if (loc.owner == SCR_PlayerController.GetLocalControlledEntity() && ctx.ctrl.IsPlayerControlled())
        {
            if (SCR_PlayerBaseConfigHelper.IsRssDebugEnabled())
                loc.needLocalDebugBatch = true;
            else
            {
                SCR_RSS_Settings batchSettings = SCR_RSS_ConfigManager.GetSettings();
                if (batchSettings && batchSettings.m_bHintDisplayEnabled)
                    loc.needLocalDebugBatch = true;
            }
        }
        if (loc.needLocalDebugBatch)
            SCR_RSS_DebugBatchManager.StartDebugBatch();

        loc.staminaBeforeUpdate = loc.staminaPercent;
        loc.maxStaCapDbg = 1.0;
        loc.fatigueNormDbg = 0.0;
        if (ctx.fatigueSystem)
        {
            loc.maxStaCapDbg = ctx.fatigueSystem.GetMaxStaminaCap();
            loc.fatigueNormDbg = ctx.fatigueSystem.GetFatigueIntegralNorm();
        }

        loc.metabPowerDbg = -1.0;
        loc.metabPowerMetDbg = -1.0;
        loc.metabPowerRawDbg = -1.0;
        loc.metabCpDbg = loc.drainParams.effectiveCriticalPowerWatts;
        loc.metabAerobicDbg = -1.0;
        SCR_RSS_UpdateLoopMetabDebug.ComputeMetabDebugPowers(
            loc.useSwimmingModel,
            loc.currentSpeed,
            ctx.appliedSpeedLimitMs,
            loc.totalWeightWithWetAndBody,
            loc.gradePercent,
            loc.terrainFactor,
            loc.effectiveMovementPhase,
            loc.encumbranceSpeedPenalty,
            loc.drainParams.effectiveCriticalPowerWatts,
            loc.metabPowerDbg,
            loc.metabPowerMetDbg,
            loc.metabPowerRawDbg,
            loc.metabCpDbg,
            loc.metabAerobicDbg);

        loc.finalDrainDbg = SCR_RSS_StaminaNetRate.ComputeFinalDrainRatePerTick(
            loc.useSwimmingModel,
            loc.currentSpeed,
            loc.totalDrainRate,
            ctx.epocState,
            false);
        loc.metabolicNetDbg = SCR_RSS_StaminaNetRate.GetNetStaminaRatePerSecond(
            loc.staminaBeforeUpdate,
            loc.useSwimmingModel,
            loc.currentSpeed,
            loc.totalDrainRate,
            loc.baseDrainRateByVelocity,
            loc.baseDrainRateByVelocityForModule,
            loc.heatStressMultiplier,
            ctx.epocState,
            ctx.encumbranceCache,
            ctx.exerciseTracker,
            ctx.ctrl,
            ctx.environmentFactor,
            false) / 5.0;

        loc.overspeedExtraPerSec = 0.0;
        if (!loc.useSwimmingModel)
        {
            // 代谢限速模式：须有 applied limit；不压速模式：按 P−CP 罚 STA，不依赖限速
            bool canTaxOverspeed = true;
            if (SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            {
                if (ctx.appliedSpeedLimitMs <= 0.05)
                    canTaxOverspeed = false;
            }
            if (canTaxOverspeed)
            {
                bool wPrimeArmedForTax = false;
                if (ctx.anaerobicBurst && ctx.anaerobicBurst.GetCpModel())
                {
                    if (!SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(
                        ctx.anaerobicBurst.GetCpModel()))
                        wPrimeArmedForTax = true;
                }
                float limitForTax = ctx.appliedSpeedLimitMs;
                if (limitForTax <= 0.05)
                    limitForTax = loc.currentSpeed;
                loc.overspeedExtraPerSec = SCR_RSS_DrainCalculator.GetClientOverspeedExcessDrainPerSecond(
                    loc.currentSpeed,
                    limitForTax,
                    loc.drainParams.wPrimePool01,
                    loc.totalWeightWithWetAndBody,
                    loc.gradePercent,
                    loc.terrainFactor,
                    loc.effectiveMovementPhase,
                    loc.drainParams.effectiveCriticalPowerWatts,
                    wPrimeArmedForTax,
                    ctx.ctrl.RSS_IsCpWalkOverrideActive());
            }
        }

        if (ctx.staminaComponent)
        {
            float newTargetStamina = SCR_RSS_UpdateCoordinator.UpdateStaminaValue(
                ctx.staminaComponent,
                loc.staminaPercent,
                loc.useSwimmingModel,
                loc.currentSpeed,
                loc.totalDrainRate,
                loc.baseDrainRateByVelocity,
                loc.baseDrainRateByVelocityForModule,
                loc.heatStressMultiplier,
                ctx.epocState,
                ctx.encumbranceCache,
                ctx.exerciseTracker,
                ctx.fatigueSystem,
                ctx.ctrl,
                ctx.environmentFactor,
                loc.timeDeltaSec);

            if (loc.overspeedExtraPerSec > 0.000001)
                newTargetStamina = newTargetStamina - loc.overspeedExtraPerSec * loc.timeDeltaSec;
            newTargetStamina = Math.Clamp(newTargetStamina, 0.0, 1.0);

            ctx.staminaComponent.SetTargetStamina(newTargetStamina);
            if (ctx.staminaState)
                ctx.staminaState.SetAerobic(newTargetStamina);
            ctx.lastStaminaUpdateTime = loc.currentTime;

            bool sprintIntentAfterUpdate = loc.isSprintActive || ctx.ctrl.GetIsSprintingToggle();
            ctx.ctrl.RSS_PokeEngineStaminaForSprintBlock(sprintIntentAfterUpdate);

            // transient 活跃时 GetStamina()!=有氧目标是预期（冲刺门 / W′ 表现）
            if (!ctx.sprintGateEnginePokeActive)
            {
                float verifyStamina = ctx.staminaComponent.GetStamina();
                if (Math.AbsFloat(verifyStamina - newTargetStamina) > 0.005)
                {
                    if (SCR_RSS_DebugBatchManager.IsDebugBatchActive())
                    {
                        string intLine = string.Format("[RSS] 原生干扰: 目标=%1%% 实际=%2%% 偏差=%3%%",
                            Math.Round(newTargetStamina * 100.0).ToString(),
                            Math.Round(verifyStamina * 100.0).ToString(),
                            Math.Round(Math.AbsFloat(verifyStamina - newTargetStamina) * 10000.0) / 100.0);
                        SCR_RSS_DebugBatchManager.AddDebugBatchLine(intLine);
                    }
                    ctx.staminaComponent.SetTargetStamina(newTargetStamina);
                    if (ctx.staminaState)
                        ctx.staminaState.SetAerobic(newTargetStamina);
                    ctx.ctrl.RSS_PokeEngineStaminaForSprintBlock(sprintIntentAfterUpdate);
                }
            }

            loc.staminaPercent = newTargetStamina;
        }

        if (loc.isPlayer && ctx.cardioDrive)
        {
            float cardioCp = loc.drainParams.effectiveCriticalPowerWatts;
            float cardioWPrime = loc.drainParams.wPrimePool01;
            if (ctx.anaerobicBurst && ctx.anaerobicBurst.GetCpModel())
            {
                SCR_RSS_CriticalPowerModel cardioCpModel = ctx.anaerobicBurst.GetCpModel();
                cardioCp = cardioCpModel.GetEffectiveCriticalPowerWatts();
                cardioWPrime = cardioCpModel.GetPool01();
            }
            ctx.cardioDrive.SetMetabolicSample(
                loc.powerWattsDbg,
                cardioCp,
                cardioWPrime,
                loc.staminaPercent);
            ctx.cardioDrive.Tick(loc.currentTime);
        }

        if (loc.isPlayer && ctx.uiSignalBridge)
        {
            ctx.uiSignalBridge.UpdateUISignal(
                loc.staminaPercent,
                loc.isExhausted,
                loc.currentSpeed,
                loc.totalDrainRate,
                false,
                ctx.ctrl.GetRssWPrimePool01());
        }

        ctx.lastStaminaPercent = loc.staminaPercent;
        ctx.lastSpeedMultiplier = loc.finalSpeedMultiplier;

        loc.netStaminaTickDbg = loc.staminaPercent - loc.staminaBeforeUpdate;

        loc.metabSnap = new RSS_StatusMetabLogSnapshot();
        loc.metabSnap.metabolismPowerW = loc.metabPowerDbg;
        loc.metabSnap.metabolismPowerMetW = loc.metabPowerMetDbg;
        loc.metabSnap.metabolismPowerRawW = loc.metabPowerRawDbg;
        loc.metabSnap.effectiveCpW = loc.metabCpDbg;
        loc.metabSnap.aerobicPowerW = loc.metabAerobicDbg;
        loc.metabSnap.finalDrainPerTick = loc.finalDrainDbg;
        loc.metabSnap.metabolicNetPerTick = loc.metabolicNetDbg;
        loc.metabSnap.capRatchetPerTick = ctx.lastCapRatchetPerTick;
        loc.metabSnap.netStaminaPerTick = loc.netStaminaTickDbg;

        ctx.ctrl.RSS_UpdateStatusLogSnapshot(
            loc.currentSpeed,
            loc.staminaPercent,
            loc.finalSpeedMultiplier,
            loc.isSprinting,
            loc.phaseNow,
            loc.effectivePhase,
            loc.maxStaCapDbg,
            ctx.ctrl.GetRssAnaerobicPercent(),
            loc.fatigueNormDbg,
            loc.metabSnap);

        SCR_RSS_UpdateLoopDebugOutput.LogAiStaminaTick(
            loc.owner,
            loc.staminaPercent,
            loc.currentWeight,
            loc.finalSpeedMultiplier,
            loc.currentSpeed,
            loc.isSprinting,
            loc.currentMovementPhase,
            ctx.aiManager,
            ctx.fatigueSystem,
            ctx.lastSpeedSource);

        loc.debugTick = new RSS_StaminaDebugOutputParams();
        loc.targetStaCapDbg = 1.0;
        loc.capShrinkDbg = 0.0;
        if (ctx.fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
        {
            loc.targetStaCapDbg = ctx.fatigueSystem.GetMaxStaminaCap();
            if (!loc.useSwimmingModel && loc.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            {
                float powerFat = SCR_RSS_DrainCalculator.GetMetabolicFatiguePowerWatts(
                    loc.currentSpeed,
                    ctx.appliedSpeedLimitMs,
                    loc.totalWeightWithWetAndBody,
                    loc.gradePercent,
                    loc.terrainFactor,
                    loc.phaseNow);
                loc.capShrinkDbg = ctx.fatigueSystem.EstimateCapShrinkPerSecond(
                    powerFat,
                    loc.currentWeight,
                    loc.gradePercent,
                    loc.terrainFactor,
                    loc.currentSpeed,
                    loc.effectiveCriticalPowerWattsDbg);
            }
        }
        loc.epocActiveDbg = false;
        if (ctx.epocState)
            loc.epocActiveDbg = ctx.epocState.IsInEpocDelay();
        SCR_RSS_UpdateLoopMetabDebug.FillDebugTickCore(
            loc.debugTick,
            loc.staminaPercent,
            loc.useSwimmingModel,
            loc.currentSpeed,
            loc.totalDrainRate,
            loc.baseDrainRateByVelocity,
            loc.baseDrainRateByVelocityForModule,
            loc.heatStressMultiplier,
            loc.baseSpeedMultiplier,
            loc.encumbranceSpeedPenalty,
            loc.finalSpeedMultiplier,
            loc.gradePercent,
            loc.slopeAngleDegrees,
            loc.isSwimming,
            loc.isSprinting,
            loc.isSprintActive);
        SCR_RSS_UpdateLoopMetabDebug.FillDebugTickMetab(
            loc.debugTick,
            loc.currentMovementPhase,
            loc.effectiveMovementPhase,
            loc.rainWeight,
            loc.maxStaCapDbg,
            loc.fatigueNormDbg,
            loc.metabPowerDbg,
            loc.metabPowerMetDbg,
            loc.metabPowerRawDbg,
            loc.metabCpDbg,
            loc.metabAerobicDbg,
            loc.finalDrainDbg,
            loc.metabolicNetDbg,
            ctx.lastCapRatchetPerTick,
            loc.netStaminaTickDbg,
            loc.terrainFactor);
        SCR_RSS_UpdateLoopMetabDebug.FillDebugTickExtras(
            loc.debugTick,
            ctx.appliedSpeedLimitMs,
            loc.effectiveCriticalPowerWattsDbg,
            loc.timeDeltaSec,
            loc.totalWeightWithWetAndBody,
            loc.powerWattsDbg,
            loc.wPrimePool01Dbg,
            ctx.landPositionDeltaSpeedMs,
            loc.overspeedExtraPerSec,
            loc.environmentMultDbg,
            loc.targetStaCapDbg,
            loc.capShrinkDbg,
            loc.epocActiveDbg);
        SCR_RSS_UpdateLoopDebugOutput.OutputPlayerStaminaAndHints(
            ctx.ctrl,
            loc.owner,
            loc.debugTick,
            ctx.encumbranceCache,
            ctx.fatigueSystem,
            ctx.epocState,
            ctx.exerciseTracker,
            ctx.environmentFactor,
            ctx.terrainDetector,
            ctx.stanceTransitionManager,
            ctx.currentWetWeight,
            ctx.lastSpeedSource,
            ctx.ctrl.GetRssWPrimePool01(),
            ctx.ctrl.GetRssSprintCooldownRemainingSec());

        SCR_RSS_DebugBatchManager.FlushDebugBatch();


        return true;
    }
}
