/*
 * SCR_PandolfCalculator_V5.c
 * 
 * Phase 2.3: 完整的 Pandolf 代谢功率计算器
 * 
 * 目的：
 * - 替换 PlayerBase.c 中的简化版 Pandolf 公式
 * - 提供标准化的代谢功率计算（Watts）
 * - 支持结果缓存以优化性能
 * - 与 SCR_RealisticStaminaSystem.CalculatePandolfEnergyExpenditure 共享相同的物理模型
 */

class SCR_PandolfCalculator_V5
{
	// =========================================================================
	// 缓存机制（避免同一帧内重复计算）
	// =========================================================================
	
	protected static float s_fCachedWatts = 0.0;
	protected static float s_fCachedSpeed = -1.0;
	protected static float s_fCachedWeight = -1.0;
	protected static float s_fCachedGrade = 999.0;
	protected static float s_fCachedTerrain = -1.0;
	protected static float s_fCacheTimestamp = -1.0;
	protected static const float CACHE_VALIDITY_TIME = 0.05;  // 50ms缓存有效期
	
	// =========================================================================
	// 主计算方法：计算代谢功率（瓦特）
	// =========================================================================
	
	// 计算 Pandolf 代谢功率（Watts）
	// @param velocity 当前速度（m/s）
	// @param bodyWeight 身体重量（kg）- 通常为 CHARACTER_WEIGHT (90kg)
	// @param loadWeight 负重（kg）- 装备重量
	// @param gradePercent 坡度百分比（例如 5% = 5.0）
	// @param terrainFactor 地形系数（1.0 = 铺装路面，1.2 = 草地，1.8 = 沙地）
	// @param currentTime 当前时间（秒），用于缓存失效判断，-1 表示不使用缓存
	// @return 代谢功率（Watts）
	static float CalculateMetabolicPower(
		float velocity,
		float bodyWeight,
		float loadWeight,
		float gradePercent,
		float terrainFactor,
		float currentTime = -1.0)
	{
		// 输入验证
		velocity = Math.Max(velocity, 0.0);
		bodyWeight = Math.Max(bodyWeight, 1.0);
		loadWeight = Math.Max(loadWeight, 0.0);
		terrainFactor = Math.Clamp(terrainFactor, 0.5, 3.0);
		
		// 缓存检查（如果提供了时间戳）
		if (currentTime >= 0.0)
		{
			float timeSinceCache = currentTime - s_fCacheTimestamp;
			if (timeSinceCache < CACHE_VALIDITY_TIME &&
				Math.AbsFloat(velocity - s_fCachedSpeed) < 0.01 &&
				Math.AbsFloat(bodyWeight + loadWeight - s_fCachedWeight) < 0.1 &&
				Math.AbsFloat(gradePercent - s_fCachedGrade) < 0.1 &&
				Math.AbsFloat(terrainFactor - s_fCachedTerrain) < 0.01)
			{
				// 缓存命中
				return s_fCachedWatts;
			}
		}
		
		// =========================================================================
		// Pandolf 公式实现
		// =========================================================================
		
		// 极低速处理：使用静态站立成本
		if (velocity < 0.1)
		{
			float staticWatts = CalculateStaticStandingPower(bodyWeight, loadWeight);
			
			// 更新缓存
			if (currentTime >= 0.0)
			{
				s_fCachedWatts = staticWatts;
				s_fCachedSpeed = velocity;
				s_fCachedWeight = bodyWeight + loadWeight;
				s_fCachedGrade = gradePercent;
				s_fCachedTerrain = terrainFactor;
				s_fCacheTimestamp = currentTime;
			}
			
			return staticWatts;
		}
		
		// Pandolf 模型常量
		const float REFERENCE_WEIGHT = 70.0;  // kg
		const float VELOCITY_OFFSET = 0.7;    // m/s
		const float BASE_COEFF = 2.7;         // 基础代谢系数
		const float VELOCITY_COEFF = 3.2;     // 速度二次项系数
		const float GRADE_BASE = 0.23;        // 坡度基础系数
		const float GRADE_VELOCITY = 1.34;    // 坡度-速度交互系数
		
		// Fitness Bonus（固定值，匹配 RealisticStaminaSystem）
		const float FITNESS_BONUS = 0.80;  // 1.0 - 0.2 * FITNESS_LEVEL (1.0)
		
		// 计算基础项：(2.7 * fitness_bonus) + 3.2·(V-0.7)²
		float vOffset = velocity - VELOCITY_OFFSET;
		float vOffsetSquared = vOffset * vOffset;
		float baseTerm = (BASE_COEFF * FITNESS_BONUS) + (VELOCITY_COEFF * vOffsetSquared);
		
		// 计算坡度项：G·(0.23 + 1.34·V²)
		float gradeDecimal = gradePercent * 0.01;
		float vSquared = velocity * velocity;
		float gradeTerm = gradeDecimal * (GRADE_BASE + (GRADE_VELOCITY * vSquared));
		
		// 坡度保护：限制坡度项的最大贡献（防止极端坡度导致功率爆炸）
		float maxGradeTerm = baseTerm * 3.0;
		gradeTerm = Math.Min(gradeTerm, maxGradeTerm);
		
		// 缓下坡修正（0% ~ -12%）：能耗最低区
		if (gradePercent < 0.0 && gradePercent > -12.0)
		{
			gradeTerm = gradeTerm * 1.2;  // 放大负坡度项的"省能"效果
		}
		
		// 陡下坡惩罚（< -15%）：离心收缩/刹车耗能
		float steepDownhillPenalty = 0.0;
		if (gradePercent < -15.0)
		{
			float absGrade = Math.AbsFloat(gradePercent);
			float ramp = (absGrade - 15.0) / 15.0;
			if (ramp > 1.0)
				ramp = 1.0;
			steepDownhillPenalty = baseTerm * ramp * 0.40;  // 最多增加 40% 基础消耗
		}
		
		// 体重倍数（相对于 70kg 参考体重）
		float totalWeight = bodyWeight + loadWeight;
		float weightMultiplier = totalWeight / REFERENCE_WEIGHT;
		weightMultiplier = Math.Max(weightMultiplier, 0.1);  // 防止负数或过小值
		
		// 完整 Pandolf 公式：Watts = M · (基础项 + 坡度项 + 陡下坡惩罚) · η · REFERENCE_WEIGHT
		// 注意：必须乘以 REFERENCE_WEIGHT 才能得到瓦特数
		float watts = weightMultiplier * (baseTerm + gradeTerm + steepDownhillPenalty) * terrainFactor * REFERENCE_WEIGHT;
		
		// 确保非负
		watts = Math.Max(watts, 0.0);
		
		// 更新缓存
		if (currentTime >= 0.0)
		{
			s_fCachedWatts = watts;
			s_fCachedSpeed = velocity;
			s_fCachedWeight = totalWeight;
			s_fCachedGrade = gradePercent;
			s_fCachedTerrain = terrainFactor;
			s_fCacheTimestamp = currentTime;
		}
		
		return watts;
	}
	
