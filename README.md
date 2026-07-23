# Realistic Stamina System (RSS) v6.0.0

[中文完整 README](README_CN.md) | [English full README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**RSS** is a realistic stamina & movement-speed mod for **Arma Reforger** (AGPL-3.0).  
v6 uses **Pandolf/ACSM metabolism → dynamic Critical Power → W′ (joules)** to drive drain and `SetSpeedLimit` (merged with foliage/wire slowdown on 1.7+).

**RSS** 是面向 Arma Reforger 的拟真体力模组。  
v6 闭环：**代谢功率 → 动态 CP → W′ 放电/再填充 → 反解限速**（与灌木/铁丝网减速取最小值）。

| | |
|--|--|
| Author / 作者 | ViVi141 · 747384120@qq.com |
| GUID | `68649101601CC93D` |
| Config | `6.0.0` |
| Game | Reforger **1.7+** recommended |

### Highlights / 要点

- No willpower plateau; low STA → limp only / 无意志力平台期，低体力跛行
- Presets: EliteStandard · StandardMilsim (default) · TacticalAction · Custom
- Domains: `Integration/` + `RSS/{Core,Environment,AI,NetworkConfig,Presentation,MudSlip}`
- Tools: `tools/rss_pipeline_v6.py` (+ optional Rust `rss_sim`)
- API: `SCR_RSS_API` · field `wPrimePool01`

### Docs / 文档

- Math / 计算权威版 → [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)
- API → [docs/RSS_API.md](docs/RSS_API.md)
- Coding standards → [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md)
- Changelog → [CHANGELOG.md](CHANGELOG.md)

完整说明请打开语言对应的 README；本文件仅作仓库入口摘要。  
For full detail, open the language-specific README above.
