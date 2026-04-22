# P0-CFG-1：GameMode 配置复制链路验收清单

**目的**：可重复地验证客户端在专服场景下与权威配置一致（含热重载与晚加入）。本表为**手测记录模板**，请在游戏内执行后在「结果」列填写通过/失败与备注。

**前置**：专服已加载本 addon；服务端 `RealisticStaminaSystem.json`（或你实际使用的 profile 路径）可编辑；至少 1 名客户端连接。

---

## 1. 专服启动后首次同步（10s 内）

| 步骤 | 操作 | 预期 | 结果 |
| --- | --- | --- | --- |
| 1.1 | 专服启动任务/会话，记录服务端配置中的 `m_sConfigVersion` 与 `m_sSelectedPreset`（或自 JSON 读） | 与随后客户端一致 |  |
| 1.2 | 客户端在进服后 **10 秒内** 在能观察日志/断点处确认 `SCR_RSS_ConfigManager.IsServerConfigApplied() == true` | 为真 |  |
| 1.3 | 同一时刻读取客户端 `SCR_RSS_ConfigManager.GetSettings()` 的 `m_sConfigVersion`、`m_sSelectedPreset` | 与 1.1 一致 |  |

---

## 2. 运行中 Reload / ForceSync

| 步骤 | 操作 | 预期 | 结果 |
| --- | --- | --- | --- |
| 2.1 | 在专服上修改配置（例如改 `m_sConfigVersion` 或预设名），执行 Reload 或触发 `ForceSyncToClients` | 不崩溃、不刷屏卡死 |  |
| 2.2 | 已在线客户端在 **10 秒内** 再次对账 1.2 / 1.3 的字段与 `IsServerConfigApplied` | 与专服新值一致 |  |
| 2.3 | 若 `m_bHintDisplayEnabled` 在重载中切换，观察右上角 Hint HUD | 开则显示、关则隐藏，与 [RSS_Native_Audit_Report.md](RSS_Native_Audit_Report.md) 5.1 节描述一致 |  |

---

## 3. 晚加入客户端

| 步骤 | 操作 | 预期 | 结果 |
| --- | --- | --- | --- |
| 3.1 | 在专服已运行、配置已至少同步一轮后，新客户端加入 | 新客户端 10s 内 `IsServerConfigApplied` 为真 |  |
| 3.2 | 新客户端 `GetSettings()` 与专服当前配置对账 | 与专服一致 |  |

---

## 4. 空配置 / 降级（按产品定义）

| 步骤 | 操作 | 预期 | 结果 |
| --- | --- | --- | --- |
| 4.1 | 按你们产品定义，测试「无有效 JSON / 空文件」的专服行为 | 不软锁、客户端可预期（崩溃/回默认/打日志 等由你们定义） |  |
| 4.2 | 客户端 `RSS_WaitForGameModeConfig` 相关超时（若仍启用）不刷屏 | 与当前 `PlayerBase` 实现一致 |  |

---

## 5. 签名与版本

- **执行人**：
- **游戏/模组版本**：
- **日期**：
