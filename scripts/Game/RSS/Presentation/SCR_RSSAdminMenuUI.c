// RSS Admin Settings Menu - 游戏内管理配置面板
// 仅管理员可见，通过暂停菜单或 GameMode 入口打开
// 布局: UI/layouts/Menus/RSSSettings/RSSSettings.layout

class SCR_RSSAdminMenuUI
{
    protected static ref SCR_RSSAdminMenuUI s_Instance;

    protected Widget m_wRoot;
    protected TextWidget m_wPresetNameDisplay;
    protected TextWidget m_wStatusText;
    protected CheckBoxWidget m_wChkDebug;
    protected CheckBoxWidget m_wChkHUD;
    protected CheckBoxWidget m_wChkDataExport;
    protected CheckBoxWidget m_wChkMudSlip;
    protected CheckBoxWidget m_wChkAICombat;

    // 当前预设名缓存
    protected string m_sCurrentPreset;

    //------------------------------------------------------------------------------------------------
    // 打开管理面板（静态入口）
    static void Open()
    {
        if (s_Instance)
        {
            s_Instance.BringToFront();
            return;
        }

        SCR_RSSAdminMenuUI inst = new SCR_RSSAdminMenuUI();
        if (!inst.CreatePanel())
            return;
        s_Instance = inst;
    }

    //------------------------------------------------------------------------------------------------
    // 关闭管理面板
    static void Close()
    {
        if (s_Instance)
        {
            s_Instance.DestroyPanel();
            s_Instance = null;
        }
    }

    //------------------------------------------------------------------------------------------------
    // 切换面板（打开/关闭）
    static void Toggle()
    {
        if (s_Instance)
            Close();
        else
            Open();
    }

    //------------------------------------------------------------------------------------------------
    // 检查面板是否已打开
    static bool IsOpen()
    {
        return s_Instance != null;
    }

    //------------------------------------------------------------------------------------------------
    // 将已存在的面板置顶
    protected void BringToFront()
    {
        RefreshUI();
    }

    //------------------------------------------------------------------------------------------------
    // 创建面板
    protected bool CreatePanel()
    {
        WorkspaceWidget workspace = GetGame().GetWorkspace();
        if (!workspace)
            return false;

        m_wRoot = workspace.CreateWidgets("{5932EB24D1397F01}UI/layouts/Menus/RSSSettings/RSSSettings.layout");
        if (!m_wRoot)
            return false;

        // 查找控件
        m_wPresetNameDisplay = TextWidget.Cast(m_wRoot.FindAnyWidget("PresetNameDisplay"));
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

        // 绑定复选框
        if (m_wChkDebug)
        {
            m_wChkDebug.SetChecked(GetSettings().m_bDebugLogEnabled);
        }
        if (m_wChkHUD)
        {
            m_wChkHUD.SetChecked(GetSettings().m_bHintDisplayEnabled);
        }
        if (m_wChkDataExport)
        {
            m_wChkDataExport.SetChecked(GetSettings().m_bDataExportEnabled);
        }
        if (m_wChkMudSlip)
        {
            m_wChkMudSlip.SetChecked(GetSettings().m_bEnableMudSlipMechanism);
        }
        if (m_wChkAICombat)
        {
            m_wChkAICombat.SetChecked(GetSettings().m_bEnableAIStaminaCombatEffects);
        }

        // 绑定操作按钮
        btn = SCR_ButtonTextComponent.GetButtonText("BtnReload", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnReloadConfig);
        btn = SCR_ButtonTextComponent.GetButtonText("BtnSave", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnSaveAndSync);
        btn = SCR_ButtonTextComponent.GetButtonText("CloseButton", m_wRoot);
        if (btn) btn.m_OnClicked.Insert(OnCloseButton);

        // 刷新 UI
        RefreshUI();

        return true;
    }

    //------------------------------------------------------------------------------------------------
    // 销毁面板
    protected void DestroyPanel()
    {
        if (m_wRoot)
        {
            m_wRoot.RemoveFromHierarchy();
            m_wRoot = null;
        }
        m_wPresetNameDisplay = null;
        m_wStatusText = null;
        m_wChkDebug = null;
        m_wChkHUD = null;
        m_wChkDataExport = null;
        m_wChkMudSlip = null;
        m_wChkAICombat = null;
    }

