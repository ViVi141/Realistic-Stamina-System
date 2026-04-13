//! 苯甲酸钠咖啡因 20% 自注射器：延迟生效 + 药效期内体力/天气等见 PlayerBase。

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
