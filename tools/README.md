# Realistic Stamina System - Tools 工具集

## 📋 目录概览

本目录包含 Realistic Stamina System (RSS) 模组的优化工具链和配置文件。

### 核心文件结构

```
tools/
├── README.md                                    # 📖 本文件
├── rss_super_pipeline.py                        # 🎯 主优化管道（推荐运行，内置多目标优化）
├── rss_digital_twin_fix.py                      # 🔬 数字孪生仿真器（被 pipeline 调用）
├── stamina_constants.py                         # 📋 常数定义工具库
├── calibrate_run_3_5km.py                       # 📐 3.5km/15:27 校准（最低体力 20%）
├── calibrate_recovery.py                        # 📐 恢复时间校准
├── embed_json_to_c.py                           # 📄 JSON → SCR_RSS_Settings.c 嵌入
├── verify_json_params.py                        # ✅ JSON 参数校验
├── rss_optimizer_gui.py                         # 🖥️ 优化器 GUI（可选）
├── requirements.txt                             # 📦 Python 依赖列表
├── optimized_rss_config_realism_super.json      # 📊 精英拟真配置
├── optimized_rss_config_playability_super.json  # 📊 战术平衡配置 ⭐ 推荐
└── optimized_rss_config_balanced_super.json     # 📊 保守配置
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
python rss_super_pipeline.py
```

这将执行：
- ✅ 可配置次数的 Optuna 多目标试验（默认 500，可在 pipeline 中改为 10000 等）
- ✅ 多目标：Realism_Loss、Playability_Burden、Stability_Risk
- ✅ 物理与逻辑约束验证（恢复倍数顺序、姿态消耗等）
- ✅ 3 个 Pareto 最优预设生成
- ✅ 自动保存为 JSON 配置文件

**预计耗时**: 默认 500 次约数分钟至十几分钟；若改为 10000 次约 1–3 小时（取决于计算机性能）

### 3. 在游戏中应用配置

生成的 JSON 文件会自动保存，游戏启动时通过以下路径加载：

```
$profile:RealisticStaminaSystem.json
```

---

## 📄 文件详解

### 🎯 rss_super_pipeline.py（主优化管道）

**用途**: 执行完整的 NSGA-II 多目标优化

**功能**:
- 运行 Optuna 多目标优化（试验次数可配置，默认 500）
- 三目标：Realism_Loss、Playability_Burden、Stability_Risk（含鲁棒性/稳定性测试）
- 约束与合理性验证：
  - ✅ 基础体能：0kg Run 3.5km/15:27 最低体力约 20%
  - ✅ 恢复倍数顺序：prone > standing > slow；fast > medium > slow
  - ✅ 姿态消耗：蹲姿/趴姿倍数 > 1，趴姿 > 蹲姿
  - ✅ 参数范围与生物学/物理学逻辑

**优化目标**:
1. 最小化拟真损失 (realism_loss) → EliteStandard
2. 最小化可玩性负担 (playability_burden) → TacticalAction
3. 平衡两者 (balanced) → StandardMilsim

**输出**:
- 3 个 JSON 配置文件
- SQLite 优化数据库 (`rss_super_optimization.db`)
- 优化历史记录

**调用关系**:
```
rss_super_pipeline.py（内置优化逻辑）
  └─> rss_digital_twin_fix.py (数字孪生仿真)
  └─> stamina_constants.py (常数定义，若被引用)
```

**运行示例**:
```bash
python rss_super_pipeline.py
# 输出：
# [INFO] Starting RSS Super Optimization Pipeline...
# [INFO] Running Optuna optimization (500 trials, 可配置)...
# [PROGRESS] Trial 1/500 | Best realism_loss=0.185 | Best playability_burden=425
# ...
# [SUCCESS] Optimization complete!
# [SUCCESS] Generated 3 Pareto-optimal presets
# [SUCCESS] Saved to: optimized_rss_config_*.json
```

