#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
将 v4/v6 优化器导出的 JSON 预设合并写入 SCR_RSS_SettingsPresetBake.c。
仅覆盖 JSON 中出现的键，其余 Init*Defaults / ApplyV6TierCpDefaults 字段保持原 C 值不变。
"""

import json
import re
from pathlib import Path
from typing import Dict, List, Tuple

V6_TIER_BY_PRESET = {
    'EliteStandard': 0,
    'StandardMilsim': 1,
    'TacticalAction': 2,
}

V6_PARAM_KEYS = {
    'critical_power_watts',
    'w_prime_max_joules',
    'w_prime_recovery_w_per_s',
    'sprint_power_cap_watts',
    'v5_walk_speed_ms',
    'v5_run_speed_ms',
    'v5_sprint_speed_ms',
    'burst_cooldown_full_seconds',
    'burst_cooldown_short_seconds',
}


def format_c_float(value) -> str:
    """格式化为 EnforceScript 可解析的浮点字面量。"""
    if isinstance(value, bool):
        if value:
            return "1.0"
        return "0.0"
    if not isinstance(value, (int, float)):
        return str(value)
    v = float(value)
    if abs(v) < 0.001 or abs(v) > 10000:
        return f"{v:.16e}"
    return str(v)


class JsonToCEmbedder:
    """JSON 到 SCR_RSS_SettingsPresetBake.c 的合并嵌入器。"""

    def __init__(self, c_file_path: str):
        self.c_file_path = Path(c_file_path)
        self.c_content = self.c_file_path.read_text(encoding='utf-8')
        self.json_data: Dict[str, float] = {}
        self.json_meta: Dict[str, object] = {}

    def load_json(self, json_file_path: str):
        json_path = Path(json_file_path)
        if not json_path.exists():
            raise FileNotFoundError(f"JSON文件不存在: {json_file_path}")

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # v4: 根级扁平键；旧版: {"parameters": {...}}
        if isinstance(data.get('parameters'), dict):
            raw = data['parameters']
        else:
            raw = data

        self.json_data = {}
        self.json_meta = {}
        for key, value in raw.items():
            if key.startswith('_'):
                self.json_meta[key] = value
            elif isinstance(value, (int, float)):
                self.json_data[key] = float(value)

        print(f"已加载JSON: {json_file_path}")
        print(f"  可写入参数: {len(self.json_data)}")

    def _init_method_pattern(self, preset_name: str) -> str:
        return (
            r'static void Init' + preset_name
            + r'Defaults\(SCR_RSS_Settings s, bool shouldInit\)\s*\{'
            + r'(.*?)'
            + r'\n\}'
        )

    def _parse_existing_assignments(
        self, preset_name: str
    ) -> Tuple[List[str], Dict[str, str]]:
        """解析 Init{preset}Defaults 内已有赋值，保留字段顺序。"""
        match = re.search(
            self._init_method_pattern(preset_name), self.c_content, re.DOTALL
        )
        if not match:
            return [], {}

        body = match.group(1)
        field_order: List[str] = []
        values: Dict[str, str] = {}
        assign_re = re.compile(
            rf's\.m_{preset_name}\.(\w+)\s*=\s*([^;]+);'
        )
        for m in assign_re.finditer(body):
            name = m.group(1)
            if name not in values:
                field_order.append(name)
            values[name] = m.group(2).strip()
        return field_order, values

    def update_preset(self, preset_name: str) -> bool:
        if not self.json_data:
            raise ValueError("请先加载JSON文件！")

        print(f"\n正在更新 {preset_name} 预设...")
        field_order, existing = self._parse_existing_assignments(preset_name)
        if not field_order:
            print(f"警告：未找到 Init{preset_name}Defaults")
            return False

        merged = dict(existing)
        updated_keys = []
        v6_overrides = {}
        for key, value in self.json_data.items():
            formatted = format_c_float(value)
            if key in V6_PARAM_KEYS:
                v6_overrides[key] = formatted
                updated_keys.append(key)
                continue
            merged[key] = formatted
            if key not in existing or existing.get(key) != merged[key]:
                updated_keys.append(key)
            if key not in field_order:
                field_order.append(key)

        philosophy = self.json_meta.get('_philosophy', '')
        metrics = self.json_meta.get('_metrics_v6')
        if metrics is None:
            metrics = self.json_meta.get('_metrics')
        version_tag = 'v6'
        if not v6_overrides:
            version_tag = 'v4'
        header_lines = [f"\t// {preset_name} — {version_tag} optimizer merge"]
        if philosophy:
            header_lines.append(f"\t// {philosophy}")
        if isinstance(metrics, dict):
            cr = metrics.get(
                'combat_ease',
                metrics.get('combat_reserve', metrics.get('combat_endurance')),
            )
            rp = metrics.get(
                'recovery_ease',
                metrics.get('recovery_pace', metrics.get('recovery_efficiency')),
            )
            pr = metrics.get('parameter_realism', metrics.get('param_drift'))
            if all(isinstance(x, (int, float)) for x in (cr, rp, pr)):
                header_lines.append(
                    f"\t// metrics: ease={cr:.4f} recovery={rp:.6f} realism={pr:.4f}"
                )

        assignments = []
        for key in field_order:
            if key not in merged:
                continue
            if key in V6_PARAM_KEYS:
                continue
            assignments.append(
                f"\ts.m_{preset_name}.{key} = {merged[key]};"
            )

        tier = V6_TIER_BY_PRESET.get(preset_name, 0)
        tail_lines = [
            f"\tApplyV6TierCpDefaults(s.m_{preset_name}, {tier});"
        ]
        for key in sorted(v6_overrides.keys()):
            tail_lines.append(
                f"\ts.m_{preset_name}.{key} = {v6_overrides[key]};"
            )

        new_method = (
            f"static void Init{preset_name}Defaults("
            f"SCR_RSS_Settings s, bool shouldInit)\n"
            f"{{\n"
            f"\tif (!shouldInit)\n"
            f"\t\treturn;\n"
            f"\n"
            + "\n".join(header_lines)
            + "\n"
            + "\n".join(assignments)
            + "\n"
            + "\n".join(tail_lines)
            + "\n}"
        )

        old_pattern = (
            r'static void Init' + preset_name
            + r'Defaults\(SCR_RSS_Settings s, bool shouldInit\)\s*\{.*?\n\}'
        )
        self.c_content, n = re.subn(
            old_pattern, new_method, self.c_content, count=1, flags=re.DOTALL
        )
        if n != 1:
            print(f"警告：替换 Init{preset_name}Defaults 失败 (n={n})")
            return False

        print(f"  已合并 {len(updated_keys)} 个优化参数: {', '.join(updated_keys)}")
        print(
            f"  保留 {len(field_order) - len(updated_keys)} 个未在 JSON 中的 C 端字段"
        )
        return True

    def _apply_v6_method_span(self) -> Tuple[int, int]:
        """返回 ApplyV6TierCpDefaults 方法体在文件中的 [start, end)。"""
        match = re.search(
            r'static void ApplyV6TierCpDefaults\(SCR_RSS_Params p, int tier\)\s*\{',
            self.c_content,
        )
        if not match:
            return -1, -1
        body_start = match.end()
        depth = 1
        i = body_start
        while i < len(self.c_content) and depth > 0:
            ch = self.c_content[i]
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
            i += 1
        if depth != 0:
            return -1, -1
        return body_start, i - 1

    def _extract_v6_tier_block(self, tier: int) -> Tuple[str, int, int]:
        """返回 ApplyV6TierCpDefaults 中指定 tier 代码块 (text, start, end)。"""
        method_start, method_end = self._apply_v6_method_span()
        if method_start < 0:
            return '', -1, -1
        method_body = self.c_content[method_start:method_end]

        if tier == 0:
            pattern = r'(if \(tier == 0\)\s*\{)(.*?)(\}\s*else if \(tier == 1\))'
        elif tier == 1:
            pattern = (
                r'(else if \(tier == 1\)\s*\{)(.*?)(\}\s*else\s*\{)'
            )
        else:
            pattern = r'(else\s*\{)(.*?)(\}\s*p\.sustainable_watts)'
        match = re.search(pattern, method_body, re.DOTALL)
        if not match:
            return '', -1, -1
        start = method_start + match.start(2)
        end = method_start + match.end(2)
        return match.group(2), start, end

    def update_v6_tier_params(self, tier: int) -> bool:
        """将 v6 CP/W'/v5 速度参数写入 ApplyV6TierCpDefaults 对应 tier 块。"""
        v6_updates = {
            k: v for k, v in self.json_data.items() if k in V6_PARAM_KEYS
        }
        if not v6_updates:
            print(f"  v6 tier {tier}: JSON 中无 v6 参数字段，跳过")
            return True

        block, start, end = self._extract_v6_tier_block(tier)
        if start < 0:
            print(f"  警告：未找到 ApplyV6TierCpDefaults tier={tier} 代码块")
            return False

        updated_keys = []
        new_block = block
        for key, value in v6_updates.items():
            formatted = format_c_float(value)
            # PresetBake 使用空格缩进；兼容 tab。
            assign_re = re.compile(rf'(\s*p\.{key}\s*=\s*)([^;]+)(;)')
            if assign_re.search(new_block):
                new_block = assign_re.sub(
                    rf'\g<1>{formatted}\g<3>', new_block, count=1
                )
                updated_keys.append(key)

        if not updated_keys:
            print(f"  警告：tier {tier} 块内未匹配到任何 v6 字段")
            return False

        self.c_content = self.c_content[:start] + new_block + self.c_content[end:]
        print(
            f"  v6 tier {tier}: 已合并 {len(updated_keys)} 个参数 "
            f"({', '.join(updated_keys)})"
        )
        return True

    def save_c_file(self, backup: bool = True):
        if backup:
            backup_path = self.c_file_path.with_suffix('.c.backup')
            backup_path.write_text(
                self.c_file_path.read_text(encoding='utf-8'), encoding='utf-8'
            )
            print(f"\n已创建备份: {backup_path}")
        self.c_file_path.write_text(self.c_content, encoding='utf-8')
        print(f"已保存: {self.c_file_path}")


