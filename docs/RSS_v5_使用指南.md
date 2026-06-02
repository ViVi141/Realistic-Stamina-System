# RSS v5 使用指南

## 概述

本指南说明如何在游戏中启用和使用 RSS v5 的新功能。

---

## 1. 启用 v5 系统

### 1.1 添加网络同步组件

如果你希望无氧状态能够在多人游戏中同步，需要在玩家实体预制体中添加网络同步组件：

1. 打开玩家预制体（通常是 `Character_US_Rifleman.et` 或类似文件）
2. 添加 `SCR_AnaerobicStateComponent_V5` 组件
3. 保存预制体

**注意**: 如果不添加此组件，v5 系统仍然可以正常工作，但无氧状态不会在网络上同步。

### 1.2 启用调试输出（可选）

如果要查看 v5 系统的运行状态，可以启用调试日志：

1. 打开游戏内的 RSS 配置界面（如果有）
2. 启用 "调试日志" 选项
3. 或者通过配置文件设置 `m_bDebugLogEnabled = true`

启用后，你会在控制台看到类似以下的输出：
```
[RSS v5] 冲刺冷却开始: 时长=180.0s 类型=完全耗尽
[RSS v5] 代谢过载警告: 比例=1.35 等级=轻度
[RSS v5 HUD] 冷却剩余: 175.2s
[RSS v5 HUD] 速度限制: 85% 状态=正常
```

---

## 2. v5 系统功能说明

### 2.1 无氧冲刺系统

**功能描述**:
- 冲刺时消耗"无氧能量"（独立于有氧体力）
- 无氧能量耗尽后强制进入长冷却（180秒）
- 提前释放冲刺可获得较短冷却（75-180秒，根据剩余能量）

**游戏体验**:
- **战术冲刺**: 短时间冲刺 → 较短冷却（75-120秒）
- **全力冲刺**: 持续冲刺至耗尽 → 长冷却（180秒）

**参数调节** (在 `SCR_RSS_V5_Params.c` 中):
```cpp
// 无氧能量参数
const float ANAEROBIC_DRAIN_RATE = 0.08;           // 消耗速率（每秒8%）
const float ANAEROBIC_RECOVERY_BASE = 0.01;        // 恢复速率（每秒1%）
const float ANAEROBIC_SPRINT_ENABLE_THRESHOLD = 0.20;  // 启用冲刺阈值（20%）

// 冷却时间
const float ANAEROBIC_COOLDOWN_FULL = 180.0;       // 完全耗尽冷却（秒）
const float ANAEROBIC_COOLDOWN_SHORT = 75.0;       // 战术释放最短冷却（秒）
const float ANAEROBIC_EARLY_RELEASE_BONUS = 0.4;   // 提前释放奖励系数
```

### 2.2 代谢速度限制系统

**功能描述**:
- 当代谢功率超过可持续阈值（默认400W）时，自动降低移动速度
- 玩家可以"硬撑"（Override）暂时忽略限制（持续45秒）
- 速度限制最低降至50%

**游戏体验**:
- **正常行军**: 保持在可持续功率下，无速度限制
- **过载行军**: 超过阈值 → 速度逐渐降低 → 提示"代谢过载"
- **硬撑模式**: 按特定按键（待实现）→ 暂时解除速度限制

**参数调节** (在 `SCR_RSS_V5_Params.c` 中):
```cpp
// 代谢限制参数
const float METABOLIC_SUSTAINABLE_WATTS = 400.0;           // 可持续功率阈值
const float METABOLIC_FORCED_SLOWDOWN_DECAY = 0.02;        // 降速衰减率
const float METABOLIC_FORCED_SLOWDOWN_MIN_RATIO = 0.5;     // 最小速度比例（50%）
const bool METABOLIC_ENABLE_PLAYER_OVERRIDE = true;        // 允许玩家硬撑
const float METABOLIC_OVERRIDE_MAX_DURATION = 45.0;        // 硬撑最大持续时间（秒）
```

### 2.3 速度-消耗闭环 (v_drain)

**功能描述**:
- 体力消耗基于"实际速度"而非"理论速度"
- 修复 v4 中"固定消耗速度"导致的 bug

**游戏体验**:
- 体力不足时，速度降低 → 消耗也相应降低 → 更容易恢复
- 避免"精疲力尽后仍按全速消耗"的不合理情况

---

## 3. UI 显示元素

### 3.1 冲刺冷却圆环

**位置**: 待实现（当前为调试输出）

**显示内容**:
- 冷却剩余时间
- 冷却类型（"完全耗尽" 或 "战术释放"）

**示例**:
```
[冷却圆环]
 ⏱ 175s
 类型: 完全耗尽
```

### 3.2 代谢过载警告

**位置**: 待实现（当前为调试输出）

**显示内容**:
- 速度限制百分比
- 过载等级（"轻度" 或 "严重"）
- 硬撑状态（如果启用）

**示例**:
```
[代谢过载]
 ⚠ 速度限制: 85%
 等级: 轻度
 状态: 正常
```

---

## 4. 开发者调试

### 4.1 查看无氧状态

在 `SCR_AnaerobicBurstState.c` 中调用 `DebugPrint()`:
```cpp
void DebugPrint()
{
    Print(string.Format("[AnaerobicState] Energy=%.2f Cooldown=%.1f CanSprint=%s",
        m_fAnaerobicEnergy, m_fCooldownRemaining, canSprintStr));
}
```

输出示例:
```
[AnaerobicState] Energy=0.65 Cooldown=0.0 CanSprint=true
```

