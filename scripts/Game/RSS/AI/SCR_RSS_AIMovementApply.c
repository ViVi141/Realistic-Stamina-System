//! AI 限速应用层（与计算层分离）。
//!
//! 引擎事实：
//!   - AI 原生控制是离散步态：AICharacterMovementComponent.SetMovementTypeWanted
//!   - BT 节点 SCR_AICharacterSetMovementSpeed 每模拟帧重写 Wanted；一次性 Set 会被盖掉
//!   - 持久上限必须写在 Agent 的 SCR_AICharacterSettingsComponent
//!     （SCR_AICharacterMovementSpeedSetting / _Range），BT 经 GetSpeed() 裁剪
//!   - Origin 优先级：EDITOR > SCENARIO > WAYPOINT > COMMANDING > …
//!     航点常以 WAYPOINT 写冲刺；RSS 须用 SCENARIO 才能压过航点
//!   - 巡航/跛行用固定 Setting（钉死 WALK），勿用 Range（仍允许 BT 选 WALK 内最高）
//!   - Chimera SetSpeedLimit / SetMovementMaxSpeed 是角色层帽；步态仍 Sprint 时
//!     分数乘冲刺顶 → 看起来「原速」。先锁步态再写分数。

class SCR_RSS_AIMovementApply
{
    //! 压过航点 WAYPOINT(4000)：SCENARIO=5000
    protected static const SCR_EAISettingOrigin RSS_AI_SPEED_ORIGIN =
        SCR_EAISettingOrigin.SCENARIO;
    //! 6.2.35 曾用 COMMANDING；升级时清掉残留以免与 SCENARIO 并存混淆诊断
    protected static const SCR_EAISettingOrigin RSS_AI_SPEED_ORIGIN_LEGACY =
        SCR_EAISettingOrigin.COMMANDING;

    //! 最近一次 ApplyTargetMs 诊断（供 AI 调试日志读取）
    protected static bool s_bLastSettingsOk;
    protected static bool s_bLastAiMoveOk;
    protected static bool s_bLastUsedFixedSetting;
    protected static EMovementType s_eLastMaxGait;
    protected static EMovementType s_eLastWanted;
    protected static EMovementType s_eLastOverride;
    protected static float s_fLastPhaseTopMs;

    //! 每实体已安装的最高步态（避免每 tick 重复 AddSetting）
    protected static ref map<EntityID, int> s_mInstalledMaxGait;

    static bool GetLastSettingsOk()
    {
        return s_bLastSettingsOk;
    }

    static bool GetLastAiMoveOk()
    {
        return s_bLastAiMoveOk;
    }

    static bool GetLastUsedFixedSetting()
    {
        return s_bLastUsedFixedSetting;
    }

    static EMovementType GetLastMaxGait()
    {
        return s_eLastMaxGait;
    }

    static EMovementType GetLastWanted()
    {
        return s_eLastWanted;
    }

    static EMovementType GetLastOverride()
    {
        return s_eLastOverride;
    }

    static float GetLastPhaseTopMs()
    {
        return s_fLastPhaseTopMs;
    }

