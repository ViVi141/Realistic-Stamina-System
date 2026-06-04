/*
 * SCR_RSS_V5_Params.c
 *
 * v5新增参数定义
 * 补充SCR_RSS_Params.c中v5双池、强制降速、系数物理化相关参数
 */

class SCR_RSS_V5_Params
{
	// =========================================================================
	// v5无氧系统参数(Phase 2)
	// =========================================================================

	// 无氧消耗与恢复
	static const float ANAEROBIC_DRAIN_COEFF_BASE = 0.08;      // 基础消耗系数(每秒@4m/s)
	static const float ANAEROBIC_DRAIN_LOAD_FACTOR = 0.01;     // 负重增益(每kg)
	static const float ANAEROBIC_RECOVERY_BASE_RATE = 0.01;    // 基础恢复速率(/s)
	static const float ANAEROBIC_RECOVERY_AEROBIC_GATE = 0.15;  // 有氧池门槛

	// 冷却时间配置(秒)
	static const float BURST_COOLDOWN_FULL_ELITE = 180.0;      // Elite抽干冷却
	static const float BURST_COOLDOWN_FULL_STANDARD = 150.0;   // Standard
	static const float BURST_COOLDOWN_FULL_TACTICAL = 120.0;   // Tactical

	static const float BURST_COOLDOWN_SHORT_ELITE = 75.0;      // Elite短冲刺冷却(60-90s中点)
	static const float BURST_COOLDOWN_SHORT_STANDARD = 70.0;   // Standard
	static const float BURST_COOLDOWN_SHORT_TACTICAL = 60.0;   // Tactical

	static const float BURST_EARLY_RELEASE_BONUS = 0.4;        // 提前释放奖励系数

	// 冲刺门槛
	static const float SPRINT_ENABLE_ANAEROBIC_THRESHOLD = 0.20;  // 无氧池20%门槛
	static const float SPRINT_ENABLE_AEROBIC_MIN = 0.10;          // 有氧池10%最低要求

	// =========================================================================
	// v5强制降速参数(Phase 1)
	// =========================================================================

	static const float SUSTAINABLE_WATTS_DEFAULT = 400.0;           // W
	static const float SUSTAINABLE_WATTS_LOAD_PENALTY = 2.0;        // W/kg(超过35kg)
	static const float FORCED_SLOWDOWN_DECAY_RATE = 0.02;           // %/s
	static const float FORCED_SLOWDOWN_MIN_RATIO = 0.5;             // 50%最低速度
	static const bool ENABLE_PLAYER_OVERRIDE_SLOWDOWN = true;       // 允许硬撑
	static const float OVERRIDE_STAMINA_PENALTY_MULT = 1.5;         // 1.5-2.0倍惩罚
	static const float OVERRIDE_MAX_DURATION_SECONDS = 45.0;        // 30-60s

	// =========================================================================
	// v5速度重标定(Phase 1)
	// =========================================================================

	// 35kg理想环境下的目标速度(m/s)
	// Elite: 保守行军档
	static const float WALK_SPEED_IDEAL_35KG_ELITE = 1.4;
	static const float RUN_SPEED_IDEAL_35KG_ELITE = 2.8;
	static const float SPRINT_SPEED_IDEAL_35KG_ELITE = 4.0;

	// Standard: 中等行军档
	static const float WALK_SPEED_IDEAL_35KG_STANDARD = 1.5;
	static const float RUN_SPEED_IDEAL_35KG_STANDARD = 3.0;
	static const float SPRINT_SPEED_IDEAL_35KG_STANDARD = 4.2;

	// Tactical: 快速行军档
	static const float WALK_SPEED_IDEAL_35KG_TACTICAL = 1.7;
	static const float RUN_SPEED_IDEAL_35KG_TACTICAL = 3.2;
	static const float SPRINT_SPEED_IDEAL_35KG_TACTICAL = 4.5;

	// =========================================================================
	// v5系数物理化(Phase 3)
	// =========================================================================

	// 参考点标定
	static const float REFERENCE_MARCH_SPEED_MS = 1.4;      // m/s
	static const float REFERENCE_WATTS_AT_MARCH = 400.0;    // W

	// 能量-体力转换系数(由优化器计算,这里是初始值)
	// 在参考点(1.4 m/s, 35kg, 平路)净消耗应接近0
	static const float ENERGY_TO_STAMINA_COEFF_ELITE = 9.5e-7;     // Phase 3后优化
	static const float ENERGY_TO_STAMINA_COEFF_STANDARD = 1.0e-6;
	static const float ENERGY_TO_STAMINA_COEFF_TACTICAL = 1.05e-6;

	// 负重代谢削弱系数(v5默认1.0,即不削弱)
	static const float LOAD_METABOLIC_DAMPENING = 1.0;

	// =========================================================================
	// v5 Schema版本控制
	// =========================================================================

	static const int STAMINA_SCHEMA_V4 = 4;
	static const int STAMINA_SCHEMA_V5 = 5;  // 本次实现版本

	// =========================================================================
	// v5 Legacy兼容开关
	// =========================================================================

	static const bool LEGACY_V4_SINGLE_POOL_MODE = false;           // 默认关
	static const bool LEGACY_V4_DISABLE_FORCED_SLOW = false;        // 默认关
	static const bool LEGACY_V4_USE_OLD_DRAIN_VELOCITY = false;     // 默认关(不用3.8/5.5)

	// =========================================================================
	// 预设辅助函数
	// =========================================================================

	// 根据预设名称获取参数组
	static ref array<float> GetSpeedsByPreset(string presetName)
	{
		array<float> speeds = new array<float>;

		if (presetName == "Elite")
		{
			speeds.Insert(WALK_SPEED_IDEAL_35KG_ELITE);
			speeds.Insert(RUN_SPEED_IDEAL_35KG_ELITE);
			speeds.Insert(SPRINT_SPEED_IDEAL_35KG_ELITE);
		}
		else if (presetName == "Standard")
		{
			speeds.Insert(WALK_SPEED_IDEAL_35KG_STANDARD);
			speeds.Insert(RUN_SPEED_IDEAL_35KG_STANDARD);
			speeds.Insert(SPRINT_SPEED_IDEAL_35KG_STANDARD);
		}
		else if (presetName == "Tactical")
		{
			speeds.Insert(WALK_SPEED_IDEAL_35KG_TACTICAL);
			speeds.Insert(RUN_SPEED_IDEAL_35KG_TACTICAL);
			speeds.Insert(SPRINT_SPEED_IDEAL_35KG_TACTICAL);
		}

		return speeds;
	}

	// 获取冷却时间(根据预设)
	static float GetCooldownFull(string presetName)
	{
		if (presetName == "Elite")
			return BURST_COOLDOWN_FULL_ELITE;
		else if (presetName == "Standard")
			return BURST_COOLDOWN_FULL_STANDARD;
		else if (presetName == "Tactical")
			return BURST_COOLDOWN_FULL_TACTICAL;

		return BURST_COOLDOWN_FULL_ELITE;  // 默认Elite
	}

	static float GetCooldownShort(string presetName)
	{
		if (presetName == "Elite")
			return BURST_COOLDOWN_SHORT_ELITE;
		else if (presetName == "Standard")
			return BURST_COOLDOWN_SHORT_STANDARD;
		else if (presetName == "Tactical")
			return BURST_COOLDOWN_SHORT_TACTICAL;

		return BURST_COOLDOWN_SHORT_ELITE;  // 默认Elite
	}
};
