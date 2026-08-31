//! 专服 / 进退服窗口：GetGame、World、Callqueue 可能已空，解引会 Access violation。
//! 启发式空引用：脚本侧非空句柄仍可能悬空，多层交叉校验后仍不碰高危 native。
class SCR_RSS_RuntimeGuard
{
    //! World 为 sealed 原生类型，不能作 out 参数（编译器报 Class/World unrelated）。
    static World GetWorldOrNull()
    {
        if (!GetGame())
            return null;
        return GetGame().GetWorld();
    }

    static bool TryGetWorldTimeMs(out float timeMs)
    {
        timeMs = 0.0;
        World world = GetWorldOrNull();
        if (!world)
            return false;
        timeMs = world.GetWorldTime();
        return true;
    }

    static bool TryGetWorldTimeSec(out float timeSec)
    {
        timeSec = 0.0;
        float timeMs = 0.0;
        if (!TryGetWorldTimeMs(timeMs))
            return false;
        timeSec = timeMs / 1000.0;
        return true;
    }

    static ScriptCallQueue GetCallqueueOrNull()
    {
        if (!GetGame())
            return null;
        return GetGame().GetCallqueue();
    }

    //! 实体是否挂在当前游戏世界上（启发式：Game/World/Owner 交叉校验）。
    //! @param owner 角色或道具实体
    //! @return false 时禁止解引该实体上的 Physics / 控制器运动 native
    static bool IsEntityWorldUsable(IEntity owner)
    {
        if (!owner)
            return false;
        if (!GetGame())
            return false;
        World gameWorld = GetGame().GetWorld();
        if (!gameWorld)
            return false;
        World ownerWorld = owner.GetWorld();
        if (!ownerWorld)
            return false;
        if (ownerWorld != gameWorld)
            return false;
        return true;
    }

    //! Physics 句柄是否「看起来」可用：二次 GetPhysics 交叉校验。
    //! 注意：返回 true 仍不保证 GetVelocity 安全（进服窗口已证实非空句柄 AV）。
    //! @param owner 角色实体
    //! @return true 仅表示脚本侧两次取到同一非空 Physics
    static bool IsPhysicsHandlePresent(IEntity owner)
    {
        if (!IsEntityWorldUsable(owner))
            return false;
        Physics first = owner.GetPhysics();
        if (!first)
            return false;
        Physics second = owner.GetPhysics();
        if (!second)
            return false;
        if (first != second)
            return false;
        return true;
    }
}
