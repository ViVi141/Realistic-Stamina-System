# Arma Reforger - 拟真体力-速度系统

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)

一个结合体力和负重动态调整移动速度的拟真模组，基于精确的医学/生理学模型。

## 作者信息

- **作者**: ViVi141
- **邮箱**: 747384120@qq.com
- **许可证**: [AGPL-3.0](LICENSE)

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE) 许可证。

Copyright (C) 2024 ViVi141

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

## 功能说明

本模组根据玩家的体力值和负重动态调整移动速度，实现更真实的游戏体验。当体力充沛时，玩家可以全速移动；当体力下降时，移动速度会逐渐减慢。同时，负重也会影响移动速度。

### 主要特性

- ✅ **动态速度调整**：根据体力百分比动态调整移动速度
- ✅ **负重影响系统**：负重越高，移动速度越慢
- ✅ **分段式速度控制**：5个体力区间对应不同的速度倍数
- ✅ **实时状态显示**：每秒显示速度、体力百分比和速度倍率
- ✅ **平滑速度过渡**：体力变化时速度平滑过渡，避免突兀变化

## 项目结构

```
test_rubn_speed/
├── LICENSE                               # AGPL-3.0 许可证
├── README.md                             # 项目说明文档
├── AUTHORS.md                            # 作者信息
├── CONTRIBUTING.md                       # 贡献指南
├── CHANGELOG.md                          # 更新日志
├── .gitignore                            # Git 忽略文件配置
├── addon.gproj                           # Arma Reforger 项目配置文件
├── scripts/                              # EnforceScript 脚本目录
│   └── Game/
│       ├── PlayerBase.c                  # 主控制器组件（速度更新和状态显示）
│       └── Components/
│           └── Stamina/
│               ├── SCR_RealisticStaminaSystem.c  # 体力-速度系统核心（数学计算）
│               └── SCR_StaminaOverride.c          # 体力系统覆盖（拦截原生系统）
├── Prefabs/                              # 预制体文件
│   └── Characters/
│       └── Core/
│           └── Character_Base.et         # 角色基础预制体
└── tools/                                # 开发工具和脚本
    ├── simulate_stamina_system.py        # 基础趋势图生成脚本
    ├── generate_comprehensive_trends.py  # 综合趋势图生成脚本
    ├── optimize_2miles_params.py        # 2英里参数优化脚本
    ├── test_2miles.py                    # 2英里测试脚本
    ├── calculate_recovery_time.py        # 恢复时间计算脚本
    └── *.png                             # 生成的趋势图
```

## 技术实现

### 实现方式

- 使用 `modded class SCR_CharacterStaminaComponent` 扩展体力组件
- 使用 `modded class SCR_CharacterControllerComponent` 显示状态信息
- 通过 `OverrideMaxSpeed(fraction)` 动态设置最大速度倍数
- 每 0.2 秒更新一次速度，确保实时响应体力变化
- **使用精确的数学模型**：所有计算都基于精确的数学函数，不使用近似

### 速度计算逻辑

#### 1. 体力-速度关系模型（精确数学模型）

基于耐力下降模型（Fatigue-based speed model）：
**S(E) = S_max × E^α**

其中：
- S = 当前速度倍数
- E = 体力百分比 (0-1)
- α = 0.6（体力影响指数，基于医学文献）
- S_max = 0.885（目标最大速度倍数）

**精确计算结果**（无负重，优化后参数）：
- 100% 体力：92.0% 速度（4.78 m/s）
- 50% 体力：60.7% 速度（3.16 m/s）
- 25% 体力：40.0% 速度（2.08 m/s）
- 10% 体力：23.1% 速度（1.20 m/s）
- 0% 体力：15.0% 速度（0.78 m/s，最小速度限制）

**技术实现**：使用精确的幂函数计算（牛顿法计算5次方根），不使用平方根近似

#### 2. 负重影响系统（精确非线性模型）

基于 US Army 背包负重实验数据（Knapik et al., 1996）：

**精确数学模型**：
**速度惩罚 = β × (负重百分比)^γ**

其中：
- β = 0.40（负重速度惩罚系数）
- γ = 1.0（负重影响指数，可调整为 1.0-1.2 以模拟非线性）
- 负重百分比 = 当前重量 / 40.5 kg

**配置参数**：
- **最大负重**：40.5 kg（角色可以携带的最大重量）
- **战斗负重**：30.0 kg（战斗状态下的推荐负重阈值）
- **最大速度惩罚**：40%（当负重达到最大负重时）
- **战斗负重状态**：当负重超过 30 kg 时，可能影响战斗表现

**技术实现**：使用精确的幂函数计算，不使用线性近似

