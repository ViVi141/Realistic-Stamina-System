// ============================================================
// SCR_MudSlip.c
// 职责: 泥泞滑倒系统——摩擦力阈值模型 / Poisson 过程判定 / 相机预警 / 状态管理
// 所属域: MudSlip
// ============================================================
// 合并自:
//   RSS/MudSlip/SCR_MudSlipEffects.c
//   RSS/MudSlip/SCR_RSS_MudSlipRunner.c
// ============================================================

// ---------------------------------------------------------------------------
// 1. 泥泞滑倒物理模型 (MudSlipEffects)
//    摩擦力阈值模型（RCOF > ACOF）+ Poisson 过程
// ---------------------------------------------------------------------------
class MudSlipEffects
{
    protected const float M_EULER = 2.718281828459045;

    static bool TryRollMudSlip(
        EnvironmentFactor env, float horizontalSpeed, bool isSprinting,
        float encumbranceKg, bool isCrouching, float slopeAngleDegreesSigned,
        float turnRateRadPerSec, float staminaPercent,
        float verticalVelocityY, float prevVerticalVelocityY,
        float deltaTimeSec, float currentWorldTime, float cooldownAnchorTime)
    {
        if (!env) return false;
        if (!StaminaConfigBridge.IsMudPenaltyEnabled()) return false;
        if (env.GetSlipRisk() <= 0.0) return false;
        if (cooldownAnchorTime >= 0.0)
        {
            if (currentWorldTime - cooldownAnchorTime < StaminaMudSlipConstants.ENV_MUD_SLIP_COOLDOWN_SEC)
                return false;
        }
        if (horizontalSpeed < StaminaMudSlipConstants.ENV_MUD_SLIP_MIN_SPEED_MS)
            return false;

        float acof = ComputeAvailableCof(env);
        float rcof = ComputeRequiredCof(horizontalSpeed, isSprinting, encumbranceKg,
            isCrouching, slopeAngleDegreesSigned, turnRateRadPerSec, staminaPercent,
            verticalVelocityY, prevVerticalVelocityY, true);
        float gap = rcof - acof;
        if (gap <= StaminaMudSlipConstants.ENV_SLIP_GAP_EPSILON_DICE) return false;

        float lambda = ComputeLambdaFromGap(gap, env);
        if (lambda <= 0.0) return false;

        float exponent = -lambda * deltaTimeSec;
        float prob = 1.0 - Math.Pow(M_EULER, exponent);
        if (prob > 0.6) prob = 0.6;
        return Math.RandomFloat01() < prob;
    }

    static float ComputeSlipCameraStress01(
        EnvironmentFactor env, float horizontalSpeed, bool isSprinting,
        float encumbranceKg, bool isCrouching, float slopeAngleDegreesSigned,
        float turnRateRadPerSec, float staminaPercent,
        float verticalVelocityY, float prevVerticalVelocityY)
    {
        if (!env) return 0.0;
        if (!StaminaConfigBridge.IsMudPenaltyEnabled()) return 0.0;
        if (env.GetSlipRisk() <= 0.0) return 0.0;
        if (horizontalSpeed < StaminaMudSlipConstants.ENV_MUD_SLIP_MIN_SPEED_MS)
            return 0.0;

        float acof = ComputeAvailableCof(env);
        float rcof = ComputeRequiredCof(horizontalSpeed, isSprinting, encumbranceKg,
            isCrouching, slopeAngleDegreesSigned, turnRateRadPerSec, staminaPercent,
            verticalVelocityY, prevVerticalVelocityY, false);
        float gap = rcof - acof;

        float epsCam = StaminaMudSlipConstants.ENV_SLIP_GAP_EPSILON_CAMERA;
        float thrFat = StaminaMudSlipConstants.ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_STAMINA_THRESHOLD;
        if (thrFat > 0.0001)
        {
            if (staminaPercent < thrFat)
            {
                float s = staminaPercent;
                if (s < 0.0) s = 0.0;
                float u = s / thrFat;
                float sc = StaminaMudSlipConstants.ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_EPS_SCALE;
                epsCam = epsCam * (sc + (1.0 - sc) * u);
            }
        }
        if (gap <= epsCam) return 0.0;

        float span = StaminaMudSlipConstants.ENV_MUD_SLIP_CAM_SHAKE_GAP_SPAN;
        if (span <= 0.0001) return 0.0;
        float t = (gap - epsCam) / span;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        return t;
    }

