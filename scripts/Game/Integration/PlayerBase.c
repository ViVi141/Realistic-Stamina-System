modded class SCR_CharacterControllerComponent
{
    // --- ICE: stamina tick phase C (kept out of UpdateLoop for size) ---
    bool RSS_StaminaTickPhaseC(RSS_StaminaTickLocals loc)
    {
        loc.drainParams = new StaminaDrainTickParams();
        loc.drainParams.useSwimmingModel = loc.useSwimmingModel;
        loc.drainParams.currentSpeed = loc.currentSpeed;
        loc.drainParams.gearWeightKg = loc.currentWeight;
        loc.drainParams.encumbranceSpeedPenalty = loc.encumbranceSpeedPenalty;
        loc.drainParams.bodyPlusGearWeightKg = loc.totalWeight;
        loc.drainParams.totalWeightWithWetAndBody = loc.totalWeightWithWetAndBody;
        loc.drainParams.gradePercent = loc.gradePercent;
        loc.drainParams.terrainFactor = loc.terrainFactor;
        loc.drainParams.velocityForDrain = loc.velocityForDrain;
        loc.drainParams.swimmingVelocityDebugPrinted = m_bSwimmingVelocityDebugPrinted;
        loc.drainParams.owner = loc.owner;
        loc.drainParams.controller = this;
        loc.drainParams.environmentFactor = m_pEnvironmentFactor;
        loc.drainParams.isSprinting = loc.isSprinting;
        loc.drainParams.currentMovementPhase = loc.effectiveMovementPhase;
        loc.drainParams.speedRatio = loc.speedRatio;
        loc.drainParams.heatStressMultiplier = loc.heatStressMultiplier;
        loc.drainParams.isSprintActive = loc.isSprintActive;
        loc.drainParams.staminaPercent = loc.staminaPercent;
        loc.drainParams.combatStimActive = loc.combatStimActive;
        loc.drainParams.encumbranceCache = m_pEncumbranceCache;
        loc.drainParams.fatigueSystem = m_pFatigueSystem;
        loc.drainParams.exerciseTracker = m_pExerciseTracker;
        loc.drainParams.epocState = m_pEpocState;
        loc.drainParams.currentTimeSec = loc.currentTime;
        loc.drainParams.currentTimeForExerciseMs = loc.currentTimeForExerciseMs;
        loc.drainParams.appliedSpeedLimitMs = m_fAppliedSpeedLimitMs;
        loc.drainParams.effectiveCriticalPowerWatts = -1.0;
        loc.drainParams.wPrimePool01 = 1.0;
        if (m_pAnaerobicBurst)
        {
            SCR_RSS_CriticalPowerModel cpForDrain = m_pAnaerobicBurst.GetCpModel();
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
        m_bSwimmingVelocityDebugPrinted = loc.drainTick.swimmingVelocityDebugPrinted;

        loc.effectiveCriticalPowerWattsDbg = loc.drainParams.effectiveCriticalPowerWatts;
        loc.environmentMultDbg = 1.0;
        if (m_pEnvironmentFactor)
            loc.environmentMultDbg = m_pEnvironmentFactor.GetQuickEnvironmentMultiplier();

        loc.powerWattsDbg = 0.0;
        loc.wPrimePool01Dbg = loc.drainParams.wPrimePool01;
        if (!loc.useSwimmingModel)
        {
            loc.powerWattsDbg = SCR_RSS_DrainCalculator.GetMetabolicAccountingPowerWatts(
                loc.currentSpeed,
                m_fAppliedSpeedLimitMs,
                loc.totalWeightWithWetAndBody,
                loc.gradePercent,
                loc.terrainFactor,
                loc.effectiveMovementPhase,
                loc.wPrimePool01Dbg);
        }

        loc.needLocalDebugBatch = false;
        if (loc.owner == SCR_PlayerController.GetLocalControlledEntity() && IsPlayerControlled())
        {
            if (IsRssDebugEnabled())
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
        if (m_pFatigueSystem)
        {
            loc.maxStaCapDbg = m_pFatigueSystem.GetMaxStaminaCap();
            loc.fatigueNormDbg = m_pFatigueSystem.GetFatigueIntegralNorm();
        }

        loc.metabPowerDbg = -1.0;
        loc.metabPowerMetDbg = -1.0;
        loc.metabPowerRawDbg = -1.0;
        loc.metabCpDbg = loc.drainParams.effectiveCriticalPowerWatts;
        loc.metabAerobicDbg = -1.0;
        SCR_RSS_UpdateLoopMetabDebug.ComputeMetabDebugPowers(
            loc.useSwimmingModel,
            loc.currentSpeed,
            m_fAppliedSpeedLimitMs,
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
            m_pEpocState,
            false);
        loc.metabolicNetDbg = SCR_RSS_StaminaNetRate.GetNetStaminaRatePerSecond(
            loc.staminaBeforeUpdate,
            loc.useSwimmingModel,
            loc.currentSpeed,
            loc.totalDrainRate,
            loc.baseDrainRateByVelocity,
            loc.baseDrainRateByVelocityForModule,
            loc.heatStressMultiplier,
            m_pEpocState,
            m_pEncumbranceCache,
            m_pExerciseTracker,
            this,
            m_pEnvironmentFactor,
            false) / 5.0;
        
        loc.overspeedExtraPerSec = 0.0;
        if (!loc.useSwimmingModel && m_fAppliedSpeedLimitMs > 0.05)
        {
            loc.overspeedExtraPerSec = SCR_RSS_DrainCalculator.GetClientOverspeedExcessDrainPerSecond(
                loc.currentSpeed,
                m_fAppliedSpeedLimitMs,
                loc.drainParams.wPrimePool01,
                loc.totalWeightWithWetAndBody,
                loc.gradePercent,
                loc.terrainFactor,
                loc.effectiveMovementPhase,
                loc.drainParams.effectiveCriticalPowerWatts);
        }

        if (m_pStaminaComponent)
        {
            float newTargetStamina = SCR_RSS_UpdateCoordinator.UpdateStaminaValue(
                m_pStaminaComponent,
                loc.staminaPercent,
                loc.useSwimmingModel,
                loc.currentSpeed,
                loc.totalDrainRate,
                loc.baseDrainRateByVelocity,
                loc.baseDrainRateByVelocityForModule,
                loc.heatStressMultiplier,
                m_pEpocState,
                m_pEncumbranceCache,
                m_pExerciseTracker,
                m_pFatigueSystem,
                this,
                m_pEnvironmentFactor,
                loc.timeDeltaSec);
            
            if (loc.overspeedExtraPerSec > 0.000001)
                newTargetStamina = newTargetStamina - loc.overspeedExtraPerSec * loc.timeDeltaSec;
            newTargetStamina = Math.Clamp(newTargetStamina, 0.0, 1.0);

            m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            if (m_pStaminaState)
                m_pStaminaState.SetAerobic(newTargetStamina);
            m_fLastStaminaUpdateTime = loc.currentTime;

            bool sprintIntentAfterUpdate = loc.isSprintActive || GetIsSprintingToggle();
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
            
            loc.staminaPercent = newTargetStamina;
        }

        if (loc.isPlayer && m_pUISignalBridge)
        {
            m_pUISignalBridge.UpdateUISignal(loc.staminaPercent, loc.isExhausted, loc.currentSpeed, loc.totalDrainRate, false);
        }
        
        m_fLastStaminaPercent = loc.staminaPercent;
        m_fLastSpeedMultiplier = loc.finalSpeedMultiplier;

        loc.netStaminaTickDbg = loc.staminaPercent - loc.staminaBeforeUpdate;

        loc.metabSnap = new RSS_StatusMetabLogSnapshot();
        loc.metabSnap.metabolismPowerW = loc.metabPowerDbg;
        loc.metabSnap.metabolismPowerMetW = loc.metabPowerMetDbg;
        loc.metabSnap.metabolismPowerRawW = loc.metabPowerRawDbg;
        loc.metabSnap.effectiveCpW = loc.metabCpDbg;
        loc.metabSnap.aerobicPowerW = loc.metabAerobicDbg;
        loc.metabSnap.finalDrainPerTick = loc.finalDrainDbg;
        loc.metabSnap.metabolicNetPerTick = loc.metabolicNetDbg;
        loc.metabSnap.capRatchetPerTick = m_fLastCapRatchetPerTick;
        loc.metabSnap.netStaminaPerTick = loc.netStaminaTickDbg;

        RSS_UpdateStatusLogSnapshot(
            loc.currentSpeed,
            loc.staminaPercent,
            loc.finalSpeedMultiplier,
            loc.isSprinting,
            loc.phaseNow,
            loc.effectivePhase,
            loc.maxStaCapDbg,
            GetRssAnaerobicPercent(),
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
            m_pAIManager,
            m_pFatigueSystem,
            m_sLastSpeedSource);

        loc.debugTick = new RSS_StaminaDebugOutputParams();
        loc.targetStaCapDbg = 1.0;
        loc.capShrinkDbg = 0.0;
        if (m_pFatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
        {
            loc.targetStaCapDbg = m_pFatigueSystem.GetMaxStaminaCap();
            if (!loc.useSwimmingModel && loc.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            {
                float powerFat = SCR_RSS_DrainCalculator.GetMetabolicFatiguePowerWatts(
                    loc.currentSpeed,
                    m_fAppliedSpeedLimitMs,
                    loc.totalWeightWithWetAndBody,
                    loc.gradePercent,
                    loc.terrainFactor,
                    loc.phaseNow);
                loc.capShrinkDbg = m_pFatigueSystem.EstimateCapShrinkPerSecond(
                    powerFat,
                    loc.currentWeight,
                    loc.gradePercent,
                    loc.terrainFactor,
                    loc.currentSpeed);
            }
        }
        loc.epocActiveDbg = false;
        if (m_pEpocState)
            loc.epocActiveDbg = m_pEpocState.IsInEpocDelay();
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
            m_fLastCapRatchetPerTick,
            loc.netStaminaTickDbg,
            loc.terrainFactor);
        SCR_RSS_UpdateLoopMetabDebug.FillDebugTickExtras(
            loc.debugTick,
            m_fAppliedSpeedLimitMs,
            loc.effectiveCriticalPowerWattsDbg,
            loc.timeDeltaSec,
            loc.totalWeightWithWetAndBody,
            loc.powerWattsDbg,
            loc.wPrimePool01Dbg,
            m_fLandPositionDeltaSpeedMs,
            loc.overspeedExtraPerSec,
            loc.environmentMultDbg,
            loc.targetStaCapDbg,
            loc.capShrinkDbg,
            loc.epocActiveDbg);
        SCR_RSS_UpdateLoopDebugOutput.OutputPlayerStaminaAndHints(
            this,
            loc.owner,
            loc.debugTick,
            m_pEncumbranceCache,
            m_pFatigueSystem,
            m_pEpocState,
            m_pExerciseTracker,
            m_pEnvironmentFactor,
            m_pTerrainDetector,
            m_pStanceTransitionManager,
            m_fCurrentWetWeight,
            m_sLastSpeedSource,
            GetRssWPrimePool01(),
            GetRssSprintCooldownRemainingSec());
        
        SCR_RSS_DebugBatchManager.FlushDebugBatch();
        return true;
    }

    protected float m_fLastSecondSpeed = 0.0;
    protected float m_fCurrentSecondSpeed = 0.0;
    protected bool m_bHasPreviousSpeed = false;
    protected const int SPEED_SAMPLE_INTERVAL_MS = 1000;

    //! 与体力 tick 同步的快照，供 1 秒状态行使用（避免旧速度 + 新 phase 错位）
    protected float m_fStatusLogSpeed = 0.0;
    protected float m_fStatusLogStamina = 1.0;
    protected float m_fStatusLogMultiplier = 1.0;
    protected bool m_bStatusLogSprinting = false;
    protected int m_iStatusLogEnginePhase = 0;
    protected int m_iStatusLogEffectivePhase = 0;
    protected float m_fStatusLogMaxCap = 1.0;
    protected float m_fStatusLogAnaerobicPct = 1.0;
    protected float m_fStatusLogPowerW = -1.0;
    protected float m_fStatusLogPowerMetW = -1.0;
    protected float m_fStatusLogPowerRawW = -1.0;
    protected float m_fStatusLogCpW = -1.0;
    protected float m_fStatusLogStaPW = -1.0;
    protected float m_fStatusLogDrainTick = 0.0;
    protected float m_fStatusLogNetTick = 0.0;
    protected float m_fStatusLogFatigueNorm = 0.0;
    protected float m_fStatusLogCapRatchetTick = 0.0;
    protected float m_fStatusLogMetabolicNetTick = 0.0;
    protected float m_fLastCapRatchetPerTick = 0.0;
    //! 松键惯性：上一非 Idle 引擎相位（1=Walk, 2=Run, 3=Sprint）
    protected int m_iLastNonIdleMovementPhase = 2;
    
    protected float m_fLastStaminaPercent = 1.0;
    protected float m_fLastSpeedMultiplier = 1.0;
    protected float m_fLastStaminaUpdateTime = -1.0;
    protected SCR_CharacterStaminaComponent m_pStaminaComponent;
    protected bool m_bLastExhaustedState = false;
    
    protected ref SCR_RSS_NetworkSyncManager m_pNetworkSyncManager;
    protected string m_sLastSpeedSource = "";
    protected float m_fLastReconnectTime = -1.0;
    protected const float CONFIG_FETCH_TIMEOUT_SEC = 30.0;
    protected float m_fLastConfigTimeoutWarningTime = -1.0;

    protected bool m_bRssWaitingGameModeConfig = false;
    protected float m_fRssConfigWaitStartTime = -1.0;

    protected ref SCR_RSS_CollapseTransition m_pCollapseTransition;
    protected ref SCR_RSS_SlopeSpeedTransition m_pSlopeSpeedTransition;
    protected ref SCR_RSS_SprintBlockSpeedTransition m_pSprintBlockSpeedTransition;
    
    protected ref SCR_RSS_ExerciseTracker m_pExerciseTracker;
    
    protected ref SCR_RSS_JumpVaultDetector m_pJumpVaultDetector;
    
    protected ref SCR_RSS_EncumbranceCache m_pEncumbranceCache;
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent;
    
    protected ref SCR_RSS_FatigueSystem m_pFatigueSystem;
    
    protected ref SCR_RSS_TerrainDetector m_pTerrainDetector;
    
    protected ref SCR_RSS_EnvironmentFactor m_pEnvironmentFactor;
    
    protected ref SCR_RSS_UISignalBridge m_pUISignalBridge;
    
    protected ref SCR_RSS_EpocState m_pEpocState;
    
    protected ref SCR_RSS_StanceTransitionManager m_pStanceTransitionManager;
    
    protected bool m_bWasSwimming = false;
    protected float m_fWetWeightStartTime = -1.0;
    protected float m_fSwimStartTimeSec = -1.0;
    protected float m_fCurrentWetWeight = 0.0;
    protected bool m_bSwimmingVelocityDebugPrinted = false;

    protected vector m_vLastPositionSample = vector.Zero;
    protected bool m_bHasLastPositionSample = false;
    protected vector m_vComputedVelocity = vector.Zero;
    protected CompartmentAccessComponent m_pCompartmentAccess;
    protected CharacterAnimationComponent m_pAnimComponent;
    protected ChimeraCharacter m_pCachedOwnerCharacter;

    protected float m_fAnimSpeedCompensation = 1.0;
    protected float m_fLastAnimCompensationUpdateTime = -1.0;
    protected static const float ANIM_COMPENSATION_UPDATE_INTERVAL = 2.0;
    
    protected float m_fSprintStartTime = -1.0;
    protected bool m_bLastWasSprinting = false;
    protected float m_fSprintCooldownUntil = -1.0;

    protected int m_iCombatStimPhase = ERSS_CombatStimPhase.NONE;
    protected float m_fCombatStimPhaseEndsAt = -1.0;
    protected int m_iCombatStimDelayInjectionCount = 0;
    //! 注射 Buff 前记录的 GetBleedingScale()，用于阶段结束时还原；-1 表示未处于 Buff
    protected float m_fRSS_CombatStimBleedingBaseline = -1.0;
    
    protected ref SCR_RSS_MudSlipRunner m_pMudSlipRunner;
    protected float m_fRssMudSlipCameraShake01 = 0.0;
    protected float m_fLastRssSpeedMultiplierApplied = 1.0;
    protected float m_fLastRssEngineBaseForLimit = 0.0;
    protected float m_fAppliedSpeedLimitMs = -1.0;
    protected float m_fLandPositionDeltaSpeedMs = 0.0;
    protected bool m_bRssStaminaLoopActive = false;
    protected bool m_bIsDeleted = false;
    
    protected ref SCR_RSS_AIManager m_pAIManager;

    protected ref SCR_RSS_AnaerobicBurst m_pAnaerobicBurst;
    protected ref SCR_RSS_StaminaState m_pStaminaState;
    protected bool m_bSprintGateEnginePokeActive = false;

    [RplProp(onRplName: "OnRssAnaerobicReplicated")]
    protected float m_fReplAnaerobicPool = 1.0;

    [RplProp(onRplName: "OnRssAnaerobicReplicated")]
    protected float m_fReplAnaerobicCooldownUntil = -1.0;

    void OnRssAnaerobicReplicated()
    {
        SCR_RSS_NetworkSyncManager.ApplyAnaerobicReplication(m_pAnaerobicBurst, m_fReplAnaerobicPool, m_fReplAnaerobicCooldownUntil);
        if (m_pStaminaState)
        {
            m_pStaminaState.SetWPrimePool01(m_fReplAnaerobicPool);
            m_pStaminaState.SetAerobic(GetRssAerobicPercent());
        }
    }

    //! 析构函数：在实体删除时同步调用，标记组件已删除，防止 CallLater/ActionListener
    //! 等异步回调在已释放的内存上执行读写操作（Access Violation）。
    //! CallLater 清理：EnforceScript Remove 无带参重载，依赖 m_bIsDeleted 停循环（见 RSS_RemoveScheduledCallbacks）。
    void ~SCR_CharacterControllerComponent()
    {
        m_bIsDeleted = true;

        if (!GetGame())
        {
            m_pCachedOwnerCharacter = null;
            m_pStaminaComponent = null;
            m_pCompartmentAccess = null;
            m_pAnimComponent = null;
            m_pCachedInventoryComponent = null;
            m_pJumpVaultDetector = null;
            m_pStanceTransitionManager = null;
            m_pExerciseTracker = null;
            m_pCollapseTransition = null;
            m_pSlopeSpeedTransition = null;
            m_pSprintBlockSpeedTransition = null;
            m_pTerrainDetector = null;
            m_pMudSlipRunner = null;
            m_pEnvironmentFactor = null;
            m_pFatigueSystem = null;
            m_pEncumbranceCache = null;
            m_pUISignalBridge = null;
            m_pEpocState = null;
            m_pNetworkSyncManager = null;
            return;
        }

        IEntity ownerForHud = GetOwner();
        if (ownerForHud && ownerForHud == SCR_PlayerController.GetLocalControlledEntity())
            SCR_RSS_StaminaHUDComponent.Destroy();

        IEntity ownerForSpeed = GetOwner();
        if (ownerForSpeed)
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(ownerForSpeed, 1.0);

        m_bRssStaminaLoopActive = false;

        RSS_RemoveScheduledCallbacks();

        if (GetGame())
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                inputManager.RemoveActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
            }
        }

        // AI 恢复注册表已由 GroupSync 替代，此处无需额外清理

        if (m_pUISignalBridge)
            m_pUISignalBridge.Cleanup();

        m_pCachedOwnerCharacter = null;
        m_pStaminaComponent = null;
        m_pCompartmentAccess = null;
        m_pAnimComponent = null;
        m_pCachedInventoryComponent = null;
        m_pJumpVaultDetector = null;
        m_pStanceTransitionManager = null;
        m_pExerciseTracker = null;
        m_pCollapseTransition = null;
        m_pSlopeSpeedTransition = null;
        m_pSprintBlockSpeedTransition = null;
        m_pTerrainDetector = null;
        m_pMudSlipRunner = null;
        m_pEnvironmentFactor = null;
        m_pFatigueSystem = null;
        m_pEncumbranceCache = null;
        m_pUISignalBridge = null;
        m_pEpocState = null;
        m_pNetworkSyncManager = null;
        if (m_pAIManager)
        {
            m_pAIManager.OnEntityDeleted();
            m_pAIManager = null;
        }
    }
    
    protected int m_iAiLoopRetryCount = 0;
    protected const int AI_LOOP_MAX_RETRIES = 5;

    float GetSprintStartTime()
    {
        return m_fSprintStartTime;
    }

    float GetSprintCooldownUntil()
    {
        return m_fSprintCooldownUntil;
    }


    bool HasRssData()
    {
        return SCR_PlayerBaseRssApiHelper.HasRssData(m_pStaminaComponent, m_pEnvironmentFactor);
    }

    float GetRssStaminaPercent()
    {
        return SCR_PlayerBaseRssApiHelper.ClampStaminaPercent(m_pStaminaComponent);
    }

    float GetRssSpeedMultiplier()
    {
        return m_fLastSpeedMultiplier;
    }

    float GetRssCurrentSpeed()
    {
        return SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(GetVelocity());
    }

    int GetRssMovementPhase()
    {
        return GetCurrentMovementPhase();
    }

    bool GetRssIsSprinting()
    {
        return IsSprinting();
    }

    bool GetRssIsExhausted()
    {
        float stamina = GetRssStaminaPercent();
        return SCR_RSS_MetabolismMath.IsExhausted(stamina);
    }

    bool GetRssIsSwimming()
    {
        return SCR_RSS_SwimmingStateManager.IsSwimming(this);
    }

    float GetRssCurrentWeight()
    {
        return SCR_PlayerBaseRssApiHelper.GetCurrentWeight(m_pEncumbranceCache, m_pCachedInventoryComponent);
    }

    SCR_RSS_EnvironmentFactor GetRssEnvironmentFactor()
    {
        return m_pEnvironmentFactor;
    }

    float GetOriginalEngineMaxSpeed_Run()
    {
        return GetDynamicOriginalEngineMaxSpeed(2);
    }

    float GetOriginalEngineMaxSpeed_Sprint()
    {
        return GetDynamicOriginalEngineMaxSpeed(3);
    }

    //! 与 UpdateCoordinator 一致：当前 RSS 限速倍率所乘的引擎原始最大速度
    float GetRssSpeedLimitEngineBaseMs()
    {
        bool engineStillSprinting = IsSprinting() || (GetCurrentMovementPhase() == 3);
        if (engineStillSprinting)
            return GetOriginalEngineMaxSpeed_Sprint();
        return GetOriginalEngineMaxSpeed_Run();
    }

    protected float GetDynamicOriginalEngineMaxSpeed(int movementPhase)
    {
        if (!m_pAnimComponent)
        {
            if (movementPhase == 3)
                return SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
            else
                return SCR_RSS_MetabolismMath.GAME_MAX_SPEED * SCR_RSS_MetabolismMath.TARGET_RUN_SPEED_MULTIPLIER;
        }
        
        float currentMultiplier = m_fLastSpeedMultiplier;
        IEntity ownerEnt = GetOwner();

        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(ownerEnt, 1.0);

        float realOriginalSpeed = m_pAnimComponent.GetMaxSpeed(1.0, 0.0, movementPhase);

        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(ownerEnt, currentMultiplier);
        
        return realOriginalSpeed;
    }

    void RSS_UpdateStatusLogSnapshot(
        float currentSpeed,
        float staminaPercent,
        float finalSpeedMultiplier,
        bool isSprinting,
        int engineMovementPhase,
        int effectiveMovementPhase,
        float maxStaminaCap,
        float anaerobicPercent,
        float fatigueIntegralNorm,
        RSS_StatusMetabLogSnapshot metab)
    {
        m_fStatusLogSpeed = currentSpeed;
        m_fStatusLogStamina = staminaPercent;
        m_fStatusLogMultiplier = finalSpeedMultiplier;
        m_bStatusLogSprinting = isSprinting;
        m_iStatusLogEnginePhase = engineMovementPhase;
        m_iStatusLogEffectivePhase = effectiveMovementPhase;
        m_fStatusLogMaxCap = maxStaminaCap;
        m_fStatusLogAnaerobicPct = anaerobicPercent;
        m_fStatusLogFatigueNorm = fatigueIntegralNorm;
        if (metab)
        {
            m_fStatusLogPowerW = metab.metabolismPowerW;
            m_fStatusLogPowerMetW = metab.metabolismPowerMetW;
            m_fStatusLogPowerRawW = metab.metabolismPowerRawW;
            m_fStatusLogCpW = metab.effectiveCpW;
            m_fStatusLogStaPW = metab.aerobicPowerW;
            m_fStatusLogDrainTick = metab.finalDrainPerTick;
            m_fStatusLogMetabolicNetTick = metab.metabolicNetPerTick;
            m_fStatusLogCapRatchetTick = metab.capRatchetPerTick;
            m_fStatusLogNetTick = metab.netStaminaPerTick;
        }
    }

    string RSS_FormatStatusMetabolismDiagnostic()
    {
        return SCR_RSS_DebugDisplay.FormatStatusMetabolismDiagnostic(
            m_fStatusLogPowerW,
            m_fStatusLogPowerMetW,
            m_fStatusLogPowerRawW,
            m_fStatusLogMaxCap,
            m_fStatusLogAnaerobicPct,
            m_fStatusLogCpW,
            m_fStatusLogFatigueNorm,
            m_fStatusLogDrainTick,
            m_fStatusLogMetabolicNetTick,
            m_fStatusLogCapRatchetTick,
            m_fStatusLogNetTick);
    }

    void CollectSpeedSample()
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
        
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return;
        
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        if (!GetGame())
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }
        World world = GetGame().GetWorld();
        if (!world)
            return;
        
        vector velocity = GetVelocity();
        vector velocityXZ = vector.Zero;
        velocityXZ[0] = velocity[0];
        velocityXZ[2] = velocity[2];
        float speedHorizontal = velocityXZ.Length();
        
        if (m_bHasPreviousSpeed)
        {
            DisplayStatusInfo();
        }
        
        m_fLastSecondSpeed = m_fCurrentSecondSpeed;
        m_fCurrentSecondSpeed = speedHorizontal;
        m_bHasPreviousSpeed = true;
        
        if (IsRssDebugEnabled() && GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.CollectSpeedSampleBridge, SPEED_SAMPLE_INTERVAL_MS, false, this);
    }

    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        bool isSwimming = IsSwimmingByCommand();
        
        SCR_PlayerBaseDebugHelper.OutputStatusInfo(
            owner,
            m_fStatusLogSpeed,
            m_fStatusLogStamina,
            m_fStatusLogMultiplier,
            isSwimming,
            m_bStatusLogSprinting,
            m_iStatusLogEnginePhase,
            m_iStatusLogEffectivePhase,
            this);
    }

    void OnItemRemovedFromInventory()
    {
        SCR_PlayerBaseInventoryHelper.UpdateEncumbranceCache(m_pEncumbranceCache);
    }

    void OnItemAddedToInventory()
    {
        SCR_PlayerBaseInventoryHelper.UpdateEncumbranceCache(m_pEncumbranceCache);
    }

    void RSS_TriggerMudSlipRagdoll()
    {
        if (m_pAnimComponent && m_pAnimComponent.IsRagdollActive())
            return;
        Ragdoll();
        RefreshRagdoll(SCR_RSS_EnvConstants.ENV_MUD_SLIP_RAGDOLL_WARMUP_SEC);
        if (GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.MudSlipFinishRagdollBridge, SCR_RSS_EnvConstants.ENV_MUD_SLIP_RAGDOLL_BLEND_DELAY_MS, false, this);
    }

    void RSS_MudSlip_FinishRagdoll()
    {
        if (m_bIsDeleted)
            return;
        if (!GetOwner())
            return;
        RefreshRagdoll(0.0);
    }

    void RSS_SetMudSlipCameraShake01(float value)
    {
        m_fRssMudSlipCameraShake01 = SCR_RSS_PresentationBridge.ClampShake01(value);
    }

    float RSS_GetMudSlipCameraShake01()
    {
        return m_fRssMudSlipCameraShake01;
    }

    bool RSS_IsRagdollActiveForCamera()
    {
        return SCR_RSS_PresentationBridge.IsRagdollActive(m_pAnimComponent);
    }

    bool RSS_IsAiMudSlipBlockedBySafety(IEntity owner)
    {
        if (IsPlayerControlled())
            return false;
        return true;
    }

    bool RSS_ShouldAiAllowMudSlipRagdoll(IEntity owner)
    {
        return !RSS_IsAiMudSlipBlockedBySafety(owner);
    }

    float RSS_GetLastAppliedSpeedMultiplier()
    {
        return m_fLastRssSpeedMultiplierApplied;
    }

    ERSS_AIStaminaState RSS_GetAIStaminaState()
    {
        if (m_pAIManager)
            return m_pAIManager.GetStaminaState();
        return ERSS_AIStaminaState.FRESH;
    }

    void RSS_SetAIStaminaState(ERSS_AIStaminaState state)
    {
    }

    protected string GetPlayerLabel(IEntity entity)
    {
        return SCR_PlayerBaseConfigHelper.GetPlayerLabel(entity);
    }

    protected bool IsRssDebugEnabled()
    {
        return SCR_PlayerBaseConfigHelper.IsRssDebugEnabled();
    }

    protected bool IsSwimmingByCommand()
    {
        return SCR_PlayerBaseMovementHelper.IsSwimmingByCommand(m_pAnimComponent);
    }

    protected bool HasSwimInput()
    {
        return SCR_PlayerBaseMovementHelper.HasSwimInput(m_pAnimComponent);
    }

    override void OnControlledByPlayer(IEntity owner, bool controlled)
    {
        super.OnControlledByPlayer(owner, controlled);

        if (m_bIsDeleted)
            return;

        if (m_pTerrainDetector)
            m_pTerrainDetector.SetIsAiEntity(!controlled);

        if (controlled)
            SCR_PlayerBaseLoop.EnsureClientLoop(this);
        
        if (controlled && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                inputManager.AddActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.AddActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.AddActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
                
                if (IsRssDebugEnabled())
                    Print("[RSS] 跳跃动作监听器已添加 / Jump Action Listener Added");
            }
            
            if (GetGame() && GetGame().GetCallqueue())
                GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.InitStaminaHudBridge, 1000, false, this);
        }
        else
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                inputManager.RemoveActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
            }
            
            SCR_RSS_StaminaHUDComponent.Destroy();
        }
    }

    void InitStaminaHUD()
    {
        if (m_bIsDeleted)
            return;
        if (!GetOwner())
            return;
        SCR_RSS_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    void OnJumpActionTriggered(float value, EActionTrigger trigger)
    {
        IEntity owner = GetOwner();
        if (!owner || owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        bool isInVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(m_pCompartmentAccess);
        
        if (!isInVehicle && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            if (IsRssDebugEnabled())
                Print("[RSS] 动作监听器检测到跳跃输入！/ Action Listener Detected Jump Input!");
        }
    }

    override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
    {
        super.OnPrepareControls(owner, am, dt, player);

        if (Replication.IsServer() && IsPlayerControlled() && !ShouldProcessStaminaUpdate())
            RSS_MaybeServerTickAnaerobic();

        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            RSS_ApplySprintGateOnPrepareControls(am);
        
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        bool isInVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(m_pCompartmentAccess);
        
        if (!isInVehicle && am.GetActionTriggered("Jump") && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            if (IsRssDebugEnabled())
                Print("[RSS] OnPrepareControls 检测到跳跃输入！/ OnPrepareControls Detected Jump Input!");
        }
    }

    protected bool ShouldProcessStaminaUpdate()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return false;
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            return true;
        IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
        if (controlled && m_pCompartmentAccess)
        {
            BaseCompartmentSlot slot = m_pCompartmentAccess.GetCompartment();
            if (slot)
            {
                IEntity vehicle = slot.GetVehicle();
                if (vehicle == controlled)
                    return true;
            }
        }
        if (Replication.IsServer() && !IsPlayerControlled())
        {
            if (SCR_RSS_ConfigBridge.IsAiAllCalcDisabled())
                return false;
            return true;
        }
        return false;
    }

    //! 停止本实体 RSS CallLater 循环（仅标志位；EnforceScript Remove 不支持带参，全局 Remove 会误伤其他实体）
    void RSS_RemoveScheduledCallbacks()
    {
        // m_bIsDeleted / m_bRssStaminaLoopActive 已在调用方置位；
        // pending 桥接回调入口 guard 后 return 且不再 CallLater。
    }

    //! 实体即将被删除时调用：清理所有引用、停止 CallLater 循环、注销静态注册表
    void RSS_NotifyEntityDeleting()
    {
        if (m_bIsDeleted)
            return;
        m_bIsDeleted = true;
        m_bRssStaminaLoopActive = false;

        IEntity ownerForSpeed = GetOwner();
        if (ownerForSpeed)
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(ownerForSpeed, 1.0);

        RSS_RemoveScheduledCallbacks();

        if (m_pUISignalBridge)
            m_pUISignalBridge.Cleanup();
        
        m_pCachedOwnerCharacter = null;
        m_pStaminaComponent = null;
        m_pCompartmentAccess = null;
        m_pAnimComponent = null;
        m_pCachedInventoryComponent = null;
        
        m_pJumpVaultDetector = null;
        m_pStanceTransitionManager = null;
        m_pExerciseTracker = null;
        m_pCollapseTransition = null;
        m_pSlopeSpeedTransition = null;
        m_pSprintBlockSpeedTransition = null;
        m_pTerrainDetector = null;
        m_pMudSlipRunner = null;
        m_pEnvironmentFactor = null;
        m_pFatigueSystem = null;
        m_pEncumbranceCache = null;
        m_pUISignalBridge = null;
        m_pEpocState = null;
        m_pNetworkSyncManager = null;
    }


    override void OnInit(IEntity owner)
    {
        super.OnInit(owner);
        
        if (!owner)
            return;
        
        if (m_pStaminaComponent || m_pCollapseTransition || m_pEnvironmentFactor)
        {
            m_bIsDeleted = false;

            if (m_pEnvironmentFactor)
                m_pEnvironmentFactor.ClearStaleReferences();

            return;
        }

        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();

            if (IsRssDebugEnabled())
            {
                float coeff = SCR_RSS_ConfigBridge.GetEnergyToStaminaCoeff();
                PrintFormat("[RSS] 初始 energie->stamina coeff = %1", coeff);
            }
        }
        
        CharacterStaminaComponent staminaComp = GetStaminaComponent();
        m_pStaminaComponent = SCR_CharacterStaminaComponent.Cast(staminaComp);
        
        if (m_pStaminaComponent)
        {
            m_pStaminaComponent.SetAllowNativeStaminaSystem(false);
            
            m_pStaminaComponent.SetTargetStamina(SCR_RSS_MetabolismMath.INITIAL_STAMINA_AFTER_ACFT);
        }
        
        m_pJumpVaultDetector = new SCR_RSS_JumpVaultDetector();
        if (m_pJumpVaultDetector)
            m_pJumpVaultDetector.Initialize();
        
        m_pStanceTransitionManager = new SCR_RSS_StanceTransitionManager();
        if (m_pStanceTransitionManager)
        {
            m_pStanceTransitionManager.Initialize();
            m_pStanceTransitionManager.SetInitialStance(GetStance());
        }
        
        m_pExerciseTracker = new SCR_RSS_ExerciseTracker();
        if (m_pExerciseTracker)
        {
            float initTime = 0.0;
            if (GetGame() && GetGame().GetWorld())
                initTime = GetGame().GetWorld().GetWorldTime();
            m_pExerciseTracker.Initialize(initTime);
        } 
        
        m_pCollapseTransition = new SCR_RSS_CollapseTransition();
        if (m_pCollapseTransition)
            m_pCollapseTransition.Initialize();

        m_pSlopeSpeedTransition = new SCR_RSS_SlopeSpeedTransition();
        if (m_pSlopeSpeedTransition)
            m_pSlopeSpeedTransition.Initialize();

        m_pSprintBlockSpeedTransition = new SCR_RSS_SprintBlockSpeedTransition();
        if (m_pSprintBlockSpeedTransition)
            m_pSprintBlockSpeedTransition.Initialize();
        
        m_pTerrainDetector = new SCR_RSS_TerrainDetector();
        if (m_pTerrainDetector)
            m_pTerrainDetector.Initialize(!IsPlayerControlled());
        
        m_pMudSlipRunner = new SCR_RSS_MudSlipRunner();

        m_pEnvironmentFactor = new SCR_RSS_EnvironmentFactor();
        if (m_pEnvironmentFactor)
        {
            World world = null;
            if (GetGame())
                world = GetGame().GetWorld();
            if (world)
            {
                m_pEnvironmentFactor.Initialize(world, owner);
                m_pEnvironmentFactor.SetUseEngineWeather(true);
                m_pEnvironmentFactor.SetUseEngineTemperature(false);
            }
        }
        
        if (!m_pAIManager)
            m_pAIManager = new SCR_RSS_AIManager();

        if (!m_pAnaerobicBurst)
            m_pAnaerobicBurst = new SCR_RSS_AnaerobicBurst();
        if (!m_pStaminaState)
            m_pStaminaState = new SCR_RSS_StaminaState();
        if (m_pStaminaState && m_pStaminaComponent)
            m_pStaminaState.SetAerobic(m_pStaminaComponent.GetTargetStamina());

        m_pFatigueSystem = new SCR_RSS_FatigueSystem();
        if (m_pFatigueSystem)
        {
            float currentTime = 0.0;
            if (GetGame() && GetGame().GetWorld())
                currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            m_pFatigueSystem.Initialize(currentTime);
        }
        
        m_pEncumbranceCache = new SCR_RSS_EncumbranceCache();
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
            m_pCachedInventoryComponent = inventoryComponent;
            if (m_pEncumbranceCache)
                m_pEncumbranceCache.Initialize(inventoryComponent);
            m_pCachedOwnerCharacter = character;
            m_pCompartmentAccess = character.GetCompartmentAccessComponent();
            m_pAnimComponent = character.GetAnimationComponent();
        }
        
        m_pUISignalBridge = new SCR_RSS_UISignalBridge();
        if (m_pUISignalBridge)
            m_pUISignalBridge.Init(owner);
        
        m_pEpocState = new SCR_RSS_EpocState();
        
        m_pNetworkSyncManager = new SCR_RSS_NetworkSyncManager();
        if (m_pNetworkSyncManager)
            m_pNetworkSyncManager.Initialize();
        
        if (GetGame() && GetGame().GetCallqueue())
        {
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.DelayedStart, 500, false, this);
            if (!Replication.IsServer())
                GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.DelayedEnsureClient, 2000, false, this);
            if (Replication.IsServer() && !IsPlayerControlled())
                GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.DelayedEnsureAiServer, 3000, false, this);
        }
        
        if (!Replication.IsServer())
        {
            m_bRssWaitingGameModeConfig = true;
            if (GetGame() && GetGame().GetWorld())
                m_fRssConfigWaitStartTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            if (GetGame() && GetGame().GetCallqueue())
                GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.WaitForGameModeConfigBridge, 1000, false, this);
        }
    }

    void RSS_WaitForGameModeConfig()
    {
        if (m_bIsDeleted)
            return;
        if (!GetGame() || !GetGame().GetWorld())
            return;
        if (!GetOwner())
            return;
        if (Replication.IsServer())
            return;
        if (!m_bRssWaitingGameModeConfig)
            return;

        if (SCR_RSS_ConfigManager.IsServerConfigApplied())
        {
            m_bRssWaitingGameModeConfig = false;
            return;
        }

        float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        if (m_fRssConfigWaitStartTime >= 0.0 && (nowSec - m_fRssConfigWaitStartTime) >= CONFIG_FETCH_TIMEOUT_SEC)
        {
            if (m_fLastConfigTimeoutWarningTime < 0.0 || (nowSec - m_fLastConfigTimeoutWarningTime) >= CONFIG_FETCH_TIMEOUT_SEC)
            {
                m_fLastConfigTimeoutWarningTime = nowSec;
                Print("[RSS] 等待 GameMode 同步配置超时。请确认服务器端 RSS 已加载且复制正常。");
            }
        }
        
        if (m_bRssWaitingGameModeConfig && GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.WaitForGameModeConfigBridge, 1000, false, this);
    }


    float GetRssAnaerobicPercent()
    {
        return GetRssWPrimePool01();
    }

    //! W′ 池归一化储量（0–1）；权威读数来自 AnaerobicBurst
    float GetRssWPrimePool01()
    {
        if (m_pAnaerobicBurst)
            return m_pAnaerobicBurst.GetPool();
        return 1.0;
    }

    //! v5 有氧池权威读数（不受冲刺门禁临时压低引擎条影响）
    float GetRssAerobicPercent()
    {
        if (m_pStaminaState)
            return m_pStaminaState.GetAerobic();
        if (m_pStaminaComponent)
            return m_pStaminaComponent.GetTargetStamina();
        return 1.0;
    }

    float GetRssSprintCooldownRemainingSec()
    {
        if (!m_pAnaerobicBurst)
            return 0.0;
        if (!GetGame() || !GetGame().GetWorld())
            return 0.0;
        float t = GetGame().GetWorld().GetWorldTime() / 1000.0;
        return m_pAnaerobicBurst.GetCooldownRemainingSec(t);
    }

    //! 专服上玩家体力主循环不在服务端跑时，补 tick 无氧池（权威写 RplProp）。
    void RSS_MaybeServerTickAnaerobic()
    {
        float lastUpdate = m_fLastStaminaUpdateTime;
        float replPool = m_fReplAnaerobicPool;
        float replCd = m_fReplAnaerobicCooldownUntil;
        bool didTick = SCR_RSS_WPrimeServerTick.MaybeTick(
            Replication.IsServer(),
            IsPlayerControlled(),
            ShouldProcessStaminaUpdate(),
            m_pAnaerobicBurst,
            m_pStaminaState,
            IsSprinting() || (GetCurrentMovementPhase() == 3),
            GetRssAerobicPercent(),
            GetSpeedUpdateIntervalMs() / 1000.0,
            lastUpdate,
            replPool,
            replCd);
        if (!didTick)
            return;

        m_fLastStaminaUpdateTime = lastUpdate;
        m_fReplAnaerobicPool = replPool;
        m_fReplAnaerobicCooldownUntil = replCd;
        Replication.BumpMe();
    }

    SCR_RSS_AnaerobicBurst RSS_GetAnaerobicBurst()
    {
        return RSS_GetWPrimeBurst();
    }

    //! W′ 爆发控制器；与 RSS_GetAnaerobicBurst 同义
    SCR_RSS_AnaerobicBurst RSS_GetWPrimeBurst()
    {
        return m_pAnaerobicBurst;
    }

    bool GetRssSprintAllowed()
    {
        float worldTime = 0.0;
        if (GetGame() && GetGame().GetWorld())
            worldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        if (m_pStaminaState && m_pAnaerobicBurst && m_pAnaerobicBurst.GetCpModel())
        {
            return m_pStaminaState.IsSprintAllowedWithCp(
                m_pAnaerobicBurst.GetCpModel(), worldTime);
        }

        if (GetRssSprintCooldownRemainingSec() > 0.0)
            return false;

        if (m_pAnaerobicBurst)
        {
            if (m_pAnaerobicBurst.GetPool() <= SCR_RSS_ConfigBridge.GetWPrimeSprintEnableThreshold())
                return false;
        }

        if (m_pStaminaState && m_pStaminaState.GetCollapse())
            return false;

        if (!SCR_RSS_MetabolismMath.CanSprint(GetRssAerobicPercent()))
            return false;

        return true;
    }

    protected void RSS_ClearSprintGateEnginePoke()
    {
        bool pokeActive = m_bSprintGateEnginePokeActive;
        SCR_RSS_SprintGate.ClearEnginePoke(m_pStaminaComponent, pokeActive);
        m_bSprintGateEnginePokeActive = pokeActive;
    }

    //! proto external 的 IsSprintingAllowed 无法 modded override；仅临时压低引擎条读数，有氧目标保留在 StaminaState。
    protected void RSS_PokeEngineStaminaForSprintBlock(bool sprintIntent)
    {
        bool pokeActive = m_bSprintGateEnginePokeActive;
        SCR_RSS_SprintGate.PokeEngineStaminaForSprintBlock(
            GetRssSprintAllowed(),
            sprintIntent,
            m_pStaminaComponent,
            pokeActive);
        m_bSprintGateEnginePokeActive = pokeActive;
    }

    protected void RSS_ApplySprintGateOnPrepareControls(ActionManager am)
    {
        bool pokeActive = m_bSprintGateEnginePokeActive;
        SCR_RSS_SprintGate.ApplyOnPrepareControls(
            am,
            GetRssSprintAllowed(),
            IsSprinting(),
            GetCurrentMovementPhase(),
            GetIsSprintingToggle(),
            m_pStaminaComponent,
            pokeActive);
        m_bSprintGateEnginePokeActive = pokeActive;
    }

    protected float GetNearestPlayerDistanceM(IEntity ownerEntity)
    {
        return SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(ownerEntity);
    }

    protected int GetSpeedUpdateIntervalMs()
    {
        return SCR_RSS_AIUpdateInterval.GetSpeedUpdateIntervalMs(IsPlayerControlled(), GetOwner());
    }

    //! 在 Workbench 编辑器中，预览实体不应启动体力 tick
    protected bool IsWorkbenchPreviewEntity()
    {
        return SCR_RSS_AIUpdateInterval.IsWorkbenchPreviewEntity(IsPlayerControlled(), GetOwner());
    }

    static float CalcAiGroupSpreadM(SCR_AIGroup scrGrp)
    {
        return SCR_RSS_AIUpdateInterval.CalcAiGroupSpreadM(scrGrp);
    }

    static int GetAliveMemberCount(SCR_AIGroup scrGrp)
    {
        return SCR_RSS_AIUpdateInterval.GetAliveMemberCount(scrGrp);
    }


    int RSS_GetCombatStimPhase()
    {
        return m_iCombatStimPhase;
    }

    bool RSS_IsCaffeineSodiumBenzoateActive()
    {
        return SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase);
    }

    bool RSS_IsCombatStimOverdosed()
    {
        return SCR_CombatStimStateMachine.IsOverdosed(m_iCombatStimPhase);
    }

    protected void RSS_CombatStim_OnTickTransitions()
    {
        float wt = GetGame().GetWorld().GetWorldTime() / 1000.0;
        int oldPhase = m_iCombatStimPhase;
        float oldEndsAt = m_fCombatStimPhaseEndsAt;
        int nextPhase = m_iCombatStimPhase;
        float nextEndsAt = m_fCombatStimPhaseEndsAt;
        if (!SCR_CombatStimStateMachine.AdvancePhase(
                m_iCombatStimPhase, m_fCombatStimPhaseEndsAt, wt,
                nextPhase, nextEndsAt))
            return;

        m_iCombatStimPhase = nextPhase;
        m_fCombatStimPhaseEndsAt = nextEndsAt;
        if (m_iCombatStimPhase != ERSS_CombatStimPhase.DELAY)
            m_iCombatStimDelayInjectionCount = 0;

        if (IsRssDebugEnabled())
            PrintFormat("[RSS][CombatStim] TickTransition: phase %1 -> %2, endsAt %3 -> %4, now=%5",
                oldPhase, m_iCombatStimPhase,
                Math.Round(oldEndsAt * 10.0) / 10.0,
                Math.Round(m_fCombatStimPhaseEndsAt * 10.0) / 10.0,
                Math.Round(wt * 10.0) / 10.0);

        if (Replication.IsServer() && IsPlayerControlled())
            Rpc(RPC_CombatStimSyncToOwner, m_iCombatStimPhase, m_fCombatStimPhaseEndsAt);

        ChimeraCharacter ch = ChimeraCharacter.Cast(GetOwner());
        if (ch)
            SCR_RSS_CombatStimController.UpdateBleedingScale(
                m_iCombatStimPhase, m_fRSS_CombatStimBleedingBaseline, ch,
                m_fRSS_CombatStimBleedingBaseline);
    }

    void RSS_CombatStim_OnInjectServer()
    {
        if (!Replication.IsServer())
            return;
        IEntity ownerEnt = GetOwner();
        ChimeraCharacter ch = ChimeraCharacter.Cast(ownerEnt);
        if (!ch)
            return;

        float wt = GetGame().GetWorld().GetWorldTime() / 1000.0;
        int nextPhase = m_iCombatStimPhase;
        float nextEndsAt = m_fCombatStimPhaseEndsAt;
        int nextDelayInjectionCount = m_iCombatStimDelayInjectionCount;
        bool shouldDie = false;
        if (!SCR_CombatStimStateMachine.TryStartFromInjection(
                m_iCombatStimPhase, m_fCombatStimPhaseEndsAt, wt,
                m_iCombatStimDelayInjectionCount,
                nextPhase, nextEndsAt, nextDelayInjectionCount, shouldDie))
            return;

        if (IsRssDebugEnabled())
            PrintFormat("[RSS][CombatStim][Server] Inject: oldPhase=%1 => nextPhase=%2 shouldDie=%3 now=%4",
                m_iCombatStimPhase, nextPhase, shouldDie, Math.Round(wt * 10.0) / 10.0);

        if (shouldDie)
        {
            SCR_RSS_CombatStimController.ResetBleedingScaleBeforeKill(ch, m_fRSS_CombatStimBleedingBaseline, m_fRSS_CombatStimBleedingBaseline);
            SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(ch.GetDamageManager());
            if (dmgMgr)
                dmgMgr.Kill(Instigator.CreateInstigator(ch));
            return;
        }

        m_iCombatStimPhase = nextPhase;
        m_fCombatStimPhaseEndsAt = nextEndsAt;
        m_iCombatStimDelayInjectionCount = nextDelayInjectionCount;
        if (m_iCombatStimPhase != ERSS_CombatStimPhase.DELAY)
            m_iCombatStimDelayInjectionCount = 0;

        if (Replication.IsServer() && IsPlayerControlled())
            Rpc(RPC_CombatStimSyncToOwner, m_iCombatStimPhase, m_fCombatStimPhaseEndsAt);

        SCR_RSS_CombatStimController.UpdateBleedingScale(
            m_iCombatStimPhase, m_fRSS_CombatStimBleedingBaseline, ch,
            m_fRSS_CombatStimBleedingBaseline);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void RPC_CombatStimSyncToOwner(int phase, float phaseEndsAt)
    {
        m_iCombatStimPhase = phase;
        m_fCombatStimPhaseEndsAt = phaseEndsAt;
    }


//! 客户端上报：仅用于数据导出/对照（m_bDataExportEnabled），非强反作弊与玩法权威判据。
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientReportStamina(float staminaPercent, float weight, float clientTimestamp, bool isCriticalData)
    {
        if (!Replication.IsServer())
            return;
        if (!SCR_RSS_ConfigManager.GetSettings() || !SCR_RSS_ConfigManager.GetSettings().m_bDataExportEnabled)
            return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        bool shouldIgnore = false;
        float clampedStamina = SCR_PlayerBaseRpcHandler.ProcessClientReport_ValidateStamina(
            staminaPercent, weight, currentTime, clientTimestamp, isCriticalData,
            m_pNetworkSyncManager, IsRssDebugEnabled(), shouldIgnore);
        if (shouldIgnore)
            return;

        float serverWeight = SCR_PlayerBaseNetworkHelper.GetServerWeight(GetOwner(), m_pEncumbranceCache);
        float encPenalty = SCR_PlayerBaseNetworkHelper.CalculateEncumbrancePenaltyFallback(serverWeight);
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            encPenalty = m_pEncumbranceCache.GetSpeedPenaltyFraction();

        IEntity ownerEnt = GetOwner();
        bool shouldSuppressSlopeServer = (m_pEnvironmentFactor && ownerEnt && m_pEnvironmentFactor.ShouldSuppressTerrainSlopeForEntity(ownerEnt));

        float slopeAngleDegrees = 0.0;
        if (!shouldSuppressSlopeServer)
            slopeAngleDegrees = SCR_RSS_SpeedCalculator.GetSlopeAngle(this, m_pEnvironmentFactor, GetVelocity());

        float rawSlopeServer = SCR_RSS_SpeedCalculator.GetRawSlopeAngle(this, GetVelocity());
        if (shouldSuppressSlopeServer && Math.AbsFloat(rawSlopeServer) > 0.0)
            encPenalty = encPenalty * SCR_RSS_Constants.GetIndoorStairsEncumbranceSpeedFactor();

        float validated = SCR_PlayerBaseRpcHandler.ProcessClientReport_CalculateValidation(
            clampedStamina, serverWeight, encPenalty,
            IsSprinting(), GetCurrentMovementPhase(),
            SCR_RSS_MetabolismMath.IsExhausted(clampedStamina),
            SCR_RSS_MetabolismMath.CanSprint(clampedStamina),
            SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(GetVelocity()),
            slopeAngleDegrees, GetSprintStartTime());

        if (m_pNetworkSyncManager)
        {
            currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float speedDiff = 0.0;
            if (SCR_PlayerBaseRpcHandler.ProcessClientReport_ShouldSync(
                    m_pNetworkSyncManager, validated, currentTime, speedDiff))
            {
                m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(validated);
                Rpc(RPC_ServerSyncSpeedMultiplier, validated, currentTime);
            }
        }
    }

    //! 客户端 → 服务端：必须用本方法包装 Rpc()，依据引擎源码约定，而非猜测。
    void RSS_RequestAdminConfigUpdate(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        if (Replication.IsServer())
            return;
        Rpc(RPC_AdminUpdateConfig, preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_AdminUpdateConfig(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        if (!Replication.IsServer())
            return;
        IEntity owner = GetOwner();
        if (!owner)
            return;

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return;

        int pid = pm.GetPlayerIdFromControlledEntity(owner);
        if (pid <= 0)
        {
            Print("[RSS] RPC_AdminUpdateConfig: no controlling player for entity");
            return;
        }
        if (!pm.HasPlayerRole(pid, EPlayerRole.ADMINISTRATOR)
            && !pm.HasPlayerRole(pid, EPlayerRole.SESSION_ADMINISTRATOR)
            && !pm.HasPlayerRole(pid, EPlayerRole.GAME_MASTER))
        {
            PrintFormat("[RSS] RPC_AdminUpdateConfig: access denied for playerId=%1", pid);
            return;
        }

        SCR_RSS_ConfigManager.AdminApplyAndSave(preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerSyncSpeedMultiplier(float speedMultiplier, float serverTimestamp)
    {
        if (Replication.IsServer() || !m_pNetworkSyncManager)
            return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
        float timestampDelta = 0.0;
        if (!SCR_PlayerBaseRpcHandler.IsValidServerSyncTimestamp(currentTime, serverTimestamp, timestampDelta))
        {
            if (IsRssDebugEnabled())
                PrintFormat("[RSS] Stale speed validation ignored (latency: %1s)", timestampDelta);
            return;
        }
        m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(speedMultiplier);
    }

}
