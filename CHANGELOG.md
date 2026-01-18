# 更新日志

所有重要的项目变更都会记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
并且本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [2.1.0] - 2024

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

## [2.0.0] - 2024

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

## [1.0.0] - 2024

### 新增
- 初始版本
- 固定速度修改功能（50%）
- 速度显示功能（每秒一次）

[2.1.0]: https://github.com/ViVi141/test_rubn_speed/releases/tag/v2.1.0
[2.0.0]: https://github.com/ViVi141/test_rubn_speed/releases/tag/v2.0.0
[1.0.0]: https://github.com/ViVi141/test_rubn_speed/releases/tag/v1.0.0
