# Realistic Stamina System (RSS) v6.0.0

[中文完整 README](README_CN.md) | [English full README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

**RSS** — realistic stamina & movement for **Arma Reforger** (AGPL-3.0, ViVi141).  
v6 loop: **Pandolf/ACSM → dynamic CP → W′ (joules) → invert speed limit** via `SCR_RSS_SpeedBridge` (min with foliage/wire on 1.7+).

**RSS** — Arma Reforger 拟真体力模组。  
v6 闭环：**代谢功率 → 动态 CP → W′ 放电/再填充 → 反解限速**（与灌木/铁丝网取最小值）。

| | |
|--|--|
| Author / 作者 | ViVi141 · 747384120@qq.com |
| GUID | `68649101601CC93D` |
| Config | **6.0.0** |
| Game | Reforger **1.7+** |

### Highlights / 要点

- **No willpower plateau** — low STA limp + 5 s collapse damping / 无意志力平台期；低体力跛行 + 5 秒撞墙阻尼  
- **Encumbrance & slope** — load raises metabolic cost; Tobler-style grade pacing / 负重抬油耗；坡度自适应  
- **Recovery stack** — breathing deficit, load penalty, marginal decay, stance multipliers / 呼吸困顿、负重惩罚、边际衰减、姿态倍率  
- **Environment** — heat/cold, rain wet weight, wind, mud, indoor / 热冷雨风泥、室内  
- **Swim 3D drag** — horizontal v², vertical work, treading, post-exit wet weight / 游泳三维阻力与上岸湿重  
- **Jump/vault** — load-scaled cost, consecutive penalty, cooldown / 跳跃翻越与连续惩罚  
- **Presets** — EliteStandard · **StandardMilsim** (default) · TacticalAction · Custom  
- **AI (experimental)** — AIManager state → speed cap → intent → combat decay / 实验性 AI 体力链  
- **API** — `SCR_RSS_API` · **`wPrimePool01`**  
- **Layout** — `Integration/` + `RSS/{Core,Environment,AI,NetworkConfig,Presentation,MudSlip}` (~98 `.c`)  
- **Tools** — `tools/rss_pipeline_v6.py` (+ optional Rust `rss_sim`)

### Docs / 文档

- Full CN → [README_CN.md](README_CN.md)（特性、技术实现、安装、参考文献最全）  
- Full EN → [README_EN.md](README_EN.md)  
- Math → [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)  
- API → [docs/RSS_API.md](docs/RSS_API.md)  
- Standards → [docs/RSS_CODING_STANDARDS.md](docs/RSS_CODING_STANDARDS.md)  
- Changelog → [CHANGELOG.md](CHANGELOG.md)

```bash
cd tools
pip install -r requirements.txt
python rss_pipeline_v6.py validate
python test_v6_smoke.py
```
