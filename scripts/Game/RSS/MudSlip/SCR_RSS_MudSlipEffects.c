// 泥泞滑倒：摩擦力阈值模型（RCOF > ACOF）+ Poisson 过程；环境门槛与 CalculateSlipRisk 一致
// RCOF 额外含：有符号坡度（下坡加权）、急转 min(ω·v·k, T_max)、低体力平衡惩罚、落地/起跳蹬地

class SCR_RSS_MudSlipEffects
{
    protected const float M_EULER = 2.718281828459045;

    // @param env 已 UpdateEnvironmentFactors 后的环境模块
    // @param horizontalSpeed 水平速度 m/s
    // @param isSprinting 是否处于冲刺相
    // @param encumbranceKg 当前负重 kg（装备，不含身体）
    // @param isCrouching 蹲姿（步幅小，降低 RCOF）
    // @param slopeAngleDegreesSigned SCR_RSS_SpeedCalculator：正=上坡，负=下坡
    // @param turnRateRadPerSec 水平速度方向变化率（rad/s），由 PlayerBase 两帧水平向夹角/Δt
    // @param staminaPercent 当前目标体力 0~1，低体力提高 RCOF
    // @param deltaTimeSec 本帧时间步长（秒）
    // @param currentWorldTime 世界时间（秒）
    // @param cooldownAnchorTime 9s 冷却起点（秒）；<0 表示尚未锚定（滑倒后未从趴起身则不计时）
    static bool TryRollMudSlip(
        SCR_RSS_EnvironmentFactor env,
        float horizontalSpeed,
        bool isSprinting,
        float encumbranceKg,
        bool isCrouching,
        float slopeAngleDegreesSigned,
        float turnRateRadPerSec,
        float staminaPercent,
        float verticalVelocityY,
        float prevVerticalVelocityY,
        float deltaTimeSec,
        float currentWorldTime,
        float cooldownAnchorTime)
    {
        if (!env)
            return false;
        if (!SCR_RSS_ConfigBridge.IsMudPenaltyEnabled())
            return false;
        if (env.GetSlipRisk() <= 0.0)
            return false;
        if (cooldownAnchorTime >= 0.0)
        {
            if (currentWorldTime - cooldownAnchorTime < SCR_RSS_Constants.ENV_MUD_SLIP_COOLDOWN_SEC)
                return false;
        }

        if (horizontalSpeed < SCR_RSS_Constants.ENV_MUD_SLIP_MIN_SPEED_MS)
            return false;

        float acof = ComputeAvailableCof(env);
        float rcof = ComputeRequiredCof(
            horizontalSpeed,
            isSprinting,
            encumbranceKg,
            isCrouching,
            slopeAngleDegreesSigned,
            turnRateRadPerSec,
            staminaPercent,
            verticalVelocityY,
            prevVerticalVelocityY,
            true);
        float gap = rcof - acof;
        if (gap <= SCR_RSS_Constants.ENV_SLIP_GAP_EPSILON_DICE)
            return false;

        float lambda = ComputeLambdaFromGap(gap, env);
        if (lambda <= 0.0)
            return false;

        float exponent = -lambda * deltaTimeSec;
        float prob = 1.0 - Math.Pow(M_EULER, exponent);
        if (prob > 0.6)
            prob = 0.6;

        return Math.RandomFloat01() < prob;
    }

