//! Thin UpdateLoop orchestrator (ICE: full tick split into RSS_StaminaTickPhaseA/B/C).
//! 状态行代谢诊断（EnforceScript 方法参数上限 16）
class RSS_StatusMetabLogSnapshot
{
    float metabolismPowerW;
    float metabolismPowerMetW;
    float metabolismPowerRawW;
    float effectiveCpW;
    float aerobicPowerW;
    float finalDrainPerTick;
    float metabolicNetPerTick;
    float capRatchetPerTick;
    float netStaminaPerTick;
}

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
    float maxStaminaCap;
    float fatigueIntegralNorm;
    float metabolismPowerW;
    float metabolismPowerMetW;
    float metabolismPowerRawW;
    float effectiveCpW;
    float aerobicPowerW;
    float finalDrainRate;
    float metabolicNetPerTick;
    float capRatchetPerTick;
    float netStaminaPerTick;
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
    float wPrimePool01;
    float landPositionDeltaSpeedMs;
    float overspeedExtraDrainPerSec;
}

//! Cross-phase scratch for stamina tick (ICE split of UpdateSpeedBasedOnStamina)
class RSS_StaminaTickLocals
{
    IEntity owner;
    World world;
    bool isPlayer;
    float staminaPercent;
    float encumbranceSpeedPenalty;
    bool isExhausted;
    bool isSwimmingForSpeed;
    vector velocity;
    float currentSpeed;
    bool isSprintingNow;
    int phaseNow;
    int effectivePhase;
    bool isSprintActive;
    bool sprintIntent;
    float currentTimeForExerciseMs;
    float currentTime;
    float terrainFactor;
    float finalSpeedMultiplier;
    float customSprintSpeedMult;
    float baseSpeedMultiplier;
    float currentWeight;
    float speedToApply;
    float finalSpeedToApply;
    float storedEngineBase;
    bool isCriticalData;
    bool isSwimming;
    float timeDeltaSec;
    float heatStressMultiplier;
    float rainWeight;
    float totalWetWeight;
    float currentWeightWithWet;
    float totalWeight;
    float totalWeightWithWetAndBody;
    bool useSwimmingModel;
    float speedRatio;
    vector velocityForDrain;
    float slopeAngleDegrees;
    ref GradeCalculationResult gradeResult;
    float gradePercent;
    bool isSprinting;
    int currentMovementPhase;
    int effectiveMovementPhase;
    float totalDrainRate;
    float baseDrainRateByVelocity;
    float baseDrainRateByVelocityForModule;
    bool combatStimActive;
    ref StaminaDrainTickParams drainParams;
    ref StaminaDrainTickResult drainTick;
    float effectiveCriticalPowerWattsDbg;
    float environmentMultDbg;
    float powerWattsDbg;
    float wPrimePool01Dbg;
    bool needLocalDebugBatch;
    float staminaBeforeUpdate;
    float maxStaCapDbg;
    float fatigueNormDbg;
    float metabPowerDbg;
    float metabPowerMetDbg;
    float metabPowerRawDbg;
    float metabCpDbg;
    float metabAerobicDbg;
    float finalDrainDbg;
    float metabolicNetDbg;
    float overspeedExtraPerSec;
    float netStaminaTickDbg;
    ref RSS_StatusMetabLogSnapshot metabSnap;
    ref RSS_StaminaDebugOutputParams debugTick;
    float targetStaCapDbg;
    float capShrinkDbg;
    bool epocActiveDbg;
}

