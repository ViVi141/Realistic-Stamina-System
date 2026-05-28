#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
将 v4 优化器导出的 JSON 预设合并写入 SCR_RSS_Settings.c。
仅覆盖 JSON 中出现的键，其余 Init*Defaults 字段保持原 C 值不变。
"""

import json
import re
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def format_c_float(value) -> str:
    """格式化为 EnforceScript 可解析的浮点字面量。"""
    if isinstance(value, bool):
        return "1.0" if value else "0.0"
    if not isinstance(value, (int, float)):
        return str(value)
    v = float(value)
    if abs(v) < 0.001 or abs(v) > 10000:
        return f"{v:.16e}"
    return str(v)


class JsonToCEmbedder:
    """JSON 到 SCR_RSS_Settings.c 的合并嵌入器。"""

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

    def _parse_existing_assignments(
        self, preset_name: str
    ) -> Tuple[List[str], Dict[str, str]]:
        """解析 Init{preset}Defaults 内已有赋值，保留字段顺序。"""
        pattern = (
            'protected void Init' + preset_name + r'Defaults\(bool shouldInit\)\s*\{'
            + r'(.*?)'
            + r'\n\}'
        )
        match = re.search(pattern, self.c_content, re.DOTALL)
        if not match:
            return [], {}

        body = match.group(1)
        field_order: List[str] = []
        values: Dict[str, str] = {}
        assign_re = re.compile(
            rf'm_{preset_name}\.(\w+)\s*=\s*([^;]+);'
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
        for key, value in self.json_data.items():
            merged[key] = format_c_float(value)
            if key not in existing or existing.get(key) != merged[key]:
                updated_keys.append(key)
            if key not in field_order:
                field_order.append(key)

        philosophy = self.json_meta.get('_philosophy', '')
        metrics = self.json_meta.get('_metrics')
        header_lines = [f"\t// {preset_name} — v4 optimizer merge"]
        if philosophy:
            header_lines.append(f"\t// {philosophy}")
        if isinstance(metrics, dict):
            cr = metrics.get('combat_ease', metrics.get('combat_reserve', metrics.get('combat_endurance')))
            rp = metrics.get('recovery_ease', metrics.get('recovery_pace', metrics.get('recovery_efficiency')))
            pr = metrics.get('parameter_realism')
            if all(isinstance(x, (int, float)) for x in (cr, rp, pr)):
                header_lines.append(
                    f"\t// metrics: ease={cr:.4f} recovery={rp:.6f} realism={pr:.4f}"
                )

        assignments = []
        for key in field_order:
            if key not in merged:
                continue
            assignments.append(f"\tm_{preset_name}.{key} = {merged[key]};")

        new_method = (
            f"protected void Init{preset_name}Defaults(bool shouldInit)\n"
            f"{{\n"
            f"\tif (!shouldInit)\n"
            f"\t\treturn;\n"
            f"\n"
            + "\n".join(header_lines)
            + "\n"
            + "\n".join(assignments)
            + "\n}"
        )

        old_pattern = (
            'protected void Init' + preset_name
            + r'Defaults\(bool shouldInit\)\s*\{.*?\n\}'
        )
        self.c_content, n = re.subn(
            old_pattern, new_method, self.c_content, count=1, flags=re.DOTALL
        )
        if n != 1:
            print(f"警告：替换 Init{preset_name}Defaults 失败 (n={n})")
            return False

        print(f"  已合并 {len(updated_keys)} 个优化参数: {', '.join(updated_keys)}")
        print(f"  保留 {len(field_order) - len(updated_keys)} 个未在 JSON 中的 C 端字段")
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


def main():
    print("=" * 80)
    print("JSON 配置合并嵌入 SCR_RSS_Settings.c")
    print("=" * 80)

    project_root = Path(__file__).parent.parent
    c_file_path = (
        project_root / "scripts" / "Game" / "RSS" / "NetworkConfig" / "SCR_RSS_Settings.c"
    )

    json_files = {
        'EliteStandard': project_root / "tools" / "optimized_rss_config_elitestandard_v4.json",
        'StandardMilsim': project_root / "tools" / "optimized_rss_config_standardmilsim_v4.json",
        'TacticalAction': project_root / "tools" / "optimized_rss_config_tacticalaction_v4.json",
    }

    print("\n检查文件...")
    if not c_file_path.exists():
        print(f"错误：C 文件不存在: {c_file_path}")
        return 1

    missing = [name for name, p in json_files.items() if not p.exists()]
    if missing:
        print("错误：缺少 JSON 预设，请先运行 rss_pipeline_v4.py：")
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
        except Exception as exc:
            print(f"错误：{preset_name}: {exc}")
            return 1

    print("\n" + "=" * 80)
    embedder.save_c_file(backup=True)
    print("完成。请检查 SCR_RSS_Settings.c diff 后在 Workbench 编译验证。")
    print("=" * 80)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
