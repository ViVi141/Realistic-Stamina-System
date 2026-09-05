//! AI 专用体力链路（精度优先、避开玩家 UpdateSpeed/环境全链）。
//!
//! 与玩家对齐：
//!   - Pandolf/ACSM MetabolismPowerWatts + StaminaDrainRatePerSecondFromPowerWatts
//!   - CalculateTotalDrainRate / UpdateStaminaValue / W′ TickPower / 疲劳积分
//!   - 限速：廉价骨架 + Tobler 坡度 +（开消耗时）CP 巡航/Sprint 反解
//!   - 6.2.34：先 TickPower 闩巡航，再 ApplyCheap；巡航限速 instant SetSpeedLimit
//!
//! 相对玩家的有意简化：
//!   - 坡度：引擎复用 / Y 差分（无 Trace）；地形稀采样
//!   - 热应激：全服共享 1Hz 近似，无室内/湿重/游泳动作消耗
//!   - 不做完整 UpdateSpeed / 泥泞 / 跳跃翻越
//!
//! 入口：DisableAIStaminaCalc=false 时由 PlayerBase_UpdateLoop 调用 Tick，不再进玩家 Phase B/C。

class RSS_AIStaminaPipelineContext
{
    SCR_CharacterControllerComponent ctrl;
    IEntity owner;
    World world;
    SCR_CharacterStaminaComponent staminaComponent;
    SCR_RSS_StaminaState staminaState;
    SCR_RSS_EncumbranceCache encumbranceCache;
    SCR_RSS_AnaerobicBurst anaerobicBurst;
    SCR_RSS_FatigueSystem fatigueSystem;
    SCR_RSS_EpocState epocState;
    SCR_RSS_ExerciseTracker exerciseTracker;
    SCR_RSS_TerrainDetector terrainDetector;
    SCR_RSS_AIManager aiManager;
    float animSpeedCompensation;
    float lastStaminaUpdateTime;
    vector lastPositionSample;
    bool hasLastPositionSample;
    vector computedVelocity;
    float appliedSpeedLimitMs;
    float lastRssSpeedMultiplierApplied;
}

class SCR_RSS_AIStaminaPipeline
{
    protected float m_fCachedGradePercent;
    protected float m_fCachedTerrainFactor;
    protected float m_fLastTerrainSampleSec;
    protected vector m_vLastGradePos;
    protected bool m_bHasGradePos;

    void SCR_RSS_AIStaminaPipeline()
    {
        m_fCachedGradePercent = 0.0;
        m_fCachedTerrainFactor = 1.0;
        m_fLastTerrainSampleSec = -1000.0;
        m_vLastGradePos = vector.Zero;
        m_bHasGradePos = false;
    }

    //! @return 更新后的 lastStaminaUpdateTime（写入 ctx 侧字段由调用方回写）
    float Tick(RSS_AIStaminaPipelineContext ctx)
    {
        if (!ctx || !ctx.ctrl || !ctx.owner || !ctx.world)
            return ctx.lastStaminaUpdateTime;

        float nowMs = ctx.world.GetWorldTime();
        float nowSec = nowMs / 1000.0;
        float timeDeltaSec = SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_INTERVAL_MS / 1000.0;
        if (ctx.lastStaminaUpdateTime >= 0.0)
            timeDeltaSec = nowSec - ctx.lastStaminaUpdateTime;
        timeDeltaSec = Math.Clamp(timeDeltaSec, 0.05, 1.0);

        float distM = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(ctx.owner);
        bool farLod = false;
        if (distM >= 0.0 && distM > SCR_RSS_AIConstants.RSS_PERF_AI_LOD_FAR_M)
            farLod = true;

        // 测速 → 坡度/地形（先于限速，使 Tobler/CP 帽与消耗同源）
        float dtSample = timeDeltaSec;
        RSS_SpeedCalculationResult pos = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
            ctx.owner,
            ctx.lastPositionSample,
            ctx.hasLastPositionSample,
            ctx.computedVelocity,
            dtSample);
        ctx.lastPositionSample = pos.lastPositionSample;
        ctx.hasLastPositionSample = pos.hasLastPositionSample;
        ctx.computedVelocity = pos.computedVelocity;
        vector velocity = SCR_PlayerBaseRssApiHelper.SampleEntityVelocity(
            ctx.owner, pos.computedVelocity);
        float currentSpeed = SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(velocity);

