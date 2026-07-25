# vps/

VPS 上跑 Python / Rust 孪生网格的部署与一次性脚本。

| 脚本 | 用途 |
|------|------|
| `_deploy_vps_twin.py` | 同步 Python twin tools 并抽样网格 |
| `_deploy_vps_rust_grid.py` | 同步 `rss_sim` 并部署 Rust 网格 |
| `_vps_*.py` | 会话期检查 / 续跑 / 全量 / 统计（按需） |

需要环境变量：`RSS_VPS_HOST`、`RSS_VPS_PORT`、`RSS_VPS_USER`、`RSS_VPS_PASSWORD`。

在 `tools/vps` 下执行，或保证能 `import` 到上层 `tools` 模块（部署脚本自身以 SFTP 推送为主）。
