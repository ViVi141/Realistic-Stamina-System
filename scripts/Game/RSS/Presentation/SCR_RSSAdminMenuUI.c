// RSS Admin Settings Menu — 嵌入到 SettingsSuperMenu 的 Tab 中
// 与 Video / Audio / Interface 同一层级
// 通过 modded SCR_SettingsSuperMenu 注入

class SCR_RSSAdminMenuUI
{
    protected static ref SCR_RSSAdminMenuUI s_Instance;

    protected Widget m_wRoot;
    protected TextWidget m_wCurrentPresetLabel;
    protected TextWidget m_wStatusText;
    protected CheckBoxWidget m_wChkDebug;
    protected CheckBoxWidget m_wChkHUD;
    protected CheckBoxWidget m_wChkDataExport;
    protected CheckBoxWidget m_wChkMudSlip;
    protected CheckBoxWidget m_wChkAICombat;
    protected CheckBoxWidget m_wChkDisableAI;
    protected CheckBoxWidget m_wChkDisableAIStamina;

    //------------------------------------------------------------------------------------------------
    // 当 TabView 创建 RSS 标签内容时调用（由 SCR_RSSSettingsTab 触发）
    static void OnTabCreated(SCR_TabViewComponent tabView)
    {
        if (s_Instance)
            return;

        SCR_RSSAdminMenuUI inst = new SCR_RSSAdminMenuUI();
        if (!inst.BindTabContent(tabView))
            return;
        s_Instance = inst;
    }

