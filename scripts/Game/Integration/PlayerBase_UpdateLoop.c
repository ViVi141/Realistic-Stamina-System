//! 本地玩家 Debug/HUD 单 tick 快照（EnforceScript 方法参数上限 16）
class RSS_StaminaDebugOutputParams
{
    float staminaPercent;
    bool useSwimmingModel;
    float currentSpeed;
    float totalDrainRate;
    float baseDrainRateByVelocity;
    float baseDrainRateByVelocityForModule;
    float heatStressMultiplier;
    float baseSpeedMultiplier;
    float encumbranceSpeedPenalty;
    float finalSpeedMultiplier;
    float gradePercent;
    float slopeAngleDegrees;
    bool isSwimming;
    bool isSprinting;
    bool isSprintActive;
    int currentMovementPhase;
    int effectiveMovementPhase;
    float rainWeight;
    float terrainFactor;
    float appliedSpeedLimitMs;
    float effectiveCriticalPowerWatts;
    float timeDeltaSec;
    float totalWeightWithWetAndBody;
    float powerWatts;
    float environmentMult;
    float targetStaminaCap;
    float capShrinkPerSec;
    float timeToDepleteSec;
    float timeToFullSec;
    bool epocActive;
}

modded class SCR_CharacterControllerComponent
{
    void UpdateSpeedBasedOnStamina()
    {
        if (m_bIsDeleted)
            return;
        IEntity owner = GetOwner();
        if (!owner)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }

        if (!GetGame())
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }
        World world = GetGame().GetWorld();
        if (!world)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }

        if (!ShouldProcessStaminaUpdate())
        {
            RSS_SetMudSlipCameraShake01(0.0);
            m_bRssStaminaLoopActive = false;
            return;
        }

        if (SCR_PlayerBaseVehicleHelper.HandleVehicleStaminaUpdate(
                this, owner, m_pCompartmentAccess, m_pStaminaComponent,
                m_pExerciseTracker, m_pFatigueSystem, m_pEpocState,
                m_pEncumbranceCache, m_pEnvironmentFactor,
                m_pTerrainDetector, m_pStanceTransitionManager,
                m_fLastStaminaUpdateTime, m_fCurrentWetWeight,
                GetSpeedUpdateIntervalMs(), IsRssDebugEnabled()))
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, GetSpeedUpdateIntervalMs(), false, this);
            return;
        }
        
        bool isPlayer = IsPlayerControlled();

        if (isPlayer)
        {
            RSS_CombatStim_OnTickTransitions();

            if (Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
            {
                if (m_pCachedOwnerCharacter)
                {
                    SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                    if (stimDmgMgr)
                        SCR_RSS_CombatStimController.RefreshBleedingEffectsToMatchScale(stimDmgMgr);
                }
            }
        }

        if (!isPlayer && Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
        {
            if (m_pCachedOwnerCharacter)
            {
                SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                if (stimDmgMgr)
                    SCR_RSS_CombatStimController.RefreshBleedingEffectsToMatchScale(stimDmgMgr);
            }
        }

        float staminaPercent = GetRssAerobicPercent();
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        float encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.CheckAndUpdate();
            encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenalty();
        }

        bool isExhausted = SCR_RSS_MetabolismMath.IsExhausted(staminaPercent);
        if (isExhausted)
        {
            float limpSpeedMultiplier = SCR_RSS_MetabolismMath.GetDynamicLimpMultiplier(encumbranceSpeedPenalty);
            float compensatedLimpMultiplier = Math.Clamp(limpSpeedMultiplier * m_fAnimSpeedCompensation, 0.01, 1.0);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, compensatedLimpMultiplier);

            if (!m_bLastExhaustedState && IsRssDebugEnabled())
            {
                Print("[RSS] Exhausted: limp speed");
                m_bLastExhaustedState = true;
            }

        }
        else
        {
            if (m_bLastExhaustedState && IsRssDebugEnabled())
            {
                Print("[RSS] Recovered from Exhaustion");
                m_bLastExhaustedState = false;
            }
        }
        
        bool isSwimmingForSpeed = SCR_RSS_SwimmingStateManager.IsSwimming(this);
        vector velocity;
        float currentSpeed;
        if (isSwimmingForSpeed)
        {
            float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
            SpeedCalculationResult speedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
            velocity = speedResult.computedVelocity;
            currentSpeed = Math.Min(speedResult.computedVelocity.Length(), 7.0);
            m_vLastPositionSample = speedResult.lastPositionSample;
            m_bHasLastPositionSample = speedResult.hasLastPositionSample;
            m_vComputedVelocity = speedResult.computedVelocity;
        }
        else
        {
            velocity = GetVelocity();
            currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(velocity);
            m_bHasLastPositionSample = false;
        }
        
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        if (phaseNow >= 1 && phaseNow <= 3)
            m_iLastNonIdleMovementPhase = phaseNow;
        else if (isSprintingNow)
            m_iLastNonIdleMovementPhase = 3;

        int effectivePhase = SCR_RSS_SpeedCalculator.ResolveCoastingMovementPhase(
            phaseNow, currentSpeed, m_iLastNonIdleMovementPhase);
        bool isSprintActive = isSprintingNow || (phaseNow == 3);

        bool sprintIntent = isSprintActive || GetIsSprintingToggle();
        RSS_PokeEngineStaminaForSprintBlock(sprintIntent);
        
        float currentTimeForExerciseMs = world.GetWorldTime();
        float currentTime = currentTimeForExerciseMs / 1000.0;

        float terrainFactor = 1.0;
        if (m_pTerrainDetector)
            terrainFactor = m_pTerrainDetector.GetTerrainFactor(owner, currentTime, currentSpeed);

        if (m_pEnvironmentFactor)
            m_pEnvironmentFactor.UpdateEnvironmentFactors(currentTime, owner, velocity, terrainFactor, m_fCurrentWetWeight);

        float finalSpeedMultiplier = SCR_RSS_UpdateCoordinator.UpdateSpeed(
            this,
            staminaPercent,
            encumbranceSpeedPenalty,
            m_pCollapseTransition,
            currentSpeed,
            m_pEnvironmentFactor,
            m_pSlopeSpeedTransition,
            velocity,
            terrainFactor,
            effectivePhase);

        if (m_pSprintBlockSpeedTransition)
        {
            bool sprintAllowed = GetRssSprintAllowed();
            float engineBaseForLimit = GetRssSpeedLimitEngineBaseMs();
            float targetAbsoluteSpeedMs = finalSpeedMultiplier * engineBaseForLimit;
            float lastEngineBase = m_fLastRssEngineBaseForLimit;
            if (lastEngineBase <= 0.1)
                lastEngineBase = engineBaseForLimit;
            finalSpeedMultiplier = m_pSprintBlockSpeedTransition.UpdateAndGet(
                currentTime,
                targetAbsoluteSpeedMs,
                engineBaseForLimit,
                sprintAllowed,
                m_fLastRssSpeedMultiplierApplied,
                lastEngineBase);
        }
        
        float baseSpeedMultiplier = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(
            staminaPercent, effectivePhase, encumbranceSpeedPenalty);
        
        float currentWeight = 0.0;
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            currentWeight = m_pEncumbranceCache.GetCurrentWeight();
        else
        {
            if (!m_pCachedInventoryComponent)
                m_pCachedInventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(owner.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (m_pCachedInventoryComponent)
                currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
        }

        float speedToApply = finalSpeedMultiplier;
        if (!Replication.IsServer() && IsPlayerControlled() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled() && m_pNetworkSyncManager.HasServerValidation())
        {
            m_pNetworkSyncManager.GetTargetSpeedMultiplier(finalSpeedMultiplier);
            speedToApply = m_pNetworkSyncManager.GetSmoothedSpeedMultiplier(currentTime);
        }
        float finalSpeedToApply = Math.Clamp(speedToApply, 0.01, 3.0);
        m_fLastRssSpeedMultiplierApplied = finalSpeedToApply;
        float storedEngineBase = GetRssSpeedLimitEngineBaseMs();
        if (storedEngineBase > 0.1)
            m_fLastRssEngineBaseForLimit = storedEngineBase;
        m_fAppliedSpeedLimitMs = finalSpeedToApply * storedEngineBase;
        if (m_fAppliedSpeedLimitMs <= 0.05)
            m_fAppliedSpeedLimitMs = -1.0;
        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, finalSpeedToApply);
        if (IsPlayerControlled())
            m_sLastSpeedSource = "Client";
        else
            m_sLastSpeedSource = "Server";

        if (!isPlayer && SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, GetSpeedUpdateIntervalMs(), false, this);
            return;
        }

        bool isCriticalData = (staminaPercent <= 0.05 || (m_pNetworkSyncManager && m_pNetworkSyncManager.GetLastReportedStaminaPercent() > 0.5 && staminaPercent <= 0.1));
        if (isPlayer && !Replication.IsServer() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled())
        {
            int syncType = 0;
            if (isCriticalData)
                syncType = 1;
            if (m_pNetworkSyncManager.ShouldSync(currentTime, syncType))
            {
                if (!SCR_RSS_ConfigManager.GetServerDataExportEnabled())
                    return;
                Rpc(RPC_ClientReportStamina, staminaPercent, currentWeight, currentTime, isCriticalData);
                if (isCriticalData && IsRssDebugEnabled())
                    PrintFormat("[RSS] Critical stamina event reported (stamina=%1)", staminaPercent);
            }
        }

        bool isSwimming = SCR_RSS_SwimmingStateManager.IsSwimming(this);

        float timeDeltaSec;
        if (m_fLastStaminaUpdateTime >= 0.0)
            timeDeltaSec = currentTime - m_fLastStaminaUpdateTime;
        else
            timeDeltaSec = GetSpeedUpdateIntervalMs() / 1000.0;
        timeDeltaSec = Math.Clamp(timeDeltaSec, 0.01, 0.5);

        if (isSwimming != m_bWasSwimming)
            m_bSwimmingVelocityDebugPrinted = false;
        
        if (isPlayer)
        {
            WetWeightUpdateResult wetWeightResult = SCR_RSS_SwimmingStateManager.UpdateWetWeight(
                m_bWasSwimming,
                isSwimming,
                currentTime,
                m_fWetWeightStartTime,
                m_fCurrentWetWeight,
                m_fSwimStartTimeSec,
                owner);
            m_fWetWeightStartTime = wetWeightResult.wetWeightStartTime;
            m_fCurrentWetWeight = wetWeightResult.currentWetWeight;
            m_fSwimStartTimeSec = wetWeightResult.swimStartTimeSec;
            m_bWasSwimming = isSwimming;
        }
        
        float heatStressMultiplier = 1.0;
        if (m_pEnvironmentFactor)
            heatStressMultiplier = m_pEnvironmentFactor.GetHeatStressMultiplier();
        
        float rainWeight = 0.0;
        if (m_pEnvironmentFactor)
            rainWeight = m_pEnvironmentFactor.GetRainWeight();

        if (isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
        {
            heatStressMultiplier = 1.0;
            rainWeight = 0.0;
        }
        
        float totalWetWeight = SCR_RSS_SwimmingStateManager.CalculateTotalWetWeight(m_fCurrentWetWeight, rainWeight);
        float currentWeightWithWet = currentWeight + totalWetWeight;

        float totalWeight = currentWeight + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        float totalWeightWithWetAndBody = currentWeightWithWet + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;

        bool useSwimmingModel = isSwimming;

        if (isPlayer && m_pStaminaComponent && m_pJumpVaultDetector && m_pStanceTransitionManager)
        {
            float currentTimeSec = world.GetWorldTime() / 1000.0;
            staminaPercent = SCR_RSS_UpdateCoordinator.ApplyPlayerActionStaminaCosts(
                this,
                owner,
                staminaPercent,
                timeDeltaSec,
                currentTimeSec,
                m_pJumpVaultDetector,
                m_pStanceTransitionManager,
                m_pEncumbranceCache,
                m_pUISignalBridge,
                IsRssDebugEnabled());
        }
        
        float speedRatio = Math.Clamp(currentSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED, 0.0, 1.0);

        vector velocityForDrain = velocity;
        if (useSwimmingModel && !HasSwimInput())
            velocityForDrain = vector.Zero;

        float slopeAngleDegrees = 0.0;
        GradeCalculationResult gradeResult = SCR_RSS_SpeedCalculator.CalculateGradePercent(
            this,
            currentSpeed,
            m_pJumpVaultDetector,
            slopeAngleDegrees,
            m_pEnvironmentFactor,
            velocityForDrain);
        float gradePercent = gradeResult.gradePercent;
        slopeAngleDegrees = gradeResult.slopeAngleDegrees;

        if (!isExhausted)
        {
            float engineBase = GetRssSpeedLimitEngineBaseMs();
            if (engineBase <= 0.05)
                engineBase = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

            SCR_RSS_CriticalPowerModel cpModel = null;
            if (m_pAnaerobicBurst)
                cpModel = m_pAnaerobicBurst.GetCpModel();

            float correctedSpeed = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                m_fLastRssSpeedMultiplierApplied,
                currentSpeed,
                phaseNow,
                encumbranceSpeedPenalty,
                totalWeightWithWetAndBody,
                gradePercent,
                terrainFactor,
                isExhausted,
                engineBase,
                currentTime,
                cpModel);
            if (correctedSpeed != m_fLastRssSpeedMultiplierApplied)
            {
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, correctedSpeed);
                m_fLastRssSpeedMultiplierApplied = correctedSpeed;
                finalSpeedMultiplier = correctedSpeed;
                m_fAppliedSpeedLimitMs = correctedSpeed * engineBase;
            }
        }

        if (m_pAnaerobicBurst)
        {
            bool tickAnaerobic = Replication.IsServer();

            if (tickAnaerobic)
            {
                float drainSpeed = SCR_RSS_DrainCalculator.GetDrainVelocityMs(
                    currentSpeed, m_fAppliedSpeedLimitMs);
                float powerW = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                    drainSpeed,
                    totalWeightWithWetAndBody,
                    gradePercent,
                    terrainFactor,
                    true,
                    effectivePhase);

                SCR_RSS_CriticalPowerModel cpModel = m_pAnaerobicBurst.GetCpModel();
                if (cpModel)
                {
                    float loadKg = Math.Max(totalWeightWithWetAndBody - SCR_RSS_MetabolismMath.CHARACTER_WEIGHT, 0.0);
                    float envCpMult = 1.0;
                    if (m_pEnvironmentFactor)
                    {
                        float heatPen = m_pEnvironmentFactor.GetHeatStressPenalty();
                        envCpMult = 1.0 - heatPen * 0.35;
                    }
                    float fatigueNorm = 0.0;
                    if (m_pFatigueSystem)
                        fatigueNorm = m_pFatigueSystem.GetFatigueIntegralNorm();
                    cpModel.SetRuntimeContext(loadKg, gradePercent, envCpMult, fatigueNorm);
                    if (m_pFatigueSystem)
                    {
                        float cpMult = m_pFatigueSystem.GetCpFatigueMultiplier();
                        cpModel.SetFatigueCpMultiplier(cpMult);
                    }
                }

                m_pAnaerobicBurst.TickPower(powerW, isSprintActive, currentTime, timeDeltaSec, currentSpeed);
                if (m_pEpocState)
                    m_pEpocState.UpdateExercisePowerSample(powerW, currentSpeed);
                if (m_pStaminaState)
                {
                    m_pStaminaState.SetAnaerobicFromCpModel(m_pAnaerobicBurst.GetCpModel());
                    m_pStaminaState.SetAerobic(staminaPercent);
                }
                if (Replication.IsServer())
                {
                    SCR_RSS_NetworkSyncManager.ReadAnaerobicForReplication(
                        m_pAnaerobicBurst, m_fReplAnaerobicPool, m_fReplAnaerobicCooldownUntil);
                    Replication.BumpMe();
                }
            }
        }

        if (m_pFatigueSystem && !useSwimmingModel && currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
        {
            float drainSpeedFat = SCR_RSS_DrainCalculator.GetDrainVelocityMs(
                currentSpeed, m_fAppliedSpeedLimitMs);
            float powerFat = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                drainSpeedFat,
                totalWeightWithWetAndBody,
                gradePercent,
                terrainFactor,
                true,
                phaseNow);
            m_pFatigueSystem.ProcessFatigueIntegral(
                powerFat,
                currentWeight,
                gradePercent,
                terrainFactor,
                timeDeltaSec,
                currentSpeed);
        }

        if (SCR_RSS_ConfigBridge.IsMudSlipMechanismEnabled())
        {
            if (m_pMudSlipRunner)
            {
                m_pMudSlipRunner.ProcessAfterSlope(
                    this,
                    useSwimmingModel,
                    isSwimming,
                    m_pEnvironmentFactor,
                    m_pStaminaComponent,
                    currentSpeed,
                    isSprintActive,
                    currentWeight,
                    staminaPercent,
                    velocity,
                    slopeAngleDegrees,
                    timeDeltaSec,
                    currentTime,
                    IsRssDebugEnabled());
            }

        }
        else
        {
            RSS_SetMudSlipCameraShake01(0.0);
        }
        if (!isPlayer && Replication.IsServer() && m_pAIManager)
        {
            float fatigueVal;
            if (m_pFatigueSystem)
                fatigueVal = m_pFatigueSystem.GetFatigueAccumulation();
            else
                fatigueVal = 0.0;

            m_pAIManager.Tick(
                owner, currentTime, timeDeltaSec,
                staminaPercent, fatigueVal, currentSpeed, isPlayer);
        }

        bool isSprinting = isSprintingNow;
        int currentMovementPhase = phaseNow;
        int effectiveMovementPhase = effectivePhase;
        float totalDrainRate = 0.0;
        float baseDrainRateByVelocity = 0.0;
        float baseDrainRateByVelocityForModule = 0.0;

        bool combatStimActive = false;
        if (isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
            combatStimActive = true;

        StaminaDrainTickParams drainParams = new StaminaDrainTickParams();
        drainParams.useSwimmingModel = useSwimmingModel;
        drainParams.currentSpeed = currentSpeed;
        drainParams.currentWeight = currentWeight;
        drainParams.encumbranceSpeedPenalty = encumbranceSpeedPenalty;
        drainParams.totalWeight = totalWeight;
        drainParams.totalWeightWithWetAndBody = totalWeightWithWetAndBody;
        drainParams.gradePercent = gradePercent;
        drainParams.terrainFactor = terrainFactor;
        drainParams.velocityForDrain = velocityForDrain;
        drainParams.swimmingVelocityDebugPrinted = m_bSwimmingVelocityDebugPrinted;
        drainParams.owner = owner;
        drainParams.controller = this;
        drainParams.environmentFactor = m_pEnvironmentFactor;
        drainParams.isSprinting = isSprinting;
        drainParams.currentMovementPhase = effectiveMovementPhase;
        drainParams.speedRatio = speedRatio;
        drainParams.heatStressMultiplier = heatStressMultiplier;
        drainParams.isSprintActive = isSprintActive;
        drainParams.staminaPercent = staminaPercent;
        drainParams.combatStimActive = combatStimActive;
        drainParams.encumbranceCache = m_pEncumbranceCache;
        drainParams.fatigueSystem = m_pFatigueSystem;
        drainParams.exerciseTracker = m_pExerciseTracker;
        drainParams.epocState = m_pEpocState;
        drainParams.currentTimeSec = currentTime;
        drainParams.currentTimeForExerciseMs = currentTimeForExerciseMs;
        drainParams.appliedSpeedLimitMs = m_fAppliedSpeedLimitMs;
        drainParams.effectiveCriticalPowerWatts = -1.0;
        if (m_pAnaerobicBurst)
        {
            SCR_RSS_CriticalPowerModel cpForDrain = m_pAnaerobicBurst.GetCpModel();
            if (cpForDrain)
                drainParams.effectiveCriticalPowerWatts = cpForDrain.GetEffectiveCriticalPowerWatts();
        }
        else
        {
            float cpFallback = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
            if (cpFallback > 1.0)
                drainParams.effectiveCriticalPowerWatts = cpFallback;
        }

        StaminaDrainTickResult drainTick = SCR_RSS_UpdateCoordinator.CalculateTotalDrainRate(drainParams);
        totalDrainRate = drainTick.totalDrainRate;
        baseDrainRateByVelocity = drainTick.baseDrainRateByVelocity;
        baseDrainRateByVelocityForModule = drainTick.baseDrainRateByVelocityForModule;
        m_bSwimmingVelocityDebugPrinted = drainTick.swimmingVelocityDebugPrinted;

        float effectiveCriticalPowerWattsDbg = drainParams.effectiveCriticalPowerWatts;
        float environmentMultDbg = 1.0;
        if (m_pEnvironmentFactor)
            environmentMultDbg = m_pEnvironmentFactor.GetQuickEnvironmentMultiplier();

        float drainSpeedDbg = SCR_RSS_DrainCalculator.GetDrainVelocityMs(
            currentSpeed, m_fAppliedSpeedLimitMs);
        float powerWattsDbg = 0.0;
        if (!useSwimmingModel)
        {
            powerWattsDbg = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                drainSpeedDbg,
                totalWeightWithWetAndBody,
                gradePercent,
                terrainFactor,
                true,
                effectiveMovementPhase);
        }

        bool needLocalDebugBatch = false;
        if (owner == SCR_PlayerController.GetLocalControlledEntity() && IsPlayerControlled())
        {
            if (IsRssDebugEnabled())
                needLocalDebugBatch = true;
            else
            {
                SCR_RSS_Settings batchSettings = SCR_RSS_ConfigManager.GetSettings();
                if (batchSettings && batchSettings.m_bHintDisplayEnabled)
                    needLocalDebugBatch = true;
            }
        }
        if (needLocalDebugBatch)
            SCR_RSS_DebugBatchManager.StartDebugBatch();
        
        if (m_pStaminaComponent)
        {
            float newTargetStamina = SCR_RSS_UpdateCoordinator.UpdateStaminaValue(
                m_pStaminaComponent,
                staminaPercent,
                useSwimmingModel,
                currentSpeed,
                totalDrainRate,
                baseDrainRateByVelocity,
                baseDrainRateByVelocityForModule,
                heatStressMultiplier,
                m_pEpocState,
                m_pEncumbranceCache,
                m_pExerciseTracker,
                m_pFatigueSystem,
                this,
                m_pEnvironmentFactor,
                timeDeltaSec);
            
            m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            if (m_pStaminaState)
                m_pStaminaState.SetAerobic(newTargetStamina);
            m_fLastStaminaUpdateTime = currentTime;

            bool sprintIntentAfterUpdate = isSprintActive || GetIsSprintingToggle();
            RSS_PokeEngineStaminaForSprintBlock(sprintIntentAfterUpdate);
            
            if (!m_bSprintGateEnginePokeActive)
            {
                float verifyStamina = m_pStaminaComponent.GetStamina();
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
                    m_pStaminaComponent.SetTargetStamina(newTargetStamina);
                    if (m_pStaminaState)
                        m_pStaminaState.SetAerobic(newTargetStamina);
                }
            }
            
            staminaPercent = newTargetStamina;
        }

        if (isPlayer && m_pUISignalBridge)
        {
            m_pUISignalBridge.UpdateUISignal(staminaPercent, isExhausted, currentSpeed, totalDrainRate, false);
        }
        
        m_fLastStaminaPercent = staminaPercent;
        m_fLastSpeedMultiplier = finalSpeedMultiplier;

        RSS_UpdateStatusLogSnapshot(
            currentSpeed,
            staminaPercent,
            finalSpeedMultiplier,
            isSprinting,
            phaseNow,
            effectivePhase);

        RSS_DebugLogAiStaminaTick(
            owner, staminaPercent, currentWeight, finalSpeedMultiplier,
            currentSpeed, isSprinting, currentMovementPhase);

        RSS_StaminaDebugOutputParams debugTick = new RSS_StaminaDebugOutputParams();
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
        debugTick.currentMovementPhase = currentMovementPhase;
        debugTick.effectiveMovementPhase = effectiveMovementPhase;
        debugTick.rainWeight = rainWeight;
        debugTick.isSprintActive = isSprintActive;
        debugTick.terrainFactor = terrainFactor;
        debugTick.appliedSpeedLimitMs = m_fAppliedSpeedLimitMs;
        debugTick.effectiveCriticalPowerWatts = effectiveCriticalPowerWattsDbg;
        debugTick.timeDeltaSec = timeDeltaSec;
        debugTick.totalWeightWithWetAndBody = totalWeightWithWetAndBody;
        debugTick.powerWatts = powerWattsDbg;
        debugTick.environmentMult = environmentMultDbg;
        debugTick.targetStaminaCap = 1.0;
        debugTick.capShrinkPerSec = 0.0;
        if (m_pFatigueSystem)
        {
            debugTick.targetStaminaCap = m_pFatigueSystem.GetMaxStaminaCap();
            if (!useSwimmingModel && currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            {
                float drainSpeedFat = SCR_RSS_DrainCalculator.GetDrainVelocityMs(
                    currentSpeed, m_fAppliedSpeedLimitMs);
                float powerFat = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                    drainSpeedFat,
                    totalWeightWithWetAndBody,
                    gradePercent,
                    terrainFactor,
                    true,
                    phaseNow);
                debugTick.capShrinkPerSec = m_pFatigueSystem.EstimateCapShrinkPerSecond(
                    powerFat,
                    currentWeight,
                    gradePercent,
                    terrainFactor,
                    currentSpeed);
            }
        }
        debugTick.epocActive = false;
        if (m_pEpocState)
            debugTick.epocActive = m_pEpocState.IsInEpocDelay();
        RSS_DebugOutputPlayerStaminaAndHints(owner, debugTick);
        
        SCR_RSS_DebugBatchManager.FlushDebugBatch();
        
        m_bRssStaminaLoopActive = true;
        GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, GetSpeedUpdateIntervalMs(), false, this);
    }


    void RSS_LoopStartSystem()
    {
        if (m_bIsDeleted || !GetOwner())
            return;
        if (IsWorkbenchPreviewEntity())
            return;
        if (!ShouldProcessStaminaUpdate())
            return;
        if (!GetGame())
            return;
        m_bRssStaminaLoopActive = true;
        int intervalMs = GetSpeedUpdateIntervalMs();
        GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, intervalMs, false, this);
        
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.CollectSpeedSampleBridge, SPEED_SAMPLE_INTERVAL_MS, false, this);
    }


    void EnsureRssStaminaLoopIfNeeded()
    {
        if (m_bIsDeleted)
            return;
        if (!ShouldProcessStaminaUpdate())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        RSS_LoopStartSystem();
    }


    void EnsureAiStaminaLoopOnServer()
    {
        if (m_bIsDeleted || !GetOwner())
            return;
        if (!Replication.IsServer() || IsPlayerControlled())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        m_iAiLoopRetryCount = m_iAiLoopRetryCount + 1;
        if (m_iAiLoopRetryCount > AI_LOOP_MAX_RETRIES)
            return;
        RSS_LoopStartSystem();
        if (!m_bRssStaminaLoopActive && GetGame())
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.DelayedEnsureAiServer, 3000, false, this);
    }

    protected void RSS_DebugLogAiStaminaTick(
        IEntity owner,
        float staminaPercent,
        float currentWeight,
        float finalSpeedMultiplier,
        float currentSpeed,
        bool isSprinting,
        int currentMovementPhase)
    {
        if (!owner || !IsRssDebugEnabled())
            return;
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            return;

        float nowMs = GetGame().GetWorld().GetWorldTime();
        float aiDebugLastPrint = -1.0;
        if (m_pAIManager)
            aiDebugLastPrint = m_pAIManager.GetDebugLastPrintTime();
        if (aiDebugLastPrint >= 0.0 && (nowMs - aiDebugLastPrint) < 30000.0)
            return;

        if (m_pAIManager)
            m_pAIManager.SetDebugLastPrintTime(nowMs);

        string movementStr = SCR_RSS_DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
        ERSS_AIStaminaState aiState = ERSS_AIStaminaState.FRESH;
        if (m_pAIManager)
            aiState = m_pAIManager.GetStaminaState();
        string stateStr = SCR_RSS_AIStaminaState.StateToString(aiState);
        float fatigueVal = 0.0;
        if (m_pFatigueSystem)
            fatigueVal = m_pFatigueSystem.GetFatigueAccumulation();

        PrintFormat("[RSS] AI: %1 | 状态=%2 体力=%3%% 疲劳=%4% 负重=%5kg 速度倍=%6 速度=%7m/s %8 | %9",
            owner.GetName(),
            stateStr,
            Math.Round(staminaPercent * 100.0).ToString(),
            Math.Round(fatigueVal * 100.0).ToString(),
            Math.Round(currentWeight * 10.0) / 10.0,
            Math.Round(finalSpeedMultiplier * 100.0) / 100.0,
            Math.Round(currentSpeed * 10.0) / 10.0,
            movementStr,
            m_sLastSpeedSource);

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
        float spread = CalcAiGroupSpreadM(scrGrp);
        if (spread <= 0.0)
            return;
        PrintFormat("[RSS] Group: id=%1 分散=%2m 成员=%3",
            scrGrp.GetGroupID().ToString(),
            Math.Round(spread * 10.0) / 10.0,
            GetAliveMemberCount(scrGrp).ToString());
    }

    protected void RSS_DebugOutputPlayerStaminaAndHints(IEntity ownerEnt, RSS_StaminaDebugOutputParams tick)
    {
        if (!ownerEnt || ownerEnt != SCR_PlayerController.GetLocalControlledEntity())
            return;
        if (!tick)
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        bool needDebugOutput = SCR_RSS_DebugBatchManager.IsDebugBatchActive();
        bool needHintOutput = (settings && settings.m_bHintDisplayEnabled);
        if (!needDebugOutput && !needHintOutput)
            return;

        float debugCurrentWeight = 0.0;
        float combatEncumbrancePercent = 0.0;
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
        {
            debugCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            combatEncumbrancePercent = SCR_RSS_MetabolismMath.CalculateCombatEncumbrancePercent(ownerEnt);
        }

        float timeToDepleteSec = -1.0;
        float timeToFullSec = -1.0;
        if (needHintOutput)
        {
            float targetStamina = 1.0;
            if (m_pFatigueSystem)
                targetStamina = m_pFatigueSystem.GetMaxStaminaCap();

            StaminaEtaResult eta = SCR_RSS_UpdateCoordinator.ComputeStaminaEta(
                tick.staminaPercent,
                targetStamina,
                tick.useSwimmingModel,
                tick.currentSpeed,
                tick.totalDrainRate,
                tick.baseDrainRateByVelocity,
                tick.baseDrainRateByVelocityForModule,
                tick.heatStressMultiplier,
                m_pEpocState,
                m_pEncumbranceCache,
                m_pExerciseTracker,
                this,
                m_pEnvironmentFactor,
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
                this,
                m_pEpocState,
                m_pEncumbranceCache,
                m_pExerciseTracker,
                m_pEnvironmentFactor);
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
        debugParams.terrainDetector = m_pTerrainDetector;
        debugParams.environmentFactor = m_pEnvironmentFactor;
        debugParams.heatStressMultiplier = tick.heatStressMultiplier;
        debugParams.rainWeight = tick.rainWeight;
        debugParams.swimmingWetWeight = m_fCurrentWetWeight;
        debugParams.currentSpeed = tick.currentSpeed;
        debugParams.isSwimming = tick.isSwimming;
        debugParams.stanceTransitionManager = m_pStanceTransitionManager;
        debugParams.timeToDepleteSec = timeToDepleteSec;
        debugParams.timeToFullSec = timeToFullSec;
        debugParams.speedSource = m_sLastSpeedSource;
        debugParams.anaerobicPercent = GetRssAnaerobicPercent();
        debugParams.sprintCooldownSec = GetRssSprintCooldownRemainingSec();
        debugParams.burstCooldownFullSec = SCR_RSS_ConfigBridge.GetBurstCooldownFullSeconds();
        if (needDebugOutput)
            SCR_RSS_DebugDisplay.OutputDebugInfo(debugParams);
        if (needHintOutput)
            SCR_RSS_DebugDisplay.OutputHintInfo(debugParams);
    }
}