    protected static float ComputeAvailableCof(EnvironmentFactor env)
    {
        float eta = env.GetCachedTerrainFactor();
        float mud = env.GetMudFactor();
        float thr = StaminaEnvironmentAdvConstants.ENV_MUD_SLIPPERY_THRESHOLD;

        float muDry = StaminaMudSlipConstants.ENV_SLIP_ACOF_DRY_BASE;
        if (eta > 1.0)
        {
            float t = eta - 1.0;
            if (t > 1.0) t = 1.0;
            muDry = muDry - t * StaminaMudSlipConstants.ENV_SLIP_ACOF_OFFROAD_DROP;
        }

        float lub = 0.0;
        if (mud > thr) lub = (mud - thr) / (1.0 - thr);
        lub = Math.Clamp(lub, 0.0, 1.0);

        float mu = muDry * (1.0 - StaminaMudSlipConstants.ENV_SLIP_LUBRICATION_MAX * lub);
        mu = Math.Clamp(mu, StaminaMudSlipConstants.ENV_SLIP_ACOF_MIN, StaminaMudSlipConstants.ENV_SLIP_ACOF_MAX);
        return mu;
    }

    protected static float ComputeRequiredCof(
        float horizontalSpeed, bool isSprinting, float encumbranceKg,
        bool isCrouching, float slopeAngleDegreesSigned, float turnRateRadPerSec,
        float staminaPercent, float verticalVelocityY, float prevVerticalVelocityY,
        bool applyBalanceJitter)
    {
        float vmin = StaminaMudSlipConstants.ENV_MUD_SLIP_MIN_SPEED_MS;
        float ex = horizontalSpeed - vmin;
        if (ex < 0.0) ex = 0.0;

        float rcof = StaminaMudSlipConstants.ENV_SLIP_RCOF_BASE;
        rcof = rcof + StaminaMudSlipConstants.ENV_SLIP_RCOF_VSQ * ex * ex;
        if (isSprinting)
            rcof = rcof + StaminaMudSlipConstants.ENV_SLIP_RCOF_SPRINT;

        float enc = Math.Clamp(encumbranceKg, 0.0, 55.0);
        rcof = rcof + StaminaMudSlipConstants.ENV_SLIP_RCOF_WEIGHT * (enc / 55.0);
        if (isCrouching)
            rcof = rcof * StaminaMudSlipConstants.ENV_SLIP_RCOF_CROUCH_MULT;

        float slopeMagDeg = Math.AbsFloat(slopeAngleDegreesSigned);
        float slopeAdd = StaminaMudSlipConstants.ENV_SLIP_RCOF_SLOPE_PER_DEG * slopeMagDeg;
        if (slopeAdd > StaminaMudSlipConstants.ENV_SLIP_RCOF_SLOPE_MAX)
            slopeAdd = StaminaMudSlipConstants.ENV_SLIP_RCOF_SLOPE_MAX;
        float slopeDirMult = 1.0;
        if (slopeAngleDegreesSigned < 0.0)
            slopeDirMult = StaminaMudSlipConstants.ENV_SLIP_RCOF_SLOPE_DOWNHILL_MULT;
        else if (slopeAngleDegreesSigned > 0.0)
            slopeDirMult = StaminaMudSlipConstants.ENV_SLIP_RCOF_SLOPE_UPHILL_MULT;
        slopeAdd = slopeAdd * slopeDirMult;
        rcof = rcof + slopeAdd;

        float omega = turnRateRadPerSec;
        if (omega < 0.0) omega = 0.0;
        if (omega > StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC)
            omega = StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC;
        float vTurn = horizontalSpeed;
        if (vTurn < vmin) vTurn = vmin;
        float turnAdd = omega * vTurn * StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_OMEGA_V_COEFF;
        if (turnAdd > StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_MAX)
            turnAdd = StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_MAX;
        rcof = rcof + turnAdd;

        float st = staminaPercent;
        st = Math.Clamp(st, 0.0, 1.0);
        float staminaDeficit = 1.0 - st;
        rcof = rcof + StaminaMudSlipConstants.ENV_SLIP_RCOF_BALANCE_STAMINA * staminaDeficit;
        if (applyBalanceJitter)
        {
            if (staminaDeficit > 0.01)
                rcof = rcof + Math.RandomFloat01()
                    * StaminaMudSlipConstants.ENV_SLIP_RCOF_BALANCE_JITTER * staminaDeficit;
        }

        if (prevVerticalVelocityY < StaminaMudSlipConstants.ENV_SLIP_LANDING_VY_PREV)
        {
            if (verticalVelocityY > StaminaMudSlipConstants.ENV_SLIP_LANDING_VY_CUR)
                rcof = rcof + StaminaMudSlipConstants.ENV_SLIP_RCOF_LANDING;
        }
        if (verticalVelocityY > StaminaMudSlipConstants.ENV_SLIP_RCOF_JUMP_UP_VY)
            rcof = rcof + StaminaMudSlipConstants.ENV_SLIP_RCOF_JUMP_UP;

        return rcof;
    }

    protected static float ComputeLambdaFromGap(float gap, EnvironmentFactor env)
    {
        float g = gap;
        if (g < 0.0) g = 0.0;
        float lambda = StaminaMudSlipConstants.ENV_MUD_SLIP_PHYS_SCALE * g;
        lambda = lambda * StaminaMudSlipConstants.ENV_MUD_SLIP_GLOBAL_SCALE;
        if (env.GetMudTerrainFactor() > 0.0)
            lambda = lambda * 1.18;
        return lambda;
    }
}

