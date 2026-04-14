# 伤害系统 API 参考（Arma Reforger / Enfusion）

本文档整理 **角色伤害管线** 中与模组开发相关的类型与接口，主数据源为仓库内摘录的 `docs/1.c`（`SCR_CharacterDamageManagerComponent`），并补充本模组（RSS）中的用法。

官方完整定义以游戏/Workbench 附带的脚本为准；本文件仅作速查与协作说明。

---

## 1. 核心类型关系

| 类型 | 说明 |
|------|------|
| `SCR_ExtendedDamageManagerComponent` | 伤害管理器基类：命中区、伤害效果、DOT 等 |
| `SCR_CharacterDamageManagerComponent` | **人形角色**专用子类：流血、止血带、护甲映射、坠落与碰撞、昏迷等 |
| `SCR_DamageEffect` / `SCR_PersistentDamageEffect` | 瞬时 / 持久伤害效果 |
| `SCR_DotDamageEffect` | 持续伤害（DOT）基类；`SCR_BleedingDamageEffect` 等继承自此 |
| `SCR_CharacterHitZone` | 角色物理/逻辑命中区 |
| `EDamageType` | 伤害类型枚举（含 `BLEEDING` 等） |

**获取角色伤害管理器：**

```c
ChimeraCharacter ch = // ...
SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(ch.GetDamageManager());
```

---

## 2. 全局医疗倍率（与预制体属性对应）

角色预制体 **Damage** 组件上常见字段与脚本对应关系：

| 预制体（约） | 成员 / API | 含义 |
|--------------|------------|------|
| Bleeding → **DOT Scale** | `m_fDOTScale` | 全局**流血速率**倍率 |
| Regeneration → **Regen Scale** | `m_fRegenScale` | 再生速率倍率 |
| Bleeding → **Tourniquet Strength Multiplier** | `GetTourniquetStrengthMultiplier()` | 止血带减轻流血的程度 |

### `SetBleedingScale` / `GetBleedingScale`

- **`SetBleedingScale(float rate, bool changed)`**  
  - 将角色本地流血倍率设为 `rate`。  
  - `changed == true` 时标记为已覆盖，**优先于**关卡 `SCR_GameModeHealthSettings` 中的全局流血倍率（见 `docs/1.c` 中 `GetBleedingScale()` 分支）。

- **`GetBleedingScale()`**  
  - 若存在关卡设置且未强制覆盖，可能返回 `s_HealthSettings.GetBleedingScale()`；否则返回角色上的倍率。

**新建流血时的用法（官方）：**  
在 `AddBleedingEffectOnHitZone` 内，对 `SCR_BleedingDamageEffect` 执行：

`SetDPS(bleedingRate * GetBleedingScale())`

因此**提高 `GetBleedingScale()` 会影响之后通过该路径创建的流血 DPS**；已存在的流血实例是否随倍率变化，取决于引擎内 `SCR_BleedingDamageEffect` 是否每帧重读倍率（通常 DPS 在创建或合并时已写入）。

### `SetRegenScale` / `GetRegenScale`

与再生类 DOT 的全局倍率相关，逻辑与流血倍率类似（`changed` 与 `s_HealthSettings` 的优先级）。

### `SetPermitUnconsciousness` / `GetPermitUnconsciousness`

是否允许昏迷；与 `UpdateConsciousness()`、血量/韧性状态共同决定是否进入昏迷或直接被 `Kill()`。

---

## 3. 流血：创建、查询、清除

### 3.1 创建与注册

| 方法 | 说明 |
|------|------|
| `AddBleedingEffectOnHitZone(SCR_CharacterHitZone hitZone, int colliderDescriptorIndex = -1)` | 按部位创建 `SCR_BleedingDamageEffect`，并设置 DPS、命中区等（权威端逻辑，代理命中区会提前返回） |
| `AddBleedingToArray(HitZone hitZone)` | 将部位记入流血列表（与效果表现、查询配合） |
| `CreateBleedingParticleEffect(HitZone hitZone, int colliderDescriptorIndex)` | 流血粒子（依赖预制体粒子资源等） |

### 3.2 查询

| 方法 | 说明 |
|------|------|
| `IsBleeding()` | 遍历持久效果，是否存在 `EDamageType.BLEEDING`（官方注释中的标准流血判断方式） |
| `GetBleedingHitZones()` | 返回登记在流血列表中的命中区 |
| `GetMostDOTHitZone(EDamageType damageType, ...)` | 按 DOT 类型查找主要承受持续伤害的命中区 |

### 3.3 清除与粒子

| 方法 | 说明 |
|------|------|
| `RemoveAllBleedings()` | `TerminateDamageEffectsOfType(SCR_BleedingDamageEffect)`，移除所有流血效果 |
| `RemoveGroupBleeding(ECharacterHitZoneGroup group)` | 移除某一组上的流血效果 |
| `RemoveBleedingParticleEffect(HitZone hitZone)` | 移除该部位流血粒子实体 |
| `RemoveAllBleedingParticles()` | 清理所有流血粒子 |
| `RemoveAllBleedingParticlesAfterDeath()` | 死亡后按流血速率延迟清理粒子等 |

### 3.4 辅助 / 调试向

| 方法 | 说明 |
|------|------|
| `AddParticularBleeding(...)` | 向指定名称/强度部位添加流血 |
| `AddRandomBleeding()` | 随机物理部位流血 |
| `GetRandomPhysicalHitZone(...)` | 随机物理命中区 |

---

## 4. 部位组与医疗状态

