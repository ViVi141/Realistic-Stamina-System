//! 水壶喝水：完全按官方医疗消耗品同一套调用链。
//! 吗啡/绷带/止血带：
//!   1) 角色 CharacterAnimationComponent.BindCommand(命令)
//!   2) GetAnimationParameters → SetCommandID → TryUseItemOverrideParams
//!   3) 预制体 ItemActionAnimAttributes + AnimationAttachment(BindingName Gadget)
//!   4) 无 GadgetAnimationComponent
//! 水壶唯一差别：物品图命令是 CMD_Item_Action（官方 Canteen.agr），不是 CMD_HealSelf。

[BaseContainerProps()]
class SCR_RSS_CanteenDrinkEffect : SCR_ConsumableEffectBase
{
    protected static const string CANTEEN_DRINK_COMMAND = "CMD_Item_Action";
    //! 喝水 clip 非循环；MaxAnimLength 须大于实际时长，避免引擎发 CommandIntArg=-1 二次切入。
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
    //! 与 SCR_ConsumableEffectHealthItems.ActivateEffect 相同：缺参时先组 ItemUseParameters。
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
    //! 与 SCR_ConsumableEffectHealthItems 相同：在角色动画组件上 BindCommand。
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
    //! 与 SCR_ConsumableMorphine 相同：super 组参后再改移动/持握；MaxAnimLength 单独加大。
    override ItemUseParameters GetAnimationParameters(IEntity item, notnull IEntity target, ECharacterHitZoneGroup group = ECharacterHitZoneGroup.VIRTUAL)
    {
        ItemUseParameters itemUseParams = super.GetAnimationParameters(item, target, group);
        itemUseParams.SetAllowMovementDuringAction(true);
        itemUseParams.SetKeepInHandAfterSuccess(true);
        itemUseParams.SetMaxAnimLength(CANTEEN_MAX_ANIM_LENGTH);
        return itemUseParams;
    }
}