    //! 安装/刷新步态上限。WALK/IDLE → 固定 Setting；RUN/SPRINT → Range。
    static void ApplyMaxMovementTypeSetting(IEntity characterOwner, EMovementType maxType)
    {
        if (!characterOwner)
            return;

        SCR_AICharacterSettingsComponent settingsComp =
            SCR_AICharacterSettingsComponent.FindOnControlledEntity(characterOwner);
        if (!settingsComp)
        {
            s_bLastSettingsOk = false;
            s_bLastUsedFixedSetting = false;
            return;
        }

        EntityID entId = characterOwner.GetID();
        if (!s_mInstalledMaxGait)
            s_mInstalledMaxGait = new map<EntityID, int>();

        int prevGait = -1;
        bool hasPrev = s_mInstalledMaxGait.Find(entId, prevGait);
        if (hasPrev)
        {
            if (prevGait == maxType)
            {
                // ApplyTargetMs 入口会清 s_bLastSettingsOk；已安装同档则直接认为 Setting 仍有效
                s_bLastUsedFixedSetting = NeedsFixedSpeedSetting(maxType);
                s_bLastSettingsOk = true;
                return;
            }
        }

        settingsComp.RemoveSettingsOfTypeAndOrigin(
            SCR_AICharacterMovementSpeedSettingBase, RSS_AI_SPEED_ORIGIN_LEGACY);

        bool useFixed = NeedsFixedSpeedSetting(maxType);
        s_bLastUsedFixedSetting = useFixed;

        if (useFixed)
        {
            SCR_AICharacterMovementSpeedSetting fixedSetting =
                SCR_AICharacterMovementSpeedSetting.Create(
                    RSS_AI_SPEED_ORIGIN,
                    SCR_EAIBehaviorCause.ALWAYS,
                    maxType);
            settingsComp.AddCharacterSetting(fixedSetting, false, true);
        }
        else
        {
            SCR_AICharacterMovementSpeedSetting_Range range =
                SCR_AICharacterMovementSpeedSetting_Range.Create(
                    RSS_AI_SPEED_ORIGIN,
                    SCR_EAIBehaviorCause.ALWAYS,
                    EMovementType.IDLE,
                    maxType);
            settingsComp.AddCharacterSetting(range, false, true);
        }

        s_mInstalledMaxGait.Set(entId, maxType);
        s_bLastSettingsOk = true;
    }

    //! 立刻把 Wanted 压到不超过 maxType（不等下一次 BT）
    static void ForceMovementTypeWanted(IEntity characterOwner, EMovementType maxType)
    {
        if (!characterOwner)
            return;

        AICharacterMovementComponent aiMove = AICharacterMovementComponent.Cast(
            characterOwner.FindComponent(AICharacterMovementComponent));
        if (!aiMove)
        {
            s_bLastAiMoveOk = false;
            s_eLastWanted = maxType;
            s_eLastOverride = maxType;
            return;
        }

        EMovementType resolved = maxType;
        SCR_AICharacterSettingsComponent settingsComp =
            SCR_AICharacterSettingsComponent.FindOnControlledEntity(characterOwner);
        if (settingsComp)
        {
            SCR_AICharacterMovementSpeedSettingBase setting =
                SCR_AICharacterMovementSpeedSettingBase.Cast(
                    settingsComp.GetCurrentSetting(SCR_AICharacterMovementSpeedSettingBase));
            if (setting)
                resolved = setting.GetSpeed(resolved);
        }

        aiMove.SetMovementTypeWanted(resolved);
        s_bLastAiMoveOk = true;
        s_eLastWanted = resolved;
        s_eLastOverride = aiMove.GetMovementTypeOverride();
    }

    //! 计算目标 → 应用：Setting 锁步态 + SetSpeedLimit + 绝对 MovementMaxSpeed
    //! @param targetMs 计算层绝对帽
    //! @param maxGait 允许的最高步态
    //! @param instant 巡航等需立刻压速时 true
    static void ApplyTargetMs(
        SCR_CharacterControllerComponent ctrl,
        IEntity characterOwner,
        float targetMs,
        EMovementType maxGait,
        bool instant,
        out float outFrac)
    {
        outFrac = 0.999;
        s_bLastSettingsOk = false;
        s_bLastAiMoveOk = false;
        s_bLastUsedFixedSetting = false;
        s_eLastMaxGait = maxGait;
        s_eLastWanted = maxGait;
        s_eLastOverride = maxGait;
        s_fLastPhaseTopMs = 0.0;

        if (!ctrl || !characterOwner)
            return;
        if (targetMs < 0.05)
            targetMs = 0.05;

        ApplyMaxMovementTypeSetting(characterOwner, maxGait);
        ForceMovementTypeWanted(characterOwner, maxGait);

        // SetSpeedLimit 倍率相对「引擎当前相位顶」，不是意图步态顶。
        // Idle(engPh=0) 时不要用 walkTop 当分母（否则 frac≈1 且 top 显示 1.49）。
        int engPhase = ctrl.GetCurrentMovementPhase();
        float phaseTopMs = 0.0;
        if (engPhase >= 1)
            phaseTopMs = ResolvePhaseTopMsForEnginePhase(ctrl, characterOwner, engPhase);
        if (phaseTopMs < 0.1)
            phaseTopMs = ResolvePhaseTopMs(ctrl, characterOwner, maxGait);
        s_fLastPhaseTopMs = phaseTopMs;
        outFrac = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(targetMs, phaseTopMs, true);
        outFrac = SCR_RSS_DrainCalculator.ClampSpeedLimitFractionToGaitBand(outFrac, false);

        if (SCR_RSS_SpeedBridge.IsStaminaSpeedPressEnabled())
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(characterOwner, outFrac, instant);
        else
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(characterOwner, 0.999, instant);

        // AI：SetSpeedLimit 常被 BT/相位滞后吃掉。并行写绝对 MovementMaxSpeed。
        SCR_RSS_SpeedBridge.ApplyAbsoluteMovementMaxSpeed(characterOwner, targetMs);

        // 巡航/跛行：玩家同款 CapsLock Walk 覆盖，迫使 engPh 离开 Run
        if (maxGait == EMovementType.WALK)
            SCR_RSS_SpeedBridge.HoldWalkDynamicSpeedOverride(ctrl);
        else if (maxGait == EMovementType.IDLE)
            SCR_RSS_SpeedBridge.HoldWalkDynamicSpeedOverride(ctrl);
        else
            SCR_RSS_SpeedBridge.EndWalkDynamicSpeedOverride(ctrl, 1.0);

        // 角色层写完后再钉一次 Wanted（防本 tick 内被其它系统盖掉）
        ForceMovementTypeWanted(characterOwner, maxGait);
    }

