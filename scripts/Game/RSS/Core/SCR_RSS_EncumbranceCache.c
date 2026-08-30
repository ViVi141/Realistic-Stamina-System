// 负重缓存管理模块
// 负责管理负重的缓存计算和更新（事件驱动，性能优化）
// 模块化拆分：从 PlayerBase.c 提取的独立功能模块

class SCR_RSS_EncumbranceCache
{
    // 与 UpdateCache 内分段多项式一致，供服务器 RPC 等在缓存无效时复用
    static float ComputeSpeedPenaltyFromEffectiveWeight(float effectiveWeight)
    {
        float ratio = Math.Clamp(effectiveWeight / SCR_RSS_Constants.CHARACTER_WEIGHT, 0.0, 2.0);
        float rawPenalty = 0.0;
        if (ratio <= 0.3)
        {
            rawPenalty = 0.15 * ratio;
        }
        else if (ratio <= 0.6)
        {
            float segment = ratio - 0.3;
            rawPenalty = 0.045 + 0.35 * Math.Pow(segment, 1.5);
        }
        else
        {
            float segment = ratio - 0.6;
            rawPenalty = 0.25 + 0.65 * (segment * segment);
        }
        float coeff = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyCoeff();
        rawPenalty = rawPenalty * (coeff / 0.20);
        float max_pen = SCR_RSS_ConfigBridge.GetEncumbranceSpeedPenaltyMax();
        return Math.Clamp(rawPenalty, 0.0, max_pen);
    }

    // ==================== 状态变量 ====================
    protected float m_fCachedCurrentWeight = 0.0; // 缓存的当前重量（kg）
    protected float m_fCachedEncumbranceSpeedPenalty = 0.0; // 缓存的速度惩罚
    protected float m_fCachedBodyMassPercent = 0.0; // 缓存的有效负重占体重百分比
    protected float m_fCachedEncumbranceStaminaDrainMultiplier = 1.0; // 缓存的体力消耗倍数
    protected bool m_bEncumbranceCacheValid = false; // 缓存是否有效
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent; // 缓存的库存组件引用
    protected SCR_InventoryStorageManagerComponent m_pCachedInventoryManager; // 缓存的库存管理器组件（避免每 tick FindComponent）
    protected SCR_GadgetManagerComponent m_pCachedGadgetManager; // 缓存的 Gadget 管理器（IN_HAND 称重）
    protected BaseWeaponManagerComponent m_pCachedWeaponManager; // 缓存的武器管理器（当前武器称重）
    protected float m_fCachedHeldItemWeight = 0.0; // 缓存的手持物品重量（kg）；IN_HAND gadget 或当前武器
    protected float m_fLastCheckTime = 0.0; // 上次检查时间（秒）
    protected const float ENCUMBRANCE_CHECK_INTERVAL = 0.5; // 轮询间隔（秒），perf: 0.2→0.5，事件驱动已覆盖变更

    // ==================== 公共方法 ====================

    // 初始化缓存
    // @param inventoryComponent 库存组件引用（可为null）
    void Initialize(SCR_CharacterInventoryStorageComponent inventoryComponent = null)
    {
        m_fCachedCurrentWeight = 0.0;
        m_fCachedEncumbranceSpeedPenalty = 0.0;
        m_fCachedBodyMassPercent = 0.0;
        m_fCachedEncumbranceStaminaDrainMultiplier = 1.0;
        m_fCachedHeldItemWeight = 0.0;
        m_bEncumbranceCacheValid = false;
        m_pCachedInventoryComponent = inventoryComponent;
        m_pCachedInventoryManager = null;
        m_pCachedGadgetManager = null;
        m_pCachedWeaponManager = null;

        // 如果提供了库存组件，初始化时计算一次负重
        if (m_pCachedInventoryComponent)
            UpdateCache();
    }

