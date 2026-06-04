// ============================================================
// SCR_RSS_ScreenEffects.c
// 职责: RSS 屏幕效果集合——冲刺发闷音效 / CombatStim OD 去饱和 / 首针色差脉冲
// 所属域: UI (Presentation)
// ============================================================
// 合并自:
//   SCR_NoiseFilterEffect_Stamina.c
//   SCR_DesaturationEffect_CombatStimOD.c
//   SCR_RegenerationScreenEffect_CombatStim.c
// ============================================================

// ---------------------------------------------------------------------------
// 1. 冲刺爆发期音频发闷效果
//    将 CharacterLifeState 设为 INCAPACITATED 以触发低频滤波
// ---------------------------------------------------------------------------
modded class SCR_NoiseFilterEffect
{
    protected const string AUDIO_VAR_CONF = "{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf";

    [Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox,
        desc: "冲刺爆发期是否启用发闷音效（0=关，保证作战听感；1=开，更沉浸）")]
    protected bool m_bSprintBurstAudioMuffleEnabled;

    override protected void DisplayUpdate(IEntity owner, float timeSlice)
    {
        super.DisplayUpdate(owner, timeSlice);

        if (StaminaConstants.RSS_PRESENTATION_NATIVE_ONLY())
            return;

        if (m_bDisplaySuspended || !m_pCharacterEntity)
            return;

        CharacterControllerComponent ctrl = m_pCharacterEntity.GetCharacterController();
        if (!ctrl)
            return;

        ECharacterLifeState realState = ctrl.GetLifeState();
        ECharacterLifeState effectiveAudioState = realState;

        if (realState == ECharacterLifeState.ALIVE && m_bSprintBurstAudioMuffleEnabled)
        {
            SCR_CharacterControllerComponent rssCtrl = SCR_CharacterControllerComponent.Cast(ctrl);
            if (rssCtrl)
            {
                float startTime = rssCtrl.GetSprintStartTime();
                if (startTime >= 0.0)
                {
                    float now = GetGame().GetWorld().GetWorldTime() / 1000.0;
                    float elapsed = now - startTime;
                    float burstDuration = StaminaConstants.TACTICAL_SPRINT_BURST_DURATION;
                    if (elapsed >= 0.0 && elapsed < burstDuration)
                    {
                        bool isSprintActive = rssCtrl.IsSprinting()
                            || (rssCtrl.GetCurrentMovementPhase() == 3);
                        if (isSprintActive)
                            effectiveAudioState = ECharacterLifeState.INCAPACITATED;
                    }
                }
            }
        }

        AudioSystem.SetVariableByName("CharacterLifeState", effectiveAudioState, AUDIO_VAR_CONF);
    }
}

// ---------------------------------------------------------------------------
// 2. CombatStim OD 期间统一视觉：基于 ColorPP 做曝光/饱和浮动
// ---------------------------------------------------------------------------
modded class SCR_DesaturationEffect
{
    [Attribute(defvalue: "0.62", uiwidget: UIWidgets.EditBox,
        desc: "OD 最低饱和度")]
    protected float m_fCombatStimODSaturationMin;

    [Attribute(defvalue: "0.88", uiwidget: UIWidgets.EditBox,
        desc: "OD 最高饱和度")]
    protected float m_fCombatStimODSaturationMax;

    [Attribute(defvalue: "0.2", uiwidget: UIWidgets.EditBox,
        desc: "OD 呼吸频率（Hz）")]
    protected float m_fCombatStimODPulseFrequency;

    [Attribute(defvalue: "1.7", uiwidget: UIWidgets.EditBox,
        desc: "呼吸曲线指数（>1 更柔和）")]
    protected float m_fCombatStimODBreathCurve;

    protected override void AddDesaturationEffect()
    {
        if (StaminaConstants.RSS_PRESENTATION_NATIVE_ONLY())
        {
            super.AddDesaturationEffect();
            return;
        }

        if (!m_pCharacterEntity)
        {
            super.AddDesaturationEffect();
            return;
        }

        CharacterControllerComponent baseController = m_pCharacterEntity.GetCharacterController();
        SCR_CharacterControllerComponent rssController = SCR_CharacterControllerComponent.Cast(baseController);
        if (!rssController)
        {
            super.AddDesaturationEffect();
            return;
        }

        if (!rssController.RSS_IsCombatStimOverdosed())
        {
            super.AddDesaturationEffect();
            return;
        }

        float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        float phase = nowSec * m_fCombatStimODPulseFrequency * 2.0 * Math.PI;
        float breathBase = 0.5 - 0.5 * Math.Cos(phase);
        float normalized = Math.Pow(breathBase, m_fCombatStimODBreathCurve);
        float saturation = Math.Lerp(
            m_fCombatStimODSaturationMin, m_fCombatStimODSaturationMax, normalized);

        s_fSaturation = Math.Clamp(saturation, 0.0, 1.0);
        s_bEnableSaturation = true;
    }
}

// ---------------------------------------------------------------------------
// 3. CombatStim 首针视觉反馈：复用官方 RegenerationScreenEffect 的色差脉冲
//    触发条件：本地控制角色 CombatStim 阶段从 NONE 切换到 DELAY
// ---------------------------------------------------------------------------
modded class SCR_RegenerationScreenEffect
{
    protected int m_iLastCombatStimPhase = ERSS_CombatStimPhase.NONE;
    protected bool m_bCombatStimPhaseInitialized = false;

    override void UpdateEffect(float timeSlice)
    {
        if (StaminaConstants.RSS_PRESENTATION_NATIVE_ONLY())
        {
            super.UpdateEffect(timeSlice);
            return;
        }

        if (m_pCharacterEntity)
        {
            SCR_CharacterControllerComponent rssController =
                SCR_CharacterControllerComponent.Cast(m_pCharacterEntity.GetCharacterController());
            if (rssController)
            {
                int currentPhase = rssController.RSS_GetCombatStimPhase();

                if (!m_bCombatStimPhaseInitialized)
                {
                    m_iLastCombatStimPhase = currentPhase;
                    m_bCombatStimPhaseInitialized = true;
                }
                else
                {
                    if (m_iLastCombatStimPhase == ERSS_CombatStimPhase.NONE
                        && currentPhase == ERSS_CombatStimPhase.DELAY)
                    {
                        m_fRegenEffectTimeRemaining = m_fRegenEffectDuration;
                        m_bRegenerationEffect = true;
                    }
                    m_iLastCombatStimPhase = currentPhase;
                }
            }
        }

        super.UpdateEffect(timeSlice);
    }

    override protected void ClearEffects()
    {
        super.ClearEffects();
        m_bCombatStimPhaseInitialized = false;
        m_iLastCombatStimPhase = ERSS_CombatStimPhase.NONE;
    }
}
