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

// 恢复率计算上下文结构体（供 UpdateStaminaValue / GetNetStaminaRatePerSecond 共用）
class RecoveryContext
{
    float currentWeightForRecovery;
    float staticDrainForRecovery;
    int stanceInt;
    ref EnvironmentFactor envFactor;
    float speedForRecovery;
    float heatPenalty;
    float restDurationMinutes;
    float exerciseDurationMinutes;
}

class StaminaUpdateCoordinator
{
    // ── 静态共享结果对象 ──────────────────────────────────────────────────────
    // 【使用约定】每次调用后立即读取返回值，不得跨调用持有引用。
    // 同一帧内若需要两次调用结果，须将第一次返回值的字段复制到局部变量后再发起第二次调用，
    // 否则第二次调用会覆盖同一对象导致第一次结果丢失。
    protected static ref SpeedCalculationResult s_pResultSpeedCalc;
    protected static ref BaseDrainRateResult s_pResultBaseDrainRate;
    protected static ref RecoveryContext s_pRecoveryCtx;

    // ==================== 公共静态方法：计算陆地基础消耗率（用于消除重复代码）====================
    // 修复：提取此方法以避免在 SCR_StaminaConsumption.c 中重复实现
    // @param currentSpeed 当前速度（m/s）
    // @param encumbranceSpeedPenalty 负重速度惩罚（基础惩罚项）
    // @param currentWeightWithWet 包含湿重的总重量（kg）
    // @param gradePercent 坡度百分比
    // @param terrainFactor 地形系数（已包含泥泞修正）
    // @param windDrag 风阻系数
    // @param coldStaticPenalty 冷应激静态惩罚
    // @return 基础消耗率（每0.2秒）
    static float CalculateLandBaseDrainRate(
        float currentSpeed,
        float encumbranceSpeedPenalty,
        float currentWeightWithWet,
        float gradePercent,
        float terrainFactor,
        float windDrag,
        float coldStaticPenalty,
        bool isSprinting = false,
        int currentMovementPhase = -1)
    {
        float baseDrainRate = 0.0;

        // 优先使用移动类型（如果可用）：0=idle, 1=walk, 2=run, 3=sprint
        // 已统一公式：Walk/Run/Sprint 均用 Pandolf，不再对 Sprint 单独乘倍数
        if (currentMovementPhase >= 0)
        {
            if (currentMovementPhase == 0)
            {
                // Idle/Rest：仅在真正静止时恢复。
                // 引擎/动画状态偶发把移动中的实体标记为 idle；若 currentSpeed 已经在移动范围内，
                // 继续恢复会导致“冲刺/移动也回血”的反直觉行为。
                if (currentSpeed < 0.1)
                {
                    baseDrainRate = -StaminaConstants.REST_RECOVERY_PER_TICK;
                }
                else
                {
                    float pandolfPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                        currentSpeed,
                        currentWeightWithWet,
                        gradePercent,
                        terrainFactor,
                        true);

                    float bodyWeightIdle = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                    if (currentWeightWithWet > bodyWeightIdle && StaminaConstants.GetLoadMetabolicDampening() < 1.0)
                    {
                        float unloadedPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                            currentSpeed, bodyWeightIdle, gradePercent, terrainFactor, true);
                        float loadExtra = pandolfPerS - unloadedPerS;
                        pandolfPerS = unloadedPerS + loadExtra * StaminaConstants.GetLoadMetabolicDampening();
                    }

                    pandolfPerS = pandolfPerS * (1.0 + windDrag);
                    baseDrainRate = pandolfPerS * 0.2;
                }
            }
            else
            {
                // Walk/Run/Sprint：统一使用 Pandolf 能量消耗模型（以实际速度为主）
                float pandolfPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                    currentSpeed,
                    currentWeightWithWet,
                    gradePercent,
                    terrainFactor,
                    true);

