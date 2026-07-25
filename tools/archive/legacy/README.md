# legacy/

旧诊断 / bench 脚本。从 `tools/` 运行并保证根目录在路径中：

```bat
cd tools
set PYTHONPATH=.
python archive\legacy\rss_mudslip_analysis.py
```

产物请写到 `tools/artifacts/diagnostics/`（若脚本仍写当前目录，请手动挪过去）。
