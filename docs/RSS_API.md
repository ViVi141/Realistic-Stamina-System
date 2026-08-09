# Realistic Stamina System (RSS) - 外部模组 API

> **中文** | [English](en/API.md)

供其他模组从 RSS 获取玩家体力状态与环境信息的接口。

## 实现位置（与代码一致）

| 内容 | 路径 |
|------|------|
| API 入口 | `scripts/Game/RSS/NetworkConfig/SCR_RSS_API.c`（`SCR_RSS_API`、`RSS_PlayerInfo`、`RSS_EnvironmentInfo`） |
| 数据导出 | `scripts/Game/RSS/NetworkConfig/SCR_RSS_DataExport.c`（`SCR_RSS_DataExport`、`RSS_ExportData`、`RSS_ExportPlayerEntry`） |

## 依赖

- 模组需依赖 RSS 模组（`Realistic Stamina System`）
- 调用前确保目标实体为角色（ChimeraCharacter）

## 有氧 STA vs W′（必读）

| 概念 | 字段 | 权威来源 | 说明 |
|------|------|----------|------|
| **有氧体力** | `staminaPercent` | `GetTargetStamina()` / `m_fTargetStamina` | **不受** W′→引擎条表现映射影响 |
| **W′ 归一化** | `wPrimePool01` | `AnaerobicBurst` / CP–W′ 池 | `0`=空，`1`=满；**请优先读这个** |
| **W′ 焦耳** | `wPrimeJoules` / `wPrimeMaxJoules` | 同上 + 当前预设 max | 绝对焦耳与容量 |
| 兼容别名 | `anaerobicPercent` | 同 `wPrimePool01` | **已弃用**，勿写新逻辑 |
| 引擎条表现 | （API **不**暴露） | `GetStamina()` transient | 仅原生晃动/模糊；**不是**有氧权威 |

```c
RSS_PlayerInfo info = SCR_RSS_API.GetPlayerInfo(player);
if (info.isValid)
{
    float sta = info.staminaPercent;       // 有氧 0~1
    float w01 = info.wPrimePool01;         // W′ 0~1
    float wJ = info.wPrimeJoules;          // 当前焦耳
    float wMax = info.wPrimeMaxJoules;     // 容量焦耳
    // 需要百分比条：w01 * 100，或 wJ / wMax（wMax>0 时）
}
```

高级：也可 `ctrl.RSS_GetWPrimeBurst().GetPool()` / `GetWPrimeJoules()`（`GetRssController`）。

---

## 用法示例

```c
IEntity player = SCR_PlayerController.GetLocalControlledEntity();
if (!player)
    return;

RSS_PlayerInfo playerInfo = SCR_RSS_API.GetPlayerInfo(player);
if (playerInfo.isValid)
{
    PrintFormat("STA=%1%% W'=%2%% (%3/%4 J) sprintOk=%5",
        playerInfo.staminaPercent * 100.0,
        playerInfo.wPrimePool01 * 100.0,
        playerInfo.wPrimeJoules,
        playerInfo.wPrimeMaxJoules,
        playerInfo.sprintAllowed);
}

RSS_EnvironmentInfo envInfo = SCR_RSS_API.GetEnvironmentInfo(player);
if (envInfo.isValid)
{
    PrintFormat("气温: %1°C | 降雨: %2 | 室内: %3",
        envInfo.temperature,
        envInfo.rainIntensity,
        envInfo.isIndoor);
}
```

## API 方法

### SCR_RSS_API.GetRssController(IEntity entity)

获取 RSS 控制的角色控制器组件（高级用法）。

| 参数 | 类型 | 说明 |
|------|------|------|
| entity | IEntity | 角色实体 |
| **返回** | SCR_CharacterControllerComponent | 控制器或 null |

---

### SCR_RSS_API.GetPlayerInfo(IEntity entity)

获取玩家当前体力与运动状态。

| 参数 | 类型 | 说明 |
|------|------|------|
| entity | IEntity | 角色实体 |
| **返回** | RSS_PlayerInfo | 玩家信息结构体 |

