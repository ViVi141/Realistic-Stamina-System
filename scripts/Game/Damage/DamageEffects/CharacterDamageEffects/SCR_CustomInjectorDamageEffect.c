//! Persistent healing DoT used with SCR_ConsumableCustomInjector.
//! Assign this class in prefab SCR_ConsumableItemComponent → Damage Effects To Load (instead of SCR_MorphineDamageEffect).
//! Tune DPS/duration via the effect instance on the prefab or subclass for different curves.
class SCR_CustomInjectorDamageEffect : SCR_DotDamageEffect
{
	float m_fDurationPerHitZone;
	float m_fAccurateTimeSlice;
	protected ref array<HitZone> m_aPhysicalHitZones = {};

	//------------------------------------------------------------------------------------------------
	protected override void HandleConsequences(SCR_ExtendedDamageManagerComponent dmgManager, DamageEffectEvaluator evaluator)
	{
		super.HandleConsequences(dmgManager, evaluator);

		evaluator.HandleEffectConsequences(this, dmgManager);
	}

	//------------------------------------------------------------------------------------------------
	override bool HijackDamageEffect(SCR_ExtendedDamageManagerComponent dmgManager)
	{
		SetAffectedHitZone(dmgManager.GetDefaultHitZone());
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnEffectAdded(SCR_ExtendedDamageManagerComponent dmgManager)
	{
		super.OnEffectAdded(dmgManager);

		dmgManager.GetPhysicalHitZones(m_aPhysicalHitZones);
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnFrame(float timeSlice, SCR_ExtendedDamageManagerComponent dmgManager)
	{
		m_fAccurateTimeSlice = GetAccurateTimeSlice(timeSlice);

		float damageAmount = GetDPS() * m_fAccurateTimeSlice;

		DotDamageEffectTimerToken token = UpdateTimer(m_fAccurateTimeSlice, dmgManager);

		foreach (HitZone hz : m_aPhysicalHitZones)
		{
			DealCustomDot(hz, damageAmount, token, dmgManager);
		}
	}

	//------------------------------------------------------------------------------------------------
	override EDamageType GetDefaultDamageType()
	{
		return EDamageType.HEALING;
	}
}
