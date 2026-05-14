# C 脚本文件大小硬限制

## 规则

> **所有 `.c` 脚本文件不得超过 65535 字节（64 KB）。超出后在 Arma Reforger 工作台编译或游戏运行时有概率直接崩溃，无报错信息。**

此为 EnforceScript 编译器/运行时的已知硬限制，非游戏设计约束，无法通过配置或命令行绕过。

---

## 当前违规文件（2026-05-08）

| 文件 | 大小 | 超出/余量 |
|------|------|-----------|
| `scripts/Game/RSS/Core/SCR_RealisticStaminaSystem.c` | 73.1 KB | ❌ **超限 +8.1 KB** |
| `scripts/Game/Integration/PlayerBase.c` | 64.0 KB | ⚠️ **仅余 43 字节** |
| `scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c` | 62.9 KB | ⚠️ 余 1.1 KB |
| `scripts/Game/RSS/Core/SCR_StaminaConstants.c` | 59.7 KB | 余 4.2 KB |
| `scripts/Game/RSS/Environment/SCR_EnvironmentFactor.c` | 58.0 KB | 余 6.0 KB |

> 检查命令（PowerShell）：
> ```powershell
> Get-ChildItem -Path scripts -Recurse -Filter '*.c' | ForEach-Object {
>     if ($_.Length -gt 60000) {
>         Write-Host "$($_.Length) bytes  $($_.FullName)" -ForegroundColor $(if ($_.Length -gt 65535) {'Red'} else {'Yellow'})
>     }
> }
> ```

---

## 拆分计划（2026-05-08）

### 优先级 1：SCR_RealisticStaminaSystem.c（73.1 KB → 目标 ≤55 KB）

**当前结构**：1 个类 `RealisticStaminaSpeedSystem`，22 个 static 方法，1329 行。

| 提取批次 | 目标文件 | 提取的方法 | 行数 | 预估节省 |
|----------|----------|-----------|------|---------|
| **批次 A**（首选） | `RSS/Core/SCR_SwimmingStaminaModel.c` | `CalculateSwimmingStaminaDrain()` (L1145–1231) + `CalculateSwimmingStaminaDrain3D()` (L1233–1310) | ~167 行 | **~9 KB** → 64 KB |
| **批次 B**（若 A 后仍紧张） | `RSS/Core/SCR_TerrainDensityMapper.c` | `GetTerrainFactorFromDensity()` (L980–1075) | ~95 行 | ~5 KB → ~59 KB |
| **批次 C**（可选） | `RSS/Core/SCR_RecoveryRateModel.c` | `CalculateMultiDimensionalRecoveryRate()` (L395–549) | ~154 行 | ~9 KB（注意：此函数被 `SCR_StaminaRecovery.c` 内部调用，需确认引用链） |

**推荐路线**：先执行批次 A（游泳提取），这是最内聚的子系统——仅被 `StaminaUpdateCoordinator.CalculateBaseDrainRate()` 调用，无文件内交叉引用。如果抽取后文件仍 ≥60 KB，再追加批次 B。

**批次 A 详细操作**：
1. 新建 `scripts/Game/RSS/Core/SCR_SwimmingStaminaModel.c`
2. 声明 `class SCR_SwimmingStaminaModel { ... }`，将两个游泳方法移入，`static` 保持不变
3. 原文件中调用 `SCR_SwimmingStaminaModel.CalculateSwimmingStaminaDrain3D(...)` 替换直接调用
4. 常量引用（`StaminaConstants.SWIMMING_*`）在新文件中直接使用，无需桥接

---

### 优先级 2：PlayerBase.c（62.4 KB → 目标 ≤50 KB）

**当前结构**：1 个 modded 类 `SCR_CharacterControllerComponent`，41 个方法，1609 行。

