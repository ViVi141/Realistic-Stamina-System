class SCR_PlayerBaseRssApiHelper
{
    static bool HasRssData(SCR_CharacterStaminaComponent staminaComponent, SCR_RSS_EnvironmentFactor environmentFactor)
    {
        return (staminaComponent != null && environmentFactor != null);
    }

    static float ClampStaminaPercent(SCR_CharacterStaminaComponent staminaComponent)
    {
        if (staminaComponent)
            return Math.Clamp(staminaComponent.GetTargetStamina(), 0.0, 1.0);
        return 1.0;
    }

    static float CalculateCurrentSpeed(vector velocity)
    {
        vector horizontalVelocity = velocity;
        horizontalVelocity[1] = 0.0;
        return Math.Min(horizontalVelocity.Length(), 7.0);
    }

    static float GetCurrentWeight(
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_CharacterInventoryStorageComponent cachedInventoryComponent)
    {
        if (encumbranceCache && encumbranceCache.IsCacheValid())
            return encumbranceCache.GetCurrentWeight();
        if (cachedInventoryComponent)
            return cachedInventoryComponent.GetTotalWeight();
        return 0.0;
    }
}

class SCR_PlayerBaseDebugHelper
{
    static void OutputStatusInfo(
        IEntity owner,
        float snapshotSpeed,
        float snapshotStaminaPercent,
        float snapshotSpeedMultiplier,
        bool isSwimming,
        bool isSprinting,
        int engineMovementPhase,
        int effectiveMovementPhase,
        SCR_CharacterControllerComponent controller)
    {
        SCR_RSS_DebugDisplay.OutputStatusInfo(
            owner,
            snapshotSpeed,
            snapshotStaminaPercent,
            snapshotSpeedMultiplier,
            isSwimming,
            isSprinting,
            engineMovementPhase,
            effectiveMovementPhase,
            controller);
    }
}

class SCR_PlayerBaseInventoryHelper
{
    static void UpdateEncumbranceCache(SCR_RSS_EncumbranceCache encumbranceCache)
    {
        if (encumbranceCache)
            encumbranceCache.UpdateCache();
    }
}

class SCR_PlayerBaseConfigHelper
{
    static string GetPlayerLabel(IEntity entity)
    {
        if (!entity)
            return "unknown";

        string entityName = entity.GetName();
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return entityName;

        int playerId = playerManager.GetPlayerIdFromControlledEntity(entity);
        if (playerId <= 0)
            return entityName;

        string playerName = playerManager.GetPlayerName(playerId);
        if (!playerName || playerName == "")
            playerName = "unknown";

        return playerName + " (id=" + playerId.ToString() + ")";
    }

    static bool IsRssDebugEnabled()
    {
        return SCR_RSS_ConfigBridge.IsDebugEnabled();
    }
}

class SCR_PlayerBaseCombatStimHelper
{
    static bool AdvancePhase(
        int currentPhase,
        float currentEndsAt,
        float worldTimeSec,
        out int nextPhase,
        out float nextEndsAt)
    {
        return SCR_CombatStimStateMachine.AdvancePhase(currentPhase, currentEndsAt, worldTimeSec, nextPhase, nextEndsAt);
    }

    static bool TryStartFromInjection(
        int currentPhase,
        float currentEndsAt,
        float worldTimeSec,
        int delayInjectionCount,
        out int nextPhase,
        out float nextEndsAt,
        out int nextDelayInjectionCount,
        out bool shouldDie)
    {
        return SCR_CombatStimStateMachine.TryStartFromInjection(
            currentPhase,
            currentEndsAt,
            worldTimeSec,
            delayInjectionCount,
            nextPhase,
            nextEndsAt,
            nextDelayInjectionCount,
            shouldDie);
    }
}

class SCR_PlayerBaseMovementHelper
{
    static bool IsSwimmingByCommand(CharacterAnimationComponent animComponent)
    {
        if (!animComponent)
            return false;

        CharacterCommandHandlerComponent handler = animComponent.GetCommandHandler();
        if (!handler)
            return false;

        if (handler.GetCommandSwim() == null)
            return false;
        return true;
    }

    static bool HasSwimInput(CharacterAnimationComponent animComponent)
    {
        if (!animComponent)
            return false;

        CharacterCommandHandlerComponent handler = animComponent.GetCommandHandler();
        if (!handler)
            return false;

        CharacterCommandMove moveCmd = handler.GetCommandMove();
        if (!moveCmd)
            return false;

        float inputAngle = 0.0;
        return moveCmd.GetCurrentInputAngle(inputAngle);
    }

    static bool IsInVehicle(CompartmentAccessComponent compartmentAccess)
    {
        if (!compartmentAccess)
            return false;
        if (!compartmentAccess.GetCompartment())
            return false;
        return true;
    }

