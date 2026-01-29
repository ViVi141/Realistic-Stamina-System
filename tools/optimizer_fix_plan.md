# RSS优化器收敛问题修复计划

## 修复日期
2026年1月29日

## 问题概述
无论设置多少次迭代，各个优化参数始终收敛不到良好的数值。

---

## 根本原因分析

### 1. 约束条件过于严格且相互冲突
- 恢复倍数约束将姿态（prone, standing）与恢复阶段（fast, medium, slow）混在一起比较
- 要求 `prone > fast > standing > medium > slow` 没有生理学依据

### 2. 参数搜索空间与约束严重不匹配
- `standing`范围：1.0-1.8，`medium`范围：1.2-1.8
- 约束要求 `standing > medium`，但随机采样经常违反

### 3. 目标函数权重严重失衡
- `realism_weight = 5000.0` 过大
- 是 `stability_risk` 权重的5000倍
- 导致优化器忽略其他目标

### 4. 移动平衡惩罚系数过大
- Run/Sprint惩罚系数：20000, 25000
- Walk惩罚系数：15000, 8000
- 即使轻微偏离要求也会产生巨大惩罚

### 5. 可玩性负担评估标准过高
- 30KG负载要求平均体力>45%过于苛刻
- 最低体力<20%惩罚系数2500过大

### 6. 高维搜索空间复杂度
- 41个优化参数
- 参数之间存在复杂耦合关系
- 默认500次迭代对于41维空间来说太少

---

## 修复方案

### 阶段1：立即修复（高优先级）🔴

#### 修复1.1：降低生理学合理性权重
**文件**：`tools/rss_super_pipeline.py`
**行号**：608
**修改内容**：
```python
# 修改前
realism_weight = 5000.0

# 修改后
realism_weight = 100.0  # 降低50倍，使其与其他目标权重平衡
```
**预期效果**：防止优化器过度关注生理学合理性，忽略其他目标

---

#### 修复1.2：修正恢复倍数约束逻辑
**文件**：`tools/rss_super_pipeline.py`
**行号**：662
**修改内容**：
```python
# 修改前
if not (prone_recovery > fast_recovery > standing_recovery > medium_recovery > slow_recovery):
    realism_score += 5.0

# 修改后
# 分离姿态和恢复阶段的约束
if not (prone_recovery > standing_recovery):
    realism_score += 3.0  # 趴下应该比站立恢复快
    
if not (fast_recovery > medium_recovery > slow_recovery):
    realism_score += 3.0  # 恢复阶段应该递减
```
**预期效果**：修正逻辑错误，使约束具有生理学合理性

---

#### 修复1.3：调整参数搜索范围（standing和medium）
**文件**：`tools/rss_super_pipeline.py`
**行号**：317-318
**修改内容**：
```python
# 修改前
standing_recovery_multiplier = trial.suggest_float('standing_recovery_multiplier', 1.0, 1.8)
medium_recovery_multiplier = trial.suggest_float('medium_recovery_multiplier', 1.2, 1.8)

# 修改后
standing_recovery_multiplier = trial.suggest_float('standing_recovery_multiplier', 1.3, 1.8)  # 提高下界，确保>medium
medium_recovery_multiplier = trial.suggest_float('medium_recovery_multiplier', 1.0, 1.5)  # 降低上界，确保<standing
```
**预期效果**：确保参数搜索空间与约束匹配，减少无效采样

---

#### 修复1.4：调整prone和fast的范围
**文件**：`tools/rss_super_pipeline.py`
**行号**：314, 364
**修改内容**：
```python
# 修改前
prone_recovery_multiplier = trial.suggest_float('prone_recovery_multiplier', 1.5, 2.5)
fast_recovery_multiplier = trial.suggest_float('fast_recovery_multiplier', 2.2, 3.0)

# 修改后
prone_recovery_multiplier = trial.suggest_float('prone_recovery_multiplier', 2.0, 2.8)  # 提高下界，确保>fast
fast_recovery_multiplier = trial.suggest_float('fast_recovery_multiplier', 1.6, 2.4)  # 降低范围，确保<prone
```
**预期效果**：确保 `prone > fast` 约束更容易满足

---

### 阶段2：中期优化（中优先级）🟡

#### 修复2.1：降低移动平衡惩罚系数
**文件**：`tools/rss_super_pipeline.py`
**行号**：743, 756, 771
**修改内容**：
```python
# 修改前
penalty += (required_run_drop - actual_run_drop) * 20000.0
penalty += (required_sprint_drop - actual_sprint_drop) * 25000.0
if walk_delta < min_walk_gain:
    penalty += (min_walk_gain - walk_delta) * 15000.0
elif walk_delta > max_walk_gain:
    penalty += (walk_delta - max_walk_gain) * 8000.0

# 修改后
penalty += (required_run_drop - actual_run_drop) * 5000.0  # 降低4倍
penalty += (required_sprint_drop - actual_sprint_drop) * 6000.0  # 降低4倍多
if walk_delta < min_walk_gain:
    penalty += (min_walk_gain - walk_delta) * 5000.0  # 降低3倍
elif walk_delta > max_walk_gain:
    penalty += (walk_delta - max_walk_gain) * 3000.0  # 降低2.7倍
```
**预期效果**：降低惩罚强度，允许更大的灵活性

