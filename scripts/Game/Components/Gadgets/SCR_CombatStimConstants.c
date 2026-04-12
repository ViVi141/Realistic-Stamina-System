//! 苯甲酸钠咖啡因注射液（20%）— 阶段与数值（吸收延迟 → 药效期）。
enum ERSS_CombatStimPhase
{
	NONE = 0,
	DELAY = 1,
	ACTIVE = 2
}

class SCR_CombatStimConstants
{
	//! 注射后至药效开始（秒）
	static const float ABSORPTION_DELAY_SEC = 30.0;
	//! 药效持续（秒），15 分钟
	static const float ACTIVE_DURATION_SEC = 900.0;
	//! 药效期内体力消耗率倍率（降低 15% 消耗 → 乘以 0.85）
	static const float STAMINA_DRAIN_MULTIPLIER = 0.85;
}
