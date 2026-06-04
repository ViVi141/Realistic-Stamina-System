// PlayerBase split: 生命周期 / 初始化 / 循环保障
modded class SCR_CharacterControllerComponent
{
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
            m_pAnaerobicState = null;
            m_pMetabolicLimiter = null;
            m_pAnaerobicNetworkComponent = null;
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
        m_pAnaerobicState = null;
        m_pMetabolicLimiter = null;
        m_pAnaerobicNetworkComponent = null;
        // 清理 AI 子系统管理器
        if (m_pAIManager)
        {
            m_pAIManager.OnEntityDeleted();
            m_pAIManager = null;
        }
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
                SCR_PlayerBaseDebugHelper.LogInitialEnergyCoeff(coeff);
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
        
        // AI 子系统管理器（接管行为层/编队/代理/节流）
        if (!m_pAIManager)
            m_pAIManager = new SCR_RSS_AIManager();

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
        
        // v5系统初始化
        m_pAnaerobicState = new SCR_AnaerobicBurstState();
        m_pMetabolicLimiter = new SCR_MetabolicSpeedLimiter();
        
        // 查找网络同步组件（如果实体上已添加）
        if (owner)
            m_pAnaerobicNetworkComponent = SCR_AnaerobicStateComponent_V5.Cast(owner.FindComponent(SCR_AnaerobicStateComponent_V5));
        
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
                SCR_RSS_Logger.Warn("[RSS] 等待 GameMode 同步配置超时。请确认服务器端 RSS 已加载且复制正常。");
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
}
