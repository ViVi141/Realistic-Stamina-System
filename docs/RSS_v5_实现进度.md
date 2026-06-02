# RSS v5 实现进度总结

**日期**: 2026-06-02  
**状态**: Phase 0 代码框架完成

---

## 实现清单

### ✅ 已完成

#### Phase 0 基线工具
- [x] **bench_physio_anchors_v5.py** - 生理锚点基准测试脚本
  - 锚点A: 4小时行军测试(有氧池>20%)
  - 锚点B: 冲刺爆发时长测试(5-15s)
  - 锚点C: 冷却分层测试
  - 强制降速验证
  - 支持pytest + 手动运行模式

#### Phase 1 核心组件
- [x] **SCR_MetabolicSpeedLimiter.c** - 强制降速机制
  - 渐进式降速算法
  - 硬撑(Override)选项
  - 可持续功率计算
  - 反馈系统(UI/音效/FOV)

#### Phase 2 无氧系统
- [x] **SCR_AnaerobicBurstState.c** - 无氧池组件
  - 独立无氧能量池(0-1)
  - 消耗逻辑(与速度+负重相关)
  - 分层冷却(抽干180s/短冲60-90s)
  - 恢复逻辑(门槛+加速)
  - RplProp网络同步(服务器权威)
  - 智能同步策略(2%-0.5s-状态切换-5s心跳)

#### 配置与参数
- [x] **SCR_RSS_V5_Params.c** - v5参数常量定义
  - 三档预设参数(Elite/Standard/Tactical)
  - 无氧系统参数
  - 强制降速参数
  - 速度重标定
  - 系数物理化参考值
  - Legacy兼容开关

#### UI/信号系统
- [x] **SCR_UISignalBridge_V5.c** - UI信号桥接
  - 冲刺CD环显示(准星周围,颜色渐变)
  - 代谢警告面板(严重/中度分级)
  - 硬撑提示
  - 事件委托系统(订阅-发布)
  - 无代谢过载恢复事件

---

## 待实现

### Phase 1: 速度-消耗闭环(预计3周)

- [ ] **修改StaminaUpdateCoordinator** - v_drain闭环
  - `GetDrainVelocity()` 实现
  - Sprint/Run消耗改用 min(GetVelocity, theoretical)
  - 移除固定3.8/5.5
  
- [ ] **修改SpeedCalculation** - 速度重标定
  - 35kg Walk降到1.4 m/s (Elite)
  - Run/Sprint相应调整
  - 与灌木桥接集成(final = min(RSS, foliage))

- [ ] **单元测试** - Phase 1验收
  - v_drain一致性(<5%误差)
  - 2.87 m/s强制降速(<3分钟)
  - 消耗速度验证

### Phase 2: 网络同步与无氧池集成(预计3周)

- [ ] **PlayerBase.c修改**
  - Sprint发起检查(双池门槛)
  - RPC处理(RpcAsk_StartSprint等)
  - 客户端预测+服务器校正

- [ ] **StaminaUpdateCoordinator集成**
  - 调用AnaerobicBurstState.TickUpdate()
  - 无氧消耗分流
  - Sprint速度衰减(无氧<20%时)

- [ ] **CharacterCamera1stPerson**
  - FOV绑定无氧池
  - 硬撑时视角反馈

- [ ] **联机测试**
  - 双人同步验证
  - 冷却状态一致性
  - RPC往返延迟处理

### Phase 3: 系数物理化与优化(预计2周)

- [ ] **物理化能量转换**
  - sustainable_watts反算能量-体力转换系数
  - 1.4 m/s @ 35kg净消耗接近0%

- [ ] **优化器v5**
  - rss_pipeline_v5.py
  - 硬约束: 4h行军>20% / 5-15s冲刺 / 120-180s冷却
  - 软约束: 8场景剖面

### Phase 4: AI与文档(预计2周)

- [ ] **SCR_RSS_AIStaminaState**
  - AI简化模型(单池+冷却)
  - 冷却略短(90s vs 120s)

- [ ] **行为树集成**
  - Sprint条件检查
  - 时长限制

- [ ] **文档与迁移**
  - v4->v5自动迁移
  - LegacyV4Style预设
  - 首次启动弹窗

---

## 代码结构

