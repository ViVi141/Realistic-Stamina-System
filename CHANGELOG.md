# 更新日志
#
# 所有重要的项目变更都会记录在此文件中。
#
# 格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/),
# 并且本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。
#

## [3.13.1] - 2026-02-27

### 🔁 变更

- **坡度-速度模型改用托布勒徒步函数** - 使用 Tobler's Hiking Function (1993)：W = 6·e^(-3.5·|S+0.05|)；最大速度出现在约 -3° 到 -5° 小下坡，上坡和过陡下坡均会快速衰减（[SCR_RealisticStaminaSystem.c](scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c)）
- **坡度速度 5 秒平滑过渡** - 新增 SlopeSpeedTransition 模块，坡度变化时速度在 5 秒内平滑过渡，避免从平地冲上陡坡时瞬间从 3 m/s 骤降到 1 m/s 的"被胶水粘住"感（[SCR_SlopeSpeedTransition.c](scripts/Game/Components/Stamina/SCR_SlopeSpeedTransition.c)）

### 🐞 修复

- **室内坡度影响速度错误应用** - 修复在室内时仍错误应用坡度影响速度的问题；新增 `IsIndoorForEntity(owner)` 方法，使用传入实体进行室内检测，避免服务器处理远程玩家 RPC 时 `m_pCachedOwner` 未更新导致的室内误判；`IsIndoor`/`IsIndoorForEntity` 现会检查 `IsIndoorDetectionEnabled` 配置（[SCR_EnvironmentFactor.c](scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)、[SCR_SpeedCalculation.c](scripts/Game/Components/Stamina/SCR_SpeedCalculation.c)、[SCR_StaminaConsumption.c](scripts/Game/Components/Stamina/SCR_StaminaConsumption.c)、[SCR_StaminaUpdateCoordinator.c](scripts/Game/Components/Stamina/SCR_StaminaUpdateCoordinator.c)）

---

## [3.13.0] - 2026-02-26

### ✅ 新增

- **物理模型优化跳跃与翻越消耗** - 使用物理模型计算跳跃和翻越的体力消耗，移除旧的基准费用，使消耗更贴近真实动作
- **环境因子温度模拟与风速补偿** - 增加环境因子模块的温度模拟与风速补偿逻辑，优化体力消耗计算
- **调试能力增强** - 增加天气管理器调试信息，优化经纬度估算逻辑；增强环境因子模块的调试日志，优化位置估算日志输出
- **快速优化测试脚本** - 添加快速优化测试脚本和基础体能检查测试，便于调试与参数校准
- **35kg 行走追踪脚本** - 新增 `trace_35kg_walk.py`，用于追踪 35kg 负重行走场景下的体力消耗与恢复

### 🔁 变更

- **弃用 Givoni-Goldman 模型** - 重构体力消耗模型，Pandolf 模型现为唯一能量消耗模型；跑步与冲刺阶段均使用 Pandolf 模型
- **耐力常量与计算重构** - 重构与耐力相关的常量和计算，使用固定体能效率因子 (0.70)，避免游戏体验不平衡
- **代码结构调整** - 重构代码结构，提升可读性与可维护性
- **动态配置与恢复机制** - 重构体力系统以支持动态配置，恢复逻辑改用动态 getter 获取最低恢复阈值与休息时间，确保 JSON 配置生效
- **坡度计算优化** - 坡度角度计算直接返回角度值并做 clamp，避免引擎异常
- **能量转体力系数** - 调整能量到体力转换系数并 clamp 至合理范围，防止配置错误
- **移除废弃常量** - 移除废弃的姿态消耗静态常量，改为动态配置获取

### 🐞 修复

- **降雨湿重上限调整** - 调整降雨湿重上限以提高游戏平衡性和可玩性
- **体力消耗计算优化** - 防止过度消耗率，确保不同状态下行为一致
- **C 与 Python 常量同步** - 更新 `rss_digital_twin_fix.py` 和 `stamina_constants.py`，与 C 端常量保持一致；`rss_super_pipeline.py` 将有氧/无氧效率因子锁定为共识值

### 🔸 包含的提交（3.13.0，自 c948891 至 HEAD）

- f9dd1c9 fix: 调整降雨湿重上限以提高游戏平衡性和可玩性
- c47e28f fix: Refactor stamina system for dynamic configuration and improved recovery mechanics
- 5b8f84f Refactor stamina management and optimization parameters
- 9071445 feat: 添加快速优化测试脚本和基础体能检查测试，增强调试能力
- 54eb267 Refactor code structure for improved readability and maintainability
- 7d3b977 重构与耐力相关的常量和计算，以避免游戏体验不平衡
- e944b70 feat: 增加环境因子模块的温度模拟与风速补偿逻辑，优化体力消耗计算
- 0ed0195 feat: 增强环境因子模块的调试日志，优化位置估算日志输出
- 0f3f71c feat: 增加天气管理器调试信息，优化经纬度估算逻辑
- 85071dd 重构体力消耗模型：弃用 Givoni-Goldman 模型
- 25d87b1 feat: 使用物理模型优化跳跃和翻越消耗计算，移除旧的基准费用

---

## [3.12.0] - 2026-02-09

本版本仅记录 C 脚本层面的变化。

### ✅ 新增

- **环境温度物理模型** - 新增温度步进、短波/长波与云量修正、太阳/日出日落与月相推断，支持引擎温度或模组模型切换（[scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c](scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)）
- **室内检测增强** - 增加向上射线与水平封闭检测，降低开放屋顶/天窗误判（[scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c](scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)）
- **配置变更同步链路** - 新增监听器注册、变更检测、全量参数/设置数组同步与广播（[scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c](scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c)、[scripts/Game/Components/Stamina/SCR_RSS_Settings.c](scripts/Game/Components/Stamina/SCR_RSS_Settings.c)、[scripts/Game/PlayerBase.c](scripts/Game/PlayerBase.c)）
- **网络校验与平滑** - 客户端上报体力/负重，服务器权威校验并下发速度倍率，含重连延迟同步（[scripts/Game/PlayerBase.c](scripts/Game/PlayerBase.c)、[scripts/Game/Components/Stamina/SCR_NetworkSync.c](scripts/Game/Components/Stamina/SCR_NetworkSync.c)）
- **日志节流工具** - 统一 Debug/Verbose 日志节流接口（[scripts/Game/Components/Stamina/SCR_StaminaConstants.c](scripts/Game/Components/Stamina/SCR_StaminaConstants.c)）

### 🔁 变更

- **服务器权威配置** - 客户端不再写入 JSON，仅内存默认值等待同步；服务器写盘并增加备份/修复流程（[scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c](scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c)）
- **移动相位驱动消耗** - 优先以移动相位/冲刺状态决定 Pandolf 路径，并提供服务端权威速度倍数计算接口（[scripts/Game/Components/Stamina/SCR_StaminaUpdateCoordinator.c](scripts/Game/Components/Stamina/SCR_StaminaUpdateCoordinator.c)）。Givoni 模型已弃用。
- **负重参数约束** - 新增负重惩罚指数/上限并对预设进行 clamp（[scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c](scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c)、[scripts/Game/Components/Stamina/SCR_RSS_Settings.c](scripts/Game/Components/Stamina/SCR_RSS_Settings.c)、[scripts/Game/Components/Stamina/SCR_StaminaConstants.c](scripts/Game/Components/Stamina/SCR_StaminaConstants.c)）
- **预设参数刷新** - Elite/Standard/Tactical 预设全面更新，并补充天气模型顶层默认值（[scripts/Game/Components/Stamina/SCR_RSS_Settings.c](scripts/Game/Components/Stamina/SCR_RSS_Settings.c)）
- **冲刺消耗默认值** - Sprint 消耗倍数默认改为 3.5，支持配置覆盖（[scripts/Game/Components/Stamina/SCR_RSS_Settings.c](scripts/Game/Components/Stamina/SCR_RSS_Settings.c)、[scripts/Game/Components/Stamina/SCR_StaminaConstants.c](scripts/Game/Components/Stamina/SCR_StaminaConstants.c)）
- **体重参与消耗** - 体力消耗输入改为“装备+身体”的总重，并优化调试输出（[scripts/Game/PlayerBase.c](scripts/Game/PlayerBase.c)）

### 🐞 修复

- **室内判定误报** - 通过屋顶射线与水平封闭检测降低开放屋顶/天窗误判（[scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c](scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)）
- **引擎温度极值退化** - 当日最小/最大温度趋同会回退到物理/模拟估算，避免温度异常（[scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c](scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c)）
- **客户端写盘覆盖** - 客户端不再写入本地 JSON，避免覆盖服务器配置（[scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c](scripts/Game/Components/Stamina/SCR_RSS_ConfigManager.c)）


## [3.11.1] - 2026-02-02

本版本为 3.11.0 的配置系统增强与修正版本。

### 🔧 配置修复

- **修复 JSON 配置被强制覆盖** - 用户通过 JSON 修改的 `m_bHintDisplayEnabled`、`m_bDebugLogEnabled` 等 UI 设置不再被 `EnsureDefaultValues()` 覆盖
- **修复 ValidateSettings 过于激进** - 单个参数越界时改为 `FixInvalidSettings()` 仅 clamp 越界字段，不再清空全部配置
- **修复 Custom 预设大小写敏感** - "custom" / "CUSTOM" 等变体能正确识别为 Custom 模式

### ⚙️ 配置与默认值

- **HUD 开关默认关闭** - `m_bHintDisplayEnabled` 默认值改为 false。为不破坏已有设置：若你曾使用过该模组并已生成 JSON 配置文件，HUD 会按之前的设置正常工作；首次下载该模组的用户则默认关闭 HUD。工作台模式仍强制开启以便调试。
- **配置字段可读性优化** - 提取 magic number 为常量，精简 Attribute 描述，统一分组与格式


## [3.11.0] - 2026-01-30

本版本涵盖自 **main 分支最后一次提交** 至 **当前 dev 最新提交** 的全部变更。

### 🌟 核心功能更新

#### ✅ 体力系统优化
- **优化体力系统响应速度和起步体验** - 提升系统响应灵敏度
- **修复体力系统的高频监听和速度计算问题** - 解决性能瓶颈
- **添加室内环境忽略坡度影响功能** - 室内场景更合理
- **优化代谢净值算法并增强绝境呼吸机制** - 起步与恢复更自然
- **修复坡度计算、体力消耗和恢复逻辑的多个问题**
- **更新体力消耗和恢复计算逻辑**
- **修正姿态消耗倍数参数范围错误**
- **调整消耗与恢复参数以修复极端情况问题**
- **移除 PlayerBase 自定义速度计算逻辑，改用原生 GetVelocity 方法**

#### ✅ 负重系统增强
- **实现库存变更时实时更新负重缓存** - 更准确的负重计算
- **修复武器重量未计入总负重的问题** - 解决负重计算偏差

#### ✅ 配置与常量
- **修复预设配置逻辑** - 确保系统预设值保持最新
- **优化 RSS 系统参数并调整配置文件路径**
- **重构常量获取方式为动态配置方法** - 移除硬编码常量并迁移至配置管理器
- **修复 C 文件与 Python 数字孪生常量不匹配问题**
- **将 GIVONI_CONSTANT 从 0.3 调整为 0.6**
- **优化体力系统参数以匹配不同运动模型**

### 🔧 Tools 工具集（3.11.0 现状）

#### ✅ 核心管道与仿真
- **rss_super_pipeline.py** - 主优化管道（内置多目标优化，试验次数可配置，默认 500）
- **rss_digital_twin_fix.py** - 数字孪生仿真器（与游戏逻辑对齐，被 pipeline / 校准脚本调用）
- **stamina_constants.py** - 常数定义工具库

#### ✅ 校准与工具
- **calibrate_run_3_5km.py** - 3.5km/15:27 校准（最低体力约 20%）
- **calibrate_recovery.py** - 恢复时间校准
- **embed_json_to_c.py** - JSON → SCR_RSS_Settings.c 嵌入
- **verify_json_params.py** - JSON 参数校验
- **rss_optimizer_gui.py** - 优化器 GUI（可选）

#### ✅ 优化器与管道改进
- 多目标优化：Realism_Loss、Playability_Burden、Stability_Risk
- 多阶段优化与数值保护机制
- 生理学合理性评估与进度条计算
- 修正恢复倍数顺序检查逻辑
- 重构优化器目标函数和约束条件
- 增强场景库与生理学合理性评估
- 优化数字孪生仿真器性能与超级管道配置

#### ✅ 已移除或更替
- 删除过时脚本与图表文件（*.png、旧诊断/生成/预设脚本等）
- 移除稳定性 BUG 检测报告和一致性检查工具
- 优化器逻辑已并入 `rss_super_pipeline.py`，不再单独保留 `rss_optimizer_optuna.py`

### 📁 项目清理与优化

- **删除所有生成的图表文件** (*.png)，减小仓库体积
- **删除过时优化配置文件**，更新相关路径
- **清理不再使用的工具和文档文件**
- **移除旧的优化配置文件和更新相关路径**

### 📚 文档完善

