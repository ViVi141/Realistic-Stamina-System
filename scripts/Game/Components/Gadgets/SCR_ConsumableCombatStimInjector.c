//! Tactical stim autoinjector: RSS combat rush / crash / overdose (no morphine HP healing).
//! Prefab: Consumable Effect = this class; clear Damage Effects To Load; same MORPHINE gadget animations.
[BaseContainerProps()]
class SCR_ConsumableCombatStimInjector : SCR_ConsumableEffectHealthItems
{
	//------------------------------------------------------------------------------------------------
	override bool CanApplyEffect(notnull IEntity target, notnull IEntity user, out SCR_EConsumableFailReason failReason)
	{
		ChimeraCharacter char = ChimeraCharacter.Cast(target);
		if (!char)
			return false;

		SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(char.GetCharacterController());
		if (!ctrl)
			return false;

		int phase = ctrl.RSS_GetCombatStimPhase();
		if (phase == ERSS_CombatStimPhase.RUSH)
		{
			failReason = SCR_EConsumableFailReason.ALREADY_APPLIED;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanApplyEffectToHZ(notnull IEntity target, notnull IEntity user, ECharacterHitZoneGroup group, out SCR_EConsumableFailReason failReason = SCR_EConsumableFailReason.NONE)
	{
		return CanApplyEffect(target, user, failReason);
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		super.ApplyEffect(target, user, item, animParams);

		ChimeraCharacter char = ChimeraCharacter.Cast(target);
		if (!char)
			return;

		if (!Replication.IsServer())
			return;

		SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(char.GetCharacterController());
		if (!ctrl)
			return;

		ctrl.RSS_CombatStim_OnInjectServer();
	}

	//------------------------------------------------------------------------------------------------
	override ItemUseParameters GetAnimationParameters(IEntity item, notnull IEntity target, ECharacterHitZoneGroup group = ECharacterHitZoneGroup.VIRTUAL)
	{
		ItemUseParameters itemUseParams = super.GetAnimationParameters(item, target, group);
		itemUseParams.SetAllowMovementDuringAction(true);
		return itemUseParams;
	}

	//------------------------------------------------------------------------------------------------
	void SCR_ConsumableCombatStimInjector()
	{
		m_eConsumableType = SCR_EConsumableType.MORPHINE;
	}
}
