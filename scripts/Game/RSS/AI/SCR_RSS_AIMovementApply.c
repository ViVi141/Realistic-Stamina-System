//! AI 限速应用层（与计算层分离）。
//!
//! 引擎事实：
//!   - 单体：AICharacterMovementComponent.SetMovementTypeWanted
//!   - 群组：AIGroupMovementComponent.SetGroupCharactersWantedMovementType
//!     → 写入各成员 GetMovementTypeOverride；会盖掉单体 Wanted
//!   - BT 每帧重写；须用 Agent/Group Settings（Origin=SCENARIO）持久裁剪
//!   - 日志 wanted=WALK 且 ovr=RUN ⇒ 群组 Override 未钉住
//!   - 6.2.39：群组帽 = 成员 maxGait 的 min；禁止未疲劳队友把群组抬回 RUN
//!   - 6.3.0：maxGait 不得镜像 engPh==WALK（否则新刷/批量放置自锁慢走）；解除钉时重写 Wanted=RUN
//!   - 角色层 SetSpeedLimit 对 AI 常无效；真正掉速靠步态离开 Run

class SCR_RSS_AIMovementApply
{
    //! 压过航点 WAYPOINT(4000)：SCENARIO=5000
    protected static const SCR_EAISettingOrigin RSS_AI_SPEED_ORIGIN =
        SCR_EAISettingOrigin.SCENARIO;
    //! 6.2.35 曾用 COMMANDING；升级时清掉残留
    protected static const SCR_EAISettingOrigin RSS_AI_SPEED_ORIGIN_LEGACY =
        SCR_EAISettingOrigin.COMMANDING;

    protected static bool s_bLastSettingsOk;
    protected static bool s_bLastAiMoveOk;
    protected static bool s_bLastGroupOk;
    protected static bool s_bLastUsedFixedSetting;
    protected static EMovementType s_eLastMaxGait;
    protected static EMovementType s_eLastWanted;
    protected static EMovementType s_eLastOverride;
    protected static EMovementType s_eLastGroupCap;
    protected static int s_iLastGroupAgentCount;
    protected static float s_fLastPhaseTopMs;

    protected static ref map<EntityID, int> s_mInstalledMaxGait;
    //! 群组当前已安装的限制档（仅 IDLE/WALK）；RUN+ 时移除 Setting
    protected static ref map<EntityID, int> s_mInstalledGroupMaxGait;
    //! 各成员最近一次 RSS 限速步态（用于群组 min 聚合）
    protected static ref map<EntityID, int> s_mMemberMaxGait;

    static bool GetLastSettingsOk()
    {
        return s_bLastSettingsOk;
    }

    static bool GetLastAiMoveOk()
    {
        return s_bLastAiMoveOk;
    }

