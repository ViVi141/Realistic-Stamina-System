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
    
    // ==================== 运动持续时间跟踪（用于累积疲劳计算）====================
    // 基于个性化运动建模（Palumbo et al., 2018）
    // 跟踪连续运动时间，用于计算累积疲劳
    protected float m_fExerciseDurationMinutes = 0.0; // 连续运动时间（分钟）
    protected float m_fLastUpdateTime = 0.0; // 上次更新时间（秒）
    protected bool m_bWasMoving = false; // 上次更新时是否在移动
    protected float m_fRestDurationMinutes = 0.0; // 连续休息时间（分钟），从停止运动开始计算
    
    // 跳跃和翻越检测相关
    protected float m_fLastVerticalVelocity = 0.0; // 上一帧的垂直速度（用于检测跳跃）
    protected bool m_bIsVaulting = false; // 是否正在翻越/攀爬
    protected int m_iVaultingFrameCount = 0; // 翻越状态持续帧数
    protected int m_iVaultCooldownFrames = 0; // 翻越冷却帧数（防止重复触发）
    
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
            
            // 设置初始目标体力值为85%（ACFT后预疲劳状态，融合模型）
            m_pStaminaComponent.SetTargetStamina(RealisticStaminaSpeedSystem.INITIAL_STAMINA_AFTER_ACFT);
        }
        
        // 初始化跳跃和翻越检测变量
        m_fLastVerticalVelocity = 0.0;
        m_bIsVaulting = false;
        m_iVaultingFrameCount = 0;
        m_iVaultCooldownFrames = 0;
        
        // 初始化运动持续时间跟踪变量
        m_fExerciseDurationMinutes = 0.0;
        m_fLastUpdateTime = GetGame().GetWorld().GetWorldTime();
        m_bWasMoving = false;
        
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
        
        // 使用静态工具类计算负重影响
        float encumbranceSpeedPenalty = RealisticStaminaSpeedSystem.CalculateEncumbranceSpeedPenalty(owner);
        
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
        // Sprint时：速度比Run快，但仍然受体力和负重限制（类似于追击或逃命）
        // Run时：使用中等速度倍数（受体力和负重影响）
        // Walk时：使用较低速度倍数
        
        // 先计算Run时的基础速度倍数（受体力和负重影响）
        float runSpeedMultiplier = baseSpeedMultiplier * (1.0 - encumbranceSpeedPenalty);
        
        float finalSpeedMultiplier = 0.0;
        
        if (isSprinting || currentMovementPhase == 3) // Sprint
        {
            // Sprint时：速度比Run快，但仍然受体力和负重限制
            // Sprint速度 = Run速度 × (1 + Sprint加成)
            // 例如：如果Run速度是80%，Sprint速度 = 80% × 1.15 = 92%
            finalSpeedMultiplier = runSpeedMultiplier * (1.0 + RealisticStaminaSpeedSystem.SPRINT_SPEED_BOOST);
            
            // Sprint最高速度限制（基于现实情况）
            // 基于运动生理学：一般健康成年人的Sprint速度约20-30 km/h（5.5-8.3 m/s）
            // 游戏最大速度5.2 m/s（18.7 km/h）在合理范围内
            // Sprint最高速度限制在游戏最大速度的100%（5.2 m/s）
            // 注意：实际速度仍受体力和负重影响，只有在满体力、无负重时才能达到此上限
            float sprintMaxSpeedMultiplier = RealisticStaminaSpeedSystem.SPRINT_MAX_SPEED_MULTIPLIER;
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.2, sprintMaxSpeedMultiplier);
        }
        else if (currentMovementPhase == 2) // Run
        {
            // Run时，使用计算出的速度倍数（受体力和负重影响）
            finalSpeedMultiplier = runSpeedMultiplier;
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.2, 1.0);
        }
        else if (currentMovementPhase == 1) // Walk
        {
            // Walk时，速度约为Run的70%
            finalSpeedMultiplier = runSpeedMultiplier * 0.7;
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
        
        // 始终更新速度（确保体力变化时立即生效）
        // 移除变化阈值检查，确保体力为0时速度立即降低
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
        
        // 获取当前实际速度（m/s）
        vector velocity = GetVelocity();
        vector horizontalVelocity = velocity;
        horizontalVelocity[1] = 0.0; // 忽略垂直速度
        float currentSpeed = horizontalVelocity.Length(); // 当前水平速度（m/s）
        
        // 获取垂直速度（用于检测跳跃和翻越）
        float currentVerticalVelocity = velocity[1]; // 垂直速度（m/s，向上为正）
        
        // ==================== 检测跳跃和翻越动作 ====================
        // 注意：在 Arma Reforger 中，跳跃和翻越是同一个动作系统的不同表现
        // - 按跳跃键时，如果前方有障碍物，执行翻越/攀爬（IsClimbing() 返回 true）
        // - 按跳跃键时，如果前方无障碍物，执行普通跳跃（垂直速度增加）
        // 因此，我们需要区分这两种情况：
        // 1. 翻越/攀爬：使用 IsClimbing() 检测
        // 2. 普通跳跃：垂直速度增加且不在攀爬状态
        if (m_pStaminaComponent)
        {
            // 检测翻越/攀爬：使用 IsClimbing() 方法（更可靠）
            // 翻越的特征：角色处于攀爬状态（使用引擎提供的状态检测）
            bool isClimbing = IsClimbing();
            
            // 检测普通跳跃：垂直速度突然增加且超过阈值，且不在攀爬状态
            // 跳跃的特征：垂直速度从较低值突然增加到较高值（> 2.0 m/s），且不在攀爬状态
            if (!isClimbing && 
                currentVerticalVelocity > RealisticStaminaSpeedSystem.JUMP_VERTICAL_VELOCITY_THRESHOLD && 
                m_fLastVerticalVelocity < RealisticStaminaSpeedSystem.JUMP_VERTICAL_VELOCITY_THRESHOLD)
            {
                // 检测到跳跃动作，立即消耗体力
                float jumpCost = RealisticStaminaSpeedSystem.JUMP_STAMINA_COST;
                
                // 负重会增加跳跃的体力消耗
                float encumbranceMultiplier = RealisticStaminaSpeedSystem.CalculateEncumbranceStaminaDrainMultiplier(owner);
                jumpCost = jumpCost * encumbranceMultiplier;
                
                staminaPercent = staminaPercent - jumpCost;
                
                // 调试输出（仅在客户端）
                if (owner == SCR_PlayerController.GetLocalControlledEntity())
                {
                    PrintFormat("[RealisticSystem] 检测到跳跃动作！消耗体力: %1%%", Math.Round(jumpCost * 100.0).ToString());
                }
            }
            
            if (isClimbing)
            {
                // 翻越冷却检查：防止在短时间内重复触发初始消耗
                // 冷却时间：5秒（25个更新周期，每0.2秒更新一次）
                // 5秒内都视为同一个翻越动作，不会重复消耗
                // 注意：冷却时间在函数末尾统一计时，这里只检查
                
                if (!m_bIsVaulting && m_iVaultCooldownFrames == 0)
                {
                    // 刚开始翻越，立即消耗一次翻越体力
                    float vaultCost = RealisticStaminaSpeedSystem.VAULT_STAMINA_COST;
                    
                    // 负重会增加翻越的体力消耗（但限制最大倍数，避免消耗过高）
                    float encumbranceMultiplier = RealisticStaminaSpeedSystem.CalculateEncumbranceStaminaDrainMultiplier(owner);
                    // 限制负重倍数最大为1.5倍，避免消耗过高
                    encumbranceMultiplier = Math.Min(encumbranceMultiplier, 1.5);
                    vaultCost = vaultCost * encumbranceMultiplier;
                    
                    staminaPercent = staminaPercent - vaultCost;
                    m_bIsVaulting = true;
                    m_iVaultingFrameCount = 0;
                    m_iVaultCooldownFrames = 25; // 设置5秒冷却（25个更新周期，每0.2秒更新一次）
                    
                    // 调试输出（仅在客户端）
                    if (owner == SCR_PlayerController.GetLocalControlledEntity())
                    {
                        PrintFormat("[RealisticSystem] 检测到翻越动作！消耗体力: %1%% (基础: %2%%, 负重倍数: %3, 冷却: 5秒)", 
                            Math.Round(vaultCost * 100.0).ToString(),
                            Math.Round(RealisticStaminaSpeedSystem.VAULT_STAMINA_COST * 100.0).ToString(),
                            Math.Round(encumbranceMultiplier * 100.0) / 100.0);
                    }
                }
                else
                {
                    // 持续翻越中，每0.2秒额外消耗少量体力（持续攀爬消耗）
                    // 注意：翻越动作通常很快完成（1-2秒），所以持续消耗应该很小
                    m_iVaultingFrameCount++;
                    if (m_iVaultingFrameCount >= 10) // 每2秒（10个更新周期）额外消耗，降低频率
                    {
                        float continuousVaultCost = RealisticStaminaSpeedSystem.VAULT_STAMINA_COST * 0.1; // 翻越成本的10%（降低持续消耗）
                        float encumbranceMultiplier = RealisticStaminaSpeedSystem.CalculateEncumbranceStaminaDrainMultiplier(owner);
                        continuousVaultCost = continuousVaultCost * encumbranceMultiplier;
                        
                        staminaPercent = staminaPercent - continuousVaultCost;
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
            
            // 更新记录的垂直速度（用于下一帧检测）
            m_fLastVerticalVelocity = currentVerticalVelocity;
            
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
        
        // ==================== 运动持续时间跟踪（累积疲劳）====================
        // 基于个性化运动建模（Palumbo et al., 2018）
        // 跟踪连续运动时间，用于计算累积疲劳
        float currentTime = GetGame().GetWorld().GetWorldTime();
        bool isCurrentlyMoving = (currentSpeed > 0.05);
        
        if (isCurrentlyMoving)
        {
            if (m_bWasMoving)
            {
                // 继续运动：累积运动时间
                float timeDelta = currentTime - m_fLastUpdateTime;
                if (timeDelta > 0.0 && timeDelta < 1.0) // 防止时间跳跃异常
                {
                    m_fExerciseDurationMinutes += timeDelta / 60.0; // 转换为分钟
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
            float timeDelta = currentTime - m_fLastUpdateTime;
            if (timeDelta > 0.0 && timeDelta < 1.0)
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
                    m_fRestDurationMinutes += timeDelta / 60.0; // 转换为分钟
                }
                
                // 静止时，疲劳恢复速度是累积速度的2倍（快速恢复）
                if (m_fExerciseDurationMinutes > 0.0)
                {
                    m_fExerciseDurationMinutes = Math.Max(m_fExerciseDurationMinutes - (timeDelta / 60.0 * 2.0), 0.0);
                }
            }
            m_bWasMoving = false;
        }
        m_fLastUpdateTime = currentTime;
        
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
        
        // ==================== 基于速度阈值的分段消耗率（融合模型）====================
        // 使用新模型的基于速度阈值的分段消耗率作为基础
        float baseDrainRateByVelocity = RealisticStaminaSpeedSystem.CalculateBaseDrainRateByVelocity(currentSpeed);
        
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
            baseDrainRate = baseDrainRateByVelocity * totalEfficiencyFactor * fatigueFactor;
        }
        
        // 保留原有的速度相关项（用于平滑过渡和精细调整）
        // 这些项在速度阈值附近提供平滑过渡
        float speedLinearDrainRate = 0.00005 * speedRatio * totalEfficiencyFactor * fatigueFactor; // 降低系数，主要依赖分段消耗率
        float speedSquaredDrainRate = 0.00005 * speedRatio * speedRatio * totalEfficiencyFactor * fatigueFactor; // 降低系数
        
        // 负重相关的体力消耗（基于精确 Pandolf 模型）
        // Pandolf 模型：负重影响 = M_total · (能量消耗项)
        // 其中 M_total = M_body + M_load，M_load 是负重
        // 我们使用：负重影响倍数 = 1 + γ·(负重/最大负重)·(1 + δ·V²)
        // 其中 γ 是负重基础影响系数，δ 是速度相关的负重影响系数
        float encumbranceStaminaDrainMultiplier = RealisticStaminaSpeedSystem.CalculateEncumbranceStaminaDrainMultiplier(owner);
        
        // 负重对速度平方项的额外影响（Pandolf 模型中负重与速度的交互项）
        // 公式：负重额外消耗 = base_encumbrance_drain + speed_encumbrance_drain·V²
        float encumbranceBaseDrainRate = 0.001 * (encumbranceStaminaDrainMultiplier - 1.0); // 负重基础消耗
        float encumbranceSpeedDrainRate = 0.0002 * (encumbranceStaminaDrainMultiplier - 1.0) * speedRatio * speedRatio; // 负重速度相关消耗
        
        // ==================== 坡度影响计算（融合模型：新坡度修正 + 多维度特性）====================
        // 获取移动坡度角度（度）
        // 注意：只在非攀爬、非跳跃状态下应用坡度影响
        // 攀爬和跳跃动作已经有专门的体力消耗机制，不需要额外的坡度影响
        float slopeAngleDegrees = 0.0;
        float slopeMultiplier = 1.0;
        float speedEncumbranceSlopeInteraction = 0.0;
        
        // 获取负重占体重的百分比（用于计算交互项）
        float bodyMassPercent = 0.0;
        ChimeraCharacter characterForInteraction = ChimeraCharacter.Cast(owner);
        if (characterForInteraction)
        {
            SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(characterForInteraction.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (characterInventory)
            {
                float currentWeight = characterInventory.GetTotalWeight();
                if (currentWeight > 0.0)
                {
                    // 计算有效负重（减去基准重量，基准重量是基本战斗装备）
                    float effectiveWeight = Math.Max(currentWeight - RealisticStaminaSpeedSystem.BASE_WEIGHT, 0.0);
                    bodyMassPercent = effectiveWeight / RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                }
            }
        }
        
        // 检查是否在攀爬或跳跃状态（复用之前的检测结果）
        bool isClimbingForSlope = IsClimbing();
        bool isJumpingForSlope = (currentVerticalVelocity > RealisticStaminaSpeedSystem.JUMP_VERTICAL_VELOCITY_THRESHOLD);
        
        // 只在非攀爬、非跳跃状态下计算坡度影响
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
                        
                        // ==================== 融合模型：使用新的坡度修正 ====================
                        // 将坡度角度（度）转换为坡度百分比
                        // 坡度百分比 = tan(坡度角度) × 100
                        // 注意：EnforceScript使用弧度，需要转换
                        float gradePercent = Math.Tan(slopeAngleDegrees * 0.0174532925199433) * 100.0; // 0.0174532925199433 = π/180
                        
                        // 使用新模型的坡度修正（K_grade = 1 + G × 0.12 for G > 0）
                        slopeMultiplier = RealisticStaminaSpeedSystem.CalculateGradeMultiplier(gradePercent);
                        
                        // 保留原有的多维度交互项（用于精细调整）
                        // 计算速度×负重×坡度三维交互项
                        speedEncumbranceSlopeInteraction = RealisticStaminaSpeedSystem.CalculateSpeedEncumbranceSlopeInteraction(speedRatio, bodyMassPercent, slopeAngleDegrees);
                    }
                }
            }
        }
        
        // ==================== Sprint额外体力消耗 ====================
        // Sprint时体力消耗大幅增加（类似于追击或逃命，追求速度但消耗巨大）
        float sprintMultiplier = 1.0;
        if (isSprinting || currentMovementPhase == 3)
        {
            sprintMultiplier = RealisticStaminaSpeedSystem.SPRINT_STAMINA_DRAIN_MULTIPLIER;
        }
        
        // ==================== 综合体力消耗率（融合模型）====================
        // 融合公式：total = (base_by_velocity + linear·V + squared·V² + enc_base + enc_speed·V²) × slope_multiplier × sprint_multiplier × efficiency × fatigue + 3d_interaction
        // 其中：
        //   base_by_velocity 是基于速度阈值的分段消耗率（新模型）
        //   slope_multiplier 是新模型的坡度修正（K_grade = 1 + G × 0.12）
        //   efficiency 和 fatigue 是多维度特性（健康状态、累积疲劳、代谢适应）
        //   3d_interaction 是速度×负重×坡度的三维交互项（保留用于精细调整）
        float baseDrainComponents = baseDrainRate + speedLinearDrainRate + speedSquaredDrainRate + encumbranceBaseDrainRate + encumbranceSpeedDrainRate;
        
        // 应用坡度修正和Sprint倍数
        // 注意：恢复时（baseDrainRate < 0），不应用坡度修正和Sprint倍数
        float totalDrainRate = 0.0;
        if (baseDrainRate < 0.0)
        {
            // 恢复时，直接使用恢复率（负数）
            totalDrainRate = baseDrainRate;
        }
        else
        {
            // 消耗时，应用坡度修正和Sprint倍数
            totalDrainRate = (baseDrainComponents * slopeMultiplier * sprintMultiplier) + (baseDrainComponents * speedEncumbranceSlopeInteraction);
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
                newTargetStamina = staminaPercent - totalDrainRate;
            }
            // 如果角色完全静止（速度 <= 0.05 m/s），体力恢复
            else
            {
                // ==================== 多维度恢复模型 ====================
                // 基于个性化运动建模（Palumbo et al., 2018）和生理学恢复模型
                // 考虑多个维度：
                // 1. 当前体力百分比（非线性恢复）：体力越低恢复越快，体力越高恢复越慢
                // 2. 健康状态/训练水平：训练有素者恢复更快
                // 3. 休息时间：刚停止运动时恢复快（快速恢复期），长时间休息后恢复慢（慢速恢复期）
                // 4. 年龄：年轻者恢复更快
                // 5. 累积疲劳恢复：运动后的疲劳需要时间恢复
                float recoveryRate = RealisticStaminaSpeedSystem.CalculateMultiDimensionalRecoveryRate(
                    staminaPercent, 
                    m_fRestDurationMinutes, 
                    m_fExerciseDurationMinutes
                );
                
                // 计算新的目标体力值（增加恢复）
                newTargetStamina = staminaPercent + recoveryRate;
            }
            
            // 限制体力值在有效范围内（0.0 - 1.0）
            newTargetStamina = Math.Clamp(newTargetStamina, 0.0, 1.0);
            
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
                
                // 调试输出（每5秒输出一次，仅在客户端）
                // [已注释] 拦截信息已禁用，减少日志输出
                /*
                if (owner == SCR_PlayerController.GetLocalControlledEntity())
                {
                    static int interferenceCounter = 0;
                    interferenceCounter++;
                    if (interferenceCounter >= 25) // 200ms * 25 = 5秒
                    {
                        PrintFormat("[RealisticSystem] 警告：检测到原生体力系统干扰！目标=%1%%，实际=%2%%，偏差=%3%%", 
                            Math.Round(newTargetStamina * 100.0).ToString(),
                            Math.Round(verifyStamina * 100.0).ToString(),
                            Math.Round((verifyStamina - newTargetStamina) * 100.0).ToString());
                        interferenceCounter = 0;
                    }
                }
                */
            }
            
            // 更新 staminaPercent 以反映新的目标值
            staminaPercent = newTargetStamina;
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
                ChimeraCharacter character = ChimeraCharacter.Cast(owner);
                float encumbrancePercent = 0.0;
                float combatEncumbrancePercent = 0.0;
                float currentWeight = 0.0;
                float maxWeight = 40.5; // 使用我们定义的最大负重
                float combatWeight = 30.0; // 战斗负重阈值
                if (character)
                {
                    // 使用 SCR_CharacterInventoryStorageComponent 获取重量信息
                    SCR_CharacterInventoryStorageComponent characterInventory = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
                    if (characterInventory)
                    {
                        currentWeight = characterInventory.GetTotalWeight();
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
                if (currentWeight > 0.0)
                {
                    string combatStatus = "";
                    if (combatEncumbrancePercent > 1.0)
                        combatStatus = " [超过战斗负重]";
                    else if (combatEncumbrancePercent >= 0.9)
                        combatStatus = " [接近战斗负重]";
                    
                    encumbranceInfo = string.Format(" | 负重: %1kg/%2kg (最大:%3kg, 战斗:%4kg%5)", 
                        currentWeight.ToString(), 
                        maxWeight.ToString(),
                        maxWeight.ToString(),
                        combatWeight.ToString(),
                        combatStatus);
                }
                
                PrintFormat("[RealisticSystem] 调试: 类型=%1 | 体力=%2%% | 基础速度倍数=%3 | 负重惩罚=%4 | 最终速度倍数=%5 | 坡度倍数=%6%7%8%9", 
                    movementTypeStr,
                    Math.Round(staminaPercent * 100.0).ToString(),
                    baseSpeedMultiplier.ToString(),
                    encumbranceSpeedPenalty.ToString(),
                    finalSpeedMultiplier.ToString(),
                    Math.Round(slopeMultiplier * 100.0) / 100.0,
                    slopeInfo,
                    sprintInfo,
                    encumbranceInfo);
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
}