- **tools/README.md** - 完整工具集文档（核心文件、快速开始、工作流、基准、常见任务、故障排除）
- **docs/体力系统计算逻辑文档.md** - 体力系统计算逻辑与常量说明
- **docs/数字孪生优化器计算逻辑文档.md** - 数字孪生仿真器公式与决策树
- **更新中文 README 和文档**，添加新工具及优化说明

### ✨ 3.11.0 的主要收益

1. **项目精简** - 删除无用文件，减小仓库体积
2. **维护更清晰** - Tools 目录结构优化，核心管道与数字孪生职责明确
3. **文档一致** - 文档与当前代码、工具链一致
4. **性能稳定** - 各项优化确保系统稳定运行
5. **易于部署** - 清晰的文件结构和说明文档
6. **功能增强** - 体力/负重/配置/优化器全面优化
7. **计算准确** - 修复负重与常量一致性问题
8. **环境智能** - 室内环境优化，场景更合理

### 🎯 推荐配置预设

- **EliteStandard** - 最高拟真度（realism_loss: 0.1815）
- **TacticalAction** ⭐ **推荐** - 最佳平衡（playability_burden: 422.8）
- **StandardMilsim** - 保守稳定配置（易于调整）

### 🔸 包含的提交（3.11.0，自 main 至 dev 最新）

- 404885b 更新中文README和文档，添加新工具及优化说明
- be89825 delete: 移除稳定性BUG检测报告和一致性检查工具
- 9528e0a refactor(rss): 优化体力系统参数以匹配不同运动模型
- 2100010 fix(模型): 修复C代码与Python数字孪生间的不一致问题
- a99f128 feat: 更新RSS系统配置参数并添加常量对比工具
- cfe4094 refactor(stamina): 重构体力系统配置并清理无用文件
- c846fac feat: 添加RSS优化器修复与验证工具
- 28792e3 fix(rss_super_pipeline): 修正恢复倍数顺序检查逻辑
- 7341d30 feat(rss): 实现多阶段优化和数值保护机制
- a17fbe0 feat: 添加生理学合理性评估并更新进度条计算
- 01423dc feat: 添加RSS优化器可视化GUI及优化报告文档
- 518e10b refactor(optimizer): 重构优化器目标函数和约束条件
- 0757a19 fix: 将GIVONI_CONSTANT从0.3调整为0.6
- 8651637 refactor(StaminaSystem): 重构常量获取方式为动态配置方法
- b8315da fix(体力系统): 修正姿态消耗倍数参数范围错误
- 498bce5 fix(耐力系统): 调整消耗与恢复参数以修复极端情况问题
- d93b618 fix(耐力系统): 修复C文件与Python数字孪生常量不匹配问题
- 4505132 revert(Stamina): 恢复SCR_StaminaConstants.c到eb5125e状态
- af883f2 feat(rss_super_pipeline): 增强场景库并添加生理学合理性评估
- 18afb64 refactor(Stamina): 移除硬编码常量并迁移至配置管理器
- eb5125e fix(耐力系统): 调整耐力恢复参数以平衡游戏性
- e0f720b fix(Stamina): 修复坡度计算、体力消耗和恢复逻辑的多个问题
- 6b34706 feat(RSS配置): 更新优化后的RSS配置参数及新增稳定性报告文件
- 6c01f28 refactor(PlayerBase): 移除自定义速度计算逻辑，改用原生GetVelocity方法
- 6aacdf0 feat: 更新RSS配置参数及预设值
- 1070135 feat(rss): 优化数字孪生仿真器性能并调整超级管道配置
- 9b237d8 chore: 清理不再使用的工具和文档文件
- 103fb5a fix(体力系统): 更新体力消耗和恢复计算逻辑
- 064a6ea fix(数字孪生模拟器): 修复硬编码参数问题并更新优化器配置
- d4689ee feat: 更新RSS系统配置和工具脚本
- a155c53 chore: 清理冗余文件和更新文档
- 2ad423c chore: 清理项目文件并更新文档
- 8baf755 delete: 移除旧的优化配置文件和更新相关路径
- 810070e feat: 优化RSS系统参数并调整配置文件路径
- 714d055 feat(体力系统): 添加室内环境忽略坡度影响功能
- e8d7e7f fix(负重计算): 修复武器重量未计入总负重的问题
- 3ff1f6e feat(负重系统): 实现库存变更时实时更新负重缓存
- 4d1be25 fix(Stamina): 修复体力系统的高频监听和速度计算问题
- ac06fc2 fix(配置管理): 修复预设配置逻辑，确保系统预设值保持最新
- d1ebb9c feat(Stamina): 优化体力系统响应速度和起步体验
- 42e446f feat(体力系统): 优化代谢净值算法并增强绝境呼吸机制
- 8bf680e fix(耐力系统): 修复速度计算异常并优化性能
- f2352b5 feat: 添加拟真体力系统(RSS)核心模块和工具脚本

---

## [3.10.0] - 2026-01-24

### 🎯 主要更新

- 优化代谢净值与呼吸相关机制，改进起步体验和响应（性能与稳定性提升）

### 🔸 包含的提交（3.10.0）

- f2352b5 feat: 添加拟真体力系统(RSS)核心模块和工具脚本
- 8bf680e fix(耐力系统): 修复速度计算异常并优化性能
- 42e446f feat(体力系统): 优化代谢净值算法并增强绝境呼吸机制

---

## [3.8.0] - 2026-01-22

### 🎯 重大更新

#### ✅ OOP绑定修复
- **完全重写 `rss_digital_twin.py`** - 严格面向对象设计
  - 所有计算函数移入类内
  - 所有全局常量访问替换为 `self.constants`
  - 修复逻辑耦合问题（`RUN_DRAIN_PER_TICK` vs `BASE_RECOVERY_RATE`）

#### ✅ 可视化脚本重构
- **`generate_comprehensive_trends.py`** - 使用 `RSSDigitalTwin`
- **`multi_dimensional_analysis.py`** - 使用 `RSSDigitalTwin`
- 移除所有独立计算函数

#### ✅ 常量更新
- **`ENERGY_TO_STAMINA_COEFF`**: 3.5e-05（优化后的值）
- **`BASE_RECOVERY_RATE`**: 0.0005（优化后的值）
- **`GIVONI_CONSTANT`**: 0.15（物理基线）
- **`MAX_DRAIN_RATE_PER_TICK`**: 1.0（>= 0.10，无截断）

#### ✅ OOP绑定验证
- 创建 `debug_binding.py` - OOP绑定调试测试
- 创建 `test_complete.py` - 完整测试脚本
- 所有测试通过：
  - 直接速度倍数敏感性测试 ✓
  - 短场景测试 ✓
  - OOP绑定验证 ✓

#### ✅ 配置转换
- **`convert_optimized_config.py`** - JSON转C代码
- 更新 `SCR_RSS_Settings.c` 中的预设参数：
  - `StandardMilsim` 预设
  - `TacticalAction` 预设
  - `EliteStandard` 预设

#### ✅ 敏感性分析
- **`rss_sensitivity_analysis_enhanced.py`** - 增强版敏感性分析
- 验证参数敏感性：
  - `encumbrance_stamina_drain_coeff`: 184.0727（High/Strong）✓
  - `base_recovery_rate`: 0.0000（场景设计问题，非OOP绑定问题）

#### ✅ 工具目录清理
- 删除过时或测试脚本：
  - `auto_calibrate.py` - 自动校准脚本
  - `calibration_results.txt` - 校准结果
  - `calibration_results_v2.txt` - 校准结果v2
  - `debug_binding.py` - OOP绑定调试测试
  - `test_complete.py` - 完整测试脚本
  - `validation_test.py` - 旧验证测试
  - `validation_test_final.py` - 旧验证测试
  - `validation_test_improved.py` - 旧验证测试
  - `validation_test_less_demanding.py` - 旧验证测试
  - `validation_test_ultra_simple.py` - 旧验证测试
  - `validation_test_simple.py` - 旧验证测试
  - `simulate_stamina_system.py` - 旧仿真脚本

- 保留有用且可复用的脚本：
  - `rss_digital_twin.py` - 核心仿真引擎
  - `rss_scenarios.py` - 场景定义
  - `stamina_constants.py` - 常量定义
  - `requirements.txt` - Python依赖
  - `rss_optimizer_optuna.py` - 优化引擎
  - `run_complete_optimization.py` - 运行优化
  - `convert_optimized_config.py` - JSON转C代码
  - `rss_report_generator.py` - 生成报告
  - `rss_report_generator_english.py` - 生成报告
  - `rss_visualization.py` - 生成可视化
  - `generate_comprehensive_trends.py` - 生成趋势分析
  - `multi_dimensional_analysis.py` - 多维度分析
  - `rss_sensitivity_analysis.py` - 敏感性分析
  - `rss_sensitivity_analysis_enhanced.py` - 敏感性分析

### 📋 技术改进

#### 1. 逻辑耦合修复
- **问题**：`calculate_stamina_consumption` 中使用 `self.constants.RUN_DRAIN_PER_TICK` 作为恢复率基准
- **修复**：改为使用 `self.constants.BASE_RECOVERY_RATE`
- **影响**：恢复系统现在使用正确的基准值

#### 2. 全局常量访问
- **问题**：所有计算函数使用全局常量（如 `ENERGY_TO_STAMINA_COEFF`）
- **修复**：所有常量访问改为 `self.constants.XXX`
- **影响**：OOP绑定现在完全正确，参数修改会立即生效

#### 3. 生理上限调整
- **问题**：`MAX_DRAIN_RATE_PER_TICK = 0.10` 过低，导致参数截断
- **修复**：提高到 `MAX_DRAIN_RATE_PER_TICK = 1.0`
- **影响**：参数现在可以自由变化，不会被截断

### 🎯 优化结果

#### 3个预设配置
1. **StandardMilsim（平衡模式）**
   - `energy_to_stamina_coeff`: 3.03e-05
   - `base_recovery_rate`: 0.000433

2. **TacticalAction（可玩性优先）**
   - `energy_to_stamina_coeff`: 2.98e-05
   - `base_recovery_rate`: 0.000488

3. **EliteStandard（拟真度优先）**
   - `energy_to_stamina_coeff`: 2.96e-05
   - `base_recovery_rate`: 0.000421

### 📊 测试结果

#### OOP绑定测试
- **直接速度倍数测试**：12.07% 差异 ✓
- **短场景测试**：7.37% 距离差异 ✓
- **OOP绑定验证**：所有测试通过 ✓

#### 敏感性分析
- **总距离**：215.4m（合理范围200m-1000m）✓
- **最终体力**：5.8%（> 0%）✓
- **消耗参数敏感性**：184.0727（High/Strong）✓

### 🔧 破坏性修复

1. **OOP绑定问题** - 完全修复
2. **逻辑耦合问题** - 完全修复
3. **参数截断问题** - 完全修复
4. **场景设计问题** - 识别并说明

### 📚 文档更新

- **README.md** - 更新到版本3.8.0
- **CHANGELOG.md** - 创建更新日志

## [3.7.0] - 2026-01-22

### 新增
- **增强版参数敏感性分析工具（Enhanced Sensitivity Analysis Tools v3.7.0）**
  - **核心功能**：
    - 局部敏感性分析（One-at-a-time）
    - 全局敏感性分析（Spearman 秩相关）
    - 参数交互效应分析（多维度）
    - 联合敏感性分析（同时调整多个参数）
  - **新增模块**：
    - `rss_sensitivity_analysis_enhanced.py`: 增强版敏感性分析工具（约 800 行）

### 改进
- **参数敏感性分析优化**
  - **移除未使用参数**：移除了数字孪生仿真器中未使用的参数（energy_to_stamina_coeff, encumbrance_speed_penalty_coeff, load_recovery_penalty_exponent）
  - **扩大参数范围**：从 ±20% 扩大到 ±50%，共 10 个参数
  - **优化目标函数**：从 2 个指标增加到 3 个指标（完成时间、恢复时间、最低体力）
  - **增加参数对**：从 3 对增加到 9 对，覆盖恢复系统、负重系统、疲劳系统以及跨系统交互
  - **修复参数传递**：为每次仿真创建新的常量实例，确保参数变化生效
  - **使用真实仿真器**：使用真正的数字孪生仿真器，而不是返回固定值 50.0

### 修复
- **体力为 0 死锁问题**
  - **问题描述**：体力系统在体力降为 0 时无法恢复，导致玩家卡死
  - **修复方案**：
    - 修改体力恢复逻辑，允许体力从 0 开始恢复
    - 移除体力为 0 时的恢复限制
  - **影响**：玩家在体力耗尽后可以正常恢复体力
- **绝境呼吸保护机制（Desperation Wind）**
  - **问题描述**：体力极低时（< 2%），负重静态消耗可能抵消恢复，导致无法脱离危险区
  - **修复方案**：
    - 引入绝境呼吸保护机制：当体力 < 2% 且非水下时，给予保底恢复值 0.0001
    - 忽略背包重量的静态消耗，保证哪怕一丝微弱的体力恢复，打破死锁
    - 相当于每秒恢复 0.05%，20 秒可脱离危险区
  - **影响**：玩家在体力极低时可以缓慢恢复体力，避免卡死