---

### 🔬 rss_digital_twin_fix.py（数字孪生仿真器）

**用途**: 与游戏内体力逻辑对齐的 Python 仿真器，用于优化与校准

**包含内容**:
- `RSSDigitalTwin`：单步更新、场景模拟、EPOC 延迟
- **Pandolf** 消耗、恢复率、姿态/负重/疲劳等（Givoni 模型已弃用，仅保留历史记录）
- `RSSConstants`、`MovementType`、`Stance`、`EnvironmentFactor`

**不直接运行** - 被 `rss_super_pipeline.py`、`calibrate_run_3_5km.py`、`calibrate_recovery.py` 等调用

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

### 其他工具（校准 / 嵌入 / 校验 / GUI）

| 脚本 | 用途 |
|------|------|
| **calibrate_run_3_5km.py** | 校准 `energy_to_stamina_coeff`，使 0kg Run 3.5km/15:27 结束时最低体力约 20% |
| **calibrate_recovery.py** | 校准恢复相关参数（恢复时间等） |
| **embed_json_to_c.py** | 将 JSON 预设嵌入到 `SCR_RSS_Settings.c` 中 |
| **verify_json_params.py** | 校验 JSON 参数格式与范围 |
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

三个优化的预设配置文件，包含 41 个参数。

#### 1. optimized_rss_config_realism_super.json
**优化目标**: 最大化拟真度（最小化拟真损失）

**特征**:
- 拟真损失: **0.1815** (最低)
- 可玩性负担: 434.0
- 特点: 更难玩、更接近现实

**适用场景**:
- 🎯 硬核玩家（想要高难度体验）
- 🎯 竞技模式（需要真实体力消耗）
- 🎯 教学/军事训练模拟

**样本参数**:
```json
{
  "energy_to_stamina_coeff": 2.5057006371784408e-05,
  "base_recovery_rate": 0.0001717787540783644,
  "standing_recovery_multiplier": 1.105066137151609,
  "sprint_stamina_drain_multiplier": 3.01,
  ...
}
```

#### 2. optimized_rss_config_playability_super.json ⭐ 推荐
**优化目标**: 最大化可玩性（最小化可玩性负担）

**特征**:
- 拟真损失: 0.1816 (几乎相同)
- 可玩性负担: **422.8** (最低)
- 特点: 容易玩、平衡体验

**适用场景**:
- 🎯 **主流玩家（推荐这个）**
- 🎯 普通游戏体验
- 🎯 多人合作模式
- 🎯 战术团队游戏

**样本参数**:
```json
{
  "energy_to_stamina_coeff": 2.5057006371784408e-05,
  "base_recovery_rate": 0.0001972519934567765,
  "standing_recovery_multiplier": 1.275494136257953,
  "sprint_stamina_drain_multiplier": 2.45,
  ...
}
```

#### 3. optimized_rss_config_balanced_super.json
**优化目标**: 平衡拟真度和可玩性

**特征**:
- 拟真损失: 0.1816
- 可玩性负担: 422.8 (与playability相同)
- 特点: 保守、稳定、容易调整

**适用场景**:
- 🎯 服务器管理员（易于微调）
- 🎯 保守游戏体验
- 🎯 新手友好模式

---

## 🔄 优化工作流

```
┌─────────────────────────────────────┐
│  rss_super_pipeline.py (启动)        │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 加载 stamina_constants.py            │
│ (获取游戏常数、医学模型参数)          │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 初始化 Optuna Study                  │
│ (创建优化问题、设置目标函数)          │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 运行 10,000 次试验                   │
│ (每次试验：参数取值→仿真→约束验证→目标值)│
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 提取 Pareto 前沿                     │
│ (找到不可能同时改进的解集合)         │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 生成 3 个预设配置                    │
│ (EliteStandard, TacticalAction,     │
│  StandardMilsim)                     │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 保存为 JSON 文件                     │
│ (optimized_rss_config_*.json)        │
└─────────────────────────────────────┘
```

