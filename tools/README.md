# Realistic Stamina System — Tools（仅 V4 管线）

本目录仅保留 **v4 多目标优化** 及其直接依赖：数字孪生仿真、预设 JSON、嵌入脚本与烟雾/快速验证。

## 文件一览

| 文件 | 说明 |
|------|------|
| `rss_pipeline_v4.py` | v4 主入口：Optuna NSGA-II、8 场景、`extract_presets` 写出三份 JSON |
| `rss_digital_twin_fix.py` | 与 C 端对齐的数字孪生（供管线 import，勿单独当 CLI 依赖） |
| `optimized_rss_config_*_v4.json` | 三份预设（EliteStandard / StandardMilsim / TacticalAction） |
| `embed_json_to_c.py` | 将 JSON 参数嵌入 `SCR_RSS_Settings.c`（可选工作流） |
| `test_v4_smoke.py` | 无 Optuna：跑通 8 任务仿真 + `compute_metrics` |
| `quick_verify.py` | 少量 trial + 临时目录写出预设，检查管线可运行 |
| `requirements.txt` | `numpy`、`optuna` |

## 安装

```bash
cd tools
pip install -r requirements.txt
```

## 运行优化（生成新 JSON）

```bash
python rss_pipeline_v4.py --trials 300 --jobs 4 --output .
```

## 烟雾测试 / 快速验证

```bash
python test_v4_smoke.py
python quick_verify.py
```

## 将预设写入 C 代码（可选）

```bash
python embed_json_to_c.py
```

（按脚本内路径约定执行；提交前请核对 `SCR_RSS_Settings.c` diff。）

---

**最后更新**：与仓库「仅保留 V4 工具」策略一致；其它离线校验、材质表生成、绘图 GUI 等已移出本目录。
