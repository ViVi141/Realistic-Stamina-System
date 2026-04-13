modded class SCR_CharacterControllerComponent
{
    protected float m_fLastSecondSpeed = 0.0; // 上一秒的速度
    protected float m_fCurrentSecondSpeed = 0.0; // 当前秒的速度
    protected bool m_bHasPreviousSpeed = false; // 是否已有上一秒的速度数据
    protected const int SPEED_SAMPLE_INTERVAL_MS = 1000; // 每秒采集一次速度样本
    
    protected float m_fLastStaminaPercent = 1.0;
    protected float m_fLastSpeedMultiplier = 1.0;
    protected float m_fLastStaminaUpdateTime = -1.0; // 上次体力更新时间（秒，GetWorldTime），用于按时间计算 timeDelta
    protected SCR_CharacterStaminaComponent m_pStaminaComponent; // 体力组件引用
    protected bool m_bLastExhaustedState = false; // 上次精疲力尽状态，用于状态切换日志
    
    protected ref NetworkSyncManager m_pNetworkSyncManager;
    protected string m_sLastSpeedSource = "";  // 调试用：上次速度计算来源（Server/Client）
    protected float m_fLastReconnectTime = -1.0; // 上次重连时间
    protected const float SERVER_CONFIG_SYNC_INTERVAL = 5.0; // 未收到配置时的重试间隔（秒），收到后不再请求
    protected const float RECONNECT_SYNC_DELAY = 2.0; // 重连后同步延迟（秒）
    protected const float CONFIG_FETCH_TIMEOUT_SEC = 30.0; // 配置获取超时（秒），超时后每 30 秒打印一次警告
    protected bool m_bIsConnected = false; // 网络连接状态
    protected float m_fFirstConfigRequestTime = -1.0; // 首次请求配置时间（秒），-1 表示未在等待
    protected float m_fLastConfigTimeoutWarningTime = -1.0; // 上次打印超时警告时间，避免刷屏
    protected bool m_bClientAckedConfig = false;  // 服务器：该客户端是否已回执"已收到配置"，收到后不再推送
    protected float m_fLastHeartbeatTime = 0.0; // 上次心跳时间
    protected int m_iPlayerId = 0; // 玩家ID缓存
    protected int m_iConfigRetryCount = 0; // 配置请求重试次数
    protected const int MAX_CONFIG_RETRY_COUNT = 3; // 最大配置重试次数，超过后请求广播配置
    protected float m_fLastHeartbeatSendTime = 0.0; // 上次发送心跳时间
    protected const float HEARTBEAT_INTERVAL = 5.0; // 心跳发送间隔（秒）
    protected const float HEARTBEAT_TIMEOUT = 15.0; // 心跳超时阈值（秒）
    protected string m_sLastAppliedConfigHash = ""; // 上次应用的配置哈希值，用于检测内容变化

    protected ref CollapseTransition m_pCollapseTransition;
    protected ref SlopeSpeedTransition m_pSlopeSpeedTransition;
    
    protected ref ExerciseTracker m_pExerciseTracker;
    
    protected ref JumpVaultDetector m_pJumpVaultDetector;
    
    protected ref EncumbranceCache m_pEncumbranceCache;
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent; // 缓存库存组件（EncumbranceCache 无效时的回退）
    
    protected ref FatigueSystem m_pFatigueSystem;
    
    protected ref TerrainDetector m_pTerrainDetector;
    
    protected ref EnvironmentFactor m_pEnvironmentFactor;
    
    protected ref UISignalBridge m_pUISignalBridge;
    
    protected ref EpocState m_pEpocState;
    
    protected ref StanceTransitionManager m_pStanceTransitionManager;
    
    protected bool m_bWasSwimming = false; // 上一帧是否在游泳
    protected float m_fWetWeightStartTime = -1.0; // 湿重开始时间（上岸时间）
    protected float m_fCurrentWetWeight = 0.0; // 当前湿重（kg）
    protected bool m_bSwimmingVelocityDebugPrinted = false; // 是否已输出游泳速度调试信息

    protected vector m_vLastPositionSample = vector.Zero;
    protected bool m_bHasLastPositionSample = false;
    protected vector m_vComputedVelocity = vector.Zero;
    protected CompartmentAccessComponent m_pCompartmentAccess;
    protected CharacterAnimationComponent m_pAnimComponent;

    protected float m_fAnimSpeedCompensation = 1.0;
    protected float m_fLastAnimCompensationUpdateTime = -1.0;
    protected static const float ANIM_COMPENSATION_UPDATE_INTERVAL = 2.0;
    
    protected float m_fSprintStartTime = -1.0;       // 本次冲刺开始时间（世界时间秒），-1 表示未在爆发/已进入平稳期
    protected bool m_bLastWasSprinting = false;       // 上一帧是否处于 Sprint 状态，用于检测进入/离开冲刺
    protected float m_fSprintCooldownUntil = -1.0;    // 冷却结束时间（世界时间秒），此时间前再次冲刺不触发爆发，直接平稳期

    protected int m_iCombatStimPhase = ERSS_CombatStimPhase.NONE;
    protected float m_fCombatStimPhaseEndsAt = -1.0;
    protected int m_iCombatStimDelayInjectionCount = 0;
    
    protected ref RSS_MudSlipRunner m_pMudSlipRunner;
    protected float m_fRssMudSlipCameraShake01 = 0.0;
    protected float m_fLastRssSpeedMultiplierApplied = 1.0;
    protected bool m_bRssStaminaLoopActive = false;
    
    override void OnInit(IEntity owner)
    {
        super.OnInit(owner);
        
        if (!owner)
            return;
        
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();
            SCR_RSS_ConfigManager.RegisterConfigChangeListener(owner);
            if (IsPlayerControlled() && GetGame() && GetGame().GetCallqueue())
                GetGame().GetCallqueue().CallLater(ServerPushConfigToOwner, 3000, false);

            if (IsRssDebugEnabled())
            {
                float coeff = StaminaConstants.GetEnergyToStaminaCoeff();
                PrintFormat("[RSS] 初始 energie->stamina coeff = %1", coeff);
            }
        }
        
        CharacterStaminaComponent staminaComp = GetStaminaComponent();
        m_pStaminaComponent = SCR_CharacterStaminaComponent.Cast(staminaComp);
        
        if (m_pStaminaComponent)
        {
            m_pStaminaComponent.SetAllowNativeStaminaSystem(false);
            
            m_pStaminaComponent.SetTargetStamina(RealisticStaminaSpeedSystem.INITIAL_STAMINA_AFTER_ACFT);
        }
        
        m_pJumpVaultDetector = new JumpVaultDetector();
        if (m_pJumpVaultDetector)
            m_pJumpVaultDetector.Initialize();
        
        m_pStanceTransitionManager = new StanceTransitionManager();
        if (m_pStanceTransitionManager)
        {
            m_pStanceTransitionManager.Initialize();
            m_pStanceTransitionManager.SetInitialStance(GetStance());
        }
        
        m_pExerciseTracker = new ExerciseTracker();
        if (m_pExerciseTracker)
        {
            float initTime = 0.0;
            if (GetGame() && GetGame().GetWorld())
                initTime = GetGame().GetWorld().GetWorldTime();
            m_pExerciseTracker.Initialize(initTime);
        } 
        
        m_pCollapseTransition = new CollapseTransition();
        if (m_pCollapseTransition)
            m_pCollapseTransition.Initialize();

        m_pSlopeSpeedTransition = new SlopeSpeedTransition();
        if (m_pSlopeSpeedTransition)
            m_pSlopeSpeedTransition.Initialize();
        
        m_pTerrainDetector = new TerrainDetector();
        if (m_pTerrainDetector)
            m_pTerrainDetector.Initialize(!IsPlayerControlled());
        
        m_pMudSlipRunner = new RSS_MudSlipRunner();

        m_pEnvironmentFactor = new EnvironmentFactor();
        if (m_pEnvironmentFactor)
        {
            World world = null;
            if (GetGame())
                world = GetGame().GetWorld();
            if (world)
            {
                m_pEnvironmentFactor.Initialize(world, owner);
                m_pEnvironmentFactor.SetUseEngineWeather(true);
                m_pEnvironmentFactor.SetUseEngineTemperature(false); // 使用模组内温度计算，每 5 秒步进
            }
        }
        
        m_pFatigueSystem = new FatigueSystem();
        if (m_pFatigueSystem)
        {
            float currentTime = 0.0;
            if (GetGame() && GetGame().GetWorld())
                currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            m_pFatigueSystem.Initialize(currentTime);
        }
        
        m_pEncumbranceCache = new EncumbranceCache();
        if (m_pEncumbranceCache)
        {
            ChimeraCharacter character = ChimeraCharacter.Cast(owner);
            if (character)
            {
                SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
                m_pCachedInventoryComponent = inventoryComponent;

                InventoryStorageManagerComponent inventoryManagerComponent = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));

                m_pEncumbranceCache.Initialize(inventoryComponent);
            }
        }
        
        m_pUISignalBridge = new UISignalBridge();
        if (m_pUISignalBridge)
            m_pUISignalBridge.Init(owner);
        
        m_pEpocState = new EpocState();
        
        m_pNetworkSyncManager = new NetworkSyncManager();
        if (m_pNetworkSyncManager)
            m_pNetworkSyncManager.Initialize();
        
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            m_pCompartmentAccess = character.GetCompartmentAccessComponent();
            m_pAnimComponent = character.GetAnimationComponent();
        }
        
        GetGame().GetCallqueue().CallLater(StartSystem, 500, false);
        if (!Replication.IsServer())
            GetGame().GetCallqueue().CallLater(EnsureRssStaminaLoopIfNeeded, 2000, false);
        if (Replication.IsServer() && !IsPlayerControlled())
            GetGame().GetCallqueue().CallLater(EnsureAiStaminaLoopOnServer, 3000, false);
        
        if (!Replication.IsServer())
        {
            m_bIsConnected = false;
            GetGame().GetCallqueue().CallLater(MonitorNetworkConnection, 5000, true);
            GetGame().GetCallqueue().CallLater(RequestServerConfig, 1500, false);
            GetGame().GetCallqueue().CallLater(RequestServerConfigUntilApplied, 2500, false);
            GetGame().GetCallqueue().CallLater(UpdateHeartbeat, HEARTBEAT_INTERVAL * 1000, true);
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

    void UpdateSpeedBasedOnStamina()
    {
        IEntity owner = GetOwner();
        if (!owner)
        {
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS, false);
            return;
        }

        if (!ShouldProcessStaminaUpdate())
        {
            RSS_SetMudSlipCameraShake01(0.0);
            m_bRssStaminaLoopActive = false;
            return;
        }

        if (Replication.IsServer() && !IsPlayerControlled())
        {
            if (SCR_RSS_AIGroupStaminaProxy.ProcessFollowerProxySync(this, owner))
            {
                RSS_SetMudSlipCameraShake01(0.0);
                m_bRssStaminaLoopActive = true;
                GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, StaminaConstants.RSS_PERF_AI_GROUP_PROXY_INTERVAL_MS, false);
                return;
            }
        }
        
        if (RSS_HandleVehicleStaminaUpdate(owner))
            return;
        
        RSS_CombatStim_OnTickTransitions();

        float staminaPercent = 1.0;
        if (m_pStaminaComponent)
            staminaPercent = m_pStaminaComponent.GetTargetStamina();

        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        staminaPercent = RSS_CombatStim_AdjustStaminaRead(staminaPercent);
        
        float encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.CheckAndUpdate();
            encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenalty();
        }

        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);
        if (isExhausted)
        {
            float limpSpeedMultiplier = RealisticStaminaSpeedSystem.GetDynamicLimpMultiplier(encumbranceSpeedPenalty);
            float compensatedLimpMultiplier = Math.Clamp(limpSpeedMultiplier * m_fAnimSpeedCompensation, 0.01, 1.0);
            OverrideMaxSpeed(compensatedLimpMultiplier);

            if (!m_bLastExhaustedState && IsRssDebugEnabled())
            {
                Print("[RSS] 精疲力尽 / Exhausted: 速度限制为动态跛行速度 | Speed Limited to Dynamic Limp Speed");
                m_bLastExhaustedState = true;
            }

        }
        else
        {
            if (m_bLastExhaustedState && IsRssDebugEnabled())
            {
                Print("[RSS] 脱离精疲力尽状态 / Recovered from Exhaustion: 速度恢复正常 | Speed Restored");
                m_bLastExhaustedState = false;
            }
        }
        
        bool isSwimmingForSpeed = SwimmingStateManager.IsSwimming(this);
        vector velocity;
        float currentSpeed;
        if (isSwimmingForSpeed)
        {
            float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
            SpeedCalculationResult speedResult = StaminaUpdateCoordinator.CalculateCurrentSpeed(
                owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
            velocity = speedResult.computedVelocity;
            currentSpeed = Math.Min(speedResult.computedVelocity.Length(), 7.0);  // 游泳用 3D 速度模长
            m_vLastPositionSample = speedResult.lastPositionSample;
            m_bHasLastPositionSample = speedResult.hasLastPositionSample;
            m_vComputedVelocity = speedResult.computedVelocity;
        }
        else
        {
            velocity = GetVelocity();
            currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(velocity);
            m_bHasLastPositionSample = false;  // 离开游泳时重置
        }
        
        RSS_UpdateTacticalSprintState();
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        bool isSprintActive = isSprintingNow || (phaseNow == 3);
        
        float currentTimeForExerciseMs = GetGame().GetWorld().GetWorldTime();
        float currentTime = currentTimeForExerciseMs / 1000.0; // 秒，供地形/环境/RPC节流等模块使用
        float terrainFactor = 1.0; // 默认值（铺装路面）
        if (m_pTerrainDetector)
            terrainFactor = m_pTerrainDetector.GetTerrainFactor(owner, currentTime, currentSpeed);

        if (m_pEnvironmentFactor)
            m_pEnvironmentFactor.UpdateEnvironmentFactors(currentTime, owner, velocity, terrainFactor, m_fCurrentWetWeight);

        float finalSpeedMultiplier = StaminaUpdateCoordinator.UpdateSpeed(
            this,
            staminaPercent,
            encumbranceSpeedPenalty,
            m_pCollapseTransition,
            currentSpeed,
            m_pEnvironmentFactor,
            m_pSlopeSpeedTransition,
            velocity);
        
        float baseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
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
        float finalSpeedToApply = Math.Clamp(speedToApply, 0.01, 1.0);
        m_fLastRssSpeedMultiplierApplied = finalSpeedToApply;
        OverrideMaxSpeed(finalSpeedToApply);
        if (IsPlayerControlled())
            m_sLastSpeedSource = "Client";
        else
            m_sLastSpeedSource = "Server";

        bool isCriticalData = (staminaPercent <= 0.05 || (m_pNetworkSyncManager && m_pNetworkSyncManager.GetLastReportedStaminaPercent() > 0.5 && staminaPercent <= 0.1));
        if (!Replication.IsServer() && IsPlayerControlled() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled())
        {
            int syncType = 0;
            if (isCriticalData)
                syncType = 1;
            if (m_pNetworkSyncManager.ShouldSync(currentTime, syncType))
            {
                Rpc(RPC_ClientReportStamina, staminaPercent, currentWeight, currentTime, isCriticalData);
                if (isCriticalData && IsRssDebugEnabled())
                    PrintFormat("[RSS] Critical stamina event reported (stamina=%1)", staminaPercent);
            }
        }

        bool isSwimming = SwimmingStateManager.IsSwimming(this);

        float timeDeltaSec;
        if (m_fLastStaminaUpdateTime >= 0.0)
            timeDeltaSec = currentTime - m_fLastStaminaUpdateTime;
        else
            timeDeltaSec = GetSpeedUpdateIntervalMs() / 1000.0;
        timeDeltaSec = Math.Clamp(timeDeltaSec, 0.01, 0.5);
        
        if (isSwimming != m_bWasSwimming)
            m_bSwimmingVelocityDebugPrinted = false;
        
        WetWeightUpdateResult wetWeightResult = SwimmingStateManager.UpdateWetWeight(
            m_bWasSwimming,
            isSwimming,
            currentTime,
            m_fWetWeightStartTime,
            m_fCurrentWetWeight,
            owner);
        m_fWetWeightStartTime = wetWeightResult.wetWeightStartTime;
        m_fCurrentWetWeight = wetWeightResult.currentWetWeight;
        
        m_bWasSwimming = isSwimming;
        
        float heatStressMultiplier = 1.0;
        if (m_pEnvironmentFactor)
            heatStressMultiplier = m_pEnvironmentFactor.GetHeatStressMultiplier();
        
        float rainWeight = 0.0;
        if (m_pEnvironmentFactor)
            rainWeight = m_pEnvironmentFactor.GetRainWeight();

        if (RSS_IsCaffeineSodiumBenzoateActive())
        {
            heatStressMultiplier = 1.0;
            rainWeight = 0.0;
        }
        
        float totalWetWeight = SwimmingStateManager.CalculateTotalWetWeight(m_fCurrentWetWeight, rainWeight);
        float currentWeightWithWet = currentWeight + totalWetWeight;

        float totalWeight = currentWeight + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+身体
        float totalWeightWithWetAndBody = currentWeightWithWet + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+湿重+身体

        bool useSwimmingModel = isSwimming;

        if (m_pStaminaComponent && m_pJumpVaultDetector)
        {
            SignalsManagerComponent signalsManager = null;
            if (m_pUISignalBridge)
                signalsManager = m_pUISignalBridge.GetSignalsManager();
            int exhaustionSignalID = -1;
            if (m_pUISignalBridge)
                exhaustionSignalID = m_pUISignalBridge.GetExhaustionSignalID();
            
            bool encumbranceCacheValid = false;
            float encumbranceCurrentWeight = 0.0;
            if (m_pEncumbranceCache)
            {
                encumbranceCacheValid = m_pEncumbranceCache.IsCacheValid();
                encumbranceCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            }
            float jumpCost = m_pJumpVaultDetector.ProcessJump(
                owner, 
                this, 
                staminaPercent, 
                encumbranceCacheValid, 
                encumbranceCurrentWeight,
                signalsManager, 
                exhaustionSignalID
            );
            if (jumpCost > 0.0 && IsRssDebugEnabled())
            {
                PrintFormat("[RSS] 跳跃消耗 / Jump Cost: -%1%% | -%1%%", 
                    Math.Round(jumpCost * 100.0 * 10.0) / 10.0);
            }
            staminaPercent = staminaPercent - jumpCost;
            
            bool vaultEncumbranceCacheValid = false;
            float vaultEncumbranceCurrentWeight = 0.0;
            if (m_pEncumbranceCache)
            {
                vaultEncumbranceCacheValid = m_pEncumbranceCache.IsCacheValid();
                vaultEncumbranceCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            }
            float vaultCost = m_pJumpVaultDetector.ProcessVault(
                owner, 
                this, 
                vaultEncumbranceCacheValid, 
                vaultEncumbranceCurrentWeight
            );
            if (vaultCost > 0.0 && IsRssDebugEnabled())
            {
                PrintFormat("[RSS] 翻越消耗 / Vault Cost: -%1%% | -%1%%", 
                    Math.Round(vaultCost * 100.0 * 10.0) / 10.0);
            }
            staminaPercent = staminaPercent - vaultCost;
            
            m_pJumpVaultDetector.UpdateCooldowns();
            
            m_pStanceTransitionManager.UpdateFatigue(timeDeltaSec);
            
            float currentTimeSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float stanceSettleCost = m_pStanceTransitionManager.TrySettleWindow(currentTimeSec);
            if (stanceSettleCost > 0.0)
                staminaPercent = staminaPercent - stanceSettleCost;
            
            bool stanceEncumbranceCacheValid = false;
            float stanceEncumbranceCurrentWeight = 0.0;
            if (m_pEncumbranceCache)
            {
                stanceEncumbranceCacheValid = m_pEncumbranceCache.IsCacheValid();
                stanceEncumbranceCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            }
            float stanceTransitionCost = m_pStanceTransitionManager.ProcessStanceTransition(
                owner,
                this,
                staminaPercent,
                stanceEncumbranceCacheValid,
                stanceEncumbranceCurrentWeight
            );
            if (stanceTransitionCost > 0.0)
            {
                staminaPercent = staminaPercent - stanceTransitionCost;
            }
            
            staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        }
        
        float speedRatio = Math.Clamp(currentSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED, 0.0, 1.0);
        
        if (m_pFatigueSystem)
        {
            float currentTimeForFatigue = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
            m_pFatigueSystem.ProcessFatigueDecay(currentTimeForFatigue, currentSpeed);
        }
        
        bool isCurrentlyMoving = (currentSpeed > 0.05);
        float fatigueFactor = 1.0; // 默认值（无疲劳）
        
        if (m_pExerciseTracker)
        {
            m_pExerciseTracker.Update(currentTimeForExerciseMs, isCurrentlyMoving);
            
            fatigueFactor = m_pExerciseTracker.CalculateFatigueFactor();
        }
        
        float metabolicEfficiencyFactor = StaminaConsumptionCalculator.CalculateMetabolicEfficiencyFactor(speedRatio);
        float fitnessEfficiencyFactor = StaminaConsumptionCalculator.CalculateFitnessEfficiencyFactor();
        float totalEfficiencyFactor = fitnessEfficiencyFactor * metabolicEfficiencyFactor;
        
        vector velocityForDrain = velocity;
        if (useSwimmingModel && !HasSwimInput())
            velocityForDrain = vector.Zero;

        float slopeAngleDegrees = 0.0; // 初始化坡度角度
        GradeCalculationResult gradeResult = SpeedCalculator.CalculateGradePercent(
            this,
            currentSpeed,
            m_pJumpVaultDetector,
            slopeAngleDegrees,
            m_pEnvironmentFactor,
            velocityForDrain);
        float gradePercent = gradeResult.gradePercent;
        slopeAngleDegrees = gradeResult.slopeAngleDegrees;

        if (StaminaConstants.IsMudSlipMechanismEnabled())
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

            SCR_RSS_AIStaminaBridge.MaybeApplyMudSlipSpeedCap(this, owner);
        }
        else
        {
            RSS_SetMudSlipCameraShake01(0.0);
        }
        SCR_RSS_AIStaminaBridge.TickAbortRestRecoveryIfBattlefieldDanger(owner);
        SCR_RSS_AIStaminaBridge.ApplyOnFootMovementPolicy(this, owner, staminaPercent);
        SCR_RSS_AIGroupRestCoordinator.TryScheduleGroupRestFromStamina(owner, staminaPercent);
        SCR_RSS_AIGroupRestCoordinator.TryCompleteGroupRestDefendWaypointIfReady(owner);
        SCR_RSS_AICoverSeeker.TickVerifyCombatCover(owner);

        if (Replication.IsServer() && !IsPlayerControlled())
            SCR_RSS_AIStaminaCombatEffects.ApplyStaminaToCombat(owner, staminaPercent);

        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();

        BaseDrainRateResult drainRateResult = StaminaUpdateCoordinator.CalculateBaseDrainRate(
            useSwimmingModel,
            currentSpeed,
            encumbranceSpeedPenalty,
            totalWeight,
            totalWeightWithWetAndBody,
            gradePercent,
            terrainFactor,
            velocityForDrain,
            m_bSwimmingVelocityDebugPrinted,
            owner,
            m_pEnvironmentFactor, // v2.14.0修复：传递环境因子
            isSprinting,
            currentMovementPhase);
        float baseDrainRateByVelocity = drainRateResult.baseDrainRate;
        m_bSwimmingVelocityDebugPrinted = drainRateResult.swimmingVelocityDebugPrinted;
        
        float postureMultiplier = 1.0;
        float gradePercentForConsumption = 0.0;
        float terrainFactorForConsumption = 1.0;
        
        if (!useSwimmingModel)
        {
            postureMultiplier = StaminaConsumptionCalculator.CalculatePostureMultiplier(currentSpeed, this);
            gradePercentForConsumption = gradePercent;
            terrainFactorForConsumption = terrainFactor;
        }
        
        float encumbranceStaminaDrainMultiplier = 1.0;
        if (m_pEncumbranceCache)
            encumbranceStaminaDrainMultiplier = m_pEncumbranceCache.GetStaminaDrainMultiplier();
        
        if (useSwimmingModel)
            encumbranceStaminaDrainMultiplier = 1.0;
        
        float sprintMultiplier = 1.0;
        
        float baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        float totalDrainRate = 0.0;
        
        if (useSwimmingModel)
        {
            totalDrainRate = baseDrainRateByVelocity;
            totalDrainRate = totalDrainRate * totalEfficiencyFactor * fatigueFactor;
        }
        else
        {
            totalDrainRate = StaminaConsumptionCalculator.CalculateStaminaConsumption(
                currentSpeed,
                currentWeight,
                gradePercentForConsumption,
                terrainFactorForConsumption,
                postureMultiplier,
                totalEfficiencyFactor,
                fatigueFactor,
                sprintMultiplier,
                encumbranceStaminaDrainMultiplier,
                m_pFatigueSystem,
                baseDrainRateByVelocityForModule,
                m_pEnvironmentFactor, // v2.14.0：传递环境因子模块
                owner, // v2.15.0：传递角色实体，用于手持物品检测
                isSprinting,
                currentMovementPhase);
            
            float drainRateBeforeHeat = totalDrainRate;
            totalDrainRate = totalDrainRate * heatStressMultiplier;
            
        }

        if (RSS_IsCaffeineSodiumBenzoateActive())
            totalDrainRate = totalDrainRate * SCR_CombatStimConstants.STAMINA_DRAIN_MULTIPLIER;
        
        if (baseDrainRateByVelocityForModule == 0.0 && baseDrainRateByVelocity > 0.0)
            baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
        if (m_pEpocState && !useSwimmingModel)
        {
            float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
            bool isInEpocDelay = StaminaRecoveryCalculator.UpdateEpocDelay(
                m_pEpocState,
                currentSpeed,
                currentWorldTime);
        }
        
        if (owner == SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled() && IsPlayerControlled())
            StaminaConstants.StartDebugBatch();
        
        if (m_pStaminaComponent)
        {
            float newTargetStamina = StaminaUpdateCoordinator.UpdateStaminaValue(
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
            m_fLastStaminaUpdateTime = currentTime;
            
            float verifyStamina = m_pStaminaComponent.GetStamina();
            if (Math.AbsFloat(verifyStamina - newTargetStamina) > 0.005) // 偏差超过0.5%
            {
                if (StaminaConstants.IsDebugBatchActive())
                {
                    string intLine = string.Format("[RSS] 原生干扰: 目标=%1%% 实际=%2%% 偏差=%3%%",
                        Math.Round(newTargetStamina * 100.0).ToString(),
                        Math.Round(verifyStamina * 100.0).ToString(),
                        Math.Round(Math.AbsFloat(verifyStamina - newTargetStamina) * 10000.0) / 100.0);
                    StaminaConstants.AddDebugBatchLine(intLine);
                }
                m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            }
            
            staminaPercent = newTargetStamina;
        }
        
        if (m_pUISignalBridge)
        {
            m_pUISignalBridge.UpdateUISignal(staminaPercent, isExhausted, currentSpeed, totalDrainRate, false);
        }
        
        m_fLastStaminaPercent = staminaPercent;
        m_fLastSpeedMultiplier = finalSpeedMultiplier;

        if (owner != SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled())
        {
            string movementStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
            string aiLine = string.Format("[RSS] AI: %1 | 体力=%2%% 速度倍=%3 速度=%4m/s 类型=%5 | 来源:%6",
                owner.GetName(),
                Math.Round(staminaPercent * 100.0).ToString(),
                Math.Round(finalSpeedMultiplier * 100.0) / 100.0,
                Math.Round(currentSpeed * 10.0) / 10.0,
                movementStr,
                m_sLastSpeedSource);
            SCR_RSS_AIStaminaBridge.AppendAIDebugLine(aiLine);
        }
        
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
            bool needDebugOutput = StaminaConstants.IsDebugBatchActive();
            bool needHintOutput = (settings && settings.m_bHintDisplayEnabled);

            if (needDebugOutput || needHintOutput)
            {
                float combatEncumbrancePercent = 0.0;
                float debugCurrentWeight = 0.0;
                if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
                {
                    debugCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
                    combatEncumbrancePercent = RealisticStaminaSpeedSystem.CalculateCombatEncumbrancePercent(owner);
                }

                float timeToDepleteSec = -1.0;
                float timeToFullSec = -1.0;
                if (needHintOutput)
                {
                    float netRate = StaminaUpdateCoordinator.GetNetStaminaRatePerSecond(
                        staminaPercent, useSwimmingModel, currentSpeed, totalDrainRate,
                        baseDrainRateByVelocity, baseDrainRateByVelocityForModule,
                        heatStressMultiplier, m_pEpocState, m_pEncumbranceCache,
                        m_pExerciseTracker, this, m_pEnvironmentFactor);
                    float targetStamina = 1.0;
                    if (m_pFatigueSystem)
                        targetStamina = m_pFatigueSystem.GetMaxStaminaCap();
                if (netRate < -0.0001)
                    {
                        timeToDepleteSec = staminaPercent / Math.AbsFloat(netRate);
                        if (timeToDepleteSec > 7200.0)
                            timeToDepleteSec = 7200.0;  // 上限 2h，避免净率接近 0 时显示异常大值
                    }
                    else if (netRate > 0.0001 && staminaPercent < targetStamina)
                    {
                        timeToFullSec = (targetStamina - staminaPercent) / netRate;
                        if (timeToFullSec > 7200.0)
                            timeToFullSec = 7200.0;
                    }
                }

                string movementTypeStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
                DebugInfoParams debugParams = new DebugInfoParams();
                debugParams.owner = owner;
                debugParams.movementTypeStr = movementTypeStr;
                debugParams.staminaPercent = staminaPercent;
                debugParams.baseSpeedMultiplier = baseSpeedMultiplier;
                debugParams.encumbranceSpeedPenalty = encumbranceSpeedPenalty;
                debugParams.finalSpeedMultiplier = finalSpeedMultiplier;
                debugParams.gradePercent = gradePercent;
                if (isSwimming)
                {
                    float horizontalLen = Math.Sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
                    float swimmingPitchDeg = 0.0;
                    if (horizontalLen > 0.01 || Math.AbsFloat(velocity[1]) > 0.01)
                    {
                        swimmingPitchDeg = Math.Atan2(velocity[1], horizontalLen) * Math.RAD2DEG;
                        swimmingPitchDeg = Math.Clamp(swimmingPitchDeg, -90.0, 90.0);
                    }
                    debugParams.slopeAngleDegrees = swimmingPitchDeg;
                }
                else
                {
                    debugParams.slopeAngleDegrees = slopeAngleDegrees;
                }
                debugParams.isSprinting = isSprinting;
                debugParams.currentMovementPhase = currentMovementPhase;
                debugParams.debugCurrentWeight = debugCurrentWeight;
                debugParams.combatEncumbrancePercent = combatEncumbrancePercent;
                debugParams.terrainDetector = m_pTerrainDetector;
                debugParams.environmentFactor = m_pEnvironmentFactor;
                debugParams.heatStressMultiplier = heatStressMultiplier;
                debugParams.rainWeight = rainWeight;
                debugParams.swimmingWetWeight = m_fCurrentWetWeight;
                debugParams.currentSpeed = currentSpeed;
                debugParams.isSwimming = isSwimming;
                debugParams.stanceTransitionManager = m_pStanceTransitionManager;
                debugParams.timeToDepleteSec = timeToDepleteSec;
                debugParams.timeToFullSec = timeToFullSec;
                debugParams.speedSource = m_sLastSpeedSource;

                if (needDebugOutput)
                {
                    DebugDisplay.OutputDebugInfo(debugParams);
                    SCR_RSS_AIStaminaBridge.FlushAIDebugLinesToBatch();
                }
                if (needHintOutput)
                    DebugDisplay.OutputHintInfo(debugParams);
            }
        }
        
        StaminaConstants.FlushDebugBatch();
        
        m_bRssStaminaLoopActive = true;
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
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
        return RealisticStaminaSpeedSystem.IsExhausted(stamina);
    }

    bool GetRssIsSwimming()
    {
        return SwimmingStateManager.IsSwimming(this);
    }

    float GetRssCurrentWeight()
    {
        return SCR_PlayerBaseRssApiHelper.GetCurrentWeight(m_pEncumbranceCache, m_pCachedInventoryComponent);
    }

    EnvironmentFactor GetRssEnvironmentFactor()
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

    protected float GetDynamicOriginalEngineMaxSpeed(int movementPhase)
    {
        if (!m_pAnimComponent)
        {
            if (movementPhase == 3)
                return RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            else
                return RealisticStaminaSpeedSystem.GAME_MAX_SPEED * RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        }
        
        float currentMultiplier = m_fLastSpeedMultiplier;
        
        OverrideMaxSpeed(1.0);
        
        float realOriginalSpeed = m_pAnimComponent.GetMaxSpeed(1.0, 0.0, movementPhase);
        
        OverrideMaxSpeed(currentMultiplier);
        
        return realOriginalSpeed;
    }

    void CollectSpeedSample()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return;
        
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        World world = GetGame().GetWorld();
        if (!world)
            return;
        
        vector velocity = GetVelocity();
        vector velocityXZ = vector.Zero;
        velocityXZ[0] = velocity[0];
        velocityXZ[2] = velocity[2];
        float speedHorizontal = velocityXZ.Length(); // 水平速度（米/秒）
        
        if (m_bHasPreviousSpeed)
        {
            DisplayStatusInfo();
        }
        
        m_fLastSecondSpeed = m_fCurrentSecondSpeed;
        m_fCurrentSecondSpeed = speedHorizontal;
        m_bHasPreviousSpeed = true; // 标记已有上一秒的数据
        
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }

    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        bool isSwimming = IsSwimmingByCommand();
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        
        SCR_PlayerBaseDebugHelper.OutputStatusInfo(
            owner,
            m_fLastSecondSpeed,
            m_fLastStaminaPercent,
            m_fLastSpeedMultiplier,
            isSwimming,
            isSprinting,
            currentMovementPhase,
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
        bool changed = SCR_PlayerBaseCombatStimHelper.AdvancePhase(
            m_iCombatStimPhase,
            m_fCombatStimPhaseEndsAt,
            wt,
            nextPhase,
            nextEndsAt);
        if (!changed)
            return;

        m_iCombatStimPhase = nextPhase;
        m_fCombatStimPhaseEndsAt = nextEndsAt;
        if (m_iCombatStimPhase != ERSS_CombatStimPhase.DELAY)
            m_iCombatStimDelayInjectionCount = 0;
        if (IsRssDebugEnabled())
        {
            PrintFormat("[RSS][CombatStim][Server] TickTransition: phase %1 -> %2, endsAt %3 -> %4, now=%5",
                oldPhase,
                m_iCombatStimPhase,
                Math.Round(oldEndsAt * 10.0) / 10.0,
                Math.Round(m_fCombatStimPhaseEndsAt * 10.0) / 10.0,
                Math.Round(wt * 10.0) / 10.0);
        }
        if (Replication.IsServer() && IsPlayerControlled())
            Rpc(RPC_CombatStimSyncToOwner, m_iCombatStimPhase, m_fCombatStimPhaseEndsAt);
    }

    protected float RSS_CombatStim_AdjustStaminaRead(float staminaPercent)
    {
        return staminaPercent;
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
        bool started = SCR_PlayerBaseCombatStimHelper.TryStartFromInjection(
            m_iCombatStimPhase,
            m_fCombatStimPhaseEndsAt,
            wt,
            m_iCombatStimDelayInjectionCount,
            nextPhase,
            nextEndsAt,
            nextDelayInjectionCount,
            shouldDie);
        if (!started)
            return;

        if (IsRssDebugEnabled())
        {
            PrintFormat("[RSS][CombatStim][Server] Inject: oldPhase=%1 oldEndsAt=%2 delayCount=%3 => nextPhase=%4 nextEndsAt=%5 nextDelayCount=%6 shouldDie=%7 now=%8",
                m_iCombatStimPhase,
                Math.Round(m_fCombatStimPhaseEndsAt * 10.0) / 10.0,
                m_iCombatStimDelayInjectionCount,
                nextPhase,
                Math.Round(nextEndsAt * 10.0) / 10.0,
                nextDelayInjectionCount,
                shouldDie,
                Math.Round(wt * 10.0) / 10.0);
        }

        if (shouldDie)
        {
            SCR_CharacterDamageManagerComponent damageMgr = SCR_CharacterDamageManagerComponent.Cast(ch.GetDamageManager());
            if (damageMgr)
            {
                Instigator selfInstigator = Instigator.CreateInstigator(ch);
                damageMgr.Kill(selfInstigator);
                if (IsRssDebugEnabled())
                    Print("[RSS][CombatStim][Server] OD threshold reached: player killed");
            }
            return;
        }

        m_iCombatStimPhase = nextPhase;
        m_fCombatStimPhaseEndsAt = nextEndsAt;
        m_iCombatStimDelayInjectionCount = nextDelayInjectionCount;
        if (m_iCombatStimPhase != ERSS_CombatStimPhase.DELAY)
            m_iCombatStimDelayInjectionCount = 0;

        if (Replication.IsServer() && IsPlayerControlled())
            Rpc(RPC_CombatStimSyncToOwner, m_iCombatStimPhase, m_fCombatStimPhaseEndsAt);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void RPC_CombatStimSyncToOwner(int phase, float phaseEndsAt)
    {
        m_iCombatStimPhase = phase;
        m_fCombatStimPhaseEndsAt = phaseEndsAt;
        if (IsRssDebugEnabled())
        {
            float wt = GetGame().GetWorld().GetWorldTime() / 1000.0;
            PrintFormat("[RSS][CombatStim][Client] Sync received: phase=%1 endsAt=%2 now=%3",
                m_iCombatStimPhase,
                Math.Round(m_fCombatStimPhaseEndsAt * 10.0) / 10.0,
                Math.Round(wt * 10.0) / 10.0);
        }
    }

    void RSS_TriggerMudSlipRagdoll()
    {
        if (m_pAnimComponent && m_pAnimComponent.IsRagdollActive())
            return;
        Ragdoll();
        RefreshRagdoll(StaminaConstants.ENV_MUD_SLIP_RAGDOLL_WARMUP_SEC);
        if (GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(RSS_MudSlip_FinishRagdoll, StaminaConstants.ENV_MUD_SLIP_RAGDOLL_BLEND_DELAY_MS, false);
    }

    protected void RSS_MudSlip_FinishRagdoll()
    {
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
        return SCR_RSS_AIMudSlipPolicy.IsBlockedBySafety(this, owner);
    }

    bool RSS_ShouldAiAllowMudSlipRagdoll(IEntity owner)
    {
        return SCR_RSS_AIMudSlipPolicy.ShouldAllowRagdoll(this, owner);
    }

    float RSS_GetLastAppliedSpeedMultiplier()
    {
        return m_fLastRssSpeedMultiplierApplied;
    }

    void ServerPushConfigToOwner()
    {
        if (!Replication.IsServer())
            return;
        if (m_bClientAckedConfig)
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;

        array<float> combinedPresetParams;
        array<float> floatSettings;
        array<int> intSettings;
        array<bool> boolSettings;
        BuildConfigArrays(combinedPresetParams, floatSettings, intSettings, boolSettings);

        Rpc(RPC_SendFullConfigOwner,
            settings.m_sConfigVersion,
            settings.m_sSelectedPreset,
            combinedPresetParams,
            floatSettings,
            intSettings,
            boolSettings
        );
        if (IsRssDebugEnabled())
            PrintFormat("[RSS] Server pushed config to client (will retry until ack): %1", GetPlayerLabel(GetOwner()));
        GetGame().GetCallqueue().CallLater(ServerPushConfigToOwner, 3000, false);
    }

    void OnConfigChanged()
    {
        if (Replication.IsServer())
        {
            SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
            if (settings)
            {
                array<float> combinedPresetParams = new array<float>();
                array<float> floatSettings = new array<float>();
                array<int> intSettings = new array<int>();
                array<bool> boolSettings = new array<bool>();
                BuildCombinedPresetArray(settings, combinedPresetParams);
                BuildSettingsArrays(settings, floatSettings, intSettings, boolSettings);

                Rpc(RPC_SendFullConfigOwner,
                    settings.m_sConfigVersion,
                    settings.m_sSelectedPreset,
                    combinedPresetParams,
                    floatSettings,
                    intSettings,
                    boolSettings
                );
            }
        }
    }

    protected string GetPlayerLabel(IEntity entity)
    {
        return SCR_PlayerBaseConfigHelper.GetPlayerLabel(entity);
    }

    protected bool IsRssDebugEnabled()
    {
        return SCR_PlayerBaseConfigHelper.IsRssDebugEnabled();
    }

    protected array<float> BuildPresetArray(SCR_RSS_Params p)
    {
        return SCR_PlayerBaseConfigHelper.BuildPresetArray(p);
    }

    protected void BuildSettingsArrays(SCR_RSS_Settings s, array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        SCR_PlayerBaseConfigHelper.BuildSettingsArrays(s, floatSettings, intSettings, boolSettings);
    }

    protected void BuildConfigArrays(out array<float> combinedPresetParams, out array<float> floatSettings, out array<int> intSettings, out array<bool> boolSettings)
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        SCR_PlayerBaseConfigHelper.BuildConfigArrays(settings, combinedPresetParams, floatSettings, intSettings, boolSettings);
    }

    protected void BuildCombinedPresetArray(SCR_RSS_Settings s, array<float> outCombined)
    {
        SCR_PlayerBaseConfigHelper.BuildCombinedPresetArray(s, outCombined);
    }

    protected void ApplyFullConfig(string configVersion, string selectedPreset,
        array<float> eliteParams, array<float> standardParams, array<float> tacticalParams, array<float> customParams,
        array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        if (Replication.IsServer())
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            settings = new SCR_RSS_Settings();

        string newConfigHash = CalculateConfigHash(configVersion, selectedPreset, floatSettings, intSettings, boolSettings);

        bool shouldSkipApply = SCR_PlayerBaseConfigHelper.ShouldSkipApply(
            SCR_RSS_ConfigManager.IsServerConfigApplied(),
            m_sLastAppliedConfigHash,
            newConfigHash);
        if (shouldSkipApply)
        {
            m_fFirstConfigRequestTime = -1.0;
            m_fLastConfigTimeoutWarningTime = -1.0;
            if (IsRssDebugEnabled())
                Print("[RSS] Config unchanged (hash match), skipping apply");
            return;
        }

        SCR_PlayerBaseConfigHelper.ApplyFullConfigPayload(
            settings,
            configVersion,
            selectedPreset,
            eliteParams,
            standardParams,
            tacticalParams,
            customParams,
            floatSettings,
            intSettings,
            boolSettings);

        SCR_RSS_ConfigManager.Save();
        SCR_RSS_ConfigManager.SetServerConfigApplied(true);
        m_fFirstConfigRequestTime = -1.0;  // 收到配置，清除超时计时
        m_fLastConfigTimeoutWarningTime = -1.0;
        m_sLastAppliedConfigHash = newConfigHash; // 更新配置哈希
        PrintFormat("[RSS] Applied full server config: preset=%1, version=%2, hash=%3", selectedPreset, configVersion, newConfigHash);

        if (settings.m_bHintDisplayEnabled)
            SCR_StaminaHUDComponent.Init();
        else
            SCR_StaminaHUDComponent.Destroy();
    }

    protected string CalculateConfigHash(string configVersion, string selectedPreset, array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        return SCR_PlayerBaseConfigHelper.CalculateConfigHash(configVersion, selectedPreset, floatSettings, intSettings, boolSettings);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_SendFullConfigOwner(string configVersion, string selectedPreset,
                                 array<float> combinedPresetParams,
                                 array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        bool applied = ApplyFullConfigFromCombined(configVersion, selectedPreset, combinedPresetParams, floatSettings, intSettings, boolSettings);
        if (!Replication.IsServer() && applied)
            Rpc(RPC_ClientAckConfig);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientAckConfig()
    {
        if (Replication.IsServer())
        {
            m_bClientAckedConfig = true;
            Rpc(RPC_ServerAckReceived);
            if (IsRssDebugEnabled())
                PrintFormat("[RSS] 收到客户端配置回执，停止推送: %1", GetPlayerLabel(GetOwner()));
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerAckReceived()
    {
        if (!Replication.IsServer() && IsRssDebugEnabled())
            Print("[RSS] 服务器已确认收到回执，后续不再推送配置");
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    void RPC_SendFullConfigBroadcast(string configVersion, string selectedPreset,
                                     array<float> combinedPresetParams,
                                     array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        ApplyFullConfigFromCombined(configVersion, selectedPreset, combinedPresetParams, floatSettings, intSettings, boolSettings);
    }

    protected bool ApplyFullConfigFromCombined(string configVersion, string selectedPreset,
        array<float> combinedPresetParams,
        array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        array<float> eliteParams = null;
        array<float> standardParams = null;
        array<float> tacticalParams = null;
        array<float> customParams = null;
        bool isValid = SCR_RSS_ConfigSyncUtils.SplitCombinedPresetParams(combinedPresetParams, eliteParams, standardParams, tacticalParams, customParams);
        if (!isValid)
            return false;
        ApplyFullConfig(configVersion, selectedPreset, eliteParams, standardParams, tacticalParams, customParams, floatSettings, intSettings, boolSettings);
        return true;
    }

    void MonitorNetworkConnection()
    {
        if (Replication.IsServer()) return;

        if (!GetGame()) return;
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager) return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        int playerId = playerManager.GetPlayerIdFromControlledEntity(GetOwner());
        bool hasValidPlayerId = (playerId != 0);

        bool isHeartbeatActive = SCR_RSS_NetworkStateUtils.IsHeartbeatActive(hasValidPlayerId, m_fLastHeartbeatTime, currentTime, 10.0);

        bool isConnected = SCR_RSS_NetworkStateUtils.IsConnected(hasValidPlayerId, m_fLastHeartbeatTime, isHeartbeatActive);

        bool playerIdChanged = SCR_RSS_NetworkStateUtils.HasPlayerIdChanged(m_iPlayerId, playerId);

        if (hasValidPlayerId && m_iPlayerId != playerId)
        {
            if (m_iPlayerId != 0)
            {
                PrintFormat("[RSS] 检测到玩家ID变化: %1 -> %2 (可能是重连)", m_iPlayerId, playerId);
            }
            else
            {
                PrintFormat("[RSS] 检测到玩家ID: %1", playerId);
            }
            m_iPlayerId = playerId;
        }

        if (!m_bIsConnected && isConnected)
        {
            m_fLastHeartbeatTime = currentTime; // 初始化心跳
            m_bIsConnected = true;
            m_fLastReconnectTime = currentTime;
            if (SCR_RSS_ConfigManager.IsServerConfigApplied())
            {
                Print("[RSS] 网络已连接，服务器配置已应用");
            }
            else
            {
                Print("[RSS] 网络已连接，等待服务器推送配置");
                RequestServerConfig();
            }
        }
        else if (m_bIsConnected && playerIdChanged && isConnected)
        {
            SCR_RSS_ConfigManager.ResetClientConfigAwaitingSync();
            m_iConfigRetryCount = 0; // 重置重试计数
            m_fFirstConfigRequestTime = -1.0;
            m_fLastConfigTimeoutWarningTime = -1.0;
            m_fLastReconnectTime = currentTime;
            Print("[RSS] 网络已重连，等待服务器推送配置");
            GetGame().GetCallqueue().CallLater(RequestServerConfig, RECONNECT_SYNC_DELAY * 1000, false);
        }
        else if (m_bIsConnected && !isConnected)
        {
            m_bIsConnected = false;
            m_iPlayerId = 0;
            m_fLastHeartbeatTime = 0.0;
            Print("[RSS] 网络连接已断开");
        }

        if (isConnected)
        {
            m_fLastHeartbeatTime = currentTime;
        }
    }

    void RequestServerConfig(bool forceBroadcast = false)
    {
        if (!Replication.IsServer())
        {
            if (SCR_RSS_ConfigManager.IsServerConfigApplied())
                return;

            if (m_fFirstConfigRequestTime < 0.0)
                m_fFirstConfigRequestTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float elapsed = nowSec - m_fFirstConfigRequestTime;

            if (elapsed >= CONFIG_FETCH_TIMEOUT_SEC)
            {
                m_iConfigRetryCount++;

                if (m_fLastConfigTimeoutWarningTime < 0.0 || (nowSec - m_fLastConfigTimeoutWarningTime) >= CONFIG_FETCH_TIMEOUT_SEC)
                {
                    m_fLastConfigTimeoutWarningTime = nowSec;

                    if (m_iConfigRetryCount >= MAX_CONFIG_RETRY_COUNT)
                    {
                        PrintFormat("[RSS] 配置获取超时（重试 %1/%2），请求服务器广播配置", m_iConfigRetryCount, MAX_CONFIG_RETRY_COUNT);
                        forceBroadcast = true;
                    }
                    else
                    {
                        PrintFormat("[RSS] 配置获取超时（重试 %1/%2），继续重试。若持续无响应请检查服务器或网络。", m_iConfigRetryCount, MAX_CONFIG_RETRY_COUNT);
                    }
                }
            }
            if (forceBroadcast)
            {
                Rpc(RPC_ServerRequestBroadcastConfig);
            }
            else
            {
                Rpc(RPC_ServerRequestConfig);
            }
        }
    }

    void RequestServerConfigUntilApplied()
    {
        if (Replication.IsServer())
            return;

        if (SCR_RSS_ConfigManager.IsServerConfigApplied())
            return;

        RequestServerConfig();

        if (GetGame() && GetGame().GetCallqueue())
        {
            int retryIntervalMs = Math.Round(SERVER_CONFIG_SYNC_INTERVAL * 1000.0);
            if (retryIntervalMs < 1000)
                retryIntervalMs = 1000;
            GetGame().GetCallqueue().CallLater(RequestServerConfigUntilApplied, retryIntervalMs, false);
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ServerRequestConfig()
    {
        if (!Replication.IsServer())
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;

        PrintFormat("[RSS] Sync config to client (owner request): %1", GetPlayerLabel(GetOwner()));

        array<float> combinedPresetParams = new array<float>();
        array<float> floatSettings = new array<float>();
        array<int> intSettings = new array<int>();
        array<bool> boolSettings = new array<bool>();
        BuildCombinedPresetArray(settings, combinedPresetParams);
        BuildSettingsArrays(settings, floatSettings, intSettings, boolSettings);

        Rpc(RPC_SendFullConfigOwner,
            settings.m_sConfigVersion,
            settings.m_sSelectedPreset,
            combinedPresetParams,
            floatSettings,
            intSettings,
            boolSettings
        );
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ServerRequestBroadcastConfig()
    {
        if (!Replication.IsServer())
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;

        PrintFormat("[RSS] Broadcast config to all clients (timeout fallback): %1", GetPlayerLabel(GetOwner()));

        array<float> combinedPresetParams = new array<float>();
        array<float> floatSettings = new array<float>();
        array<int> intSettings = new array<int>();
        array<bool> boolSettings = new array<bool>();
        BuildCombinedPresetArray(settings, combinedPresetParams);
        BuildSettingsArrays(settings, floatSettings, intSettings, boolSettings);

        Rpc(RPC_SendFullConfigBroadcast,
            settings.m_sConfigVersion,
            settings.m_sSelectedPreset,
            combinedPresetParams,
            floatSettings,
            intSettings,
            boolSettings
        );
    }

    void UpdateHeartbeat()
    {
        if (Replication.IsServer()) return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        if (currentTime - m_fLastHeartbeatSendTime >= HEARTBEAT_INTERVAL)
        {
            m_fLastHeartbeatSendTime = currentTime;
            Rpc(RPC_ClientHeartbeat, GetGame().GetWorld().GetWorldTime() / 1000.0);
        }

        if (m_fLastHeartbeatTime > 0.0 && (currentTime - m_fLastHeartbeatTime) > HEARTBEAT_TIMEOUT)
        {
            if (m_bIsConnected)
            {
                m_bIsConnected = false;
                m_iPlayerId = 0;
                Print("[RSS] 心跳超时，判定网络已断开");
            }
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientHeartbeat(int clientTimestamp)
    {
        if (Replication.IsServer())
        {
            m_fLastHeartbeatTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

            Rpc(RPC_ServerHeartbeatAck, GetGame().GetWorld().GetWorldTime() / 1000.0);
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerHeartbeatAck(float serverTimestamp)
    {
        if (!Replication.IsServer())
        {
            float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float rtt = currentTime - serverTimestamp;

            m_fLastHeartbeatTime = currentTime;

            if (IsRssDebugEnabled() && rtt > 0.5)
                PrintFormat("[RSS] High latency detected (RTT: %1s)", rtt);
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientReportStamina(float staminaPercent, float weight, float clientTimestamp, bool isCriticalData)
    {
        if (Replication.IsServer())
        {
            if (!SCR_RSS_ConfigManager.GetSettings() || !SCR_RSS_ConfigManager.GetSettings().m_bDataExportEnabled)
                return;

            float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

            float timestampDelta = 0.0;
            bool isValidClientTimestamp = SCR_PlayerBaseNetworkHelper.IsValidClientReportTimestamp(
                currentTime,
                clientTimestamp,
                timestampDelta);
            if (!isValidClientTimestamp)
            {
                if (timestampDelta > 0.0)
                {
                    if (IsRssDebugEnabled())
                        PrintFormat("[RSS] Stale stamina report ignored (timestamp delta: %1s)", timestampDelta);
                    return;
                }
                else
                {
                    if (IsRssDebugEnabled())
                        PrintFormat("[RSS] Stale stamina report ignored (time regression: %1s)", timestampDelta);
                    return;
                }
            }

            if (m_pNetworkSyncManager && !m_pNetworkSyncManager.AcceptClientReport(currentTime, isCriticalData))
            {
                if (IsRssDebugEnabled())
                    PrintFormat("[RSS] Ignored too-frequent stamina report (time=%1)", currentTime);
                return;
            }

            float clampedStamina = Math.Clamp(staminaPercent, 0.0, 1.0);

            if (m_pNetworkSyncManager)
            {
                float lastReported = m_pNetworkSyncManager.GetLastReportedStaminaPercent();
                if (Math.AbsFloat(clampedStamina - lastReported) > 0.5 && IsRssDebugEnabled())
                    PrintFormat("[RSS] Suspicious stamina jump reported: last=%1 -> reported=%2", lastReported, clampedStamina);

                m_pNetworkSyncManager.UpdateReportedState(clampedStamina, weight);
            }

            float serverWeight = SCR_PlayerBaseNetworkHelper.GetServerWeight(GetOwner(), m_pEncumbranceCache);

            float encPenalty = 0.0;
            if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            {
                encPenalty = m_pEncumbranceCache.GetSpeedPenalty();
            }
            else
            {
                encPenalty = SCR_PlayerBaseNetworkHelper.CalculateEncumbrancePenaltyFallback(serverWeight);
            }

            bool isSprinting = IsSprinting();
            int currentMovementPhase = GetCurrentMovementPhase();
            bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(clampedStamina);
            bool canSprint = RealisticStaminaSpeedSystem.CanSprint(clampedStamina);

            vector velocity = GetVelocity();
            float currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(velocity);

            IEntity ownerEnt = GetOwner();
            bool shouldSuppressSlopeServer = false;
            if (m_pEnvironmentFactor && ownerEnt)
                shouldSuppressSlopeServer = m_pEnvironmentFactor.ShouldSuppressTerrainSlopeForEntity(ownerEnt);

            float slopeAngleDegrees = 0.0;
            if (!shouldSuppressSlopeServer)
                slopeAngleDegrees = SpeedCalculator.GetSlopeAngle(this, m_pEnvironmentFactor, velocity);

            float rawSlopeServer = SpeedCalculator.GetRawSlopeAngle(this, velocity);
            bool isIndoorStairsServer = (shouldSuppressSlopeServer && Math.AbsFloat(rawSlopeServer) > 0.0);
            if (isIndoorStairsServer)
                encPenalty = encPenalty * StaminaConstants.GetIndoorStairsEncumbranceSpeedFactor();

            float validated = StaminaUpdateCoordinator.CalculateFinalSpeedMultiplierFromInputs(
                clampedStamina,
                encPenalty,
                isSprinting,
                currentMovementPhase,
                isExhausted,
                canSprint,
                currentSpeed,
                slopeAngleDegrees,
                GetSprintStartTime());

            validated = Math.Clamp(validated, 0.15, 1.0);

            if (m_pNetworkSyncManager)
            {
                currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

                if (!m_pNetworkSyncManager.HasServerValidation())
                {
                    m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(validated);
                    Rpc(RPC_ServerSyncSpeedMultiplier, validated, currentTime);
                }
                else
                {
                    float speedDiff = Math.AbsFloat(validated - m_pNetworkSyncManager.GetServerValidatedSpeedMultiplier());
                    if (m_pNetworkSyncManager.ProcessDeviation(speedDiff, currentTime))
                    {
                        m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(validated);
                        Rpc(RPC_ServerSyncSpeedMultiplier, validated, currentTime);
                    }
                    else
                    {
                    }
                }
            }
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerSyncSpeedMultiplier(float speedMultiplier, float serverTimestamp)
    {
        if (!Replication.IsServer())
        {
            if (m_pNetworkSyncManager)
            {
                float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
                float timestampDelta = 0.0;
                bool isValidServerTimestamp = SCR_PlayerBaseNetworkHelper.IsValidServerSyncTimestamp(
                    currentTime,
                    serverTimestamp,
                    timestampDelta);
                if (!isValidServerTimestamp)
                {
                    if (IsRssDebugEnabled())
                        PrintFormat("[RSS] Stale speed validation ignored (latency: %1s)", timestampDelta);
                    return;
                }

                m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(speedMultiplier);
            }
        }
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

        if (m_pTerrainDetector)
            m_pTerrainDetector.SetIsAiEntity(!controlled);

        if (controlled)
            EnsureRssStaminaLoopIfNeeded();
        
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
            
            GetGame().GetCallqueue().CallLater(InitStaminaHUD, 1000, false);
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
            
            SCR_StaminaHUDComponent.Destroy();
        }
    }

    void InitStaminaHUD()
    {
        SCR_StaminaHUDComponent.Init();
    }

    void OnJumpActionTriggered(float value = 0.0, EActionTrigger trigger = 0)
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
            return true;
        return false;
    }

    protected int GetSpeedUpdateIntervalMs()
    {
        return SCR_RSS_AIStaminaBridge.GetSpeedUpdateIntervalMs(this);
    }

    void StartSystem()
    {
        if (!ShouldProcessStaminaUpdate())
            return;
        m_bRssStaminaLoopActive = true;
        int intervalMs = SCR_RSS_AIStaminaBridge.GetSpeedUpdateIntervalMs(this);
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, intervalMs, false);
        
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }

    void EnsureRssStaminaLoopIfNeeded()
    {
        if (!ShouldProcessStaminaUpdate())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        StartSystem();
    }

    void EnsureAiStaminaLoopOnServer()
    {
        if (!Replication.IsServer() || IsPlayerControlled())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        m_iAiLoopRetryCount = m_iAiLoopRetryCount + 1;
        if (m_iAiLoopRetryCount > AI_LOOP_MAX_RETRIES)
            return;
        StartSystem();
        if (!m_bRssStaminaLoopActive)
            GetGame().GetCallqueue().CallLater(EnsureAiStaminaLoopOnServer, 3000, false);
    }

    protected bool RSS_HandleVehicleStaminaUpdate(IEntity owner)
    {
        bool isInVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(m_pCompartmentAccess);

        if (!isInVehicle)
            return false;

        static int vehicleDebugCounter = 0;
        vehicleDebugCounter++;
        if (vehicleDebugCounter >= 25)
        {
            vehicleDebugCounter = 0;
            if (m_pStaminaComponent && IsRssDebugEnabled())
            {
                float currentStamina = m_pStaminaComponent.GetTargetStamina();
                PrintFormat("[RSS] 载具中 / In Vehicle: 体力=%1%% | Stamina=%1%%",
                    Math.Round(currentStamina * 100.0).ToString());
            }
        }

        float vehicleRestMinutes = 0.0;
        float vehicleExerciseMinutes = 0.0;
        if (m_pExerciseTracker)
        {
            float vehicleCurrentTimeMs = GetGame().GetWorld().GetWorldTime();
            m_pExerciseTracker.Update(vehicleCurrentTimeMs, false, true);
            vehicleRestMinutes = m_pExerciseTracker.GetRestDurationMinutes();
            vehicleExerciseMinutes = m_pExerciseTracker.GetExerciseDurationMinutes();
        }

        float vehicleStaminaPercent = 1.0;
        if (m_pStaminaComponent)
            vehicleStaminaPercent = Math.Clamp(m_pStaminaComponent.GetTargetStamina(), 0.0, 1.0);

        float vehicleNetRatePerSec = 0.0;
        if (vehicleStaminaPercent < 1.0)
        {
            vehicleNetRatePerSec = StaminaUpdateCoordinator.GetNetStaminaRatePerSecond(
                vehicleStaminaPercent, false, 0.0, -StaminaConstants.REST_RECOVERY_PER_TICK, 0.0, 0.0, 1.0,
                m_pEpocState, m_pEncumbranceCache, m_pExerciseTracker, this, null, true);
            float vehicleRecoveryRate = vehicleNetRatePerSec / 5.0;

            float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            if (m_pFatigueSystem)
                m_pFatigueSystem.ProcessFatigueDecay(currentWorldTime, 0.0);
            float maxStaminaCap = 1.0;
            if (m_pFatigueSystem)
                maxStaminaCap = m_pFatigueSystem.GetMaxStaminaCap();
            float timeDeltaSec;
            if (m_fLastStaminaUpdateTime >= 0.0)
                timeDeltaSec = currentWorldTime - m_fLastStaminaUpdateTime;
            else
                timeDeltaSec = GetSpeedUpdateIntervalMs() / 1000.0;
            timeDeltaSec = Math.Clamp(timeDeltaSec, 0.01, 0.5);
            float tickScale = Math.Clamp(timeDeltaSec / 0.2, 0.01, 2.0);
            float oldStamina = vehicleStaminaPercent;
            float newStamina = Math.Clamp(oldStamina + vehicleRecoveryRate * tickScale, 0.0, maxStaminaCap);
            if (vehicleStaminaPercent > maxStaminaCap)
                newStamina = maxStaminaCap;
            m_pStaminaComponent.SetTargetStamina(newStamina);
            m_fLastStaminaUpdateTime = currentWorldTime;
            vehicleStaminaPercent = newStamina;

            if (vehicleDebugCounter == 0 && IsRssDebugEnabled())
            {
                PrintFormat("[RSS] 载具中恢复 / Vehicle Recovery: %1%% -> %2%% (净率: %3/s)",
                    Math.Round(oldStamina * 100.0).ToString(),
                    Math.Round(newStamina * 100.0).ToString(),
                    vehicleNetRatePerSec.ToString());
            }
        }

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_bHintDisplayEnabled)
        {
            float vehicleTimeToDepleteSec = -1.0;
            float vehicleTimeToFullSec = -1.0;
            float targetStamina = 1.0;
            if (m_pFatigueSystem)
                targetStamina = m_pFatigueSystem.GetMaxStaminaCap();
            if (vehicleStaminaPercent < targetStamina && vehicleNetRatePerSec > 0.00001)
            {
                vehicleTimeToFullSec = (targetStamina - vehicleStaminaPercent) / vehicleNetRatePerSec;
                if (vehicleTimeToFullSec < 0.5)
                    vehicleTimeToFullSec = 0.5;
                if (vehicleTimeToFullSec > 7200.0)
                    vehicleTimeToFullSec = 7200.0;
            }

            float vehicleDebugWeight = 0.0;
            if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
                vehicleDebugWeight = m_pEncumbranceCache.GetCurrentWeight();

            DebugInfoParams vehicleParams = SCR_PlayerBaseNetworkHelper.BuildVehicleDebugInfoParams(
                owner,
                vehicleStaminaPercent,
                vehicleDebugWeight,
                m_fCurrentWetWeight,
                vehicleTimeToDepleteSec,
                vehicleTimeToFullSec,
                m_pTerrainDetector,
                m_pEnvironmentFactor,
                m_pStanceTransitionManager);
            DebugDisplay.OutputHintInfo(vehicleParams);
        }

        RSS_SetMudSlipCameraShake01(0.0);
        m_bRssStaminaLoopActive = true;
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
        return true;
    }

    protected void RSS_UpdateTacticalSprintState()
    {
        float currentTimeSprint = GetGame().GetWorld().GetWorldTime() / 1000.0;
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        bool isSprintActive = isSprintingNow || (phaseNow == 3);
        bool nextLastWasSprinting = m_bLastWasSprinting;
        float nextSprintCooldownUntil = m_fSprintCooldownUntil;
        float nextSprintStartTime = m_fSprintStartTime;
        SCR_PlayerBaseMovementHelper.UpdateTacticalSprintState(
            currentTimeSprint,
            isSprintActive,
            m_bLastWasSprinting,
            m_fSprintCooldownUntil,
            m_fSprintStartTime,
            nextLastWasSprinting,
            nextSprintCooldownUntil,
            nextSprintStartTime);
        m_bLastWasSprinting = nextLastWasSprinting;
        m_fSprintCooldownUntil = nextSprintCooldownUntil;
        m_fSprintStartTime = nextSprintStartTime;
    }

}
