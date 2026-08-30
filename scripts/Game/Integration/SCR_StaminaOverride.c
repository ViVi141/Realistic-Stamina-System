// 体力系统完全覆盖
// 完全禁用游戏原生的体力恢复和消耗计算，只使用自定义系统
modded class SCR_CharacterStaminaComponent : CharacterStaminaComponent
{
    // 上次的体力值（用于检测非预期的体力变化）
    protected float m_fLastKnownStamina = 1.0;

    // 标记：是否允许原生体力系统工作
    // false = 完全禁用原生系统，只使用自定义系统
    protected bool m_bAllowNativeStaminaSystem = false;

    // 目标体力值（由我们的自定义系统控制）
    protected float m_fTargetStamina = 1.0;

    // 主动监控标志（控制是否继续监控）
    protected bool m_bIsMonitoring = false;

    // 监控间隔（毫秒）。16=60Hz 与服务器同步，50/100 可降低 CPU 占用
    // DESIGN: 200ms matches UpdateSpeedBasedOnStamina main loop interval.
    // 50ms provided no measurable benefit because SetTargetStamina already
    // double-checks and corrects on every write. 200ms is sufficient to catch
    // engine-origin stamina changes that bypass AddStamina().
    protected const int STAMINA_MONITOR_INTERVAL_MS = 200;

    // 标记：是否是我们自己的调用（避免循环）
    protected bool m_bIsOurOwnCall = false;

    //! >= 0 时引擎条应显示此值（冲刺门禁 / W′ 晃动呼吸），m_fTargetStamina 仍是有氧权威
    protected float m_fTransientEngineDisplay = -1.0;
    //! 引擎表现读数平滑（防 SetTarget/帧间隙一闪有一无）
    protected float m_fSmoothedEngineDisplay = 1.0;
    protected const float ENGINE_FX_SMOOTH_ALPHA = 0.45;

    // 关键发现：
    // 1. OnStaminaDrain 是一个 event，每次体力值改变时都会触发（包括 AddStamina 调用）
    // 2. AddStamina 是 proto external，无法覆盖，但调用它会触发 OnStaminaDrain 事件
    // 3. 因此，我们可以通过覆盖 OnStaminaDrain 来拦截所有体力值变化

    // 每帧/每次体力变化时，拦截并重新设置体力值
    // 这确保只有我们的自定义系统可以改变体力值
    // 注意：原生系统调用 AddStamina 时会触发此事件，我们可以在这里拦截
    override void OnStaminaDrain(float pDrain)
    {
        // 如果允许原生系统工作，调用父类方法
        if (m_bAllowNativeStaminaSystem)
        {
            super.OnStaminaDrain(pDrain);
            return;
        }

        // 如果是我们自己的调用（通过 SetTargetStamina），允许执行
        if (m_bIsOurOwnCall)
        {
            super.OnStaminaDrain(pDrain);
            return;
        }

        // 完全禁用原生系统：不调用 super，立即纠正非预期变化
        CorrectStaminaToTarget();
    }

    protected float GetExpectedEngineStamina()
    {
        // 平滑后的引擎读数；Monitor / SetTarget 都对齐此值，避免隔帧跳回有氧满值
        return m_fSmoothedEngineDisplay;
    }

    protected void ClampTransientToAerobic()
    {
        if (m_fTransientEngineDisplay < 0.0)
            return;
        if (m_fTransientEngineDisplay > m_fTargetStamina)
            m_fTransientEngineDisplay = m_fTargetStamina;
    }

    protected void RefreshSmoothedEngineDisplay(bool snap)
    {
        float desired = m_fTargetStamina;
        if (m_fTransientEngineDisplay >= 0.0)
            desired = m_fTransientEngineDisplay;

        if (snap)
        {
            m_fSmoothedEngineDisplay = desired;
            return;
        }

        m_fSmoothedEngineDisplay = m_fSmoothedEngineDisplay
            + (desired - m_fSmoothedEngineDisplay) * ENGINE_FX_SMOOTH_ALPHA;
    }

    protected void WriteEngineStaminaToExpected()
    {
        float currentStamina = GetStamina();
        if (currentStamina < 0.0)
            return;

        float expectedStamina = GetExpectedEngineStamina();
        float correction = expectedStamina - currentStamina;
        if (Math.AbsFloat(correction) < 0.0001)
            return;

        m_bIsOurOwnCall = true;
        super.AddStamina(correction);
        m_bIsOurOwnCall = false;

        float finalStamina = GetStamina();
        if (finalStamina >= 0.0)
            m_fLastKnownStamina = finalStamina;
    }

    // 纠正引擎条到期望表现读数（有氧权威或 transient 平滑值）
    void CorrectStaminaToTarget()
    {
        ClampTransientToAerobic();
        RefreshSmoothedEngineDisplay(false);
        WriteEngineStaminaToExpected();
    }

    // 启动主动监控（每帧检查，确保完全覆盖原生系统）
    // 原生系统可能每帧都在恢复体力，所以需要每帧检查
    void StartStaminaMonitor()
    {
        // 如果已经在监控，不重复启动
        if (m_bIsMonitoring)
            return;

        // 设置监控标志
        m_bIsMonitoring = true;

        // 启动监控循环（检查原生系统干扰并纠正）
        // STAMINA_MONITOR_INTERVAL_MS：200ms 与主力循环同步
        // CRITICAL FIX: GetGame() may be null during late initialization / teardown.
        if (GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(MonitorStamina, STAMINA_MONITOR_INTERVAL_MS, false);
    }

    // 停止主动监控
    void StopStaminaMonitor()
    {
        // 设置标志为 false，停止监控循环
        m_bIsMonitoring = false;

        // m_bIsMonitoring=false 后 pending MonitorStamina 入口即 return 且不再重调度
        // 不使用 Remove(MonitorStamina)：全局 Remove 会取消所有实体的监控回调
    }

    // 监控体力值（定期调用，确保完全覆盖原生系统）
    // 原生系统可能每帧都在恢复体力，所以需要频繁检查
    void MonitorStamina()
    {
        // 如果允许原生系统工作，或监控已停止，不继续监控
        if (m_bAllowNativeStaminaSystem || !m_bIsMonitoring)
            return;

        // 获取当前体力值前，先检查组件是否仍然有效
        if (!this || GetOwner() == null)
        {
            // 如果组件已失效，停止监控
            StopStaminaMonitor();
            return;
        }

        // 获取当前体力值
        float currentStamina = GetStamina();

        float expectedStamina = GetExpectedEngineStamina();

        // 如果发现非预期的体力变化（原生系统试图改变体力），立即恢复到目标值
        // 提高精度到0.0001，确保与60Hz服务器同步
        if (currentStamina >= 0.0 && Math.AbsFloat(currentStamina - expectedStamina) > 0.0001)
        {
            // 检测到原生系统干扰，记录调试信息
            float deviation = currentStamina - m_fTargetStamina;

            // 如果偏差超过0.5%，输出警告（降低阈值，更敏感，仅在客户端）
            // [已注释] 拦截信息已禁用，减少日志输出
            /*
            IEntity owner = GetOwner();
            if (owner && owner == SCR_PlayerController.GetLocalControlledEntity())
            {
                static int warningCounter = 0;
                warningCounter++;
                if (Math.AbsFloat(deviation) > 0.005 && warningCounter >= 10) // 每10次输出一次
                {
                    PrintFormat("[RSS] Override 检测到原生系统干扰！当前体力=%1%%，目标=%2%%，偏差=%3%%",
                        Math.Round(currentStamina * 100.0).ToString(),
                        Math.Round(m_fTargetStamina * 100.0).ToString(),
                        Math.Round(deviation * 100.0).ToString());
                    warningCounter = 0;
                }
            }
            */

            // 立即纠正体力值
            CorrectStaminaToTarget();
        }

        if (GetGame() && GetGame().GetCallqueue())
            GetGame().GetCallqueue().CallLater(MonitorStamina, STAMINA_MONITOR_INTERVAL_MS, false);
    }

    // 设置有氧权威；不拆除 W′/冲刺门禁 transient，避免中间一帧弹回满体力
    void SetTargetStamina(float targetStamina)
    {
        m_fTargetStamina = Math.Clamp(targetStamina, 0.0, 1.0);
        ClampTransientToAerobic();
        // 权威刚写入时对齐期望读数；若仍有 transient 则保持压条，不闪高
        RefreshSmoothedEngineDisplay(false);
        WriteEngineStaminaToExpected();
    }

    // 获取目标体力值
    float GetTargetStamina()
    {
        return m_fTargetStamina;
    }

    //! 仅改引擎 GetStamina() 显示/原生门禁/晃动读数，不修改 m_fTargetStamina
    void ApplyTransientEngineStamina(float engineValue)
    {
        engineValue = Math.Clamp(engineValue, 0.0, 1.0);
        if (engineValue > m_fTargetStamina)
            engineValue = m_fTargetStamina;
        m_fTransientEngineDisplay = engineValue;
        RefreshSmoothedEngineDisplay(false);
        WriteEngineStaminaToExpected();
    }

    //! 清除表现伪装，引擎条回到有氧权威（可平滑收回）
    void RestoreEngineStaminaFromTarget()
    {
        m_fTransientEngineDisplay = -1.0;
        RefreshSmoothedEngineDisplay(false);
        WriteEngineStaminaToExpected();
    }

    //! 强制引擎条立即等于有氧权威（初始化 / 重置）
    void SnapEngineStaminaToTarget()
    {
        m_fTransientEngineDisplay = -1.0;
        RefreshSmoothedEngineDisplay(true);
        WriteEngineStaminaToExpected();
    }

    // 允许/禁用原生体力系统
    // false = 完全禁用原生系统（推荐）
    // true = 允许原生系统工作（不推荐）
    void SetAllowNativeStaminaSystem(bool allow)
    {
        m_bAllowNativeStaminaSystem = allow;

        // 根据设置启动或停止监控
        if (allow)
        {
            StopStaminaMonitor();
        }
        else
        {
            // CRITICAL FIX: Only start the 50ms monitor loop for player-controlled
            // entities. AI stamina is managed by the server's UpdateSpeedBasedOnStamina,
            // and the OnStaminaDrain override plus CorrectStaminaToTarget in
            // SetTargetStamina already provide sufficient protection without a
            // per-entity 50ms polling loop. This eliminates ~1280 calls/sec for 64 AI.
            IEntity owner = GetOwner();
            if (owner)
            {
                SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
                if (ctrl && ctrl.IsPlayerControlled())
                    StartStaminaMonitor();
            }
        }
    }

    //! 析构函数：实体删除时取消 MonitorStamina 的 CallLater，防止 use-after-free
    void ~SCR_CharacterStaminaComponent()
    {
        StopStaminaMonitor();
    }

    // 组件初始化时启动监控
    // 注意：使用 OnInit 而不是 OnPostInit（如果基类支持）
    // 如果基类不支持 OnInit，监控将在 SetAllowNativeStaminaSystem(false) 时启动

    // 获取是否允许原生体力系统
    bool GetAllowNativeStaminaSystem()
    {
        return m_bAllowNativeStaminaSystem;
    }
}
