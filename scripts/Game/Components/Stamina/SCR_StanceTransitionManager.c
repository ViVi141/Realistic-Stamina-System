// 姿态转换管理模块 2.0（乳酸堆积模型）
// 负责处理姿态转换的体力消耗计算
// 核心思路：单次动作很轻，连续动作剧增。模拟肌肉在连续负重深蹲时从有氧到无氧的转变。
// 生理学依据：肌肉在连续爆发时会产生乳酸堆积，导致肌肉疲劳和力量下降
// 参考文献：Knapik et al., 1996; Pandolf et al., 1977; Brooks et al., 2000

class StanceTransitionManager
{
    // ==================== 状态变量 ====================
    
    // 上一帧姿态（用于检测变化）
    protected ECharacterStance m_eLastStance;
    
    // 是否已初始化（避免第一帧误判）
    protected bool m_bInitialized;
    
    // ==================== 疲劳堆积器（Stance Fatigue Accumulator）====================
    // 核心机制：引入隐藏变量 m_fStanceFatigue，模拟肌肉在连续负重深蹲时的乳酸堆积
    // 增加：每次变换姿态，这个变量增加 1.0
    // 衰减：每秒钟自动减少 0.5
    // 影响：实际体力消耗 = BaseCost × WeightFactor × (1.0 + m_fStanceFatigue)
    
    protected float m_fStanceFatigue; // 疲劳堆积值
    
    // ==================== 公共方法 ====================
    
    // 初始化状态
    void Initialize()
    {
        m_eLastStance = ECharacterStance.STAND;
        m_bInitialized = false;
        m_fStanceFatigue = 0.0;
    }
    
    // 设置初始姿态（避免第一帧误判）
    void SetInitialStance(ECharacterStance stance)
    {
        m_eLastStance = stance;
        m_bInitialized = true;
    }
    
    // 更新疲劳堆积（每帧调用）
    // @param deltaTime 帧时间（秒）
    void UpdateFatigue(float deltaTime)
    {
        // 疲劳堆积自动衰减：每秒衰减 0.5
        float decayAmount = StaminaConstants.STANCE_FATIGUE_DECAY * deltaTime;
        m_fStanceFatigue = Math.Max(0.0, m_fStanceFatigue - decayAmount);
    }
    
    // 处理姿态转换逻辑
    // @param owner 角色实体
    // @param controller 角色控制器组件
    // @param staminaPercent 当前体力百分比
    // @param encumbranceCacheValid 负重缓存是否有效
    // @param cachedCurrentWeight 缓存的当前重量
    // @return 体力消耗值（如果为0，表示没有发生姿态转换）
    float ProcessStanceTransition(
        IEntity owner,
        SCR_CharacterControllerComponent controller,
        float staminaPercent,
        bool encumbranceCacheValid,
        float cachedCurrentWeight)
    {
        if (!owner || !controller || !m_bInitialized)
            return 0.0;
        
        // 获取当前姿态
        ECharacterStance currentStance = controller.GetStance();
        
        // 判断姿态是否发生变化
        if (currentStance == m_eLastStance)
            return 0.0;
        
        // 计算姿态转换消耗
        float transitionCost = CalculateStanceTransitionCost(
            m_eLastStance,
            currentStance,
            staminaPercent,
            encumbranceCacheValid,
            cachedCurrentWeight,
            owner
        );
        
        // 更新上一帧姿态
        m_eLastStance = currentStance;
        
        // 增加疲劳堆积（每次姿态转换增加 1.0）
        m_fStanceFatigue = Math.Min(m_fStanceFatigue + StaminaConstants.STANCE_FATIGUE_ACCUMULATION, 
                                     StaminaConstants.STANCE_FATIGUE_MAX);
        
        return transitionCost;
    }
    
    // ==================== 核心计算方法 ====================
    
    // 计算姿态转换消耗（v2.0 乳酸堆积模型）
    // @param oldStance 旧姿态
    // @param newStance 新姿态
    // @param staminaPercent 当前体力百分比
    // @param encumbranceCacheValid 负重缓存是否有效
    // @param cachedCurrentWeight 缓存的当前重量
    // @param owner 角色实体（用于调试）
    // @return 体力消耗值（0.0-1.0范围）
    protected float CalculateStanceTransitionCost(
        ECharacterStance oldStance,
        ECharacterStance newStance,
        float staminaPercent,
        bool encumbranceCacheValid,
        float cachedCurrentWeight,
        IEntity owner)
    {
        // 获取基础消耗（基于转换类型）
        float baseCost = GetBaseTransitionCost(oldStance, newStance);
        
        if (baseCost <= 0.0)
            return 0.0;
        
        // 获取当前总重量
        float currentTotalWeight = StaminaConstants.CHARACTER_WEIGHT;
        if (encumbranceCacheValid)
            currentTotalWeight = StaminaConstants.CHARACTER_WEIGHT + cachedCurrentWeight;
        else
        {
            ChimeraCharacter character = ChimeraCharacter.Cast(owner);
            if (character)
            {
                SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
                    character.FindComponent(SCR_CharacterInventoryStorageComponent));
                if (inventoryComponent)
                    currentTotalWeight = StaminaConstants.CHARACTER_WEIGHT + inventoryComponent.GetTotalWeight();
            }
        }
        
