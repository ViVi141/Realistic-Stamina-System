//! RSS：在 SCR_AIGroup 初始化/销毁时注册群组路点事件（GroupSync）。

modded class SCR_AIGroup
{
    protected bool m_bRssGroupSyncRegistered;

    //------------------------------------------------------------------------------------------------
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);

        if (!Replication.IsServer())
            return;

        // 事件处理内已有 IsAIStaminaIntegrationEnabled 守卫；此处始终注册以免配置晚于 EOnInit 加载。
        SCR_RSS_AIGroupSync.RegisterForGroup(this);
        m_bRssGroupSyncRegistered = true;
    }

    //------------------------------------------------------------------------------------------------
    void ~SCR_AIGroup()
    {
        if (m_bRssGroupSyncRegistered)
        {
            SCR_RSS_AIGroupSync.UnregisterForGroup(this);
            m_bRssGroupSyncRegistered = false;
        }
    }
}
