//! Thin UpdateLoop orchestrator (ICE: full tick split into RSS_StaminaTickPhaseA/B/C).
//! Tick DTOs: SCR_RSS_StaminaTickTypes.c

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
            RSS_ScheduleNextStaminaTick();
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
            float limpAbs = SCR_RSS_MetabolismMath.GetDynamicLimpSpeedMs(loc.encumbranceSpeedPenalty);
            limpAbs = limpAbs * m_fAnimSpeedCompensation;
            if (limpAbs < SCR_RSS_Constants.EXHAUSTION_LIMP_SPEED)
                limpAbs = SCR_RSS_Constants.EXHAUSTION_LIMP_SPEED;
            if (IsPlayerControlled())
            {
                float phaseTop = GetRssSpeedLimitEngineBaseMs();
                if (phaseTop < 0.1)
                    phaseTop = GetOriginalEngineMaxSpeed_Run();
                float limpFrac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(limpAbs, phaseTop);
                limpFrac = SCR_RSS_DrainCalculator.ClampSpeedLimitFractionToGaitBand(limpFrac, false);
                limpAbs = limpFrac * phaseTop;
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, limpFrac);
                m_fLastRssSpeedMultiplierApplied = limpFrac;
                float safeCap = SCR_RSS_SpeedBridge.GetPhaseSafePhysicsCapMs(
                    limpAbs, phaseTop, false, GetCurrentMovementPhase());
                m_fAppliedSpeedLimitMs = safeCap;
                if (SCR_RSS_SpeedBridge.IsHorizontalSpeedClampEnabled())
                    SCR_RSS_SpeedBridge.ClampOwnerHorizontalSpeed(loc.owner, safeCap);
            }
            else
            {
                float limpSpeedMultiplier = SCR_RSS_MetabolismMath.GetDynamicLimpMultiplier(loc.encumbranceSpeedPenalty);
                float compensatedLimpMultiplier = Math.Clamp(limpSpeedMultiplier * m_fAnimSpeedCompensation, 0.01, 1.0);
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, compensatedLimpMultiplier);
            }

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
            RSS_SpeedCalculationResult speedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
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
            RSS_SpeedCalculationResult posSpeedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
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

        if (m_pSprintBlockSpeedTransition && !loc.isExhausted)
        {
            bool sprintAllowed = GetRssSprintAllowed();
            // 过渡与落盘限速必须共用同一 engineBase（按有效相位，Walk 不可再用 Run 分母）
            // 禁 Sprint 但引擎仍停在 phase 3 时仍用 Sprint 分母，否则 SetSpeedLimit 会按冲刺顶速放大
            float engineBaseForLimit = GetOriginalEngineMaxSpeed_Run();
            if (loc.isSprintingNow || loc.phaseNow == 3)
                engineBaseForLimit = GetOriginalEngineMaxSpeed_Sprint();
            else if (loc.effectivePhase == 1 || loc.phaseNow == 1)
                engineBaseForLimit = GetOriginalEngineMaxSpeed_Walk();
            float targetAbsoluteSpeedMs = loc.finalSpeedMultiplier * engineBaseForLimit;
            float lastEngineBase = m_fLastRssEngineBaseForLimit;
            if (lastEngineBase <= 0.1)
                lastEngineBase = engineBaseForLimit;
            bool overspeedArmedForTransition = true;
            if (m_pAnaerobicBurst && m_pAnaerobicBurst.GetCpModel())
            {
                if (!SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(
                    m_pAnaerobicBurst.GetCpModel()))
                    overspeedArmedForTransition = false;
            }
            loc.finalSpeedMultiplier = m_pSprintBlockSpeedTransition.UpdateAndGet(
                loc.currentTime,
                targetAbsoluteSpeedMs,
                engineBaseForLimit,
                sprintAllowed,
                m_fLastRssSpeedMultiplierApplied,
                lastEngineBase,
                overspeedArmedForTransition);
            m_fLastRssEngineBaseForLimit = engineBaseForLimit;
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

        // 与过渡器同一 engineBase 还原绝对速（避免 Run/Sprint/Walk 分母切换跳变）
        float refEngineBase = m_fLastRssEngineBaseForLimit;
        if (refEngineBase <= 0.1)
            refEngineBase = GetRssSpeedLimitEngineBaseMs();
        if (refEngineBase <= 0.1)
        {
            refEngineBase = GetOriginalEngineMaxSpeed_Run();
            if (loc.isSprintingNow || loc.phaseNow == 3)
                refEngineBase = GetOriginalEngineMaxSpeed_Sprint();
            else if (loc.effectivePhase == 1 || loc.phaseNow == 1)
                refEngineBase = GetOriginalEngineMaxSpeed_Walk();
        }
        float desiredAbsMs = loc.finalSpeedToApply * refEngineBase;

        loc.storedEngineBase = refEngineBase;
        if (loc.storedEngineBase > 0.1)
            m_fLastRssEngineBaseForLimit = loc.storedEngineBase;
        m_fAppliedSpeedLimitMs = desiredAbsMs;
        if (m_fAppliedSpeedLimitMs <= 0.05)
            m_fAppliedSpeedLimitMs = -1.0;
        if (!SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
        {
            // 试跑：清掉体力限速源，不硬钳；代谢用 v_meas（applied=-1）
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, 1.0);
            RSS_RestoreNativeMovementMaxSpeed(loc.owner);
            m_fAppliedSpeedLimitMs = -1.0;
            m_fLastRssSpeedMultiplierApplied = 1.0;
        }
        else if (IsPlayerControlled())
        {
            // CP 巡航：SetSpeedLimit 只改指令速，物理仍可跑飞 → 超速时钳水平速度
            bool cruiseDisarmed = false;
            if (m_pAnaerobicBurst && m_pAnaerobicBurst.GetCpModel())
            {
                if (!SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(
                    m_pAnaerobicBurst.GetCpModel()))
                    cruiseDisarmed = true;
            }

            // 始终保住 RSS 限速源：SetSpeedLimit(1.0) 会拆掉 source，Sprint→Run 会瞬间 uncapped。
            bool keepSpeedSource = true;
            if (loc.phaseNow == 1 || loc.effectivePhase == 1)
            {
                float walkTopMs = loc.storedEngineBase;
                if (walkTopMs > 0.1 && desiredAbsMs > walkTopMs)
                    desiredAbsMs = walkTopMs;
            }

            float desiredFrac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                desiredAbsMs, loc.storedEngineBase, keepSpeedSource);
            if (!loc.isExhausted)
                desiredFrac = RSS_SlewSpeedLimitFraction(desiredFrac, loc.currentTime);
            if (desiredFrac >= 0.999)
                desiredFrac = 0.999;
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, desiredFrac);
            float slewedAbsMs = desiredFrac * loc.storedEngineBase;
            float safeCap = SCR_RSS_SpeedBridge.GetPhaseSafePhysicsCapMs(
                slewedAbsMs,
                loc.storedEngineBase,
                loc.isSprintingNow,
                loc.phaseNow);
            m_fAppliedSpeedLimitMs = safeCap;
            m_fLastRssSpeedMultiplierApplied = desiredFrac;
            if (loc.storedEngineBase > 0.1)
                m_fLastRssSpeedMultiplierApplied = safeCap / loc.storedEngineBase;
            if (m_fLastRssSpeedMultiplierApplied > 0.999)
                m_fLastRssSpeedMultiplierApplied = 0.999;
            if (m_fLastRssSpeedMultiplierApplied < 0.01)
                m_fLastRssSpeedMultiplierApplied = 0.01;
            if (SCR_RSS_SpeedBridge.IsHorizontalSpeedClampEnabled())
                SCR_RSS_SpeedBridge.ClampOwnerHorizontalSpeed(loc.owner, safeCap);
            if (SCR_RSS_SpeedBridge.IsMovementMaxSpeedTrialEnabled())
                RSS_ApplyTrialMovementMaxSpeed(loc.owner, safeCap);

            // 仅当 V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP=true 时生效；默认不压速只扣条
            if (SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
            {
                bool enforceCruisePhys = cruiseDisarmed;
                if (loc.phaseNow == 1 || loc.effectivePhase == 1)
                    enforceCruisePhys = true;
                if (enforceCruisePhys)
                {
                    float clampDt = GetSpeedUpdateIntervalMs() / 1000.0;
                    if (clampDt < 0.01)
                        clampDt = 0.05;
                    int clampPhase = loc.phaseNow;
                    if (loc.effectivePhase == 1)
                        clampPhase = 1;
                    SCR_RSS_SpeedBridge.EnforceCpCruisePhysicsCap(
                        loc.owner,
                        safeCap,
                        loc.currentSpeed,
                        clampDt,
                        RSS_GetSmoothedGradePercentForSpeed(),
                        clampPhase);
                }
            }
        }
        else
        {
            float aiFrac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                desiredAbsMs, loc.storedEngineBase, false);
            if (aiFrac > 1.0)
                aiFrac = 1.0;
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, aiFrac);
            m_fLastRssSpeedMultiplierApplied = aiFrac;
        }

        if (IsPlayerControlled())
        {
            bool inVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(m_pCompartmentAccess);
            bool wPrimeEmpty = true;
            if (m_pAnaerobicBurst && m_pAnaerobicBurst.GetCpModel())
            {
                if (SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(
                    m_pAnaerobicBurst.GetCpModel()))
                    wPrimeEmpty = false;
            }
            bool holdingMove = false;
            CharacterInputContext moveCtx = GetInputContext();
            if (moveCtx)
                holdingMove = moveCtx.IsMoving();
            if (loc.isSprintingNow)
                holdingMove = true;
            vector moveInput = GetMovementInput();
            float moveInputSq = moveInput[0] * moveInput[0] + moveInput[2] * moveInput[2];
            if (moveInputSq > 0.04)
                holdingMove = true;
            RSS_UpdateCpOutOfBandWalkOverride(
                loc.isSwimmingForSpeed, inVehicle, holdingMove, wPrimeEmpty);
        }

        if (IsPlayerControlled())
            m_sLastSpeedSource = "Client";
        else
            m_sLastSpeedSource = "Server";

        if (!loc.isPlayer && SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
        {
            RSS_ScheduleNextStaminaTick();
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
            RSS_WetWeightUpdateResult wetWeightResult = SCR_RSS_SwimmingStateManager.UpdateWetWeight(
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

        // 默认 drain-only：CP 代谢伺服关时整块跳过（避免坡度平滑 + 空调用）
        if (!loc.isExhausted && SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
        {
            float engineBase = GetRssSpeedLimitEngineBaseMs();
            if (engineBase <= 0.05)
                engineBase = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;

            SCR_RSS_CriticalPowerModel cpModel = null;
            if (m_pAnaerobicBurst)
                cpModel = m_pAnaerobicBurst.GetCpModel();

            // 禁 Sprint 时按 Run 相位做代谢压速，避免 phase=3 + W′≈0 走冲刺功率悬崖。
            // 惯性滑行引擎 phase=Idle：用有效相位，否则跳过巡航帽导致过脊后限速乱跳。
            int metabPhase = loc.phaseNow;
            if (loc.phaseNow == 0 && loc.effectivePhase >= 1)
                metabPhase = loc.effectivePhase;
            if (!GetRssSprintAllowed())
            {
                if (metabPhase == 3)
                    metabPhase = 2;
            }

            float gradeForCap = RSS_SmoothGradePercentForSpeed(loc.gradePercent, loc.currentTime);
            float correctedSpeed = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                m_fLastRssSpeedMultiplierApplied,
                loc.currentSpeed,
                metabPhase,
                loc.encumbranceSpeedPenalty,
                loc.totalWeightWithWetAndBody,
                gradeForCap,
                loc.terrainFactor,
                loc.isExhausted,
                engineBase,
                loc.currentTime,
                cpModel,
                m_fAppliedSpeedLimitMs);
            if (correctedSpeed != m_fLastRssSpeedMultiplierApplied
                && SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
            {
                if (IsPlayerControlled())
                {
                    float hardFrac = RSS_SlewSpeedLimitFraction(correctedSpeed, loc.currentTime);
                    if (hardFrac > 0.999)
                        hardFrac = 0.999;
                    SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, hardFrac);
                    float hardAbs = hardFrac * engineBase;
                    float safeCap = SCR_RSS_SpeedBridge.GetPhaseSafePhysicsCapMs(
                        hardAbs, engineBase, loc.isSprintingNow, loc.phaseNow);
                    m_fAppliedSpeedLimitMs = safeCap;
                    m_fLastRssSpeedMultiplierApplied = hardFrac;
                    loc.finalSpeedMultiplier = hardFrac;
                    if (SCR_RSS_SpeedBridge.IsHorizontalSpeedClampEnabled())
                        SCR_RSS_SpeedBridge.ClampOwnerHorizontalSpeed(loc.owner, safeCap);
                    if (SCR_RSS_Constants.V6_CP_CRUISE_OVERSPEED_PHYSICS_CLAMP)
                    {
                        bool metabDisarmed = false;
                        if (cpModel)
                        {
                            if (!SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(cpModel))
                                metabDisarmed = true;
                        }
                        if (metabDisarmed || loc.phaseNow == 1)
                        {
                            SCR_RSS_SpeedBridge.EnforceCpCruisePhysicsCap(
                                loc.owner,
                                safeCap,
                                loc.currentSpeed,
                                loc.timeDeltaSec,
                                loc.gradePercent,
                                loc.phaseNow);
                        }
                    }
                }
                else
                {
                    SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, correctedSpeed);
                    m_fLastRssSpeedMultiplierApplied = correctedSpeed;
                    m_fAppliedSpeedLimitMs = correctedSpeed * engineBase;
                    loc.finalSpeedMultiplier = correctedSpeed;
                }
            }
        }

        float replPoolTmp = m_fReplAnaerobicPool;
        float replCooldownTmp = m_fReplAnaerobicCooldownUntil;
        SCR_PlayerBaseWPrimeTickHelper.TickPhaseBAnaerobic(
            this,
            loc,
            m_pAnaerobicBurst,
            m_pStaminaState,
            m_pEnvironmentFactor,
            m_pFatigueSystem,
            m_pEpocState,
            m_fAppliedSpeedLimitMs,
            RSS_IsCpWalkOverrideActive(),
            replPoolTmp,
            replCooldownTmp);
        m_fReplAnaerobicPool = replPoolTmp;
        m_fReplAnaerobicCooldownUntil = replCooldownTmp;

        SCR_PlayerBaseOverspeedClampHelper.ApplyPostTickOverspeedClamp(
            this,
            loc,
            m_pAnaerobicBurst,
            m_pSprintBlockSpeedTransition,
            m_fAppliedSpeedLimitMs,
            m_fLastRssSpeedMultiplierApplied,
            m_fLastRssEngineBaseForLimit);

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
            float fatigueCpWatts = -1.0;
            if (m_pAnaerobicBurst)
            {
                SCR_RSS_CriticalPowerModel cpForFatigue = m_pAnaerobicBurst.GetCpModel();
                if (cpForFatigue)
                    fatigueCpWatts = cpForFatigue.GetEffectiveCriticalPowerWatts();
            }
            m_pFatigueSystem.ProcessFatigueIntegral(
                powerFat,
                loc.currentWeight,
                loc.gradePercent,
                loc.terrainFactor,
                loc.timeDeltaSec,
                loc.currentSpeed,
                fatigueCpWatts);

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

        RSS_ScheduleNextStaminaTick();
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
        RSS_ScheduleNextStaminaTick();

        if (IsRssDebugEnabled() && IsPlayerControlled())
        {
            string hasStamina = "0";
            if (m_pStaminaComponent)
                hasStamina = "1";
            PrintFormat(
                "[RSS] Player stamina loop started (interval=%1ms, staminaComp=%2)",
                GetSpeedUpdateIntervalMs(),
                hasStamina);
        }

        if (IsRssDebugEnabled())
        {
            ScriptCallQueue sampleQueue = SCR_RSS_RuntimeGuard.GetCallqueueOrNull();
            if (sampleQueue)
                sampleQueue.CallLater(SCR_PlayerBaseLoop.CollectSpeedSampleBridge, SPEED_SAMPLE_INTERVAL_MS, false, this);
        }
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
        if (!m_bRssStaminaLoopActive)
        {
            ScriptCallQueue aiQueue = SCR_RSS_RuntimeGuard.GetCallqueueOrNull();
            if (aiQueue)
                aiQueue.CallLater(SCR_PlayerBaseLoop.DelayedEnsureAiServer, 3000, false, this);
        }
    }

}
