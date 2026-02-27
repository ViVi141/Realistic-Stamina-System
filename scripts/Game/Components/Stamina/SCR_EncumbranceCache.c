// 负重缓存管理模块
// 负责管理负重的缓存计算和更新（事件驱动，性能优化）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class EncumbranceCache
{
    // ==================== 状态变量 ====================
    protected float m_fCachedCurrentWeight = 0.0; // 缓存的当前重量（kg）
    protected float m_fCachedEncumbranceSpeedPenalty = 0.0; // 缓存的速度惩罚
    protected float m_fCachedBodyMassPercent = 0.0; // 缓存的有效负重占体重百分比
    protected float m_fCachedEncumbranceStaminaDrainMultiplier = 1.0; // 缓存的体力消耗倍数
    protected bool m_bEncumbranceCacheValid = false; // 缓存是否有效
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent; // 缓存的库存组件引用
    
    // ==================== 公共方法 ====================
    
    // 初始化缓存
    // @param inventoryComponent 库存组件引用（可为null）
    void Initialize(SCR_CharacterInventoryStorageComponent inventoryComponent = null)
    {
        m_fCachedCurrentWeight = 0.0;
        m_fCachedEncumbranceSpeedPenalty = 0.0;
        m_fCachedBodyMassPercent = 0.0;
        m_fCachedEncumbranceStaminaDrainMultiplier = 1.0;
        m_bEncumbranceCacheValid = false;
        m_pCachedInventoryComponent = inventoryComponent;
        
        // 如果提供了库存组件，初始化时计算一次负重
        if (m_pCachedInventoryComponent)
            UpdateCache();
    }
    
    // 设置库存组件引用
    // @param inventoryComponent 库存组件引用
    void SetInventoryComponent(SCR_CharacterInventoryStorageComponent inventoryComponent)
    {
        m_pCachedInventoryComponent = inventoryComponent;
        if (m_pCachedInventoryComponent)
            UpdateCache();
        else
            m_bEncumbranceCacheValid = false;
    }
    
    // 更新缓存（事件驱动）
    // 仅在库存变化时调用，避免每0.2秒重复计算
    // 注意：需要在库存组件的事件中调用此方法（如 OnItemAdded/OnItemRemoved）
    void UpdateCache()
    {
        if (!m_pCachedInventoryComponent)
        {
            m_bEncumbranceCacheValid = false;
            return;
        }
        
        float currentWeight = 0.0;
        
        // 获取角色实体
        IEntity ownerEntity = m_pCachedInventoryComponent.GetOwner();
        if (!ownerEntity)
        {
            m_bEncumbranceCacheValid = false;
            return;
        }
        
        // 使用 SCR_InventoryStorageManagerComponent.GetTotalWeightOfAllStorages() 方法（唯一方式）
        SCR_InventoryStorageManagerComponent inventoryManager = SCR_InventoryStorageManagerComponent.Cast(ownerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
        if (inventoryManager)
        {
            // 使用官方推荐的 GetTotalWeightOfAllStorages() 方法，确保计算所有存储的重量
            currentWeight = inventoryManager.GetTotalWeightOfAllStorages();
        }
        else
        {
            // 如果无法获取 inventoryManager，设置缓存无效
            m_bEncumbranceCacheValid = false;
            if (StaminaConstants.IsDebugEnabled())
                Print("[RSS] UpdateCache - 无法获取 SCR_InventoryStorageManagerComponent");
            return;
        }
        
        if (currentWeight < 0.0)
        {
            m_bEncumbranceCacheValid = false;
            return;
        }
        
        // 更新缓存值
        m_fCachedCurrentWeight = currentWeight;

        // 计算有效负重（负载 = 载具/装备重量 - 基准装备重量）
        // GetTotalWeightOfAllStorages() 返回的是装备/背包重量，不含身体重量
        float effectiveWeight = Math.Max(currentWeight - StaminaConstants.BASE_WEIGHT, 0.0);
        m_fCachedBodyMassPercent = effectiveWeight / StaminaConstants.CHARACTER_WEIGHT;
        
        // 计算速度惩罚（基于体重百分比，使用幂函数）
        float encumbranceSpeedPenaltyCoeff = StaminaConstants.GetEncumbranceSpeedPenaltyCoeff();
        float exp = StaminaConstants.GetEncumbranceSpeedPenaltyExponent();
        float max_pen = StaminaConstants.GetEncumbranceSpeedPenaltyMax();
        m_fCachedEncumbranceSpeedPenalty = encumbranceSpeedPenaltyCoeff * Math.Pow(m_fCachedBodyMassPercent, exp);
        m_fCachedEncumbranceSpeedPenalty = Math.Clamp(m_fCachedEncumbranceSpeedPenalty, 0.0, max_pen);
        
        // 计算体力消耗倍数
        float encumbranceStaminaDrainCoeff = StaminaConstants.GetEncumbranceStaminaDrainCoeff();
        m_fCachedEncumbranceStaminaDrainMultiplier = 1.0 + (encumbranceStaminaDrainCoeff * m_fCachedBodyMassPercent);
        m_fCachedEncumbranceStaminaDrainMultiplier = Math.Clamp(m_fCachedEncumbranceStaminaDrainMultiplier, 1.0, 3.0);
        
        m_bEncumbranceCacheValid = true;
    }
    
    // 检查并更新缓存（仅在变化时更新）
    // 在 UpdateSpeedBasedOnStamina 中调用，检查负重是否变化
    void CheckAndUpdate()
    {
        if (!m_pCachedInventoryComponent)
            return;
        
        float currentWeight = 0.0;
        
        // 获取角色实体
        IEntity ownerEntity = m_pCachedInventoryComponent.GetOwner();
        if (ownerEntity)
        {
            // 尝试使用 SCR_InventoryStorageManagerComponent.GetTotalWeightOfAllStorages() 方法（推荐方式）
            SCR_InventoryStorageManagerComponent inventoryManager = SCR_InventoryStorageManagerComponent.Cast(ownerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
            if (inventoryManager)
            {
                // 使用官方推荐的 GetTotalWeightOfAllStorages() 方法，确保计算所有存储的重量
                currentWeight = inventoryManager.GetTotalWeightOfAllStorages();
            }
            else
            {
                // 回退方案：手动计算所有必要存储的重量
                array<BaseInventoryStorageComponent> storages = {};
                float weaponWeight = 0.0;
                float characterWeight = 0.0;
                
                // 按照官方 GetTotalWeightOfAllStorages() 方法的逻辑，手动添加必要的存储
                BaseInventoryStorageComponent weaponStorage = m_pCachedInventoryComponent.GetWeaponStorage();
                storages.Insert(weaponStorage); // 主副武器插槽
                storages.Insert(m_pCachedInventoryComponent);                    // 衣服、背包、背心里的东西
                
                // 遍历所有存储，累加重量
                foreach (BaseInventoryStorageComponent storage : storages)
                {
                    if (storage)
                    {
                        float storageWeight = storage.GetTotalWeight();
                        currentWeight += storageWeight;
                        
                        if (storage == weaponStorage)
                            weaponWeight = storageWeight;
                        else if (storage == m_pCachedInventoryComponent)
                            characterWeight = storageWeight;
                    }
                }
                

            }
        }
        else
        {
            // 无角色实体时的回退方案
            float weaponWeight = 0.0;
            currentWeight = m_pCachedInventoryComponent.GetTotalWeight();
            
            // 添加武器存储重量
            BaseInventoryStorageComponent weaponStorage = m_pCachedInventoryComponent.GetWeaponStorage();
            if (weaponStorage)
            {
                weaponWeight = weaponStorage.GetTotalWeight();
                currentWeight += weaponWeight;
            }
            

        }
        
        // 如果重量变化超过0.1kg，重新计算缓存（避免微小浮点误差触发）
        if (Math.AbsFloat(currentWeight - m_fCachedCurrentWeight) > 0.1 || !m_bEncumbranceCacheValid)
        {
            UpdateCache();
        }
    }
    
    // ==================== 获取缓存值的方法 ====================
    
    // 获取缓存的当前重量
    // @return 当前重量（kg），如果缓存无效则返回0.0
    float GetCurrentWeight()
    {
        if (m_bEncumbranceCacheValid)
            return m_fCachedCurrentWeight;
        return 0.0;
    }
    
    // 获取缓存的速度惩罚
    // @return 速度惩罚值（0.0-0.5），如果缓存无效则返回0.0
    float GetSpeedPenalty()
    {
        if (m_bEncumbranceCacheValid)
            return m_fCachedEncumbranceSpeedPenalty;
        return 0.0;
    }
    
    // 获取缓存的有效负重占体重百分比
    // @return 有效负重占体重百分比（0.0-1.0+），如果缓存无效则返回0.0
    float GetBodyMassPercent()
    {
        if (m_bEncumbranceCacheValid)
            return m_fCachedBodyMassPercent;
        return 0.0;
    }
    
    // 获取缓存的体力消耗倍数
    // @return 体力消耗倍数（1.0-3.0），如果缓存无效则返回1.0
    float GetStaminaDrainMultiplier()
    {
        if (m_bEncumbranceCacheValid)
            return m_fCachedEncumbranceStaminaDrainMultiplier;
        return 1.0;
    }
    
    // 检查缓存是否有效
    // @return true表示缓存有效，false表示缓存无效
    bool IsCacheValid()
    {
        return m_bEncumbranceCacheValid;
    }
    
    // 获取库存组件引用
    // @return 库存组件引用（可为null）
    SCR_CharacterInventoryStorageComponent GetInventoryComponent()
    {
        return m_pCachedInventoryComponent;
    }
}
