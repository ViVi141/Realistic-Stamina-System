// Server bootstrap: load RSS config as soon as the game mode starts on server.
modded class SCR_BaseGameMode
{
    protected int m_iRssLoadRetries = 0;
    protected static const int RSS_LOAD_RETRY_MAX = 10;

    // --------------------------------------------------------------------------------------------
    // RSS config replication (lightweight)
    //   • 客户端本地 InitPresets() 生成预设参数，服务器只复制"选哪个预设 + 管理员改了哪些开关"
    //   • Custom 预设时额外复制 m_aRssCustomParams（47 floats），非 Custom 不复制
    // --------------------------------------------------------------------------------------------
    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected string m_sRssConfigVersion;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected string m_sRssSelectedPreset;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssCustomActive;  // true = Custom 预设选中，m_aRssCustomParams 有效

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected ref array<float> m_aRssCustomParams;  // 47 floats，仅 Custom 模式有效

    // 顶层开关（5 个，管理员通过 Settings → RSS Tab 修改）
    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssDebugLog;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssHintDisplay;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssDataExport;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssMudSlip;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssAICombat;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssDisableAIAll;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected bool m_bRssDisableAIStamina;

    //! 服务端数据导出链是否在跑
    protected bool m_bRssDataExportLoopRunning = false;

    //! 退出/销毁时取消 CallLater，避免回调在 GameMode 或 World 释放后仍执行（空指针读）。
    void ~SCR_BaseGameMode()
    {
        m_bRssDataExportLoopRunning = false;
        if (GetGame() && GetGame().GetCallqueue())
        {
            GetGame().GetCallqueue().Remove(DeferredRssConfigLoad);
            GetGame().GetCallqueue().Remove(RssServerDataExportTick);
        }

        // CRITICAL FIX: Release static ref to this world's GameSignalsManager before
        // the engine destroys it. When a character was placed then deleted in Workbench,
        // EnvironmentFactor.Initialize() set s_pGlobalSignals (static ref) to the world's
        // GameSignalsManager. The entity destructor does NOT clear this static ref.
        // Without this cleanup, s_pGlobalSignals holds a strong reference to a C++ object
        // that will be freed during destroy-game, causing Access Violation at 0x0.
        EnvironmentFactor.ResetGlobalSignalsCache();
        SCR_DebugBatchManager.ResetForNewWorld();
    }

    //------------------------------------------------------------------------------------------------
    void RSS_ReplicateConfigNow()
    {
        RSS_BuildAndReplicateConfig();
        RssServerDataExportScheduleIfNeeded();
    }

    //------------------------------------------------------------------------------------------------
    void RSS_OpenAdminMenu()
    {
        if (!GetGame() || !GetGame().GetWorkspace())
            return;

        int playerId = GetGame().GetPlayerController().GetPlayerId();
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm) return;

        if (!pm.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR)
            && !pm.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR)
            && !pm.HasPlayerRole(playerId, EPlayerRole.GAME_MASTER))
        {
            Print("[RSS] RSS_OpenAdminMenu: access denied");
            return;
        }

        GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SettingsSuperMenu);
    }

    //------------------------------------------------------------------------------------------------
    override void OnGameStart()
    {
        // Workbench / 重载世界：先清空跨世界复用的脚本静态缓存，避免悬空原生引用（信号、HUD、AI 列表）。
        EnvironmentFactor.ResetGlobalSignalsCache();
        SCR_RSS_AIGroupSync.ClearAllForNewWorldSession();
        SCR_RSS_AIGroupLocomotionPolicy.ClearAllForNewWorldSession();
        SCR_RSS_AIGroupStaminaProxy.ClearAllForNewWorldSession();
        SCR_StaminaHUDComponent.OnNewWorldSession();
        SCR_DebugBatchManager.ResetForNewWorld();   // 重置调试批次时间戳，防止重载世界后无输出
        m_iRssLoadRetries = 0;

        super.OnGameStart();
        if (GetGame())
            GetGame().GetCallqueue().CallLater(DeferredRssConfigLoad, 1000, false);
    }

    //------------------------------------------------------------------------------------------------
    protected void DeferredRssConfigLoad()
    {
        if (!GetGame())
            return;

        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();
            RSS_BuildAndReplicateConfig();
            RssServerDataExportScheduleIfNeeded();
            return;
        }

        m_iRssLoadRetries++;
        if (m_iRssLoadRetries <= RSS_LOAD_RETRY_MAX && GetGame())
            GetGame().GetCallqueue().CallLater(DeferredRssConfigLoad, 1000, false);
    }

    //------------------------------------------------------------------------------------------------
    protected void RssServerDataExportScheduleIfNeeded()
    {
        if (!Replication.IsServer()) return;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings || !settings.m_bDataExportEnabled) return;
        if (m_bRssDataExportLoopRunning) return;

        m_bRssDataExportLoopRunning = true;
        if (GetGame())
            GetGame().GetCallqueue().CallLater(RssServerDataExportTick, 1000, false);
    }

    //------------------------------------------------------------------------------------------------
    protected void RssServerDataExportTick()
    {
        if (!GetGame())
        {
            m_bRssDataExportLoopRunning = false;
            return;
        }

        if (!Replication.IsServer())
        {
            m_bRssDataExportLoopRunning = false;
            return;
        }
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings || !settings.m_bDataExportEnabled)
        {
            m_bRssDataExportLoopRunning = false;
            return;
        }
        SCR_RSS_DataExport.TryExport();
        if (GetGame())
            GetGame().GetCallqueue().CallLater(RssServerDataExportTick, 1000, false);
        else
            m_bRssDataExportLoopRunning = false;
    }

    //------------------------------------------------------------------------------------------------
    // Server: 只复制差异数据（预设名 + 开关 + Custom 参数若有）
    protected void RSS_BuildAndReplicateConfig()
    {
        if (!Replication.IsServer()) return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings) return;

        m_sRssConfigVersion  = settings.m_sConfigVersion;
        m_sRssSelectedPreset = settings.m_sSelectedPreset;
        m_bRssDebugLog       = settings.m_bDebugLogEnabled;
        m_bRssHintDisplay    = settings.m_bHintDisplayEnabled;
        m_bRssDataExport     = settings.m_bDataExportEnabled;
        m_bRssMudSlip        = settings.m_bEnableMudSlipMechanism;
        m_bRssAICombat       = settings.m_bEnableAIStaminaCombatEffects;
        m_bRssDisableAIAll   = settings.m_bDisableAIAllCalc;
        m_bRssDisableAIStamina = settings.m_bDisableAIStaminaCalc;

        // Custom 预设：额外复制 47 参数
        string preset = settings.m_sSelectedPreset;
        if (preset == "Custom" && settings.m_Custom)
        {
            m_bRssCustomActive = true;
            if (!m_aRssCustomParams)
                m_aRssCustomParams = new array<float>();
            SCR_RSS_Settings.WriteParamsToArray(settings.m_Custom, m_aRssCustomParams);
        }
        else
        {
            m_bRssCustomActive = false;
            if (m_aRssCustomParams)
                m_aRssCustomParams.Clear();
        }

        Replication.BumpMe();
    }

    //------------------------------------------------------------------------------------------------
    // Client: 收到差异数据 → 本地 InitPresets + 应用开关
    protected void OnRssConfigReplicated()
    {
        if (Replication.IsServer()) return;
        if (!GetGame())
            return;
        // m_bRssConfigInitialized 未参与 Rpl 复制，客户端恒为 false 会误拦截；改为校验已复制的字段。
        if (!m_sRssConfigVersion || m_sRssConfigVersion == "")
            return;
        if (!m_sRssSelectedPreset || m_sRssSelectedPreset == "")
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            settings = new SCR_RSS_Settings();

        settings.m_sConfigVersion  = m_sRssConfigVersion;
        settings.m_sSelectedPreset = m_sRssSelectedPreset;

        // 客户端本地生成预设参数（与服务器相同代码路径）
        bool isCustom = (m_sRssSelectedPreset == "Custom");
        settings.InitPresets(!isCustom);

        // Custom 模式：应用服务器传来的自定义参数
        if (isCustom && m_bRssCustomActive && m_aRssCustomParams)
        {
            SCR_RSS_Settings.ApplyParamsFromArray(settings.m_Custom, m_aRssCustomParams);
        }

        // 应用顶层开关
        settings.m_bDebugLogEnabled              = m_bRssDebugLog;
        settings.m_bHintDisplayEnabled           = m_bRssHintDisplay;
        settings.m_bDataExportEnabled            = m_bRssDataExport;
        settings.m_bEnableMudSlipMechanism       = m_bRssMudSlip;
        settings.m_bEnableAIStaminaCombatEffects = m_bRssAICombat;
        settings.m_bDisableAIAllCalc             = m_bRssDisableAIAll;
        settings.m_bDisableAIStaminaCalc         = m_bRssDisableAIStamina;

        // 与 SCR_RSS_Settings 旧版同步路径一致：客户端记录“服务器是否开启导出”，供 GetServerDataExportEnabled / 体力上报使用。
        SCR_RSS_ConfigManager.SetServerDataExportEnabled(m_bRssDataExport);

        SCR_RSS_ConfigManager.SetServerConfigApplied(true);
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }
}