#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
诊断帕累托前沿工具
分析优化结果，检查帕累托前沿的多样性和解的数量
"""

import json
import numpy as np
from pathlib import Path
from typing import Dict, List, Tuple
import optuna


def load_study(study_name: str = "rss_super_optimization") -> optuna.Study:
    """
    加载Optuna研究
    
    Args:
        study_name: 研究名称
    
    Returns:
        Optuna研究对象
    """
    storage_url = f"sqlite:///{study_name}.db"
    
    # 检查数据库文件是否存在
    db_path = Path(f"{study_name}.db")
    if not db_path.exists():
        print(f"数据库文件不存在：{db_path}")
        print("请先运行优化以创建数据库")
        return None
    
    try:
        # 尝试从SQLite数据库加载
        study = optuna.load_study(study_name=study_name, storage=storage_url)
        return study
    except Exception as e:
        print(f"无法从数据库加载研究：{e}")
        print(f"数据库文件：{db_path.absolute()}")
        print("请确保优化已经运行并保存到数据库")
        return None


def analyze_pareto_front(study: optuna.Study) -> Dict:
    """
    分析帕累托前沿
    
    Args:
        study: Optuna研究对象
    
    Returns:
        分析结果字典
    """
    if study is None:
        return None
    
    # 获取所有完成的试验
    trials = [t for t in study.trials if t.state == optuna.trial.TrialState.COMPLETE]
    
    if len(trials) == 0:
        print("没有完成的试验")
        return None
    
    print(f"总试验数：{len(trials)}")
    
    # 提取目标值
    objectives = np.array([[t.values[0], t.values[1], t.values[2]] for t in trials])
    realism_values = objectives[:, 0]
    playability_values = objectives[:, 1]
    stability_values = objectives[:, 2]
    
    # 获取帕累托前沿
    best_trials = study.best_trials
    print(f"\n帕累托前沿解数量：{len(best_trials)}")
    
    if len(best_trials) == 0:
        print("警告：帕累托前沿为空！")
        return None
    
    # 分析目标值范围
    pareto_objectives = np.array([[t.values[0], t.values[1], t.values[2]] for t in best_trials])
    pareto_realism = pareto_objectives[:, 0]
    pareto_playability = pareto_objectives[:, 1]
    pareto_stability = pareto_objectives[:, 2]
    
    print(f"\n目标值范围（所有试验）：")
    print(f"  拟真损失：[{realism_values.min():.4f}, {realism_values.max():.4f}]")
    print(f"  可玩性负担：[{playability_values.min():.2f}, {playability_values.max():.2f}]")
    print(f"  稳定性风险：[{stability_values.min():.2f}, {stability_values.max():.2f}]")
    
    print(f"\n目标值范围（帕累托前沿）：")
    print(f"  拟真损失：[{pareto_realism.min():.4f}, {pareto_realism.max():.4f}]")
    print(f"  可玩性负担：[{pareto_playability.min():.2f}, {pareto_playability.max():.2f}]")
    print(f"  稳定性风险：[{pareto_stability.min():.2f}, {pareto_stability.max():.2f}]")
    
    # 检查目标值是否完全相同
    realism_unique = len(np.unique(pareto_realism))
    playability_unique = len(np.unique(pareto_playability))
    stability_unique = len(np.unique(pareto_stability))
    
    print(f"\n帕累托前沿唯一值数量：")
    print(f"  拟真损失唯一值：{realism_unique}/{len(best_trials)}")
    print(f"  可玩性负担唯一值：{playability_unique}/{len(best_trials)}")
    print(f"  稳定性风险唯一值：{stability_unique}/{len(best_trials)}")
    
    # 分析参数多样性
    if len(best_trials) > 0:
        param_names = list(best_trials[0].params.keys())
        param_diversity = {}
        
        for param_name in param_names:
            values = [t.params[param_name] for t in best_trials]
            unique_values = len(set(values))
            param_diversity[param_name] = {
                'unique_count': unique_values,
                'total_count': len(best_trials),
                'diversity_ratio': unique_values / len(best_trials) if len(best_trials) > 0 else 0
            }
        
        # 找出多样性最低的参数
        low_diversity_params = [
            (name, info) for name, info in param_diversity.items()
            if info['diversity_ratio'] < 0.1  # 少于10%的解有不同值
        ]
        
        print(f"\n参数多样性分析：")
        print(f"  总参数数：{len(param_names)}")
        print(f"  低多样性参数（<10%差异）：{len(low_diversity_params)}")
        
        if len(low_diversity_params) > 0:
            print(f"\n  低多样性参数列表（前10个）：")
            low_diversity_params.sort(key=lambda x: x[1]['diversity_ratio'])
            for name, info in low_diversity_params[:10]:
                print(f"    {name}: {info['unique_count']}/{info['total_count']} ({info['diversity_ratio']*100:.1f}%)")
    
    # 检查解之间的参数差异
    if len(best_trials) > 1:
        param_differences = []
        for i, trial_i in enumerate(best_trials):
            for j, trial_j in enumerate(best_trials):
                if i < j:
                    # 计算参数差异（欧氏距离）
                    params_i = np.array([trial_i.params[p] for p in param_names])
                    params_j = np.array([trial_j.params[p] for p in param_names])
                    diff = np.linalg.norm(params_i - params_j)
                    param_differences.append((i, j, diff))
        
        if len(param_differences) > 0:
            param_differences.sort(key=lambda x: x[2])
            min_diff = param_differences[0][2]
            max_diff = param_differences[-1][2]
            avg_diff = np.mean([d[2] for d in param_differences])
            
            print(f"\n参数差异分析：")
            print(f"  最小差异：{min_diff:.6e}")
            print(f"  最大差异：{max_diff:.6e}")
            print(f"  平均差异：{avg_diff:.6e}")
            
            if min_diff < 1e-10:
                print(f"  警告：存在参数完全相同的解对！")
    
    return {
        'total_trials': len(trials),
        'pareto_size': len(best_trials),
        'objectives_range': {
            'realism': (pareto_realism.min(), pareto_realism.max()),
            'playability': (pareto_playability.min(), pareto_playability.max()),
            'stability': (pareto_stability.min(), pareto_stability.max())
        },
        'objectives_uniqueness': {
            'realism': realism_unique,
            'playability': playability_unique,
            'stability': stability_unique
        },
        'param_diversity': param_diversity if len(best_trials) > 0 else None
    }


def main():
    """主函数"""
    print("=" * 80)
    print("帕累托前沿诊断工具")
    print("=" * 80)
    
    # 加载研究
    study = load_study("rss_super_optimization")
    
    if study is None:
        print("\n无法加载研究。请确保：")
        print("1. 优化已经运行完成")
        print("2. 研究名称正确（默认：rss_super_optimization）")
        print("3. 数据库文件存在（rss_super_optimization.db）")
        return
    
    # 分析帕累托前沿
    analysis = analyze_pareto_front(study)
    
    if analysis:
        print("\n" + "=" * 80)
        print("诊断总结")
        print("=" * 80)
        
        if analysis['pareto_size'] == 1:
            print("\n❌ 问题：帕累托前沿只有1个解")
            print("   建议：")
            print("   1. 增加优化迭代次数（n_trials）")
            print("   2. 增加NSGA-II种群大小（population_size）")
            print("   3. 增加变异概率（mutation_prob）")
            print("   4. 检查目标函数是否有足够的冲突")
        elif analysis['objectives_uniqueness']['realism'] == 1 and \
             analysis['objectives_uniqueness']['playability'] == 1 and \
             analysis['objectives_uniqueness']['stability'] == 1:
            print("\n⚠️ 警告：帕累托前沿所有解的目标值完全相同")
            print("   这说明三个目标函数可能过于一致，导致所有解都相同")
            print("   建议：")
            print("   1. 检查目标函数设计，确保有足够的冲突")
            print("   2. 调整目标函数权重")
            print("   3. 增加优化迭代次数")
        else:
            print(f"\n✅ 帕累托前沿有 {analysis['pareto_size']} 个解")
            print(f"   目标值多样性：")
            print(f"   - 拟真损失：{analysis['objectives_uniqueness']['realism']} 个唯一值")
            print(f"   - 可玩性负担：{analysis['objectives_uniqueness']['playability']} 个唯一值")
            print(f"   - 稳定性风险：{analysis['objectives_uniqueness']['stability']} 个唯一值")


if __name__ == '__main__':
    main()
