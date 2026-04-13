// 库存管理器组件扩展
// 负责监听物品移除事件，立即更新负重缓存
modded class SCR_InventoryStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
    // 当物品从库存中移除时调用
    override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
    {
        // 调用父类方法
        super.OnItemRemoved(storageOwner, item);
        
        // 获取角色控制器组件
        SCR_CharacterControllerComponent characterController = SCR_CharacterControllerComponent.Cast(GetOwner().FindComponent(SCR_CharacterControllerComponent));
        if (characterController)
        {
            // 通知角色控制器组件更新负重缓存
            characterController.OnItemRemovedFromInventory();
        }
    }
    
    // 当物品添加到库存中时调用
    override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
    {
        // 调用父类方法
        super.OnItemAdded(storageOwner, item);
        
        // 获取角色控制器组件
        SCR_CharacterControllerComponent characterController = SCR_CharacterControllerComponent.Cast(GetOwner().FindComponent(SCR_CharacterControllerComponent));
        if (characterController)
        {
            // 通知角色控制器组件更新负重缓存
            characterController.OnItemAddedToInventory();
        }
    }
}