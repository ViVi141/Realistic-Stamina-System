//! 可重复使用的消耗品：喝水后不 ModeClear(IN_HAND)，保留回调以便再次 R 键使用。

class SCR_RSS_CanteenConsumableComponentClass : SCR_ConsumableItemComponentClass
{
}

class SCR_RSS_CanteenConsumableComponent : SCR_ConsumableItemComponent
{
    //------------------------------------------------------------------------------------------------
    override void ActivateAction()
    {
        if (!m_ConsumableEffect || !m_CharacterOwner)
            return;

        CharacterControllerComponent controller = m_CharacterOwner.GetCharacterController();
        if (controller && controller.IsUsingItem())
            return;

        super.ActivateAction();
    }

    //------------------------------------------------------------------------------------------------
    override void ApplyItemEffect(IEntity target, IEntity user, ItemUseParameters animParams, IEntity item, bool deleteItem = true)
    {
        if (!m_ConsumableEffect)
            return;

        m_ConsumableEffect.ApplyEffect(target, user, item, animParams);

        if (deleteItem)
        {
            ModeClear(EGadgetMode.IN_HAND);
            RplComponent.DeleteRplEntity(GetOwner(), false);
        }
    }
}
