# RSS v5 体力系统完整实施方案

> **版本**: v1.0  
> **日期**: 2026-06-02  
> **基于**: RSS_v5_体力系统修改计划.md v0.3  
> **目标**: 补充原计划中的技术细节、实现方案、风险缓解措施

---

## 目录

1. [架构设计详解](#一架构设计详解)
2. [数据结构与存储方案](#二数据结构与存储方案)
3. [网络同步完整方案](#三网络同步完整方案)
4. [核心算法实现](#四核心算法实现)
5. [UI/UX完整设计](#五uiux完整设计)
6. [AI系统适配方案](#六ai系统适配方案)
7. [测试与验收标准](#七测试与验收标准)
8. [迁移与兼容性方案](#八迁移与兼容性方案)
9. [风险缓解措施](#九风险缓解措施)
10. [开发排期与里程碑](#十开发排期与里程碑)

---

## 一、架构设计详解

### 1.1 系统层次结构

```
┌─────────────────────────────────────────────────────────────┐
│  PlayerBase.c / SCR_CharacterStaminaComponent               │
│  - 体力系统入口点                                            │
│  - 每帧/每刻度调用                                           │
└──────────────────────────┬──────────────────────────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         │                                   │
┌────────▼──────────┐            ┌──────────▼─────────────┐
│ 有氧池 (Aerobic)   │            │ 无氧池 (Anaerobic)      │
│ - 行军/跑步        │            │ - 冲刺爆发              │
│ - 长时间活动       │            │ - 短时高强度            │
│ - HUD主显示        │            │ - 冲刺CD显示            │
└────────┬──────────┘            └──────────┬─────────────┘
         │                                   │
         └─────────────────┬─────────────────┘
                           │
         ┌─────────────────▼─────────────────┐
         │  SCR_StaminaUpdateCoordinator     │
         │  - 统一消耗计算                    │
         │  - v_drain速度闭环                 │
         │  - 恢复逻辑协调                    │
         └─────────────────┬─────────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         │                                   │
┌────────▼──────────┐            ┌──────────▼─────────────┐
│ SCR_SpeedCalc     │            │ SCR_MetabolicEngine    │
│ - 负重限速         │            │ - Pandolf模型          │
│ - 强制降速         │            │ - 代谢功率计算         │
│ - 速度token管理    │            │ - 环境修正             │
└───────────────────┘            └────────────────────────┘
```

### 1.2 双池交互规则

| 动作状态 | 有氧池消耗 | 无氧池消耗 | 速度限制 | 备注 |
|---------|-----------|-----------|---------|------|
| **静止/慢走** | 0或微正(恢复) | 恢复(分层CD) | Walk限速 | 主要恢复状态 |
| **行军(1.4 m/s)** | 极低(接近平衡) | 不消耗 | Walk限速 | 可持续4-5h |
| **跑步(2.8 m/s)** | 中等 | 不消耗 | Run限速 | 有氧为主 |
| **冲刺(4.0 m/s)** | 低 | **高** | Sprint限速 | 无氧为主 |
| **无氧耗尽** | 继续按Run/Walk | **冷却中** | 禁止Sprint | 关键门槛 |
| **有氧过低(<15%)** | 跛行恢复区 | 暂停恢复 | 步行限速 | 力竭状态 |
| **彻底力竭(0%)** | 禁止恢复 | 冻结 | 跛行 | 需休息 |

**关键设计原则**:
- 无氧池耗尽 ≠ 有氧池耗尽
- 冲刺同时消耗两池，但无氧池是**门槛条件**
- 有氧池是**长期耐力指标**

---

## 二、数据结构与存储方案

### 2.1 核心状态结构

#### 2.1.1 新增组件: `SCR_AnaerobicBurstState.c`

```cpp
// 无氧爆发状态组件
[ComponentEditorProps(category: "RSS/Stamina", description: "无氧冲刺爆发状态管理")]
class SCR_AnaerobicBurstStateComponent : ScriptComponent
{
    // ============ 网络同步变量 ============
    [RplProp(onRplName: "OnAnaerobicChanged")]
    protected float m_fAnaerobicEnergy;  // 0-1, 当前无氧池能量
    
    [RplProp(onRplName: "OnCooldownChanged")]
    protected float m_fCooldownRemaining;  // 秒, 剩余冷却时间
    
    [RplProp()]
    protected bool m_bCanSprint;  // 是否可冲刺(服务器权威)
    
    // ============ 本地计算变量 ============
    protected float m_fLastSprintExitEnergy;  // 上次退出冲刺时的能量
    protected float m_fSprintStartTime;       // 本次冲刺开始时间
    protected bool m_bIsSprintActive;         // 是否正在冲刺
    
    // ============ 配置参数(从预设读取) ============
    protected float m_fAnaerobicDrainRate;      // 消耗速率(每秒)
    protected float m_fAnaerobicRecoveryBase;   // 基础恢复速率
    protected float m_fSprintEnableThreshold;   // 可冲刺阈值(0.2)
    protected float m_fCooldownFull;            // 满冷却时间(180s Elite)
    protected float m_fCooldownShort;           // 短冲刺冷却(60-90s)
    protected float m_fEarlyReleaseBonusCoeff;  // 提前释放奖励系数
    
    // ============ 公开接口 ============
    
    // 查询接口
    float GetAnaerobicEnergy() { return m_fAnaerobicEnergy; }
    bool CanSprint() { return m_bCanSprint && m_fCooldownRemaining <= 0; }
    float GetCooldownRemaining() { return m_fCooldownRemaining; }
    float GetCooldownProgress() { return 1.0 - (m_fCooldownRemaining / m_fCooldownFull); }
    
    // 状态变更接口(仅服务器调用)
    void OnSprintStart(float currentVelocity, float loadWeight);
    void OnSprintEnd(bool forcedByDepletion);
    void TickUpdate(float deltaTime, float aerobicStamina);
    
    // 网络回调
    protected void OnAnaerobicChanged();
    protected void OnCooldownChanged();
}
```

#### 2.1.2 扩展现有: `SCR_CharacterStaminaComponent`

```cpp
class SCR_CharacterStaminaComponent : ScriptComponent
{
    // ============ 现有变量(保持) ============
    [RplProp(onRplName: "OnStaminaChanged")]
    protected float m_fTargetStamina;  // 重命名语义: 有氧池(aerobic)
    
    // ============ 新增引用 ============
    protected SCR_AnaerobicBurstStateComponent m_AnaerobicState;
    
    // ============ Schema版本控制 ============
    protected static const int STAMINA_SCHEMA_V4 = 4;
    protected static const int STAMINA_SCHEMA_V5 = 5;
    [RplProp()]
    protected int m_iStaminaSchemaVersion = STAMINA_SCHEMA_V5;
    
    // ============ v5新增方法 ============
    
    // 获取有氧池(明确语义)
    float GetAerobicStamina() { return m_fTargetStamina; }
    
    // 获取无氧组件
    SCR_AnaerobicBurstStateComponent GetAnaerobicState() 
    { 
        if (!m_AnaerobicState)
            m_AnaerobicState = SCR_AnaerobicBurstStateComponent.Cast(GetOwner().FindComponent(SCR_AnaerobicBurstStateComponent));
        return m_AnaerobicState; 
    }
    
    // 检查是否可以发起冲刺(双池门槛)
    bool CanInitiateSprint()
    {
        if (m_fTargetStamina < RSS_V5_AEROBIC_SPRINT_MIN)  // 有氧池最低要求(如0.1)
            return false;
        
        auto anaerobic = GetAnaerobicState();
        if (!anaerobic)
            return false;  // 降级到v4行为或禁用
            
        return anaerobic.CanSprint();
    }
}
```

### 2.2 参数配置结构

#### 2.2.1 `SCR_RSS_Params.c` 新增字段

```cpp
class SCR_RSS_Params
{
    // ============ v5新增: 无氧系统参数 ============
    
    // 无氧消耗与恢复
    float anaerobic_drain_coeff_base;       // 基础消耗系数(速度相关)
    float anaerobic_drain_load_factor;      // 负重增益因子
    float anaerobic_recovery_base_rate;     // 基础恢复速率(/s)
    float anaerobic_recovery_aerobic_gate;  // 有氧池门槛(低于此减速恢复)
    
    // 冷却时间配置
    float burst_cooldown_full_seconds;      // 抽干后满冷却(180s Elite)
    float burst_cooldown_short_seconds;     // 短冲刺基准冷却(60-90s)
    float burst_early_release_bonus;        // 提前释放奖励(0.4-0.5)
    
    // 冲刺门槛
    float sprint_enable_anaerobic_threshold; // 无氧池阈值(0.20)
    float sprint_enable_aerobic_min;         // 有氧池最低要求(0.10)
    
    // ============ v5新增: 强制降速参数 ============
    
    float sustainable_watts_default;         // 可持续功率基准(400W)
    float sustainable_watts_load_penalty;    // 负重惩罚系数
    float forced_slowdown_decay_rate;        // 降速衰减速率(%/s)
    float forced_slowdown_min_ratio;         // 最低速度比例(0.5)
    bool enable_player_override_slowdown;    // 允许硬撑
    float override_stamina_penalty_mult;     // 硬撑惩罚倍率(1.5-2.0)
    float override_max_duration_seconds;     // 硬撑最长时间(30-60s)
    
    // ============ v5新增: 速度重标定 ============
    
    // 35kg理想环境下的目标速度(m/s)
    float walk_speed_ideal_35kg;    // 1.4 (Elite) / 1.5 (Standard) / 1.7 (Tactical)
    float run_speed_ideal_35kg;     // 2.8 / 3.0 / 3.2
    float sprint_speed_ideal_35kg;  // 4.0 / 4.2 / 4.5
    
    // ============ v5修正: 物理化系数 ============
    
    // 移除或重新定义
    // float energy_to_stamina_coeff;  // 现在通过sustainable_watts反算
    // float load_metabolic_dampening; // v5设为1.0
    
    float reference_march_speed_ms;  // 参考行军速度(1.4)
    float reference_watts_at_march;  // 参考点功率(400W)
    
    // ============ v5新增: Legacy兼容开关 ============
    
    bool legacy_v4_single_pool_mode;      // true=退回单池
    bool legacy_v4_disable_forced_slow;   // true=禁用强制降速
    bool legacy_v4_use_old_drain_velocity; // true=使用固定3.8/5.5
}
```

### 2.3 网络数据包结构

```cpp
// 无氧状态同步包(紧凑格式, ~12 bytes)
class SCR_AnaerobicStateSyncPacket
{
    float energy;           // 4 bytes, 0-1
    float cooldown;         // 4 bytes, 秒
    bool canSprint;         // 1 byte
    uint8 sequenceNumber;   // 1 byte, 用于丢包检测
    // 总计: ~10-12 bytes
}

// 同步频率策略
// - 冲刺期间: 每0.2秒同步一次(与体力同频)
// - 冷却期间: 每1秒同步一次
// - 静止时: 每5秒心跳一次
```

---

## 三、网络同步完整方案

### 3.1 同步架构选择

**决策: 混合模式 = 服务器权威 + 客户端预测 + 校正**

| 方案 | 优点 | 缺点 | 采用 |
|------|------|------|------|
| 纯服务器权威 | 绝对公平 | 输入延迟明显 | ❌ |
| 纯客户端预测 | 无延迟 | 易作弊/不同步 | ❌ |
| **混合模式** | 平衡体验与公平 | 需校正逻辑 | ✅ |

### 3.2 实现细节

#### 3.2.1 客户端预测逻辑

```cpp
// PlayerBase.c - 客户端输入处理
void OnSprintKeyPressed()
{
    // 1. 本地立即检查
    if (!m_StaminaComponent.CanInitiateSprint())
    {
        // 播放本地反馈: 音效/UI提示
        PlaySprintDeniedFeedback();
        return;
    }
    
    // 2. 本地预测开始冲刺
    m_bLocalPredictedSprinting = true;
    m_fSprintPredictStartTime = GetGame().GetWorld().GetWorldTime();
    
    // 3. 发送RPC到服务器
    Rpc(RpcAsk_StartSprint, GetVelocity(), GetLoadWeight());
}

// 服务器验证
[RpcTarget(Server)]
void RpcAsk_StartSprint(float clientVelocity, float clientLoad)
{
    auto anaerobic = m_StaminaComponent.GetAnaerobicState();
    
    // 服务器权威检查
    bool serverAllows = m_StaminaComponent.CanInitiateSprint();
    
    if (serverAllows)
    {
        // 允许冲刺
        anaerobic.OnSprintStart(clientVelocity, clientLoad);
        
        // 广播给所有客户端(除请求者)
        Rpc(RpcDo_ConfirmSprint, GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(this));
    }
    else
    {
        // 拒绝冲刺 - 发送校正包
        Rpc(RpcDo_RejectSprint, anaerobic.GetAnaerobicEnergy(), anaerobic.GetCooldownRemaining());
    }
}

// 客户端接收校正
[RpcTarget(Caller)]
void RpcDo_RejectSprint(float serverEnergy, float serverCooldown)
{
    // 回滚本地预测
    m_bLocalPredictedSprinting = false;
    
    // 同步服务器状态
    auto anaerobic = m_StaminaComponent.GetAnaerobicState();
    anaerobic.ForceSetEnergy(serverEnergy);
    anaerobic.ForceSetCooldown(serverCooldown);
    
    // 播放校正反馈
    PlaySprintCorrectionFeedback();
}
```

#### 3.2.2 状态同步策略

```cpp
// SCR_AnaerobicBurstStateComponent.c

void TickUpdate(float deltaTime, float aerobicStamina)
{
    if (!Replication.IsServer())
        return;  // 仅服务器执行
    
    float oldEnergy = m_fAnaerobicEnergy;
    float oldCooldown = m_fCooldownRemaining;
    
    // 更新逻辑(见4.2节)
    UpdateAnaerobicEnergy(deltaTime);
    UpdateCooldown(deltaTime, aerobicStamina);
    
    // 智能同步判断
    bool needSync = false;
    
    // 条件1: 能量变化超过阈值
    if (Math.AbsFloat(m_fAnaerobicEnergy - oldEnergy) > 0.02)  // 2%阈值
        needSync = true;
    
    // 条件2: 冷却时间变化超过阈值
    if (Math.AbsFloat(m_fCooldownRemaining - oldCooldown) > 0.5)  // 0.5秒阈值
        needSync = true;
    
    // 条件3: 状态切换(可冲刺<->不可冲刺)
    bool canSprintNow = (m_fAnaerobicEnergy >= m_fSprintEnableThreshold) && (m_fCooldownRemaining <= 0);
    if (canSprintNow != m_bCanSprint)
    {
        m_bCanSprint = canSprintNow;
        needSync = true;
    }
    
    // 条件4: 强制心跳(每5秒)
    m_fSyncHeartbeatTimer += deltaTime;
    if (m_fSyncHeartbeatTimer >= 5.0)
    {
        m_fSyncHeartbeatTimer = 0;
        needSync = true;
    }
    
    if (needSync)
    {
        // RplProp自动同步
        Replication.BumpMe();
    }
}
```

### 3.3 带宽分析

| 场景 | 数据包大小 | 频率 | 带宽(每玩家) |
|------|-----------|------|--------------|
| 冲刺期间 | ~12 bytes | 5 Hz | 60 bytes/s |
| 冷却期间 | ~12 bytes | 1 Hz | 12 bytes/s |
| 静止期间 | ~12 bytes | 0.2 Hz | 2.4 bytes/s |
| **均值估算** | - | - | **~20 bytes/s** |

**结论**: 对于Arma Reforger的联机环境,这是**可忽略的开销**。

---

## 四、核心算法实现

### 4.1 速度-消耗闭环(Phase 1核心)

#### 4.1.1 修正后的消耗速度计算

```cpp
// SCR_StaminaUpdateCoordinator.c

float GetDrainVelocity(PlayerBase player, EMovementState moveState)
{
    vector velocity = player.GetVelocity();
    float vMeasured = velocity.Length();  // 水平速度
    
    // 获取理论限速
    float vTheoretical = 0;
    
    switch (moveState)
    {
        case EMovementState.WALK:
            vTheoretical = m_SpeedCalc.GetWalkSpeedLimit(player.GetLoadWeight());
            break;
            
        case EMovementState.RUN:
            vTheoretical = m_SpeedCalc.GetRunSpeedLimit(player.GetLoadWeight());
            break;
            
        case EMovementState.SPRINT:
            // Sprint有特殊处理: 峰值允许超限,但decay
            vTheoretical = m_SpeedCalc.GetSprintSpeedLimit(player.GetLoadWeight());
            
            // 检查无氧池衰减
            auto anaerobic = player.GetStaminaComponent().GetAnaerobicState();
            if (anaerobic)
            {
                float anaerobicRatio = anaerobic.GetAnaerobicEnergy();
                // Sprint速度随无氧池线性衰减(最后20%时速度降到Run档)
                if (anaerobicRatio < 0.20)
                {
                    float decayFactor = anaerobicRatio / 0.20;
                    float vRun = m_SpeedCalc.GetRunSpeedLimit(player.GetLoadWeight());
                    vTheoretical = Math.Lerp(vRun, vTheoretical, decayFactor);
                }
            }
            break;
    }
    
    // 关键修正: 消耗速度 = min(测量值, 理论值)
    float vDrain = Math.Min(vMeasured, vTheoretical);
    
    // Legacy v4模式兼容
    if (m_Params.legacy_v4_use_old_drain_velocity)
    {
        // 回退到固定速度(v4 bug行为)
        if (moveState == EMovementState.RUN)
            vDrain = 3.8;
        else if (moveState == EMovementState.SPRINT)
            vDrain = 5.5;
    }
    
    return vDrain;
}
```

#### 4.1.2 强制降速机制

```cpp
// SCR_SpeedCalculation.c

class SCR_MetabolicSpeedLimiter
{
    protected float m_fCurrentSpeedRatio = 1.0;  // 当前速度比例(1.0=无限制)
    protected float m_fOverrideTimer = 0;        // 硬撑计时器
    protected bool m_bPlayerOverriding = false;  // 玩家是否正在硬撑
    
    // 每帧调用
    void UpdateForcedSlowdown(float deltaTime, PlayerBase player, float pandolfWatts)
    {
        float sustainableWatts = CalculateSustainableWatts(player);
        
        // 检查是否超过可持续功率
        if (pandolfWatts > sustainableWatts)
        {
            // 超载情况
            float overloadRatio = pandolfWatts / sustainableWatts;  // >1.0
            
            // 玩家硬撑逻辑
            if (m_bPlayerOverriding && m_Params.enable_player_override_slowdown)
            {
                m_fOverrideTimer += deltaTime;
                
                // 硬撑超时或有氧池过低则强制结束
                if (m_fOverrideTimer > m_Params.override_max_duration_seconds ||
                    player.GetStaminaComponent().GetAerobicStamina() < 0.1)
                {
                    m_bPlayerOverriding = false;
                    // 触发强制恢复窗口(禁止再次硬撑30秒)
                    StartRecoveryWindow();
                }
            }
            else
            {
                // 正常降速: 渐进式下调
                float decayRate = m_Params.forced_slowdown_decay_rate * deltaTime;
                decayRate *= Math.Pow(overloadRatio - 1.0, 0.5);  // 超载越多,降速越快
                
                m_fCurrentSpeedRatio -= decayRate;
                m_fCurrentSpeedRatio = Math.Max(m_fCurrentSpeedRatio, m_Params.forced_slowdown_min_ratio);
            }
            
            // 触发UI/音效反馈
            if (m_fCurrentSpeedRatio < 0.95)
                TriggerSlowdownFeedback(player, overloadRatio);
        }
        else
        {
            // 功率正常,逐渐恢复速度比例
            if (m_fCurrentSpeedRatio < 1.0)
            {
                m_fCurrentSpeedRatio += 0.05 * deltaTime;  // 每秒恢复5%
                m_fCurrentSpeedRatio = Math.Min(m_fCurrentSpeedRatio, 1.0);
            }
            
            m_fOverrideTimer = 0;
        }
    }
    
    float CalculateSustainableWatts(PlayerBase player)
    {
        float base = m_Params.sustainable_watts_default;  // 400W
        
        // 负重惩罚
        float load = player.GetLoadWeight();
        float penalty = (load - 35.0) * m_Params.sustainable_watts_load_penalty;  // 超过35kg每kg惩罚
        
        // TODO Phase 4+: 加入热量、坡度等修正
        
        return Math.Max(base - penalty, 200.0);  // 最低200W
    }
    
    // 应用到限速token
    void ApplyToSpeedToken(SCR_RSS_CharacterSpeedBridge bridge)
    {
        float currentLimit = bridge.GetCurrentLimit();
        float adjustedLimit = currentLimit * m_fCurrentSpeedRatio;
        
        bridge.SetRSSStaminaSpeedLimitToken(adjustedLimit);
    }
}
```

### 4.2 无氧池动力学

#### 4.2.1 无氧消耗

```cpp
// SCR_AnaerobicBurstStateComponent.c

void UpdateAnaerobicEnergy(float deltaTime)
{
    if (!m_bIsSprintActive)
    {
        // 非冲刺状态: 恢复逻辑(见4.2.2)
        RecoverAnaerobicEnergy(deltaTime);
        return;
    }
    
    // 冲刺消耗计算
    PlayerBase player = PlayerBase.Cast(GetOwner());
    float vDrain = GetDrainVelocity();  // 从coordinator获取
    float load = player.GetLoadWeight();
    
    // 基础消耗: 与速度和负重相关
    float baseDrain = m_fAnaerobicDrainRate * (vDrain / 4.0);  // 4.0 m/s为参考冲刺速度
    
    // 负重增益(35kg为基准)
    float loadFactor = 1.0 + (load - 35.0) * m_fAnaerobicDrainLoadFactor;
    loadFactor = Math.Max(loadFactor, 0.5);  // 最低0.5倍
    
    float totalDrain = baseDrain * loadFactor * deltaTime;
    
    // 扣除无氧池
    m_fAnaerobicEnergy -= totalDrain;
    m_fAnaerobicEnergy = Math.Max(m_fAnaerobicEnergy, 0);
    
    // 检查是否耗尽(自动结束冲刺)
    if (m_fAnaerobicEnergy < 0.01)  // 接近0
    {
        OnSprintEnd(true);  // forcedByDepletion=true
    }
}
```

#### 4.2.2 无氧恢复与分层冷却

```cpp
void RecoverAnaerobicEnergy(float deltaTime)
{
    // 冷却期间禁止恢复
    if (m_fCooldownRemaining > 0)
    {
        m_fCooldownRemaining -= deltaTime;
        m_fCooldownRemaining = Math.Max(m_fCooldownRemaining, 0);
        return;
    }
    
    // 有氧池门槛检查
    float aerobic = GetOwner().GetStaminaComponent().GetAerobicStamina();
    if (aerobic < m_fAnaerobicRecoveryAerobicGate)  // 如0.15
    {
        // 有氧池过低,无氧恢复减速或暂停
        return;
    }
    
    // 正常恢复
    float recoveryRate = m_fAnaerobicRecoveryBaseRate * deltaTime;
    
    // 加速恢复buff(如静止/蹲下)
    if (GetOwner().GetVelocity().Length() < 0.1)
        recoveryRate *= 1.2;
    
    m_fAnaerobicEnergy += recoveryRate;
    m_fAnaerobicEnergy = Math.Min(m_fAnaerobicEnergy, 1.0);
}

// 冲刺结束时计算冷却时间
void OnSprintEnd(bool forcedByDepletion)
{
    m_bIsSprintActive = false;
    
    float cooldownTime;
    
    if (forcedByDepletion)
    {
        // 抽干: 满冷却
        cooldownTime = m_fCooldownFull;
    }
    else
    {
        // 提前释放: 分层奖励
        float reserveRatio = m_fAnaerobicEnergy;  // 剩余比例
        float bonus = m_fEarlyReleaseBonusCoeff * reserveRatio;  // 如0.5 * 0.8 = 0.4
        
        cooldownTime = m_fCooldownFull * (1.0 - bonus);
        cooldownTime = Math.Max(cooldownTime, m_fCooldownShort);  // 最低60-90s
    }
    
    m_fCooldownRemaining = cooldownTime;
    
    // 通知UI更新
    NotifyCooldownStart(cooldownTime);
}
```

### 4.3 系数物理化

#### 4.3.1 新的能量-体力转换

```cpp
// SCR_StaminaUpdateCoordinator.c

float CalculateStaminaDrainRate(PlayerBase player, float vDrain, EMovementState moveState)
{
    // 1. 计算Pandolf功率
    float pandolfWatts = m_MetabolicEngine.CalculatePandolf(
        vDrain,
        player.GetLoadWeight(),
        GetCurrentGrade(player),
        GetTerrainFactor(player)
    );
    
    // 2. 计算净消耗功率(减去基础代谢)
    float netWatts = pandolfWatts - m_Params.basal_metabolic_rate;  // 如100W基础代谢
    
    // 3. 参考点标定转换
    // 在1.4 m/s, 35kg, 平路时,功率应接近sustainable_watts(400W)
    // 此时体力变化应接近0
    float referenceWatts = m_Params.reference_watts_at_march;  // 400W
    
    // 转换公式: drainRate = (netWatts - referenceWatts) / scaleFactor
    // scaleFactor使得+100W → -5%/min (示例)
    float scaleFactor = 100.0 / (0.05 / 60.0);  // W / (%/s)
    
    float drainRate = (netWatts - referenceWatts) / scaleFactor;
    
    // 4. Sprint时分流到无氧池
    if (moveState == EMovementState.SPRINT)
    {
        // 80%到无氧池, 20%到有氧池(示例分配)
        drainRate *= 0.20;
    }
    
    // 5. Legacy damping(v5默认移除)
    if (!m_Params.legacy_v4_single_pool_mode)
    {
        // v5: load_metabolic_dampening = 1.0
    }
    else
    {
        // v4兼容
        drainRate *= m_Params.load_metabolic_dampening;
    }
    
    return drainRate;  // %/s
}
```

---

## 五、UI/UX完整设计

### 5.1 HUD元素布局

```
┌────────────────────────────────────────────────┐
│                                                │
│           [准星]  ← 冲刺CD环包围准星           │
│                                                │
│  左下角:                                       │
│  ┌──────────────────────────┐                │
│  │ ████████████░░░░ 68%     │ ← 有氧池(主条)  │
│  └──────────────────────────┘                │
│                                                │
│  [⚡ 冲刺冷却: 45秒]  ← 仅冷却时显示          │
│                                                │
│  [⚠ 节奏过快 建议减速] ← 强制降速警告         │
│                                                │
└────────────────────────────────────────────────┘
```

### 5.2 冲刺CD环设计

```cpp
// UI/SCR_SprintCooldownWidget.c

class SCR_SprintCooldownWidget : SCR_HUDWidget
{
    protected WidgetReference m_wCooldownRing;  // 环形进度条
    protected WidgetReference m_wSprintIcon;    // 冲刺图标
    
    override void Update(float timeslice)
    {
        auto player = GetPlayerBase();
        if (!player)
            return;
        
        auto anaerobic = player.GetStaminaComponent().GetAnaerobicState();
        if (!anaerobic)
            return;
        
        float cooldown = anaerobic.GetCooldownRemaining();
        
        // 仅在以下情况显示:
        // 1. 正在冷却
        // 2. 玩家尝试冲刺但被拒绝(闪烁2秒)
        bool shouldShow = (cooldown > 0) || (m_fDeniedBlinkTimer > 0);
        
        m_wCooldownRing.SetVisible(shouldShow);
        
        if (shouldShow && cooldown > 0)
        {
            // 环形进度: 从0%到100%
            float progress = anaerobic.GetCooldownProgress();
            UpdateRingProgress(m_wCooldownRing, progress);
            
            // 颜色渐变: 红(0%) -> 黄(50%) -> 绿(100%)
            Color color = ColorLerp(COLOR_RED, COLOR_GREEN, progress);
            m_wCooldownRing.SetColor(color);
            
            // 可选: 显示剩余秒数(仅>10秒时)
            if (cooldown > 10.0)
            {
                m_wSprintIcon.SetText(((int)cooldown).ToString() + "s");
            }
        }
    }
    
    // 当尝试冲刺被拒绝时调用
    void OnSprintDenied()
    {
        m_fDeniedBlinkTimer = 2.0;
        PlayDeniedAnimation();  // 红色闪烁+音效
    }
}
```

### 5.3 强制降速反馈

```cpp
// UI/SCR_MetabolicWarningWidget.c

void UpdateSlowdownWarning(float overloadRatio, float speedRatio)
{
    // 仅在速度被降低>10%时显示
    if (speedRatio >= 0.90)
    {
        m_wWarningPanel.SetVisible(false);
        return;
    }
    
    m_wWarningPanel.SetVisible(true);
    
    // 分级警告
    if (speedRatio < 0.60)
    {
        // 严重超载
        m_wWarningText.SetText("#RSS_WARNING_SEVERE_OVERLOAD");  // "严重超载! 强制减速中"
        m_wWarningText.SetColor(COLOR_RED);
        TriggerHeavyBreathingAudio(2.0);  // 沉重喘息
    }
    else if (speedRatio < 0.80)
    {
        // 中度超载
        m_wWarningText.SetText("#RSS_WARNING_MODERATE_OVERLOAD");  // "负重过高 建议放慢节奏"
        m_wWarningText.SetColor(COLOR_YELLOW);
        TriggerHeavyBreathingAudio(1.0);
    }
    
    // 可选: 轻微视野收窄(FOV -2到-5度)
    float fovPenalty = (1.0 - speedRatio) * 5.0;  // 最多-5度
    ApplyMetabolicFOVPenalty(fovPenalty);
    
    // 硬撑提示
    if (m_Params.enable_player_override_slowdown)
    {
        m_wOverrideTip.SetText("#RSS_TIP_HOLD_SPRINT_OVERRIDE");  // "[按住冲刺键] 强行维持速度"
        m_wOverrideTip.SetVisible(true);
    }
}
```

### 5.4 多语言文本定义

```xml
<!-- Localization/stringtable.xml -->

<Container>
    <Package name="RSS_v5_UI">
        <Key ID="RSS_WARNING_SEVERE_OVERLOAD">
            <Original>严重超载! 强制减速中</Original>
            <English>Severe overload! Force slowing down</English>
        </Key>
        
        <Key ID="RSS_WARNING_MODERATE_OVERLOAD">
            <Original>负重过高 建议放慢节奏</Original>
            <English>Heavy load - Suggest slowing pace</English>
        </Key>
        
        <Key ID="RSS_TIP_HOLD_SPRINT_OVERRIDE">
            <Original>[按住冲刺键] 强行维持速度 (消耗额外体力)</Original>
            <English>[Hold Sprint] Override slowdown (drains extra stamina)</English>
        </Key>
        
        <Key ID="RSS_SPRINT_COOLDOWN">
            <Original>冲刺冷却: %1秒</Original>
            <English>Sprint cooldown: %1s</English>
        </Key>
        
        <Key ID="RSS_SPRINT_DENIED_EXHAUSTED">
            <Original>无氧池耗尽 无法冲刺</Original>
            <English>Anaerobic depleted - Cannot sprint</English>
        </Key>
    </Package>
</Container>
```

---

## 六、AI系统适配方案

### 6.1 AI简化模型

**设计原则**: AI不完整模拟玩家双池,但必须遵守冲刺冷却,避免无限冲刺作弊。

```cpp
// AI/SCR_RSS_AIStaminaState.c

class SCR_RSS_AIStaminaState
{
    // AI简化状态
    protected float m_fAIStaminaSimple;         // 0-1, 简化单池(对应玩家有氧池)
    protected float m_fAISprintCooldown;        // 冲刺冷却计时器
    protected bool m_bAICanSprint;              // 是否可冲刺
    protected float m_fAILastSprintTime;        // 上次冲刺时间戳
    
    // AI专用参数(略宽松于玩家)
    protected static const float AI_SPRINT_DURATION_MAX = 8.0;      // AI最长冲刺8秒
    protected static const float AI_SPRINT_COOLDOWN_MIN = 90.0;     // AI冷却90秒(玩家Elite 120秒)
    protected static const float AI_SPRINT_COOLDOWN_SHORT = 45.0;   // AI短冲刺冷却45秒(玩家60-90秒)
    protected static const float AI_SPRINT_STAMINA_GATE = 0.25;     // AI需要25%体力才能冲刺
    
    // AI行为树查询接口
    bool AICanAttemptSprint()
    {
        // 检查冷却
        if (m_fAISprintCooldown > 0)
            return false;
        
        // 检查体力门槛
        if (m_fAIStaminaSimple < AI_SPRINT_STAMINA_GATE)
            return false;
        
        return true;
    }
    
    void AIOnSprintStart()
    {
        m_bAICanSprint = false;
        m_fAILastSprintTime = GetGame().GetWorld().GetWorldTime();
    }
    
    void AIOnSprintEnd(float sprintDuration)
    {
        // 根据冲刺时长决定冷却
        float cooldown;
        
        if (sprintDuration >= AI_SPRINT_DURATION_MAX * 0.7)  // 冲刺>5.6秒
        {
            cooldown = AI_SPRINT_COOLDOWN_MIN;  // 满冷却90秒
        }
        else
        {
            // 短冲刺: 线性插值
            float ratio = sprintDuration / (AI_SPRINT_DURATION_MAX * 0.7);
            cooldown = Math.Lerp(AI_SPRINT_COOLDOWN_SHORT, AI_SPRINT_COOLDOWN_MIN, ratio);
        }
        
        m_fAISprintCooldown = cooldown;
    }
    
    void UpdateAIStamina(float deltaTime)
    {
        // 简化消耗: 使用玩家的有氧池逻辑,忽略无氧池
        // (代码复用StaminaUpdateCoordinator的有氧部分)
        
        // 更新冷却
        if (m_fAISprintCooldown > 0)
        {
            m_fAISprintCooldown -= deltaTime;
            m_fAISprintCooldown = Math.Max(m_fAISprintCooldown, 0);
        }
    }
}
```

### 6.2 行为树集成

```cpp
// AI/BehaviorTree/SCR_AITacticalSprint.c

class SCR_AITacticalSprint : AITaskBase
{
    // 行为树条件检查
    override bool CanPerformTask()
    {
        AIAgent agent = GetAgent();
        SCR_RSS_AIStaminaState aiStamina = GetAIStaminaState(agent);
        
        if (!aiStamina)
            return false;
        
        // 必须通过冷却检查
        if (!aiStamina.AICanAttemptSprint())
            return false;
        
        // 战术条件: 例如距离敌人30-80米,需要快速接近/撤离
        float distToEnemy = GetDistanceToNearestEnemy(agent);
        if (distToEnemy < 30.0 || distToEnemy > 80.0)
            return false;
        
        return true;
    }
    
    override void OnTaskStart()
    {
        AIAgent agent = GetAgent();
        SCR_RSS_AIStaminaState aiStamina = GetAIStaminaState(agent);
        
        aiStamina.AIOnSprintStart();
        
        // 设置移动速度为冲刺
        agent.GetMovementComponent().SetMoveSpeed(EMoveSpeed.SPRINT);
        
        m_fSprintStartTime = GetGame().GetWorld().GetWorldTime();
    }
    
    override EAITaskResult OnTaskTick(float deltaTime)
    {
        AIAgent agent = GetAgent();
        float sprintDuration = GetGame().GetWorld().GetWorldTime() - m_fSprintStartTime;
        
        // AI自动限制冲刺时长
        if (sprintDuration >= SCR_RSS_AIStaminaState.AI_SPRINT_DURATION_MAX)
        {
            return EAITaskResult.COMPLETED;
        }
        
        // AI体力过低强制停止
        SCR_RSS_AIStaminaState aiStamina = GetAIStaminaState(agent);
        if (aiStamina.GetAIStamina() < 0.15)
        {
            return EAITaskResult.FAILED;
        }
        
        return EAITaskResult.IN_PROGRESS;
    }
    
    override void OnTaskEnd(EAITaskResult result)
    {
        AIAgent agent = GetAgent();
        SCR_RSS_AIStaminaState aiStamina = GetAIStaminaState(agent);
        
        float sprintDuration = GetGame().GetWorld().GetWorldTime() - m_fSprintStartTime;
        aiStamina.AIOnSprintEnd(sprintDuration);
        
        // 恢复正常移动速度
        agent.GetMovementComponent().SetMoveSpeed(EMoveSpeed.RUN);
    }
}
```

---

## 七、测试与验收标准

### 7.1 Phase 0 基线测试

#### 7.1.1 自动化基准测试

```python
# tools/bench_physio_anchors.py

import pytest
from rss_digital_twin_fix import GamePlayer, RSS_Params

class TestPhysiologicalAnchors:
    """v5生理锚点基准测试"""
    
    @pytest.mark.parametrize("preset", ["Elite", "Standard", "Tactical"])
    def test_35kg_march_4h(self, preset):
        """锚点A: 35kg理想环境下行军4小时"""
        params = RSS_Params.load_preset(preset)
        player = GamePlayer(params)
        
        # 初始化
        player.load_weight = 35.0
        player.temperature = 20.0
        player.terrain = "flat"
        player.aerobic_stamina = 1.0
        
        # 模拟4小时行军 @ 1.4 m/s
        duration_seconds = 4 * 3600
        march_speed = params.walk_speed_ideal_35kg  # 1.4 m/s (Elite)
        
        for _ in range(int(duration_seconds / 0.017)):  # 17ms tick
            player.tick(0.017, "WALK", march_speed)
        
        # 验收标准
        assert player.aerobic_stamina > 0.20, \
            f"{preset}: 4h行军后有氧池应>20%, 实际={player.aerobic_stamina:.2f}"
        
        # 可选: 检查代谢功率接近可持续水平
        watts = player.calculate_pandolf(march_speed, player.load_weight)
        assert 350 <= watts <= 500, \
            f"{preset}: 1.4m/s代谢功率应在350-500W, 实际={watts:.1f}W"
    
    @pytest.mark.parametrize("preset", ["Elite", "Standard", "Tactical"])
    def test_35kg_sprint_burst(self, preset):
        """锚点B: 35kg满速冲刺5-15秒"""
        params = RSS_Params.load_preset(preset)
        player = GamePlayer(params)
        
        player.load_weight = 35.0
        player.aerobic_stamina = 1.0
        player.anaerobic_energy = 1.0
        
        sprint_speed = params.sprint_speed_ideal_35kg  # ~4.0 m/s
        
        sprint_duration = 0
        while player.anaerobic_energy > 0.01:
            player.tick(0.017, "SPRINT", sprint_speed)
            sprint_duration += 0.017
            
            # 安全阀: 防止无限循环
            if sprint_duration > 60:
                break
        
        # 验收标准
        if preset == "Elite":
            assert 5.0 <= sprint_duration <= 15.0, \
                f"Elite: 冲刺应维持5-15秒, 实际={sprint_duration:.1f}s"
        elif preset == "Standard":
            assert 8.0 <= sprint_duration <= 20.0, \
                f"Standard: 冲刺应维持8-20秒, 实际={sprint_duration:.1f}s"
        elif preset == "Tactical":
            assert 10.0 <= sprint_duration <= 25.0, \
                f"Tactical: 冲刺应维持10-25秒, 实际={sprint_duration:.1f}s"
    
    def test_elite_sprint_cooldown_full(self):
        """无氧抽干后冷却时间"""
        params = RSS_Params.load_preset("Elite")
        player = GamePlayer(params)
        
        # 抽干无氧池
        player.anaerobic_energy = 0.0
        player.on_sprint_end(forced_by_depletion=True)
        
        # 检查冷却时间
        assert 120 <= player.anaerobic_cooldown <= 180, \
            f"Elite抽干冷却应为120-180秒, 实际={player.anaerobic_cooldown:.1f}s"
    
    def test_elite_sprint_cooldown_short(self):
        """短冲刺(3秒)后冷却时间"""
        params = RSS_Params.load_preset("Elite")
        player = GamePlayer(params)
        
        # 冲刺3秒后主动停止
        player.anaerobic_energy = 1.0
        for _ in range(int(3.0 / 0.017)):
            player.tick(0.017, "SPRINT", 4.0)
        
        player.on_sprint_end(forced_by_depletion=False)
        
        # 检查冷却时间(应享受提前释放奖励)
        assert 60 <= player.anaerobic_cooldown <= 90, \
            f"Elite短冲刺冷却应为60-90秒, 实际={player.anaerobic_cooldown:.1f}s"
```

#### 7.1.2 实机体感清单

```markdown
## Workbench实机测试清单 (Phase 0)

### 测试环境
- 地图: Everon
- 天气: 晴天20°C
- 负重: 精确35kg (步枪+弹药+背包)
- 预设: 三档都要测

### A. 行军节奏测试
- [ ] Elite 1.4 m/s 行军1km,主观感受是否过慢
- [ ] Tactical 1.7 m/s 行军同样1km,对比体感
- [ ] 测试期间记录: 是否感到无聊? 是否希望能跑?

### B. 冲刺爆发测试
- [ ] 从静止加速到最大冲刺,维持至自动降速
  - 记录秒表时间
  - 主观:是否感觉合理?
- [ ] 短冲刺2-3秒后松开,观察冷却提示
  - 冷却环是否清晰?
  - 是否理解"提前释放奖励"?
- [ ] 抽干无氧池后,尝试再次冲刺
  - 180秒等待是否过长?
  - 是否因为冷却而不敢冲刺?

### C. 强制降速测试
- [ ] 35kg下尝试维持2.5 m/s步行>5分钟
  - 是否触发降速?
  - UI警告是否清晰?
  - 是否像bug还是合理的疲劳?
- [ ] 测试硬撑功能(按住加速)
  - 能否理解这是主动选择?
  - 惩罚是否明显?

### D. 灌木交互测试
- [ ] 在密集灌木中行军,观察速度token叠加
  - RSS降速 + 灌木降速是否合理叠加?
  - 体力消耗是否正常?

### 验收标准
- 至少2名测试者完成全部测试
- 所有"是否像bug"问题回答"否"
- 记录任何参数调整建议
```

### 7.2 Phase 1 验收测试

```cpp
// tests/integration/test_v5_phase1.cpp

TEST(RSS_v5_Phase1, DrainVelocityConsistency)
{
    // 测试: 消耗速度与理论速度一致性
    
    PlayerBase player = CreateTestPlayer();
    player.SetLoadWeight(35.0);
    
    // 场景1: 步行
    player.SetMovementState(EMovementState.WALK);
    player.SimulateTick(1.0);  // 1秒
    
    float vTheoretical = GetWalkSpeedLimit(35.0);
    float vDrain = GetLastDrainVelocity();
    
    EXPECT_NEAR(vDrain, vTheoretical, vTheoretical * 0.05);  // 误差<5%
    
    // 场景2: 跑步 (不应使用固定3.8)
    player.SetMovementState(EMovementState.RUN);
    player.SimulateTick(1.0);
    
    vTheoretical = GetRunSpeedLimit(35.0);
    vDrain = GetLastDrainVelocity();
    
    EXPECT_NE(vDrain, 3.8);  // 不应是固定值
    EXPECT_NEAR(vDrain, vTheoretical, vTheoretical * 0.05);
}

TEST(RSS_v5_Phase1, ForcedSlowdownAt287)
{
    // 测试: 2.87 m/s下强制降速
    
    PlayerBase player = CreateTestPlayer();
    player.SetLoadWeight(35.0);
    player.SetPreset("Elite");
    
    // 尝试维持2.87 m/s步行
    float targetSpeed = 2.87;
    player.SetMovementState(EMovementState.WALK);
    
    float duration = 0;
    float currentSpeed = targetSpeed;
    
    while (currentSpeed > 1.5 && duration < 300)  // 最多5分钟
    {
        player.SimulateTickAtSpeed(1.0, targetSpeed);
        currentSpeed = player.GetCurrentSpeedLimit();
        duration += 1.0;
    }
    
    // 验收: 应在3分钟内降速至~1.4 m/s
    EXPECT_LE(duration, 180);  // ≤3分钟
    EXPECT_NEAR(currentSpeed, 1.4, 0.2);  // 最终速度~1.4
}
```

### 7.3 Phase 2 验收测试

```cpp
TEST(RSS_v5_Phase2, AnaerobicDepletion)
{
    // 测试: 无氧池耗尽时间
    
    PlayerBase player = CreateTestPlayer();
    player.SetLoadWeight(35.0);
    player.SetPreset("Elite");
    player.GetStaminaComponent().GetAerobicStamina() = 1.0;
    player.GetStaminaComponent().GetAnaerobicState().SetEnergy(1.0);
    
    // 开始冲刺
    player.SetMovementState(EMovementState.SPRINT);
    
    float sprintDuration = 0;
    while (player.GetAnaerobicState().GetEnergy() > 0.01)
    {
        player.SimulateTick(0.2);  // 200ms tick
        sprintDuration += 0.2;
        
        if (sprintDuration > 30)  // 安全阀
            break;
    }
    
    // 验收: Elite应在5-15秒耗尽
    EXPECT_GE(sprintDuration, 5.0);
    EXPECT_LE(sprintDuration, 15.0);
}

TEST(RSS_v5_Phase2, CooldownLayered)
{
    // 测试: 分层冷却
    
    PlayerBase player = CreateTestPlayer();
    player.SetPreset("Elite");
    
    // 场景1: 抽干无氧池
    auto anaerobic = player.GetAnaerobicState();
    anaerobic.SetEnergy(0.0);
    anaerobic.OnSprintEnd(true);  // forcedByDepletion
    
    float cooldownFull = anaerobic.GetCooldownRemaining();
    EXPECT_GE(cooldownFull, 120.0);
    EXPECT_LE(cooldownFull, 180.0);
    
    // 场景2: 短冲刺(剩余80%能量)
    anaerobic.SetEnergy(0.8);
    anaerobic.OnSprintEnd(false);
    
    float cooldownShort = anaerobic.GetCooldownRemaining();
    EXPECT_GE(cooldownShort, 60.0);
    EXPECT_LE(cooldownShort, 90.0);
    
    // 验证: 短冲刺冷却 < 抽干冷却
    EXPECT_LT(cooldownShort, cooldownFull * 0.8);
}

TEST(RSS_v5_Phase2, NetworkSync)
{
    // 测试: 联机同步
    
    // 创建服务器端和客户端player
    PlayerBase serverPlayer = CreateServerPlayer();
    PlayerBase clientPlayer = CreateClientPlayer();
    
    // 服务器: 开始冲刺
    serverPlayer.GetAnaerobicState().OnSprintStart(4.0, 35.0);
    
    // 模拟同步延迟(100ms)
    Sleep(0.1);
    SimulateNetworkReplication();
    
    // 验证: 客户端应收到状态
    EXPECT_TRUE(clientPlayer.GetAnaerobicState().IsSprintActive());
    
    // 服务器: 耗尽无氧池
    serverPlayer.GetAnaerobicState().SetEnergy(0.0);
    serverPlayer.GetAnaerobicState().OnSprintEnd(true);
    
    SimulateNetworkReplication();
    
    // 验证: 客户端应收到冷却状态
    EXPECT_FALSE(clientPlayer.GetAnaerobicState().CanSprint());
    EXPECT_GT(clientPlayer.GetAnaerobicState().GetCooldownRemaining(), 0);
}
```

---

## 八、迁移与兼容性方案

### 8.1 Schema版本迁移

```cpp
// SCR_RSS_ConfigMigration.c

class SCR_RSS_ConfigMigration
{
    // 从v4迁移到v5
    static bool MigrateV4ToV5(inout SCR_RSS_Params params)
    {
        Print("[RSS] 开始迁移配置 v4 -> v5", LogLevel.NORMAL);
        
        // 1. 速度参数重标定
        // v4的walk_speed在35kg下通常是2.87 m/s
        // v5降为1.4 m/s (Elite) / 1.7 m/s (Tactical)
        
        // 检测预设类型(通过参数特征推断)
        string presetType = InferPresetType(params);
        
        if (presetType == "Elite")
        {
            params.walk_speed_ideal_35kg = 1.4;
            params.run_speed_ideal_35kg = 2.8;
            params.sprint_speed_ideal_35kg = 4.0;
        }
        else if (presetType == "Tactical")
        {
            params.walk_speed_ideal_35kg = 1.7;
            params.run_speed_ideal_35kg = 3.2;
            params.sprint_speed_ideal_35kg = 4.5;
        }
        else  // Standard或未知
        {
            params.walk_speed_ideal_35kg = 1.5;
            params.run_speed_ideal_35kg = 3.0;
            params.sprint_speed_ideal_35kg = 4.2;
        }
        
        // 2. 无氧系统参数(新增,使用默认值)
        params.anaerobic_drain_coeff_base = 0.08;  // 每秒8%@4m/s
        params.anaerobic_recovery_base_rate = 0.01;  // 每秒1%
        params.burst_cooldown_full_seconds = (presetType == "Elite") ? 180.0 : 120.0;
        params.burst_cooldown_short_seconds = (presetType == "Elite") ? 75.0 : 60.0;
        params.sprint_enable_anaerobic_threshold = 0.20;
        params.sprint_enable_aerobic_min = 0.10;
        
        // 3. 强制降速参数
        params.sustainable_watts_default = 400.0;
        params.forced_slowdown_decay_rate = 0.02;  // 每秒2%降速
        
        // 4. 系数物理化
        // v4的energy_to_stamina_coeff需要根据新锚点反算
        // 暂时保留旧值,Phase 3优化器会重新计算
        
        // 5. Legacy模式开关(默认关闭,使用v5特性)
        params.legacy_v4_single_pool_mode = false;
        params.legacy_v4_disable_forced_slow = false;
        params.legacy_v4_use_old_drain_velocity = false;
        
        // 6. 更新schema版本号
        params.schema_version = 5;
        
        Print("[RSS] 配置迁移完成", LogLevel.NORMAL);
        return true;
    }
    
    // 推断v4预设类型
    static string InferPresetType(SCR_RSS_Params params)
    {
        // 通过特征参数推断(示例)
        // Elite: sprint_enable_threshold ~0.21, walk_recovery_rate高
        // Tactical: sprint_enable_threshold ~0.15, 恢复快
        
        if (params.sprint_enable_threshold > 0.20)
            return "Elite";
        else if (params.sprint_enable_threshold < 0.16)
            return "Tactical";
        else
            return "Standard";
    }
}
```

### 8.2 首次启动提示

```cpp
// UI/SCR_RSS_V5MigrationDialog.c

class SCR_RSS_V5MigrationDialog : SCR_ConfiguratorDialog
{
    override void OnMenuOpen()
    {
        // 检查是否首次启动v5
        int lastSchemaVersion = GetLastKnownSchemaVersion();
        
        if (lastSchemaVersion < 5)
        {
            ShowMigrationDialog();
        }
    }
    
    void ShowMigrationDialog()
    {
        SCR_MessageBox.CreateYesNo(
            title: "#RSS_V5_MIGRATION_TITLE",  // "体力系统已重构"
            message: "#RSS_V5_MIGRATION_MESSAGE",  // 见下方文本
            callback: OnUserChoice
        );
    }
    
    void OnUserChoice(EMessageBoxButton button)
    {
        if (button == EMessageBoxButton.YES)
        {
            // 打开预设选择界面
            GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.RSS_PresetSelector);
        }
        else
        {
            // 使用默认迁移参数
            ApplyDefaultMigratedPreset();
        }
        
        // 记录已显示迁移提示
        SetLastKnownSchemaVersion(5);
    }
}
```

```xml
<!-- 迁移提示文本 -->
<Key ID="RSS_V5_MIGRATION_TITLE">
    <Original>体力系统已重构 (v5)</Original>
    <English>Stamina System Reworked (v5)</English>
</Key>

<Key ID="RSS_V5_MIGRATION_MESSAGE">
    <Original>RSS v5引入了重大改进:

• 双池系统: 有氧池(行军)与无氧池(冲刺)分离
• 负重行军速度调整: 更贴近现实(1.4-1.7 m/s)
• 冲刺冷却机制: 防止无限制冲刺
• 强制降速: 超载时自动减速保护

建议您重新选择预设以获得最佳体验。

是否现在选择预设?</Original>
    
    <English>RSS v5 introduces major improvements:

• Dual pool system: Aerobic (marching) & Anaerobic (sprinting) separated
• Load marching speed adjusted: More realistic (1.4-1.7 m/s)
• Sprint cooldown mechanics: Prevents unlimited sprinting
• Forced slowdown: Auto-slowdown when overloaded

We recommend re-selecting your preset for best experience.

Choose preset now?</English>
</Key>
```

### 8.3 Legacy v4模式

```cpp
// SCR_RSS_LegacyModeController.c

class SCR_RSS_LegacyModeController
{
    // 从UI配置面板设置
    static void EnableLegacyV4Mode(SCR_RSS_Params params, bool enable)
    {
        if (enable)
        {
            Print("[RSS] 启用Legacy v4兼容模式", LogLevel.WARNING);
            
            // 开关1: 单池模式
            params.legacy_v4_single_pool_mode = true;
            
            // 开关2: 禁用强制降速
            params.legacy_v4_disable_forced_slow = true;
            
            // 开关3: 使用旧消耗速度
            params.legacy_v4_use_old_drain_velocity = true;
            
            // 速度恢复为v4数值(近似)
            params.walk_speed_ideal_35kg = 2.87;
            params.run_speed_ideal_35kg = 3.59;
            params.sprint_speed_ideal_35kg = 4.14;
            
            // 系数恢复为v4 Tactical近似值
            params.energy_to_stamina_coeff = 9.4e-7;
            params.load_metabolic_dampening = 0.70;
            
            // UI提示
            ShowLegacyModeWarning();
        }
        else
        {
            // 恢复v5默认值
            params.legacy_v4_single_pool_mode = false;
            params.legacy_v4_disable_forced_slow = false;
            params.legacy_v4_use_old_drain_velocity = false;
            
            // 其他参数保持用户选择的预设
        }
    }
    
    static void ShowLegacyModeWarning()
    {
        SCR_HintManagerComponent.GetInstance().ShowCustomHint(
            "#RSS_LEGACY_MODE_WARNING",  // "已启用Legacy v4模式 - 不建议长期使用"
            duration: 10.0,
            category: EHint.WARNING
        );
    }
}
```

---

## 九、风险缓解措施

### 9.1 技术风险缓解

| 风险 | 缓解措施 | 负责模块 | 验证方法 |
|------|---------|---------|---------|
| **单float存储冲突** | Phase 0前确定无氧池存储位置(新组件) | 3.2.1 | 代码审查 |
| **网络不同步** | 混合模式+心跳同步+10byte包 | 3.2 | Phase 2联机测试 |
| **GetVelocity时序** | 降速→读速度→算消耗 固定顺序 | 4.1.2 | 单元测试 |
| **灌木双重惩罚** | 仅下调RSS token,桥接min逻辑 | 4.1.2 | 灌木内行军bench |
| **优化器无解** | 硬约束分级;Phase 1手工标定coeff | 10.3 | Optuna试运行 |
| **游泳/载具崩溃** | 共用有氧池;无氧冻结;smoke测试 | 4.x扩展 | 回归测试 |
| **AI无限冲刺** | 简化计数器+行为树gate | 6.1-6.2 | AI战斗测试 |

### 9.2 体验风险缓解

| 风险 | 缓解措施 | 何时部署 | 成功指标 |
|------|---------|---------|---------|
| **1.4 m/s过慢** | 档位差异(1.4/1.5/1.7);强行军姿态 | Phase 0实机测试后调整 | >70%测试者接受 |
| **180s冷却过长** | 分层冷却(短冲60-90s);Tactical档更短 | Phase 2设计内置 | 短冲刺使用率>50% |
| **降速像bug** | 渐进降速+UI警告+音效+硬撑选项 | Phase 1 UI同步上线 | <10%玩家报告"bug" |
| **双池UI复杂** | 单主条+冲刺CD环;拒绝RPG双条 | Phase 2 | 无"看不懂"反馈 |
| **老玩家抵触** | Legacy模式+首次弹窗+社区说明 | v5.0发布日 | 社区负面<30% |

### 9.3 项目风险缓解

| 风险 | 缓解措施 | 检查点 |
|------|---------|--------|
| **Phase间依赖阻塞** | Phase 1可独立发布(无双池);Phase 2不依赖Phase 3 | 每Phase结束 |
| **孪生vs实机偏离** | Phase 0/3强制实机门禁;不允许纯孪生调参 | Phase 0, Phase 3 |
| **优化器时间爆炸** | 硬约束先验证可行域;100次试验无改善则终止 | Phase 3第1周 |
| **一次改太多回滚难** | 分步发布: 3.23.x稳定线 + dev/v5实验分支 | 全程 |
| **联机Custom参数冲突** | Schema版本检测;服务器强制同步;客户端警告 | Phase 2联机测试 |

### 9.4 应急回滚方案

```cpp
// tools/emergency_rollback_v5.py

"""
v5紧急回滚脚本
用途: 若v5上线后出现严重问题,快速回退至v4稳定版
"""

def rollback_to_v4():
    print("[紧急回滚] 开始回退RSS v5 -> v4")
    
    # 1. Git回退
    os.system("git checkout main")  # 切换到3.23.x稳定分支
    os.system("git pull origin main")
    
    # 2. 清理v5临时文件
    v5_files = [
        "scripts/Game/RSS/Core/SCR_AnaerobicBurstState.c",
        "scripts/Game/RSS/Core/SCR_MetabolicSpeedLimiter.c",
        # ...其他v5特有文件
    ]
    for f in v5_files:
        if os.path.exists(f):
            os.remove(f)
            print(f"[回滚] 删除 {f}")
    
    # 3. 恢复v4预设
    restore_v4_presets()
    
    # 4. 重新编译
    os.system("WorkbenchCLI.exe -rebuild")
    
    # 5. 通知用户
    print("[回滚完成] 已恢复至v4稳定版 (3.23.x)")
    print("请在Workshop发布回滚公告")

if __name__ == "__main__":
    confirm = input("确认紧急回滚至v4? (yes/NO): ")
    if confirm.lower() == "yes":
        rollback_to_v4()
```

---

## 十、开发排期与里程碑

### 10.1 Phase 0: 基线与工具 (2周)

| 周 | 任务 | 交付物 | 责任人 |
|----|------|--------|--------|
| W1 | 生理锚点bench脚本 | `bench_physio_anchors.py` 全pass | 数值策划 |
| W1 | 孪生stub扩展 | `rss_digital_twin_fix.py` 支持双池 | 工具开发 |
| W1 | 文档更新 | 本文档+计算逻辑文档链接 | 文档管理 |
| W2 | 实机体感测试 | 体感清单完成+参数建议 | QA+设计 |
| W2 | Schema版本系统 | `m_sStaminaSchemaVersion` 机制 | 引擎层 |

**里程碑 M0**: Phase 0验收会议,决定是否进入Phase 1

### 10.2 Phase 1: 速度-消耗闭环 (3周)

| 周 | 任务 | 交付物 | 依赖 |
|----|------|--------|------|
| W3 | v_drain闭环实现 | `GetDrainVelocity()` 修正 | - |
| W3 | 强制降速框架 | `SCR_MetabolicSpeedLimiter` | - |
| W4 | 速度重标定 | 35kg Walk 1.4m/s | W3 |
| W4 | 灌木叠加验证 | 灌木bench通过 | 3.23.1基线 |
| W5 | UI反馈集成 | 降速警告+音效 | W3,W4 |
| W5 | Phase 1 bench | v5_phase1测试全pass | W3,W4 |

**里程碑 M1**: Phase 1可独立发布为3.24 beta(可选)

### 10.3 Phase 2: 无氧池与冲刺冷却 (3周)

| 周 | 任务 | 交付物 | 依赖 |
|----|------|--------|------|
| W6 | 无氧组件 | `SCR_AnaerobicBurstState` | M1 |
| W6 | 网络同步 | RplProp+RPC+混合模式 | W6组件 |
| W7 | 分层冷却逻辑 | 抽干180s/短冲60-90s | W6 |
| W7 | Sprint速度衰减 | 无氧池<20%降速 | W6 |
| W8 | UI冲刺CD环 | `SCR_SprintCooldownWidget` | W7 |
| W8 | 联机测试 | 2人联机bench通过 | W6-W7 |

**里程碑 M2**: 双池系统完整,进入内部测试

### 10.4 Phase 3: coeff物理化与优化 (2周)

| 周 | 任务 | 交付物 | 依赖 |
|----|------|--------|------|
| W9 | 物理化coeff | `sustainable_watts` 反算 | M2 |
| W9 | 优化器v5 | `rss_pipeline_v5.py` | 孪生完整 |
| W10 | 优化器运行 | 三档预设生成 | W9 |
| W10 | 实机验证 | 优化后预设实机测试 | W10 |

**里程碑 M3**: 参数优化完成,准备发布

### 10.5 Phase 4: AI/文档/发布 (2周)

| 周 | 任务 | 交付物 | 依赖 |
|----|------|--------|------|
| W11 | AI简化模型 | `SCR_RSS_AIStaminaState` | M2 |
| W11 | 行为树集成 | AI冲刺gate | W11 |
| W11 | 文档完善 | 所有文档更新 | - |
| W12 | 迁移系统 | v4->v5迁移+弹窗 | M3 |
| W12 | Legacy模式 | `LegacyV4Style` 预设 | M3 |
| W12 | 发布准备 | Workshop页面+社区说明 | 全部 |

**里程碑 M4**: RSS v5.0 正式发布

### 10.6 甘特图(简化)

```
Phase 0 (W1-W2):  [████████]
                          |
Phase 1 (W3-W5):          [████████████]
                                      |
Phase 2 (W6-W8):                      [████████████]
                                                  |
Phase 3 (W9-W10):                                 [████████]
                                                          |
Phase 4 (W11-W12):                                        [████████]
                                                                  |
                                                                 v5.0
```

### 10.7 关键路径

```
[Phase 0 bench] → [Phase 1 v_drain] → [Phase 2 双池] → [Phase 3 优化] → [Phase 4 发布]
      ↓必须通过         ↓可独立发布       ↓核心功能       ↓参数调优      ↓必须完成
```

---

## 十一、附录

### A. 参考资料

1. **原始计划**: `docs/RSS_v5_体力系统修改计划.md` v0.3
2. **Pandolf模型**: 军事负重行军代谢功率计算
3. **v4数字孪生**: `tools/rss_digital_twin_fix.py`
4. **灌木机制**: `docs/灌木丛移动减速机制.md`
5. **v4预设**: commit b2e1cd9

### B. 术语表

| 术语 | 定义 |
|------|------|
| **有氧池 (Aerobic)** | 长时间活动的耐力池,对应主体力条 |
| **无氧池 (Anaerobic)** | 短时爆发的能量池,控制冲刺资格 |
| **强制降速** | 代谢功率超过可持续阈值时自动降低限速 |
| **分层冷却** | 根据冲刺时长决定冷却时间(抽干vs短冲) |
| **v_drain** | 用于计算消耗的速度(区别于显示速度) |
| **sustainable_watts** | 可持续代谢功率阈值(默认400W) |
| **硬撑 (Override)** | 玩家主动选择维持高速,付出额外体力代价 |
| **Legacy v4模式** | 兼容模式,禁用v5特性,近似v4行为 |

### C. 代码清单

```
新增文件:
+ scripts/Game/RSS/Core/SCR_AnaerobicBurstState.c
+ scripts/Game/RSS/Core/SCR_MetabolicSpeedLimiter.c
+ scripts/Game/RSS/AI/SCR_RSS_AIStaminaState.c
+ scripts/Game/RSS/Migration/SCR_RSS_ConfigMigration.c
+ UI/SCR_SprintCooldownWidget.c
+ UI/SCR_MetabolicWarningWidget.c
+ tools/bench_physio_anchors.py
+ tools/rss_pipeline_v5.py
+ tests/integration/test_v5_phase1.cpp
+ tests/integration/test_v5_phase2.cpp
+ docs/RSS_v5_完整实施方案.md (本文档)

修改文件:
~ scripts/Game/Integration/PlayerBase.c
~ scripts/Game/RSS/Core/SCR_StaminaUpdateCoordinator.c
~ scripts/Game/RSS/Core/SCR_SpeedCalculation.c
~ scripts/Game/RSS/Core/SCR_StaminaConstants.c
~ scripts/Game/RSS/Core/SCR_CharacterStaminaComponent.c
~ scripts/Game/RSS/NetworkConfig/SCR_RSS_Settings.c
~ scripts/Game/RSS/NetworkConfig/SCR_RSS_Params.c
~ tools/rss_digital_twin_fix.py
~ tools/embed_json_to_c.py
```

### D. 联系与反馈

- **项目主页**: [Workshop链接]
- **GitHub Issues**: [GitHub仓库]
- **Discord讨论**: [Discord服务器]
- **设计文档**: `docs/` 文件夹

---

## 变更日志

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| v1.0 | 2026-06-02 | 初始版本,完整实施方案 |

---

**本方案状态**: ✅ 待评审

**下一步行动**: 
1. 召开设计评审会议
2. Phase 0 资源分配
3. 创建 `dev/v5` 分支

---

*本文档为RSS v5体力系统修改计划的补充实施方案,提供详细的技术实现、测试标准、风险缓解措施。与原计划配合使用。*
