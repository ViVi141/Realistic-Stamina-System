//! User action for SCR_ConsumableCombatStimInjector (CSB 20% injector; same rules as morphine self-heal eligibility).
class SCR_CombatStimUserAction : SCR_HealingUserAction
{
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(pUserEntity);
		if (!character)
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return;

		if (controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return;

		SCR_ConsumableItemComponent consumableComponent = GetConsumableComponent(character);
		if (consumableComponent)
			consumableComponent.SetAlternativeModel(false);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		ChimeraCharacter userCharacter = ChimeraCharacter.Cast(user);
		if (!userCharacter)
			return false;

		SCR_ConsumableItemComponent consumableComponent = GetConsumableComponent(userCharacter);
		if (!consumableComponent)
			return false;

		SCR_EConsumableFailReason failReason;
		if (!consumableComponent.GetConsumableEffect().CanApplyEffect(GetOwner(), userCharacter, failReason))
		{
			if (failReason == SCR_EConsumableFailReason.ALREADY_APPLIED)
				SetCannotPerformReason(m_sAlreadyApplied);

			return false;
		}

		return true;
	}
}
