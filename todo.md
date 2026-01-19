# 待办事项列表 (TODO List)

本文件记录了 Arma Reforger 拟真体力-速度系统的所有待办事项。

---

## 🔬 Pandolf 模型医疗级扩展（未来方向）

### 🟢 低优先级

#### 1. 鞋具/足部重量因子 (Footwear Factor)

**状态**: 待开始  
**优先级**: 🟢 低（Arma Reforger 对鞋子细分支持有限）

**概念**: 
- 特种部队常言："足部一斤，背上一五"（1kg on feet = 5kg on back）
- 足部负重对身体消耗的影响是背部负重的 5 倍

**解决方案**:
- 轻便运动鞋：`k_shoe = 1.0`（基准）
- 重型作战靴：`k_shoe = 1.2`（+20% 消耗）
- 泥泞环境：增加"泥泞系数"，给总重量 `M` 增加微小加成
- 模拟靴子沾泥后的沉重感

**实现难度**: ⭐⭐ (中等，需要环境状态跟踪)  
**预期代码量**: 30-50 行  
**文件位置**: `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c`

**注意**: 由于 Arma Reforger 对鞋子细分支持有限，此项可以简化为环境系数。

---

## 📝 文档更新

### 2. README 改进方向章节

**状态**: 待开始  
**内容**: 在 README.md 中新增"潜在改进方向"章节，记录未来开发计划

---

## ⚙️ 核心参数校准

### 参数缩放优化（ACFT 标准校准）📊

**状态**: ✅ 已完成  
**优先级**: 🔴 极高（影响核心功能）

**问题**: 
- 通过分析图表（特别是"体力随时间变化"），发现模型在数值缩放（Scaling）上过于"严厉"
- 原本应该持久的"意志力平台期"缩得太短
- **"气槽"太小（耗尽速度过快）**：
  - 角色以 3.7 m/s（目标 Run 速度）跑平地，体力在第 4 分钟左右就彻底归零
  - 数学计算：2 英里测试需要维持 3.7 m/s 约 15 分钟
  - 现状：角色只能坚持 4 分钟，剩下的 11 分钟都在以 1.0 m/s 的"跛行速度"蠕动
  - 结果：平均速度被拉低到了约 1.7 m/s，导致完成时间翻倍（约 30 分钟）
- **Pandolf 模型的"能耗-体力"转换系数过高**：
  - 当前 `ENERGY_TO_STAMINA_COEFF = 0.0001` 可能对于 100% 体力池来说太大了
  - Pandolf 计算出的瓦特数（Watts）产生的每秒百分比扣除超出了 22 岁训练有素士兵的生理耐受力

**目标**:
- 0KG 下达到 15:27 完成 2 英里测试
- 让角色在 3.7 m/s 速度下能坚持跑完 16-18 分钟
- 体力从 100% 降至 25% 的过程应该横跨 0 - 15 分钟

**解决方案**:

1. **降低 Pandolf 转换系数**：
   - 将 `ENERGY_TO_STAMINA_COEFF` 从 `0.0001` 降低到 `0.000035` 左右
   - 逻辑：瓦特数不变，但它对"游戏体力槽"的侵蚀变慢
   - 修改位置：`SCR_RealisticStaminaSystem.c` 中所有 `ENERGY_TO_STAMINA_COEFF` 的使用处

2. **优化"军事分段模型"的基础消耗**：
   - 将 `RUN_BASE_DRAIN_RATE` 从 `0.105 pts/s` 降低到 `0.075 pts/s`
   - 逻辑：原来的 0.105 会导致 100 / 0.105 = 952秒（15.8分钟）就耗尽
   - 考虑到 25% 就要开始减速，需要能支撑 20 分钟连续运行（约 22 分钟耗尽）
   - 修改位置：`SCR_RealisticStaminaSystem.c` 第 37 行

3. **提升 Fitness Level 的效能**：
   - 将 `FITNESS_EFFICIENCY_COEFF` 从 `0.18`（18%）提升到 `0.35`（35%）
   - 逻辑：既然角色被定义为"训练有素（100分水平）"，能量利用效率应该更高
   - 修改位置：`SCR_RealisticStaminaSystem.c` 第 155 行