| 方法 | 说明 |
|------|------|
| `GetGroupTourniquetted` / `SetTourniquettedGroup` | 肢体组是否上止血带 |
| `GetGroupIsBeingHealed` / `SetGroupIsBeingHealed` | 是否正在该组上被治疗动画/逻辑占用 |
| `GetGroupSalineBagged` / `SetSalineBaggedGroup` | 是否挂盐水袋等（依游戏版本） |
| `GetGroupHealthScaled(ECharacterHitZoneGroup)` | 组内平均健康比例 |
| `GetGroupDamageOverTime(ECharacterHitZoneGroup, EDamageType)` | 组内某类 DOT 总和（override） |
| `UpdateCharacterGroupDamage(ECharacterHitZoneGroup)` | 组状态变化后刷新角色伤害表现等 |

---

## 5. 护甲与衣物

| 方法 | 说明 |
|------|------|
| `InsertArmorData` / `RemoveArmorData` / `UpdateArmorDataMap` | 衣物护甲数据写入 `m_mClothItemDataMap` |
| `GetArmorData` / `GetArmorProtection` | 查询部位护甲与防护 |
| `ArmorHitEventEffects` / `ArmorHitEventDamage` | 中甲时的效果与伤害传递（与 `SCR_ArmorDamageManagerComponent` 协作） |
| `OverrideHitMaterial` | 命中材质覆盖 |

---

## 6. 碰撞、坠落与特殊接触

| 方法 | 说明 |
|------|------|
| `FilterContact` / `OnFilteredContact` | 过滤低速碰撞等 |
| `OnHandleFallDamage` / `HandleAnimatedFallDamage` / `HandleRagdollFallDamage` | 坠落伤害分支 |
| `AddSpecialContactEffect` | 添加特殊碰撞伤害效果 |
| `HijackDamageHandling` | 劫持伤害上下文（如默认命中区上的坠落特殊处理） |

---

## 7. 再生与治愈

| 方法 | 说明 |
|------|------|
| `RegenPhysicalHitZones(bool skipRegenDelay = false)` | 物理命中区再生 |
| `RegenVirtualHitZone(SCR_RegeneratingHitZone, float dps, ...)` | 虚拟命中区再生 |
| `CanBeHealed` / `FullHeal` | 是否可治疗、满血（可忽略治疗类 DOT） |

---

## 8. 意识与特殊 HitZone 访问

| 方法 | 说明 |
|------|------|
| `GetBloodHitZone` / `SetBloodHitZone` | 血液命中区 |
| `GetResilienceHitZone` / `SetResilienceHitZone` | 韧性（疲劳/休克等）命中区 |
| `GetHeadHitZone` / `SetHeadHitZone` | 头部命中区 |
| `UpdateConsciousness()` | 根据 `ShouldBeUnconscious()` 等更新昏迷状态 |
| `ForceUnconsciousness(float resilienceHealth = 0)` | 强制进入昏迷条件（受 `GetPermitUnconsciousness()` 约束） |
| `GetResilienceRegenScale()` | 随失血等变化的韧性再生缩放 |

---

## 9. 音效与调试

| 方法 | 说明 |
|------|------|
| `SynchronizedSoundEvent(...)` | 同步播放伤害相关音效 |
| `SoundHit` / `SoundKnockout` / `SoundDeath` / `SoundHeal` | 各类反馈音效 |
| `PlaySoundEvent` | 按名称播放事件 |
| `GetNearestHitZones` | 由世界坐标找最近命中区 |
| `ProcessDebug` | 调试输出 |

---

## 10. 基类常用能力（`SCR_ExtendedDamageManagerComponent`）

以下在角色脚本中经常与 `SCR_CharacterDamageManagerComponent` 一起使用（具体签名以引擎为准）：

- `AddDamageEffect(SCR_DamageEffect)`  
- `TerminateDamageEffectsOfType(typename)`  
- `GetAllPersistentEffectsOfType(typename)`  
- `FindDamageEffectOnHitZone` / `FindAllDamageEffectsOfTypeOnHitZone`  
- `GetPhysicalHitZones` / `GetAllHitZones` / `GetHitZonesOfGroup`

本模组示例：`SCR_ConsumableMorphine` / `SCR_ConsumableCustomInjector` 使用 `GetAllPersistentEffectsOfType` 判断是否已存在对应治疗 DOT。

---

## 11. 本模组（RSS）相关扩展

| 位置 | 说明 |
|------|------|
| `SCR_CustomInjectorDamageEffect` | 自定义注射器治疗 DOT；在物品上配置 **Damage Effects To Load** |
| `SCR_ConsumableCustomInjector` | 消耗品逻辑，查询 `SCR_CustomInjectorDamageEffect` |
| `SCR_CombatStimConstants` | CombatStim 阶段与 `BLEEDING_SCALE_MULT_*` 等常量 |
| `PlayerBase.c` → `RSS_CombatStim_UpdateBleedingScale` | 药效/OD 期间在服务端调用 `SetBleedingScale`，结束时还原基准倍率 |

---

## 12. 网络与权威

- **修改伤害状态、添加/移除流血、Kill、SetBleedingScale** 等应在 **服务端权威** 上执行（本模组中 `RSS_CombatStim_UpdateBleedingScale` 已用 `Replication.IsServer()` 保护）。  
- 客户端常以 **复制后的状态** 表现 UI 与特效；具体复制字段以引擎组件为准。

---

## 13. 参考文件

| 文件 | 内容 |
|------|------|
| `docs/1.c` | `SCR_CharacterDamageManagerComponent` 长摘录 |
| `scripts/Game/Damage/DamageEffects/CharacterDamageEffects/` | 模组内 DOT 子类 |
| `scripts/Game/Components/Gadgets/SCR_Consumable*.c` | 消耗品与 `damageMgr` 交互 |
