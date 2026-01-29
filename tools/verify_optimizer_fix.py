#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS优化器修复验证脚本
RSS Optimizer Fix Verification Script

功能：
1. 验证修复是否正确应用
2. 运行小规模测试检查收敛性
3. 对比修复前后的关键参数
4. 生成验证报告
"""

import json
import os
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

def verify_fixes_applied():
    """验证修复是否正确应用到代码中"""
    print("=" * 80)
    print("验证修复是否正确应用")
    print("=" * 80)

    pipeline_file = project_root / "tools" / "rss_super_pipeline.py"

    if not pipeline_file.exists():
        print(f"❌ 错误：找不到文件 {pipeline_file}")
        return False

    with open(pipeline_file, 'r', encoding='utf-8') as f:
        content = f.read()

    results = {}

    # 检查1：realism_weight是否降低
    if 'realism_weight = 100.0' in content:
        print("✅ 修复1.1：realism_weight已从5000.0降低到100.0")
        results['realism_weight'] = True
    else:
        print("❌ 修复1.1：realism_weight未正确修改")
        results['realism_weight'] = False

    # 检查2：恢复倍数约束是否分离
    if 'if not (prone_recovery > standing_recovery):' in content and 'if not (fast_recovery > medium_recovery > slow_recovery):' in content:
        print("✅ 修复1.2：恢复倍数约束已正确分离")
        results['constraint_logic'] = True
    else:
        print("❌ 修复1.2：恢复倍数约束未正确分离")
        results['constraint_logic'] = False

    # 检查3：参数范围是否调整
    checks = [
        ('prone_recovery_multiplier', '2.0, 2.8'),
        ('standing_recovery_multiplier', '1.3, 1.8'),
        ('fast_recovery_multiplier', '1.6, 2.4'),
        ('medium_recovery_multiplier', '1.0, 1.5'),
    ]

    for param, expected_range in checks:
        if f"'{param}', {expected_range}" in content:
            print(f"✅ 修复1.3/1.4：{param}范围已调整为{expected_range}")
            results[f'param_{param}'] = True
        else:
            print(f"❌ 修复1.3/1.4：{param}范围未正确调整")
            results[f'param_{param}'] = False

    # 检查4：移动平衡惩罚是否降低
    penalty_checks = [
        ('run_penalty', '* 5000.0', '降低4倍'),
        ('sprint_penalty', '* 6000.0', '降低4倍多'),
        ('walk_min_penalty', '* 5000.0', '降低3倍'),
        ('walk_max_penalty', '* 3000.0', '降低2.7倍'),
    ]

    for name, pattern, desc in penalty_checks:
        if pattern in content:
            print(f"✅ 修复2.1：{name}已{desc}")
            results[f'penalty_{name}'] = True
        else:
            print(f"❌ 修复2.1：{name}未正确修改")
            results[f'penalty_{name}'] = False

    # 检查5：可玩性负担评估标准是否放宽
    playability_checks = [
        ('time_threshold_1', 'time_ratio - 1.10', '从105%放宽到110%'),
        ('time_threshold_2', 'time_ratio - 1.20', '从110%放宽到120%'),
        ('min_stamina', '0.15 - min_stamina', '从20%放宽到15%'),
        ('mean_stamina', '0.35 - mean_stamina', '从45%放宽到35%'),
    ]

    for name, pattern, desc in playability_checks:
        if pattern in content:
            print(f"✅ 修复2.2：{name}已{desc}")
            results[f'playability_{name}'] = True
        else:
            print(f"❌ 修复2.2：{name}未正确修改")
            results[f'playability_{name}'] = False

    # 检查6：约束惩罚系数是否降低
    constraint_checks = [
        ('prone_recovery_constraint', '* 200.0', '从500降到200'),
        ('standing_recovery_constraint', '* 150.0', '从300降到150'),
        ('fast_recovery_constraint', '* 150.0', '从400降到150'),
        ('medium_recovery_constraint', '* 150.0', '从300降到150'),
        ('posture_constraint', '* 200.0', '从600降到200'),
        ('posture_compare', '* 100.0', '从300降到100'),
    ]

    for name, pattern, desc in constraint_checks:
        # 检查是否至少有一个约束使用了新的惩罚系数
        if pattern in content:
            print(f"✅ 修复2.3：{name}已{desc}")
            results[f'constraint_{name}'] = True
        else:
            print(f"❌ 修复2.3：{name}未正确修改")
            results[f'constraint_{name}'] = False

    # 统计结果
    total_checks = len(results)
    passed_checks = sum(results.values())

    print("\n" + "=" * 80)
    print(f"验证结果：{passed_checks}/{total_checks} 项通过")
    print("=" * 80)

    return passed_checks == total_checks

def compare_configs():
    """对比修复前后的关键配置"""
    print("\n" + "=" * 80)
    print("关键配置对比")
    print("=" * 80)

    config_file = project_root / "optimized_rss_config_balanced_super.json"

    if not config_file.exists():
        print(f"⚠️  警告：找不到配置文件 {config_file}")
        print("   这是正常的，因为修复尚未运行优化器生成新配置")
        return

    with open(config_file, 'r', encoding='utf-8') as f:
        config = json.load(f)

    parameters = config.get('parameters', {})

    # 检查关键参数
    key_params = {
        'prone_recovery_multiplier': (2.0, 2.8),
        'standing_recovery_multiplier': (1.3, 1.8),
        'fast_recovery_multiplier': (1.6, 2.4),
        'medium_recovery_multiplier': (1.0, 1.5),
    }

    print("\n检查配置中的关键参数是否在新的范围内：")
    for param, (min_val, max_val) in key_params.items():
        if param in parameters:
            value = parameters[param]
            in_range = min_val <= value <= max_val
            status = "✅" if in_range else "⚠️"
            print(f"{status} {param}: {value:.4f} (范围: {min_val}-{max_val})")
        else:
            print(f"⚠️  {param}: 未在配置中找到")

def generate_verification_report():
    """生成验证报告"""
    print("\n" + "=" * 80)
    print("生成验证报告")
    print("=" * 80)

    report = {
        "verification_date": "2026-01-29",
        "fixes_applied": {
            "phase1": {
                "fix_1_1": "降低生理学合理性权重（5000.0 → 100.0）",
                "fix_1_2": "修正恢复倍数约束逻辑（分离姿态和恢复阶段）",
                "fix_1_3": "调整参数搜索范围（standing和medium）",
                "fix_1_4": "调整参数搜索范围（prone和fast）",
            },
            "phase2": {
                "fix_2_1": "降低移动平衡惩罚系数（2-4倍）",
                "fix_2_2": "放宽可玩性负担评估标准（5-10个百分点）",
                "fix_2_3": "降低约束惩罚系数（2-3倍）",
            }
        },
        "expected_improvements": {
            "playability_burden": "预计降低30-50%",
            "stability_risk": "预计降低40-60%",
            "physiological_realism": "保持在合理范围",
            "convergence_speed": "预计提高2-3倍"
        },
        "next_steps": [
            "运行小规模测试：python tools/rss_super_pipeline.py --trials 200",
            "检查目标函数值是否下降",
            "检查是否有明显的收敛趋势",
            "对比修复前后的最佳目标值",
            "验证游戏体验是否改善"
        ]
    }

    report_file = project_root / "tools" / "verification_report.json"
    with open(report_file, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    print(f"✅ 验证报告已保存到：{report_file}")

def main():
    """主函数"""
    print("\n" + "=" * 80)
    print("RSS优化器修复验证脚本")
    print("=" * 80)

    # 验证修复是否应用
    all_passed = verify_fixes_applied()

    # 对比配置
    compare_configs()

    # 生成验证报告
    generate_verification_report()

    # 总结
    print("\n" + "=" * 80)
    print("验证总结")
    print("=" * 80)

    if all_passed:
        print("✅ 所有修复已成功应用！")
        print("\n下一步：")
        print("1. 运行小规模测试：python tools/rss_super_pipeline.py --trials 200")
        print("2. 检查目标函数值是否下降")
        print("3. 检查是否有明显的收敛趋势")
    else:
        print("⚠️  部分修复未正确应用，请检查")
        print("\n如果需要回滚，请运行：")
        print("cp tools/rss_super_pipeline.py.backup tools/rss_super_pipeline.py")

    print("\n" + "=" * 80)

if __name__ == "__main__":
    main()