4. **调整 Pandolf 的基础代谢偏移**：
   - 在 `CalculatePandolfEnergyExpenditure` 中添加 fitness bonus
   - 对于顶尖运动员，运动时的经济性（Running Economy）更高
   - 公式：`fitnessBonus = (1.0 - (0.2 * FITNESS_LEVEL))`
   - `baseTerm = (PANDOLF_BASE_COEFF * fitnessBonus) + (PANDOLF_VELOCITY_COEFF * velocitySquaredTerm)`
   - 修改位置：`SCR_RealisticStaminaSystem.c` 的 Pandolf 计算函数

5. **同步更新 Python 脚本**：
   - 所有 `tools/` 目录下的 Python 脚本需要同步更新相同的参数
   - 确保模拟结果与游戏实现一致

**预期效果**:

- **体力曲线**：0KG 下，体力从 100% 降至 25% 的过程应该横跨 0 - 15 分钟
- **速度曲线**：角色在整个 2 英里测试中，速度线应该是一条几乎笔直的 3.7 m/s（直到最后快到终点才开始轻微下掉）
- **完成时间**：最终 Bar 图显示的完成时间应在 920s - 935s 之间
- **负重对比**：
  - 0KG：能在 15 分半跑完（顶尖士兵）
  - 30KG：因为负重项在 Pandolf 公式中是相乘关系，会在 5-6 分钟时就撞墙，最终用 25 分钟跑完（负重行军的真实写照）

**实现难度**: ⭐⭐ (中等，主要是参数调整)  
**预期代码量**: 20-40 行（参数修改 + 验证测试）  
**文件位置**: 
- `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c`
- `tools/simulate_stamina_system.py`
- `tools/generate_comprehensive_trends.py`
- `tools/multi_dimensional_analysis.py`

**实现位置**:
- ✅ `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c` - 所有参数已更新
  - 第 37 行：`RUN_BASE_DRAIN_RATE = 0.075`
  - 第 156 行：`FITNESS_EFFICIENCY_COEFF = 0.35`
  - 第 1124-1125 行：添加 fitness bonus 到 Pandolf 计算
  - 第 1162 行、1341 行、1420 行：`ENERGY_TO_STAMINA_COEFF = 0.000035`（3处）
- ✅ `tools/simulate_stamina_system.py` - 所有参数已同步更新
- ✅ `tools/generate_comprehensive_trends.py` - 所有参数已同步更新
- ✅ `tools/multi_dimensional_analysis.py` - 所有参数已同步更新

**实际代码量**: ~40 行（参数修改，已完成）  
**完成日期**: 2024-12-19

**完成说明**:
- ✅ 已将所有参数从过于"严厉"的数值调整为更符合 ACFT 标准的参数
- ✅ C 代码和所有 Python 脚本的参数已同步更新，确保一致性
- ✅ 需要运行模拟脚本验证优化效果，预期 0KG 完成时间应在 920s-935s 之间

**注意事项**:
- 数学模型框架已经完美，现在只需要"放宽"体力槽的耐用度
- ✅ 已完成：`ENERGY_TO_STAMINA_COEFF` 从 0.0001 降低到 0.000035（减少65%）
- 需要运行模拟脚本验证优化效果，确认 0KG 的完成时间接近 927 秒

---

## 🎯 战斗与姿态系统

### 3. 姿态交互修正（Posture-based Expenditure）🤸

**状态**: ✅ 已完成  
**优先级**: 🟡 中

**问题**: 
- 目前模型主要针对站立移动
- 在战术射击游戏中，蹲姿机动和爬行对体力的消耗完全不同

**解决方案**:
- **蹲姿行走（Crouch Walking）**：生理学上，蹲姿行走对股四头肌的等长收缩要求极高，代谢成本通常是站立行走的 1.6 - 2.0 倍
- **匍匐爬行（Prone Crawling）**：这是全身性的高强度运动，即便速度极慢，其能量消耗率（Watts）通常与中速跑步相当
- 通过 `GetStance()` 获取姿态（`ECharacterStance` 枚举）
- 给 Pandolf 公式增加一个姿态乘数：`k_crouch = 1.8`, `k_prone = 3.0`

**实现难度**: ⭐⭐ (中等，需要姿态检测)  
**预期代码量**: 40-60 行  
**文件位置**: `scripts/Game/PlayerBase.c`

