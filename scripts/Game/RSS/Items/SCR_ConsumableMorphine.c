// ============================================================
// SCR_ConsumableMorphine.c
// 职责: 吗啡消耗品 — 持续治疗效果 + 消耗品逻辑 + 用户动作
// 所属域: Items
// ============================================================

// ---------------------------------------------------------------------------
// 1. 吗啡持续伤害效果（持续治疗 DoT）
// ---------------------------------------------------------------------------
class SCR_MorphineDamageEffect : SCR_DotDamageEffect
{
    float m_fDurationPerHitZone;
    float m_fAccurateTimeSlice;
    protected ref array<HitZone> m_aPhysicalHitZones = {};

    protected override void HandleConsequences(
        SCR_ExtendedDamageManagerComponent dmgManager, DamageEffectEvaluator evaluator)
    {
        super.HandleConsequences(dmgManager, evaluator);
        evaluator.HandleEffectConsequences(this, dmgManager);
    }

    override bool HijackDamageEffect(SCR_ExtendedDamageManagerComponent dmgManager)
    {
        SetAffectedHitZone(dmgManager.GetDefaultHitZone());
        return false;
    }

    override void OnEffectAdded(SCR_ExtendedDamageManagerComponent dmgManager)
    {
        super.OnEffectAdded(dmgManager);
        dmgManager.GetPhysicalHitZones(m_aPhysicalHitZones);
    }

    protected override void EOnFrame(
        float timeSlice, SCR_ExtendedDamageManagerComponent dmgManager)
    {
        m_fAccurateTimeSlice = GetAccurateTimeSlice(timeSlice);
        float damageAmount = GetDPS() * m_fAccurateTimeSlice;
        DotDamageEffectTimerToken token = UpdateTimer(m_fAccurateTimeSlice, dmgManager);
        foreach (HitZone hz : m_aPhysicalHitZones)
            DealCustomDot(hz, damageAmount, token, dmgManager);
    }

    override EDamageType GetDefaultDamageType()
    {
        return EDamageType.HEALING;
    }
}

// ---------------------------------------------------------------------------
// 2. 吗啡消耗品效果
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_ConsumableMorphine : SCR_ConsumableEffectHealthItems
{
    override bool CanApplyEffect(notnull IEntity target, notnull IEntity user,
        out SCR_EConsumableFailReason failReason)
    {
        ChimeraCharacter char = ChimeraCharacter.Cast(target);
        if (!char) return false;

        SCR_CharacterDamageManagerComponent damageMgr =
            SCR_CharacterDamageManagerComponent.Cast(char.GetDamageManager());
        if (!damageMgr) return false;

        array<ref SCR_PersistentDamageEffect> effects =
            damageMgr.GetAllPersistentEffectsOfType(SCR_MorphineDamageEffect);
        if (!effects.IsEmpty())
        {
            failReason = SCR_EConsumableFailReason.ALREADY_APPLIED;
            return false;
        }

        array<HitZone> hitZones = {};
        damageMgr.GetPhysicalHitZones(hitZones);
        foreach (HitZone hitZone : hitZones)
        {
            SCR_RegeneratingHitZone regenHitZone = SCR_RegeneratingHitZone.Cast(hitZone);
            if (!regenHitZone) continue;
            if (regenHitZone.GetDamageState() != EDamageState.UNDAMAGED)
                return true;
        }
        failReason = SCR_EConsumableFailReason.UNDAMAGED;
        return false;
    }

    override bool CanApplyEffectToHZ(notnull IEntity target, notnull IEntity user,
        ECharacterHitZoneGroup group,
        out SCR_EConsumableFailReason failReason = SCR_EConsumableFailReason.NONE)
    {
        return CanApplyEffect(target, user, failReason);
    }

    EDamageType GetDefaultDamageType()
    {
        return EDamageType.HEALING;
    }

    override ItemUseParameters GetAnimationParameters(IEntity item,
        notnull IEntity target, ECharacterHitZoneGroup group = ECharacterHitZoneGroup.VIRTUAL)
    {
        ItemUseParameters params = super.GetAnimationParameters(item, target, group);
        params.SetAllowMovementDuringAction(true);
        return params;
    }

    void SCR_ConsumableMorphine()
    {
        m_eConsumableType = SCR_EConsumableType.MORPHINE;
    }
}

// ---------------------------------------------------------------------------
// 3. 吗啡用户动作
// ---------------------------------------------------------------------------
class SCR_MorphineUserAction : SCR_HealingUserAction
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
        int reason;
        if (!consumableComponent.GetConsumableEffect().CanApplyEffect(
                GetOwner(), userCharacter, reason))
        {
            if (reason == SCR_EConsumableFailReason.UNDAMAGED)
                SetCannotPerformReason(m_sNotDamaged);
            else if (reason == SCR_EConsumableFailReason.ALREADY_APPLIED)
                SetCannotPerformReason(m_sAlreadyApplied);
            return false;
        }
        return true;
    }
}