### 4.2 查看代谢限制器状态

在 `SCR_MetabolicSpeedLimiter.c` 中调用 `DebugPrint()`:
```cpp
void DebugPrint()
{
    Print(string.Format("[MetabolicLimiter] SpeedRatio=%.2f Override=%s",
        m_fCurrentSpeedRatio, overrideStr));
}
```

输出示例:
```
[MetabolicLimiter] SpeedRatio=0.85 Override=false
```

### 4.3 Pandolf 功率监控

在 `PlayerBase.c` 的代谢限制器更新处，可以添加调试输出：
```cpp
if (IsRssDebugEnabled())
{
    Print(string.Format("[RSS v5] Pandolf功率: %.1fW 可持续: %.1fW 比例: %.2f",
        pandolfWatts, sustainableWatts, pandolfWatts / sustainableWatts));
}
```

---

## 5. 参数调节建议

### 5.1 调节无氧能量消耗

**如果冲刺时间太短**:
- 降低 `ANAEROBIC_DRAIN_RATE`（例如从 0.08 改为 0.06）
- 或增加初始能量（当前默认 1.0，可增加到 1.2）

**如果冷却时间太长**:
- 降低 `ANAEROBIC_COOLDOWN_FULL`（例如从 180s 改为 120s）
- 或增加 `ANAEROBIC_EARLY_RELEASE_BONUS`（从 0.4 改为 0.5）

### 5.2 调节代谢限制

**如果限制触发太频繁**:
- 提高 `METABOLIC_SUSTAINABLE_WATTS`（例如从 400W 改为 500W）

**如果限制太严格**:
- 提高 `METABOLIC_FORCED_SLOWDOWN_MIN_RATIO`（例如从 0.5 改为 0.7）
- 或降低 `METABOLIC_FORCED_SLOWDOWN_DECAY`（从 0.02 改为 0.015）

### 5.3 调节硬撑机制

**如果硬撑时间太短**:
- 增加 `METABOLIC_OVERRIDE_MAX_DURATION`（例如从 45s 改为 60s）

**如果希望禁用硬撑**:
- 设置 `METABOLIC_ENABLE_PLAYER_OVERRIDE = false`

---

## 6. 常见问题

### Q1: 为什么看不到 UI 显示？

**A**: 当前 UI 组件 (`SCR_StaminaHUDComponent_V5`) 仅实现了调试输出。要看到实际的 UI 元素，需要：
1. 启用调试日志
2. 或者实现 Widget UI 控件（待开发）

### Q2: 网络同步是否正常工作？

**A**: 网络同步需要在玩家预制体中添加 `SCR_AnaerobicStateComponent_V5` 组件。如果没有添加，系统仍然可以在单机模式下正常工作。

### Q3: 如何测试无氧冲刺系统？

**A**: 
1. 启用调试日志
2. 进入游戏，持续冲刺
3. 观察控制台输出：
   ```
   [AnaerobicState] Energy=0.92 Cooldown=0.0 CanSprint=true
   [AnaerobicState] Energy=0.84 Cooldown=0.0 CanSprint=true
   ...
   [AnaerobicState] Energy=0.01 Cooldown=180.0 CanSprint=false
   [RSS v5] 冲刺冷却开始: 时长=180.0s 类型=完全耗尽
   ```

### Q4: 如何测试代谢速度限制？

**A**:
1. 启用调试日志
2. 背负重物（>50kg）
3. 在陡坡上行军
4. 观察控制台输出：
   ```
   [RSS v5] Pandolf功率: 485.2W 可持续: 400.0W 比例: 1.21
   [RSS v5] 代谢过载警告: 比例=1.21 等级=轻度
   [MetabolicLimiter] SpeedRatio=0.95 Override=false
   [MetabolicLimiter] SpeedRatio=0.90 Override=false
   ...
   ```

---

## 7. 兼容性说明

### 7.1 v4 系统兼容

v5 系统与 v4 完全兼容：
- v5 组件作为**附加层**运行，不影响 v4 的核心逻辑
- 可以通过配置开关独立启用/禁用 v5 功能（待实现）

### 7.2 多人游戏兼容

- **服务端**: 必须运行 RSS v5 代码
- **客户端**: 如果没有添加 `SCR_AnaerobicStateComponent_V5`，会退化为"仅本地模式"
- **混合模式**: 服务端权威 + 客户端预测，确保公平性

---

## 8. 后续开发路线图

### Phase 2 集成 (计划中)

1. **StaminaUpdateCoordinator 集成**
   - 集成 `SCR_DrainVelocityCalculator_V5`
   - 替换 v4 的固定消耗速度

2. **SpeedCalculation 集成**
   - 集成 `SCR_SpeedRecalibration_V5`
   - 实现 Tobler 函数的速度重校准

3. **完整 UI 实现**
   - 实现冷却圆环的 Widget
   - 实现过载警告的动画效果
   - 实现硬撑状态的视觉反馈

4. **音效系统**（可选）
   - 冲刺冷却开始音效
   - 代谢过载警告音效
   - 硬撑状态音效

---

## 9. 反馈与支持

如果你在使用 v5 系统时遇到问题，请提供以下信息：

1. **调试日志**: 启用调试输出，复制相关日志
2. **场景描述**: 什么情况下触发的问题
3. **参数配置**: 你修改了哪些参数
4. **预期行为**: 你期望的行为是什么
5. **实际行为**: 实际发生了什么

---

**RSS v5 使用指南 - 结束**

最后更新: 2026-06-02 22:15 (UTC+8)