#### 3. 综合速度计算（精确模型）

```
最终速度倍数 = 基础速度倍数（根据体力，精确计算） × (1 - 负重惩罚（精确计算）)
最终速度限制在 20%-100% 之间
```

**计算流程**（优化后参数）：
1. 计算基础速度倍数：`S_base = 0.920 × E^0.6`（E 为体力百分比）
2. 计算负重惩罚：`P_enc = 0.40 × (W/40.5)^1.0`（W 为当前重量）
3. 计算最终速度：`S_final = S_base × (1 - P_enc)`
4. 应用限制：`S_final = clamp(S_final, 0.20, 1.0)`

**优化目标**：2英里（3218.7米）在15分27秒（927秒）内完成  
**优化结果**：完成时间 925.8秒（15.43分钟），提前1.2秒完成 ✅

### 关键参数

**体力系统参数：**
- `STAMINA_THRESHOLD_HIGH = 0.75`：高体力阈值（75%）
- `STAMINA_THRESHOLD_MED = 0.50`：中等体力阈值（50%）
- `STAMINA_THRESHOLD_LOW = 0.25`：低体力阈值（25%）

**速度倍数：**
- `SPEED_MULTIPLIER_FULL = 1.0`：100%体力时 100%速度
- `SPEED_MULTIPLIER_HIGH = 0.95`：高体力时 95%速度
- `SPEED_MULTIPLIER_MED = 0.85`：中等体力时 85%速度
- `SPEED_MULTIPLIER_LOW = 0.70`：低体力时 70%速度
- `SPEED_MULTIPLIER_EXHAUSTED = 0.55`：精疲力尽时 55%速度

**更新频率：**
- 速度更新间隔：200 毫秒（0.2 秒）
- 状态显示间隔：1000 毫秒（1 秒）

## 系统特性

### 体力-速度关系

系统会根据体力百分比动态调整速度，实现以下效果：

1. **体力充沛时（75-100%）**：
   - 速度倍数：95-100%
   - 几乎不影响移动速度
   - 适合长时间奔跑

2. **体力中等时（50-75%）**：
   - 速度倍数：85-95%
   - 轻微影响移动速度
   - 开始感到疲劳

3. **体力偏低时（25-50%）**：
   - 速度倍数：70-85%
   - 明显影响移动速度
   - 需要休息恢复体力

4. **精疲力尽时（0-25%）**：
   - 速度倍数：55-70%
   - 严重影响移动速度
   - 强烈建议休息

### 负重-速度关系

负重系统会根据携带物品的重量影响移动速度：

- **无负重（0 kg）**：无影响
- **轻度负重（0-15 kg）**：轻微影响，速度减少 < 15%
- **中度负重（15-30 kg）**：明显影响，速度减少 15-30%
- **战斗负重阈值（30 kg）**：达到战斗负重阈值，可能影响战斗表现
- **重度负重（30-40.5 kg）**：严重影响，速度减少 30-40%
- **最大负重（40.5 kg）**：达到最大负重，速度最多减少 40%

### 状态显示

系统每秒输出一次状态信息，格式如下：

```
[RealisticSystem] 移动速度: 4.2 m/s | 体力: 65% | 速度倍率: 88%
```

显示内容：
- **移动速度**：上一秒的水平移动速度（米/秒）
- **体力**：当前体力百分比（0-100%）
- **速度倍率**：当前速度倍数（相对于标准速度的百分比）

## 安装方法

1. 将整个 `test_rubn_speed` 文件夹复制到 Arma Reforger 工作台的 `addons` 目录
2. 在 Arma Reforger 工作台中打开项目
3. 编译模组
4. 在游戏中选择并启用此模组

## 使用方法

1. 启动游戏并加载模组
2. 系统会自动监控体力值和负重
3. 移动速度会根据体力百分比和负重自动调整
4. 状态信息会每秒一次输出到控制台，格式如下：
   ```
   [RealisticSystem] 移动速度: 4.2 m/s | 体力: 65% | 速度倍率: 88%
   ```

## 调整系统参数

### 修改体力分段速度

编辑 `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c` 文件：

```c
// 速度倍数（根据体力百分比）
protected const float SPEED_MULTIPLIER_FULL = 1.0;      // 100%体力：100%速度
protected const float SPEED_MULTIPLIER_HIGH = 0.95;     // 75-100%体力：95%速度
protected const float SPEED_MULTIPLIER_MED = 0.85;      // 50-75%体力：85%速度
protected const float SPEED_MULTIPLIER_LOW = 0.70;      // 25-50%体力：70%速度
protected const float SPEED_MULTIPLIER_EXHAUSTED = 0.55; // 0-25%体力：55%速度
```

