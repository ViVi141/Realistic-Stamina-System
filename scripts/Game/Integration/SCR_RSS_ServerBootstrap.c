// Server bootstrap: load RSS config as soon as the game mode starts on server.
modded class SCR_BaseGameMode
{
    protected int m_iRssLoadRetries = 0;
    protected static const int RSS_LOAD_RETRY_MAX = 10;

    // --------------------------------------------------------------------------------------------
    // RSS config replication (native-style): replicate from GameMode to all clients.
    // This replaces per-player config request/ACK loops in PlayerBase.
    //
    // NOTE: We intentionally keep the replicated payload compact and versioned.
    // Clients apply it into SCR_RSS_ConfigManager in onRpl callbacks.
    // --------------------------------------------------------------------------------------------
    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected string m_sRssConfigVersion;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected string m_sRssSelectedPreset;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected ref array<float> m_aRssCombinedPresetParams;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected ref array<float> m_aRssFloatSettings;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected ref array<int> m_aRssIntSettings;

    [RplProp(onRplName: "OnRssConfigReplicated")]
    protected ref array<bool> m_aRssBoolSettings;

    protected bool m_bRssConfigInitialized = false;

    //! 服务端数据导出链是否在跑（m_bDataExportEnabled 为假时不跑、不占每秒 CallLater true）。
    protected bool m_bRssDataExportLoopRunning = false;

    // Public entry for server hot-reload / save paths.
    // Can be called from SCR_RSS_ConfigManager after a successful Load/Save/Reload.
    void RSS_ReplicateConfigNow()
    {
        RSS_BuildAndReplicateConfig();
        RssServerDataExportScheduleIfNeeded();
    }

    override void OnGameStart()
    {
        super.OnGameStart();

        // Replication can be uninitialized at OnGameStart; defer and retry briefly.
        if (GetGame())
            GetGame().GetCallqueue().CallLater(DeferredRssConfigLoad, 1000, false);
    }

    protected void DeferredRssConfigLoad()
    {
        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();
            RSS_BuildAndReplicateConfig();
            RssServerDataExportScheduleIfNeeded();
            return;
        }

        m_iRssLoadRetries++;
        if (m_iRssLoadRetries <= RSS_LOAD_RETRY_MAX && GetGame())
        {
            GetGame().GetCallqueue().CallLater(DeferredRssConfigLoad, 1000, false);
        }
    }

    //! 仅在 m_bDataExportEnabled 为真时启动/维持约 1s 一次的 TryExport；关导出时停止链并不再调度。
    protected void RssServerDataExportScheduleIfNeeded()
    {
        if (!Replication.IsServer())
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;
        if (!settings.m_bDataExportEnabled)
            return;
        if (m_bRssDataExportLoopRunning)
            return;

        m_bRssDataExportLoopRunning = true;
        if (GetGame())
            GetGame().GetCallqueue().CallLater(RssServerDataExportTick, 1000, false);
    }

    protected void RssServerDataExportTick()
    {
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

    // Server: build payload from current settings and replicate.
    protected void RSS_BuildAndReplicateConfig()
    {
        if (!Replication.IsServer())
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;

        if (!m_aRssCombinedPresetParams)
            m_aRssCombinedPresetParams = new array<float>();
        if (!m_aRssFloatSettings)
            m_aRssFloatSettings = new array<float>();
        if (!m_aRssIntSettings)
            m_aRssIntSettings = new array<int>();
        if (!m_aRssBoolSettings)
            m_aRssBoolSettings = new array<bool>();

        SCR_RSS_ConfigSyncUtils.BuildCombinedPresetArray(settings, m_aRssCombinedPresetParams);
        SCR_RSS_ConfigSyncUtils.BuildSettingsArrays(settings, m_aRssFloatSettings, m_aRssIntSettings, m_aRssBoolSettings);

        m_sRssConfigVersion = settings.m_sConfigVersion;
        m_sRssSelectedPreset = settings.m_sSelectedPreset;

        m_bRssConfigInitialized = true;
        Replication.BumpMe();
    }

    // Client: apply replicated config into SCR_RSS_ConfigManager.
    // Also runs on server when replication updates locally; server side is no-op.
    protected void OnRssConfigReplicated()
    {
        if (Replication.IsServer())
            return;

        if (!m_bRssConfigInitialized)
            return;

        if (!m_sRssConfigVersion || !m_sRssSelectedPreset)
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            settings = new SCR_RSS_Settings();

        array<float> eliteParams = null;
        array<float> standardParams = null;
        array<float> tacticalParams = null;
        array<float> customParams = null;

        bool ok = SCR_RSS_ConfigSyncUtils.SplitCombinedPresetParams(
            m_aRssCombinedPresetParams,
            eliteParams,
            standardParams,
            tacticalParams,
            customParams);
        if (!ok)
            return;

        // Apply settings and presets in the same format as the old RPC payload.
        settings.InitPresets(false);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_EliteStandard, eliteParams);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_StandardMilsim, standardParams);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_TacticalAction, tacticalParams);
        SCR_RSS_Settings.ApplyParamsFromArray(settings.m_Custom, customParams);
        SCR_RSS_Settings.ApplySettingsFromArrays(settings, m_aRssFloatSettings, m_aRssIntSettings, m_aRssBoolSettings);

        settings.m_sConfigVersion = m_sRssConfigVersion;
        settings.m_sSelectedPreset = m_sRssSelectedPreset;

        SCR_RSS_ConfigManager.SetServerConfigApplied(true);

        // Hint HUD 与 m_bHintDisplayEnabled 同步（含首次同步与专服热重载）。
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }
}
