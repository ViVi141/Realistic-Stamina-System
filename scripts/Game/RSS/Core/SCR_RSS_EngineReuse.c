//! 官方引擎已算状态复用（优先读缓存，失败再回退 RSS 自算/Trace）。
//!
//! 优先级约定：
//!   坡度：CommandMove.GetMovementSlopeAngle → FloorNormal → Trace
//!   速度（陆地）：仅 CharacterMovement.GetVelocityWS（禁止位置差分 / Physics.GetVelocity）
//!   速度（游泳）：可由调用方另开位置差分
//!   地形：Movement.GetFloorSurface → Trace 材质表

class SCR_RSS_EngineReuse
{
    //------------------------------------------------------------------------------------------------
    //! CharacterMovementComponent（已缓存优先）
    static CharacterMovementComponent ResolveMovement(IEntity owner)
    {
        if (!owner)
            return null;
        CharacterEntity charEnt = CharacterEntity.Cast(owner);
        if (charEnt)
        {
            CharacterMovementComponent fromEnt = charEnt.GetMovementComponent();
            if (fromEnt)
                return fromEnt;
        }
        return CharacterMovementComponent.Cast(
            owner.FindComponent(CharacterMovementComponent));
    }

    //------------------------------------------------------------------------------------------------
    //! CharacterCommandMove（可能为 null：非 Move 命令时）
    static CharacterCommandMove ResolveCommandMove(SCR_CharacterControllerComponent ctrl)
    {
        if (!ctrl)
            return null;
        CharacterAnimationComponent anim = ctrl.GetAnimationComponent();
        if (!anim)
            return null;
        CharacterCommandHandlerComponent handler = anim.GetCommandHandler();
        if (!handler)
            return null;
        return handler.GetCommandMove();
    }

    //------------------------------------------------------------------------------------------------
    //! 引擎移动坡度（度）。官方 debug 标注为 deg。
    //! @return true 且 outAngleDeg 为运动向坡度
    static bool TryGetCommandMoveSlopeDegrees(
        SCR_CharacterControllerComponent ctrl,
        out float outAngleDeg)
    {
        outAngleDeg = 0.0;
        CharacterCommandMove moveCmd = ResolveCommandMove(ctrl);
        if (!moveCmd)
            return false;
        outAngleDeg = moveCmd.GetMovementSlopeAngle();
        if (outAngleDeg > 45.0)
            outAngleDeg = 45.0;
        if (outAngleDeg < -45.0)
            outAngleDeg = -45.0;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 世界速度（m/s）。仅读 GetVelocityWS；失败返回 false。
    //! 陆地调用方禁止再用位置差分兜底。
    static bool TryGetVelocityWS(IEntity owner, out vector outVelocity)
    {
        outVelocity = vector.Zero;
        if (!owner)
            return false;
        if (!SCR_RSS_RuntimeGuard.IsEntityWorldUsable(owner))
            return false;

        CharacterMovementComponent move = ResolveMovement(owner);
        if (!move)
            return false;

        vector v = move.GetVelocityWS();
        float len = v.Length();
        // 拒绝荒诞值（旧差分曾把陆地打到 7m/s 顶）
        float maxLen = SCR_RSS_MetabolismMath.GAME_MAX_SPEED + 0.75;
        if (len > maxLen)
            return false;
        if (len != len)
            return false;

        outVelocity = v;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 脚下 Surface → GameMaterial（引擎接触缓存）
    //! GameMaterial 是 sealed SurfaceProperties，不可 Class.Cast / 赋 null；与 TraceParam.SurfaceProps 同写法。
    static bool TryGetFloorGameMaterial(IEntity owner, out GameMaterial outMaterial)
    {
        CharacterMovementComponent move = ResolveMovement(owner);
        if (!move)
            return false;
        // 游泳/坠落时脚下材质常空；其余情况直接读引擎缓存（站立也可能有 FloorSurface）
        if (move.IsSwimming())
            return false;
        if (move.IsFalling())
            return false;

        SurfaceProperties surf = move.GetFloorSurface();
        if (!surf)
            return false;

        // SurfaceProperties → GameMaterial（与 TerrainDetector Trace 赋值一致）
        outMaterial = surf;
        return true;
    }

    //------------------------------------------------------------------------------------------------
    //! 从 GameMaterial 解析地形密度（与 TerrainDetector Trace 路径同表）
    static float ResolveDensityFromMaterial(GameMaterial material)
    {
        if (!material)
            return -1.0;
        BallisticInfo ballisticInfo = material.GetBallisticInfo();
        float density = -1.0;
        if (ballisticInfo)
            density = ballisticInfo.GetDensity();
        density = SCR_RSS_MaterialTerrainTable.ResolveDensity(material, density);
        return density;
    }
}
