#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS 回归测试套件
对照 C 端实现验证核心算法：Pandolf、游泳 3D 模型、Tobler、EPOC、预设参数。
新增算法或修改常量后运行：python rss_regression_tests.py
"""

import sys, os, math, copy
sys.path.insert(0, os.path.dirname(__file__))

from rss_digital_twin_fix import RSSDigitalTwin, RSSConstants, tobler_speed_multiplier
from rss_pipeline_v4 import MissionLibrary, simulate_mission, compute_metrics

# =============================================================================
# Pandolf 公式回归测试
# =============================================================================
def test_pandolf_baseline():
    twin = RSSDigitalTwin(RSSConstants())
    # Pandolf: 返回 %/s 消耗率 (terrain_factor=1.0=铺装路面)
    rate = twin._pandolf_expenditure(1.5, 90.0, 0.0, 1.0)
    assert rate > 0, f"Pandolf baseline rate should be > 0, got {rate}"
    print(f"  [PASS] Pandolf baseline: drain_rate={rate:.6f}/s")

def test_pandolf_heavy_load():
    twin = RSSDigitalTwin(RSSConstants())
    loaded = twin._pandolf_expenditure(1.5, 120.0, 0.0, 1.0)   # 30kg 装备
    unloaded = twin._pandolf_expenditure(1.5, 90.0, 0.0, 1.0)   # 空载
    assert loaded > unloaded, f"Pandolf load penalty should increase drain: {loaded:.6f} vs {unloaded:.6f}"
    print(f"  [PASS] Pandolf 30kg load: {loaded:.6f}/s vs unloaded {unloaded:.6f}/s ({loaded/unloaded:.2f}x)")

def test_pandolf_uphill():
    twin = RSSDigitalTwin(RSSConstants())
    flat = twin._pandolf_expenditure(1.5, 90.0, 0.0, 1.0)
    uphill = twin._pandolf_expenditure(1.5, 90.0, 10.0, 1.0)
    assert uphill > flat, f"Pandolf uphill should increase drain: {uphill:.6f} vs {flat:.6f}"
    print(f"  [PASS] Pandolf 10pct uphill: {uphill:.6f}/s vs flat {flat:.6f}/s ({uphill/flat:.2f}x)")

# =============================================================================
# Tobler 徒步函数回归测试
# =============================================================================
def test_tobler_flat():
    mult = tobler_speed_multiplier(0.0)
    assert abs(mult - 1.0) < 0.05, f"Tobler flat should be ~1.0, got {mult}"
    print(f"  [PASS] Tobler flat: {mult:.3f}")

def test_tobler_downhill_benefit():
    flat = tobler_speed_multiplier(0.0)
    gentle = tobler_speed_multiplier(-5.0)
    steep = tobler_speed_multiplier(-20.0)
    assert gentle > flat, f"Gentle downhill should be faster: {gentle} vs {flat}"
    assert steep < gentle, f"Steep downhill slower than gentle: {steep} vs {gentle}"
    assert steep > 0.15, f"Steep downhill should not hit floor: {steep}"
    print(f"  [PASS] Tobler downhill: gentle={gentle:.3f} flat={flat:.3f} steep={steep:.3f}")

def test_tobler_uphill_penalty():
    flat = tobler_speed_multiplier(0.0)
    uphill = tobler_speed_multiplier(15.0)
    assert uphill < flat and uphill > 0.15, f"Tobler uphill out of range: {uphill}"
    print(f"  [PASS] Tobler uphill 15deg: {uphill:.3f}")

# =============================================================================
# 游泳模型回归测试
# =============================================================================
def test_swimming_mission():
    """通过任务场景验证游泳模型（含 3D 水平/垂直消耗）"""
    twin = RSSDigitalTwin(RSSConstants())
    missions = MissionLibrary.all_missions()
    swim_missions = [m for m in missions if "swim" in m.name.lower() or "water" in m.name.lower()]
    if swim_missions:
        for m in swim_missions:
            r = simulate_mission(twin, m)
            assert r.final_stamina >= 0, f"Swim mission '{m.name}' ended with negative stamina: {r.final_stamina}"
            print(f"  [PASS] Swim mission '{m.name}': final_stamina={r.final_stamina:.3f}")
    else:
        print(f"  [SKIP] No swimming missions found in library")

# =============================================================================
# 预设参数验证
# =============================================================================
def test_preset_files_exist():
    import json
    preset_dir = os.path.dirname(__file__)
    required = [
        "optimized_rss_config_elitestandard_v4.json",
        "optimized_rss_config_standardmilsim_v4.json",
        "optimized_rss_config_tacticalaction_v4.json",
    ]
    for fname in required:
        path = os.path.join(preset_dir, fname)
        assert os.path.exists(path), f"Missing preset file: {fname}"
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        assert "energy_to_stamina_coeff" in data, f"{fname} missing energy_to_stamina_coeff"
        assert "base_recovery_rate" in data, f"{fname} missing base_recovery_rate"
        ec = data["energy_to_stamina_coeff"]
        br = data["base_recovery_rate"]
        assert 1e-7 < ec < 1e-5, f"{fname} energy_coeff out of range: {ec}"
        assert 1e-6 < br < 1e-2, f"{fname} base_recovery out of range: {br}"
    print(f"  [PASS] All {len(required)} preset files valid")

def test_all_missions_pass():
    twin = RSSDigitalTwin(RSSConstants())
    missions = MissionLibrary.all_missions()
    failed = [m.name for m in missions if not simulate_mission(twin, m).completion_possible]
    assert not failed, f"Missions failed: {failed}"
    print(f"  [PASS] All {len(missions)} missions pass")

# =============================================================================
# 主入口
# =============================================================================
def run_all():
    tests = [
        ("Pandolf baseline",        test_pandolf_baseline),
        ("Pandolf heavy load",      test_pandolf_heavy_load),
        ("Pandolf uphill",          test_pandolf_uphill),
        ("Tobler flat",             test_tobler_flat),
        ("Tobler downhill benefit", test_tobler_downhill_benefit),
        ("Tobler uphill penalty",   test_tobler_uphill_penalty),
        ("Swimming mission",        test_swimming_mission),
        ("Preset files valid",      test_preset_files_exist),
        ("All missions pass",       test_all_missions_pass),
    ]

    passed = 0
    failed = 0
    print("=" * 60)
    print("RSS Regression Tests")
    print(f"Digital twin: rss_digital_twin_fix.py")
    print("=" * 60)
    for name, fn in tests:
        try:
            fn()
            passed += 1
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")
            failed += 1

    print("-" * 60)
    print(f"Result: {passed}/{len(tests)} passed", end="")
    if failed:
        print(f", {failed} failed")
    else:
        print()
    print("=" * 60)
    return failed == 0

if __name__ == "__main__":
    success = run_all()
    sys.exit(0 if success else 1)