	// =========================================================================
	// 辅助方法：静态站立功率
	// =========================================================================
	
	// 计算静态站立时的代谢功率（Watts）
	// 基于：Bergstrom et al. (2020) - Energy Expenditure During Standing
	// 约 1.2-1.5 MET（代谢当量），1 MET ≈ 3.5 ml O2/kg/min ≈ 1.2 kcal/kg/hr
	static float CalculateStaticStandingPower(float bodyWeight, float loadWeight)
	{
		float totalWeight = bodyWeight + loadWeight;
		
		// 基础站立功率：约 1.2 W/kg（身体重量）
		float baseWatts = bodyWeight * 1.2;
		
		// 负重额外功率：约 0.3 W/kg（负重）
		float loadWatts = loadWeight * 0.3;
		
		return baseWatts + loadWatts;
	}
	
	// =========================================================================
	// 调试方法
	// =========================================================================
	
	// 打印 Pandolf 计算调试信息
	static void DebugPrint(
		float velocity,
		float bodyWeight,
		float loadWeight,
		float gradePercent,
		float terrainFactor,
		float watts)
	{
		SCR_RSS_Logger.Debug(string.Format(
			"[PandolfCalc] v=%.2f m/s, W=%.1f kg (body=%.1f load=%.1f), G=%.1f%%, η=%.2f -> %.0f W",
			velocity, bodyWeight + loadWeight, bodyWeight, loadWeight, gradePercent, terrainFactor, watts
		));
	}
	
	// =========================================================================
	// 缓存管理
	// =========================================================================
	
	// 清除缓存（在场景切换或角色切换时调用）
	static void InvalidateCache()
	{
		s_fCacheTimestamp = -1.0;
		s_fCachedWatts = 0.0;
		s_fCachedSpeed = -1.0;
		s_fCachedWeight = -1.0;
		s_fCachedGrade = 999.0;
		s_fCachedTerrain = -1.0;
	}
};