        UpdateGrade(ctx.ctrl, ctx.owner.GetOrigin(), velocity, currentSpeed, timeDeltaSec);
        UpdateTerrainFactor(ctx, nowSec, currentSpeed, distM, farLod);

        float heatMult = 1.0;
        if (!farLod)
            heatMult = SCR_RSS_AISharedEnvCache.GetHeatStressMultiplier(nowSec);

        float gearKg = 0.0;
        if (ctx.encumbranceCache && ctx.encumbranceCache.IsCacheValid())
        {
            ctx.encumbranceCache.CheckAndUpdate();
            gearKg = ctx.encumbranceCache.GetCurrentWeight();
        }
        float totalWeight = gearKg + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        float totalWithWet = totalWeight;

        // 限速前先更新 CP 上下文 + 烧 W′，本 tick 即可闩上有氧巡航（原先先限速后 TickPower，
        // 玩家已巡航时 AI 仍按满 Run 顶跑一整档 LOD）
        int phaseForDrain = ctx.ctrl.GetCurrentMovementPhase();
        if (phaseForDrain < 1)
            phaseForDrain = 2;
        if (ctx.ctrl.IsSprinting())
        {
            if (SCR_RSS_ConfigBridge.IsAIStaminaCombatEffectsEnabled())
                phaseForDrain = 3;
            else
                phaseForDrain = 2;
        }

        float prevLimitMs = ctx.appliedSpeedLimitMs;
        float speedRatio = Math.Clamp(
            currentSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED, 0.0, 1.0);

        float staminaPercent = ctx.ctrl.GetRssAerobicPercent();
        staminaPercent = Math.Clamp(staminaPercent, 0.0, 1.0);

        float cheapEnc = 0.0;
        if (ctx.encumbranceCache)
        {
            cheapEnc = ctx.encumbranceCache.GetSpeedPenaltyFraction();
            cheapEnc = cheapEnc * SCR_RSS_ConfigBridge.GetCustomEncumbranceSpeedPenaltyMultiplier();
            float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
            cheapEnc = Math.Clamp(cheapEnc, 0.0, maxPenalty);
        }

        RSS_StaminaDrainTickParams drainParams = new RSS_StaminaDrainTickParams();
        drainParams.useSwimmingModel = false;
        drainParams.currentSpeed = currentSpeed;
        drainParams.gearWeightKg = gearKg;
        drainParams.encumbranceSpeedPenalty = cheapEnc;
        drainParams.bodyPlusGearWeightKg = totalWeight;
        drainParams.totalWeightWithWetAndBody = totalWithWet;
        drainParams.gradePercent = m_fCachedGradePercent;
        drainParams.terrainFactor = m_fCachedTerrainFactor;
        drainParams.velocityForDrain = velocity;
        drainParams.swimmingVelocityDebugPrinted = false;
        drainParams.owner = ctx.owner;
        drainParams.controller = ctx.ctrl;
        drainParams.environmentFactor = null;
        drainParams.isSprinting = false;
        if (phaseForDrain == 3)
            drainParams.isSprinting = true;
        drainParams.currentMovementPhase = phaseForDrain;
        drainParams.speedRatio = speedRatio;
        drainParams.heatStressMultiplier = heatMult;
        drainParams.isSprintActive = drainParams.isSprinting;
        drainParams.staminaPercent = staminaPercent;
        drainParams.combatStimActive = false;
        drainParams.encumbranceCache = ctx.encumbranceCache;
        drainParams.fatigueSystem = ctx.fatigueSystem;
        drainParams.exerciseTracker = ctx.exerciseTracker;
        drainParams.epocState = ctx.epocState;
        drainParams.currentTimeSec = nowSec;
        drainParams.currentTimeForExerciseMs = nowMs;
        drainParams.appliedSpeedLimitMs = prevLimitMs;
        drainParams.effectiveCriticalPowerWatts = -1.0;
        drainParams.wPrimePool01 = 1.0;

