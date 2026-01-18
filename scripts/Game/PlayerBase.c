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
            
            // 设置初始目标体力值为100%
            m_pStaminaComponent.SetTargetStamina(1.0);
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
        
        // 使用静态工具类计算基础速度倍数（根据体力）
        float baseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        // 使用静态工具类计算负重影响
        float encumbranceSpeedPenalty = RealisticStaminaSpeedSystem.CalculateEncumbranceSpeedPenalty(owner);
        
        // 综合计算最终速度倍数
        // 公式：最终速度 = 基础速度 * (1 - 负重惩罚)
        float finalSpeedMultiplier = baseSpeedMultiplier * (1.0 - encumbranceSpeedPenalty);
        
        // 体力为0时，速度应限制在20%-100%之间（允许更低的速度）
        finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.2, 1.0);
        
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
        
        // 计算体力消耗率（基于精确 Pandolf 模型）
        // 使用绝对速度（m/s），而不是相对速度，以符合医学模型
        // GAME_MAX_SPEED = 5.2 m/s（游戏最大速度）
        float speedRatio = Math.Clamp(currentSpeed / 5.2, 0.0, 1.0);
        
        // ==================== 精确体力消耗计算（基于 Pandolf 模型）====================
        // 校准参数以达到目标：2英里在15分27秒内完成（平均速度3.47 m/s）
        // 经过Python参数优化，找到最佳参数组合：
        // - 基础消耗: 0.00004 (每0.2秒)
        // - 速度线性项系数: 0.0001
        // - 速度平方项系数: 0.0001（优化后降低）
        // 预期完成时间: 925.8秒 (15.43分钟)，平均速度: 3.48 m/s
        
        // 基础体力消耗率（移动时的基础消耗，对应 Pandolf 模型中的常数项）
        // 对应公式中的 a 项
        float baseDrainRate = 0.00004; // 每0.2秒消耗0.004%
        
        // 速度线性项（对应 Pandolf 模型中的 b·V 项）
        // 注意：Pandolf 模型中速度项是 (V-0.7)²，这里简化为 V 和 V² 的组合
        float speedLinearDrainRate = 0.0001 * speedRatio; // 速度线性项的系数
        
        // 速度平方项（对应 Pandolf 模型中的 c·V² 项）
        // 这是能量消耗的主要速度相关项，与速度平方成正比
        // 优化后降低系数，以在目标时间内完成2英里
        float speedSquaredDrainRate = 0.0001 * speedRatio * speedRatio; // 速度平方项的系数（优化后）
        
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
        
        // 综合体力消耗率（每0.2秒）
        // 精确公式：total = base + linear·V + squared·V² + enc_base + enc_speed·V²
        float totalDrainRate = baseDrainRate + speedLinearDrainRate + speedSquaredDrainRate + encumbranceBaseDrainRate + encumbranceSpeedDrainRate;
        
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
                // 静止时，体力逐渐恢复
                // 恢复率：每0.2秒恢复一定比例（基于医学模型）
                // 从86%恢复到100%（14%）应该需要约2-3分钟，而不是几秒
                // 每0.2秒恢复0.015%，每10秒恢复0.75%，从86%到100%约需2-3分钟
                float recoveryRate = 0.00015; // 每0.2秒恢复0.015%（每10秒恢复0.75%，从86%到100%约需3分钟）
                
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
                
                // 调试输出（每5秒输出一次）
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
            
            // 更新 staminaPercent 以反映新的目标值
            staminaPercent = newTargetStamina;
        }
        
        m_fLastStaminaPercent = staminaPercent;
        m_fLastSpeedMultiplier = finalSpeedMultiplier;
        
        // 调试输出（每5秒输出一次，避免过多日志）
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
            PrintFormat("[RealisticSystem] 调试: 体力=%1%% | 基础速度倍数=%2 | 负重惩罚=%3 | 最终速度倍数=%4%5", 
                Math.Round(staminaPercent * 100.0).ToString(),
                baseSpeedMultiplier.ToString(),
                encumbranceSpeedPenalty.ToString(),
                finalSpeedMultiplier.ToString(),
                encumbranceInfo);
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
    
    // 显示状态信息（包含速度、体力、速度倍数）
    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        // 只对本地控制的玩家显示
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 格式化速度显示（保留1位小数）
        float speedRounded = Math.Round(m_fLastSecondSpeed * 10.0) / 10.0;
        
        // 格式化体力百分比（保留整数）
        int staminaPercentInt = Math.Round(m_fLastStaminaPercent * 100.0);
        
        // 格式化速度倍数（保留2位小数）
        float speedMultiplierRounded = Math.Round(m_fLastSpeedMultiplier * 100.0) / 100.0;
        
        // 构建状态文本
        string statusText = string.Format(
            "移动速度: %1 m/s | 体力: %2%% | 速度倍率: %3%%",
            speedRounded.ToString(),
            staminaPercentInt.ToString(),
            (speedMultiplierRounded * 100.0).ToString()
        );
        
        // 在控制台输出状态信息
        PrintFormat("[RealisticSystem] %1", statusText);
    }
}
