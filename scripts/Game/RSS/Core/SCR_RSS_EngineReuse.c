//! 官方引擎已算状态复用（优先读缓存，失败再回退 RSS 自算/Trace）。
//!
//! 优先级约定：
//!   坡度：CommandMove.GetMovementSlopeAngle → FloorNormal → Trace
//!   速度（陆地）：CharacterController.GetVelocity（官方案例）→ GetVelocityWS / GetRawVelocityWS
//!                禁止位置差分 / Physics.GetVelocity
//!   速度（游泳）：可由调用方另开位置差分（游泳时 Controller.GetVelocity 常为 0）
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
    //! 陆地角色速度（m/s）。官方案例：CharacterController.GetVelocity()（Y 恒为 0）。
    //! 其次 Movement.GetVelocityWS / GetRawVelocityWS。超上限钳制，不整段判失败。
    static bool TryGetCharacterVelocity(IEntity owner, out vector outVelocity)
    {
        outVelocity = vector.Zero;
        if (!owner)
            return false;
        if (!SCR_RSS_RuntimeGuard.IsEntityWorldUsable(owner))
            return false;

        float vmax = SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
        float rejectCap = vmax + 2.0;

        CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
            owner.FindComponent(CharacterControllerComponent));
        if (ctrl)
        {
            // 官方：相机 bob / 铁丝网伤害等均用 GetVelocity；注释写明 Y 恒为 0
            vector vCtrl = ctrl.GetVelocity();
            if (IsFiniteVec(vCtrl))
            {
                float lenCtrl = vCtrl.Length();
                // 近 0 时再试 Movement（避免漏采）；有读数则钳制后直接用
                if (lenCtrl > 0.05 && lenCtrl <= rejectCap)
                {
                    if (lenCtrl > vmax)
                        vCtrl = vCtrl.Normalized() * vmax;
                    outVelocity = vCtrl;
                    return true;
                }
            }
        }

        CharacterMovementComponent move = ResolveMovement(owner);
        if (!move)
            return false;

        // WS 近 0 时试 Raw（官方案例仍优先 Controller；此处仅兜底）
        vector vWs = move.GetVelocityWS();
        vector vRaw = move.GetRawVelocityWS();
        vector vPick = vWs;
        vector hWs = vWs;
        hWs[1] = 0.0;
        vector hRaw = vRaw;
        hRaw[1] = 0.0;
        if (hRaw.Length() > hWs.Length() + 0.05)
            vPick = vRaw;

        if (AcceptMovementVelocity(vPick, vmax, rejectCap, outVelocity))
            return true;

        return false;
    }

    //! @deprecated 请用 TryGetCharacterVelocity；保留别名避免旧调用断裂
    static bool TryGetVelocityWS(IEntity owner, out vector outVelocity)
    {
        return TryGetCharacterVelocity(owner, outVelocity);
    }

    protected static bool IsFiniteVec(vector v)
    {
        float x = v[0];
        float y = v[1];
        float z = v[2];
        if (x != x || y != y || z != z)
            return false;
        return true;
    }

    protected static bool AcceptMovementVelocity(
        vector v,
        float vmax,
        float rejectCap,
        out vector outVelocity)
    {
        outVelocity = vector.Zero;
        if (!IsFiniteVec(v))
            return false;

        vector horiz = v;
        horiz[1] = 0.0;
        float hLen = horiz.Length();
        if (hLen > rejectCap)
            return false;
        if (hLen > vmax)
            horiz = horiz.Normalized() * vmax;

        outVelocity = horiz;
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
