#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快速验证微调预设
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from verify_json_params import verify_json

if __name__ == '__main__':
    p = Path(__file__).parent / 'optimized_rss_config_balanced_tweaked.json'
    if not p.exists():
        print('Tweaked preset not found:', p)
        sys.exit(1)
    print('验证文件:', p)
    verify_json(str(p))
