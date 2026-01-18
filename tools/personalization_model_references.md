# 个性化运动建模文献参考

## 核心文献

### Palumbo et al. (2018) - Personalizing physical exercise in a computational model of fuel homeostasis
**来源**: [PLOS Computational Biology](https://journals.plos.org/ploscompbiol/article?id=10.1371/journal.pcbi.1006073)

**关键发现**：

1. **运动效果的多因素依赖性**
   - 运动效果取决于**强度**（intensity）、**持续时间**（duration）、**方式**（modality）
   - 受试者特征显著影响运动响应：**年龄**、**性别**、**体重**、**健康状态**（fitness status）

2. **个性化建模的重要性**
   - 现有模型缺乏"个性化"能力
   - 需要更详细地描述运动特征和受试者特征
   - 代谢响应是动态的，需要多尺度建模

3. **代谢动力学**
   - 涉及激素（胰岛素、胰高血糖素、肾上腺素）和代谢物（葡萄糖、脂肪酸）的动力学
   - 运动强度影响代谢途径的选择（有氧 vs 无氧）

## 对当前模型的改进建议

### 1. 健康状态/体能水平（Fitness Level）

**当前状态**：模型未考虑个体健康状态差异

**建议实现**：
- 添加 `FITNESS_LEVEL` 参数（0.0-1.0）
  - 0.0 = 未训练（untrained）
  - 0.5 = 普通健康（average fitness）
  - 1.0 = 训练有素（well-trained）

**影响机制**：
- **体能水平越高，能量效率越高**
  - 基础消耗倍数：`1.0 - FITNESS_EFFICIENCY_COEFF × fitness_level`
  - 例如：训练有素者（fitness=1.0）基础消耗减少15-20%
  
- **体能水平影响恢复速度**
  - 恢复率倍数：`1.0 + FITNESS_RECOVERY_COEFF × fitness_level`
  - 例如：训练有素者恢复速度增加20-30%

- **体能水平影响速度-消耗关系**
  - 速度平方项系数：`SPEED_SQUARED_DRAIN_COEFF × (1.0 - FITNESS_EFFICIENCY_COEFF × fitness_level)`
  - 训练有素者在高速度时消耗更少

### 2. 运动持续时间的影响（Exercise Duration）

**当前状态**：模型每0.2秒独立计算，未考虑累积疲劳

**建议实现**：
- 添加累积疲劳因子（Cumulative Fatigue Factor）
  - `fatigue_factor = 1.0 + FATIGUE_ACCUMULATION_COEFF × exercise_duration_minutes`
  - 长时间运动后，相同速度的消耗会逐渐增加

**生理学依据**：
- 糖原耗竭后，依赖脂肪代谢，效率降低
- 肌肉疲劳导致机械效率下降
- 体温升高导致代谢率增加

### 3. 年龄因素（Age）

**当前状态**：模型假设标准年龄（22-26岁，基于ACFT标准）

**建议实现**（可选）：
- 添加年龄因子（Age Factor）
  - `age_factor = 1.0 + AGE_PENALTY_COEFF × (age - 25) / 10`
  - 年龄每增加10岁，基础代谢率增加5-10%
  - 最大速度能力下降：`max_speed_multiplier × (1.0 - AGE_SPEED_PENALTY × (age - 25) / 10)`

### 4. 性别因素（Gender）

**当前状态**：模型基于男性数据（ACFT标准）

**建议实现**（可选）：
- 添加性别因子（Gender Factor）
  - 女性基础代谢率通常比男性低10-15%
  - 女性最大力量/速度通常比男性低15-20%
  - 但女性耐力可能更好（体脂率更高，糖原储备更丰富）

### 5. 代谢适应（Metabolic Adaptation）

**当前状态**：模型未考虑运动中的代谢切换

**建议实现**（高级）：
- 根据运动强度和时间，动态调整能量来源
  - **有氧区**（低强度，<60% VO2max）：主要依赖脂肪，效率高
  - **混合区**（中等强度，60-80% VO2max）：糖原+脂肪混合
  - **无氧区**（高强度，>80% VO2max）：主要依赖糖原，效率低但功率高

**实现方式**：
- 根据速度比（speed_ratio）判断运动强度
- 调整基础消耗系数：`base_drain × metabolic_efficiency_factor`
  - 低强度（<0.6）：效率因子 = 0.9（更高效）
  - 中等强度（0.6-0.8）：效率因子 = 1.0（标准）
  - 高强度（>0.8）：效率因子 = 1.2（低效但高功率）

## 推荐实现优先级

### 高优先级（对游戏体验影响大）
1. ✅ **健康状态/体能水平** - 可以区分不同角色的能力差异
2. ✅ **运动持续时间影响** - 增加长时间运动的挑战性

### 中优先级（可选功能）
3. ⚠️ **代谢适应** - 增加模型的生理学准确性
4. ⚠️ **年龄因素** - 如果需要模拟不同年龄角色

### 低优先级（可能不必要）
5. ❌ **性别因素** - 如果游戏不区分性别角色

## 参考文献

1. Palumbo MC, Morettini M, Tieri P, Diele F, Sacchetti M, Castiglione F (2018) Personalizing physical exercise in a computational model of fuel homeostasis. PLoS Comput Biol 14(4): e1006073. https://doi.org/10.1371/journal.pcbi.1006073

2. Pandolf KB, Givoni B, Goldman RF (1977) Predicting energy expenditure with loads while standing or walking very slowly. J Appl Physiol 43(4): 577-581.

3. Knapik JJ, Harman E, Reynolds K (1996) Load carriage using packs: a review of physiological, biomechanical and medical aspects. Appl Ergon 27(3): 207-216.

4. Minetti AE, Moia C, Roi GS, Susta D, Ferretti G (2002) Energy cost of walking and running at extreme uphill and downhill slopes. J Appl Physiol 93(3): 1039-1046.
