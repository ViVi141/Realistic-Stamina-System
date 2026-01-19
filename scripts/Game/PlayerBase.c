// Realistic Stamina System (RSS) - v2.10.0
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
    
    // 在组件初始化后
    override void OnInit(IEntity owner)
    {
        super.OnInit(owner);
        
        if (!owner)
            return;
        
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
        
        // 初始化EPOC状态管理
        m_pEpocState = new EpocState();
        
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
        CharacterAnimationComponent animComponent = GetAnimationComponent();
        if (!animComponent)
            return false;
        
        CharacterCommandHandlerComponent handler = animComponent.GetCommandHandler();
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
        
        // 只对本地控制的玩家处理
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
        {
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, SPEED_UPDATE_INTERVAL_MS, false);
            return;
        }
        
        // 获取当前体力百分比（使用目标体力值，这是我们控制的）
        float staminaPercent = 1.0;
        if (m_pStaminaComponent)
            staminaPercent = m_pStaminaComponent.GetTargetStamina();
        
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
        
        // ==================== 速度计算（模块化）====================
        float currentWorldTime = GetGame().GetWorld().GetWorldTime();
        float slopeAngleDegrees = SpeedCalculator.GetSlopeAngle(this);
        float runBaseSpeedMultiplier = SpeedCalculator.CalculateBaseSpeedMultiplier(staminaPercent, m_pCollapseTransition, currentWorldTime);
        
        // 计算坡度自适应目标速度倍数
        float slopeAdjustedTargetSpeed = SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
            RealisticStaminaSpeedSystem.TARGET_RUN_SPEED, slopeAngleDegrees);
        float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
        float speedScaleFactor = slopeAdjustedTargetMultiplier / RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;
        
        // 计算最终速度倍数（使用模块）
        float finalSpeedMultiplier = SpeedCalculator.CalculateFinalSpeedMultiplier(
            runBaseSpeedMultiplier,
            encumbranceSpeedPenalty,
            isSprinting,
            currentMovementPhase,
            isExhausted,
            canSprint,
            staminaPercent);
        
        // 注意：完全替换模式
        // - 速度限制：通过 OverrideMaxSpeed() 完全替换游戏原有的速度设置
        // - 负重惩罚：通过我们的计算完全替换游戏原有的负重惩罚（如果有）
        // - 体力消耗：手动根据实际速度和负重计算体力消耗（基于医学模型）
        
        // 应用速度倍数
        OverrideMaxSpeed(finalSpeedMultiplier);
        
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
        
        // ==================== 获取当前实际速度（m/s）====================
        // 说明：游泳命令通过 PrePhys_SetTranslation 直接改变位移，GetVelocity() 可能为 0
        // 解决：使用位置差分测速（每 0.2 秒一次），得到更可靠的速度向量
        vector currentPos = owner.GetOrigin();
        vector computedVelocity = vector.Zero;
        float dtSeconds = SPEED_UPDATE_INTERVAL_MS * 0.001;
        if (m_bHasLastPositionSample)
        {
            vector deltaPos = currentPos - m_vLastPositionSample;

            // 防止传送/同步跳变导致天文速度
            float deltaLen = deltaPos.Length();
            if (deltaLen < 20.0 && dtSeconds > 0.001)
            {
                computedVelocity = deltaPos / dtSeconds;
            }
        }

        m_vLastPositionSample = currentPos;
        m_bHasLastPositionSample = true;
        m_vComputedVelocity = computedVelocity;

        vector velocity = computedVelocity;
        vector horizontalVelocity = velocity;
        horizontalVelocity[1] = 0.0; // 忽略垂直速度
        float currentSpeed = horizontalVelocity.Length(); // 当前水平速度（m/s）
        
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
        // 检测方法：通过动画组件获取游泳命令（CharacterCommandSwim）
        bool isSwimming = IsSwimmingByCommand();
        
        // ==================== 游泳检测调试信息 ====================
        // 只在状态变化时输出调试信息
        if (isSwimming != m_bWasSwimming)
        {
            // 只对本地控制的玩家输出调试信息
            if (owner == SCR_PlayerController.GetLocalControlledEntity())
            {
                string oldState = "";
                if (m_bWasSwimming)
                    oldState = "游泳";
                else
                    oldState = "陆地";
                
                string newState = "";
                if (isSwimming)
                    newState = "游泳";
                else
                    newState = "陆地";
                
                string stateChange = string.Format(
                    "[游泳检测] 状态变化: %1 -> %2",
                    oldState,
                    newState
                );
                Print(stateChange);
            }
        }
        
        // 如果游泳状态变化，重置调试标志
        if (isSwimming != m_bWasSwimming)
            m_bSwimmingVelocityDebugPrinted = false;
        
        // 更新上一帧状态
        m_bWasSwimming = isSwimming;
        
        // ==================== 湿重效应跟踪 ====================
        // 上岸后30秒内，由于衣服吸水，临时增加5-10kg虚拟负重
        float currentTime = GetGame().GetWorld().GetWorldTime();
        
        if (isSwimming)
        {
            // 正在游泳：重置湿重计时器
            m_fWetWeightStartTime = -1.0;
            m_fCurrentWetWeight = 0.0;
        }
        else if (m_bWasSwimming && !isSwimming)
        {
            // 刚从水中上岸：启动湿重计时器
            m_fWetWeightStartTime = currentTime;
            // 根据游泳时间计算湿重（游泳时间越长，湿重越大）
            // 简化：使用固定湿重（7.5kg，介于5-10kg之间）
            m_fCurrentWetWeight = (StaminaConstants.WET_WEIGHT_MIN + StaminaConstants.WET_WEIGHT_MAX) / 2.0;
        }
        else if (m_fWetWeightStartTime > 0.0)
        {
            // 检查湿重是否过期（30秒）
            float wetWeightElapsed = currentTime - m_fWetWeightStartTime;
            if (wetWeightElapsed >= StaminaConstants.WET_WEIGHT_DURATION)
            {
                // 湿重过期：清除湿重
                m_fWetWeightStartTime = -1.0;
                m_fCurrentWetWeight = 0.0;
            }
            else
            {
                // 湿重逐渐减少（线性衰减）
                float wetWeightRatio = 1.0 - (wetWeightElapsed / StaminaConstants.WET_WEIGHT_DURATION);
                m_fCurrentWetWeight = ((StaminaConstants.WET_WEIGHT_MIN + StaminaConstants.WET_WEIGHT_MAX) / 2.0) * wetWeightRatio;
            }
        }
        
        // ==================== 环境因子更新（模块化）====================
        // 每5秒更新一次环境因子（性能优化）
        // 传入角色实体用于室内检测
        if (m_pEnvironmentFactor)
        {
            m_pEnvironmentFactor.UpdateEnvironmentFactors(currentTime, owner);
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
        // 包括游泳湿重和降雨湿重，但应用饱和上限（防止叠加导致数值爆炸）
        float totalWetWeight = 0.0;
        if (m_fCurrentWetWeight > 0.0 && rainWeight > 0.0)
        {
            // 如果两者都大于0，使用加权平均（更真实的物理模型）
            // 加权平均：游泳湿重权重0.6，降雨湿重权重0.4（游泳更湿）
            totalWetWeight = m_fCurrentWetWeight * 0.6 + rainWeight * 0.4;
        }
        else
        {
            // 如果只有一个大于0，直接使用较大值
            totalWetWeight = Math.Max(m_fCurrentWetWeight, rainWeight);
        }
        // 应用饱和上限（防止数值爆炸）
        totalWetWeight = Math.Min(totalWetWeight, StaminaConstants.ENV_MAX_TOTAL_WET_WEIGHT);
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
        
        // ==================== 效率因子计算（模块化）====================
        float metabolicEfficiencyFactor = StaminaConsumptionCalculator.CalculateMetabolicEfficiencyFactor(speedRatio);
        float fitnessEfficiencyFactor = StaminaConsumptionCalculator.CalculateFitnessEfficiencyFactor();
        float totalEfficiencyFactor = fitnessEfficiencyFactor * metabolicEfficiencyFactor;
        
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
            CharacterAnimationComponent animComponentForSlope = GetAnimationComponent();
            if (animComponentForSlope)
            {
                CharacterCommandHandlerComponent handler = animComponentForSlope.GetCommandHandler();
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
        
        // ==================== 游泳模式判断 ====================
        // 如果正在游泳，使用游泳专用的消耗模型
        // 游泳时不需要考虑坡度、地形等因素（在水中）
        float baseDrainRateByVelocity = 0.0;
        if (useSwimmingModel)
        {
            // ==================== 游泳体力消耗模型（物理阻力模型）====================
            // 游泳的能量消耗远高于陆地移动（约3-5倍）
            // 使用物理阻力模型：F_d = 0.5 * ρ * v² * C_d * A
            // 注意：使用包含湿重的重量（上岸后30秒内仍有湿重影响）
            // 
            // 3D 游泳体能模型：使用“位置差分测速”的速度向量（包含垂直分量）
            // 原因：游泳命令通过 PrePhys_SetTranslation 推动位移，GetVelocity()/输入可能不反映真实移动
            vector swimmingVelocity = m_vComputedVelocity;

            // 调试信息：只在第一次检测到游泳速度仍为 0 时输出（避免刷屏）
            if (owner == SCR_PlayerController.GetLocalControlledEntity() && !m_bSwimmingVelocityDebugPrinted)
            {
                float swimVelLen = swimmingVelocity.Length();
                if (swimVelLen < 0.01)
                {
                    Print("[游泳速度] 位置差分测速仍为0：可能未发生位移（静止/卡住/命令未推动位置）");
                    m_bSwimmingVelocityDebugPrinted = true;
                }
            }
            
            float swimmingDrainRate = RealisticStaminaSpeedSystem.CalculateSwimmingStaminaDrain3D(swimmingVelocity, currentWeightWithWet);
            baseDrainRateByVelocity = swimmingDrainRate * 0.2; // 转换为每0.2秒的消耗率
            
            // 游泳时不应用其他修正（坡度、地形等），直接使用基础消耗
            // 但需要应用效率因子和疲劳因子
        }
        // ==================== 陆地移动模式判断（Givoni-Goldman / Pandolf）====================
        else
        {
            // 当速度 V > 2.2 m/s 时，使用 Givoni-Goldman 跑步模型
            // 否则使用标准 Pandolf 步行模型
            bool isRunning = (currentSpeed > 2.2);
            bool useGivoniGoldman = isRunning; // 是否使用 Givoni-Goldman 模型
            
            // 始终使用完整的 Pandolf 模型（包含坡度项、地形系数、Santee下坡修正）
            // E = M·(2.7 + 3.2·(V-0.7)² + G·(0.23 + 1.34·V²)) · η
            // 如果无法获取坡度，坡度项为 0（平地情况）
            // 如果速度为0，计算静态站立消耗
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
            // 注意：使用包含湿重的重量（上岸后30秒内仍有湿重影响）
            float weightForRunning = currentWeight;
            if (m_fCurrentWetWeight > 0.0)
                weightForRunning = currentWeight + m_fCurrentWetWeight;
            
            float runningDrainRate = RealisticStaminaSpeedSystem.CalculateGivoniGoldmanRunning(currentSpeed, weightForRunning, true);
            
            // 应用地形系数（跑步时同样受地形影响）
            runningDrainRate = runningDrainRate * terrainFactor;
            
            baseDrainRateByVelocity = runningDrainRate * 0.2; // 转换为每0.2秒的消耗率
        }
        else
        {
            // 步行模式：使用 Pandolf 模型（包含地形系数和 Santee 下坡修正）
            // 注意：使用包含湿重的重量（上岸后30秒内仍有湿重影响）
            float weightForLandMovement = currentWeight;
            if (m_fCurrentWetWeight > 0.0)
                weightForLandMovement = currentWeight + m_fCurrentWetWeight;
            
            baseDrainRateByVelocity = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                currentSpeed, 
                weightForLandMovement, 
                gradePercent,
                terrainFactor,  // 地形系数
                true            // 使用 Santee 下坡修正
            );
            }
            
            // Pandolf 模型的结果是每秒的消耗率，需要转换为每0.2秒的消耗率
            baseDrainRateByVelocity = baseDrainRateByVelocity * 0.2; // 转换为每0.2秒的消耗率
        }
        
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
        
        float sprintMultiplier = 1.0;
        if (!useSwimmingModel && (isSprinting || currentMovementPhase == 3))
            sprintMultiplier = RealisticStaminaSpeedSystem.SPRINT_STAMINA_DRAIN_MULTIPLIER;
        
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
                baseDrainRateByVelocityForModule);
            
            // 应用热应激倍数（影响体力消耗）
            totalDrainRate = totalDrainRate * heatStressMultiplier;
        }
        
        // 如果模块计算的基础消耗率为0，使用本地计算的baseDrainRateByVelocity
        if (baseDrainRateByVelocityForModule == 0.0 && baseDrainRateByVelocity > 0.0)
            baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
        // ==================== EPOC（过量耗氧）延迟检测（模块化）====================
        // 注意：currentWorldTime已在上面声明（第381行），这里重用
        // 游泳时跳过 EPOC：避免水面漂移/抖动导致“停下→EPOC”误触发
        if (m_pEpocState && !useSwimmingModel)
        {
            bool isInEpocDelay = StaminaRecoveryCalculator.UpdateEpocDelay(
                m_pEpocState,
                currentSpeed,
                currentWorldTime);
        }
        
        // ==================== 完全控制体力值（基于医学模型）====================
        // 由于已禁用原生体力系统，我们完全控制体力值的变化
        // 根据计算出的消耗率/恢复率，更新目标体力值
        if (m_pStaminaComponent)
        {
            float newTargetStamina = staminaPercent;
            
            // 关键修复：只要在游泳状态（包括静止踩水），就必须走“消耗分支”
            // 仅在非游泳且静止时，才进入恢复分支
            if (useSwimmingModel || currentSpeed > 0.05)
            {
                // 计算新的目标体力值（扣除消耗）
                // baseDrainRateByVelocity 已经包含了 Pandolf 模型或 Givoni-Goldman 跑步模型的消耗
                // 并且已经应用了地形系数（跑步模式）或地形系数+坡度修正（步行模式）
                newTargetStamina = staminaPercent - totalDrainRate;
            }
            // 如果角色完全静止（速度 <= 0.05 m/s），根据EPOC延迟决定是消耗还是恢复
            else
            {
                // ==================== EPOC延迟期间：继续应用消耗（模块化）====================
                bool isInEpocDelay = false;
                float speedBeforeStop = 0.0;
                if (m_pEpocState)
                {
                    isInEpocDelay = m_pEpocState.IsInEpocDelay();
                    speedBeforeStop = m_pEpocState.GetSpeedBeforeStop();
                }
                
                if (isInEpocDelay)
                {
                    float epocDrainRate = StaminaRecoveryCalculator.CalculateEpocDrainRate(speedBeforeStop);
                    newTargetStamina = staminaPercent - epocDrainRate;
                }
                // ==================== EPOC延迟结束后：正常恢复（模块化）====================
                else
                {
                    float currentWeightForRecovery = 0.0;
                    if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
                        currentWeightForRecovery = m_pEncumbranceCache.GetCurrentWeight();
                    
                    currentWeightForRecovery = StaminaRecoveryCalculator.CalculateRecoveryWeight(currentWeightForRecovery, this);
                    
                    float restDurationMinutes = 0.0;
                    float exerciseDurationMinutes = 0.0;
                    if (m_pExerciseTracker)
                    {
                        restDurationMinutes = m_pExerciseTracker.GetRestDurationMinutes();
                        exerciseDurationMinutes = m_pExerciseTracker.GetExerciseDurationMinutes();
                    }
                    
                    float staticDrainForRecovery = baseDrainRateByVelocity;
                    if (baseDrainRateByVelocityForModule > 0.0)
                        staticDrainForRecovery = baseDrainRateByVelocityForModule;
                    
                    // 趴下休息：不应沿用“静态站立消耗”，否则会把恢复压成持续损耗
                    ECharacterStance stanceForRecovery = GetStance();
                    if (stanceForRecovery == ECharacterStance.PRONE)
                        staticDrainForRecovery = 0.0;
                    
                    float recoveryRate = StaminaRecoveryCalculator.CalculateRecoveryRate(
                        staminaPercent,
                        restDurationMinutes,
                        exerciseDurationMinutes,
                        currentWeightForRecovery,
                        staticDrainForRecovery,
                        false);
                    
                    // ==================== 热应激对恢复的影响（模块化）====================
                    // 生理学依据：高温不仅让人动起来累，更让人休息不回来
                    // 热应激越大，恢复倍数越小（恢复速度越慢）
                    float heatRecoveryPenalty = 1.0 / heatStressMultiplier; // 热应激1.3倍时，恢复速度降至约77%
                    recoveryRate = recoveryRate * heatRecoveryPenalty;
                    
                    newTargetStamina = staminaPercent + recoveryRate;
                }
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
                // 注意：currentTime 已在第551行声明，这里需要重新获取用于调试
                float currentTimeForDebug = GetGame().GetWorld().GetWorldTime();
                if (m_pTerrainDetector)
                {
                    // 强制更新地形检测（用于调试显示）
                    m_pTerrainDetector.ForceUpdate(owner, currentTimeForDebug);
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
                
                // ==================== 环境因子调试信息 =====================
                string envInfo = "";
                if (m_pEnvironmentFactor)
                {
                    // 获取当前时间
                    float currentHour = m_pEnvironmentFactor.GetCurrentHour();
                    string timeStr = "";
                    if (currentHour >= 0.0)
                    {
                        int hourInt = Math.Floor(currentHour);
                        int minuteInt = Math.Floor((currentHour - hourInt) * 60.0);
                        string minuteStr = minuteInt.ToString();
                        if (minuteStr.Length() == 1)
                            minuteStr = "0" + minuteStr;
                        timeStr = string.Format("%1:%2", 
                            hourInt.ToString(),
                            minuteStr);
                    }
                    else
                    {
                        timeStr = "未知";
                    }
                    
                    // 获取热应激倍数
                    float heatStress = heatStressMultiplier;
                    
                    // 获取降雨信息
                    float rainWeightDebug = rainWeight;
                    bool isRainingDebug = m_pEnvironmentFactor.IsRaining();
                    float rainIntensity = m_pEnvironmentFactor.GetRainIntensity();
                    string rainStr = "";
                    if (isRainingDebug && rainWeightDebug > 0.0)
                    {
                        string rainLevel = "";
                        if (rainIntensity >= 0.8)
                            rainLevel = "暴雨";
                        else if (rainIntensity >= 0.5)
                            rainLevel = "中雨";
                        else
                            rainLevel = "小雨";
                        rainStr = string.Format("降雨: %1 (%2kg, 强度%3%%)", 
                            rainLevel,
                            Math.Round(rainWeightDebug * 10.0) / 10.0,
                            Math.Round(rainIntensity * 100.0));
                    }
                    else if (rainWeightDebug > 0.0)
                    {
                        // 停止降雨但仍有湿重（衰减中）
                        rainStr = string.Format("降雨: 已停止 (残留%1kg)", 
                            Math.Round(rainWeightDebug * 10.0) / 10.0);
                    }
                    else
                    {
                        rainStr = "降雨: 无";
                    }
                    
                    // 获取室内状态
                    bool isIndoorDebug = m_pEnvironmentFactor.IsIndoor();
                    string indoorStr = "";
                    if (isIndoorDebug)
                        indoorStr = "室内";
                    else
                        indoorStr = "室外";
                    
                    // 获取游泳湿重
                    string swimWetStr = "";
                    if (m_fCurrentWetWeight > 0.0)
                    {
                        swimWetStr = string.Format("游泳湿重: %1kg", 
                            Math.Round(m_fCurrentWetWeight * 10.0) / 10.0);
                    }
                    else
                    {
                        swimWetStr = "游泳湿重: 0kg";
                    }
                    
                    // 构建环境信息字符串
                    envInfo = string.Format(" | 时间: %1 | 热应激: %2x | %3 | %4 | %5", 
                        timeStr,
                        Math.Round(heatStress * 100.0) / 100.0,
                        rainStr,
                        indoorStr,
                        swimWetStr);
                }
                else
                {
                    envInfo = " | 环境因子: 未初始化";
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
                // 追加地形密度信息和环境因子信息
                Print(debugMessage + terrainInfo + envInfo);
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
    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        // 只对本地控制的玩家显示
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 获取当前移动类型（游泳时显示 Swim）
        bool isSwimming = IsSwimmingByCommand();
        
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        string movementTypeStr = "Unknown";
        if (isSwimming)
        {
            movementTypeStr = "Swim";
        }
        else
        {
            if (isSprinting || currentMovementPhase == 3)
                movementTypeStr = "Sprint";
            else if (currentMovementPhase == 2)
                movementTypeStr = "Run";
            else if (currentMovementPhase == 1)
                movementTypeStr = "Walk";
            else if (currentMovementPhase == 0)
                movementTypeStr = "Idle";
        }
        
        // 获取坡度信息
        float slopeAngleDegrees = 0.0;
        CharacterAnimationComponent animComponent = GetAnimationComponent();
        if (animComponent)
        {
            CharacterCommandHandlerComponent handler = animComponent.GetCommandHandler();
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
    
}
