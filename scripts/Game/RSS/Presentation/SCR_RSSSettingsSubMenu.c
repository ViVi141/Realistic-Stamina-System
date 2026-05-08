// RSS Settings SubMenu
//   所有人：HUD (Local) — 只影响自己，不经过服务器
//   管理员额外：预设 + HUD Server Default + Debug/MudSlip/AICombat

class SCR_RSSSettingsSubMenu : SCR_SettingsSubMenuBase
{
    protected SCR_ComboBoxComponent m_wPresetSelector;
    protected SCR_SpinBoxComponent m_wHUDLocal;
    protected SCR_SpinBoxComponent m_wHUDServer;
    protected SCR_SpinBoxComponent m_wDebugToggle;
    protected SCR_SpinBoxComponent m_wMudSlipToggle;
    protected SCR_SpinBoxComponent m_wAICombatToggle;
    protected SCR_SpinBoxComponent m_wDisableAIToggle;
    protected SCR_SpinBoxComponent m_wDisableAIStaminaToggle;
    protected bool m_bIsAdmin;

    //------------------------------------------------------------------------------------------------
    override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
    {
        super.OnTabCreate(menuRoot, buttonsLayout, index);
        m_bIsAdmin = IsPlayerAdmin();

        m_wPresetSelector  = FindComboBox("PresetSelector");
        m_wHUDLocal        = FindSpinBox("ToggleHUD");
        m_wHUDServer       = FindSpinBox("ToggleServerHUD");
        m_wDebugToggle     = FindSpinBox("ToggleDebug");
        m_wMudSlipToggle   = FindSpinBox("ToggleMudSlip");
        m_wAICombatToggle  = FindSpinBox("ToggleAICombat");
        m_wDisableAIToggle       = FindSpinBox("ToggleDisableAI");
        m_wDisableAIStaminaToggle = FindSpinBox("ToggleDisableAIStamina");

        if (m_wPresetSelector)
            m_wPresetSelector.m_OnChanged.Insert(OnPresetChanged);

        UpdateVisibility();
        LoadFromSettings();
    }

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

        // 本地 HUD：所有人立即生效
        SCR_StaminaHUDComponent.SetLocalHUDEnabled(GetSpin(m_wHUDLocal));

