// PlayerBase split: 系统启动 / AI 调试 static / 实体删除
modded class SCR_CharacterControllerComponent
{
    void StartSystem()
    {
        if (m_bIsDeleted || !GetOwner())
            return;
        // Workbench 预览实体不启动体力 tick
        if (IsWorkbenchPreviewEntity())
            return;
        if (!ShouldProcessStaminaUpdate())
            return;
        if (!GetGame())
            return;
        m_bRssStaminaLoopActive = true;
        int intervalMs = GetSpeedUpdateIntervalMs();
        GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, intervalMs, false);
        
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }

    void EnsureRssStaminaLoopIfNeeded()
    {
        if (m_bIsDeleted)
            return;
        if (!ShouldProcessStaminaUpdate())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        StartSystem();
    }

    void EnsureAiStaminaLoopOnServer()
    {
        if (m_bIsDeleted || !GetOwner())
            return;
        if (!Replication.IsServer() || IsPlayerControlled())
            return;
        if (m_bRssStaminaLoopActive)
            return;
        m_iAiLoopRetryCount = m_iAiLoopRetryCount + 1;
        if (m_iAiLoopRetryCount > AI_LOOP_MAX_RETRIES)
            return;
        StartSystem();
        if (!m_bRssStaminaLoopActive && GetGame())
            GetGame().GetCallqueue().CallLater(EnsureAiStaminaLoopOnServer, 3000, false);
    }



    // ====== 群组调试辅助方法 ======
    //! 计算群组成员间最大分散距离（存活成员间的 max distance）
    static float CalcAiGroupSpreadM(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return -1.0;
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        if (ac < 2)
            return -1.0;

        float maxDist = 0.0;
        for (int i = 0; i < ac - 1; i++)
        {
            AIAgent agI = agents.Get(i);
            if (!agI)
                continue;
            IEntity ceI = agI.GetControlledEntity();
            if (!ceI)
                continue;
            for (int j = i + 1; j < ac; j++)
            {
                AIAgent agJ = agents.Get(j);
                if (!agJ)
                    continue;
                IEntity ceJ = agJ.GetControlledEntity();
                if (!ceJ)
                    continue;
                float d = vector.Distance(ceI.GetOrigin(), ceJ.GetOrigin());
                if (d > maxDist)
                    maxDist = d;
            }
        }
        return maxDist;
    }

    //! 获取群组中存活成员数量
    static int GetAliveMemberCount(SCR_AIGroup scrGrp)
    {
        if (!scrGrp)
            return 0;
        array<AIAgent> agents = {};
        int ac = scrGrp.GetAgents(agents);
        int alive = 0;
        for (int i = 0; i < ac; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ce = ag.GetControlledEntity();
            if (!ce)
                continue;
            if (SCR_CharacterDamageManagerComponent.Cast(ce.FindComponent(SCR_CharacterDamageManagerComponent)))
                alive++;
        }
        return alive;
    }

    //! 实体即将被删除时调用：清理所有引用、停止 CallLater 循环、注销静态注册表
    void RSS_NotifyEntityDeleting()
    {
        if (m_bIsDeleted)
            return;
        m_bIsDeleted = true;
        m_bRssStaminaLoopActive = false;
        
        IEntity owner = GetOwner();
        
        // 清理静态 AI 恢复注册表中的悬空引用
        if (owner && !IsPlayerControlled())
            // [REMOVED v3.23.0] SCR_RSS_AIRestRecoveryRegistry.CleanupEntity(owner);
            // 新架构中不再使用 RecoveryRegistry，cleanup 由 GroupSync.ClearAll 替代
        
        // 清理 UISignalBridge 的实体引用
        if (m_pUISignalBridge)
            m_pUISignalBridge.Cleanup();
        
        // 清零所有缓存的引擎组件引用（防止悬空指针访问）
        m_pCachedOwnerCharacter = null;
        m_pStaminaComponent = null;
        m_pCompartmentAccess = null;
        m_pAnimComponent = null;
        m_pCachedInventoryComponent = null;
        
        // 清理子模块引用（ref 类型可延迟 GC）
        m_pJumpVaultDetector = null;
        m_pStanceTransitionManager = null;
        m_pExerciseTracker = null;
        m_pCollapseTransition = null;
        m_pSlopeSpeedTransition = null;
        m_pTerrainDetector = null;
        m_pMudSlipRunner = null;
        m_pEnvironmentFactor = null;
        m_pFatigueSystem = null;
        m_pEncumbranceCache = null;
        m_pUISignalBridge = null;
        m_pEpocState = null;
        m_pNetworkSyncManager = null;
        m_pAnaerobicState = null;
        m_pMetabolicLimiter = null;
        m_pAnaerobicNetworkComponent = null;
    }

    protected void RSS_UpdateTacticalSprintState()
    {
        float currentTimeSprint = GetGame().GetWorld().GetWorldTime() / 1000.0;
        bool isSprintingNow = IsSprinting();
        int phaseNow = GetCurrentMovementPhase();
        bool isSprintActive = isSprintingNow || (phaseNow == 3);
        bool nextLastWasSprinting = m_bLastWasSprinting;
        float nextSprintCooldownUntil = m_fSprintCooldownUntil;
        float nextSprintStartTime = m_fSprintStartTime;
        SCR_PlayerBaseMovementHelper.UpdateTacticalSprintState(
            currentTimeSprint,
            isSprintActive,
            m_bLastWasSprinting,
            m_fSprintCooldownUntil,
            m_fSprintStartTime,
            nextLastWasSprinting,
            nextSprintCooldownUntil,
            nextSprintStartTime);
        m_bLastWasSprinting = nextLastWasSprinting;
        m_fSprintCooldownUntil = nextSprintCooldownUntil;
        m_fSprintStartTime = nextSprintStartTime;
    }
}