    //------------------------------------------------------------------------------------------------
    // 绑定到 TabView 中已创建的 RSS 标签内容
    protected bool BindTabContent(SCR_TabViewComponent tabView)
    {
        // 查找 RSS 标签页的内容 widget
        if (!tabView)
            return false;

        array<ref SCR_TabViewContent> contents = tabView.GetContents();
        if (!contents)
            return false;

        for (int i = 0; i < contents.Count(); i++)
        {
            SCR_TabViewContent content = contents[i];
            if (content.m_sTabIdentifier == "RSSSettings" && content.m_wTab)
            {
                m_wRoot = content.m_wTab;
                break;
            }
        }

        if (!m_wRoot)
            return false;

        // 查找控件
        m_wCurrentPresetLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("CurrentPresetLabel"));
        m_wStatusText = TextWidget.Cast(m_wRoot.FindAnyWidget("StatusText"));
        m_wChkDebug = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkDebug"));
        m_wChkHUD = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkHUD"));
        m_wChkDataExport = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkDataExport"));
        m_wChkMudSlip = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkMudSlip"));
        // MudSlip checkbox visible; admin can toggle in settings
        m_wChkAICombat = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkAICombat"));
        m_wChkDisableAI = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkDisableAI"));
        m_wChkDisableAIStamina = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("ChkDisableAIStamina"));

        // 绑定预设按钮
        SCR_ButtonTextComponent btn;
        btn = SCR_ButtonTextComponent.GetButtonText("BtnElite", m_wRoot);
        if (btn)
        {
            btn.m_OnClicked.Insert(OnPresetElite);
        }
        btn = SCR_ButtonTextComponent.GetButtonText("BtnStandard", m_wRoot);
        if (btn)
        {
            btn.m_OnClicked.Insert(OnPresetStandard);
        }
        btn = SCR_ButtonTextComponent.GetButtonText("BtnTactical", m_wRoot);
        if (btn)
        {
            btn.m_OnClicked.Insert(OnPresetTactical);
        }
        btn = SCR_ButtonTextComponent.GetButtonText("BtnCustom", m_wRoot);
        if (btn)
        {
            btn.m_OnClicked.Insert(OnPresetCustom);
        }

        // 绑定操作按钮
        btn = SCR_ButtonTextComponent.GetButtonText("BtnReload", m_wRoot);
        if (btn)
        {
            btn.m_OnClicked.Insert(OnReloadConfig);
        }
        btn = SCR_ButtonTextComponent.GetButtonText("BtnSave", m_wRoot);
        if (btn)
        {
            btn.m_OnClicked.Insert(OnSaveAndSync);
        }

        // 初始化复选框状态
        RefreshCheckboxes();

        // 刷新 UI
        RefreshUI();

        return true;
    }

    //------------------------------------------------------------------------------------------------
    // 刷新复选框状态
    protected void RefreshCheckboxes()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings)
        {
            return;
        }

        if (m_wChkDebug)
        {
            m_wChkDebug.SetChecked(settings.m_bDebugLogEnabled);
        }
        if (m_wChkHUD)
        {
            m_wChkHUD.SetChecked(settings.m_bHintDisplayEnabled);
        }
        if (m_wChkDataExport)
        {
            m_wChkDataExport.SetChecked(settings.m_bDataExportEnabled);
        }
        if (m_wChkMudSlip)
        {
            m_wChkMudSlip.SetChecked(settings.m_bEnableMudSlipMechanism);
        }
        if (m_wChkAICombat)
        {
            m_wChkAICombat.SetChecked(settings.m_bEnableAIStaminaCombatEffects);
        }
        if (m_wChkDisableAI)
        {
            m_wChkDisableAI.SetChecked(settings.m_bDisableAIAllCalc);
        }
        if (m_wChkDisableAIStamina)
        {
            m_wChkDisableAIStamina.SetChecked(settings.m_bDisableAIStaminaCalc);
        }
    }

    //------------------------------------------------------------------------------------------------
    // 刷新标签显示
    protected void RefreshUI()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings)
        {
            return;
        }

        string preset = settings.m_sSelectedPreset;
        if (!preset || preset == "")
            preset = "StandardMilsim";

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
        if (!settings)
        {
            return;
        }

        if (m_wChkDebug)
        {
            settings.m_bDebugLogEnabled = m_wChkDebug.IsChecked();
        }
        if (m_wChkHUD)
        {
            settings.m_bHintDisplayEnabled = m_wChkHUD.IsChecked();
        }
        if (m_wChkDataExport)
        {
            settings.m_bDataExportEnabled = m_wChkDataExport.IsChecked();
        }
        if (m_wChkMudSlip)
        {
            settings.m_bEnableMudSlipMechanism = m_wChkMudSlip.IsChecked();
        }
        if (m_wChkAICombat)
        {
            settings.m_bEnableAIStaminaCombatEffects = m_wChkAICombat.IsChecked();
        }
        if (m_wChkDisableAI)
        {
            settings.m_bDisableAIAllCalc = m_wChkDisableAI.IsChecked();
        }
        if (m_wChkDisableAIStamina)
        {
            settings.m_bDisableAIStaminaCalc = m_wChkDisableAIStamina.IsChecked();
        }
    }

    //------------------------------------------------------------------------------------------------
    protected void TriggerReplication()
    {
        if (!GetGame())
        {
            return;
        }

        SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
        if (gameMode)
            gameMode.RSS_ReplicateConfigNow();

        SCR_RSS_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //! 通过本地玩家控制器的 RPC 将配置推送到服务端
    protected void SendConfigToServer(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        IEntity player = SCR_PlayerController.GetLocalControlledEntity();
        if (!player)
        {
            return;
        }

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            player.FindComponent(SCR_CharacterControllerComponent));
        if (ctrl)
            ctrl.RSS_RequestAdminConfigUpdate(preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    protected bool IsChkChecked(CheckBoxWidget chk)
    {
        if (!chk)
        {
            return false;
        }
        return chk.IsChecked();
    }

    //------------------------------------------------------------------------------------------------
    protected void OnPresetElite()   { ApplyPreset("EliteStandard", true);  }
    protected void OnPresetStandard(){ ApplyPreset("StandardMilsim", true); }
    protected void OnPresetTactical() { ApplyPreset("TacticalAction", true); }
    protected void OnPresetCustom()   { ApplyPreset("Custom", false); }

    protected void ApplyPreset(string preset, bool forceRefresh)
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings)
        {
            return;
        }

        if (Replication.IsServer())
        {
            // 监听服务器：直接修改 + 保存 + 复制
            settings.m_sSelectedPreset = preset;
            settings.InitPresets(forceRefresh);
            SaveSwitchesToSettings();
            SCR_RSS_ConfigManager.Save();
            TriggerReplication();
            RefreshUI();
            SetStatus("Preset: " + preset + " — applied & synced");
        }
        else
        {
            // 专用服务器管理员客户端：临时更新本地显示 + RPC 推送
            settings.m_sSelectedPreset = preset;
            settings.InitPresets(forceRefresh);
            SaveSwitchesToSettings();
            SendConfigToServer(preset, IsChkChecked(m_wChkDebug), IsChkChecked(m_wChkHUD),
                IsChkChecked(m_wChkDataExport), IsChkChecked(m_wChkMudSlip),
                IsChkChecked(m_wChkAICombat), IsChkChecked(m_wChkDisableAI),
                IsChkChecked(m_wChkDisableAIStamina));
            RefreshUI();
            SetStatus("Preset: " + preset + " — sent to server");
        }
    }

    protected void OnReloadConfig()
    {
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Reload();
            TriggerReplication();
            RefreshUI();
            RefreshCheckboxes();
            SetStatus("Config reloaded from disk & synced");
        }
        else
        {
            SetStatus("Only server can reload config");
        }
    }

    protected void OnSaveAndSync()
    {
        SaveSwitchesToSettings();

        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Save();
            TriggerReplication();
            SetStatus("Settings saved & synced to all clients");
        }
        else
        {
            // 专用服务器管理员客户端：通过 RPC 推送到服务端
            SendConfigToServer("", IsChkChecked(m_wChkDebug), IsChkChecked(m_wChkHUD),
                IsChkChecked(m_wChkDataExport), IsChkChecked(m_wChkMudSlip),
                IsChkChecked(m_wChkAICombat), IsChkChecked(m_wChkDisableAI),
                IsChkChecked(m_wChkDisableAIStamina));
            SetStatus("Settings sent to server & syncing to all clients");
        }
    }
}
