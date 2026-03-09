// 冲刺视野变化（Sprint FOV）
// 仅保留冲刺时的 FOV 变化（Burst/Cruise/Limp），已移除镜头摇晃（起步/急停惯性、步伐颠簸、上坡摇摆）。

modded class CharacterCamera1stPerson
{
    protected float m_fSprintFovBonusCurrent = 0.0; // 当前 FOV 加成（度），平滑趋近目标 Burst/Cruise/Limp

    override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
    {
        super.OnUpdate(pDt, pOutResult);

        if (!m_ControllerComponent || !m_OwnerCharacter)
            return;

        SCR_CharacterControllerComponent rssController = SCR_CharacterControllerComponent.Cast(m_ControllerComponent);
        if (!rssController)
            return;

        float worldTimeSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        float staminaPercent = GetStaminaPercent();

        float targetFovBonus = ComputeTargetSprintFovBonus(rssController, worldTimeSec, staminaPercent);
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
        if (Math.AbsFloat(m_fSprintFovBonusCurrent) > 0.001)
            pOutResult.m_fFOV = pOutResult.m_fFOV + m_fSprintFovBonusCurrent;
    }

    protected float ComputeTargetSprintFovBonus(SCR_CharacterControllerComponent rssController, float worldTimeSec, float staminaPercent)
    {
        float limpThreshold = StaminaConstants.GetCamSprintFovLimpStaminaThreshold();
        if (staminaPercent < limpThreshold)
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
        return staminaComp.GetStamina();
    }
}