    // 设置库存组件引用
    // @param inventoryComponent 库存组件引用
    void SetInventoryComponent(SCR_CharacterInventoryStorageComponent inventoryComponent)
    {
        m_pCachedInventoryComponent = inventoryComponent;
        m_pCachedInventoryManager = null; // 重置，下次 UpdateCache 时重新查找
        m_pCachedGadgetManager = null;
        m_pCachedWeaponManager = null;
        m_fCachedHeldItemWeight = 0.0;
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
        if (!ownerEntity.GetWorld())
        {
            m_bEncumbranceCacheValid = false;
            return;
        }

        // 使用 SCR_InventoryStorageManagerComponent.GetTotalWeightOfAllStorages() 方法（唯一方式）
        if (!m_pCachedInventoryManager)
            m_pCachedInventoryManager = SCR_InventoryStorageManagerComponent.Cast(ownerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
        if (m_pCachedInventoryManager)
        {
            // 使用官方推荐的 GetTotalWeightOfAllStorages() 方法，确保计算所有存储的重量
            currentWeight = m_pCachedInventoryManager.GetTotalWeightOfAllStorages();
        }
        else
        {
            // 如果无法获取 inventoryManager，设置缓存无效
            m_bEncumbranceCacheValid = false;
            if (SCR_RSS_ConfigBridge.IsDebugEnabled())
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
        m_fCachedHeldItemWeight = SampleHeldItemWeight(ownerEntity);

        // 计算有效负重（负载 = 载具/装备重量 - 基准装备重量）
        // GetTotalWeightOfAllStorages() 返回的是装备/背包重量，不含身体重量
        float effectiveWeight = Math.Max(currentWeight - SCR_RSS_Constants.BASE_WEIGHT, 0.0);
        m_fCachedBodyMassPercent = effectiveWeight / SCR_RSS_Constants.CHARACTER_WEIGHT;
        m_fCachedEncumbranceSpeedPenalty = ComputeSpeedPenaltyFromEffectiveWeight(effectiveWeight);

        // 计算体力消耗倍数
        float encumbranceStaminaDrainCoeff = SCR_RSS_ConfigBridge.GetEncumbranceStaminaDrainCoeff();
        m_fCachedEncumbranceStaminaDrainMultiplier = 1.0 + (encumbranceStaminaDrainCoeff * m_fCachedBodyMassPercent);
        m_fCachedEncumbranceStaminaDrainMultiplier = Math.Clamp(m_fCachedEncumbranceStaminaDrainMultiplier, 1.0, 3.0);

        m_bEncumbranceCacheValid = true;
    }

    // 检查并更新缓存（仅在变化时更新）
    // 在 UpdateSpeedBasedOnStamina 中调用，检查负重是否变化
    // 性能优化：按 ENCUMBRANCE_CHECK_INTERVAL 节流，减少 GetTotalWeightOfAllStorages 调用频率
    void CheckAndUpdate()
    {
        if (!m_pCachedInventoryComponent)
            return;

        float currentTime = 0.0;
        if (!GetGame())
            return;
        World world = GetGame().GetWorld();
        if (!world)
            return;
        currentTime = world.GetWorldTime() / 1000.0;
        if (m_bEncumbranceCacheValid && (currentTime - m_fLastCheckTime < ENCUMBRANCE_CHECK_INTERVAL))
            return;
        m_fLastCheckTime = currentTime;

        float currentWeight = 0.0;

        // 获取角色实体
        IEntity ownerEntity = m_pCachedInventoryComponent.GetOwner();
        if (ownerEntity)
        {
            if (!ownerEntity.GetWorld())
            {
                m_bEncumbranceCacheValid = false;
                return;
            }
            if (!m_pCachedInventoryManager)
                m_pCachedInventoryManager = SCR_InventoryStorageManagerComponent.Cast(ownerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
            if (m_pCachedInventoryManager)
            {
                currentWeight = m_pCachedInventoryManager.GetTotalWeightOfAllStorages();
            }
            else
            {
                // 回退方案：手动计算所有必要存储的重量
                array<BaseInventoryStorageComponent> storages = {};

                // 按照官方 GetTotalWeightOfAllStorages() 方法的逻辑，手动添加必要的存储
                BaseInventoryStorageComponent weaponStorage = m_pCachedInventoryComponent.GetWeaponStorage();
                if (weaponStorage)
                    storages.Insert(weaponStorage); // 主副武器插槽（仅非空时添加）
                storages.Insert(m_pCachedInventoryComponent);                    // 衣服、背包、背心里的东西

                // 遍历所有存储，累加重量
                foreach (BaseInventoryStorageComponent storage : storages)
                {
                    if (storage)
                        currentWeight += storage.GetTotalWeight();
                }
            }
        }
        else
        {
            // CRITICAL FIX: owner entity is null, mark cache invalid immediately.
            // Previously the fallback calculation could keep the cache nominally
            // valid when weight hadn't changed beyond the 0.1kg threshold.
            m_bEncumbranceCacheValid = false;
            return;
        }

        // 手持物品独立于存储重量采样（拿起/放下会触发库存事件，此处兜底轮询）
        m_fCachedHeldItemWeight = SampleHeldItemWeight(ownerEntity);

        // 如果重量变化超过0.1kg，重新计算缓存（避免微小浮点误差触发）
        if (Math.AbsFloat(currentWeight - m_fCachedCurrentWeight) > 0.1 || !m_bEncumbranceCacheValid)
        {
            UpdateCache();
        }
    }

    // 实体仍挂在世界里才可安全调 native（专服生成/删除窗口易出现悬空实体）
    protected bool EntityIsUsable(IEntity ent)
    {
        if (!ent)
            return false;
        if (!ent.GetWorld())
            return false;
        return true;
    }

    // 实体物品总重（kg）：含 additional weight / 附件，避免只读属性集合漏计
    protected float SampleEntityItemWeight(IEntity itemEntity)
    {
        if (!EntityIsUsable(itemEntity))
            return 0.0;
        InventoryItemComponent itemComponent = InventoryItemComponent.Cast(itemEntity.FindComponent(InventoryItemComponent));
        if (!itemComponent)
            return 0.0;
        if (!EntityIsUsable(itemComponent.GetOwner()))
            return 0.0;
        return Math.Max(itemComponent.GetTotalWeight(), 0.0);
    }

    // 当前选中武器实体：优先 GetCurrentSlot（避免 GetCurrent 返回槽/武器混型）
    protected IEntity ResolveCurrentWeaponEntity(IEntity ownerEntity)
    {
        if (!EntityIsUsable(ownerEntity))
            return null;
        if (m_pCachedWeaponManager)
        {
            if (m_pCachedWeaponManager.GetOwner() != ownerEntity)
                m_pCachedWeaponManager = null;
        }
        if (!m_pCachedWeaponManager)
            m_pCachedWeaponManager = BaseWeaponManagerComponent.Cast(ownerEntity.FindComponent(BaseWeaponManagerComponent));
        if (!m_pCachedWeaponManager)
            return null;
        if (!EntityIsUsable(m_pCachedWeaponManager.GetOwner()))
        {
            m_pCachedWeaponManager = null;
            return null;
        }

        WeaponSlotComponent slot = m_pCachedWeaponManager.GetCurrentSlot();
        if (slot)
        {
            IEntity slottedWeapon = slot.GetWeaponEntity();
            if (EntityIsUsable(slottedWeapon))
                return slottedWeapon;
        }

        BaseWeaponComponent weaponComp = m_pCachedWeaponManager.GetCurrentWeapon();
        if (!weaponComp)
            weaponComp = m_pCachedWeaponManager.GetCurrent();
        if (!weaponComp)
            return null;

        WeaponSlotComponent weaponSlot = WeaponSlotComponent.Cast(weaponComp);
        if (weaponSlot)
            return weaponSlot.GetWeaponEntity();
        return weaponComp.GetOwner();
    }

    // 手持物品重量采样（kg）
    // 1) 仅 EGadgetMode.IN_HAND 的 gadget（GetHeldGadget 会回退到隐藏腕表/指南针，必须过滤）
    // 2) 无 IN_HAND gadget 时采当前武器（双手占用于持枪；武器已在存储总重中，此处只供 itemBonus）
    protected float SampleHeldItemWeight(IEntity ownerEntity)
    {
        if (!EntityIsUsable(ownerEntity))
            return 0.0;

        IEntity heldGadgetEntity = null;
        if (m_pCachedGadgetManager)
        {
            if (m_pCachedGadgetManager.GetOwner() != ownerEntity)
                m_pCachedGadgetManager = null;
        }
        if (!m_pCachedGadgetManager)
            m_pCachedGadgetManager = SCR_GadgetManagerComponent.Cast(ownerEntity.FindComponent(SCR_GadgetManagerComponent));
        if (m_pCachedGadgetManager && EntityIsUsable(m_pCachedGadgetManager.GetOwner()))
        {
            SCR_GadgetComponent gadgetComp = m_pCachedGadgetManager.GetHeldGadgetComponent();
            if (gadgetComp)
            {
                IEntity gadgetOwner = gadgetComp.GetOwner();
                if (EntityIsUsable(gadgetOwner))
                {
                    if (gadgetComp.GetMode() == EGadgetMode.IN_HAND)
                        heldGadgetEntity = gadgetOwner;
                }
            }
        }
        else
        {
            m_pCachedGadgetManager = null;
        }

        if (heldGadgetEntity)
            return SampleEntityItemWeight(heldGadgetEntity);

        return SampleEntityItemWeight(ResolveCurrentWeaponEntity(ownerEntity));
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

    // 获取缓存的手持物品重量
    // @return 手持物品重量（kg），无手持或缓存无效时返回 0.0
    float GetHeldItemWeight()
    {
        if (m_bEncumbranceCacheValid)
            return m_fCachedHeldItemWeight;
        return 0.0;
    }

    // 获取缓存的速度惩罚
    // @return 速度惩罚值（0.0-0.5），如果缓存无效则返回0.0
    float GetSpeedPenalty()
    {
        return GetSpeedPenaltyFraction();
    }

    //! 负重速度惩罚比例（0.0–0.5）；与 GetSpeedPenalty 同义
    float GetSpeedPenaltyFraction()
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