### 已知问题
- **当前场景（ACFT 2英里测试）对参数变化不敏感**
  - 所有参数敏感性为 0.0000
  - 原因：场景过于简单（固定速度、固定负重）
  - 建议：使用更复杂的场景（包含负重变化、坡度变化、速度变化）
  - 建议：增加更多测试工况（如 Everon 拉练、游泳测试等）

### 代码统计
- `rss_sensitivity_analysis_enhanced.py`: 新建文件（约 800 行）
- `enhanced_sensitivity_report.txt`: 自动生成报告（约 60 行）
- `local_sensitivity_enhanced.png`: 自动生成图表
- `joint_sensitivity.png`: 自动生成图表
- `global_sensitivity_enhanced.png`: 自动生成图表
- `interaction_analysis_enhanced.png`: 自动生成图表
- **总计**: 约 800 行新增/修改

### 文档
- 更新 `rss_sensitivity_analysis_enhanced.py` 版本号到 v3.7.0
- 更新 `rss_sensitivity_analysis_enhanced.py` 添加更新日志和已知问题说明
- 更新 CHANGELOG.md，添加 v3.7.0 详细更新日志

---

## [3.6.0] - 2026-01-21

### 新增
- **姿态转换成本系统 v1.0（Stance Transition Cost v1.0 - 乳酸堆积模型）**
  - **核心思路**：单次动作很轻，连续动作剧增。模拟肌肉在连续负重深蹲时从有氧到无氧的转变。
  - **基础数值**：
    - PRONE → STAND: 1.5%
    - PRONE → CROUCH: 1.0%
    - CROUCH → STAND: 0.5%
    - STAND → PRONE: 0.3%
    - 其他转换: 0.3%
  - **疲劳堆积器（Stance Fatigue Accumulator）**：
    - 引入隐藏变量 `m_fStanceFatigue`，模拟肌肉在连续负重深蹲时的乳酸堆积
    - 增加：每次变换姿态，这个变量增加 1.0
    - 衰减：每秒钟自动减少 0.5
    - 影响：实际体力消耗 = BaseCost × WeightFactor × (1.0 + m_fStanceFatigue)
  - **负重因子线性化**：将原来的 1.5 次幂改为线性（WeightFactor = CurrentWeight / 90.0）
  - **协同保护逻辑**：
    - 不触发"呼吸困顿期"：姿态转换属于瞬时消耗，不应该像 Sprint 停止后那样触发 5 秒禁止恢复的逻辑
    - 体力阈值保护：当体力低于 10% 时，强制减慢姿态切换动画（如果能控制动画速度）或禁止直接从趴姿跳跃站起
  - **新增模块**：
    - `SCR_StanceTransitionManager.c`: 姿态转换管理模块（约 220 行）
  - **新增常量**：
    - `STANCE_COST_PRONE_TO_STAND`: 1.5%
    - `STANCE_COST_PRONE_TO_CROUCH`: 1.0%
    - `STANCE_COST_CROUCH_TO_STAND`: 0.5%
    - `STANCE_COST_STAND_TO_PRONE`: 0.3%
    - `STANCE_COST_OTHER`: 0.3%
    - `STANCE_FATIGUE_ACCUMULATION`: 1.0
    - `STANCE_FATIGUE_DECAY`: 0.5
    - `STANCE_FATIGUE_MAX`: 10.0
    - `STANCE_WEIGHT_BASE`: 90.0
    - `STANCE_TRANSITION_MIN_STAMINA_THRESHOLD`: 0.10

### 改进
- **跳跃检测优化（Jump Detection Optimization）**
  - **问题描述**：从趴/蹲姿跳跃时，同时扣除跳跃消耗和姿态转换消耗，导致体力骤降
  - **修复方案**：
    - 检查前一帧姿态：如果从趴着或蹲着跳跃，则不算跳跃消耗，而是姿态变换
    - 新增 `m_eLastStance` 变量记录上一帧姿态
    - 在所有返回路径中更新 `m_eLastStance`
    - 添加调试信息显示原始姿态名称
  - **修改的文件**：
    - `SCR_JumpVaultDetection.c`: 添加姿态检查逻辑（约 30 行修改）
  - **修复后行为**：
    - 从站姿跳跃：扣除跳跃消耗（3.5%）
    - 从蹲姿跳跃：不扣除跳跃消耗，由姿态转换系统处理（0.5%）
    - 从趴姿跳跃：不扣除跳跃消耗，由姿态转换系统处理（1.5%）

### 技术亮点
- **乳酸堆积模型**：基于真实的肌肉疲劳机制，模拟从有氧到无氧的转变
- **阶梯式累进惩罚**：单次动作很轻，连续动作剧增
- **线性负重因子**：避免负重影响过快，更符合游戏平衡
- **姿态感知跳跃检测**：准确判断是否从趴/蹲姿跳跃，避免重复扣除体力
- **协同保护逻辑**：不触发"呼吸困顿期"，允许玩家在变换姿态后立即开始恢复

### 代码统计
- **SCR_StanceTransitionManager.c**: 新建文件（约 220 行）
- **SCR_StaminaConstants.c**: 新增 8 个姿态转换相关常量（约 40 行修改）
- **SCR_JumpVaultDetection.c**: 添加姿态检查逻辑（约 30 行修改）
- **PlayerBase.c**: 集成姿态转换模块（约 5 行修改）
- **SCR_DebugDisplay.c**: 更新调试信息显示（约 25 行修改）
- **总计**: 约 150 行新增/修改

### 文档
- 更新 README.md 版本历史，添加 v3.6.0 更新内容
- 更新 CHANGELOG.md，添加 v3.6.0 详细更新日志

---

## [3.5.0] - 2026-01-21

### 修复
- **降雨湿重系统重构（Rain Wet Weight System Refactoring）**
  - **问题描述**：降雨湿重计算存在双重方法冲突，室内检测失效
  - **根本原因**：
    - 存在两个湿重计算方法：`CalculateRainWeight()`（基于字符串匹配）和 `CalculateRainWetWeight()`（基于降雨强度API）
    - `CalculateRainWeight()` 方法覆盖了 `CalculateRainWetWeight()` 的结果
    - `CalculateRainWeight()` 不检查室内状态，立即设置湿重到最大值
    - `deltaTime` 计算错误：`m_fLastUpdateTime` 在方法开始时就被更新，导致 `deltaTime` 永远是 0
  - **修复方案**：
    - **删除字符串匹配方法**：完全移除 `CalculateRainWeight()` 方法
    - **统一数据源**：所有降雨计算都使用 `GetRainIntensity()` API（0.0-1.0范围）
    - **修复deltaTime计算**：将 `m_fLastUpdateTime` 更新移到方法末尾，在方法开始时计算 `deltaTime`
    - **修复室内检测**：室内时不增加湿重，只进行衰减
    - **简化逻辑**：室内和室外使用统一的线性衰减逻辑
  - **修复后行为**：
    - 室外下雨：湿重逐渐增加（基于降雨强度）
    - 室内下雨：湿重开始衰减（不增加）
    - 室外停止降雨：湿重开始衰减
    - 室内停止降雨：湿重继续衰减
    - 室内开始下雨：湿重保持为0
  - **删除的变量**：
    - `m_bWasRaining` - 上一帧是否在下雨
    - `m_bWasIndoor` - 上一帧是否在室内
  - **修改的方法**：
    - `SCR_EnvironmentFactor.CalculateRainWetWeight()`: 修改为接受 `deltaTime` 参数，简化逻辑
    - `SCR_EnvironmentFactor.UpdateAdvancedEnvironmentFactors()`: 修复 `deltaTime` 计算位置
    - `SCR_EnvironmentFactor.IsRaining()`: 改为基于降雨强度判断
    - `SCR_RSS_ConfigManager`: 更新版本号到 v3.5.0

- **游泳湿重系统优化（Swimming Wet Weight Optimization）**
  - **问题描述**：游泳时湿重为0，上岸后才设置湿重
  - **根本原因**：`UpdateWetWeight()` 方法在游泳时设置 `wetWeightStartTime = -1.0` 和 `currentWetWeight = 0.0`
  - **修复方案**：
    - **非线性增长**：游泳时湿重使用平方根函数增长：`wetWeight = 10.0 * sqrt(duration / 60.0)`
    - **增长时间**：60秒时达到最大值10kg
    - **即时生效**：游泳时湿重立即增加到负重，不是上岸后才生效
    - **共用负重池**：游泳湿重和降雨湿重共用一个负重池：`totalWetWeight = swimmingWetWeight + rainWeight`
    - **最大限制**：10KG
  - **新增变量**：
    - `m_fSwimStartTime` - 游泳开始时间（秒）
    - `m_fSwimDuration` - 游泳持续时间（秒）
  - **修改的方法**：
    - `SCR_SwimmingState.UpdateWetWeight()`: 添加游泳时间追踪，修改湿重计算逻辑
    - `SCR_SwimmingState.CalculateTotalWetWeight()`: 修改为简单相加，移除加权平均逻辑
  - **修复后行为**：
    - 游泳时：湿重非线性增长（0 → 10kg，60秒达到最大值）
    - 上岸后：湿重保持不变，开始30秒线性衰减
    - 游泳+下雨：总湿重 = 游泳湿重 + 降雨湿重（最大10kg）

- **HUD显示更新（HUD Display Update）**
  - **问题描述**：游泳时运动类型和地面材质显示不准确
  - **修复方案**：
    - **游泳时运动类型**：显示"Swim"而非当前移动类型
    - **游泳时地面材质**：显示"Water"而非地面密度判断的材质
    - **颜色编码**：Water使用蓝色（Color.FromRGBA(0, 150, 255, 255)）
  - **新增参数**：
    - `UpdateAllValues()` 方法：添加 `isSwimming` 参数
    - `s_bCachedIsSwimming` - 缓存的游泳状态
  - **修改的方法**：
    - `SCR_StaminaHUDComponent.UpdateAllValues()`: 添加游泳状态参数
    - `SCR_StaminaHUDComponent.UpdateDisplay()`: 添加游泳状态显示逻辑

### 改进
- **代码简化**：删除了约100行重复代码（`CalculateRainWeight()` 方法）
- **逻辑清晰**：室内和室外使用统一的衰减逻辑，不再需要复杂的状态追踪
- **性能优化**：减少不必要的状态变量和条件判断

### 技术亮点
- **统一数据源**：所有降雨计算都使用 `GetRainIntensity()` API，不再使用字符串匹配
- **正确的deltaTime**：确保湿重增加和衰减都能正常工作
- **非线性增长**：游泳湿重使用平方根函数，增长逐渐变慢
- **共用负重池**：游泳湿重和降雨湿重相加，最大限制10KG
- **HUD状态显示**：游泳时正确显示运动类型和地面材质

### 代码统计
- **SCR_EnvironmentFactor.c**: 删除 `CalculateRainWeight()` 方法，修复 `deltaTime` 计算（约80行修改）
- **SCR_SwimmingState.c**: 修改湿重计算逻辑，添加游泳时间追踪（约50行修改）
- **SCR_StaminaHUDComponent.c**: 添加游泳状态显示支持（约30行修改）
- **SCR_DebugDisplay.c**: 更新调试信息显示游泳状态（约10行修改）
- **SCR_RSS_ConfigManager.c**: 更新版本号到 v3.5.0（1行修改）

### 文档
- 更新 README.md 版本历史，添加 v3.5.0 更新内容
- 更新 CHANGELOG.md，添加 v3.5.0 详细更新日志

---

## [3.4.1] - 2026-01-21

### 修复
- **紧急修复：配置文件覆盖未生效（Config Override Not Working）**
  - **问题描述**：用户在 JSON 配置文件中修改的预设参数值被硬编码的默认值覆盖
  - **根本原因**：`InitPresets()` 方法在从 JSON 读取配置后被调用，无条件覆盖所有预设参数
  - **修复方案**：将 `InitPresets()` 拆分为 4 个独立的初始化方法，只有当预设对象为 `null` 时才初始化默认值
    - `InitEliteStandardDefaults(bool shouldInit)`
    - `InitStandardMilsimDefaults(bool shouldInit)`
    - `InitTacticalActionDefaults(bool shouldInit)`
    - `InitCustomDefaults(bool shouldInit)`
  - **修复后行为**：
    - 如果 JSON 中存在预设配置，保留用户的配置值
    - 如果 JSON 中缺少某个预设，才使用硬编码的默认值

- **紧急修复：配置字段默认值为空（Config Fields Empty）**
  - **问题描述**：JSON 配置文件中 `m_sConfigVersion`、`m_sSelectedPreset` 等字段为空字符串，HUD 显示默认关闭
  - **根本原因**：`[Attribute]` 的 `defvalue` 参数只是 UI 编辑器的默认值，不会在代码中自动初始化变量
  - **修复方案**：新增 `EnsureDefaultValues()` 方法，在加载配置后检查并设置所有必要字段的默认值
  - **处理的字段**：
    - `m_sConfigVersion` - 默认为当前版本号
    - `m_sSelectedPreset` - 默认为 "StandardMilsim"
    - `m_bHintDisplayEnabled` - 旧版本配置默认开启（新版本保留用户设置）
    - `m_iHintUpdateInterval` - 默认 5000ms
    - `m_fHintDuration` - 默认 2.0s
    - `m_iDebugUpdateInterval` - 默认 5000ms
    - `m_iTerrainUpdateInterval` - 默认 5000ms
    - `m_iEnvironmentUpdateInterval` - 默认 5000ms
    - `m_fStaminaDrainMultiplier` - 默认 1.0
    - `m_fStaminaRecoveryMultiplier` - 默认 1.0
    - `m_fSprintSpeedMultiplier` - 默认 1.3
    - `m_fSprintStaminaDrainMultiplier` - 默认 3.0
  - **特殊处理**：`m_bHintDisplayEnabled` 由于 bool 默认为 false，无法区分"用户设置为 false"和"字段不存在"，所以只有在旧版本配置时才强制开启

