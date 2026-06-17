// RPC 处理器辅助模块
// 从 PlayerBase.c 拆分，处理 RPC 消息的业务逻辑

class SCR_PlayerBaseRpcHandler
{
    static float ProcessClientReport_ValidateStamina(
        float staminaPercent,
        float weight,
        float currentTime,
        float clientTimestamp,
        bool isCriticalData,
        SCR_RSS_NetworkSyncManager networkSync,
        bool isDebugEnabled,
        out bool shouldIgnore)
    {
        shouldIgnore = false;

        float timestampDelta = 0.0;
        bool isValidTimestamp = SCR_PlayerBaseNetworkHelper.IsValidClientReportTimestamp(
            currentTime, clientTimestamp, timestampDelta);
        if (!isValidTimestamp)
        {
            if (isDebugEnabled)
            {
                if (timestampDelta > 0.0)
                    PrintFormat("[RSS] Stale stamina report ignored (timestamp delta: %1s)", timestampDelta);
                else
                    PrintFormat("[RSS] Stale stamina report ignored (time regression: %1s)", timestampDelta);
            }
            shouldIgnore = true;
            return 0.0;
        }

        if (networkSync && !networkSync.AcceptClientReport(currentTime, isCriticalData))
        {
            if (isDebugEnabled)
                PrintFormat("[RSS] Ignored too-frequent stamina report (time=%1)", currentTime);
            shouldIgnore = true;
            return 0.0;
        }

        float clampedStamina = Math.Clamp(staminaPercent, 0.0, 1.0);

        if (networkSync)
        {
            float lastReported = networkSync.GetLastReportedStaminaPercent();
            if (Math.AbsFloat(clampedStamina - lastReported) > 0.5 && isDebugEnabled)
                PrintFormat("[RSS] Suspicious stamina jump reported: last=%1 -> reported=%2", lastReported, clampedStamina);
            networkSync.UpdateReportedState(clampedStamina, weight);
        }

        return clampedStamina;
    }

    static float ProcessClientReport_CalculateValidation(
        float clampedStamina,
        float serverWeight,
        float encumbrancePenalty,
        bool isSprinting,
        int movementPhase,
        bool isExhausted,
        bool canSprint,
        float currentSpeed,
        float slopeAngleDegrees,
        float sprintStartTime)
    {
        float validated = SCR_RSS_UpdateCoordinator.CalculateFinalSpeedMultiplierFromInputs(
            clampedStamina, encumbrancePenalty, isSprinting, movementPhase,
            isExhausted, canSprint, currentSpeed, slopeAngleDegrees, sprintStartTime);
        return Math.Clamp(validated, 0.15, 3.0);
    }

    static bool ProcessClientReport_ShouldSync(
        SCR_RSS_NetworkSyncManager networkSync,
        float validated,
        float currentTime,
        out float speedDiff)
    {
        speedDiff = 0.0;
        if (!networkSync)
            return false;

        if (!networkSync.HasServerValidation())
            return true;

        speedDiff = Math.AbsFloat(validated - networkSync.GetServerValidatedSpeedMultiplier());
        return networkSync.ProcessDeviation(speedDiff, currentTime);
    }

    static bool IsValidServerSyncTimestamp(float currentTime, float serverTimestamp, out float timestampDelta)
    {
        return SCR_PlayerBaseNetworkHelper.IsValidServerSyncTimestamp(currentTime, serverTimestamp, timestampDelta);
    }
}