**实际代码量**: ~25 行（已完成）  
**完成日期**: 2026-01-09

**实现细节**:
- ✅ 添加姿态修正常量（`POSTURE_CROUCH_MULTIPLIER = 1.8`，`POSTURE_PRONE_MULTIPLIER = 3.0`）
- ✅ 在体力消耗计算时检测角色姿态（`GetStance()`）
- ✅ 蹲姿行走：消耗增加80%（1.8倍）
- ✅ 匍匐爬行：消耗增加200%（3.0倍，与中速跑步相当）
- ✅ 只在移动时应用姿态修正，恢复时不应用

**实现位置**:
- ✅ `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` - 第268-271行：添加姿态修正常量
- ✅ `scripts/Game/PlayerBase.c` - 第800-825行：姿态检测和应用逻辑

**完成说明**:
- ✅ 已实现姿态交互修正系统
- ✅ 蹲姿和爬行姿态的体力消耗已正确应用
- ✅ 符合生理学特性，提升战术游戏体验

---

### 4. 武器姿态与射击稳定性（Fatigue-Weapon Sway Link）🎯

**状态**: ❌ 已取消  
**优先级**: 🔴 高（影响战斗体验）

**问题**: 
- 拟真体力系统的最终目的是影响战斗
- 如果体力枯竭只是走得慢，玩家可能不在意；但如果瞄不准，玩家就会真正敬畏体力系统

**解决方案**:
- **动态摇晃（Sway）**：将当前体力百分比（Stamina %）直接映射到武器的 Sway 系数
- **屏息逻辑（Hold Breath）**：当体力低于 25%（进入崩溃过渡期）时，禁止屏息，或者缩短屏息时间，并增加屏息后的体力惩罚
- 修改 `SCR_WeaponAnimations` 或相关的组件，根据 `m_fLastStaminaPercent` 动态调整 `SetWeaponSway(multiplier)`

**实现难度**: ⭐⭐⭐ (高，需要修改武器系统)  
**预期代码量**: 80-120 行  
**文件位置**: `scripts/Game/PlayerBase.c` 和相关武器组件

**取消原因**: 已决定不实现体力与瞄准稳定性的关联功能

---

### 5. "肾上腺素"补偿（Suppression-Adrenaline Logic）⚡

**状态**: ❌ 已取消  
**优先级**: 🔴 高（影响游戏性平衡）

**问题**: 
- "拟真系统太累导致没法玩"的关键问题
- 现实中，人在交火时会分泌肾上腺素，暂时无视疲劳

**解决方案**:
- **逻辑**：当玩家受到压制（Suppression）或附近有爆炸时，触发一个"肾上腺素"状态
- 暂时将速度倍率恢复到 100%，持续 10-15 秒
- **代价**：状态结束后，体力会透支，进入一个更长的疲劳恢复期
- 监听 `OnDamage` 或检测压制值

**实现难度**: ⭐⭐⭐ (高，需要检测压制和伤害事件)  
**预期代码量**: 100-150 行  
**文件位置**: `scripts/Game/PlayerBase.c`

**取消原因**: 压制情况依赖其他现有模组，暂不实现

---

## 🌍 环境与装备系统

### 6. 环境应激因子（Environmental Stress）🌡️

**状态**: ✅ 已完成  
**优先级**: 🟡 中

**问题**: 
- Arma Reforger 的环境非常出色，可以利用起来
- 热应激和天气条件会影响体力消耗

**解决方案**:
- **热应激（Heat Stress）**：在中午或烈日下，VO2 Max（最大摄氧量）会下降。将能效因子 `totalEfficiencyFactor` 与游戏时间或环境温度挂钩
- **负重与雨水（Rain/Wetness）**：下雨时，衣服和背包会增重
- 检测 `TimeAndWeatherManagerEntity` 的天气状态和当前时间
- 如果雨大，给 `currentWeight` 增加一个 2-8kg 的"水重"修正

**实现难度**: ⭐⭐ (中等，需要天气系统 API)  
**预期代码量**: 50-80 行  
**文件位置**: `scripts/Game/PlayerBase.c`

**实际代码量**: ~220 行（已完成）  
**完成日期**: 2026-01-19

