// PlayerBase split: RSS 公开 API / 速度采样 / 库存回调
modded class SCR_CharacterControllerComponent
{
    bool HasRssData()
    {
        return SCR_PlayerBaseRssApiHelper.HasRssData(m_pStaminaComponent, m_pEnvironmentFactor);
    }

    float GetRssStaminaPercent()
    {
        return SCR_PlayerBaseRssApiHelper.ClampStaminaPercent(m_pStaminaComponent);
    }

    float GetRssSpeedMultiplier()
    {
        return m_fLastSpeedMultiplier;
    }

    float GetRssCurrentSpeed()
    {
        return SCR_PlayerBaseRssApiHelper.CalculateCurrentSpeed(GetVelocity());
    }

    int GetRssMovementPhase()
    {
        return GetCurrentMovementPhase();
    }

    bool GetRssIsSprinting()
    {
        return IsSprinting();
    }

    bool GetRssIsExhausted()
    {
        float stamina = GetRssStaminaPercent();
        return RealisticStaminaSpeedSystem.IsExhausted(stamina);
    }

    bool GetRssIsSwimming()
    {
        return SwimmingStateManager.IsSwimming(this);
    }

    float GetRssCurrentWeight()
    {
        return SCR_PlayerBaseRssApiHelper.GetCurrentWeight(m_pEncumbranceCache, m_pCachedInventoryComponent);
    }

    EnvironmentFactor GetRssEnvironmentFactor()
    {
        return m_pEnvironmentFactor;
    }

    float GetOriginalEngineMaxSpeed_Run()
    {
        return GetDynamicOriginalEngineMaxSpeed(2);
    }

    float GetOriginalEngineMaxSpeed_Sprint()
    {
        return GetDynamicOriginalEngineMaxSpeed(3);
    }

    protected float GetDynamicOriginalEngineMaxSpeed(int movementPhase)
    {
        if (!m_pAnimComponent)
        {
            if (movementPhase == 3)
                return RealisticStaminaSpeedSystem.GAME_MAX_SPEED;
            else
                return RealisticStaminaSpeedSystem.GAME_MAX_SPEED * RealisticStaminaSpeedSystem.TARGET_RUN_SPEED_MULTIPLIER;
        }
        
        float currentMultiplier = m_fLastSpeedMultiplier;
        IEntity ownerEnt = GetOwner();

        SCR_RSS_CharacterSpeedBridge.ApplyStaminaSpeedLimit(ownerEnt, 1.0);

        float realOriginalSpeed = m_pAnimComponent.GetMaxSpeed(1.0, 0.0, movementPhase);

        SCR_RSS_CharacterSpeedBridge.ApplyStaminaSpeedLimit(ownerEnt, currentMultiplier);
        
        return realOriginalSpeed;
    }

    void CollectSpeedSample()
    {
        if (m_bIsDeleted)
            return;
        IEntity owner = GetOwner();
        if (!owner)
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }
        
        ChimeraCharacter character = ChimeraCharacter.Cast(owner);
        if (!character)
            return;
        
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        if (!GetGame())
        {
            m_bIsDeleted = true;
            RSS_NotifyEntityDeleting();
            return;
        }
        World world = GetGame().GetWorld();
        if (!world)
            return;
        
        vector velocity = GetVelocity();
        vector velocityXZ = vector.Zero;
        velocityXZ[0] = velocity[0];
        velocityXZ[2] = velocity[2];
        float speedHorizontal = velocityXZ.Length(); // 水平速度（米/秒）
        
        if (m_bHasPreviousSpeed)
        {
            DisplayStatusInfo();
        }
        
        m_fLastSecondSpeed = m_fCurrentSecondSpeed;
        m_fCurrentSecondSpeed = speedHorizontal;
        m_bHasPreviousSpeed = true; // 标记已有上一秒的数据
        
        if (IsRssDebugEnabled())
            GetGame().GetCallqueue().CallLater(CollectSpeedSample, SPEED_SAMPLE_INTERVAL_MS, false);
    }

    void DisplayStatusInfo()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return;
        
        bool isSwimming = IsSwimmingByCommand();
        bool isSprinting = IsSprinting();
        int currentMovementPhase = GetCurrentMovementPhase();
        
        SCR_PlayerBaseDebugHelper.OutputStatusInfo(
            owner,
            m_fLastSecondSpeed,
            m_fLastStaminaPercent,
            m_fLastSpeedMultiplier,
            isSwimming,
            isSprinting,
            currentMovementPhase,
            this);
    }

    void OnItemRemovedFromInventory()
    {
        SCR_PlayerBaseInventoryHelper.UpdateEncumbranceCache(m_pEncumbranceCache);
    }

    void OnItemAddedToInventory()
    {
        SCR_PlayerBaseInventoryHelper.UpdateEncumbranceCache(m_pEncumbranceCache);
    }
}