- **新增调试日志**：
  - 加载时打印预设状态（`Elite=OK/NULL Standard=OK/NULL Tactical=OK/NULL Custom=OK/NULL`）
  - 启动时打印当前激活预设的关键参数值
  - 设置默认值时打印日志（便于排查问题）

### 改进
- **Custom 预设配置生效逻辑（Custom Preset Configuration Logic）**
  - **问题描述**：全局配置项（体力倍率、环境因子开关、高级系统开关）在所有预设下都生效，与预设系统设计冲突
  - **修复方案**：这些配置项现在**仅在选择 Custom 预设时生效**
  - **配置生效规则**：
    | 预设 | 40 个核心参数 | 全局配置项 |
    |------|--------------|-----------|
    | EliteStandard | `m_EliteStandard` | 忽略，使用默认值 |
    | StandardMilsim | `m_StandardMilsim` | 忽略，使用默认值 |
    | TacticalAction | `m_TacticalAction` | 忽略，使用默认值 |
    | **Custom** | `m_Custom` | **读取用户配置** |
  - **仅 Custom 预设生效的配置项**：
    - 体力系统：`m_fStaminaDrainMultiplier`、`m_fStaminaRecoveryMultiplier`、`m_fEncumbranceSpeedPenaltyMultiplier`、`m_fSprintSpeedMultiplier`、`m_fSprintStaminaDrainMultiplier`
    - 环境因子开关：`m_bEnableHeatStress`、`m_bEnableRainWeight`、`m_bEnableWindResistance`、`m_bEnableMudPenalty`
    - 高级系统开关：`m_bEnableFatigueSystem`、`m_bEnableMetabolicAdaptation`、`m_bEnableIndoorDetection`
  - **非 Custom 预设默认值**：所有倍率为 1.0（Sprint 速度为 1.3），所有开关为 true

- **HUD 配置项描述更新**
  - 更新 `m_bHintDisplayEnabled` 描述：说明它控制的是屏幕右上角 HUD 显示
  - 标记 `m_iHintUpdateInterval` 和 `m_fHintDuration` 为已弃用（HUD 现在是实时更新的常驻显示）

### 代码统计
- **SCR_RSS_Settings.c**: 重构 `InitPresets()` 方法，更新配置项描述（约 50 行修改）
- **SCR_RSS_ConfigManager.c**: 新增 `EnsureDefaultValues()` 方法和调试日志（约 120 行）
- **SCR_StaminaConstants.c**: 新增 `IsCustomPreset()` 方法，修改 12 个配置获取函数（约 40 行修改）

---

## [3.4.0] - 2026-01-21

### 新增
- **实时状态 HUD 显示系统（Real-time Status HUD Display System）**
  - 在屏幕右上角显示完整的体力系统状态信息（类似游戏原生的 FPS/延迟显示条）
  - 使用自定义 UI 布局文件（`StaminaHUD.layout`），水平排列的紧凑条状设计
  - 每 0.2 秒更新一次，与体力系统同步
  - 通过 `m_bHintDisplayEnabled` 配置项控制开关
  - **显示内容（10项）**：
    - **STA XX%** - 体力百分比（<20% 红色, <40% 橙色, 其他白色）
    - **SPD X.Xm/s** - 当前实际速度（颜色基于速度倍数：≥95% 红色, ≥80% 橙色）
    - **WT XXkg** - 当前负重（≥40kg 红色, ≥30kg 橙色）
    - **Idle/Walk/Run/Sprint** - 移动类型
    - **SLOPE ±Xdeg** - 坡度角度（≥20° 红色, ≥10° 橙色）
    - **TEMP XXC** - 虚拟气温（≥35°C 红色, ≥28°C 橙色, 15-27°C 白色, 5-14°C 浅蓝, <5°C 蓝色）
    - **WIND [方向] Xm/s** - 风速风向（≥15m/s 红色, ≥8m/s 橙色；无风显示 "Calm"；8方向：N/NE/E/SE/S/SW/W/NW）
    - **Indoor/Outdoor** - 室内/室外状态（室内绿色, 室外白色）
    - **地面类型** - 基于物理密度的地面材质（Wood/Floor/Grass/Dirt/Gravel/Paved/Sand/Rock）
    - **WET Xkg** - 湿重（有湿重时青色, 无湿重白色）
  - **SCR_StaminaHUDComponent.c** - HUD 组件模块（约 500 行）
    - 单例模式管理，自动初始化和销毁
    - 智能变化检测（只有数值变化时才更新 UI，优化性能）
    - 丰富的颜色编码系统，直观反映状态好坏

- **配置系统扩展**
  - **SCR_RSS_Settings.c** 新增配置项：
    - `m_sConfigVersion` - 配置文件版本号（用于自动迁移）
    - `m_bHintDisplayEnabled` - HUD 显示开关（默认开启）
    - `m_iHintUpdateInterval` - HUD 更新间隔（毫秒）
    - `m_fHintDuration` - Hint 持续时间（秒）
  - 工作台模式下自动开启 HUD 显示

- **配置版本检测与自动迁移系统（Config Version Detection & Auto-Migration）**
  - JSON 配置文件新增 `m_sConfigVersion` 字段，记录配置版本号
  - 模组启动时自动检测配置版本与模组版本是否匹配
  - 版本不匹配时自动执行迁移：
    - 保留用户已有的配置值
    - 只添加新版本新增字段的默认值
    - 更新配置版本号并保存
  - 日志输出迁移过程，方便服主排查问题
  - 版本比较支持语义化版本号（如 "3.4.0" vs "3.3.0"）

### 改进
- **风向显示修复**
  - 修复 API 返回"风吹向的方向"而非"风来自的方向"的问题
  - 反转 180 度后正确显示风的来源方向（如游戏设置 SE 风，HUD 显示 SE）
- **地面类型显示优化**
  - 使用游戏 API 的物理密度值（`BallisticInfo.GetDensity()`）
  - 基于实际密度范围判断地面类型（参考 `SCR_RealisticStaminaSystem.GetTerrainFactorFromDensity()`）：
    - Wood (≤0.7): 木质表面 - 绿色
    - Floor (≤1.15): 室内地板 - 绿色
    - Grass (≤1.25): 草地 - 白色
    - Dirt (≤1.4): 土质 - 白色
    - Gravel (≤1.65): 鹅卵石/碎石路 - 橙色
    - Paved (≤2.4): 沥青/混凝土 - 绿色
    - Sand (≤2.8): 沙地 - 橙色
    - Rock (>2.8): 岩石/碎石 - 红色
- **颜色编码系统**
  - 负重颜色基于战斗负重阈值（30kg 橙色）和最大负重（40kg 红色）
  - 坡度颜色基于陡峭程度（≥10° 橙色, ≥20° 红色）
  - 温度颜色基于舒适度（寒冷蓝色, 舒适白色, 炎热橙色, 高温红色）
  - 地面颜色：绿色=良好路面，白色=普通，橙色=困难，红色=非常困难

### 技术亮点
- **模块化 HUD 架构**：独立的 HUD 组件，使用 `WorkspaceWidget.CreateWidgets()` 直接创建，不依赖复杂的 InfoDisplay 注册
- **配置门禁**：通过 `m_bHintDisplayEnabled` 配置项控制 HUD 显示，支持热配置
- **性能优化**：智能变化检测，构建完整文本哈希比对，避免不必要的 UI 更新
- **国际化支持**：所有标签使用英文缩写，避免游戏字体不支持中文的问题
- **颜色语义化**：红色=危险/高消耗，橙色=警告/中等，白色=正常，绿色=良好，蓝色=寒冷/湿润
- **配置版本管理**：自动检测并迁移旧版本配置，保留用户设置的同时添加新功能

### 代码统计
- **SCR_StaminaHUDComponent.c**: 新建 HUD 组件（约 500 行）
- **StaminaHUD.layout**: 新建 UI 布局文件（约 320 行）
- **SCR_DebugDisplay.c**: 更新参数传递，新增环境数据获取（约 30 行修改）
- **SCR_RSS_Settings.c**: 新增 HUD 配置项和版本号字段（约 30 行）
- **SCR_RSS_ConfigManager.c**: 新增版本检测和自动迁移系统（约 80 行）
- **PlayerBase.c**: 添加 currentSpeed 参数传递，HUD 初始化/销毁调用（约 10 行修改）

## [3.3.0] - 2026-01-21

### 新增
- **贝叶斯优化器全参数扩展（Bayesian Optimizer Full Parameter Expansion）**
  - 将优化参数从 11 个扩展到 40 个，覆盖完整的体力系统
  - 新增参数类别：
    - **有氧/无氧效率因子**：aerobic_efficiency_factor、anaerobic_efficiency_factor
    - **恢复系统参数**：recovery_nonlinear_coeff、fast_recovery_multiplier、medium_recovery_multiplier、slow_recovery_multiplier、marginal_decay_threshold、marginal_decay_coeff、min_recovery_stamina_threshold、min_recovery_rest_time_seconds
    - **Sprint和姿态参数**：sprint_speed_boost、posture_crouch_multiplier、posture_prone_multiplier
    - **动作参数**：jump_stamina_base_cost、vault_stamina_start_cost、climb_stamina_tick_cost、jump_consecutive_penalty
    - **坡度参数**：slope_uphill_coeff、slope_downhill_coeff
    - **游泳参数**：swimming_base_power、swimming_encumbrance_threshold、swimming_static_drain_multiplier、swimming_dynamic_power_efficiency、swimming_energy_to_stamina_coeff
    - **环境参数**：env_heat_stress_max_multiplier、env_rain_weight_max、env_wind_resistance_coeff、env_mud_penalty_max、env_temperature_heat_penalty_coeff、env_temperature_cold_recovery_penalty_coeff
  - 优化范围更全面，覆盖体力消耗、恢复、移动、环境等所有系统

- **三预设系统（Three-Preset Configuration System）**
  - **SCR_RSS_Params 类扩展**：从 11 个参数扩展到 40 个参数
  - **新增 StandardMilsim 预设**：标准平衡模式（Optuna 优化出的平衡配置）
  - **预设系统完善**：
    - EliteStandard：精英拟真模式（极致拟真，医学精确度优先）
    - StandardMilsim：标准平衡模式（平衡拟真与可玩性，默认预设）
    - TacticalAction：战术动作模式（流畅游戏体验，可玩性优先）
    - Custom：自定义模式（管理员可以手动调整所有参数）
  - **默认预设切换**：从 EliteStandard 切换到 StandardMilsim
    - 工作台模式仍使用 EliteStandard（用于验证）
    - 游戏模式默认使用 StandardMilsim（平衡体验）

### 改进
- **预设参数完整性**：所有 40 个参数都有详细的 Optuna 优化值
- **配置系统用户体验**：
  - 管理员可以通过修改 JSON 文件中的 `m_sSelectedPreset` 字段快速切换预设
  - StandardMilsim 作为默认预设，提供平衡的游戏体验
  - 所有参数都有详细的中英双语注释，方便管理员理解参数作用

### 技术亮点
- **全参数优化**：40 个参数覆盖完整的体力系统，优化更全面
- **三预设架构**：完美覆盖帕累托前沿上的三个关键点（极致拟真、标准平衡、战术动作）
- **Optuna 优化集成**：所有预设使用 Optuna 优化的参数
- **动态参数获取**：所有核心参数从配置管理器动态获取，支持热重载
- **中英双语注释**：所有参数都有详细的中英双语注释，提升国际化支持

### 代码统计
- **SCR_RSS_Params.c**: 从 11 个参数扩展到 40 个参数（约 320 行新增）
- **SCR_RSS_Settings.c**: 更新 InitPresets() 方法，为所有 4 个预设配置 40 个参数（约 200 行修改）
- **SCR_StaminaConstants.c**: 新增 29 个配置桥接方法（约 350 行新增）
- **rss_digital_twin.py**: 新增 20+ 个常量定义（约 100 行新增）
- **rss_optimizer_optuna.py**: 优化参数从 11 个扩展到 40 个（约 150 行修改）
- **run_complete_optimization.py**: 更新参数列表（约 50 行修改）

### 文档
- 所有 40 个参数都有详细的中英双语注释
- 更新预设选择器说明，包含三个预设的详细说明

## [3.2.0] - 2026-01-21

