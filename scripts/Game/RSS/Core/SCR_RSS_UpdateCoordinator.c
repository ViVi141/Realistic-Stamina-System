// 体力更新协调器模块
// DTOs: SCR_RSS_UpdateCoordinatorTypes.c；净率/ETA: SCR_RSS_StaminaNetRate.c

class SCR_RSS_UpdateCoordinator
{
    // ── 结果对象（每次调用新分配，消除静态共享竞态）──
    // 高密度 AI 并行调用时静态共享对象会导致数据覆盖。
    // 修复：移除 s_pResultSpeedCalc / s_pResultBaseDrainRate，
    // BuildRecoveryContext 每次 new RecoveryContext()，调用方读取后即可释放。

    // ==================== 公共静态方法：计算陆地基础消耗率（用于消除重复代码）====================
    // 修复：提取此方法以避免在 SCR_RSS_StaminaConsumptionCalculator.c 中重复实现
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
        float appliedSpeedLimitMs,
        float currentWeightWithWet,
        float gradePercent,
        float terrainFactor,
        float windDrag,
        float coldStaticPenalty,
        int currentMovementPhase = -1,
        float encumbranceSpeedPenalty = 0.0,
        float effectiveCriticalPowerWatts = -1.0,
        float wPrimePool01 = 1.0)
    {
        float idleThreshold = SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS;

        float measuredSpeedMs = currentSpeed;
        float speedForPowerMs = measuredSpeedMs;
        if (currentMovementPhase >= 1 && measuredSpeedMs > 0.05)
        {
            float capMs = appliedSpeedLimitMs;
            if (capMs <= 0.05)
            {
                capMs = SCR_RSS_DrainCalculator.GetTheoreticalMaxSpeedMs(
                    currentMovementPhase, encumbranceSpeedPenalty);
            }
            speedForPowerMs = SCR_RSS_DrainCalculator.GetMetabolicAccountingVelocityMs(
                measuredSpeedMs, capMs, wPrimePool01);
        }

        int phase = currentMovementPhase;
        if (phase < 0)
            phase = 2;

        if (measuredSpeedMs < idleThreshold)
        {
            float bodyWeight = SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
            float loadWeight = Math.Max(currentWeightWithWet - bodyWeight, 0.0);
            float staticPerS = SCR_RSS_MetabolismModel.CalculateStaticStandingCost(bodyWeight, loadWeight);
            if (coldStaticPenalty > 0.0)
                staticPerS = staticPerS * (1.0 + coldStaticPenalty);
            return staticPerS * SCR_RSS_Constants.RSS_STAMINA_TICK_SEC;
        }

        float powerW = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
            speedForPowerMs,
            currentWeightWithWet,
            gradePercent,
            terrainFactor,
            true,
            phase);
        float pandolfPerS = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(
            powerW, effectiveCriticalPowerWatts);

        if (phase == 3)
        {
            pandolfPerS = pandolfPerS * SCR_RSS_Constants.V6_SPRINT_AEROBIC_DRAIN_FACTOR;
            if (effectiveCriticalPowerWatts > 1.0 && powerW > effectiveCriticalPowerWatts)
                pandolfPerS = pandolfPerS * SCR_RSS_Constants.V6_SPRINT_WPRIME_STA_RELIEF;
        }

        // 风阻/热应激由 StaminaConsumptionCalculator.GetQuickEnvironmentMultiplier 单层施加，此处不再叠 windDrag
        return pandolfPerS * SCR_RSS_Constants.RSS_STAMINA_TICK_SEC;
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

        float runBaseSpeedMultiplier = SCR_RSS_SpeedCalculator.CalculateBaseSpeedMultiplier(
            staminaPercent, null, currentWorldTime);

        // 计算坡度自适应目标速度倍数
        float slopeAdjustedTargetSpeed = SCR_RSS_SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
            SCR_RSS_MetabolismMath.TARGET_RUN_SPEED, slopeAngleDegrees);
        float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
        float speedScaleFactor = slopeAdjustedTargetMultiplier / SCR_RSS_MetabolismMath.TARGET_RUN_SPEED_MULTIPLIER;
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;

        float finalSpeedMultiplier = SCR_RSS_SpeedCalculator.CalculateFinalSpeedMultiplier(
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
        SCR_RSS_CollapseTransition collapseTransition,
        float currentSpeed = 0.0,
        SCR_RSS_EnvironmentFactor environmentFactor = null,
        SCR_RSS_SlopeSpeedTransition slopeSpeedTransition = null,
        vector velocity = vector.Zero,
        float terrainFactor = 1.0,
        int effectiveMovementPhase = -1)
    {
        if (!controller)
            return 1.0;
        
        IEntity ownerForStairs = controller.GetOwner();
        bool shouldSuppressSlope = false;
        if (environmentFactor && ownerForStairs)
            shouldSuppressSlope = environmentFactor.ShouldSuppressTerrainSlopeForEntity(ownerForStairs);

        // 室内楼梯：有顶建筑物内且原始坡度>0 时减轻负重对速度的惩罚（与 shouldSuppressSlope 范围一致）
        float rawSlopeAngle = SCR_RSS_SpeedCalculator.GetRawSlopeAngle(controller, velocity);
        bool isIndoorStairs = (shouldSuppressSlope && Math.AbsFloat(rawSlopeAngle) > 0.0);
        if (isIndoorStairs)
            encumbranceSpeedPenalty = encumbranceSpeedPenalty * SCR_RSS_Constants.GetIndoorStairsEncumbranceSpeedFactor();
        
        // 检查是否可以 Sprint（v5：有氧门槛 + 无氧池/冷却）
        bool canSprint = controller.GetRssSprintAllowed();
        bool isExhausted = SCR_RSS_MetabolismMath.IsExhausted(staminaPercent);

        // 检测移动类型
        bool isSprinting = controller.IsSprinting();
        int currentMovementPhase = controller.GetCurrentMovementPhase();
        if (effectiveMovementPhase >= 0 && effectiveMovementPhase <= 3)
            currentMovementPhase = effectiveMovementPhase;
        
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
            slopeAngleDegrees = SCR_RSS_SpeedCalculator.GetSlopeAngle(controller, environmentFactor, velocity);
        float runBaseSpeedMultiplier = SCR_RSS_SpeedCalculator.CalculateBaseSpeedMultiplier(
            staminaPercent, collapseTransition, currentWorldTime);
        
        // 通知过渡器当前帧的室内/室外状态，检测切换时机以便及时重置残留减速状态
        if (slopeSpeedTransition)
            slopeSpeedTransition.NotifySuppressSlope(shouldSuppressSlope);

        // 计算坡度自适应目标速度倍数
        float speedScaleFactor = 1.0;
        if (!shouldSuppressSlope)
        {
            float slopeRunBaseMs = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
            float slopeRunRefMult = slopeRunBaseMs / SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
            if (slopeRunRefMult < 0.01)
                slopeRunRefMult = 0.01;
            float slopeAdjustedTargetSpeed = SCR_RSS_SpeedCalculator.CalculateSlopeAdjustedTargetSpeed(
                slopeRunBaseMs, slopeAngleDegrees);
            float slopeAdjustedTargetMultiplier = slopeAdjustedTargetSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
            speedScaleFactor = slopeAdjustedTargetMultiplier / slopeRunRefMult;
            // 坡度变化时 5 秒平滑过渡，避免 3 m/s→1 m/s 瞬间骤降的"胶水感"
            if (slopeSpeedTransition)
                speedScaleFactor = slopeSpeedTransition.UpdateAndGet(currentWorldTime, speedScaleFactor);
        }
        runBaseSpeedMultiplier = runBaseSpeedMultiplier * speedScaleFactor;
        
        // 战术冲刺爆发期需要冲刺开始时间（由 controller 记录）
        float sprintStartTime = controller.GetSprintStartTime();
        
        // 计算最终绝对速度（Run/Sprint/Walk）；负重已在 GetMarchAbsoluteSpeedMs 内施加
        float finalAbsoluteSpeedWithEnc = SCR_RSS_SpeedCalculator.CalculateFinalAbsoluteSpeed(
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
        
        if (finalAbsoluteSpeedWithEnc > 0.0)
        {
            // 负重惩罚（供 Sprint 功率反解与 Run CP 封顶；勿再乘 (1-enc)，GetMarch 已计入）
            float speedRatio = Math.Clamp(currentSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED, 0.0, 1.0);
            float encumbrancePenalty = encumbranceSpeedPenalty * (1.0 + speedRatio);
            if (isSprinting || currentMovementPhase == 3)
                encumbrancePenalty = encumbrancePenalty * 1.5;
            float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
            encumbrancePenalty = Math.Clamp(encumbrancePenalty, 0.0, maxPenalty);
            encumbrancePenalty = SCR_RSS_SpeedCalculator.ApplyTacticalSprintBurstEncumbranceRelief(
                encumbrancePenalty,
                isSprinting,
                currentMovementPhase,
                currentWorldTime,
                sprintStartTime);

            float theoreticalTargetSpeed = finalAbsoluteSpeedWithEnc;

            if (isSprinting || currentMovementPhase == 3)
            {
                SCR_RSS_AnaerobicBurst anaBurst = controller.RSS_GetWPrimeBurst();
                if (anaBurst && anaBurst.GetCpModel())
                {
                    float totalWeightKg = controller.GetRssCurrentWeight()
                        + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
                    float gradePct = 0.0;
                    if (!shouldSuppressSlope)
                    {
                        GradeCalculationResult gradeRes = SCR_RSS_SpeedCalculator.CalculateGradePercent(
                            controller, currentSpeed, null, slopeAngleDegrees, environmentFactor, velocity);
                        gradePct = gradeRes.gradePercent;
                    }
                    float sprintTerrainFactor = terrainFactor;
                    if (sprintTerrainFactor < 0.5)
                        sprintTerrainFactor = 0.5;
                    if (sprintTerrainFactor > 3.0)
                        sprintTerrainFactor = 3.0;
                    theoreticalTargetSpeed = SCR_RSS_SpeedCalculator.GetV6SprintSpeedMs(
                        encumbrancePenalty,
                        totalWeightKg,
                        gradePct,
                        sprintTerrainFactor,
                        anaBurst.GetCpModel(),
                        currentWorldTime,
                        0.017);
                }
            }

            if (!(isSprinting || currentMovementPhase == 3))
            {
                SCR_RSS_AnaerobicBurst anaRun = controller.RSS_GetWPrimeBurst();
                if (anaRun && anaRun.GetCpModel())
                {
                    SCR_RSS_CriticalPowerModel cpRun = anaRun.GetCpModel();
                    if (!SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(cpRun.GetPool01()))
                    {
                        float totalWeightKg = controller.GetRssCurrentWeight()
                            + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
                        float gradePct = 0.0;
                        if (!shouldSuppressSlope)
                        {
                            GradeCalculationResult gradeRes = SCR_RSS_SpeedCalculator.CalculateGradePercent(
                                controller, currentSpeed, null, slopeAngleDegrees, environmentFactor, velocity);
                            gradePct = gradeRes.gradePercent;
                        }
                        float runTerrain = terrainFactor;
                        if (runTerrain < 0.5)
                            runTerrain = 0.5;
                        if (runTerrain > 3.0)
                            runTerrain = 3.0;
                        int runPhase = currentMovementPhase;
                        if (runPhase < 1)
                            runPhase = 2;
                        float cpCapMs = SCR_RSS_MetabolismModel.InvertSpeedForPowerWatts(
                            cpRun.GetEffectiveCriticalPowerWatts(),
                            totalWeightKg,
                            gradePct,
                            runTerrain,
                            runPhase);
                        if (cpCapMs > 0.05 && theoreticalTargetSpeed > cpCapMs)
                            theoreticalTargetSpeed = cpCapMs;
                    }
                }
            }

            if ((isSprinting || currentMovementPhase == 3) && environmentFactor)
            {
                float mudSprintPenalty = environmentFactor.GetMudSprintPenalty();
                if (mudSprintPenalty > 0.0)
                    theoreticalTargetSpeed = theoreticalTargetSpeed * (1.0 - mudSprintPenalty);
            }
            
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
                    theoreticalBaseSpeed = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
                else
                    theoreticalBaseSpeed = SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
                finalSpeedMultiplier = theoreticalTargetSpeed / theoreticalBaseSpeed;
            }
            
            // 允许倍数大于1.0来补偿引擎的速度降低
            // 设置一个合理的上限，防止数值爆炸
            finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.01, 3.0);

            // 无氧/冷却禁冲刺时玩家仍可能按住 Shift：引擎处于 Sprint 基准，必须用 Sprint 原始速度作分母，
            // 否则 limit 作用在冲刺上限上会得到 runTarget×(sprint/run) > runTarget，表现为 ANA=0 仍可冲刺。
            bool engineStillSprinting = controller.IsSprinting() || controller.GetCurrentMovementPhase() == 3;
            if (!canSprint && engineStillSprinting)
            {
                float sprintEngineOriginal = controller.GetOriginalEngineMaxSpeed_Sprint();
                if (sprintEngineOriginal > 0.1)
                    finalSpeedMultiplier = theoreticalTargetSpeed / sprintEngineOriginal;
                finalSpeedMultiplier = Math.Clamp(finalSpeedMultiplier, 0.01, 3.0);
            }
        }
        else
        {
            // 非Run/Sprint模式，使用原来的速度倍数方法保持向后兼容
            finalSpeedMultiplier = SCR_RSS_SpeedCalculator.CalculateFinalSpeedMultiplier(
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
        
        SpeedCalculationResult s_pResultSpeedCalc = new SpeedCalculationResult();
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
        SCR_RSS_EnvironmentFactor environmentFactor = null,
        int currentMovementPhase = -1,
        float appliedSpeedLimitMs = -1.0,
        float effectiveCriticalPowerWatts = -1.0,
        float wPrimePool01 = 1.0)
    {
        float baseDrainRate = 0.0;
        
        if (isSwimming)
        {
            // ==================== 游泳体力消耗模型（物理阻力模型）====================
            // 调试信息：只在第一次检测到游泳速度仍为 0 时输出（避免刷屏）
            if (SCR_RSS_ConfigBridge.IsDebugEnabled() && owner == SCR_PlayerController.GetLocalControlledEntity() && !swimmingVelocityDebugPrinted)
            {
                float swimVelLen = computedVelocity.Length();
                if (swimVelLen < 0.01)
                {
                    Print("[RSS] 游泳 位置差分测速仍为0：可能未发生位移（静止/卡住/命令未推动位置）");
                    swimmingVelocityDebugPrinted = true;
                }
            }
            
            float swimmingDrainRate = SCR_RSS_SwimmingStaminaModel.CalculateSwimmingStaminaDrain3D(computedVelocity, currentWeightWithWet);
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

            // 注意：currentWeightWithWet 在调用方(PlayerBase)已经包含了 totalWetWeight，
            // 此处不再重复累加，避免湿重被计入两次


                // 修复：调用内部方法计算陆地基础消耗率，避免与 SCR_RSS_StaminaConsumptionCalculator.c 重复
            baseDrainRate = CalculateLandBaseDrainRate(
                currentSpeed,
                appliedSpeedLimitMs,
                currentWeightWithWet,
                gradePercent,
                terrainFactor,
                windDrag,
                coldStaticPenalty,
                currentMovementPhase,
                encumbranceSpeedPenalty,
                effectiveCriticalPowerWatts,
                wPrimePool01);

            // 负重影响现在通过后续的 encumbranceStaminaDrainMultiplier 应用，
            // 因此不再需要对固定 Sprint 基线做特殊处理。
        }
        
        BaseDrainRateResult s_pResultBaseDrainRate = new BaseDrainRateResult();
        s_pResultBaseDrainRate.baseDrainRate = baseDrainRate;
        s_pResultBaseDrainRate.swimmingVelocityDebugPrinted = swimmingVelocityDebugPrinted;
        return s_pResultBaseDrainRate;
    }

    //! 玩家跳跃/翻越/姿态切换即时体力扣减（Integration 仅传参）
    //! @return 扣减后的 staminaPercent（已 Clamp 0-1）
    static float ApplyPlayerActionStaminaCosts(
        SCR_CharacterControllerComponent controller,
        IEntity owner,
        float staminaPercent,
        float timeDeltaSec,
        float currentTimeSec,
        SCR_RSS_JumpVaultDetector jumpVault,
        SCR_RSS_StanceTransitionManager stanceMgr,
        SCR_RSS_EncumbranceCache encCache,
        SCR_RSS_UISignalBridge uiBridge,
        bool debugEnabled)
    {
        if (!controller || !owner || !jumpVault || !stanceMgr)
            return staminaPercent;

        SignalsManagerComponent signalsManager = null;
        if (uiBridge)
            signalsManager = uiBridge.GetSignalsManager();
        int exhaustionSignalID = -1;
        if (uiBridge)
            exhaustionSignalID = uiBridge.GetExhaustionSignalID();

        bool encValid = false;
        float encWeight = 0.0;
        if (encCache)
        {
            encValid = encCache.IsCacheValid();
            encWeight = encCache.GetCurrentWeight();
        }

        float jumpCost = jumpVault.ProcessJump(
            owner, controller, staminaPercent, encValid, encWeight,
            signalsManager, exhaustionSignalID);
        if (jumpCost > 0.0 && debugEnabled)
        {
            PrintFormat("[RSS] Jump Cost: -%1%%",
                Math.Round(jumpCost * 100.0 * 10.0) / 10.0);
        }
        staminaPercent = staminaPercent - jumpCost;

        float vaultCost = jumpVault.ProcessVault(owner, controller, encValid, encWeight);
        if (vaultCost > 0.0 && debugEnabled)
        {
            PrintFormat("[RSS] Vault Cost: -%1%%",
                Math.Round(vaultCost * 100.0 * 10.0) / 10.0);
        }
        staminaPercent = staminaPercent - vaultCost;

        jumpVault.UpdateCooldowns();
        stanceMgr.UpdateFatigue(timeDeltaSec);

        float stanceSettleCost = stanceMgr.TrySettleWindow(currentTimeSec);
        if (stanceSettleCost > 0.0)
            staminaPercent = staminaPercent - stanceSettleCost;

        float stanceCost = stanceMgr.ProcessStanceTransition(
            owner, controller, staminaPercent, encValid, encWeight);
        if (stanceCost > 0.0)
            staminaPercent = staminaPercent - stanceCost;

        return Math.Clamp(staminaPercent, 0.0, 1.0);
    }

    //! 汇总单 tick 总消耗率（含疲劳/热应激/战斗兴奋剂/步行恢复区）
    static StaminaDrainTickResult CalculateTotalDrainRate(StaminaDrainTickParams tick)
    {
        StaminaDrainTickResult result = new StaminaDrainTickResult();
        result.totalDrainRate = 0.0;
        result.baseDrainRateByVelocity = 0.0;
        result.baseDrainRateByVelocityForModule = 0.0;
        if (!tick)
            return result;

        result.swimmingVelocityDebugPrinted = tick.swimmingVelocityDebugPrinted;

        if (tick.fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
            tick.fatigueSystem.ProcessFatigueDecay(tick.currentTimeSec, tick.currentSpeed);

        bool isCurrentlyMoving = (tick.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS);
        float fatigueFactor = 1.0;
        if (tick.exerciseTracker)
        {
            tick.exerciseTracker.Update(tick.currentTimeForExerciseMs, isCurrentlyMoving);
            if (tick.useSwimmingModel)
                fatigueFactor = tick.exerciseTracker.CalculateFatigueFactor();
        }

        float metabolicEfficiencyFactor = 1.0;
        float fitnessEfficiencyFactor = 1.0;
        if (tick.useSwimmingModel)
        {
            metabolicEfficiencyFactor = SCR_RSS_StaminaConsumptionCalculator.CalculateMetabolicEfficiencyFactor(tick.speedRatio);
            fitnessEfficiencyFactor = SCR_RSS_StaminaConsumptionCalculator.CalculateFitnessEfficiencyFactor();
        }
        float totalEfficiencyFactor = fitnessEfficiencyFactor * metabolicEfficiencyFactor;

        BaseDrainRateResult drainRateResult = CalculateBaseDrainRate(
            tick.useSwimmingModel,
            tick.currentSpeed,
            tick.encumbranceSpeedPenalty,
            tick.bodyPlusGearWeightKg,
            tick.totalWeightWithWetAndBody,
            tick.gradePercent,
            tick.terrainFactor,
            tick.velocityForDrain,
            result.swimmingVelocityDebugPrinted,
            tick.owner,
            tick.environmentFactor,
            tick.currentMovementPhase,
            tick.appliedSpeedLimitMs,
            tick.effectiveCriticalPowerWatts,
            tick.wPrimePool01);
        result.baseDrainRateByVelocity = drainRateResult.baseDrainRate;
        result.swimmingVelocityDebugPrinted = drainRateResult.swimmingVelocityDebugPrinted;

        float postureMultiplier = 1.0;
        float gradePercentForConsumption = 0.0;
        float terrainFactorForConsumption = 1.0;
        if (!tick.useSwimmingModel)
        {
            postureMultiplier = SCR_RSS_StaminaConsumptionCalculator.CalculatePostureMultiplier(tick.currentSpeed, tick.controller);
            gradePercentForConsumption = tick.gradePercent;
            terrainFactorForConsumption = tick.terrainFactor;
        }

        float encumbranceStaminaDrainMultiplier = 1.0;
        if (tick.useSwimmingModel)
        {
            if (tick.encumbranceCache)
                encumbranceStaminaDrainMultiplier = tick.encumbranceCache.GetStaminaDrainMultiplier();
        }

        result.baseDrainRateByVelocityForModule = result.baseDrainRateByVelocity;

        if (tick.useSwimmingModel)
        {
            result.totalDrainRate = result.baseDrainRateByVelocity;
            result.totalDrainRate = result.totalDrainRate * totalEfficiencyFactor * fatigueFactor;
        }
        else
        {
            result.totalDrainRate = SCR_RSS_StaminaConsumptionCalculator.CalculateStaminaConsumption(
                tick.currentSpeed,
                tick.gearWeightKg,
                gradePercentForConsumption,
                terrainFactorForConsumption,
                postureMultiplier,
                totalEfficiencyFactor,
                fatigueFactor,
                encumbranceStaminaDrainMultiplier,
                tick.fatigueSystem,
                result.baseDrainRateByVelocityForModule,
                tick.environmentFactor,
                tick.owner,
                tick.isSprinting,
                tick.currentMovementPhase);
            // 陆地：热/风/泥已在 CalculateStaminaConsumption 快路径施加；勿再乘 heatStressMultiplier（避免双重叠层）
        }

        if (tick.combatStimActive)
            result.totalDrainRate = result.totalDrainRate * SCR_CombatStimConstants.STAMINA_DRAIN_MULTIPLIER;

        if (tick.isSprintActive)
            result.totalDrainRate = result.totalDrainRate * SCR_RSS_ConfigBridge.GetSprintStaminaDrainMultiplierEffective();

        result.totalDrainRate = result.totalDrainRate * SCR_RSS_ConfigBridge.GetCustomStaminaDrainMultiplier();

        float walkRecoveryThreshold = SCR_RSS_Constants.GetWalkRecoveryZoneThreshold();
        float walkRecoveryRatePerTick = SCR_RSS_Constants.GetWalkRecoveryZoneRate();
        if (!tick.useSwimmingModel && !tick.isSprintActive && tick.staminaPercent < walkRecoveryThreshold && tick.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
        {
            result.totalDrainRate = -walkRecoveryRatePerTick;
            result.baseDrainRateByVelocity = -walkRecoveryRatePerTick;
            result.baseDrainRateByVelocityForModule = -walkRecoveryRatePerTick;
        }

        if (result.baseDrainRateByVelocityForModule == 0.0 && result.baseDrainRateByVelocity > 0.0)
            result.baseDrainRateByVelocityForModule = result.baseDrainRateByVelocity;

        if (tick.epocState && !tick.useSwimmingModel)
            SCR_RSS_RecoveryCalculator.UpdateEpocDelay(tick.epocState, tick.currentSpeed, tick.currentTimeSec);

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
        SCR_RSS_EpocState epocState,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_RSS_FatigueSystem fatigueSystem,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EnvironmentFactor environmentFactor = null,
        float timeDeltaSeconds = 0.2)
    {
        if (!staminaComponent)
            return staminaPercent;
        
        float newTargetStamina = staminaPercent;
        
        float recoveryRate = SCR_RSS_StaminaNetRate.ComputeRecoveryRatePerTick(
            staminaPercent,
            currentSpeed,
            baseDrainRateByVelocity,
            baseDrainRateByVelocityForModule,
            heatStressMultiplier,
            epocState,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            false);

        float finalDrainRate = SCR_RSS_StaminaNetRate.ComputeFinalDrainRatePerTick(
            useSwimmingModel,
            currentSpeed,
            totalDrainRate,
            epocState,
            false);

        // AI 伤害-体力联动（模块 F）
        if (controller && !controller.IsPlayerControlled() && SCR_RSS_ConfigBridge.IsAIInjuryLinkEnabled())
        {
            IEntity injuryOwner = controller.GetOwner();
            if (injuryOwner)
            {
                float injuryDrainMul = 1.0;
                float injuryRecoveryMul = 1.0;
                if (SCR_RSS_AIInjuryLink.GetInjuryMultipliers(injuryOwner, injuryDrainMul, injuryRecoveryMul))
                {
                    finalDrainRate = finalDrainRate * injuryDrainMul;
                    recoveryRate = recoveryRate * injuryRecoveryMul;
                }
            }
        }
        
        // 代谢净值算法：netChange = (recoveryRate - totalDrainRate) * (timeDelta/0.2)
        // 恢复/消耗率按每0.2秒设计；实际更新间隔可能为50ms，需按时间比例缩放
        float tickScale = Math.Clamp(timeDeltaSeconds / 0.2, 0.01, 2.0);
        float netChange = (recoveryRate - finalDrainRate) * tickScale;
        
        // 更新目标体力值
        newTargetStamina = staminaPercent + netChange;
        
        // ==================== 应用疲劳惩罚：限制最大体力上限（模块化）====================
        float maxStaminaCap = 1.0;
        if (fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
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
