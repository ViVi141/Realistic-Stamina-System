// 速度计算模块
// 负责计算基础速度倍数、移动类型处理、Sprint速度计算等
// 模块化拆分：从 PlayerBase.c 提取的速度计算逻辑

// 坡度计算结果结构体
class GradeCalculationResult
{
    float gradePercent;
    float slopeAngleDegrees;
}

class SpeedCalculator
{
    // ==================== 公共方法 ====================
    
    // 计算基础速度倍数（根据体力百分比）
    // @param staminaPercent 当前体力百分比 (0.0-1.0)
    // @param collapseTransition "撞墙"阻尼过渡模块引用
    // @param currentWorldTime 当前世界时间
    // @return 基础速度倍数
    static float CalculateBaseSpeedMultiplier(float staminaPercent, CollapseTransition collapseTransition, float currentWorldTime)
    {
        // 更新"撞墙"阻尼过渡模块状态
        if (collapseTransition)
            collapseTransition.Update(currentWorldTime, staminaPercent);
        
        // 先计算正常情况下的基础速度倍数
        float normalBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        // 如果处于5秒阻尼过渡期间，使用模块计算过渡速度
        if (collapseTransition && collapseTransition.IsInTransition())
        {
            return collapseTransition.CalculateTransitionSpeedMultiplier(currentWorldTime, normalBaseSpeedMultiplier);
        }
        
        return normalBaseSpeedMultiplier;
    }
    
    // 计算坡度自适应目标速度
    // @param baseTargetSpeed 基础目标速度 (m/s)
    // @param slopeAngleDegrees 坡度角度（度）
    // @return 坡度自适应后的目标速度 (m/s)
    static float CalculateSlopeAdjustedTargetSpeed(float baseTargetSpeed, float slopeAngleDegrees)
    {
        return RealisticStaminaSpeedSystem.CalculateSlopeAdjustedTargetSpeed(baseTargetSpeed, slopeAngleDegrees);
    }
    
    // 计算最终速度倍数（根据移动类型）
    // @param runBaseSpeedMultiplier Run的基础速度倍数
    // @param encumbranceSpeedPenalty 负重速度惩罚
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段 (0=idle, 1=walk, 2=run, 3=sprint)
    // @param isExhausted 是否精疲力尽
    // @param canSprint 是否可以Sprint
    // @param staminaPercent 当前体力百分比
    // @param currentSpeed 当前速度 (m/s)
    // @return 最终速度倍数
    static float CalculateFinalSpeedMultiplier(
        float runBaseSpeedMultiplier,
        float encumbranceSpeedPenalty,
        bool isSprinting,
        int currentMovementPhase,
        bool isExhausted,
        bool canSprint,
        float staminaPercent,
        float currentSpeed = 0.0)
    {
        // 如果精疲力尽，禁用Sprint
        if (isExhausted || !canSprint)
        {
            if (isSprinting || currentMovementPhase == 3)
            {
                currentMovementPhase = 2; // 强制切换到Run
                isSprinting = false;
            }
        }
        
        // 计算坡度自适应目标速度倍数
        float slopeAdjustedTargetSpeed = CalculateSlopeAdjustedTargetSpeed(
            RealisticStaminaSpeedSystem.TARGET_RUN_SPEED,
            0.0 // 坡度角度在调用处获取
        );
        float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
        
        // 将速度从基础目标速度（3.7 m/s）缩放到坡度自适应速度
        float speedScaleFactor = slopeAdjustedTargetMultiplier / RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        float scaledRunSpeed = runBaseSpeedMultiplier * speedScaleFactor;
        
        float finalSpeedMultiplier = 0.0;
        
        if (isSprinting || currentMovementPhase == 3) // Sprint
        {
            // Sprint速度 = Run基础倍率 × (1 + 30%)
            float sprintMultiplier = 1.0 + RealisticStaminaSpeedSystem.SPRINT_SPEED_BOOST; // 1.30
            finalSpeedMultiplier = (scaledRunSpeed * sprintMultiplier) - (encumbranceSpeedPenalty * 0.15);
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 2) // Run
        {
            finalSpeedMultiplier = scaledRunSpeed - (encumbranceSpeedPenalty * 0.2);
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 1) // Walk
        {
            float walkBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
            finalSpeedMultiplier = walkBaseSpeedMultiplier * 0.8; // 提高Walk基础速度
            
            // 放宽Walk阶段的速度限制范围
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.2, 0.9);
        }
        else // Idle
        {
            finalSpeedMultiplier = 0.0;
        }
        
