# 贡献指南

感谢您对本项目的关注！我们欢迎任何形式的贡献。

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

开发前请阅读 **[docs/RSS_开发者指南.md](docs/RSS_开发者指南.md)** / [English](docs/en/DEVELOPER_GUIDE.md)（目录、改动入口、提交前检查）。  
关键文档中英索引：[docs/en/README.md](docs/en/README.md)。

编码细则以 [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md) / [EN](docs/en/CODING_STANDARDS.md) 为准，尤其注意：

- 禁止 EnforceScript 三元运算符 `?:`
- 单文件不超过 65535 字节
- 限速经 `SCR_RSS_SpeedBridge` / `SetSpeedLimit`，勿盖掉灌木减速
- 提交前运行 `tools/check_script_size.py`、`tools/check_enforce_syntax.py`、`tools/test_v6_smoke.py`

## 许可证

通过贡献代码，您同意您的贡献将在 [AGPL-3.0](LICENSE) 许可证下发布。