    static void UpdateTacticalSprintState(
        float currentTimeSprint,
        bool isSprintActive,
        bool lastWasSprinting,
        float sprintCooldownUntil,
        float sprintStartTime,
        out bool nextLastWasSprinting,
        out float nextSprintCooldownUntil,
        out float nextSprintStartTime)
    {
        nextLastWasSprinting = lastWasSprinting;
        nextSprintCooldownUntil = sprintCooldownUntil;
        nextSprintStartTime = sprintStartTime;

        if (isSprintActive)
        {
            if (!lastWasSprinting)
            {
                if (currentTimeSprint >= sprintCooldownUntil)
                    nextSprintStartTime = currentTimeSprint;
            }
        }
        else
        {
            if (lastWasSprinting)
                nextSprintCooldownUntil = currentTimeSprint + SCR_RSS_Constants.GetTacticalSprintCooldown();
        }

        if (isSprintActive && nextSprintStartTime >= 0.0)
        {
            float burstDuration = SCR_RSS_Constants.GetTacticalSprintBurstDuration();
            float bufferDuration = SCR_RSS_Constants.GetTacticalSprintBurstBufferDuration();
            float elapsed = currentTimeSprint - nextSprintStartTime;
            if (burstDuration > 0.0)
            {
                if (bufferDuration >= 0.0)
                {
                    if (elapsed >= burstDuration + bufferDuration)
                    {
                        nextSprintCooldownUntil = currentTimeSprint + SCR_RSS_Constants.GetTacticalSprintCooldown();
                        nextSprintStartTime = -1.0;
                    }
                }
            }
        }

        nextLastWasSprinting = isSprintActive;
    }
}

class SCR_PlayerBaseNetworkHelper
{
    static float GetServerWeight(IEntity owner, SCR_RSS_EncumbranceCache encumbranceCache)
    {
        if (encumbranceCache && encumbranceCache.IsCacheValid())
            return encumbranceCache.GetCurrentWeight();

        if (!owner)
            return 0.0;

        SCR_CharacterInventoryStorageComponent inventoryComponent = SCR_CharacterInventoryStorageComponent.Cast(
            owner.FindComponent(SCR_CharacterInventoryStorageComponent));
        if (inventoryComponent)
            return inventoryComponent.GetTotalWeight();
        return 0.0;
    }

    static bool IsValidClientReportTimestamp(float currentTime, float clientTimestamp, out float timestampDelta)
    {
        timestampDelta = 0.0;
        if (clientTimestamp <= 0.0)
            return true;

        timestampDelta = currentTime - clientTimestamp;
        const float maxValidRtt = 2.0;
        if (timestampDelta > maxValidRtt)
            return false;
        if (timestampDelta < -0.5)
            return false;
        return true;
    }

    static float CalculateEncumbrancePenaltyFallback(float serverWeight)
    {
        float effectiveWeight = Math.Max(serverWeight - SCR_RSS_Constants.BASE_WEIGHT, 0.0);
        float bodyMassPercent = effectiveWeight / SCR_RSS_Constants.CHARACTER_WEIGHT;
        float ratio = Math.Clamp(bodyMassPercent, 0.0, 2.0);
        float rawPenalty = 0.0;
        if (ratio <= 0.3)
        {
            rawPenalty = 0.15 * ratio;
        }
        else
        {
            if (ratio <= 0.6)
            {
                float segmentMid = ratio - 0.3;
                rawPenalty = 0.045 + 0.35 * Math.Pow(segmentMid, 1.5);
            }
            else
            {
                float segmentHigh = ratio - 0.6;
                rawPenalty = 0.25 + 0.65 * (segmentHigh * segmentHigh);
            }
        }

        float coeff = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyCoeff();
        rawPenalty = rawPenalty * (coeff / 0.20);
        float maxPenalty = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
        return Math.Clamp(rawPenalty, 0.0, maxPenalty);
    }

    static bool IsValidServerSyncTimestamp(float currentTime, float serverTimestamp, out float timestampDelta)
    {
        timestampDelta = 0.0;
        if (serverTimestamp <= 0.0)
            return true;

        timestampDelta = currentTime - serverTimestamp;
        const float maxValidOneWayLatency = 1.0;
        if (timestampDelta > maxValidOneWayLatency)
            return false;
        return true;
    }

    static DebugInfoParams BuildVehicleDebugInfoParams(
        IEntity owner,
        float vehicleStaminaPercent,
        float vehicleDebugWeight,
        float currentWetWeight,
        float vehicleTimeToDepleteSec,
        float vehicleTimeToFullSec,
        SCR_RSS_TerrainDetector terrainDetector,
        SCR_RSS_EnvironmentFactor environmentFactor,
        SCR_RSS_StanceTransitionManager stanceTransitionManager)
    {
        DebugInfoParams vehicleParams = new DebugInfoParams();
        vehicleParams.owner = owner;
        vehicleParams.movementTypeStr = "Vehicle";
        vehicleParams.staminaPercent = vehicleStaminaPercent;
        vehicleParams.baseSpeedMultiplier = 1.0;
        vehicleParams.encumbranceSpeedPenalty = 0.0;
        vehicleParams.finalSpeedMultiplier = 1.0;
        vehicleParams.gradePercent = 0.0;
        vehicleParams.slopeAngleDegrees = 0.0;
        vehicleParams.isSprinting = false;
        vehicleParams.currentMovementPhase = 0;
        vehicleParams.debugCurrentWeight = vehicleDebugWeight;
        vehicleParams.combatEncumbrancePercent = 0.0;
        vehicleParams.terrainDetector = terrainDetector;
        vehicleParams.environmentFactor = environmentFactor;
        vehicleParams.heatStressMultiplier = 1.0;
        vehicleParams.rainWeight = 0.0;
        vehicleParams.swimmingWetWeight = currentWetWeight;
        vehicleParams.currentSpeed = 0.0;
        vehicleParams.isSwimming = false;
        vehicleParams.stanceTransitionManager = stanceTransitionManager;
        vehicleParams.timeToDepleteSec = vehicleTimeToDepleteSec;
        vehicleParams.timeToFullSec = vehicleTimeToFullSec;
        return vehicleParams;
    }
}
