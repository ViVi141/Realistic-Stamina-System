# C 脚本文件大小硬限制

## 规则

> **所有 `.c` 脚本文件不得超过 65535 字节（64 KB）。超出后在 Arma Reforger 工作台编译或游戏运行时有概率直接崩溃，无报错信息。**

此为 EnforceScript 编译器/运行时的已知硬限制，非游戏设计约束，无法通过配置或命令行绕过。

---

## 当前高风险文件（2026-05-15 实测）

> 在仓库根目录执行下方 PowerShell 可复现字节数。65535 字节为 EnforceScript 硬上限。

| 文件 | 大小 | 相对 64 KB |
|------|------|--------------|
| `scripts/Game/Integration/PlayerBase.c` | 78891 字节（约 77.0 KB） | **超出约 12.9 KB** |
| `scripts/Game/RSS/Environment/SCR_EnvironmentFactor.c` | 64620 字节（约 63.1 KB） | 余量约 0.9 KB |
| `scripts/Game/RSS/Core/SCR_StaminaConstants.c` | 62883 字节（约 61.4 KB） | 余量约 2.5 KB |
| `scripts/Game/RSS/Core/SCR_RealisticStaminaSystem.c` | 61758 字节（约 60.3 KB） | 余量约 3.6 KB |
| `scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c` | 41210 字节（约 40.2 KB） | 余量充足 |

> 检查命令（PowerShell，仓库根目录）：
> ```powershell
> Get-ChildItem -Path scripts -Recurse -Filter '*.c' | ForEach-Object {
>     if ($_.Length -gt 60000) {
>         Write-Host "$($_.Length) bytes  $($_.FullName)" -ForegroundColor $(if ($_.Length -gt 65535) {'Red'} else {'Yellow'})
>     }
> }
> ```

---

## 拆分计划（与当前仓库对齐，2026-05-15）

### 优先级 1：`PlayerBase.c`（唯一仍超 64 KB 上限）

`modded class SCR_CharacterControllerComponent` 仍约 **1916 行 / 77 KB**。应继续将 CSB、泥泞代理、RPC 大块迁往 `RSS/` 下独立 helper（见仓库内 `SCR_CombatStim*`、`SCR_RSS_MudSlipRunner.c` 等已有拆分方向）。

### 优先级 2：`SCR_EnvironmentFactor.c`（接近上限）

约 **63 KB**，新增逻辑前宜按领域拆到 `SCR_EnvironmentWeatherApi.c` / `SCR_EnvironmentPenaltyMath.c` 等已有卫星文件（见本文件后续「SCR_EnvironmentFactor」小节）。

### 优先级 3：`SCR_RealisticStaminaSystem.c`

约 **60 KB / 约 932 行**；游泳消耗已迁至 `scripts/Game/RSS/Core/SCR_SwimmingStaminaModel.c`。若再次逼近上限，可再抽 **地形密度→系数** 等纯函数块。

### 优先级 4：`SCR_RSS_Settings.c`

约 **40 KB**；若未来膨胀，可维持原方案将 `SCR_RSS_Params` 独立为 `SCR_RSS_Params.c`（见下表历史备忘）。

---

### 附：历史拆分备忘（行号/行数仅供回忆，以当前文件为准）

| 方向 | 说明 |
|------|------|
| 游泳模型 | 已实现：`SCR_SwimmingStaminaModel.c` |
| Params 独立 | 仍可选：`SCR_RSS_Params` → `SCR_RSS_Params.c` |
| PlayerBase 减负 | CSB / 泥泞 / RPC 外移（见上优先级 1） |

#### SCR_RSS_Params 独立（操作步骤备忘）

1. 新建 `scripts/Game/RSS/NetworkConfig/SCR_RSS_Params.c`
2. 将 `class SCR_RSS_Params { ... }` 完整移入
3. `SCR_RSS_Settings.c` 中保留对 `SCR_RSS_Params` 的引用（`m_EliteStandard`、`m_StandardMilsim`、`m_TacticalAction`、`m_Custom` 四个成员）
4. `WriteParamsToArray` / `ApplyParamsFromArray` 留在 `SCR_RSS_Settings` 中（它们操作的是 `SCR_RSS_Params` 实例，属于 Settings 的序列化职责）

