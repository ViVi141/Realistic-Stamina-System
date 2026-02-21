// 体力更新协调器模块
// 负责协调体力消耗和恢复的计算流程
// 模块化拆分：从 PlayerBase.c 提取的体力更新协调逻辑

// 速度计算结果结构体
class SpeedCalculationResult
{
    float currentSpeed;
    vector lastPositionSample;
    bool hasLastPositionSample;
    vector computedVelocity;
}

// 基础消耗率计算结果结构体
class BaseDrainRateResult
{
    float baseDrainRate;
    bool swimmingVelocityDebugPrinted;
}

class StaminaUpdateCoordinator
{
    // ==================== 公共静态方法：计算陆地基础消耗率（用于消除重复代码）====================
    // 修复：提取此方法以避免在 SCR_StaminaConsumption.c 中重复实现
    // @param currentSpeed 当前速度（m/s）
    // @param currentWeightWithWet 包含湿重的总重量（kg）
    // @param gradePercent 坡度百分比
    // @param terrainFactor 地形系数（已包含泥泞修正）
    // @param windDrag 风阻系数
    // @param coldStaticPenalty 冷应激静态惩罚
    // @return 基础消耗率（每0.2秒）
    static float CalculateLandBaseDrainRate(
        float currentSpeed,
        float currentWeightWithWet,
        float gradePercent,
        float terrainFactor,
        float windDrag,
        float coldStaticPenalty,
        bool isSprinting = false,
        int currentMovementPhase = -1)
    {
        float baseDrainRate = 0.0;

        // 优先使用移动类型（如果可用）：isSprinting 或 currentMovementPhase（0=idle,1=walk,2=run,3=sprint）
        if (currentMovementPhase >= 0)
        {
            if (isSprinting || currentMovementPhase == 3)
            {
                // Sprint & Run：统一使用 Pandolf 能量消耗模型
                float pandolfPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                    currentSpeed,
                    currentWeightWithWet,
                    gradePercent,
                    terrainFactor,
                    true);
                pandolfPerS = pandolfPerS * (1.0 + windDrag);
                baseDrainRate = pandolfPerS * 0.2; // 转换为每0.2秒
            }
            else if (currentMovementPhase == 2)
            {
                // Run（同上 Pandolf 模型）
                float pandolfPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                    currentSpeed,
                    currentWeightWithWet,
                    gradePercent,
                    terrainFactor,
                    true);
                pandolfPerS = pandolfPerS * (1.0 + windDrag);
                baseDrainRate = pandolfPerS * 0.2; // 转换为每0.2秒的消耗率
            }
            else if (currentMovementPhase == 1)
            {
                // Walk（Pandolf）
                float pandolfPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                    currentSpeed,
                    currentWeightWithWet,
                    gradePercent,
                    terrainFactor,
                    true);
                pandolfPerS = pandolfPerS * (1.0 + windDrag);
                baseDrainRate = pandolfPerS * 0.2;
            }
            else
            {
                // Idle/Rest：恢复（强制）
                baseDrainRate = -StaminaConstants.REST_RECOVERY_PER_TICK;
            }
        }
        else
        {
            // 向后兼容：基于速度的原始逻辑
            bool isRunning = (currentSpeed > 2.2);

            if (currentSpeed < 0.1)
            {
                // 静态负重站立消耗（Pandolf 静态项）
                float bodyWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 90kg
                float loadWeight = Math.Max(currentWeightWithWet - bodyWeight, 0.0); // 负重（去除身体重量，使用湿重）

                float staticDrainRate = RealisticStaminaSpeedSystem.CalculateStaticStandingCost(bodyWeight, loadWeight);
                staticDrainRate = staticDrainRate * (1.0 + coldStaticPenalty);
                baseDrainRate = staticDrainRate * 0.2; // 每0.2秒
            }
            else if (isRunning)
            {
                // Backward-compatibility path: previous versions used the Givoni-Goldman
                // running formula for speeds >2.2 m/s.  The current system no longer
                // invokes this branch during normal operation because movement-phase
                // logic now routes everything through Pandolf.  We keep it here solely
                // so that older config presets or saved data still produce a plausible
                // drain value if they somehow re-enter this legacy branch.
                float runningDrainRate = RealisticStaminaSpeedSystem.CalculateGivoniGoldmanRunning(currentSpeed, currentWeightWithWet, true);
                runningDrainRate = runningDrainRate * terrainFactor;
                runningDrainRate = runningDrainRate * (1.0 + windDrag);
                baseDrainRate = runningDrainRate * 0.2;
            }
            else
            {
                baseDrainRate = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                    currentSpeed,
                    currentWeightWithWet,
                    gradePercent,
                    terrainFactor,
                    true);
                baseDrainRate = baseDrainRate * (1.0 + windDrag);
                baseDrainRate = baseDrainRate * 0.2;
            }
        }

        return baseDrainRate;
    }

    // ==================== 速度计算和更新 ====================
    
    // 更新速度（基于体力和负重）
    // @param controller 角色控制器组件
    // @param staminaPercent 当前体力百分比
    // @param encumbranceSpeedPenalty 负重速度惩罚
    // @param collapseTransition "撞墙"阻尼过渡模块
    // @param currentSpeed 当前速度 (m/s)
    // @param environmentFactor 环境因子组件（可选，用于室内检测）
    // @return 最终速度倍数
    // 新增：基于输入计算最终速度倍数（供服务器权威使用）
    static float CalculateFinalSpeedMultiplierFromInputs(
        float staminaPercent,
        float encumbranceSpeedPenalty,
        bool isSprinting,
        int currentMovementPhase,
        bool isExhausted,
        bool canSprint,
        float currentSpeed,
        float slopeAngleDegrees)
    {
        // 与 UpdateSpeed 中相同的决策逻辑，但使用传入参数（无 controller）
        if (isExhausted || !canSprint)
        {
            if (isSprinting || currentMovementPhase == 3)
            {
                currentMovementPhase = 2;
                isSprinting = false;
            }
        }

        float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 秒

        float runBaseSpeedMultiplier = SpeedCalculator.CalculateBaseSpeedMultiplier(
            staminaPercent, null, currentWorldTime);

        // 计算坡度自适应目标速度倍数
        float slopeAdjustedTargetSpeed = SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
            RealisticStaminaSpeedSystem.TARGET_RUN_SPEED, slopeAngleDegrees);
        float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
        float speedScaleFactor = slopeAdjustedTargetMultiplier / RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;

        float finalSpeedMultiplier = SpeedCalculator.CalculateFinalSpeedMultiplier(
            runBaseSpeedMultiplier,
            encumbranceSpeedPenalty,
            isSprinting,
            currentMovementPhase,
            isExhausted,
            canSprint,
            staminaPercent,
            currentSpeed);

        return finalSpeedMultiplier;
    }

    static float UpdateSpeed(
        SCR_CharacterControllerComponent controller,
        float staminaPercent,
        float encumbranceSpeedPenalty,
        CollapseTransition collapseTransition,
        float currentSpeed = 0.0,
        EnvironmentFactor environmentFactor = null)
    {
        if (!controller)
            return 1.0;
        
        // 检查是否可以Sprint
        bool canSprint = RealisticStaminaSpeedSystem.CanSprint(staminaPercent);
        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);

        // 检测移动类型
        bool isSprinting = controller.IsSprinting();
        int currentMovementPhase = controller.GetCurrentMovementPhase();
        
        // 如果精疲力尽，禁用Sprint
        if (isExhausted || !canSprint)
        {
            if (isSprinting || currentMovementPhase == 3)
            {
                currentMovementPhase = 2;
                isSprinting = false;
            }
        }
        
        // 计算速度倍数
        float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
        
        // 获取坡度角度，考虑室内检测
        float slopeAngleDegrees = SpeedCalculator.GetSlopeAngle(controller, environmentFactor);
        float runBaseSpeedMultiplier = SpeedCalculator.CalculateBaseSpeedMultiplier(
            staminaPercent, collapseTransition, currentWorldTime);
        
        // 计算坡度自适应目标速度倍数
        float slopeAdjustedTargetSpeed = SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
            RealisticStaminaSpeedSystem.TARGET_RUN_SPEED, slopeAngleDegrees);
        float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
        float speedScaleFactor = slopeAdjustedTargetMultiplier / RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;
        
        // 计算最终速度倍数
        float finalSpeedMultiplier = SpeedCalculator.CalculateFinalSpeedMultiplier(
            runBaseSpeedMultiplier,
            encumbranceSpeedPenalty,
            isSprinting,
            currentMovementPhase,
            isExhausted,
            canSprint,
            staminaPercent,
            currentSpeed);
        
        // 应用速度倍数
        controller.OverrideMaxSpeed(finalSpeedMultiplier);
        
        return finalSpeedMultiplier;
    }
    
    // ==================== 速度计算（位置差分测速）====================
    
    // 计算当前速度（使用位置差分测速，适用于游泳）
    // @param owner 角色实体
    // @param lastPositionSample 上一帧位置（输入）
    // @param hasLastPositionSample 是否有上一帧位置（输入）
    // @param computedVelocity 计算得到的速度（输入，通常为vector.Zero）
    // @param dtSeconds 时间步长（秒）
    // @return 速度计算结果（包含速度、位置、标志和速度向量）
    static SpeedCalculationResult CalculateCurrentSpeed(
        IEntity owner,
        vector lastPositionSample,
        bool hasLastPositionSample,
        vector computedVelocity,
        float dtSeconds)
    {
        vector currentPos = owner.GetOrigin();
        vector velocity = vector.Zero;
        
        if (hasLastPositionSample)
        {
            vector deltaPos = currentPos - lastPositionSample;
            float deltaLen = deltaPos.Length();
            
            // 防止传送/同步跳变导致天文速度
            // 在0.2s采样周期内，如果位移超过1.6米（对应时速约28.8km/h，超过人体Sprint极限），判定为异常跳变
            if (deltaLen < 1.6 && dtSeconds > 0.001)
            {
                velocity = deltaPos / dtSeconds;
                // 增加物理硬上限保护：强制将velocity模长限制在7.0 m/s以内
                if (velocity.Length() > 7.0)
                    velocity = velocity.Normalized() * 7.0;
            }
            else
            {
                // 异常跳变时，保持上一周期的速度
                velocity = computedVelocity;
            }
        }
        
        // 计算水平速度
        vector horizontalVelocity = velocity;
        horizontalVelocity[1] = 0.0; // 忽略垂直速度
        float currentSpeed = horizontalVelocity.Length();
        
        // 确保currentSpeed不超过物理上限
        currentSpeed = Math.Min(currentSpeed, 7.0);
        
        SpeedCalculationResult result = new SpeedCalculationResult();
        result.currentSpeed = currentSpeed;
        result.lastPositionSample = currentPos;
        result.hasLastPositionSample = true;
        result.computedVelocity = velocity;
        
        return result;
    }
    
    // ==================== 基础消耗率计算 ====================
    
    // 计算基础消耗率（游泳或陆地）
    // @param isSwimming 是否在游泳
    // @param currentSpeed 当前速度（m/s）
    // @param currentWeight 当前重量（kg）
    // @param currentWeightWithWet 包含湿重的总重量（kg）
    // @param gradePercent 坡度百分比
    // @param terrainFactor 地形系数
    // @param computedVelocity 计算得到的速度向量（用于游泳）
    // @param swimmingVelocityDebugPrinted 是否已输出游泳速度调试信息（输入）
    // @param owner 角色实体（用于调试）
    // @param environmentFactor 环境因子模块引用（v2.14.0修复：添加此参数以支持环境因子）
    // @return 基础消耗率结果（包含消耗率和调试标志）
    static BaseDrainRateResult CalculateBaseDrainRate(
        bool isSwimming,
        float currentSpeed,
        float currentWeight,
        float currentWeightWithWet,
        float gradePercent,
        float terrainFactor,
        vector computedVelocity,
        bool swimmingVelocityDebugPrinted,
        IEntity owner,
        EnvironmentFactor environmentFactor = null,
        bool isSprinting = false,
        int currentMovementPhase = -1)
    {
        float baseDrainRate = 0.0;
        
        if (isSwimming)
        {
            // ==================== 游泳体力消耗模型（物理阻力模型）====================
            // 调试信息：只在第一次检测到游泳速度仍为 0 时输出（避免刷屏）
            if (StaminaConstants.IsDebugEnabled() && owner == SCR_PlayerController.GetLocalControlledEntity() && !swimmingVelocityDebugPrinted)
            {
                float swimVelLen = computedVelocity.Length();
                if (swimVelLen < 0.01)
                {
                    Print("[游泳速度] 位置差分测速仍为0：可能未发生位移（静止/卡住/命令未推动位置）");
                    swimmingVelocityDebugPrinted = true;
                }
            }
            
            float swimmingDrainRate = RealisticStaminaSpeedSystem.CalculateSwimmingStaminaDrain3D(computedVelocity, currentWeightWithWet);
            baseDrainRate = swimmingDrainRate * 0.2; // 转换为每0.2秒的消耗率
        }
        else
        {
            // ==================== v2.14.0修复：环境因子处理 ====================
            // 获取环境因子（如果环境因子模块存在）
            float windDrag = 0.0;
            float mudTerrainFactor = 0.0;
            float totalWetWeight = 0.0;
            float coldStaticPenalty = 0.0;

            if (environmentFactor)
            {
                windDrag = environmentFactor.GetWindDrag();
                mudTerrainFactor = environmentFactor.GetMudTerrainFactor();
                totalWetWeight = environmentFactor.GetTotalWetWeight();
                coldStaticPenalty = environmentFactor.GetColdStaticPenalty();

                // 检查是否在室内，如果是则忽略坡度影响
                if (environmentFactor.IsIndoor())
                {
                    gradePercent = 0.0; // 室内时坡度为0
                }
            }

            // 应用泥泞地形系数（修正地形因子）
            terrainFactor = terrainFactor + mudTerrainFactor;

            // 应用降雨湿重（修正当前重量）
            currentWeightWithWet = currentWeightWithWet + totalWetWeight;

            // 计算负重影响因子（与 RealisticStaminaSpeedSystem 中的实现一致）
            float weightRatio = currentWeightWithWet / RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
            float weightRatioPower = StaminaHelpers.Pow(weightRatio, 1.2);
            float loadFactor = 1.0 + (weightRatioPower * 1.5);

                // 修复：调用内部方法计算陆地基础消耗率，避免与 SCR_StaminaConsumption.c 重复
            baseDrainRate = CalculateLandBaseDrainRate(
                currentSpeed,
                currentWeightWithWet,
                gradePercent,
                terrainFactor,
                windDrag,
                coldStaticPenalty,
                isSprinting,
                currentMovementPhase);

            // 负重影响现在通过后续的 encumbranceStaminaDrainMultiplier 应用，
            // 因此不再需要对固定 Sprint 基线做特殊处理。
        }
        
        BaseDrainRateResult result = new BaseDrainRateResult();
        result.baseDrainRate = baseDrainRate;
        result.swimmingVelocityDebugPrinted = swimmingVelocityDebugPrinted;
        return result;
    }
    
    // ==================== 体力更新协调 ====================
    
    // 协调体力消耗和恢复计算，更新目标体力值
    // @param staminaComponent 体力组件
    // @param staminaPercent 当前体力百分比
    // @param useSwimmingModel 是否使用游泳模型
    // @param currentSpeed 当前速度（m/s）
    // @param totalDrainRate 总消耗率（每0.2秒）
    // @param baseDrainRateByVelocity 基础消耗率（每0.2秒）
    // @param baseDrainRateByVelocityForModule 模块计算的基础消耗率（每0.2秒）
    // @param heatStressMultiplier 热应激倍数
    // @param epocState EPOC状态管理
    // @param encumbranceCache 负重缓存
    // @param exerciseTracker 运动跟踪器
    // @param fatigueSystem 疲劳系统
    // @param controller 角色控制器组件
    // @param environmentFactor 环境因子模块引用（v2.14.0新增）
    // @param timeDeltaSeconds 实际距上次更新的秒数（恢复/消耗率按每0.2s设计，需按 timeDelta/0.2 缩放）
    // @return 新的目标体力值
    static float UpdateStaminaValue(
        SCR_CharacterStaminaComponent staminaComponent,
        float staminaPercent,
        bool useSwimmingModel,
        float currentSpeed,
        float totalDrainRate,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        EpocState epocState,
        EncumbranceCache encumbranceCache,
        ExerciseTracker exerciseTracker,
        FatigueSystem fatigueSystem,
        SCR_CharacterControllerComponent controller,
        EnvironmentFactor environmentFactor = null,
        float timeDeltaSeconds = 0.2)
    {
        if (!staminaComponent)
            return staminaPercent;
        
        float newTargetStamina = staminaPercent;
        
        // ==================== 代谢净值算法：netChange = recoveryRate - totalDrainRate ====================
        
        // 计算恢复率（无论是否运动，都计算恢复率）
        float recoveryRate = 0.0;
        
        // 检查是否处于EPOC延迟期间
        bool isInEpocDelay = false;
        float speedBeforeStop = 0.0;
        if (epocState)
        {
            isInEpocDelay = epocState.IsInEpocDelay();
            speedBeforeStop = epocState.GetSpeedBeforeStop();
        }
        
        if (!isInEpocDelay)
        {
            // 计算恢复相关参数
            float currentWeightForRecovery = 0.0;
            if (encumbranceCache && encumbranceCache.IsCacheValid())
                currentWeightForRecovery = encumbranceCache.GetCurrentWeight();
            
            currentWeightForRecovery = StaminaRecoveryCalculator.CalculateRecoveryWeight(currentWeightForRecovery, controller);
            
            float restDurationMinutes = 0.0;
            float exerciseDurationMinutes = 0.0;
            if (exerciseTracker)
            {
                restDurationMinutes = exerciseTracker.GetRestDurationMinutes();
                exerciseDurationMinutes = exerciseTracker.GetExerciseDurationMinutes();
            }
            
            float staticDrainForRecovery = baseDrainRateByVelocity;
            if (baseDrainRateByVelocityForModule > 0.0)
                staticDrainForRecovery = baseDrainRateByVelocityForModule;
            
            // 趴下休息：不应沿用"静态站立消耗"，否则会把恢复压成持续损耗
            ECharacterStance stanceForRecovery = controller.GetStance();
            if (stanceForRecovery == ECharacterStance.PRONE)
                staticDrainForRecovery = 0.0;
            
            // 将姿态转换为整数（0=站立，1=蹲姿，2=趴姿）
            int stanceInt = 0;
            if (stanceForRecovery == ECharacterStance.PRONE)
                stanceInt = 2;
            else if (stanceForRecovery == ECharacterStance.CROUCH)
                stanceInt = 1;
            else
                stanceInt = 0;
            
            recoveryRate = StaminaRecoveryCalculator.CalculateRecoveryRate(
                staminaPercent,
                restDurationMinutes,
                exerciseDurationMinutes,
                currentWeightForRecovery,
                staticDrainForRecovery,
                false,
                stanceInt,
                environmentFactor,
                currentSpeed);
            
            // ==================== 热应激对恢复的影响（模块化）====================
            // 生理学依据：高温不仅让人动起来累，更让人休息不回来
            // 热应激越大，恢复倍数越小（恢复速度越慢）
            float heatRecoveryPenalty = 1.0 / heatStressMultiplier; // 热应激1.3倍时，恢复速度降至约77%
            recoveryRate = recoveryRate * heatRecoveryPenalty;
        }
        
        // 计算EPOC延迟期间的消耗
        float epocDrainRate = 0.0;
        if (isInEpocDelay)
        {
            epocDrainRate = StaminaRecoveryCalculator.CalculateEpocDrainRate(speedBeforeStop);
        }
        
        // 计算总消耗率
        float finalDrainRate = totalDrainRate + epocDrainRate;
        
        // 代谢净值算法：netChange = (recoveryRate - totalDrainRate) * (timeDelta/0.2)
        // 恢复/消耗率按每0.2秒设计；实际更新间隔可能为50ms，需按时间比例缩放
        float tickScale = Math.Clamp(timeDeltaSeconds / 0.2, 0.01, 2.0);
        float netChange = (recoveryRate - finalDrainRate) * tickScale;
        
        // 更新目标体力值
        newTargetStamina = staminaPercent + netChange;
        
        // ==================== 调试信息 ====================
        static float nextMetabolismLogTime = 0.0;
        if (StaminaConstants.ShouldLog(nextMetabolismLogTime))
        {
            PrintFormat("[RealisticSystem] 代谢净值 / Metabolism Net Change: %1%% → %2%% (恢复率: %3/0.2s, 消耗率: %4/0.2s, 净值×%.2f: %5) | %1%% → %2%%",
                Math.Round(staminaPercent * 100.0).ToString(),
                Math.Round(newTargetStamina * 100.0).ToString(),
                Math.Round(recoveryRate * 1000000.0) / 1000000.0,
                Math.Round(finalDrainRate * 1000000.0) / 1000000.0,
                tickScale,
                Math.Round(netChange * 1000000.0) / 1000000.0);
        }
        
        // ==================== 应用疲劳惩罚：限制最大体力上限（模块化）====================
        float maxStaminaCap = 1.0;
        if (fatigueSystem)
            maxStaminaCap = fatigueSystem.GetMaxStaminaCap();
        
        // 限制体力值在有效范围内（0.0 - maxStaminaCap）
        newTargetStamina = Math.Clamp(newTargetStamina, 0.0, maxStaminaCap);
        
        // 如果当前体力超过疲劳惩罚后的上限，需要降低
        if (staminaPercent > maxStaminaCap)
        {
            // 立即降低到上限（模拟疲劳导致的体力上限降低）
            newTargetStamina = maxStaminaCap;
        }
        
        return newTargetStamina;
    }
}
