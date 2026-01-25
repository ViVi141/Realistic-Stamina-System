// Realistic Stamina System (RSS) - v2.14.0
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
    
    // 速度更新相关
    protected const int SPEED_UPDATE_INTERVAL_MS = 50; // 每0.05秒更新一次速度
    
    // 状态信息缓存
    protected float m_fLastStaminaPercent = 1.0;
    protected float m_fLastSpeedMultiplier = 1.0;
    protected SCR_CharacterStaminaComponent m_pStaminaComponent; // 体力组件引用
    
    // ==================== "撞墙"阻尼过渡模块 ====================
    // 模块化拆分：使用独立的 CollapseTransition 类管理"撞墙"临界点的5秒阻尼过渡逻辑
    protected ref CollapseTransition m_pCollapseTransition;
    
    // ==================== 运动持续时间跟踪模块 ====================
    // 模块化拆分：使用独立的 ExerciseTracker 类管理运动/休息时间跟踪
    protected ref ExerciseTracker m_pExerciseTracker;
    
    // ==================== 跳跃和翻越检测模块 ====================
    // 模块化拆分：使用独立的 JumpVaultDetector 类管理跳跃和翻越检测逻辑
    protected ref JumpVaultDetector m_pJumpVaultDetector;
    
    // ==================== 负重缓存管理模块 ====================
    // 模块化拆分：使用独立的 EncumbranceCache 类管理负重缓存
    protected ref EncumbranceCache m_pEncumbranceCache;
    
    
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
    // 因此通过“位置差分/时间步长”计算速度向量，作为消耗模型的速度输入
    protected bool m_bHasLastPositionSample = false;
    protected vector m_vLastPositionSample = vector.Zero;
    protected vector m_vComputedVelocity = vector.Zero; // 上次更新周期计算得到的速度（m/s）
    
    // ==================== 游泳状态缓存（用于调试显示）====================
    protected CompartmentAccessComponent m_pCompartmentAccess;
    protected CharacterAnimationComponent m_pAnimComponent;
    
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
            m_pExerciseTracker.Initialize(GetGame().GetWorld().GetWorldTime()); 
        
        // 初始化"撞墙"阻尼过渡模块
        m_pCollapseTransition = new CollapseTransition();
        if (m_pCollapseTransition)
            m_pCollapseTransition.Initialize();
        
        // 初始化地形检测模块
        m_pTerrainDetector = new TerrainDetector();
        if (m_pTerrainDetector)
            m_pTerrainDetector.Initialize();
        
        // 初始化环境因子模块
        m_pEnvironmentFactor = new EnvironmentFactor();
        if (m_pEnvironmentFactor)
        {
            World world = GetGame().GetWorld();
            if (world)
                m_pEnvironmentFactor.Initialize(world, owner);
        }
        
        // 初始化疲劳积累系统模块
        m_pFatigueSystem = new FatigueSystem();
        if (m_pFatigueSystem)
        {
            float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
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
                m_pEncumbranceCache.Initialize(inventoryComponent);
            }
        }
        
        // 初始化UI信号桥接模块
        m_pUISignalBridge = new UISignalBridge();
        if (m_pUISignalBridge)
            m_pUISignalBridge.Init(owner);
        
        // 初始化EPOC状态管理
        m_pEpocState = new EpocState();
        
        // 缓存组件，避免在每0.2s的更新循环中重复查找
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            m_pCompartmentAccess = character.GetCompartmentAccessComponent();
            m_pAnimComponent = character.GetAnimationComponent();
        }
        
        // 延迟初始化，确保组件完全加载
        GetGame().GetCallqueue().CallLater(StartSystem, 500, false);
    }
    
    
    // 启动系统
    void StartSystem()
    {
        // 启动速度更新循环（每0.2秒更新一次速度）
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
        
        // 启动速度采集循环（每秒一次）
        GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
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
    
    // ==================== 改进：使用动作监听器检测跳跃输入 ====================
    // 采用多重检测方法确保准确性：
    // 1. 动作监听器（OnJumpActionTriggered）- 实时检测输入动作
    // 2. OnPrepareControls - 备用检测方法，每帧检测输入动作
    
    // 覆盖 OnControlledByPlayer 来添加/移除动作监听器
    override void OnControlledByPlayer(IEntity owner, bool controlled)
    {
        super.OnControlledByPlayer(owner, controlled);
        
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
                Print("[RealisticSystem] 跳跃动作监听器已添加 / Jump Action Listener Added");
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
            Print("[RealisticSystem] 动作监听器检测到跳跃输入！/ Action Listener Detected Jump Input!");
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
            Print("[RealisticSystem] OnPrepareControls 检测到跳跃输入！/ OnPrepareControls Detected Jump Input!");
        }
    }
    
    // 根据体力更新速度（每0.2秒调用一次）
    void UpdateSpeedBasedOnStamina()
    {
        IEntity owner = GetOwner();
        if (!owner)
        {
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
            return;
        }
        
        // 只对本地控制的玩家处理
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
        {
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
            return;
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
                if (m_pStaminaComponent)
                {
                    float currentStamina = m_pStaminaComponent.GetTargetStamina();
                    PrintFormat("[RealisticSystem] 载具中 / In Vehicle: 体力=%1%% | Stamina=%1%%", 
                        Math.Round(currentStamina * 100.0).ToString());
                }
            }
            
            // 在载具中可以恢复体力（静止状态）
            if (m_pStaminaComponent)
            {
                float currentStamina = m_pStaminaComponent.GetTargetStamina();
                if (currentStamina < 1.0)
                {
                    // 计算恢复率（使用标准恢复模型）
                    float restDurationMinutes = 0.0; // 载具中视为静止，但恢复时间较短
                    float exerciseDurationMinutes = 0.0; // 无运动累积疲劳
                    float currentWeightForRecovery = 0.0; // 载具中负重不影响恢复
                    float baseDrainRateByVelocity = 0.0; // 无消耗
                    float recoveryRate = StaminaRecoveryCalculator.CalculateRecoveryRate(
                        currentStamina,
                        restDurationMinutes,
                        exerciseDurationMinutes,
                        currentWeightForRecovery,
                        baseDrainRateByVelocity,
                        false, // 不禁用正向恢复
                        0, // 站立姿态
                        m_pEnvironmentFactor, // v2.15.0：传递环境因子模块
                        0.0); // 载具中视为静止，currentSpeed为0.0
                    
                    // 更新体力值
                    float newStamina = Math.Clamp(currentStamina + recoveryRate, 0.0, 1.0);
                    m_pStaminaComponent.SetTargetStamina(newStamina);
                    
                    // 调试信息：载具中体力恢复
                    if (vehicleDebugCounter == 0)
                    {
                        PrintFormat("[RealisticSystem] 载具中恢复 / Vehicle Recovery: %1%% → %2%% (恢复率: %3/0.2s) | %1%% → %2%% (Rate: %3/0.2s)",
                            Math.Round(currentStamina * 100.0).ToString(),
                            Math.Round(newStamina * 100.0).ToString(),
                            recoveryRate.ToString());
                    }
                }
            }
            
            // 继续调度下一次更新，但不进行速度更新和体力消耗计算
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
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
        
        // ==================== 精疲力尽逻辑（融合模型）====================
        // 如果体力 ≤ 0，强制速度为跛行速度（1.0 m/s）
        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);
        static bool lastExhaustedState = false;
        if (isExhausted)
        {
            // 精疲力尽：强制速度为跛行速度（1.0 m/s）
            float limpSpeedMultiplier = RealisticStaminaSpeedSystem.EXHAUSTION_LIMP_SPEED / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            OverrideMaxSpeed(limpSpeedMultiplier);
            
            // 调试信息：精疲力尽状态
            if (!lastExhaustedState)
            {
                Print("[RealisticSystem] 精疲力尽 / Exhausted: 速度限制为跛行速度 | Speed Limited to Limp Speed");
                lastExhaustedState = true;
            }
            
            // 继续执行后续逻辑（但速度已被限制）
            // 注意：即使精疲力尽，仍然需要更新体力值（恢复）
        }
        else
        {
            if (lastExhaustedState)
            {
                Print("[RealisticSystem] 脱离精疲力尽状态 / Recovered from Exhaustion: 速度恢复正常 | Speed Restored");
                lastExhaustedState = false;
            }
        }
        
        // ==================== 性能优化：使用缓存的负重值（模块化）====================
        // 检查并更新负重缓存（仅在变化时重新计算）
        if (m_pEncumbranceCache)
            m_pEncumbranceCache.CheckAndUpdate();
        
        // 使用缓存的速度惩罚（避免重复计算）
        float encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
            encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenalty();
        
        // ==================== 获取当前实际速度（m/s）====================
        // 说明：游泳命令通过 PrePhys_SetTranslation 直接改变位移，GetVelocity() 可能为 0
        // 解决：使用位置差分测速（每 0.2 秒一次），得到更可靠的速度向量
        // 模块化：使用 StaminaUpdateCoordinator 计算速度
        float dtSeconds = SPEED_UPDATE_INTERVAL_MS * 0.001;
        SpeedCalculationResult speedResult = StaminaUpdateCoordinator.CalculateCurrentSpeed(
            owner,
            m_vLastPositionSample,
            m_bHasLastPositionSample,
            m_vComputedVelocity,
            dtSeconds);
        float currentSpeed = speedResult.currentSpeed;
        m_vLastPositionSample = speedResult.lastPositionSample;
        m_bHasLastPositionSample = speedResult.hasLastPositionSample;
        m_vComputedVelocity = speedResult.computedVelocity;
        
        // ==================== 速度计算和更新（模块化）====================
        // 模块化：使用 StaminaUpdateCoordinator 更新速度
        float finalSpeedMultiplier = StaminaUpdateCoordinator.UpdateSpeed(
            this,
            staminaPercent,
            encumbranceSpeedPenalty,
            m_pCollapseTransition,
            currentSpeed);
        
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
        static int speedDebugCounter = 0;
        speedDebugCounter++;
        if (speedDebugCounter >= 25) // 每5秒输出一次
        {
            speedDebugCounter = 0;
            PrintFormat("[RealisticSystem] 速度计算 / Speed Calculation: 当前速度=%1 m/s | Current Speed=%1 m/s",
                Math.Round(currentSpeed * 10.0) / 10.0);
        }
        
        // ==================== 性能优化：使用缓存的当前重量（模块化）====================
        // 使用缓存的当前重量（避免重复查找组件）
        float currentWeight = 0.0;
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            currentWeight = m_pEncumbranceCache.GetCurrentWeight();
        else
        {
            SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                owner.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (inventoryComponent)
                currentWeight = inventoryComponent.GetTotalWeight();
        }
        
        // ==================== 检测游泳状态（游泳体力管理）====================
        // 模块化：使用 SwimmingStateManager 管理游泳状态和湿重
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
        bool isSwimming = SwimmingStateManager.IsSwimming(this);
        
        // 运动/休息时间跟踪（用于地形检测和疲劳计算）
        float currentTimeForExercise = GetGame().GetWorld().GetWorldTime(); 
        
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
        // 注意：使用上面已声明的 currentTimeForExercise
        float terrainFactor = 1.0; // 默认值（铺装路面）
        if (m_pTerrainDetector)
        {
            terrainFactor = m_pTerrainDetector.GetTerrainFactor(owner, currentTimeForExercise, currentSpeed);
        }
        
        // ==================== 环境因子更新（模块化）====================
        // 每5秒更新一次环境因子（性能优化）
        // 传入角色实体用于室内检测，传入速度向量用于风阻计算，传入地形系数用于泥泞计算，传入游泳湿重用于总湿重计算
        if (m_pEnvironmentFactor)
        {
            m_pEnvironmentFactor.UpdateEnvironmentFactors(currentTime, owner, m_vComputedVelocity, terrainFactor, m_fCurrentWetWeight);
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
            if (jumpCost > 0.0)
            {
                PrintFormat("[RealisticSystem] 跳跃消耗 / Jump Cost: -%1%% | -%1%%", 
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
            if (vaultCost > 0.0)
            {
                PrintFormat("[RealisticSystem] 翻越消耗 / Vault Cost: -%1%% | -%1%%", 
                    Math.Round(vaultCost * 100.0 * 10.0) / 10.0);
            }
            staminaPercent = staminaPercent - vaultCost;
            
            // 更新冷却时间（每0.2秒调用一次）
            m_pJumpVaultDetector.UpdateCooldowns();
            
            // ==================== 姿态转换处理（模块化）====================
            // 更新疲劳堆积（每0.2秒调用一次）
            m_pStanceTransitionManager.UpdateFatigue(0.2);
            
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
        // GAME_MAX_SPEED = 5.2 m/s（游戏最大速度）
        float speedRatio = Math.Clamp(currentSpeed / 5.2, 0.0, 1.0);
        
        // ==================== 融合模型：基于速度阈值的分段消耗率 + 多维度特性 ====================
        // 融合两种模型：
        // 1. 基于速度阈值的分段消耗率（Military Stamina System Model）
        // 2. 多维度特性（健康状态、累积疲劳、代谢适应）
        // 
        // 基础消耗率基于速度阈值：
        // - Sprint (V ≥ 5.2 m/s): 0.480 pts/s
        // - Run (3.7 ≤ V < 5.2 m/s): 0.105 pts/s
        // - Walk (3.2 ≤ V < 3.7 m/s): 0.060 pts/s
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
            // 更新运动/休息时间跟踪
            m_pExerciseTracker.Update(currentTimeForExercise, isCurrentlyMoving);
            
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
        
        // 获取坡度信息（模块化：使用 SpeedCalculator 计算坡度百分比）
        float slopeAngleDegrees = 0.0; // 初始化坡度角度
        GradeCalculationResult gradeResult = SpeedCalculator.CalculateGradePercent(
            this,
            currentSpeed,
            m_pJumpVaultDetector,
            slopeAngleDegrees);
        float gradePercent = gradeResult.gradePercent;
        slopeAngleDegrees = gradeResult.slopeAngleDegrees;
        
        // ==================== 基础消耗率计算（模块化）====================
        // 模块化：使用 StaminaUpdateCoordinator 计算基础消耗率
        BaseDrainRateResult drainRateResult = StaminaUpdateCoordinator.CalculateBaseDrainRate(
            useSwimmingModel,
            currentSpeed,
            currentWeight,
            currentWeightWithWet,
            gradePercent,
            terrainFactor,
            m_vComputedVelocity,
            m_bSwimmingVelocityDebugPrinted,
            owner);
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
        
        // 获取当前移动状态（用于计算冲刺倍数）
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        
        float sprintMultiplier = 1.0;
        if (!useSwimmingModel && (isSprinting || currentMovementPhase == 3))
            sprintMultiplier = StaminaConstants.GetSprintStaminaDrainMultiplier();
        
        // 调试信息：体力消耗计算参数（每5秒输出一次）
        static int drainDebugCounter = 0;
        drainDebugCounter++;
        if (drainDebugCounter >= 25) // 每5秒输出一次
        {
            drainDebugCounter = 0;
            string movementTypeDebug = "";
            if (useSwimmingModel)
                movementTypeDebug = "游泳 / Swimming";
            else
                movementTypeDebug = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
            PrintFormat("[RealisticSystem] 体力消耗参数 / Stamina Consumption Params: 类型=%1 | 速度=%2 m/s | 负重=%3kg | 坡度=%4%% | 地形系数=%5 | 姿态=%6x | 热应激=%7x | 效率=%8 | 疲劳=%9 | Sprint倍数=%10x",
                movementTypeDebug,
                Math.Round(currentSpeed * 10.0) / 10.0,
                Math.Round(currentWeight * 10.0) / 10.0,
                Math.Round(gradePercentForConsumption * 10.0),
                Math.Round(terrainFactorForConsumption * 100.0) / 100.0,
                Math.Round(postureMultiplier * 100.0) / 100.0,
                Math.Round(heatStressMultiplier * 100.0) / 100.0,
                Math.Round(totalEfficiencyFactor * 100.0) / 100.0,
                Math.Round(fatigueFactor * 100.0) / 100.0,
                Math.Round(sprintMultiplier * 100.0) / 100.0);
        }
        
        float baseDrainRateByVelocityForModule = 0.0; // 用于模块计算的基础消耗率
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
                owner); // v2.15.0：传递角色实体，用于手持物品检测
            
            // 应用热应激倍数（影响体力消耗）
            float drainRateBeforeHeat = totalDrainRate;
            totalDrainRate = totalDrainRate * heatStressMultiplier;
            
            // 调试信息：热应激影响
            if (drainDebugCounter == 0 && heatStressMultiplier > 1.01)
            {
                PrintFormat("[RealisticSystem] 热应激影响 / Heat Stress Effect: 消耗率 %1 → %2 (倍数: %3x) | Drain Rate %1 → %2 (Multiplier: %3x)",
                    Math.Round(drainRateBeforeHeat * 1000000.0).ToString(),
                    Math.Round(totalDrainRate * 1000000.0).ToString(),
                    Math.Round(heatStressMultiplier * 100.0) / 100.0);
            }
        }
        
        // 调试信息：最终体力消耗率
        if (drainDebugCounter == 0)
        {
            PrintFormat("[RealisticSystem] 最终体力消耗率 / Final Stamina Drain Rate: %1/0.2s | %1/0.2s",
                Math.Round(totalDrainRate * 1000000.0) / 1000000.0);
        }
        
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
        
        // ==================== 完全控制体力值（基于医学模型）====================
        // 模块化：使用 StaminaUpdateCoordinator 协调体力更新
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
                m_pEnvironmentFactor); // v2.14.0：传递环境因子模块
            
            // 设置目标体力值（这会自动应用到体力组件）
            m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            
            // 立即验证体力值是否被正确设置（检测原生系统干扰）
            // 注意：由于监控频率很高，这里可能不需要立即验证
            // 但保留此检查作为双重保险
            float verifyStamina = m_pStaminaComponent.GetStamina();
            if (Math.AbsFloat(verifyStamina - newTargetStamina) > 0.005) // 如果偏差超过0.5%（降低阈值，更敏感）
            {
                // 检测到原生系统干扰，强制纠正
                PrintFormat("[RealisticSystem] 检测到原生系统干扰 / Native System Interference Detected: 目标=%1%% | 实际=%2%% | 偏差=%3%% | Target=%1%% | Actual=%2%% | Diff=%3%%",
                    Math.Round(newTargetStamina * 100.0).ToString(),
                    Math.Round(verifyStamina * 100.0).ToString(),
                    Math.Round(Math.AbsFloat(verifyStamina - newTargetStamina) * 10000.0) / 100.0);
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
        
        // ==================== 调试输出（模块化）====================
        // 每5秒输出一次完整调试信息，避免过多日志，仅在客户端
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            static int debugCounter = 0;
            debugCounter++;
            if (debugCounter >= 25) // 200ms * 25 = 5秒
            {
                debugCounter = 0;
                
                // 获取负重信息用于调试
                ChimeraCharacter characterForDebug = ChimeraCharacter.Cast(owner);
                float combatEncumbrancePercent = 0.0;
                float debugCurrentWeight = 0.0;
                if (characterForDebug)
                {
                    SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(characterForDebug.FindComponent(SCR_CharacterInventoryStorageComponent));
                    if (characterInventory)
                    {
                        debugCurrentWeight = characterInventory.GetTotalWeight();
                        combatEncumbrancePercent = RealisticStaminaSpeedSystem.CalculateCombatEncumbrancePercent(owner);
                    }
                }
                
                // 获取移动类型字符串（模块化）
                string movementTypeStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
                
                // 输出完整调试信息（模块化）
                DebugInfoParams debugParams = new DebugInfoParams();
                debugParams.owner = owner;
                debugParams.movementTypeStr = movementTypeStr;
                debugParams.staminaPercent = staminaPercent;
                debugParams.baseSpeedMultiplier = baseSpeedMultiplier;
                debugParams.encumbranceSpeedPenalty = encumbranceSpeedPenalty;
                debugParams.finalSpeedMultiplier = finalSpeedMultiplier;
                debugParams.gradePercent = gradePercent;
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
                DebugDisplay.OutputDebugInfo(debugParams);
                
                // 输出屏幕 Hint 信息（独立于控制台日志）
                DebugDisplay.OutputHintInfo(debugParams);
            }
        }
        
        // 继续更新
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
    }
    
    // 采集速度样本（每秒一次）
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
        
        // 获取当前速度（使用位置差分测速缓存）
        bool isSwimming = IsSwimmingByCommand();
        vector velForDisplay = m_vComputedVelocity;
        float speedHorizontal = 0.0;

        if (isSwimming)
        {
            speedHorizontal = velForDisplay.Length();
        }
        else
        {
            vector velocityXZ = vector.Zero;
            velocityXZ[0] = velForDisplay[0];
            velocityXZ[2] = velForDisplay[2];
            speedHorizontal = velocityXZ.Length(); // 水平速度（米/秒）
        }
        
        // 如果已有上一秒的数据，则显示上一秒的速度和状态
        if (m_bHasPreviousSpeed)
        {
            DisplayStatusInfo();
        }
        
        // 保存当前秒的速度作为"上一秒"（用于下次显示）
        m_fLastSecondSpeed = m_fCurrentSecondSpeed;
        m_fCurrentSecondSpeed = speedHorizontal;
        m_bHasPreviousSpeed = true; // 标记已有上一秒的数据
        
        // 继续下一秒的采样
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
    
}
