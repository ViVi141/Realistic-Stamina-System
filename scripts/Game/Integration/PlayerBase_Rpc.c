// PlayerBase split: RPC 与网络同步
modded class SCR_CharacterControllerComponent
{
    float RSS_GetLastAppliedSpeedMultiplier()
    {
        return m_fLastRssSpeedMultiplierApplied;
    }

    ERSS_AIStaminaState RSS_GetAIStaminaState()
    {
        if (m_pAIManager)
            return m_pAIManager.GetStaminaState();
        return ERSS_AIStaminaState.FRESH;
    }

    void RSS_SetAIStaminaState(ERSS_AIStaminaState state)
    {
        // 状态现由 AIManager 管理，此 setter 保留兼容性但不直接写入
        // 实质状态通过 m_pAIManager.Tick() 内部的状态机维护
    }

    protected string GetPlayerLabel(IEntity entity)
    {
        return SCR_PlayerBaseConfigHelper.GetPlayerLabel(entity);
    }

    protected bool IsRssDebugEnabled()
    {
        return SCR_PlayerBaseConfigHelper.IsRssDebugEnabled();
    }

    //! 客户端上报：仅用于数据导出/对照（m_bDataExportEnabled），非强反作弊与玩法权威判据。
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_ClientReportStamina(float staminaPercent, float weight, float clientTimestamp, bool isCriticalData)
    {
        if (!Replication.IsServer())
            return;
        if (!SCR_RSS_ConfigManager.GetSettings() || !SCR_RSS_ConfigManager.GetSettings().m_bDataExportEnabled)
            return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;

        bool shouldIgnore = false;
        float clampedStamina = SCR_PlayerBaseRpcHandler.ProcessClientReport_ValidateStamina(
            staminaPercent, weight, currentTime, clientTimestamp,
            m_pNetworkSyncManager, IsRssDebugEnabled(), shouldIgnore);
        if (shouldIgnore)
            return;

        float serverWeight = SCR_PlayerBaseNetworkHelper.GetServerWeight(GetOwner(), m_pEncumbranceCache);
        float encPenalty = SCR_PlayerBaseNetworkHelper.CalculateEncumbrancePenaltyFallback(serverWeight);
        if (m_pEncumbranceCache && m_pEncumbranceCache.IsCacheValid())
            encPenalty = m_pEncumbranceCache.GetSpeedPenalty();

        IEntity ownerEnt = GetOwner();
        bool shouldSuppressSlopeServer = (m_pEnvironmentFactor && ownerEnt && m_pEnvironmentFactor.ShouldSuppressTerrainSlopeForEntity(ownerEnt));

        float slopeAngleDegrees = 0.0;
        if (!shouldSuppressSlopeServer)
            slopeAngleDegrees = SpeedCalculator.GetSlopeAngle(this, m_pEnvironmentFactor, GetVelocity());

        float rawSlopeServer = SpeedCalculator.GetRawSlopeAngle(this, GetVelocity());
        if (shouldSuppressSlopeServer && Math.AbsFloat(rawSlopeServer) > 0.0)
            encPenalty = encPenalty * StaminaConstants.INDOOR_STAIRS_ENCUMBRANCE_SPEED_FACTOR;

        float validated = SCR_PlayerBaseRpcHandler.ProcessClientReport_CalculateValidation(
            clampedStamina, serverWeight, encPenalty,
            IsSprinting(), GetCurrentMovementPhase(),
            RealisticStaminaSpeedSystem.IsExhausted(clampedStamina),
            RealisticStaminaSpeedSystem.CanSprint(clampedStamina),
            SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(GetVelocity()),
            slopeAngleDegrees, GetSprintStartTime());

        if (m_pNetworkSyncManager)
        {
            currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            float speedDiff = 0.0;
            if (SCR_PlayerBaseRpcHandler.ProcessClientReport_ShouldSync(
                    m_pNetworkSyncManager, validated, currentTime, speedDiff))
            {
                m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(validated);
                Rpc(RPC_ServerSyncSpeedMultiplier, validated, currentTime);
            }
        }
    }

    //! 客户端 → 服务端：必须用本方法包装 Rpc()，依据引擎源码约定，而非猜测。
    //! - GenericComponent：Rpc 为 proto protected（外部/UI 类无法调用 Rpc）。
    //! - 官方用法示例：SCR_ChimeraCharacter.SetIllumination → Rpc(RPC_SetIllumination_S, …)；
    //!   RPC_* 标 [RplRpc(…, RplRcver.Server)]，由 Rpc() 发到服务端执行。
    //! 若从 UI 直接调用 RPC_AdminUpdateConfig(...)，等同本地普通函数调用，首行 IsServer 守卫即 return，不会走网络。
    void RSS_RequestAdminConfigUpdate(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        if (Replication.IsServer())
            return;
        Rpc(RPC_AdminUpdateConfig, preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RPC_AdminUpdateConfig(string preset, bool debugLog, bool hintDisplay, bool dataExport, bool mudSlip, bool aiCombat, bool disableAI, bool disableAIStamina)
    {
        if (!Replication.IsServer())
            return;
        IEntity owner = GetOwner();
        if (!owner) return;

        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm) return;

        int pid = pm.GetPlayerIdFromControlledEntity(owner);
        if (pid <= 0)
        {
            SCR_RSS_Logger.Warn("[RSS] RPC_AdminUpdateConfig: no controlling player for entity");
            return;
        }
        if (!pm.HasPlayerRole(pid, EPlayerRole.ADMINISTRATOR)
            && !pm.HasPlayerRole(pid, EPlayerRole.SESSION_ADMINISTRATOR)
            && !pm.HasPlayerRole(pid, EPlayerRole.GAME_MASTER))
        {
            SCR_PlayerBaseDebugHelper.LogAdminUpdateConfigDenied(pid);
            return;
        }

        SCR_RSS_ConfigManager.AdminApplyAndSave(preset, debugLog, hintDisplay, dataExport, mudSlip, aiCombat, disableAI, disableAIStamina);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RPC_ServerSyncSpeedMultiplier(float speedMultiplier, float serverTimestamp)
    {
        if (Replication.IsServer() || !m_pNetworkSyncManager)
            return;

        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
        float timestampDelta = 0.0;
        if (!SCR_PlayerBaseRpcHandler.IsValidServerSyncTimestamp(currentTime, serverTimestamp, timestampDelta))
        {
            if (IsRssDebugEnabled())
                SCR_PlayerBaseDebugHelper.LogStaleSpeedValidation(timestampDelta);
            return;
        }
        m_pNetworkSyncManager.SetServerValidatedSpeedMultiplier(speedMultiplier);
    }
}
