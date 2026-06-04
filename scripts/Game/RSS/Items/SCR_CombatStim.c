// ============================================================
// SCR_CombatStim.c
// 职责: 战斗兴奋剂（苯甲酸钠咖啡因 20%）完整逻辑
//       包含：常量定义 / 阶段状态机 / 消耗品效果 / 用户动作 / 控制器
// 所属域: Items
// ============================================================
// 合并自:
//   Components/Gadgets/SCR_CombatStimConstants.c
//   Components/Gadgets/SCR_CombatStimStateMachine.c
//   Components/Gadgets/SCR_ConsumableCombatStimInjector.c
//   UserActions/SCR_CombatStimUserAction.c
//   RSS/Core/SCR_CombatStimController.c
// ============================================================

// ---------------------------------------------------------------------------
// 1. 战斗兴奋剂阶段枚举与常量
// ---------------------------------------------------------------------------
enum ERSS_CombatStimPhase
{
    NONE = 0,
    DELAY = 1,
    ACTIVE = 2,
    OD = 3
}

class SCR_CombatStimConstants
{
    //! 首针后延迟生效时间（秒）
    static const float ABSORPTION_DELAY_SEC = 30.0;
    //! 药效持续（秒），15 分钟
    static const float ACTIVE_DURATION_SEC = 900.0;
    //! OD 注射时的新增时长倍率（本次注射只按 75% 计入）
    static const float OD_ADDITIVE_DURATION_MULTIPLIER = 0.75;
    //! 药效期内体力消耗率倍率（降低 15% 消耗 → 乘以 0.85）
    static const float STAMINA_DRAIN_MULTIPLIER = 0.85;
    //! OD 期间强制写入 Exhaustion 信号
    static const float OD_EXHAUSTION_SIGNAL_VALUE = 1.0;
    //! 药效期（ACTIVE）内全局流血速率倍率
    static const float BLEEDING_SCALE_MULT_ACTIVE = 2.0;
    //! OD 在 ACTIVE 之上再乘一次 1.5
    static const float BLEEDING_SCALE_MULT_OD_EXTRA = 1.5;
}

// ---------------------------------------------------------------------------
// 2. 战斗兴奋剂阶段状态机
// ---------------------------------------------------------------------------
class SCR_CombatStimStateMachine
{
    static bool IsActive(int phase)
    {
        return (phase == ERSS_CombatStimPhase.ACTIVE || phase == ERSS_CombatStimPhase.OD);
    }

    static bool IsOverdosed(int phase)
    {
        return (phase == ERSS_CombatStimPhase.OD);
    }

    static bool AdvancePhase(
        int currentPhase, float currentPhaseEndsAt, float worldTimeSec,
        out int outPhase, out float outPhaseEndsAt)
    {
        outPhase = currentPhase;
        outPhaseEndsAt = currentPhaseEndsAt;
        if (currentPhaseEndsAt < 0.0 || worldTimeSec < currentPhaseEndsAt)
            return false;

        if (currentPhase == ERSS_CombatStimPhase.ACTIVE)
        {
            outPhase = ERSS_CombatStimPhase.NONE;
            outPhaseEndsAt = -1.0;
            return true;
        }
        if (currentPhase == ERSS_CombatStimPhase.DELAY)
        {
            outPhase = ERSS_CombatStimPhase.ACTIVE;
            outPhaseEndsAt = worldTimeSec + SCR_CombatStimConstants.ACTIVE_DURATION_SEC;
            return true;
        }
        if (currentPhase == ERSS_CombatStimPhase.OD)
        {
            outPhase = ERSS_CombatStimPhase.NONE;
            outPhaseEndsAt = -1.0;
            return true;
        }
        return false;
    }

