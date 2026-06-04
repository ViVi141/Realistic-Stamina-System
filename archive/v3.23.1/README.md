# RSS v3.23.1 源码归档

本目录为 v5 重写前的只读快照，**不参与 Workbench 编译**。

| 项目 | 说明 |
|------|------|
| 原版本 | Realistic Stamina System **v3.23.1** |
| 归档日期 | 2026-06-04 |
| 文件数 | 77 个 `.c`（见 `MANIFEST.txt`） |
| 活跃副本 | `scripts/Game/**/*.c.v323archived`（同内容，改后缀禁用编译） |

## 目录结构

```
archive/v3.23.1/
├── README.md
├── MANIFEST.txt
├── BEHAVIOR_CHECKLIST.md
└── scripts/Game/   # 扩展名已恢复为 .c
```

## 还原 v3 到 scripts/（仅供对照，慎用）

```powershell
# 在仓库根目录：将某个归档 .c 复制回 scripts 并去掉 .v323archived 后缀
Copy-Item archive/v3.23.1/scripts/Game/Integration/PlayerBase.c `
  scripts/Game/Integration/PlayerBase.c
```

或 checkout v3.23.1-final tag（若已打 tag）。

## v5 开发

新代码位于 `scripts/Game/` 下标准 `.c` 文件；约定见 `docs/RSS_CODING_STANDARDS.md`。
