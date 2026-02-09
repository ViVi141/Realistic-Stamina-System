// Server bootstrap: load RSS config as soon as the game mode starts on server.
modded class SCR_BaseGameMode
{
    protected int m_iRssLoadRetries = 0;
    protected static const int RSS_LOAD_RETRY_MAX = 10;

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
            return;
        }

        m_iRssLoadRetries++;
        if (m_iRssLoadRetries <= RSS_LOAD_RETRY_MAX && GetGame())
        {
            GetGame().GetCallqueue().CallLater(DeferredRssConfigLoad, 1000, false);
        }
    }
}