这是最安全的拆分——两个类本来就职责分明（Params = 数据模型，Settings = 配置管理 + 序列化）。

---

### `SCR_EnvironmentFactor.c` 领域拆分（预防性）

以下文件在仓库中已存在，用于把环境计算从单体中摊薄：

| 提取批次 | 目标文件 | 提取内容 |
|----------|----------|---------|
| **批次 A** | `RSS/Environment/SCR_EnvironmentAstronomyMath.c` | 天文/太阳几何/辐射数学 |
| **批次 B** | `RSS/Environment/SCR_EnvironmentWeatherApi.c` | 天气 API 读取/风阻计算 |
| **批次 C** | `RSS/Environment/SCR_EnvironmentPenaltyMath.c` | 惩罚项计算（热/冷/泥泞/地表/雨呼吸） |

---

## 执行顺序总览

```
第 1 步: PlayerBase.c → 继续外移 CSB / 泥泞 / RPC（仍超 64 KB）
第 2 步: SCR_EnvironmentFactor.c → 按领域维持卫星文件分担（接近 64 KB）
第 3 步: SCR_RealisticStaminaSystem.c → 视增长再抽地形/恢复纯函数块（游泳已拆至 SCR_SwimmingStaminaModel.c）
第 4 步: SCR_RSS_Settings.c → 视需要再拆 SCR_RSS_Params.c
```

每一步完成后验证：编译通过 + 单机冲刺/恢复 + 配置同步。

提取原则：

1. **每次只提取一个领域**（如：仅提取游泳模型，不分拆多个模块），确保单次提交可编译、可回归。
2. **被提取的函数保持 `static` 访问级别不变**，由原文件通过 `#include` 或 EnforceScript 的跨文件引用机制调用。
3. **禁止为"凑字数"拆分**：如果一个文件只有 30 KB 且职责内聚，不要拆。
4. **拆分后原文件的 `#include` 或前向声明必须能通过编译**。

---

## 预提交检查

每次提交前执行（加入到 CI / 手动检查）：

```powershell
# 列出所有超过 60000 字节的 .c 文件
Get-ChildItem -Path scripts -Recurse -Filter '*.c' | 
    Where-Object { $_.Length -gt 60000 } | 
    Sort-Object Length -Descending | 
    Format-Table Length, Name

# 列出所有超过 65535 字节的 .c 文件（硬阻断）
$violations = Get-ChildItem -Path scripts -Recurse -Filter '*.c' | 
    Where-Object { $_.Length -gt 65535 }
if ($violations) {
    Write-Host "BLOCKED: Files exceed 65535 byte limit:" -ForegroundColor Red
    $violations | Format-Table Length, FullName
    exit 1
}
```

或将以下 Python 脚本作为 Git pre-commit hook：

```python
import os, sys

LIMIT = 65535
WARN = 60000
scripts_dir = os.path.join(os.path.dirname(__file__), 'scripts')
violations = []

for root, _, files in os.walk(scripts_dir):
    for f in files:
        if f.endswith('.c'):
            path = os.path.join(root, f)
            size = os.path.getsize(path)
            if size > WARN:
                level = "BLOCK" if size > LIMIT else "WARN"
                violations.append((level, size, os.path.relpath(path, scripts_dir)))

violations.sort(key=lambda x: x[1], reverse=True)
for level, size, path in violations:
    print(f"[{level}] {size:>6} bytes  {path}")

if any(v[0] == "BLOCK" for v in violations):
    print("\n提交被阻止：存在超过 65535 字节的 .c 文件，请拆分后再提交。")
    sys.exit(1)
```

---

## 同步更新

本规则与以下文档联动：

- [scripts_naming_and_layout_rules.md](scripts_naming_and_layout_rules.md) — 命名与分层约束
- [CHANGELOG.md](../CHANGELOG.md) — 拆分记录（`refactor` 标签）

任何拆分行为必须在 CHANGELOG 中以 `### 🏗️ 重构` 条目记录，标注原文件大小 → 拆分后各文件大小。