        // 计算负重加成（线性化：WeightFactor = CurrentWeight / 90.0）
        float weightMultiplier = currentTotalWeight / StaminaConstants.STANCE_WEIGHT_BASE;
        
        // 计算最终消耗：基础 × 负重 × (1 + 疲劳堆积)
        float finalCost = baseCost * weightMultiplier * (1.0 + m_fStanceFatigue);
        
        // 调试输出（仅在客户端）
        if (StaminaConstants.IsDebugEnabled() && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            string transitionName = GetTransitionName(oldStance, newStance);
            PrintFormat("[RealisticSystem] 姿态转换！%1 | 消耗: %2%% (基础: %3%%, 负重: %4kg, 倍数: %5, 疲劳堆积: %6)",
                transitionName,
                Math.Round(finalCost * 100.0).ToString(),
                Math.Round(baseCost * 100.0).ToString(),
                Math.Round(currentTotalWeight - StaminaConstants.CHARACTER_WEIGHT).ToString(),
                Math.Round(weightMultiplier * 100.0) / 100.0,
                Math.Round(m_fStanceFatigue * 100.0) / 100.0);
        }
        
        return finalCost;
    }
    
    // 获取姿态转换的基础消耗（v2.0 优化：下调基础消耗）
    // @param oldStance 旧姿态
    // @param newStance 新姿态
    // @return 基础消耗值（0.0-1.0范围）
    protected float GetBaseTransitionCost(ECharacterStance oldStance, ECharacterStance newStance)
    {
        // 向上转换（克服重力，肌肉做向心收缩）
        if (oldStance == ECharacterStance.PRONE && newStance == ECharacterStance.STAND)
            return StaminaConstants.STANCE_COST_PRONE_TO_STAND; // 1.5% - 趴到站
        else if (oldStance == ECharacterStance.PRONE && newStance == ECharacterStance.CROUCH)
            return StaminaConstants.STANCE_COST_PRONE_TO_CROUCH; // 1.0% - 趴到蹲
        else if (oldStance == ECharacterStance.CROUCH && newStance == ECharacterStance.STAND)
            return StaminaConstants.STANCE_COST_CROUCH_TO_STAND; // 0.5% - 蹲到站
        
        // 向下转换（利用重力，但需要离心收缩刹车）
        else if (oldStance == ECharacterStance.STAND && newStance == ECharacterStance.PRONE)
            return StaminaConstants.STANCE_COST_STAND_TO_PRONE; // 0.3% - 站到趴
        
        // 其他转换（如 STAND -> CROUCH 或 CROUCH -> PRONE）
        return StaminaConstants.STANCE_COST_OTHER; // 0.3% - 其他转换
    }
    
    // 获取姿态转换的名称（用于调试输出）
    // @param oldStance 旧姿态
    // @param newStance 新姿态
    // @return 转换名称字符串
    protected string GetTransitionName(ECharacterStance oldStance, ECharacterStance newStance)
    {
        string oldName = GetStanceName(oldStance);
        string newName = GetStanceName(newStance);
        return string.Format("%1 → %2", oldName, newName);
    }
    
    // 获取姿态名称（用于调试输出）
    // @param stance 姿态
    // @return 姿态名称字符串
    protected string GetStanceName(ECharacterStance stance)
    {
        if (stance == ECharacterStance.STAND)
            return "STAND";
        else if (stance == ECharacterStance.CROUCH)
            return "CROUCH";
        else if (stance == ECharacterStance.PRONE)
            return "PRONE";
        else
            return "UNKNOWN";
    }
    
    // ==================== 获取器方法 ====================
    
    // 获取上一帧姿态
    ECharacterStance GetLastStance()
    {
        return m_eLastStance;
    }
    
    // 获取当前疲劳堆积值
    float GetStanceFatigue()
    {
        return m_fStanceFatigue;
    }
}