def _resolve_preset_json(project_root: Path, slug: str) -> Path:
    """优先 v6 JSON，回退 v4。"""
    tools = project_root / 'tools'
    v6 = tools / f'optimized_rss_config_{slug}_v6.json'
    v4 = tools / f'optimized_rss_config_{slug}_v4.json'
    if v6.exists():
        return v6
    return v4


def main():
    print("=" * 80)
    print("JSON 配置合并嵌入 SCR_RSS_SettingsPresetBake.c")
    print("=" * 80)

    project_root = Path(__file__).parent.parent
    c_file_path = (
        project_root
        / "scripts"
        / "Game"
        / "RSS"
        / "NetworkConfig"
        / "SCR_RSS_SettingsPresetBake.c"
    )

    json_files = {
        'EliteStandard': _resolve_preset_json(project_root, 'elitestandard'),
        'StandardMilsim': _resolve_preset_json(project_root, 'standardmilsim'),
        'TacticalAction': _resolve_preset_json(project_root, 'tacticalaction'),
    }

    print("\n检查文件...")
    if not c_file_path.exists():
        print(f"错误：C 文件不存在: {c_file_path}")
        return 1

    missing = [name for name, p in json_files.items() if not p.exists()]
    if missing:
        print("错误：缺少 JSON 预设，请先运行优化流水线：")
        for name in missing:
            print(f"  - {name}: {json_files[name]}")
        return 1

    print("所有文件检查通过。")
    embedder = JsonToCEmbedder(str(c_file_path))

    for preset_name, json_path in json_files.items():
        print(f"\n--- {preset_name} ---")
        try:
            embedder.load_json(str(json_path))
            embedder.update_preset(preset_name)
            tier = V6_TIER_BY_PRESET.get(preset_name)
            if tier is not None:
                embedder.update_v6_tier_params(tier)
        except Exception as exc:
            print(f"错误：{preset_name}: {exc}")
            return 1

    print("\n" + "=" * 80)
    embedder.save_c_file(backup=True)
    print("完成。请检查 SCR_RSS_SettingsPresetBake.c diff 后在 Workbench 编译验证。")
    print("=" * 80)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
