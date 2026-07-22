#!/usr/bin/env python3
"""对比 v4 vs v6 优化预设参数"""
import json

presets = ['elitestandard', 'standardmilsim', 'tacticalaction']
print('=== v4 vs v6 参数对比 ===')
print()

for preset in presets:
    v4_file = f'optimized_rss_config_{preset}_v4.json'
    v6_file = f'optimized_rss_config_{preset}_v6.json'
    
    with open(v4_file) as f:
        v4 = json.load(f)
    with open(v6_file) as f:
        v6 = json.load(f)
    
    sep = '=' * 60
    print(sep)
    print(f'  {preset}')
    print(sep)
    
    print('  --- v4 关键参数 ---')
    for k in ['energy_to_stamina_coeff', 'base_recovery_rate',
              'encumbrance_speed_penalty_coeff', 'sprint_speed_boost',
              'sprint_enable_threshold']:
        v4v, v6v = v4.get(k, 0), v6.get(k, 0)
        if v4v and v6v:
            diff = ((v6v - v4v) / v4v * 100)
            arrow = '↑' if diff > 1 else ('↓' if diff < -1 else '≈')
            print(f'    {k:45s}  v4={v4v:>12.6f}  v6={v6v:>12.6f}  {arrow} {diff:+.1f}%')
    
    print('  --- v6 CP-W\' 参数 ---')
    for k in ['critical_power_watts', 'w_prime_max_joules',
              'w_prime_recovery_w_per_s', 'sprint_power_cap_watts']:
        if k in v6:
            print(f'    {k:45s}  v6={v6[k]:>12.1f}')
    
    if '_metrics_v6' in v6:
        print('  --- 指标 ---')
        for k, v in v6['_metrics_v6'].items():
            print(f'    {k:45s}  {v:.4f}')
    print()
