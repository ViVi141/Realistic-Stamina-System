#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
简单测试：确保生理学评分被正确转换为损失（越小越好）
"""
from rss_super_pipeline import RSSSuperPipeline
from rss_digital_twin_fix import RSSConstants

p = RSSSuperPipeline(n_trials=1, n_jobs=1, use_database=False)

# 基准常量（应产生较高 realism score -> 低 loss）
const_good = RSSConstants()
const_good.BASE_RECOVERY_RATE = 0.00018  # 在理想区间
score_good = p._evaluate_physiological_realism(const_good)
loss_good = max(0.0, 100.0 - score_good)
print(f"good realism score={score_good:.2f}, loss={loss_good:.2f}")

# 异常常量（会被惩罚 -> 低 score -> 高 loss）
const_bad = RSSConstants()
const_bad.BASE_RECOVERY_RATE = 1.0  # 极端不合理
score_bad = p._evaluate_physiological_realism(const_bad)
loss_bad = max(0.0, 100.0 - score_bad)
print(f"bad realism score={score_bad:.2f}, loss={loss_bad:.2f}")

assert loss_bad >= loss_good, "预期异常常量的损失应不小于基准常量的损失"
print('测试通过：生理学损失转换正常')
