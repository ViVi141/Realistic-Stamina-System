# Debug 开关和 Hint 显示开关 - 应用验证

## 📌 两个开关的定义

### 位置: [SCR_RSS_Settings.c](scripts/Game/Components/Stamina/SCR_RSS_Settings.c#L649-L680)

```c
[Attribute(defvalue: "0", desc: "Enable debug logging (输出调试日志)")]
bool m_bDebugLogEnabled;      // ✅ Debug日志开关

[Attribute(defvalue: "1", desc: "Enable hint display (显示提示文字)")]
bool m_bHintDisplayEnabled;   // ✅ Hint显示开关
```

**默认值**:
- `m_bDebugLogEnabled` = **false** (默认关闭调试)
- `m_bHintDisplayEnabled` = **true** (默认打开提示)

---

## 🔍 Debug 开关应用位置

### 1️⃣ 日志输出 - 参数日志
**位置**: [SCR_DebugDisplay.c#L352](scripts/Game/Components/Stamina/SCR_DebugDisplay.c#L352)

```c
static void LogDebugInfo(/* params */)
{
    SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
    
    // ✅ 开关检查：如果没开，直接退出（性能消耗极低）
    if (!settings || !settings.m_bDebugLogEnabled)
        return;  // 关闭时零开销
    
    // 只有开关打开才执行以下代码
    float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
    if (currentTime < m_fNextDebugLogTime)
        return;
    
    // 只对本地控制的玩家输出
    if (params.owner != SCR_PlayerController.GetLocalControlledEntity())
        return;
    
    // 输出详细的调试信息：坡度、冲刺状态、负重、地形、环境因子等
    Print("[调试日志] 坡度: " + slopeInfo + ", 冲刺: " + sprintInfo + ", ...);
}
```

**输出的调试信息包括**:
- 坡度角度 (slope angle)
- 冲刺状态 (sprint state)
- 负重与百分比 (encumbrance)
- 地形信息 (terrain type)
- 环境因子 (heat stress, wind, rain)
- 姿态转换信息 (stance changes)

### 2️⃣ 日志输出 - 状态日志
**位置**: [SCR_DebugDisplay.c#L423](scripts/Game/Components/Stamina/SCR_DebugDisplay.c#L423)

```c
static void LogStatusInfo(/* params */)
{
    SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
    
    // ✅ 开关检查
    if (!settings || !settings.m_bDebugLogEnabled)
        return;  // 关闭时零开销
    
    // 输出实时状态
    Print("[状态 / Status] 速度: %1 m/s | 体力: %2%% | 速度倍数: %3x | Type: %4");
}
```

**输出的状态信息包括**:
- 当前速度 (current speed)
- 体力百分比 (stamina %)
- 速度倍数 (speed multiplier)
- 运动类型 (movement type)

**输出频率**: 每秒一次（带时间间隔检查）

---

## 💡 Hint 显示开关应用位置

### 1️⃣ HUD 系统信息显示
**位置**: [SCR_DebugDisplay.c#L558](scripts/Game/Components/Stamina/SCR_DebugDisplay.c#L558)

```c
static void DisplayHintInfo(/* params */)
{
    SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
    
    // ✅ 开关检查：如果没开，直接退出
    if (!settings || !settings.m_bHintDisplayEnabled)
        return;  // 关闭时不显示 HUD 提示
    
    // 只对本地控制的玩家显示
    if (params.owner != SCR_PlayerController.GetLocalControlledEntity())
        return;
    
    // 显示以下信息：
    // - 环境数据（温度、风速、风向、室内/室外）
    // - 地形密度
    // - 湿重（降雨 + 游泳）
    // - 精疲力尽警告
    // - 恢复状态
    // ... 其他 HUD 信息
}
```

**显示的 HUD 信息包括**:
- ✅ 温度 (temperature)
- ✅ 风速和风向 (wind speed & direction)
- ✅ 室内/室外 (indoor/outdoor)
- ✅ 地形密度 (terrain density)
- ✅ 湿重 (wet weight: rain + swimming)
- ✅ 精疲力尽警告 (exhaustion warning)
- ✅ 恢复状态提示 (recovery status)

### 2️⃣ 体力值 HUD 更新
**位置**: [SCR_DebugDisplay.c#L619](scripts/Game/Components/Stamina/SCR_DebugDisplay.c#L619)

```c
static void UpdateStaminaHUD(/* params */)
{
    SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
    
    // ✅ 开关检查
    if (!settings || !settings.m_bHintDisplayEnabled)
        return;  // 关闭时不更新 HUD
    
    // 只对本地控制的玩家更新
    if (owner != SCR_PlayerController.GetLocalControlledEntity())
        return;
    
    // 更新 HUD 中的体力值显示
    SCR_StaminaHUDComponent.UpdateStaminaValue(staminaPercent);
}
```

---

## 🔧 开关的实际影响

### Debug 开关 OFF (m_bDebugLogEnabled = false)
```
❌ 不输出参数日志 (NO parameter logs)
❌ 不输出状态日志 (NO status logs)
✅ 零性能开销 (zero performance cost)
✅ 输出窗口保持干净 (clean output window)
```

### Debug 开关 ON (m_bDebugLogEnabled = true)
```
✅ 每0.5秒输出详细参数日志 (detailed parameter logs every 0.5s)
✅ 每1秒输出状态日志 (status logs every 1s)
⚠️ 少量性能开销 (minor performance impact)
📊 用于调试和性能分析 (for debugging & analysis)
```

### Hint 开关 OFF (m_bHintDisplayEnabled = false)
```
❌ HUD 提示文字完全隐藏 (no HUD hints)
❌ 环境信息不显示 (no environment info display)
❌ 体力条不更新 (stamina bar not updated)
✅ 最大视觉清晰度 (maximum visual clarity)
✅ 轻微性能提升 (minor performance boost)
```

### Hint 开关 ON (m_bHintDisplayEnabled = true)
```
✅ HUD 显示体力条 (stamina bar visible)
✅ 显示环境信息 (environment info displayed)
✅ 显示地形信息 (terrain info displayed)
✅ 显示警告提示 (warning hints displayed)
⚠️ 轻微性能开销 (minor performance cost)
📈 增强游戏信息反馈 (enhanced gameplay feedback)
```

---

## 📋 开关在配置文件中的位置

### 在 JSON 配置中
```json
{
  "m_bDebugLogEnabled": false,
  "m_bHintDisplayEnabled": true,
  ...
}
```

### 修改方法

1. **编辑 JSON 文件**:
   ```
   $profile:RealisticStaminaSystem.json
   ```
   
2. **修改参数**:
   ```json
   {
     "m_bDebugLogEnabled": true,      // 改为 true 开启调试日志
     "m_bHintDisplayEnabled": false   // 改为 false 隐藏 HUD 提示
   }
   ```

3. **重启服务器** - 配置自动重新加载

---

## 🔄 开关加载流程

```
JSON 文件
    ↓
SCR_RSS_ConfigManager.Load()
    ↓ (JsonLoadContext.ReadValue)
SCR_RSS_Settings 对象
    ↓ (m_bDebugLogEnabled / m_bHintDisplayEnabled)
SCR_DebugDisplay 中的检查
    ↓ if (!settings.m_bDebugLogEnabled) return;
    ↓ if (!settings.m_bHintDisplayEnabled) return;
功能 ON/OFF ✅
```

---

## ⚡ 性能优化特点

### 零开销设计
```c
// 如果开关没开，第一行就返回，零执行成本
if (!settings || !settings.m_bDebugLogEnabled)
    return;  // ✅ 极其轻量的检查

// 后续代码（日志格式化、输出等）完全不执行
```

**关键**: 开关关闭时，日志格式化、字符串操作等 CPU 密集的操作都不会执行！

---

## 📊 总结表格

| 开关 | 默认值 | 作用 | 位置 |
|-----|-------|------|------|
| `m_bDebugLogEnabled` | **OFF** | 控制详细调试日志输出 | [SCR_DebugDisplay.c#L352, #L423](scripts/Game/Components/Stamina/SCR_DebugDisplay.c) |
| `m_bHintDisplayEnabled` | **ON** | 控制 HUD 提示和环境信息显示 | [SCR_DebugDisplay.c#L558, #L619](scripts/Game/Components/Stamina/SCR_DebugDisplay.c) |

---

## ✅ 验证结论

### Debug 开关
- ✅ **完全正常** - 在 SCR_DebugDisplay.c 中有 **2处调用**
- ✅ 日志输出有完整的开关检查
- ✅ 关闭时零开销（早期返回）
- ✅ 可通过 JSON 配置动态控制

### Hint 显示开关  
- ✅ **完全正常** - 在 SCR_DebugDisplay.c 中有 **2处调用**
- ✅ HUD 显示有完整的开关检查
- ✅ 关闭时不执行 HUD 更新
- ✅ 可通过 JSON 配置动态控制

**结论**: 两个开关都完全正常，流程正确，功能有效 ✨

---

**检查时间**: 2026年1月26日  
**系统评分**: 5/5 ⭐ - **完美实现**
