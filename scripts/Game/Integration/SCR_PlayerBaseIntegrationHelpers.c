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

    //! 进服/生成窗口：实体已有 Owner 但尚未挂世界或物理未激活时，
    //! CharacterController.GetVelocity / IsSprinting 等 native 易 Access Violation。
    //! @param owner 角色实体
    //! @param controller 角色控制器
    //! @return true 表示可安全读运动相关 native
    static bool IsCharacterMotionReady(IEntity owner, SCR_CharacterControllerComponent controller)
    {
        if (!owner || !controller)
            return false;
        if (!owner.GetWorld())
            return false;
        if (!owner.GetPhysics())
            return false;
        if (!controller.GetAnimationComponent())
            return false;
        return true;
    }

    //! 安全测速：优先 Physics.GetVelocity（已做 null 检查）；不可用则 Zero。
    //! 避免 CharacterController.GetVelocity 在生成窗口崩溃。
    //! @param owner 角色实体
    //! @return 世界空间速度（m/s），不可用时为 Zero
    static vector SampleEntityVelocity(IEntity owner)
    {
        if (!owner)
            return vector.Zero;
        if (!owner.GetWorld())
            return vector.Zero;
        Physics physics = owner.GetPhysics();
        if (!physics)
            return vector.Zero;
        return physics.GetVelocity();
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

    static RSS_DebugInfoParams BuildVehicleRSS_DebugInfoParams(
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
        RSS_DebugInfoParams vehicleParams = new RSS_DebugInfoParams();
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
