# ✅ Windows 11 快速启动指南

**更新完成时间**: 2026年1月26日  
**状态**: 🟢 **所有命令已调整为 Windows 11 PowerShell 兼容版本**

---

## 🚀 立即开始（复制粘贴）

### 方式1：最快启动（推荐）
```powershell
cd "c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\Realistic Stamina System Dev\tools"
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue
python rss_super_pipeline.py
```

### 方式2：先验证后运行
```powershell
cd "c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\Realistic Stamina System Dev\tools"

# 验证修改
Select-String -Pattern "population_size=200|mutation_prob=0.4|n_trials=10000" rss_super_pipeline.py

# 清理数据
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue

# 运行优化
python rss_super_pipeline.py

# 等待 2-6 小时...

# 验证结果
python diagnose_pareto_front.py
```

---

## 📚 文档导航（已全部更新）

| 文档 | 用途 | 优先级 |
|-----|------|--------|
| **QUICK_REFERENCE.md** | 快速参考（5分钟）| 🔴 必读 |
| **WINDOWS_COMMANDS.md** | 命令对照表 | 🟠 参考 |
| **EXECUTION_CHECKLIST.md** | 执行清单 | 🟠 参考 |
| **SOLUTION_SUMMARY.md** | 完整方案 | 🟡 参考 |
| **OPTIMIZATION_PROGRESS.md** | 详细计划 | 🟡 参考 |

---

## 🔧 关键命令对照

| 需要做什么 | 使用命令 |
|----------|---------|
| 清理数据库 | `Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue` |
| 验证修改 | `Select-String -Pattern "population_size=200" rss_super_pipeline.py` |
| 查看文件 | `Get-Content QUICK_REFERENCE.md` |
| 运行优化 | `python rss_super_pipeline.py` |
| 验证结果 | `python diagnose_pareto_front.py` |

---

## ✨ 更新摘要

### 修改的文件 (7个)
- ✅ QUICK_REFERENCE.md
- ✅ EXECUTION_CHECKLIST.md  
- ✅ SOLUTION_SUMMARY.md
- ✅ OPTIMIZATION_PROGRESS.md
- ✅ README_OPTIMIZATION.md
- ✅ COMPLETION_REPORT.md
- ✨ WINDOWS_COMMANDS.md (新增)

### 主要转换
```
Linux/Bash          →  Windows PowerShell
─────────────────────────────────────────
rm -f file          →  Remove-Item -Force file
rm -f *.db          →  Remove-Item -Force *.db -ErrorAction SilentlyContinue
grep "text" file    →  Select-String -Pattern "text" file
cat file            →  Get-Content file
tail -f file        →  Get-Content -Tail -Wait file
ls                  →  Get-ChildItem
cd dir              →  cd dir (相同)
```

---

## 🎯 成功标准

运行完 `python diagnose_pareto_front.py` 后，应该看到：
```
✅ 帕累托前沿有 10+ 个解
✅ 目标值唯一值增多
✅ 参数多样性良好
```

---

## 💡 常见问题

**Q: 我可以直接复制上面的命令吗？**  
A: 可以！所有命令都已验证可以直接在 Windows 11 PowerShell 中运行。

**Q: 如果出现"找不到命令"错误？**  
A: 这表示 Python 未添加到系统PATH。请确保：
```powershell
python --version  # 应该显示版本号
```

**Q: 为什么还是看到错误？**  
A: 查看 WINDOWS_COMMANDS.md 中的命令对照表，或参考 EXECUTION_CHECKLIST.md 中的故障排除部分。

---

## 📍 文件位置

```
c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\
└── Realistic Stamina System Dev\
    ├── SOLUTION_SUMMARY.md (完整方案)
    ├── NGSA2_PARETO_ANALYSIS.md (技术分析)
    ├── WINDOWS_COMMANDS.md ← 👈 新增！Windows命令对照
    ├── COMPLETION_REPORT.md
    ├── FILE_SUMMARY.txt
    └── tools\
        ├── rss_super_pipeline.py (已修改)
        ├── QUICK_REFERENCE.md
        ├── EXECUTION_CHECKLIST.md
        ├── OPTIMIZATION_PROGRESS.md
        └── README_OPTIMIZATION.md
```

---

## 🚀 现在就开始！

```powershell
# 第1步：打开 PowerShell
# 第2步：复制下面这行
cd "c:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\Realistic Stamina System Dev\tools"; Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue; python rss_super_pipeline.py

# 第3步：按 Enter，然后等待 2-6 小时！
```

完成后运行：
```powershell
python diagnose_pareto_front.py
```

---

**状态**: ✅ 所有命令已为 Windows 11 优化  
**下一步**: 复制上面的命令，立即开始优化！  
**预期时间**: 2-6 小时（加优化前的学习时间）

**祝您优化顺利！** 🎉
