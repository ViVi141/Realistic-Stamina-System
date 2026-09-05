//! 官方引擎已算状态复用（优先读缓存，失败再回退 RSS 自算/Trace）。
//!
//! 优先级约定：
//!   坡度：CommandMove.GetMovementSlopeAngle → FloorNormal → Trace
//!   速度：Movement.GetVelocityWS → 位置差分（永不 Physics.GetVelocity）
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
    //! 世界速度（m/s）。优先 GetVelocityWS；失败返回 false（调用方用位置差分）。
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

        // 游泳/坠落仍可读 WS；进服无 Anim 时外层 IsCharacterMotionReady 已挡
        vector v = move.GetVelocityWS();
        float len = v.Length();
        if (len > 20.0)
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