### 修复
- **游泳湿重系统修复（Swimming Wet Weight Fix）**
  - 修复 `SCR_EnvironmentFactor.UpdateEnvironmentFactors()` 方法没有接收游泳湿重参数的问题
  - 修复前：总湿重计算只考虑降雨湿重，完全忽略游泳湿重
  - 修复后：正确接收并使用 `swimmingWetWeight` 参数，使用 `SwimmingStateManager.CalculateTotalWetWeight()` 计算总湿重
  - 修复前：上岸后湿重不增加，体力消耗不受影响
  - 修复后：上岸后立即获得 7.5kg 湿重，30秒内线性衰减到 0kg
  - 更新调试信息，添加总湿重显示

- **时间单位错误修复（Time Unit Bug Fixes）**
  - 修复 `GetWorldTime()` 返回毫秒但多处代码没有转换为秒的问题
  - 修复前：所有时间相关的计算都错误了 1000 倍
  - 修复后：所有时间计算正确使用秒作为单位
  - 影响范围：
    - `PlayerBase.c`: 6 处修复（FatigueSystem、SwimmingStateManager、EPOC延迟）
    - `SCR_JumpVaultDetection.c`: 1 处修复（连续跳跃检测）
    - `SCR_RSS_ConfigManager.c`: 1 处修复（配置重载冷却）
    - `SCR_DebugDisplay.c`: 3 处修复（调试日志时间检查）
    - `SCR_StaminaUpdateCoordinator.c`: 1 处修复（速度计算时间）

- **ExerciseTracker 时间单位修复**
  - 修复 `ExerciseTracker` 期望接收毫秒但传入了秒的问题
  - 修复前：时间被除以 1000.0 两次，运动时间和休息时间累积慢 1000 倍
  - 修复后：正确传入毫秒，`ExerciseTracker` 内部转换为秒
  - 影响范围：
    - `PlayerBase.c:113`: `ExerciseTracker.Initialize()` 传入毫秒
    - `PlayerBase.c:493`: `currentTimeForExercise` 使用毫秒
  - 修复前的影响：
    - 疲劳恢复需要 16.7 小时后才开始（而不是 60 秒）
    - 快速恢复期持续 50 小时（而不是 3 分钟）
    - 中等恢复期持续 167 小时（而不是 10 分钟）
  - 修复后：所有时间相关功能正常工作

### 已知问题
- **体力恢复速度可能过快**
  - 当前恢复速度基于 Optuna 优化的参数，可能在实际游戏中感觉过快
  - 作者对目前体力恢复速度感到不满意，可能会在以后调整
  - 恢复速度相关参数：
    - `base_recovery_rate`: 基础恢复率（每 0.2 秒）
    - `standing_recovery_multiplier`: 站姿恢复倍数
    - `prone_recovery_multiplier`: 趴姿恢复倍数
    - `FAST_RECOVERY_MULTIPLIER`: 快速恢复期倍数（3.5x）
    - `MEDIUM_RECOVERY_MULTIPLIER`: 中等恢复期倍数（1.8x）
  - 管理员可以通过修改配置文件中的 `m_sSelectedPreset` 字段切换预设
  - 或者在 `Custom` 预设中手动调整恢复速度相关参数

### 代码统计
- **SCR_EnvironmentFactor.c**: 修复游泳湿重参数传递（约 10 行修改）
- **PlayerBase.c**: 修复时间单位错误（6 处修改）
- **SCR_JumpVaultDetection.c**: 修复连续跳跃检测时间单位（1 处修改）
- **SCR_RSS_ConfigManager.c**: 修复配置重载冷却时间单位（1 处修改）
- **SCR_DebugDisplay.c**: 修复调试日志时间单位（3 处修改）
- **SCR_StaminaUpdateCoordinator.c**: 修复速度计算时间单位（1 处修改）

## [3.1.0] - 2026-01-20

### 新增
- **预设驱动配置系统（Preset-Driven Configuration）**
  - **SCR_RSS_Params 类**：
    - 新增预设参数包类，包含 11 个由 Optuna 优化的核心参数
    - 每个参数都有详细的中英双语注释，包括参数说明、公式说明、Optuna 优化范围、预设值、影响说明
    - 参数包括：
      - energy_to_stamina_coeff：能量到体力转换系数
      - base_recovery_rate：基础恢复率
      - standing_recovery_multiplier：站姿恢复倍数
      - prone_recovery_multiplier：趴姿恢复倍数
      - load_recovery_penalty_coeff：负重恢复惩罚系数
      - load_recovery_penalty_exponent：负重恢复惩罚指数
      - encumbrance_speed_penalty_coeff：负重速度惩罚系数
      - encumbrance_stamina_drain_coeff：负重体力消耗系数
      - sprint_stamina_drain_multiplier：Sprint体力消耗倍数
      - fatigue_accumulation_coeff：疲劳累积系数
      - fatigue_max_factor：最大疲劳因子
  - **SCR_RSS_Settings 类**：
    - 新增预设选择器 `m_sSelectedPreset`（ComboBox）
      - EliteStandard：精英拟真模式（Optuna 优化出的最严格模式）
      - TacticalAction：战术动作模式（更流畅的游戏体验）
      - Custom：自定义模式（管理员可以手动调整所有参数）
    - 新增三个预设参数包属性：
      - m_EliteStandard：精英拟真预设参数包
      - m_TacticalAction：战术动作预设参数包
      - m_Custom：自定义预设参数包
    - 新增 `InitPresets()` 方法：初始化三个预设的默认值
      - EliteStandard：11 个 Optuna 优化值（精英拟真）
      - TacticalAction：11 个 Optuna 优化值（战术动作）
      - Custom：11 个合理的默认备份值
    - 为所有配置参数添加详细的中英双语注释：
      - 预设选择器：包含三个预设的详细说明
      - 预设参数包：包含每个预设的适用场景和特点
      - 调试配置：包含调试日志的用途和自动启用机制
      - 体力系统配置：包含全局体力消耗和恢复倍率
      - 移动系统配置：包含 Sprint 速度和体力消耗倍率
      - 环境因子配置：包含热应激、降雨湿重、风阻、泥泞惩罚系统
      - 高级配置：包含疲劳积累、代谢适应、室内检测系统
      - 性能配置：包含地形检测和环境因子更新间隔
  - **SCR_StaminaConstants.c**：
    - 新增 11 个配置桥接方法，从配置管理器动态获取参数：
      - GetEnergyToStaminaCoeff()
      - GetBaseRecoveryRate()
      - GetStandingRecoveryMultiplier()
      - GetProneRecoveryMultiplier()
      - GetLoadRecoveryPenaltyCoeff()
      - GetLoadRecoveryPenaltyExponent()
      - GetEncumbranceSpeedPenaltyCoeff()
      - GetEncumbranceStaminaDrainCoeff()
      - GetSprintStaminaDrainMultiplier()
      - GetFatigueAccumulationCoeff()
      - GetFatigueMaxFactor()
    - 更新常量定义注释，说明这些值现在从配置管理器获取
  - **SCR_RealisticStaminaSystem.c**：
    - 更新常量定义注释，说明部分常量现在从配置管理器获取
    - 更新所有使用这些常量的位置，改为从配置管理器动态获取
  - **SCR_StaminaConsumption.c**：
    - 更新 CalculateFatigueFactor() 方法，从配置管理器获取疲劳参数
  - **SCR_EncumbranceCache.c**：
    - 更新 UpdateCache() 方法，从配置管理器获取负重参数
  - **SCR_DebugDisplay.c**：
    - 更新 FormatSprintInfo() 方法，从配置管理器获取 Sprint 消耗倍数
  - **PlayerBase.c**：
    - 更新体力消耗计算，从配置管理器获取 Sprint 消耗倍数
  - **SCR_RSS_ConfigManager.c**：
    - 更新版本号到 v3.1.0
    - 在 Load() 方法中调用 InitPresets() 初始化预设默认值
    - 在 Load() 方法中添加工作台模式检测和预设名称日志输出
    - 在 ResetToDefaults() 方法中调用 InitPresets()
  - **工作台自动化**：
    - 工作台模式下自动使用 EliteStandard 预设
    - 工作台模式下自动开启调试日志
    - 输出工作台检测日志：`[RSS_ConfigManager] Workbench detected - Forcing EliteStandard model for verification.`

### 改进
- **配置系统用户体验优化**
  - 管理员可以通过修改 JSON 文件中的 `m_sSelectedPreset` 字段快速切换预设
  - 所有参数都有详细的中英双语注释，方便管理员理解参数作用
  - 预设参数包自动初始化，无需手动配置

### 技术亮点
- **预设驱动架构**：管理员只需修改一行 JSON 即可切换整个服务器的体能模型
- **Optuna 优化集成**：EliteStandard 和 TacticalAction 预设使用 Optuna 优化的参数
- **动态参数获取**：所有核心参数从配置管理器动态获取，支持热重载
- **中英双语注释**：所有参数都有详细的中英双语注释，提升国际化支持
- **工作台自动化**：工作台模式下自动使用 EliteStandard 预设并开启调试

### 代码统计
- **SCR_RSS_Settings.c**: 约 320 行（新增 11 个参数的详细注释）
- **SCR_StaminaConstants.c**: 新增 11 个配置桥接方法（约 120 行）
- **SCR_RSS_ConfigManager.c**: 更新版本号和预设初始化逻辑（约 10 行修改）
- **SCR_RealisticStaminaSystem.c**: 更新常量使用位置（约 5 行修改）
- **SCR_StaminaConsumption.c**: 更新疲劳计算方法（约 5 行修改）
- **SCR_EncumbranceCache.c**: 更新负重缓存方法（约 5 行修改）
- **SCR_DebugDisplay.c**: 更新 Sprint 信息格式化方法（约 5 行修改）
- **PlayerBase.c**: 更新体力消耗计算（约 5 行修改）

### 文档
- 所有参数都有详细的中英双语注释，方便管理员理解和使用

## [3.0.0] - 2026-01-20

### 新增
- **多目标优化系统（Multi-Objective Optimization System）**
  - **数字孪生仿真器（rss_digital_twin.py）**：
    - 完全复刻 EnforceScript 的体力系统逻辑
    - 支持所有核心特性：双稳态-应激性能模型、Pandolf 能量消耗模型、Givoni-Goldman 跑步模型、Santee 下坡修正模型
    - 支持深度生理压制恢复系统、累积疲劳和代谢适应系统
    - 支持游泳体力管理（3D物理模型）
    - 支持环境因子系统（热应激、降雨湿重、风阻、泥泞度、气温）
  - **标准测试工况库（rss_scenarios.py）**：
    - 13 个标准测试工况：ACFT 2英里测试、Everon拉练、火力突击、趴姿恢复测试、游泳体力测试、环境因子测试、长时间耐力测试等
    - 支持自定义测试工况
  - **多目标优化器**：
    - **Optuna 贝叶斯优化器（rss_optimizer_optuna.py）**：200 次采样，~47 秒优化时间，86 个帕累托前沿解
    - **NSGA-II 遗传算法优化器（rss_optimizer.py）**：5000 次采样，~10 分钟优化时间，50 个帕累托前沿解
    - 支持多目标优化：拟真度损失（Realism Loss）和可玩性负担（Playability Burden）
    - 自动生成 3 种配置方案：平衡型、拟真优先、可玩性优先
  - **可视化工具集（rss_visualization.py）**：
    - 帕累托前沿散点图：显示拟真度 vs 可玩性的权衡
    - 参数敏感度热力图：显示每个参数的变异系数
    - 收敛曲线图：显示优化过程中的目标函数变化
    - 参数分布图：显示帕累托前沿上参数的分布
    - 雷达图：对比不同配置方案的参数
  - **参数敏感性分析**：
    - **局部敏感性分析（rss_sensitivity_analysis.py）**：分析单个参数对目标函数的影响
    - **全局敏感性分析**：使用 Spearman 秩相关系数分析参数与目标函数的相关性
    - **联合敏感性分析（rss_sensitivity_analysis_enhanced.py）**：同时调整多个参数，展示参数之间的交互效应
  - **报告生成器**：
    - **HTML 报告（rss_report_generator.py）**：美观的交互式 HTML 格式报告
    - **Markdown 报告（rss_report_generator_english.py）**：标准 Markdown 格式报告
    - 包含优化结果、敏感度分析、配置对比
  - **完整优化流程脚本（run_complete_optimization.py）**：
    - 一键运行完整优化流程
    - 自动生成所有可视化图表和报告
  - **基础趋势图生成脚本**：
    - `simulate_stamina_system.py`：基础趋势图生成
    - `generate_comprehensive_trends.py`：综合趋势图生成（包含移动类型对比）
    - `multi_dimensional_analysis.py`：多维度趋势分析

### 改进
- **优化算法对比**：
  - Optuna 贝叶斯优化：200 次采样 vs NSGA-II 5000 次采样（25x 更快）
  - Optuna 解空间多样性更高（86 个帕累托前沿解 vs 50 个）
- **配置方案推荐**：
  - 硬核 Milsim 服务器：拟真优先配置
  - 公共服务器：平衡型配置
  - 休闲服务器：可玩性优先配置

### 技术亮点
- **科学性**：基于医学模型（Pandolf、Givoni-Goldman）
- **效率**：200 次采样 vs 5000 次评估（25x 更快）
- **可解释性**：参数敏感度分析
- **灵活性**：3 种优化配置方案
- **工业级**：使用 Optuna 贝叶斯优化

