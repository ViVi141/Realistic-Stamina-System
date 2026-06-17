# Realistic Stamina System — Tools

Python 数字孪生与 **v6 优化/校验管线**（v4 保留作对照）。

## 文件一览

| 文件 | 说明 |
|------|------|
| **`rss_pipeline_v6.py`** | **主入口**：`validate`（CI 门禁）/ `optimize`（NSGA-II） |
| `rss_constraints_v6.py` | 生理锚点硬约束 + soft 行军锚点 |
| `rss_pipeline_v4.py` | v4 三目标优化（历史对照） |
| `rss_pipeline_v5.py` | 薄包装 → `rss_pipeline_v6 validate` |
| `rss_digital_twin_fix.py` | 与 C 端对齐的数字孪生 |
| `bench_physio_anchors.py` | 锚点 bench（调用 constraints 模块） |
| `test_v6_smoke.py` | v6 CP–W′ / 代谢契约冒烟 |
| `schemas/rss_params_v5.schema.json` | v5/v6 可嵌入参数字段 |
| `optimized_rss_config_*_v4.json` | v4 三档预设（当前 C 端主要来源） |
| `embed_json_to_c.py` | JSON → `SCR_RSS_Settings.c`（可选） |

设计说明：`docs/RSS_v6_优化管线设计.md`

## 安装

```bash
cd tools
pip install -r requirements.txt
```

## 校验（推荐 CI / 提交前）

```bash
python rss_pipeline_v6.py validate
python test_v6_smoke.py
```

## 优化（生成 v6 预设 JSON）

```bash
python rss_pipeline_v6.py optimize --trials 400 --jobs 4 --output .
```

## v4 优化（对照）

```bash
python rss_pipeline_v4.py --trials 300 --jobs 4 --output .
python test_v4_smoke.py
```
