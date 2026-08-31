//! 专服 W′ 补 tick（历史：PrepareControls 粗估）。
//! 已停用：粗估功率会把 W′ 灌回 100% 并 Rpl 盖掉客户端权威；
//! 专服玩家 W′ 改由客户端 Phase B（SCR_PlayerBaseWPrimeTickHelper）tick。

class SCR_RSS_WPrimeServerTick
{
    //! 恒返回 false（保留符号以免旧调用链编译失败）
    static bool MaybeTick(
        bool isServer,
        bool isPlayerControlled,
        bool shouldProcessStaminaUpdate,
        SCR_RSS_AnaerobicBurst anaerobicBurst,
        SCR_RSS_StaminaState staminaState,
        bool isSprintActive,
        float aerobicPercent,
        float intervalSec,
        inout float lastStaminaUpdateTime,
        out float replPool,
        out float replCooldownUntil)
    {
        replPool = 1.0;
        replCooldownUntil = -1.0;
        return false;
    }
}