### 代码统计
- **rss_digital_twin.py**：数字孪生仿真器（约 800 行）
- **rss_optimizer.py**：NSGA-II 多目标优化器（约 300 行）
- **rss_optimizer_optuna.py**：Optuna 贝叶斯优化器（约 250 行）
- **rss_scenarios.py**：标准测试工况库（约 400 行）
- **rss_visualization.py**：可视化工具集（约 350 行）
- **rss_sensitivity_analysis.py**：参数敏感性分析工具（约 300 行）
- **rss_sensitivity_analysis_enhanced.py**：增强版敏感性分析工具（约 400 行）
- **rss_report_generator.py**：报告生成器（约 300 行）
- **rss_report_generator_english.py**：英文版报告生成器（约 300 行）
- **run_complete_optimization.py**：完整优化流程脚本（约 200 行）
- **simulate_stamina_system.py**：基础趋势图生成脚本（约 450 行）
- **generate_comprehensive_trends.py**：综合趋势图生成脚本（约 350 行）
- **multi_dimensional_analysis.py**：多维度趋势分析脚本（约 400 行）
- **stamina_constants.py**：Python 常量定义（约 150 行）
- **requirements.txt**：Python 依赖包（15 行）
- **README.md**：Tools 模块完整使用指南（约 680 行）
- **BUGFIX_SUMMARY.md**：问题修复总结（约 240 行）

### 文档
- 新增 [tools/README.md](tools/README.md)：Tools 模块完整使用指南
- 新增 [tools/BUGFIX_SUMMARY.md](tools/BUGFIX_SUMMARY.md)：问题修复总结
- 更新主 [README.md](README.md)：添加 Tools 模块说明

## [2.17.0] - 2026-01-20

### 新增
- **Python模拟器完整优化（Python Simulator Complete Optimization）**
  - **地形因素影响系统**：
    - 铺装路面（1.0x）、碎石路（1.1x）、高草丛（1.2x）、重度灌木丛（1.5x）、软沙地（1.8x）
    - 地形影响因子直接作用于体力消耗率
  - **环境因素影响系统**：
    - 温度影响：基于时间段的热应激计算（10:00-18:00，正午14:00达到峰值）
    - 风速影响：顺风减少消耗（最多10%），逆风增加消耗（最多5%）
    - 表面湿度影响：湿地趴下时的恢复惩罚（15%）
  - **动作成本计算系统**：
    - 跳跃消耗：3.5%体力（单次），连续跳跃50%惩罚（2秒窗口）
    - 攀爬消耗：1%/秒（持续攀爬）
  - **特殊运动模式系统**：
    - 游泳消耗：静态踩水（25W基础）+ 动态v³阻尼
    - 静态站立消耗：Pandolf静态项（1.5·W_body + 2.0·(W_body + L)·(L/W_body)²）
  - **高级修正模型系统**：
    - Santee下坡修正：陡峭下坡（>15%）的修正系数（0.5-1.0）
    - Givoni-Goldman跑步模型：高速跑步（>2.2 m/s）的能量消耗
  - **战斗负重系统**：
    - 战斗负重百分比计算（基于30kg阈值）
    - 战斗负重阈值判断（超过30kg触发）
  - **模拟器参数扩展**：
    - 新增地形类型参数（terrain_type）
    - 新增温度参数（temperature_celsius）
    - 新增风速参数（wind_speed、is_tailwind）
    - 新增表面湿度参数（surface_wetness）
    - 新增姿态参数（is_prone）
    - 新增时间参数（current_hour）
    - 新增室内参数（is_indoor）

### 改进
- **模拟器函数签名更新**：
  - `calculate_stamina_drain()`：新增8个环境参数
  - `simulate_stamina_system()`：新增6个环境参数
  - `plot_trends()`：新增6个环境参数
  - `calculate_temperature_factor()`：新增热应激时间计算
- **测试用例扩展**：
  - 示例1：Run模式，无负重，平地，铺装路面，正午
  - 示例2：Run模式，无负重，平地，软沙地，正午
  - 示例3：Run模式，无负重，平地，铺装路面，上午
- **测试结果验证**：
  - 正午+铺装路面：体力降至50%需要7.86分钟
  - 正午+软沙地：体力降至50%需要4.40分钟（地形影响显著）
  - 上午+铺装路面：体力降至50%需要10.10分钟（热应激影响显著）

### 代码统计
- **simulate_stamina_system.py**：新增12个函数（约450行）
  - `calculate_terrain_factor()` - 地形影响因子
  - `calculate_temperature_factor()` - 温度影响因子（含热应激）
  - `calculate_wind_factor()` - 风速影响因子
  - `calculate_surface_wetness_factor()` - 表面湿度影响因子
  - `calculate_jump_cost()` - 跳跃体力消耗
  - `calculate_climb_cost()` - 攀爬体力消耗
  - `calculate_static_standing_cost()` - 静态站立消耗
  - `calculate_swimming_drain()` - 游泳体力消耗
  - `calculate_santee_downhill_correction()` - Santee下坡修正
  - `calculate_givoni_goldman_running()` - Givoni-Goldman跑步模型
  - `calculate_combat_encumbrance_percent()` - 战斗负重百分比
  - `is_over_combat_encumbrance()` - 战斗负重判断

## [2.16.0] - 2026-01-20

### 新增
- **休息时间累积修复（Rest Time Accumulation Fix）**
  - 修复时间单位错误：`GetWorldTime()` 返回毫秒，需要除以1000转换为秒
  - 使用 idle 状态判断是否休息（静止时间 ≥ 1秒）
  - 避免速度波动导致休息时间频繁重置
- **虚拟气温曲线（Simulated Diurnal Temperature）**
  - 使用余弦函数模拟昼夜温度变化，峰值出现在 14:00
  - 清晨（05:00）：最低温 3°C
  - 正午（14:00）：最高温 27°C
  - 基于虚拟气温计算热应激，而非固定时间段
- **热应激阈值优化（Heat Stress Threshold）**
  - 将热应激挂钩到阈值（26°C），而非线性时间（10:00-18:00）
  - 只有当虚拟气温超过 26°C 时，才开始计算热应激
  - 倍数 = 1.0 + (虚拟气温 - 阈值) * 0.02
- **静态消耗优化（Static Consumption Optimization）**
  - 降低 `PANDOLF_STATIC_COEFF_1` 从 1.5 到 1.2（降低20%）
  - 降低 `PANDOLF_STATIC_COEFF_2` 从 2.0 到 1.6（降低20%）
  - 30KG站立总消耗从 0.000543/0.2s 降低到约 0.00035/0.2s
- **跳跃和攀爬冷却拦截（Jump/Vault Cooldown Interception）**
  - 跳跃冷却：2秒冷却时间，冷却期间拦截跳跃输入
  - 攀爬冷却：5秒冷却时间，冷却期间拦截攀爬输入
  - 通过设置 `m_bJumpInputTriggered = false` 阻止游戏引擎执行动作

### 改进
- **调试信息优化**
  - 更新环境因子调试信息，显示虚拟气温
  - 格式：`虚拟气温=22°C | 热应激=1.0x | 降雨湿重=0kg | 风速=6.4m/s`

### 代码统计
- **SCR_ExerciseTracking.c**: 修复时间单位错误（约10行）
- **SCR_EnvironmentFactor.c**: 新增虚拟气温计算和热应激阈值优化（约40行）
- **SCR_StaminaConstants.c**: 降低静态消耗系数（约5行）
- **SCR_JumpVaultDetection.c**: 添加冷却拦截逻辑（约20行）

## [2.15.0] - 2026-01-20

### 新增
- **配置系统（Config System）**
  - 使用 `[BaseContainerProps]` 和 `[Attribute]` 属性实现自动序列化
  - 使用官方的 `SCR_JsonLoadContext` 和 `SCR_JsonSaveContext`
  - 配置文件路径：`$profile:RealisticStaminaSystem.json`
  - 支持热重载（无需重启服务器）
- **调试日志门禁系统（Debug Log Gate）**
  - 零开销门禁：调试关闭时完全不执行字符串格式化
  - 时间间隔控制：防止日志刷屏
  - 工作台模式自动开启调试：`#ifdef WORKBENCH` 宏
- **配置管理器（SCR_RSS_ConfigManager）**
  - 单例模式，自动加载/保存配置
  - 内置配置验证，自动检测无效配置值
  - 支持配置热重载
- **配置类（SCR_RSS_Settings）**
  - 包含所有可配置参数（调试、体力、移动、环境、高级、性能）
  - 支持中英双语注释
  - 使用 `[Attribute]` 属性定义默认值和UI控件类型
- **常量桥接方法（SCR_StaminaConstants.c）**
  - 15个静态方法用于获取配置值
  - 工作台模式自动开启调试

### 改进
- **性能优化**
  - 配置只加载一次，避免重复IO
  - 调试关闭时零开销
  - 使用配置中的刷新间隔控制日志频率
- **API兼容性**
  - 使用 `Replication.IsServer()` 判断服务器端
  - 移除 `FileIO` 使用，直接使用JsonLoadContext/JsonSaveContext

### 代码统计
- **SCR_RSS_Settings.c**: 新建配置类（约80行）
- **SCR_RSS_ConfigManager.c**: 新建配置管理器（约160行）
- **SCR_StaminaConstants.c**: 新增15个配置桥接方法（约120行）
- **SCR_DebugDisplay.c**: 集成配置门禁系统（约20行）
- **PlayerBase.c**: 集成配置初始化（约15行）
- **RealisticStaminaSystem.json**: 新建示例配置文件（约20行）

## [2.14.1] - 2026-01-20

### 新增
- **室内检测系统（Indoor Detection System）**
  - 使用建筑物边界框检测角色是否在室内
  - 检测逻辑：检查角色位置是否在建筑物的世界坐标边界框内（X、Y、Z 三个坐标都在范围内）
  - 局限性说明：
    - 边界框可能包含建筑物周围的空间，导致角色在建筑物附近室外时被误判为室内
    - 无法区分"在边界框内"和"真正在建筑物内部"
    - 简单的坐标比较无法检测建筑物的实际内部空间
    - 对于复杂建筑结构（如多层建筑、开放空间等），检测精度有限
  - 调试信息：输出中英双语调试信息，包括角色位置、建筑物边界框、室内/室外判定等
  - 性能：使用边界框检测性能优秀，每秒检测一次
  
## [2.14.0] - 2026-01-20

### 新增
- **高级环境因子系统（Advanced Environmental Factors System）**
  - **精确降雨强度系统**：使用 `GetRainIntensity()` API 替代字符串匹配
    - 降雨强度范围：0.0-1.0（0%-100%）
    - 湿重计算：`accumulationRate = 0.5 × intensity^1.5`
    - 暴雨呼吸阻力：强度 > 80% 时触发，增加无氧代谢消耗
  - **风阻模型**：基于风速和风向计算逆风阻力
    - 风速：使用 `GetWindSpeed()` API 获取（m/s）
    - 风向：使用 `GetWindDirection()` API 获取（度）
    - 风阻系数：`windDrag = 0.05 × windSpeed × cos(angle)`
    - 顺风时减少消耗，逆风时增加消耗
  - **路面泥泞度系统**：基于积水程度计算泥泞惩罚
    - 使用 `GetCurrentWaterAccumulationPuddles()` API 获取积水程度
    - 泥泞地形系数：最大40%地形阻力增加
    - 泥泞Sprint惩罚：只在Sprint时应用（速度 ≥ 5.2 m/s）
    - 滑倒风险：基于泥泞度计算（每0.2秒滑倒概率）
  - **实时气温热应激模型**：使用 `GetTemperatureAirMinOverride()` API
    - 热应激阈值：30°C（高于此值恢复率降低2%/度）
    - 冷应激阈值：0°C（低于此值恢复率降低5%/度，静态消耗增加3%/度）
  - **地表湿度和静态恢复惩罚**：趴下时受地表湿度影响
    - 使用 `GetCurrentWetness()` API 获取地表湿度（0.0-1.0）
    - 只在趴姿（stance==2）时计算地表湿度惩罚
    - 湿地趴下时恢复率降低15%
  - **SCR_EnvironmentFactor.c** - 高级环境因子模块（约1250行）
    - 33个高级环境因子常量定义
    - 15个环境因子状态变量
    - 15个环境因子getter方法
    - 12个环境因子计算方法
    - 完整的中英双语调试信息输出

### 改进
- **站姿恢复倍数调整**：从0.4提升到2.0
  - 确保静态站立时能够恢复体力（净恢复约0.53%/秒）
  - 解决复活后站着不动掉体力的问题
- **环境惩罚应用条件优化**
  - 泥泞Sprint惩罚：只在Sprint时应用（速度 ≥ 5.2 m/s）
  - 地表湿度惩罚：只在趴姿时应用（stance==2）
  - 避免在Run状态时错误应用环境惩罚

### 修复
- **滑倒风险显示精度**：从2位小数提升到4位小数
  - 修改前：0.001 → 0.00（显示为0）
  - 修改后：0.001 → 0.001（正确显示）
- **暴雨呼吸阻力显示精度**：从2位小数提升到4位小数

