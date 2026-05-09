# Realistic Stamina System - Tools 工具集

## 📋 目录概览

本目录包含 Realistic Stamina System (RSS) 模组的优化工具链和配置文件。

### 核心文件结构

```
tools/
├── README.md                                          # 📖 本文件
├── rss_pipeline_v4.py                                 # 🎯 主优化管道（v4: 8场景 + 3目标）
├── rss_digital_twin_fix.py                            # 🔬 数字孪生仿真器
├── stamina_constants.py                               # 📋 常数定义工具库
├── test_v4_smoke.py                                   # 🧪 v4 烟雾测试
├── test_basic_fitness.py                              # 🧪 基础体能验证
├── quick_verify.py                                    # 🧪 快速验证
├── embed_json_to_c.py                                 # 📄 JSON → C 代码嵌入
├── requirements.txt                                   # 📦 Python 依赖列表
├── rss_optimizer_gui.py                               # 🖥️ 优化器 GUI（可选）
├── optimized_rss_config_elitestandard_v4.json         # 📊 精英标准预设
├── optimized_rss_config_standardmilsim_v4.json        # 📊 标准军事模拟预设
└── optimized_rss_config_tacticalaction_v4.json        # 📊 战术行动预设
```

---

## 🚀 快速开始

### 1. 安装依赖

```bash
cd tools
pip install -r requirements.txt
```

### 2. 运行优化管道

```bash
python rss_pipeline_v4.py --trials 300 --jobs 4
```

这将执行：
- ✅ 300 次 Optuna 多目标试验（可配置 `--trials` 和 `--jobs`）
- ✅ 3 目标：combat_endurance、recovery_efficiency、parameter_realism
- ✅ 8 个真实 Arma 多阶段战斗场景（含环境压力：高温/低温/风雨）
- ✅ 16 参数搜索空间（与 C 端 SCR_RSS_Params 对齐）
- ✅ 3 个 Pareto 最优预设按设计哲学自动选择
- ✅ 自动保存为 JSON 配置文件

**预计耗时**: 300 次约 15–30 分钟（4 线程）

### 3. 在游戏中应用配置

生成的 JSON 文件会自动保存，游戏启动时通过以下路径加载：

```
$profile:RealisticStaminaSystem.json
```

---

## 📄 文件详解

### 🎯 rss_pipeline_v4.py（主优化管道）

**用途**: 执行完整的 NSGA-II 多目标优化（v4 重新设计）

**v4 对比旧 super_pipeline 的改进**:
- 场景体系：4→8 个真实 Arma 多阶段战斗任务（含环境压力）
- 指标：20+ 惩罚系数 → 3 个结果指标（combat_endurance / recovery_efficiency / parameter_realism）
- 预设选择：按设计哲学选点（非单目标排序）
- 搜索空间：41→16 参数（仅调校与 C 端 getter 对齐的核心参数）

**优化目标**:
1. 最小化 `combat_endurance` → TacticalAction（最宽容）
2. 最小化 `parameter_realism` → EliteStandard（最贴近 C 参考值）
3. 均衡 → StandardMilsim（归一化距原点最近）

**8 个战斗场景**:
- 巡逻接敌 (30kg/20°C) | 沙漠巡逻 (30kg/35°C) | 地狱沙漠突围 (30kg/35°C/多冲刺)
- 两栖登陆 (25kg/3m/s风) | 城镇清扫 (25kg) | 山地接近 (35kg)
- 载具突击 (20kg) | 重载撤离 (45kg)

**输出**:
- 3 个 JSON 配置文件（`optimized_rss_config_*_v4.json`）
- Optuna 优化日志

**调用关系**:
```
rss_pipeline_v4.py（内置优化逻辑 + Optuna）
  └─> rss_digital_twin_fix.py (数字孪生仿真)
  └─> stamina_constants.py (常数定义)
```

**运行示例**:
```bash
python rss_pipeline_v4.py --trials 300 --jobs 4
# 输出：
# [v4] 8 missions loaded
# Trial 1/300 | Best combat_endurance=0.035 ...
# ...
# === Presets ===
# EliteStandard:  combat=0.180  recovery=0.000  realism=0.070
# StandardMilsim: combat=0.103  recovery=0.000  realism=0.958
# TacticalAction: combat=0.022  recovery=0.000  realism=2.931
```

---

### 🔬 rss_digital_twin_fix.py（数字孪生仿真器）

**用途**: 与游戏内体力逻辑对齐的 Python 仿真器，用于优化与校准

**包含内容**:
- `RSSDigitalTwin`：单步更新、场景模拟、EPOC 延迟
- **Pandolf** 消耗、恢复率、姿态/负重/疲劳等（Givoni 模型已弃用，仅保留历史记录）
- `RSSConstants`、`MovementType`、`Stance`、`EnvironmentFactor`

