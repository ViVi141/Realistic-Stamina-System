// RSS：在官方恢复色差基础上，在「冲刺冷却即将完成 / 体力即将恢复到良好」时触发一次轻微色差
// 保留原有 HEALING 触发；体力触发默认关闭（观感易被误认为边缘模糊），需时可在编辑器开启。

modded class SCR_RegenerationScreenEffect
{
    [Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "是否启用体力/冷却触发的色差（0=关，观感更干净；1=开）")]
    protected bool m_bStaminaRegenEffectEnabled;

    [Attribute(defvalue: "2.0", uiwidget: UIWidgets.EditBox, desc: "冲刺冷却剩余秒数低于此值时触发（即将完成）")]
    protected float m_fStaminaRegenCooldownSecondsLeft;

    [Attribute(defvalue: "0.5", uiwidget: UIWidgets.EditBox, desc: "体力高于此值视为「良好」；从低于此值恢复到此时触发")]
    protected float m_fStaminaGoodThreshold;

    [Attribute(defvalue: "4.0", uiwidget: UIWidgets.EditBox, desc: "体力恢复触发的色差持续秒数")]
    protected float m_fStaminaRegenEffectDuration;

    [Attribute(defvalue: "0.6", uiwidget: UIWidgets.EditBox, desc: "体力恢复时色差最大强度比例 (0~1)，相对材质上限 0.05")]
    protected float m_fStaminaRegenChromAberScale;

    [Attribute(defvalue: "20.0", uiwidget: UIWidgets.EditBox, desc: "体力恢复效果触发后，此秒数内不再触发")]
    protected float m_fStaminaRegenTriggerCooldownSec;

    protected float m_fLastStaminaForRegen = -1.0;
    protected float m_fStaminaRegenTriggerCooldownUntil = -1.0;
    protected bool m_bStaminaRegenEffectActive = false;
    protected float m_fStaminaRegenTimeRemaining = 0.0;
    protected float m_fStaminaRegenChromAberMax = 0.03;

    override void DisplayControlledEntityChanged(IEntity from, IEntity to)
    {
        super.DisplayControlledEntityChanged(from, to);
        m_fLastStaminaForRegen = -1.0;
        m_fStaminaRegenTriggerCooldownUntil = -1.0;
        m_bStaminaRegenEffectActive = false;
        m_fStaminaRegenTimeRemaining = 0.0;
    }

    override void UpdateEffect(float timeSlice)
    {
        if (m_bRegenerationEffect)
        {
            RegenerationEffect(timeSlice);
            return;
        }

        if (m_bStaminaRegenEffectActive)
        {
            UpdateStaminaRegenEffect(timeSlice);
            return;
        }

        TryTriggerStaminaRegenEffect(timeSlice);
    }

    protected void TryTriggerStaminaRegenEffect(float timeSlice)
    {
        if (!m_bStaminaRegenEffectEnabled || !m_pCharacterEntity)
            return;

        float now = GetGame().GetWorld().GetWorldTime() / 1000.0;
        if (m_fStaminaRegenTriggerCooldownUntil > 0.0 && now < m_fStaminaRegenTriggerCooldownUntil)
            return;

        CharacterStaminaComponent staminaComp = CharacterStaminaComponent.Cast(m_pCharacterEntity.FindComponent(CharacterStaminaComponent));
        if (!staminaComp)
            return;

        SCR_CharacterControllerComponent rssController = SCR_CharacterControllerComponent.Cast(m_pCharacterEntity.FindComponent(SCR_CharacterControllerComponent));
        float stamina = staminaComp.GetStamina();
        bool trigger = false;

        if (rssController)
        {
            float cooldownUntil = rssController.GetSprintCooldownUntil();
            if (cooldownUntil >= 0.0)
            {
                float secLeft = cooldownUntil - now;
                if (secLeft > 0.0 && secLeft <= m_fStaminaRegenCooldownSecondsLeft)
                    trigger = true;
            }
        }

        if (!trigger && m_fLastStaminaForRegen >= 0.0)
        {
            if (m_fLastStaminaForRegen < m_fStaminaGoodThreshold && stamina >= m_fStaminaGoodThreshold)
                trigger = true;
        }
        m_fLastStaminaForRegen = stamina;

        if (!trigger)
            return;

        m_fStaminaRegenTriggerCooldownUntil = now + m_fStaminaRegenTriggerCooldownSec;
        m_bStaminaRegenEffectActive = true;
        m_fStaminaRegenTimeRemaining = m_fStaminaRegenEffectDuration;
        m_fStaminaRegenChromAberMax = 0.05 * Math.Clamp(m_fStaminaRegenChromAberScale, 0.0, 1.0);
    }

    protected void UpdateStaminaRegenEffect(float timeSlice)
    {
        m_fStaminaRegenTimeRemaining -= timeSlice;

        float chromAberProgress = Math.InverseLerp(m_fStaminaRegenEffectDuration, 0.0, m_fStaminaRegenTimeRemaining);
        chromAberProgress = Math.Clamp(chromAberProgress, 0.0, 1.0);

        vector chromAberPowerScaled = LegacyCurve.Curve(ECurveType.CatmullRom, chromAberProgress, m_Curve);
        s_fChromAberPower = Math.Lerp(0.0, m_fStaminaRegenChromAberMax, chromAberPowerScaled[1]);

        if (m_fStaminaRegenTimeRemaining <= 0.0)
        {
            m_bStaminaRegenEffectActive = false;
            m_fStaminaRegenTimeRemaining = 0.0;
            s_fChromAberPower = 0.0;
        }
    }

    override protected void RegenerationEffect(float timeSlice)
    {
        m_bStaminaRegenEffectActive = false;
        m_fStaminaRegenTimeRemaining = 0.0;
        super.RegenerationEffect(timeSlice);
    }

    override protected void ClearEffects()
    {
        super.ClearEffects();
        m_bStaminaRegenEffectActive = false;
        m_fStaminaRegenTimeRemaining = 0.0;
    }
}
