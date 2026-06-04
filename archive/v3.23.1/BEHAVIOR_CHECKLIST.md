# v3.23.1 行为回归清单（v5 移植对照）

移植完成后逐项勾选，确保 v5 未遗漏边缘行为。

## Core / 体力

- [ ] 0.2s 主循环更新
- [ ] Pandolf 陆地消耗 + v_drain 与测速一致
- [ ] 有氧池注入引擎条（拦截壳）
- [ ] 无氧池不写入引擎条
- [ ] 5s 平台期阻尼（25% 临界点）
- [ ] 负重缓存 + 库存变更刷新
- [ ] 跳跃/翻越消耗 + 连续跳跃惩罚
- [ ] 游泳 3D 模型 + 湿重
- [ ] EPOC / 疲劳 / 姿态切换成本

## Environment

- [ ] 自建温度/热应激/冷应激
- [ ] 降雨湿重 + 停雨衰减
- [ ] 风阻、泥泞、地形材质
- [ ] 室内检测（屋顶 + 水平封闭）
- [ ] GlobalSignalsManager 环境信号

## Network

- [ ] 服务端权威配置（GameMode RplProp）
- [ ] 客户端 stamina/速度上报与校验
- [ ] 重连后配置同步
- [ ] v5 无氧池 RplProp 同步

## AI

- [ ] 6 态 FSM + 500ms 行为节流
- [ ] SpeedCap / IntentFilter / CombatDecay / InjuryLink
- [ ] 禁用 AI RSS / 仅禁用 AI 体力 开关

## Presentation

- [ ] HUD 状态条 + 冲刺 CD 环（v5）
- [ ] SetSpeedLimit 灌木合并
- [ ] 屏效/相机（RSS_PRESENTATION_NATIVE_ONLY）
- [ ] Admin Settings Tab

## Items / 其他

- [ ] CSB 注射器状态机
- [ ] 吗啡 / 自定义注射器
- [ ] 泥泞滑倒
- [ ] SCR_RSS_API 向后兼容

## 联机 / 生命周期

- [ ] 实体删除析构 CallLater 清理
- [ ] Workbench 重载世界静态缓存清理
- [ ] 2 客户端 Sprint 冷却一致