**不直接运行** - 被 `rss_pipeline_v4.py` 等调用

详细公式与决策树见：[docs/数字孪生优化器计算逻辑文档.md](../docs/数字孪生优化器计算逻辑文档.md)

---

### 📋 stamina_constants.py（常数定义）

**用途**: 定义全局常数和计算工具

**包含内容**:
- 游戏常数（速度、体重等）
- 医学模型参数（Pandolf；Givoni-Goldman 常量已弃用，仅作兼容）
- 恢复模型参数
- 工具函数（约束验证、参数转换等）

**主要常数**:
```python
# 游戏常数
GAME_MAX_SPEED = 5.5  # m/s（0kg 冲刺最大）
CHARACTER_WEIGHT = 90.0  # kg
TARGET_RUN_SPEED = 3.8  # m/s（0kg Run 最大）

# 医学模型
PANDOLF_VELOCITY_COEFF = 0.6
# GIVONI_VELOCITY_EXPONENT = 1.5  # (legacy, unused)

# 恢复模型
FAST_RECOVERY_DURATION_MINUTES = 1.5
MEDIUM_RECOVERY_DURATION_MINUTES = 5.0
```

**主要函数**:
```python
def calculate_completion_time(params):
    # 根据参数计算完成时间
    
def calculate_recovery_time(params):
    # 根据参数计算恢复时间
    
def validate_constraints(params, values):
    # 验证约束条件
```

---

### 其他工具

| 脚本 | 用途 |
|------|------|
| **test_v4_smoke.py** | v4 管道烟雾测试 |
| **test_basic_fitness.py** | 基础体能验证 |
| **quick_verify.py** | 快速验证 |
| **embed_json_to_c.py** | 将 JSON 预设嵌入到 `SCR_RSS_Settings.c` 中 |
| **rss_optimizer_gui.py** | 优化器图形界面（可选） |

---

### 📦 requirements.txt（Python 依赖）

**包含的依赖**:
```
optuna>=3.0.0        # 贝叶斯优化框架
numpy>=1.20.0        # 数值计算
scipy>=1.7.0         # 科学计算
pandas>=1.3.0        # 数据处理
matplotlib>=3.4.0    # 数据可视化（可选）
```

**安装**:
```bash
pip install -r requirements.txt
```

---

### 📊 JSON 配置文件

三个优化的预设配置文件，包含 16 个调校参数（其余使用 C 端静态参考值）。

#### 1. optimized_rss_config_elitestandard_v4.json
**优化目标**: parameter_realism 最小 → 最贴近 C 参考值

**特征**:
- combat_endurance: **0.180**
- parameter_realism: **0.070** (最低)
- 特点: 严格拟真，每项参数都接近 C 端静态常量

**适用场景**:
- 🎯 硬核拟真社区
- 🎯 军事训练模拟
- 🎯 竞技/PvP 服务器

**核心参数**:
```
energy_to_stamina_coeff: 7.17e-7
base_recovery_rate:      1.53e-4
fast_recovery_multiplier: 2.39
```

#### 2. optimized_rss_config_standardmilsim_v4.json
**优化目标**: 三目标均衡 → 拟真与可玩性最佳折中

**特征**:
- combat_endurance: **0.103**
- parameter_realism: **0.958**
- 特点: 均衡折中，归一化距原点最近

**适用场景**:
- 🎯 **主流 MILSIM 社区（推荐）**
- 🎯 多人合作模式
- 🎯 战术团队游戏

**核心参数**:
```
energy_to_stamina_coeff: 5.20e-7
base_recovery_rate:      1.97e-4
fast_recovery_multiplier: 2.35
```

#### 3. optimized_rss_config_tacticalaction_v4.json
**优化目标**: combat_endurance 最小 → 最宽容的体力系统

**特征**:
- combat_endurance: **0.022** (最低)
- parameter_realism: **2.931**
- 特点: 高恢复/低消耗，允许连续高强度动作

**适用场景**:
- 🎯 快节奏 PvP
- 🎯 新手友好
- 🎯 动作密集型玩法

**核心参数**:
```
energy_to_stamina_coeff: 5.20e-7
base_recovery_rate:      2.96e-4
fast_recovery_multiplier: 2.35
```

---

## 🔄 优化工作流

