# C文件与Python数字孪生常量修复总结

## 修复日期
2026年1月29日

## 修复目标
以Python数字孪生文件（`rss_digital_twin_fix.py`）的值为准，修复C文件（`SCR_StaminaConstants.c`）中的硬编码常量不匹配问题。

## 修复详情

### 1. ENCUMBRANCE_STAMINA_DRAIN_COEFF（负重体力消耗系数）✅ 已修复

**修复前**:
- C文件值: 1.5
- Python值: 2.0
- 差异: -0.5（33.3%）

**修复后**:
- C文件值: 2.0
- Python值: 2.0
- 状态: ✅ 匹配

**影响分析**（以29KG负重为例）:
- 体重：90kg
- 负重/体重 = 29/90 ≈ 0.322

修复前（C=1.5）:
- 体力消耗倍数 = 1 + 1.5 × 0.322 = 1.483
- 增加48.3%的体力消耗

修复后（C=2.0）:
- 体力消耗倍数 = 1 + 2.0 × 0.322 = 1.644
- 增加64.4%的体力消耗

**影响**: 负重惩罚增加16.1%，更接近Python数字孪生的行为

**修改文件**:
- `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` (第96行)

---

### 2. MEDIUM_RECOVERY_DURATION_MINUTES（中等恢复期持续时间）✅ 已修复

**修复前**:
- C文件值: 8.5分钟
- Python值: 5.0分钟
- 差异: +3.5分钟（70%）
- 恢复期范围: 1.5-10分钟

**修复后**:
- C文件值: 5.0分钟
- Python值: 5.0分钟
- 状态: ✅ 匹配
- 恢复期范围: 1.5-6.5分钟

**影响**: 中等恢复期持续时间缩短3.5分钟（从6.5分钟缩短到5分钟）

**修改文件**:
- `scripts/Game/Components/Stamina/SCR_StaminaConstants.c` (第155行)
- `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c` (第461行注释)

---

## 未修复的常量（用途不同）

### 1. POSTURE_CROUCH_MULTIPLIER
- C文件值: 1.8（蹲姿行走消耗倍数）
- Python值: 0.7（蹲姿移动速度倍数）
- 状态: ❌ 不需要修复（用途不同）

### 2. POSTURE_PRONE_MULTIPLIER
- C文件值: 3.0（匍匐爬行消耗倍数）
- Python值: 0.3（趴姿移动速度倍数）
- 状态: ❌ 不需要修复（用途不同）

---

## 修复前后对比

| 指标 | 修复前 | 修复后 | 改善 |
|-----|--------|--------|------|
| 硬编码常量不匹配数量 | 4 | 2 | ↓ 50% |
| ENCUMBRANCE_STAMINA_DRAIN_COEFF | 不匹配 | ✅ 匹配 | 已修复 |
| MEDIUM_RECOVERY_DURATION_MINUTES | 不匹配 | ✅ 匹配 | 已修复 |
| POSTURE_CROUCH_MULTIPLIER | 不匹配 | 不匹配 | 用途不同 |
| POSTURE_PRONE_MULTIPLIER | 不匹配 | 不匹配 | 用途不同 |

---

## 验证结果

运行 `detailed_comparison.py` 验证：

```
硬编码常量数值不匹配
----------------------------------------------------------------------------------------------------
  POSTURE_CROUCH_MULTIPLIER:
    C文件:  1.8
    Python:  0.7
    差异:    1.1
  POSTURE_PRONE_MULTIPLIER:
    C文件:  3.0
    Python:  0.3
    差异:    2.7

汇总统计
====================================================================================================
  优化参数数量:  41
  硬编码常量数量: 190
  总计参数数量:  231
  优化参数不匹配: 0
  硬编码常量不匹配: 2  (从4减少到2)
====================================================================================================
```

---

## 下一步建议

### 高优先级
1. **将 ENCUMBRANCE_STAMINA_DRAIN_COEFF 添加到优化器配置**
   - 当前值：2.0（硬编码）
   - 建议：让优化器决定最佳值
   - 好处：可以针对不同预设（StandardMilsim, TacticalAction, EliteStandard）优化不同的值

2. **将 MEDIUM_RECOVERY_DURATION_MINUTES 添加到优化器配置**
   - 当前值：5.0（硬编码）
   - 建议：让优化器决定最佳值
   - 好处：可以针对不同预设优化恢复期持续时间

### 中优先级
3. **重命名 Python 中的姿态倍数常量**
   - `POSTURE_CROUCH_MULTIPLIER` → `POSTURE_CROUCH_SPEED_MULTIPLIER`
   - `POSTURE_PRONE_MULTIPLIER` → `POSTURE_PRONE_SPEED_MULTIPLIER`
   - 好处：避免命名冲突，提高代码可读性

### 低优先级
4. **添加单元测试**
   - 验证C文件和Python数字孪生的计算结果一致
   - 特别是静态站立消耗和负重消耗的计算

---

## 修复文件清单

1. `scripts/Game/Components/Stamina/SCR_StaminaConstants.c`
   - 第96行：ENCUMBRANCE_STAMINA_DRAIN_COEFF 从1.5改为2.0
   - 第155行：MEDIUM_RECOVERY_DURATION_MINUTES 从8.5改为5.0

2. `scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c`
   - 第461行：更新注释，从"3-10分钟"改为"1.5-6.5分钟"

---

## 备注

- 所有修复都基于Python数字孪生文件（`rss_digital_twin_fix.py`）的值
- 修复后，C文件和Python数字孪生的行为更加一致
- 剩余的2个不匹配常量（POSTURE_CROUCH_MULTIPLIER, POSTURE_PRONE_MULTIPLIER）用途不同，不需要修复
- 建议定期运行对比脚本（`detailed_comparison.py`）以确保C文件和Python数字孪生保持一致