### 修改负重影响

编辑 `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c` 文件：

```c
protected const float ENCUMBRANCE_SPEED_INFLUENCE = 0.3; // 负重对速度的影响系数（最多30%）
```

### 修改更新频率

编辑 `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c` 文件：

```c
// 启动定期更新（每0.2秒更新一次速度）
GetGame().GetCallqueue().CallLater(UpdateSpeedBasedOnStamina, 200, false);
```

修改 `200` 为其他值（单位：毫秒）可以调整更新频率。

## 系统优势

### 拟真体验

- **动态响应**：速度随体力实时变化，更真实
- **平滑过渡**：体力变化时速度平滑过渡，避免突兀
- **综合影响**：同时考虑体力和负重，更全面

### 游戏平衡

- **战略考虑**：需要管理体力和负重
- **资源管理**：携带物品需要权衡重量和速度
- **战术选择**：体力管理影响战术决策

## 已知问题

- 状态显示目前仅输出到控制台，尚未实现游戏内 UI 显示
- 速度倍数值大于 1.0 时会被引擎限制为 1.0（无法超过标准速度）
- 需要测试 `GetStamina()` 和 `GetMaxStamina()` 方法是否可用（如果不可用，可能需要通过其他方式获取体力值）

## 开发说明

### 编译要求

- Arma Reforger 工作台
- EnforceScript 编译器

### 代码结构

**`scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c`** - 体力-速度系统核心：
- `Pow()`: 精确计算幂函数（使用牛顿法，不使用近似）
- `CalculateSpeedMultiplierByStamina()`: 根据体力百分比精确计算速度倍数（S = 0.920 × E^0.6）
- `CalculateEncumbranceSpeedPenalty()`: 精确计算负重对速度的影响（P = 0.40 × (W/40.5)^1.0）
- `CalculateEncumbrancePercent()`: 计算负重百分比
- `CalculateCombatEncumbrancePercent()`: 计算战斗负重百分比
- `IsOverCombatEncumbrance()`: 检查是否超过战斗负重阈值

**`scripts/Game/Components/Stamina/SCR_StaminaOverride.c`** - 体力系统覆盖：
- `OnStaminaDrain()`: 覆盖体力变化事件，拦截原生系统修改
- `SetTargetStamina()`: 设置目标体力值（唯一允许的修改方式）
- `MonitorStamina()`: 主动监控机制（每50ms检查一次）
- `CorrectStaminaToTarget()`: 纠正体力值到目标值

**`scripts/Game/PlayerBase.c`** - 主控制器组件：
- `OnInit()`: 初始化控制器组件，获取体力组件引用
- `UpdateSpeedBasedOnStamina()`: 根据体力和负重精确更新速度（每0.2秒）
- `CollectSpeedSample()`: 每秒采集一次速度样本
- `DisplayStatusInfo()`: 显示速度、体力、速度倍率信息

**精确体力消耗模型**（基于 Pandolf 模型，优化后参数 v2.1）：
- 基础消耗项：`a = 0.00004`
- 速度线性项：`b·V = 0.0001·V`
- 速度平方项：`c·V² = 0.0001·V²`（优化后降低）
- 负重基础项：`d·M_enc = 0.001·(M_mult - 1)`
- 负重速度交互项：`e·M_enc·V² = 0.0002·(M_mult - 1)·V²`
- 总消耗：`total = a + b·V + c·V² + d·M_enc + e·M_enc·V²`

## 版本历史

- **v2.1** - 参数优化版本
  - 优化参数以达到2英里15分27秒目标
  - 速度倍数从0.885提升至0.920（满体力速度4.78 m/s）
  - 速度平方项系数从0.0003降低至0.0001
  - 完成时间：925.8秒（15.43分钟），提前1.2秒完成目标 ✅
  - 使用精确数学模型（α=0.6，Pandolf模型）

- **v2.0** - 拟真体力-速度系统
  - 实现动态速度调整系统（根据体力百分比）
  - 实现负重影响系统（负重影响速度）
  - 实现精确数学模型（不使用近似）
  - 实现综合状态显示（速度、体力、倍率）
  - 实现平滑速度过渡机制

- **v1.0** - 初始版本（已废弃）
  - 实现固定速度修改功能（50%）
  - 实现速度显示功能（每秒一次）

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

本项目采用 [GNU Affero General Public License v3.0](LICENSE) 许可证。

Copyright (C) 2024 ViVi141

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

## 作者

- **ViVi141**
- Email: 747384120@qq.com

---

**注意：** 本模组修改了游戏的核心速度机制，可能会影响游戏体验。建议在测试环境中使用。