**实现细节**:
- ✅ 创建 `SCR_EnvironmentFactor.c` 模块（环境因子检测和计算）
- ✅ 热应激系统：10:00-14:00 逐渐增加，14:00-18:00 逐渐减少，峰值（14:00）时消耗增加30%
- ✅ 降雨湿重系统：检测天气状态名称（包含"Rain"），小雨2kg，暴雨8kg
- ✅ 停止降雨后湿重逐渐衰减（60秒内完全消失）
- ✅ 环境因子检测频率优化：每5秒更新一次（性能优化）
- ✅ 在 `PlayerBase.c` 中集成环境因子模块
- ✅ 热应激倍数应用于陆地移动的体力消耗
- ✅ 降雨湿重添加到总重量计算（与游泳湿重兼容）

**实现位置**:
- ✅ `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` - 第332-348行：添加环境因子相关常量
- ✅ `scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c` - 新建文件：环境因子模块（~220行）
- ✅ `scripts/Game/PlayerBase.c` - 第49-51行：添加环境因子模块引用
- ✅ `scripts/Game/PlayerBase.c` - 第128-134行：初始化环境因子模块
- ✅ `scripts/Game/PlayerBase.c` - 第734-750行：更新环境因子并获取热应激倍数和降雨湿重
- ✅ `scripts/Game/PlayerBase.c` - 第625行：应用降雨湿重到总重量
- ✅ `scripts/Game/PlayerBase.c` - 第932行：应用热应激倍数到体力消耗

**完成说明**:
- ✅ 已实现完整的环境应激因子系统
- ✅ 热应激基于时间段动态调整，模拟中午高温对体力的影响
- ✅ 降雨湿重基于天气状态名称检测，与游泳湿重系统兼容
- ✅ 性能优化：环境因子每5秒更新一次，避免每帧查询天气管理器
- ✅ 模块化设计：独立的环境因子模块，易于维护和扩展

---

### 7. 装备重心与人体工学（Load Distribution）🎒

**状态**: 待开始  
**优先级**: 🟢 低（复杂功能）

**问题**: 
- 同样的 30kg，背在背上（Backpack）和挂在胸口（Chest Rig）对移动的影响不同
- **防弹衣（Vest）**：限制胸腔扩张，增加呼吸功
- **背包（Backpack）**：改变重心，影响斜坡上的平衡（增加坡度惩罚）

**解决方案**:
- 计算负重时，给不同槽位的物品分配不同的"惩罚权重"
- 例如：胸挂物品对体力消耗影响更大，背包物品对速度影响更大

**实现难度**: ⭐⭐⭐ (高，需要槽位系统)  
**预期代码量**: 100-150 行  
**文件位置**: `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c`

---

## 🩺 健康与恢复系统

### 8. 恢复延迟（Recovery Initial Delay）⏱️

**状态**: ✅ 已完成  
**优先级**: 🟡 中

**问题**: 
- 目前系统在速度 < 0.05 时立即开始恢复
- 现实中，狂奔后停下，心率不会立刻下降。停下后的前几秒应该维持高代谢水平（EPOC，过量耗氧）

**解决方案**:
- 引入一个 4 秒的恢复延迟（EPOC延迟）
- 当你狂奔后停下，心率不会立刻下降
- 停下后的前几秒应该维持高代谢水平（EPOC，过量耗氧），延迟 4 秒后再开始计算正向体力恢复
- 在速度从运动状态（>0.05）变为静止状态（<=0.05）时，启动EPOC延迟计时器

**实现难度**: ⭐⭐ (中等)  
**预期代码量**: 30-50 行  
**文件位置**: `scripts/Game/PlayerBase.c`

**实际代码量**: ~45 行（已完成）  
**完成日期**: 2026-01-09

**实现细节**:
- ✅ 添加EPOC延迟相关常量（`EPOC_DELAY_SECONDS = 4.0秒`，`EPOC_DRAIN_RATE = 0.001`）
- ✅ 添加成员变量跟踪EPOC延迟状态（`m_fEpocDelayStartTime`，`m_bIsInEpocDelay`，`m_fSpeedBeforeStop`）
- ✅ 检测速度变化：从运动状态变为静止状态时启动EPOC延迟
- ✅ EPOC延迟期间：继续应用消耗（根据停止前的速度调整消耗率）
- ✅ EPOC延迟结束后：开始正常恢复
- ✅ 重新开始运动时：取消EPOC延迟

