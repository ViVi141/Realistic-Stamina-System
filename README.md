# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**RSS** 是面向 **Arma Reforger** 的拟真体力与移动模组。  
当前主模型：**Pandolf / ACSM → 动态 CP → W′ 焦耳池 → `SCR_RSS_SpeedBridge` 限速**。

**RSS** is a realistic stamina and locomotion mod for **Arma Reforger**.  
Current model: **Pandolf / ACSM → dynamic CP → W′ joule pool → `SCR_RSS_SpeedBridge` speed limiting**.

## 当前要点 / Current highlights

- **已移除意志力平台期 / No willpower plateau**
- **极低体力跛行 + 5 秒撞墙阻尼 / Limp below very low STA + 5 s collapse damping**
- **Sprint 不是固定 `Run × 1.30` / Sprint is not fixed `Run × 1.30`**
- **代谢按实测速度记账 / Metabolic accounting uses measured speed**
- **与灌木/铁丝网减速取 min / Merges with foliage/wire slowdown by `min`**
- **当前运行时步态顶速约 1.45 / 3.8 / 5.5 m/s / Live gait tops are about 1.45 / 3.8 / 5.5 m/s**
- **外部 API 字段：`wPrimePool01` / External API field: `wPrimePool01`**

## 仓库结构 / Layout

- `scripts/Game/Integration/` — `PlayerBase.c`, `PlayerBase_UpdateLoop.c`, `SCR_StaminaOverride.c`, ...
- `scripts/Game/RSS/Core/` — 速度、代谢、CP–W′、恢复、协调器 / speed, metabolism, CP–W′, recovery, coordinator
- `scripts/Game/RSS/Environment/` — 环境、游泳、跳跃 / environment, swimming, jump-vault
- `scripts/Game/RSS/AI/` — 当前 AI 实现 / current AI implementation
- `scripts/Game/RSS/NetworkConfig/` — 配置、同步、API / config, sync, API
- `scripts/Game/RSS/Presentation/` — HUD、设置、镜头表现 / HUD, settings, camera presentation
- `tools/` — v6 管线、PyO3 `rss_sim`、Rust CLI / v6 pipeline, PyO3 `rss_sim`, Rust CLI

## 文档 / Docs

- 现行完整中文说明 / full current CN: [README_CN.md](README_CN.md)
- English summary: [README_EN.md](README_EN.md)
- v6 权威计算 / v6 math: [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)
- API: [docs/RSS_API.md](docs/RSS_API.md)
- 版本历史 / version history: [CHANGELOG.md](CHANGELOG.md)

## 许可证 / License

[GNU AGPL-3.0](LICENSE)
