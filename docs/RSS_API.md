# Realistic Stamina System (RSS) - 外部模组 API

供其他模组从 RSS 获取玩家体力状态与环境信息的接口。

## 依赖

- 模组需依赖 RSS 模组（`Realistic Stamina System`）
- 调用前确保目标实体为角色（ChimeraCharacter）

## 用法示例

```c
// 获取本地玩家实体
IEntity player = SCR_PlayerController.GetLocalControlledEntity();
if (!player)
    return;

// 获取玩家体力与运动状态
RSS_PlayerInfo playerInfo = SCR_RSS_API.GetPlayerInfo(player);
if (playerInfo.isValid)
{
    PrintFormat("体力: %1%% | 速度倍率: %2 | 速度: %3 m/s",
        playerInfo.staminaPercent * 100.0,
        playerInfo.speedMultiplier,
        playerInfo.currentSpeed);
}

// 获取环境信息
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
| staminaPercent | float | 体力百分比 (0.0 ~ 1.0) |
| speedMultiplier | float | 当前速度倍率 (0.15 ~ 1.0) |
| currentSpeed | float | 当前水平速度 (m/s) |
| movementPhase | int | 0=idle, 1=walk, 2=run, 3=sprint |
| isSprinting | bool | 是否在冲刺 |
| isExhausted | bool | 是否精疲力尽 |
| isSwimming | bool | 是否在游泳 |
| currentWeight | float | 当前负重 (kg) |
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

JSON 示例：

```json
{
  "timestamp": 1234567890,
  "players": [
    {
      "playerId": 1,
      "playerName": "Player1",
      "staminaPercent": 0.85,
      "speedMultiplier": 0.92,
      "currentSpeed": 4.2,
      "movementPhase": 2,
      "isSprinting": false,
      "isExhausted": false,
      "isSwimming": false,
      "currentWeight": 12.5,
      "temperature": 22.0,
      "rainIntensity": 0.0,
      "windSpeed": 2.1,
      "isIndoor": false
    }
  ]
}
```

配置项（在 `RealisticStaminaSystem.json` 中）：

- `m_bDataExportEnabled`：是否启用（默认 false）
- `m_iDataExportIntervalMs`：导出间隔毫秒（默认 1000）

## 注意事项

1. **isValid 检查**：调用 `GetPlayerInfo` / `GetEnvironmentInfo` 后务必检查 `isValid`，若为 false 表示实体无 RSS 组件或未初始化。
2. **返回值复用**：API 使用静态缓存返回结构体，每次调用会覆盖上次结果，如需保存请复制字段。
3. **执行端**：玩家实体在客户端调用可获得本地计算数据；AI 实体需在服务器端调用。
4. **数据导出**：仅在服务器端执行，客户端需在 profile 目录读取生成的 JSON。
5. **服务器端环境数据**：玩家体力/速度在客户端计算，服务器不运行其 UpdateSpeedBasedOnStamina。体力由引擎复制；环境（室内、气温、降雨等）在导出前由服务器调用 `EnvironmentFactor.ForceUpdate()` 独立计算，确保室内外、环境数据正确。
