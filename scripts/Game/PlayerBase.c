// Realistic Stamina System (RSS) - v3.19.3
// 拟真体力-速度系统：结合体力值和负重，动态调整移动速度并显示状态信息
// 使用精确数学模型（α=0.6，Pandolf模型），不使用近似
// 优化目标：2英里在15分27秒内完成（完成时间：925.8秒，提前1.2秒）
modded class SCR_CharacterControllerComponent
{
    // 速度显示相关
    protected float m_fLastSecondSpeed = 0.0; // 上一秒的速度
    protected float m_fCurrentSecondSpeed = 0.0; // 当前秒的速度
    protected bool m_bHasPreviousSpeed = false; // 是否已有上一秒的速度数据
    protected const int SPEED_SAMPLE_INTERVAL_MS = 1000; // 每秒采集一次速度样本
    
    // 速度更新相关（60Hz 同步）
    // 速度更新间隔见 StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS / RSS_AI_SPEED_UPDATE_INTERVAL_MS；
    // 查询使用 SCR_RSS_AIStaminaBridge.GetSpeedUpdateIntervalMs。
    
    // 状态信息缓存
    protected float m_fLastStaminaPercent = 1.0;
    protected float m_fLastSpeedMultiplier = 1.0;
    protected float m_fLastStaminaUpdateTime = -1.0; // 上次体力更新时间（秒，GetWorldTime），用于按时间计算 timeDelta
    protected SCR_CharacterStaminaComponent m_pStaminaComponent; // 体力组件引用
    protected bool m_bLastExhaustedState = false; // 上次精疲力尽状态，用于状态切换日志
    
    // 网络同步相关
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

    // ==================== "撞墙"阻尼过渡模块 ====================
    // 模块化拆分：使用独立的 CollapseTransition 类管理"撞墙"临界点的5秒阻尼过渡逻辑
    protected ref CollapseTransition m_pCollapseTransition;
    protected ref SlopeSpeedTransition m_pSlopeSpeedTransition;
    
    // ==================== 运动持续时间跟踪模块 ====================
    // 模块化拆分：使用独立的 ExerciseTracker 类管理运动/休息时间跟踪
    protected ref ExerciseTracker m_pExerciseTracker;
    
    // ==================== 跳跃和翻越检测模块 ====================
    // 模块化拆分：使用独立的 JumpVaultDetector 类管理跳跃和翻越检测逻辑
    protected ref JumpVaultDetector m_pJumpVaultDetector;
    
    // ==================== 负重缓存管理模块 ====================
    // 模块化拆分：使用独立的 EncumbranceCache 类管理负重缓存
    protected ref EncumbranceCache m_pEncumbranceCache;
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent; // 缓存库存组件（EncumbranceCache 无效时的回退）
    
    
    // ==================== 疲劳积累系统模块 ====================
    // 模块化拆分：使用独立的 FatigueSystem 类管理疲劳积累和恢复
    protected ref FatigueSystem m_pFatigueSystem;
    
    // ==================== 地形检测模块 ====================
    // 模块化拆分：使用独立的 TerrainDetector 类管理地形检测和系数计算
    protected ref TerrainDetector m_pTerrainDetector;
    
    // ==================== 环境因子模块 ====================
    // 模块化拆分：使用独立的 EnvironmentFactor 类管理环境因子（热应激、降雨湿重等）
    protected ref EnvironmentFactor m_pEnvironmentFactor;
    
    // ==================== UI信号桥接模块 ====================
    // 模块化拆分：使用独立的 UISignalBridge 类管理UI信号桥接
    protected ref UISignalBridge m_pUISignalBridge;
    
    // ==================== EPOC（过量耗氧）延迟机制 ====================
    // 运动停止后，前几秒应该维持高代谢水平，延迟后才开始恢复
    // 使用EpocState类管理EPOC延迟状态（因为EnforceScript不支持基本类型的ref参数）
    protected ref EpocState m_pEpocState;
    
    // ==================== 姿态转换管理模块 ====================
    // 模块化拆分：使用独立的 StanceTransitionManager 类管理姿态转换的体力消耗
    // 基于生物力学做功逻辑：重心在重力场中的垂直位移
    protected ref StanceTransitionManager m_pStanceTransitionManager;
    
    // ==================== 游泳状态和湿重跟踪 ====================
    // 湿重效应：上岸后30秒内，由于衣服吸水，临时增加5-10kg虚拟负重
    protected bool m_bWasSwimming = false; // 上一帧是否在游泳
    protected float m_fWetWeightStartTime = -1.0; // 湿重开始时间（上岸时间）
    protected float m_fCurrentWetWeight = 0.0; // 当前湿重（kg）
    protected bool m_bSwimmingVelocityDebugPrinted = false; // 是否已输出游泳速度调试信息

    // ==================== 速度差分缓存（用于游泳/命令位移测速）====================
    // 说明：游泳命令通过 PrePhys_SetTranslation 直接改变位移，GetVelocity() 可能不更新
    // 因此通过"位置差分/时间步长"计算速度向量，作为消耗模型的速度输入
    
    // ==================== 游泳状态缓存（用于调试显示）====================
    protected vector m_vLastPositionSample = vector.Zero;
    protected bool m_bHasLastPositionSample = false;
    protected vector m_vComputedVelocity = vector.Zero;
    protected CompartmentAccessComponent m_pCompartmentAccess;
    protected CharacterAnimationComponent m_pAnimComponent;

    // ==================== 引擎动画速度补偿 ====================
    // 引擎会因装备动画属性降低角色的基础最大速度，导致 OverrideMaxSpeed 的乘数
    // 作用在一个更低的基数上。此模块检测引擎实际最大速度并补偿。
    protected float m_fAnimSpeedCompensation = 1.0;
    protected float m_fLastAnimCompensationUpdateTime = -1.0;
    protected static const float ANIM_COMPENSATION_UPDATE_INTERVAL = 2.0;
    
    // ==================== 引擎原始最大速度获取 ====================
    // 动态获取引擎的真实原始最大速度，用于正确计算速度倍数
    // 每次需要时先临时重置为1.0获取真实引擎速度，然后再应用我们的速度控制
    // 这样可以正确处理装备改变导致引擎原始速度变化的情况

    // ==================== 战术冲刺爆发（短时全速）====================
    protected float m_fSprintStartTime = -1.0;       // 本次冲刺开始时间（世界时间秒），-1 表示未在爆发/已进入平稳期
    protected bool m_bLastWasSprinting = false;       // 上一帧是否处于 Sprint 状态，用于检测进入/离开冲刺
    protected float m_fSprintCooldownUntil = -1.0;    // 冷却结束时间（世界时间秒），此时间前再次冲刺不触发爆发，直接平稳期
    
    // 泥泞滑倒：状态与每帧判定（独立文件以控制 PlayerBase 单文件体积）
    protected ref RSS_MudSlipRunner m_pMudSlipRunner;
    // 泥泞失稳镜头预警强度 0~1（由 RSS_MudSlipRunner 每帧写入，供 CharacterCamera1stPerson 读取）
    protected float m_fRssMudSlipCameraShake01 = 0.0;
    // 本帧 UpdateSpeedBasedOnStamina 主路径已应用的 RSS 速度倍率（供 AI 泥泞预警二次 OverrideMaxSpeed）
    protected float m_fLastRssSpeedMultiplierApplied = 1.0;
    //! 性能：仅应在参与体力循环时为 true；ShouldProcess==false 时停表不再 CallLater
    protected bool m_bRssStaminaLoopActive = false;
    
    // 在组件初始化后
    override void OnInit(IEntity owner)
    {
        super.OnInit(owner);
        
        if (!owner)
            return;
        
        // ==================== 初始化配置系统 ====================
        // 仅在服务器端加载配置
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();
            // 服务器注册为配置变更监听器
            SCR_RSS_ConfigManager.RegisterConfigChangeListener(owner);
            // 服务器检测到玩家（本控制器）：主动推送配置，直到收到客户端回执
            if (IsPlayerControlled() && GetGame() && GetGame().GetCallqueue())
                GetGame().GetCallqueue().CallLater(ServerPushConfigToOwner, 3000, false);

            // debug: 输出当前的能量转体力系数
            if (IsRssDebugEnabled())
            {
                float coeff = StaminaConstants.GetEnergyToStaminaCoeff();
                PrintFormat("[RSS] 初始 energie->stamina coeff = %1", coeff);
            }
        }
        
        // 获取体力组件引用
        CharacterStaminaComponent staminaComp = GetStaminaComponent();
        m_pStaminaComponent = SCR_CharacterStaminaComponent.Cast(staminaComp);
        
        // 完全禁用原生体力系统
        // 只使用我们的自定义体力计算系统
        if (m_pStaminaComponent)
        {
            // 禁用原生体力系统
            m_pStaminaComponent.SetAllowNativeStaminaSystem(false);
            
            // 设置初始目标体力值为100%（满值）
            m_pStaminaComponent.SetTargetStamina(RealisticStaminaSpeedSystem.INITIAL_STAMINA_AFTER_ACFT);
        }
        
        // 初始化跳跃和翻越检测模块
        m_pJumpVaultDetector = new JumpVaultDetector();
        if (m_pJumpVaultDetector)
            m_pJumpVaultDetector.Initialize();
        
        // 初始化姿态转换管理模块
        m_pStanceTransitionManager = new StanceTransitionManager();
        if (m_pStanceTransitionManager)
        {
            m_pStanceTransitionManager.Initialize();
            // 设置初始姿态（避免第一帧误判）
            m_pStanceTransitionManager.SetInitialStance(GetStance());
        }
        
        // 初始化运动持续时间跟踪模块
        m_pExerciseTracker = new ExerciseTracker();
        if (m_pExerciseTracker)
        {
            float initTime = 0.0;
            if (GetGame() && GetGame().GetWorld())
                initTime = GetGame().GetWorld().GetWorldTime();
            m_pExerciseTracker.Initialize(initTime);
        } 
        
        // 初始化"撞墙"阻尼过渡模块
        m_pCollapseTransition = new CollapseTransition();
        if (m_pCollapseTransition)
            m_pCollapseTransition.Initialize();

        m_pSlopeSpeedTransition = new SlopeSpeedTransition();
        if (m_pSlopeSpeedTransition)
            m_pSlopeSpeedTransition.Initialize();
        
        // 初始化地形检测模块
        m_pTerrainDetector = new TerrainDetector();
        if (m_pTerrainDetector)
            m_pTerrainDetector.Initialize();
        
        m_pMudSlipRunner = new RSS_MudSlipRunner();

        // 初始化环境因子模块
        m_pEnvironmentFactor = new EnvironmentFactor();
        if (m_pEnvironmentFactor)
        {
            World world = null;
            if (GetGame())
                world = GetGame().GetWorld();
            if (world)
            {
                m_pEnvironmentFactor.Initialize(world, owner);
                // 强制使用引擎实时天气（真实天气），避免使用虚拟昼夜模型（但禁用引擎温度，使用模组内计算）
                m_pEnvironmentFactor.SetUseEngineWeather(true);
                m_pEnvironmentFactor.SetUseEngineTemperature(false); // 使用模组内温度计算，每 5 秒步进
            }
        }
        
        // 初始化疲劳积累系统模块
        m_pFatigueSystem = new FatigueSystem();
        if (m_pFatigueSystem)
        {
            float currentTime = 0.0;
            if (GetGame() && GetGame().GetWorld())
                currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            m_pFatigueSystem.Initialize(currentTime);
        }
        
        // 初始化负重缓存模块
        m_pEncumbranceCache = new EncumbranceCache();
        if (m_pEncumbranceCache)
        {
            // 获取并缓存库存组件引用（避免重复查找）
            ChimeraCharacter character = ChimeraCharacter.Cast(owner);
            if (character)
            {
                SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
                m_pCachedInventoryComponent = inventoryComponent;

                // 获取库存管理器组件引用
                InventoryStorageManagerComponent inventoryManagerComponent = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));

                // 初始化负重缓存
                m_pEncumbranceCache.Initialize(inventoryComponent);
            }
        }
        
        // 初始化UI信号桥接模块
        m_pUISignalBridge = new UISignalBridge();
        if (m_pUISignalBridge)
            m_pUISignalBridge.Init(owner);
        
        // 初始化EPOC状态管理
        m_pEpocState = new EpocState();
        
        // 初始化网络同步管理器
        m_pNetworkSyncManager = new NetworkSyncManager();
        if (m_pNetworkSyncManager)
            m_pNetworkSyncManager.Initialize();
        
        // 缓存组件，避免在每0.2s的更新循环中重复查找
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            m_pCompartmentAccess = character.GetCompartmentAccessComponent();
            m_pAnimComponent = character.GetAnimationComponent();
        }
        
        // 延迟初始化，确保组件完全加载
        GetGame().GetCallqueue().CallLater(StartSystem, 500, false);
        // 客户端：载具内乘客等若首帧未建立循环，2s 后再尝试启动（与 ShouldProcess 一致）
        if (!Replication.IsServer())
            GetGame().GetCallqueue().CallLater(EnsureRssStaminaLoopIfNeeded, 2000, false);
        
        if (!Replication.IsServer())
        {
            // 客户端连接状态从 false 开始，由 MonitorNetworkConnection 统一判定首次连入并触发配置请求。
            // 修复：此前默认 true 会跳过首次连入分支，若服务器主动推送因时序未命中，则客户端可能长期收不到配置。
            m_bIsConnected = false;
            GetGame().GetCallqueue().CallLater(MonitorNetworkConnection, 5000, true);
            // 兜底：无论连接状态判定何时成立，先发起一次请求，后续由 IsServerConfigApplied 去重。
            GetGame().GetCallqueue().CallLater(RequestServerConfig, 1500, false);
            // 稳态兜底：持续请求直到客户端已应用服务器配置，避免无AI场景/时序波动导致的一次性漏同步。
            GetGame().GetCallqueue().CallLater(RequestServerConfigUntilApplied, 2500, false);
            GetGame().GetCallqueue().CallLater(UpdateHeartbeat, HEARTBEAT_INTERVAL * 1000, true);
        }
    }
    
    // 服务器主动推送配置给本控制器所属玩家，直到收到客户端回执后不再推送
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

    // 处理配置变更通知
    void OnConfigChanged()
    {
        if (Replication.IsServer())
        {
            // 服务器配置变更，通知各客户端（点对点，由 ConfigManager 对每个监听器调用本方法）
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

    // 获取用于日志的玩家标识（名称/ID）
    protected string GetPlayerLabel(IEntity entity)
    {
        if (!entity)
            return "unknown";

        string entityName = entity.GetName();
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return entityName;

        int playerId = playerManager.GetPlayerIdFromControlledEntity(entity);
        if (playerId <= 0)
            return entityName;

        string playerName = playerManager.GetPlayerName(playerId);
        if (!playerName || playerName == "")
            playerName = "unknown";

        return playerName + " (id=" + playerId.ToString() + ")";
    }

    protected bool IsRssDebugEnabled()
    {
        return StaminaConstants.IsDebugEnabled();
    }

    protected array<float> BuildPresetArray(SCR_RSS_Params p)
    {
        array<float> values = new array<float>();
        SCR_RSS_Settings.WriteParamsToArray(p, values);
        return values;
    }

    protected void BuildSettingsArrays(SCR_RSS_Settings s, array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        SCR_RSS_Settings.WriteSettingsToArrays(s, floatSettings, intSettings, boolSettings);
    }

    // 统一的配置数组构建方法（减少重复代码）
    // 自动构建预设数组和设置数组
    protected void BuildConfigArrays(out array<float> combinedPresetParams, out array<float> floatSettings, out array<int> intSettings, out array<bool> boolSettings)
    {
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;

        combinedPresetParams = new array<float>();
        floatSettings = new array<float>();
        intSettings = new array<int>();
        boolSettings = new array<bool>();

        BuildCombinedPresetArray(settings, combinedPresetParams);
        BuildSettingsArrays(settings, floatSettings, intSettings, boolSettings);
    }

    // 将 4 个预设数组合并为 1 个，用于 RPC（Enfusion Rpc 有参数数量限制）
    protected void BuildCombinedPresetArray(SCR_RSS_Settings s, array<float> outCombined)
    {
        if (!outCombined || !s)
            return;
        outCombined.Clear();
        int size = SCR_RSS_Settings.PARAMS_ARRAY_SIZE;
        array<float> elite = BuildPresetArray(s.m_EliteStandard);
        array<float> standard = BuildPresetArray(s.m_StandardMilsim);
        array<float> tactical = BuildPresetArray(s.m_TacticalAction);
        array<float> custom = BuildPresetArray(s.m_Custom);
        for (int i = 0; i < size; i++)
        {
            if (i < elite.Count())
                outCombined.Insert(elite[i]);
            else
                outCombined.Insert(0.0);
        }
        for (int i = 0; i < size; i++)
        {
            if (i < standard.Count())
                outCombined.Insert(standard[i]);
            else
                outCombined.Insert(0.0);
        }
        for (int i = 0; i < size; i++)
        {
            if (i < tactical.Count())
                outCombined.Insert(tactical[i]);
            else
                outCombined.Insert(0.0);
        }
        for (int i = 0; i < size; i++)
        {
            if (i < custom.Count())
                outCombined.Insert(custom[i]);
            else
                outCombined.Insert(0.0);
        }
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

        // 计算配置哈希值（基于关键配置参数）
        string newConfigHash = CalculateConfigHash(configVersion, selectedPreset, floatSettings, intSettings, boolSettings);

        // 配置未变化时跳过应用（基于哈希值，而非仅版本/预设名）
        // 必须已应用过服务器配置才可跳过：首次连接时客户端默认值可能与服务器版本/预设名相同，若直接跳过会导致预设参数从未被应用
        if (SCR_RSS_ConfigManager.IsServerConfigApplied() && m_sLastAppliedConfigHash != "" && newConfigHash == m_sLastAppliedConfigHash)
        {
            m_fFirstConfigRequestTime = -1.0;
            m_fLastConfigTimeoutWarningTime = -1.0;
            if (IsRssDebugEnabled())
                Print("[RSS] Config unchanged (hash match), skipping apply");
            return;
        }

        if (!settings.m_EliteStandard) settings.m_EliteStandard = new SCR_RSS_Params();
        if (!settings.m_StandardMilsim) settings.m_StandardMilsim = new SCR_RSS_Params();
        if (!settings.m_TacticalAction) settings.m_TacticalAction = new SCR_RSS_Params();
        if (!settings.m_Custom) settings.m_Custom = new SCR_RSS_Params();

        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_EliteStandard, eliteParams);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_StandardMilsim, standardParams);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_TacticalAction, tacticalParams);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_Custom, customParams);

        SCR_RSS_Settings.ApplySettingsFromArrays(settings, floatSettings, intSettings, boolSettings);

        settings.m_sConfigVersion = configVersion;
        settings.m_sSelectedPreset = selectedPreset;

        SCR_RSS_ConfigManager.Save();
        SCR_RSS_ConfigManager.SetServerConfigApplied(true);
        m_fFirstConfigRequestTime = -1.0;  // 收到配置，清除超时计时
        m_fLastConfigTimeoutWarningTime = -1.0;
        m_sLastAppliedConfigHash = newConfigHash; // 更新配置哈希
        PrintFormat("[RSS] Applied full server config: preset=%1, version=%2, hash=%3", selectedPreset, configVersion, newConfigHash);

        // 服务器配置应用后，根据 hint 开关创建或销毁 HUD（修复：配置晚于 InitStaminaHUD 到达时 HUD 未创建的问题）
        if (settings.m_bHintDisplayEnabled)
            SCR_StaminaHUDComponent.Init();
        else
            SCR_StaminaHUDComponent.Destroy();
    }

    // 计算配置哈希值（基于关键配置参数）
    // 用于检测配置内容变化，避免重复应用相同配置
    protected string CalculateConfigHash(string configVersion, string selectedPreset, array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        string hashString = configVersion + "|" + selectedPreset + "|";

        // 合并关键浮点数设置到哈希字符串
        if (floatSettings && floatSettings.Count() > 0)
        {
            for (int i = 0; i < floatSettings.Count(); i++)
            {
                // 四舍五入到3位小数，避免浮点精度差异
                int roundedValue = Math.Round(floatSettings[i] * 1000);
                hashString += string.ToString(roundedValue) + ",";
            }
        }

        // 合并关键整数设置到哈希字符串
        if (intSettings && intSettings.Count() > 0)
        {
            for (int j = 0; j < intSettings.Count(); j++)
            {
                hashString += string.ToString(intSettings[j]) + ",";
            }
        }

        // 合并布尔设置（含 m_bEnableMudSlipMechanism 等），避免仅开关变更时被误判为未变化
        if (boolSettings && boolSettings.Count() > 0)
        {
            for (int bi = 0; bi < boolSettings.Count(); bi++)
            {
                if (boolSettings[bi])
                    hashString += "1,";
                else
                    hashString += "0,";
            }
        }

        // 使用简单的字符串哈希（EnforceScript无内置哈希函数）
        int hash = 0;
        for (int k = 0; k < hashString.Length(); k++)
        {
            hash = ((hash << 5) - hash) + hashString.ToAscii(k);
            hash = hash & 0x7FFFFFFF; // 限制为正整数
        }

        return string.ToString(hash);
    }

    // RPC: 服务器发送完整配置给客户端（目标客户端）
    // 使用合并的 presetParams 以规避 Enfusion Rpc 参数数量限制（最多约 6–7 个）
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

    // RPC: 服务器广播完整配置给所有客户端
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
        int size = SCR_RSS_Settings.PARAMS_ARRAY_SIZE;
        if (!combinedPresetParams || combinedPresetParams.Count() < size * 4)
            return false;
        array<float> eliteParams = new array<float>();
        array<float> standardParams = new array<float>();
        array<float> tacticalParams = new array<float>();
        array<float> customParams = new array<float>();
        for (int i = 0; i < size; i++)
            eliteParams.Insert(combinedPresetParams[i]);
        for (int i = size; i < size * 2; i++)
            standardParams.Insert(combinedPresetParams[i]);
        for (int i = size * 2; i < size * 3; i++)
            tacticalParams.Insert(combinedPresetParams[i]);
        for (int i = size * 3; i < size * 4; i++)
            customParams.Insert(combinedPresetParams[i]);
        ApplyFullConfig(configVersion, selectedPreset, eliteParams, standardParams, tacticalParams, customParams, floatSettings, intSettings, boolSettings);
        return true;
    }
    
    // 网络连接监控
    void MonitorNetworkConnection()
    {
        if (Replication.IsServer()) return;

        if (!GetGame()) return;
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager) return;

        // 获取当前时间（秒）
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        // 方法1：使用 PlayerManager 检测玩家ID
        int playerId = playerManager.GetPlayerIdFromControlledEntity(GetOwner());
        bool hasValidPlayerId = (playerId != 0);

        // 方法2：心跳检测（补充验证）
        // 如果玩家ID存在，检查心跳超时（防止静默断开）
        bool isHeartbeatActive = false;
        if (hasValidPlayerId && m_fLastHeartbeatTime > 0.0)
        {
            float heartbeatTimeout = 10.0; // 心跳超时阈值（秒）
            isHeartbeatActive = ((currentTime - m_fLastHeartbeatTime) < heartbeatTimeout);
        }

        // 综合判断连接状态
        // 条件：有效的玩家ID AND（未启动心跳 或 心跳活跃）
        bool isConnected = hasValidPlayerId && (m_fLastHeartbeatTime == 0.0 || isHeartbeatActive);

        // 检测PlayerID变化（重连/重新连接）
        bool playerIdChanged = (m_iPlayerId != 0 && playerId != 0 && m_iPlayerId != playerId);

        // 更新玩家ID缓存
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

        // 检测首次连接（初始化时触发）
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
                // 发起配置请求
                RequestServerConfig();
            }
        }
        // 检测断线重连（通过PlayerID变化检测）
        else if (m_bIsConnected && playerIdChanged && isConnected)
        {
            // 网络重连：立即重置配置状态，避免使用旧服务器配置；重置超时计时以便重新计时
            SCR_RSS_ConfigManager.ResetClientConfigAwaitingSync();
            m_iConfigRetryCount = 0; // 重置重试计数
            m_fFirstConfigRequestTime = -1.0;
            m_fLastConfigTimeoutWarningTime = -1.0;
            m_fLastReconnectTime = currentTime;
            Print("[RSS] 网络已重连，等待服务器推送配置");
            // 延迟请求配置（避免服务器推送冲突）
            GetGame().GetCallqueue().CallLater(RequestServerConfig, RECONNECT_SYNC_DELAY * 1000, false);
        }
        // 检测断线
        else if (m_bIsConnected && !isConnected)
        {
            m_bIsConnected = false;
            m_iPlayerId = 0;
            m_fLastHeartbeatTime = 0.0;
            Print("[RSS] 网络连接已断开");
        }

        // 更新心跳时间（如果连接正常）
        if (isConnected)
        {
            m_fLastHeartbeatTime = currentTime;
        }
    }
    

    
    
    // 启动系统（仅当本实体需要参与 RSS 时调度；远端复制体不启动以节省性能）
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

    //! 获得本地控制等时机补挂体力循环（与 StartSystem 互斥重复调度）
    void EnsureRssStaminaLoopIfNeeded()
    {
        if (!ShouldProcessStaminaUpdate())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        StartSystem();
    }

    // ==================== 游泳状态检测（用于调试显示/分支判断）====================
    protected bool IsSwimmingByCommand()
    {
        if (!m_pAnimComponent)
            return false;
        
        CharacterCommandHandlerComponent handler = m_pAnimComponent.GetCommandHandler();
        if (!handler)
            return false;
        
        return (handler.GetCommandSwim() != null);
    }
    
    // 检测游泳时是否有动作输入（用于区分主动游泳与海浪漂移）
    // GetCurrentInputAngle 在空闲时返回 false，海浪漂移时玩家未按键，故不计算水平/垂直消耗
    protected bool HasSwimInput()
    {
        if (!m_pAnimComponent)
            return false;
        CharacterCommandHandlerComponent handler = m_pAnimComponent.GetCommandHandler();
        if (!handler)
            return false;
        CharacterCommandMove moveCmd = handler.GetCommandMove();
        if (!moveCmd)
            return false;
        float inputAngle = 0.0;
        return moveCmd.GetCurrentInputAngle(inputAngle);
    }
    
    // ==================== 改进：使用动作监听器检测跳跃输入 ====================
    // 采用多重检测方法确保准确性：
    // 1. 动作监听器（OnJumpActionTriggered）- 实时检测输入动作
    // 2. OnPrepareControls - 备用检测方法，每帧检测输入动作
    
    // 覆盖 OnControlledByPlayer 来添加/移除动作监听器
    override void OnControlledByPlayer(IEntity owner, bool controlled)
    {
        super.OnControlledByPlayer(owner, controlled);
        
        if (controlled)
            EnsureRssStaminaLoopIfNeeded();
        
        // 只对本地控制的玩家处理
        if (controlled && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                // 尝试添加多个可能的动作名称
                inputManager.AddActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.AddActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.AddActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
                
                // 调试输出
                if (IsRssDebugEnabled())
                    Print("[RSS] 跳跃动作监听器已添加 / Jump Action Listener Added");
            }
            
            // 初始化体力 HUD 显示（延迟初始化，确保 HUD 系统已加载）
            GetGame().GetCallqueue().CallLater(InitStaminaHUD, 1000, false);
        }
        else
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                // 移除所有可能的动作监听器
                inputManager.RemoveActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
            }
            
            // 销毁体力 HUD 显示
            SCR_StaminaHUDComponent.Destroy();
        }
    }
    
    // 初始化体力 HUD 显示
    void InitStaminaHUD()
    {
        SCR_StaminaHUDComponent.Init();
    }
    
    // 跳跃动作监听器回调函数
    // 当玩家按下跳跃键时立即调用
    void OnJumpActionTriggered(float value = 0.0, EActionTrigger trigger = 0)
    {
        IEntity owner = GetOwner();
        if (!owner || owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 检查是否在载具中（载具中的 "Jump" 是离开载具的动作，不是跳跃）
        bool isInVehicle = false;
        if (m_pCompartmentAccess && m_pCompartmentAccess.GetCompartment())
            isInVehicle = true;
        
        // 如果不在载具中，设置跳跃输入标志（使用模块方法）
        if (!isInVehicle && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            if (IsRssDebugEnabled())
                Print("[RSS] 动作监听器检测到跳跃输入！/ Action Listener Detected Jump Input!");
        }
    }
    
    // 在 OnPrepareControls 中作为备用检测方法
    override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
    {
        super.OnPrepareControls(owner, am, dt, player);
        
        // 只对本地控制的玩家处理
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 检查是否在载具中
        bool isInVehicle = false;
        if (m_pCompartmentAccess && m_pCompartmentAccess.GetCompartment())
            isInVehicle = true;
        
        // 备用检测：如果在 OnPrepareControls 中检测到跳跃输入，也设置标志（使用模块方法）
        if (!isInVehicle && am.GetActionTriggered("Jump") && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            if (IsRssDebugEnabled())
                Print("[RSS] OnPrepareControls 检测到跳跃输入！/ OnPrepareControls Detected Jump Input!");
        }
    }
    
    // 姿态切换锁定说明：Override SetStanceChange/CanChangeStance 会与基类或合并模组产生 Multiple declaration，
    // 无法在本 addon 中通过拦截上述接口实现"动画未完成时禁止再次切换"。反复蹲起仍由 StanceTransitionManager
    // 的疲劳堆积与单次消耗计费约束。
    
    // 判断当前角色是否应参与体力系统更新
    // 玩家：仅本地控制实体（含载具内：本地控制的载具且本角色在该载具内）；AI：仅服务器端
    protected bool ShouldProcessStaminaUpdate()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return false;
        // 本地控制的玩家（角色直接受控）
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            return true;
        // 载具内：本地控制的实体是载具，且本角色在该载具内，也应处理（否则 HUD 不更新）
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
        // 服务器端的 AI（非玩家控制）
        if (Replication.IsServer() && !IsPlayerControlled())
            return true;
        return false;
    }

    // 获取当前角色的速度更新间隔（玩家≈17ms，AI 100ms）
    protected int GetSpeedUpdateIntervalMs()
    {
        return SCR_RSS_AIStaminaBridge.GetSpeedUpdateIntervalMs(this);
    }

    // 获取本次冲刺开始时间（世界时间秒）；供战术冲刺爆发期判断使用
    float GetSprintStartTime()
    {
        return m_fSprintStartTime;
    }

    // 获取冲刺冷却结束时间（世界时间秒）；-1 表示无冷却。供屏幕效果等判断「冷却即将完成」使用
    float GetSprintCooldownUntil()
    {
        return m_fSprintCooldownUntil;
    }

    // 泥泞滑倒：启动引擎 Ragdoll，并在短延迟后混合回动画（供 RSS_MudSlipRunner 调用，勿改为 protected）
    void RSS_TriggerMudSlipRagdoll()
    {
        if (m_pAnimComponent && m_pAnimComponent.IsRagdollActive())
            return;
        Ragdoll();
        // 必须延长 warmup，否则引擎会因“骨骼已静止”立刻结束布娃娃，肉眼几乎无反馈
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
        if (value < 0.0)
            value = 0.0;
        if (value > 1.0)
            value = 1.0;
        m_fRssMudSlipCameraShake01 = value;
    }

    float RSS_GetMudSlipCameraShake01()
    {
        return m_fRssMudSlipCameraShake01;
    }

    bool RSS_IsRagdollActiveForCamera()
    {
        if (m_pAnimComponent && m_pAnimComponent.IsRagdollActive())
            return true;
        return false;
    }

    // AI 泥泞安全 / 限速 / Ragdoll 许可：实现见 SCR_RSS_AIStaminaBridge（供 MudSlipRunner 等调用）
    bool RSS_IsAiMudSlipBlockedBySafety(IEntity owner)
    {
        return SCR_RSS_AIStaminaBridge.IsMudSlipBlockedBySafety(this, owner);
    }

    bool RSS_ShouldAiAllowMudSlipRagdoll(IEntity owner)
    {
        return SCR_RSS_AIStaminaBridge.ShouldAllowMudSlipRagdoll(this, owner);
    }

    //! 供群组代理队员同步队长 OverrideMaxSpeed 倍率
    float RSS_GetLastAppliedSpeedMultiplier()
    {
        return m_fLastRssSpeedMultiplierApplied;
    }

    // 根据体力更新速度（玩家≈17ms，AI 100ms）
    void UpdateSpeedBasedOnStamina()
    {
        IEntity owner = GetOwner();
        if (!owner)
        {
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, StaminaConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS, false);
            return;
        }

        // 仅对本地玩家或服务器端 AI 处理
        if (!ShouldProcessStaminaUpdate())
        {
            RSS_SetMudSlipCameraShake01(0.0);
            m_bRssStaminaLoopActive = false;
            return;
        }

        // 服端：远距非交战群组跟随者仅同步队长体力与速度，不跑全量 RSS
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
        
        // ==================== 载具检测：如果在载具中，不消耗体力 ====================
        // 检查是否在载具中（载具中不应该消耗体力）
        bool isInVehicle = false;
        if (m_pCompartmentAccess && m_pCompartmentAccess.GetCompartment())
            isInVehicle = true;
        
        // 如果在载具中，只恢复体力，不消耗体力，也不更新速度
        if (isInVehicle)
        {
            // 调试信息：载具状态
            static int vehicleDebugCounter = 0;
            vehicleDebugCounter++;
            if (vehicleDebugCounter >= 25) // 每5秒输出一次
            {
                vehicleDebugCounter = 0;
                if (m_pStaminaComponent && IsRssDebugEnabled())
                {
                    float currentStamina = m_pStaminaComponent.GetTargetStamina();
                    PrintFormat("[RSS] 载具中 / In Vehicle: 体力=%1%% | Stamina=%1%%", 
                        Math.Round(currentStamina * 100.0).ToString());
                }
            }
            
            // ==================== 载具内更新运动/休息跟踪（运动累积疲劳继承上车前最后一帧）====================
            // 先调用 Update 再读取，使恢复率与 ETA 使用更新后的 restDuration/exerciseDuration
            float vehicleRestMinutes = 0.0;
            float vehicleExerciseMinutes = 0.0;
            if (m_pExerciseTracker)
            {
                // ExerciseTracker.Update 期望毫秒（与 Initialize 一致）
                float vehicleCurrentTimeMs = GetGame().GetWorld().GetWorldTime();
                m_pExerciseTracker.Update(vehicleCurrentTimeMs, false, true);  // 载具内视为休息，保留休息进度
                vehicleRestMinutes = m_pExerciseTracker.GetRestDurationMinutes();
                vehicleExerciseMinutes = m_pExerciseTracker.GetExerciseDurationMinutes();
            }
            
            // ==================== 载具内恢复体力（优化：无视环境、趴姿、无负重、零速度）====================
            // 运动累积疲劳继承上车前最后一帧；环境=null，姿态=趴下，负重=0，速度=0
            float vehicleStaminaPercent = 1.0;
            if (m_pStaminaComponent)
                vehicleStaminaPercent = Math.Clamp(m_pStaminaComponent.GetTargetStamina(), 0.0, 1.0);
            
            // 与行人路径共用 GetNetStaminaRatePerSecond，仅参数不同（载具内 inVehicle=true）
            float vehicleNetRatePerSec = 0.0;
            if (vehicleStaminaPercent < 1.0)
            {
                // 行人空闲时 totalDrainRate 为负（Rest 恢复），净率 = recoveryRate - totalDrainRate 会额外贡献；载具内需传入相同负值
                vehicleNetRatePerSec = StaminaUpdateCoordinator.GetNetStaminaRatePerSecond(
                    vehicleStaminaPercent, false, 0.0, -StaminaConstants.REST_RECOVERY_PER_TICK, 0.0, 0.0, 1.0,
                    m_pEpocState, m_pEncumbranceCache, m_pExerciseTracker, this, null, true);
                float vehicleRecoveryRate = vehicleNetRatePerSec / 5.0;  // 转为每 0.2 秒，供体力更新
                
                // 体力更新（与行人路径一致：应用疲劳上限，更新疲劳衰减）
                float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
                if (m_pFatigueSystem)
                    m_pFatigueSystem.ProcessFatigueDecay(currentWorldTime, 0.0);  // 载具内视为静止
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
                    newStamina = maxStaminaCap;  // 超过疲劳上限时立即降至上限
                m_pStaminaComponent.SetTargetStamina(newStamina);
                m_fLastStaminaUpdateTime = currentWorldTime;
                vehicleStaminaPercent = newStamina;
                
                if (vehicleDebugCounter == 0 && IsRssDebugEnabled())
                {
                    PrintFormat("[RSS] 载具中恢复 / Vehicle Recovery: %1%% → %2%% (净率: %3/s)",
                        Math.Round(oldStamina * 100.0).ToString(),
                        Math.Round(newStamina * 100.0).ToString(),
                        vehicleNetRatePerSec.ToString());
                }
            }
            
            // ==================== 载具内 HUD 更新 ====================
            // 载具内始终为恢复模式，确保 ETA 显示绿色（不受上一帧消耗/恢复状态影响）
            SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
            if (settings && settings.m_bHintDisplayEnabled)
            {
                float vehicleTimeToDepleteSec = -1.0;  // 载具内永不消耗
                float vehicleTimeToFullSec = -1.0;
                float targetStamina = 1.0;
                if (m_pFatigueSystem)
                    targetStamina = m_pFatigueSystem.GetMaxStaminaCap();
                if (vehicleStaminaPercent < targetStamina && vehicleNetRatePerSec > 0.00001)
                {
                    vehicleTimeToFullSec = (targetStamina - vehicleStaminaPercent) / vehicleNetRatePerSec;
                    if (vehicleTimeToFullSec < 0.5)
                        vehicleTimeToFullSec = 0.5;  // 避免 HUD 因 <0.5 显示黑色
                    if (vehicleTimeToFullSec > 7200.0)
                        vehicleTimeToFullSec = 7200.0;
                }
                
                float vehicleDebugWeight = 0.0;
                if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
                    vehicleDebugWeight = m_pEncumbranceCache.GetCurrentWeight();
                
                DebugInfoParams vehicleParams = new DebugInfoParams();
                vehicleParams.owner = owner;
                vehicleParams.movementTypeStr = "Vehicle";
                vehicleParams.staminaPercent = vehicleStaminaPercent;
                vehicleParams.baseSpeedMultiplier = 1.0;
                vehicleParams.encumbranceSpeedPenalty = 0.0;
                vehicleParams.finalSpeedMultiplier = 1.0;
                vehicleParams.gradePercent = 0.0;
                vehicleParams.slopeAngleDegrees = 0.0;
                vehicleParams.isSprinting = false;
                vehicleParams.currentMovementPhase = 0;
                vehicleParams.debugCurrentWeight = vehicleDebugWeight;
                vehicleParams.combatEncumbrancePercent = 0.0;
                vehicleParams.terrainDetector = m_pTerrainDetector;
                vehicleParams.environmentFactor = m_pEnvironmentFactor;
                vehicleParams.heatStressMultiplier = 1.0;
                vehicleParams.rainWeight = 0.0;
                vehicleParams.swimmingWetWeight = m_fCurrentWetWeight;
                vehicleParams.currentSpeed = 0.0;
                vehicleParams.isSwimming = false;
                vehicleParams.stanceTransitionManager = m_pStanceTransitionManager;
                vehicleParams.timeToDepleteSec = vehicleTimeToDepleteSec;
                vehicleParams.timeToFullSec = vehicleTimeToFullSec;
                DebugDisplay.OutputHintInfo(vehicleParams);
            }
            
            // 继续调度下一次更新，但不进行速度更新和体力消耗计算
            RSS_SetMudSlipCameraShake01(0.0);
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
            return;
        }
        
        // 获取当前体力百分比（使用目标体力值，这是我们控制的）
        float staminaPercent = 1.0;
        if (m_pStaminaComponent)
            staminaPercent = m_pStaminaComponent.GetTargetStamina();
        
        // 限制在有效范围内
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        // [修复 v3.7.0] 移除了强制归零逻辑(staminaPercent < 0.01 = 0.0)
        // 允许浮点数精度的微量恢复，防止进入归零死锁
        // 原逻辑：if (staminaPercent < 0.01) staminaPercent = 0.0;
        // 问题：这会导致体力在 0.9% 时被强制清零，与静态消耗形成死循环
        
        // ==================== 负重缓存 & 惩罚 ====================
        // 在检查精疲力尽之前预先计算负重惩罚，用于动态跛行速度
        float encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.CheckAndUpdate();
            encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenalty();
        }

        // ==================== 精疲力尽逻辑（融合模型）====================
        // 如果体力 ≤ 0，强制速度为当前最大Walk速度（考虑负重）
        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);
        if (isExhausted)
        {
            // 计算动态跛行倍率
            float limpSpeedMultiplier = RealisticStaminaSpeedSystem.GetDynamicLimpMultiplier(encumbranceSpeedPenalty);
            float compensatedLimpMultiplier = Math.Clamp(limpSpeedMultiplier * m_fAnimSpeedCompensation, 0.01, 1.0);
            OverrideMaxSpeed(compensatedLimpMultiplier);

            // 调试信息：精疲力尽状态
            if (!m_bLastExhaustedState && IsRssDebugEnabled())
            {
                Print("[RSS] 精疲力尽 / Exhausted: 速度限制为动态跛行速度 | Speed Limited to Dynamic Limp Speed");
                m_bLastExhaustedState = true;
            }

            // 继续执行后续逻辑（但速度已被限制）
            // 注意：即使精疲力尽，仍然需要更新体力值（恢复）
        }
        else
        {
            if (m_bLastExhaustedState && IsRssDebugEnabled())
            {
                Print("[RSS] 脱离精疲力尽状态 / Recovered from Exhaustion: 速度恢复正常 | Speed Restored");
                m_bLastExhaustedState = false;
            }
        }
        
        // ==================== 性能优化：使用缓存的负重值（模块化）====================
        // 注意：负重惩罚已在上文为精疲力尽计算过一次，这里无需再次获取。
        
        // ==================== 获取当前实际速度（m/s）====================
        // 游泳时 GetVelocity() 可能不更新（PrePhys_SetTranslation 直接改位移），使用位置差分测速
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
            vector horizontalVelocity = velocity;
            horizontalVelocity[1] = 0.0;  // 忽略垂直速度
            currentSpeed = horizontalVelocity.Length();
            currentSpeed = Math.Min(currentSpeed, 7.0);
            m_bHasLastPositionSample = false;  // 离开游泳时重置
        }
        
        // ==================== 战术冲刺爆发与冷却 ====================
        float currentTimeSprint = GetGame().GetWorld().GetWorldTime() / 1000.0;
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        bool isSprintActive = isSprintingNow || (phaseNow == 3);
        if (isSprintActive && !m_bLastWasSprinting)
        {
            if (currentTimeSprint >= m_fSprintCooldownUntil)
                m_fSprintStartTime = currentTimeSprint;
        }
        else if (m_bLastWasSprinting && !isSprintActive)
        {
            m_fSprintCooldownUntil = currentTimeSprint + StaminaConstants.GetTacticalSprintCooldown();
        }
        if (isSprintActive && m_fSprintStartTime >= 0.0)
        {
            float burstDuration = StaminaConstants.GetTacticalSprintBurstDuration();
            float bufferDuration = StaminaConstants.GetTacticalSprintBurstBufferDuration();
            float elapsed = currentTimeSprint - m_fSprintStartTime;
            if (burstDuration > 0.0 && bufferDuration >= 0.0 && elapsed >= burstDuration + bufferDuration)
            {
                m_fSprintCooldownUntil = currentTimeSprint + StaminaConstants.GetTacticalSprintCooldown();
                m_fSprintStartTime = -1.0;
            }
        }
        m_bLastWasSprinting = isSprintActive;
        
        // ==================== 速度计算和更新（模块化）====================
        // 模块化：使用 StaminaUpdateCoordinator 更新速度（传入 velocity 用于坡度上下判断）
        float finalSpeedMultiplier = StaminaUpdateCoordinator.UpdateSpeed(
            this,
            staminaPercent,
            encumbranceSpeedPenalty,
            m_pCollapseTransition,
            currentSpeed,
            m_pEnvironmentFactor,
            m_pSlopeSpeedTransition,
            velocity);
        
        // 获取基础速度倍数（用于调试显示）
        float baseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        // ==================== 手动控制体力消耗（基于精确医学模型）====================
        // 根据实际速度和负重计算体力消耗率
        // 
        // 精确数学模型：基于 Pandolf 能量消耗模型（Pandolf et al., 1977）
        // 完整公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
        // 其中：
        //   E = 能量消耗率（W/kg）
        //   M = 总重量（身体重量 + 负重）
        //   V = 速度（m/s）
        //   G = 坡度（0 = 平地）
        // 
        // 简化版本（平地，G=0）：E = M·(2.7 + 3.2·(V-0.7)²)
        // 
        // 对于游戏实现，我们使用相对化的版本：
        // 体力消耗率 = a + b·V + c·V² + d·M_encumbrance·(1 + e·V²)
        // 其中 M_encumbrance 是负重相对于身体重量的比例
        
        // 调试信息：速度计算（每5秒输出一次）
        
        // ==================== 性能优化：使用缓存的当前重量（模块化）====================
        // 使用缓存的当前重量（避免重复查找组件）
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

        // 提前取得当前时间（秒），供速度应用与体力 RPC 节流使用
        float currentTimeForExerciseMs = GetGame().GetWorld().GetWorldTime();
        float currentTime = currentTimeForExerciseMs / 1000.0;  // 秒，供湿重/环境因子等模块使用

        // ==================== 已移除引擎动画速度补偿 ====================
        // 由于现在模组完全直接控制绝对速度，不再需要动画速度补偿
        // 直接计算目标绝对速度并转换为相对于GAME_MAX_SPEED的正确倍数即可

        // ==================== 速度倍率应用 ====================
        // 玩家：客户端本地计算；导出开启时采用服务器校验速度（平滑值）；AI：服务器端计算
        float speedToApply = finalSpeedMultiplier;
        if (!Replication.IsServer() && IsPlayerControlled() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled() && m_pNetworkSyncManager.HasServerValidation())
        {
            m_pNetworkSyncManager.GetTargetSpeedMultiplier(finalSpeedMultiplier);
            speedToApply = m_pNetworkSyncManager.GetSmoothedSpeedMultiplier(currentTime);
        }
        // 由于我们现在直接计算绝对速度并转换为相对于GAME_MAX_SPEED的正确倍数，
        // 不需要动画速度补偿，因为我们的计算已经基于真实目标速度，不再依赖引擎的动画速度
        float finalSpeedToApply = Math.Clamp(speedToApply, 0.01, 1.0);
        m_fLastRssSpeedMultiplierApplied = finalSpeedToApply;
        OverrideMaxSpeed(finalSpeedToApply);
        if (IsPlayerControlled())
            m_sLastSpeedSource = "Client";
        else
            m_sLastSpeedSource = "Server";

        // 仅当服务器开启数据导出时向服务器上报体力与负重
        // 关键数据（体力耗尽等）允许即时上报，绕过速率限制
        bool isCriticalData = (staminaPercent <= 0.05 || (m_pNetworkSyncManager && m_pNetworkSyncManager.GetLastReportedStaminaPercent() > 0.5 && staminaPercent <= 0.1));
        if (!Replication.IsServer() && IsPlayerControlled() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled())
        {
            bool shouldSync = m_pNetworkSyncManager.ShouldSync(currentTime);
            if (shouldSync || isCriticalData)
            {
                Rpc(RPC_ClientReportStamina, staminaPercent, currentWeight, currentTime, isCriticalData);
                if (isCriticalData && IsRssDebugEnabled())
                    PrintFormat("[RSS] Critical stamina event reported (stamina=%1)", staminaPercent);
            }
        }

        // ==================== 检测游泳状态（游泳体力管理）====================
        // 模块化：使用 SwimmingStateManager 管理游泳状态和湿重
        bool isSwimming = SwimmingStateManager.IsSwimming(this);

        // 运动/休息时间跟踪（用于地形检测和疲劳计算）
        // GetWorldTime 返回毫秒；ExerciseTracker.Update 期望毫秒；其他模块用秒（currentTime 已在上方取得）
        
        // 体力更新用实际时间差（GetWorldTime），不依赖预期间隔
        float timeDeltaSec;
        if (m_fLastStaminaUpdateTime >= 0.0)
            timeDeltaSec = currentTime - m_fLastStaminaUpdateTime;
        else
            timeDeltaSec = GetSpeedUpdateIntervalMs() / 1000.0;
        timeDeltaSec = Math.Clamp(timeDeltaSec, 0.01, 0.5);
        
        // 如果游泳状态变化，重置调试标志
        if (isSwimming != m_bWasSwimming)
            m_bSwimmingVelocityDebugPrinted = false;
        
        // 更新湿重（模块化）
        WetWeightUpdateResult wetWeightResult = SwimmingStateManager.UpdateWetWeight(
            m_bWasSwimming,
            isSwimming,
            currentTime,
            m_fWetWeightStartTime,
            m_fCurrentWetWeight,
            owner);
        m_fWetWeightStartTime = wetWeightResult.wetWeightStartTime;
        m_fCurrentWetWeight = wetWeightResult.currentWetWeight;
        
        // 更新上一帧状态
        m_bWasSwimming = isSwimming;
        
        // 地形系数：铺装路面 1.0 → 深雪 2.1-3.0
        // 优化检测频率：移动时0.5秒检测一次，静止时2秒检测一次（性能优化）
        // 注意：terrain 检测使用秒单位的 currentTime
        float terrainFactor = 1.0; // 默认值（铺装路面）
        if (m_pTerrainDetector)
        {
            terrainFactor = m_pTerrainDetector.GetTerrainFactor(owner, currentTime, currentSpeed);
        }
        
        // ==================== 环境因子更新（模块化）====================
        // 每5秒更新一次环境因子（性能优化）
        // 传入角色实体用于室内检测，传入速度向量用于风阻计算，传入地形系数用于泥泞计算，传入游泳湿重用于总湿重计算
        // 性能优化：复用前面已获取的 velocity，避免重复 GetVelocity()
        if (m_pEnvironmentFactor)
        {
            m_pEnvironmentFactor.UpdateEnvironmentFactors(currentTime, owner, velocity, terrainFactor, m_fCurrentWetWeight);
        }
        
        // 获取热应激倍数（影响体力消耗和恢复）
        float heatStressMultiplier = 1.0;
        if (m_pEnvironmentFactor)
            heatStressMultiplier = m_pEnvironmentFactor.GetHeatStressMultiplier();
        
        // 获取降雨湿重（影响总重量）
        float rainWeight = 0.0;
        if (m_pEnvironmentFactor)
            rainWeight = m_pEnvironmentFactor.GetRainWeight();
        
        // 应用湿重到当前重量（用于消耗计算）
        // 模块化：使用 SwimmingStateManager 计算总湿重
        float totalWetWeight = SwimmingStateManager.CalculateTotalWetWeight(m_fCurrentWetWeight, rainWeight);
        float currentWeightWithWet = currentWeight + totalWetWeight;

        // 修复：计算包含身体重量的总重并传入消耗模块（Pandolf 期望的输入）
        // currentWeight/currentWeightWithWet 原本为装备/背包重量（不含身体质量）
        float totalWeight = currentWeight + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+身体
        float totalWeightWithWetAndBody = currentWeightWithWet + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+湿重+身体

        bool useSwimmingModel = isSwimming;

        // ==================== 检测跳跃和翻越动作（模块化：使用 JumpVaultDetector）====================
        // 模块化拆分：使用独立的 JumpVaultDetector 类处理跳跃和翻越检测逻辑
        if (m_pStaminaComponent && m_pJumpVaultDetector)
        {
            // 获取信号管理器引用（用于跳跃/翻越时的UI信号更新）
            SignalsManagerComponent signalsManager = null;
            if (m_pUISignalBridge)
                signalsManager = m_pUISignalBridge.GetSignalsManager();
            int exhaustionSignalID = -1;
            if (m_pUISignalBridge)
                exhaustionSignalID = m_pUISignalBridge.GetExhaustionSignalID();
            
            // 处理跳跃逻辑（返回体力消耗值）
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
            
            // 处理翻越逻辑（返回体力消耗值）
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
            
            // 更新冷却时间（已改为时间戳判定，保留调用以兼容）
            m_pJumpVaultDetector.UpdateCooldowns();
            
            // ==================== 姿态转换处理（模块化）====================
            m_pStanceTransitionManager.UpdateFatigue(timeDeltaSec);
            
            // 无切换满 1 秒则立刻结算上一窗口（先结算再处理本次切换）
            float currentTimeSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float stanceSettleCost = m_pStanceTransitionManager.TrySettleWindow(currentTimeSec);
            if (stanceSettleCost > 0.0)
                staminaPercent = staminaPercent - stanceSettleCost;
            
            // 处理姿态转换的体力消耗（基于乳酸堆积模型）
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
            
            // 限制体力值在有效范围内
            staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        }
        
        // 计算体力消耗率（基于精确 Pandolf 模型）
        // 使用绝对速度（m/s），而不是相对速度，以符合医学模型
        // GAME_MAX_SPEED = 5.5 m/s（0kg 冲刺最大），Run 3.8 m/s
        float speedRatio = Math.Clamp(currentSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED, 0.0, 1.0);
        
        // ==================== 融合模型：基于速度阈值的分段消耗率 + 多维度特性 ====================
        // 融合两种模型：
        // 1. 基于速度阈值的分段消耗率（Military Stamina System Model）
        // 2. 多维度特性（健康状态、累积疲劳、代谢适应）
        // 
        // 基础消耗率基于速度阈值：
        // - Sprint (V ≥ 5.5 m/s): 0.480 pts/s
        // - Run (3.8 ≤ V < 5.5 m/s): 0.105 pts/s
        // - Walk (3.2 ≤ V < 3.8 m/s): 0.060 pts/s
        // - Rest (V < 3.2 m/s): -0.250 pts/s（恢复）
        //
        // 然后应用多维度修正：
        // - 健康状态效率因子
        // - 累积疲劳因子
        // - 代谢适应效率因子
        // - 坡度修正（K_grade = 1 + G × 0.12）
        
        // ==================== 疲劳积累恢复机制（模块化）====================
        // 疲劳积累需要长时间休息才能恢复（模拟身体恢复过程）
        // 只有静止时间超过60秒后，疲劳才开始恢复
        if (m_pFatigueSystem)
        {
            float currentTimeForFatigue = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
            m_pFatigueSystem.ProcessFatigueDecay(currentTimeForFatigue, currentSpeed);
        }
        
        // ==================== 运动持续时间跟踪（模块化）====================
        // 更新运动/休息时间跟踪，并计算累积疲劳因子
        // 注意：currentTimeForExercise 已在上面声明
        bool isCurrentlyMoving = (currentSpeed > 0.05);
        float fatigueFactor = 1.0; // 默认值（无疲劳）
        
        if (m_pExerciseTracker)
        {
            // 更新运动/休息时间跟踪（ExerciseTracker 期望毫秒）
            m_pExerciseTracker.Update(currentTimeForExerciseMs, isCurrentlyMoving);
            
            // 计算累积疲劳因子（基于运动持续时间）
            // 公式：fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × max(0, exercise_duration - FATIGUE_START_TIME)
            fatigueFactor = m_pExerciseTracker.CalculateFatigueFactor();
        }
        
        // ==================== 效率因子计算（模块化）====================
        float metabolicEfficiencyFactor = StaminaConsumptionCalculator.CalculateMetabolicEfficiencyFactor(speedRatio);
        float fitnessEfficiencyFactor = StaminaConsumptionCalculator.CalculateFitnessEfficiencyFactor();
        float totalEfficiencyFactor = fitnessEfficiencyFactor * metabolicEfficiencyFactor;
        
        // ==================== 使用完整的 Pandolf 模型（包括坡度项）====================
        // 始终使用完整的 Pandolf 模型：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
        // 坡度项 G·(0.23 + 1.34·V²) 已整合在公式中
        // 注意：由于几乎没有完全平地的地形，所以始终使用包含坡度的 Pandolf 模型
        
        // 游泳优化：未检测到动作输入时（海浪漂移）传零速度
        vector velocityForDrain = velocity;
        if (useSwimmingModel && !HasSwimInput())
            velocityForDrain = vector.Zero;

        // 获取坡度信息（模块化：使用 SpeedCalculator 计算坡度百分比）
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
        //! 休整恢复中若变为战场危险，先于移动策略终止休整（清锁定、取消战斗移动/掩护）；定时与体力 tick 同步
        SCR_RSS_AIStaminaBridge.TickAbortRestRecoveryIfBattlefieldDanger(owner);
        SCR_RSS_AIStaminaBridge.ApplyOnFootMovementPolicy(this, owner, staminaPercent);
        SCR_RSS_AIGroupRestCoordinator.TryScheduleGroupRestFromStamina(owner, staminaPercent);
        SCR_RSS_AIGroupRestCoordinator.TryCompleteGroupRestDefendWaypointIfReady(owner);
        SCR_RSS_AICoverSeeker.TickVerifyCombatCover(owner);

        if (Replication.IsServer() && !IsPlayerControlled())
            SCR_RSS_AIStaminaCombatEffects.ApplyStaminaToCombat(owner, staminaPercent);

        // 获取当前移动状态（用于计算冲刺倍数）
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();

        // ==================== 基础消耗率计算（模块化）====================
        // 模块化：使用 StaminaUpdateCoordinator 计算基础消耗率
        // 修复：传递环境因子参数，使基础消耗率计算支持环境因子
        // 性能优化：复用前面已获取的 velocity，避免重复 GetVelocity()
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
        
        // ==================== 体力消耗计算（模块化）====================
        // 游泳时不需要应用姿态、坡度、地形等修正（已在游泳模型中考虑）
        float postureMultiplier = 1.0;
        float gradePercentForConsumption = 0.0;
        float terrainFactorForConsumption = 1.0;
        
        if (!useSwimmingModel)
        {
            // 陆地移动：应用姿态、坡度、地形修正
            postureMultiplier = StaminaConsumptionCalculator.CalculatePostureMultiplier(currentSpeed, this);
            gradePercentForConsumption = gradePercent;
            terrainFactorForConsumption = terrainFactor;
        }
        
        float encumbranceStaminaDrainMultiplier = 1.0;
        if (m_pEncumbranceCache)
            encumbranceStaminaDrainMultiplier = m_pEncumbranceCache.GetStaminaDrainMultiplier();
        
        // 游泳时负重影响已在游泳模型中考虑，不需要额外应用
        if (useSwimmingModel)
            encumbranceStaminaDrainMultiplier = 1.0;
        
        // 已统一为 Pandolf 公式，不再使用 Sprint 倍数（保留参数兼容）
        float sprintMultiplier = 1.0;
        
        // [修复 v2.16.0] 初始化为预计算的基础消耗率，传入 CalculateStaminaConsumption。
        // 原来初始化为 0.0，导致 CalculateStaminaConsumption 内部的 fallback（<= 0.0 分支）
        // 总是触发，而该 fallback 路径会经过 AdjustEnergyForTemperature，
        // 在单位不匹配的情况下将几瓦的温度额外开销直接加到 0.000x 的体力/tick 上，造成数值爆炸。
        // 传入正确预计算值后，fallback 不会触发（若值 > 0），消除该问题。
        float baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        float totalDrainRate = 0.0;
        
        if (useSwimmingModel)
        {
            // 游泳模式：直接使用游泳消耗，只应用效率因子和疲劳因子
            totalDrainRate = baseDrainRateByVelocity;
            // 应用效率因子和疲劳因子
            totalDrainRate = totalDrainRate * totalEfficiencyFactor * fatigueFactor;
        }
        else
        {
            // 陆地模式：使用完整的消耗计算模块
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
            
            // 应用热应激倍数（影响体力消耗）
            float drainRateBeforeHeat = totalDrainRate;
            totalDrainRate = totalDrainRate * heatStressMultiplier;
            
        }
        
        // 调试信息：最终体力消耗率
        
        // 如果模块计算的基础消耗率为0，使用本地计算的baseDrainRateByVelocity
        if (baseDrainRateByVelocityForModule == 0.0 && baseDrainRateByVelocity > 0.0)
            baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
        // ==================== EPOC（过量耗氧）延迟检测（模块化）====================
        // 游泳时跳过 EPOC：避免水面漂移/抖动导致"停下→EPOC"误触发
        if (m_pEpocState && !useSwimmingModel)
        {
            float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
            bool isInEpocDelay = StaminaRecoveryCalculator.UpdateEpocDelay(
                m_pEpocState,
                currentSpeed,
                currentWorldTime);
        }
        
        // ==================== 统一调试批次：每秒启动一次 ====================
        if (owner == SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled() && IsPlayerControlled())
            StaminaConstants.StartDebugBatch();
        
        // ==================== 完全控制体力值（基于医学模型）====================
        // 模块化：使用 StaminaUpdateCoordinator 协调体力更新
        // timeDeltaSec 已在上面计算（基于 GetWorldTime 实际流逝时间）
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
            
            // 设置目标体力值（这会自动应用到体力组件）
            m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            m_fLastStaminaUpdateTime = currentTime;
            
            // 立即验证体力值是否被正确设置（检测原生系统干扰）
            // 注意：由于监控频率很高，这里可能不需要立即验证
            // 但保留此检查作为双重保险
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
            
            // 更新 staminaPercent 以反映新的目标值
            staminaPercent = newTargetStamina;
        }
        
        // ==================== UI信号桥接系统：更新官方UI特效和音效（模块化）====================
        // 通过 UISignalBridge 模块更新 "Exhaustion" 信号，让官方UI特效和音效响应自定义体力系统状态
        // 官方UI特效阈值：0.45，拟真模型崩溃点：0.25
        if (m_pUISignalBridge)
        {
            // 检查是否精疲力尽
            // 注意：isExhausted 已在函数作用域内声明
            m_pUISignalBridge.UpdateUISignal(staminaPercent, isExhausted, currentSpeed, totalDrainRate);
        }
        
        m_fLastStaminaPercent = staminaPercent;
        m_fLastSpeedMultiplier = finalSpeedMultiplier;

        // ==================== AI 调试数据收集（非玩家实体）====================
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
        
        // ==================== 调试输出与 HUD 实时更新 =====================
        // HUD：Hint 开启时每 50ms 更新，实现实时显示；调试：与批次同步（每 m_iDebugUpdateInterval）
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
                    // ETA 基于当前净率与当前体力，净率与 UpdateStaminaValue 一致（每 0.2s 率×5 为/秒）
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
                // 游泳时显示速度向量的俯仰角（正=上浮，负=下潜），陆地时显示地形坡度
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
        
        // 刷新调试批次（输出所有累积行）
        StaminaConstants.FlushDebugBatch();
        
        // 继续更新
        m_bRssStaminaLoopActive = true;
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
    }
    
    // 采集速度样本（每秒一次，仅玩家）
    void CollectSpeedSample()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return;
        
        // 只对本地控制的玩家处理
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        World world = GetGame().GetWorld();
        if (!world)
            return;
        
        // 获取当前速度（使用游戏引擎原生的 GetVelocity() 方法）
        vector velocity = GetVelocity();
        vector velocityXZ = vector.Zero;
        velocityXZ[0] = velocity[0];
        velocityXZ[2] = velocity[2];
        float speedHorizontal = velocityXZ.Length(); // 水平速度（米/秒）
        
        // 如果已有上一秒的数据，则显示上一秒的速度和状态
        if (m_bHasPreviousSpeed)
        {
            DisplayStatusInfo();
        }
        
        // 保存当前秒的速度作为"上一秒"（用于下次显示）
        m_fLastSecondSpeed = m_fCurrentSecondSpeed;
        m_fCurrentSecondSpeed = speedHorizontal;
        m_bHasPreviousSpeed = true; // 标记已有上一秒的数据
        
        // 继续下一秒的采样（仅调试开启时）
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }
    
    // 显示状态信息（包含速度、体力、速度倍数、移动类型、坡度等）
    // 模块化：使用 DebugDisplay 模块输出状态信息
    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        // 获取当前移动类型（游泳时显示 Swim）
        bool isSwimming = IsSwimmingByCommand();
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        
        // 模块化：使用 DebugDisplay 输出状态信息
        DebugDisplay.OutputStatusInfo(
            owner,
            m_fLastSecondSpeed,
            m_fLastStaminaPercent,
            m_fLastSpeedMultiplier,
            isSwimming,
            isSprinting,
            currentMovementPhase,
            this);
    }
    
    // 当物品从库存中移除时调用
    void OnItemRemovedFromInventory()
    {
        // 立即更新负重缓存
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.UpdateCache();
        }
    }
    
    // 当物品添加到库存中时调用
    void OnItemAddedToInventory()
    {
        // 立即更新负重缓存
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.UpdateCache();
        }
    }
    
    // 客户端请求服务器配置（连接/重连后推送到收到为止，收到后不再请求；配置启动后不变）
    void RequestServerConfig(bool forceBroadcast = false)
    {
        if (!Replication.IsServer())
        {
            if (SCR_RSS_ConfigManager.IsServerConfigApplied())
                return;

            // 尚未收到配置：记录首次请求时间与超时提示（重置仅在重连时由 MonitorNetworkConnection 调用）
            if (m_fFirstConfigRequestTime < 0.0)
                m_fFirstConfigRequestTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float elapsed = nowSec - m_fFirstConfigRequestTime;

            // 超时检测：超过30秒且重试次数达到上限，请求广播配置
            if (elapsed >= CONFIG_FETCH_TIMEOUT_SEC)
            {
                m_iConfigRetryCount++;

                if (m_fLastConfigTimeoutWarningTime < 0.0 || (nowSec - m_fLastConfigTimeoutWarningTime) >= CONFIG_FETCH_TIMEOUT_SEC)
                {
                    m_fLastConfigTimeoutWarningTime = nowSec;

                    if (m_iConfigRetryCount >= MAX_CONFIG_RETRY_COUNT)
                    {
                        PrintFormat("[RSS] 配置获取超时（重试 %1/%2），请求服务器广播配置", m_iConfigRetryCount, MAX_CONFIG_RETRY_COUNT);
                        // 强制请求广播配置（超时后使用广播兜底）
                        forceBroadcast = true;
                    }
                    else
                    {
                        PrintFormat("[RSS] 配置获取超时（重试 %1/%2），继续重试。若持续无响应请检查服务器或网络。", m_iConfigRetryCount, MAX_CONFIG_RETRY_COUNT);
                    }
                }
            }
            // 发送RPC请求服务器配置（必须用 Rpc() 发送，直接调用仅在本地执行）
            if (forceBroadcast)
            {
                // 超时后使用广播请求，避免点对点推送失败
                Rpc(RPC_ServerRequestBroadcastConfig);
            }
            else
            {
                Rpc(RPC_ServerRequestConfig);
            }
        }
    }

    // 客户端配置同步兜底循环：未应用服务器配置时按固定间隔重试，请求成功后自动停止。
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
    
    // RPC: 客户端请求服务器配置
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ServerRequestConfig()
    {
        if (!Replication.IsServer())
            return;

        // 服务器发送完整配置给请求的客户端（避免客户端只收到预设名称而丢失关键字段）
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

    // RPC: 客户端请求广播配置（超时兜底）
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ServerRequestBroadcastConfig()
    {
        if (!Replication.IsServer())
            return;

        // 服务器向所有客户端广播配置（超时兜底机制）
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
    
    // 心跳管理（客户端定时发送）
    void UpdateHeartbeat()
    {
        if (Replication.IsServer()) return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        // 定时发送心跳
        if (currentTime - m_fLastHeartbeatSendTime >= HEARTBEAT_INTERVAL)
        {
            m_fLastHeartbeatSendTime = currentTime;
            Rpc(RPC_ClientHeartbeat, GetGame().GetWorld().GetWorldTime() / 1000.0);
        }

        // 检测心跳超时（静默断开检测）
        if (m_fLastHeartbeatTime > 0.0 && (currentTime - m_fLastHeartbeatTime) > HEARTBEAT_TIMEOUT)
        {
            // 心跳超时：认为网络已断开
            if (m_bIsConnected)
            {
                m_bIsConnected = false;
                m_iPlayerId = 0;
                Print("[RSS] 心跳超时，判定网络已断开");
            }
        }
    }

    // RPC: 客户端向服务器上报体力与负重（周期性）
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientReportStamina(float staminaPercent, float weight, float clientTimestamp, bool isCriticalData)
    {
        if (Replication.IsServer())
        {
            if (!SCR_RSS_ConfigManager.GetSettings() || !SCR_RSS_ConfigManager.GetSettings().m_bDataExportEnabled)
                return;

            float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

            // 时间戳校验：检测异常延迟或时间回退
            if (clientTimestamp > 0.0)
            {
                float timestampDelta = currentTime - clientTimestamp;
                // 允许的最大往返延迟（秒），超过此值认为数据过期
                const float MAX_VALID_RTT = 2.0;
                if (timestampDelta > MAX_VALID_RTT)
                {
                    if (IsRssDebugEnabled())
                        PrintFormat("[RSS] Stale stamina report ignored (timestamp delta: %1s)", timestampDelta);
                    return;
                }
                else if (timestampDelta < -0.5)
                {
                    // 时间回退：可能客户端时钟异常
                    if (IsRssDebugEnabled())
                        PrintFormat("[RSS] Stale stamina report ignored (time regression: %1s)", timestampDelta);
                    return;
                }
            }

            // 速率限制：关键数据允许即时上报
            if (m_pNetworkSyncManager && !m_pNetworkSyncManager.AcceptClientReport(currentTime, isCriticalData))
            {
                if (IsRssDebugEnabled())
                    PrintFormat("[RSS] Ignored too-frequent stamina report (time=%1)", currentTime);
                return;
            }

            // 服务器端基础校验
            float clampedStamina = Math.Clamp(staminaPercent, 0.0, 1.0);

            // 记录并检测异常跳变
            if (m_pNetworkSyncManager)
            {
                float lastReported = m_pNetworkSyncManager.GetLastReportedStaminaPercent();
                if (Math.AbsFloat(clampedStamina - lastReported) > 0.5 && IsRssDebugEnabled())
                    PrintFormat("[RSS] Suspicious stamina jump reported: last=%1 -> reported=%2", lastReported, clampedStamina);

                // 更新服务器端记录（用于后续检测和统计）
                m_pNetworkSyncManager.UpdateReportedState(clampedStamina, weight);
            }

            // 使用服务器端的权威负重数据（优先）
            float serverWeight = 0.0;
            if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
                serverWeight = m_pEncumbranceCache.GetCurrentWeight();
            else
            {
                SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                    GetOwner().FindComponent(SCR_CharacterInventoryStorageComponent));
                if (inventoryComponent)
                    serverWeight = inventoryComponent.GetTotalWeight();
            }

            // 速度惩罚：优先使用 EncumbranceCache 的三段分段多项式（与客户端一致）
            float encPenalty = 0.0;
            if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            {
                encPenalty = m_pEncumbranceCache.GetSpeedPenalty();
            }
            else
            {
                float effectiveWeight = Math.Max(serverWeight - StaminaConstants.BASE_WEIGHT, 0.0);
                float bodyMassPercent = effectiveWeight / StaminaConstants.CHARACTER_WEIGHT;
                float ratio = Math.Clamp(bodyMassPercent, 0.0, 2.0);
                float rawPenalty = 0.0;
                if (ratio <= 0.3)
                {
                    rawPenalty = 0.15 * ratio;
                }
                else if (ratio <= 0.6)
                {
                    float segment = ratio - 0.3;
                    rawPenalty = 0.045 + 0.35 * Math.Pow(segment, 1.5);
                }
                else
                {
                    float segment = ratio - 0.6;
                    rawPenalty = 0.25 + 0.65 * (segment * segment);
                }
                float coeff = StaminaConstants.GetEncumbranceSpeedPenaltyCoeff();
                rawPenalty = rawPenalty * (coeff / 0.20);
                float max_pen = StaminaConstants.GetEncumbranceSpeedPenaltyMax();
                encPenalty = Math.Clamp(rawPenalty, 0.0, max_pen);
            }

            // 获取服务器端的移动状态（权威）
            bool isSprinting = IsSprinting();
            int currentMovementPhase = GetCurrentMovementPhase();
            bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(clampedStamina);
            bool canSprint = RealisticStaminaSpeedSystem.CanSprint(clampedStamina);

            // 计算当前速度（使用服务器位置差分）
            vector velocity = GetVelocity();
            vector horizontalVelocity = velocity;
            horizontalVelocity[1] = 0.0;
            float currentSpeed = horizontalVelocity.Length();
            currentSpeed = Math.Min(currentSpeed, 7.0);

            IEntity ownerEnt = GetOwner();
            bool shouldSuppressSlopeServer = false;
            if (m_pEnvironmentFactor && ownerEnt)
                shouldSuppressSlopeServer = m_pEnvironmentFactor.ShouldSuppressTerrainSlopeForEntity(ownerEnt);

            // 获取坡度角度（服务器端计算，传入 velocity 用于判断上下坡）
            // 室内（含楼梯间宽松判定）时硬归零，确保权威校验路径不吃坡度限速
            float slopeAngleDegrees = 0.0;
            if (!shouldSuppressSlopeServer)
                slopeAngleDegrees = SpeedCalculator.GetSlopeAngle(this, m_pEnvironmentFactor, velocity);

            // 室内楼梯：与 UpdateSpeed 一致，减轻负重对速度的惩罚
            float rawSlopeServer = SpeedCalculator.GetRawSlopeAngle(this, velocity);
            bool isIndoorStairsServer = (shouldSuppressSlopeServer && Math.AbsFloat(rawSlopeServer) > 0.0);
            if (isIndoorStairsServer)
                encPenalty = encPenalty * StaminaConstants.GetIndoorStairsEncumbranceSpeedFactor();

            // 使用权威计算函数生成最终速度倍数（传入冲刺开始时间以保持战术爆发期一致）
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
                // 使用外部已声明的 currentTime 以避免重复声明
                currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

                // 初次验证立即下发，之后仅在偏差持续超时/显著时下发以减少带宽
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
                        // 未达到触发阈值：暂不下发（保持现有客户端平滑逻辑）
                    }
                }
            }
        }
    }

    // RPC: 服务器将验证后的速度同步回客户端（目标客户端）
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerSyncSpeedMultiplier(float speedMultiplier, float serverTimestamp)
    {
        if (!Replication.IsServer())
        {
            if (m_pNetworkSyncManager)
            {
                // 时间戳校验：检查数据是否过期
                if (serverTimestamp > 0.0)
                {
                    float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
                    float timestampDelta = currentTime - serverTimestamp;

                    // 允许的最大单向延迟（秒），超过此值认为验证值已过期
                    const float MAX_VALID_ONE_WAY_LATENCY = 1.0;
                    if (timestampDelta > MAX_VALID_ONE_WAY_LATENCY)
                    {
                        if (IsRssDebugEnabled())
                            PrintFormat("[RSS] Stale speed validation ignored (latency: %1s)", timestampDelta);
                        return; // 拒绝应用过期的验证值
                    }
                }

                m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(speedMultiplier);
            }
        }
    }

    // RPC: 客户端心跳（服务器端检测超时断开）
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientHeartbeat(int clientTimestamp)
    {
        if (Replication.IsServer())
        {
            // 服务器端记录心跳时间（用于后续超时检测）
            m_fLastHeartbeatTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

            // 心跳响应：回传服务器时间戳用于客户端延迟计算
            Rpc(RPC_ServerHeartbeatAck, GetGame().GetWorld().GetWorldTime() / 1000.0);
        }
    }

    // RPC: 服务器心跳确认（客户端计算往返延迟）
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerHeartbeatAck(float serverTimestamp)
    {
        if (!Replication.IsServer())
        {
            float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float rtt = currentTime - serverTimestamp;

            // 更新心跳时间
            m_fLastHeartbeatTime = currentTime;

            // 可选：输出延迟信息用于调试
            if (IsRssDebugEnabled() && rtt > 0.5)
                PrintFormat("[RSS] High latency detected (RTT: %1s)", rtt);
        }
    }

    // ==================== 外部模组 API（SCR_RSS_API 调用）====================
    // 检查 RSS 数据是否已初始化（体力组件、环境因子等）
    bool HasRssData()
    {
        return (m_pStaminaComponent != null && m_pEnvironmentFactor != null);
    }

    float GetRssStaminaPercent()
    {
        if (m_pStaminaComponent)
            return Math.Clamp(m_pStaminaComponent.GetTargetStamina(), 0.0, 1.0);
        return 1.0;
    }

    float GetRssSpeedMultiplier()
    {
        return m_fLastSpeedMultiplier;
    }

    float GetRssCurrentSpeed()
    {
        vector velocity = GetVelocity();
        vector horizontalVelocity = velocity;
        horizontalVelocity[1] = 0.0;
        float speed = horizontalVelocity.Length();
        return Math.Min(speed, 7.0);
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
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            return m_pEncumbranceCache.GetCurrentWeight();
        if (m_pCachedInventoryComponent)
            return m_pCachedInventoryComponent.GetTotalWeight();
        return 0.0;
    }

    EnvironmentFactor GetRssEnvironmentFactor()
    {
        return m_pEnvironmentFactor;
    }
    
    // 动态获取引擎原始最大速度（Run模式）
    float GetOriginalEngineMaxSpeed_Run()
    {
        return GetDynamicOriginalEngineMaxSpeed(2);
    }
    
    // 动态获取引擎原始最大速度（Sprint模式）
    float GetOriginalEngineMaxSpeed_Sprint()
    {
        return GetDynamicOriginalEngineMaxSpeed(3);
    }
    
    // 内部方法：动态获取引擎原始最大速度
    protected float GetDynamicOriginalEngineMaxSpeed(int movementPhase)
    {
        if (!m_pAnimComponent)
        {
            if (movementPhase == 3)
                return RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            else
                return RealisticStaminaSpeedSystem.GAME_MAX_SPEED * RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        }
        
        // 临时保存当前的速度倍数
        float currentMultiplier = m_fLastSpeedMultiplier;
        
        // 临时重置为1.0以获取真实的引擎原始速度
        OverrideMaxSpeed(1.0);
        
        // 获取真实的引擎原始速度
        float realOriginalSpeed = m_pAnimComponent.GetMaxSpeed(1.0, 0.0, movementPhase);
        
        // 恢复原来的速度倍数
        OverrideMaxSpeed(currentMultiplier);
        
        return realOriginalSpeed;
    }

}
