//! 苯甲酸钠咖啡因注射液（20%）— 阶段与数值（吸收延迟 → 药效期）。
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
	//! OD 期间强制写入 Exhaustion 信号，触发中毒/恶化画面效果
	static const float OD_EXHAUSTION_SIGNAL_VALUE = 1.0;
}