// ---------------------------------------------------------------------------
// 2. 泥泞滑倒每帧状态管理 (RSS_MudSlipRunner)
// ---------------------------------------------------------------------------
class RSS_MudSlipRunner
{
    protected float m_fMudSlipCooldownAnchorTime = -1.0;
    protected bool m_bMudSlipAwaitCooldownAnchor = false;
    protected float m_fMudSlipEventWorldTime = 0.0;
    protected bool m_bMudSlipHasPrevStance = false;
    protected ECharacterStance m_ePrevStanceForMudSlip = ECharacterStance.STAND;
    protected float m_fPrevVerticalVelocityY = 0.0;
    protected vector m_vLastHorizontalVelocity = vector.Zero;

    void ProcessAfterSlope(
        SCR_CharacterControllerComponent ctrl, bool useSwimmingModel,
        bool isSwimming, EnvironmentFactor env,
        SCR_CharacterStaminaComponent stamina, float currentSpeed,
        bool isSprintActive, float currentWeight, float staminaPercent,
        vector velocity, float slopeAngleDegrees, float timeDeltaSec,
        float currentWorldTime, bool rssDebug)
    {
        if (!ctrl) return;

        vector horizontalVelForSlip = velocity;
        horizontalVelForSlip[1] = 0.0;
        float turnRateRadPerSec = 0.0;
        float hLenSlip = horizontalVelForSlip.Length();
        float lastHLenSlip = m_vLastHorizontalVelocity.Length();
        if (hLenSlip > StaminaMudSlipConstants.ENV_SLIP_TURN_MIN_HORIZ_MS)
        {
            if (lastHLenSlip > StaminaMudSlipConstants.ENV_SLIP_TURN_MIN_HORIZ_MS)
            {
                if (timeDeltaSec > 0.00001)
                {
                    float nx = horizontalVelForSlip[0] / hLenSlip;
                    float nz = horizontalVelForSlip[2] / hLenSlip;
                    float lx = m_vLastHorizontalVelocity[0] / lastHLenSlip;
                    float lz = m_vLastHorizontalVelocity[2] / lastHLenSlip;
                    float dot = nx * lx + nz * lz;
                    dot = Math.Clamp(dot, -1.0, 1.0);
                    float angleRad = Math.Acos(dot);
                    turnRateRadPerSec = angleRad / timeDeltaSec;
                    if (turnRateRadPerSec > StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC)
                        turnRateRadPerSec = StaminaMudSlipConstants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC;
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
                        if (slipStance == ECharacterStance.STAND
                            || slipStance == ECharacterStance.CROUCH)
                        {
                            m_fMudSlipCooldownAnchorTime = currentWorldTime;
                            m_bMudSlipAwaitCooldownAnchor = false;
                            anchored = true;
                        }
                    }
                    if (!anchored)
                    {
                        float fbSec = StaminaMudSlipConstants.ENV_MUD_SLIP_COOLDOWN_ANCHOR_FALLBACK_SEC;
                        if (fbSec > 0.0)
                        {
                            if (!ctrl.RSS_IsRagdollActiveForCamera())
                            {
                                if (slipStance == ECharacterStance.STAND
                                    || slipStance == ECharacterStance.CROUCH)
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
                        env, currentSpeed, isSprintActive, currentWeight,
                        slipCrouch, slopeAngleDegrees, turnRateRadPerSec,
                        staminaPercent, vySlip, m_fPrevVerticalVelocityY);
                }
                ctrl.RSS_SetMudSlipCameraShake01(camStress);

                IEntity slipOwner = ctrl.GetOwner();
                bool allowMudSlipRoll = ctrl.RSS_ShouldAiAllowMudSlipRagdoll(slipOwner);
                if (allowMudSlipRoll && !ctrl.RSS_IsRagdollActiveForCamera())
                {
                    if (!m_bMudSlipAwaitCooldownAnchor)
                    {
                        if (MudSlipEffects.TryRollMudSlip(
                            env, currentSpeed, isSprintActive, currentWeight,
                            slipCrouch, slopeAngleDegrees, turnRateRadPerSec,
                            staminaPercent, vySlip, m_fPrevVerticalVelocityY,
                            timeDeltaSec, currentWorldTime,
                            m_fMudSlipCooldownAnchorTime))
                        {
                            m_fMudSlipEventWorldTime = currentWorldTime;
                            m_bMudSlipAwaitCooldownAnchor = true;
                            float stmBefore = stamina.GetTargetStamina();
                            float stmAfter = stmBefore
                                - StaminaMudSlipConstants.ENV_MUD_SLIP_STAMINA_FRAC;
                            stmAfter = Math.Clamp(stmAfter, 0.0, 1.0);
                            stamina.SetTargetStamina(stmAfter);
                            ctrl.RSS_TriggerMudSlipRagdoll();
                            if (rssDebug)
                                SCR_RSS_Logger.Debug(
                                    "[RSS] 泥泞滑倒 / Mud slip: ragdoll + stamina drain");
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