modded class SCR_CharacterControllerComponent
{
    // --- ICE split: tick phases (must live in THIS modded class block) ---
//! @return false = early-out (caller should CallLater and return)
    bool RSS_StaminaTickPhaseA(RSS_StaminaTickLocals loc)
    {
        if (m_bIsDeleted)
            return false;
        loc.owner = GetOwner();
        if (!loc.owner)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return false;
        }

        if (!GetGame())
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return false;
        }
        loc.world = GetGame().GetWorld();
        if (!loc.world)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return false;
        }

        if (!ShouldProcessStaminaUpdate())
        {
            RSS_SetMudSlipCameraShake01(0.0);
            m_bRssStaminaLoopActive = false;
            return false;
        }

        if (SCR_PlayerBaseVehicleHelper.HandleVehicleStaminaUpdate(
                this, loc.owner, m_pCompartmentAccess, m_pStaminaComponent,
                m_pExerciseTracker, m_pFatigueSystem, m_pEpocState,
                m_pEncumbranceCache, m_pEnvironmentFactor,
                m_pTerrainDetector, m_pStanceTransitionManager,
                m_fLastStaminaUpdateTime, m_fCurrentWetWeight,
                GetSpeedUpdateIntervalMs(), IsRssDebugEnabled()))
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, GetSpeedUpdateIntervalMs(), false, this);
            return false;
        }
        
        loc.isPlayer = IsPlayerControlled();

        if (loc.isPlayer)
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

        if (!loc.isPlayer && Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
        {
            if (m_pCachedOwnerCharacter)
            {
                SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                if (stimDmgMgr)
                    SCR_RSS_CombatStimController.RefreshBleedingEffectsToMatchScale(stimDmgMgr);
            }
        }

        loc.staminaPercent = GetRssAerobicPercent();
        loc.staminaPercent = Math.Clamp(loc.staminaPercent, 0.0, 1.0);
        
        loc.encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.CheckAndUpdate();
            loc.encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenaltyFraction();
            loc.encumbranceSpeedPenalty = loc.encumbranceSpeedPenalty * SCR_RSS_ConfigBridge.GetCustomEncumbranceSpeedPenaltyMultiplier();
            float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
            loc.encumbranceSpeedPenalty = Math.Clamp(loc.encumbranceSpeedPenalty, 0.0, maxPenalty);
        }

        loc.isExhausted = SCR_RSS_MetabolismMath.IsExhausted(loc.staminaPercent);
        if (loc.isExhausted)
        {
            float limpSpeedMultiplier = SCR_RSS_MetabolismMath.GetDynamicLimpMultiplier(loc.encumbranceSpeedPenalty);
            float compensatedLimpMultiplier = Math.Clamp(limpSpeedMultiplier * m_fAnimSpeedCompensation, 0.01, 1.0);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, compensatedLimpMultiplier);

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
        
        loc.isSwimmingForSpeed = SCR_RSS_SwimmingStateManager.IsSwimming(this);
        if (loc.isSwimmingForSpeed)
        {
            float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
            SpeedCalculationResult speedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                loc.owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
            loc.velocity = speedResult.computedVelocity;
            loc.currentSpeed = Math.Min(speedResult.computedVelocity.Length(), 7.0);
            m_vLastPositionSample = speedResult.lastPositionSample;
            m_bHasLastPositionSample = speedResult.hasLastPositionSample;
            m_vComputedVelocity = speedResult.computedVelocity;
        }
        else
        {
            loc.velocity = GetVelocity();
            loc.currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(loc.velocity);

            float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
            SpeedCalculationResult posSpeedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                loc.owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
            m_vLastPositionSample = posSpeedResult.lastPositionSample;
            m_bHasLastPositionSample = posSpeedResult.hasLastPositionSample;
            m_vComputedVelocity = posSpeedResult.computedVelocity;
            m_fLandPositionDeltaSpeedMs = posSpeedResult.currentSpeed;
        }
        
        loc.isSprintingNow = IsSprinting();
        loc.phaseNow = GetCurrentMovementPhase();
        if (loc.phaseNow >= 1 && loc.phaseNow <= 3)
            m_iLastNonIdleMovementPhase = loc.phaseNow;
        else if (loc.isSprintingNow)
            m_iLastNonIdleMovementPhase = 3;

        loc.effectivePhase = SCR_RSS_SpeedCalculator.ResolveCoastingMovementPhase(
            loc.phaseNow, loc.currentSpeed, m_iLastNonIdleMovementPhase);
        loc.isSprintActive = loc.isSprintingNow || (loc.phaseNow == 3);

        loc.sprintIntent = loc.isSprintActive || GetIsSprintingToggle();
        RSS_PokeEngineStaminaForSprintBlock(loc.sprintIntent);
        
        loc.currentTimeForExerciseMs = loc.world.GetWorldTime();
        loc.currentTime = loc.currentTimeForExerciseMs / 1000.0;

        loc.terrainFactor = 1.0;
        if (m_pTerrainDetector)
            loc.terrainFactor = m_pTerrainDetector.GetTerrainFactor(loc.owner, loc.currentTime, loc.currentSpeed);

        if (m_pEnvironmentFactor)
            m_pEnvironmentFactor.UpdateEnvironmentFactors(loc.currentTime, loc.owner, loc.velocity, loc.terrainFactor, m_fCurrentWetWeight);

        loc.finalSpeedMultiplier = SCR_RSS_UpdateCoordinator.UpdateSpeed(
            this,
            loc.staminaPercent,
            loc.encumbranceSpeedPenalty,
            m_pCollapseTransition,
            loc.currentSpeed,
            m_pEnvironmentFactor,
            m_pSlopeSpeedTransition,
            loc.velocity,
            loc.terrainFactor,
            loc.effectivePhase);

        loc.customSprintSpeedMult = SCR_RSS_ConfigBridge.GetCustomSprintSpeedMultiplier();
        if (loc.customSprintSpeedMult != 1.0)
            loc.finalSpeedMultiplier = loc.finalSpeedMultiplier * loc.customSprintSpeedMult;

        if (m_pSprintBlockSpeedTransition)
        {
            bool sprintAllowed = GetRssSprintAllowed();
            float engineBaseForLimit = GetRssSpeedLimitEngineBaseMs();
            float targetAbsoluteSpeedMs = loc.finalSpeedMultiplier * engineBaseForLimit;
            float lastEngineBase = m_fLastRssEngineBaseForLimit;
            if (lastEngineBase <= 0.1)
                lastEngineBase = engineBaseForLimit;
            loc.finalSpeedMultiplier = m_pSprintBlockSpeedTransition.UpdateAndGet(
                loc.currentTime,
                targetAbsoluteSpeedMs,
                engineBaseForLimit,
                sprintAllowed,
                m_fLastRssSpeedMultiplierApplied,
                lastEngineBase);
        }
        
        loc.baseSpeedMultiplier = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(
            loc.staminaPercent, loc.effectivePhase, loc.encumbranceSpeedPenalty);
        
        loc.currentWeight = 0.0;
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            loc.currentWeight = m_pEncumbranceCache.GetCurrentWeight();
        else
        {
            if (!m_pCachedInventoryComponent)
                m_pCachedInventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(loc.owner.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (m_pCachedInventoryComponent)
                loc.currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
        }

        loc.speedToApply = loc.finalSpeedMultiplier;
        if (!Replication.IsServer() && IsPlayerControlled() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled() && m_pNetworkSyncManager.HasServerValidation())
        {
            m_pNetworkSyncManager.GetTargetSpeedMultiplier(loc.finalSpeedMultiplier);
            loc.speedToApply = m_pNetworkSyncManager.GetSmoothedSpeedMultiplier(loc.currentTime);
        }
        loc.finalSpeedToApply = Math.Clamp(loc.speedToApply, 0.01, 3.0);
        m_fLastRssSpeedMultiplierApplied = loc.finalSpeedToApply;
        loc.storedEngineBase = GetRssSpeedLimitEngineBaseMs();
        if (loc.storedEngineBase > 0.1)
            m_fLastRssEngineBaseForLimit = loc.storedEngineBase;
        m_fAppliedSpeedLimitMs = loc.finalSpeedToApply * loc.storedEngineBase;
        if (m_fAppliedSpeedLimitMs <= 0.05)
            m_fAppliedSpeedLimitMs = -1.0;
        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, loc.finalSpeedToApply);
        if (IsPlayerControlled())
            m_sLastSpeedSource = "Client";
        else
            m_sLastSpeedSource = "Server";

        if (!loc.isPlayer && SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, GetSpeedUpdateIntervalMs(), false, this);
            return false;
        }

        loc.isCriticalData = (loc.staminaPercent <= 0.05 || (m_pNetworkSyncManager && m_pNetworkSyncManager.GetLastReportedStaminaPercent() > 0.5 && loc.staminaPercent <= 0.1));
        if (loc.isPlayer && !Replication.IsServer() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled())
        {
            int syncType = 0;
            if (loc.isCriticalData)
                syncType = 1;
            if (m_pNetworkSyncManager.ShouldSync(loc.currentTime, syncType))
            {
                if (!SCR_RSS_ConfigManager.GetServerDataExportEnabled())
                    return false;
                Rpc(RPC_ClientReportStamina, loc.staminaPercent, loc.currentWeight, loc.currentTime, loc.isCriticalData);
                if (loc.isCriticalData && IsRssDebugEnabled())
                    PrintFormat("[RSS] Critical stamina event reported (stamina=%1)", loc.staminaPercent);
            }
        }

        return true;
    }