        // 管理员：服务器开关写入
        if (m_bIsAdmin)
        {
            bool debugLog    = GetSpin(m_wDebugToggle);
            bool hudServer   = GetSpin(m_wHUDServer);
            bool mudSlip     = GetSpin(m_wMudSlipToggle);
            bool aiCombat    = GetSpin(m_wAICombatToggle);
            bool disableAI   = GetSpin(m_wDisableAIToggle);
            bool disableAISt = GetSpin(m_wDisableAIStaminaToggle);

            if (Replication.IsServer())
            {
                // 监听服务器：直接写入内存 + 保存 JSON + 复制
                SCR_RSS_Settings s = GetSettings();
                if (s)
                {
                    s.m_bHintDisplayEnabled           = hudServer;
                    s.m_bDebugLogEnabled              = debugLog;
                    s.m_bEnableMudSlipMechanism       = mudSlip;
                    s.m_bEnableAIStaminaCombatEffects = aiCombat;
                    s.m_bDisableAIAllCalc             = disableAI;
                    s.m_bDisableAIStaminaCalc         = disableAISt;
                    SCR_RSS_ConfigManager.Save();
                }
            }
            else
            {
                // 专用服务器管理员客户端：通过 RPC 推送到服务端
                SendConfigToServer("", debugLog, hudServer, false, mudSlip, aiCombat, disableAI, disableAISt);
            }
            SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
        }
    }

    //------------------------------------------------------------------------------------------------
    protected void UpdateVisibility()
    {
        HideWidget("TitlePreset",      !m_bIsAdmin);
        HideWidget("PresetSelector",   !m_bIsAdmin);
        HideWidget("TitleToggles",     !m_bIsAdmin);
        HideWidget("ToggleDebug",      !m_bIsAdmin);
        HideWidget("ToggleServerHUD",  !m_bIsAdmin);
        HideWidget("ToggleMudSlip",    !m_bIsAdmin);
        HideWidget("ToggleAICombat",   !m_bIsAdmin);
        HideWidget("ToggleDisableAI",         !m_bIsAdmin);
        HideWidget("ToggleDisableAIStamina",  !m_bIsAdmin);
    }

    //------------------------------------------------------------------------------------------------
    protected void LoadFromSettings()
    {
        // HUD Local：始终读本地覆盖
        SetSpin(m_wHUDLocal, SCR_StaminaHUDComponent.GetLocalHUDEnabled());

        if (!m_bIsAdmin) return;

        SCR_RSS_Settings s = GetSettings();
        if (!s) return;

        // 管理员区域
        SetSpin(m_wHUDServer,  s.m_bHintDisplayEnabled);
        SetSpin(m_wDebugToggle,    s.m_bDebugLogEnabled);
        SetSpin(m_wMudSlipToggle,  s.m_bEnableMudSlipMechanism);
        SetSpin(m_wAICombatToggle, s.m_bEnableAIStaminaCombatEffects);
        SetSpin(m_wDisableAIToggle,       s.m_bDisableAIAllCalc);
        SetSpin(m_wDisableAIStaminaToggle, s.m_bDisableAIStaminaCalc);

        if (m_wPresetSelector)
        {
            string preset = s.m_sSelectedPreset;
            if (!preset || preset == "") preset = "StandardMilsim";
            int idx = 1;
            if (preset == "EliteStandard")   idx = 0;
            else if (preset == "StandardMilsim") idx = 1;
            else if (preset == "TacticalAction") idx = 2;
            else                              idx = 3;
            m_wPresetSelector.SetCurrentItem(idx, false, false);
        }
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

        if (Replication.IsServer())
        {
            // 监听服务器：直接修改 + 保存 + 复制
            s.m_sSelectedPreset = preset;
            s.InitPresets(preset != "Custom");
            SCR_RSS_ConfigManager.Save();
        }
        else
        {
            // 专用服务器管理员客户端：通过 RPC 推送预设变更
            s.m_sSelectedPreset = preset;  // 临时更新本地显示
            s.InitPresets(preset != "Custom");
            SendConfigToServer(preset, GetSpin(m_wDebugToggle), GetSpin(m_wHUDServer),
                false, GetSpin(m_wMudSlipToggle), GetSpin(m_wAICombatToggle),
                GetSpin(m_wDisableAIToggle), GetSpin(m_wDisableAIStaminaToggle));
        }
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
        LoadFromSettings();
    }

    //! 通过本地玩家控制器的 RPC 将配置推送到服务端
    protected void SendConfigToServer(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        IEntity player = SCR_PlayerController.GetLocalControlledEntity();
        if (!player) return;

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            player.FindComponent(SCR_CharacterControllerComponent));
        if (ctrl)
            ctrl.RPC_AdminUpdateConfig(preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    //------------------------------------------------------------------------------------------------
    protected SCR_RSS_Settings GetSettings()
    {
        return SCR_RSS_ConfigManager.GetSettings();
    }

    protected bool IsPlayerAdmin()
    {
        int pid = GetGame().GetPlayerController().GetPlayerId();
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm) return false;
        return pm.HasPlayerRole(pid, EPlayerRole.ADMINISTRATOR)
            || pm.HasPlayerRole(pid, EPlayerRole.SESSION_ADMINISTRATOR)
            || pm.HasPlayerRole(pid, EPlayerRole.GAME_MASTER);
    }

    protected void SetSpin(SCR_SpinBoxComponent spin, bool value)
    {
        if (!spin) return;
        int idx = 0;
        if (value)
            idx = 1;
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