# RSS 编码规范（权威）

> 取代 [`scripts_naming_and_layout_rules.md`](scripts_naming_and_layout_rules.md) 中的冲突条目。  
> 版本：**6.1.x**（命名与分层继承 v5；体力表现见 §5）

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
运行：`python tools/check_enforce_syntax.py`（禁用语法 + 单行 `if`）

## 4. Official-first + 两大例外

**默认**：优先官方 API（`SetSpeedLimit`、`RplProp`、`CallLater`、`GetTotalWeightOfAllStorages` 等）。

| 例外 | 策略 |
|------|------|
| **天气/环境** | RSS 自建为主；`TimeAndWeatherManagerEntity` 仅采样 |
| **引擎体力条** | 仅**拦截**（`OnStaminaDrain` / `ApplyDrain`）；`AddStamina` 不可 override |

## 5. 体力拦截（有氧 / W′）

- **有氧池** → RSS 计算 → `SetTargetStamina` / 受控 `AddStamina`；权威在 `m_fTargetStamina`
- **W′（无氧）** → `SCR_RSS_CriticalPowerModel` / `SCR_RSS_AnaerobicBurst`；**不改**有氧权威
- **W′→引擎表现**（默认开）：`SCR_RSS_SprintGate` 经 `ApplyTransientEngineStamina` 写 transient `GetStamina()`，驱动原生晃动/`Exhaustion`；业务公式仍只在 `RSS/Core/`
- CPR 等禁用原生 `CharacterStaminaComponent` 的兼容不在本模组内；需独立 compat 模组

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
- [ ] `check_enforce_syntax.py` 通过
- [ ] 写明官方锚点或例外理由
- [ ] 未在 Integration 内联 Pandolf / 环境惩罚
