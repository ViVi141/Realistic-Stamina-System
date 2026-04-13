# Scripts 重构映射与整理基线

## 目标
- 建立 `scripts` 目录的重构基线。
- 统一后续拆分时的模块归属，降低跨文件耦合。
- 记录必须保持稳定的兼容面，避免联机与配置回归。

## 当前关键入口（保持稳定）
- `scripts/Game/PlayerBase.c` -> `modded class SCR_CharacterControllerComponent`
- `scripts/Game/SCR_RSS_ServerBootstrap.c` -> `modded class SCR_BaseGameMode`
- `scripts/Game/Components/Stamina/SCR_StaminaOverride.c` -> `modded class SCR_CharacterStaminaComponent`
- `scripts/Game/Components/Stamina/SCR_InventoryStorageManagerComponent_Override.c` -> `modded class SCR_InventoryStorageManagerComponent`

## 拆分后模块归属（逻辑映射）
- `PlayerBase.c`
  - 配置同步构建/哈希 -> `scripts/Game/Components/Stamina/SCR_RSS_ConfigSyncUtils.c`
  - 网络状态判定 -> `scripts/Game/Components/Stamina/SCR_RSS_NetworkStateUtils.c`
  - 战斗兴奋剂状态机 -> `scripts/Game/Components/Gadgets/SCR_CombatStimStateMachine.c`
- `SCR_EnvironmentFactor.c`
  - 天文/太阳几何/辐射基础数学 -> `scripts/Game/Components/Stamina/SCR_EnvironmentAstronomyMath.c`
  - 天气 API 读取/风阻计算 -> `scripts/Game/Components/Stamina/SCR_EnvironmentWeatherApi.c`
  - 惩罚项计算（热/冷/泥泞/地表/雨呼吸） -> `scripts/Game/Components/Stamina/SCR_EnvironmentPenaltyMath.c`

## 计划中的目标目录（下一步可继续迁移）
- `scripts/Game/RSS/Core`
- `scripts/Game/RSS/Environment`
- `scripts/Game/RSS/NetworkConfig`
- `scripts/Game/RSS/AI`
- `scripts/Game/RSS/Presentation`
- `scripts/Game/RSS/MudSlip`
- `scripts/Game/Integration`

## 兼容面清单（禁破坏）
- RPC 语义：`RPC_SendFullConfigOwner`、`RPC_ClientReportStamina`、`RPC_ServerSyncSpeedMultiplier`、心跳链路。
- 配置路径：`$profile:RealisticStaminaSystem.json`。
- 对外 API：`SCR_RSS_API` 以及 `SCR_CharacterControllerComponent` 中 `GetRss*` 能力。
- Prefab 绑定脚本类：如 `SCR_ConsumableCombatStimInjector`。

## 验收检查
- 无重复类定义。
- 关键入口可编译。
- 配置可加载、可迁移。
- 单机和联机核心链路可运行。
