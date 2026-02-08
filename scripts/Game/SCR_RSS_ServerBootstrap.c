// Server bootstrap: load RSS config as soon as the game mode starts on server.
modded class SCR_BaseGameMode
{
    override void OnGameStart()
    {
        super.OnGameStart();

        if (Replication.IsServer())
        {
            SCR_RSS_ConfigManager.Load();
        }
    }
}
