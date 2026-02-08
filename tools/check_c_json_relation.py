#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
检查 constants_master.json 中的键在代码中的映射关系：
- 是否出现在 SCR_StaminaConstants.c（作为 static const float 常量）
- 或者出现在 SCR_RSS_Settings.c（作为预设参数或字段）

输出分类报告
"""
import json
from pathlib import Path

ROOT = Path(__file__).parent
JSON_PATH = ROOT / 'constants_master.json'
C_CONSTS = ROOT.parent / 'scripts' / 'Game' / 'Components' / 'Stamina' / 'SCR_StaminaConstants.c'
C_SETTINGS = ROOT.parent / 'scripts' / 'Game' / 'Components' / 'Stamina' / 'SCR_RSS_Settings.c'

if not JSON_PATH.exists():
    print('ERR: constants_master.json not found')
    raise SystemExit(1)
if not C_CONSTS.exists():
    print('ERR: SCR_StaminaConstants.c not found')
    raise SystemExit(1)
if not C_SETTINGS.exists():
    print('ERR: SCR_RSS_Settings.c not found')
    raise SystemExit(1)

params = json.loads(JSON_PATH.read_text(encoding='utf-8')).get('parameters', {})
consts_text = C_CONSTS.read_text(encoding='utf-8')
settings_text = C_SETTINGS.read_text(encoding='utf-8')

report = {
    'in_consts': [],
    'in_settings': [],
    'in_both': [],
    'missing': []
}

for key in sorted(params.keys()):
    found_in_consts = (key in consts_text)
    # in settings we look for field uses like 'encumbrance_speed_penalty_max' lower-case OR direct uppercase
    lower_key = key.lower()
    found_in_settings = (lower_key in settings_text) or (key in settings_text)

    if found_in_consts and found_in_settings:
        report['in_both'].append(key)
    elif found_in_consts:
        report['in_consts'].append(key)
    elif found_in_settings:
        report['in_settings'].append(key)
    else:
        report['missing'].append(key)

print('C <-> JSON 关系检查报告')
print('=================================')
print(f"Total keys in JSON: {len(params)}")
print(f"Found in SCR_StaminaConstants.c only: {len(report['in_consts'])}")
for k in report['in_consts'][:50]:
    print('  -', k)
print(f"Found in SCR_RSS_Settings.c only: {len(report['in_settings'])}")
for k in report['in_settings'][:50]:
    print('  -', k)
print(f"Found in both: {len(report['in_both'])}")
for k in report['in_both'][:50]:
    print('  -', k)
print(f"Missing in both C files: {len(report['missing'])}")
for k in report['missing'][:50]:
    print('  -', k)

# Suggest next steps for missing items
if report['missing']:
    print('\n建议: 对缺失项进行审查: 这些项既不是全局常量也不是配置预设字段，可能需要:')
    print('  - 将其加入 SCR_StaminaConstants.c（若应为全局常量）')
    print('  - 或者加入 SCR_RSS_Settings.c 的参数/预设（若应为配置）')
    print('  - 或者从 constants_master.json 中移除（若不再需要）')
else:
    print('\n所有 JSON 键已在 C 文件中找到（作为全局常量或设置字段）。')
