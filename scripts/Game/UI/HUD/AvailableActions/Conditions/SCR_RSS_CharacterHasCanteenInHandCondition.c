//! 左手持水壶（SCR_RSS_CanteenDrinkEffect）时为真；不依赖 modded SCR_EConsumableType 在 conf 中的序列化。

[BaseContainerProps()]
class SCR_RSS_CharacterHasCanteenInHandCondition : SCR_AvailableActionCondition
{
    //------------------------------------------------------------------------------------------------
    override bool IsAvailable(notnull SCR_AvailableActionsConditionData data)
    {
        IEntity item = data.GetCurrentItemEntity();
        if (!item)
            return GetReturnResult(false);

        SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
        if (!consumable)
            return GetReturnResult(false);

        SCR_RSS_CanteenDrinkEffect effect = SCR_RSS_CanteenDrinkEffect.Cast(consumable.GetConsumableEffect());
        return GetReturnResult(effect != null);
    }
}