    static bool GetLastGroupOk()
    {
        return s_bLastGroupOk;
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

    static EMovementType GetLastGroupCap()
    {
        return s_eLastGroupCap;
    }

    static int GetLastGroupAgentCount()
    {
        return s_iLastGroupAgentCount;
    }

    static float GetLastPhaseTopMs()
    {
        return s_fLastPhaseTopMs;
    }

    //! 安装/刷新单体步态上限。WALK/IDLE → 固定 Setting；RUN/SPRINT → Range。
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

    //! 群组步态：成员 maxGait 取 min → 仅 IDLE/WALK 钉 Setting；禁止队友抬回 RUN。
    static void ApplyGroupMovementCap(IEntity characterOwner, EMovementType maxType)
    {
        s_bLastGroupOk = false;
        s_eLastGroupCap = maxType;
        s_iLastGroupAgentCount = 0;

        AIGroup parentGroup = ResolveParentGroup(characterOwner);
        if (!parentGroup)
            return;

        array<AIAgent> agentsProbe = {};
        parentGroup.GetAgents(agentsProbe);
        s_iLastGroupAgentCount = agentsProbe.Count();

        if (!s_mMemberMaxGait)
            s_mMemberMaxGait = new map<EntityID, int>();

        EntityID charId = characterOwner.GetID();
        s_mMemberMaxGait.Set(charId, maxType);

        EMovementType groupCap = ResolveGroupCapFromMembers(parentGroup, maxType);
        s_eLastGroupCap = groupCap;

        SCR_AIGroupSettingsComponent groupSettings =
            SCR_AIGroupSettingsComponent.Cast(
                parentGroup.FindComponent(SCR_AIGroupSettingsComponent));
        AIGroupMovementComponent groupMove =
            AIGroupMovementComponent.Cast(
                parentGroup.FindComponent(AIGroupMovementComponent));

        EntityID groupId = parentGroup.GetID();
        if (!s_mInstalledGroupMaxGait)
            s_mInstalledGroupMaxGait = new map<EntityID, int>();

        bool restrictGait = NeedsFixedSpeedSetting(groupCap);
        bool wasRestricted = false;
        int prevInstalledGait = -1;
        if (s_mInstalledGroupMaxGait.Find(groupId, prevInstalledGait))
            wasRestricted = true;

        if (groupSettings)
        {
            groupSettings.RemoveSettingsOfTypeAndOrigin(
                SCR_AIGroupCharactersMovementSpeedSettingBase, RSS_AI_SPEED_ORIGIN_LEGACY);

            if (restrictGait)
            {
                int prevGroupGait = -1;
                bool hasPrev = s_mInstalledGroupMaxGait.Find(groupId, prevGroupGait);
                bool needInstall = true;
                if (hasPrev)
                {
                    if (prevGroupGait == groupCap)
                        needInstall = false;
                }

                if (needInstall)
                {
                    // 群组 Setting 是固定档（GetSpeed 忽略 desired）——仅体力需要时钉 WALK/IDLE
                    SCR_AIGroupCharactersMovementSpeedSetting groupSetting =
                        SCR_AIGroupCharactersMovementSpeedSetting.Create(
                            RSS_AI_SPEED_ORIGIN,
                            groupCap);
                    groupSettings.AddSetting(groupSetting, false, true);
                    s_mInstalledGroupMaxGait.Set(groupId, groupCap);
                }
            }
            else
            {
                // 成员均允许 RUN+：撤掉我们的 SCENARIO 钉，把控制权还给 BT/航点
                groupSettings.RemoveSettingsOfTypeAndOrigin(
                    SCR_AIGroupCharactersMovementSpeedSettingBase, RSS_AI_SPEED_ORIGIN);
                int unusedPrev = -1;
                if (s_mInstalledGroupMaxGait.Find(groupId, unusedPrev))
                    s_mInstalledGroupMaxGait.Remove(groupId);
            }
        }

        if (groupMove)
        {
            if (restrictGait)
            {
                EMovementType resolved = groupCap;
                if (groupSettings)
                {
                    SCR_AIGroupCharactersMovementSpeedSettingBase gSetting =
                        SCR_AIGroupCharactersMovementSpeedSettingBase.Cast(
                            groupSettings.GetCurrentSetting(
                                SCR_AIGroupCharactersMovementSpeedSettingBase));
                    if (gSetting)
                        resolved = gSetting.GetSpeed(resolved);
                }

                groupMove.SetGroupCharactersWantedMovementType(resolved);
            }
            else if (wasRestricted)
            {
                // RemoveSetting 不会清掉先前写入的 Wanted=WALK；不重写会自锁慢走。
                // 先回到 RUN，下一拍由编队/航点 BT 按意图覆盖。
                groupMove.SetGroupCharactersWantedMovementType(EMovementType.RUN);
            }

            s_bLastGroupOk = true;
        }
    }

    //! 本群已知成员限速步态取最严（枚举 IDLE<WALK<RUN<SPRINT）
    protected static EMovementType ResolveGroupCapFromMembers(
        AIGroup parentGroup,
        EMovementType selfMaxType)
    {
        EMovementType groupCap = selfMaxType;
        if (!parentGroup || !s_mMemberMaxGait)
            return groupCap;

        array<AIAgent> agents = {};
        parentGroup.GetAgents(agents);
        int agentCount = agents.Count();
        int i = 0;
        while (i < agentCount)
        {
            AIAgent agent = agents[i];
            i = i + 1;
            if (!agent)
                continue;

            IEntity ent = agent.GetControlledEntity();
            if (!ent)
                continue;

            int otherGait = -1;
            if (!s_mMemberMaxGait.Find(ent.GetID(), otherGait))
                continue;

            if (otherGait < groupCap)
            {
                if (otherGait == EMovementType.IDLE)
                    groupCap = EMovementType.IDLE;
                else if (otherGait == EMovementType.WALK)
                    groupCap = EMovementType.WALK;
                else if (otherGait == EMovementType.RUN)
                    groupCap = EMovementType.RUN;
                else
                    groupCap = EMovementType.SPRINT;
            }
        }

        return groupCap;
    }

    //! 立刻把单体 Wanted 压到不超过 maxType
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

    //! 计算目标 → 应用：群组+单体 Setting 锁步态，再角色层限速
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
        s_bLastGroupOk = false;
        s_bLastUsedFixedSetting = false;
        s_eLastMaxGait = maxGait;
        s_eLastWanted = maxGait;
        s_eLastOverride = maxGait;
        s_eLastGroupCap = maxGait;
        s_iLastGroupAgentCount = 0;
        s_fLastPhaseTopMs = 0.0;

        if (!ctrl || !characterOwner)
            return;
        if (targetMs < 0.05)
            targetMs = 0.05;

        // 先群组（清 Override），再单体 Wanted
        ApplyGroupMovementCap(characterOwner, maxGait);
        ApplyMaxMovementTypeSetting(characterOwner, maxGait);
        ForceMovementTypeWanted(characterOwner, maxGait);

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

        SCR_RSS_SpeedBridge.ApplyAbsoluteMovementMaxSpeed(characterOwner, targetMs);

        if (maxGait == EMovementType.WALK)
            SCR_RSS_SpeedBridge.HoldWalkDynamicSpeedOverride(ctrl);
        else if (maxGait == EMovementType.IDLE)
            SCR_RSS_SpeedBridge.HoldWalkDynamicSpeedOverride(ctrl);
        else
            SCR_RSS_SpeedBridge.EndWalkDynamicSpeedOverride(ctrl, 1.0);

        // 再钉群组+单体（角色层写完后 Override 可能被刷回）
        ApplyGroupMovementCap(characterOwner, maxGait);
        ForceMovementTypeWanted(characterOwner, maxGait);
    }

    protected static AIGroup ResolveParentGroup(IEntity characterOwner)
    {
        if (!characterOwner)
            return null;

        AIControlComponent aiCtrl =
            AIControlComponent.Cast(characterOwner.FindComponent(AIControlComponent));
        if (!aiCtrl)
            return null;

        AIAgent agent = aiCtrl.GetAIAgent();
        if (!agent)
            return null;

        return agent.GetParentGroup();
    }

    protected static bool NeedsFixedSpeedSetting(EMovementType maxType)
    {
        if (maxType == EMovementType.WALK)
            return true;
        if (maxType == EMovementType.IDLE)
            return true;
        return false;
    }

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
