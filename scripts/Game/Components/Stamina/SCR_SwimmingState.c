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
        if (isSwimming != wasSwimming && owner == SCR_PlayerController.GetLocalControlledEntity())
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
            // 正在游泳：重置湿重计时器
            result.wetWeightStartTime = -1.0;
            result.currentWetWeight = 0.0;
        }
        else if (wasSwimming && !isSwimming)
        {
            // 刚从水中上岸：启动湿重计时器
            result.wetWeightStartTime = currentTime;
            // 使用固定湿重（7.5kg，介于5-10kg之间）
            result.currentWetWeight = (StaminaConstants.WET_WEIGHT_MIN + StaminaConstants.WET_WEIGHT_MAX) / 2.0;
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
                result.currentWetWeight = ((StaminaConstants.WET_WEIGHT_MIN + StaminaConstants.WET_WEIGHT_MAX) / 2.0) * wetWeightRatio;
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
        float totalWetWeight = 0.0;
        
        if (swimmingWetWeight > 0.0 && rainWeight > 0.0)
        {
            // 如果两者都大于0，使用加权平均（更真实的物理模型）
            // 加权平均：游泳湿重权重0.6，降雨湿重权重0.4（游泳更湿）
            totalWetWeight = swimmingWetWeight * 0.6 + rainWeight * 0.4;
        }
        else
        {
            // 如果只有一个大于0，直接使用较大值
            totalWetWeight = Math.Max(swimmingWetWeight, rainWeight);
        }
        
        // 应用饱和上限（防止数值爆炸）
        totalWetWeight = Math.Min(totalWetWeight, StaminaConstants.ENV_MAX_TOTAL_WET_WEIGHT);
        
        return totalWetWeight;
    }
}
