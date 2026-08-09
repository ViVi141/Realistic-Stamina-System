# C 脚本文件大小硬限制

## 规则

> **所有 `.c` 脚本文件不得超过 65535 字节（64 KB）。超出后在 Arma Reforger 工作台编译或游戏运行时有概率直接崩溃，无报错信息。**

此为 EnforceScript 编译器/运行时的已知硬限制，非游戏设计约束，无法通过配置或命令行绕过。

---

## 当前高风险文件（2026-08-09 实测）

> 在仓库根目录执行下方 PowerShell 可复现字节数。65535 字节为 EnforceScript 硬上限。

| 文件 | 大小 | 相对 64 KB |
|------|------|--------------|
| `scripts/Game/Integration/PlayerBase.c` | ~78385 字节（约 76.5 KB） | **超出约 12.5 KB** |
| `scripts/Game/RSS/Core/SCR_RSS_Constants.c` | ~52570 字节（约 51.3 KB） | 余量约 12.7 KB |
| `scripts/Game/Integration/PlayerBase_UpdateLoop.c` | ~49135 字节（约 48.0 KB） | 余量约 16 KB |
| `scripts/Game/RSS/Core/SCR_RSS_UpdateCoordinator.c` | ~44666 字节（约 43.6 KB） | 余量约 20 KB |
| `scripts/Game/RSS/Environment/SCR_RSS_EnvironmentFactor.c` | ~43639 字节（约 42.6 KB） | 余量约 21 KB |

> 检查命令（PowerShell，仓库根目录）：
> ```powershell
> Get-ChildItem -Path scripts -Recurse -Filter '*.c' | ForEach-Object {
>     $color = 'Yellow'
>     if ($_.Length -gt 65535) { $color = 'Red' }
>     if ($_.Length -gt 60000) {
>         Write-Host "$($_.Length) bytes  $($_.FullName)" -ForegroundColor $color
>     }
> }
> ```
>
> 或：`python tools/check_script_size.py`

---

## 拆分计划（与当前仓库对齐）

### 优先级 1：`PlayerBase.c`（唯一仍超 64 KB 上限）

`modded class SCR_CharacterControllerComponent` 入口仍约 **76 KB**。应继续将泥泞代理、RPC、CSB 等大块迁往 `RSS/` 下独立 helper（见已有 `SCR_RSS_MudSlipRunner.c` 等拆分方向）。同 modded 类仅允许 `PlayerBase.c` + `PlayerBase_UpdateLoop.c` 两文件扩展。

### 优先级 2：`SCR_RSS_Constants.c` / `PlayerBase_UpdateLoop.c`

接近或超过 45–50 KB 时，新增常量前宜拆领域常量文件；UpdateLoop 继续外移纯编排到 `SCR_RSS_UpdateCoordinator` 等。

### 优先级 3：`SCR_RSS_EnvironmentFactor.c`

环境栈已拆为 `SCR_RSS_*` 卫星文件；新增逻辑前宜继续按领域摊到 WeatherApi / PenaltyMath / EnvConstants。

### 优先级 4：`SCR_RSS_Settings.c` / Config

若膨胀，可将 `SCR_RSS_Params` 独立为 `SCR_RSS_Params.c`（职责：Params = 数据模型，Settings = 配置管理 + 序列化）。

---

### 附：历史拆分备忘

| 方向 | 说明 |
|------|------|
| 单体 `SCR_RealisticStaminaSystem.c` | **已移除**；职责分散到 `SCR_RSS_*` Core 模块 |
| 游泳模型 | `SCR_SwimmingStaminaModel.c`（或等价 RSS Core 文件） |
| Params 独立 | 仍可选 |
| PlayerBase 减负 | 泥泞 / RPC / 表现外移（见优先级 1） |

#### SCR_RSS_Params 独立（操作步骤备忘）

1. 新建 `scripts/Game/RSS/NetworkConfig/SCR_RSS_Params.c`
2. 将 `class SCR_RSS_Params { ... }` 完整移入
3. `SCR_RSS_Settings.c` 中保留对 `SCR_RSS_Params` 的引用
4. `WriteParamsToArray` / `ApplyParamsFromArray` 留在 `SCR_RSS_Settings`

---

## 执行顺序总览

```
第 1 步: PlayerBase.c → 继续外移泥泞 / RPC / 表现（仍超 64 KB）
第 2 步: PlayerBase_UpdateLoop / Constants → 控增长
第 3 步: EnvironmentFactor → 维持卫星文件分担
第 4 步: Settings → 视需要再拆 Params
```

提取原则：

1. **每次只提取一个领域**，确保单次提交可编译、可回归。
2. **被提取的函数保持 `static` 访问级别不变**。
3. **禁止为凑字数拆分**：文件职责内聚且远低于上限时不要拆。
4. **拆分后须能通过编译**。

---

## 预提交检查

每次提交前执行：

```powershell
Get-ChildItem -Path scripts -Recurse -Filter '*.c' |
    Where-Object { $_.Length -gt 60000 } |
    Sort-Object Length -Descending |
    Format-Table Length, Name

$violations = Get-ChildItem -Path scripts -Recurse -Filter '*.c' |
    Where-Object { $_.Length -gt 65535 }
if ($violations) {
    Write-Host "BLOCKED: Files exceed 65535 byte limit:" -ForegroundColor Red
    $violations | Format-Table Length, FullName
    exit 1
}
```

或 `python tools/check_script_size.py`。

---

## 同步更新

本规则与以下文档联动：

- [scripts_naming_and_layout_rules.md](scripts_naming_and_layout_rules.md)
- [RSS_CODING_STANDARDS.md](RSS_CODING_STANDARDS.md)

任何拆分行为建议在 CHANGELOG 中记录原文件大小 → 拆分后各文件大小。