```
scripts/Game/RSS/Core/
├── SCR_AnaerobicBurstState.c          [NEW] 无氧池
├── SCR_MetabolicSpeedLimiter.c        [NEW] 强制降速
├── SCR_RSS_V5_Params.c                [NEW] v5参数
├── SCR_StaminaUpdateCoordinator.c     [MODIFY] 集成无氧
├── SCR_SpeedCalculation.c             [MODIFY] 速度重标定
├── SCR_StaminaConstants.c             [MODIFY] v5常量
└── ...

scripts/Game/RSS/Presentation/
├── SCR_UISignalBridge_V5.c            [NEW] UI信号系统
├── SCR_StaminaHUDComponent.c          [MODIFY] 集成CD环
└── ...

tools/
├── bench_physio_anchors_v5.py         [NEW] 基准测试
├── rss_digital_twin_fix.py            [MODIFY] 支持双池
└── rss_pipeline_v5.py                 [NEW] v5优化器
```

---

## 测试命令

### 运行基准测试(手动模式)
```bash
cd tools/
python bench_physio_anchors_v5.py --manual
```

### 运行单个测试
```bash
pytest bench_physio_anchors_v5.py::TestPhysiologicalAnchorsV5::test_anchor_a_march_4h -v
```

### 运行所有测试(跳过耗时)
```bash
pytest bench_physio_anchors_v5.py -m "not slow" -v
```

---

## 验收标准

### Phase 0 验收
- [x] 基准测试脚本可运行
- [x] 三个锚点的计算模型正确
- [ ] 孪生对接完成
- [ ] 至少2名测试者完成体感清单

### Phase 1 验收
- [ ] v_drain闭环 <5%误差
- [ ] 2.87 m/s强制降速 <3分钟
- [ ] 灌木内行军bench通过
- [ ] UI/音效/FOV反馈可见

### Phase 2 验收
- [ ] 冲刺维持5-15秒
- [ ] 冷却时间正确(180s抽干/60-90s短冲)
- [ ] 联机双人状态同步
- [ ] Sprint可在有氧低时禁用

### Phase 3 验收
- [ ] 优化器收敛(100次迭代)
- [ ] 三档预设生成
- [ ] 实机体感确认

---

## 关键决策记录

### 1. 无氧池存储方案
**决策**: 新组件 `SCR_AnaerobicBurstState`
**理由**: 避免与m_fTargetStamina冲突,独立同步策略
**风险**: 引入新组件增加系统复杂度
**缓解**: 使用标准RplProp框架

### 2. 网络同步模式
**决策**: 混合模式(服务器权威+客户端预测+校正)
**理由**: 平衡输入延迟与公平性
**风险**: 客户端预测失败时输入黏滞
**缓解**: 智能同步阈值(2%-0.5s)减少校正频度

### 3. 速度-消耗解耦
**决策**: v_drain = min(GetVelocity, theoretical)
**理由**: 修复v4 "10km/h走14分钟"问题
**风险**: 与现有恢复系统兼容性
**缓解**: Phase 1完整测试

### 4. UI设计
**决策**: 禁止RPG双条,采用冲刺CD环
**理由**: 游戏化呈现,避免UI复杂
**风险**: 新手可能不理解CD含义
**缓解**: 教程+提示+社区说明

---

## 下一步行动

1. **集成现有代码** 
   - 检查与3.23.x灌木桥接的兼容性
   - 确认CharacterCamera1stPerson接口

2. **Phase 1实现**
   - 修改StaminaUpdateCoordinator
   - 单元测试验证

3. **Phase 0实机测试**
   - Workbench跑1km行军测试
   - 参数调整建议

---

## 文件清单

| 文件 | 行数 | 状态 | 备注 |
|------|------|------|------|
| bench_physio_anchors_v5.py | 450 | ✅ | 4个测试用例 |
| SCR_AnaerobicBurstState.c | 350 | ✅ | 完整注释 |
| SCR_MetabolicSpeedLimiter.c | 300 | ✅ | 支持硬撑 |
| SCR_RSS_V5_Params.c | 180 | ✅ | 3档预设 |
| SCR_UISignalBridge_V5.c | 320 | ✅ | CD环+警告 |

**总代码量**: ~1600行(新增代码)

---

## 风险看板

| 风险 | 等级 | 缓解措施 | 状态 |
|------|------|---------|------|
| 孪生vs实机偏离 | 🔴 高 | Phase 0实机测试 | ⏳待执行 |
| 网络不同步 | 🔴 高 | 混合模式+心跳 | ✅框架就绪 |
| 优化器无解 | 🟡 中 | 约束分级 | ⏳待测试 |
| 玩家接受度 | 🟡 中 | Legacy模式 | ✅设计就绪 |

---

## 变更日志

| 日期 | 作者 | 变更 |
|------|------|------|
| 2026-06-02 | Agent | Phase 0框架完成,5个新增文件 |
