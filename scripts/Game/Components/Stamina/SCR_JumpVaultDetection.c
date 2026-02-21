// 跳跃和翻越检测模块
// 负责处理跳跃输入检测、翻越检测、冷却机制和连续跳跃惩罚
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class JumpVaultDetector
{
    // ==================== 状态变量 ====================
    // 跳跃相关
    protected bool m_bJumpInputTriggered = false; // 跳跃输入是否被触发（由动作监听器设置）
    protected int m_iJumpCooldownFrames = 0; // 跳跃冷却帧数（防止重复触发，2秒冷却）
    protected ECharacterStance m_eLastStance; // 上一帧姿态（用于判断是否从趴/蹲姿跳跃）
    
    // 连续跳跃惩罚（无氧欠债）机制
    protected int m_iRecentJumpCount = 0; // 连续跳跃计数
    protected float m_fJumpTimer = 0.0; // 上次跳跃时间（秒）
    
    // 翻越相关
    protected bool m_bIsVaulting = false; // 是否正在翻越/攀爬
    protected int m_iVaultingFrameCount = 0; // 翻越状态持续帧数
    protected int m_iVaultCooldownFrames = 0; // 翻越冷却帧数（防止重复触发）
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    void Initialize()
    {
        m_bJumpInputTriggered = false;
        m_iJumpCooldownFrames = 0;
        m_eLastStance = ECharacterStance.STAND;
        m_iRecentJumpCount = 0;
        m_fJumpTimer = 0.0;
        m_bIsVaulting = false;
        m_iVaultingFrameCount = 0;
        m_iVaultCooldownFrames = 0;
    }
    
    // 设置跳跃输入标志（由动作监听器调用）
    void SetJumpInputTriggered(bool value)
    {
        m_bJumpInputTriggered = value;
    }
    
    // 获取跳跃输入标志
    bool GetJumpInputTriggered()
    {
        return m_bJumpInputTriggered;
    }
    
    // 检查并处理跳跃逻辑
    // @param owner 角色实体
    // @param controller 角色控制器组件（用于调用IsClimbing等方法）
    // @param staminaPercent 当前体力百分比（用于判断是否允许跳跃）
    // @param encumbranceCacheValid 负重缓存是否有效
    // @param cachedCurrentWeight 缓存的当前重量
    // @param signalsManager UI信号管理器（可选，用于更新Exhaustion信号）
    // @param exhaustionSignal Exhaustion信号ID（可选）
    // @return 体力消耗值（如果为0，表示没有发生跳跃消耗）
    float ProcessJump(IEntity owner, SCR_CharacterControllerComponent controller, float staminaPercent, 
                     bool encumbranceCacheValid, float cachedCurrentWeight,
                     SignalsManagerComponent signalsManager = null, int exhaustionSignal = -1)
    {
        if (!owner || !controller)
            return 0.0;
        
        // 获取当前姿态
        ECharacterStance currentStance = controller.GetStance();
        
        // 检测翻越/攀爬：使用 IsClimbing() 方法（更可靠）
        bool isClimbing = controller.IsClimbing();
        
        // 检测普通跳跃：使用动作检测（动作监听器或 OnPrepareControls）
        bool hasJumpInput = m_bJumpInputTriggered;
        
        // 如果检测到跳跃输入标志（且不在攀爬状态），则判定为跳跃
        if (!isClimbing && hasJumpInput)
        {
            // 检查前一帧姿态：如果从趴着或蹲着跳跃，则不算跳跃消耗，而是姿态变换
            if (m_eLastStance == ECharacterStance.PRONE || m_eLastStance == ECharacterStance.CROUCH)
            {
                // 保存原始姿态名称（在更新之前）
                string originalStanceName = GetStanceName(m_eLastStance);
                
                // 从趴/蹲姿跳跃，不算跳跃消耗，由姿态转换系统处理
                m_bJumpInputTriggered = false;
                m_eLastStance = currentStance;
                
                // 调试输出（仅在客户端）
                if (StaminaConstants.IsDebugEnabled() && owner == SCR_PlayerController.GetLocalControlledEntity())
                {
                    PrintFormat("[RealisticSystem] 从%1姿态跳跃，不计入跳跃消耗，由姿态转换系统处理 / Jump from %1 stance, handled by stance transition system", originalStanceName);
                }
                
                return 0.0;
            }
            
            // 跳跃冷却检查：2秒冷却时间（10个更新周期）
            if (m_iJumpCooldownFrames > 0)
            {
                // 在冷却中，拦截动作输入，不让游戏引擎执行跳跃
                m_bJumpInputTriggered = false;
                if (StaminaConstants.IsVerboseLoggingEnabled())
                    Print("[RealisticSystem] 跳跃冷却中，拦截动作输入！/ Jump Cooldown Active, Blocking Input!");
                m_eLastStance = currentStance;
                return 0.0;
            }
            
            // 低体力禁用跳跃：体力 < 10% 时禁用跳跃
            if (staminaPercent < RealisticStaminaSpeedSystem.JUMP_MIN_STAMINA_THRESHOLD)
            {
                m_bJumpInputTriggered = false;
                m_eLastStance = currentStance;
                return 0.0;
            }
            {
                float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
                
                // 连续跳跃惩罚（无氧欠债）：检测是否在2秒内连续跳跃
                if (currentTime - m_fJumpTimer < RealisticStaminaSpeedSystem.JUMP_CONSECUTIVE_WINDOW)
                {
                    m_iRecentJumpCount++;
                }
                else
                {
                    m_iRecentJumpCount = 1;
                }
                
                m_fJumpTimer = currentTime;
                
                // 获取当前总重量（包括身体重量和负重）
                float currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                if (encumbranceCacheValid)
                    currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + cachedCurrentWeight;
                else
                {
                    ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                    if (character)
                    {
                        SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                            character.FindComponent(SCR_CharacterInventoryStorageComponent));
                        if (inventoryComponent)
                            currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
                    }
                }
                
                float finalJumpCost = 0.0;
                // 物理模型计算跳跃消耗
                float eta = StaminaConstants.GetJumpEfficiency();     // [SOFT] 从 Settings 读取，生理学约束 0.20-0.25
                float hguess = StaminaConstants.GetJumpHeightGuess(); // [SOFT] 重心抬升高度估算
                float vguess = StaminaConstants.GetJumpHorizSpeedGuess(); // [SOFT] 水平速度估算
                finalJumpCost = RealisticStaminaSpeedSystem.ComputeJumpCostPhys(
                    currentTotalWeight, hguess, vguess, eta);
                
                // 应用连续跳跃惩罚：每次连续跳跃额外增加50%消耗
                float consecutiveMultiplier = 1.0 + (m_iRecentJumpCount - 1) * RealisticStaminaSpeedSystem.JUMP_CONSECUTIVE_PENALTY;
                finalJumpCost *= consecutiveMultiplier;
                
                // 设置2秒冷却（10个更新周期）
                m_iJumpCooldownFrames = 10;
                
                // UI交互：更新Exhaustion信号
                if (signalsManager && exhaustionSignal != -1)
                {
                    float exhaustionIncrement = 0.1;
                    if (currentTotalWeight > RealisticStaminaSpeedSystem.CHARACTER_WEIGHT)
                    {
                        float weightRatio = (currentTotalWeight - RealisticStaminaSpeedSystem.CHARACTER_WEIGHT) / RealisticStaminaSpeedSystem.MAX_ENCUMBRANCE_WEIGHT;
                        weightRatio = Math.Clamp(weightRatio, 0.0, 1.0);
                        exhaustionIncrement = 0.1 + (weightRatio * 0.1); // 0.1 - 0.2
                    }
                    
                    float currentExhaustion = signalsManager.GetSignalValue(exhaustionSignal);
                    float newExhaustion = Math.Clamp(currentExhaustion + exhaustionIncrement, 0.0, 1.0);
                    signalsManager.SetSignalValue(exhaustionSignal, newExhaustion);
                }
                
                // 调试输出（仅在客户端）
                if (StaminaConstants.IsDebugEnabled() && owner == SCR_PlayerController.GetLocalControlledEntity())
                {
                    PrintFormat("[RealisticSystem] 检测到跳跃动作！消耗体力: %1%% (连续: %2次, 倍数: %3, 冷却: 2秒)", 
                        Math.Round(finalJumpCost * 100.0).ToString(),
                        m_iRecentJumpCount.ToString(),
                        Math.Round(consecutiveMultiplier * 100.0) / 100.0);
                }
                
                m_bJumpInputTriggered = false;
                m_eLastStance = currentStance;
                return finalJumpCost;
            }
            
            m_bJumpInputTriggered = false;
            m_eLastStance = currentStance;
        }
        
        // 更新上一帧姿态
        m_eLastStance = currentStance;
        
        return 0.0;
    }
    
    // 检查并处理翻越逻辑
    // @param owner 角色实体
    // @param controller 角色控制器组件
    // @param encumbranceCacheValid 负重缓存是否有效
    // @param cachedCurrentWeight 缓存的当前重量
    // @return 体力消耗值（如果为0，表示没有发生翻越消耗）
    float ProcessVault(IEntity owner, SCR_CharacterControllerComponent controller,
                      bool encumbranceCacheValid, float cachedCurrentWeight)
    {
        if (!owner || !controller)
            return 0.0;
        
        bool isClimbing = controller.IsClimbing();
        float totalCost = 0.0;
        
        if (isClimbing)
        {
            // 翻越冷却检查：防止在短时间内重复触发初始消耗
            // 冷却时间：5秒（25个更新周期）
            if (m_iVaultCooldownFrames > 0)
            {
                // 在冷却中，拦截动作输入，不让游戏引擎执行攀爬
                if (StaminaConstants.IsVerboseLoggingEnabled())
                    Print("[RealisticSystem] 攀爬冷却中，拦截动作输入！/ Vault Cooldown Active, Blocking Input!");
                return 0.0;
            }
            
            if (!m_bIsVaulting && m_iVaultCooldownFrames == 0)
            {
                // 翻越起始消耗（使用动态负重倍率）
                float currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                if (encumbranceCacheValid)
                    currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + cachedCurrentWeight;
                else
                {
                    ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                    if (character)
                    {
                        SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                            character.FindComponent(SCR_CharacterInventoryStorageComponent));
                        if (inventoryComponent)
                            currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
                    }
                }
                
                float vaultCost = 0.0;
                // 物理模型计算翻越初始消耗
                // [HARD] eta_iso 从 Settings 读取（生理学约束 0.10-0.15），使用 GetClimbIsoEfficiency() 而非硬编码
                float eta_iso = StaminaConstants.GetClimbIsoEfficiency();
                float vert = StaminaConstants.VAULT_VERT_LIFT_GUESS;          // [HARD] 垂直抬升估算 0.5m
                float limbForce = currentTotalWeight * StaminaConstants.VAULT_LIMB_FORCE_RATIO; // [HARD] 四肢力 = 总重 × 0.5
                vaultCost = RealisticStaminaSpeedSystem.ComputeClimbCostPhys(
                    currentTotalWeight, vert, limbForce, eta_iso);
                
                totalCost = vaultCost;
                m_bIsVaulting = true;
                m_iVaultingFrameCount = 0;
                m_iVaultCooldownFrames = 25; // 5秒冷却
                
                // 调试输出（仅在客户端）
                if (StaminaConstants.IsDebugEnabled() && owner == SCR_PlayerController.GetLocalControlledEntity())
                {
                    PrintFormat("[RealisticSystem] 检测到翻越动作！消耗体力: %1%% (冷却: 5秒)", 
                        Math.Round(vaultCost * 100.0).ToString());
                }
            }
            else
            {
                // 持续攀爬消耗（每秒1%）
                m_iVaultingFrameCount++;
                if (m_iVaultingFrameCount >= 5) // 每1秒（5个更新周期）额外消耗
                {
                    float currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                    if (encumbranceCacheValid)
                        currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + cachedCurrentWeight;
                    else
                    {
                        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                        if (character)
                        {
                            SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                                character.FindComponent(SCR_CharacterInventoryStorageComponent));
                            if (inventoryComponent)
                                currentTotalWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
                        }
                    }
                    
                    float continuousClimbCost = 0.0;
                    // 物理模型计算持续攀爬消耗（参数与翻越初始消耗完全一致，从 Settings 读取）
                    float eta_iso = StaminaConstants.GetClimbIsoEfficiency();
                    float vertSpeed = StaminaConstants.VAULT_VERT_LIFT_GUESS;
                    float limbForce = currentTotalWeight * StaminaConstants.VAULT_LIMB_FORCE_RATIO;
                    continuousClimbCost = RealisticStaminaSpeedSystem.ComputeClimbCostPhys(
                        currentTotalWeight, vertSpeed, limbForce, eta_iso);
                    totalCost = continuousClimbCost;
                    m_iVaultingFrameCount = 0;
                }
            }
        }
        else
        {
            // 不在攀爬状态，结束翻越状态
            if (m_bIsVaulting)
            {
                m_bIsVaulting = false;
                m_iVaultingFrameCount = 0;
            }
        }
        
        return totalCost;
    }
    
    // 更新冷却时间（每0.2秒调用一次）
    void UpdateCooldowns()
    {
        if (m_iVaultCooldownFrames > 0)
            m_iVaultCooldownFrames--;
        
        if (m_iJumpCooldownFrames > 0)
            m_iJumpCooldownFrames--;
    }
    
    // 获取是否正在翻越
    bool IsVaulting()
    {
        return m_bIsVaulting;
    }
    
    // 检查是否在跳跃冷却中
    bool IsJumpOnCooldown()
    {
        return m_iJumpCooldownFrames > 0;
    }
    
    // 获取姿态名称（用于调试输出）
    // @param stance 姿态
    // @return 姿态名称字符串
    protected string GetStanceName(ECharacterStance stance)
    {
        if (stance == ECharacterStance.STAND)
            return "站姿/STAND";
        else if (stance == ECharacterStance.CROUCH)
            return "蹲姿/CROUCH";
        else if (stance == ECharacterStance.PRONE)
            return "趴姿/PRONE";
        else
            return "未知/UNKNOWN";
    }
}