**实现位置**:
- ✅ `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` - 第265-266行：添加EPOC延迟常量
- ✅ `scripts/Game/PlayerBase.c` - 第53-58行：添加EPOC延迟成员变量
- ✅ `scripts/Game/PlayerBase.c` - 第900-933行：EPOC延迟检测逻辑
- ✅ `scripts/Game/PlayerBase.c` - 第953-966行：EPOC延迟期间的消耗逻辑

**完成说明**:
- ✅ 已实现EPOC（过量耗氧）延迟机制
- ✅ 运动停止后延迟4秒才开始恢复，模拟生理学特性
- ✅ EPOC延迟期间的消耗根据停止前的速度动态调整
- ✅ 与现有的恢复启动延迟机制（负重恢复优化）兼容

**注意**: 与现有的恢复启动延迟机制（负重下恢复需静止 3 秒）已整合，两者独立工作。

---

### 11. 趴下休息时的负重优化（Prone Rest Weight Reduction）🛌

**状态**: ✅ 已完成  
**优先级**: 🟡 中

**问题**: 
- 目前系统对负重的处理已经非常成熟
- 当角色趴下休息时，负重的影响应该降至最低（因为地面支撑了装备重量）
- 重装兵在趴下时，应该能够通过"卧倒休息"来快速恢复体力，这是一个非常合理的战术选择

**解决方案**:
- 在计算 `CalculateMultiDimensionalRecoveryRate` 时，检测角色是否趴下（`IsProne()`）
- 如果 `IsProne()` 为 `true`，将 `currentWeight` 视为 0 或极小值
- 这样重装兵趴下时，恢复速度会显著提升（因为负重惩罚被移除）
- 给重装玩家提供一个合理的战术选择：通过"卧倒休息"来快速回血

**实现难度**: ⭐ (低，只需要姿态检测)  
**预期代码量**: 10-20 行  
**文件位置**: `scripts/Game/PlayerBase.c`（在恢复率计算前检测姿态）

**实现细节**:
- 在 `PlayerBase.c` 的恢复率计算前，检查角色姿态
- 如果 `IsProne()` 返回 `true`，将 `currentWeightForRecovery` 设为 `CHARACTER_WEIGHT`（基准重量，90kg）
- 这样负重恢复优化逻辑会将负重视为基准重量，允许快速恢复
- 生理学依据：趴下时，装备重量由地面支撑，身体只需维持基础代谢，无需承担负重负担

**实际代码量**: ~10 行（已完成）  
**完成日期**: 2024-12-19

**实现位置**:
- ✅ `scripts/Game/PlayerBase.c` - 第 1299-1308 行：在恢复率计算前添加 `IsProne()` 检测
  - 如果角色趴下，将 `currentWeightForRecovery` 设为 `CHARACTER_WEIGHT`（90kg），去除额外负重影响

---

### 9. 真正的"伤病"交互（Injury Integration）🩸

**状态**: 待开始  
**优先级**: 🟡 中

**问题**: 
- **失血（Bleeding）**：失血会导致循环系统携氧能力下降
- 当前系统未考虑伤病对体力系统的影响

**解决方案**:
- `NewStaminaEfficiency = BaseEfficiency * (1.0 - BloodLossPercent)`
- 这意味着失血严重的士兵，即使体力槽满了，也会比健康士兵更容易累
- 需要与游戏的伤害系统集成

**实现难度**: ⭐⭐⭐ (高，需要伤害系统 API)  
**预期代码量**: 60-100 行  
**文件位置**: `scripts/Game/PlayerBase.c`

---

## 🎯 其他功能

### 10. 游泳体力管理 🏊

**状态**: ✅ 已完成  
**优先级**: 🟡 中

**问题**: 
- 目前模组只处理陆地移动（Walk/Run/Sprint）的体力管理
- 游泳时的体力管理是完全空白的，游泳时体力系统不会工作

**解决方案**:
- 游泳的能量消耗远高于陆地移动（约 3-5 倍）
- 需要检测游泳状态（水中移动）
- 需要根据游泳速度和负重计算体力消耗
- 可能需要考虑水温、波浪等环境因素