---

#### 修复2.2：放宽可玩性负担评估标准
**文件**：`tools/rss_super_pipeline.py`
**行号**：868, 871, 858-859
**修改内容**：
```python
# 修改前
scenario_burden += max(0.0, time_ratio - 1.05) * 200.0
scenario_burden += max(0.0, time_ratio - 1.10) * 1200.0
scenario_burden += max(0.0, 0.20 - min_stamina) * 2500.0
scenario_burden += max(0.0, 0.45 - mean_stamina) * 600.0

# 修改后
scenario_burden += max(0.0, time_ratio - 1.10) * 300.0  # 从105%放宽到110%，降低系数
scenario_burden += max(0.0, time_ratio - 1.20) * 800.0  # 从110%放宽到120%，降低系数
scenario_burden += max(0.0, 0.15 - min_stamina) * 1500.0  # 从20%/2500降到15%/1500
scenario_burden += max(0.0, 0.35 - mean_stamina) * 400.0  # 从45%/600降到35%/400
```
**预期效果**：降低30KG负载下的评估标准，使其更现实

---

#### 修复2.3：降低约束惩罚系数
**文件**：`tools/rss_super_pipeline.py`
**行号**：544, 551, 558, 564, 570, 577, 584
**修改内容**：
```python
# 修改前
if prone_recovery_multiplier <= standing_recovery_multiplier:
    violation_factor = standing_recovery_multiplier - prone_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 500.0
    stability_risk += constraint_penalty

if standing_recovery_multiplier <= slow_recovery_multiplier:
    violation_factor = slow_recovery_multiplier - standing_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 300.0
    stability_risk += constraint_penalty

if fast_recovery_multiplier <= medium_recovery_multiplier:
    violation_factor = medium_recovery_multiplier - fast_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 400.0
    stability_risk += constraint_penalty

if medium_recovery_multiplier <= slow_recovery_multiplier:
    violation_factor = slow_recovery_multiplier - medium_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 300.0
    stability_risk += constraint_penalty

if posture_crouch_multiplier > 1.0:
    violation_factor = posture_crouch_multiplier - 1.0
    constraint_penalty += violation_factor * 600.0
    stability_risk += constraint_penalty

if posture_prone_multiplier > 1.0:
    violation_factor = posture_prone_multiplier - 1.0
    constraint_penalty += violation_factor * 600.0
    stability_risk += constraint_penalty

if posture_prone_multiplier > posture_crouch_multiplier:
    violation_factor = posture_prone_multiplier - posture_crouch_multiplier
    constraint_penalty += violation_factor * 300.0
    stability_risk += constraint_penalty

# 修改后
if prone_recovery_multiplier <= standing_recovery_multiplier:
    violation_factor = standing_recovery_multiplier - prone_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 200.0  # 从500降到200
    stability_risk += constraint_penalty

if standing_recovery_multiplier <= slow_recovery_multiplier:
    violation_factor = slow_recovery_multiplier - standing_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 150.0  # 从300降到150
    stability_risk += constraint_penalty

if fast_recovery_multiplier <= medium_recovery_multiplier:
    violation_factor = medium_recovery_multiplier - fast_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 150.0  # 从400降到150
    stability_risk += constraint_penalty

if medium_recovery_multiplier <= slow_recovery_multiplier:
    violation_factor = slow_recovery_multiplier - medium_recovery_multiplier + 0.1
    constraint_penalty += violation_factor * 100.0  # 从300降到100
    stability_risk += constraint_penalty

if posture_crouch_multiplier > 1.0:
    violation_factor = posture_crouch_multiplier - 1.0
    constraint_penalty += violation_factor * 200.0  # 从600降到200
    stability_risk += constraint_penalty

if posture_prone_multiplier > 1.0:
    violation_factor = posture_prone_multiplier - 1.0
    constraint_penalty += violation_factor * 200.0  # 从600降到200
    stability_risk += constraint_penalty

if posture_prone_multiplier > posture_crouch_multiplier:
    violation_factor = posture_prone_multiplier - posture_crouch_multiplier
    constraint_penalty += violation_factor * 100.0  # 从300降到100
    stability_risk += constraint_penalty
```
**预期效果**：降低约束惩罚强度，减少对搜索空间的过度限制

---

### 阶段3：长期改进（低优先级）🟢

