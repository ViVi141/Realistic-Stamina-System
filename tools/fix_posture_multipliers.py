#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快速修复姿态消耗倍数参数
将优化器生成的错误参数（<1.0）修正为正确的值（>1.0）
"""

import json
from pathlib import Path


def fix_posture_multipliers(json_file_path: str):
    """
    修复姿态消耗倍数参数

    Args:
        json_file_path: JSON文件路径
    """
    json_path = Path(json_file_path)

    if not json_path.exists():
        print(f"❌ JSON文件不存在: {json_file_path}")
        return False

    # 读取JSON文件
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # 获取参数
    parameters = data.get('parameters', {})
    posture_crouch = parameters.get('posture_crouch_multiplier', 0.0)
    posture_prone = parameters.get('posture_prone_multiplier', 0.0)

    print(f"\n{'='*80}")
    print(f"修复文件: {json_file_path}")
    print(f"{'='*80}")
    print(f"\n原始值:")
    print(f"  posture_crouch_multiplier: {posture_crouch}")
    print(f"  posture_prone_multiplier: {posture_prone}")

    # 检查是否需要修复
    if posture_crouch >= 1.8 and posture_prone >= 2.2:
        print(f"\n✅ 参数值正确，无需修复")
        return True

    # 修复参数
    # 使用Custom预设的值作为参考
    fixed_crouch = 2.0
    fixed_prone = 2.5

    parameters['posture_crouch_multiplier'] = fixed_crouch
    parameters['posture_prone_multiplier'] = fixed_prone

    print(f"\n修复后:")
    print(f"  posture_crouch_multiplier: {fixed_crouch}")
    print(f"  posture_prone_multiplier: {fixed_prone}")

    # 保存修复后的JSON文件
    backup_path = json_path.with_suffix('.json.backup')
    with open(backup_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
    print(f"\n✅ 已创建备份文件: {backup_path}")

    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
    print(f"✅ 已保存修复后的文件: {json_file_path}")

    return True


def main():
    """主函数"""
    print("="*80)
    print("姿态消耗倍数快速修复工具")
    print("="*80)

    # 项目根目录
    project_root = Path(__file__).parent.parent

    # JSON文件列表
    json_files = {
        'EliteStandard': project_root / "tools" / "optimized_rss_config_realism_super.json",
        'StandardMilsim': project_root / "tools" / "optimized_rss_config_balanced_super.json",
        'TacticalAction': project_root / "tools" / "optimized_rss_config_playability_super.json"
    }

    # 逐个修复
    success_count = 0
    for preset_name, json_path in json_files.items():
        print(f"\n处理 {preset_name} 预设...")
        if fix_posture_multipliers(str(json_path)):
            success_count += 1

    print(f"\n{'='*80}")
    print(f"修复完成！")
    print(f"{'='*80}")
    print(f"\n成功修复 {success_count}/{len(json_files)} 个预设")

    print(f"\n下一步:")
    print(f"1. 运行 embed_json_to_c.py 更新C文件:")
    print(f"   cd tools")
    print(f"   python embed_json_to_c.py")
    print(f"\n2. 在游戏中测试修复后的配置")
    print(f"\n3. 如果需要重新优化，请先修复 rss_super_pipeline.py 中的参数范围")


if __name__ == "__main__":
    main()
