# archive/

历史与一次性脚本，**不参与**日常 validate / optimize。

| 路径 | 内容 |
|------|------|
| `bisect_backup/` | UpdateLoop 拆分前的完整备份与 bisect 辅助脚本 |
| `legacy/` | 旧 bench / diag / mudslip / 回归脚本（需时从仓库根或本目录加 `tools` 到 `PYTHONPATH`） |

日常请用 `tools/` 根目录下的 v6 管线与 `test_v6_smoke.py`。
