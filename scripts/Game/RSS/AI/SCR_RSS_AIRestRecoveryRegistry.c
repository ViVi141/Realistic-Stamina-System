//! AI 进入「低体力休整路线」后的恢复锁定：体力未到 RSS_AI_REST_RECOVERY_RESUME_STAMINA_MIN 前强制步行，
//! 避免行为树仍要求快跑；达到阈值后解除。任意时刻若判定为战场危险（与滑倒一致）则由桥接层清除并取消战斗移动。

class SCR_RSS_AIRestRecoveryRegistry
{
    protected static ref array<IEntity> s_aRestRecoveryEntities;

    //------------------------------------------------------------------------------------------------
    protected static void EnsureArray()
    {
        if (!s_aRestRecoveryEntities)
            s_aRestRecoveryEntities = new array<IEntity>();
    }

    //------------------------------------------------------------------------------------------------
    protected static int FindEntityIndex(IEntity owner)
    {
        if (!s_aRestRecoveryEntities || !owner)
            return -1;
        int n = s_aRestRecoveryEntities.Count();
        for (int i = 0; i < n; i++)
        {
            if (s_aRestRecoveryEntities.Get(i) == owner)
                return i;
        }
        return -1;
    }

    //------------------------------------------------------------------------------------------------
    //! 单名 AI 进入休整恢复锁定（当前群组休整路径以 MarkRestRecoveryForGroup 为主）。
    static void MarkRestRecovery(IEntity owner)
    {
        if (!owner)
            return;
        EnsureArray();
        if (FindEntityIndex(owner) >= 0)
            return;
        s_aRestRecoveryEntities.Insert(owner);
    }

    //------------------------------------------------------------------------------------------------
    //! 群组路点：存活且未失能成员进入恢复锁定，直至各自体力达标（死亡/昏迷不参与）。
    static void MarkRestRecoveryForGroup(SCR_AIGroup grp)
    {
        if (!grp)
            return;
        array<AIAgent> agents = {};
        int ac = grp.GetAgents(agents);
        for (int i = 0; i < ac; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            if (!SCR_RSS_AIStaminaBridge.IsAliveAndActionableForGroupRest(ce))
                continue;
            MarkRestRecovery(ce);
        }
    }

    //------------------------------------------------------------------------------------------------
    static bool IsInRestRecovery(IEntity owner)
    {
        if (!owner)
            return false;
        if (FindEntityIndex(owner) >= 0)
            return true;
        return false;
    }

    //------------------------------------------------------------------------------------------------
    static void ClearRestRecovery(IEntity owner)
    {
        if (!owner)
            return;
        int idx = FindEntityIndex(owner);
        if (idx >= 0)
            s_aRestRecoveryEntities.RemoveOrdered(idx);
    }
}
