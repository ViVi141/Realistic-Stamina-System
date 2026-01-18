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
    
    // ==================== "撞墙"阻尼过渡跟踪 ====================
    // 解决"撞墙"瞬间的陡峭度问题：在体力触及25%时，使用5秒阻尼过渡
    // 让玩家感觉到角色是"腿越来越重"，而不是"引擎突然断油"
    protected bool m_bInCollapseTransition = false; // 是否处于"撞墙"过渡状态
    protected float m_fCollapseTransitionStartTime = 0.0; // "撞墙"过渡开始时间（秒）
    protected const float COLLAPSE_TRANSITION_DURATION = 5.0; // "撞墙"过渡持续时间（秒）
    protected const float COLLAPSE_THRESHOLD = 0.25; // "撞墙"临界点（25%体力）
    
    // ==================== 运动持续时间跟踪（用于累积疲劳计算）====================
    // 基于个性化运动建模（Palumbo et al., 2018）
    // 跟踪连续运动时间，用于计算累积疲劳
    protected float m_fExerciseDurationMinutes = 0.0; // 连续运动时间（分钟）
    protected float m_fLastUpdateTime = 0.0; // 上次更新时间（秒）
    protected bool m_bWasMoving = false; // 上次更新时是否在移动
    protected float m_fRestDurationMinutes = 0.0; // 连续休息时间（分钟），从停止运动开始计算
    
    // 跳跃和翻越检测相关（改进：使用动作监听器检测跳跃输入）
    protected bool m_bJumpInputTriggered = false; // 跳跃输入是否被触发（由动作监听器设置）
    protected int m_iJumpCooldownFrames = 0; // 跳跃冷却帧数（防止重复触发，3秒冷却）
    
    // 连续跳跃惩罚（无氧欠债）机制
    protected int m_iRecentJumpCount = 0; // 连续跳跃计数
    protected float m_fJumpTimer = 0.0; // 上次跳跃时间（秒）
    
    protected bool m_bIsVaulting = false; // 是否正在翻越/攀爬
    protected int m_iVaultingFrameCount = 0; // 翻越状态持续帧数
    protected int m_iVaultCooldownFrames = 0; // 翻越冷却帧数（防止重复触发）
    
    // ==================== 性能优化：负重缓存（事件驱动）====================
    // 避免每0.2秒重复计算负重，改为事件驱动（仅在库存变化时更新）
    protected float m_fCachedCurrentWeight = 0.0; // 缓存的当前重量（kg）
    protected float m_fCachedEncumbranceSpeedPenalty = 0.0; // 缓存的速度惩罚
    protected float m_fCachedBodyMassPercent = 0.0; // 缓存的有效负重占体重百分比
    protected float m_fCachedEncumbranceStaminaDrainMultiplier = 1.0; // 缓存的体力消耗倍数
    protected bool m_bEncumbranceCacheValid = false; // 缓存是否有效
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent; // 缓存的库存组件引用
    
    // ==================== 网络同步：服务器端验证（容差优化）====================
    // 服务器端验证速度变化，防止客户端作弊和"拉回"问题
    // 优化：添加连续偏差累计触发机制，防止地形LOD差异导致的频繁拉回
    protected float m_fServerValidatedSpeedMultiplier = 1.0; // 服务器端验证的速度倍数
    protected float m_fLastReportedStaminaPercent = 1.0; // 上次客户端报告的体力百分比
    protected float m_fLastReportedWeight = 0.0; // 上次客户端报告的重量
    protected const float VALIDATION_TOLERANCE = 0.05; // 验证容差（5%差异视为正常）
    protected const float NETWORK_SYNC_INTERVAL = 1.0; // 网络同步间隔（秒）
    protected float m_fLastNetworkSyncTime = 0.0; // 上次网络同步时间
    
    // ==================== 网络同步容差优化：连续偏差累计触发 =====================
    // 问题：地形LOD差异可能导致客户端和服务器端计算的速度倍数略有差异
    // 单次检查会在每次差异超过容差时立即同步，导致频繁拉回（Rubber-banding）
    // 解决方案：只有当偏差连续超过容差一段时间后才触发同步
    protected float m_fDeviationStartTime = -1.0; // 偏差开始时间（-1表示无偏差）
    protected const float DEVIATION_TRIGGER_DURATION = 2.0; // 偏差触发持续时间（秒），连续超过此时间才触发同步
    protected const float SMOOTH_TRANSITION_DURATION = 0.5; // 速度插值平滑过渡时间（秒）
    protected float m_fTargetSpeedMultiplier = 1.0; // 目标速度倍数（用于插值）
    protected float m_fSmoothedSpeedMultiplier = 1.0; // 平滑后的速度倍数
    
    // ==================== 生理上限溢出处理：疲劳积累系统 =====================
    // 问题：超出 MAX_DRAIN_RATE_PER_TICK 的消耗被直接截断，超负荷行为无累积后果
    // 解决方案：将超出消耗转化为疲劳积累，降低最大体力上限，需要长时间休息才能恢复
    protected float m_fFatigueAccumulation = 0.0; // 疲劳积累值（0.0-1.0），影响最大体力上限
    protected const float FATIGUE_DECAY_RATE = 0.0001; // 疲劳恢复率（每0.2秒），长时间休息时疲劳逐渐减少
    protected const float FATIGUE_DECAY_MIN_REST_TIME = 60.0; // 疲劳恢复所需的最小休息时间（秒），需要长时间休息
    protected float m_fLastFatigueDecayTime = 0.0; // 上次疲劳恢复检查时间
    protected float m_fLastRestStartTime = -1.0; // 上次开始休息的时间（-1表示未休息）
    protected const float MAX_FATIGUE_PENALTY = 0.3; // 最大疲劳惩罚（30%），即疲劳积累最高时可降低30%最大体力上限
    
    // ==================== 地形系数调试：地面密度检测 =====================
    // 用于调试和校准地形系数映射表
    protected float m_fCachedTerrainDensity = -1.0; // 缓存的地面密度值
    protected float m_fCachedTerrainFactor = 1.0; // 缓存的地形系数
    protected float m_fLastTerrainCheckTime = 0.0; // 上次地形检测时间
    protected float m_fLastMovementTime = 0.0; // 上次移动时间（用于优化静止时的检测）
    protected const float TERRAIN_CHECK_INTERVAL = 0.5; // 地形检测间隔（秒，移动时）
    protected const float TERRAIN_CHECK_INTERVAL_IDLE = 2.0; // 地形检测间隔（秒，静止时，优化性能）
    protected const float IDLE_THRESHOLD_TIME = 1.0; // 静止判定阈值（秒，超过此时间视为静止）
    
    // ==================== UI信号桥接系统：对接官方UI特效和音效 =====================
    // 问题：自定义体力系统接管了体力逻辑，但官方UI特效（模糊、呼吸声）可能失效
    // 解决方案：通过 SignalsManagerComponent 手动更新 "Exhaustion" 信号，让官方UI听从拟真模型的指挥
    protected SignalsManagerComponent m_pSignalsManager; // 信号管理器组件引用
    protected int m_iExhaustionSignal = -1; // "Exhaustion" 信号的ID（-1表示未找到）
    
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
        
        // 初始化跳跃和翻越检测变量（改进：使用动作监听器）
        m_bJumpInputTriggered = false;
        m_iJumpCooldownFrames = 0;
        m_iRecentJumpCount = 0;
        m_fJumpTimer = 0.0;
        m_bIsVaulting = false;
        m_iVaultingFrameCount = 0;
        m_iVaultCooldownFrames = 0;
        
        // 初始化运动持续时间跟踪变量
        m_fExerciseDurationMinutes = 0.0;
        m_fLastUpdateTime = GetGame().GetWorld().GetWorldTime();
        m_bWasMoving = false;
        
        // 初始化"撞墙"阻尼过渡变量
        m_bInCollapseTransition = false;
        m_fCollapseTransitionStartTime = 0.0;
        
        // 初始化地形密度检测变量
        m_fCachedTerrainDensity = -1.0;
        m_fCachedTerrainFactor = 1.0;
        m_fLastTerrainCheckTime = 0.0;
        m_fLastMovementTime = 0.0;
        
        // 初始化网络同步容差优化变量
        m_fDeviationStartTime = -1.0;
        m_fTargetSpeedMultiplier = 1.0;
        m_fSmoothedSpeedMultiplier = 1.0;
        
        // 初始化疲劳积累系统变量
        m_fFatigueAccumulation = 0.0;
        m_fLastFatigueDecayTime = GetGame().GetWorld().GetWorldTime();
        m_fLastRestStartTime = -1.0;
        
        // 初始化负重缓存（事件驱动）
        m_fCachedCurrentWeight = 0.0;
        m_fCachedEncumbranceSpeedPenalty = 0.0;
        m_fCachedBodyMassPercent = 0.0;
        m_fCachedEncumbranceStaminaDrainMultiplier = 1.0;
        m_bEncumbranceCacheValid = false;
        m_pCachedInventoryComponent = null;
        
        // 获取并缓存库存组件引用（避免重复查找）
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (character)
        {
            m_pCachedInventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (m_pCachedInventoryComponent)
            {
                // 初始化时计算一次负重
                UpdateEncumbranceCache();
            }
        }
        
        // ==================== 初始化UI信号桥接系统 =====================
        // 获取信号管理器组件，用于更新官方UI特效和音效
        m_pSignalsManager = SignalsManagerComponent.Cast(owner.FindComponent(SignalsManagerComponent));
        if (m_pSignalsManager)
        {
            // 查找 "Exhaustion" 信号（官方UI模糊效果和呼吸声基于此信号）
            m_iExhaustionSignal = m_pSignalsManager.FindSignal("Exhaustion");
        }
        
        // 延迟初始化，确保组件完全加载
        GetGame().GetCallqueue().CallLater(StartSystem, 500, false);
    }
    
    // ==================== 性能优化：更新负重缓存（事件驱动）====================
    // 仅在库存变化时调用，避免每0.2秒重复计算
    // 注意：需要在库存组件的事件中调用此方法（如 OnItemAdded/OnItemRemoved）
    void UpdateEncumbranceCache()
    {
        IEntity owner = GetOwner();
        if (!owner || !m_pCachedInventoryComponent)
        {
            m_bEncumbranceCacheValid = false;
            return;
        }
        
        // 获取当前重量（这是唯一需要访问库存组件的地方）
        float currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
        if (currentWeight < 0.0)
        {
            m_bEncumbranceCacheValid = false;
            return;
        }
        
        // 更新缓存值
        m_fCachedCurrentWeight = currentWeight;
        
        // 计算有效负重（减去基准重量）
        float effectiveWeight = Math.Max(currentWeight - RealisticStaminaSpeedSystem.BASE_WEIGHT, 0.0);
        m_fCachedBodyMassPercent = effectiveWeight / RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
        
        // 计算速度惩罚（基于体重百分比）
        m_fCachedEncumbranceSpeedPenalty = RealisticStaminaSpeedSystem.ENCUMBRANCE_SPEED_PENALTY_COEFF * m_fCachedBodyMassPercent;
        m_fCachedEncumbranceSpeedPenalty = Math.Clamp(m_fCachedEncumbranceSpeedPenalty, 0.0, 0.5);
        
        // 计算体力消耗倍数
        m_fCachedEncumbranceStaminaDrainMultiplier = 1.0 + (RealisticStaminaSpeedSystem.ENCUMBRANCE_STAMINA_DRAIN_COEFF * m_fCachedBodyMassPercent);
        m_fCachedEncumbranceStaminaDrainMultiplier = Math.Clamp(m_fCachedEncumbranceStaminaDrainMultiplier, 1.0, 3.0);
        
        m_bEncumbranceCacheValid = true;
    }
    
    // ==================== 性能优化：检查并更新负重缓存（仅在变化时更新）====================
    // 在 UpdateSpeedBasedOnStamina 中调用，检查负重是否变化
    void CheckAndUpdateEncumbranceCache()
    {
        if (!m_pCachedInventoryComponent)
            return;
        
        // 快速检查：获取当前重量并与缓存比较
        float currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
        
        // 如果重量变化超过0.1kg，重新计算缓存（避免微小浮点误差触发）
        if (Math.AbsFloat(currentWeight - m_fCachedCurrentWeight) > 0.1 || !m_bEncumbranceCacheValid)
        {
            UpdateEncumbranceCache();
        }
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
        
        // 如果不在载具中，设置跳跃输入标志
        if (!isInVehicle)
        {
            m_bJumpInputTriggered = true;
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
        
        // 备用检测：如果在 OnPrepareControls 中检测到跳跃输入，也设置标志
        if (!isInVehicle && am.GetActionTriggered("Jump"))
        {
            m_bJumpInputTriggered = true;
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
        
        // ==================== 性能优化：使用缓存的负重值 ====================
        // 检查并更新负重缓存（仅在变化时重新计算）
        CheckAndUpdateEncumbranceCache();
        
        // 使用缓存的速度惩罚（避免重复计算）
        float encumbranceSpeedPenalty = 0.0;
        if (m_bEncumbranceCacheValid)
            encumbranceSpeedPenalty = m_fCachedEncumbranceSpeedPenalty;
        
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
        
        // ==================== "撞墙"临界点检测和5秒阻尼过渡 ====================
        // 检测体力是否刚从25%以上降下来
        float currentWorldTime = GetGame().GetWorld().GetWorldTime();
        bool justCrossedThreshold = false;
        
        if (m_fLastStaminaPercent >= COLLAPSE_THRESHOLD && staminaPercent < COLLAPSE_THRESHOLD)
        {
            // 体力刚从25%以上降下来，启动5秒阻尼过渡
            m_bInCollapseTransition = true;
            m_fCollapseTransitionStartTime = currentWorldTime;
            justCrossedThreshold = true;
        }
        
        // 检查5秒阻尼过渡是否已过期
        if (m_bInCollapseTransition)
        {
            float elapsedTime = currentWorldTime - m_fCollapseTransitionStartTime;
            if (elapsedTime >= COLLAPSE_TRANSITION_DURATION)
            {
                // 5秒阻尼过渡结束，恢复正常平滑过渡逻辑
                m_bInCollapseTransition = false;
            }
        }
        
        // 如果体力恢复到25%以上，取消阻尼过渡
        if (staminaPercent >= COLLAPSE_THRESHOLD)
        {
            m_bInCollapseTransition = false;
        }
        
        // 计算Run的基础速度倍数（包含双稳态-平台期和5秒阻尼过渡逻辑）
        float runBaseSpeedMultiplier = 0.0;
        
        // 如果在5秒阻尼过渡期间，使用时间插值来平滑过渡速度
        if (m_bInCollapseTransition)
        {
            // 计算阻尼过渡进度（0.0-1.0）
            float elapsedTime = currentWorldTime - m_fCollapseTransitionStartTime;
            float transitionProgress = elapsedTime / COLLAPSE_TRANSITION_DURATION;
            transitionProgress = Math.Clamp(transitionProgress, 0.0, 1.0);
            
            // 使用平滑的S型曲线（ease-in-out）来插值速度
            float smoothProgress = transitionProgress * transitionProgress * (3.0 - 2.0 * transitionProgress); // smoothstep
            
            // 计算目标速度（阻尼过渡开始时的速度）和结束速度（5%体力时的速度）
            float startSpeedMultiplier = RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER; // 25%体力时的速度（3.7 m/s）
            float endSpeedMultiplier = RealisticStaminaSpeedSystem.MIN_LIMP_SPEED_MULTIPLIER + (RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER - RealisticStaminaSpeedSystem.MIN_LIMP_SPEED_MULTIPLIER) * 0.8; // 5%体力时的速度（大约80%过渡位置）
            
            // 在5秒内平滑插值速度（从3.7 m/s逐渐降到约2.2 m/s）
            runBaseSpeedMultiplier = startSpeedMultiplier + (endSpeedMultiplier - startSpeedMultiplier) * smoothProgress;
        }
        else
        {
            // 正常情况：使用静态工具类计算速度（基于坡度自适应目标速度，包含平滑过渡逻辑）
            // 如果体力充足，就维持自适应速度；如果体力进入红区，就平滑降速
            runBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
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
            float runBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
            // Walk = Run × 0.7
            finalSpeedMultiplier = runBaseSpeedMultiplier * 0.7;
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
            // 获取当前重量用于验证
            float currentWeight = 0.0;
            if (m_bEncumbranceCacheValid)
                currentWeight = m_fCachedCurrentWeight;
            else
            {
                SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                    owner.FindComponent(SCR_CharacterInventoryStorageComponent));
                if (inventoryComponent)
                    currentWeight = inventoryComponent.GetTotalWeight();
            }
            
            // 定期发送状态到服务器端（每1秒发送一次，避免频繁网络调用）
            World worldCheck = GetGame().GetWorld();
            if (worldCheck)
            {
                float currentTime = worldCheck.GetWorldTime();
                if (currentTime - m_fLastNetworkSyncTime >= NETWORK_SYNC_INTERVAL)
                {
                    // 发送 RPC 到服务器端进行验证
                    RPC_UpdateStaminaState(staminaPercent, currentWeight, finalSpeedMultiplier);
                    m_fLastNetworkSyncTime = currentTime;
                }
            }
        }
        
        // ==================== 速度插值平滑处理 =====================
        // 问题：服务器端同步的速度倍数可能与客户端计算的值不同，立即切换会导致"拉回"感
        // 解决方案：使用插值平滑过渡，避免突然的速度变化
        
        // 如果服务器端验证的速度倍数与客户端计算的不同，使用插值平滑过渡
        float targetSpeedMultiplier = finalSpeedMultiplier;
        if (m_fServerValidatedSpeedMultiplier > 0.0 && m_fServerValidatedSpeedMultiplier != finalSpeedMultiplier)
        {
            // 检查是否需要使用服务器端的值（差异较大时）
            float serverDifference = Math.AbsFloat(finalSpeedMultiplier - m_fServerValidatedSpeedMultiplier);
            if (serverDifference > VALIDATION_TOLERANCE * 2.0) // 双倍容差时才使用服务器端值
            {
                // 使用服务器端的值作为目标，但使用插值平滑过渡
                targetSpeedMultiplier = m_fServerValidatedSpeedMultiplier;
            }
        }
        
        // 平滑插值：从当前速度倍数过渡到目标速度倍数
        // 使用线性插值，过渡时间为 SMOOTH_TRANSITION_DURATION
        float timeDelta = GetGame().GetWorld().GetWorldTime() - m_fLastUpdateTime;
        if (timeDelta > 0.0 && timeDelta < 1.0 && Math.AbsFloat(m_fSmoothedSpeedMultiplier - targetSpeedMultiplier) > 0.001)
        {
            // 计算插值系数（每秒过渡速度）
            float lerpSpeed = timeDelta / SMOOTH_TRANSITION_DURATION;
            lerpSpeed = Math.Clamp(lerpSpeed, 0.0, 1.0); // 限制在0-1之间
            
            // 线性插值
            m_fSmoothedSpeedMultiplier = Math.Lerp(m_fSmoothedSpeedMultiplier, targetSpeedMultiplier, lerpSpeed);
        }
        else
        {
            // 无需插值：直接使用目标值
            m_fSmoothedSpeedMultiplier = targetSpeedMultiplier;
        }
        
        // 始终更新速度（确保体力变化时立即生效）
        // 使用平滑后的速度倍数，避免突然的速度变化
        // 注意：在服务器端，这个调用会影响所有客户端（通过网络同步）
        // 在客户端，这个调用只影响本地玩家
        OverrideMaxSpeed(m_fSmoothedSpeedMultiplier);
        
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
        
        // ==================== 检测跳跃和翻越动作（改进：动作监听器检测）====================
        // 改进：使用动作监听器检测跳跃输入
        // 1. 动作监听器（OnJumpActionTriggered）- 实时检测输入动作
        // 2. OnPrepareControls - 备用检测方法，每帧检测输入动作
        // 
        // 注意：在 Arma Reforger 中，跳跃和翻越是同一个动作系统的不同表现
        // - 按跳跃键时，如果前方有障碍物，执行翻越/攀爬（IsClimbing() 返回 true）
        // - 按跳跃键时，如果前方无障碍物，执行普通跳跃
        // 因此，我们需要区分这两种情况：
        // 1. 翻越/攀爬：使用 IsClimbing() 检测
        // 2. 普通跳跃：使用动作监听器检测（OnJumpActionTriggered 或 OnPrepareControls）
        if (m_pStaminaComponent)
        {
            // 检测翻越/攀爬：使用 IsClimbing() 方法（更可靠）
            // 翻越的特征：角色处于攀爬状态（使用引擎提供的状态检测）
            bool isClimbing = IsClimbing();
            
            // 检测普通跳跃：使用动作检测（动作监听器或 OnPrepareControls）
            bool hasJumpInput = m_bJumpInputTriggered;
            
            // 如果检测到跳跃输入标志（且不在攀爬状态），则判定为跳跃
            if (!isClimbing && hasJumpInput)
            {
                // ==================== 低体力禁用跳跃 ====================
                // 体力 < 10% 时禁用跳跃（肌肉在力竭时无法提供爆发力）
                if (staminaPercent < RealisticStaminaSpeedSystem.JUMP_MIN_STAMINA_THRESHOLD)
                {
                    // 重置跳跃输入标志（避免重复触发）
                    m_bJumpInputTriggered = false;
                    // 不输出调试信息，避免刷屏
                }
                // ==================== 跳跃冷却检查 ====================
                // 冷却时间：3秒（15个更新周期，每0.2秒更新一次）
                // 3秒内都视为同一个跳跃动作，不会重复消耗
                else if (m_iJumpCooldownFrames == 0)
                {
                    // 获取当前时间
                    float currentTime = GetGame().GetWorld().GetWorldTime();
                    
                    // ==================== 连续跳跃惩罚（无氧欠债）====================
                    // 检测是否在3秒内连续跳跃
                    if (currentTime - m_fJumpTimer < RealisticStaminaSpeedSystem.JUMP_CONSECUTIVE_WINDOW)
                    {
                        // 在时间窗口内，增加连续跳跃计数
                        m_iRecentJumpCount++;
                    }
                    else
                    {
                        // 超出时间窗口，重置计数
                        m_iRecentJumpCount = 1;
                    }
                    
                    // 更新跳跃时间
                    m_fJumpTimer = currentTime;
                    
                    // ==================== 计算基础消耗（使用动态负重倍率）====================
                    // 获取当前总重量（包括身体重量和负重）
                    float currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 基础身体重量（90kg）
                    if (m_bEncumbranceCacheValid)
                        currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + m_fCachedCurrentWeight;
                    else
                    {
                        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                        if (character)
                        {
                            SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
                            if (inventoryComponent)
                                currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
                        }
                    }
                    
                    // 使用动态负重倍率计算基础消耗
                    // 公式：实际消耗 = 基础消耗 * (currentWeight / 90.0) ^ 1.5
                    float baseJumpCost = RealisticStaminaSpeedSystem.CalculateActionCost(
                        RealisticStaminaSpeedSystem.JUMP_STAMINA_BASE_COST, 
                        currentTotalWeight
                    );
                    
                    // ==================== 应用连续跳跃惩罚 ====================
                    // 每次连续跳跃额外增加50%消耗
                    // 第一次跳：1.0倍，第二次跳：1.5倍，第三次跳：2.0倍
                    float consecutiveMultiplier = 1.0 + (m_iRecentJumpCount - 1) * RealisticStaminaSpeedSystem.JUMP_CONSECUTIVE_PENALTY;
                    float finalJumpCost = baseJumpCost * consecutiveMultiplier;
                    
                    staminaPercent = staminaPercent - finalJumpCost;
                    
                    // 设置3秒冷却（15个更新周期，每0.2秒更新一次）
                    m_iJumpCooldownFrames = 15;
                    
                    // ==================== UI交互：更新Exhaustion信号 ====================
                    // 重载跳跃后，增加Exhaustion信号（触发深呼吸音效和视觉模糊）
                    if (m_pSignalsManager && m_iExhaustionSignal != -1)
                    {
                        // 计算Exhaustion增量：基础0.1 + 负重加成（最多0.2）
                        float exhaustionIncrement = 0.1;
                        if (currentTotalWeight > RealisticStaminaSpeedSystem.CHARACTER_WEIGHT)
                        {
                            float weightRatio = (currentTotalWeight - RealisticStaminaSpeedSystem.CHARACTER_WEIGHT) / RealisticStaminaSpeedSystem.MAX_ENCUMBRANCE_WEIGHT;
                            weightRatio = Math.Clamp(weightRatio, 0.0, 1.0);
                            exhaustionIncrement = 0.1 + (weightRatio * 0.1); // 0.1 - 0.2
                        }
                        
                        // 获取当前Exhaustion值并增加
                        float currentExhaustion = m_pSignalsManager.GetSignalValue(m_iExhaustionSignal);
                        float newExhaustion = Math.Clamp(currentExhaustion + exhaustionIncrement, 0.0, 1.0);
                        m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, newExhaustion);
                    }
                    
                    // 调试输出（仅在客户端）
                    if (owner == SCR_PlayerController.GetLocalControlledEntity())
                    {
                        PrintFormat("[RealisticSystem] 检测到跳跃动作！消耗体力: %1%% (基础: %2%%, 连续: %3次, 倍数: %4, 冷却: 3秒)", 
                            Math.Round(finalJumpCost * 100.0).ToString(),
                            Math.Round(baseJumpCost * 100.0).ToString(),
                            m_iRecentJumpCount.ToString(),
                            Math.Round(consecutiveMultiplier * 100.0) / 100.0);
                    }
                }
                
                // 重置跳跃输入标志（避免重复触发）
                m_bJumpInputTriggered = false;
            }
            
            if (isClimbing)
            {
                // 翻越冷却检查：防止在短时间内重复触发初始消耗
                // 冷却时间：5秒（25个更新周期，每0.2秒更新一次）
                // 5秒内都视为同一个翻越动作，不会重复消耗
                // 注意：冷却时间在函数末尾统一计时，这里只检查
                
                if (!m_bIsVaulting && m_iVaultCooldownFrames == 0)
                {
                    // ==================== 翻越起始消耗（使用动态负重倍率）====================
                    // 获取当前总重量（包括身体重量和负重）
                    float currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 基础身体重量（90kg）
                    if (m_bEncumbranceCacheValid)
                        currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + m_fCachedCurrentWeight;
                    else
                    {
                        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                        if (character)
                        {
                            SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
                            if (inventoryComponent)
                                currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
                        }
                    }
                    
                    // 使用动态负重倍率计算翻越起始消耗
                    float vaultCost = RealisticStaminaSpeedSystem.CalculateActionCost(
                        RealisticStaminaSpeedSystem.VAULT_STAMINA_START_COST, 
                        currentTotalWeight
                    );
                    
                    staminaPercent = staminaPercent - vaultCost;
                    m_bIsVaulting = true;
                    m_iVaultingFrameCount = 0;
                    m_iVaultCooldownFrames = 25; // 设置5秒冷却（25个更新周期，每0.2秒更新一次）
                    
                    // 调试输出（仅在客户端）
                    if (owner == SCR_PlayerController.GetLocalControlledEntity())
                    {
                        PrintFormat("[RealisticSystem] 检测到翻越动作！消耗体力: %1%% (基础: %2%%, 冷却: 5秒)", 
                            Math.Round(vaultCost * 100.0).ToString(),
                            Math.Round(RealisticStaminaSpeedSystem.VAULT_STAMINA_START_COST * 100.0).ToString());
                    }
                }
                else
                {
                    // ==================== 持续攀爬消耗（每秒1%）====================
                    // 对于高墙（Climb），在挂在墙上期间按时间持续扣除
                    // 每0.2秒消耗 0.01 / 5 = 0.002（每秒1%）
                    m_iVaultingFrameCount++;
                    if (m_iVaultingFrameCount >= 5) // 每1秒（5个更新周期）额外消耗
                    {
                        // 获取当前总重量（用于动态负重倍率）
                        float currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                        if (m_bEncumbranceCacheValid)
                            currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + m_fCachedCurrentWeight;
                        else
                        {
                            ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                            if (character)
                            {
                                SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
                                if (inventoryComponent)
                                    currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
                            }
                        }
                        
                        // 使用动态负重倍率计算持续攀爬消耗
                        float continuousClimbCost = RealisticStaminaSpeedSystem.CalculateActionCost(
                            RealisticStaminaSpeedSystem.CLIMB_STAMINA_TICK_COST, 
                            currentTotalWeight
                        );
                        
                        staminaPercent = staminaPercent - continuousClimbCost;
                        m_iVaultingFrameCount = 0;
                    }
                }
            }
            else
            {
                // 不在攀爬状态，结束翻越状态
                // 注意：冷却时间继续计时，确保5秒内都视为同一个翻越动作，不会重复消耗
                if (m_bIsVaulting)
                {
                    m_bIsVaulting = false;
                    m_iVaultingFrameCount = 0;
                    // 不重置冷却时间，让冷却继续计时，确保5秒内都视为同一个动作
                }
            }
            
            // 无论是否在攀爬状态，冷却时间都要继续计时
            // 这样可以确保5秒内都视为同一个翻越动作
            if (m_iVaultCooldownFrames > 0)
                m_iVaultCooldownFrames--;
            
            // 跳跃冷却时间继续计时（无论是否检测到跳跃输入）
            // 这样可以确保3秒内都视为同一个跳跃动作，不会重复消耗
            if (m_iJumpCooldownFrames > 0)
                m_iJumpCooldownFrames--;
            
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
        
        // ==================== 疲劳积累恢复机制 =====================
        // 疲劳积累需要长时间休息才能恢复（模拟身体恢复过程）
        // 只有静止时间超过 FATIGUE_DECAY_MIN_REST_TIME（60秒）后，疲劳才开始恢复
        // 注意：currentTime 和 isCurrentlyMoving 已在函数作用域内声明，这里直接使用
        // currentTime 在速度插值部分已计算（第670行附近）
        // isCurrentlyMoving 在第866行已声明，但如果之前没有声明，这里需要声明
        float currentTimeForFatigue = GetGame().GetWorld().GetWorldTime();
        bool isCurrentlyMovingForFatigue = (currentSpeed > 0.05);
        
        // 跟踪休息时间（用于疲劳恢复）
        if (!isCurrentlyMovingForFatigue)
        {
            // 静止：记录休息开始时间
            if (m_fLastRestStartTime < 0.0)
                m_fLastRestStartTime = currentTimeForFatigue;
            
            // 检查是否满足疲劳恢复条件（长时间休息）
            if (m_fFatigueAccumulation > 0.0)
            {
                float restDuration = currentTimeForFatigue - m_fLastRestStartTime;
                
                if (restDuration >= FATIGUE_DECAY_MIN_REST_TIME)
                {
                    // 满足长时间休息条件：开始疲劳恢复
                    // 注意：使用不同的变量名避免与第670行的 timeDelta 冲突
                    float fatigueTimeDelta = currentTimeForFatigue - m_fLastFatigueDecayTime;
                    if (fatigueTimeDelta > 0.0 && fatigueTimeDelta < 1.0)
                    {
                        // 疲劳逐渐恢复（每0.2秒恢复 FATIGUE_DECAY_RATE）
                        m_fFatigueAccumulation = Math.Max(m_fFatigueAccumulation - (FATIGUE_DECAY_RATE * (fatigueTimeDelta / 0.2)), 0.0);
                    }
                }
                // 否则：休息时间不足，疲劳不恢复
            }
            
            m_fLastFatigueDecayTime = currentTimeForFatigue;
        }
        else
        {
            // 运动：重置休息时间
            m_fLastRestStartTime = -1.0;
        }
        
        // ==================== 运动持续时间跟踪（累积疲劳）====================
        // 基于个性化运动建模（Palumbo et al., 2018）
        // 跟踪连续运动时间，用于计算累积疲劳
        // 注意：currentTime 在疲劳恢复部分已计算（currentTimeForFatigue）
        float currentTimeForExercise = GetGame().GetWorld().GetWorldTime();
        bool isCurrentlyMoving = (currentSpeed > 0.05);
        
        if (isCurrentlyMoving)
        {
            if (m_bWasMoving)
            {
                // 继续运动：累积运动时间
                // 注意：使用不同的变量名避免与第670行的 timeDelta 冲突
                float exerciseTimeDelta = currentTimeForExercise - m_fLastUpdateTime;
                if (exerciseTimeDelta > 0.0 && exerciseTimeDelta < 1.0) // 防止时间跳跃异常
                {
                    m_fExerciseDurationMinutes += exerciseTimeDelta / 60.0; // 转换为分钟
                }
            }
            else
            {
                // 从静止转为运动：重置运动时间和休息时间
                m_fExerciseDurationMinutes = 0.0;
                m_fRestDurationMinutes = 0.0;
            }
            m_bWasMoving = true;
        }
        else
        {
            // 静止：累积休息时间，并逐渐减少运动时间（疲劳恢复）
            // 注意：使用不同的变量名避免与第670行的 timeDelta 冲突
            float restTimeDelta = currentTimeForExercise - m_fLastUpdateTime;
            if (restTimeDelta > 0.0 && restTimeDelta < 1.0)
            {
                // 累积休息时间（用于多维度恢复模型）
                if (m_bWasMoving)
                {
                    // 从运动转为静止：重置休息时间
                    m_fRestDurationMinutes = 0.0;
                }
                else
                {
                    // 继续休息：累积休息时间
                    m_fRestDurationMinutes += restTimeDelta / 60.0; // 转换为分钟
                }
                
                // 静止时，疲劳恢复速度是累积速度的2倍（快速恢复）
                if (m_fExerciseDurationMinutes > 0.0)
                {
                    m_fExerciseDurationMinutes = Math.Max(m_fExerciseDurationMinutes - (restTimeDelta / 60.0 * 2.0), 0.0);
                }
            }
            m_bWasMoving = false;
        }
        m_fLastUpdateTime = currentTimeForExercise;
        
        // 计算累积疲劳因子（基于运动持续时间）
        // 公式：fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × max(0, exercise_duration - FATIGUE_START_TIME)
        float effectiveExerciseDuration = Math.Max(m_fExerciseDurationMinutes - RealisticStaminaSpeedSystem.FATIGUE_START_TIME_MINUTES, 0.0);
        float fatigueFactor = 1.0 + (RealisticStaminaSpeedSystem.FATIGUE_ACCUMULATION_COEFF * effectiveExerciseDuration);
        fatigueFactor = Math.Clamp(fatigueFactor, 1.0, RealisticStaminaSpeedSystem.FATIGUE_MAX_FACTOR); // 限制在1.0-2.0之间
        
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
        
        // ==================== 性能优化：使用缓存的当前重量 ====================
        // 使用缓存的当前重量（避免重复查找组件）
        float currentWeight = 0.0;
        if (m_bEncumbranceCacheValid)
            currentWeight = m_fCachedCurrentWeight;
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
        
        // 获取坡度信息
        float slopeAngleDegrees = 0.0;
        float gradePercent = 0.0;
        
        // 检查是否在攀爬或跳跃状态（改进：使用动作监听器检测的跳跃输入）
        bool isClimbingForSlope = IsClimbing();
        bool isJumpingForSlope = m_bJumpInputTriggered;
        
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
        
        // ==================== 地形系数获取（基于密度插值映射）====================
        // 优化检测频率：移动时0.5秒检测一次，静止时2秒检测一次（性能优化）
        // 注意：isCurrentlyMoving 已在第779行声明，此处直接使用
        float terrainFactor = 1.0; // 默认值（铺装路面）
        
        // 更新移动时间（用于判断静止状态）
        // 注意：使用 currentTimeForExercise（已在第909行声明）
        if (isCurrentlyMoving)
        {
            m_fLastMovementTime = currentTimeForExercise;
        }
        
        // 检查是否需要更新地形系数
        // 移动时：每0.5秒检测一次
        // 静止时：每2秒检测一次（或停止检测）
        bool shouldCheckTerrain = false;
        float terrainCheckInterval = TERRAIN_CHECK_INTERVAL;
        
        if (isCurrentlyMoving)
        {
            // 移动中：正常频率检测
            shouldCheckTerrain = (currentTimeForExercise - m_fLastTerrainCheckTime > terrainCheckInterval);
        }
        else
        {
            // 静止时：检查静止时间
            float idleDuration = currentTimeForExercise - m_fLastMovementTime;
            if (idleDuration < IDLE_THRESHOLD_TIME)
            {
                // 刚停止移动（<1秒）：继续检测（因为可能还在变化地形）
                shouldCheckTerrain = (currentTimeForExercise - m_fLastTerrainCheckTime > terrainCheckInterval);
            }
            else
            {
                // 长时间静止（≥1秒）：降低检测频率或停止检测
                terrainCheckInterval = TERRAIN_CHECK_INTERVAL_IDLE; // 2秒
                shouldCheckTerrain = (currentTimeForExercise - m_fLastTerrainCheckTime > terrainCheckInterval);
            }
        }
        
        if (shouldCheckTerrain)
        {
            // 获取地面密度
            float density = GetTerrainDensity(owner);
            m_fCachedTerrainDensity = density;
            
            // 根据密度计算地形系数（插值映射）
            if (density >= 0.0)
                m_fCachedTerrainFactor = RealisticStaminaSpeedSystem.GetTerrainFactorFromDensity(density);
            
            m_fLastTerrainCheckTime = currentTimeForExercise;
        }
        
        // 使用缓存的地形系数
        terrainFactor = m_fCachedTerrainFactor;
        
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
        
        // ==================== 性能优化：使用缓存的负重消耗倍数 ====================
        // 使用缓存的体力消耗倍数（避免重复计算）
        float encumbranceStaminaDrainMultiplier = 1.0;
        if (m_bEncumbranceCacheValid)
            encumbranceStaminaDrainMultiplier = m_fCachedEncumbranceStaminaDrainMultiplier;
        
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
            
            // 计算超出生理上限的消耗（用于疲劳积累）
            float excessDrainRate = totalDrainRate - MAX_DRAIN_RATE_PER_TICK;
            if (excessDrainRate > 0.0)
            {
                // 将超出消耗转化为疲劳积累
                // 疲劳积累速度 = 超出消耗 × 转换系数
                // 例如：超出0.01（50%超负荷）→ 疲劳积累 +0.0005
                const float FATIGUE_CONVERSION_COEFF = 0.05; // 转换系数（5%）
                float fatigueGain = excessDrainRate * FATIGUE_CONVERSION_COEFF;
                m_fFatigueAccumulation = Math.Clamp(m_fFatigueAccumulation + fatigueGain, 0.0, MAX_FATIGUE_PENALTY);
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
                // ==================== 性能优化：使用缓存的当前重量 ====================
                // 使用缓存的当前重量（避免重复查找组件）
                float currentWeightForRecovery = 0.0;
                if (m_bEncumbranceCacheValid)
                    currentWeightForRecovery = m_fCachedCurrentWeight;
                
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
                
                float recoveryRate = RealisticStaminaSpeedSystem.CalculateMultiDimensionalRecoveryRate(
                    staminaPercent, 
                    m_fRestDurationMinutes, 
                    m_fExerciseDurationMinutes,
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
            
            // ==================== 应用疲劳惩罚：限制最大体力上限 =====================
            // 疲劳积累会降低最大体力上限（例如：30%疲劳 → 最大体力上限70%）
            // 这模拟了超负荷行为对身体造成的累积伤害
            float maxStaminaCap = 1.0 - m_fFatigueAccumulation; // 最大体力上限（考虑疲劳）
            
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
        
        // ==================== UI信号桥接系统：更新官方UI特效和音效 =====================
        // 问题：自定义体力系统接管了体力逻辑，官方UI特效（模糊、呼吸声）可能失效
        // 解决方案：通过 SignalsManagerComponent 手动更新 "Exhaustion" 信号
        // 
        // 官方UI特效阈值：STAMINA_EFFECT_THRESHOLD = 0.45
        // 拟真模型崩溃点：EXHAUSTION_THRESHOLD = 0.25
        // 
        // 映射逻辑：
        // 1. 基础模糊（由体力剩余量决定）
        // 2. 瞬时模糊（由运动强度/消耗功率决定）
        // 3. 崩溃期强化（体力 < 25% 时模糊感剧增）
        if (m_iExhaustionSignal != -1 && m_pSignalsManager)
        {
            // 检查是否精疲力尽
            // 注意：isExhausted 已在函数作用域内声明（第437行），这里直接使用
            
            float uiExhaustion = 0.0;
            
            if (isExhausted)
            {
                // 彻底没气了，满模糊（达到或超过官方阈值 0.45）
                uiExhaustion = 1.0;
            }
            else
            {
                // 综合考虑体力剩余量和瞬时消耗功率来决定UI表现
                // 基础模糊：由体力剩余量决定（体力越低，模糊越强）
                float fatigueBase = 1.0 - staminaPercent;
                
                // 瞬时模糊：由运动强度/消耗功率决定（消耗越大，模糊越强）
                // totalDrainRate 约 0.001-0.02（每0.2秒），乘以系数转换为信号值
                // 例如：totalDrainRate = 0.01 → intensityBoost = 0.5（50%瞬时模糊）
                float intensityBoost = 0.0;
                if (currentSpeed > 0.05)
                {
                    // 计算瞬时消耗功率（归一化）
                    // totalDrainRate 通常在 0.001-0.02 之间（每0.2秒）
                    // 归一化到 0.0-1.0 范围，然后乘以强度系数
                    float normalizedDrainRate = Math.Clamp(totalDrainRate / 0.02, 0.0, 1.0); // 假设最大消耗为0.02
                    intensityBoost = normalizedDrainRate * 0.5; // 瞬时模糊最高50%
                }
                
                // 崩溃期强化：体力 < 25% 时模糊感剧增
                // 官方阈值是 0.45，拟真模型的崩溃点是 0.25
                // 当体力掉到 0.25 时，信号值应该超过 0.45，立即触发视觉模糊
                if (staminaPercent <= 0.25)
                {
                    // 非线性映射：进入崩溃期，模糊感呈指数级加强
                    // 0.25 → 0.5, 0.1 → 0.8, 0.0 → 1.0
                    float collapseFactor = (0.25 - staminaPercent) / 0.25; // 0.0-1.0
                    float collapseBoost = 0.3 + (collapseFactor * 0.5); // 0.3-0.8（额外强化）
                    uiExhaustion = fatigueBase + intensityBoost + collapseBoost;
                }
                else
                {
                    // 正常期：基础模糊 + 瞬时模糊
                    uiExhaustion = fatigueBase + intensityBoost;
                }
            }
            
            // 限制信号值在有效范围内（0.0-1.0）
            uiExhaustion = Math.Clamp(uiExhaustion, 0.0, 1.0);
            
            // 更新信号值（这会让官方UI特效和音效系统响应）
            m_pSignalsManager.SetSignalValue(m_iExhaustionSignal, uiExhaustion);
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
                
                // ==================== 地形系数调试：获取并显示地面密度 =====================
                // 每0.5秒检测一次地面密度（性能优化）
				 float currentTime = GetGame().GetWorld().GetWorldTime();
                if (currentTime - m_fLastTerrainCheckTime > TERRAIN_CHECK_INTERVAL)
                {
                    m_fCachedTerrainDensity = GetTerrainDensity(owner);
                    m_fLastTerrainCheckTime = currentTime;
                }
                
                // 构建地形密度信息字符串
                string terrainInfo = "";
                if (m_fCachedTerrainDensity >= 0.0)
                {
                    terrainInfo = string.Format(" | 地面密度: %1", 
                        Math.Round(m_fCachedTerrainDensity * 100.0) / 100.0);
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
        float staminaChange = Math.AbsFloat(staminaPercent - m_fLastReportedStaminaPercent);
        if (staminaChange > 0.5) // 单次更新最多变化50%
        {
            // 如果变化过快，记录警告（仅在调试模式下）
            // PrintFormat("[RealisticSystem] 警告：体力值变化过快！上次=%1, 当前=%2, 变化=%3", 
            //     m_fLastReportedStaminaPercent.ToString(), 
            //     staminaPercent.ToString(), 
            //     staminaChange.ToString());
            // 允许但记录警告（可能是正常的快速消耗）
        }
        
        // 更新记录的值
        m_fLastReportedStaminaPercent = staminaPercent;
        m_fLastReportedWeight = weight;
        
        return true;
    }
    
    // ==================== 地形系数调试：获取地面密度 =====================
    // 使用射线追踪获取角色脚下的地面材质密度
    // 参考：SCR_RecoilForceAimModifier 的实现方式
    // 
    // @param owner 角色实体
    // @return 地面密度值（-1 表示获取失败）
    float GetTerrainDensity(IEntity owner)
    {
        if (!owner)
            return -1.0;
        
        // 执行射线追踪检测地面
        TraceParam paramGround = new TraceParam();
        paramGround.Start = owner.GetOrigin() + (vector.Up * 0.1); // 角色位置上方 0.1 米
        paramGround.End = paramGround.Start - (vector.Up * 0.5); // 向下追踪 0.5 米
        paramGround.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        paramGround.Exclude = owner;
        // 优化：使用更合适的物理层以确保稳定获取地面材质
        // 建议：EPhysicsLayerPresets.Character 或 EPhysicsLayerPresets.Static 可能更稳定
        // 如果 Projectile 层在某些情况下无法命中地面，可以尝试其他层
        paramGround.LayerMask = EPhysicsLayerPresets.Projectile; // 当前使用 Projectile（与官方示例一致）
        
        // 执行追踪
        owner.GetWorld().TraceMove(paramGround, FilterTerrainCallback);
        
        // 获取表面材质
        GameMaterial material = paramGround.SurfaceProps;
        if (!material)
            return -1.0;
        
        // 获取弹道信息（包含密度）
        BallisticInfo ballisticInfo = material.GetBallisticInfo();
        if (!ballisticInfo)
            return -1.0;
        
        // 返回密度值
        float density = ballisticInfo.GetDensity();
        return density;
    }
    
    // 地形检测过滤回调（排除角色实体）
    bool FilterTerrainCallback(IEntity e)
    {
        if (ChimeraCharacter.Cast(e))
            return false;
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
        
        // 计算速度差异
        float speedDifference = Math.AbsFloat(speedMultiplier - serverSpeedMultiplier);
        float currentTime = GetGame().GetWorld().GetWorldTime();
        
        if (speedDifference > VALIDATION_TOLERANCE)
        {
            // 偏差超过容差：检查是否为连续偏差
            if (m_fDeviationStartTime < 0.0)
            {
                // 首次检测到偏差：记录开始时间
                m_fDeviationStartTime = currentTime;
            }
            else
            {
                // 已有偏差：检查持续时间
                float deviationDuration = currentTime - m_fDeviationStartTime;
                
                if (deviationDuration >= DEVIATION_TRIGGER_DURATION)
                {
                    // 连续偏差持续时间超过阈值：触发同步
                    // 客户端速度倍数偏差过大，同步回服务器端的结果
                    RPC_SyncStaminaState(staminaPercent, weight, serverSpeedMultiplier);
                    
                    // 保存服务器端验证的速度倍数
                    m_fServerValidatedSpeedMultiplier = serverSpeedMultiplier;
                    
                    // 重置偏差计时器
                    m_fDeviationStartTime = -1.0;
                }
                // 否则：偏差持续时间不足，不触发同步（容忍小幅度偏差）
            }
        }
        else
        {
            // 偏差在容差范围内：验证通过
            // 重置偏差计时器
            m_fDeviationStartTime = -1.0;
            
            // 保存验证结果
            m_fServerValidatedSpeedMultiplier = serverSpeedMultiplier;
            m_fLastReportedStaminaPercent = staminaPercent;
            m_fLastReportedWeight = weight;
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
        
        // 保存服务器端同步的值
        m_fServerValidatedSpeedMultiplier = speedMultiplier;
        
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
