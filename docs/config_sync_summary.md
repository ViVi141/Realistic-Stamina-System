# 配置文件生成与同步逻辑总结

## 概览
本节总结 Realistic Stamina System 在 C 端、JSON 文件、服务器与客户端之间的配置读写与同步流程，并强调客户端写入被禁用后的行为变化。

## 参与者与数据载体
- C 端配置对象: `SCR_RSS_Settings`
- 配置管理器: `SCR_RSS_ConfigManager`
- JSON 文件路径: `$profile:RealisticStaminaSystem.json`

## 核心流程

### 1. 启动/加载
- 系统启动时调用 `SCR_RSS_ConfigManager.Load()`。
- 服务器读取 `$profile` 目录下的 JSON 配置，解析为 `SCR_RSS_Settings`。
- 若文件不存在则创建默认配置（内存），并在服务器端写入 JSON。
- 客户端跳过本地 JSON 读取，直接生成内存默认配置等待服务器同步。

### 2. 服务器与客户端职责
- 服务器: 负责持久化 JSON 文件，并作为权威配置源。
- 客户端: 不生成任何硬盘 JSON，仅保留内存配置。

### 3. 预设与迁移
- 读取配置后进行版本检测与迁移。
- 非 Custom 预设可触发预设值覆盖（仅服务器写入 JSON）。
- Custom 预设保留用户值，仅补全缺失字段。

### 4. 同步与通知
- 服务器保存配置后通过 `NotifyConfigChanges()` 通知监听器。
- 客户端在收到变更后更新内存配置并切换到服务器 JSON 内容。
- 客户端不进行落盘写入，避免覆盖服务器 JSON。

## 当前保护策略（客户端写入禁用）

### 写入控制
- 新增 `CanWriteConfig()`：仅服务器（或 Workbench）允许写入 JSON。
- `Save()` 在客户端直接返回，仅更新缓存。

### 影响
- 客户端不会生成或读取本地 JSON，所有客户端配置都来自内存默认值和服务器同步。
- 服务器仍可正常写入并同步配置到客户端。

## 关键行为总结

- C 端生成/维护配置对象，读写 JSON 仅在服务器端发生。
- 服务器是唯一的持久化来源，客户端先用内存默认值，收到同步后切换为服务器配置。
- 客户端不再有本地 JSON 参与配置流程，防止错判与覆盖。

## 相关文件
- `scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c`
- `scripts/Game/Components/Stamina/SCR_RSS_Settings.c`
