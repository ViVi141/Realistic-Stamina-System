// 泥泞滑倒每帧逻辑与状态（从 PlayerBase 拆出以控制单文件编译体积）

class RSS_MudSlipRunner
{
    // 9s 冷却：从「趴→蹲/站」锚定时刻起算；<0 表示本轮滑倒后尚未锚定
    protected float m_fMudSlipCooldownAnchorTime = -1.0;
    // 为 true 表示已触发滑倒，正在等趴起锚定；此期间禁止再次掷骰
    protected bool m_bMudSlipAwaitCooldownAnchor = false;
    protected float m_fMudSlipEventWorldTime = 0.0;
    protected bool m_bMudSlipHasPrevStance = false;
    protected ECharacterStance m_ePrevStanceForMudSlip = ECharacterStance.STAND;

    protected float m_fPrevVerticalVelocityY = 0.0;
    protected vector m_vLastHorizontalVelocity = vector.Zero;

    // 在 CalculateGradePercent 之后调用；内部更新上一帧水平/垂向速度供下帧使用
    void ProcessAfterSlope(
        SCR_CharacterControllerComponent ctrl,
        bool useSwimmingModel,
        bool isSwimming,
        EnvironmentFactor env,
        SCR_CharacterStaminaComponent stamina,
        float currentSpeed,
        bool isSprintActive,
        float currentWeight,
        float staminaPercent,
        vector velocity,
        float slopeAngleDegrees,
        float timeDeltaSec,
        float currentWorldTime,
        bool rssDebug)
    {
        if (!ctrl)
            return;

        vector horizontalVelForSlip = velocity;
        horizontalVelForSlip[1] = 0.0;
        float turnRateRadPerSec = 0.0;
        float hLenSlip = horizontalVelForSlip.Length();
        float lastHLenSlip = m_vLastHorizontalVelocity.Length();
        if (hLenSlip > StaminaConstants.ENV_SLIP_TURN_MIN_HORIZ_MS)
        {
            if (lastHLenSlip > StaminaConstants.ENV_SLIP_TURN_MIN_HORIZ_MS)
            {
                if (timeDeltaSec > 0.00001)
                {
                    float nx = horizontalVelForSlip[0] / hLenSlip;
                    float nz = horizontalVelForSlip[2] / hLenSlip;
                    float lx = m_vLastHorizontalVelocity[0] / lastHLenSlip;
                    float lz = m_vLastHorizontalVelocity[2] / lastHLenSlip;
                    float dot = nx * lx + nz * lz;
                    if (dot > 1.0)
                        dot = 1.0;
                    if (dot < -1.0)
                        dot = -1.0;
                    float angleRad = Math.Acos(dot);
                    turnRateRadPerSec = angleRad / timeDeltaSec;
                    if (turnRateRadPerSec > StaminaConstants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC)
                        turnRateRadPerSec = StaminaConstants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC;
                }
            }
        }

        if (!useSwimmingModel && !isSwimming && env && stamina)
        {
            ECharacterStance slipStance = ctrl.GetStance();

            if (!m_bMudSlipHasPrevStance)
            {
                m_ePrevStanceForMudSlip = slipStance;
                m_bMudSlipHasPrevStance = true;
            }
            else
            {
                if (m_bMudSlipAwaitCooldownAnchor)
                {
                    bool anchored = false;
                    if (m_ePrevStanceForMudSlip == ECharacterStance.PRONE)
                    {
                        if (slipStance == ECharacterStance.STAND || slipStance == ECharacterStance.CROUCH)
                        {
                            m_fMudSlipCooldownAnchorTime = currentWorldTime;
                            m_bMudSlipAwaitCooldownAnchor = false;
                            anchored = true;
                        }
                    }
                    if (!anchored)
                    {
                        float fbSec = StaminaConstants.ENV_MUD_SLIP_COOLDOWN_ANCHOR_FALLBACK_SEC;
                        if (fbSec > 0.0)
                        {
                            if (!ctrl.RSS_IsRagdollActiveForCamera())
                            {
                                if (slipStance == ECharacterStance.STAND || slipStance == ECharacterStance.CROUCH)
                                {
                                    if (currentWorldTime - m_fMudSlipEventWorldTime >= fbSec)
                                    {
                                        m_fMudSlipCooldownAnchorTime = currentWorldTime;
                                        m_bMudSlipAwaitCooldownAnchor = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (slipStance != ECharacterStance.PRONE)
            {
                bool slipCrouch = (slipStance == ECharacterStance.CROUCH);
                float vySlip = velocity[1];

                float camStress = 0.0;
                if (!ctrl.RSS_IsRagdollActiveForCamera())
                {
                    camStress = MudSlipEffects.ComputeSlipCameraStress01(
                        env,
                        currentSpeed,
                        isSprintActive,
                        currentWeight,
                        slipCrouch,
                        slopeAngleDegrees,
                        turnRateRadPerSec,
                        staminaPercent,
                        vySlip,
                        m_fPrevVerticalVelocityY);
                }
                ctrl.RSS_SetMudSlipCameraShake01(camStress);

                // 布娃娃进行中不掷骰；滑倒后须先趴起锚定冷却，未锚定前禁止再次掷骰
                // AI 安全：本分支不调用 TryRoll（泥泞滑倒完全禁用，非靠压速间接避免）
                IEntity slipOwner = ctrl.GetOwner();
                bool allowMudSlipRoll = ctrl.RSS_ShouldAiAllowMudSlipRagdoll(slipOwner);
                if (allowMudSlipRoll && !ctrl.RSS_IsRagdollActiveForCamera())
                {
                    if (!m_bMudSlipAwaitCooldownAnchor)
                    {
                        if (MudSlipEffects.TryRollMudSlip(
                            env,
                            currentSpeed,
                            isSprintActive,
                            currentWeight,
                            slipCrouch,
                            slopeAngleDegrees,
                            turnRateRadPerSec,
                            staminaPercent,
                            vySlip,
                            m_fPrevVerticalVelocityY,
                            timeDeltaSec,
                            currentWorldTime,
                            m_fMudSlipCooldownAnchorTime))
                        {
                            m_fMudSlipEventWorldTime = currentWorldTime;
                            m_bMudSlipAwaitCooldownAnchor = true;
                            float stmBefore = stamina.GetTargetStamina();
                            float stmAfter = stmBefore - StaminaConstants.ENV_MUD_SLIP_STAMINA_FRAC;
                            stmAfter = Math.Clamp(stmAfter, 0.0, 1.0);
                            stamina.SetTargetStamina(stmAfter);
                            ctrl.RSS_TriggerMudSlipRagdoll();
                            if (rssDebug)
                                Print("[RSS] 泥泞滑倒 / Mud slip: ragdoll + stamina drain");
                        }
                    }
                }
                m_fPrevVerticalVelocityY = vySlip;
            }
            else
            {
                ctrl.RSS_SetMudSlipCameraShake01(0.0);
                m_fPrevVerticalVelocityY = velocity[1];
            }

            m_ePrevStanceForMudSlip = slipStance;
        }
        else
        {
            ctrl.RSS_SetMudSlipCameraShake01(0.0);
            if (isSwimming)
                m_fPrevVerticalVelocityY = 0.0;
            else
                m_fPrevVerticalVelocityY = velocity[1];
        }

        if (isSwimming || useSwimmingModel)
            m_vLastHorizontalVelocity = vector.Zero;
        else
            m_vLastHorizontalVelocity = horizontalVelForSlip;
    }
}