| 提取批次 | 目标文件 | 提取的方法 | 行数 | 预估节省 |
|----------|----------|-----------|------|---------|
| **批次 A**（首选） | `RSS/NetworkConfig/SCR_PlayerBaseCombatStim.c` | CSB 全量：`RSS_GetCombatStimPhase()` (L1011)、`RSS_IsCaffeineSodiumBenzoateActive()` (L1016)、`RSS_IsCombatStimOverdosed()` (L1021)、`RSS_CombatStim_ComputeBleedingBaseRateForEffect()` (L1027)、`RSS_CombatStim_RefreshBleedingEffectsToMatchScale()` (L1046)、`RSS_CombatStim_UpdateBleedingScale()` (L1065)、`RSS_CombatStim_AdjustStaminaRead()` (L1146)、`RSS_CombatStim_OnTickTransitions()` (L1090–1150)、`RSS_CombatStim_OnInjectServer()` (L1152–1215)、`RPC_CombatStimSyncToOwner()` (L1223–1235) | ~250 行 | **~14 KB** → 48 KB |
| **批次 B**（可选） | `RSS/MudSlip/SCR_PlayerBaseMudSlipDelegates.c` | 泥泞滑倒代理：`RSS_TriggerMudSlipRagdoll()` (L1232)、`RSS_MudSlip_FinishRagdoll()` (L1240)、`RSS_SetMudSlipCameraShake01()` (L1247)、`RSS_GetMudSlipCameraShake01()` (L1252)、`RSS_IsRagdollActiveForCamera()` (L1257)、`RSS_IsAiMudSlipBlockedBySafety()` (L1262)、`RSS_ShouldAiAllowMudSlipRagdoll()` (L1267)、`RSS_GetLastAppliedSpeedMultiplier()` (L1272) | ~45 行 | ~2.5 KB |
| **批次 C**（远期） | `RSS/NetworkConfig/SCR_PlayerBaseNetworkSync.c` | `RPC_ClientReportStamina()` (L1289–1407) + `RPC_ServerSyncSpeedMultiplier()` (L1410–1432) | ~142 行 | ~8 KB |

**推荐路线**：执行批次 A（CSB 提取）。CSB 状态机是 PlayerBase 中最独立、最大的逻辑块——有独立常量类 (`SCR_CombatStimConstants`)、独立状态机类 (`SCR_CombatStimStateMachine`)，PlayerBase 中的方法只是"桥接 + RPC 壳"。提取后的目标文件放在 `RSS/NetworkConfig/`（因为它与 RPC 同步紧密相关），作为 helper 类（非 modded），由 PlayerBase 委托调用。

---

### 优先级 3：SCR_RSS_Settings.c（62.9 KB → 目标 ≤45 KB）

**当前结构**：2 个类——`SCR_RSS_Params`（L9–356，~347 行）和 `SCR_RSS_Settings`（L357–1143，~786 行）。

| 提取批次 | 目标文件 | 提取内容 | 行数 | 预估节省 |
|----------|----------|---------|------|---------|
| **批次 A**（首选） | `RSS/NetworkConfig/SCR_RSS_Params.c` | 完整 `class SCR_RSS_Params`（L9–356）：所有 47 个参数字段 + `InitEliteStandardDefaults()`、`InitStandardMilsimDefaults()`、`InitTacticalActionDefaults()`、`PARAMS_ARRAY_SIZE` | ~347 行 | **~19 KB** → 44 KB |

**批次 A 详细操作**：
1. 新建 `scripts/Game/RSS/NetworkConfig/SCR_RSS_Params.c`
2. 将 `class SCR_RSS_Params { ... }` 完整移入
3. `SCR_RSS_Settings.c` 中保留对 `SCR_RSS_Params` 的引用（`m_EliteStandard`、`m_StandardMilsim`、`m_TacticalAction`、`m_Custom` 四个成员）
4. `WriteParamsToArray` / `ApplyParamsFromArray` 留在 `SCR_RSS_Settings` 中（它们操作的是 `SCR_RSS_Params` 实例，属于 Settings 的序列化职责）

这是最安全的拆分——两个类本来就职责分明（Params = 数据模型，Settings = 配置管理 + 序列化）。

---

### 优先级 4：SCR_EnvironmentFactor.c（58.0 KB → 目标 ≤45 KB）

`SCR_EnvironmentFactor` 过大时的拆分顺序建议如下：

| 提取批次 | 目标文件 | 提取内容 |
|----------|----------|---------|
| **批次 A** | `RSS/Environment/SCR_EnvironmentAstronomyMath.c` | 天文/太阳几何/辐射数学 |
| **批次 B** | `RSS/Environment/SCR_EnvironmentWeatherApi.c` | 天气 API 读取/风阻计算 |
| **批次 C** | `RSS/Environment/SCR_EnvironmentPenaltyMath.c` | 惩罚项计算（热/冷/泥泞/地表/雨呼吸） |

---

## 执行顺序总览

```
第 1 步: SCR_RealisticStaminaSystem.c → 提取游泳模型        (73 KB → ~64 KB)  ← 立即执行，已超限
第 2 步: PlayerBase.c → 提取 CSB 状态机                      (62 KB → ~48 KB)  ← 紧随其后
第 3 步: SCR_RSS_Settings.c → 提取 SCR_RSS_Params            (63 KB → ~44 KB)  ← 安全余量修复
第 4 步: SCR_EnvironmentFactor.c → 按基线分三批               (58 KB → ~45 KB)  ← 预防性拆分
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
