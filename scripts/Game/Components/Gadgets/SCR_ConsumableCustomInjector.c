//! Custom injector aligned with MorphineInjection_01.et layout:
//! - Consumable Effect = this class; Damage Effects To Load = SCR_CustomInjectorDamageEffect (set in Workbench).
//! - m_eConsumableType MORPHINE keeps the same use animations / gadget flow as morphine.
//! - Healing comes from the loaded damage effect + engine handling of Item Regeneration fields on the item component.
//! - Do not duplicate CustomRegeneration here when using Damage Effects To Load (avoids double application).
[BaseContainerProps()]
class SCR_ConsumableCustomInjector : SCR_ConsumableEffectHealthItems
{
	//------------------------------------------------------------------------------------------------
	//! Return false and set failReason when this injector must not be used (role, buffs, RSS hooks, etc.).
	protected bool PassesCustomInjectorRules(notnull ChimeraCharacter character, notnull IEntity user, out SCR_EConsumableFailReason failReason)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanApplyEffect(notnull IEntity target, notnull IEntity user, out SCR_EConsumableFailReason failReason)
	{
		ChimeraCharacter char = ChimeraCharacter.Cast(target);
		if (!char)
			return false;

		if (!PassesCustomInjectorRules(char, user, failReason))
			return false;

		SCR_CharacterDamageManagerComponent damageMgr = SCR_CharacterDamageManagerComponent.Cast(char.GetDamageManager());
		if (!damageMgr)
			return false;

		array<ref SCR_PersistentDamageEffect> effects = damageMgr.GetAllPersistentEffectsOfType(SCR_CustomInjectorDamageEffect);
		if (!effects.IsEmpty())
		{
			failReason = SCR_EConsumableFailReason.ALREADY_APPLIED;
			return false;
		}

		array<HitZone> hitZones = {};
		damageMgr.GetPhysicalHitZones(hitZones);
		SCR_RegeneratingHitZone regenHitZone;

		foreach (HitZone hitZone : hitZones)
		{
			regenHitZone = SCR_RegeneratingHitZone.Cast(hitZone);
			if (!regenHitZone)
				continue;

			if (regenHitZone.GetDamageState() != EDamageState.UNDAMAGED)
				return true;
		}

		failReason = SCR_EConsumableFailReason.UNDAMAGED;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanApplyEffectToHZ(notnull IEntity target, notnull IEntity user, ECharacterHitZoneGroup group, out SCR_EConsumableFailReason failReason = SCR_EConsumableFailReason.NONE)
	{
		return CanApplyEffect(target, user, failReason);
	}

	//------------------------------------------------------------------------------------------------
	override ItemUseParameters GetAnimationParameters(IEntity item, notnull IEntity target, ECharacterHitZoneGroup group = ECharacterHitZoneGroup.VIRTUAL)
	{
		ItemUseParameters itemUseParams = super.GetAnimationParameters(item, target, group);
		itemUseParams.SetAllowMovementDuringAction(true);
		return itemUseParams;
	}

	//------------------------------------------------------------------------------------------------
	void SCR_ConsumableCustomInjector()
	{
		m_eConsumableType = SCR_EConsumableType.MORPHINE;
	}
}