### 已知问题
- **室内外检测无效**：当前室内外检测系统无法正常工作
  - 原因：射线检测方法可能存在兼容性问题
  - 影响：热应激室内豁免功能无法生效
  - 状态：正在寻找解决方案

### 代码统计
- **SCR_StaminaConstants.c**: 新增高级环境因子常量（33行）
- **SCR_EnvironmentFactor.c**: 实现高级环境因子系统（约1250行）
- **SCR_StaminaConsumption.c**: 集成环境因子到消耗计算（约30行）
- **SCR_StaminaRecovery.c**: 集成环境因子到恢复计算（约30行）
- **SCR_StaminaUpdateCoordinator.c**: 传递环境因子模块引用（约10行）
- **PlayerBase.c**: 传递姿态参数到环境因子更新（约30行）

## [2.13.0] - 2026-01-19

### 新增
- **深度生理压制恢复系统（Deep Physiological Suppression Recovery System）**
  - 核心概念：从"净增加"改为"代谢净值"
  - 最终恢复率 = (基础恢复率 × 姿态修正) - (负重压制 + 氧债惩罚)
  
  - **呼吸困顿期（RECOVERY_COOLDOWN）**
    - 停止运动后5秒内系统完全不处理恢复
    - 医学依据：剧烈运动停止后的前10-15秒，身体处于摄氧量极度不足状态（Oxygen Deficit）
    - 游戏目的：消除"跑两步停一下瞬间回血"的游击战式打法
  
  - **负重对恢复的静态剥夺机制（LOAD_RECOVERY_PENALTY）**
    - 惩罚公式：Penalty = (当前总重 / 身体耐受基准)^2 × 0.0004
    - 测试结果：
      - 0kg负重：站姿恢复4.2分钟
      - 30kg负重：站姿恢复20.0分钟（几乎不恢复），趴姿恢复0.6分钟
      - 40kg负重：站姿恢复20.0分钟（几乎不恢复），趴姿恢复0.7分钟
    - 战术意图：强迫重装兵必须趴下（通过姿态加成抵消负重扣除）
  
  - **边际效应衰减机制（MARGINAL_DECAY）**
    - 当体力>80%时，恢复率 = 原始恢复率 × (1.1 - 当前体力百分比)
    - 测试结果：
      - 体力80%以下：恢复率0.86-1.13%/秒
      - 体力85%以上：恢复率0.16-0.21%/秒（约原来的20-25%）
    - 战术意图：玩家经常会处于80%-90%的"亚健康"状态
  
  - **最低体力阈值限制（MIN_RECOVERY_STAMINA_THRESHOLD）**
    - 体力<20%时，必须静止10秒后才允许开始回血
    - 测试结果：体力<20%且休息<10秒时，恢复率为0
    - 战术意图：防止玩家在极度疲劳时通过"跑两步停一下"快速回血

### 改进
- **恢复系统参数全面调整**
  - BASE_RECOVERY_RATE：0.0005 → 0.0003（基础恢复时间从6.6分钟延长到11分钟）
  - RECOVERY_STARTUP_DELAY_SECONDS：1.5 → 5.0（呼吸困顿期从1.5秒延长到5秒）
  - STANDING_RECOVERY_MULTIPLIER：1.0 → 0.4（站姿恢复效率从100%降到40%）
  - CROUCHING_RECOVERY_MULTIPLIER：1.3 → 1.5（蹲姿恢复效率从130%提升到150%）
  - PRONE_RECOVERY_MULTIPLIER：1.7 → 2.2（趴姿恢复效率从170%提升到220%）

### 技术改进
- 实现呼吸困顿期逻辑（最低体力阈值限制）
- 实现负重对恢复的静态剥夺机制（平方惩罚模型）
- 实现边际效应衰减机制（体力>80%时恢复速度显著降低）
- 实现最低体力阈值限制（体力<20%时需要10秒休息）
- 更新Python趋势图脚本以反映新的恢复参数
- 创建深度生理压制恢复系统测试脚本

### 代码统计
- **SCR_StaminaConstants.c**: 新增深度生理压制参数（26行）
- **SCR_RealisticStaminaSystem.c**: 实现四大机制的数学逻辑（约60行）
- **stamina_constants.py**: 更新Python常量定义（26行）
- **generate_comprehensive_trends.py**: 更新恢复计算函数（约30行）
- **test_deep_physiological_suppression.py**: 新建测试脚本（280行）

## [2.12.0] - 2026-01-19

### 新增
- **全面调试信息系统（Comprehensive Debug Information System）**
  - 为所有关键功能模块添加了中英双语调试信息输出
  - **载具检测调试**：显示载具中状态和体力恢复情况
  - **速度计算调试**：每5秒输出当前速度信息
  - **精疲力尽状态调试**：进入/脱离状态时输出提示
  - **跳跃/翻越调试**：显示体力消耗信息
  - **体力消耗参数调试**：显示类型、速度、负重、坡度、地形系数、姿态、热应激、效率、疲劳、Sprint倍数等完整参数
  - **体力恢复调试**：显示恢复前后状态、恢复率、休息时间、热应激影响
  - **EPOC延迟调试**：显示EPOC状态变化
  - **室内检测调试**：显示室内/室外状态、命中点数量、命中率
  - **环境因子调试**：显示热应激倍数、降雨湿重、降雨状态变化
  - **原生系统干扰检测**：检测并警告原生体力系统的干扰

### 改进
- **调试信息输出频率优化**
  - 大部分调试信息限制为每5秒输出一次，避免日志过多
  - 状态变化（如精疲力尽、EPOC、降雨等）时立即输出
  - 平衡了调试信息的完整性和日志的可读性
- **调试显示模块扩展**
  - `SCR_DebugDisplay.c` 模块所有格式化函数支持中英双语
  - 坡度信息、Sprint信息、状态信息等统一使用中英双语格式
  - 提升国际化支持水平

### 技术改进
- 实现全面的调试信息输出系统
- 统一调试信息格式（中英双语）
- 优化调试信息输出频率机制
- 增强系统可观测性和问题排查能力

## [2.11.0] - 2026-01-19

### 新增
- **进一步模块化重构（Advanced Modular Refactoring）**
  - **SCR_SwimmingState.c** - 游泳状态管理模块（121行）
    - 游泳状态检测逻辑
    - 湿重跟踪和计算逻辑
    - 总湿重计算逻辑
  - **SCR_StaminaUpdateCoordinator.c** - 体力更新协调器模块（333行）
    - 速度更新协调逻辑
    - 位置差分测速逻辑
    - 基础消耗率计算（游泳/陆地）
    - 体力更新协调（消耗/恢复）

### 改进
- **代码精简优化**
  - PlayerBase.c: 从 1362 行减少到 817 行（减少 545 行，约 40%）
  - UpdateSpeedBasedOnStamina() 函数从约 946 行精简到约 700 行
  - 大幅提升代码可读性和可维护性
- **调试信息输出优化**
  - 扩展 SCR_DebugDisplay.c 模块（从 145 行扩展到 352 行）
  - 新增 `FormatEnvironmentInfo()` 方法：统一格式化环境因子信息
  - 新增 `OutputDebugInfo()` 方法：统一调试信息输出接口（使用参数结构体，解决参数数量限制）
  - 新增 `OutputStatusInfo()` 方法：统一状态信息输出接口
  - 所有调试信息格式化逻辑集中在 DebugDisplay 模块
- **工具趋势图输出双语化**
  - `tools/` 下生成的趋势图（`stamina_system_trends.png` / `comprehensive_stamina_trends.png` / `multi_dimensional_analysis.png`）统一为中英双语标注
- **坡度检测模块化**
  - 扩展 SCR_SpeedCalculation.c 模块
  - 新增 `CalculateGradePercent()` 方法：统一坡度检测和计算逻辑
  - 考虑攀爬和跳跃状态的坡度检测

### 技术改进
- 实现游泳状态管理模块化架构（SwimmingStateManager类）
- 实现体力更新协调器模块化架构（StaminaUpdateCoordinator类）
- 实现调试信息统一管理（DebugDisplay类扩展）
- 优化代码组织，每个模块职责单一，符合单一职责原则
- 提高代码复用性和可测试性

### 代码统计
- **PlayerBase.c**: 1362行 → 817行（减少40%）
- **SCR_SwimmingState.c**: 新建（121行）
- **SCR_StaminaUpdateCoordinator.c**: 新建（333行）
- **SCR_DebugDisplay.c**: 145行 → 352行（扩展）
- **SCR_SpeedCalculation.c**: 128行 → 163行（扩展）

## [2.10.0] - 2026-01-19

### 新增
- **环境因子系统（Environmental Stress System）**
  - **热应激系统（Heat Stress）**：基于时间段的热应激影响
    - 10:00-14:00 逐渐增加，14:00-18:00 逐渐减少
    - 峰值（14:00）时体力消耗增加30%
    - 室内豁免：在室内（头顶有遮蔽物）时热应激减少50%
    - 热应激不仅影响体力消耗，还影响恢复速度（高温让人休息不回来）
  - **降雨湿重系统（Rain Weight）**：下雨时衣服和背包增重
    - 小雨：2kg，中雨：5kg，暴雨：8kg
    - 停止降雨后湿重逐渐衰减（60秒内完全消失，使用二次方衰减模拟自然蒸发）
    - 湿重饱和上限：总湿重（游泳+降雨）不超过10kg，防止数值爆炸
    - 加权平均：游泳湿重权重0.6，降雨湿重权重0.4（更真实的物理模型）
  - **室内检测系统**：向上射线检测10米，判断角色是否在室内
    - 用于热应激室内豁免
    - 每5秒检测一次（与环境因子更新同步）
  - **SCR_EnvironmentFactor.c** - 环境因子模块（355行）
    - 环境因子检测和计算
    - 热应激倍数计算（考虑室内豁免）
    - 降雨湿重计算（基于天气状态名称）
    - 防御性编程：自动重新获取丢失的天气管理器

### 改进
- **代码精简优化**
  - 删除服务器端验证相关代码（永远不会执行）
    - 删除 `CalculateServerSpeedMultiplier` 方法
    - 删除 `ValidateStaminaState` 方法
    - 删除所有 RPC 方法（`RPC_UpdateStaminaState`、`RPC_SyncStaminaState`）
    - 删除网络同步 RPC 调用代码
    - 删除速度插值平滑处理代码
  - 删除无用的兼容性代码
    - 删除手动查找体力组件的备用代码
    - 删除如果没有体力组件引用时使用 `GetStamina()` 的备用代码
  - 删除无用的模块引用（`NetworkSyncManager`）
  - 简化客户端检查逻辑（直接检查是否为本地控制的玩家）
  - **代码减少约200+行**，提高可读性和维护性
- **调试信息增强**
  - 在每5秒的调试输出中新增完整的环境因子信息
    - 当前时间（格式：HH:MM）
    - 热应激倍数（1.0-1.3x）
    - 降雨信息（降雨类型、湿重、强度）
    - 室内/室外状态
    - 游泳湿重
  - 环境因子模块新增调试方法
    - `GetCurrentHour()` - 获取当前时间（小时）
    - `IsIndoor()` - 检查是否在室内
    - `GetRainIntensity()` - 获取降雨强度
    - `IsRaining()` - 检查是否正在下雨

### 技术改进
- 实现环境因子模块化架构（EnvironmentFactor类）
- 实现热应激时间段计算（10:00-18:00）
- 实现室内检测（向上射线追踪）
- 实现降雨湿重计算（基于天气状态名称）
- 实现湿重饱和上限机制（防止叠加导致数值爆炸）
- 实现二次方衰减（模拟自然蒸发过程）
- 优化环境因子检测频率（每5秒更新一次）

### 修复
- 修复 EnforceScript 三元运算符语法错误（改为 if-else 语句）
- 修复 `rainWeight` 变量定义位置问题（移到了使用之前）

## [2.9.0]

### 新增
- **深度模块化重构（Deep Modular Refactoring）**
  - 新增4个核心计算模块，进一步拆分复杂逻辑
  - **SCR_SpeedCalculation.c** - 速度计算模块（129行）
    - 基础速度倍数计算
    - 坡度自适应目标速度计算
    - 最终速度倍数计算（根据移动类型）
  - **SCR_StaminaConsumption.c** - 体力消耗计算模块（187行）
    - Pandolf模型计算
    - 姿态修正计算
    - 代谢适应效率因子计算
    - 健康状态效率因子计算
    - 综合体力消耗率计算
  - **SCR_StaminaRecovery.c** - 体力恢复计算模块（122行）
    - EPOC延迟状态管理
    - EPOC消耗计算
    - 多维度恢复率计算
    - 恢复重量计算（考虑姿态优化）
  - **SCR_DebugDisplay.c** - 调试信息显示模块（141行）
    - 移动类型格式化
    - 坡度信息格式化
    - Sprint信息格式化
    - 负重信息格式化
    - 地形信息格式化
    - 调试消息构建

### 改进
- **代码结构大幅优化**
  - PlayerBase.c: 1554行 → 1283行（减少271行，约17%）
  - UpdateSpeedBasedOnStamina函数从990行减少到更易管理的长度
  - 主函数逻辑更清晰，可读性显著提升
- **模块化架构完善**
  - 所有复杂计算逻辑已提取到独立模块
  - 每个模块职责单一，符合单一职责原则
  - 代码更易于维护和测试
