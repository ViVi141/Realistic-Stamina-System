// 游泳状态管理模块
// 负责检测游泳状态、跟踪湿重效应、计算总湿重等
// 模块化拆分：从 PlayerBase.c 提取的游泳相关逻辑

// 湿重更新结果结构体
class WetWeightUpdateResult
{
    float wetWeightStartTime;
    float currentWetWeight;
}

class SwimmingStateManager
{
    // ==================== 游泳状态追踪 ====================
    
    protected static float m_fSwimStartTime = -1.0; // 游泳开始时间（秒）
    protected static float m_fSwimDuration = 0.0; // 游泳持续时间（秒）
    
    // ==================== 游泳状态检测 ====================
    
    // 检测角色是否在游泳（通过动画组件）
    // @param controller 角色控制器组件
    // @return true表示正在游泳，false表示在陆地
    static bool IsSwimming(SCR_CharacterControllerComponent controller)
    {
        if (!controller)
            return false;
        
        CharacterAnimationComponent animComponent = controller.GetAnimationComponent();
        if (!animComponent)
            return false;
        
        CharacterCommandHandlerComponent handler = animComponent.GetCommandHandler();
        if (!handler)
            return false;
        
        return (handler.GetCommandSwim() != null);
    }
    
    // ==================== 湿重跟踪 ====================
    
    // 更新湿重（基于游泳状态）
    // @param wasSwimming 上一帧是否在游泳
    // @param isSwimming 当前是否在游泳
    // @param currentTime 当前世界时间
    // @param wetWeightStartTime 湿重开始时间（输入）
    // @param currentWetWeight 当前湿重（输入）
    // @param owner 角色实体（用于调试输出）
    // @return 更新后的湿重结果（包含湿重开始时间和当前湿重）
    static WetWeightUpdateResult UpdateWetWeight(
        bool wasSwimming, 
        bool isSwimming, 
        float currentTime,
        float wetWeightStartTime,
        float currentWetWeight,
        IEntity owner)
    {
        // 如果状态变化，输出调试信息
        if (StaminaConstants.IsDebugEnabled() && isSwimming != wasSwimming && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            string oldState = "";
            if (wasSwimming)
                oldState = "游泳";
            else
                oldState = "陆地";
            
            string newState = "";
            if (isSwimming)
                newState = "游泳";
            else
                newState = "陆地";
            
            string stateChange = string.Format("[游泳检测] 状态变化: %1 -> %2", oldState, newState);
            Print(stateChange);
        }
        
        // 更新湿重
        WetWeightUpdateResult result = new WetWeightUpdateResult();
        result.wetWeightStartTime = wetWeightStartTime;
        result.currentWetWeight = currentWetWeight;
        
        if (isSwimming)
        {
            // 正在游泳：湿重非线性增长
            if (m_fSwimStartTime < 0.0)
            {
                // 首次进入游泳：记录开始时间
                m_fSwimStartTime = currentTime;
                m_fSwimDuration = 0.0;
            }
            
            // 计算游泳持续时间
            m_fSwimDuration = currentTime - m_fSwimStartTime;
            
            // 非线性增长：使用平方根函数，让湿重增长逐渐变慢
            // 最大湿重：WET_WEIGHT_MAX（10kg，与降雨湿重共用组合池）
            // 增长公式：wetWeight = WET_WEIGHT_MAX * sqrt(duration / 60.0)
            // 60秒时达到最大值 WET_WEIGHT_MAX kg
            float swimProgress = Math.Clamp(m_fSwimDuration / 60.0, 0.0, 1.0);
            float swimWetWeight = StaminaConstants.WET_WEIGHT_MAX * Math.Sqrt(swimProgress);
            
            result.wetWeightStartTime = -1.0;
            result.currentWetWeight = swimWetWeight;
        }
        else if (wasSwimming && !isSwimming)
        {
            // 刚从水中上岸：启动湿重计时器
            result.wetWeightStartTime = currentTime;
            // 湿重保持不变（已经在游泳时设置了）
            // 重置游泳时间
            m_fSwimStartTime = -1.0;
            m_fSwimDuration = 0.0;
        }
        else if (result.wetWeightStartTime > 0.0)
        {
            // 检查湿重是否过期（30秒）
            float wetWeightElapsed = currentTime - result.wetWeightStartTime;
            if (wetWeightElapsed >= StaminaConstants.WET_WEIGHT_DURATION)
            {
                // 湿重过期：清除湿重
                result.wetWeightStartTime = -1.0;
                result.currentWetWeight = 0.0;
            }
            else
            {
                // 湿重逐渐减少（线性衰减）
                float wetWeightRatio = 1.0 - (wetWeightElapsed / StaminaConstants.WET_WEIGHT_DURATION);
                result.currentWetWeight = StaminaConstants.WET_WEIGHT_MAX * wetWeightRatio;
            }
        }
        
        return result;
    }
    
    // ==================== 总湿重计算 ====================
    
    // 计算总湿重（游泳湿重 + 降雨湿重）
    // @param swimmingWetWeight 游泳湿重（kg）
    // @param rainWeight 降雨湿重（kg）
    // @return 总湿重（kg），已应用饱和上限
    static float CalculateTotalWetWeight(float swimmingWetWeight, float rainWeight)
    {
        // 游泳湿重和降雨湿重共用一个负重池，最大10KG
        float totalWetWeight = swimmingWetWeight + rainWeight;
        
        // 应用饱和上限（防止数值爆炸）
        totalWetWeight = Math.Min(totalWetWeight, StaminaConstants.ENV_MAX_TOTAL_WET_WEIGHT);
        
        return totalWetWeight;
    }
}
