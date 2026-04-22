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

    // Public entry for server hot-reload / save paths.
    // Can be called from SCR_RSS_ConfigManager after a successful Load/Save/Reload.
    void RSS_ReplicateConfigNow()
    {
        RSS_BuildAndReplicateConfig();
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
            if (GetGame())
                GetGame().GetCallqueue().CallLater(RssServerDataExportTick, 1000, true);
            return;
        }

        m_iRssLoadRetries++;
        if (m_iRssLoadRetries <= RSS_LOAD_RETRY_MAX && GetGame())
        {
            GetGame().GetCallqueue().CallLater(DeferredRssConfigLoad, 1000, false);
        }
    }

    //! 数据导出单点触发，避免每个角色 tick 调用 TryExport（性能）
    protected void RssServerDataExportTick()
    {
        if (Replication.IsServer())
            SCR_RSS_DataExport.TryExport();
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

        // Notify systems which depend on settings (HUD etc.) is still handled by PlayerBase apply path
        // when it detects server config is applied; we keep ConfigManager as the single source of truth.
    }
}