    // 泥泞失稳镜头预警：与滑倒同一套 ACOF/RCOF，但 RCOF 不含低体力随机抖动，避免画面高频跳变
    // @return 0~1，供第一人称摄像机叠加角抖动强度
    static float ComputeSlipCameraStress01(
        SCR_RSS_EnvironmentFactor env,
        float horizontalSpeed,
        bool isSprinting,
        float encumbranceKg,
        bool isCrouching,
        float slopeAngleDegreesSigned,
        float turnRateRadPerSec,
        float staminaPercent,
        float verticalVelocityY,
        float prevVerticalVelocityY)
    {
        if (!env)
            return 0.0;
        if (!SCR_RSS_ConfigBridge.IsMudPenaltyEnabled())
            return 0.0;
        if (env.GetSlipRisk() <= 0.0)
            return 0.0;
        if (horizontalSpeed < SCR_RSS_Constants.ENV_MUD_SLIP_MIN_SPEED_MS)
            return 0.0;

        float acof = ComputeAvailableCof(env);
        float rcof = ComputeRequiredCof(
            horizontalSpeed,
            isSprinting,
            encumbranceKg,
            isCrouching,
            slopeAngleDegreesSigned,
            turnRateRadPerSec,
            staminaPercent,
            verticalVelocityY,
            prevVerticalVelocityY,
            false);
        float gap = rcof - acof;
        float epsCam = SCR_RSS_Constants.ENV_SLIP_GAP_EPSILON_CAMERA;
        float thrFat = SCR_RSS_Constants.ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_STAMINA_THRESHOLD;
        if (thrFat > 0.0001)
        {
            if (staminaPercent < thrFat)
            {
                float s = staminaPercent;
                if (s < 0.0)
                    s = 0.0;
                float u = s / thrFat;
                float sc = SCR_RSS_Constants.ENV_MUD_SLIP_CAM_SHAKE_FATIGUE_EPS_SCALE;
                epsCam = epsCam * (sc + (1.0 - sc) * u);
            }
        }
        if (gap <= epsCam)
            return 0.0;

        float span = SCR_RSS_Constants.ENV_MUD_SLIP_CAM_SHAKE_GAP_SPAN;
        if (span <= 0.0001)
            return 0.0;
        float t = (gap - epsCam) / span;
        if (t < 0.0)
            t = 0.0;
        if (t > 1.0)
            t = 1.0;
        return t;
    }

    // 可用摩擦：干燥材质基线 ×（1 − 泥泞润滑）；越野略降干摩擦上界
    protected static float ComputeAvailableCof(SCR_RSS_EnvironmentFactor env)
    {
        float eta = env.GetCachedTerrainFactor();
        float mud = env.GetMudFactor();
        float thr = SCR_RSS_Constants.ENV_MUD_SLIPPERY_THRESHOLD;

        float muDry = SCR_RSS_Constants.ENV_SLIP_ACOF_DRY_BASE;
        if (eta > 1.0)
        {
            float t = eta - 1.0;
            if (t > 1.0)
                t = 1.0;
            muDry = muDry - t * SCR_RSS_Constants.ENV_SLIP_ACOF_OFFROAD_DROP;
        }

        float lub = 0.0;
        if (mud > thr)
            lub = (mud - thr) / (1.0 - thr);
        lub = Math.Clamp(lub, 0.0, 1.0);

        float mu = muDry * (1.0 - SCR_RSS_Constants.ENV_SLIP_LUBRICATION_MAX * lub);
        mu = Math.Clamp(mu, SCR_RSS_Constants.ENV_SLIP_ACOF_MIN, SCR_RSS_Constants.ENV_SLIP_ACOF_MAX);
        return mu;
    }

