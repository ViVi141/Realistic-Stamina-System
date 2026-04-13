# Scripts 命名与布局规范

## 文件命名
- 入口 `modded` 文件保持现有稳定命名，不在大版本内随意改名。
- 新增工具类统一使用 `SCR_` 前缀，文件名与主类名一致。
- Helper/Utils 文件以职责结尾：
  - `*Utils.c`：轻量编排或状态判定
  - `*Math.c`：纯计算
  - `*Api.c`：引擎 API 读取封装

## 代码分层
- `Integration`：仅保留引擎入口与 RPC 壳。
- `Core`：体力主流程与协调器。
- `Environment`：天气、地形、温度、惩罚。
- `NetworkConfig`：配置模型、同步、迁移。
- `AI`：AI 体力与行为桥接。
- `Presentation`：HUD、调试、UI 信号、相机表现。

## 约束
- 不在大文件里继续堆叠纯逻辑函数。
- 新增逻辑优先放到领域 helper，再由入口层委托调用。
- 新增 public 方法必须补中文注释，说明输入、输出、边界值。

## 重构提交建议
- 每次提交只做一个主题：
  - `PlayerBase` 拆分
  - `Environment` 拆分
  - `NetworkConfig` 拆分
  - `AI/Presentation` 拆分
- 每次提交后执行一次最小回归：
  - 编译
  - 单机冲刺/恢复
  - 配置同步
