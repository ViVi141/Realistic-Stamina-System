//! Combat stim-pen phase state (tactical injector, not HP healing).
enum ERSS_CombatStimPhase
{
	NONE = 0,
	RUSH = 1,
	CRASH = 2
}

//! Durations in seconds (design: ~60–90s rush, ~2–3min crash).
class SCR_CombatStimConstants
{
	static const float RUSH_DURATION_SEC = 75.0;
	static const float CRASH_DURATION_SEC = 150.0;
	static const float CRASH_SPEED_MULTIPLIER = 0.38;
	static const float OVERDOSE_HEALTH_SCALED = 0.1;
}
