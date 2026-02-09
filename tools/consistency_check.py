#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
简单一致性检查：
- 使用 `constants_master.json` 的参数实例化数字孪生（RSSDigitalTwin）
- 运行若干基础场景（静止、匀速跑、冲刺）并检测数值问题（NaN、负数、溢出）
"""
import json
import math
from pathlib import Path
from rss_digital_twin_fix import RSSDigitalTwin, RSSConstants, MovementType, Stance

ROOT = Path(__file__).parent
JSON_PATH = ROOT / 'constants_master.json'


def load_params():
    data = json.loads(JSON_PATH.read_text(encoding='utf-8'))
    params = data.get('parameters', {})
    # convert keys to lower-case-keys expected by RSSConstants __init__? it accepts kwargs and maps attributes
    return params


def run_scenario(twin, duration_s, speed_m_s, weight, grade=0.0, terrain=1.0, stance=Stance.STAND, movement_type=MovementType.RUN):
    # build speed profile: [(0, speed), (duration, 0)] ensures simulate_scenario loops
    speed_profile = [(0.0, speed_m_s), (duration_s, speed_m_s)]
    twin.simulate_scenario(speed_profile, current_weight=weight, grade_percent=grade, terrain_factor=terrain, stance=stance, movement_type=movement_type, enable_randomness=False)
    return twin.stamina_history


def analyze_history(hist):
    arr = hist
    if not arr:
        return {'ok': False, 'reason': 'empty'}
    minv = min(arr)
    maxv = max(arr)
    has_nan = any(math.isnan(x) for x in arr)
    has_inf = any(math.isinf(x) for x in arr)
    return {'ok': not (has_nan or has_inf or minv < -0.01 or maxv > 1.5), 'min': minv, 'max': maxv, 'nan': has_nan, 'inf': has_inf}


def main():
    print('[consistency_check] 加载参数...')
    params = load_params()
    rc = RSSConstants(**{k: v for k, v in params.items() if isinstance(k, str)})
    twin = RSSDigitalTwin(rc)

    checks = []

    print('[consistency_check] 场景1：静止 120 秒（验证静态恢复）')
    run_scenario(twin, duration_s=120.0, speed_m_s=0.0, weight=30.0, stance=Stance.STAND, movement_type=MovementType.IDLE)
    res = analyze_history(twin.stamina_history)
    print('  ->', res)
    checks.append(('idle_120s', res))

    twin.reset()
    print('[consistency_check] 场景2：匀速跑 10 分钟（Run）')
    run_scenario(twin, duration_s=600.0, speed_m_s=3.7, weight=30.0, stance=Stance.STAND, movement_type=MovementType.RUN)
    res = analyze_history(twin.stamina_history)
    print('  ->', res)
    checks.append(('run_10min', res))

    twin.reset()
    print('[consistency_check] 场景3：冲刺短时 60 秒（Sprint）')
    run_scenario(twin, duration_s=60.0, speed_m_s=5.2, weight=20.0, stance=Stance.STAND, movement_type=MovementType.SPRINT)
    res = analyze_history(twin.stamina_history)
    print('  ->', res)
    checks.append(('sprint_60s', res))

    # summarize
    bad = [name for name, r in checks if not r['ok']]
    if bad:
        print('[consistency_check] 检查未通过，存在问题场景: ', bad)
        for name, r in checks:
            if not r['ok']:
                print(f' - {name}: min={r.get("min")}, max={r.get("max")}, nan={r.get("nan")}, inf={r.get("inf")}')
        return 1

    print('[consistency_check] 所有场景通过基本数值检查')
    return 0

if __name__ == '__main__':
    import sys
    exit(main())
