# 编译崩溃排查（壳子法 + 逐个文件编译）

> **中文** | [English](en/SCRIPT_FILE_SIZE_LIMIT.md)

## 重要更正

> ~~所有 `.c` 脚本文件不得超过 65535 字节（64 KB），超出后编译/运行时会直接崩溃。~~
>
> **该说法已取消。文件大小（64 KB）不是 Workbench 编译或游戏运行时崩溃的原因。**

此前「64 KB 硬上限导致崩溃」的判断不成立，予以撤销。文件中保留本页标题仅为兼容历史链接。

## 正确的排查方法

当 Workbench 编译脚本时崩溃（ICE / 堆损坏 / 无明确报错），唯一可靠的方法是**逐个文件编译检查**，配合**壳子法**逐步隔离出具体文件：

1. **只留壳子**：把嫌疑文件精简为「壳子」——保留类与方法的**签名**、删除方法体，保证其他模块仍能引用、项目整体仍能编译通过。
2. **逐个文件恢复编译**：一次恢复一个文件（或一个方法），重新编译，定位到导致崩溃的具体文件。
3. **对定位文件二分**：壳子 → 半量恢复 → 全量，逐段缩小到具体方法/代码块。

> `tools/check_script_size.py` 现只作为**可被其他模块引用的壳子**，不再因文件体积阻断提交。它提供：
>
> - `iter_script_files()` — 枚举 `.c` 文件，供逐个编译脚本引用
> - `tier_for()` — 分层可维护性上限（仅供参考，非崩溃原因）
> - `has_bom()` — UTF-8 BOM 检测（仍为真实语法错误，阻断提交）

## 可维护性建议（非崩溃原因）

以下分层上限仅用于**可维护性**（便于编辑与审查），与编译稳定性无关：

| 层级 | 建议上限 |
|------|----------|
| Integration | ≤ 40 KB / ≤ 600 行 |
| StaminaOverride | ≤ 15 KB / ≤ 250 行（拦截壳 only） |
| RSS/Core 等 | ≤ 45 KB / ≤ 700 行 |

文件偏大时**建议**外移领域逻辑到 helper，但不再因此阻断提交。

## 预提交检查

```powershell
python tools/check_script_size.py     # BOM 阻断 + 可维护性提示（不再因大小阻断）
python tools/check_enforce_syntax.py  # 禁用语法 + 单行 if
python tools/test_v6_smoke.py
```

## 同步更新

本页与以下文档联动：

- [scripts_naming_and_layout_rules.md](scripts_naming_and_layout_rules.md)
- [RSS_CODING_STANDARDS.md](RSS_CODING_STANDARDS.md)
