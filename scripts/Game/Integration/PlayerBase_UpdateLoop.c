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

        // 进服/AI 生成：启发式 Game/World/Anim 未就绪则跳过；测速只用位置差分。
        if (!SCR_PlayerBaseRssApiHelper.IsCharacterMotionReady(loc.owner, this))
        {
            RSS_ScheduleNextStaminaTick();
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

        // AI：永不进玩家 Phase B/C（避开 UpdateSpeed/环境全链）。
        // DisableAIStaminaCalc → 仅廉价限速；否则走 AI 专用体力管线（同源代谢核 + Y 估坡）。
        if (!loc.isPlayer)
        {
            if (SCR_RSS_ConfigBridge.IsAiStaminaCalcDisabled())
            {
                float cheapFrac = 1.0;
                float cheapEnc = 0.0;
                float cheapSta = 1.0;
                int cheapPhase = 2;
                bool cheapExhausted = false;
                SCR_PlayerBaseAiLightTickHelper.ApplyCheapAiSpeed(
                    this, loc.owner, m_pEncumbranceCache, m_fAnimSpeedCompensation,
                    cheapFrac, cheapEnc, cheapSta, cheapPhase, cheapExhausted);
                m_fLastRssSpeedMultiplierApplied = cheapFrac;
                m_fAppliedSpeedLimitMs = -1.0;
                RSS_ScheduleNextStaminaTick();
                return false;
            }

            if (!m_pAIStaminaPipeline)
                m_pAIStaminaPipeline = new SCR_RSS_AIStaminaPipeline();

            RSS_AIStaminaPipelineContext aiCtx = new RSS_AIStaminaPipelineContext();
            aiCtx.ctrl = this;
            aiCtx.owner = loc.owner;
            aiCtx.world = loc.world;
            aiCtx.staminaComponent = m_pStaminaComponent;
            aiCtx.staminaState = m_pStaminaState;
            aiCtx.encumbranceCache = m_pEncumbranceCache;
            aiCtx.anaerobicBurst = m_pAnaerobicBurst;
            aiCtx.fatigueSystem = m_pFatigueSystem;
            aiCtx.epocState = m_pEpocState;
            aiCtx.exerciseTracker = m_pExerciseTracker;
            aiCtx.terrainDetector = m_pTerrainDetector;
            aiCtx.aiManager = m_pAIManager;
            aiCtx.animSpeedCompensation = m_fAnimSpeedCompensation;
            aiCtx.lastStaminaUpdateTime = m_fLastStaminaUpdateTime;
            aiCtx.lastPositionSample = m_vLastPositionSample;
            aiCtx.hasLastPositionSample = m_bHasLastPositionSample;
            aiCtx.computedVelocity = m_vComputedVelocity;
            aiCtx.appliedSpeedLimitMs = m_fAppliedSpeedLimitMs;
            aiCtx.lastRssSpeedMultiplierApplied = m_fLastRssSpeedMultiplierApplied;

            m_fLastStaminaUpdateTime = m_pAIStaminaPipeline.Tick(aiCtx);
            m_vLastPositionSample = aiCtx.lastPositionSample;
            m_bHasLastPositionSample = aiCtx.hasLastPositionSample;
            m_vComputedVelocity = aiCtx.computedVelocity;
            m_fAppliedSpeedLimitMs = aiCtx.appliedSpeedLimitMs;
            m_fLastRssSpeedMultiplierApplied = aiCtx.lastRssSpeedMultiplierApplied;

            RSS_ScheduleNextStaminaTick();
            return false;
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
            float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
            RSS_SpeedCalculationResult posSpeedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                loc.owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
            m_vLastPositionSample = posSpeedResult.lastPositionSample;
            m_bHasLastPositionSample = posSpeedResult.hasLastPositionSample;
            m_vComputedVelocity = posSpeedResult.computedVelocity;
            m_fLandPositionDeltaSpeedMs = posSpeedResult.currentSpeed;

            // 启发式：永不调用 GetVelocity；陆地权威测速 = 位置差分。
            loc.velocity = SCR_PlayerBaseRssApiHelper.SampleEntityVelocity(
                loc.owner, posSpeedResult.computedVelocity);
            loc.currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(loc.velocity);
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
            // 蹲/趴：清掉站立 W′ 巡航限速，交给引擎姿态顶速（否则蹲走≈站走）。
            if (GetStance() != ECharacterStance.STAND)
            {
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, 1.0);
                RSS_RestoreNativeMovementMaxSpeed(loc.owner);
                m_fAppliedSpeedLimitMs = -1.0;
                m_fLastRssSpeedMultiplierApplied = 1.0;
            }
            else
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
                // Walk 意图：绝对顶用 Walk 引擎顶，勿用 sticky Run 分母（否则 clamp 失效）。
                if (loc.phaseNow == 1 || loc.effectivePhase == 1)
                {
                    float walkTopMs = GetOriginalEngineMaxSpeed_Walk();
                    if (walkTopMs < SCR_RSS_Constants.ENGINE_WALK_TOP_MS)
                        walkTopMs = SCR_RSS_Constants.ENGINE_WALK_TOP_MS;
                    if (desiredAbsMs > walkTopMs)
                        desiredAbsMs = walkTopMs;
                }

                // SetSpeedLimit 倍率相对「引擎当前相位顶」。意图 Walk 但相位仍停 Run 时，
                // 若用 Walk 顶算 frac≈0.999 再写入，会按 Run 顶放大 → 3m/s+ 尖峰。
                float applyEngineBase = GetRssSpeedLimitEngineBaseMs();
                if (applyEngineBase <= 0.1)
                    applyEngineBase = loc.storedEngineBase;
                if (applyEngineBase <= 0.1)
                    applyEngineBase = GetOriginalEngineMaxSpeed_Run();

                float desiredFrac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(
                    desiredAbsMs, applyEngineBase, keepSpeedSource);
                // 分母跨相位切换时禁止在倍率空间斜率（SprintBlock 已在 m/s 平滑）；
                // 否则 0.63(Run)→0.999(Walk) 会被当成提速，松巡航缩轴后窜速。
                float lastBaseForSlew = m_fLastRssEngineBaseForLimit;
                bool baseChanged = false;
                if (lastBaseForSlew > 0.1 && applyEngineBase > 0.1)
                {
                    float baseDelta = Math.AbsFloat(applyEngineBase - lastBaseForSlew);
                    if (baseDelta > 0.3)
                        baseChanged = true;
                }
                if (!loc.isExhausted && !baseChanged)
                    desiredFrac = RSS_SlewSpeedLimitFraction(desiredFrac, loc.currentTime);
                if (desiredFrac >= 0.999)
                    desiredFrac = 0.999;
                SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, desiredFrac);
                float slewedAbsMs = desiredFrac * applyEngineBase;
                float safeCap = SCR_RSS_SpeedBridge.GetPhaseSafePhysicsCapMs(
                    slewedAbsMs,
                    applyEngineBase,
                    loc.isSprintingNow,
                    loc.phaseNow);
                m_fAppliedSpeedLimitMs = safeCap;
                m_fLastRssSpeedMultiplierApplied = desiredFrac;
                if (applyEngineBase > 0.1)
                    m_fLastRssSpeedMultiplierApplied = safeCap / applyEngineBase;
                if (m_fLastRssSpeedMultiplierApplied > 0.999)
                    m_fLastRssSpeedMultiplierApplied = 0.999;
                if (m_fLastRssSpeedMultiplierApplied < 0.01)
                    m_fLastRssSpeedMultiplierApplied = 0.01;
                if (applyEngineBase > 0.1)
                {
                    loc.storedEngineBase = applyEngineBase;
                    m_fLastRssEngineBaseForLimit = applyEngineBase;
                }
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

    //! AI 全量体力：Phase A 只做位置测速 + 体重，跳过地形/环境/UpdateSpeed。
    //! @deprecated 6.2.28 起 AI 走 SCR_RSS_AIStaminaPipeline，不再经 Phase B。
    //! @return true 继续 Phase B
    bool RSS_AiPhaseAFillForDrain(RSS_StaminaTickLocals loc)
    {
        if (!loc || !loc.owner || !loc.world)
        {
            RSS_ScheduleNextStaminaTick();
            return false;
        }

        float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
        RSS_SpeedCalculationResult posSpeedResult = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
            loc.owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
        m_vLastPositionSample = posSpeedResult.lastPositionSample;
        m_bHasLastPositionSample = posSpeedResult.hasLastPositionSample;
        m_vComputedVelocity = posSpeedResult.computedVelocity;
        m_fLandPositionDeltaSpeedMs = posSpeedResult.currentSpeed;

        loc.velocity = SCR_PlayerBaseRssApiHelper.SampleEntityVelocity(
            loc.owner, posSpeedResult.computedVelocity);
        loc.currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(loc.velocity);

        loc.currentTimeForExerciseMs = loc.world.GetWorldTime();
        loc.currentTime = loc.currentTimeForExerciseMs / 1000.0;

        // AI 不采样地形/环境（射线与天气是 Speed 路径尖刺源）
        loc.terrainFactor = 1.0;
        loc.isSwimmingForSpeed = false;

        loc.currentWeight = 0.0;
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            loc.currentWeight = m_pEncumbranceCache.GetCurrentWeight();

        loc.speedToApply = loc.finalSpeedMultiplier;
        loc.finalSpeedToApply = Math.Clamp(loc.speedToApply, 0.01, 3.0);
        m_sLastSpeedSource = "Server";
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
        loc.rainWeight = 0.0;
        if (loc.isPlayer)
        {
            if (m_pEnvironmentFactor)
                loc.heatStressMultiplier = m_pEnvironmentFactor.GetHeatStressMultiplier();
            if (m_pEnvironmentFactor)
                loc.rainWeight = m_pEnvironmentFactor.GetRainWeight();
            if (RSS_IsCaffeineSodiumBenzoateActive())
            {
                loc.heatStressMultiplier = 1.0;
                loc.rainWeight = 0.0;
            }
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
        loc.gradePercent = 0.0;
        // AI：跳过坡度射线与 CP 代谢二次限速（Speed 尖刺）；玩家保留全链。
        if (loc.isPlayer)
        {
            loc.gradeResult = SCR_RSS_SpeedCalculator.CalculateGradePercent(
                this,
                loc.currentSpeed,
                m_pJumpVaultDetector,
                loc.slopeAngleDegrees,
                m_pEnvironmentFactor,
                loc.velocityForDrain);
            loc.gradePercent = loc.gradeResult.gradePercent;
            loc.slopeAngleDegrees = loc.gradeResult.slopeAngleDegrees;
        }

        // 默认 drain-only：CP 代谢伺服关时整块跳过（避免坡度平滑 + 空调用）
        if (loc.isPlayer && !loc.isExhausted && SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
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
            float appliedBeforeMetab = m_fAppliedSpeedLimitMs;
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
                float hardFrac = RSS_SlewSpeedLimitFraction(correctedSpeed, loc.currentTime);
                if (hardFrac > 0.999)
                    hardFrac = 0.999;
                float hardAbs = hardFrac * engineBase;
                // 同 tick 二次 Apply 只允许压低；步态地板托高已在首轮 UpdateSpeed/落盘完成。
                bool allowMetabApply = true;
                if (appliedBeforeMetab > 0.05 && hardAbs > appliedBeforeMetab + 0.01)
                    allowMetabApply = false;
                if (allowMetabApply)
                {
                    SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(loc.owner, hardFrac);
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
            }
        }

        float replPoolTmp = m_fReplAnaerobicPool;
        float replCooldownTmp = m_fReplAnaerobicCooldownUntil;
        bool wPrimeReplDirty = SCR_PlayerBaseWPrimeTickHelper.TickPhaseBAnaerobic(
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
        if (wPrimeReplDirty)
            Replication.BumpMe();

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

            float distM = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(loc.owner);
            m_pAIManager.Tick(
                loc.owner, this, loc.currentTime, loc.timeDeltaSec,
                loc.staminaPercent, fatigueVal, loc.currentSpeed, loc.isPlayer, distM);
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
