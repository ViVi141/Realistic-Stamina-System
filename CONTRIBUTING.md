# 贡献指南

感谢您对本项目的关注！我们欢迎任何形式的贡献。

## 开发者文档

动手改代码前请阅读：

- **[docs/RSS_开发者指南.md](docs/RSS_开发者指南.md)** — 环境、目录地图、硬性约束、常见改动、PR 自检
- [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) — 编码规范
- [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md) — 计算逻辑权威说明

## 如何贡献

### 报告问题

如果您发现了 bug 或有功能建议，请：

1. 检查 [Issues](https://github.com/ViVi141/RealisticStaminaSystem/issues) 中是否已有相关问题
2. 如果没有，请创建新的 Issue，详细描述问题或建议

### 提交代码

1. Fork 本项目
2. 创建您的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启一个 Pull Request

### 代码规范

- 遵循 [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md)（禁止三元 `?:`、单文件 ≤ 64 KB 等）
- 提交前运行 `python tools/check_script_size.py` 与 `python tools/check_enforce_syntax.py`
- 添加必要的中文注释（公共 static：`//!` + `@param` / `@return`）
- 确保 Workbench 可编译；更新相关 docs / CHANGELOG

## 许可证

通过贡献代码，您同意您的贡献将在 [AGPL-3.0](LICENSE) 许可证下发布。
