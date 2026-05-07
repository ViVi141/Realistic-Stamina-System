// RSS Settings SubMenu — 嵌入 SettingsSuperMenu 的标签页
// 继承 SCR_SettingsSubMenuBase，使用游戏标准控件

class SCR_RSSSettingsSubMenu : SCR_SettingsSubMenuBase
{
    protected SCR_ComboBoxComponent m_wPresetSelector;
    protected SCR_SpinBoxComponent m_wDebugToggle;
    protected SCR_SpinBoxComponent m_wHUDToggle;
    protected SCR_SpinBoxComponent m_wDataExportToggle;
    protected SCR_SpinBoxComponent m_wMudSlipToggle;
    protected SCR_SpinBoxComponent m_wAICombatToggle;

    //------------------------------------------------------------------------------------------------
    override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
    {
        super.OnTabCreate(menuRoot, buttonsLayout, index);

        m_wPresetSelector    = FindSpinOrCombo("PresetSelector");
        m_wDebugToggle       = FindSpinBox("ToggleDebug");
        m_wHUDToggle         = FindSpinBox("ToggleHUD");
        m_wDataExportToggle  = FindSpinBox("ToggleDataExport");
        m_wMudSlipToggle     = FindSpinBox("ToggleMudSlip");
        m_wAICombatToggle    = FindSpinBox("ToggleAICombat");

        // 预设下拉框 Change 事件
        if (m_wPresetSelector)
            m_wPresetSelector.m_OnChanged.Insert(OnPresetChanged);

        // 使用标准的 SettingsBinding 系统自动保存（游戏内置机制）
        m_aSettingsBindings.Clear();
        if (m_wDebugToggle)
            m_aSettingsBindings.Insert(new SCR_SettingBindingRSS("RSS", "m_bDebugLogEnabled",         "ToggleDebug"));
        if (m_wHUDToggle)
            m_aSettingsBindings.Insert(new SCR_SettingBindingRSS("RSS", "m_bHintDisplayEnabled",       "ToggleHUD"));
        if (m_wDataExportToggle)
            m_aSettingsBindings.Insert(new SCR_SettingBindingRSS("RSS", "m_bDataExportEnabled",        "ToggleDataExport"));
        if (m_wMudSlipToggle)
            m_aSettingsBindings.Insert(new SCR_SettingBindingRSS("RSS", "m_bEnableMudSlipMechanism",   "ToggleMudSlip"));
        if (m_wAICombatToggle)
            m_aSettingsBindings.Insert(new SCR_SettingBindingRSS("RSS", "m_bEnableAIStaminaCombatEffects", "ToggleAICombat"));

        LoadSettings();
        RefreshPresetUI();
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabShow()
    {
        super.OnTabShow();
        RefreshPresetUI();
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabHide()
    {
        super.OnTabHide();
        ApplyAndSync();
    }

    //------------------------------------------------------------------------------------------------
    protected void RefreshPresetUI()
    {
        if (!m_wPresetSelector) return;

        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        string preset = s.m_sSelectedPreset;
        if (!preset || preset == "") preset = "StandardMilsim";

        int idx = 1; // default StandardMilsim
        if (preset == "EliteStandard")   idx = 0;
        if (preset == "StandardMilsim")  idx = 1;
        if (preset == "TacticalAction")  idx = 2;
        if (preset == "Custom")          idx = 3;

        m_wPresetSelector.SetCurrentItem(idx, false, false);
    }

    //------------------------------------------------------------------------------------------------
    protected void OnPresetChanged(SCR_ComboBoxComponent combo, int index)
    {
        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        string preset;
        switch (index)
        {
            case 0: preset = "EliteStandard"; break;
            case 1: preset = "StandardMilsim"; break;
            case 2: preset = "TacticalAction"; break;
            case 3: preset = "Custom"; break;
        }

        if (preset == s.m_sSelectedPreset) return;

        s.m_sSelectedPreset = preset;
        s.InitPresets(preset != "Custom");
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
        SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
        if (gameMode) gameMode.RSS_ReplicateConfigNow();
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //------------------------------------------------------------------------------------------------
    protected SCR_SpinBoxComponent FindSpinBox(string name)
    {
        Widget w = m_wRoot.FindAnyWidget(name);
        if (!w) return null;
        return SCR_SpinBoxComponent.Cast(w.FindHandler(SCR_SpinBoxComponent));
    }

    protected SCR_ComboBoxComponent FindSpinOrCombo(string name)
    {
        Widget w = m_wRoot.FindAnyWidget(name);
        if (!w) return null;
        SCR_ComboBoxComponent c = SCR_ComboBoxComponent.Cast(w.FindHandler(SCR_ComboBoxComponent));
        if (c) return c;
        return null;
    }
}

// 简单的 SettingBinding：将 SCR_SpinBox 的 On/Off 状态映射到 RSS 配置
class SCR_SettingBindingRSS : SCR_SettingsBindingBase
{
    protected string m_sSettingName;

    void SCR_SettingBindingRSS(string module, string settingName, string widgetName)
    {
        m_sParentModule = module;
        m_sSettingName  = settingName;
        m_sWidgetName   = widgetName;
    }

    override void SaveSetting()
    {
        Widget w = m_wBoundWidget;
        if (!w) return;

        SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(w.FindHandler(SCR_SpinBoxComponent));
        if (!spin) return;

        SCR_RSS_Settings s = SCR_RSS_ConfigManager.GetSettings();
        if (!s) return;

        bool value = spin.GetCurrentIndex() != 0;

        switch (m_sSettingName)
        {
            case "m_bDebugLogEnabled":               s.m_bDebugLogEnabled = value; break;
            case "m_bHintDisplayEnabled":             s.m_bHintDisplayEnabled = value; break;
            case "m_bDataExportEnabled":              s.m_bDataExportEnabled = value; break;
            case "m_bEnableMudSlipMechanism":         s.m_bEnableMudSlipMechanism = value; break;
            case "m_bEnableAIStaminaCombatEffects":   s.m_bEnableAIStaminaCombatEffects = value; break;
        }
    }

    override void LoadSetting()
    {
        SCR_RSS_Settings s = SCR_RSS_ConfigManager.GetSettings();
        if (!s) return;

        bool value = false;
        switch (m_sSettingName)
        {
            case "m_bDebugLogEnabled":               value = s.m_bDebugLogEnabled; break;
            case "m_bHintDisplayEnabled":             value = s.m_bHintDisplayEnabled; break;
            case "m_bDataExportEnabled":              value = s.m_bDataExportEnabled; break;
            case "m_bEnableMudSlipMechanism":         value = s.m_bEnableMudSlipMechanism; break;
            case "m_bEnableAIStaminaCombatEffects":   value = s.m_bEnableAIStaminaCombatEffects; break;
        }

        Widget w = m_wBoundWidget;
        if (!w) return;

        SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(w.FindHandler(SCR_SpinBoxComponent));
        if (spin)
            spin.SetCurrentItem(value ? 1 : 0, false, false);
    }
}
