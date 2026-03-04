// RSS：冲刺爆发期内将音频系统的 CharacterLifeState 设为 INCAPACITATED（昏迷），
// 使爆发期听到的发闷/低通效果与昏迷一致。默认关闭，避免影响听声辨位与作战。

modded class SCR_NoiseFilterEffect
{
    protected const string AUDIO_VAR_CONF = "{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf";

    [Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "冲刺爆发期是否启用发闷音效（0=关，保证作战听感；1=开，更沉浸）")]
    protected bool m_bSprintBurstAudioMuffleEnabled;

    override protected void DisplayUpdate(IEntity owner, float timeSlice)
    {
        super.DisplayUpdate(owner, timeSlice);

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
                    float burstDuration = StaminaConstants.GetTacticalSprintBurstDuration();
                    if (elapsed >= 0.0 && elapsed < burstDuration)
                    {
                        bool isSprintActive = rssCtrl.IsSprinting() || (rssCtrl.GetCurrentMovementPhase() == 3);
                        if (isSprintActive)
                            effectiveAudioState = ECharacterLifeState.INCAPACITATED;
                    }
                }
            }
        }

        AudioSystem.SetVariableByName("CharacterLifeState", effectiveAudioState, AUDIO_VAR_CONF);
    }
}
