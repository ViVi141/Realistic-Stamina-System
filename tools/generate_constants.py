#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成并同步常量：从 `constants_master.json` 生成/更新 Python 常量文件 `tools/stamina_constants.py`。
并校验 C 文件 `SCR_StaminaConstants.c` 中对应常量是否与 JSON 一致。

用法：
    python tools/generate_constants.py
"""

import json
import re
from pathlib import Path
import math

ROOT = Path(__file__).parent
JSON_PATH = ROOT / 'constants_master.json'
PY_PATH = ROOT / 'stamina_constants.py'
C_PATH = ROOT.parent / 'scripts' / 'Game' / 'Components' / 'Stamina' / 'SCR_StaminaConstants.c'

# 列出我们要同步的常量映射（JSON_KEY -> (PY_VAR, C_REGEX_NAME))
SYNC_MAP = {
    'BASE_RECOVERY_RATE': ('BASE_RECOVERY_RATE', r'BASE_RECOVERY_RATE'),
    'FAST_RECOVERY_DURATION_MINUTES': ('FAST_RECOVERY_DURATION_MINUTES', r'FAST_RECOVERY_DURATION_MINUTES'),
    'FAST_RECOVERY_MULTIPLIER': ('FAST_RECOVERY_MULTIPLIER', r'FAST_RECOVERY_MULTIPLIER'),
    'MEDIUM_RECOVERY_START_MINUTES': ('MEDIUM_RECOVERY_START_MINUTES', r'MEDIUM_RECOVERY_START_MINUTES'),
    'MEDIUM_RECOVERY_DURATION_MINUTES': ('MEDIUM_RECOVERY_DURATION_MINUTES', r'MEDIUM_RECOVERY_DURATION_MINUTES'),
    'MEDIUM_RECOVERY_MULTIPLIER': ('MEDIUM_RECOVERY_MULTIPLIER', r'MEDIUM_RECOVERY_MULTIPLIER'),
    'STANDING_RECOVERY_MULTIPLIER': ('STANDING_RECOVERY_MULTIPLIER', r'STANDING_RECOVERY_MULTIPLIER'),
    'CROUCHING_RECOVERY_MULTIPLIER': ('CROUCHING_RECOVERY_MULTIPLIER', r'CROUCHING_RECOVERY_MULTIPLIER'),
    'PRONE_RECOVERY_MULTIPLIER': ('PRONE_RECOVERY_MULTIPLIER', r'PRONE_RECOVERY_MULTIPLIER'),
    'LOAD_RECOVERY_PENALTY_COEFF': ('LOAD_RECOVERY_PENALTY_COEFF', r'LOAD_RECOVERY_PENALTY_COEFF'),
    'LOAD_RECOVERY_PENALTY_EXPONENT': ('LOAD_RECOVERY_PENALTY_EXPONENT', r'LOAD_RECOVERY_PENALTY_EXPONENT'),
    'BODY_TOLERANCE_BASE': ('BODY_TOLERANCE_BASE', r'BODY_TOLERANCE_BASE'),
    'ENCUMBRANCE_STAMINA_DRAIN_COEFF': ('ENCUMBRANCE_STAMINA_DRAIN_COEFF', r'ENCUMBRANCE_STAMINA_DRAIN_COEFF'),
    'ENCUMBRANCE_SPEED_PENALTY_COEFF': ('ENCUMBRANCE_SPEED_PENALTY_COEFF', r'ENCUMBRANCE_SPEED_PENALTY_COEFF'),
    'ENCUMBRANCE_SPEED_PENALTY_EXPONENT': ('ENCUMBRANCE_SPEED_PENALTY_EXPONENT', r'ENCUMBRANCE_SPEED_EXPONENT|ENCUMBRANCE_SPEED_PENALTY_EXPONENT'),
    'ENCUMBRANCE_SPEED_PENALTY_MAX': ('ENCUMBRANCE_SPEED_PENALTY_MAX', r'ENCUMBRANCE_SPEED_PENALTY_MAX'),
    'SPRINT_STAMINA_DRAIN_MULTIPLIER': ('SPRINT_STAMINA_DRAIN_MULTIPLIER', r'SPRINT_STAMINA_DRAIN_MULTIPLIER'),
    'PANDOLF_STATIC_COEFF_1': ('PANDOLF_STATIC_COEFF_1', r'PANDOLF_STATIC_COEFF_1'),
    'PANDOLF_STATIC_COEFF_2': ('PANDOLF_STATIC_COEFF_2', r'PANDOLF_STATIC_COEFF_2'),
    'ENERGY_TO_STAMINA_COEFF': ('ENERGY_TO_STAMINA_COEFF', r'ENERGY_TO_STAMINA_COEFF'),
    'AEROBIC_THRESHOLD': ('AEROBIC_THRESHOLD', r'AEROBIC_THRESHOLD'),
    'ANAEROBIC_THRESHOLD': ('ANAEROBIC_THRESHOLD', r'ANAEROBIC_THRESHOLD'),
    'AEROBIC_EFFICIENCY_FACTOR': ('AEROBIC_EFFICIENCY_FACTOR', r'AEROBIC_EFFICIENCY_FACTOR'),
    'ANAEROBIC_EFFICIENCY_FACTOR': ('ANAEROBIC_EFFICIENCY_FACTOR', r'ANAEROBIC_EFFICIENCY_FACTOR'),
}

TOL = 1e-9


def format_value_for_py(v):
    if isinstance(v, float):
        # prefer compact representation
        return repr(float(v))
    return repr(v)


def update_python_constants(data):
    text = PY_PATH.read_text(encoding='utf-8')
    backup = PY_PATH.with_suffix('.py.bak')
    backup.write_text(text, encoding='utf-8')

    updated = 0
    for key, (py_var, _) in SYNC_MAP.items():
        if key not in data:
            continue
        val = data[key]
        pattern = re.compile(rf'^({py_var})\s*=\s*.+$', re.M)
        repl = f"{py_var} = {format_value_for_py(val)}"
        if pattern.search(text):
            text = pattern.sub(repl, text)
            updated += 1
        else:
            # append if not found
            text += f"\n{repl}\n"
            updated += 1

    PY_PATH.write_text(text, encoding='utf-8')
    print(f"[generate_constants] 已更新 {updated} 个 Python 常量 (备份: {backup.name})")


def check_c_constants(data):
    # Check both the global constants file and the settings (preset) file.
    settings_path = C_PATH.parent / 'SCR_RSS_Settings.c'

    if not C_PATH.exists() and not settings_path.exists():
        print(f"[generate_constants] 警告：未找到 {C_PATH} 或 {settings_path}, 跳过校验")
        return

    c_text = C_PATH.read_text(encoding='utf-8') if C_PATH.exists() else ''
    s_text = settings_path.read_text(encoding='utf-8') if settings_path.exists() else ''

    mismatches = []
    missing_in_both = []
    found_in_settings = []

    for key, (_, c_name_pattern) in SYNC_MAP.items():
        if key not in data:
            continue

        # 1) Try to find in SCR_StaminaConstants.c as a static const float
        m = re.search(rf'static\s+const\s+float\s+({c_name_pattern})\s*=\s*([^;]+);', c_text)
        if m:
            name = m.group(1)
            raw = m.group(2).strip()
            try:
                c_val = float(raw)
            except:
                try:
                    c_val = float(raw.replace('f', ''))
                except:
                    c_val = None
            json_val = float(data[key])
            if c_val is None or abs(c_val - json_val) > max(TOL, abs(json_val) * 1e-6):
                mismatches.append((name, c_val, json_val))
            continue

        # 2) If not found in global constants, check if it's present as a settings field (lower-case) in SCR_RSS_Settings.c
        lower_key = key.lower()
        if s_text and (re.search(rf'\b{re.escape(lower_key)}\b', s_text) or re.search(rf'\b{re.escape(key)}\b', s_text)):
            found_in_settings.append(key)
            continue

        # 3) Not found in either
        missing_in_both.append((key, data[key]))

    # Report results
    if mismatches:
        print("[generate_constants] C 与 JSON 存在数值不一致项（在全局常量中）：")
        for name, c_val, j_val in mismatches:
            print(f"  - {name}: C={c_val}  JSON={j_val}")
    else:
        print("[generate_constants] 全局常量（SCR_StaminaConstants.c）与 JSON 的数值一致或未声明为静态常量。")

    if found_in_settings:
        print("[generate_constants] 以下键未在全局常量中找到，但存在于预设/设置文件 SCR_RSS_Settings.c（视为有效）：")
        for k in found_in_settings:
            print(f"  - {k}")

    if missing_in_both:
        print("[generate_constants] 以下键未在全局常量或设置文件中找到（需要审查）：")
        for k, val in missing_in_both:
            print(f"  - {k}: JSON={val}")


def main():
    print("[generate_constants] 正在加载 %s" % JSON_PATH)
    if not JSON_PATH.exists():
        print("ERROR: constants_master.json 不存在")
        return
    data = json.loads(JSON_PATH.read_text(encoding='utf-8'))
    params = data.get('parameters', {})
    if not params:
        print("ERROR: JSON 中未找到 'parameters' 字段或为空")
        return

    # 1) 更新 Python 常量
    update_python_constants(params)

    # 2) 校验 C 文件
    check_c_constants(params)

    print('[generate_constants] 完成。请查看输出并在需要时提交更改。')


if __name__ == '__main__':
    main()
