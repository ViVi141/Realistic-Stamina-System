# RSS v5 编码规范（权威）

> 取代 [`scripts_naming_and_layout_rules.md`](scripts_naming_and_layout_rules.md) 中的冲突条目。  
> 版本：**5.0.0**

## 1. 命名

| 类型 | 规则 | 示例 |
|------|------|------|
| RSS 类 | `SCR_RSS_<Domain><Name>` | `SCR_RSS_DrainCalculator` |
| 枚举 | `ERSS_<Name>` | `ERSS_MovementPhase` |
| DTO | `RSS_<Name>` | `RSS_PlayerInfo` |
| 文件名 | 与主类名一致 | `SCR_RSS_DrainCalculator.c` |
| modded 入口 | 引擎类名；文件名可稳定 | `PlayerBase.c` + `PlayerBase_UpdateLoop.c`（同 modded 类仅这两文件） |
| 成员 | `m_f`/`m_i`/`m_b`/`m_p`/`m_e`/`m_s`/`m_v` | `m_fAerobicStamina` |

## 2. 格式

- 4 空格缩进；K&R 大括号
- **禁止** `?:` 三元运算符
- 单行 `if` 必须 `{}`
- 公共 static：`//!` + `@param` / `@return`（中文）

## 3. 文件大小

| 层级 | 上限 |
|------|------|
| 全 `.c` 文件 | **65535 字节**（硬崩溃） |
| Integration | ≤ 40 KB / ≤ 600 行 |
| StaminaOverride | ≤ 15 KB / ≤ 250 行（拦截壳 only） |
| RSS/Core 等 | ≤ 45 KB / ≤ 700 行 |

运行：`python tools/check_script_size.py`

## 4. Official-first + 两大例外

**默认**：优先官方 API（`SetSpeedLimit`、`RplProp`、`CallLater`、`GetTotalWeightOfAllStorages` 等）。

| 例外 | 策略 |
|------|------|
| **天气/环境** | RSS 自建为主；`TimeAndWeatherManagerEntity` 仅采样 |
| **引擎体力条** | 仅**拦截**（`OnStaminaDrain` / `ApplyDrain`）；`AddStamina` 不可 override |

## 5. 体力拦截（有氧 / 无氧）

- **有氧池** → RSS 计算 → `SetTargetStamina` → 受控 `AddStamina` → `GetStamina()` UI
- **无氧池** → `SCR_RSS_AnaerobicBurst` + Controller；**never** 写入引擎条
- 业务公式只在 `RSS/Core/`，不在 `StaminaOverride`

## 6. EnforceScript 禁用

- `?:`、`ScriptCaller`、单文件超 64 KB
- 废弃 `autoptr`（用 `ref`）
- 无 try/catch、无用户泛型类

## 7. 分层

```
Integration/     → modded 薄壳 + RPC（`PlayerBase.c` / `PlayerBase_UpdateLoop.c` 两文件扩展同一 modded 类；勿再拆 `PlayerBase_*.c`）
RSS/Core/        → 代谢、双池、速度、协调器
RSS/Environment/ → 自建环境栈
RSS/NetworkConfig/ → 配置、同步、API
RSS/AI/          → AI 体力
RSS/Presentation/→ HUD、屏效
RSS/Items/       → 注射器、UserActions
RSS/MudSlip/     → 泥泞滑倒
```

## 8. PR 检查

- [ ] `check_script_size.py` 通过
- [ ] 写明官方锚点或例外理由
- [ ] 未在 Integration 内联 Pandolf / 环境惩罚
