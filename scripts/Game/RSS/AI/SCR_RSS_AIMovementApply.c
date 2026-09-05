//! AI 限速应用层（与计算层分离）。
//!
//! 引擎事实：
//!   - AI 原生控制是离散步态：AICharacterMovementComponent.SetMovementTypeWanted
//!   - BT 节点 SCR_AICharacterSetMovementSpeed 每模拟帧重写 Wanted；一次性 Set 会被盖掉
//!   - 持久上限必须写在 Agent 的 SCR_AICharacterSettingsComponent
//!     （SCR_AICharacterMovementSpeedSetting_Range），BT 经 GetSpeed() 裁剪
//!   - Chimera SetSpeedLimit/OverrideMaxSpeed 是角色层分数帽；步态仍是 Sprint 时
//!     会对着冲刺顶乘分数 → 看起来「原速」。先锁步态再写分数。

class SCR_RSS_AIMovementApply
{
    protected static const SCR_EAISettingOrigin RSS_AI_SPEED_ORIGIN = SCR_EAISettingOrigin.COMMANDING;

    //! 最近一次 ApplyTargetMs 诊断（供 AI 调试日志读取）
    protected static bool s_bLastSettingsOk;
    protected static bool s_bLastAiMoveOk;
    protected static EMovementType s_eLastMaxGait;
    protected static EMovementType s_eLastWanted;
    protected static float s_fLastPhaseTopMs;

    static bool GetLastSettingsOk()
    {
        return s_bLastSettingsOk;
    }

    static bool GetLastAiMoveOk()
    {
        return s_bLastAiMoveOk;
    }

    static EMovementType GetLastMaxGait()
    {
        return s_eLastMaxGait;
    }

    static EMovementType GetLastWanted()
    {
        return s_eLastWanted;
    }

    static float GetLastPhaseTopMs()
    {
        return s_fLastPhaseTopMs;
    }

    //! 安装/刷新步态上限（IDLE..maxType）。removeSameTypeAndOrigin 替换本模组旧项。
    static void ApplyMaxMovementTypeSetting(IEntity characterOwner, EMovementType maxType)
    {
        if (!characterOwner)
            return;

        SCR_AICharacterSettingsComponent settingsComp =
            SCR_AICharacterSettingsComponent.FindOnControlledEntity(characterOwner);
        if (!settingsComp)
        {
            s_bLastSettingsOk = false;
            return;
        }

        SCR_AICharacterMovementSpeedSetting_Range range =
            SCR_AICharacterMovementSpeedSetting_Range.Create(
                RSS_AI_SPEED_ORIGIN,
                SCR_EAIBehaviorCause.ALWAYS,
                EMovementType.IDLE,
                maxType);
        settingsComp.AddCharacterSetting(range, false, true);
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
        s_eLastMaxGait = maxGait;
        s_eLastWanted = maxGait;
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

        // AI：SetSpeedLimit 常被 BT/相位滞后吃掉（日志 latch+frac=0.64 仍 v=3.5）。
        // 并行写绝对 MovementMaxSpeed（与玩家试跑同 API），直接帽住 m/s。
        SCR_RSS_SpeedBridge.ApplyAbsoluteMovementMaxSpeed(characterOwner, targetMs);

        // 巡航/跛行：与玩家 CP 掉带同款 CapsLock Walk 覆盖，迫使 engPh 离开 Run
        if (maxGait == EMovementType.WALK)
            SCR_RSS_SpeedBridge.HoldWalkDynamicSpeedOverride(ctrl);
        else
            SCR_RSS_SpeedBridge.EndWalkDynamicSpeedOverride(ctrl, 1.0);
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