- **游戏工作台兼容性**
  - 解决了游戏工作台对多行代码支持较差的问题
  - 每个模块文件大小适中，便于编辑和查看
  - 代码结构更适合Arma Reforger工作台环境

### 技术改进
- 实现深度模块化架构，提高代码可维护性
- 提取速度计算逻辑到独立模块（SpeedCalculation）
- 提取体力消耗计算逻辑到独立模块（StaminaConsumption）
- 提取体力恢复计算逻辑到独立模块（StaminaRecovery）
- 提取调试显示逻辑到独立模块（DebugDisplay）
- 优化代码组织，每个模块专注于单一功能领域

### 修复
- 修复EnforceScript语法兼容性问题（三元运算符改为if-else语句）
- 修复模块间参数传递问题（使用ref参数替代out参数）

## [2.9.0]

### 新增
- **游泳体力管理（3D物理模型）**
  - 游泳时使用独立消耗模型（水平阻力 + 垂直上浮/下潜功率 + 静态踩水功率）
  - 引入负重阈值与非线性惩罚、湿重效应等参数（用于更拟真的水中负浮力体验）

### 改进
- **游泳速度检测可靠性**
  - 统一改为使用 `GetOrigin()` 的位置差分测速（避免游泳命令通过 `PrePhys_SetTranslation` 位移导致 `GetVelocity()` 为0）
  - 游泳消耗计算与每秒速度显示共用同一套差分速度输入，避免“显示速度=0但实际在动”的错觉

### 修复
- 修复EnforceScript `Math.Round()` 仅支持单参数导致的编译错误（调试输出保留两位小数改为手动缩放）

## [2.7.0] 

### 新增
- **代码模块化重构（Modular Refactoring）**
  - 创建10个模块化组件，提高代码可维护性
  - **SCR_CollapseTransition.c** - "撞墙"阻尼过渡模块
  - **SCR_StaminaConstants.c** - 常量定义模块（262行）
  - **SCR_StaminaHelpers.c** - 辅助函数模块（94行）
  - 所有常量定义已提取到独立模块
  - 所有辅助函数已提取到独立模块

### 改进
- **代码结构优化**
  - PlayerBase.c: 2037行 → 1464行（减少573行，约28%）
  - SCR_RealisticStaminaSystem.c: 1483行 → 1144行（减少339行，约23%）
  - 代码结构更清晰，模块职责单一
- **模块化组件集成**
  - 所有模块化组件已成功集成到主控制器
  - 保持向后兼容，所有公共API不变
  - 代码编译通过，功能完整

### 技术改进
- 实现模块化架构，提高代码可维护性
- 提取常量定义到独立模块（StaminaConstants）
- 提取辅助函数到独立模块（StaminaHelpers）
- 优化代码组织，符合单一职责原则

## [2.6.0] 

### 新增
- **趴下休息时的负重优化（Prone Rest Weight Reduction）**
  - 当角色趴下休息时，负重影响降至最低（地面支撑装备重量）
  - 重装兵趴下时能够快速恢复体力，提供合理的战术选择
  - 使用 `ECharacterStance.PRONE` 检测姿态（通过 `GetStance()` 方法）
- **跳跃冷却机制**
  - 添加3秒冷却时间（15个更新周期），防止重复触发
  - 与翻越冷却机制保持一致的设计模式
- **连续跳跃惩罚机制（无氧欠债）**
  - 检测3秒内的连续跳跃，每次额外增加50%消耗
  - 第一次跳：3.5%，第二次跳：5.25%，第三次跳：7.0%
  - 有效防止"兔子跳"战术滥用
- **低体力禁用跳跃**
  - 体力 < 10% 时禁用跳跃（肌肉在力竭时无法提供爆发力）
  - 符合生理学特性

### 改进
- **Sprint 速度系统优化（统一增量模型）**
  - Sprint 速度增量从 15% 提升到 30%，确保负重状态下仍有明显差距
  - Sprint 计算完全基于 Run 的完整逻辑（双稳态-平台期、5秒阻尼过渡等）
  - Sprint 负重惩罚系数从 20% 降为 15%（模拟爆发力克服阻力）
  - 重构 Sprint 计算逻辑，确保始终比 Run 快 30% 的固定阶梯
  - **效果**：28KG 负重下，Run 3.6 m/s vs Sprint 4.7 m/s（差距 1.1 m/s）
- **Sprint 体力消耗优化**
  - Sprint 消耗倍数从 2.5x 提升到 3.0x，平衡速度提升带来的优势
  - 确保玩家不能长时间使用 Sprint，只能作为战术爆发
- **跳跃和翻越消耗优化（动态负重倍率）**
  - 跳跃基础消耗从 3% 提升到 3.5%
  - 翻越起始消耗从 1.5% 提升到 2%
  - 持续攀爬消耗：每秒 1%（每 0.2 秒 0.002）
  - 使用动态负重倍率：`实际消耗 = 基础消耗 × (currentWeight / 90.0) ^ 1.5`
  - 30KG 负重时，跳跃消耗约 4.6%，翻越起始消耗约 2.7%
- **跳跃和翻越 UI 交互增强**
  - 跳跃后更新 Exhaustion 信号（基础 0.1 + 负重加成，最多 0.2）
  - 触发深呼吸音效和视觉模糊效果，增强感官反馈

### 修复
- **Sprint 和 Run 速度差异不明显**
  - 修复 Sprint 独立计算导致的逻辑脱节
  - Sprint 现在完全基于 Run 的完整逻辑进行加乘
  - 确保在任何负重和体力状态下，Sprint 都比 Run 快 30%

### 技术改进
- 重构 Sprint 速度计算逻辑（统一增量模型）
- 实现动态负重倍率计算函数 `CalculateActionCost()`
- 实现连续跳跃惩罚机制（无氧欠债）
- 实现低体力禁用跳跃机制
- 优化跳跃和翻越的体力消耗计算（基于总重量而非额外负重）

## [2.5.0]

### 新增
- **地形系数系统 (Terrain Factor η)**
  - 集成地形系数到 Pandolf 模型，根据地面类型动态调整消耗
  - 地形系数范围：铺装路面 1.0 → 深雪 2.1-3.0
  - 基于地面密度检测实现地形类型识别
  - 性能优化：静止时降低检测频率至 2 秒
- **静态负重站立消耗 (Static Standing Cost)**
  - 集成 Pandolf 静态项公式，负重下站立时减缓恢复速度
  - 空载时每秒恢复 1%，负重 40kg 时每秒仅恢复 0.4%
  - 模拟现实中背负装备站立时的静态等长收缩
- **Santee 下坡修正模型**
  - 精确处理下坡时的体力消耗（超过 -15% 时需"刹车"）
  - 修正系数：`μ = 1.0 - [G·(1 - G/15)/2]`
  - 缓下坡（-5%）消耗减少约 2-3%，陡下坡（-15%+）消耗回升
- **Givoni-Goldman 跑步模式切换**
  - 速度 >2.2 m/s 时自动切换到跑步能量消耗模型
  - 跑步效率比步行低，消耗随速度平方剧增
- **恢复启动延迟机制**
  - 负重下恢复速率提升需静止 3 秒后才生效
  - 防止"短促冲刺+立即静止"循环滥用
  - 模拟心率下降和呼吸调整的生理过程
- **网络同步容差优化**
  - 连续偏差累计触发机制（2 秒容差），避免地形 LOD 差异导致的频繁拉回
  - 速度插值平滑处理（0.5 秒过渡），减少服务器端同步时的抖动
- **生理上限溢出处理（疲劳积累系统）**
  - 超出最大消耗的体力转化为疲劳积累
  - 疲劳降低最大体力上限（最多 30%）
  - 需要长时间休息（60 秒）才能开始恢复疲劳
- **UI 信号桥接系统（官方 UI 特效集成）**
  - 通过 SignalsManagerComponent 手动更新 "Exhaustion" 信号
  - 让官方 UI 特效（视觉模糊、呼吸声）响应自定义体力系统状态
  - 智能映射：基础模糊（体力剩余量）+ 瞬时模糊（运动强度）+ 崩溃期强化（体力 < 25%）
  - 当体力掉到 25% 时，信号值超过 0.45，立即触发视觉模糊效果
  - 完美衔接拟真模型的崩溃点（25%）和官方 UI 特效阈值（45%）

### 改进
- **性能优化**：地形检测频率根据移动状态动态调整
- **代码结构**：修复编译错误，优化代码组织

### 技术改进
- 实现地形系数查询和缓存机制
- 实现静态消耗计算（仅在速度接近 0 时）
- 实现 Santee 下坡修正（二次函数代替线性判定）
- 实现 Givoni-Goldman 跑步模型切换
- 实现恢复延迟跟踪机制
- 实现网络同步偏差累计系统
- 实现疲劳积累和恢复系统
- 实现 UI 信号桥接系统（SignalsManagerComponent 集成）

## [2.2.0]

### 新增
- **双稳态-应激性能模型（Dual-State Stress Performance Model）**
  - 平台期（25%-100%体力）：保持恒定目标速度（3.7 m/s），模拟意志力克服早期疲劳
  - 衰减期（0%-25%体力）：速度平滑下降到跛行速度，使用SmoothStep过渡（25%-5%缓冲区）
- **5秒阻尼过渡（"撞墙"瞬间优化）**
  - 在体力触及25%临界点时，使用5秒时间阻尼过渡，让玩家感觉"腿越来越重"
  - 避免速度瞬间跳变（"引擎突然断油"），提供平滑的疲劳体验
  - 使用SmoothStep函数实现渐进式速度下降
- **坡度自适应步幅逻辑（坡度-速度负反馈）**
  - 上坡时自动降低目标速度（每度降低2.5%），换取更持久的续航
  - 模拟现实中人爬坡时自动缩短步幅的行为
- **坡度消耗非线性增长（幂函数）**
  - 使用 `(gradePercent × 0.01)^1.2 × 5.0` 替代线性增长
  - 小坡（5%以下）几乎无感，陡坡才真正吃力，避免Everon缓坡频繁断气
- **体力消耗生理上限（VO2 Max限制）**
  - 每0.2秒最大消耗不超过0.02（每秒最多掉10%）
  - 防止负重+坡度组合导致的数值爆炸

### 改进
- **平滑过渡优化**：将缓冲区间从10%-25%扩展至5%-25%，避免突兀的"撞墙"效果
- **"撞墙"瞬间优化**：在体力触及25%临界点时，添加5秒阻尼过渡机制
  - 使用时间插值（SmoothStep）实现渐进式速度下降
  - 让玩家感觉"腿越来越重"，而不是"引擎突然断油"
  - 速度在5秒内从3.7 m/s逐渐降到约2.2 m/s
- **负重影响优化**：负重主要影响"油耗"（体力消耗）而非直接降低"最高档位"（速度）
  - 负重对速度的影响从50%降低到20%
  - 30kg负重时仍能短时间跑3.7 m/s，只是消耗更快
- **重载下恢复优化**：负重时恢复速率提升（0-30kg：10%提升，30-40.5kg：30%提升）
  - 模拟士兵通过深呼吸快速调整的能力
  - 避免重装冲刺后的"跛行"惩罚太久

### 技术改进
- 实现SmoothStep函数（三阶多项式插值）用于平滑过渡
- 5秒阻尼过渡机制：时间跟踪变量（`m_bInCollapseTransition`, `m_fCollapseTransitionStartTime`）
- 坡度自适应函数：`CalculateSlopeAdjustedTargetSpeed()`
- 非线性坡度消耗：`CalculateGradeMultiplier()` 使用幂函数
- 生理上限限制：在体力消耗计算后应用 `MAX_DRAIN_RATE_PER_TICK`

## [2.1.0]

### 新增
- 参数优化功能，达到2英里15分27秒目标
- 精确数学模型实现（α=0.6，Pandolf模型）
- 完整的原生体力系统覆盖机制
- 高频监控系统（50ms间隔）

### 改进
- 速度倍数从0.885提升至0.920（满体力速度4.78 m/s）
- 速度平方项系数从0.0003降低至0.0001
- 使用目标体力值而非当前体力值进行计算，避免原生系统干扰
- 优化监控频率，平衡性能和拦截效果

### 修复
- 修复原生体力系统干扰问题
- 修复体力恢复速度异常问题
- 修复监控机制中的逻辑问题

### 技术改进
- 实现精确的幂函数计算（牛顿法）
- 完整的 Pandolf 模型实现
- 改进的拦截机制（OnStaminaDrain事件覆盖 + 主动监控）

## [2.0.0] 

### 新增
- 动态速度调整系统（根据体力百分比）
- 负重影响系统（负重影响速度）
- 精确数学模型（不使用近似）
- 综合状态显示（速度、体力、倍率）
- 平滑速度过渡机制

### 技术实现
- 使用 `modded class SCR_CharacterStaminaComponent` 扩展体力组件
- 使用 `modded class SCR_CharacterControllerComponent` 显示状态信息
- 通过 `OverrideMaxSpeed(fraction)` 动态设置最大速度倍数

## [1.0.0]

### 新增
- 初始版本
- 固定速度修改功能（50%）
- 速度显示功能（每秒一次）