```
┌─────────────────────────────────────┐
│  rss_pipeline_v4.py (启动)           │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 加载 8 个多阶段战斗场景              │
│ (巡逻/沙漠/两栖/城镇/山地/载具/重载)  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 初始化 Optuna Study                  │
│ (3目标: endurance/recovery/realism)  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 运行 300 次试验（可配置）            │
│ (每次：参数取值→8场景仿真→指标计算)   │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 提取 Pareto 前沿                     │
│ (combat_endurance ↔ parameter_realism)│
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 按设计哲学选择 3 个预设              │
│ EliteStandard ← realism最小          │
│ StandardMilsim ← 距原点最近          │
│ TacticalAction ← endurance最小       │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 保存为 JSON 文件                     │
│ (optimized_rss_config_*_v4.json)     │
└─────────────────────────────────────┘
```

---

## 📊 v4 优化结果（300 次试验，8 场景）

### 预设指标

| 预设 | combat_endurance | parameter_realism | 哲学 |
|------|:---:|:---:|------|
| EliteStandard | 0.180 | **0.070** | 最贴近 C 参考值 |
| StandardMilsim | 0.103 | 0.958 | 三目标均衡折中 |
| TacticalAction | **0.022** | 2.931 | 战斗最宽容 |

- `combat_endurance`：越小 = 体力越耐用
- `parameter_realism`：越小 = 越贴近 C 端静态参考常量

---

## 🔧 常见任务

### 任务1: 重新运行优化

```bash
# 运行 v4 优化管道
python rss_pipeline_v4.py --trials 300 --jobs 4
```

### 任务2: 烟雾测试（验证管道可运行）

```bash
python test_v4_smoke.py
```

### 任务3: 修改优化目标

编辑 `rss_pipeline_v4.py` 中的 `compute_metrics()` 函数，调整三个指标的权重或定义。

### 任务4: 添加新的战斗场景

在 `rss_pipeline_v4.py` 的 `MissionLibrary` 中添加新场景定义（时长/速度/姿态/负重/环境）。

---

## 📈 优化进度监控

### 运行时输出示例

```
[v4] 8 missions loaded
[v4] Search space: 16 parameters
Trial 1/300 | Best combat_endurance=0.035 | Best realism=0.085
Trial 100/300 | Best combat_endurance=0.022 | Best realism=0.070
...
=== Presets ===
EliteStandard:  combat=0.180  recovery=0.000  realism=0.070
StandardMilsim: combat=0.103  recovery=0.000  realism=0.958
TacticalAction: combat=0.022  recovery=0.000  realism=2.931
Config files saved:
  - optimized_rss_config_elitestandard_v4.json
  - optimized_rss_config_standardmilsim_v4.json
  - optimized_rss_config_tacticalaction_v4.json
```

---

## 🐛 故障排除

### 问题1: "ModuleNotFoundError: No module named 'optuna'"

**解决**:
```bash
pip install -r requirements.txt
```

### 问题2: 优化过程中断

**解决**: 优化器会自动保存进度到 SQLite 数据库，下次运行会继续

```bash
# 增加试验次数并重试
python rss_pipeline_v4.py --trials 500 --jobs 4
```

### 问题3: 优化时间太长

**优化**:
- 减少试验次数（在 pipeline 中修改 `n_trials`，默认 500）
- 使用更快的计算机
- 在后台运行，不影响其他工作

### 问题4: 生成的参数看起来不对

**检查清单**:
- ✅ 确认约束条件是否生效
- ✅ 检查医学模型参数是否正确
- ✅ 查看优化历史数据库中的试验记录

---

## 📚 参考资源

### 使用的优化方法
- **Optuna**: 贝叶斯优化框架
- **NSGA-II**: 多目标遗传算法
- **Pareto 前沿**: 不可支配解的集合

### 使用的医学模型
- **Pandolf 模型**: 步行能量消耗 (Pandolf et al., 1977)
- **氧债模型**: 恢复过程 (Palumbo et al., 2018)

### v4 优化参数
- **场景数**: 8 个多阶段战斗任务（负重 20–45 kg，含环境压力）
- **搜索空间**: 16 个核心参数
- **试验次数**: 300（可配置）

---

## ✅ 维护清单

定期检查清单：

- [ ] 所有依赖库更新到最新版本
- [ ] 优化结果与预期保持一致
- [ ] JSON 配置文件格式有效
- [ ] 修改优化参数后重新运行管道
- [ ] 保存优化历史用于追踪

---

## 📞 支持

**问题报告**:
在模组的 GitHub Issues 中报告 tools 相关问题

**贡献**:
欢迎提交改进优化算法的 Pull Requests

---

**最后更新**: 2026-05-07  
**RSS 版本**: 3.22.5  
**工具版本**: v4 (rss_pipeline_v4.py)  
**优化版本**: Optuna 多目标（endurance / recovery / realism）  
**状态**: ✅ 生产就绪
