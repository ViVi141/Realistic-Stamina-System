//! v5 双池体力状态（有氧引擎条 + 无氧 RSS 自有）

class SCR_RSS_StaminaState
{
    protected float m_fAerobicStamina;
    protected float m_fAnaerobicBurst;
    protected bool m_bCollapseState;

    void SCR_RSS_StaminaState()
    {
        m_fAerobicStamina = 1.0;
        m_fAnaerobicBurst = 1.0;
        m_bCollapseState = false;
    }

    float GetAerobic()
    {
        return m_fAerobicStamina;
    }

    float GetAnaerobic()
    {
        return m_fAnaerobicBurst;
    }

    //! W′ 池归一化储量（0–1）；与 GetAnaerobic 同义
    float GetWPrimePool01()
    {
        return m_fAnaerobicBurst;
    }

    bool GetCollapse()
    {
        return m_bCollapseState;
    }

    void SetAerobic(float value)
    {
        m_fAerobicStamina = Math.Clamp(value, 0.0, 1.0);
        if (m_fAerobicStamina > SCR_RSS_Constants.V5_COLLAPSE_AEROBIC_EXIT)
            m_bCollapseState = false;
        if (m_fAerobicStamina <= SCR_RSS_Constants.V5_COLLAPSE_AEROBIC_ENTER)
            m_bCollapseState = true;
    }

    void SetAnaerobic(float value)
    {
        m_fAnaerobicBurst = Math.Clamp(value, 0.0, 1.0);
    }

    //! W′ 池归一化储量写入；与 SetAnaerobic 同义
    void SetWPrimePool01(float value)
    {
        SetAnaerobic(value);
    }

    void SetAnaerobicFromCpModel(SCR_RSS_CriticalPowerModel cpModel)
    {
        SetWPrimePoolFromCpModel(cpModel);
    }

    //! 从 CP 模型同步 W′ 池归一化储量
    void SetWPrimePoolFromCpModel(SCR_RSS_CriticalPowerModel cpModel)
    {
        if (cpModel)
            m_fAnaerobicBurst = cpModel.GetPool01();
    }

    bool IsSprintAllowed()
    {
        if (m_bCollapseState)
            return false;
        if (m_fAnaerobicBurst <= SCR_RSS_ConfigBridge.GetWPrimeSprintEnableThreshold())
            return false;
        if (m_fAerobicStamina < SCR_RSS_ConfigBridge.GetSprintEnableThreshold())
            return false;
        return true;
    }

    bool IsSprintAllowedWithCp(SCR_RSS_CriticalPowerModel cpModel, float worldTimeSec)
    {
        if (!cpModel)
            return IsSprintAllowed();
        return cpModel.IsSprintAllowed(m_fAerobicStamina, m_bCollapseState, worldTimeSec);
    }
}
