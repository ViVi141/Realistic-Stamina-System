class SCR_PlayerBaseRssApiHelper
{
    static bool HasRssData(SCR_CharacterStaminaComponent staminaComponent, EnvironmentFactor environmentFactor)
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
        EncumbranceCache encumbranceCache,
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
    static void LogInitialEnergyCoeff(float coeff)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] 初始 energie->stamina coeff = %1", coeff));
    }

    static void LogCriticalStaminaEvent(float staminaPercent)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] Critical stamina event reported (stamina=%1)", staminaPercent));
    }

    static void LogJumpCost(float jumpCost)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] Jump Cost: -%1%%",
            Math.Round(jumpCost * 100.0 * 10.0) / 10.0));
    }

    static void LogVaultCost(float vaultCost)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] Vault Cost: -%1%%",
            Math.Round(vaultCost * 100.0 * 10.0) / 10.0));
    }

    static void LogAiStatus(
        IEntity owner,
        string stateStr,
        float staminaPercent,
        float fatigueVal,
        float currentWeight,
        float finalSpeedMultiplier,
        float currentSpeed,
        string movementStr,
        string speedSource)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] AI: %1 | 状态=%2 体力=%3%% 疲劳=%4% 负重=%5kg 速度倍=%6 速度=%7m/s %8 | %9",
            owner.GetName(),
            stateStr,
            Math.Round(staminaPercent * 100.0).ToString(),
            Math.Round(fatigueVal * 100.0).ToString(),
            Math.Round(currentWeight * 10.0) / 10.0,
            Math.Round(finalSpeedMultiplier * 100.0) / 100.0,
            Math.Round(currentSpeed * 10.0) / 10.0,
            movementStr,
            speedSource));
    }

    static void LogAiGroupSpread(SCR_AIGroup group, float spread, int aliveCount)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] Group: id=%1 分散=%2m 成员=%3",
            group.GetGroupID().ToString(),
            Math.Round(spread * 10.0) / 10.0,
            aliveCount.ToString()));
    }

    static void LogCombatStimTickTransition(int oldPhase, int newPhase, float oldEndsAt, float newEndsAt, float nowSec)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS][CombatStim] TickTransition: phase %1 -> %2, endsAt %3 -> %4, now=%5",
            oldPhase,
            newPhase,
            Math.Round(oldEndsAt * 10.0) / 10.0,
            Math.Round(newEndsAt * 10.0) / 10.0,
            Math.Round(nowSec * 10.0) / 10.0));
    }

    static void LogCombatStimInject(int oldPhase, int nextPhase, bool shouldDie, float nowSec)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS][CombatStim][Server] Inject: oldPhase=%1 => nextPhase=%2 shouldDie=%3 now=%4",
            oldPhase, nextPhase, shouldDie, Math.Round(nowSec * 10.0) / 10.0));
    }

    static void LogAdminUpdateConfigDenied(int playerId)
    {
        SCR_RSS_Logger.Warn(string.Format("[RSS] RPC_AdminUpdateConfig: access denied for playerId=%1", playerId));
    }

    static void LogStaleSpeedValidation(float timestampDelta)
    {
        SCR_RSS_Logger.Debug(string.Format("[RSS] Stale speed validation ignored (latency: %1s)", timestampDelta));
    }

    static void OutputStatusInfo(
        IEntity owner,
        float lastSecondSpeed,
        float lastStaminaPercent,
        float lastSpeedMultiplier,
        bool isSwimming,
        bool isSprinting,
        int currentMovementPhase,
        SCR_CharacterControllerComponent controller)
    {
        DebugDisplay.OutputStatusInfo(
            owner,
            lastSecondSpeed,
            lastStaminaPercent,
            lastSpeedMultiplier,
            isSwimming,
            isSprinting,
            currentMovementPhase,
            controller);
    }
}

class SCR_PlayerBaseInventoryHelper
{
    static void UpdateEncumbranceCache(EncumbranceCache encumbranceCache)
    {
        if (encumbranceCache)
            encumbranceCache.UpdateCache();
    }
}

class SCR_PlayerBaseConfigHelper
{
    static string MapSelectedPresetToV5PresetName(SCR_RSS_Settings settings)
    {
        if (!settings)
            return "Standard";

        string selectedPreset = settings.m_sSelectedPreset;
        if (selectedPreset == "EliteStandard")
            return "Elite";
        if (selectedPreset == "TacticalAction")
            return "Tactical";
        return "Standard";
    }

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
        return StaminaConfigBridge.IsDebugEnabled();
    }
}