                // T1 负重代谢阻尼
                float bodyWeightWRS = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;
                float unloadedPerSAtCurrentSpeed = -1.0;
                bool needsDampening = (currentWeightWithWet > bodyWeightWRS && StaminaConstants.GetLoadMetabolicDampening() < 1.0);
                if (needsDampening)
                {
                    unloadedPerSAtCurrentSpeed = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                        currentSpeed, bodyWeightWRS, gradePercent, terrainFactor, true);
                    float loadExtra = pandolfPerS - unloadedPerSAtCurrentSpeed;
                    pandolfPerS = unloadedPerSAtCurrentSpeed + loadExtra * StaminaConstants.GetLoadMetabolicDampening();
                }

                // 负重限速努力补偿（生理学近似）：当负重显著压低“跑/冲刺”的实际速度时，
                // 单纯用实际速度会低估局部肌肉招募与步态低效带来的代谢成本。
                // 用“无负重惩罚下的速度估计”计算同模型的上界，并按负重惩罚强度插值补回一部分。
                // 仅对 Run/Sprint 生效，避免把 Walk 的自我节能策略也当作“努力维持目标速度”。
                if (encumbranceSpeedPenalty > 0.0 && (currentMovementPhase == 2 || currentMovementPhase == 3) && currentSpeed > 0.1)
                {
                    float speedRatio = Math.Clamp(currentSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED, 0.0, 1.0);
                    float encPenalty = encumbranceSpeedPenalty * (1.0 + speedRatio);
                    if (isSprinting || currentMovementPhase == 3)
                        encPenalty = encPenalty * 1.5;
                    float maxPenalty = StaminaConstants.GetEncumbranceSpeedPenaltyMax();
                    encPenalty = Math.Clamp(encPenalty, 0.0, maxPenalty);

                    float denom = 1.0 - encPenalty;
                    denom = Math.Max(denom, 0.15);
                    float unencumberedSpeedEstimate = currentSpeed / denom;
                    unencumberedSpeedEstimate = Math.Min(unencumberedSpeedEstimate, 6.0); // 限制无负重速度最大值，避免消耗率过高

                    float effortPandolfPerS = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                        unencumberedSpeedEstimate,
                        currentWeightWithWet,
                        gradePercent,
                        terrainFactor,
                        true);

                    if (needsDampening)
                    {
                        float unloadedEffort;
                        if (Math.AbsFloat(unencumberedSpeedEstimate - currentSpeed) < 0.01)
                        {
                            unloadedEffort = unloadedPerSAtCurrentSpeed;
                        }
                        else
                        {
                            unloadedEffort = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
                                unencumberedSpeedEstimate, bodyWeightWRS, gradePercent, terrainFactor, true);
                        }
                        float effortExtra = effortPandolfPerS - unloadedEffort;
                        effortPandolfPerS = unloadedEffort + effortExtra * StaminaConstants.GetLoadMetabolicDampening();
                    }

                    if (effortPandolfPerS > pandolfPerS)
                    {
                        float blend = Math.Clamp(encPenalty / 0.7, 0.0, 0.8); // 降低混合因子，减少额外消耗
                        pandolfPerS = pandolfPerS + (effortPandolfPerS - pandolfPerS) * blend;
                    }
                }

                pandolfPerS = pandolfPerS * (1.0 + windDrag);
                baseDrainRate = pandolfPerS * 0.2; // 转换为每0.2秒
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
    // @param slopeSpeedTransition 坡度速度过渡模块（可选，用于 5 秒平滑，避免骤降感）
    // @return 最终速度倍数
    // 新增：基于输入计算最终速度倍数（供服务器权威使用）
    // @param sprintStartTime 本次冲刺开始时间（秒），-1 表示未在冲刺；服务器端由 controller.GetSprintStartTime() 传入
    static float CalculateFinalSpeedMultiplierFromInputs(
        float staminaPercent,
        float encumbranceSpeedPenalty,
        bool isSprinting,
        int currentMovementPhase,
        bool isExhausted,
        bool canSprint,
        float currentSpeed,
        float slopeAngleDegrees,
        float sprintStartTime = -1.0)
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
            currentSpeed,
            currentWorldTime,
            sprintStartTime);

        return finalSpeedMultiplier;
    }

    static float UpdateSpeed(
        SCR_CharacterControllerComponent controller,
        float staminaPercent,
        float encumbranceSpeedPenalty,
        CollapseTransition collapseTransition,
        float currentSpeed = 0.0,
        EnvironmentFactor environmentFactor = null,
        SlopeSpeedTransition slopeSpeedTransition = null,
        vector velocity = vector.Zero)
    {
        if (!controller)
            return 1.0;
        
        IEntity ownerForStairs = controller.GetOwner();
        bool shouldSuppressSlope = false;
        if (environmentFactor && ownerForStairs)
            shouldSuppressSlope = environmentFactor.ShouldSuppressTerrainSlopeForEntity(ownerForStairs);

        // 室内楼梯：有顶建筑物内且原始坡度>0 时减轻负重对速度的惩罚（与 shouldSuppressSlope 范围一致）
        float rawSlopeAngle = SpeedCalculator.GetRawSlopeAngle(controller, velocity);
        bool isIndoorStairs = (shouldSuppressSlope && Math.AbsFloat(rawSlopeAngle) > 0.0);
        if (isIndoorStairs)
            encumbranceSpeedPenalty = encumbranceSpeedPenalty * StaminaConstants.GetIndoorStairsEncumbranceSpeedFactor();
        
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
        
        // 室内（含楼梯间宽松判定）时硬归零，避免任何坡度速度惩罚
        float slopeAngleDegrees = 0.0;
        if (!shouldSuppressSlope)
            slopeAngleDegrees = SpeedCalculator.GetSlopeAngle(controller, environmentFactor, velocity);
        float runBaseSpeedMultiplier = SpeedCalculator.CalculateBaseSpeedMultiplier(
            staminaPercent, collapseTransition, currentWorldTime);
        
        // 通知过渡器当前帧的室内/室外状态，检测切换时机以便及时重置残留减速状态
        if (slopeSpeedTransition)
            slopeSpeedTransition.NotifySuppressSlope(shouldSuppressSlope);

        // 计算坡度自适应目标速度倍数
        float speedScaleFactor = 1.0;
        if (!shouldSuppressSlope)
        {
            float slopeAdjustedTargetSpeed = SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
                RealisticStaminaSpeedSystem.TARGET_RUN_SPEED, slopeAngleDegrees);
            float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            speedScaleFactor = slopeAdjustedTargetMultiplier / RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
            // 坡度变化时 5 秒平滑过渡，避免 3 m/s→1 m/s 瞬间骤降的"胶水感"
            if (slopeSpeedTransition)
                speedScaleFactor = slopeSpeedTransition.UpdateAndGet(currentWorldTime, speedScaleFactor);
        }
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;
        
        // 战术冲刺爆发期需要冲刺开始时间（由 controller 记录）
        float sprintStartTime = controller.GetSprintStartTime();
        
        // 计算最终绝对速度（仅在Run和Sprint模式下）
        // 注意：这个函数现在返回的是不含负重惩罚的理论速度
        float finalAbsoluteSpeedNoEncumbrance = SpeedCalculator.CalculateFinalAbsoluteSpeed(
            runBaseSpeedMultiplier,
            encumbranceSpeedPenalty,
            isSprinting,
            currentMovementPhase,
            isExhausted,
            canSprint,
            staminaPercent,
            currentSpeed,
            currentWorldTime,
            sprintStartTime);
        
        float finalSpeedMultiplier;
        
        if (finalAbsoluteSpeedNoEncumbrance > 0.0)
        {
            // 先计算负重惩罚（与原来的逻辑一致）
            float speedRatio = Math.Clamp(currentSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED, 0.0, 1.0);
            float encumbrancePenalty = encumbranceSpeedPenalty * (1.0 + speedRatio);
            if (isSprinting || currentMovementPhase == 3)
                encumbrancePenalty = encumbrancePenalty * 1.5;
            float maxPenalty = StaminaConstants.GetEncumbranceSpeedPenaltyMax();
            encumbrancePenalty = Math.Clamp(encumbrancePenalty, 0.0, maxPenalty);
            
            // 战术冲刺爆发期处理
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
            
            // 应用负重惩罚得到最终的理论目标速度
            float theoreticalTargetSpeed = finalAbsoluteSpeedNoEncumbrance * (1.0 - encumbrancePenalty);
            
            // 动态获取引擎当前的原始速度（已被负重降低后的速度）
            float currentEngineOriginalSpeed;
            if (isSprinting || currentMovementPhase == 3)
            {
                currentEngineOriginalSpeed = controller.GetOriginalEngineMaxSpeed_Sprint();
            }
            else
            {
                currentEngineOriginalSpeed = controller.GetOriginalEngineMaxSpeed_Run();
            }
            
            // 计算需要的补偿倍数：
            // 最终速度 = 引擎原始速度 × 倍数
            // 我们希望最终速度 = theoreticalTargetSpeed（理论计算速度）
            // 所以：倍数 = theoreticalTargetSpeed / 引擎原始速度
            if (currentEngineOriginalSpeed > 0.1)
            {
                finalSpeedMultiplier = theoreticalTargetSpeed / currentEngineOriginalSpeed;
            }
            else
            {
                // 回退方案：使用理论基准速度
                float theoreticalBaseSpeed;
                if (isSprinting || currentMovementPhase == 3)
                    theoreticalBaseSpeed = RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
                else
                    theoreticalBaseSpeed = RealisticStaminaSpeedSystem.TARGET_RUN_SPEED;
                finalSpeedMultiplier = theoreticalTargetSpeed / theoreticalBaseSpeed;
            }
            
            // 允许倍数大于1.0来补偿引擎的速度降低
            // 设置一个合理的上限，防止数值爆炸
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.01, 3.0);
        }
        else
        {
            // 非Run/Sprint模式，使用原来的速度倍数方法保持向后兼容
            finalSpeedMultiplier = SpeedCalculator.CalculateFinalSpeedMultiplier(
                runBaseSpeedMultiplier,
                encumbranceSpeedPenalty,
                isSprinting,
                currentMovementPhase,
                isExhausted,
                canSprint,
                staminaPercent,
                currentSpeed,
                currentWorldTime,
                sprintStartTime);
        }
        
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
        
        if (!s_pResultSpeedCalc)
            s_pResultSpeedCalc = new SpeedCalculationResult();
        s_pResultSpeedCalc.currentSpeed = currentSpeed;
        s_pResultSpeedCalc.lastPositionSample = currentPos;
        s_pResultSpeedCalc.hasLastPositionSample = true;
        s_pResultSpeedCalc.computedVelocity = velocity;
        
        return s_pResultSpeedCalc;
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
        float encumbranceSpeedPenalty,
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
                    Print("[RSS] 游泳 位置差分测速仍为0：可能未发生位移（静止/卡住/命令未推动位置）");
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

                // 完整室内或建筑物内有顶（镂空楼梯间）：忽略地形坡度/Pandolf 坡度项
                if (owner && environmentFactor.ShouldSuppressTerrainSlopeForEntity(owner))
                {
                    gradePercent = 0.0;
                }
                else if (!owner && environmentFactor.IsIndoor())
                {
                    gradePercent = 0.0; // 无 owner 时回退到 IsIndoor()
                }
            }

            // 应用泥泞地形系数（修正地形因子）
            terrainFactor = terrainFactor + mudTerrainFactor;

            // 应用降雨湿重（修正当前重量）
            currentWeightWithWet = currentWeightWithWet + totalWetWeight;


                // 修复：调用内部方法计算陆地基础消耗率，避免与 SCR_StaminaConsumption.c 重复
            baseDrainRate = CalculateLandBaseDrainRate(
                currentSpeed,
                encumbranceSpeedPenalty,
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
        
        if (!s_pResultBaseDrainRate)
            s_pResultBaseDrainRate = new BaseDrainRateResult();
        s_pResultBaseDrainRate.baseDrainRate = baseDrainRate;
        s_pResultBaseDrainRate.swimmingVelocityDebugPrinted = swimmingVelocityDebugPrinted;
        return s_pResultBaseDrainRate;
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
            RecoveryContext ctx = BuildRecoveryContext(
                false,
                encumbranceCache,
                exerciseTracker,
                controller,
                environmentFactor,
                baseDrainRateByVelocity,
                baseDrainRateByVelocityForModule,
                heatStressMultiplier,
                currentSpeed);

            recoveryRate = StaminaRecoveryCalculator.CalculateRecoveryRate(
                staminaPercent,
                ctx.restDurationMinutes,
                ctx.exerciseDurationMinutes,
                ctx.currentWeightForRecovery,
                ctx.staticDrainForRecovery,
                false,
                ctx.stanceInt,
                ctx.envFactor,
                ctx.speedForRecovery,
                0,
                controller);

            recoveryRate = recoveryRate * ctx.heatPenalty;
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

    // 获取当前净体力变化率（用于 HUD 显示耗尽/回满时间）
    // @param inVehicle 是否在载具中；载具内只恢复不消耗，不应用 EPOC 延迟消耗
    // @return 净变化率（/秒）：负=消耗中，正=恢复中。恢复/消耗率按每0.2秒设计，乘以5转为每秒
    // ==================== 私有辅助方法：构建恢复率计算上下文 ====================
    // 供 UpdateStaminaValue 和 GetNetStaminaRatePerSecond 共用，消除重复的参数组装逻辑。
    // @param inVehicle    是否在载具内（载具内使用固定参数：零负重、趴姿、无环境）
    // @param encumbranceCache  负重缓存（inVehicle=true 时忽略）
    // @param exerciseTracker   运动跟踪器
    // @param controller        角色控制器（inVehicle=true 时可为 null）
    // @param environmentFactor 环境因子（inVehicle=true 时忽略）
    // @param baseDrainRateByVelocity        基础消耗率（行人用）
    // @param baseDrainRateByVelocityForModule 模块基础消耗率（行人用）
    // @param heatStressMultiplier 热应激倍数（行人用）
    // @param currentSpeed 当前速度（行人用）
    // @return 填充好的 RecoveryContext（复用静态实例 s_pRecoveryCtx）
    static RecoveryContext BuildRecoveryContext(
        bool inVehicle,
        EncumbranceCache encumbranceCache,
        ExerciseTracker exerciseTracker,
        SCR_CharacterControllerComponent controller,
        EnvironmentFactor environmentFactor,
        float baseDrainRateByVelocity,
        float baseDrainRateByVelocityForModule,
        float heatStressMultiplier,
        float currentSpeed)
    {
        if (!s_pRecoveryCtx)
            s_pRecoveryCtx = new RecoveryContext();

        if (inVehicle)
        {
            s_pRecoveryCtx.currentWeightForRecovery = 0.0;
            s_pRecoveryCtx.staticDrainForRecovery = -StaminaConstants.REST_RECOVERY_PER_TICK;
            s_pRecoveryCtx.stanceInt = 2;
            s_pRecoveryCtx.envFactor = null;
            s_pRecoveryCtx.speedForRecovery = 0.0;
            s_pRecoveryCtx.heatPenalty = 1.0;
        }
        else
        {
            float currentWeightForRecovery = 0.0;
            if (encumbranceCache && encumbranceCache.IsCacheValid())
                currentWeightForRecovery = encumbranceCache.GetCurrentWeight();
            s_pRecoveryCtx.currentWeightForRecovery = StaminaRecoveryCalculator.CalculateRecoveryWeight(currentWeightForRecovery, controller);

            s_pRecoveryCtx.staticDrainForRecovery = baseDrainRateByVelocity;
            if (baseDrainRateByVelocityForModule > 0.0)
                s_pRecoveryCtx.staticDrainForRecovery = baseDrainRateByVelocityForModule;

            ECharacterStance stanceForRecovery = controller.GetStance();
            if (stanceForRecovery == ECharacterStance.PRONE)
                s_pRecoveryCtx.staticDrainForRecovery = 0.0;

            if (stanceForRecovery == ECharacterStance.PRONE)
                s_pRecoveryCtx.stanceInt = 2;
            else if (stanceForRecovery == ECharacterStance.CROUCH)
                s_pRecoveryCtx.stanceInt = 1;
            else
                s_pRecoveryCtx.stanceInt = 0;

            s_pRecoveryCtx.envFactor = environmentFactor;
            s_pRecoveryCtx.speedForRecovery = currentSpeed;
            s_pRecoveryCtx.heatPenalty = 1.0 / heatStressMultiplier;
        }

        s_pRecoveryCtx.restDurationMinutes = 0.0;
        s_pRecoveryCtx.exerciseDurationMinutes = 0.0;
        if (exerciseTracker)
        {
            s_pRecoveryCtx.restDurationMinutes = exerciseTracker.GetRestDurationMinutes();
            s_pRecoveryCtx.exerciseDurationMinutes = exerciseTracker.GetExerciseDurationMinutes();
        }

        return s_pRecoveryCtx;
    }

    static float GetNetStaminaRatePerSecond(
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
        SCR_CharacterControllerComponent controller,
        EnvironmentFactor environmentFactor = null,
        bool inVehicle = false)
    {
        float recoveryRate = 0.0;
        bool isInEpocDelay = false;
        float speedBeforeStop = 0.0;
        if (epocState)
        {
            isInEpocDelay = epocState.IsInEpocDelay();
            speedBeforeStop = epocState.GetSpeedBeforeStop();
        }

        // 载具内始终计算恢复（载具不应用 EPOC 消耗）；行人仅在非 EPOC 时计算恢复
        if (!isInEpocDelay || inVehicle)
        {
            RecoveryContext ctx = BuildRecoveryContext(
                inVehicle,
                encumbranceCache,
                exerciseTracker,
                controller,
                environmentFactor,
                baseDrainRateByVelocity,
                baseDrainRateByVelocityForModule,
                heatStressMultiplier,
                currentSpeed);

            recoveryRate = StaminaRecoveryCalculator.CalculateRecoveryRate(
                staminaPercent, ctx.restDurationMinutes, ctx.exerciseDurationMinutes,
                ctx.currentWeightForRecovery, ctx.staticDrainForRecovery, false, ctx.stanceInt,
                ctx.envFactor, ctx.speedForRecovery, 0, controller);

            recoveryRate = recoveryRate * ctx.heatPenalty;
        }

        float epocDrainRate = 0.0;
        if (isInEpocDelay && !inVehicle)
            epocDrainRate = StaminaRecoveryCalculator.CalculateEpocDrainRate(speedBeforeStop);

        float finalDrainRate = totalDrainRate + epocDrainRate;
        float netRatePerTick = recoveryRate - finalDrainRate;
        return netRatePerTick * 5.0;
    }
}