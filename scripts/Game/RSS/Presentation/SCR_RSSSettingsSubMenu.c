// RSS Settings SubMenu — 嵌入 SettingsSuperMenu 的标签页
// 布局: UI/layouts/Menus/RSSSettings/RSSSettings.layout
// 继承 SCR_SubMenuBase，由 SCR_SuperMenuComponent 管理生命周期

class SCR_RSSSettingsSubMenu : SCR_SubMenuBase
{
    protected TextWidget m_wCurrentPresetLabel;
    protected TextWidget m_wStatusText;
    protected CheckBoxWidget m_wChkDebug;
    protected CheckBoxWidget m_wChkHUD;
    protected CheckBoxWidget m_wChkDataExport;
    protected CheckBoxWidget m_wChkMudSlip;
    protected CheckBoxWidget m_wChkAICombat;

    //------------------------------------------------------------------------------------------------
    override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
    {
        super.OnTabCreate(menuRoot, buttonsLayout, index);

        // m_wRoot 现在指向布局的根 widget（由 SCR_SubMenuBase.OnTabCreate 设置）
        m_wCurrentPresetLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("CurrentPresetLabel"));
        m_wStatusText = TextWidget.Cast(m_wRoot.FindAnyWidget("StatusText"));
        m_wChkDebug = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkDebug"));
        m_wChkHUD = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkHUD"));
        m_wChkDataExport = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkDataExport"));
        m_wChkMudSlip = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkMudSlip"));
        m_wChkAICombat = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkAICombat"));

        // 绑定预设按钮
        SCR_ButtonTextComponent btn;
        btn = SCR_ButtonTextComponent.GetButtonText("BtnElite", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnPresetElite);
        btn = SCR_ButtonTextComponent.GetButtonText("BtnStandard", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnPresetStandard);
        btn = SCR_ButtonTextComponent.GetButtonText("BtnTactical", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnPresetTactical);
        btn = SCR_ButtonTextComponent.GetButtonText("BtnCustom", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnPresetCustom);

        // 绑定操作按钮
        btn = SCR_ButtonTextComponent.GetButtonText("BtnReload", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnReloadConfig);
        btn = SCR_ButtonTextComponent.GetButtonText("BtnSave", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnSaveAndSync);
    }

    //------------------------------------------------------------------------------------------------
    override void OnTabShow()
    {
        super.OnTabShow();
        RefreshAll();
    }

    //------------------------------------------------------------------------------------------------
    protected void RefreshAll()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;

        // 复选框
        if (m_wChkDebug)    m_wChkDebug.SetChecked(settings.m_bDebugLogEnabled);
        if (m_wChkHUD)      m_wChkHUD.SetChecked(settings.m_bHintDisplayEnabled);
        if (m_wChkDataExport) m_wChkDataExport.SetChecked(settings.m_bDataExportEnabled);
        if (m_wChkMudSlip)  m_wChkMudSlip.SetChecked(settings.m_bEnableMudSlipMechanism);
        if (m_wChkAICombat) m_wChkAICombat.SetChecked(settings.m_bEnableAIStaminaCombatEffects);

        // 预设标签
        string preset = settings.m_sSelectedPreset;
        if (!preset || preset == "") preset = "StandardMilsim";

        if (m_wCurrentPresetLabel)
        {
            m_wCurrentPresetLabel.SetText("Active Preset: " + preset);
            Color c;
            if (preset == "EliteStandard")
                c = Color.FromRGBA(220, 80, 80, 255);
            else if (preset == "StandardMilsim")
                c = Color.FromRGBA(80, 120, 220, 255);
            else if (preset == "TacticalAction")
                c = Color.FromRGBA(80, 200, 80, 255);
            else
                c = Color.FromRGBA(180, 180, 180, 255);
            m_wCurrentPresetLabel.SetColor(c);
        }

        SetStatus("");
    }

    //------------------------------------------------------------------------------------------------
    protected void SetStatus(string text)
    {
        if (m_wStatusText)
            m_wStatusText.SetText(text);
    }

    //------------------------------------------------------------------------------------------------
    protected SCR_RSS_Settings GetSettings()
    {
        return SCR_RSS_ConfigManager.GetSettings();
    }

    //------------------------------------------------------------------------------------------------
    protected void SaveSwitchesToSettings()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;
        if (m_wChkDebug)    settings.m_bDebugLogEnabled = m_wChkDebug.IsChecked();
        if (m_wChkHUD)      settings.m_bHintDisplayEnabled = m_wChkHUD.IsChecked();
        if (m_wChkDataExport) settings.m_bDataExportEnabled = m_wChkDataExport.IsChecked();
        if (m_wChkMudSlip)  settings.m_bEnableMudSlipMechanism = m_wChkMudSlip.IsChecked();
        if (m_wChkAICombat) settings.m_bEnableAIStaminaCombatEffects = m_wChkAICombat.IsChecked();
    }

    //------------------------------------------------------------------------------------------------
    protected void TriggerReplication()
    {
        if (!GetGame()) return;
        SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
        if (gameMode) gameMode.RSS_ReplicateConfigNow();
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //------------------------------------------------------------------------------------------------
    void OnPresetElite()
    {
        SCR_RSS_Settings s = GetSettings(); if (!s) return;
        s.m_sSelectedPreset = "EliteStandard";
        s.InitPresets(true);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshAll();
        SetStatus("EliteStandard — applied & synced");
    }

    void OnPresetStandard()
    {
        SCR_RSS_Settings s = GetSettings(); if (!s) return;
        s.m_sSelectedPreset = "StandardMilsim";
        s.InitPresets(true);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshAll();
        SetStatus("StandardMilsim — applied & synced");
    }

    void OnPresetTactical()
    {
        SCR_RSS_Settings s = GetSettings(); if (!s) return;
        s.m_sSelectedPreset = "TacticalAction";
        s.InitPresets(true);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshAll();
        SetStatus("TacticalAction — applied & synced");
    }

    void OnPresetCustom()
    {
        SCR_RSS_Settings s = GetSettings(); if (!s) return;
        s.m_sSelectedPreset = "Custom";
        s.InitPresets(false);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshAll();
        SetStatus("Custom — applied & synced");
    }

    void OnReloadConfig()
    {
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Reload();
            TriggerReplication();
            RefreshAll();
            SetStatus("Config reloaded & synced");
        }
        else
        {
            SetStatus("Only server can reload config");
        }
    }

    void OnSaveAndSync()
    {
        SaveSwitchesToSettings();
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Save();
            TriggerReplication();
            SetStatus("Saved & synced to all clients");
        }
        else
        {
            SetStatus("Saved locally (server sync required)");
        }
    }
}
