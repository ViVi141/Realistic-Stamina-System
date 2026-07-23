// 统一调试批次管理器
// 从 SCR_StaminaConstants.c 拆分出的调试批次系统
// 提供批次化的调试输出：每秒一波次，帧末统一刷新
//
// 拆分原因：SCR_StaminaConstants.c 超过 EnforceScript 65535 字节编译限制
// 日期：2026-05-08

class SCR_RSS_DebugBatchManager
{
    protected static float s_fNextDebugBatchTime = 0.0;
    protected static bool s_bDebugBatchActive = false;
    protected static ref array<string> s_aDebugBatchLines = null;

    // 启动本秒的调试批次，返回是否应输出（每秒一次）
    // Debug 日志或 HUD Hint 任一开启时均可启动（Hint 用于 ETA/消耗诊断行）
    static bool StartDebugBatch()
    {
        bool wantBatch = SCR_RSS_ConfigBridge.IsDebugEnabled();
        if (!wantBatch)
        {
            SCR_RSS_Settings hintSettings = SCR_RSS_ConfigManager.GetSettings();
            if (hintSettings && hintSettings.m_bHintDisplayEnabled)
                wantBatch = true;
        }
        if (!wantBatch)
            return false;
        World world = GetGame().GetWorld();
        if (!world)
            return false;
        float t = world.GetWorldTime() / 1000.0;
        float interval = SCR_RSS_ConfigBridge.GetDebugUpdateInterval() / 1000.0;
        if (interval <= 0.0)
            interval = 1.0;
        if (t < s_fNextDebugBatchTime)
            return false;
        s_fNextDebugBatchTime = t + interval;
        s_bDebugBatchActive = true;
        s_bTempStepAddedThisBatch = false;
        s_bEngineTODAddedThisBatch = false;
        if (!s_aDebugBatchLines)
            s_aDebugBatchLines = new array<string>();
        s_aDebugBatchLines.Clear();
        return true;
    }

    // 添加一行到当前批次（需先调用 StartDebugBatch 启动批次）
    static void AddDebugBatchLine(string line)
    {
        if (!s_bDebugBatchActive || !s_aDebugBatchLines)
            return;
        s_aDebugBatchLines.Insert(line);
    }

    // 每批次仅添加一次（用于 TempStep、EngineTOD 等可能被多次调用的输出）
    protected static bool s_bTempStepAddedThisBatch = false;
    protected static bool s_bEngineTODAddedThisBatch = false;

    static void AddDebugBatchLineOnce(string tag, string line)
    {
        if (!s_bDebugBatchActive || !s_aDebugBatchLines)
            return;
        if (tag == "TempStep" && s_bTempStepAddedThisBatch)
            return;
        if (tag == "EngineTOD" && s_bEngineTODAddedThisBatch)
            return;
        s_aDebugBatchLines.Insert(line);
        if (tag == "TempStep")
            s_bTempStepAddedThisBatch = true;
        if (tag == "EngineTOD")
            s_bEngineTODAddedThisBatch = true;
    }

    // 当前是否处于调试批次窗口内
    static bool IsDebugBatchActive()
    {
        return s_bDebugBatchActive;
    }

    protected static float s_fLastBatchFlushTime = -999.0;

    // 在帧末刷新批次：输出所有累积行并清空
    static void FlushDebugBatch()
    {
        if (!s_bDebugBatchActive)
            return;
        s_bDebugBatchActive = false;
        if (!s_aDebugBatchLines || s_aDebugBatchLines.Count() == 0)
            return;
        World world = GetGame().GetWorld();
        if (world)
            s_fLastBatchFlushTime = world.GetWorldTime() / 1000.0;
        for (int i = 0; i < s_aDebugBatchLines.Count(); i++)
            Print(s_aDebugBatchLines.Get(i));
        s_aDebugBatchLines.Clear();
    }

    // 本轮秒内是否刚刷新过批次（用于避免 status 重复）
    static bool WasBatchJustFlushed()
    {
        World world = GetGame().GetWorld();
        if (!world)
            return false;
        float t = world.GetWorldTime() / 1000.0;
        return (t - s_fLastBatchFlushTime) < 0.5;
    }

    // 统一调试日志节流（基于 DebugUpdateInterval）
    static bool ShouldLog(inout float nextTime)
    {
        if (!SCR_RSS_ConfigBridge.IsDebugEnabled())
            return false;

        return ShouldLogInternal(nextTime);
    }

    // 统一详细日志节流（需要 Debug + Verbose）
    static bool ShouldVerboseLog(inout float nextTime)
    {
        if (!SCR_RSS_ConfigBridge.IsDebugEnabled() || !SCR_RSS_ConfigBridge.IsVerboseLoggingEnabled())
            return false;

        return ShouldLogInternal(nextTime);
    }

    // 内部时间节流实现
    protected static bool ShouldLogInternal(inout float nextTime)
    {
        World world = GetGame().GetWorld();
        if (!world)
            return false;

        float currentTime = world.GetWorldTime() / 1000.0;
        float interval = SCR_RSS_ConfigBridge.GetDebugUpdateInterval() / 1000.0;
        if (interval <= 0.0)
            interval = 5.0;

        if (currentTime < nextTime)
            return false;

        nextTime = currentTime + interval;
        return true;
    }

    //! 新世界开始时调用，重置所有静态时间戳，避免 Workbench 重载世界后
    //! 前几秒无调试输出的问题。
    static void ResetForNewWorld()
    {
        s_fNextDebugBatchTime = 0.0;
        s_bDebugBatchActive = false;
        s_bTempStepAddedThisBatch = false;
        s_bEngineTODAddedThisBatch = false;
        s_fLastBatchFlushTime = -999.0;
    }
}
