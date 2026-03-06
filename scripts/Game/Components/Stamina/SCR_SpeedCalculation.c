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
    protected static ref GradeCalculationResult s_pResultGrade;

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
    // @param encumbranceSpeedPenalty 负重速度惩罚（基础惩罚项）
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段 (0=idle, 1=walk, 2=run, 3=sprint)
    // @param isExhausted 是否精疲力尽
    // @param canSprint 是否可以Sprint
    // @param staminaPercent 当前体力百分比
    // @param currentSpeed 当前速度 (m/s)
    // @param currentWorldTime 当前世界时间（秒），用于战术冲刺爆发期判断，-1 表示不参与
    // @param sprintStartTime 本次冲刺开始时间（秒），-1 表示未在冲刺
    // @return 最终速度倍数
    static float CalculateFinalSpeedMultiplier(
        float runBaseSpeedMultiplier,
        float encumbranceSpeedPenalty,
        bool isSprinting,
        int currentMovementPhase,
        bool isExhausted,
        bool canSprint,
        float staminaPercent,
        float currentSpeed = 0.0,
        float currentWorldTime = -1.0,
        float sprintStartTime = -1.0)
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
        
        // runBaseSpeedMultiplier 已由调用方（UpdateSpeed / CalculateFinalSpeedMultiplierFromInputs）
        // 按坡度完成缩放，此处无需再次应用坡度，直接使用
        float scaledRunSpeed = runBaseSpeedMultiplier;
        
        float finalSpeedMultiplier = 0.0;

        // 负重速度惩罚（含速度相关项与Sprint额外惩罚）
        float speedRatio = Math.Clamp(currentSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED, 0.0, 1.0);
        float encumbrancePenalty = encumbranceSpeedPenalty * (1.0 + speedRatio);
        if (isSprinting || currentMovementPhase == 3)
            encumbrancePenalty = encumbrancePenalty * 1.5;
        float maxPenalty = StaminaConstants.GetEncumbranceSpeedPenaltyMax();
        encumbrancePenalty = Math.Clamp(encumbrancePenalty, 0.0, maxPenalty);
        
        // 战术冲刺爆发期 + 缓冲区：前 8s 爆发，8s 后 5s 内线性过渡到平稳期
        if ((isSprinting || currentMovementPhase == 3) && currentWorldTime >= 0.0 && sprintStartTime >= 0.0)
        {
            float burstDuration = StaminaConstants.GetTacticalSprintBurstDuration();
            float bufferDuration = StaminaConstants.GetTacticalSprintBurstBufferDuration();
            float elapsed = currentWorldTime - sprintStartTime;
            if (burstDuration > 0.0 && elapsed <= burstDuration)
            {
                float burstFactor = StaminaConstants.GetTacticalSprintBurstEncumbranceFactor();
                encumbrancePenalty = encumbrancePenalty * burstFactor;
            }
            else if (bufferDuration > 0.0 && elapsed > burstDuration && elapsed <= burstDuration + bufferDuration)
            {
                float burstFactor = StaminaConstants.GetTacticalSprintBurstEncumbranceFactor();
                float t = (elapsed - burstDuration) / bufferDuration;
                t = Math.Clamp(t, 0.0, 1.0);
                float blendFactor = burstFactor + (1.0 - burstFactor) * t;
                encumbrancePenalty = encumbrancePenalty * blendFactor;
            }
        }
        
        if (isSprinting || currentMovementPhase == 3) // Sprint
        {
            // Sprint速度 = Run基础倍率 × (1 + 30%)
            float sprintSpeedBoost = StaminaConstants.GetSprintSpeedBoost();
            float sprintMultiplier = 1.0 + sprintSpeedBoost; // 1.30
            finalSpeedMultiplier = (scaledRunSpeed * sprintMultiplier) * (1.0 - encumbrancePenalty);
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 2) // Run
        {
            finalSpeedMultiplier = scaledRunSpeed * (1.0 - encumbrancePenalty);
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.15, 1.0);
        }
        else if (currentMovementPhase == 1) // Walk
        {
            float walkBaseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
            finalSpeedMultiplier = (walkBaseSpeedMultiplier * 0.8) * (1.0 - encumbrancePenalty); // 提高Walk基础速度
            
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
    
    // 速度阈值（m/s）：低于此值视为静止，坡度符号按上坡处理
    protected static const float VELOCITY_SIGN_THRESHOLD = 0.1;
    // 等高线判定阈值：|cos(速度与坡向夹角)| < 此值时视为沿等高线移动，按上坡惩罚
    // 0.2 约对应夹角 78°～102°，即大致垂直于坡向的侧移/等高线移动
    protected static const float CONTOUR_COS_THRESHOLD = 0.2;

    // 通过地面法线计算地形坡度幅值（与移动方向无关）
    // @param controller 角色控制器组件
    // @return 坡度角度幅值（0..45 度），失败返回 0
    static float GetTerrainSlopeAngleMagnitude(SCR_CharacterControllerComponent controller)
    {
        if (!controller)
            return 0.0;
        IEntity owner = controller.GetOwner();
        if (!owner)
            return 0.0;
        BaseWorld world = owner.GetWorld();
        if (!world)
            return 0.0;
        vector pos = owner.GetOrigin();
        vector normal = SCR_TerrainHelper.GetTerrainNormal(pos, world, false, null);
        float dotUp = vector.Dot(normal, vector.Up);
        dotUp = Math.Clamp(dotUp, 0.0, 1.0);
        float slopeAngleDegrees = Math.Acos(dotUp) * Math.RAD2DEG;
        return Math.Clamp(slopeAngleDegrees, 0.0, 45.0);
    }

    // 根据速度矢量与坡向判断上/下坡符号
    // 上坡方向 = 法线水平分量的反方向（陡升方向）
    // 沿等高线移动（速度垂直于坡向）时按上坡惩罚，因需维持平衡、侧向踩坡
    // @param normal 地面法线
    // @param velocity 速度矢量（水平分量参与判断）
    // @return +1 上坡/等高线，-1 下坡，静止时 +1
    static int GetSlopeSignFromVelocity(vector normal, vector velocity)
    {
        vector horizontalVel = velocity;
        horizontalVel[1] = 0.0;
        float velLen = horizontalVel.Length();
        if (velLen < VELOCITY_SIGN_THRESHOLD)
            return 1;
        vector horizontalNormal = normal;
        horizontalNormal[1] = 0.0;
        float hLen = horizontalNormal.Length();
        if (hLen < 0.001)
            return 1;
        vector uphillDir = horizontalNormal * (-1.0 / hLen);
        float dot = vector.Dot(horizontalVel, uphillDir);
        float cosAngle = dot / velLen;
        if (cosAngle > CONTOUR_COS_THRESHOLD)
            return 1;
        if (cosAngle < -CONTOUR_COS_THRESHOLD)
            return -1;
        return 1;
    }

    // 获取原始坡度角度（不做室内归零，供室内楼梯判定等使用）
    // 完全用法线坡度 + 速度矢量判断上下坡
    // @param controller 角色控制器组件
    // @param velocity 速度矢量（可选，vector.Zero 时从 controller 获取或按上坡处理）
    // @return 坡度角度（度）
    static float GetRawSlopeAngle(SCR_CharacterControllerComponent controller, vector velocity = vector.Zero)
    {
        if (!controller)
            return 0.0;
        IEntity owner = controller.GetOwner();
        if (!owner)
            return 0.0;
        float magnitude = GetTerrainSlopeAngleMagnitude(controller);
        if (magnitude < 0.01)
            return 0.0;
        vector pos = owner.GetOrigin();
        vector normal = SCR_TerrainHelper.GetTerrainNormal(pos, owner.GetWorld(), false, null);
        if (velocity.Length() < VELOCITY_SIGN_THRESHOLD)
            velocity = controller.GetVelocity();
        int sign = GetSlopeSignFromVelocity(normal, velocity);
        float slopeAngleDegrees = magnitude * sign;
        return Math.Clamp(slopeAngleDegrees, -45.0, 45.0);
    }
    
    // 获取坡度角度（完全用法线坡度 + 速度矢量判断上下坡）
    // 此方法直接返回坡度角度，单位为度。
    // @param controller 角色控制器组件
    // @param environmentFactor 环境因子组件（可选，用于室内检测)
    // @param velocity 速度矢量（可选，用于判断上下坡）
    // @return 坡度角度（度）
    static float GetSlopeAngle(SCR_CharacterControllerComponent controller, EnvironmentFactor environmentFactor = null, vector velocity = vector.Zero)
    {
        if (environmentFactor && controller)
        {
            IEntity ownerForCheck = controller.GetOwner();
            if (ownerForCheck && environmentFactor.IsIndoorForEntity(ownerForCheck))
                return 0.0;
        }
        return GetRawSlopeAngle(controller, velocity);
    }
    
    // 计算坡度百分比（考虑攀爬和跳跃状态）
    // 返回值中的 gradePercent 为坡度的百分比（rise/run × 100）。
    // 注意：原始角度由 GetSlopeAngle 返回（度），需要通过 tan() 转换。
    // @param controller 角色控制器组件
    // @param currentSpeed 当前速度（m/s）
    // @param jumpVaultDetector 跳跃检测器（可选）
    // @param slopeAngleDegrees 坡度角度（输入，通常为0.0；输出会被替换）
    // @param environmentFactor 环境因子组件（可选，用于室内检测）
    // @param velocity 速度矢量（可选，用于判断上下坡，游泳时传 computedVelocity）
    // @return 坡度计算结果（包含坡度百分比和角度）
    static GradeCalculationResult CalculateGradePercent(
        SCR_CharacterControllerComponent controller,
        float currentSpeed,
        JumpVaultDetector jumpVaultDetector,
        float slopeAngleDegrees,
        EnvironmentFactor environmentFactor = null,
        vector velocity = vector.Zero)
    {
        if (!s_pResultGrade)
            s_pResultGrade = new GradeCalculationResult();
        s_pResultGrade.gradePercent = 0.0;
        s_pResultGrade.slopeAngleDegrees = slopeAngleDegrees;
        
        // 检查是否在室内，如果是则返回0坡度
        // 使用 IsIndoorForEntity(owner) 确保服务器 RPC 路径下也能正确检测（m_pCachedOwner 可能未更新）
        if (environmentFactor && controller)
        {
            IEntity ownerForCheck = controller.GetOwner();
            if (ownerForCheck && environmentFactor.IsIndoorForEntity(ownerForCheck))
                return s_pResultGrade;
        }
        
        // 检查是否在攀爬或跳跃状态
        bool isClimbingForSlope = controller.IsClimbing();
        bool isJumpingForSlope = false;
        if (jumpVaultDetector)
            isJumpingForSlope = jumpVaultDetector.GetJumpInputTriggered();
        
        // 只在非攀爬、非跳跃状态下获取坡度
        if (!isClimbingForSlope && !isJumpingForSlope && currentSpeed > 0.05)
        {
            // 获取坡度角度并转换为坡度百分比（传入 velocity 用于判断上下坡）
            float rawAngleDeg = GetSlopeAngle(controller, environmentFactor, velocity);
            s_pResultGrade.slopeAngleDegrees = rawAngleDeg;
            // 将角度转换为斜率比：tan(angle_rad)
            float slopeRatio = Math.Tan(rawAngleDeg * Math.DEG2RAD);
            // Clamp ratio to reasonable range (-100%..100%) to avoid terrain/measurement glitches
            if (slopeRatio < -1.0 || slopeRatio > 1.0)
            {
                // log warning once per debug cycle
                if (StaminaConstants.IsDebugEnabled())
                slopeRatio = Math.Clamp(slopeRatio, -1.0, 1.0);
            }
            s_pResultGrade.gradePercent = slopeRatio * 100.0;
            if (StaminaConstants.IsDebugEnabled())
            {
            }
        }
        
        return s_pResultGrade;
    }
}