**RSS_PlayerInfo 字段：**

| 字段 | 类型 | 说明 |
|------|------|------|
| staminaPercent | float | **有氧**体力 (0.0 ~ 1.0)，权威条 |
| speedMultiplier | float | 当前速度倍率 (约 0.15 ~ 1.0) |
| currentSpeed | float | 当前水平速度 (m/s) |
| movementPhase | int | 0=idle, 1=walk, 2=run, 3=sprint |
| isSprinting | bool | 是否在冲刺 |
| isExhausted | bool | 是否精疲力尽（有氧侧判定） |
| isSwimming | bool | 是否在游泳 |
| currentWeight | float | 当前负重 (kg) |
| wPrimePool01 | float | **W′** 归一化 (0.0 ~ 1.0) |
| wPrimeJoules | float | **W′** 当前焦耳 |
| wPrimeMaxJoules | float | 当前预设 **W′_max** 焦耳 |
| anaerobicPercent | float | @deprecated＝`wPrimePool01` |
| sprintCooldownRemainingSec | float | 冷却剩余秒（多为 0） |
| sprintAllowed | bool | 当前是否允许冲刺 |
| isValid | bool | 数据是否有效 |

---

### SCR_RSS_API.GetEnvironmentInfo(IEntity entity)

获取角色所在位置的环境信息。

| 参数 | 类型 | 说明 |
|------|------|------|
| entity | IEntity | 角色实体（用于室内检测） |
| **返回** | RSS_EnvironmentInfo | 环境信息结构体 |

**RSS_EnvironmentInfo 字段：**

| 字段 | 类型 | 说明 |
|------|------|------|
| temperature | float | 气温 (°C) |
| rainIntensity | float | 降雨强度 (0.0 ~ 1.0) |
| windSpeed | float | 风速 (m/s) |
| windDirection | float | 风向 (度) |
| surfaceWetness | float | 地表湿度 (0.0 ~ 1.0) |
| totalWetWeight | float | 总湿重 (kg) |
| isIndoor | bool | 是否室内 |
| heatStressMultiplier | float | 热应激倍数 |
| heatStressPenalty | float | 热应激惩罚 |
| coldStressPenalty | float | 冷应激惩罚 |
| isValid | bool | 数据是否有效 |

---

### SCR_RSS_API.IsRssManaged(IEntity entity)

检查实体是否由 RSS 管理。

| 参数 | 类型 | 说明 |
|------|------|------|
| entity | IEntity | 角色实体 |
| **返回** | bool | 是否有效 |

## 数据导出（文件桥接）

启用配置 `m_bDataExportEnabled` 后，服务器会按 `m_iDataExportIntervalMs` 间隔将玩家数据写入 JSON：

- **路径**：`$profile:RSS_PlayerData.json`（profile 目录）
- **格式**：JSON，含 `timestamp` 与 `players` 数组
- **用途**：供外部应用（命令控制台等）轮询读取

> 当前导出条目仍以 STA/速度/环境为主；**脚本侧读 W′ 请用 `GetPlayerInfo`**（含 `wPrimePool01` / 焦耳）。导出 JSON 若需 W′ 字段可另提需求。

`timestamp` 由 `GetGame().GetWorld().GetWorldTime()` 写入（引擎世界时间毫秒；**不是** Unix Epoch 秒）。

配置项（`RealisticStaminaSystem.json`）：

- `m_bDataExportEnabled`：是否启用（默认 false）
- `m_iDataExportIntervalMs`：导出间隔毫秒（默认 1000）

## 注意事项

1. **isValid 检查**：调用后务必检查；false 表示无 RSS 组件或未初始化。
2. **返回值复用**：静态缓存，下次调用覆盖；需保存请复制字段。
3. **执行端**：玩家可在客户端读本地计算；AI 宜在服务器读。
4. **不要**用引擎 `GetStamina()` 当有氧或 W′ 权威（可能含 W′ 表现伪装）。
5. **数据导出**：仅服务器写文件；环境在导出前 `ForceUpdate`。
