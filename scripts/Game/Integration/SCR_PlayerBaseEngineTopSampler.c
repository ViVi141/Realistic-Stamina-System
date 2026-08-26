//! 引擎顶速采样器：一次解限标定 + 实时 GetMaxSpeed 污染检测。
//! 从 PlayerBase.c 拆出（引擎顶速参考子域），降低巨型文件的 Workbench ICE 风险。
class SCR_PlayerBaseEngineTopSampler
{
    //! 一次解限标定的相位顶速（不随 OverrideMaxSpeed 缩小的参考）
    //! 禁止周期性 SetSpeedLimit(1.0)：会瞬间 OverrideMaxSpeed(1) → 尖峰/滑步
    protected float m_fCachedEngineMaxWalkMs = -1.0;
    protected float m_fCachedEngineMaxRunMs = -1.0;
    protected float m_fCachedEngineMaxSprintMs = -1.0;
    protected bool m_bEngineTopUncappedCalibrated = false;

    static float GetEngineTopFallbackMs(int movementPhase)
    {
        if (movementPhase == 3)
            return SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
        if (movementPhase == 1)
            return SCR_RSS_ConfigBridge.GetMarchWalkSpeedMs();
        return SCR_RSS_MetabolismMath.GAME_MAX_SPEED * SCR_RSS_MetabolismMath.TARGET_RUN_SPEED_MULTIPLIER;
    }

    float GetCachedEngineTopMs(int movementPhase)
    {
        if (movementPhase == 3)
            return m_fCachedEngineMaxSprintMs;
        if (movementPhase == 1)
            return m_fCachedEngineMaxWalkMs;
        return m_fCachedEngineMaxRunMs;
    }

    //! 不解限读取动画相位顶速（可每 tick 调用）
    //! CharacterAnimationComponent 无 GetOwner（GameComponent 链，非 ScriptComponent）
    static void SampleLiveEngineTops(IEntity ownerEnt, CharacterAnimationComponent animComponent, out float walkMs, out float runMs, out float sprintMs)
    {
        walkMs = -1.0;
        runMs = -1.0;
        sprintMs = -1.0;
        if (!animComponent)
            return;
        if (!ownerEnt)
            return;
        if (!ownerEnt.GetWorld())
            return;
        walkMs = animComponent.GetMaxSpeed(1.0, 0.0, 1);
        runMs = animComponent.GetMaxSpeed(1.0, 0.0, 2);
        sprintMs = animComponent.GetMaxSpeed(1.0, 0.0, 3);
    }

    static float PickLiveEngineTopMs(int movementPhase, float walkMs, float runMs, float sprintMs)
    {
        if (movementPhase == 3)
            return sprintMs;
        if (movementPhase == 1)
            return walkMs;
        return runMs;
    }

    //! 仅首次：短暂抬限到 1.0 标定真实顶速（之后实时路径用此缓存做污染检测）
    protected void CalibrateUncappedEngineTopsOnce(IEntity ownerEnt, CharacterAnimationComponent animComponent, float restoreMult)
    {
        if (m_bEngineTopUncappedCalibrated)
            return;
        if (!animComponent)
            return;
        if (!ownerEnt)
            return;
        if (!ownerEnt.GetWorld())
            return;

        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(ownerEnt, 1.0);
        m_fCachedEngineMaxWalkMs = animComponent.GetMaxSpeed(1.0, 0.0, 1);
        m_fCachedEngineMaxRunMs = animComponent.GetMaxSpeed(1.0, 0.0, 2);
        m_fCachedEngineMaxSprintMs = animComponent.GetMaxSpeed(1.0, 0.0, 3);
        SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(ownerEnt, restoreMult);
        m_bEngineTopUncappedCalibrated = true;
    }

    float GetDynamicOriginalEngineMaxSpeed(IEntity ownerEnt, CharacterAnimationComponent animComponent, float restoreMult, int movementPhase)
    {
        float fallback = GetEngineTopFallbackMs(movementPhase);

        if (!animComponent)
        {
            float cachedOnly = GetCachedEngineTopMs(movementPhase);
            if (cachedOnly > 0.5)
                return cachedOnly;
            return fallback;
        }

        // 实时路径：不解限 GetMaxSpeed → 算 OverrideMaxSpeed 因数分母
        if (SCR_RSS_Constants.V6_ENGINE_TOP_LIVE_SAMPLE)
        {
            CalibrateUncappedEngineTopsOnce(ownerEnt, animComponent, restoreMult);

            float liveWalk;
            float liveRun;
            float liveSprint;
            SampleLiveEngineTops(ownerEnt, animComponent, liveWalk, liveRun, liveSprint);
            float live = PickLiveEngineTopMs(movementPhase, liveWalk, liveRun, liveSprint);
            float uncapped = GetCachedEngineTopMs(movementPhase);

            if (live > 0.5)
            {
                // live 接近解限标定 → API 不受 OverrideMaxSpeed 影响，可用实时值（含姿态变化）
                if (uncapped <= 0.5)
                    return live;

                float minRatio = SCR_RSS_Constants.V6_ENGINE_TOP_LIVE_MIN_RATIO;
                if (minRatio < 0.5)
                    minRatio = 0.5;
                if (live >= uncapped * minRatio)
                    return live;

                // live 明显低于标定 → 多半已被当前限速缩放，回退解限缓存避免 frac→1
                return uncapped;
            }

            if (uncapped > 0.5)
                return uncapped;
            return fallback;
        }

        // 旧路径：缓存命中则直接返回；否则解限测一次
        float cached = GetCachedEngineTopMs(movementPhase);
        if (cached > 0.5)
            return cached;

        CalibrateUncappedEngineTopsOnce(ownerEnt, animComponent, restoreMult);
        cached = GetCachedEngineTopMs(movementPhase);
        if (cached > 0.5)
            return cached;
        return fallback;
    }
}
