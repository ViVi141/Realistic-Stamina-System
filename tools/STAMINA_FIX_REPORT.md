# 体力消耗问题修复报告

## 问题描述
1. 初版：游戏中无论如何行动都不消耗体力。
2. 修复后：静止站立比奔跑更加消耗体力（逻辑颠倒）。

## 参考版本
用户提供了可正常使用的参考实现：`example/scripts/Game`。本修复以该版本为准进行对齐。

## 修复内容

### 修改文件
- `scripts/Game/Components/Stamina/SCR_StaminaConsumption.c`

### 具体改动

1. **恢复全局 ×0.2 转换（与 example 一致）**
   - 将「仅 Pandolf 分支内 ×0.2」改回「所有分支结束后统一 ×0.2」。
   - 与 `example` 的 `SCR_StaminaConsumption.c` 逻辑一致。

2. **不改动：室内坡度归零**
   - 保留 `if (environmentFactor.IsIndoor()) gradePercent = 0.0`（按你的要求不做对齐/不修复）。

3. **不改动：itemBonus 声明**
   - 保留 `const float itemBonus = 1.0`（按你的要求不做对齐/不修复）。

### 与 example 的对照

| 项目 | example | 当前（修复后） |
|------|---------|----------------|
| 全局 `baseDrainRateByVelocity *= 0.2` | 有 | 有 |
| 室内坡度归零 | 无 | 有 |
| `itemBonus` | `float` | `const float` |

## 测试建议

请验证以下场景与 `example` 体验一致：

1. **静止站立**（空载/负重）：消耗应明显低于奔跑。
2. **慢走 → 跑步 → 冲刺**：消耗随强度递增。
3. **负重跑步**：比空载消耗更多。

## 总结

✅ **已按 `example/scripts/Game` 对齐** `SCR_StaminaConsumption.c` 的消耗与转换逻辑。  
若仍有异常，可继续对比 `example` 中的 `SCR_StaminaUpdateCoordinator`、`SCR_RealisticStaminaSystem`、`SCR_StaminaConstants` 等模块。

---

## 优化器与数字孪生

优化器（`rss_super_pipeline`）使用的数字孪生（`rss_digital_twin_fix`）其**消耗模型**与 C 端 Consumption **不一致**（孪生为速度阈值模型，C 为 Pandolf/Givoni/静态 + 全局 ×0.2）。  
若希望优化结果对应实际游戏表现，**逻辑与数值都需要调整**。详见 **`tools/OPTIMIZER_ALIGNMENT.md`**。