        // 静止起步检测：如果当前速度很低但处于移动阶段，给予起步补偿
        bool isMovingPhase = (currentMovementPhase == 1 || currentMovementPhase == 2 || currentMovementPhase == 3);
        if (isMovingPhase && currentSpeed < 0.5)
        {
            // 给予起步补偿，防止瞬时限速导致的起步无力
            finalSpeedMultiplier = Math.Max(finalSpeedMultiplier, 0.5);
        }
        
        return finalSpeedMultiplier;
    }
    
    // 获取坡度角度（修复了背对坡倒车不消耗体力的漏洞）
    // @param controller 角色控制器组件
    // @param environmentFactor 环境因子组件（可选，用于室内检测）
    // @return 坡度角度（度）
    static float GetSlopeAngle(SCR_CharacterControllerComponent controller, EnvironmentFactor environmentFactor = null)
    {
        // 检查是否在室内，如果是则返回0坡度
        if (environmentFactor && environmentFactor.IsIndoor())
        {
            return 0.0;
        }
        
        float slopeAngleDegrees = 0.0;
        CharacterAnimationComponent animComponent = controller.GetAnimationComponent();
        if (animComponent)
        {
            CharacterCommandHandlerComponent handler = animComponent.GetCommandHandler();
            if (handler)
            {
                CharacterCommandMove moveCmd = handler.GetCommandMove();
                if (moveCmd)
                {
                    // 1. 获取角色面向方向的坡度角度
                    slopeAngleDegrees = moveCmd.GetMovementSlopeAngle();
                    
                    // 2. 获取移动输入方向
                    // GetCurrentInputAngle() 返回输入角度（-180 到 180 度）
                    // 0 度表示向前，180 度或 -180 度表示向后
                    // 如果角度绝对值大于 90 度，说明主要在后退
                    float inputAngle = 0.0;
                    if (moveCmd.GetCurrentInputAngle(inputAngle))
                    {
                        // 3. 核心修正逻辑：
                        // 如果移动输入方向主要在后退（角度绝对值 > 90 度）
                        // 说明角色的实际移动方向与面向方向相反，我们需要翻转角度的正负号
                        // 例如：背对山上（面向下坡 -15度）倒车往山上走，结果变为 -(-15) = +15度（上坡惩罚）
                        if (Math.AbsFloat(inputAngle) > 90.0)
                        {
                            slopeAngleDegrees = -slopeAngleDegrees;
                        }
                    }
                }
            }
        }
        return slopeAngleDegrees;
    }
    
    // 计算坡度百分比（考虑攀爬和跳跃状态）
    // @param controller 角色控制器组件
    // @param currentSpeed 当前速度（m/s）
    // @param jumpVaultDetector 跳跃检测器（可选）
    // @param slopeAngleDegrees 坡度角度（输入，通常为0.0）
    // @param environmentFactor 环境因子组件（可选，用于室内检测）
    // @return 坡度计算结果（包含坡度百分比和角度）
    static GradeCalculationResult CalculateGradePercent(
        SCR_CharacterControllerComponent controller,
        float currentSpeed,
        JumpVaultDetector jumpVaultDetector,
        float slopeAngleDegrees,
        EnvironmentFactor environmentFactor = null)
    {
        GradeCalculationResult result = new GradeCalculationResult();
        result.gradePercent = 0.0;
        result.slopeAngleDegrees = slopeAngleDegrees;
        
        // 检查是否在室内，如果是则返回0坡度
        if (environmentFactor && environmentFactor.IsIndoor())
        {
            return result;
        }
        
        // 检查是否在攀爬或跳跃状态
        bool isClimbingForSlope = controller.IsClimbing();
        bool isJumpingForSlope = false;
        if (jumpVaultDetector)
            isJumpingForSlope = jumpVaultDetector.GetJumpInputTriggered();
        
        // 只在非攀爬、非跳跃状态下获取坡度
        if (!isClimbingForSlope && !isJumpingForSlope && currentSpeed > 0.05)
        {
            // 获取坡度角度
            result.slopeAngleDegrees = GetSlopeAngle(controller, environmentFactor);
            
            // 将坡度角度（度）转换为坡度百分比
            // 坡度百分比 = tan(坡度角度) × 100
            // [修复 fix2.md #5] 在计算 Tan 之前钳制角度，防止数值爆炸
            // 如果 slopeAngleDegrees 接近 90 度，tan(90°) 趋向于无穷大
            // 这将导致 gradePercent 变成 Infinity 或 NaN，污染 Pandolf 模型的计算
            float clampedAngle = Math.Clamp(result.slopeAngleDegrees, -85.0, 85.0); // 限制在85度以内
            result.gradePercent = Math.Tan(clampedAngle * 0.0174532925199433) * 100.0; // 0.0174532925199433 = π/180
        }
        
        return result;
    }
}
