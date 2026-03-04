// 调试用：让中毒屏幕效果任何时候都默认开启并显示，便于查看毒效画面。
// 无中毒时使用固定遮罩强度；有中毒时仍按原逻辑。用完后可删除此 mod 或关闭下方开关。

modded class SCR_PoisonScreenEffect
{
    [Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "是否强制始终显示毒效画面（1=开，调试用；0=关，恢复原样）")]
    protected bool m_bForceAlwaysShowPoisonEffect;

    override void DisplayControlledEntityChanged(IEntity from, IEntity to)
    {
        super.DisplayControlledEntityChanged(from, to);

        if (!m_bForceAlwaysShowPoisonEffect)
            return;

        ChimeraCharacter character = ChimeraCharacter.Cast(to);
        if (!character)
            return;

        if (!m_wPoisonEffect1 || !m_wPoisonEffect2)
            return;

        SetEnabled(true);
        StartEffects();
    }

    override protected void DisplayUpdate(IEntity owner, float timeSlice)
    {
        if (!m_bForceAlwaysShowPoisonEffect)
        {
            super.DisplayUpdate(owner, timeSlice);
            return;
        }

        if (!m_wPoisonEffect1 || !m_wPoisonEffect2)
            return;

        float newMaskProgress;

        if (!m_DamageEffects || m_DamageEffects.IsEmpty())
            newMaskProgress = Math.Clamp((m_fMinEnabledAlpha + m_fMaxEnabledAlpha) * 0.5, m_fMinEnabledAlpha, m_fMaxEnabledAlpha);
        else
        {
            newMaskProgress = Math.Max(m_wPoisonEffect1.GetMaskProgress(), m_fMinEnabledAlpha);
            EvaluateDamageEffects(newMaskProgress);
        }

        newMaskProgress = Math.Clamp(Math.Lerp(m_wPoisonEffect1.GetMaskProgress(), newMaskProgress, timeSlice), m_fMinEnabledAlpha, m_fMaxEnabledAlpha);

        m_wPoisonEffect1.SetMaskProgress(newMaskProgress);
        m_wPoisonEffect2.SetMaskProgress(newMaskProgress);

        if (!AnimateWidget.IsAnimating(m_wPoisonEffect1))
            UpdateAnimation(m_wPoisonEffect1);
        if (!AnimateWidget.IsAnimating(m_wPoisonEffect2))
            UpdateAnimation(m_wPoisonEffect2);
    }
}
