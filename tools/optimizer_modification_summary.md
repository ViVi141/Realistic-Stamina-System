# 优化器目标函数和约束条件修改总结

## 修改日期
2026-01-30

---

## 新设计标准

### 1. 跑步（Run）目标
- 条件：0kg 负重
- 距离：3.2 km
- 时间：15分27秒（927 秒）
- 要求：最低体力不能跌破 10%，也不能长期处于 10%

### 2. 冲刺（Sprint）目标
- 消耗为 Run 的 2.5x

### 3. 行走（Walk）目标
- 净恢复：每 5 秒恢复 1% 体力

---

## 修改内容

### 修改 1：基础体能检查（_check_basic_fitness）

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
# 检查最低体力是否合理（不低于20%）
min_stamina = results['min_stamina']

# 通过条件：时间不超过120%且体力不低于20%
return time_ratio <= 1.2 and min_stamina >= 0.2
```

**修改后**：
```python
# 检查最低体力是否合理（不低于10%）
min_stamina = results['min_stamina']

# 检查平均体力是否合理（不能长期处于10%，应该 > 15%）
stamina_history = results.get('stamina_history', [])
if stamina_history:
    avg_stamina = sum(stamina_history) / len(stamina_history)
else:
    avg_stamina = 1.0

# 通过条件：时间不超过120%且体力不低于10%且平均体力 > 15%
return time_ratio <= 1.2 and min_stamina >= 0.10 and avg_stamina > 0.15
```

**理由**：
- 新设计标准要求体力最低不能跌破 10%（从 20% 降低到 10%）
- 新增平均体力检查，确保体力不会长期处于 10%（平均体力应该 > 15%）

---

### 修改 2：冲刺消耗倍数范围

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
# Sprint 相关
# 调整：提高下界，确保 Sprint 明显更耗体力。
sprint_stamina_drain_multiplier = trial.suggest_float(
    'sprint_stamina_drain_multiplier', 2.8, 5.0
)
```

**修改后**：
```python
# Sprint 相关
# 新设计标准：冲刺消耗为 Run 的 2.5x
sprint_stamina_drain_multiplier = trial.suggest_float(
    'sprint_stamina_drain_multiplier', 2.3, 2.7  # 调整范围以更接近 2.5x
)
```

**理由**：
- 新设计标准要求冲刺消耗为 Run 的 2.5x
- 调整搜索空间范围从 2.8-5.0 到 2.3-2.7，使其更接近 2.5x

---

### 修改 3：行走净恢复目标

**文件**：`tools/rss_super_pipeline.py`

**修改前**：
```python
# 3) Walk：120秒 1.8m/s 标准战斗负载，期望"缓慢恢复"
# 这里用 30KG 负载（总重 120KG）更符合玩家场景。
walk_delta = simulate_fixed_speed(
    speed=1.8,
    duration_seconds=120.0,
    current_weight=120.0,
    movement_type=MovementType.WALK,
    initial_stamina=0.50,
)
# 期望 2分钟至少 +0.8%（从1%降低到0.8%），最多 +8%（从6%提高到8%）
min_walk_gain = 0.008  # 从1%降低到0.8%
max_walk_gain = 0.08  # 从6%提高到8%
```

**修改后**：
```python
# 3) Walk：120秒 1.8m/s 空载，期望"净恢复"
# 新设计标准：每 5 秒恢复 1% 体力
# 120 秒应该恢复：120 / 5 × 1% = 24%
walk_delta = simulate_fixed_speed(
    speed=1.8,
    duration_seconds=120.0,
    current_weight=90.0,  # 空载
    movement_type=MovementType.WALK,
    initial_stamina=0.50,
)
# 期望 2 分钟恢复 20-28%（接近 24%），允许一定误差范围
min_walk_gain = 0.20  # 最小恢复 20%
max_walk_gain = 0.28  # 最大恢复 28%
```

**理由**：
- 新设计标准要求每 5 秒恢复 1% 体力
- 120 秒应该恢复：120 / 5 × 1% = 24%
- 调整从 30KG 负载改为空载（90kg）
- 调整恢复目标从 0.8%-8% 改为 20%-28%

---

## 修改后的优化器行为

### 搜索空间调整

| 参数 | 修改前范围 | 修改后范围 | 变化 |
|-----|-----------|-----------|------|
| sprint_stamina_drain_multiplier | 2.8-5.0 | 2.3-2.7 | 收窄到 2.5x 附近 |

### 约束条件调整

| 约束 | 修改前 | 修改后 | 变化 |
|-----|--------|--------|------|
| ACFT 测试最低体力 | ≥ 20% | ≥ 10% | 放宽约束 |
| ACFT 测试平均体力 | 无检查 | > 15% | 新增约束 |
| Walk 恢复率（120秒） | 0.8%-8% | 20%-28% | 大幅提高 |

---

## 预期效果

### 优化器将自动找到符合新设计标准的参数

1. **ACFT 测试**：
   - 体力最低不会跌破 10%
   - 平均体力 > 15%（不会长期处于 10%）
   - 完成时间不超过 15分27秒的 120%

2. **冲刺消耗**：
   - 冲刺消耗约为跑步消耗的 2.5x
   - 搜索空间被限制在 2.3-2.7 范围内

3. **行走净恢复**：
   - 每 5 秒恢复约 1% 体力
   - 120 秒恢复 20%-28%

---

## 下一步

### 1. 运行优化器
```bash
python rss_super_pipeline.py --n_trials 500 --n_jobs 4
```

### 2. 查看优化结果
优化器会在 `tools/` 目录下生成：
- `optimized_rss_config_realism_super.json`
- `optimized_rss_config_playability_super.json`
- `optimized_rss_config_balanced_super.json`

### 3. 应用优化后的参数到 C 代码
使用 `embed_json_to_c.py` 将优化后的参数嵌入到 C 代码中。

### 4. 编译和测试
- 编译修改后的 C 代码
- 在游戏内测试消耗率和恢复率

---

## 注意事项

1. **不要直接修改参数**：
   - 优化器会自动找到最优参数
   - 直接修改参数可能导致其他性能指标变差

2. **搜索空间调整**：
   - 如果优化结果不符合预期，可以调整搜索空间范围
   - 例如：如果冲刺消耗仍然不是 2.5x，可以进一步收窄搜索空间

3. **约束条件调整**：
   - 如果优化结果不满足某些约束，可以调整约束条件
   - 例如：如果平均体力仍然太低，可以提高最小平均体力要求

---

## 总结

所有优化器修改已完成：

✅ **基础体能检查**：体力最低 ≥ 10%，平均体力 > 15%
✅ **冲刺消耗倍数**：搜索空间调整为 2.3-2.7（接近 2.5x）
✅ **行走净恢复**：目标调整为每 5 秒恢复 1% 体力

**下一步**：运行优化器，自动找到符合新设计标准的最优参数！