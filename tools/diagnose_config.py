#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS配置诊断脚本
RSS Config Diagnosis Script

功能：
1. 分析当前配置的问题
2. 对比修复前后的参数范围
3. 识别需要重新优化的参数
4. 生成优化建议
"""

import json
import sys
from pathlib import Path

# 设置输出编码为UTF-8
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))

def analyze_config(config_file, config_name):
    """分析单个配置文件"""
    print(f"\n{'='*80}")
    print(f"分析配置文件: {config_name}")
    print(f"{'='*80}")

    with open(config_file, 'r', encoding='utf-8') as f:
        config = json.load(f)

    objectives = config.get('objectives', {})
    parameters = config.get('parameters', {})

    print(f"\n目标函数值:")
    print(f"  playability_burden: {objectives.get('playability_burden', 'N/A')}")
    print(f"  stability_risk: {objectives.get('stability_risk', 'N/A')}")

    # 修复后的参数范围
    new_ranges = {
        'prone_recovery_multiplier': (2.0, 2.8),
        'standing_recovery_multiplier': (1.3, 1.8),
        'fast_recovery_multiplier': (1.6, 2.4),
        'medium_recovery_multiplier': (1.0, 1.5),
        'slow_recovery_multiplier': (0.5, 0.8),
        'base_recovery_rate': (1.5e-4, 4.0e-4),
        'encumbrance_stamina_drain_coeff': (0.8, 2.0),
    }

    print(f"\n关键参数分析:")
    issues = []
    warnings = []

    for param, (min_val, max_val) in new_ranges.items():
        if param in parameters:
            value = parameters[param]
            in_range = min_val <= value <= max_val
            status = "✅" if in_range else "❌"

            if not in_range:
                issues.append(f"{param}: {value} (范围: {min_val}-{max_val})")
            elif value < min_val * 1.1 or value > max_val * 0.9:
                warnings.append(f"{param}: {value} (范围: {min_val}-{max_val}, 接近边界)")

            print(f"  {status} {param}: {value:.6f} (范围: {min_val:.6f}-{max_val:.6f})")
        else:
            print(f"  ⚠️  {param}: 未在配置中找到")

    # 检查约束条件
    print(f"\n约束条件检查:")

    # 恢复倍数约束
    prone = parameters.get('prone_recovery_multiplier', 0)
    standing = parameters.get('standing_recovery_multiplier', 0)
    fast = parameters.get('fast_recovery_multiplier', 0)
    medium = parameters.get('medium_recovery_multiplier', 0)
    slow = parameters.get('slow_recovery_multiplier', 0)

    # 姿态约束：prone > standing
    if prone > standing:
        print(f"  ✅ prone > standing: {prone:.3f} > {standing:.3f}")
    else:
        print(f"  ❌ prone > standing: {prone:.3f} ≤ {standing:.3f} (违反约束)")
        issues.append(f"恢复倍数约束: prone ({prone:.3f}) ≤ standing ({standing:.3f})")

    # 恢复阶段约束：fast > medium > slow
    if fast > medium > slow:
        print(f"  ✅ fast > medium > slow: {fast:.3f} > {medium:.3f} > {slow:.3f}")
    else:
        print(f"  ❌ fast > medium > slow: {fast:.3f} > {medium:.3f} > {slow:.3f} (违反约束)")
        issues.append(f"恢复阶段约束: fast ({fast:.3f}) ≤ medium ({medium:.3f}) 或 medium ≤ slow ({slow:.3f})")

    # 姿态倍数约束
    crouch = parameters.get('posture_crouch_multiplier', 0)
    prone_mult = parameters.get('posture_prone_multiplier', 0)

    if crouch < 1.0:
        print(f"  ✅ posture_crouch < 1.0: {crouch:.3f}")
    else:
        print(f"  ❌ posture_crouch < 1.0: {crouch:.3f} (违反约束)")
        issues.append(f"姿态约束: posture_crouch ({crouch:.3f}) ≥ 1.0")

    if prone_mult < 1.0:
        print(f"  ✅ posture_prone < 1.0: {prone_mult:.3f}")
    else:
        print(f"  ❌ posture_prone < 1.0: {prone_mult:.3f} (违反约束)")
        issues.append(f"姿态约束: posture_prone ({prone_mult:.3f}) ≥ 1.0")

    if prone_mult < crouch:
        print(f"  ✅ posture_prone < posture_crouch: {prone_mult:.3f} < {crouch:.3f}")
    else:
        print(f"  ❌ posture_prone < posture_crouch: {prone_mult:.3f} ≥ {crouch:.3f} (违反约束)")
        issues.append(f"姿态约束: posture_prone ({prone_mult:.3f}) ≥ posture_crouch ({crouch:.3f})")

    # 目标函数值评估
    print(f"\n目标函数评估:")
    playability = objectives.get('playability_burden', 0)
    stability = objectives.get('stability_risk', 0)

    if playability > 1000:
        print(f"  ⚠️  playability_burden过高: {playability:.1f} (期望 < 500)")
        issues.append(f"可玩性负担过高: {playability:.1f}")

    if stability > 3000:
        print(f"  ⚠️  stability_risk过高: {stability:.1f} (期望 < 1000)")
        issues.append(f"稳定性风险过高: {stability:.1f}")

    return issues, warnings

def main():
    """主函数"""
    print("\n" + "="*80)
    print("RSS配置诊断脚本")
    print("="*80)

    config_files = [
        (project_root / "optimized_rss_config_playability_super.json", "Playability配置"),
        (project_root / "optimized_rss_config_balanced_super.json", "Balanced配置"),
    ]

    all_issues = []
    all_warnings = []

    for config_file, config_name in config_files:
        if config_file.exists():
            issues, warnings = analyze_config(config_file, config_name)
            all_issues.extend([(config_name, issue) for issue in issues])
            all_warnings.extend([(config_name, warning) for warning in warnings])
        else:
            print(f"\n⚠️  警告：找不到配置文件 {config_file}")

    # 总结
    print(f"\n{'='*80}")
    print("诊断总结")
    print(f"{'='*80}")

    if all_issues:
        print(f"\n❌ 发现 {len(all_issues)} 个严重问题:")
        for config_name, issue in all_issues:
            print(f"  [{config_name}] {issue}")
    else:
        print("\n✅ 未发现严重问题")

    if all_warnings:
        print(f"\n⚠️  发现 {len(all_warnings)} 个警告:")
        for config_name, warning in all_warnings:
            print(f"  [{config_name}] {warning}")

    # 建议
    print(f"\n{'='*80}")
    print("优化建议")
    print(f"{'='*80}")

    if all_issues or all_warnings:
        print("\n1. 重新运行优化器生成新配置:")
        print("   python tools/rss_super_pipeline.py --trials 2000")

        print("\n2. 如果问题仍然存在，考虑以下调整:")
        print("   - 进一步放宽约束条件")
        print("   - 调整目标函数权重")
        print("   - 缩小参数搜索范围")

        print("\n3. 检查优化器的收敛性:")
        print("   - 观察目标函数值是否下降")
        print("   - 检查是否有明显的收敛趋势")
        print("   - 查看帕累托前沿是否多样化")
    else:
        print("\n✅ 配置良好，无需调整")

    print(f"\n{'='*80}")

if __name__ == "__main__":
    main()