class SCR_PlayerBaseCombatStimHelper
{
    static ChimeraCharacter GetOwnerCharacter(IEntity ownerEntity)
    {
        return ChimeraCharacter.Cast(ownerEntity);
    }

    static void ApplyBleedingScaleForOwner(int phase, float bleedingBaseline, ChimeraCharacter character)
    {
        if (!character)
            return;

        SCR_CombatStimController.UpdateBleedingScale(
            phase,
            bleedingBaseline,
            character,
            bleedingBaseline);
    }

    static bool TryKillCharacterFromOverdose(ChimeraCharacter character, float bleedingBaseline)
    {
        if (!character)
            return false;

        SCR_CombatStimController.ResetBleedingScaleBeforeKill(
            character,
            bleedingBaseline,
            bleedingBaseline);

        SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
        if (!damageManager)
            return false;

        damageManager.Kill(Instigator.CreateInstigator(character));
        return true;
    }

    static bool TryBuildTickTransition(
        int currentPhase,
        float currentEndsAt,
        float worldTimeSec,
        out int oldPhase,
        out float oldEndsAt,
        out int nextPhase,
        out float nextEndsAt,
        out int nextDelayInjectionCount,
        int currentDelayInjectionCount)
    {
        oldPhase = currentPhase;
        oldEndsAt = currentEndsAt;
        nextPhase = currentPhase;
        nextEndsAt = currentEndsAt;
        nextDelayInjectionCount = currentDelayInjectionCount;

        if (!AdvancePhase(currentPhase, currentEndsAt, worldTimeSec, nextPhase, nextEndsAt))
            return false;

        if (nextPhase != ERSS_CombatStimPhase.DELAY)
            nextDelayInjectionCount = 0;

        return true;
    }

    static bool AdvancePhase(
        int currentPhase,
        float currentEndsAt,
        float worldTimeSec,
        out int nextPhase,
        out float nextEndsAt)
    {
        return SCR_CombatStimStateMachine.AdvancePhase(currentPhase, currentEndsAt, worldTimeSec, nextPhase, nextEndsAt);
    }

    static bool TryBuildInjectionTransition(
        int currentPhase,
        float currentEndsAt,
        float worldTimeSec,
        int currentDelayInjectionCount,
        out int nextPhase,
        out float nextEndsAt,
        out int nextDelayInjectionCount,
        out bool shouldDie)
    {
        nextPhase = currentPhase;
        nextEndsAt = currentEndsAt;
        nextDelayInjectionCount = currentDelayInjectionCount;
        shouldDie = false;

        if (!TryStartFromInjection(
                currentPhase,
                currentEndsAt,
                worldTimeSec,
                currentDelayInjectionCount,
                nextPhase,
                nextEndsAt,
                nextDelayInjectionCount,
                shouldDie))
            return false;

        if (nextPhase != ERSS_CombatStimPhase.DELAY)
            nextDelayInjectionCount = 0;

        return true;
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
                nextSprintCooldownUntil = currentTimeSprint + StaminaConstants.GetTacticalSprintCooldown();
        }

        if (isSprintActive && nextSprintStartTime >= 0.0)
        {
            float burstDuration = StaminaConstants.GetTacticalSprintBurstDuration();
            float bufferDuration = StaminaConstants.GetTacticalSprintBurstBufferDuration();
            float elapsed = currentTimeSprint - nextSprintStartTime;
            if (burstDuration > 0.0)
            {
                if (bufferDuration >= 0.0)
                {
                    if (elapsed >= burstDuration + bufferDuration)
                    {
                        nextSprintCooldownUntil = currentTimeSprint + StaminaConstants.GetTacticalSprintCooldown();
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
    static float GetServerWeight(IEntity owner, EncumbranceCache encumbranceCache)
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
        float effectiveWeight = Math.Max(serverWeight - StaminaConstants.BASE_WEIGHT, 0.0);
        float bodyMassPercent = effectiveWeight / StaminaConstants.CHARACTER_WEIGHT;
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

        float coeff = StaminaConfigBridge.GetEncumbranceSpeedPenaltyCoeff();
        rawPenalty = rawPenalty * (coeff / 0.20);
        float maxPenalty = StaminaConfigBridge.GetEncumbranceSpeedPenaltyMax();
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
        TerrainDetector terrainDetector,
        EnvironmentFactor environmentFactor,
        StanceTransitionManager stanceTransitionManager)
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
