// PlayerBase split: 性能 LOD / Workbench 过滤
modded class SCR_CharacterControllerComponent
{
    protected float GetNearestPlayerDistanceM(IEntity ownerEntity)
    {
        if (!ownerEntity)
            return -1.0;

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return -1.0;

        array<int> playerIds = {};
        pm.GetPlayers(playerIds);
        float nearM = 99999.0;
        for (int pi = 0; pi < playerIds.Count(); pi++)
        {
            IEntity pe = pm.GetPlayerControlledEntity(playerIds.Get(pi));
            if (pe)
            {
                float d = vector.Distance(ownerEntity.GetOrigin(), pe.GetOrigin());
                if (d < nearM)
                    nearM = d;
            }
        }
        if (nearM < 99999.0)
            return nearM;
        return -1.0;
    }

    protected int GetSpeedUpdateIntervalMs()
    {
        // 玩家：高速刷新（~60Hz）
        if (IsPlayerControlled())
            return StaminaMudSlipConstants.RSS_PLAYER_SPEED_UPDATE_INTERVAL_MS;

        // AI：使用旧的独立距离 LOD 计算
        if (!Replication.IsServer())
            return StaminaMudSlipConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        if (!StaminaMudSlipConstants.RSS_PERF_AI_DISTANCE_LOD_ENABLED)
            return StaminaMudSlipConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        IEntity ownerEntity = GetOwner();
        if (!ownerEntity)
            return StaminaMudSlipConstants.RSS_AI_SPEED_UPDATE_INTERVAL_MS;

        float distM = GetNearestPlayerDistanceM(ownerEntity);

        if (distM < 0.0 || distM <= StaminaMudSlipConstants.RSS_PERF_AI_LOD_NEAR_M)
            return StaminaMudSlipConstants.RSS_PERF_AI_LOD_NEAR_INTERVAL_MS;
        if (distM <= StaminaMudSlipConstants.RSS_PERF_AI_LOD_FAR_M)
            return StaminaMudSlipConstants.RSS_PERF_AI_LOD_MID_INTERVAL_MS;
        return StaminaMudSlipConstants.RSS_PERF_AI_LOD_FAR_INTERVAL_MS;
    }

    //! 在 Workbench 编辑器中，预览实体不应启动体力 tick
    //! 特征：有 CharacterControllerComponent + AIControlComponent + 无玩家控制 + 无 AI 群组
    protected bool IsWorkbenchPreviewEntity()
    {
        #ifdef WORKBENCH
        IEntity owner = GetOwner();
        if (!owner)
            return true;
        if (IsPlayerControlled())
            return false;
        // 查找 AI 组件
        AIControlComponent aiCtrl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiCtrl)
            return true;  // 无 AI 控制组件 = 不是真实 AI
        AIAgent agent = aiCtrl.GetAIAgent();
        if (!agent)
            return true;  // 无 AI Agent = 不是真实 AI
        // 真实 AI 一定有父群组（SCR_AIGroup 或引擎 group）
        AIGroup parentGroup = agent.GetParentGroup();
        if (!parentGroup)
            return true;
        return false;
        #else
        return false;
        #endif
    }
}
