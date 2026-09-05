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

    //! 进服/生成窗口启发式：Game/World 交叉 + Anim 存在才继续 tick。
    //! 不要求 Physics：GetPhysics 非空仍可能对 GetVelocity AV。
    //! @param owner 角色实体
    //! @param controller 角色控制器
    //! @return true 表示可继续体力 tick（测速须走位置差分，勿调 GetVelocity）
    static bool IsCharacterMotionReady(IEntity owner, SCR_CharacterControllerComponent controller)
    {
        if (!owner || !controller)
            return false;
        if (!SCR_RSS_RuntimeGuard.IsEntityWorldUsable(owner))
            return false;
        if (!controller.GetAnimationComponent())
            return false;
        return true;
    }

    //! 启发式测速：优先 CharacterMovement.GetVelocityWS（引擎已算）；
    //! 失败则用 fallback（通常为位置差分）。永不 Physics.GetVelocity。
    //! @param owner 角色实体
    //! @param fallback 已由位置差分等算出的速度
    //! @return 可用速度；不可用则为 Zero
    static vector SampleEntityVelocity(IEntity owner, vector fallback)
    {
        if (!SCR_RSS_RuntimeGuard.IsEntityWorldUsable(owner))
            return vector.Zero;

        vector engineVel;
        if (SCR_RSS_EngineReuse.TryGetVelocityWS(owner, engineVel))
        {
            vector horiz = engineVel;
            horiz[1] = 0.0;
            float hLen = horiz.Length();
            if (hLen > 0.01 && hLen <= 7.5)
                return engineVel;
        }

        // 二次启发式：Physics 句柄不稳定则仍只用 fallback
        if (!SCR_RSS_RuntimeGuard.IsPhysicsHandlePresent(owner))
            return fallback;
        return fallback;
    }

    //! 无 fallback 时的兼容入口：仅启发式校验，永不调用 GetVelocity。
    //! @param owner 角色实体
    //! @return 恒为 Zero（调用方应改用带 fallback 重载或位置差分）
    static vector SampleEntityVelocity(IEntity owner)
    {
        return SampleEntityVelocity(owner, vector.Zero);
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