    protected static bool NeedsFixedSpeedSetting(EMovementType maxType)
    {
        if (maxType == EMovementType.WALK)
            return true;
        if (maxType == EMovementType.IDLE)
            return true;
        return false;
    }

    //! 按引擎当前相位取顶速（供 SetSpeedLimit 分母）
    protected static float ResolvePhaseTopMsForEnginePhase(
        SCR_CharacterControllerComponent ctrl,
        IEntity characterOwner,
        int enginePhase)
    {
        EMovementType gait = EMovementType.RUN;
        if (enginePhase <= 0)
            gait = EMovementType.IDLE;
        else if (enginePhase == 1)
            gait = EMovementType.WALK;
        else if (enginePhase >= 3)
            gait = EMovementType.SPRINT;
        return ResolvePhaseTopMs(ctrl, characterOwner, gait);
    }

    protected static float ResolvePhaseTopMs(
        SCR_CharacterControllerComponent ctrl,
        IEntity characterOwner,
        EMovementType maxGait)
    {
        int phase = 2;
        if (maxGait == EMovementType.WALK)
            phase = 1;
        else if (maxGait == EMovementType.SPRINT)
            phase = 3;
        else if (maxGait == EMovementType.IDLE)
            phase = 1;

        CharacterAnimationComponent anim = null;
        if (ctrl)
            anim = ctrl.GetAnimationComponent();
        if (anim && characterOwner && characterOwner.GetWorld())
        {
            float walkMs;
            float runMs;
            float sprintMs;
            SCR_PlayerBaseEngineTopSampler.SampleLiveEngineTops(
                characterOwner, anim, walkMs, runMs, sprintMs);
            float live = SCR_PlayerBaseEngineTopSampler.PickLiveEngineTopMs(
                phase, walkMs, runMs, sprintMs);
            if (live > 0.1)
                return live;
        }

        if (ctrl)
        {
            if (phase == 3)
            {
                float sprintTop = ctrl.GetOriginalEngineMaxSpeed_Sprint();
                if (sprintTop > 0.1)
                    return sprintTop;
            }
            else if (phase == 1)
            {
                float walkTop = ctrl.GetOriginalEngineMaxSpeed_Walk();
                if (walkTop > 0.1)
                    return walkTop;
                return SCR_RSS_Constants.ENGINE_WALK_TOP_MS;
            }
            else
            {
                float runTop = ctrl.GetOriginalEngineMaxSpeed_Run();
                if (runTop > 0.1)
                    return runTop;
            }
        }

        return SCR_PlayerBaseEngineTopSampler.GetEngineTopFallbackMs(phase);
    }
}
