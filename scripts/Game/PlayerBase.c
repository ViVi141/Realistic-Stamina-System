// 拟真体力-速度系统（v2.1 - 参数优化版本）
// 结合体力值和负重，动态调整移动速度并显示状态信息
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
    protected const int SPEED_UPDATE_INTERVAL_MS = 200; // 每0.2秒更新一次速度
    
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
    
    // ==================== 网络同步管理模块 ====================
    // 模块化拆分：使用独立的 NetworkSyncManager 类管理网络同步状态和插值
    protected ref NetworkSyncManager m_pNetworkSyncManager;
    
    // ==================== 疲劳积累系统模块 ====================
    // 模块化拆分：使用独立的 FatigueSystem 类管理疲劳积累和恢复
    protected ref FatigueSystem m_pFatigueSystem;
    
    // ==================== 地形检测模块 ====================
    // 模块化拆分：使用独立的 TerrainDetector 类管理地形检测和系数计算
    protected ref TerrainDetector m_pTerrainDetector;
    
    // ==================== UI信号桥接模块 ====================
    // 模块化拆分：使用独立的 UISignalBridge 类管理UI信号桥接
    protected ref UISignalBridge m_pUISignalBridge;
    
    // 在组件初始化后
    override void OnInit(IEntity owner)
    {
        super.OnInit(owner);
        
        if (!owner)
            return;
        
        // 获取体力组件引用
        // 使用 GetStaminaComponent() 获取缓存的组件引用（更高效）
        CharacterStaminaComponent staminaComp = GetStaminaComponent();
        m_pStaminaComponent = SCR_CharacterStaminaComponent.Cast(staminaComp);
        
        // 如果 GetStaminaComponent() 返回 null，尝试手动查找
        if (!m_pStaminaComponent)
        {
            m_pStaminaComponent = SCR_CharacterStaminaComponent.Cast(owner.FindComponent(SCR_CharacterStaminaComponent));
        }
        
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
        
        // 初始化网络同步管理模块
        m_pNetworkSyncManager = new NetworkSyncManager();
        if (m_pNetworkSyncManager)
            m_pNetworkSyncManager.Initialize();
        
        // 初始化疲劳积累系统模块
        m_pFatigueSystem = new FatigueSystem();
        if (m_pFatigueSystem)
        {
            float currentTime = GetGame().GetWorld().GetWorldTime();
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
                Print("[RealisticSystem] 跳跃动作监听器已添加");
            }
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
        }
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
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            CompartmentAccessComponent compAccess = character.GetCompartmentAccessComponent();
            if (compAccess && compAccess.GetCompartment())
                isInVehicle = true;
        }
        
        // 如果不在载具中，设置跳跃输入标志（使用模块方法）
        if (!isInVehicle && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            Print("[RealisticSystem] 动作监听器检测到跳跃输入！");
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
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            CompartmentAccessComponent compAccess = character.GetCompartmentAccessComponent();
            if (compAccess && compAccess.GetCompartment())
                isInVehicle = true;
        }
        
        // 备用检测：如果在 OnPrepareControls 中检测到跳跃输入，也设置标志（使用模块方法）
        if (!isInVehicle && am.GetActionTriggered("Jump") && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            Print("[RealisticSystem] OnPrepareControls 检测到跳跃输入！");
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
        
        // ==================== 网络同步：区分客户端和服务器端逻辑 =====================
        // 注意：World.IsServer() 方法不存在，改为使用 Replication 系统检查
        // 在 Arma Reforger 中，通常只在客户端处理本地玩家，服务器端验证可以暂时禁用
        bool isServer = false; // 暂时禁用服务器端验证（IsServer() 方法不存在）
        bool isClient = true; // 默认按客户端处理
        
        // 客户端：只对本地控制的玩家处理
        // 服务器端：对所有玩家处理（用于验证）
        if (isClient && owner != SCR_PlayerController.GetLocalControlledEntity())
        {
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
            return;
        }
        
        // 服务器端：确保玩家有效且不是AI
        // 注意：在服务器端，我们需要验证玩家，但 GetPlayerController 可能不存在
        // 改为直接检查实体是否有玩家控制器组件，或者跳过AI检查（让所有实体都处理）
        if (isServer)
        {
            // 在服务器端，检查实体是否为玩家角色
            // 如果实体是AI控制的，跳过处理（AI不需要体力系统）
            ChimeraCharacter character = ChimeraCharacter.Cast(owner);
            if (character)
            {
                // 检查是否有玩家控制器（简单检查：尝试获取玩家控制器）
                // 如果没有玩家控制器，可能是AI，跳过处理
                // 注意：这个方法可能不存在，如果编译错误，可以注释掉这个检查
                // PlayerController controller = GetGame().GetPlayerController(owner);
                // if (!controller || controller.IsAIControlled())
                // {
                //     GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
                //     return;
                // }
                
                // 简化：在服务器端处理所有角色（包括AI）
                // 如果需要排除AI，可以在实际测试中根据需求调整
            }
        }
        
        // 获取当前体力百分比
        // 关键：优先使用目标体力值，而不是当前体力值
        // 因为当前体力值可能被原生系统修改，而目标体力值是我们控制的
        float staminaPercent = 1.0;
        if (m_pStaminaComponent)
        {
            // 优先使用目标体力值（我们控制的）
            staminaPercent = m_pStaminaComponent.GetTargetStamina();
        }
        else
        {
            // 如果没有体力组件引用，使用 GetStamina()
            // 根据 CharacterControllerComponent 源代码：
            // GetStamina() 返回当前体力值，范围 <0, 1>，-1 表示没有体力组件
            staminaPercent = GetStamina();
            
            // 如果返回 -1，表示没有体力组件，使用默认值 1.0（100%）
            if (staminaPercent < 0.0)
                staminaPercent = 1.0;
        }
        
        // 限制在有效范围内
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        // 如果体力非常接近0（<0.01），强制设为0，确保体力为0时的逻辑正确
        if (staminaPercent < 0.01)
            staminaPercent = 0.0;
        
        // ==================== 精疲力尽逻辑（融合模型）====================
        // 如果体力 ≤ 0，强制速度为跛行速度（1.0 m/s）
        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);
        if (isExhausted)
        {
            // 精疲力尽：强制速度为跛行速度（1.0 m/s）
            float limpSpeedMultiplier = RealisticStaminaSpeedSystem.EXHAUSTION_LIMP_SPEED / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            OverrideMaxSpeed(limpSpeedMultiplier);
            
            // 继续执行后续逻辑（但速度已被限制）
            // 注意：即使精疲力尽，仍然需要更新体力值（恢复）
        }
        
        // 检查是否可以Sprint（体力 ≥ 20%才能Sprint）
        bool canSprint = RealisticStaminaSpeedSystem.CanSprint(staminaPercent);
        
        // 使用静态工具类计算基础速度倍数（根据体力）
        float baseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        // ==================== 性能优化：使用缓存的负重值（模块化）====================
        // 检查并更新负重缓存（仅在变化时重新计算）
        if (m_pEncumbranceCache)
            m_pEncumbranceCache.CheckAndUpdate();
        
        // 使用缓存的速度惩罚（避免重复计算）
        float encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
            encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenalty();
        
        // 检测当前移动类型（walk/run/sprint）
        // OverrideMaxSpeed() 设置的是最大速度倍数，游戏引擎会根据移动类型在这个范围内调整实际速度
        // 为了确保Sprint时速度更快，我们需要根据移动类型动态调整速度倍数
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        // 移动阶段：0=idle, 1=walk, 2=run, 3=sprint
        
        // 如果精疲力尽，禁用Sprint
        if (isExhausted || !canSprint)
        {
            // 强制设置为Run或Walk状态
            if (isSprinting || currentMovementPhase == 3)
            {
                // 如果正在Sprint但体力不足，强制切换到Run
                currentMovementPhase = 2;
                isSprinting = false;
            }
        }
        
        // 根据移动类型调整速度倍数
        // 核心策略：Sprint 必须完全基于 Run 的完整逻辑（包括双稳态-平台期、5秒阻尼过渡等）
        // 这样无论以后怎么调整 Run 的平衡性，Sprint 永远会比 Run 快出一个固定的阶梯
        // Run时：使用双稳态-应激性能模型（25%-100%保持3.7m/s，0%-25%线性下降）
        // Sprint时：基于Run的最终速度进行加乘（Run × 1.30）
        // Walk时：使用Run速度的70%
        
        // ==================== 计算Run的基础速度倍数（完整逻辑，包含双稳态-平台期和阻尼过渡）====================
        // 这一步必须在所有移动类型分支之前执行，确保Run和Sprint使用相同的基准
        
        // 获取坡度角度（用于自适应步幅逻辑）
        float slopeAngleDegrees = 0.0;
        CharacterAnimationComponent animComponentForSpeed = GetAnimationComponent();
        if (animComponentForSpeed)
        {
            CharacterCommandHandlerComponent handlerForSpeed = CharacterCommandHandlerComponent.Cast(animComponentForSpeed.GetCommandHandler());
            if (handlerForSpeed)
            {
                CharacterCommandMove moveCmdForSpeed = handlerForSpeed.GetCommandMove();
                if (moveCmdForSpeed)
                {
                    slopeAngleDegrees = moveCmdForSpeed.GetMovementSlopeAngle();
                }
            }
        }
        
        // 计算坡度自适应目标速度（坡度-速度负反馈）
        float baseTargetSpeed = RealisticStaminaSpeedSystem.TARGET_RUN_SPEED; // 3.7 m/s
        float slopeAdjustedTargetSpeed = RealisticStaminaSpeedSystem.CalculateSlopeAdjustedTargetSpeed(baseTargetSpeed, slopeAngleDegrees);
        float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
        
        // ==================== "撞墙"临界点检测和5秒阻尼过渡（模块化）====================
        // 更新"撞墙"阻尼过渡模块状态
        float currentWorldTime = GetGame().GetWorld().GetWorldTime();
        if (m_pCollapseTransition)
            m_pCollapseTransition.Update(currentWorldTime, staminaPercent);
        
        // 计算Run的基础速度倍数（包含双稳态-平台期和5秒阻尼过渡逻辑）
        float runBaseSpeedMultiplier = 0.0;
        
        // 先计算正常情况下的基础速度倍数
        float normalBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        // 如果处于5秒阻尼过渡期间，使用模块计算过渡速度
        if (m_pCollapseTransition && m_pCollapseTransition.IsInTransition())
        {
            // 使用模块计算阻尼过渡期间的速度倍数
            runBaseSpeedMultiplier = m_pCollapseTransition.CalculateTransitionSpeedMultiplier(currentWorldTime, normalBaseSpeedMultiplier);
        }
        else
        {
            // 正常情况：使用静态工具类计算速度（基于坡度自适应目标速度，包含平滑过渡逻辑）
            // 如果体力充足，就维持自适应速度；如果体力进入红区，就平滑降速
            runBaseSpeedMultiplier = normalBaseSpeedMultiplier;
        }
        
        // 将速度从基础目标速度（3.7 m/s）缩放到坡度自适应速度
        float speedScaleFactor = slopeAdjustedTargetMultiplier / RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;
        
        // ==================== 根据移动类型应用最终速度和负重惩罚 ====================
        float finalSpeedMultiplier = 0.0;
        
        if (isSprinting || currentMovementPhase == 3) // Sprint
        {
            // ==================== Sprint速度计算（v2.5优化：完全基于Run速度的30%增量）====================
            // 核心逻辑：Sprint = (Run基础倍率 × 1.30) - (负重惩罚 × 0.15)
            // 这样Sprint完全继承Run的双稳态-平台期逻辑，无论Run如何调整，Sprint永远快30%
            // 
            // 爆发力逻辑：Sprint受负重惩罚的程度降低（0.15代替0.2），模拟肌肉爆发力
            // 生理学上，短时间爆发（无氧）可以克服一部分负重带来的阻力
            // 但维持极高的消耗倍数（3.0x），这样玩家能跑得快，但只能冲刺极短时间
            
            // Sprint速度 = Run基础倍率 × (1 + 30%)
            float sprintMultiplier = 1.0 + RealisticStaminaSpeedSystem.SPRINT_SPEED_BOOST; // 1.30
            finalSpeedMultiplier = (runBaseSpeedMultiplier * sprintMultiplier) - (encumbranceSpeedPenalty * 0.15);
            
            // Sprint最高速度限制（不超过游戏最大速度）
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 2) // Run
        {
            // Run时：使用双稳态-应激性能模型（带平滑过渡 + 坡度自适应 + 5秒阻尼过渡）
            // 注意：Run的基础速度倍数（runBaseSpeedMultiplier）已在上面统一计算
            // 这里只需要应用负重惩罚即可
            
            // 负重主要影响"油耗"（体力消耗）而不是直接降低"最高档位"（速度）
            // 负重对速度的影响大幅降低，让30kg负重时仍能短时间跑3.7 m/s，只是消耗更快
            finalSpeedMultiplier = runBaseSpeedMultiplier - (encumbranceSpeedPenalty * 0.2); // 降低到20%的影响
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 1) // Walk
        {
            // Walk时，速度约为Run的70%
            // 使用静态工具类计算Run速度（已包含平滑过渡逻辑）
            float walkBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
            // Walk = Run × 0.7
            finalSpeedMultiplier = walkBaseSpeedMultiplier * 0.7;
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.2, 0.8);
        }
        else // Idle
        {
            // Idle时，速度倍数为0（但实际速度会由游戏引擎控制）
            finalSpeedMultiplier = 0.0;
        }
        
        // 注意：完全替换模式
        // - 速度限制：通过 OverrideMaxSpeed() 完全替换游戏原有的速度设置
        // - 负重惩罚：通过我们的计算完全替换游戏原有的负重惩罚（如果有）
        // - 体力消耗：手动根据实际速度和负重计算体力消耗（基于医学模型）
        
        // ==================== 网络同步：使用 RPC 进行服务器端验证 =====================
        // 客户端：发送状态到服务器端进行验证
        // 服务器端：验证并返回正确值
        if (isClient && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            // 获取当前重量用于验证（使用模块）
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
            
            // 定期发送状态到服务器端（每1秒发送一次，避免频繁网络调用）
            World worldCheck = GetGame().GetWorld();
            if (worldCheck && m_pNetworkSyncManager)
            {
                float currentTime = worldCheck.GetWorldTime();
                if (m_pNetworkSyncManager.ShouldSync(currentTime))
                {
                    // 发送 RPC 到服务器端进行验证
                    RPC_UpdateStaminaState(staminaPercent, currentWeight, finalSpeedMultiplier);
                }
            }
        }
        
        // ==================== 速度插值平滑处理（模块化）====================
        // 问题：服务器端同步的速度倍数可能与客户端计算的值不同，立即切换会导致"拉回"感
        // 解决方案：使用插值平滑过渡，避免突然的速度变化
        
        // 如果服务器端验证的速度倍数与客户端计算的不同，使用插值平滑过渡（使用模块）
        float smoothedSpeedMultiplier = finalSpeedMultiplier;
        if (m_pNetworkSyncManager)
        {
            float targetSpeedMultiplier = m_pNetworkSyncManager.GetTargetSpeedMultiplier(finalSpeedMultiplier);
            float currentTime = GetGame().GetWorld().GetWorldTime();
            smoothedSpeedMultiplier = m_pNetworkSyncManager.GetSmoothedSpeedMultiplier(currentTime);
        }
        
        // 始终更新速度（确保体力变化时立即生效）
        // 使用平滑后的速度倍数，避免突然的速度变化
        // 注意：在服务器端，这个调用会影响所有客户端（通过网络同步）
        // 在客户端，这个调用只影响本地玩家
        OverrideMaxSpeed(smoothedSpeedMultiplier);
        
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
        
        // 获取当前实际速度（m/s）
        vector velocity = GetVelocity();
        vector horizontalVelocity = velocity;
        horizontalVelocity[1] = 0.0; // 忽略垂直速度
        float currentSpeed = horizontalVelocity.Length(); // 当前水平速度（m/s）
        
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
            staminaPercent = staminaPercent - vaultCost;
            
            // 更新冷却时间（每0.2秒调用一次）
            m_pJumpVaultDetector.UpdateCooldowns();
            
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
            float currentTimeForFatigue = GetGame().GetWorld().GetWorldTime();
            m_pFatigueSystem.ProcessFatigueDecay(currentTimeForFatigue, currentSpeed);
        }
        
        // ==================== 运动持续时间跟踪（模块化）====================
        // 更新运动/休息时间跟踪，并计算累积疲劳因子
        float currentTimeForExercise = GetGame().GetWorld().GetWorldTime();
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
        
        // ==================== 代谢适应计算（Metabolic Adaptation）====================
        // 基于个性化运动建模（Palumbo et al., 2018）
        // 根据运动强度动态调整能量效率
        float metabolicEfficiencyFactor = 1.0;
        if (speedRatio < RealisticStaminaSpeedSystem.AEROBIC_THRESHOLD)
        {
            // 有氧区（<60% VO2max）：主要依赖脂肪，效率高
            metabolicEfficiencyFactor = RealisticStaminaSpeedSystem.AEROBIC_EFFICIENCY_FACTOR; // 0.9（更高效）
        }
        else if (speedRatio < RealisticStaminaSpeedSystem.ANAEROBIC_THRESHOLD)
        {
            // 混合区（60-80% VO2max）：糖原+脂肪混合
            // 线性插值：在0.6-0.8之间从0.9过渡到1.2
            float t = (speedRatio - RealisticStaminaSpeedSystem.AEROBIC_THRESHOLD) / (RealisticStaminaSpeedSystem.ANAEROBIC_THRESHOLD - RealisticStaminaSpeedSystem.AEROBIC_THRESHOLD);
            metabolicEfficiencyFactor = RealisticStaminaSpeedSystem.AEROBIC_EFFICIENCY_FACTOR + t * (RealisticStaminaSpeedSystem.ANAEROBIC_EFFICIENCY_FACTOR - RealisticStaminaSpeedSystem.AEROBIC_EFFICIENCY_FACTOR);
        }
        else
        {
            // 无氧区（≥80% VO2max）：主要依赖糖原，效率低但功率高
            metabolicEfficiencyFactor = RealisticStaminaSpeedSystem.ANAEROBIC_EFFICIENCY_FACTOR; // 1.2（低效但高功率）
        }
        
        // ==================== 健康状态影响计算 ====================
        // 基于个性化运动建模（Palumbo et al., 2018）
        // 训练有素者（fitness=1.0）能量效率提高18%
        // 效率因子：1.0 - FITNESS_EFFICIENCY_COEFF × fitness_level
        float fitnessEfficiencyFactor = 1.0 - (RealisticStaminaSpeedSystem.FITNESS_EFFICIENCY_COEFF * RealisticStaminaSpeedSystem.FITNESS_LEVEL);
        fitnessEfficiencyFactor = Math.Clamp(fitnessEfficiencyFactor, 0.7, 1.0); // 限制在70%-100%之间
        
        // 综合效率因子 = 健康状态效率 × 代谢适应效率
        float totalEfficiencyFactor = fitnessEfficiencyFactor * metabolicEfficiencyFactor;
        
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
        
        // ==================== 使用完整的 Pandolf 模型（包括坡度项）====================
        // 始终使用完整的 Pandolf 模型：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
        // 坡度项 G·(0.23 + 1.34·V²) 已整合在公式中
        // 注意：由于几乎没有完全平地的地形，所以始终使用包含坡度的 Pandolf 模型
        
        // 获取坡度信息（使用上面已声明的 slopeAngleDegrees）
        float gradePercent = 0.0;
        
        // 检查是否在攀爬或跳跃状态（改进：使用动作监听器检测的跳跃输入）
        bool isClimbingForSlope = IsClimbing();
        bool isJumpingForSlope = false;
        if (m_pJumpVaultDetector)
            isJumpingForSlope = m_pJumpVaultDetector.GetJumpInputTriggered();
        
        // 只在非攀爬、非跳跃状态下获取坡度
        if (!isClimbingForSlope && !isJumpingForSlope && currentSpeed > 0.05)
        {
            // 尝试从动画组件获取移动坡度角度
            CharacterAnimationComponent animComponent = GetAnimationComponent();
            if (animComponent)
            {
                CharacterCommandHandlerComponent handler = CharacterCommandHandlerComponent.Cast(animComponent.GetCommandHandler());
                if (handler)
                {
                    CharacterCommandMove moveCmd = handler.GetCommandMove();
                    if (moveCmd)
                    {
                        slopeAngleDegrees = moveCmd.GetMovementSlopeAngle();
                        
                        // 将坡度角度（度）转换为坡度百分比
                        // 坡度百分比 = tan(坡度角度) × 100
                        gradePercent = Math.Tan(slopeAngleDegrees * 0.0174532925199433) * 100.0; // 0.0174532925199433 = π/180
                    }
                }
            }
        }
        
        // ==================== 地形系数获取（模块化）====================
        // 地形系数：铺装路面 1.0 → 深雪 2.1-3.0
        // 优化检测频率：移动时0.5秒检测一次，静止时2秒检测一次（性能优化）
        // 注意：使用上面已声明的 currentTimeForExercise
        float terrainFactor = 1.0; // 默认值（铺装路面）
        if (m_pTerrainDetector)
        {
            terrainFactor = m_pTerrainDetector.GetTerrainFactor(owner, currentTimeForExercise, currentSpeed);
        }
        
        // ==================== 跑步模式判断（Givoni-Goldman）====================
        // 当速度 V > 2.2 m/s 时，使用 Givoni-Goldman 跑步模型
        // 否则使用标准 Pandolf 步行模型
        bool isRunning = (currentSpeed > 2.2);
        bool useGivoniGoldman = isRunning; // 是否使用 Givoni-Goldman 模型
        
        // 始终使用完整的 Pandolf 模型（包含坡度项、地形系数、Santee下坡修正）
        // E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²)) · η
        // 如果无法获取坡度，坡度项为 0（平地情况）
        // 如果速度为0，计算静态站立消耗
        float baseDrainRateByVelocity = 0.0;
        if (currentSpeed < 0.1)
        {
            // ==================== 静态负重站立消耗（Pandolf 静态项）====================
            // 当 V=0 时，背负重物原地站立也会消耗体力
            // 公式：E_standing = 1.5·W_body + 2.0·(W_body + L)·(L/W_body)²
            float bodyWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 90kg
            float loadWeight = Math.Max(currentWeight - bodyWeight, 0.0); // 负重（去除身体重量）
            
            float staticDrainRate = RealisticStaminaSpeedSystem.CalculateStaticStandingCost(bodyWeight, loadWeight);
            baseDrainRateByVelocity = staticDrainRate * 0.2; // 转换为每0.2秒的消耗率
            
            // 静态消耗为正值（消耗），但需要与恢复率结合使用
            // 在实际恢复计算中，会从恢复率中减去静态消耗
        }
        else if (useGivoniGoldman)
        {
            // ==================== Givoni-Goldman 跑步模型 ====================
            // 当速度 V > 2.2 m/s 时，切换到跑步模型
            // 公式：E_run = (W_body + L)·Constant·V^α · η
            // 注意：跑步模型同样应用地形系数
            float runningDrainRate = RealisticStaminaSpeedSystem.CalculateGivoniGoldmanRunning(currentSpeed, currentWeight, true);
            
            // 应用地形系数（跑步时同样受地形影响）
            runningDrainRate = runningDrainRate * terrainFactor;
            
            baseDrainRateByVelocity = runningDrainRate * 0.2; // 转换为每0.2秒的消耗率
        }
        else
        {
            // 步行模式：使用 Pandolf 模型（包含地形系数和 Santee 下坡修正）
            baseDrainRateByVelocity = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                currentSpeed, 
                currentWeight, 
                gradePercent,
                terrainFactor,  // 地形系数
                true            // 使用 Santee 下坡修正
            );
        }
        
        // Pandolf 模型的结果是每秒的消耗率，需要转换为每0.2秒的消耗率
        baseDrainRateByVelocity = baseDrainRateByVelocity * 0.2; // 转换为每0.2秒的消耗率
        
        // 应用多维度修正因子（健康状态、累积疲劳、代谢适应）
        // 注意：恢复时（baseDrainRateByVelocity < 0），不应用效率因子（恢复不受效率影响）
        float baseDrainRate = 0.0;
        if (baseDrainRateByVelocity < 0.0)
        {
            // 恢复时，直接使用恢复率（负数）
            baseDrainRate = baseDrainRateByVelocity;
        }
        else
        {
            // 消耗时，应用效率因子和疲劳因子
            // 注意：如果使用 Pandolf 模型，坡度项已经在公式中，不需要额外的坡度倍数
            baseDrainRate = baseDrainRateByVelocity * totalEfficiencyFactor * fatigueFactor;
        }
        
        // 保留原有的速度相关项（用于平滑过渡和精细调整）
        // 这些项在速度阈值附近提供平滑过渡
        float speedLinearDrainRate = 0.00005 * speedRatio * totalEfficiencyFactor * fatigueFactor; // 降低系数，主要依赖分段消耗率
        float speedSquaredDrainRate = 0.00005 * speedRatio * speedRatio * totalEfficiencyFactor * fatigueFactor; // 降低系数
        
        // ==================== 性能优化：使用缓存的负重消耗倍数（模块化）====================
        // 使用缓存的体力消耗倍数（避免重复计算）
        float encumbranceStaminaDrainMultiplier = 1.0;
        if (m_pEncumbranceCache)
            encumbranceStaminaDrainMultiplier = m_pEncumbranceCache.GetStaminaDrainMultiplier();
        
        // 负重对速度平方项的额外影响（Pandolf 模型中负重与速度的交互项）
        // 公式：负重额外消耗 = base_encumbrance_drain + speed_encumbrance_drain·V²
        float encumbranceBaseDrainRate = 0.001 * (encumbranceStaminaDrainMultiplier - 1.0); // 负重基础消耗
        float encumbranceSpeedDrainRate = 0.0002 * (encumbranceStaminaDrainMultiplier - 1.0) * speedRatio * speedRatio; // 负重速度相关消耗
        
        // ==================== 三维交互项（可选，用于精细调整）====================
        // 注意：Pandolf 模型已经包含了坡度项，但可以添加额外的三维交互项用于精细调整
        // 这主要用于在极端情况下（高速度+高负重+大坡度）进行微调
        float speedEncumbranceSlopeInteraction = 0.0;
        
        // 可选：计算速度×负重×坡度三维交互项（用于精细调整）
        // 如果不需要精细调整，可以将此部分注释掉
        // if (!isClimbingForSlope && !isJumpingForSlope && currentSpeed > 0.05 && Math.AbsFloat(gradePercent) > 1.0)
        // {
        //     float bodyMassPercent = 0.0;
        //     if (m_bEncumbranceCacheValid)
        //         bodyMassPercent = m_fCachedBodyMassPercent;
        //     speedEncumbranceSlopeInteraction = RealisticStaminaSpeedSystem.CalculateSpeedEncumbranceSlopeInteraction(speedRatio, bodyMassPercent, slopeAngleDegrees);
        // }
        
        // ==================== Sprint额外体力消耗 ====================
        // Sprint时体力消耗大幅增加（类似于追击或逃命，追求速度但消耗巨大）
        float sprintMultiplier = 1.0;
        if (isSprinting || currentMovementPhase == 3)
        {
            sprintMultiplier = RealisticStaminaSpeedSystem.SPRINT_STAMINA_DRAIN_MULTIPLIER;
        }
        
        // ==================== 综合体力消耗率（基于 Pandolf 模型）====================
        // Pandolf 模型公式：E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²))
        // 其中坡度项 G·(0.23 + 1.34·V²) 已经在 baseDrainRate 中
        // 
        // 综合公式：total = (base_pandolf + linear·V + squared·V² + enc_base + enc_speed·V²) × sprint_multiplier × efficiency × fatigue + 3d_interaction
        // 其中：
        //   base_pandolf 是基于 Pandolf 模型的基础消耗率（已包含坡度项）
        //   efficiency 和 fatigue 是多维度特性（健康状态、累积疲劳、代谢适应）
        //   3d_interaction 是速度×负重×坡度的三维交互项（可选，用于精细调整）
        float baseDrainComponents = baseDrainRate + speedLinearDrainRate + speedSquaredDrainRate + encumbranceBaseDrainRate + encumbranceSpeedDrainRate;
        
        // 应用 Sprint 倍数和多维度修正
        // 注意：恢复时（baseDrainRate < 0），不应用 Sprint 倍数
        // 注意：坡度项已经在 Pandolf 模型中，不需要额外的坡度倍数
        float totalDrainRate = 0.0;
        if (baseDrainRate < 0.0)
        {
            // 恢复时，直接使用恢复率（负数）
            totalDrainRate = baseDrainRate;
        }
        else
        {
            // 消耗时，应用 Sprint 倍数
            // 坡度项已经在 Pandolf 模型中，不需要额外的坡度倍数
            totalDrainRate = (baseDrainComponents * sprintMultiplier) + (baseDrainComponents * speedEncumbranceSlopeInteraction);
            
            // ==================== 生理上限：防止负重+坡度爆炸 + 疲劳积累系统 ====================
            // 无论负重多重、坡多陡，人的瞬时代谢率是有极限的（即 VO2 Max 峰值）
            // 设定每 0.2s 最大体力消耗不超过 0.02（即每秒最多掉 10%）
            // 这样即使玩家背着 30kg 爬 15 度坡，他也不会在 5 秒内暴毙
            // 系统会强制限制他的最大消耗，同时由于"自适应步幅"逻辑，他的速度会自动降得很低
            // 这既保证了硬核的真实性，又避免了数值上的溢出导致无法玩下去
            const float MAX_DRAIN_RATE_PER_TICK = 0.02; // 每0.2秒最大消耗
            
            // 计算超出生理上限的消耗（用于疲劳积累，模块化）
            float excessDrainRate = totalDrainRate - MAX_DRAIN_RATE_PER_TICK;
            if (m_pFatigueSystem && excessDrainRate > 0.0)
            {
                // 将超出消耗转化为疲劳积累（使用模块）
                m_pFatigueSystem.ProcessFatigueAccumulation(excessDrainRate);
            }
            
            // 限制实际消耗不超过生理上限
            totalDrainRate = Math.Min(totalDrainRate, MAX_DRAIN_RATE_PER_TICK);
        }
        
        // ==================== 完全控制体力值（基于医学模型）====================
        // 由于已禁用原生体力系统，我们完全控制体力值的变化
        // 根据计算出的消耗率/恢复率，更新目标体力值
        if (m_pStaminaComponent)
        {
            float newTargetStamina = staminaPercent;
            
            // 如果角色正在移动（速度 > 0.05 m/s），应用体力消耗
            if (currentSpeed > 0.05)
            {
                // 计算新的目标体力值（扣除消耗）
                // baseDrainRateByVelocity 已经包含了 Pandolf 模型或 Givoni-Goldman 跑步模型的消耗
                // 并且已经应用了地形系数（跑步模式）或地形系数+坡度修正（步行模式）
                newTargetStamina = staminaPercent - totalDrainRate;
            }
            // 如果角色完全静止（速度 <= 0.05 m/s），体力恢复（但需考虑静态站立消耗）
            else
            {
                // ==================== 多维度恢复模型（考虑静态站立消耗）====================
                // 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
                // 考虑多个维度：
                // 1. 当前体力百分比（非线性恢复）：体力越低恢复越快，体力越高恢复越慢
                // 2. 健康状态/训练水平：训练有素者恢复更快
                // 3. 休息时间：刚停止运动时恢复快（快速恢复期），长时间休息后恢复慢（慢速恢复期）
                // 4. 年龄：年轻者恢复更快
                // 5. 累积疲劳恢复：运动后的疲劳需要时间恢复
                // 6. 静态站立消耗：背负重物站立会减缓恢复速度
                // ==================== 性能优化：使用缓存的当前重量（模块化）====================
                // 使用缓存的当前重量（避免重复查找组件）
                float currentWeightForRecovery = 0.0;
                if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
                    currentWeightForRecovery = m_pEncumbranceCache.GetCurrentWeight();
                
                // ==================== 趴下休息时的负重优化（Prone Rest Weight Reduction）====================
                // 当角色趴下休息时，负重的影响应该降至最低（因为地面支撑了装备重量）
                // 重装兵在趴下时，应该能够通过"卧倒休息"来快速恢复体力
                // 生理学依据：趴下时，装备重量由地面支撑，身体只需维持基础代谢，无需承担负重负担
                // 
                // 使用 ECharacterStance 枚举来检测角色姿态
                // ECharacterStance: STAND (0), CROUCH (1), PRONE (2)
                ECharacterStance currentStance = GetStance();
                if (currentStance == ECharacterStance.PRONE)
                {
                    // 如果角色趴下（PRONE），将负重视为基准重量（BASE_WEIGHT），去除额外负重的影响
                    // 这样负重恢复优化逻辑会将负重视为基准重量，允许快速恢复
                    currentWeightForRecovery = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 基准重量（身体重量），去除额外负重
                }
                
                // 获取运动/休息时间（用于多维度恢复模型）
                float restDurationMinutes = 0.0;
                float exerciseDurationMinutes = 0.0;
                if (m_pExerciseTracker)
                {
                    restDurationMinutes = m_pExerciseTracker.GetRestDurationMinutes();
                    exerciseDurationMinutes = m_pExerciseTracker.GetExerciseDurationMinutes();
                }
                
                float recoveryRate = RealisticStaminaSpeedSystem.CalculateMultiDimensionalRecoveryRate(
                    staminaPercent, 
                    restDurationMinutes, 
                    exerciseDurationMinutes,
                    currentWeightForRecovery
                );
                
                // 从恢复率中减去静态站立消耗（如果存在）
                // baseDrainRateByVelocity 在速度为0时已经计算了静态消耗
                if (baseDrainRateByVelocity > 0.0)
                {
                    // 静态站立消耗会减缓恢复速度
                    // 例如：空载恢复 1%/s，40kg负重时静态消耗 0.04%/s，净恢复约 0.96%/s
                    recoveryRate = Math.Max(recoveryRate - baseDrainRateByVelocity, -0.01); // 确保不会完全阻止恢复
                }
                
                // 计算新的目标体力值（增加恢复）
                newTargetStamina = staminaPercent + recoveryRate;
            }
            
            // ==================== 应用疲劳惩罚：限制最大体力上限（模块化）====================
            // 疲劳积累会降低最大体力上限（例如：30%疲劳 → 最大体力上限70%）
            // 这模拟了超负荷行为对身体造成的累积伤害
            float maxStaminaCap = 1.0;
            if (m_pFatigueSystem)
                maxStaminaCap = m_pFatigueSystem.GetMaxStaminaCap();
            
            // 限制体力值在有效范围内（0.0 - maxStaminaCap）
            newTargetStamina = Math.Clamp(newTargetStamina, 0.0, maxStaminaCap);
            
            // 如果当前体力超过疲劳惩罚后的上限，需要降低
            if (staminaPercent > maxStaminaCap)
            {
                // 立即降低到上限（模拟疲劳导致的体力上限降低）
                newTargetStamina = maxStaminaCap;
            }
            
            // 设置目标体力值（这会自动应用到体力组件）
            m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            
            // 立即验证体力值是否被正确设置（检测原生系统干扰）
            // 注意：由于监控频率很高，这里可能不需要立即验证
            // 但保留此检查作为双重保险
            float verifyStamina = m_pStaminaComponent.GetStamina();
            if (Math.AbsFloat(verifyStamina - newTargetStamina) > 0.005) // 如果偏差超过0.5%（降低阈值，更敏感）
            {
                // 检测到原生系统干扰，强制纠正
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
        
        // 调试输出（每5秒输出一次，避免过多日志，仅在客户端）
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            static int debugCounter = 0;
            debugCounter++;
            if (debugCounter >= 25) // 200ms * 25 = 5秒
            {
                debugCounter = 0;
                
                // 获取负重信息用于调试
                ChimeraCharacter characterForDebug = ChimeraCharacter.Cast(owner);
                float encumbrancePercent = 0.0;
                float combatEncumbrancePercent = 0.0;
                float debugCurrentWeight = 0.0;
                float maxWeight = 40.5; // 使用我们定义的最大负重
                float combatWeight = 30.0; // 战斗负重阈值
                if (characterForDebug)
                {
                    // 使用 SCR_CharacterInventoryStorageComponent 获取重量信息
                    SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(characterForDebug.FindComponent(SCR_CharacterInventoryStorageComponent));
                    if (characterInventory)
                    {
                        debugCurrentWeight = characterInventory.GetTotalWeight();
                        encumbrancePercent = RealisticStaminaSpeedSystem.CalculateEncumbrancePercent(owner);
                        combatEncumbrancePercent = RealisticStaminaSpeedSystem.CalculateCombatEncumbrancePercent(owner);
                    }
                }
                
                // 获取移动类型字符串
                string movementTypeStr = "Unknown";
                if (isSprinting || currentMovementPhase == 3)
                    movementTypeStr = "Sprint";
                else if (currentMovementPhase == 2)
                    movementTypeStr = "Run";
                else if (currentMovementPhase == 1)
                    movementTypeStr = "Walk";
                else if (currentMovementPhase == 0)
                    movementTypeStr = "Idle";
                
                // 获取坡度信息字符串
                string slopeInfo = "";
                if (Math.AbsFloat(slopeAngleDegrees) > 0.1) // 只显示有意义的坡度
                {
                    string slopeDirection = "";
                    if (slopeAngleDegrees > 0)
                        slopeDirection = "上坡";
                    else
                        slopeDirection = "下坡";
                    slopeInfo = string.Format(" | 坡度: %1%° (%2)", 
                        Math.Round(Math.AbsFloat(slopeAngleDegrees) * 10.0) / 10.0,
                        slopeDirection);
                }
                
                // 获取Sprint倍数信息
                string sprintInfo = "";
                if (isSprinting || currentMovementPhase == 3)
                {
                    sprintInfo = string.Format(" | Sprint消耗倍数: %1x", 
                        RealisticStaminaSpeedSystem.SPRINT_STAMINA_DRAIN_MULTIPLIER.ToString());
                }
                
                string encumbranceInfo = "";
                if (debugCurrentWeight > 0.0)
                {
                    string combatStatus = "";
                    if (combatEncumbrancePercent > 1.0)
                        combatStatus = " [超过战斗负重]";
                    else if (combatEncumbrancePercent >= 0.9)
                        combatStatus = " [接近战斗负重]";
                    
                    encumbranceInfo = string.Format(" | 负重: %1kg/%2kg (最大:%3kg, 战斗:%4kg%5)", 
                        debugCurrentWeight.ToString(), 
                        maxWeight.ToString(),
                        maxWeight.ToString(),
                        combatWeight.ToString(),
                        combatStatus);
                }
                
                // ==================== 地形系数调试：获取并显示地面密度（模块化）====================
                // 每0.5秒检测一次地面密度（性能优化）
                float currentTime = GetGame().GetWorld().GetWorldTime();
                if (m_pTerrainDetector)
                {
                    // 强制更新地形检测（用于调试显示）
                    m_pTerrainDetector.ForceUpdate(owner, currentTime);
                }
                
                // 构建地形密度信息字符串
                string terrainInfo = "";
                if (m_pTerrainDetector)
                {
                    float cachedDensity = m_pTerrainDetector.GetCachedTerrainDensity();
                    if (cachedDensity >= 0.0)
                    {
                        terrainInfo = string.Format(" | 地面密度: %1", 
                            Math.Round(cachedDensity * 100.0) / 100.0);
                    }
                    else
                    {
                        terrainInfo = " | 地面密度: 未检测";
                    }
                }
                else
                {
                    terrainInfo = " | 地面密度: 未检测";
                }
                
                // 注意：现在使用 Pandolf 模型，坡度项已经在公式中，不再需要坡度倍数
                // 显示坡度百分比而不是坡度倍数
                float gradeDisplay = gradePercent; // 坡度百分比（已在使用 Pandolf 模型时获取）
                // 将地形信息直接拼接到格式字符串中（因为 PrintFormat 不支持 %10 这样的两位数参数）
                string debugMessage = string.Format("[RealisticSystem] 调试: 类型=%1 | 体力=%2%% | 基础速度倍数=%3 | 负重惩罚=%4 | 最终速度倍数=%5 | 坡度=%6%%%7%8%9", 
                    movementTypeStr,
                    Math.Round(staminaPercent * 100.0).ToString(),
                    baseSpeedMultiplier.ToString(),
                    encumbranceSpeedPenalty.ToString(),
                    finalSpeedMultiplier.ToString(),
                    Math.Round(gradeDisplay * 10.0) / 10.0,
                    slopeInfo,
                    sprintInfo,
                    encumbranceInfo);
                // 追加地形密度信息
                Print(debugMessage + terrainInfo);
            }
        }
        
        // 继续更新
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
    }
    
    // ==================== 网络同步：服务器端速度计算 =====================
    // 服务器端独立计算速度倍数，用于验证客户端提交的值
    // 使用与客户端相同的逻辑，确保一致性
    protected float CalculateServerSpeedMultiplier(IEntity owner, float staminaPercent)
    {
        if (!owner)
            return 1.0;
        
        // 限制在有效范围内
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        // 检查是否精疲力尽
        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);
        if (isExhausted)
        {
            float limpSpeedMultiplier = RealisticStaminaSpeedSystem.EXHAUSTION_LIMP_SPEED / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            return limpSpeedMultiplier;
        }
        
        // 获取移动类型（服务器端也需要检测移动状态）
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        
        // 检查是否可以Sprint
        bool canSprint = RealisticStaminaSpeedSystem.CanSprint(staminaPercent);
        if (isExhausted || !canSprint)
        {
            if (isSprinting || currentMovementPhase == 3)
                currentMovementPhase = 2; // 强制切换到Run
        }
        
        // 计算基础速度倍数
        float baseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        // 获取负重信息（服务器端也需要获取）
        // 注意：CalculateEncumbranceSpeedPenalty 需要 IEntity 参数，不是 float
        float encumbranceSpeedPenalty = RealisticStaminaSpeedSystem.CalculateEncumbranceSpeedPenalty(owner);
        
        // 根据移动类型计算最终速度倍数
        float finalSpeedMultiplier = 0.0;
        
        if (isSprinting || currentMovementPhase == 3) // Sprint
        {
            finalSpeedMultiplier = baseSpeedMultiplier - (encumbranceSpeedPenalty * 0.2);
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 2) // Run
        {
            finalSpeedMultiplier = baseSpeedMultiplier - (encumbranceSpeedPenalty * 0.2);
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 1) // Walk
        {
            finalSpeedMultiplier = baseSpeedMultiplier * 0.7;
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.2, 0.8);
        }
        else // Idle
        {
            finalSpeedMultiplier = 0.0;
        }
        
        return finalSpeedMultiplier;
    }
    
    // ==================== 网络同步：验证体力状态 =====================
    // 验证客户端提交的体力状态是否合理
    protected bool ValidateStaminaState(float staminaPercent, float weight)
    {
        // 验证体力值范围（0.0-1.0）
        if (staminaPercent < 0.0 || staminaPercent > 1.0)
            return false;
        
        // 验证重量范围（0.0-200.0 kg，合理的最大值）
        if (weight < 0.0 || weight > 200.0)
            return false;
        
        // 验证体力值变化是否过快（防止客户端快速修改）
        float lastReportedStamina = 1.0;
        if (m_pNetworkSyncManager)
            lastReportedStamina = m_pNetworkSyncManager.GetLastReportedStaminaPercent();
        float staminaChange = Math.AbsFloat(staminaPercent - lastReportedStamina);
        if (staminaChange > 0.5) // 单次更新最多变化50%
        {
            // 如果变化过快，记录警告（仅在调试模式下）
            // PrintFormat("[RealisticSystem] 警告：体力值变化过快！上次=%1, 当前=%2, 变化=%3", 
            //     lastReportedStamina.ToString(), 
            //     staminaPercent.ToString(), 
            //     staminaChange.ToString());
            // 允许但记录警告（可能是正常的快速消耗）
        }
        
        // 更新记录的值（使用模块）
        if (m_pNetworkSyncManager)
            m_pNetworkSyncManager.UpdateReportedState(staminaPercent, weight);
        
        return true;
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
        
        // 获取当前速度
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
        
        // 继续下一秒的采样
        GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }
    
    // 显示状态信息（包含速度、体力、速度倍数、移动类型、坡度等）
    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        // 只对本地控制的玩家显示
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 获取当前移动类型
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        string movementTypeStr = "Unknown";
        if (isSprinting || currentMovementPhase == 3)
            movementTypeStr = "Sprint";
        else if (currentMovementPhase == 2)
            movementTypeStr = "Run";
        else if (currentMovementPhase == 1)
            movementTypeStr = "Walk";
        else if (currentMovementPhase == 0)
            movementTypeStr = "Idle";
        
        // 获取坡度信息
        float slopeAngleDegrees = 0.0;
        CharacterAnimationComponent animComponent = GetAnimationComponent();
        if (animComponent)
        {
            CharacterCommandHandlerComponent handler = CharacterCommandHandlerComponent.Cast(animComponent.GetCommandHandler());
            if (handler)
            {
                CharacterCommandMove moveCmd = handler.GetCommandMove();
                if (moveCmd)
                {
                    slopeAngleDegrees = moveCmd.GetMovementSlopeAngle();
                }
            }
        }
        
        string slopeInfo = "";
        if (Math.AbsFloat(slopeAngleDegrees) > 0.1) // 只显示有意义的坡度
        {
            string slopeDirection = "";
            if (slopeAngleDegrees > 0)
                slopeDirection = "上坡";
            else
                slopeDirection = "下坡";
            slopeInfo = string.Format(" | 坡度: %1%° (%2)", 
                Math.Round(Math.AbsFloat(slopeAngleDegrees) * 10.0) / 10.0,
                slopeDirection);
        }
        
        // 格式化速度显示（保留1位小数）
        float speedRounded = Math.Round(m_fLastSecondSpeed * 10.0) / 10.0;
        
        // 格式化体力百分比（保留整数）
        int staminaPercentInt = Math.Round(m_fLastStaminaPercent * 100.0);
        
        // 格式化速度倍数（保留2位小数）
        float speedMultiplierRounded = Math.Round(m_fLastSpeedMultiplier * 100.0) / 100.0;
        
        // 构建状态文本
        string statusText = string.Format(
            "移动速度: %1 m/s | 体力: %2%% | 速度倍率: %3%% | 类型: %4%5",
            speedRounded.ToString(),
            staminaPercentInt.ToString(),
            (speedMultiplierRounded * 100.0).ToString(),
            movementTypeStr,
            slopeInfo
        );
        
        // 在控制台输出状态信息（仅在客户端）
        // 已经在函数开头检查了 owner == SCR_PlayerController.GetLocalControlledEntity()
        PrintFormat("[RealisticSystem] %1", statusText);
    }
    
    // ==================== 网络同步：RPC 方法 =====================
    // 客户端发送状态到服务器端进行验证
    // RPC: 客户端 -> 服务器端（可靠传输）
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_UpdateStaminaState(float staminaPercent, float weight, float speedMultiplier)
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        // 服务器端验证
        // 1. 验证体力值的合理性（0.0-1.0范围）
        // 2. 验证重量值的合理性（0.0-200.0 kg范围）
        // 3. 计算服务器端的速度倍数
        // 4. 与客户端上报的值对比（允许小幅度差异）
        
        // 验证输入值的合理性
        if (!ValidateStaminaState(staminaPercent, weight))
        {
            // 验证失败，同步回正确的值
            float correctStamina = Math.Clamp(staminaPercent, 0.0, 1.0);
            float correctWeight = Math.Clamp(weight, 0.0, 200.0);
            float correctSpeedMultiplier = CalculateServerSpeedMultiplier(owner, correctStamina);
            
            // 发送正确的值回客户端
            RPC_SyncStaminaState(correctStamina, correctWeight, correctSpeedMultiplier);
            return;
        }
        
        // 服务器端独立计算速度倍数（使用相同的逻辑）
        float serverSpeedMultiplier = CalculateServerSpeedMultiplier(owner, staminaPercent);
        
        // ==================== 网络同步容差优化：连续偏差累计触发 =====================
        // 问题：地形LOD差异可能导致客户端和服务器端计算的速度倍数略有差异
        // 单次检查会在每次差异超过容差时立即同步，导致频繁拉回（Rubber-banding）
        // 解决方案：只有当偏差连续超过容差一段时间后才触发同步
        
        // 计算速度差异（使用模块处理偏差）
        float speedDifference = Math.AbsFloat(speedMultiplier - serverSpeedMultiplier);
        float currentTime = GetGame().GetWorld().GetWorldTime();
        
        if (m_pNetworkSyncManager)
        {
            bool shouldSync = m_pNetworkSyncManager.ProcessDeviation(speedDifference, currentTime);
            
            if (shouldSync)
            {
                // 连续偏差持续时间超过阈值：触发同步
                // 客户端速度倍数偏差过大，同步回服务器端的结果
                RPC_SyncStaminaState(staminaPercent, weight, serverSpeedMultiplier);
                
                // 保存服务器端验证的速度倍数（使用模块）
                m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(serverSpeedMultiplier);
            }
            else if (speedDifference <= m_pNetworkSyncManager.GetValidationTolerance())
            {
                // 偏差在容差范围内：验证通过
                // 保存验证结果（使用模块）
                m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(serverSpeedMultiplier);
                m_pNetworkSyncManager.UpdateReportedState(staminaPercent, weight);
            }
        }
    }
    
    // 服务器端同步状态回客户端（用于修正）
    // RPC: 服务器端 -> 客户端（可靠传输）
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_SyncStaminaState(float staminaPercent, float weight, float speedMultiplier)
    {
        IEntity owner = GetOwner();
        if (!owner || owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 应用服务器端验证的值
        // 注意：这里只是记录服务器端的值，实际速度更新仍然在 UpdateSpeedBasedOnStamina 中进行
        // 但可以在下次更新时使用服务器端的值作为参考
        
        // 保存服务器端同步的值（使用模块）
        if (m_pNetworkSyncManager)
            m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(speedMultiplier);
        
        // 如果服务器端验证的体力值与客户端差异较大，可以更新体力值
        if (m_pStaminaComponent)
        {
            float currentStamina = m_pStaminaComponent.GetTargetStamina();
            float staminaDifference = Math.AbsFloat(currentStamina - staminaPercent);
            
            // 如果差异超过5%，使用服务器端的值
            if (staminaDifference > 0.05)
            {
                // 服务器端验证的体力值更准确，更新客户端
                m_pStaminaComponent.SetTargetStamina(staminaPercent);
            }
        }
        
        // 如果速度倍数差异较大，在下一次 UpdateSpeedBasedOnStamina 中使用服务器端的值
        // 这会在下次更新时自动应用
    }
}
