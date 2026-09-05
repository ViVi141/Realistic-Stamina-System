//! AI 专用体力链路（精度优先、避开玩家 UpdateSpeed/环境全链）。
//!
//! 与玩家对齐：
//!   - Pandolf/ACSM MetabolismPowerWatts + StaminaDrainRatePerSecondFromPowerWatts
//!   - CalculateTotalDrainRate / UpdateStaminaValue / W′ TickPower / 疲劳积分
//!   - 廉价限速 = V6 相位 × 负重（同玩家指令速骨架）
//!
//! 相对玩家的有意简化（敏感度上损失可控）：
//!   - 坡度：位置 Y 差分估 grade%（无射线）；地形射线按距离 LOD 稀采样
//!   - 热应激：全服共享 1Hz 近似，无室内/湿重/游泳动作消耗
//!   - 不做 UpdateSpeed / CP 二次限速 / 泥泞 / 跳跃翻越
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

        float cheapFrac = 1.0;
        float cheapEnc = 0.0;
        float cheapSta = 1.0;
        int cheapPhase = 2;
        bool cheapExhausted = false;
        SCR_PlayerBaseAiLightTickHelper.ApplyCheapAiSpeed(
            ctx.ctrl,
            ctx.owner,
            ctx.encumbranceCache,
            ctx.animSpeedCompensation,
            cheapFrac,
            cheapEnc,
            cheapSta,
            cheapPhase,
            cheapExhausted);

        ctx.lastRssSpeedMultiplierApplied = cheapFrac;
        ctx.appliedSpeedLimitMs = -1.0;

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

        // 测速（位置差分，与玩家同源）
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

        // 坡度：Y 差分（敏感度上坡度对油耗最大；无射线）
        UpdateGradeFromPosition(ctx.owner.GetOrigin(), currentSpeed, timeDeltaSec);

        // 地形：近/中稀采样射线；远距固定 1.0
        UpdateTerrainFactor(ctx, nowSec, currentSpeed, distM, farLod);

        float heatMult = 1.0;
        if (!farLod)
            heatMult = SCR_RSS_AISharedEnvCache.GetHeatStressMultiplier(nowSec);

        float gearKg = 0.0;
        if (ctx.encumbranceCache && ctx.encumbranceCache.IsCacheValid())
            gearKg = ctx.encumbranceCache.GetCurrentWeight();
        float totalWeight = gearKg + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        float totalWithWet = totalWeight;

        float staminaPercent = cheapSta;
        int phase = cheapPhase;
        if (phase < 1)
            phase = 2;

        float speedRatio = Math.Clamp(
            currentSpeed / SCR_RSS_MetabolismMath.GAME_MAX_SPEED, 0.0, 1.0);

        // 与玩家同源消耗核
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
        if (phase == 3)
            drainParams.isSprinting = true;
        drainParams.currentMovementPhase = phase;
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
        drainParams.appliedSpeedLimitMs = ctx.appliedSpeedLimitMs;
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

        RSS_StaminaDrainTickResult drainTick = SCR_RSS_UpdateCoordinator.CalculateTotalDrainRate(
            drainParams);
        float totalDrainRate = drainTick.totalDrainRate;
        float baseDrain = drainTick.baseDrainRateByVelocity;
        float baseDrainMod = drainTick.baseDrainRateByVelocityForModule;
        // 伤害联动在 UpdateStaminaValue 内对 AI 统一处理，此处不重复乘

        // W′：近/中距保留；远距跳过以减负（有氧仍算）
        if (!farLod && ctx.anaerobicBurst)
        {
            float pool01Before = 1.0;
            if (cpModel)
                pool01Before = cpModel.GetPool01();
            float powerW = SCR_RSS_DrainCalculator.GetMetabolicAccountingPowerWatts(
                currentSpeed,
                ctx.appliedSpeedLimitMs,
                totalWithWet,
                m_fCachedGradePercent,
                m_fCachedTerrainFactor,
                phase,
                pool01Before,
                drainParams.isSprintActive);
            ctx.anaerobicBurst.TickPower(
                powerW,
                drainParams.isSprintActive,
                nowSec,
                timeDeltaSec,
                currentSpeed);
        }

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
                    phase);
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
            staminaPercent,
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

    protected void UpdateGradeFromPosition(vector origin, float currentSpeed, float timeDeltaSec)
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

        // 低通，避免 AI 抖动台阶
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
