//! CP 巡航 / Walk 覆盖 / 限速斜率 / 坡度 EMA。
//! 从 PlayerBase.c 拆出（巡航子域），降低巨型文件体积；行为不变。

class SCR_PlayerBaseCpCruiseController
{
    //! ResolveRunCruiseCapMs 本 tick 结果：0=未算，<0=掉出 Run 带，≥地板=在带内
    protected float m_fRssLastRunCruiseCapMs = 0.0;
    protected bool m_bRssCpWalkOverrideActive = false;
    protected float m_fRssCpWalkOverrideSavedSpeed = 1.0;
    protected float m_fRssCpWalkOverrideReleaseHoldSec = 0.0;
    //! 本帧写入的 Run 巡航模拟量；未写为 -1
    protected float m_fRssLastMoveAnalog = -1.0;
    //! 满推 Run 实机测速（m/s）。武装时采样，缩放时用 v/模拟量反推。未采为 -1。
    protected float m_fRssObservedFullRunMs = -1.0;
    //! 巡航缩轴闭环：v_meas 低于目标时往上加轴幅度，避免卡在 Run 地板 2.2。
    protected float m_fRssActionScaleServo = 0.0;
    protected float m_fLastSpeedSlewTimeSec = -1.0;
    protected float m_fSmoothedGradePercentForSpeed = 0.0;
    protected float m_fLastGradeSmoothTimeSec = -1.0;
    protected bool m_bGradeSmoothInitialized = false;

    void SetRunCruiseCapMs(float capMs)
    {
        m_fRssLastRunCruiseCapMs = capMs;
    }

    float GetRunCruiseCapMs()
    {
        return m_fRssLastRunCruiseCapMs;
    }

    bool IsCpWalkOverrideActive()
    {
        return m_bRssCpWalkOverrideActive;
    }

    float GetLastMoveAnalog()
    {
        return m_fRssLastMoveAnalog;
    }

    void ResetLastMoveAnalog()
    {
        m_fRssLastMoveAnalog = -1.0;
    }

    float GetSmoothedGradePercentForSpeed()
    {
        return m_fSmoothedGradePercentForSpeed;
    }

    //! 满推 Run 实机测速，供巡航比例当分母。
    //! 武装或剩余 W′ 满推时采样；见底闩巡航后冻结，禁止用 2.2/0.61 反推把分母拧飞。
    void UpdateObservedFullRunMs(
        SCR_CharacterControllerComponent ctrl,
        ActionManager am,
        float statusLogSpeed,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        CompartmentAccessComponent compartmentAccess)
    {
        if (!am)
            return;
        if (!ctrl)
            return;
        if (m_bRssCpWalkOverrideActive)
            return;
        if (SCR_PlayerBaseMovementHelper.IsInVehicle(compartmentAccess))
            return;
        if (SCR_RSS_SwimmingStateManager.IsSwimming(ctrl))
            return;
        if (ctrl.GetStance() != ECharacterStance.STAND)
            return;
        if (ctrl.IsSprinting())
            return;
        if (ctrl.GetCurrentMovementPhase() != 2)
            return;

        bool cruiseLatched = false;
        if (anaerobicBurst && anaerobicBurst.GetCpModel())
        {
            cruiseLatched = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(
                anaerobicBurst.GetCpModel());
        }
        if (cruiseLatched)
            return;

        float vMs = statusLogSpeed;
        if (vMs < SCR_RSS_Constants.V6_RUN_GAIT_FLOOR_MS + 0.3)
            return;
        if (vMs > ctrl.GetOriginalEngineMaxSpeed_Sprint() + 0.2)
            return;

        float fwd = am.GetActionValue("CharacterForward");
        float right = am.GetActionValue("CharacterRight");
        float magSq = fwd * fwd + right * right;
        if (magSq < 0.90)
            return;

        float sample = vMs;
        if (m_fRssObservedFullRunMs < 0.1)
        {
            m_fRssObservedFullRunMs = sample;
            return;
        }

        m_fRssObservedFullRunMs = 0.80 * m_fRssObservedFullRunMs + 0.20 * sample;
    }