    static bool TryStartFromInjection(
        int currentPhase, float currentPhaseEndsAt, float worldTimeSec,
        int delayInjectionCount,
        out int outPhase, out float outPhaseEndsAt,
        out int outDelayInjectionCount, out bool outShouldDie)
    {
        outPhase = currentPhase;
        outPhaseEndsAt = currentPhaseEndsAt;
        outDelayInjectionCount = delayInjectionCount;
        outShouldDie = false;

        // 规范化：已过期视为 NONE
        int nPhase = currentPhase;
        float nEndsAt = currentPhaseEndsAt;
        int nDelayCount = delayInjectionCount;
        if (nPhase != ERSS_CombatStimPhase.NONE && nEndsAt >= 0.0 && worldTimeSec >= nEndsAt)
        {
            nPhase = ERSS_CombatStimPhase.NONE;
            nEndsAt = -1.0;
            nDelayCount = 0;
        }

        if (nPhase == ERSS_CombatStimPhase.NONE)
        {
            outPhase = ERSS_CombatStimPhase.DELAY;
            outPhaseEndsAt = worldTimeSec + SCR_CombatStimConstants.ABSORPTION_DELAY_SEC;
            outDelayInjectionCount = 1;
            return true;
        }
        if (nPhase == ERSS_CombatStimPhase.DELAY)
        {
            if (nEndsAt > worldTimeSec)
            {
                int nextCount = nDelayCount + 1;
                outDelayInjectionCount = nextCount;
                if (nextCount >= 3)
                {
                    outShouldDie = true;
                    return true;
                }
                float remaining = nEndsAt - worldTimeSec;
                if (remaining < 0.0) remaining = 0.0;
                outPhase = ERSS_CombatStimPhase.OD;
                outPhaseEndsAt = worldTimeSec + remaining
                    + SCR_CombatStimConstants.ACTIVE_DURATION_SEC
                    * SCR_CombatStimConstants.OD_ADDITIVE_DURATION_MULTIPLIER;
                return true;
            }
            outPhase = ERSS_CombatStimPhase.ACTIVE;
            outPhaseEndsAt = worldTimeSec + SCR_CombatStimConstants.ACTIVE_DURATION_SEC;
            outDelayInjectionCount = 0;
            return true;
        }
        if (nPhase == ERSS_CombatStimPhase.ACTIVE)
        {
            float remaining = nEndsAt - worldTimeSec;
            if (remaining < 0.0) remaining = 0.0;
            outPhase = ERSS_CombatStimPhase.OD;
            outPhaseEndsAt = worldTimeSec + remaining
                + SCR_CombatStimConstants.ACTIVE_DURATION_SEC
                * SCR_CombatStimConstants.OD_ADDITIVE_DURATION_MULTIPLIER;
            outDelayInjectionCount = 0;
            return true;
        }
        if (nPhase == ERSS_CombatStimPhase.OD)
        {
            outShouldDie = true;
            return true;
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
// 3. 战斗兴奋剂消耗品效果
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_ConsumableCombatStimInjector : SCR_ConsumableEffectHealthItems
{
    override bool CanApplyEffect(notnull IEntity target, notnull IEntity user,
        out SCR_EConsumableFailReason failReason)
    {
        ChimeraCharacter char = ChimeraCharacter.Cast(target);
        if (!char)
            return false;
        SCR_CharacterControllerComponent ctrl =
            SCR_CharacterControllerComponent.Cast(char.GetCharacterController());
        if (!ctrl)
            return false;
        return true;
    }

    override bool CanApplyEffectToHZ(notnull IEntity target, notnull IEntity user,
        ECharacterHitZoneGroup group,
        out SCR_EConsumableFailReason failReason = SCR_EConsumableFailReason.NONE)
    {
        return CanApplyEffect(target, user, failReason);
    }

    override void ApplyEffect(notnull IEntity target, notnull IEntity user,
        IEntity item, ItemUseParameters animParams)
    {
        super.ApplyEffect(target, user, item, animParams);
        ChimeraCharacter char = ChimeraCharacter.Cast(target);
        if (!char) return;
        if (!Replication.IsServer()) return;
        SCR_CharacterControllerComponent ctrl =
            SCR_CharacterControllerComponent.Cast(char.GetCharacterController());
        if (!ctrl) return;
        ctrl.RSS_CombatStim_OnInjectServer();
    }

    override ItemUseParameters GetAnimationParameters(IEntity item,
        notnull IEntity target, ECharacterHitZoneGroup group = ECharacterHitZoneGroup.VIRTUAL)
    {
        ItemUseParameters params = super.GetAnimationParameters(item, target, group);
        params.SetAllowMovementDuringAction(true);
        return params;
    }

    void SCR_ConsumableCombatStimInjector()
    {
        m_eConsumableType = SCR_EConsumableType.MORPHINE;
    }
}

// ---------------------------------------------------------------------------
// 4. 战斗兴奋剂用户动作
// ---------------------------------------------------------------------------
class SCR_CombatStimUserAction : SCR_HealingUserAction
{
    override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
    {
        ChimeraCharacter character = ChimeraCharacter.Cast(pUserEntity);
        if (!character) return;
        CharacterControllerComponent controller = character.GetCharacterController();
        if (!controller) return;
        if (controller.GetLifeState() != ECharacterLifeState.ALIVE) return;
        SCR_ConsumableItemComponent consumableComponent = GetConsumableComponent(character);
        if (consumableComponent)
            consumableComponent.SetAlternativeModel(false);
    }

    override bool CanBePerformedScript(IEntity user)
    {
        ChimeraCharacter userCharacter = ChimeraCharacter.Cast(user);
        if (!userCharacter) return false;
        SCR_ConsumableItemComponent consumableComponent = GetConsumableComponent(userCharacter);
        if (!consumableComponent) return false;
        SCR_EConsumableFailReason failReason;
        if (!consumableComponent.GetConsumableEffect().CanApplyEffect(
                GetOwner(), userCharacter, failReason))
        {
            if (failReason == SCR_EConsumableFailReason.ALREADY_APPLIED)
                SetCannotPerformReason(m_sAlreadyApplied);
            return false;
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// 5. 战斗兴奋剂控制器（流血倍率管理）
// ---------------------------------------------------------------------------
class SCR_CombatStimController
{
    static float ComputeBleedingBaseRateForEffect(SCR_BleedingDamageEffect bleed)
    {
        if (!bleed)
            return 0.0;
        SCR_CharacterHitZone hz = SCR_CharacterHitZone.Cast(bleed.GetAffectedHitZone());
        if (!hz)
            return 0.0;
        return hz.GetMaxBleedingRate() - hz.GetMaxBleedingRate() * hz.GetHealthScaled();
    }

    static void RefreshBleedingEffectsToMatchScale(SCR_CharacterDamageManagerComponent dmgMgr)
    {
        if (!dmgMgr)
            return;
        float scale = dmgMgr.GetBleedingScale();
        array<ref SCR_PersistentDamageEffect> effects =
            dmgMgr.GetAllPersistentEffectsOfType(SCR_BleedingDamageEffect);
        foreach (SCR_PersistentDamageEffect pe : effects)
        {
            SCR_BleedingDamageEffect bleed = SCR_BleedingDamageEffect.Cast(pe);
            if (!bleed) continue;
            bleed.SetDPS(ComputeBleedingBaseRateForEffect(bleed) * scale);
        }
    }

    static void UpdateBleedingScale(
        int combatStimPhase, float bleedingBaseline,
        ChimeraCharacter ownerCharacter, out float newBleedingBaseline)
    {
        newBleedingBaseline = bleedingBaseline;
        SCR_CharacterDamageManagerComponent dmgMgr =
            SCR_CharacterDamageManagerComponent.Cast(ownerCharacter.GetDamageManager());
        if (!dmgMgr) return;

        bool wantBuff = false;
        float mult = 1.0;
        if (combatStimPhase == ERSS_CombatStimPhase.ACTIVE)
        {
            wantBuff = true;
            mult = SCR_CombatStimConstants.BLEEDING_SCALE_MULT_ACTIVE;
        }
        else if (combatStimPhase == ERSS_CombatStimPhase.OD)
        {
            wantBuff = true;
            mult = SCR_CombatStimConstants.BLEEDING_SCALE_MULT_ACTIVE
                * SCR_CombatStimConstants.BLEEDING_SCALE_MULT_OD_EXTRA;
        }

        if (!wantBuff)
        {
            if (bleedingBaseline >= 0.0)
            {
                dmgMgr.SetBleedingScale(bleedingBaseline, true);
                RefreshBleedingEffectsToMatchScale(dmgMgr);
                newBleedingBaseline = -1.0;
            }
            return;
        }

        if (bleedingBaseline < 0.0)
            newBleedingBaseline = dmgMgr.GetBleedingScale();
        dmgMgr.SetBleedingScale(newBleedingBaseline * mult, true);
        RefreshBleedingEffectsToMatchScale(dmgMgr);
    }

    static void ResetBleedingScaleBeforeKill(
        ChimeraCharacter ownerCharacter, float bleedingBaseline,
        out float newBleedingBaseline)
    {
        newBleedingBaseline = bleedingBaseline;
        if (bleedingBaseline < 0.0) return;
        SCR_CharacterDamageManagerComponent dmgMgr =
            SCR_CharacterDamageManagerComponent.Cast(ownerCharacter.GetDamageManager());
        if (dmgMgr)
        {
            dmgMgr.SetBleedingScale(bleedingBaseline, true);
            RefreshBleedingEffectsToMatchScale(dmgMgr);
        }
        newBleedingBaseline = -1.0;
    }
}
