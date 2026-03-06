// RSS：在官方 SCR_DesaturationEffect 基础上增加体力驱动的饱和度
// 体力从 m_fStaminaDesaturationStart(40%) 开始线性下降，到 m_fStaminaDesaturationEnd(10%) 时接近黑白。
// 与血液去饱和取较严重的一侧（min），共用同一 Colors PP 与静态变量，无需替换 Display 中的效果。

modded class SCR_DesaturationEffect
{
    [Attribute(defvalue: "0.4", uiwidget: UIWidgets.EditBox, desc: "体力高于此值时饱和度正常 (0-1)")]
    protected float m_fStaminaDesaturationStart;

    [Attribute(defvalue: "0.1", uiwidget: UIWidgets.EditBox, desc: "体力低于此值时接近黑白 (0-1)")]
    protected float m_fStaminaDesaturationEnd;

    // 体力显示平滑：服务器复制频率低于客户端帧率时，避免去饱和效果抽动
    protected static float s_fSmoothedStaminaForEffect = 1.0;
    protected static const float STAMINA_EFFECT_SMOOTH_ALPHA = 0.4;

    override void AddDesaturationEffect()
    {
        if (!m_BloodHZ)
        {
            ApplyStaminaOnlyDesaturation();
            return;
        }

        float bloodLevel = Math.InverseLerp(m_fDesaturationEnd, m_fDesaturationStart, m_BloodHZ.GetHealthScaled());
        bloodLevel = Math.Clamp(bloodLevel, 0.0, 1.0);

        float staminaSaturation = GetStaminaSaturation();
        float combined = bloodLevel;
        if (staminaSaturation < combined)
            combined = staminaSaturation;

        s_fSaturation = Math.Clamp(combined, 0.0, 1.0);
        s_bEnableSaturation = s_fSaturation < 1.0;
    }

    protected float GetStaminaSaturation()
    {
        if (!m_pCharacterEntity)
            return 1.0;

        CharacterStaminaComponent staminaComp = CharacterStaminaComponent.Cast(m_pCharacterEntity.FindComponent(CharacterStaminaComponent));
        if (!staminaComp)
            return 1.0;

        float rawStamina = staminaComp.GetStamina();
        s_fSmoothedStaminaForEffect = Math.Lerp(s_fSmoothedStaminaForEffect, rawStamina, STAMINA_EFFECT_SMOOTH_ALPHA);
        float t = Math.InverseLerp(m_fStaminaDesaturationEnd, m_fStaminaDesaturationStart, s_fSmoothedStaminaForEffect);
        return Math.Clamp(t, 0.0, 1.0);
    }

    protected void ApplyStaminaOnlyDesaturation()
    {
        float staminaSaturation = GetStaminaSaturation();
        s_fSaturation = Math.Clamp(staminaSaturation, 0.0, 1.0);
        s_bEnableSaturation = s_fSaturation < 1.0;
    }
}