#### 改进3.1：添加优化配置文件
创建 `tools/optimizer_presets.json`，存储不同优化目标的配置：
```json
{
  "balanced": {
    "realism_weight": 100.0,
    "movement_penalty_scale": 1.0,
    "constraint_penalty_scale": 1.0,
    "playability_strictness": 1.0
  },
  "playability_focused": {
    "realism_weight": 50.0,
    "movement_penalty_scale": 0.5,
    "constraint_penalty_scale": 0.8,
    "playability_strictness": 0.7
  },
  "realism_focused": {
    "realism_weight": 200.0,
    "movement_penalty_scale": 1.2,
    "constraint_penalty_scale": 1.5,
    "playability_strictness": 1.2
  }
}
```

#### 改进3.2：创建参数简化版本
创建 `tools/rss_super_pipeline_simplified.py`：
- 只优化最关键的15个参数
- 固定次要参数为合理默认值
- 提高收敛速度

#### 改进3.3：添加收敛性检查
在优化过程中添加：
- 检查是否有可行解
- 检查目标函数是否收敛
- 如果不收敛，自动调整参数

---

## 修复顺序

1. **阶段1修复**（立即执行）
   - 修复1.1：降低realism_weight
   - 修复1.2：修正恢复倍数约束
   - 修复1.3：调整standing和medium范围
   - 修复1.4：调整prone和fast范围

2. **阶段2修复**（验证阶段1后执行）
   - 修复2.1：降低移动平衡惩罚系数
   - 修复2.2：放宽可玩性负担评估标准
   - 修复2.3：降低约束惩罚系数

3. **阶段3改进**（长期项目）
   - 改进3.1：添加优化配置文件
   - 改进3.2：创建简化版本
   - 改进3.3：添加收敛性检查

---

## 验证方法

### 验证1：小规模测试
```bash
python tools/rss_super_pipeline.py --trials 200 --preset balanced
```
**检查项**：
- 是否有参数组合满足所有约束
- 目标函数值是否下降
- 是否有明显的收敛趋势

### 验证2：中等规模测试
```bash
python tools/rss_super_pipeline.py --trials 2000 --preset balanced
```
**检查项**：
- 帕累托前沿是否多样化
- 是否有多个非支配解
- 目标函数值是否稳定

### 验证3：完整规模测试
```bash
python tools/rss_super_pipeline.py --trials 10000 --preset balanced
```
**检查项**：
- 与修复前对比
- 目标函数改善比例
- 参数是否收敛到合理范围

### 验证4：实际游戏测试
将优化结果应用到游戏，测试：
- 30KG负载下的可玩性
- 系统稳定性（无BUG）
- 生理学合理性

---

## 预期效果

### 量化指标
- `playability_burden`：预计降低30-50%
- `stability_risk`：预计降低40-60%
- `physiological_realism`：保持在合理范围
- 收敛速度：预计提高2-3倍

### 质性改善
- 优化器能够找到满足约束的参数组合
- 帕累托前沿更加多样化
- 参数收敛到合理范围
- 游戏体验明显改善

---

## 风险评估

### 低风险修复
- ✅ 降低realism_weight
- ✅ 修正恢复倍数约束逻辑
- ✅ 调整参数搜索范围

### 中风险修复
- ⚠️ 降低移动平衡惩罚系数
- ⚠️ 放宽可玩性负担评估标准

### 高风险修复
- 🔴 降低约束惩罚系数
- 🔴 减少优化参数数量

**缓解措施**：
- 每次修复后进行小规模测试
- 对比修复前后的效果
- 如果效果不理想，可以回滚

---

## 备份与回滚

### 修复前备份
```bash
cp tools/rss_super_pipeline.py tools/rss_super_pipeline.py.backup
```

### 回滚方法
```bash
cp tools/rss_super_pipeline.py.backup tools/rss_super_pipeline.py
```

---

## 总结

**核心问题**：约束条件过于严格，参数搜索空间与约束不匹配，目标函数权重失衡

**关键修复**：
1. 降低realism_weight从5000到100
2. 修正恢复倍数约束逻辑
3. 调整参数搜索范围
4. 降低惩罚系数

**预期结果**：优化器能够收敛到良好的数值，满足所有约束条件

---

## 附录：参数对照表

| 参数 | 修改前 | 修改后 | 说明 |
|-----|--------|--------|------|
| realism_weight | 5000.0 | 100.0 | 降低50倍 |
| prone_recovery_multiplier | 1.5-2.5 | 2.0-2.8 | 提高下界 |
| fast_recovery_multiplier | 2.2-3.0 | 1.6-2.4 | 降低范围 |
| standing_recovery_multiplier | 1.0-1.8 | 1.3-1.8 | 提高下界 |
| medium_recovery_multiplier | 1.2-1.8 | 1.0-1.5 | 降低上界 |
| run_penalty_coeff | 20000 | 5000 | 降低4倍 |
| sprint_penalty_coeff | 25000 | 6000 | 降低4倍多 |
| walk_penalty_coeff | 15000 | 5000 | 降低3倍 |
| min_stamina_threshold | 0.20 | 0.15 | 降低5个百分点 |
| mean_stamina_threshold | 0.45 | 0.35 | 降低10个百分点 |