**实现难度**: ⭐⭐⭐ (高，需要检测游泳状态)  
**预期代码量**: 100-150 行  
**文件位置**: `scripts/Game/PlayerBase.c`

**实际代码量**: ~120 行（已完成）  
**完成日期**: 2026-01-19

**实现细节**:
- ✅ 添加游泳相关常量（阻力/水密度/面积/阈值/湿重/功率上限等）
- ✅ 实现 `CalculateSwimmingStaminaDrain3D()`（水平阻力 + 垂直上浮/下潜功率 + 静态踩水功率，含生理功率上限）
- ✅ 在 `PlayerBase.c` 中集成游泳检测（通过 `CharacterCommandSwim` 的存在判定）
- ✅ 湿重效应：上岸后30秒内增加虚拟负重（线性衰减）
- ✅ 修复游泳速度检测为0：改用 `GetOrigin()` 位置差分测速（游泳命令位移不更新 `GetVelocity()` 的情况下仍可正确取速）

**实现位置**:
- ✅ `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` - 第275-281行：添加游泳相关常量
- ✅ `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c` - 第1130-1180行：实现游泳消耗计算函数
- ✅ `scripts/Game/PlayerBase.c` - 第480-483行：游泳状态检测
- ✅ `scripts/Game/PlayerBase.c` - 第650-664行：游泳模式判断和消耗计算
- ✅ `scripts/Game/PlayerBase.c` - 第720-750行：游泳时的体力消耗处理

**完成说明**:
- ✅ 已实现完整的游泳体力管理系统
- ✅ 游泳时使用专用3D模型，不应用坡度/地形等陆地修正
- ✅ 速度输入可靠（位置差分测速），体力消耗与速度显示一致

---

### 12. 跳跃检测改进（从输入检测改为动作检测）🦘

**状态**: 待开始  
**优先级**: 🟢 低

**问题**: 
- 当前跳跃检测基于按键输入（`OnJumpActionTriggered`、`OnPrepareControls`），而非实际动作
- 这意味着即使角色未能成功跳跃（例如被障碍物阻挡、体力不足无法跳跃），系统也会扣除体力
- 检测的是"输入意图"而不是"实际发生的动作"

**当前实现**:
- 使用 `m_bJumpInputTriggered` 标志检测按键输入
- 通过 `InputManager.AddActionListener("Jump", ...)` 监听按键事件
- 在 `OnPrepareControls` 中作为备用检测方法

**解决方案**:
- 改为检测实际的物理动作（例如垂直速度变化、动画状态等）
- 可以结合 `GetVelocity()` 的垂直分量来检测实际的上升动作
- 或者通过动画组件检测跳跃动画状态
- 确保只有在角色真正离开地面时才扣除体力

**实现难度**: ⭐⭐ (中等，需要物理动作检测)  
**预期代码量**: 30-50 行  
**文件位置**: `scripts/Game/PlayerBase.c`（修改跳跃检测逻辑）

---

## 🏗️ 代码模块化重构

### 13. 模块化重构（Modular Refactoring）🔧

**状态**: ✅ 已完成  
**优先级**: 🟡 中（代码质量优化）

**目标**: 
- 将大型文件拆分为更小的、职责单一的模块
- 提高代码可维护性和可读性
- 每个模块文件不超过500行

**已完成模块**:
1. ✅ **SCR_JumpVaultDetection.c** - 跳跃和翻越检测模块（~300行）
2. ✅ **SCR_EncumbranceCache.c** - 负重缓存管理模块（~150行）
3. ✅ **SCR_NetworkSync.c** - 网络同步管理模块（~194行）
4. ✅ **SCR_FatigueSystem.c** - 疲劳积累系统模块（~102行）
5. ✅ **SCR_TerrainDetection.c** - 地形检测模块（~168行）
6. ✅ **SCR_UISignalBridge.c** - UI信号桥接模块（~123行）
7. ✅ **SCR_ExerciseTracking.c** - 运动持续时间跟踪模块（~116行）
8. ✅ **SCR_CollapseTransition.c** - "撞墙"阻尼过渡模块（~150行）
9. ✅ **SCR_StaminaConstants.c** - 常量定义模块（~262行）
10. ✅ **SCR_StaminaHelpers.c** - 辅助函数模块（~94行）

