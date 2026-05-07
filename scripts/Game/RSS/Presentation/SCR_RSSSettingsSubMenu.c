// RSS Settings SubMenu
//   所有人可见：HUD 本地开关
//   管理员额外：预设选择 + 服务器开关 + "应用 HUD 到服务器"按钮

class SCR_RSSSettingsSubMenu : SCR_SettingsSubMenuBase
{
    protected SCR_ComboBoxComponent m_wPresetSelector;
    protected SCR_SpinBoxComponent m_wHUDToggle;
    protected SCR_SpinBoxComponent m_wDebugToggle;
    protected SCR_SpinBoxComponent m_wMudSlipToggle;
    protected SCR_SpinBoxComponent m_wAICombatToggle;

    protected bool m_bIsAdmin;

    //------------------------------------------------------------------------------------------------
    override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
    {
        super.OnTabCreate(menuRoot, buttonsLayout, index);

        m_bIsAdmin = IsPlayerAdmin();

        m_wPresetSelector  = FindComboBox("PresetSelector");
        m_wHUDToggle       = FindSpinBox("ToggleHUD");
        m_wDebugToggle     = FindSpinBox("ToggleDebug");
        m_wMudSlipToggle   = FindSpinBox("ToggleMudSlip");
        m_wAICombatToggle  = FindSpinBox("ToggleAICombat");

        // 预设下拉框 Change 事件
        if (m_wPresetSelector)
            m_wPresetSelector.m_OnChanged.Insert(OnPresetChanged);

        // 管理员额外：绑定 "Set Server HUD" 按钮
        if (m_bIsAdmin)
        {
            SCR_ButtonTextComponent btn = SCR_ButtonTextComponent.GetButtonText("BtnServerHUD", m_wRoot);
            if (btn) btn.m_OnClicked.Insert(OnSetServerHUD);
        }

        // 根据权限隐藏/显示控件
        UpdateVisibility();
        LoadFromSettings();
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabShow()
    {
        super.OnTabShow();
        UpdateVisibility();
        LoadFromSettings();
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabHide()
    {
        super.OnTabHide();
        if (m_bIsAdmin)
            ApplyServerChanges();
    }

    //------------------------------------------------------------------------------------------------
    protected void UpdateVisibility()
    {
        // 管理员区域（预设、服务器开关）可见性
        HideWidget("TitlePreset",    !m_bIsAdmin);
        HideWidget("PresetSelector", !m_bIsAdmin);
        HideWidget("TitleToggles",   !m_bIsAdmin);
        HideWidget("ToggleDebug",    !m_bIsAdmin);
        HideWidget("ToggleMudSlip",  !m_bIsAdmin);
        HideWidget("ToggleAICombat", !m_bIsAdmin);
        HideWidget("BtnServerHUD",   !m_bIsAdmin);
    }

    //------------------------------------------------------------------------------------------------
    protected void LoadFromSettings()
    {
        // HUD：读取本地覆盖（或服务器默认）
        SetSpin(m_wHUDToggle, SCR_StaminaHUDComponent.GetLocalHUDEnabled());

        if (!m_bIsAdmin) return;

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

        SetSpin(m_wDebugToggle,    s.m_bDebugLogEnabled);
        SetSpin(m_wMudSlipToggle,  s.m_bEnableMudSlipMechanism);
        SetSpin(m_wAICombatToggle, s.m_bEnableAIStaminaCombatEffects);
    }

    //------------------------------------------------------------------------------------------------
    protected void ApplyServerChanges()
    {
        if (!m_bIsAdmin) return;

        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        s.m_bDebugLogEnabled              = GetSpin(m_wDebugToggle);
        s.m_bEnableMudSlipMechanism       = GetSpin(m_wMudSlipToggle);
        s.m_bEnableAIStaminaCombatEffects = GetSpin(m_wAICombatToggle);

        SCR_RSS_ConfigManager.Save();
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //------------------------------------------------------------------------------------------------
    // HUD 开关变化时，立即更新本地覆盖（不需要点保存）
    override void OnTabHide()
    {
        super.OnTabHide();

        // 保存本地 HUD 状态
        SCR_StaminaHUDComponent.SetLocalHUDEnabled(GetSpin(m_wHUDToggle));

        if (m_bIsAdmin)
            ApplyServerChanges();
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
        SCR_RSS_ConfigManager.Save();
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
        LoadFromSettings();
    }

    //------------------------------------------------------------------------------------------------
    // 管理员：将当前本地 HUD 状态推送到服务器默认
    protected void OnSetServerHUD()
    {
        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        s.m_bHintDisplayEnabled = GetSpin(m_wHUDToggle);
        SCR_RSS_ConfigManager.Save();
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //------------------------------------------------------------------------------------------------
    protected SCR_RSS_Settings GetSettings()
    {
        return SCR_RSS_ConfigManager.GetSettings();
    }

    //------------------------------------------------------------------------------------------------
    protected bool IsPlayerAdmin()
    {
        int playerId = GetGame().GetPlayerController().GetPlayerId();
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm) return false;
        return pm.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR)
            || pm.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR)
            || pm.HasPlayerRole(playerId, EPlayerRole.GAME_MASTER);
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

    protected void HideWidget(string name, bool hide)
    {
        Widget w = m_wRoot.FindAnyWidget(name);
        if (w) w.SetVisible(!hide);
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
