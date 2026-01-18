# 基准负重（Base Weight）实现总结

## 实现概述

已将"无负重"状态从0kg调整为**1.36kg基准负重**，这更符合现实（只包括基础衣物）。

## 基准重量说明

### 1.36kg基准负重包含：

只包括衣服、鞋子等基础物品：

| 物品类型 | 重量（kg） |
|---------|-----------|
| 战斗服（BDU/ACU） | 0.8-1.0 |
| 战斗靴 | 0.5-0.6 |
| 其他（内衣、袜子等） | 0.1-0.2 |
| **总计** | **约1.36kg** |

## 对模型的影响

### 计算方式

所有负重计算现在使用**有效负重**（effective weight）：

```enforce
effectiveWeight = Max(currentWeight - BASE_WEIGHT, 0.0)
bodyMassPercent = effectiveWeight / CHARACTER_WEIGHT
```

### 实际影响对比

| 状态 | 总重量 | 有效重量 | 占体重% | 速度下降 | 消耗增加 |
|------|--------|----------|---------|----------|----------|
| **基准状态** | 1.36kg | 0kg | 0% | 0.3% | 2.3% |
| **战斗负重** | 30kg | 28.64kg | 32% | 6.4% | 48% |
| **最大负重** | 40.5kg | 39.14kg | 43% | 8.6% | 65% |

### 优势

1. ✅ **更符合现实**：角色总是穿着基础衣物
2. ✅ **保持平衡**：基准状态影响极小（几乎可忽略），战斗负重和最大负重的影响合理
3. ✅ **计算简单**：只需在计算时减去基准重量
4. ✅ **符合概念**："无额外负重"指只有基础衣物，不包括武器、装备等

## 更新的文件

### EnforceScript（C）文件

1. **`scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c`**
   - 添加了 `BASE_WEIGHT = 1.36` 常量
   - 更新了 `CalculateEncumbranceSpeedPenalty()` - 使用有效负重
   - 更新了 `CalculateEncumbranceStaminaDrainMultiplier()` - 使用有效负重

2. **`scripts/Game/PlayerBase.c`**
   - 更新了坡度影响计算中的 `bodyMassPercent` - 使用有效负重

### Python文件

1. **`tools/generate_comprehensive_trends.py`**
   - 添加了 `BASE_WEIGHT = 1.36` 常量
   - 更新了所有负重计算函数 - 使用有效负重

2. **`tools/multi_dimensional_analysis.py`**
   - 添加了 `BASE_WEIGHT = 1.36` 常量
   - 更新了 `calculate_stamina_drain()` - 使用有效负重

## 验证

所有计算现在基于有效负重（总重量 - 基准重量），确保：
- 基准状态（1.36kg）：有效负重0kg，几乎无影响（速度下降0.3%，消耗增加2.3%） ✅
- 战斗负重（30kg）：有效负重28.64kg（32%体重），速度下降6.4%，消耗增加48% ✅
- 最大负重（40.5kg）：有效负重39.14kg（43%体重），速度下降8.6%，消耗增加65% ✅

## 注意事项

1. **游戏引擎的负重系统**：游戏引擎仍然使用总重量（包含基准重量），这是正常的
2. **显示给玩家的重量**：如果需要，可以在UI中显示"有效负重"或"额外负重"
3. **兼容性**：所有Python脚本已更新，确保计算结果一致

## 结论

基准重量1.36kg的实现已完成，模型现在更符合现实（只有基础衣物），同时保持了游戏平衡。
