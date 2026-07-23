# ICE 修复说明（UpdateLoop）

## 原因
Enforce 对**同一 addon 里过多的** `modded class SCR_CharacterControllerComponent` 分片合并不稳定。  
Phase 放在 `RSS/Core/` 或额外 `RssTick*.c` 时，UpdateLoop 会报 `Undefined function ...RSS_StaminaTickPhase*`。

## 现行结构（只保留两个官方约定的 modded 入口）
| 文件 | 内容 | 约大小 |
|------|------|--------|
| `PlayerBase_UpdateLoop.c` | DTO / Locals / PhaseA / PhaseB / 编排 / LoopStart | ~33KB |
| `PlayerBase.c` | PhaseC + 原有状态 | ~60KB |

请重新编译。若仍崩，再把 PhaseB 也挤进 PlayerBase 前需先削 PlayerBase 体积。