        SCR_RSS_CriticalPowerModel cpModel = null;
        if (ctx.anaerobicBurst)
            cpModel = ctx.anaerobicBurst.GetCpModel();
        if (cpModel)
        {
            float loadKg = Math.Max(gearKg, 0.0);
            float envCpMult = 1.0;
            if (heatMult > 1.0)
                envCpMult = 1.0 / heatMult;
            float fatigueNorm = 0.0;
            if (ctx.fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
                fatigueNorm = ctx.fatigueSystem.GetFatigueIntegralNorm();
            cpModel.SetRuntimeContext(loadKg, m_fCachedGradePercent, envCpMult, fatigueNorm);
            drainParams.effectiveCriticalPowerWatts = cpModel.GetEffectiveCriticalPowerWatts();
            drainParams.wPrimePool01 = cpModel.GetPool01();
        }
        else
        {
            float cpFallback = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
            if (cpFallback > 1.0)
                drainParams.effectiveCriticalPowerWatts = cpFallback;
        }

        // W′ 始终算（远距也算）：否则玩家已闩巡航时近旁 AI 可能从未见底
        if (ctx.anaerobicBurst)
        {
            float pool01Before = 1.0;
            if (cpModel)
                pool01Before = cpModel.GetPool01();
            float powerW = SCR_RSS_DrainCalculator.GetMetabolicAccountingPowerWatts(
                currentSpeed,
                prevLimitMs,
                totalWithWet,
                m_fCachedGradePercent,
                m_fCachedTerrainFactor,
                phaseForDrain,
                pool01Before,
                drainParams.isSprintActive);
            ctx.anaerobicBurst.TickPower(
                powerW,
                drainParams.isSprintActive,
                nowSec,
                timeDeltaSec,
                currentSpeed);
            if (cpModel)
                drainParams.wPrimePool01 = cpModel.GetPool01();
        }

        float cheapFrac = 1.0;
        float cheapSta = staminaPercent;
        int cheapPhase = phaseForDrain;
        bool cheapExhausted = false;
        float cheapTargetMs = 0.0;
        SCR_PlayerBaseAiLightTickHelper.ApplyCheapAiSpeedEx(
            ctx.ctrl,
            ctx.owner,
            ctx.encumbranceCache,
            ctx.animSpeedCompensation,
            m_fCachedGradePercent,
            m_fCachedTerrainFactor,
            true,
            cheapFrac,
            cheapEnc,
            cheapSta,
            cheapPhase,
            cheapExhausted,
            cheapTargetMs);

        ctx.lastRssSpeedMultiplierApplied = cheapFrac;
        ctx.appliedSpeedLimitMs = cheapTargetMs;
        if (ctx.appliedSpeedLimitMs < 0.05)
            ctx.appliedSpeedLimitMs = -1.0;

        // AI 限速诊断（DebugLog 开即可；近距 2s）
        float wPrime01Log = 1.0;
        bool cruiseLatchLog = false;
        if (cpModel)
        {
            wPrime01Log = cpModel.GetPool01();
            cruiseLatchLog = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(cpModel);
        }
        RSS_AiSpeedDiagSnap diag = new RSS_AiSpeedDiagSnap();
        diag.pathTag = "pipeline";
        diag.currentSpeedMs = currentSpeed;
        diag.targetMs = cheapTargetMs;
        diag.appliedFrac = cheapFrac;
        diag.outPhase = cheapPhase;
        diag.encPenalty = cheapEnc;
        diag.gradePercent = m_fCachedGradePercent;
        diag.wPrime01 = wPrime01Log;
        diag.cruiseLatched = cruiseLatchLog;
        diag.distM = distM;
        SCR_RSS_UpdateLoopDebugOutput.LogAiSpeedDiag(
            ctx.owner, ctx.ctrl, ctx.aiManager, diag);

        drainParams.appliedSpeedLimitMs = ctx.appliedSpeedLimitMs;
        drainParams.staminaPercent = cheapSta;
        drainParams.currentMovementPhase = cheapPhase;
        drainParams.isSprinting = false;
        if (cheapPhase == 3)
            drainParams.isSprinting = true;
        drainParams.isSprintActive = drainParams.isSprinting;
        drainParams.encumbranceSpeedPenalty = cheapEnc;

        RSS_StaminaDrainTickResult drainTick = SCR_RSS_UpdateCoordinator.CalculateTotalDrainRate(
            drainParams);
        float totalDrainRate = drainTick.totalDrainRate;
        float baseDrain = drainTick.baseDrainRateByVelocity;
        float baseDrainMod = drainTick.baseDrainRateByVelocityForModule;

        // 疲劳积分：近距保留（Decay 已在 CalculateTotalDrainRate 内）
        if (!farLod && ctx.fatigueSystem && SCR_RSS_ConfigBridge.IsFatigueSystemEnabled())
        {
            if (currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
            {
                float fatigueCp = drainParams.effectiveCriticalPowerWatts;
                float powerFat = SCR_RSS_DrainCalculator.GetMetabolicFatiguePowerWatts(
                    currentSpeed,
                    ctx.appliedSpeedLimitMs,
                    totalWithWet,
                    m_fCachedGradePercent,
                    m_fCachedTerrainFactor,
                    cheapPhase);
                ctx.fatigueSystem.ProcessFatigueIntegral(
                    powerFat,
                    gearKg,
                    m_fCachedGradePercent,
                    m_fCachedTerrainFactor,
                    timeDeltaSec,
                    currentSpeed,
                    fatigueCp);
            }
        }

        float newSta = SCR_RSS_UpdateCoordinator.UpdateStaminaValue(
            ctx.staminaComponent,
            cheapSta,
            false,
            currentSpeed,
            totalDrainRate,
            baseDrain,
            baseDrainMod,
            heatMult,
            ctx.epocState,
            ctx.encumbranceCache,
            ctx.exerciseTracker,
            ctx.fatigueSystem,
            ctx.ctrl,
            null,
            timeDeltaSec);

        newSta = Math.Clamp(newSta, 0.0, 1.0);
        if (ctx.staminaState)
            ctx.staminaState.SetAerobic(newSta);
        if (ctx.staminaComponent)
            ctx.staminaComponent.SetTargetStamina(newSta);

        // 战斗行为层
        if (ctx.aiManager && Replication.IsServer())
        {
            float fatigueVal = 0.0;
            if (ctx.fatigueSystem)
                fatigueVal = ctx.fatigueSystem.GetFatigueAccumulation();
            ctx.aiManager.Tick(
                ctx.owner,
                ctx.ctrl,
                nowSec,
                timeDeltaSec,
                newSta,
                fatigueVal,
                currentSpeed,
                false,
                distM);
        }

        return nowSec;
    }

    protected void UpdateGrade(
        SCR_CharacterControllerComponent ctrl,
        vector origin,
        vector velocity,
        float currentSpeed,
        float timeDeltaSec)
    {
        // 1) 引擎 CommandMove 坡度（度）— 无 Trace
        float cmdSlopeDeg;
        if (ctrl && SCR_RSS_EngineReuse.TryGetCommandMoveSlopeDegrees(ctrl, cmdSlopeDeg))
        {
            float slopeRatio = Math.Tan(cmdSlopeDeg * Math.DEG2RAD);
            slopeRatio = Math.Clamp(slopeRatio, -1.0, 1.0);
            float gradeCmd = slopeRatio * 100.0;
            if (gradeCmd > 35.0)
                gradeCmd = 35.0;
            if (gradeCmd < -25.0)
                gradeCmd = -25.0;
            m_fCachedGradePercent = m_fCachedGradePercent * 0.7 + gradeCmd * 0.3;
            m_vLastGradePos = origin;
            m_bHasGradePos = true;
            return;
        }

        // 2) 仅复用引擎脚下法线；失败用 Y 差分。AI 路径禁止 Trace。
        if (ctrl && currentSpeed > 0.05)
        {
            vector normal;
            if (SCR_RSS_SpeedCalculator.TryGetCharacterFloorNormal(ctrl, normal))
            {
                float magnitude = SCR_RSS_SpeedCalculator.SlopeMagnitudeDegreesFromNormal(normal);
                float cosAngle = SCR_RSS_SpeedCalculator.GetSlopeProjectionCos(normal, velocity);
                float angleDeg = magnitude * cosAngle;
                angleDeg = Math.Clamp(angleDeg, -45.0, 45.0);
                float slopeRatio2 = Math.Tan(angleDeg * Math.DEG2RAD);
                slopeRatio2 = Math.Clamp(slopeRatio2, -1.0, 1.0);
                float gradeFromFloor = slopeRatio2 * 100.0;
                if (gradeFromFloor > 35.0)
                    gradeFromFloor = 35.0;
                if (gradeFromFloor < -25.0)
                    gradeFromFloor = -25.0;
                m_fCachedGradePercent = m_fCachedGradePercent * 0.7 + gradeFromFloor * 0.3;
                m_vLastGradePos = origin;
                m_bHasGradePos = true;
                return;
            }
        }

        UpdateGradeFromPositionFallback(origin, timeDeltaSec);
    }

    //! FloorNormal 不可用时：位置 Y 差分估 grade%
    protected void UpdateGradeFromPositionFallback(vector origin, float timeDeltaSec)
    {
        if (!m_bHasGradePos)
        {
            m_vLastGradePos = origin;
            m_bHasGradePos = true;
            m_fCachedGradePercent = 0.0;
            return;
        }

        float dx = origin[0] - m_vLastGradePos[0];
        float dy = origin[1] - m_vLastGradePos[1];
        float dz = origin[2] - m_vLastGradePos[2];
        float horiz = Math.Sqrt(dx * dx + dz * dz);
        m_vLastGradePos = origin;

        if (horiz < 0.08 || timeDeltaSec < 0.05)
            return;

        float grade = (dy / horiz) * 100.0;
        if (grade > 35.0)
            grade = 35.0;
        if (grade < -25.0)
            grade = -25.0;

        m_fCachedGradePercent = m_fCachedGradePercent * 0.7 + grade * 0.3;
    }

    protected void UpdateTerrainFactor(
        RSS_AIStaminaPipelineContext ctx,
        float nowSec,
        float currentSpeed,
        float distM,
        bool farLod)
    {
        if (farLod)
        {
            m_fCachedTerrainFactor = 1.0;
            return;
        }

        float interval = SCR_RSS_AIConstants.RSS_AI_TERRAIN_SAMPLE_MID_SEC;
        if (distM < 0.0 || distM <= SCR_RSS_AIConstants.RSS_PERF_AI_LOD_NEAR_M)
            interval = SCR_RSS_AIConstants.RSS_AI_TERRAIN_SAMPLE_NEAR_SEC;

        if ((nowSec - m_fLastTerrainSampleSec) < interval)
            return;

        m_fLastTerrainSampleSec = nowSec;

        // AI：优先脚下材质；失败再用 TerrainDetector（其内部亦优先 FloorSurface）
        GameMaterial floorMat;
        if (ctx.owner && SCR_RSS_EngineReuse.TryGetFloorGameMaterial(ctx.owner, floorMat))
        {
            float density = SCR_RSS_EngineReuse.ResolveDensityFromMaterial(floorMat);
            if (density >= 0.0)
            {
                m_fCachedTerrainFactor = SCR_RSS_MetabolismMath.GetTerrainFactorFromDensity(density);
                return;
            }
        }

        if (!ctx.terrainDetector)
        {
            m_fCachedTerrainFactor = 1.0;
            return;
        }
        m_fCachedTerrainFactor = ctx.terrainDetector.GetTerrainFactor(
            ctx.owner, nowSec, currentSpeed);
    }

    void OnEntityDeleted()
    {
        m_bHasGradePos = false;
        m_fCachedGradePercent = 0.0;
        m_fCachedTerrainFactor = 1.0;
    }
}