    // 所需摩擦：基线 + 与 (v−v_min)² 成正比的剪切需求 + 冲刺 + 负重；蹲姿缩放
    protected static float ComputeRequiredCof(
        float horizontalSpeed,
        bool isSprinting,
        float encumbranceKg,
        bool isCrouching,
        float slopeAngleDegreesSigned,
        float turnRateRadPerSec,
        float staminaPercent,
        float verticalVelocityY,
        float prevVerticalVelocityY,
        bool applyBalanceJitter)
    {
        float vmin = SCR_RSS_Constants.ENV_MUD_SLIP_MIN_SPEED_MS;
        float ex = horizontalSpeed - vmin;
        if (ex < 0.0)
            ex = 0.0;

        float rcof = SCR_RSS_Constants.ENV_SLIP_RCOF_BASE;
        rcof = rcof + SCR_RSS_Constants.ENV_SLIP_RCOF_VSQ * ex * ex;
        if (isSprinting)
            rcof = rcof + SCR_RSS_Constants.ENV_SLIP_RCOF_SPRINT;

        float enc = Math.Clamp(encumbranceKg, 0.0, 55.0);
        rcof = rcof + SCR_RSS_Constants.ENV_SLIP_RCOF_WEIGHT * (enc / 55.0);

        if (isCrouching)
            rcof = rcof * SCR_RSS_Constants.ENV_SLIP_RCOF_CROUCH_MULT;

        float slopeMagDeg = Math.AbsFloat(slopeAngleDegreesSigned);
        float slopeAdd = SCR_RSS_Constants.ENV_SLIP_RCOF_SLOPE_PER_DEG * slopeMagDeg;
        if (slopeAdd > SCR_RSS_Constants.ENV_SLIP_RCOF_SLOPE_MAX)
            slopeAdd = SCR_RSS_Constants.ENV_SLIP_RCOF_SLOPE_MAX;
        float slopeDirMult = 1.0;
        if (slopeAngleDegreesSigned < 0.0)
            slopeDirMult = SCR_RSS_Constants.ENV_SLIP_RCOF_SLOPE_DOWNHILL_MULT;
        else
        {
            if (slopeAngleDegreesSigned > 0.0)
                slopeDirMult = SCR_RSS_Constants.ENV_SLIP_RCOF_SLOPE_UPHILL_MULT;
        }
        slopeAdd = slopeAdd * slopeDirMult;
        rcof = rcof + slopeAdd;

        float omega = turnRateRadPerSec;
        if (omega < 0.0)
            omega = 0.0;
        if (omega > SCR_RSS_Constants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC)
            omega = SCR_RSS_Constants.ENV_SLIP_RCOF_TURN_RATE_CAP_RADSEC;
        float vTurn = horizontalSpeed;
        if (vTurn < vmin)
            vTurn = vmin;
        float turnAdd = omega * vTurn * SCR_RSS_Constants.ENV_SLIP_RCOF_TURN_OMEGA_V_COEFF;
        if (turnAdd > SCR_RSS_Constants.ENV_SLIP_RCOF_TURN_MAX)
            turnAdd = SCR_RSS_Constants.ENV_SLIP_RCOF_TURN_MAX;
        rcof = rcof + turnAdd;

        float st = staminaPercent;
        if (st < 0.0)
            st = 0.0;
        if (st > 1.0)
            st = 1.0;
        float staminaDeficit = 1.0 - st;
        rcof = rcof + SCR_RSS_Constants.ENV_SLIP_RCOF_BALANCE_STAMINA * staminaDeficit;
        if (applyBalanceJitter)
        {
            if (staminaDeficit > 0.01)
                rcof = rcof + Math.RandomFloat01() * SCR_RSS_Constants.ENV_SLIP_RCOF_BALANCE_JITTER * staminaDeficit;
        }

        if (prevVerticalVelocityY < SCR_RSS_Constants.ENV_SLIP_LANDING_VY_PREV)
        {
            if (verticalVelocityY > SCR_RSS_Constants.ENV_SLIP_LANDING_VY_CUR)
                rcof = rcof + SCR_RSS_Constants.ENV_SLIP_RCOF_LANDING;
        }

        if (verticalVelocityY > SCR_RSS_Constants.ENV_SLIP_RCOF_JUMP_UP_VY)
            rcof = rcof + SCR_RSS_Constants.ENV_SLIP_RCOF_JUMP_UP;

        return rcof;
    }

    // 缺口 → 事件率：λ∝gap（线性），大缺口时比 gap² 更平缓；越野泥泞略增
    protected static float ComputeLambdaFromGap(float gap, SCR_RSS_EnvironmentFactor env)
    {
        float g = gap;
        if (g < 0.0)
            g = 0.0;
        float lambda = SCR_RSS_Constants.ENV_MUD_SLIP_PHYS_SCALE * g;
        lambda = lambda * SCR_RSS_Constants.ENV_MUD_SLIP_GLOBAL_SCALE;
        if (env.GetMudTerrainFactor() > 0.0)
            lambda = lambda * 1.18;
        return lambda;
    }
}
