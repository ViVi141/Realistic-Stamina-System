//! 水壶喝水：走官方消耗品安全路径（角色 BindCommand + TryUseItemOverrideParams）。
//! 禁止 SyncWithCharacter / 物品侧 CallCommand —— 会 Access Violation（见 6.2.24 crash.log:85）。
//!
//! 命令用 CMD_Item_Action（player_main 与水壶图均有）。双播是资产设计问题，另议。
//! 【6.3.0】暂无体力效果：军火库保留仅为后续「体力补充食物」集成测试用开发产物，不回补 STA/W′。

[BaseContainerProps()]
class SCR_RSS_CanteenDrinkEffect : SCR_ConsumableEffectBase
{
    protected static const string CANTEEN_DRINK_COMMAND = "CMD_Item_Action";
    protected static const float CANTEEN_MAX_ANIM_LENGTH = 30.0;

    //------------------------------------------------------------------------------------------------
    void SCR_RSS_CanteenDrinkEffect()
    {
        m_eConsumableType = SCR_EConsumableType.DRINK;
        m_bDeleteOnUse = false;
        m_fApplyToSelfDuration = 5.0;
        m_fApplyToOtherDuration = 5.0;
    }

    //------------------------------------------------------------------------------------------------
    override bool CanApplyEffect(notnull IEntity target, notnull IEntity user, out SCR_EConsumableFailReason failReason)
    {
        ChimeraCharacter character = ChimeraCharacter.Cast(target);
        if (!character)
        {
            failReason = SCR_EConsumableFailReason.UNKOWN;
            return false;
        }

        CharacterControllerComponent controller = character.GetCharacterController();
        if (!controller)
        {
            failReason = SCR_EConsumableFailReason.UNKOWN;
            return false;
        }

        if (controller.GetLifeState() != ECharacterLifeState.ALIVE)
        {
            failReason = SCR_EConsumableFailReason.UNKOWN;
            return false;
        }

        if (controller.IsUsingItem())
        {
            failReason = SCR_EConsumableFailReason.UNKOWN;
            return false;
        }

        failReason = SCR_EConsumableFailReason.NONE;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    override bool ActivateEffect(IEntity target, IEntity user, IEntity item, ItemUseParameters animParams = null)
    {
        ItemUseParameters localAnimParams = animParams;
        if (!localAnimParams)
            localAnimParams = GetAnimationParameters(item, target);

        if (!localAnimParams)
            return false;

        return super.ActivateEffect(target, user, item, localAnimParams);
    }

    //------------------------------------------------------------------------------------------------
    override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
    {
        InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
        if (itemComp)
            itemComp.RequestUserLock(user, false);

        if (SCR_RSS_ConfigBridge.IsDebugEnabled())
            Print("[RSS] Canteen drink finished");
    }

    //------------------------------------------------------------------------------------------------
    override bool UpdateAnimationCommands(IEntity user)
    {
        ChimeraCharacter character = ChimeraCharacter.Cast(user);
        if (!character)
            return false;

        CharacterAnimationComponent animationComponent = character.GetAnimationComponent();
        if (!animationComponent)
            return false;

        m_iPlayerApplyToSelfCmdId = animationComponent.BindCommand(CANTEEN_DRINK_COMMAND);
        if (m_iPlayerApplyToSelfCmdId < 0)
        {
            Print("[RSS] Canteen BindCommand(CMD_Item_Action) failed on character", LogLevel.ERROR);
            return false;
        }

        return true;
    }

    //------------------------------------------------------------------------------------------------
    override ItemUseParameters GetAnimationParameters(IEntity item, notnull IEntity target, ECharacterHitZoneGroup group = ECharacterHitZoneGroup.VIRTUAL)
    {
        ItemUseParameters itemUseParams = super.GetAnimationParameters(item, target, group);
        itemUseParams.SetAllowMovementDuringAction(true);
        itemUseParams.SetKeepInHandAfterSuccess(true);
        itemUseParams.SetMaxAnimLength(CANTEEN_MAX_ANIM_LENGTH);
        return itemUseParams;
    }
}
