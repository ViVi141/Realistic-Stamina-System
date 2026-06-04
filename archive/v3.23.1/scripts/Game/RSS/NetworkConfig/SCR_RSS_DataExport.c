// RSS 数据导出模块 - 方案一：文件桥接
// 周期性将玩家体力与环境数据写入 JSON，供外部应用（命令控制台等）读取
// 配置开关：m_bDataExportEnabled、m_iDataExportIntervalMs

[BaseContainerProps()]
class RSS_ExportPlayerEntry
{
    [Attribute("0", desc: "Player ID")]
    int playerId;

    [Attribute("", desc: "Player name")]
    string playerName;

    [Attribute("1.0", desc: "Stamina 0-1")]
    float staminaPercent;

    [Attribute("1.0", desc: "Speed multiplier")]
    float speedMultiplier;

    [Attribute("0.0", desc: "Current speed m/s")]
    float currentSpeed;

    [Attribute("0", desc: "0=idle 1=walk 2=run 3=sprint")]
    int movementPhase;

    [Attribute("false", desc: "Is sprinting")]
    bool isSprinting;

    [Attribute("false", desc: "Is exhausted")]
    bool isExhausted;

    [Attribute("false", desc: "Is swimming")]
    bool isSwimming;

    [Attribute("0.0", desc: "Current weight kg")]
    float currentWeight;

    [Attribute("20.0", desc: "Temperature C")]
    float temperature;

    [Attribute("0.0", desc: "Rain intensity 0-1")]
    float rainIntensity;

    [Attribute("0.0", desc: "Wind speed m/s")]
    float windSpeed;

    [Attribute("false", desc: "Is indoor")]
    bool isIndoor;
}

[BaseContainerProps()]
class RSS_ExportData
{
    [Attribute("0", desc: "Unix timestamp ms")]
    int timestamp;

    [Attribute("", desc: "Players array")]
    ref array<ref RSS_ExportPlayerEntry> players;
}

class SCR_RSS_DataExport
{
    protected static const string EXPORT_PATH = "$profile:RSS_PlayerData.json";
    protected static float s_fLastExportTime = -1000.0;

    // 执行导出（仅服务器调用）
    static void TryExport()
    {
        if (!Replication.IsServer())
            return;

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings || !settings.m_bDataExportEnabled)
            return;

        float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
        float intervalSec = settings.m_iDataExportIntervalMs / 1000.0;
        if (intervalSec <= 0.0)
            intervalSec = 1.0;

        if (nowSec - s_fLastExportTime < intervalSec)
            return;

        s_fLastExportTime = nowSec;
        ExportToFile();
    }

    protected static void ExportToFile()
    {
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return;

        array<int> playerIds = new array<int>();
        playerManager.GetPlayers(playerIds);
        if (playerIds.Count() == 0)
            return;

        RSS_ExportData exportData = new RSS_ExportData();
        exportData.timestamp = GetGame().GetWorld().GetWorldTime();
        exportData.players = new array<ref RSS_ExportPlayerEntry>();

        for (int i = 0; i < playerIds.Count(); i++)
        {
            int playerId = playerIds[i];
            IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
            if (!entity)
                continue;

            if (!SCR_RSS_API.IsRssManaged(entity))
                continue;

            // 服务器端：玩家角色不运行 UpdateSpeedBasedOnStamina，EnvironmentFactor 缓存未更新。
            // 导出前强制刷新环境因子（气温、降雨、室内等），使服务器能独立计算。
            SCR_CharacterControllerComponent ctrl = SCR_RSS_API.GetRssController(entity);
            if (ctrl && ctrl.HasRssData())
            {
                EnvironmentFactor env = ctrl.GetRssEnvironmentFactor();
                if (env)
                {
                    float nowSec = GetGame().GetWorld().GetWorldTime() / 1000.0;
                    env.ForceUpdate(nowSec, entity, 0.0);
                }
            }

            RSS_PlayerInfo playerInfo = SCR_RSS_API.GetPlayerInfo(entity);
            RSS_EnvironmentInfo envInfo = SCR_RSS_API.GetEnvironmentInfo(entity);

            if (!playerInfo.isValid)
                continue;

            RSS_ExportPlayerEntry entry = new RSS_ExportPlayerEntry();
            entry.playerId = playerId;
            entry.playerName = playerManager.GetPlayerName(playerId);
            if (!entry.playerName || entry.playerName == "")
                entry.playerName = "Player" + playerId.ToString();

            entry.staminaPercent = playerInfo.staminaPercent;
            entry.speedMultiplier = playerInfo.speedMultiplier;
            entry.currentSpeed = playerInfo.currentSpeed;
            entry.movementPhase = playerInfo.movementPhase;
            entry.isSprinting = playerInfo.isSprinting;
            entry.isExhausted = playerInfo.isExhausted;
            entry.isSwimming = playerInfo.isSwimming;
            entry.currentWeight = playerInfo.currentWeight;

            if (envInfo.isValid)
            {
                entry.temperature = envInfo.temperature;
                entry.rainIntensity = envInfo.rainIntensity;
                entry.windSpeed = envInfo.windSpeed;
                entry.isIndoor = envInfo.isIndoor;
            }
            else
            {
                entry.temperature = 20.0;
                entry.rainIntensity = 0.0;
                entry.windSpeed = 0.0;
                entry.isIndoor = false;
            }

            exportData.players.Insert(entry);
        }

        if (exportData.players.Count() == 0)
            return;

        SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
        saveContext.WriteValue("", exportData);
        saveContext.SaveToFile(EXPORT_PATH);
    }
}
