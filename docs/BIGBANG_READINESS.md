# Big Bang 切换清单（v5.0.0）

> 在 **全部条件满足** 前，请勿将 `refactor/v5-rewrite` merge 至 `main`。

## 自动化门禁

| 检查 | 命令 | 状态 |
|------|------|------|
| 脚本体积 | `python tools/check_script_size.py` | ✅ exit 0（WARN：`MetabolismMath` ~62KB；`EnvironmentFactor` 已拆分至 ~61KB） |
| 生理锚点 | `python tools/bench_physio_anchors.py` | ✅ 可跑项 pass（4h 行军：孪生已跑，待 v5 校准 ≥0.20） |
| v5 冒烟 | `python tools/test_v5_smoke.py` | ✅ 8/8 pass |
| pre-commit | `git config core.hooksPath githooks` 后提交 | 需用户启用 |

## 代码清理（v5 推进）

- [x] 删除 `scripts/Game/**/*.c.v323archived`（77 个；只读副本在 `archive/v3.23.1/`）
- [x] RSS 类名统一为 `SCR_RSS_*`（NetworkSyncManager、UISignalBridge、DebugDisplay、MudSlipEffects、StaminaHUD 等）
- [x] 无氧 RplProp 辅助并入 `SCR_RSS_NetworkSyncManager.c`（删除独立 `SCR_RSS_NetworkSync.c`）
- [x] 拆分 `SCR_RSS_EnvironmentFactor.c` → 新增 `SCR_RSS_EnvironmentDebug.c`，删除死代码委托
- [x] `PlayerBase` 主循环接入 v5 双池：v_drain、代谢降速、无氧 tick、Sprint 门禁与速度阻尼
- [x] v5 行军档限速（Walk 1.4 / Run 2.8 / Sprint 4.0 m/s @35kg）接入 SpeedCalculator
- [x] HUD 仅有氧主条 + Sprint CD 环；FOV 绑无氧池
- [x] CHANGELOG `5.0.0` 正式条目

## Workbench

- [x] `scripts/Game/**/*.c` 零编译错误
- [ ] 实机：1 km 行军节奏、短冲 vs 抽干冷却、代谢强制降速可读
- [ ] 联机：2 客户端 Sprint 冷却一致（RplProp `m_fReplAnaerobicPool`）

## 文档与发布

- [x] README 版本 badge → `5.0.0`
- [ ] tag `v3.23.1-final`（若尚未打）
- [ ] merge → main，发布 v5.0.0
- [x] CHANGELOG `5.0.0` 正式条目