    //------------------------------------------------------------------------------------------------
    // 刷新 UI 显示
    protected void RefreshUI()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings)
            return;

        m_sCurrentPreset = settings.m_sSelectedPreset;
        if (!m_sCurrentPreset || m_sCurrentPreset == "")
            m_sCurrentPreset = "StandardMilsim";

        if (m_wPresetNameDisplay)
        {
            m_wPresetNameDisplay.SetText(m_sCurrentPreset);

            // 预设颜色
            Color presetColor;
            if (m_sCurrentPreset == "EliteStandard")
                presetColor = Color.FromRGBA(220, 80, 80, 255);
            else if (m_sCurrentPreset == "StandardMilsim")
                presetColor = Color.FromRGBA(80, 120, 220, 255);
            else if (m_sCurrentPreset == "TacticalAction")
                presetColor = Color.FromRGBA(80, 200, 80, 255);
            else
                presetColor = Color.FromRGBA(180, 180, 180, 255);

            m_wPresetNameDisplay.SetColor(presetColor);
        }

        SetStatus("");
    }

    //------------------------------------------------------------------------------------------------
    // 设置状态文本
    protected void SetStatus(string text)
    {
        if (m_wStatusText)
            m_wStatusText.SetText(text);
    }

    //------------------------------------------------------------------------------------------------
    // 获取当前配置
    protected SCR_RSS_Settings GetSettings()
    {
        return SCR_RSS_ConfigManager.GetSettings();
    }

    //------------------------------------------------------------------------------------------------
    // 保存开关状态到配置
    protected void SaveSwitchesToSettings()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings)
            return;

        if (m_wChkDebug)
            settings.m_bDebugLogEnabled = m_wChkDebug.IsChecked();
        if (m_wChkHUD)
            settings.m_bHintDisplayEnabled = m_wChkHUD.IsChecked();
        if (m_wChkDataExport)
            settings.m_bDataExportEnabled = m_wChkDataExport.IsChecked();
        if (m_wChkMudSlip)
            settings.m_bEnableMudSlipMechanism = m_wChkMudSlip.IsChecked();
        if (m_wChkAICombat)
            settings.m_bEnableAIStaminaCombatEffects = m_wChkAICombat.IsChecked();
    }

    //------------------------------------------------------------------------------------------------
    // 触发配置同步到客户端
    protected void TriggerReplication()
    {
        if (!GetGame())
            return;

        SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
        if (gameMode)
            gameMode.RSS_ReplicateConfigNow();

        // HUD 同步
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    //------------------------------------------------------------------------------------------------
    // 关闭按钮回调
    protected void OnCloseButton()
    {
        Close();
    }

    //------------------------------------------------------------------------------------------------
    // 预设按钮回调
    protected void OnPresetElite()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;
        settings.m_sSelectedPreset = "EliteStandard";
        settings.InitPresets(true);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshUI();
        SetStatus("Preset: EliteStandard — applied & synced");
    }

    protected void OnPresetStandard()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;
        settings.m_sSelectedPreset = "StandardMilsim";
        settings.InitPresets(true);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshUI();
        SetStatus("Preset: StandardMilsim — applied & synced");
    }

    protected void OnPresetTactical()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;
        settings.m_sSelectedPreset = "TacticalAction";
        settings.InitPresets(true);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshUI();
        SetStatus("Preset: TacticalAction — applied & synced");
    }

    protected void OnPresetCustom()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;
        settings.m_sSelectedPreset = "Custom";
        settings.InitPresets(false);
        SaveSwitchesToSettings();
        TriggerReplication();
        RefreshUI();
        SetStatus("Preset: Custom — applied & synced");
    }

    //------------------------------------------------------------------------------------------------
    // 重新加载配置
    protected void OnReloadConfig()
    {
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Reload();
            TriggerReplication();
            RefreshUI();
            SetStatus("Config reloaded from disk & synced");
        }
        else
        {
            SetStatus("Only server can reload config");
        }
    }

    //------------------------------------------------------------------------------------------------
    // 保存并同步
    protected void OnSaveAndSync()
    {
        SCR_RSS_Settings settings = GetSettings();
        if (!settings) return;

        SaveSwitchesToSettings();

        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Save();
            TriggerReplication();
            SetStatus("Settings saved & synced to all clients");
        }
        else
        {
            SetStatus("Settings saved locally (server sync required)");
        }
    }
}
