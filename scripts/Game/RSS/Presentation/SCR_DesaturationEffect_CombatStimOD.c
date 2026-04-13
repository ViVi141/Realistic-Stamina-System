// OD 期间统一视觉：基于 ColorPP 做曝光/饱和浮动
// 说明：modded 后仅在本地控制角色且处于 CombatStim OD 时覆盖原始去饱和逻辑。
modded class SCR_DesaturationEffect
{
    [Attribute(defvalue: "0.62", uiwidget: UIWidgets.EditBox, desc: "OD 最低饱和度")]
    protected float m_fCombatStimODSaturationMin;

    [Attribute(defvalue: "0.88", uiwidget: UIWidgets.EditBox, desc: "OD 最高饱和度")]
    protected float m_fCombatStimODSaturationMax;

    [Attribute(defvalue: "0.2", uiwidget: UIWidgets.EditBox, desc: "OD 呼吸频率（Hz）")]
    protected float m_fCombatStimODPulseFrequency;

    [Attribute(defvalue: "1.7", uiwidget: UIWidgets.EditBox, desc: "呼吸曲线指数（>1 更柔和）")]
    protected float m_fCombatStimODBreathCurve;

    protected override void AddDesaturationEffect()
    {
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
        float saturation = Math.Lerp(m_fCombatStimODSaturationMin, m_fCombatStimODSaturationMax, normalized);

        s_fSaturation = Math.Clamp(saturation, 0.0, 1.0);
        s_bEnableSaturation = true;
    }
}
