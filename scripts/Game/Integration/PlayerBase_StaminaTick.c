// PlayerBase split: 主 tick：UpdateSpeedBasedOnStamina
modded class SCR_CharacterControllerComponent
{
    void UpdateSpeedBasedOnStamina()
    {
        if (m_bIsDeleted)
            return;
        IEntity owner = GetOwner();
        if (!owner)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }

        if (!GetGame())
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }
        World world = GetGame().GetWorld();
        if (!world)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }

        if (!ShouldProcessStaminaUpdate())
        {
            RSS_SetMudSlipCameraShake01(0.0);
            m_bRssStaminaLoopActive = false;
            return;
        }

        if (SCR_PlayerBaseVehicleHelper.HandleVehicleStaminaUpdate(
                this, owner, m_pCompartmentAccess, m_pStaminaComponent,
                m_pExerciseTracker, m_pFatigueSystem, m_pEpocState,
                m_pEncumbranceCache, m_pEnvironmentFactor,
                m_pTerrainDetector, m_pStanceTransitionManager,
                m_fLastStaminaUpdateTime, m_fCurrentWetWeight,
                GetSpeedUpdateIntervalMs(), IsRssDebugEnabled()))
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
            return;
        }
        
        // perf: AI 快速路径 — 跳过 AI 不需要的玩家专属逻辑
        bool isPlayer = IsPlayerControlled();

        if (isPlayer)
        {
            RSS_CombatStim_OnTickTransitions();

            // 药效/OD 期间：流血倍率刷新
            if (Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
            {
                if (m_pCachedOwnerCharacter)
                {
                    SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                    if (stimDmgMgr)
                        SCR_CombatStimController.RefreshBleedingEffectsToMatchScale(stimDmgMgr);
                }
            }
        }

        // 药效/OD 期间：非玩家实体（如 AI 被注射）也需要刷新流血倍率
        if (!isPlayer && Replication.IsServer() && SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase))
        {
            if (m_pCachedOwnerCharacter)
            {
                SCR_CharacterDamageManagerComponent stimDmgMgr = SCR_CharacterDamageManagerComponent.Cast(m_pCachedOwnerCharacter.GetDamageManager());
                if (stimDmgMgr)
                    SCR_CombatStimController.RefreshBleedingEffectsToMatchScale(stimDmgMgr);
            }
        }

        float staminaPercent = 1.0;
        if (m_pStaminaComponent)
            staminaPercent = m_pStaminaComponent.GetTargetStamina();

        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        
        float encumbranceSpeedPenalty = 0.0;
        if (m_pEncumbranceCache)
        {
            m_pEncumbranceCache.CheckAndUpdate();
            encumbranceSpeedPenalty = m_pEncumbranceCache.GetSpeedPenalty();
        }

        bool isExhausted = RealisticStaminaSpeedSystem.IsExhausted(staminaPercent);
        if (isExhausted)
        {
            float limpSpeedMultiplier = RealisticStaminaSpeedSystem.GetDynamicLimpMultiplier(encumbranceSpeedPenalty);
            float compensatedLimpMultiplier = Math.Clamp(limpSpeedMultiplier * m_fAnimSpeedCompensation, 0.01, 1.0);
            SCR_RSS_CharacterSpeedBridge.ApplyStaminaSpeedLimit(owner, compensatedLimpMultiplier);

            if (!m_bLastExhaustedState && IsRssDebugEnabled())
            {
                SCR_RSS_Logger.Debug("[RSS] Exhausted: limp speed");
                m_bLastExhaustedState = true;
            }

        }
        else
        {
            if (m_bLastExhaustedState && IsRssDebugEnabled())
            {
                SCR_RSS_Logger.Debug("[RSS] Recovered from Exhaustion");
                m_bLastExhaustedState = false;
            }
        }
        
        bool isSwimmingForSpeed = SwimmingStateManager.IsSwimming(this);
        vector velocity;
        float currentSpeed;
        if (isSwimmingForSpeed)
        {
            float dtSeconds = GetSpeedUpdateIntervalMs() / 1000.0;
            SpeedCalculationResult speedResult = StaminaUpdateCoordinator.CalculateCurrentSpeed(
                owner, m_vLastPositionSample, m_bHasLastPositionSample, m_vComputedVelocity, dtSeconds);
            velocity = speedResult.computedVelocity;
            currentSpeed = Math.Min(speedResult.computedVelocity.Length(), 7.0);  // 游泳用 3D 速度模长
            m_vLastPositionSample = speedResult.lastPositionSample;
            m_bHasLastPositionSample = speedResult.hasLastPositionSample;
            m_vComputedVelocity = speedResult.computedVelocity;
        }
        else
        {
            velocity = GetVelocity();
            currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(velocity);
            m_bHasLastPositionSample = false;  // 离开游泳时重置
        }
        
        if (isPlayer) RSS_UpdateTacticalSprintState();
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        bool isSprintActive = isSprintingNow || (phaseNow == 3);
        
        float currentTimeForExerciseMs = world.GetWorldTime();
        float currentTime = currentTimeForExerciseMs / 1000.0;
        float terrainFactor = 1.0; // 默认值（铺装路面）
        if (m_pTerrainDetector)
            terrainFactor = m_pTerrainDetector.GetTerrainFactor(owner, currentTime, currentSpeed);

        if (m_pEnvironmentFactor)
            m_pEnvironmentFactor.UpdateEnvironmentFactors(currentTime, owner, velocity, terrainFactor, m_fCurrentWetWeight);

        float finalSpeedMultiplier = StaminaUpdateCoordinator.UpdateSpeed(
            this,
            staminaPercent,
            encumbranceSpeedPenalty,
            m_pCollapseTransition,
            currentSpeed,
            m_pEnvironmentFactor,
            m_pSlopeSpeedTransition,
            velocity);
        
        float baseSpeedMultiplier = RealisticStaminaSpeedSystem.CalculateSpeedMultiplierByStamina(staminaPercent);
        
        float currentWeight = 0.0;
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            currentWeight = m_pEncumbranceCache.GetCurrentWeight();
        else
        {
            if (!m_pCachedInventoryComponent)
                m_pCachedInventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(owner.FindComponent(SCR_CharacterInventoryStorageComponent));
            if (m_pCachedInventoryComponent)
                currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
        }

        float speedToApply = finalSpeedMultiplier;
        
        if (!Replication.IsServer() && IsPlayerControlled() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled() && m_pNetworkSyncManager.HasServerValidation())
        {
            m_pNetworkSyncManager.GetTargetSpeedMultiplier(finalSpeedMultiplier);
            speedToApply = m_pNetworkSyncManager.GetSmoothedSpeedMultiplier(currentTime);
        }
        else
        {
            speedToApply = finalSpeedMultiplier;
        }
        float finalSpeedToApply = Math.Clamp(speedToApply, 0.01, 1.0);
        m_fLastRssSpeedMultiplierApplied = finalSpeedToApply;
        SCR_RSS_CharacterSpeedBridge.ApplyStaminaSpeedLimit(owner, finalSpeedToApply);
        if (IsPlayerControlled())
            m_sLastSpeedSource = "Client";
        else
            m_sLastSpeedSource = "Server";

        // AI 仅速度模式：应用速度倍率后跳过体力消耗/恢复链
        if (!isPlayer && StaminaConfigBridge.IsAiStaminaCalcDisabled())
        {
            m_bRssStaminaLoopActive = true;
            GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
            return;
        }

        bool isCriticalData = (staminaPercent <= 0.05 || (m_pNetworkSyncManager && m_pNetworkSyncManager.GetLastReportedStaminaPercent() > 0.5 && staminaPercent <= 0.1));
        // CRITICAL FIX: Double-check GetServerDataExportEnabled() after ShouldSync.
        // If admin disabled data export between ShouldSync and Rpc(), we skip sending.
        if (isPlayer && !Replication.IsServer() && m_pNetworkSyncManager && SCR_RSS_ConfigManager.GetServerDataExportEnabled())
        {
            int syncType = 0;
            if (isCriticalData)
                syncType = 1;
            if (m_pNetworkSyncManager.ShouldSync(currentTime, syncType))
            {
                // Re-check before sending RPC: admin may have disabled data export
                // in the split second between the outer check and here.
                if (!SCR_RSS_ConfigManager.GetServerDataExportEnabled())
                    return;
                Rpc(RPC_ClientReportStamina, staminaPercent, currentWeight, currentTime, isCriticalData);
                if (isCriticalData && IsRssDebugEnabled())
                    SCR_PlayerBaseDebugHelper.LogCriticalStaminaEvent(staminaPercent);
            }
        }

        bool isSwimming = SwimmingStateManager.IsSwimming(this);

        float timeDeltaSec;
        if (m_fLastStaminaUpdateTime >= 0.0)
            timeDeltaSec = currentTime - m_fLastStaminaUpdateTime;
        else
            timeDeltaSec = GetSpeedUpdateIntervalMs() / 1000.0;
        timeDeltaSec = Math.Clamp(timeDeltaSec, 0.01, 0.5);
        
        // v5: 无氧冲刺状态更新（在 timeDeltaSec 和 currentWeight 定义之后）
        if (isPlayer && m_pAnaerobicState)
        {
            float aerobicStamina = staminaPercent;
            float drainVelocity = currentSpeed;
            float loadWeight = currentWeight;
            
            m_pAnaerobicState.TickUpdate(timeDeltaSec, aerobicStamina, drainVelocity, loadWeight);
            
            // 检测冲刺开始/结束
            bool wasSprintingLastFrame = m_bLastWasSprinting;
            if (isSprintActive && !wasSprintingLastFrame)
            {
                m_pAnaerobicState.OnSprintStart(currentSpeed, currentWeight);
            }
            else
            {
                if (!isSprintActive && wasSprintingLastFrame)
                {
                    bool forcedByDepletion = (m_pAnaerobicState.GetAnaerobicEnergy() < 0.01);
                    m_pAnaerobicState.OnSprintEnd(forcedByDepletion);
                }
            }
            
            // 服务端同步状态到网络组件
            if (Replication.IsServer() && m_pAnaerobicNetworkComponent)
            {
                m_pAnaerobicNetworkComponent.SyncFromLogicState(m_pAnaerobicState);
            }
        }

        // 编队行军：由 AIManager.Tick() 内部处理（每 tick 平滑收敛 + 行为层 500ms 节流）

        if (isSwimming != m_bWasSwimming)
            m_bSwimmingVelocityDebugPrinted = false;
        
        if (isPlayer)
        {
            WetWeightUpdateResult wetWeightResult = SwimmingStateManager.UpdateWetWeight(
                m_bWasSwimming,
                isSwimming,
                currentTime,
                m_fWetWeightStartTime,
                m_fCurrentWetWeight,
                owner);
            m_fWetWeightStartTime = wetWeightResult.wetWeightStartTime;
            m_fCurrentWetWeight = wetWeightResult.currentWetWeight;
            m_bWasSwimming = isSwimming;
        }
        
        float heatStressMultiplier = 1.0;
        if (m_pEnvironmentFactor)
            heatStressMultiplier = m_pEnvironmentFactor.GetHeatStressMultiplier();
        
        float rainWeight = 0.0;
        if (m_pEnvironmentFactor)
            rainWeight = m_pEnvironmentFactor.GetRainWeight();

        if (isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
        {
            heatStressMultiplier = 1.0;
            rainWeight = 0.0;
        }
        
        float totalWetWeight = SwimmingStateManager.CalculateTotalWetWeight(m_fCurrentWetWeight, rainWeight);
        float currentWeightWithWet = currentWeight + totalWetWeight;

        float totalWeight = currentWeight + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+身体
        float totalWeightWithWetAndBody = currentWeightWithWet + RealisticStaminaSpeedSystem.CHARACTER_WEIGHT; // 装备+湿重+身体

        bool useSwimmingModel = isSwimming;

        if (isPlayer && m_pStaminaComponent && m_pJumpVaultDetector)
        {
            SignalsManagerComponent signalsManager = null;
            if (m_pUISignalBridge)
                signalsManager = m_pUISignalBridge.GetSignalsManager();
            int exhaustionSignalID = -1;
            if (m_pUISignalBridge)
                exhaustionSignalID = m_pUISignalBridge.GetExhaustionSignalID();
            
            bool encumbranceCacheValid = false;
            float encumbranceCurrentWeight = 0.0;
            if (m_pEncumbranceCache)
            {
                encumbranceCacheValid = m_pEncumbranceCache.IsCacheValid();
                encumbranceCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            }
            float jumpCost = m_pJumpVaultDetector.ProcessJump(
                owner, 
                this, 
                staminaPercent, 
                encumbranceCacheValid, 
                encumbranceCurrentWeight,
                signalsManager, 
                exhaustionSignalID
            );
            if (jumpCost > 0.0 && IsRssDebugEnabled())
                SCR_PlayerBaseDebugHelper.LogJumpCost(jumpCost);
            staminaPercent = staminaPercent - jumpCost;
            
            bool vaultEncumbranceCacheValid = false;
            float vaultEncumbranceCurrentWeight = 0.0;
            if (m_pEncumbranceCache)
            {
                vaultEncumbranceCacheValid = m_pEncumbranceCache.IsCacheValid();
                vaultEncumbranceCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            }
            float vaultCost = m_pJumpVaultDetector.ProcessVault(
                owner, 
                this, 
                vaultEncumbranceCacheValid, 
                vaultEncumbranceCurrentWeight
            );
            if (vaultCost > 0.0 && IsRssDebugEnabled())
                SCR_PlayerBaseDebugHelper.LogVaultCost(vaultCost);
            staminaPercent = staminaPercent - vaultCost;
            
            m_pJumpVaultDetector.UpdateCooldowns();
            
            m_pStanceTransitionManager.UpdateFatigue(timeDeltaSec);
            
            float currentTimeSec = world.GetWorldTime() / 1000.0;
            float stanceSettleCost = m_pStanceTransitionManager.TrySettleWindow(currentTimeSec);
            if (stanceSettleCost > 0.0)
                staminaPercent = staminaPercent - stanceSettleCost;
            
            bool stanceEncumbranceCacheValid = false;
            float stanceEncumbranceCurrentWeight = 0.0;
            if (m_pEncumbranceCache)
            {
                stanceEncumbranceCacheValid = m_pEncumbranceCache.IsCacheValid();
                stanceEncumbranceCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
            }
            float stanceTransitionCost = m_pStanceTransitionManager.ProcessStanceTransition(
                owner,
                this,
                staminaPercent,
                stanceEncumbranceCacheValid,
                stanceEncumbranceCurrentWeight
            );
            if (stanceTransitionCost > 0.0)
            {
                staminaPercent = staminaPercent - stanceTransitionCost;
            }
            
            staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);
        }
        
        float speedRatio = Math.Clamp(currentSpeed / RealisticStaminaSpeedSystem.GAME_MAX_SPEED, 0.0, 1.0);
        float totalDrainRate = 0.0;

        if (m_pFatigueSystem)
        {
            float currentTimeForFatigue = world.GetWorldTime() / 1000.0; // 转换为秒
            m_pFatigueSystem.ProcessFatigueDecay(currentTimeForFatigue, currentSpeed);
        }
        
        bool isCurrentlyMoving = (currentSpeed > 0.05);
        float fatigueFactor = 1.0; // 默认值（无疲劳）
        
        if (m_pExerciseTracker)
        {
            m_pExerciseTracker.Update(currentTimeForExerciseMs, isCurrentlyMoving);
            
            fatigueFactor = m_pExerciseTracker.CalculateFatigueFactor();
        }
        
        float metabolicEfficiencyFactor = StaminaConsumptionCalculator.CalculateMetabolicEfficiencyFactor(speedRatio);
        float fitnessEfficiencyFactor = StaminaConsumptionCalculator.CalculateFitnessEfficiencyFactor();
        float totalEfficiencyFactor = fitnessEfficiencyFactor * metabolicEfficiencyFactor;
        
        vector velocityForDrain = velocity;
        if (useSwimmingModel && !HasSwimInput())
            velocityForDrain = vector.Zero;

        float slopeAngleDegrees = 0.0; // 初始化坡度角度
        GradeCalculationResult gradeResult = SpeedCalculator.CalculateGradePercent(
            this,
            currentSpeed,
            m_pJumpVaultDetector,
            slopeAngleDegrees,
            m_pEnvironmentFactor,
            velocityForDrain);
        float gradePercent = gradeResult.gradePercent;
        slopeAngleDegrees = gradeResult.slopeAngleDegrees;
        
        // v5: 代谢速度限制器更新（在 gradePercent 定义之后）
        // Phase 2.3: 使用完整的 PandolfCalculator_V5 替换简化版公式
        if (isPlayer && m_pMetabolicLimiter)
        {
            // 使用完整的 Pandolf 代谢功率计算器
            float bodyWeight = RealisticStaminaSpeedSystem.CHARACTER_WEIGHT;  // 90kg
            float loadWeight = currentWeight;  // 装备重量
            
            float pandolfWatts = SCR_PandolfCalculator_V5.CalculateMetabolicPower(
                currentSpeed,
                bodyWeight,
                loadWeight,
                gradePercent,
                terrainFactor,
                currentTime);  // 启用缓存
            
            m_pMetabolicLimiter.TickUpdate(timeDeltaSec, pandolfWatts);
            
            float limiterSpeedRatio = m_pMetabolicLimiter.GetCurrentSpeedRatio();
            finalSpeedMultiplier = finalSpeedMultiplier * limiterSpeedRatio;
        }

        if (StaminaConfigBridge.IsMudSlipMechanismEnabled())
        {
            if (m_pMudSlipRunner)
            {
                m_pMudSlipRunner.ProcessAfterSlope(
                    this,
                    useSwimmingModel,
                    isSwimming,
                    m_pEnvironmentFactor,
                    m_pStaminaComponent,
                    currentSpeed,
                    isSprintActive,
                    currentWeight,
                    staminaPercent,
                    velocity,
                    slopeAngleDegrees,
                    timeDeltaSec,
                    currentTime,
                    IsRssDebugEnabled());
            }

        }
        else
        {
            RSS_SetMudSlipCameraShake01(0.0);
        }
        // ── AI 子系统管理器（行为层节流 + 状态机 + 模块链） ──
        if (!isPlayer && Replication.IsServer() && m_pAIManager)
        {
            float fatigueVal;
            if (m_pFatigueSystem)
                fatigueVal = m_pFatigueSystem.GetFatigueAccumulation();
            else
                fatigueVal = 0.0;

            m_pAIManager.Tick(
                owner, currentTime, timeDeltaSec,
                staminaPercent, fatigueVal, currentSpeed, isPlayer);
        }

        bool isSprinting = isSprintingNow;
        int currentMovementPhase = phaseNow;
        float baseDrainRateByVelocity = 0.0;
        float baseDrainRateByVelocityForModule = 0.0;

        {
        // v5修正：传递无氧能量参数以支持v_drain机制
        float anaerobicEnergy = 1.0;
        if (isPlayer && m_pAnaerobicState)
        {
            anaerobicEnergy = m_pAnaerobicState.GetAnaerobicEnergy();
        }
        
        // Phase 2.2：计算动态理论速度（基于负重、坡度、地形）
        float theoreticalWalkSpeed = 1.4;
        float theoreticalRunSpeed = 3.0;
        float theoreticalSprintSpeed = 4.2;
        if (isPlayer)
        {
            SCR_RSS_Settings activeSettings = SCR_RSS_ConfigManager.GetSettings();
            string preset = SCR_PlayerBaseConfigHelper.MapSelectedPresetToV5PresetName(activeSettings);
            
            // 使用 SpeedRecalibration_V5 计算动态理论速度
            theoreticalWalkSpeed = SCR_SpeedRecalibration_V5.CalculateFinalWalkSpeed(
                preset, currentWeight, gradePercent, terrainFactor);
            theoreticalRunSpeed = SCR_SpeedRecalibration_V5.CalculateFinalRunSpeed(
                preset, currentWeight, gradePercent, terrainFactor);
            theoreticalSprintSpeed = SCR_SpeedRecalibration_V5.CalculateFinalSprintSpeed(
                preset, currentWeight, gradePercent, terrainFactor);
        }
        
        TheoreticalSpeedParams theoreticalSpeeds = new TheoreticalSpeedParams();
        theoreticalSpeeds.walkSpeed = theoreticalWalkSpeed;
        theoreticalSpeeds.runSpeed = theoreticalRunSpeed;
        theoreticalSpeeds.sprintSpeed = theoreticalSprintSpeed;
        
        BaseDrainRateResult drainRateResult = StaminaUpdateCoordinator.CalculateBaseDrainRate(
            useSwimmingModel,
            currentSpeed,
            encumbranceSpeedPenalty,
            totalWeight,
            totalWeightWithWetAndBody,
            gradePercent,
            terrainFactor,
            velocityForDrain,
            m_bSwimmingVelocityDebugPrinted,
            owner,
            m_pEnvironmentFactor, // v2.14.0修复：传递环境因子
            isSprinting,
            currentMovementPhase,
            anaerobicEnergy,  // v5新增：无氧能量
            0.2,  // v5新增：无氧阈值
            theoreticalSpeeds);  // Phase 2.2：动态速度结构体
        baseDrainRateByVelocity = drainRateResult.baseDrainRate;
        m_bSwimmingVelocityDebugPrinted = drainRateResult.swimmingVelocityDebugPrinted;
        
        float postureMultiplier = 1.0;
        float gradePercentForConsumption = 0.0;
        float terrainFactorForConsumption = 1.0;
        
        if (!useSwimmingModel)
        {
            postureMultiplier = StaminaConsumptionCalculator.CalculatePostureMultiplier(currentSpeed, this);
            gradePercentForConsumption = gradePercent;
            terrainFactorForConsumption = terrainFactor;
        }
        
        float encumbranceStaminaDrainMultiplier = 1.0;
        if (m_pEncumbranceCache)
            encumbranceStaminaDrainMultiplier = m_pEncumbranceCache.GetStaminaDrainMultiplier();
        
        if (useSwimmingModel)
            encumbranceStaminaDrainMultiplier = 1.0;
        
        const float sprintMultiplier = 1.0;
        
        baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
        if (useSwimmingModel)
        {
            totalDrainRate = baseDrainRateByVelocity;
            totalDrainRate = totalDrainRate * totalEfficiencyFactor * fatigueFactor;
        }
        else
        {
            totalDrainRate = StaminaConsumptionCalculator.CalculateStaminaConsumption(
                currentSpeed,
                currentWeight,
                gradePercentForConsumption,
                terrainFactorForConsumption,
                postureMultiplier,
                totalEfficiencyFactor,
                fatigueFactor,
                sprintMultiplier,
                encumbranceStaminaDrainMultiplier,
                m_pFatigueSystem,
                baseDrainRateByVelocityForModule,
                m_pEnvironmentFactor, // v2.14.0：传递环境因子模块
                owner, // v2.15.0：传递角色实体，用于手持物品检测
                isSprinting,
                currentMovementPhase);
            
            float drainRateBeforeHeat = totalDrainRate;
            totalDrainRate = totalDrainRate * heatStressMultiplier;
            
        }

        if (isPlayer && RSS_IsCaffeineSodiumBenzoateActive())
            totalDrainRate = totalDrainRate * SCR_CombatStimConstants.STAMINA_DRAIN_MULTIPLIER;
        
        // 低体力恢复区域：体力<阈值时步行/慢跑转为恢复
        float walkRecoveryThreshold = StaminaConstants.WALK_RECOVERY_ZONE_THRESHOLD;
        float walkRecoveryRatePerTick = StaminaConstants.WALK_RECOVERY_ZONE_RATE();
        if (!useSwimmingModel && !isSprintActive && staminaPercent < walkRecoveryThreshold && currentSpeed > 0.1)
        {
            totalDrainRate = -walkRecoveryRatePerTick;  // 负数 = 恢复
            baseDrainRateByVelocity = -walkRecoveryRatePerTick;
            baseDrainRateByVelocityForModule = -walkRecoveryRatePerTick;
        }
        
        if (baseDrainRateByVelocityForModule == 0.0 && baseDrainRateByVelocity > 0.0)
            baseDrainRateByVelocityForModule = baseDrainRateByVelocity;
        
        if (m_pEpocState && !useSwimmingModel)
        {
            float currentWorldTime = world.GetWorldTime() / 1000.0; // 转换为秒
            bool isInEpocDelay = StaminaRecoveryCalculator.UpdateEpocDelay(
                m_pEpocState,
                currentSpeed,
                currentWorldTime);
        }
        
        if (owner == SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled() && IsPlayerControlled())
            SCR_DebugBatchManager.StartDebugBatch();
        
        if (m_pStaminaComponent)
        {
            float newTargetStamina = StaminaUpdateCoordinator.UpdateStaminaValue(
                m_pStaminaComponent,
                staminaPercent,
                useSwimmingModel,
                currentSpeed,
                totalDrainRate,
                baseDrainRateByVelocity,
                baseDrainRateByVelocityForModule,
                heatStressMultiplier,
                m_pEpocState,
                m_pEncumbranceCache,
                m_pExerciseTracker,
                m_pFatigueSystem,
                this,
                m_pEnvironmentFactor,
                timeDeltaSec);
            
            m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            m_fLastStaminaUpdateTime = currentTime;
            
            float verifyStamina = m_pStaminaComponent.GetStamina();
            if (Math.AbsFloat(verifyStamina - newTargetStamina) > 0.005) // 偏差超过0.5%
            {
                if (SCR_DebugBatchManager.IsDebugBatchActive())
                {
                    string intLine = string.Format("[RSS] 原生干扰: 目标=%1%% 实际=%2%% 偏差=%3%%",
                        Math.Round(newTargetStamina * 100.0).ToString(),
                        Math.Round(verifyStamina * 100.0).ToString(),
                        Math.Round(Math.AbsFloat(verifyStamina - newTargetStamina) * 10000.0) / 100.0);
                    SCR_DebugBatchManager.AddDebugBatchLine(intLine);
                }
                m_pStaminaComponent.SetTargetStamina(newTargetStamina);
            }
            
            staminaPercent = newTargetStamina;
        }

        }
        
        if (isPlayer && m_pUISignalBridge)
        {
            m_pUISignalBridge.UpdateUISignal(staminaPercent, isExhausted, currentSpeed, totalDrainRate, false);
        }
        
        // v5: 更新HUD显示
        if (isPlayer && m_pAnaerobicState && m_pMetabolicLimiter)
        {
            SCR_StaminaHUDComponent hud = SCR_StaminaHUDComponent.GetInstance();
            if (hud)
            {
                hud.UpdateHUD(currentTime, m_pAnaerobicState, m_pMetabolicLimiter);
            }
        }
        
        m_fLastStaminaPercent = staminaPercent;
        m_fLastSpeedMultiplier = finalSpeedMultiplier;

        // AI 调试日志（使用 Print 直接输出，不受 DebugBatchManager batch 窗口限制）
        if (owner != SCR_PlayerController.GetLocalControlledEntity() && IsRssDebugEnabled())
        {
            // 每 AI 每 30s 输出一行，避免刷屏
            float nowMs = GetGame().GetWorld().GetWorldTime();
            float aiDebugLastPrint = -1.0;
            if (m_pAIManager)
                aiDebugLastPrint = m_pAIManager.GetDebugLastPrintTime();
            if (aiDebugLastPrint < 0.0 || (nowMs - aiDebugLastPrint) >= 30000.0)
            {
                if (m_pAIManager)
                    m_pAIManager.SetDebugLastPrintTime(nowMs);
                string movementStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
                ERSS_AIStaminaState aiState = ERSS_AIStaminaState.FRESH;
                if (m_pAIManager)
                    aiState = m_pAIManager.GetStaminaState();
                string stateStr = SCR_RSS_AIStaminaState.StateToString(aiState);
                float fatigueVal = 0.0;
                if (m_pFatigueSystem)
                    fatigueVal = m_pFatigueSystem.GetFatigueAccumulation();

                SCR_PlayerBaseDebugHelper.LogAiStatus(
                    owner,
                    stateStr,
                    staminaPercent,
                    fatigueVal,
                    currentWeight,
                    finalSpeedMultiplier,
                    currentSpeed,
                    movementStr,
                    m_sLastSpeedSource);

                // 群组分散
                if (Replication.IsServer())
                {
                    AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
                    if (aiCtrl)
                    {
                        AIAgent agent = aiCtrl.GetAIAgent();
                        if (agent)
                        {
                            AIGroup parentGroup = agent.GetParentGroup();
                            if (parentGroup)
                            {
                                SCR_AIGroup scrGrp = SCR_AIGroup.Cast(parentGroup);
                                if (scrGrp)
                                {
                                    float spread = CalcAiGroupSpreadM(scrGrp);
                                    if (spread > 0.0)
                                    {
                                        SCR_PlayerBaseDebugHelper.LogAiGroupSpread(
                                            scrGrp,
                                            spread,
                                            GetAliveMemberCount(scrGrp));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
            bool needDebugOutput = SCR_DebugBatchManager.IsDebugBatchActive();
            bool needHintOutput = (settings && settings.m_bHintDisplayEnabled);
            if (needDebugOutput || needHintOutput)
            {
                float debugCurrentWeight = 0.0;
                float combatEncumbrancePercent = 0.0;
                if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid()) {
                    debugCurrentWeight = m_pEncumbranceCache.GetCurrentWeight();
                    combatEncumbrancePercent = RealisticStaminaSpeedSystem.CalculateCombatEncumbrancePercent(owner);
                }
                float timeToDepleteSec = -1.0;
                float timeToFullSec = -1.0;
                if (needHintOutput) {
                    float netRate = StaminaUpdateCoordinator.GetNetStaminaRatePerSecond(
                        staminaPercent, useSwimmingModel, currentSpeed, totalDrainRate,
                        baseDrainRateByVelocity, baseDrainRateByVelocityForModule,
                        heatStressMultiplier, m_pEpocState, m_pEncumbranceCache,
                        m_pExerciseTracker, this, m_pEnvironmentFactor);
                    float targetStamina = 1.0;
                    if (m_pFatigueSystem) targetStamina = m_pFatigueSystem.GetMaxStaminaCap();
                    if (netRate < -0.0001) {
                        // 消耗 ETA 保留线性外推：Pandolf 消耗随速度近线性变化，
                        // 恢复分量在消耗场景中占比小，线性近似足够
                        timeToDepleteSec = Math.Min(staminaPercent / Math.AbsFloat(netRate), 7200.0);
                    } else if (netRate > 0.0001 && staminaPercent < targetStamina) {
                        // 恢复 ETA 改用分段积分：恢复曲线高度非线性
                        // （非线性系数、边际效应衰减、快速/中等/慢速三期），
                        // 简单线性外推在 >80% 区域误差可达 200%+
                        timeToFullSec = StaminaUpdateCoordinator.EstimateRecoveryTimeToFull(
                            staminaPercent, targetStamina, totalDrainRate,
                            baseDrainRateByVelocity, baseDrainRateByVelocityForModule,
                            heatStressMultiplier, m_pEpocState, m_pEncumbranceCache,
                            m_pExerciseTracker, this, m_pEnvironmentFactor);
                    }
                }
                DebugInfoParams debugParams = new DebugInfoParams();
                debugParams.owner = owner;
                debugParams.movementTypeStr = DebugDisplay.FormatMovementType(isSprinting, currentMovementPhase);
                debugParams.staminaPercent = staminaPercent;
                debugParams.baseSpeedMultiplier = baseSpeedMultiplier;
                debugParams.encumbranceSpeedPenalty = encumbranceSpeedPenalty;
                debugParams.finalSpeedMultiplier = finalSpeedMultiplier;
                debugParams.gradePercent = gradePercent;
                if (isSwimming)
                    debugParams.slopeAngleDegrees = 0.0;
                else
                    debugParams.slopeAngleDegrees = slopeAngleDegrees;
                debugParams.isSprinting = isSprinting;
                debugParams.currentMovementPhase = currentMovementPhase;
                debugParams.debugCurrentWeight = debugCurrentWeight;
                debugParams.combatEncumbrancePercent = combatEncumbrancePercent;
                debugParams.terrainDetector = m_pTerrainDetector;
                debugParams.environmentFactor = m_pEnvironmentFactor;
                debugParams.heatStressMultiplier = heatStressMultiplier;
                debugParams.rainWeight = rainWeight;
                debugParams.swimmingWetWeight = m_fCurrentWetWeight;
                debugParams.currentSpeed = currentSpeed;
                debugParams.isSwimming = isSwimming;
                debugParams.stanceTransitionManager = m_pStanceTransitionManager;
                debugParams.timeToDepleteSec = timeToDepleteSec;
                debugParams.timeToFullSec = timeToFullSec;
                debugParams.speedSource = m_sLastSpeedSource;
                if (needDebugOutput) {
                    DebugDisplay.OutputDebugInfo(debugParams);
                    // [v3.23.0] 调试行已直接写入 BatchManager，无需 Flush
                    // SCR_RSS_AIStaminaBridge.FlushAIDebugLinesToBatch();
                }
                if (needHintOutput) DebugDisplay.OutputHintInfo(debugParams);
            }
        }
        
        SCR_DebugBatchManager.FlushDebugBatch();
        
        m_bRssStaminaLoopActive = true;
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, GetSpeedUpdateIntervalMs(), false);
    }
}
