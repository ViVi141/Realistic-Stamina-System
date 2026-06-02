// RSS 统一日志门面
// 目标：收敛全项目 Print/PrintFormat 的直接使用，统一日志开关策略。

class SCR_RSS_Logger
{
    static void Debug(string message)
    {
        if (!StaminaConfigBridge.IsDebugEnabled())
            return;

        Print(message);
    }

    static void Info(string message)
    {
        Print(message);
    }

    static void Warn(string message)
    {
        Print("[RSS][WARN] " + message);
    }

    static void Error(string message)
    {
        Print("[RSS][ERROR] " + message);
    }
}