    //! W′ 见底闩巡航：把 CharacterForward/Right 缩到巡航比例（官方喂移动同款）。
    //! 成功才写 HUD；未写不覆盖本帧已有值（super 前后各试一次）。
    //! updateServo：仅 super 之后积一次，避免每帧双积分把轴拧抖。
    void TryScaleMoveActionValues(
        SCR_CharacterControllerComponent ctrl,
        ActionManager am,
        float dt,
        bool updateServo,
        float appliedSpeedLimitMs,
        float statusLogSpeed,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        CompartmentAccessComponent compartmentAccess)
    {
        if (!SCR_RSS_SpeedBridge.IsActionValueScaleEnabled())
            return;
        if (!SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            return;
        if (!ctrl)
            return;
        if (m_bRssCpWalkOverrideActive)
            return;
        if (SCR_PlayerBaseMovementHelper.IsInVehicle(compartmentAccess))
            return;
        if (SCR_RSS_SwimmingStateManager.IsSwimming(ctrl))
            return;
        if (appliedSpeedLimitMs <= 0.05)
            return;

        bool cruiseLatched = false;
        if (anaerobicBurst && anaerobicBurst.GetCpModel())
        {
            cruiseLatched = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(
                anaerobicBurst.GetCpModel());
        }
        if (!cruiseLatched)
        {
            m_fRssActionScaleServo = 0.0;
            return;
        }

        float walkTopMs = ctrl.GetOriginalEngineMaxSpeed_Walk();
        if (walkTopMs < SCR_RSS_Constants.ENGINE_WALK_TOP_MS)
            walkTopMs = SCR_RSS_Constants.ENGINE_WALK_TOP_MS;
        float engineRunTopMs = ctrl.GetOriginalEngineMaxSpeed_Run();
        float runTopMs = SCR_RSS_SpeedBridge.ResolveActionScaleRunTopMs(
            engineRunTopMs, m_fRssObservedFullRunMs, walkTopMs);

        float desiredAbsMs = SCR_RSS_SpeedBridge.ResolveActionScaleDesiredAbsMs(
            appliedSpeedLimitMs, m_fRssLastRunCruiseCapMs);
        float vMs = statusLogSpeed;
        if (updateServo && vMs > 0.5 && runTopMs > 0.5)
        {
            float err = desiredAbsMs - vMs;
            float stepDt = dt;
            if (stepDt < 0.008)
                stepDt = 0.008;
            if (stepDt > 0.05)
                stepDt = 0.05;
            if (vMs > desiredAbsMs + 0.40)
            {
                m_fRssActionScaleServo = m_fRssActionScaleServo * 0.85;
            }
            else if (err > 0.12)
            {
                float step = 0.6 * err * stepDt;
                if (step > 0.015)
                    step = 0.015;
                m_fRssActionScaleServo = m_fRssActionScaleServo + step;
            }
            else if (err < -0.12)
            {
                float step = 0.6 * err * stepDt;
                if (step < -0.015)
                    step = -0.015;
                m_fRssActionScaleServo = m_fRssActionScaleServo + step;
            }
            if (m_fRssActionScaleServo < 0.0)
                m_fRssActionScaleServo = 0.0;
            if (m_fRssActionScaleServo > 0.22)
                m_fRssActionScaleServo = 0.22;
        }
        float desiredForScale = desiredAbsMs + (m_fRssActionScaleServo * runTopMs);
        float written = SCR_RSS_SpeedBridge.TryScaleMoveActionValues(
            am, ctrl, desiredForScale, walkTopMs, runTopMs);
        if (written >= 0.0)
            m_fRssLastMoveAnalog = written;
    }

    void TryApplyRunCruiseMovementAnalog(
        SCR_CharacterControllerComponent ctrl,
        float appliedSpeedLimitMs,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        CompartmentAccessComponent compartmentAccess)
    {
        if (!SCR_RSS_SpeedBridge.IsMovementAnalogScaleEnabled())
            return;
        m_fRssLastMoveAnalog = -1.0;
        if (!SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
            return;
        if (!ctrl)
            return;
        if (m_bRssCpWalkOverrideActive)
            return;
        if (SCR_PlayerBaseMovementHelper.IsInVehicle(compartmentAccess))
            return;
        if (appliedSpeedLimitMs <= 0.05)
            return;

        bool cruiseLatched = false;
        if (anaerobicBurst && anaerobicBurst.GetCpModel())
        {
            cruiseLatched = SCR_RSS_DrainCalculator.IsAerobicCruiseLatched(
                anaerobicBurst.GetCpModel());
        }
        if (!cruiseLatched)
            return;

        float walkTopMs = ctrl.GetOriginalEngineMaxSpeed_Walk();
        if (walkTopMs < SCR_RSS_Constants.ENGINE_WALK_TOP_MS)
            walkTopMs = SCR_RSS_Constants.ENGINE_WALK_TOP_MS;
        float runTopMs = ctrl.GetOriginalEngineMaxSpeed_Run();
        if (runTopMs < walkTopMs + 0.2)
            runTopMs = walkTopMs + 0.2;

        m_fRssLastMoveAnalog = SCR_RSS_SpeedBridge.TryApplyRunCruiseAnalog(
            ctrl, appliedSpeedLimitMs, walkTopMs, runTopMs);
    }

    //! W′ 解除武装且反解低于 Run 地板（含软带 2.12 这类）时套引擎 Walk 档。
    //! 按住移动则保持（不因下坡反解回到 Run 带而自动改跑）；松开移动或 W′ 再武装才还原。
    void UpdateCpOutOfBandWalkOverride(
        SCR_CharacterControllerComponent ctrl,
        bool isSwimming,
        bool isInVehicle,
        bool holdingMove,
        bool wPrimeEmpty,
        float speedUpdateIntervalMs)
    {
        if (!SCR_RSS_SpeedBridge.IsCpOutOfBandWalkOverrideEnabled())
        {
            ReleaseCpWalkOverride(ctrl);
            return;
        }
        if (!SCR_RSS_SpeedBridge.IsCpMetabolicSpeedCapEnabled())
        {
            ReleaseCpWalkOverride(ctrl);
            return;
        }
        if (isSwimming || isInVehicle)
        {
            ReleaseCpWalkOverride(ctrl);
            return;
        }
        if (!ctrl)
            return;

        float capMs = m_fRssLastRunCruiseCapMs;
        bool outOfBand = SCR_RSS_DrainCalculator.IsRunCruiseCapOutOfBand(capMs);

        bool want = false;
        if (wPrimeEmpty)
        {
            if (m_bRssCpWalkOverrideActive)
            {
                if (holdingMove)
                {
                    want = true;
                    m_fRssCpWalkOverrideReleaseHoldSec = 0.0;
                }
                else
                {
                    float dt = speedUpdateIntervalMs / 1000.0;
                    if (dt < 0.01)
                        dt = 0.01;
                    if (dt > 0.5)
                        dt = 0.5;
                    m_fRssCpWalkOverrideReleaseHoldSec = m_fRssCpWalkOverrideReleaseHoldSec + dt;
                    if (m_fRssCpWalkOverrideReleaseHoldSec < SCR_RSS_Constants.V6_CP_WALK_OVERRIDE_RELEASE_HOLD_SEC)
                        want = true;
                }
            }
            else if (holdingMove && outOfBand)
            {
                want = true;
            }
        }

        if (want)
        {
            if (m_bRssCpWalkOverrideActive)
            {
                SCR_RSS_SpeedBridge.HoldWalkDynamicSpeedOverride(ctrl);
                return;
            }

            float savedSpeed = 1.0;
            if (SCR_RSS_SpeedBridge.TryBeginWalkDynamicSpeedOverride(ctrl, savedSpeed))
            {
                m_fRssCpWalkOverrideSavedSpeed = savedSpeed;
                m_bRssCpWalkOverrideActive = true;
                m_fRssCpWalkOverrideReleaseHoldSec = 0.0;
                if (SCR_PlayerBaseConfigHelper.IsRssDebugEnabled())
                    Print("[RSS] CP walk override on");
            }
            return;
        }

        ReleaseCpWalkOverride(ctrl);
    }

    void ReleaseCpWalkOverride(SCR_CharacterControllerComponent ctrl)
    {
        if (!m_bRssCpWalkOverrideActive)
            return;
        if (ctrl)
            SCR_RSS_SpeedBridge.EndWalkDynamicSpeedOverride(ctrl, m_fRssCpWalkOverrideSavedSpeed);
        m_bRssCpWalkOverrideActive = false;
        m_fRssCpWalkOverrideReleaseHoldSec = 0.0;
        if (SCR_PlayerBaseConfigHelper.IsRssDebugEnabled())
            Print("[RSS] CP walk override off");
    }

    //! 限速倍率斜率限制：压住后期 CP/坡度反解引起的 SetSpeedLimit 抖动
    //! @param previousAppliedFrac 权威限速倍率（仍由 PlayerBase 持有）
    float SlewSpeedLimitFraction(float targetFrac, float currentTimeSec, float previousAppliedFrac)
    {
        float target = targetFrac;
        if (target > 1.0)
            target = 1.0;
        if (target < 0.01)
            target = 0.01;

        float prev = previousAppliedFrac;
        if (prev < 0.01)
            prev = target;

        float dt = 0.05;
        if (m_fLastSpeedSlewTimeSec >= 0.0)
            dt = currentTimeSec - m_fLastSpeedSlewTimeSec;
        if (dt < 0.01)
            dt = 0.01;
        if (dt > 0.5)
            dt = 0.5;
        m_fLastSpeedSlewTimeSec = currentTimeSec;

        float maxStep = SCR_RSS_Constants.V6_SPEED_LIMIT_SLEW_FRAC_PER_SEC * dt;
        float delta = target - prev;
        float deadband = SCR_RSS_Constants.V6_SPEED_LIMIT_DEADBAND_FRAC;
        if (Math.AbsFloat(delta) <= deadband)
            return prev;

        float slewed = target;
        if (delta > maxStep)
        {
            // 从 Idle 钉死的 ~0.01 起步：立刻抬到目标，勿按 1.25/s 爬 0.5 s
            float gaitFloor = SCR_RSS_Constants.V6_GAIT_SPEED_LIMIT_MIN_FRAC;
            if (prev < gaitFloor && target >= gaitFloor)
                slewed = target;
            else
                slewed = prev + maxStep;
        }
        else if (delta < -maxStep)
            slewed = prev - maxStep;

        if (slewed > 1.0)
            slewed = 1.0;
        if (slewed < 0.01)
            slewed = 0.01;
        return slewed;
    }

    //! 坡度 EMA：供 CP 反解限速，避免下坡法线噪声拧速度
    float SmoothGradePercentForSpeed(float rawGradePercent, float currentTimeSec)
    {
        float raw = SCR_RSS_SpeedBridge.ClampGradePercentForMetabolicSpeed(rawGradePercent);

        if (!m_bGradeSmoothInitialized)
        {
            m_fSmoothedGradePercentForSpeed = raw;
            m_bGradeSmoothInitialized = true;
            m_fLastGradeSmoothTimeSec = currentTimeSec;
            return m_fSmoothedGradePercentForSpeed;
        }

        float dt = 0.05;
        if (m_fLastGradeSmoothTimeSec >= 0.0)
            dt = currentTimeSec - m_fLastGradeSmoothTimeSec;
        if (dt < 0.01)
            dt = 0.01;
        if (dt > 0.5)
            dt = 0.5;
        m_fLastGradeSmoothTimeSec = currentTimeSec;

        // 过脊：符号翻转时立刻跟上实测坡度，避免仍按上坡反解把下坡 v_limit 拧到 0.5×。
        float signFlipEps = 2.0;
        if (m_fSmoothedGradePercentForSpeed > signFlipEps && raw < -signFlipEps)
        {
            m_fSmoothedGradePercentForSpeed = raw;
            return m_fSmoothedGradePercentForSpeed;
        }
        if (m_fSmoothedGradePercentForSpeed < -signFlipEps && raw > signFlipEps)
        {
            m_fSmoothedGradePercentForSpeed = raw;
            return m_fSmoothedGradePercentForSpeed;
        }

        // 峭壁进出：限制坡度变化速率，避免巡航顶从 0.3↔2.0 瞬跳触发 SNAP_UP
        float maxDeltaPerSec = 55.0;
        float maxStep = maxDeltaPerSec * dt;
        float delta = raw - m_fSmoothedGradePercentForSpeed;
        if (delta > maxStep)
            raw = m_fSmoothedGradePercentForSpeed + maxStep;
        else if (delta < -maxStep)
            raw = m_fSmoothedGradePercentForSpeed - maxStep;

        float tau = 0.55;
        float alpha = dt / (tau + dt);
        m_fSmoothedGradePercentForSpeed = m_fSmoothedGradePercentForSpeed
            + (raw - m_fSmoothedGradePercentForSpeed) * alpha;
        return m_fSmoothedGradePercentForSpeed;
    }
}
