// RSS Settings SubMenu — 嵌入 SettingsSuperMenu 的标签页
// 不经过游戏设置系统，直接读写 SCR_RSS_ConfigManager
class SCR_RSSSettingsSubMenu : SCR_SettingsSubMenuBase
{
    protected SCR_ComboBoxComponent m_wPresetSelector;
    protected SCR_SpinBoxComponent m_wDebugToggle;
    protected SCR_SpinBoxComponent m_wHUDToggle;
    protected SCR_SpinBoxComponent m_wMudSlipToggle;
    protected SCR_SpinBoxComponent m_wAICombatToggle;

    //------------------------------------------------------------------------------------------------
    override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
    {
        super.OnTabCreate(menuRoot, buttonsLayout, index);

        m_wPresetSelector    = FindComboBox("PresetSelector");
        m_wDebugToggle       = FindSpinBox("ToggleDebug");
        m_wHUDToggle         = FindSpinBox("ToggleHUD");
        m_wMudSlipToggle     = FindSpinBox("ToggleMudSlip");
        m_wAICombatToggle    = FindSpinBox("ToggleAICombat");

        // 预设下拉框 Change 事件
        if (m_wPresetSelector)
            m_wPresetSelector.m_OnChanged.Insert(OnPresetChanged);

        // 从 RSS 配置加载当前值到控件
        LoadFromSettings();
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabShow()
    {
        super.OnTabShow();
        LoadFromSettings();
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabHide()
    {
        super.OnTabHide();
        SaveToSettings();
        ApplyAndSync();
    }

    //------------------------------------------------------------------------------------------------
    protected void LoadFromSettings()
    {
        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        // 预设下拉框
        if (m_wPresetSelector)
        {
            string preset = s.m_sSelectedPreset;
            if (!preset || preset == "") preset = "StandardMilsim";
            int idx = 1;
            if (preset == "EliteStandard")   idx = 0;
            else if (preset == "StandardMilsim") idx = 1;
            else if (preset == "TacticalAction") idx = 2;
            else if (preset == "Custom")     idx = 3;
            m_wPresetSelector.SetCurrentItem(idx, false, false);
        }

        // 开关控件
        SetSpin(m_wDebugToggle,      s.m_bDebugLogEnabled);
        SetSpin(m_wHUDToggle,        s.m_bHintDisplayEnabled);
        SetSpin(m_wMudSlipToggle,    s.m_bEnableMudSlipMechanism);
        SetSpin(m_wAICombatToggle,   s.m_bEnableAIStaminaCombatEffects);
    }

    //------------------------------------------------------------------------------------------------
    protected void SaveToSettings()
    {
        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        s.m_bDebugLogEnabled               = GetSpin(m_wDebugToggle);
        s.m_bHintDisplayEnabled            = GetSpin(m_wHUDToggle);
        s.m_bEnableMudSlipMechanism        = GetSpin(m_wMudSlipToggle);
        s.m_bEnableAIStaminaCombatEffects  = GetSpin(m_wAICombatToggle);
    }

    //------------------------------------------------------------------------------------------------
    protected void OnPresetChanged(SCR_ComboBoxComponent combo, int index)
    {
        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        string preset;
        if (index == 0)       preset = "EliteStandard";
        else if (index == 1)  preset = "StandardMilsim";
        else if (index == 2)  preset = "TacticalAction";
        else                  preset = "Custom";

        if (preset == s.m_sSelectedPreset) return;

        s.m_sSelectedPreset = preset;
        s.InitPresets(preset != "Custom");
        LoadFromSettings();
        ApplyAndSync();
    }

    //------------------------------------------------------------------------------------------------
    protected SCR_RSS_Settings GetSettings()
    {
        return SCR_RSS_ConfigManager.GetSettings();
    }

    //------------------------------------------------------------------------------------------------
    protected void ApplyAndSync()
    {
        // Save() 内部已调用 ReplicateConfigToClients → RSS_ReplicateConfigNow
        SCR_RSS_ConfigManager.Save();
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //------------------------------------------------------------------------------------------------
    protected void SetSpin(SCR_SpinBoxComponent spin, bool value)
    {
        if (!spin) return;
        int idx = 0;
        if (value) idx = 1;
        spin.SetCurrentItem(idx, false, false);
    }

    protected bool GetSpin(SCR_SpinBoxComponent spin)
    {
        if (!spin) return false;
        return spin.GetCurrentIndex() != 0;
    }

    protected SCR_SpinBoxComponent FindSpinBox(string name)
    {
        Widget w = m_wRoot.FindAnyWidget(name);
        if (!w) return null;
        return SCR_SpinBoxComponent.Cast(w.FindHandler(SCR_SpinBoxComponent));
    }

    protected SCR_ComboBoxComponent FindComboBox(string name)
    {
        Widget w = m_wRoot.FindAnyWidget(name);
        if (!w) return null;
        return SCR_ComboBoxComponent.Cast(w.FindHandler(SCR_ComboBoxComponent));
    }
}
