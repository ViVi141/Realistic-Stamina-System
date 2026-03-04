// 镜头惯性与头部物理（Camera Inertia & Head Bob）
// 表现“负重感”和“疲劳感”：起步/急停惯性、步伐垂直颠簸、上坡左右摇摆。
// 在 CharacterCamera1stPerson::OnUpdate 中于原生 head bob 之后对 m_CameraTM 施加附加变换。

modded class CharacterCamera1stPerson
{
    protected float m_fLastVelocityLen = 0.0;
    protected bool m_bLastWasMoving = false;
    protected float m_fStartMoveTime = -1.0;
    protected float m_fStopMoveTime = -1.0;
    protected const float MOVE_VELOCITY_THRESHOLD = 0.15;
    protected float m_fSprintFovBlend = 0.0; // 兼容旧 FOV 混合
    protected float m_fSprintFovBonusCurrent = 0.0; // 当前 FOV 加成（度），平滑趋近目标 Burst/Cruise/Limp
    protected float m_fLastVerticalBob = 0.0; // 上一帧垂直颠簸值，用于呼吸-落步同步检测

    override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
    {
        super.OnUpdate(pDt, pOutResult);

        if (!m_ControllerComponent || !m_OwnerCharacter)
            return;

        SCR_CharacterControllerComponent rssController = SCR_CharacterControllerComponent.Cast(m_ControllerComponent);
        if (!rssController)
            return;

        float worldTimeSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        vector velocity = rssController.GetVelocity();
        float velLen = velocity.Length();
        bool isMoving = velLen > MOVE_VELOCITY_THRESHOLD;

        float encFactor = GetEncumbranceFactor();
        float staminaPercent = GetStaminaPercent();
        float slopeDeg = SpeedCalculator.GetRawSlopeAngle(rssController);

        vector localOffset = "0 0 0";

        float startLag = StaminaConstants.GetCamInertiaStartLagDuration();
        float decelDur = StaminaConstants.GetCamInertiaDecelOvershootDuration();
        float decelForwardM = StaminaConstants.GetCamInertiaDecelOvershootForwardM();
        float encThreshold = StaminaConstants.GetCamInertiaEncumbranceThreshold();

        if (encFactor >= encThreshold)
        {
            if (isMoving && !m_bLastWasMoving)
                m_fStartMoveTime = worldTimeSec;
            if (!isMoving && m_bLastWasMoving)
                m_fStopMoveTime = worldTimeSec;
        }

        float inertiaScale = (encFactor - encThreshold) / (1.0 - encThreshold);
        if (inertiaScale < 0.0)
            inertiaScale = 0.0;
        if (inertiaScale > 1.0)
            inertiaScale = 1.0;

        float debugStrength = StaminaConstants.GetCamDebugStrength();

        if (encFactor >= encThreshold && startLag > 0.0)
        {
            float elapsed = worldTimeSec - m_fStartMoveTime;
            if (isMoving && elapsed >= 0.0 && elapsed <= startLag)
            {
                float t = elapsed / startLag;
                float tiltDeg = StaminaConstants.GetCamInertiaStartTiltDeg() * (1.0 - t) * inertiaScale * debugStrength;
                ApplyPitchTilt(tiltDeg, pOutResult.m_CameraTM);
            }
        }

        if (decelDur > 0.0 && decelForwardM > 0.0)
        {
            float elapsed = worldTimeSec - m_fStopMoveTime;
            if (!isMoving && elapsed >= 0.0 && elapsed <= decelDur)
            {
                float effectiveWeightKg = encFactor * StaminaConstants.CHARACTER_WEIGHT;
                float loadMin = StaminaConstants.GetCamInertiaOvershootLoadMinKg();
                float loadMax = StaminaConstants.GetCamInertiaOvershootLoadMaxKg();
                float loadRange = loadMax - loadMin;
                float loadFactor = 0.0;
                if (loadRange > 0.0)
                {
                    loadFactor = (effectiveWeightKg - loadMin) / loadRange;
                    loadFactor = Math.Clamp(loadFactor, 0.0, 1.0);
                }
                float exp = StaminaConstants.GetCamInertiaOvershootExponent();
                float actualOvershoot = decelForwardM * Math.Pow(loadFactor, exp);
                float decay = 1.0 - (elapsed / decelDur) * (elapsed / decelDur);
                float forward = actualOvershoot * decay * debugStrength;
                localOffset[2] = localOffset[2] + forward;
            }
        }

        if (isMoving && velLen > 0.2)
        {
            float ampBase = StaminaConstants.GetCamBobVerticalAmplitudeBase();
            float freqBase = StaminaConstants.GetCamBobVerticalFreqBase();
            float encScale = StaminaConstants.GetCamBobEncumbranceScale();
            float stamScale = StaminaConstants.GetCamBobStaminaScale();
            float amp = ampBase * (1.0 + encFactor * encScale + (1.0 - staminaPercent) * stamScale);
            float freq = freqBase * (1.0 - 0.15 * encFactor);
            float phase = worldTimeSec * freq * Math.PI * 2.0;
            float verticalBob = Math.Sin(phase) * amp;
            localOffset[1] = localOffset[1] + verticalBob;

            if (staminaPercent < StaminaConstants.GetCamSprintFovLimpStaminaThreshold() && amp > 0.0)
            {
                float bobBottomThreshold = -amp * 0.7;
                if (m_fLastVerticalBob >= bobBottomThreshold && verticalBob < bobBottomThreshold)
                    OnBreathFootDown();
            }
            m_fLastVerticalBob = verticalBob;

            float swayAmp = StaminaConstants.GetCamBobUphillSwayAmplitude();
            float swayFreq = StaminaConstants.GetCamBobUphillSwayFreq();
            float slopeMin = StaminaConstants.GetCamBobUphillSlopeDegMin();
            if (slopeDeg >= slopeMin && swayAmp > 0.0)
            {
                float slopeT = Math.Clamp((slopeDeg - slopeMin) / 15.0, 0.0, 1.0);
                float swayPhase = worldTimeSec * swayFreq * Math.PI * 2.0;
                float lateralSway = Math.Sin(swayPhase) * swayAmp * slopeT;
                localOffset[0] = localOffset[0] + lateralSway;
            }
        }
        else
            m_fLastVerticalBob = 0.0;

        if (debugStrength != 0.0 && (localOffset[0] != 0.0 || localOffset[1] != 0.0 || localOffset[2] != 0.0))
        {
            vector right = pOutResult.m_CameraTM[0];
            vector up = pOutResult.m_CameraTM[1];
            vector fwd = pOutResult.m_CameraTM[2];
            pOutResult.m_CameraTM[3][0] = pOutResult.m_CameraTM[3][0] + (right[0] * localOffset[0] + up[0] * localOffset[1] + fwd[0] * localOffset[2]) * debugStrength;
            pOutResult.m_CameraTM[3][1] = pOutResult.m_CameraTM[3][1] + (right[1] * localOffset[0] + up[1] * localOffset[1] + fwd[1] * localOffset[2]) * debugStrength;
            pOutResult.m_CameraTM[3][2] = pOutResult.m_CameraTM[3][2] + (right[2] * localOffset[0] + up[2] * localOffset[1] + fwd[2] * localOffset[2]) * debugStrength;
        }

        m_fLastVelocityLen = velLen;
        m_bLastWasMoving = isMoving;

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

    protected void OnBreathFootDown()
    {
        // 视听同步：低体力时每次“重踩”（镜头下沉）触发沉重喘息。需在此处接入音效（如 SoundManager.PlaySound 或事件）。
    }

    protected float GetEncumbranceFactor()
    {
        if (!m_OwnerCharacter)
            return 0.0;
        SCR_InventoryStorageManagerComponent invMgr = SCR_InventoryStorageManagerComponent.Cast(m_OwnerCharacter.FindComponent(SCR_InventoryStorageManagerComponent));
        if (!invMgr)
            return 0.0;
        float totalWeight = invMgr.GetTotalWeightOfAllStorages();
        float effectiveWeight = Math.Max(totalWeight - StaminaConstants.BASE_WEIGHT, 0.0);
        float bodyMassPercent = effectiveWeight / StaminaConstants.CHARACTER_WEIGHT;
        return Math.Clamp(bodyMassPercent, 0.0, 1.0);
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

    protected void ApplyPitchTilt(float tiltDeg, inout vector cameraTM[4])
    {
        float rad = tiltDeg * Math.DEG2RAD;
        float c = Math.Cos(rad);
        float s = Math.Sin(rad);
        vector up = cameraTM[1];
        vector fwd = cameraTM[2];
        vector newUp = "0 0 0";
        newUp[0] = up[0] * c - fwd[0] * s;
        newUp[1] = up[1] * c - fwd[1] * s;
        newUp[2] = up[2] * c - fwd[2] * s;
        vector newFwd = "0 0 0";
        newFwd[0] = up[0] * s + fwd[0] * c;
        newFwd[1] = up[1] * s + fwd[1] * c;
        newFwd[2] = up[2] * s + fwd[2] * c;
        cameraTM[1] = newUp;
        cameraTM[2] = newFwd;
    }
}
