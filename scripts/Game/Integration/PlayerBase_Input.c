// PlayerBase split: 输入 / 游泳 / 战术冲刺
modded class SCR_CharacterControllerComponent
{
    protected bool IsSwimmingByCommand()
    {
        return SCR_PlayerBaseMovementHelper.IsSwimmingByCommand(m_pAnimComponent);
    }

    protected bool HasSwimInput()
    {
        return SCR_PlayerBaseMovementHelper.HasSwimInput(m_pAnimComponent);
    }

    override void OnControlledByPlayer(IEntity owner, bool controlled)
    {
        super.OnControlledByPlayer(owner, controlled);

        if (m_bIsDeleted)
            return;

        if (m_pTerrainDetector)
            m_pTerrainDetector.SetIsAiEntity(!controlled);

        if (controlled)
            EnsureRssStaminaLoopIfNeeded();
        
        if (controlled && owner == SCR_PlayerController.GetLocalControlledEntity())
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                inputManager.AddActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.AddActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.AddActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
                
                if (IsRssDebugEnabled())
                    SCR_RSS_Logger.Debug("[RSS] 跳跃动作监听器已添加 / Jump Action Listener Added");
            }
            
            GetGame().GetCallqueue().CallLater(InitStaminaHUD, 1000, false);
        }
        else
        {
            InputManager inputManager = GetGame().GetInputManager();
            if (inputManager)
            {
                inputManager.RemoveActionListener("Jump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJump", EActionTrigger.DOWN, OnJumpActionTriggered);
                inputManager.RemoveActionListener("CharacterJumpClimb", EActionTrigger.DOWN, OnJumpActionTriggered);
            }
            
            SCR_StaminaHUDComponent.Destroy();
        }
    }

    void InitStaminaHUD()
    {
        if (m_bIsDeleted)
            return;
        if (!GetOwner())
            return;
        SCR_StaminaHUDComponent.SyncHintDisplayWithSettings();
    }

    void OnJumpActionTriggered(float value = 0.0, EActionTrigger trigger = 0)
    {
        IEntity owner = GetOwner();
        if (!owner || owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        bool isInVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(m_pCompartmentAccess);
        
        if (!isInVehicle && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            if (IsRssDebugEnabled())
                SCR_RSS_Logger.Debug("[RSS] 动作监听器检测到跳跃输入！/ Action Listener Detected Jump Input!");
        }
    }

    override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
    {
        super.OnPrepareControls(owner, am, dt, player);
        
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        bool isInVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(m_pCompartmentAccess);
        
        if (!isInVehicle && am.GetActionTriggered("Jump") && m_pJumpVaultDetector)
        {
            m_pJumpVaultDetector.SetJumpInputTriggered(true);
            if (IsRssDebugEnabled())
                SCR_RSS_Logger.Debug("[RSS] OnPrepareControls 检测到跳跃输入！/ OnPrepareControls Detected Jump Input!");
        }
    }

    protected bool ShouldProcessStaminaUpdate()
    {
        IEntity owner = GetOwner();
        if (!owner)
            return false;
        if (owner == SCR_PlayerController.GetLocalControlledEntity())
            return true;
        IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
        if (controlled && m_pCompartmentAccess)
        {
            BaseCompartmentSlot slot = m_pCompartmentAccess.GetCompartment();
            if (slot)
            {
                IEntity vehicle = slot.GetVehicle();
                if (vehicle == controlled)
                    return true;
            }
        }
        if (Replication.IsServer() && !IsPlayerControlled())
        {
            // 完全禁用 AI RSS 计算：交还引擎处理体力
            if (StaminaConfigBridge.IsAiAllCalcDisabled())
                return false;
            return true;
        }
        return false;
    }
}
