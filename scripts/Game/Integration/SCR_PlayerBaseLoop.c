//! CallLater 桥接：EnforceScript 要求回调与 modded 方法在同一编译单元可见时，
//! 通过静态方法 + 控制器引用调度 PlayerBase_UpdateLoop.c 中的 tick 逻辑。

class SCR_PlayerBaseLoop
{
    static void Tick(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.UpdateSpeedBasedOnStamina();
    }

    static void DelayedStart(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.RSS_LoopStartSystem();
    }

    static void DelayedEnsureClient(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.EnsureRssStaminaLoopIfNeeded();
    }

    static void DelayedEnsureAiServer(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.EnsureAiStaminaLoopOnServer();
    }

    static void EnsureClientLoop(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.EnsureRssStaminaLoopIfNeeded();
    }

    static void CollectSpeedSampleBridge(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return;
        ctrl.CollectSpeedSample();
    }
}
