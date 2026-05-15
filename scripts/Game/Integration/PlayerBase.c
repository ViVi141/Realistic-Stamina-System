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
    protected const float CONFIG_FETCH_TIMEOUT_SEC = 30.0; // 配置获取超时（秒），超时后每 30 秒打印一次警告
    protected float m_fLastConfigTimeoutWarningTime = -1.0; // 上次打印超时警告时间，避免刷屏

    // Native-style config replication:
    // Clients receive settings via GameMode RplProp (see SCR_RSS_ServerBootstrap.c),
    // so we do not use per-player request/ACK/broadcast fallback anymore.
    protected bool m_bRssWaitingGameModeConfig = false;
    protected float m_fRssConfigWaitStartTime = -1.0;

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
    protected ChimeraCharacter m_pCachedOwnerCharacter; // 缓存角色实体引用（避免每帧 Cast）

    protected float m_fAnimSpeedCompensation = 1.0;
    protected float m_fLastAnimCompensationUpdateTime = -1.0;
    protected static const float ANIM_COMPENSATION_UPDATE_INTERVAL = 2.0;
    
    protected float m_fSprintStartTime = -1.0;       // 本次冲刺开始时间（世界时间秒），-1 表示未在爆发/已进入平稳期
    protected bool m_bLastWasSprinting = false;       // 上一帧是否处于 Sprint 状态，用于检测进入/离开冲刺
    protected float m_fSprintCooldownUntil = -1.0;    // 冷却结束时间（世界时间秒），此时间前再次冲刺不触发爆发，直接平稳期

    protected int m_iCombatStimPhase = ERSS_CombatStimPhase.NONE;
    protected float m_fCombatStimPhaseEndsAt = -1.0;
    protected int m_iCombatStimDelayInjectionCount = 0;
    //! 注射 Buff 前记录的 GetBleedingScale()，用于阶段结束时还原；-1 表示未处于 Buff
    protected float m_fRSS_CombatStimBleedingBaseline = -1.0;
    
    protected ref RSS_MudSlipRunner m_pMudSlipRunner;
    protected float m_fRssMudSlipCameraShake01 = 0.0;
    protected float m_fLastRssSpeedMultiplierApplied = 1.0;
    protected bool m_bRssStaminaLoopActive = false;
    protected bool m_bIsDeleted = false; // 实体删除标记：阻止 CallLater 回调在删除后继续触发
    
    // ====== perf: AI 桥方法节流（高密度 AI 减负）======
    protected float m_fLastAiBridgeTickTime = -1.0;
    protected static const float AI_BRIDGE_THROTTLE_SEC = 0.5; // 500ms，引擎 AI 行为树更新间隔为 0.55s

    // ====== RSS AI 体力状态机成员变量 ======
    protected ERSS_AIStaminaState m_eRssAIStaminaState = ERSS_AIStaminaState.FRESH;
    protected float m_fRssAiTimeStationarySec = 0.0;
    protected float m_fRssLastAiDebugPrintTime = -1.0;

    //! 析构函数：在实体删除时同步调用，标记组件已删除，防止 CallLater/ActionListener
    //! 等异步回调在已释放的内存上执行读写操作（Access Violation）。
    //! 参考 SCR_StaminaOverride.c 中 StopStaminaMonitor() 的 Callqueue.Remove() 模式。
    void ~SCR_CharacterControllerComponent()
    {
        m_bIsDeleted = true;

        // CRITICAL FIX: Guard against GetGame() being null or returning a
        // partially-destroyed Game during world teardown. When the engine
        // destroys the game world, GetGame() may return a stale pointer
        // whose GetCallqueue()/GetWorkspace() resolve to 0x0.
        if (!GetGame())
        {
            // Game is already gone — skip all cleanup that requires engine services.
            // Fall through to null all ref fields to prevent future misuse.
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

        // CRITICAL FIX: Destroy HUD before the workspace is destroyed.
        // Without this, the HUD singleton (s_Instance) persists with stale widget
        // references (m_wRoot) after the entity is deleted.
        // When OnGameStart later calls SCR_StaminaHUDComponent.Destroy(),
        // DestroyHUD tries m_wRoot.RemoveFromHierarchy() on a freed C++ widget
        // object, causing Access Violation at 0x0. Also fixes "Resources are leaking"
        // assertion because the widget was never removed from the widget tree.
        SCR_StaminaHUDComponent.Destroy();

        m_bRssStaminaLoopActive = false;

        // 取消所有待执行的 CallLater 回调（引擎层面移除，彻底杜绝 use-after-free）
        if (GetGame() && GetGame().GetCallqueue())
        {
            GetGame().GetCallqueue().Remove(UpdateSpeedBasedOnStamina);
            GetGame().GetCallqueue().Remove(StartSystem);
            GetGame().GetCallqueue().Remove(EnsureRssStaminaLoopIfNeeded);
            GetGame().GetCallqueue().Remove(EnsureAiStaminaLoopOnServer);
            GetGame().GetCallqueue().Remove(RSS_WaitForGameModeConfig);
            GetGame().GetCallqueue().Remove(CollectSpeedSample);
            GetGame().GetCallqueue().Remove(InitStaminaHUD);
            GetGame().GetCallqueue().Remove(RSS_MudSlip_FinishRagdoll);
        }

        // 移除所有 InputManager 监听器（防止实体删除后跳跃事件回调到已释放的 this）
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

        // 清理 AI 恢复注册表（防止其他 AI tick 遍历已删除实体）
        IEntity owner = GetOwner();
        if (owner && !IsPlayerControlled())
            // [REMOVED v3.23.0] SCR_RSS_AIRestRecoveryRegistry.CleanupEntity(owner);
            // 新架构中不再使用 RecoveryRegistry，cleanup 由 GroupSync.ClearAll 替代

        // 清理 UISignalBridge 的实体引用
        if (m_pUISignalBridge)
            m_pUISignalBridge.Cleanup();

        // 清零所有缓存引用
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
        
        // CRITICAL FIX: Workbench script reload detection.
        // After script+world reload, OnInit runs on the SAME C++ component object
        // from the previous session. The old sub-modules (ref fields) were created
        // with the old script layout. Re-initializing through the new code can
        // crash (Access violation at offset ~0x28) because the old object's field
        // offsets don't match the new script layout.
        // Skip re-init if this is a script reload scenario.
        if (m_pStaminaComponent || m_pCollapseTransition || m_pEnvironmentFactor)
        {
            // We're being re-initialized on an existing component.
            // The sub-modules were already created in the previous session.
            // If they're still valid (script layout unchanged), calling OnInit
            // again would only start duplicate CallLater timers.
            // If layout changed, accessing old fields crashes.
            // Either way, skipping is the safest path.
            m_bIsDeleted = false;

            // CRITICAL FIX: Clear stale engine references in EnvironmentFactor
            // from the previous world session. The old m_pCachedWeatherManager
            // still points to the destroyed TimeAndWeatherManagerEntity, causing
            // Access violation when UpdateEnvironmentFactors calls ReadSignal*
            // fallback branches. See ClearStaleReferences() doc for details.
            if (m_pEnvironmentFactor)
                m_pEnvironmentFactor.ClearStaleReferences();

            return;
        }

        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();
            // Config distribution is now handled by GameMode replication.

            if (IsRssDebugEnabled())
            {
                float coeff = StaminaConfigBridge.GetEnergyToStaminaCoeff();
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
            m_pCachedOwnerCharacter = character;
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
            // Wait for GameMode replicated config to arrive; do not actively request via RPC.
            m_bRssWaitingGameModeConfig = true;
            if (GetGame() && GetGame().GetWorld())
                m_fRssConfigWaitStartTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            // CRITICAL FIX: Use non-repeating CallLater (false) + self-reschedule at tail.
            // Repeating (true) causes duplicate callbacks after Workbench script reload,
            // because C++ object persists and OnInit registers another instance.
            GetGame().GetCallqueue().CallLater(RSS_WaitForGameModeConfig, 1000, false);
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
            if (GetGame().GetCallqueue())
                GetGame().GetCallqueue().Remove(RSS_WaitForGameModeConfig);
            return;
        }

        float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        if (m_fRssConfigWaitStartTime >= 0.0 && (nowSec - m_fRssConfigWaitStartTime) >= CONFIG_FETCH_TIMEOUT_SEC)
        {
            // Single warning per timeout window to avoid spam.
            if (m_fLastConfigTimeoutWarningTime < 0.0 || (nowSec - m_fLastConfigTimeoutWarningTime) >= CONFIG_FETCH_TIMEOUT_SEC)
            {
                m_fLastConfigTimeoutWarningTime = nowSec;
                Print("[RSS] 等待 GameMode 同步配置超时。请确认服务器端 RSS 已加载且复制正常。");
            }
        }
        
        // Self-reschedule: continue waiting. Non-repeating (false) avoids duplicate
        // callbacks after Workbench script reload.
        if (m_bRssWaitingGameModeConfig)
            GetGame().GetCallqueue().CallLater(RSS_WaitForGameModeConfig, 1000, false);
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
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
            return;
        }
        
        // perf: AI 快速路径 — 跳过 AI 不需要的玩家专属逻辑
        bool isPlayer = IsPlayerControlled();

        float rssDistToPlayerM = -1.0;
        SCR_AIGroup rssProxyGroup = null;
        bool rssSkipStaminaDrainRecovery = false;

        if (!isPlayer && Replication.IsServer())
        {
            rssDistToPlayerM = GetNearestPlayerDistanceM(owner);
            rssProxyGroup = SCR_RSS_AIGroupStaminaProxy.ResolveGroup(owner);
            if (rssProxyGroup && SCR_RSS_AIGroupStaminaProxy.IsProxyModeActive(owner, rssDistToPlayerM, rssProxyGroup))
            {
                if (SCR_RSS_AIGroupStaminaProxy.IsFollower(owner, rssProxyGroup))
                {
                    SCR_RSS_AIGroupStaminaProxy.ApplyFollowerSync(owner, rssProxyGroup);
                    m_bRssStaminaLoopActive = true;
                    GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
                    return;
                }
                if (!SCR_RSS_AIGroupStaminaProxy.ShouldLeaderRunFullCalc(rssProxyGroup))
                    rssSkipStaminaDrainRecovery = true;
            }
        }
        
        if (isPlayer)
        {
            RSS_CombatStim_OnTickTransitions();

            // 药效/OD 期间：流血倍率刷新
            if (Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
            {
                if (m_pCachedOwnerCharacter)
                {
                    SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                    if (stimDmgMgr)
                        RSS_CombatStim_RefreshBleedingEffectsToMatchScale(stimDmgMgr);
                }
            }
        }

        // 药效/OD 期间：非玩家实体（如 AI 被注射）也需要刷新流血倍率
        if (!isPlayer && Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
        {
            if (m_pCachedOwnerCharacter)
            {
                SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                if (stimDmgMgr)
                    RSS_CombatStim_RefreshBleedingEffectsToMatchScale(stimDmgMgr);
            }
        }

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
        
        if (isPlayer) RSS_UpdateTacticalSprintState();
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        bool isSprintActive = isSprintingNow || (phaseNow == 3);
        
        float currentTimeForExerciseMs = world.GetWorldTime();
        float currentTime = currentTimeForExerciseMs / 1000.0;
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

        // AI 仅速度模式：应用速度倍率后跳过体力消耗/恢复链
        if (!isPlayer && StaminaConfigBridge.IsAiStaminaCalcDisabled())
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
            return;
        }

        bool isCriticalData = (staminaPercent <= 0.05 || (m_pNetworkSyncManager && m_pNetworkSyncManager.GetLastReportedStaminaPercent() > 0.5 && staminaPercent <= 0.1));
        // CRITICAL FIX: Double-check GetServerDataExportEnabled() after ShouldSync.
        // If admin disabled data export between ShouldSync and Rpc(), we skip sending.
        if (isPlayer && !Replication.IsServer() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled())
        {
            int syncType = 0;
            if (isCriticalData)
                syncType = 1;
            if (m_pNetworkSyncManager.ShouldSync(currentTime, syncType))
            {
                // Re-check before sending RPC: admin may have disabled data export
                // in the split second between the outer check and here.
                if (!SCR_RSS_ConfigManager.GetServerDataExportEnabled())
                    return;
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
        
        if (isPlayer)
        {
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
        
        float totalWetWeight = SwimmingStateManager.CalculateTotalWetWeight(m_fCurrentWetWeight, rainWeight);
        float currentWeightWithWet = currentWeight + totalWetWeight;

        float totalWeight = currentWeight + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+身体
        float totalWeightWithWetAndBody = currentWeightWithWet + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+湿重+身体

        bool useSwimmingModel = isSwimming;

        if (isPlayer && m_pStaminaComponent && m_pJumpVaultDetector)
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
                PrintFormat("[RSS] Jump Cost: -%1%%", 
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
                PrintFormat("[RSS] Vault Cost: -%1%%", 
                    Math.Round(vaultCost * 100.0 * 10.0) / 10.0);
            }
            staminaPercent = staminaPercent - vaultCost;
            
            m_pJumpVaultDetector.UpdateCooldowns();
            
            m_pStanceTransitionManager.UpdateFatigue(timeDeltaSec);
            
            float currentTimeSec = world.GetWorldTime() / 1000.0;
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
        float totalDrainRate = 0.0;

        if (m_pFatigueSystem)
        {
            float currentTimeForFatigue = world.GetWorldTime() / 1000.0; // 转换为秒
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

        if (StaminaConfigBridge.IsMudSlipMechanismEnabled())
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
        // =====================================================================
        // RSS AI 体力集成 — 新模块链 (v3.23.0)
        // =====================================================================
        // perf: AI 模块节流
        if (!IsPlayerControlled() && (currentTime - m_fLastAiBridgeTickTime < AI_BRIDGE_THROTTLE_SEC))
        {
            // 节流中
        }
        else
        {
            float prevBridgeTime = m_fLastAiBridgeTickTime;
            if (prevBridgeTime < 0.0)
                prevBridgeTime = currentTime;
            float bridgeDeltaSec = currentTime - prevBridgeTime;

            if (!IsPlayerControlled())
                m_fLastAiBridgeTickTime = currentTime;

            if (Replication.IsServer() && StaminaConfigBridge.IsAIStaminaIntegrationEnabled())
            {
                // 1. 更新体力状态机
                bool isStationary = (currentSpeed < 0.05);
                if (isStationary)
                    m_fRssAiTimeStationarySec = m_fRssAiTimeStationarySec + bridgeDeltaSec;
                else
                    m_fRssAiTimeStationarySec = 0.0;

                float fatigueVal;
                if (m_pFatigueSystem)
                    fatigueVal = m_pFatigueSystem.GetFatigueAccumulation();
                else
                    fatigueVal = 0.0;

                ERSS_AIStaminaState prevAiState = m_eRssAIStaminaState;
                ERSS_AIStaminaState aiState = SCR_RSS_AIStaminaState.Tick(
                    staminaPercent,
                    fatigueVal,
                    isStationary,
                    m_fRssAiTimeStationarySec,
                    m_eRssAIStaminaState);

                // 2. 五级移动限速
                EAIThreatState threat = EAIThreatState.SAFE;
                if (!IsPlayerControlled())
                {
                    threat = SCR_RSS_AIGroupSync.GetThreatState(owner);
                }
                bool isThreatened = (threat == EAIThreatState.THREATENED);
                SCR_RSS_AISpeedCap.Apply(this, owner, aiState, staminaPercent, isThreatened);

                // 3. 行为过滤
                SCR_RSS_AIIntentFilter.Apply(owner, aiState, prevAiState, isThreatened);

                // 4. 战斗衰减
                SCR_RSS_AICombatDecay.Apply(owner, aiState);

                // 5. 检查休息路点完成条件（体力恢复达标则提前完成）
                if (!IsPlayerControlled() && SCR_RSS_AIGroupSync.IsGroupRestWaypointActive(owner))
                {
                    SCR_RSS_AIGroupSync.TryCompleteRestWaypointIfStaminaRecovered(owner, staminaPercent);
                }

            }
        }

        bool isSprinting = isSprintingNow;
        int currentMovementPhase = phaseNow;
        float baseDrainRateByVelocity = 0.0;
        float baseDrainRateByVelocityForModule = 0.0;

        if (!rssSkipStaminaDrainRecovery)
        {
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
        baseDrainRateByVelocity = drainRateResult.baseDrainRate;
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
        
        const float sprintMultiplier = 1.0;
        
        baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
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

        if (isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
            totalDrainRate = totalDrainRate * SCR_CombatStimConstants.STAMINA_DRAIN_MULTIPLIER;
        
        // 低体力恢复区域：体力<阈值时步行/慢跑转为恢复
        float walkRecoveryThreshold = StaminaConstants.GetWalkRecoveryZoneThreshold();
        float walkRecoveryRatePerTick = StaminaConstants.GetWalkRecoveryZoneRate();
        if (!useSwimmingModel && !isSprintActive && staminaPercent < walkRecoveryThreshold && currentSpeed > 0.1)
        {
            totalDrainRate = -walkRecoveryRatePerTick;  // 负数 = 恢复
            baseDrainRateByVelocity = -walkRecoveryRatePerTick;
            baseDrainRateByVelocityForModule = -walkRecoveryRatePerTick;
        }
        
        if (baseDrainRateByVelocityForModule == 0.0 && baseDrainRateByVelocity > 0.0)
            baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
        if (m_pEpocState && !useSwimmingModel)
        {
            float currentWorldTime = world.GetWorldTime() / 1000.0; // 转换为秒
            bool isInEpocDelay = StaminaRecoveryCalculator.UpdateEpocDelay(
                m_pEpocState,
                currentSpeed,
                currentWorldTime);
        }
        
        if (owner == SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled() && IsPlayerControlled())
            SCR_DebugBatchManager.StartDebugBatch();
        
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
                if (SCR_DebugBatchManager.IsDebugBatchActive())
                {
                    string intLine = string.Format("[RSS] 原生干扰: 目标=%1%% 实际=%2%% 偏差=%3%%",
                        Math.Round(newTargetStamina * 100.0).ToString(),
                        Math.Round(verifyStamina * 100.0).ToString(),
                        Math.Round(Math.AbsFloat(verifyStamina - newTargetStamina) * 10000.0) / 100.0);
                    SCR_DebugBatchManager.AddDebugBatchLine(intLine);
                }
                m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            }
            
            staminaPercent = newTargetStamina;
        }

        if (rssProxyGroup && Replication.IsServer() && !isPlayer)
        {
            if (SCR_RSS_AIGroupStaminaProxy.IsProxyModeActive(owner, rssDistToPlayerM, rssProxyGroup))
                SCR_RSS_AIGroupStaminaProxy.NotifyLeaderFullTickCompleted(rssProxyGroup);
        }
        }
        
        if (isPlayer && m_pUISignalBridge)
        {
            m_pUISignalBridge.UpdateUISignal(staminaPercent, isExhausted, currentSpeed, totalDrainRate, false);
        }
        
        m_fLastStaminaPercent = staminaPercent;
        m_fLastSpeedMultiplier = finalSpeedMultiplier;

        // AI 调试日志（使用 Print 直接输出，不受 DebugBatchManager batch 窗口限制）
        if (owner != SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled())
        {
            // 每 AI 每 30s 输出一行，避免刷屏
            float nowMs = GetGame().GetWorld().GetWorldTime();
            if (m_fRssLastAiDebugPrintTime < 0.0 || (nowMs - m_fRssLastAiDebugPrintTime) >= 30000.0)
            {
                m_fRssLastAiDebugPrintTime = nowMs;
                string movementStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
                string stateStr = SCR_RSS_AIStaminaState.StateToString(m_eRssAIStaminaState);
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

                // 群组分散
                if (Replication.IsServer())
                {
                    AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
                    if (aiCtrl)
                    {
                        AIAgent agent = aiCtrl.GetAIAgent();
                        if (agent)
                        {
                            AIGroup parentGroup = agent.GetParentGroup();
                            if (parentGroup)
                            {
                                SCR_AIGroup scrGrp = SCR_AIGroup.Cast(parentGroup);
                                if (scrGrp)
                                {
                                    float spread = CalcAiGroupSpreadM(scrGrp);
                                    if (spread > 0.0)
                                    {
                                        PrintFormat("[RSS] Group: id=%1 分散=%2m 成员=%3",
                                            scrGrp.GetGroupID().ToString(),
                                            Math.Round(spread * 10.0) / 10.0,
                                            GetAliveMemberCount(scrGrp).ToString());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
            bool needDebugOutput = SCR_DebugBatchManager.IsDebugBatchActive();
            bool needHintOutput = (settings && settings.m_bHintDisplayEnabled);
            if (needDebugOutput || needHintOutput)
            {
                float debugCurrentWeight = 0.0;
                float combatEncumbrancePercent = 0.0;
                if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid()) {
                    debugCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
                    combatEncumbrancePercent = RealisticStaminaSpeedSystem.CalculateCombatEncumbrancePercent(owner);
                }
                float timeToDepleteSec = -1.0;
                float timeToFullSec = -1.0;
                if (needHintOutput) {
                    float netRate = StaminaUpdateCoordinator.GetNetStaminaRatePerSecond(
                        staminaPercent, useSwimmingModel, currentSpeed, totalDrainRate,
                        baseDrainRateByVelocity, baseDrainRateByVelocityForModule,
                        heatStressMultiplier, m_pEpocState, m_pEncumbranceCache,
                        m_pExerciseTracker, this, m_pEnvironmentFactor);
                    float targetStamina = 1.0;
                    if (m_pFatigueSystem) targetStamina = m_pFatigueSystem.GetMaxStaminaCap();
                    if (netRate < -0.0001) {
                        timeToDepleteSec = Math.Min(staminaPercent / Math.AbsFloat(netRate), 7200.0);
                    } else if (netRate > 0.0001 && staminaPercent < targetStamina) {
                        timeToFullSec = Math.Min((targetStamina - staminaPercent) / netRate, 7200.0);
                    }
                }
                DebugInfoParams debugParams = new DebugInfoParams();
                debugParams.owner = owner;
                debugParams.movementTypeStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
                debugParams.staminaPercent = staminaPercent;
                debugParams.baseSpeedMultiplier = baseSpeedMultiplier;
                debugParams.encumbranceSpeedPenalty = encumbranceSpeedPenalty;
                debugParams.finalSpeedMultiplier = finalSpeedMultiplier;
                debugParams.gradePercent = gradePercent;
                if (isSwimming)
                    debugParams.slopeAngleDegrees = 0.0;
                else
                    debugParams.slopeAngleDegrees = slopeAngleDegrees;
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
                if (needDebugOutput) {
                    DebugDisplay.OutputDebugInfo(debugParams);
                    // [v3.23.0] 调试行已直接写入 BatchManager，无需 Flush
                    // SCR_RSS_AIStaminaBridge.FlushAIDebugLinesToBatch();
                }
                if (needHintOutput) DebugDisplay.OutputHintInfo(debugParams);
            }
        }
        
        SCR_DebugBatchManager.FlushDebugBatch();
        
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

    // 与官方一致的流血速率公式
    protected float RSS_CombatStim_ComputeBleedingBaseRateForEffect(SCR_BleedingDamageEffect bleed)
    {
        if (!bleed)
            return 0.0;

        SCR_CharacterHitZone hz = SCR_CharacterHitZone.Cast(bleed.GetAffectedHitZone());
        if (!hz)
            return 0.0;

        float hitZoneDamageMultiplier = hz.GetHealthScaled();
        return hz.GetMaxBleedingRate() - hz.GetMaxBleedingRate() * hitZoneDamageMultiplier;
    }

    // 刷新流血 DOT DPS 以匹配当前倍率
    protected void RSS_CombatStim_RefreshBleedingEffectsToMatchScale(SCR_CharacterDamageManagerComponent dmgMgr)
    {
        if (!dmgMgr)
            return;

        float scale = dmgMgr.GetBleedingScale();
        array<ref SCR_PersistentDamageEffect> effects = dmgMgr.GetAllPersistentEffectsOfType(SCR_BleedingDamageEffect);

        foreach (SCR_PersistentDamageEffect pe : effects)
        {
            SCR_BleedingDamageEffect bleed = SCR_BleedingDamageEffect.Cast(pe);
            if (!bleed)
                continue;

            float baseRate = RSS_CombatStim_ComputeBleedingBaseRateForEffect(bleed);
            bleed.SetDPS(baseRate * scale);
        }
    }

    // 药效/OD 期间临时提高流血倍率
    protected void RSS_CombatStim_UpdateBleedingScale()
    {
        if (!Replication.IsServer())
            return;

        IEntity ownerEnt = GetOwner();
        ChimeraCharacter ch = ChimeraCharacter.Cast(ownerEnt);
        if (!ch)
            return;

        SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(ch.GetDamageManager());
        if (!dmgMgr)
            return;

        int phase = m_iCombatStimPhase;
        bool wantBuff = false;
        float mult = 1.0;

        if (phase == ERSS_CombatStimPhase.ACTIVE)
        {
            wantBuff = true;
            mult = SCR_CombatStimConstants.BLEEDING_SCALE_MULT_ACTIVE;
        }
        else if (phase == ERSS_CombatStimPhase.OD)
        {
            wantBuff = true;
            mult = SCR_CombatStimConstants.BLEEDING_SCALE_MULT_ACTIVE * SCR_CombatStimConstants.BLEEDING_SCALE_MULT_OD_EXTRA;
        }

        if (!wantBuff)
        {
            if (m_fRSS_CombatStimBleedingBaseline >= 0.0)
            {
                dmgMgr.SetBleedingScale(m_fRSS_CombatStimBleedingBaseline, true);
                m_fRSS_CombatStimBleedingBaseline = -1.0;
                RSS_CombatStim_RefreshBleedingEffectsToMatchScale(dmgMgr);
            }
            return;
        }

        if (m_fRSS_CombatStimBleedingBaseline < 0.0)
            m_fRSS_CombatStimBleedingBaseline = dmgMgr.GetBleedingScale();

        float targetScale = m_fRSS_CombatStimBleedingBaseline * mult;
        dmgMgr.SetBleedingScale(targetScale, true);
        RSS_CombatStim_RefreshBleedingEffectsToMatchScale(dmgMgr);
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

        RSS_CombatStim_UpdateBleedingScale();
    }

    // Identity passthrough for future stim read adjustments.
    // CSB active state applies drain multiplier
    // in consumption path (see RSS_IsCaffeineSodiumBenzoateActive).
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
            // CRITICAL FIX: Reset bleeding scale to baseline BEFORE Kill(),
            // because RSS_CombatStim_UpdateBleedingScale() will not be called
            // after the Kill path returns.
            if (m_fRSS_CombatStimBleedingBaseline >= 0.0)
            {
                SCR_CharacterDamageManagerComponent dmgMgrOd = SCR_CharacterDamageManagerComponent.Cast(ch.GetDamageManager());
                if (dmgMgrOd)
                {
                    dmgMgrOd.SetBleedingScale(m_fRSS_CombatStimBleedingBaseline, true);
                    RSS_CombatStim_RefreshBleedingEffectsToMatchScale(dmgMgrOd);
                }
                m_fRSS_CombatStimBleedingBaseline = -1.0;
            }

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

        RSS_CombatStim_UpdateBleedingScale();
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
        // [v3.23.0] MudSlipPolicy 已移除。非玩家 AI 在 SAFE 状态下阻止泥泞滑倒掷骰。
        if (IsPlayerControlled())
            return false;
        return true;
    }

    bool RSS_ShouldAiAllowMudSlipRagdoll(IEntity owner)
    {
        // [v3.23.0] 与 RSS_IsAiMudSlipBlockedBySafety 一致：非玩家 AI 不掷骰
        return !RSS_IsAiMudSlipBlockedBySafety(owner);
    }

    float RSS_GetLastAppliedSpeedMultiplier()
    {
        return m_fLastRssSpeedMultiplierApplied;
    }

    protected string GetPlayerLabel(IEntity entity)
    {
        return SCR_PlayerBaseConfigHelper.GetPlayerLabel(entity);
    }

    protected bool IsRssDebugEnabled()
    {
        return SCR_PlayerBaseConfigHelper.IsRssDebugEnabled();
    }

    //! 客户端上报：仅用于数据导出/对照（m_bDataExportEnabled），非强反作弊与玩法权威判据。配置由 GameMode RplProp 写入设置；Hint HUD 由 SCR_StaminaHUDComponent.SyncHintDisplayWithSettings 对齐。
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

    //! 客户端 → 服务端：必须用本方法包装 Rpc()，依据引擎源码约定，而非猜测。
    //! - GenericComponent：Rpc 为 proto protected（外部/UI 类无法调用 Rpc）。
    //! - 官方用法示例：SCR_ChimeraCharacter.SetIllumination → Rpc(RPC_SetIllumination_S, …)；
    //!   RPC_* 标 [RplRpc(…, RplRcver.Server)]，由 Rpc() 发到服务端执行。
    //! 若从 UI 直接调用 RPC_AdminUpdateConfig(...)，等同本地普通函数调用，首行 IsServer 守卫即 return，不会走网络。
    void RSS_RequestAdminConfigUpdate(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        if (Replication.IsServer())
            return;
        Rpc(RPC_AdminUpdateConfig, preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    //! 管理员客户端 → 服务端：推送配置变更（预设 + 开关）
    //! 服务端收到后写入 JSON 并复制到所有客户端
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_AdminUpdateConfig(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        if (!Replication.IsServer())
            return;

        // 验证调用者是否为管理员
        IEntity owner = GetOwner();
        if (!owner) return;

        // 专用服务器上 GetPlayerController() 不是 RPC 发起者；必须用当前实体对应的玩家 ID。
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm) return;

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
        Print("[RSS] Admin config applied & replicated from client request");
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

        if (m_bIsDeleted)
            return;

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
        if (m_bIsDeleted)
            return;
        if (!GetOwner())
            return;
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
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
        {
            // 完全禁用 AI RSS 计算：交还引擎处理体力
            if (StaminaConfigBridge.IsAiAllCalcDisabled())
                return false;
            return true;
        }
        return false;
    }

    protected float GetNearestPlayerDistanceM(IEntity ownerEntity)
    {
        if (!ownerEntity)
            return -1.0;

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return -1.0;

        array<int> playerIds = {};
        pm.GetPlayers(playerIds);
        float nearM = 99999.0;
        for (int pi = 0; pi < playerIds.Count(); pi++)
        {
            IEntity pe = pm.GetPlayerControlledEntity(playerIds.Get(pi));
            if (pe)
            {
                float d = vector.Distance(ownerEntity.GetOrigin(), pe.GetOrigin());
                if (d < nearM)
                    nearM = d;
            }
        }
        if (nearM < 99999.0)
            return nearM;
        return -1.0;
    }

    protected int GetSpeedUpdateIntervalMs()
    {
        // 玩家：高速刷新（~60Hz）
        if (IsPlayerControlled())
            return StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS;

        // AI：按距最近玩家的距离分档，减少远处 AI 的计算开销
        if (!Replication.IsServer())
            return StaminaConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        if (!StaminaConstants.RSS_PERF_AI_DISTANCE_LOD_ENABLED)
            return StaminaConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        IEntity ownerEntity = GetOwner();
        if (!ownerEntity)
            return StaminaConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        float distM = GetNearestPlayerDistanceM(ownerEntity);

        if (distM < 0.0 || distM <= StaminaConstants.RSS_PERF_AI_LOD_NEAR_M)
            return StaminaConstants.RSS_PERF_AI_LOD_NEAR_INTERVAL_MS;
        if (distM <= StaminaConstants.RSS_PERF_AI_LOD_FAR_M)
            return StaminaConstants.RSS_PERF_AI_LOD_MID_INTERVAL_MS;
        return StaminaConstants.RSS_PERF_AI_LOD_FAR_INTERVAL_MS;
    }

    //! 在 Workbench 编辑器中，预览实体不应启动体力 tick
    //! 特征：有 CharacterControllerComponent + AIControlComponent + 无玩家控制 + 无 AI 群组
    protected bool IsWorkbenchPreviewEntity()
    {
        #ifdef WORKBENCH
        IEntity owner = GetOwner();
        if (!owner)
            return true;
        if (IsPlayerControlled())
            return false;
        // 查找 AI 组件
        AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiCtrl)
            return true;  // 无 AI 控制组件 = 不是真实 AI
        AIAgent agent = aiCtrl.GetAIAgent();
        if (!agent)
            return true;  // 无 AI Agent = 不是真实 AI
        // 真实 AI 一定有父群组（SCR_AIGroup 或引擎 group）
        AIGroup parentGroup = agent.GetParentGroup();
        if (!parentGroup)
            return true;
        return false;
        #else
        return false;
        #endif
    }

    void StartSystem()
    {
        if (m_bIsDeleted || !GetOwner())
            return;
        // Workbench 预览实体不启动体力 tick
        if (IsWorkbenchPreviewEntity())
            return;
        if (!ShouldProcessStaminaUpdate())
            return;
        if (!GetGame())
            return;
        m_bRssStaminaLoopActive = true;
        int intervalMs = GetSpeedUpdateIntervalMs();
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, intervalMs, false);
        
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }

    void EnsureRssStaminaLoopIfNeeded()
    {
        if (m_bIsDeleted)
            return;
        if (!ShouldProcessStaminaUpdate())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        StartSystem();
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
        StartSystem();
        if (!m_bRssStaminaLoopActive && GetGame())
            GetGame().GetCallqueue().CallLater(EnsureAiStaminaLoopOnServer, 3000, false);
    }



    // ====== 群组调试辅助方法 ======
    //! 计算群组成员间最大分散距离（存活成员间的 max distance）
    static float CalcAiGroupSpreadM(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return -1.0;
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        if (ac < 2)
            return -1.0;

        float maxDist = 0.0;
        for (int i = 0; i < ac - 1; i++)
        {
            AIAgent agI = agents.Get(i);
            if (!agI)
                continue;
            IEntity ceI = agI.GetControlledEntity();
            if (!ceI)
                continue;
            for (int j = i + 1; j < ac; j++)
            {
                AIAgent agJ = agents.Get(j);
                if (!agJ)
                    continue;
                IEntity ceJ = agJ.GetControlledEntity();
                if (!ceJ)
                    continue;
                float d = vector.Distance(ceI.GetOrigin(), ceJ.GetOrigin());
                if (d > maxDist)
                    maxDist = d;
            }
        }
        return maxDist;
    }

    //! 获取群组中存活成员数量
    static int GetAliveMemberCount(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return 0;
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        int alive = 0;
        for (int i = 0; i < ac; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            if (SCR_RSS_AIGroupSync.IsAliveAndActionable(ce))
                alive++;
        }
        return alive;
    }

    //! 实体即将被删除时调用：清理所有引用、停止 CallLater 循环、注销静态注册表
    void RSS_NotifyEntityDeleting()
    {
        if (m_bIsDeleted)
            return;
        m_bIsDeleted = true;
        m_bRssStaminaLoopActive = false;
        
        IEntity owner = GetOwner();
        
        // 清理静态 AI 恢复注册表中的悬空引用
        if (owner && !IsPlayerControlled())
            // [REMOVED v3.23.0] SCR_RSS_AIRestRecoveryRegistry.CleanupEntity(owner);
            // 新架构中不再使用 RecoveryRegistry，cleanup 由 GroupSync.ClearAll 替代
        
        // 清理 UISignalBridge 的实体引用
        if (m_pUISignalBridge)
            m_pUISignalBridge.Cleanup();
        
        // 清零所有缓存的引擎组件引用（防止悬空指针访问）
        m_pCachedOwnerCharacter = null;
        m_pStaminaComponent = null;
        m_pCompartmentAccess = null;
        m_pAnimComponent = null;
        m_pCachedInventoryComponent = null;
        
        // 清理子模块引用（ref 类型可延迟 GC）
        m_pJumpVaultDetector = null;
        m_pStanceTransitionManager = null;
        m_pExerciseTracker = null;
        m_pCollapseTransition = null;
        m_pSlopeSpeedTransition = null;
        m_pTerrainDetector = null;
        m_pMudSlipRunner = null;
        m_pEnvironmentFactor = null;
        m_pFatigueSystem = null;
        m_pEncumbranceCache = null;
        m_pUISignalBridge = null;
        m_pEpocState = null;
        m_pNetworkSyncManager = null;
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