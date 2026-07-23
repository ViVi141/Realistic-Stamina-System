# Realistic Stamina System (RSS) v6.0.0

[中文 README](README_CN.md) | [English README](README_EN.md)

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arma Reforger](https://img.shields.io/badge/Arma-Reforger-orange)](https://www.bohemia.net/games/arma-reforger)
[![Version](https://img.shields.io/badge/Version-6.0.0-brightgreen)](CHANGELOG.md)

RSS is a realistic stamina and locomotion mod for **Arma Reforger**.
Current v6 model: **Pandolf / ACSM -> dynamic CP -> W′ -> `SCR_RSS_SpeedBridge` speed limiting**.

## Highlights

- no willpower plateau on the player path
- sprint is not fixed `Run × 1.30`
- drain accounting uses measured speed
- foliage / wire slowdown still merges by `min`
- current runtime gait tops are about `1.45 / 3.8 / 5.5 m/s`
- external API exposes `wPrimePool01`

## Docs

- current Chinese README: [README_CN.md](README_CN.md)
- English README: [README_EN.md](README_EN.md)
- v6 math reference: [docs/RSS_v6_计算逻辑权威版.md](docs/RSS_v6_计算逻辑权威版.md)
- API: [docs/RSS_API.md](docs/RSS_API.md)
- changelog: [CHANGELOG.md](CHANGELOG.md)

## License

[GNU AGPL-3.0](LICENSE)
