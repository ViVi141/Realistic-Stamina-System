// 冲刺视野变化（Sprint FOV）
// 仅保留冲刺时的 FOV 变化（Burst/Cruise/Limp），已移除镜头摇晃（起步/急停惯性、步伐颠簸、上坡摇摆）。
// 泥泞失稳：无踉跄动画时，用 gap 驱动的角抖动 + 小幅 FOV 抖动作预警（与 Ragdoll 滑倒区分：仅高摩擦缺口时强）。

modded class CharacterCamera1stPerson
{
    protected float m_fSprintFovBonusCurrent = 0.0; // 当前 FOV 加成（度），平滑趋近目标 Burst/Cruise/Limp
    protected float m_fSprintFovTargetSmoothed = 0.0; // 冲刺 FOV 目标（度）经低通后的值，抑制体力复制抖动
    protected bool m_bFovLimpHysteresisActive = false; // 跛行 FOV 滞回：避免体力在 0.2 附近反复横跳
    protected float m_fMudSlipShakeSmoothed01 = 0.0;
    protected float m_fMudSlipShakePhaseRad = 0.0;

    override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
    {
        super.OnUpdate(pDt, pOutResult);

        if (StaminaConstants.IsRssPresentationNativeOnly())
            return;

        if (!m_ControllerComponent || !m_OwnerCharacter)
            return;

        SCR_CharacterControllerComponent rssController = m_ControllerComponent;
        if (!rssController)
            return;

        Game game = GetGame();
        if (!game) return;
        World world = game.GetWorld();
        if (!world) return;
        float worldTimeSec = world.GetWorldTime() / 1000.0;
        float staminaPercent = GetStaminaPercent();

        float targetRaw = ComputeTargetSprintFovBonus(rssController, worldTimeSec, staminaPercent);
        float tauTarget = StaminaConstants.GetCamSprintFovTargetSmoothTauSec();
        float alphaTarget = 1.0;
        if (tauTarget > 0.0001 && pDt > 0.0)
            alphaTarget = 1.0 - Math.Pow(2.718281828, -pDt / tauTarget);
        if (alphaTarget > 1.0)
            alphaTarget = 1.0;
        m_fSprintFovTargetSmoothed = m_fSprintFovTargetSmoothed + (targetRaw - m_fSprintFovTargetSmoothed) * alphaTarget;
        float targetFovBonus = m_fSprintFovTargetSmoothed;
        float blendUpSec = StaminaConstants.GetCamSprintFovBlendUpSec();
        float blendDownSec = StaminaConstants.GetCamSprintFovBlendDownSec();
        float blendSec = blendUpSec;
        if (Math.AbsFloat(targetFovBonus) < Math.AbsFloat(m_fSprintFovBonusCurrent))
            blendSec = blendDownSec;
        float newValue = m_fSprintFovBonusCurrent;
        if (blendSec > 0.0)
        {
            float speed = 1.0 / blendSec;
            float factor = 1.0 - Math.Pow(2.718281828, -speed * pDt);
            newValue = Math.Lerp(m_fSprintFovBonusCurrent, targetFovBonus, factor);
        }
        float maxRate = StaminaConstants.GetCamSprintFovMaxRateDegPerSec();
        if (maxRate > 0.0 && pDt > 0.0)
        {
            float maxDelta = maxRate * pDt;
            float delta = newValue - m_fSprintFovBonusCurrent;
            if (delta > maxDelta)
                delta = maxDelta;
            if (delta < -maxDelta)
                delta = -maxDelta;
            m_fSprintFovBonusCurrent = m_fSprintFovBonusCurrent + delta;
        }
        else
            m_fSprintFovBonusCurrent = newValue;

        float fovOut = pOutResult.m_fFOV + m_fSprintFovBonusCurrent;
        ApplyMudSlipCameraShake(pDt, rssController, fovOut, pOutResult);
    }

    protected void ApplyMudSlipCameraShake(float pDt, SCR_CharacterControllerComponent rssController, float fovBase, out ScriptedCameraItemResult pOutResult)
    {
        if (!StaminaConfigBridge.IsMudSlipMechanismEnabled())
        {
            m_fMudSlipShakeSmoothed01 = 0.0;
            m_fMudSlipShakePhaseRad = 0.0;
            pOutResult.m_fFOV = fovBase;
            return;
        }

        float targetStress = rssController.RSS_GetMudSlipCameraShake01();
        float smoothRate = StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_SMOOTH_RATE;
        float blend = smoothRate * pDt;
        if (blend > 1.0)
            blend = 1.0;
        m_fMudSlipShakeSmoothed01 = m_fMudSlipShakeSmoothed01 + (targetStress - m_fMudSlipShakeSmoothed01) * blend;

        float stress = m_fMudSlipShakeSmoothed01;
        if (stress < 0.002)
        {
            pOutResult.m_fFOV = fovBase;
            return;
        }

        float freq = StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_FREQ_BASE;
        freq = freq + stress * StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_FREQ_STRESS;
        m_fMudSlipShakePhaseRad = m_fMudSlipShakePhaseRad + pDt * 2.0 * Math.PI * freq;
        if (m_fMudSlipShakePhaseRad > 100000.0)
            m_fMudSlipShakePhaseRad = m_fMudSlipShakePhaseRad - 100000.0;

        float ph = m_fMudSlipShakePhaseRad;
        float sYaw = stress * StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_YAW_DEG * Math.Sin(ph);
        float sPitch = stress * StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_PITCH_DEG * Math.Sin(ph * 1.17 + 1.1);
        float sRoll = stress * StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_ROLL_DEG * Math.Sin(ph * 1.41 + 0.73);
        vector ypr = "0 0 0";
        ypr[0] = sYaw;
        ypr[1] = sPitch;
        ypr[2] = sRoll;
        vector rotMat[4];
        Math3D.AnglesToMatrix(ypr, rotMat);
        Math3D.MatrixMultiply4(rotMat, pOutResult.m_CameraTM, pOutResult.m_CameraTM);

        float fovJit = stress * StaminaConstants.ENV_MUD_SLIP_CAM_SHAKE_FOV_JITTER_DEG * Math.Sin(ph * 2.03 + 0.4);
        pOutResult.m_fFOV = fovBase + fovJit;
    }

    protected float ComputeTargetSprintFovBonus(SCR_CharacterControllerComponent rssController, float worldTimeSec, float staminaPercent)
    {
        float limpLow = StaminaConstants.GetCamSprintFovLimpHystStaminaLow();
        float limpHigh = StaminaConstants.GetCamSprintFovLimpHystStaminaHigh();
        if (staminaPercent < limpLow)
            m_bFovLimpHysteresisActive = true;
        else if (staminaPercent > limpHigh)
            m_bFovLimpHysteresisActive = false;
        if (m_bFovLimpHysteresisActive)
            return StaminaConstants.GetCamSprintFovLimpDeg();
        if (!rssController.IsSprinting())
            return 0.0;
        float sprintStart = rssController.GetSprintStartTime();
        if (sprintStart < 0.0)
            return StaminaConstants.GetCamSprintFovCruiseDeg();
        float elapsed = worldTimeSec - sprintStart;
        float burstDur = StaminaConstants.GetTacticalSprintBurstDuration();
        if (elapsed < burstDur)
            return StaminaConstants.GetCamSprintFovBurstDeg();
        return StaminaConstants.GetCamSprintFovCruiseDeg();
    }

    protected float GetStaminaPercent()
    {
        if (!m_OwnerCharacter)
            return 1.0;
        CharacterStaminaComponent staminaComp = CharacterStaminaComponent.Cast(m_OwnerCharacter.FindComponent(CharacterStaminaComponent));
        if (!staminaComp)
            return 1.0;
        // CRITICAL FIX: Use GetTargetStamina() (RSS authoritative) instead of
        // GetStamina() (native engine raw value). The Camera runs at 60fps while
        // MonitorStamina only corrects every 200ms. Reading GetStamina() causes
        // the raw value to oscillate between correction cycles, making the sprint
        // FOV bounce between burst/cruise/limp targets — visible as "jitter on
        // screen edges" that players reported even with mud slip disabled.
        SCR_CharacterStaminaComponent rssStamina = SCR_CharacterStaminaComponent.Cast(staminaComp);
        if (rssStamina)
            return rssStamina.GetTargetStamina();
        return staminaComp.GetStamina();
    }
}