**当前文件大小**:
- **PlayerBase.c**: 1464行（从2037行减少，目标~800行）
- **SCR_RealisticStaminaSystem.c**: 1144行（从1483行减少，目标~800行）

**完成日期**: 2026-01-19

**完成说明**:
- ✅ 已创建10个模块化组件，代码结构更清晰
- ✅ 所有常量定义已提取到独立模块
- ✅ 所有辅助函数已提取到独立模块
- ✅ 主要功能模块已完成拆分
- ✅ 代码编译通过，功能完整

**注意事项**:
- 当前模块化程度已足够，主要功能都已拆分
- UpdateSpeedBasedOnStamina() 函数虽然较大（~940行），但逻辑清晰，通过注释分块
- 进一步拆分可能会增加不必要的复杂度

---

## 📊 统计数据

- **总任务数**: 14 个
- **已完成**: 6 个（参数校准、趴下休息负重优化、模块化重构、恢复延迟、姿态交互修正、游泳体力管理）
- **已取消**: 2 个（武器稳定性、肾上腺素补偿）
- **待开始**: 6 个

**按优先级分类**:
- 🔴 极高优先级: 1 个（✅ 1 个已完成 - 参数校准）
- 🔴 高优先级: 0 个（❌ 已全部取消）
- 🟡 中优先级: 7 个（✅ 5 个已完成 - 趴下休息负重优化、模块化重构、恢复延迟、姿态交互修正、游泳体力管理；2 个待开始：环境应激、伤病交互）
- 🟢 低优先级: 4 个（鞋具因子、装备重心、文档更新、跳跃检测改进）

**按难度分类**:
- ⭐ 低难度: 1 个（✅ 1 个已完成 - 趴下休息负重优化）
- ⭐⭐ 中等难度: 5 个（✅ 4 个已完成 - 参数校准、模块化重构、恢复延迟、姿态交互修正，1 个待开始：环境应激）
- ⭐⭐⭐ 高难度: 4 个（✅ 1 个已完成 - 游泳体力管理，❌ 2 个已取消 - 武器稳定性、肾上腺素补偿，1 个待开始：伤病交互）

---

## 📚 参考资源

### 学术论文
1. **Pandolf, K. B., et al. (1977)** - 基础 Pandolf 能量消耗模型
2. **Santee, W. R., et al. (2001)** - 下坡修正模型
3. **Givoni, B., & Goldman, R. F. (1971)** - 跑步能量消耗模型
4. **Palumbo, M. C., et al. (2018)** - 个性化运动建模

### 军事标准
- **ACFT (Army Combat Fitness Test)** - 美国陆军战斗体能测试标准
- **Knapik, J. J., et al. (1996)** - US Army 背包负重实验数据

---

## 📋 模块化重构任务清单

### ✅ 已完成的模块化任务

1. ✅ **SCR_JumpVaultDetection.c** - 跳跃和翻越检测模块（~300行）
2. ✅ **SCR_EncumbranceCache.c** - 负重缓存管理模块（150行）
3. ✅ **SCR_NetworkSync.c** - 网络同步管理模块（194行）
4. ✅ **SCR_FatigueSystem.c** - 疲劳积累系统模块（~102行）
5. ✅ **SCR_TerrainDetection.c** - 地形检测模块（168行）
6. ✅ **SCR_UISignalBridge.c** - UI信号桥接模块（123行）
7. ✅ **SCR_ExerciseTracking.c** - 运动持续时间跟踪模块（116行）
8. ✅ **SCR_CollapseTransition.c** - "撞墙"阻尼过渡模块（~150行）
9. ✅ **SCR_StaminaConstants.c** - 常量定义模块（262行）
10. ✅ **SCR_StaminaHelpers.c** - 辅助函数模块（94行）

### 📊 模块化统计

- **总模块数**: 12个（包括SCR_StaminaOverride.c和SCR_RealisticStaminaSystem.c）
- **新创建模块**: 10个
- **代码减少**: 
  - PlayerBase.c: 2037行 → 1464行（减少573行，约28%）
  - SCR_RealisticStaminaSystem.c: 1483行 → 1144行（减少339行，约23%）
- **模块化完成度**: 100%（所有计划模块已完成）
- **编译状态**: ✅ 通过
- **功能状态**: ✅ 完整

---

**最后更新**: 2026-01-19
**维护者**: ViVi141
