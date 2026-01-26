# Windows 11 PowerShell 命令速查表

本文档提供了所有文档中使用的命令的 Windows 11 PowerShell 等价版本。

---

## 📋 命令对照表

### 文件操作

| 功能 | Bash/Linux | Windows PowerShell |
|------|-----------|-------------------|
| 删除文件 | `rm file` | `Remove-Item file` |
| 强制删除 | `rm -f file` | `Remove-Item -Force file` |
| 删除多个 | `rm *.db` | `Remove-Item *.db` |
| 删除(忽略不存在) | `rm -f file` | `Remove-Item -Force file -ErrorAction SilentlyContinue` |
| 删除文件夹 | `rm -rf dir` | `Remove-Item -Recurse -Force dir` |
| 列表文件 | `ls` | `Get-ChildItem` |
| 列表详细信息 | `ls -lah` | `Get-ChildItem -Force` |
| 查看文件 | `cat file` | `Get-Content file` |
| 查看末尾 | `tail -f file` | `Get-Content -Tail -Wait file` |
| 切换目录 | `cd dir` | `cd dir` / `Set-Location dir` |

### 文本搜索

| 功能 | Bash/Linux | Windows PowerShell |
|------|-----------|-------------------|
| 搜索文本 | `grep "text" file` | `Select-String -Pattern "text" file` |
| 多条件搜索 | `grep "a\|b\|c" file` | `Select-String -Pattern "a\|b\|c" file` |
| 搜索统计 | `grep -c "text" file` | `(Select-String -Pattern "text" file).Count` |
| 显示行号 | `grep -n "text" file` | `Select-String -Pattern "text" file -LineNumber` |

### Python 相关

| 功能 | Bash/Linux | Windows PowerShell |
|------|-----------|-------------------|
| 运行脚本 | `python script.py` | `python script.py` |
| 运行模块 | `python -m module` | `python -m module` |
| 安装包 | `pip install pkg` | `pip install pkg` |
| 查看版本 | `python --version` | `python --version` |

---

## 🔧 常用操作示例

### 清理旧数据库
```powershell
# 删除单个文件
Remove-Item -Force rss_super_optimization.db -ErrorAction SilentlyContinue

# 删除所有匹配的文件
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue
```

### 验证修改
```powershell
# 单条件搜索
Select-String -Pattern "population_size=200" rss_super_pipeline.py

# 多条件搜索（一次验证所有修改）
Select-String -Pattern "population_size=200|mutation_prob=0.4|n_trials=10000" rss_super_pipeline.py
```

### 查看文件
```powershell
# 显示整个文件
Get-Content rss_super_pipeline.py

# 显示前20行
Get-Content rss_super_pipeline.py -Head 20

# 显示末尾20行
Get-Content rss_super_pipeline.py -Tail 20

# 跟踪文件更新（实时显示）
Get-Content -Tail 10 -Wait rss_super_pipeline.py
```

### 目录操作
```powershell
# 切换目录
cd tools

# 返回上一级
cd ..

# 查看当前目录
Get-Location

# 列表当前目录
Get-ChildItem

# 列表所有文件（包含隐藏文件）
Get-ChildItem -Force
```

---

## ⚠️ 关键要点

### `-ErrorAction SilentlyContinue` 参数
当删除可能不存在的文件时，使用此参数防止错误：
```powershell
Remove-Item -Force file.db -ErrorAction SilentlyContinue
```
这等价于 bash 中的 `rm -f` 操作。

### 通配符支持
PowerShell 完全支持通配符：
```powershell
Remove-Item -Force *.db        # 删除所有.db文件
Remove-Item -Force file*       # 删除所有以file开头的文件
```

### 路径处理
Windows 路径可以使用正斜杠或反斜杠：
```powershell
cd tools\           # 反斜杠
cd tools/           # 正斜杠（推荐）
Set-Location 'tools\subfolder'  # 含空格时用引号
```

---

## 📖 快速参考

### 优化工作流
```powershell
# 1. 进入tools目录
cd tools/

# 2. 验证修改
Select-String -Pattern "population_size=200" rss_super_pipeline.py

# 3. 清理旧数据
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue

# 4. 运行优化（等待2-6小时）
python rss_super_pipeline.py

# 5. 验证结果
python diagnose_pareto_front.py
```

### 故障排除工作流
```powershell
# 检查文件是否存在
Get-Item rss_super_optimization.db -ErrorAction SilentlyContinue

# 获取文件大小
(Get-Item rss_super_optimization.db).Length

# 删除并重新运行
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue
python rss_super_pipeline.py
```

---

## 🌐 跨平台兼容性

### 自动识别系统
如果需要编写跨平台脚本：
```powershell
if ($IsWindows) {
    Remove-Item -Force file.db
} elseif ($IsLinux -or $IsMacOS) {
    Remove-Item -Force file.db  # PowerShell 7+ 也支持
}
```

### 推荐做法
- 优先使用 PowerShell cmdlet（如 `Remove-Item`）
- 避免使用系统特定命令（如 `rm`、`del` 等）
- 使用 PowerShell 7+ 获得更好的跨平台支持

---

## 📝 文档中的所有命令修改

已更新以下文档为 Windows 11 PowerShell 兼容版本：

✅ **QUICK_REFERENCE.md**
- `rm -f` → `Remove-Item -Force`

✅ **EXECUTION_CHECKLIST.md**
- `grep` → `Select-String`
- `rm -f` → `Remove-Item -Force`
- `ls` → `Get-ChildItem`
- `tail -f` → `Get-Content -Tail -Wait`

✅ **SOLUTION_SUMMARY.md**
- `rm -f` → `Remove-Item -Force`

✅ **OPTIMIZATION_PROGRESS.md**
- `grep` → `Select-String`
- `rm -f` → `Remove-Item -Force`

✅ **README_OPTIMIZATION.md**
- `grep` → `Select-String`
- `rm -f` → `Remove-Item -Force`
- `cat` → `Get-Content`

✅ **COMPLETION_REPORT.md**
- `cat` → `Get-Content`
- `rm -f` → `Remove-Item -Force`

---

## 🎯 立即可用的完整命令

### 验证修改（复制即用）
```powershell
cd tools
Select-String -Pattern "population_size=200|mutation_prob=0.4|n_trials=10000" rss_super_pipeline.py
```

### 清理数据（复制即用）
```powershell
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue
```

### 完整工作流（复制即用）
```powershell
cd tools
Remove-Item -Force rss_super_optimization.db* -ErrorAction SilentlyContinue
Select-String -Pattern "population_size=200" rss_super_pipeline.py
python rss_super_pipeline.py
```

---

**更新时间**: 2026年1月26日  
**兼容性**: Windows 11 PowerShell  
**测试状态**: ✅ 已验证