//! @return false = early-out (caller should CallLater and return)
    bool RSS_StaminaTickPhaseB(RSS_StaminaTickLocals loc)
    {
        loc.isSwimming = SCR_RSS_SwimmingStateManager.IsSwimming(this);

        if (m_fLastStaminaUpdateTime >= 0.0)
            loc.timeDeltaSec = loc.currentTime - m_fLastStaminaUpdateTime;
        else
            loc.timeDeltaSec = GetSpeedUpdateIntervalMs() / 1000.0;
        loc.timeDeltaSec = Math.Clamp(loc.timeDeltaSec, 0.01, 0.5);

        if (loc.isSwimming != m_bWasSwimming)
            m_bSwimmingVelocityDebugPrinted = false;
        
        if (loc.isPlayer)
        {
            WetWeightUpdateResult wetWeightResult = SCR_RSS_SwimmingStateManager.UpdateWetWeight(
                m_bWasSwimming,
                loc.isSwimming,
                loc.currentTime,
                m_fWetWeightStartTime,
                m_fCurrentWetWeight,
                m_fSwimStartTimeSec,
                loc.owner);
            m_fWetWeightStartTime = wetWeightResult.wetWeightStartTime;
            m_fCurrentWetWeight = wetWeightResult.currentWetWeight;
            m_fSwimStartTimeSec = wetWeightResult.swimStartTimeSec;
            m_bWasSwimming = loc.isSwimming;
        }
        
        loc.heatStressMultiplier = 1.0;
        if (m_pEnvironmentFactor)
            loc.heatStressMultiplier = m_pEnvironmentFactor.GetHeatStressMultiplier();
        
        loc.rainWeight = 0.0;
        if (m_pEnvironmentFactor)
            loc.rainWeight = m_pEnvironmentFactor.GetRainWeight();

        if (loc.isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
        {
            loc.heatStressMultiplier = 1.0;
            loc.rainWeight = 0.0;
        }
        
        loc.totalWetWeight = SCR_RSS_SwimmingStateManager.CalculateTotalWetWeight(m_fCurrentWetWeight, loc.rainWeight);
        loc.currentWeightWithWet = loc.currentWeight + loc.totalWetWeight;

        loc.totalWeight = loc.currentWeight + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        loc.totalWeightWithWetAndBody = loc.currentWeightWithWet + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;

        loc.useSwimmingModel = loc.isSwimming;

        if (loc.isPlayer && m_pStaminaComponent && m_pJumpVaultDetector && m_pStanceTransitionManager)
        {
            float currentTimeSec = loc.world.GetWorldTime() / 1000.0;
            loc.staminaPercent = SCR_RSS_UpdateCoordinator.ApplyPlayerActionStaminaCosts(
                this,
                loc.owner,
                loc.staminaPercent,
                loc.timeDeltaSec,
                currentTimeSec,
                m_pJumpVaultDetector,
                m_pStanceTransitionManager,
                m_pEncumbranceCache,
                m_pUISignalBridge,
                IsRssDebugEnabled());
        }
        
        loc.speedRatio = Math.Clamp(loc.currentSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED, 0.0, 1.0);

        loc.velocityForDrain = loc.velocity;
        if (loc.useSwimmingModel && !HasSwimInput())
            loc.velocityForDrain = vector.Zero;

        loc.slopeAngleDegrees = 0.0;
        loc.gradeResult = SCR_RSS_SpeedCalculator.CalculateGradePercent(
            this,
            loc.currentSpeed,
            m_pJumpVaultDetector,
            loc.slopeAngleDegrees,
            m_pEnvironmentFactor,
            loc.velocityForDrain);
        loc.gradePercent = loc.gradeResult.gradePercent;
        loc.slopeAngleDegrees = loc.gradeResult.slopeAngleDegrees;

        if (!loc.isExhausted)
        {
            float engineBase = GetRssSpeedLimitEngineBaseMs();
            if (engineBase <= 0.05)
                engineBase = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

            SCR_RSS_CriticalPowerModel cpModel = null;
            if (m_pAnaerobicBurst)
                cpModel = m_pAnaerobicBurst.GetCpModel();

            float correctedSpeed = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                m_fLastRssSpeedMultiplierApplied,
                loc.currentSpeed,
                loc.phaseNow,
                loc.encumbranceSpeedPenalty,
                loc.totalWeightWithWetAndBody,
                loc.gradePercent,
                loc.terrainFactor,
                loc.isExhausted,
                engineBase,
                loc.currentTime,
                cpModel);
            if (correctedSpeed != m_fLastRssSpeedMultiplierApplied)
            {
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, correctedSpeed);
                m_fLastRssSpeedMultiplierApplied = correctedSpeed;
                loc.finalSpeedMultiplier = correctedSpeed;
                m_fAppliedSpeedLimitMs = correctedSpeed * engineBase;
            }
        }

        if (m_pAnaerobicBurst)
        {
            bool tickAnaerobic = Replication.IsServer();

            if (tickAnaerobic)
            {
                SCR_RSS_CriticalPowerModel cpModel = m_pAnaerobicBurst.GetCpModel();
                float pool01BeforeTick = 1.0;
                if (cpModel)
                    pool01BeforeTick = cpModel.GetPool01();

                float powerW = SCR_RSS_DrainCalculator.GetMetabolicAccountingPowerWatts(
                    loc.currentSpeed,
                    m_fAppliedSpeedLimitMs,
                    loc.totalWeightWithWetAndBody,
                    loc.gradePercent,
                    loc.terrainFactor,
                    loc.effectivePhase,
                    pool01BeforeTick);

                if (cpModel)
                {
                    float loadKg = Math.Max(loc.totalWeightWithWetAndBody - SCR_RSS_MetabolismMath.CHARACTER_WEIGHT, 0.0);
                    float envCpMult = 1.0;
                    if (m_pEnvironmentFactor && SCR_RSS_ConfigBridge.IsHeatStressEnabled())
                    {
                        float heatPen = m_pEnvironmentFactor.GetHeatStressPenalty();
                        envCpMult = 1.0 - heatPen * 0.35;
                    }
                    float fatigueNorm = 0.0;
                    if (m_pFatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
                        fatigueNorm = m_pFatigueSystem.GetFatigueIntegralNorm();
                    cpModel.SetRuntimeContext(loadKg, loc.gradePercent, envCpMult, fatigueNorm);
                    if (m_pFatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
                    {
                        float cpMult = m_pFatigueSystem.GetCpFatigueMultiplier();
                        cpModel.SetFatigueCpMultiplier(cpMult);
                    }
                    else
                    {
                        cpModel.SetFatigueCpMultiplier(1.0);
                    }
                }

                m_pAnaerobicBurst.TickPower(powerW, loc.isSprintActive, loc.currentTime, loc.timeDeltaSec, loc.currentSpeed);
                if (m_pEpocState)
                {
                    // EPOC 峰值只用 v_drain 功率，避免缓降速期间 v_meas 超速把停步罚抬爆
                    float powerForEpoc = SCR_RSS_DrainCalculator.GetMetabolicFatiguePowerWatts(
                        loc.currentSpeed,
                        m_fAppliedSpeedLimitMs,
                        loc.totalWeightWithWetAndBody,
                        loc.gradePercent,
                        loc.terrainFactor,
                        loc.phaseNow);
                    m_pEpocState.UpdateExercisePowerSample(powerForEpoc, loc.currentSpeed);
                }
                if (m_pStaminaState)
                {
                    m_pStaminaState.SetWPrimePoolFromCpModel(m_pAnaerobicBurst.GetCpModel());
                    m_pStaminaState.SetAerobic(loc.staminaPercent);
                }
                if (Replication.IsServer())
                {
                    SCR_RSS_NetworkSyncManager.ReadAnaerobicForReplication(
                        m_pAnaerobicBurst, m_fReplAnaerobicPool, m_fReplAnaerobicCooldownUntil);
                    Replication.BumpMe();
                }
            }
        }

        if (!loc.isExhausted && !loc.useSwimmingModel && loc.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
        {
            SCR_RSS_CriticalPowerModel cpPostTick = null;
            if (m_pAnaerobicBurst)
                cpPostTick = m_pAnaerobicBurst.GetCpModel();

            float pool01AfterTick = 1.0;
            if (cpPostTick)
                pool01AfterTick = cpPostTick.GetPool01();

            bool overspeeding = SCR_RSS_DrainCalculator.IsMetabolicOverspeedAccounting(
                loc.currentSpeed, m_fAppliedSpeedLimitMs);
            bool wPrimeAllowsOverspeed = SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(
                pool01AfterTick);

            // 客户端实测仍高于 RSS 限速（缓降残留/惯性）：每帧硬钳重申；
            // W′ 不可用时再与代谢 CP 反解上限取更严者
            if (overspeeding)
            {
                float engineBase = GetRssSpeedLimitEngineBaseMs();
                if (engineBase <= 0.05)
                    engineBase = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

                float hardMult = m_fLastRssSpeedMultiplierApplied;
                if (!wPrimeAllowsOverspeed)
                {
                    float wPrimeCapMs = SCR_RSS_DrainCalculator.GetWPrimeExhaustedOverspeedCapMs(
                        loc.currentSpeed,
                        m_fAppliedSpeedLimitMs,
                        pool01AfterTick,
                        loc.phaseNow,
                        loc.totalWeightWithWetAndBody,
                        loc.gradePercent,
                        loc.terrainFactor,
                        cpPostTick);
                    if (wPrimeCapMs > 0.05)
                    {
                        float metabMult = Math.Clamp(wPrimeCapMs / engineBase, 0.01, 3.0);
                        if (metabMult < hardMult)
                            hardMult = metabMult;
                    }
                }

                SCR_RSS_SpeedBridge.ApplyHardStaminaSpeedClamp(loc.owner, hardMult);
                m_fLastRssSpeedMultiplierApplied = hardMult;
                loc.finalSpeedMultiplier = hardMult;
                m_fAppliedSpeedLimitMs = hardMult * engineBase;
            }
        }

        if (m_pFatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled()
            && !loc.useSwimmingModel && loc.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
        {
            float capBeforeFatigue = m_pFatigueSystem.GetMaxStaminaCap();
            float powerFat = SCR_RSS_DrainCalculator.GetMetabolicFatiguePowerWatts(
                loc.currentSpeed,
                m_fAppliedSpeedLimitMs,
                loc.totalWeightWithWetAndBody,
                loc.gradePercent,
                loc.terrainFactor,
                loc.phaseNow);
            m_pFatigueSystem.ProcessFatigueIntegral(
                powerFat,
                loc.currentWeight,
                loc.gradePercent,
                loc.terrainFactor,
                loc.timeDeltaSec,
                loc.currentSpeed);

            m_fLastCapRatchetPerTick = capBeforeFatigue - m_pFatigueSystem.GetMaxStaminaCap();
        }
        else
        {
            m_fLastCapRatchetPerTick = 0.0;
        }

        if (SCR_RSS_ConfigBridge.IsMudSlipMechanismEnabled())
        {
            if (m_pMudSlipRunner)
            {
                m_pMudSlipRunner.ProcessAfterSlope(
                    this,
                    loc.useSwimmingModel,
                    loc.isSwimming,
                    m_pEnvironmentFactor,
                    m_pStaminaComponent,
                    loc.currentSpeed,
                    loc.isSprintActive,
                    loc.currentWeight,
                    loc.staminaPercent,
                    loc.velocity,
                    loc.slopeAngleDegrees,
                    loc.timeDeltaSec,
                    loc.currentTime,
                    IsRssDebugEnabled());
            }

        }
        else
        {
            RSS_SetMudSlipCameraShake01(0.0);
        }
        if (!loc.isPlayer && Replication.IsServer() && m_pAIManager)
        {
            float fatigueVal;
            if (m_pFatigueSystem)
                fatigueVal = m_pFatigueSystem.GetFatigueAccumulation();
            else
                fatigueVal = 0.0;

            m_pAIManager.Tick(
                loc.owner, loc.currentTime, loc.timeDeltaSec,
                loc.staminaPercent, fatigueVal, loc.currentSpeed, loc.isPlayer);
        }

        loc.isSprinting = loc.isSprintingNow;
        loc.currentMovementPhase = loc.phaseNow;
        loc.effectiveMovementPhase = loc.effectivePhase;
        loc.totalDrainRate = 0.0;
        loc.baseDrainRateByVelocity = 0.0;
        loc.baseDrainRateByVelocityForModule = 0.0;

        loc.combatStimActive = false;
        if (loc.isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
            loc.combatStimActive = true;

        return true;
    }

//! @return false = early-out (caller should CallLater and return)

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
            return;
        }

        RSS_StaminaTickLocals loc = new RSS_StaminaTickLocals();
        if (!RSS_StaminaTickPhaseA(loc))
            return;
        if (!RSS_StaminaTickPhaseB(loc))
            return;
        RSS_StaminaTickPhaseC(loc);

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

        if (IsRssDebugEnabled() && IsPlayerControlled())
        {
            string hasStamina = "0";
            if (m_pStaminaComponent)
                hasStamina = "1";
            PrintFormat(
                "[RSS] Player stamina loop started (interval=%1ms, staminaComp=%2)",
                intervalMs,
                hasStamina);
        }
        
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

}