---

## 📊 性能基准

### 完成时间（2英里 = 3218.7米）

| 预设 | 目标完成时间 | 优化结果 | 精度 |
|------|----------|--------|------|
| EliteStandard | 927秒 (15:27) | 925.8秒 (15:26) | ✅ 提前1.2秒 |
| TacticalAction | 927秒 (15:27) | 924.3秒 (15:24) | ✅ 提前2.7秒 |
| StandardMilsim | 927秒 (15:27) | 925.5秒 (15:26) | ✅ 提前1.5秒 |

### 恢复时间（从精疲力尽到90%体力）

| 预设 | 目标恢复时间 | 优化结果 | 精度 |
|------|----------|--------|------|
| EliteStandard | 8-10分钟 | 9.2分钟 | ✅ 在目标范围内 |
| TacticalAction | 8-10分钟 | 8.8分钟 | ✅ 在目标范围内 |
| StandardMilsim | 8-10分钟 | 9.1分钟 | ✅ 在目标范围内 |

---

## 🔧 常见任务

### 任务1: 重新运行优化

```bash
# 删除旧的优化数据库
rm rss_super_optimization.db rss_super_optimization.db-journal

# 运行新的优化
python rss_super_pipeline.py
```

### 任务2: 检查当前配置

```bash
# 查看 TacticalAction 预设的 Sprint 倍数
import json
with open('optimized_rss_config_playability_super.json') as f:
    config = json.load(f)
    print(config['sprint_stamina_drain_multiplier'])
```

### 任务3: 修改优化目标

编辑 `rss_super_pipeline.py` 中的目标函数（如 `_run_trial` / 多目标返回值），调整 Realism_Loss、Playability_Burden、Stability_Risk 的权重或定义。

### 任务4: 添加新的约束

在 `rss_super_pipeline.py` 中与「物理约束条件」「稳定性风险」相关的逻辑里添加新约束（如恢复倍数、姿态倍数、参数上下界等），并计入 `constraint_penalty` 或 `stability_risk`。

---

## 📈 优化进度监控

### 运行时输出示例

```
[INFO] RSS Super Optimization Pipeline v3.10.0
[INFO] Starting optimization run...
[INFO] Optuna Study created with seed=42

[PROGRESS] Trial 1/500 - Best realism_loss: 0.185 | Best playability_burden: 425
[PROGRESS] Trial 100/500 - Best realism_loss: 0.182 | Best playability_burden: 422
...
[SUCCESS] Optimization complete!
[SUCCESS] Total trials: 500 | Time: ~数分钟（可配置 n_trials 为 10000 等以延长）
[SUCCESS] Pareto front size: 157 points
[SUCCESS] Generated 3 presets:
  - EliteStandard (realism=0.1815, playability=434.0)
  - TacticalAction (realism=0.1816, playability=422.8) ⭐
  - StandardMilsim (realism=0.1816, playability=422.8)
[SUCCESS] Config files saved:
  - optimized_rss_config_realism_super.json
  - optimized_rss_config_playability_super.json
  - optimized_rss_config_balanced_super.json
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
# 如果需要从头开始
rm rss_super_optimization.db rss_super_optimization.db-journal
python rss_super_pipeline.py
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
- **Givoni-Goldman 模型**: 跑步代谢 (Givoni & Goldman, 1971)
- **氧债模型**: 恢复过程 (Palumbo et al., 2018)

### 游戏平衡参数
- **完成时间**: 15分27秒 / 2英里
- **恢复时间**: 8-10分钟
- **约束数量**: 7个

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

**最后更新**: 2026-01-30  
**RSS 版本**: 3.11.1  
**工具版本**: 与 RSS 主项目同步（3.11.1）  
**优化版本**: Optuna 多目标（Realism / Playability / Stability）  
**状态**: ✅ 生产就绪
