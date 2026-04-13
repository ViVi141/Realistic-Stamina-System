// CombatStim 首针视觉反馈：复用官方 RegenerationScreenEffect 的色差脉冲逻辑
// 触发条件：本地控制角色的 CombatStim 阶段从 NONE 切换到 DELAY（即首针注射瞬间）
modded class SCR_RegenerationScreenEffect
{
    protected int m_iLastCombatStimPhase = ERSS_CombatStimPhase.NONE;
    protected bool m_bCombatStimPhaseInitialized = false;

    override void UpdateEffect(float timeSlice)
    {
        if (m_pCharacterEntity)
        {
            SCR_CharacterControllerComponent rssController = SCR_CharacterControllerComponent.Cast(m_pCharacterEntity.GetCharacterController());
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
                    // 首针注射：NONE -> DELAY，触发一次类似治疗脉冲的色差效果
                    if (m_iLastCombatStimPhase == ERSS_CombatStimPhase.NONE && currentPhase == ERSS_CombatStimPhase.DELAY)
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
