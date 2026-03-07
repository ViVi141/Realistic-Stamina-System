// RSS 配置同步组件（持久实体）
// 挂在与 GameMode 同实体或任意带 RplComponent 的持久实体上，服务器写 RplProp 后由引擎复制到所有客户端，不依赖角色或 RPC。
// 参考：Radar Development Framework 的 EMFieldNetworkComponent / RDF_LidarNetworkComponent。
[ComponentEditorProps(category: "GameScripted/RSS", description: "Network config sync for RSS. Attach to same entity as GameMode (entity must have RplComponent).")]
class SCR_RSS_ConfigSyncComponentClass : ScriptComponentClass {}

class SCR_RSS_ConfigSyncComponent : ScriptComponent
{
    protected RplComponent m_RplComponent;

    [RplProp(condition: RplCondition.NoOwner, onRplName: "OnServerConfigReplicated")]
    protected bool m_bServerHintEnabled = false;
    [RplProp(condition: RplCondition.NoOwner, onRplName: "OnServerConfigReplicated")]
    protected int m_iServerPresetIndex = 1; // 0=EliteStandard, 1=StandardMilsim, 2=TacticalAction, 3=Custom

    protected static int GetPresetIndexFromName(string presetName)
    {
        if (presetName == "EliteStandard") return 0;
        if (presetName == "StandardMilsim") return 1;
        if (presetName == "TacticalAction") return 2;
        if (presetName == "Custom") return 3;
        return 1;
    }

    protected static string GetPresetNameFromIndex(int index)
    {
        if (index == 0) return "EliteStandard";
        if (index == 1) return "StandardMilsim";
        if (index == 2) return "TacticalAction";
        if (index == 3) return "Custom";
        return "StandardMilsim";
    }

    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
        m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
        if (!m_RplComponent)
        {
            Print("[RSS_ConfigSync] No RplComponent on owner - sync disabled.", LogLevel.WARNING);
            return;
        }
        if (m_RplComponent.IsProxy())
        {
            return;
        }
        SCR_RSS_ConfigManager.RegisterConfigSyncComponent(this);
        PushServerConfigToReplication();
    }

    override void OnDelete(IEntity owner)
    {
        if (!m_RplComponent || !m_RplComponent.IsProxy())
            SCR_RSS_ConfigManager.UnregisterConfigSyncComponent(this);
        super.OnDelete(owner);
    }

    // 由 ConfigManager 在配置变更时调用（仅服务器）
    void PushServerConfigToReplication()
    {
        if (!m_RplComponent || m_RplComponent.IsProxy())
            return;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;
        m_bServerHintEnabled = settings.m_bHintDisplayEnabled;
        m_iServerPresetIndex = GetPresetIndexFromName(settings.m_sSelectedPreset);
        Replication.BumpMe();
    }

    void OnServerConfigReplicated()
    {
        if (!m_RplComponent || !m_RplComponent.IsProxy())
            return;
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings)
            return;
        settings.m_bHintDisplayEnabled = m_bServerHintEnabled;
        settings.m_sSelectedPreset = GetPresetNameFromIndex(m_iServerPresetIndex);
        settings.InitPresets(true);
        SCR_RSS_ConfigManager.ApplyServerConfigNoWrite();
        if (settings.m_bHintDisplayEnabled)
            SCR_StaminaHUDComponent.Init();
        else
            SCR_StaminaHUDComponent.Destroy();
        if (StaminaConstants.IsDebugEnabled())
            PrintFormat("[RSS_ConfigSync] Applied from RplProp: preset=%1, HUD=%2", settings.m_sSelectedPreset, m_bServerHintEnabled);
    }
}
